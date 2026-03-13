/**
 ******************************************************************************
 * @file    engine_audio.h
 * @brief   多段引擎音效播放控制头文件
 * @author  RideWind Team
 * @date    2026-01-21
 * @version 2.0
 * @note    支持开机双闪启动音效 + 油门模式加速音效
 *          音频数据分别在 engine_start.h 和 engine_accel.h 中
 ******************************************************************************
 */

#ifndef __ENGINE_AUDIO_H
#define __ENGINE_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief  初始化引擎音频模块
 * @note   在 VS1003_Init() 之后调用
 */
void EngineAudio_Init(void);

/**
 * @brief  播放启动音效（开机双闪时调用）
 * @note   播放一次不循环，播放完自动停止
 */
void EngineAudio_PlayStart(void);

/**
 * @brief  开始油门模式音效（循环播放加速音效）
 * @note   进入油门模式时调用
 */
void EngineAudio_Start(void);

/**
 * @brief  停止音频播放
 * @note   退出油门模式时调用
 */
void EngineAudio_Stop(void);

/**
 * @brief  处理音频播放（主循环中调用）
 * @note   每1-10ms调用一次以保证流畅播放
 */
void EngineAudio_Process(void);

/**
 * @brief  检查是否正在播放
 * @retval 1: 正在播放, 0: 未播放
 */
uint8_t EngineAudio_IsPlaying(void);

/**
 * @brief  设置音量
 * @param  volume: 音量百分比 (0-100)
 */
void EngineAudio_SetVolume(uint8_t volume);

#ifdef __cplusplus
}
#endif

#endif /* __ENGINE_AUDIO_H */
