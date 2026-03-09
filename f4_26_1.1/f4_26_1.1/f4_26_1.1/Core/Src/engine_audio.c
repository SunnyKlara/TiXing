/**
 ******************************************************************************
 * @file    engine_audio.c
 * @brief   多段引擎音效播放控制（启动音效 + 加速音效）
 * @author  RideWind Team
 * @date    2026-01-21
 * @version 2.0
 * @note    支持开机双闪启动音效 + 油门模式加速音效
 ******************************************************************************
 */

#include "engine_audio.h"
#include "engine_start.h"   // 启动音效
#include "engine_accel.h"   // 加速音效（油门模式循环播放）
#include "vs1003.h"
#include <stdio.h>

/* ======================== 音频段定义 ======================== */
typedef enum {
    AUDIO_NONE = 0,
    AUDIO_START,    // 启动音效（开机双闪时播放一次）
    AUDIO_ACCEL,    // 加速音效（油门模式循环播放）
} AudioSegment_t;

/* ======================== 私有变量 ======================== */
static uint8_t engineAudioEnabled = 0;      // 音频启用标志
static uint8_t engineAudioPlaying = 0;      // 正在播放标志
static AudioSegment_t currentSegment = AUDIO_NONE;  // 当前播放的音频段
static uint32_t playOffset = 0;             // 当前播放位置
static uint32_t lastProcessTick = 0;        // 上次处理时间

// 当前音频段的数据指针和参数
static const uint8_t* currentAudioData = NULL;
static uint32_t currentAudioSize = 0;
static uint32_t currentAudioStartOffset = 0;
static uint8_t currentLoopEnabled = 0;      // 是否循环播放

/* ======================== 私有函数 ======================== */

/**
 * @brief  设置当前播放的音频段
 */
static void SetAudioSegment(AudioSegment_t segment, uint8_t loop)
{
    switch (segment) {
        case AUDIO_START:
            currentAudioData = engine_start_data;
            currentAudioSize = ENGINE_START_SIZE;
            currentAudioStartOffset = ENGINE_START_START_OFFSET;
            break;
        case AUDIO_ACCEL:
            currentAudioData = engine_accel_data;
            currentAudioSize = ENGINE_ACCEL_SIZE;
            currentAudioStartOffset = ENGINE_ACCEL_START_OFFSET;
            break;
        default:
            currentAudioData = NULL;
            currentAudioSize = 0;
            currentAudioStartOffset = 0;
            break;
    }
    currentSegment = segment;
    currentLoopEnabled = loop;
    playOffset = currentAudioStartOffset;
}

/**
 * @brief  发送初始数据到VS1003
 */
static void SendInitialData(uint16_t bytes)
{
    uint16_t initSent = 0;
    while (initSent < bytes && playOffset < currentAudioSize) {
        if (!VS1003_CheckDREQ()) {
            break;
        }
        
        uint32_t remaining = currentAudioSize - playOffset;
        uint16_t sendLen = (remaining > 32) ? 32 : (uint16_t)remaining;
        
        VS1003_SendMusicData((uint8_t*)&currentAudioData[playOffset], sendLen);
        
        playOffset += sendLen;
        initSent += sendLen;
    }
}

/* ======================== 公共函数 ======================== */

/**
 * @brief  初始化引擎音频模块
 */
void EngineAudio_Init(void)
{
    // 验证启动音效
    if (engine_start_data[ENGINE_START_START_OFFSET] == 0xFF &&
        (engine_start_data[ENGINE_START_START_OFFSET + 1] & 0xE0) == 0xE0) {
        printf("[EngineAudio] Start audio OK, size: %d bytes\r\n", ENGINE_START_SIZE);
    }
    
    // 验证加速音效
    if (engine_accel_data[ENGINE_ACCEL_START_OFFSET] == 0xFF &&
        (engine_accel_data[ENGINE_ACCEL_START_OFFSET + 1] & 0xE0) == 0xE0) {
        printf("[EngineAudio] Accel audio OK, size: %d bytes\r\n", ENGINE_ACCEL_SIZE);
    }
    
    engineAudioEnabled = 0;
    engineAudioPlaying = 0;
    currentSegment = AUDIO_NONE;
    
    printf("[EngineAudio] Multi-segment audio initialized\r\n");
}

/**
 * @brief  播放启动音效（开机双闪时调用，播放一次不循环）
 * @note   音量设置为最大，确保能听到
 */
void EngineAudio_PlayStart(void)
{
    // 如果正在播放，先停止
    if (engineAudioPlaying) {
        VS1003_Stop();
        HAL_Delay(50);
    }
    
    // 🔊 设置最大音量！
    VS1003_SetVolumePercent(100);
    
    SetAudioSegment(AUDIO_START, 0);  // 不循环
    engineAudioEnabled = 1;
    engineAudioPlaying = 1;
    lastProcessTick = 0;
    
    // 发送更多初始数据，确保立即开始播放
    SendInitialData(2048);
    VS1003_Play();
    
    printf("[EngineAudio] Playing start sound (size=%d, offset=%d)\r\n", 
           ENGINE_START_SIZE, ENGINE_START_START_OFFSET);
}

/**
 * @brief  开始油门模式音效（循环播放加速音效）
 * @note   音量设置为90%，确保一开始就很响
 */
void EngineAudio_Start(void)
{
    if (engineAudioPlaying && currentSegment == AUDIO_ACCEL) {
        return;  // 已经在播放加速音效
    }
    
    // 如果正在播放其他音效，先停止
    if (engineAudioPlaying) {
        VS1003_Stop();
        HAL_Delay(30);
    }
    
    // 🔊 设置高音量
    VS1003_SetVolumePercent(90);
    
    SetAudioSegment(AUDIO_ACCEL, 1);  // 循环播放
    engineAudioEnabled = 1;
    engineAudioPlaying = 1;
    lastProcessTick = 0;
    
    // 发送更多初始数据，确保立即开始播放
    SendInitialData(2048);
    VS1003_Play();
    
    printf("[EngineAudio] Playing accel sound (loop, size=%d)\r\n", ENGINE_ACCEL_SIZE);
}

/**
 * @brief  停止音频播放
 */
void EngineAudio_Stop(void)
{
    if (!engineAudioPlaying) {
        return;
    }
    
    engineAudioEnabled = 0;
    engineAudioPlaying = 0;
    currentSegment = AUDIO_NONE;
    
    VS1003_Stop();
    VS1003_SetVolumePercent(80);
    
    printf("[EngineAudio] Playback stopped\r\n");
}

/**
 * @brief  处理音频播放（主循环中调用）
 * @note   优化版本：更频繁地发送数据，确保VS1003缓冲区不会欠载
 */
void EngineAudio_Process(void)
{
    if (!engineAudioEnabled || !engineAudioPlaying || currentAudioData == NULL) {
        return;
    }
    
    // 不限制调用频率，每次调用都尝试发送数据
    // VS1003的DREQ会自动控制发送速率
    
    // 发送音频数据 - 尽可能多地发送
    uint16_t totalSent = 0;
    uint16_t maxSendPerCall = 256;  // 每次调用最多发送256字节，避免阻塞太久
    
    while (totalSent < maxSendPerCall && playOffset < currentAudioSize) {
        // 检查VS1003是否准备好接收数据
        if (!VS1003_CheckDREQ()) {
            break;  // VS1003缓冲区满，下次再发
        }
        
        uint32_t remaining = currentAudioSize - playOffset;
        uint16_t sendLen = (remaining > 32) ? 32 : (uint16_t)remaining;
        
        VS1003_SendMusicData((uint8_t*)&currentAudioData[playOffset], sendLen);
        
        playOffset += sendLen;
        totalSent += sendLen;
        
        // 播放到末尾
        if (playOffset >= currentAudioSize) {
            if (currentLoopEnabled) {
                // 循环播放：回到起始位置（跳过ID3和Xing头）
                playOffset = currentAudioStartOffset;
                // 不打印日志，避免影响播放流畅性
            } else {
                // 不循环：停止播放
                engineAudioPlaying = 0;
                engineAudioEnabled = 0;
                currentSegment = AUDIO_NONE;
                printf("[EngineAudio] Playback finished\r\n");
                break;
            }
        }
    }
}

/**
 * @brief  检查是否正在播放
 */
uint8_t EngineAudio_IsPlaying(void)
{
    return engineAudioPlaying;
}

/**
 * @brief  设置音量
 */
void EngineAudio_SetVolume(uint8_t volume)
{
    VS1003_SetVolumePercent(volume);
}

