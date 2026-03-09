/**
 ******************************************************************************
 * @file    vs1003.c
 * @brief   VS1003B-L 音频解码芯片驱动实现
 * @author  RideWind Team
 * @date    2025-12-29
 * @version 2.0
 * @note    根据实际电路原理图实现完整驱动
 *          晶振: 12.288MHz
 *          音频输出: 连接到PAM8406DR功放
 ******************************************************************************
 */

#include "vs1003.h"
#include <string.h>
#include <stdio.h>

/* ======================== 私有变量 ======================== */

static VS1003_State_t vs_state = VS_IDLE;
static uint8_t vs_volume = 50;  /* 默认音量50% */

/* ======================== SPI 通信函数 ======================== */

/**
 * @brief  SPI3单字节读写
 * @param  byte: 发送的字节
 * @retval 接收到的字节
 */
static uint8_t VS1003_SPI_ReadWriteByte(uint8_t byte) {
    uint8_t rxByte = 0;
    HAL_SPI_TransmitReceive(&hspi3, &byte, &rxByte, 1, 100);
    return rxByte;
}

/**
 * @brief  等待DREQ变高 (VS1003准备好接收数据)
 * @note   DREQ为高表示VS1003可以接收命令/数据
 */
void VS1003_WaitDREQ(void) {
    uint32_t timeout = 0xFFFF;
    while (VS1003_DREQ_Read() == GPIO_PIN_RESET) {
        if (--timeout == 0) {
            break;  /* 超时退出 */
        }
    }
}

/**
 * @brief  检查DREQ状态
 * @retval 1: 可以发送数据, 0: 忙碌
 */
uint8_t VS1003_CheckDREQ(void) {
    return (VS1003_DREQ_Read() == GPIO_PIN_SET) ? 1 : 0;
}

/* ======================== 寄存器操作 ======================== */

/**
 * @brief  写VS1003命令寄存器
 * @param  regAddr: 寄存器地址
 * @param  regVal: 寄存器值 (16位)
 */
void VS1003_WriteCmd(uint8_t regAddr, uint16_t regVal) {
    VS1003_WaitDREQ();
    
    VS1003_XDCS_High();     /* 数据片选无效 */
    VS1003_XCS_Low();       /* 命令片选有效 */
    
    VS1003_SPI_ReadWriteByte(VS_WRITE_CMD);     /* 写命令 */
    VS1003_SPI_ReadWriteByte(regAddr);          /* 寄存器地址 */
    VS1003_SPI_ReadWriteByte(regVal >> 8);      /* 高字节 */
    VS1003_SPI_ReadWriteByte(regVal & 0xFF);    /* 低字节 */
    
    VS1003_XCS_High();      /* 命令片选无效 */
}

/**
 * @brief  写VS1003寄存器 (兼容旧接口)
 */
void VS1003_WriteReg(uint8_t regAddr, uint16_t regVal) {
    VS1003_WriteCmd(regAddr, regVal);
}

/**
 * @brief  读VS1003命令寄存器
 * @param  regAddr: 寄存器地址
 * @retval 寄存器值 (16位)
 */
uint16_t VS1003_ReadReg(uint8_t regAddr) {
    uint16_t result;
    
    VS1003_WaitDREQ();
    
    VS1003_XDCS_High();     /* 数据片选无效 */
    VS1003_XCS_Low();       /* 命令片选有效 */
    
    VS1003_SPI_ReadWriteByte(VS_READ_CMD);      /* 读命令 */
    VS1003_SPI_ReadWriteByte(regAddr);          /* 寄存器地址 */
    result = VS1003_SPI_ReadWriteByte(0xFF);    /* 读高字节 */
    result <<= 8;
    result |= VS1003_SPI_ReadWriteByte(0xFF);   /* 读低字节 */
    
    VS1003_XCS_High();      /* 命令片选无效 */
    
    return result;
}

/* ======================== 音频数据发送 ======================== */

/**
 * @brief  发送音频数据到VS1003
 * @param  buf: 数据缓冲区
 * @param  len: 数据长度 (建议32字节的倍数)
 * @note   发送前必须检查DREQ状态
 */
void VS1003_SendMusicData(uint8_t *buf, uint16_t len) {
    VS1003_XCS_High();      /* 命令片选无效 */
    VS1003_XDCS_Low();      /* 数据片选有效 */
    
    for (uint16_t i = 0; i < len; i++) {
        VS1003_SPI_ReadWriteByte(buf[i]);
    }
    
    VS1003_XDCS_High();     /* 数据片选无效 */
}

/**
 * @brief  发送单个音频数据字节
 * @param  data: 数据字节
 */
void VS1003_SendMusicByte(uint8_t data) {
    VS1003_XCS_High();
    VS1003_XDCS_Low();
    VS1003_SPI_ReadWriteByte(data);
    VS1003_XDCS_High();
}

/**
 * @brief  发送指定数量的零字节 (用于结束播放)
 * @param  len: 零字节数量
 * @retval 0: 成功
 */
uint8_t VS1003_SendZeros(uint16_t len) {
    VS1003_XCS_High();
    VS1003_XDCS_Low();
    
    for (uint16_t i = 0; i < len; i++) {
        VS1003_WaitDREQ();
        VS1003_SPI_ReadWriteByte(0x00);
    }
    
    VS1003_XDCS_High();
    return 0;
}

/* ======================== 初始化与复位 ======================== */

/**
 * @brief  VS1003 GPIO初始化
 * @note   配置XCS(PB9), XDCS(PC13), XRESET(PB6)为输出, DREQ(PB7)为输入
 *         部分引脚可能已在gpio.c中配置(如vx_cs_Pin=PB9)
 */
void VS1003_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* 使能GPIO时钟 */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    
    /* 配置输出引脚: XCS(PB9), XRESET(PB6) */
    GPIO_InitStruct.Pin = VS1003_XCS_PIN | VS1003_XRESET_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    /* 配置输出引脚: XDCS(PC13) */
    GPIO_InitStruct.Pin = VS1003_XDCS_PIN;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    /* 配置输入引脚: DREQ(PB7) */
    GPIO_InitStruct.Pin = VS1003_DREQ_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    /* 默认状态: 片选无效(高电平) */
    VS1003_XCS_High();
    VS1003_XDCS_High();
    VS1003_XRESET_High();
    
    printf("[VS1003] GPIO initialized: XCS=PB9, XDCS=PC13, XRESET=PB6, DREQ=PB7\r\n");
}

/**
 * @brief  VS1003 硬件复位
 */
void VS1003_HardReset(void) {
    VS1003_XCS_High();
    VS1003_XDCS_High();
    
    VS1003_XRESET_Low();    /* 复位 */
    HAL_Delay(50);          /* 保持至少5ms */
    VS1003_XRESET_High();   /* 释放复位 */
    HAL_Delay(50);          /* 等待启动 */
    
    VS1003_WaitDREQ();      /* 等待VS1003就绪 */
}

/**
 * @brief  VS1003 软件复位
 */
void VS1003_SoftReset(void) {
    VS1003_WaitDREQ();
    
    /* 写入MODE寄存器，设置SM_RESET位 */
    VS1003_WriteCmd(VS_MODE, SM_SDINEW | SM_RESET);
    
    HAL_Delay(10);
    VS1003_WaitDREQ();
}

/**
 * @brief  VS1003 完整初始化
 * @note   执行硬件复位、软件复位、时钟配置、音量设置等
 */
void VS1003_Init(void) {
    /* 1. GPIO初始化 */
    VS1003_GPIO_Init();
    
    /* 2. 硬件复位 */
    VS1003_HardReset();
    
    /* 3. 软件复位 */
    VS1003_SoftReset();
    
    /* 4. 等待VS1003就绪 */
    VS1003_WaitDREQ();
    
    /* 5. 设置时钟
     *    VS1003使用12.288MHz晶振
     *    CLOCKF = 0x9800 表示:
     *    - SC_MULT = 3 (3.5x)
     *    - SC_ADD = 1 (允许+1.0x)
     *    - SC_FREQ = 0x000 (不调整)
     */
    VS1003_WriteCmd(VS_CLOCKF, 0x9800);
    HAL_Delay(10);
    
    /* 6. 设置模式
     *    SM_SDINEW: VS1002原生SPI模式
     *    不设置SM_LAYER12，允许所有格式包括MP3 (Layer III)
     */
    VS1003_WriteCmd(VS_MODE, SM_SDINEW);
    HAL_Delay(10);
    
    /* 7. 设置默认音量 (0x00为最大, 0xFE为静音) */
    VS1003_SetVolumePercent(50);
    
    /* 8. 设置默认音效 (无低音增强) */
    VS1003_WriteCmd(VS_BASS, 0x0000);
    
    /* 9. 清除解码时间 */
    VS1003_WriteCmd(VS_DECODE_TIME, 0x0000);
    VS1003_WriteCmd(VS_DECODE_TIME, 0x0000);
    
    /* 10. 发送4个零字节，确保VS1003进入正确状态 */
    VS1003_WaitDREQ();
    VS1003_SendMusicData((uint8_t*)"\0\0\0\0", 4);
    
    vs_state = VS_IDLE;
    
    /* 调试输出 */
    printf("[VS1003] Init completed, MODE=0x%04X, STATUS=0x%04X\r\n", 
           VS1003_ReadReg(VS_MODE), VS1003_ReadReg(VS_STATUS));
}

/* ======================== 音量与音效控制 ======================== */

/**
 * @brief  设置音量 (左右声道独立)
 * @param  left: 左声道音量 (0x00最大, 0xFE最小)
 * @param  right: 右声道音量 (0x00最大, 0xFE最小)
 */
void VS1003_SetVolume(uint8_t left, uint8_t right) {
    uint16_t vol = ((uint16_t)left << 8) | right;
    VS1003_WriteCmd(VS_VOL, vol);
}

/**
 * @brief  设置音量 (百分比)
 * @param  percent: 音量百分比 (0-100)
 */
void VS1003_SetVolumePercent(uint8_t percent) {
    if (percent > 100) percent = 100;
    
    /* 将0-100映射到0xFE-0x00 (反向) */
    uint8_t vol = (uint8_t)((100 - percent) * 254 / 100);
    
    vs_volume = percent;
    VS1003_SetVolume(vol, vol);
    
    printf("[VS1003] Volume set to %d%%\r\n", percent);
}

/**
 * @brief  设置低音和高音增强
 * @param  bassAmp: 低音增强幅度 (0-15, 单位dB)
 * @param  bassFreq: 低音截止频率 (2-15, 单位10Hz)
 * @param  trebleAmp: 高音增强幅度 (0-15, 单位1.5dB, 8-15为负值)
 * @param  trebleFreq: 高音截止频率 (0-15, 单位kHz)
 */
void VS1003_SetBass(uint8_t bassAmp, uint8_t bassFreq, uint8_t trebleAmp, uint8_t trebleFreq) {
    uint16_t bass = ((uint16_t)(trebleAmp & 0x0F) << 12) |
                    ((uint16_t)(trebleFreq & 0x0F) << 8) |
                    ((uint16_t)(bassAmp & 0x0F) << 4) |
                    (bassFreq & 0x0F);
    VS1003_WriteCmd(VS_BASS, bass);
}

/* ======================== 状态查询 ======================== */

/**
 * @brief  获取当前播放状态
 */
VS1003_State_t VS1003_GetState(void) {
    return vs_state;
}

/**
 * @brief  获取音频信息
 * @param  info: 音频信息结构体指针
 */
void VS1003_GetAudioInfo(VS1003_AudioInfo_t *info) {
    uint16_t audata = VS1003_ReadReg(VS_AUDATA);
    
    info->sampleRate = audata & 0xFFFE;
    info->channels = (audata & 0x0001) ? 2 : 1;
    info->decodeTime = VS1003_ReadReg(VS_DECODE_TIME);
    
    /* HDAT1和HDAT0包含流头信息 */
    uint16_t hdat1 = VS1003_ReadReg(VS_HDAT1);
    uint16_t hdat0 = VS1003_ReadReg(VS_HDAT0);
    
    /* 根据HDAT1高4位判断音频格式 */
    if ((hdat1 & 0xFFE0) == 0xFFE0) {
        /* MP3格式 */
        info->bitRate = hdat0;
    }
}

/* ======================== 播放控制 ======================== */

/**
 * @brief  开始播放
 */
void VS1003_Play(void) {
    vs_state = VS_PLAYING;
    printf("[VS1003] Play started\r\n");
}

/**
 * @brief  暂停播放
 */
void VS1003_Pause(void) {
    vs_state = VS_PAUSED;
    printf("[VS1003] Paused\r\n");
}

/**
 * @brief  继续播放
 */
void VS1003_Resume(void) {
    if (vs_state == VS_PAUSED) {
        vs_state = VS_PLAYING;
        printf("[VS1003] Resumed\r\n");
    }
}

/**
 * @brief  停止播放
 */
void VS1003_Stop(void) {
    /* 发送结束码 */
    VS1003_WriteCmd(VS_MODE, SM_SDINEW | SM_CANCEL);
    
    /* 发送至少2048个零字节 */
    for (int i = 0; i < 64; i++) {
        VS1003_WaitDREQ();
        VS1003_SendZeros(32);
    }
    
    /* 软件复位确保完全停止 */
    VS1003_SoftReset();
    
    vs_state = VS_STOPPED;
    printf("[VS1003] Stopped\r\n");
}

/* ======================== 测试函数 ======================== */

/**
 * @brief  正弦波测试 (用于验证硬件连接)
 * @param  idx: 正弦波频率索引 (0-7)
 *              0: 1kHz, 1: 2kHz, 2: 3kHz, ...
 */
void VS1003_SineTest(uint8_t idx) {
    /* 频率查找表 */
    static const uint8_t sine_table[] = {
        0x44, /* 1kHz */
        0x48, /* 2kHz */
        0x4C, /* 3kHz */
        0x50, /* 4kHz */
        0x54, /* 5kHz */
        0x58, /* 6kHz */
        0x5C, /* 7kHz */
        0x60  /* 8kHz */
    };
    
    if (idx > 7) idx = 7;
    
    /* 硬件复位 */
    VS1003_HardReset();
    
    /* 设置测试模式 */
    VS1003_WriteCmd(VS_MODE, SM_SDINEW | SM_TESTS);
    VS1003_WaitDREQ();
    
    /* 发送正弦波测试命令 */
    VS1003_XDCS_Low();
    VS1003_SPI_ReadWriteByte(0x53);
    VS1003_SPI_ReadWriteByte(0xEF);
    VS1003_SPI_ReadWriteByte(0x6E);
    VS1003_SPI_ReadWriteByte(sine_table[idx]);
    VS1003_SPI_ReadWriteByte(0x00);
    VS1003_SPI_ReadWriteByte(0x00);
    VS1003_SPI_ReadWriteByte(0x00);
    VS1003_SPI_ReadWriteByte(0x00);
    VS1003_XDCS_High();
    
    printf("[VS1003] Sine test started, freq index = %d\r\n", idx);
}

/**
 * @brief  停止正弦波测试
 */
void VS1003_SineTestOff(void) {
    VS1003_XDCS_Low();
    VS1003_SPI_ReadWriteByte(0x45);
    VS1003_SPI_ReadWriteByte(0x78);
    VS1003_SPI_ReadWriteByte(0x69);
    VS1003_SPI_ReadWriteByte(0x74);
    VS1003_SPI_ReadWriteByte(0x00);
    VS1003_SPI_ReadWriteByte(0x00);
    VS1003_SPI_ReadWriteByte(0x00);
    VS1003_SPI_ReadWriteByte(0x00);
    VS1003_XDCS_High();
    
    HAL_Delay(10);
    
    /* 恢复正常模式 */
    VS1003_HardReset();
    VS1003_Init();
    
    printf("[VS1003] Sine test stopped\r\n");
}

/**
 * @brief  RAM测试
 * @retval 0: 测试通过, 其他: 测试失败
 */
uint8_t VS1003_RamTest(void) {
    VS1003_HardReset();
    
    /* 进入测试模式 */
    VS1003_WriteCmd(VS_MODE, SM_SDINEW | SM_TESTS);
    VS1003_WaitDREQ();
    
    /* 发送RAM测试命令 */
    VS1003_XDCS_Low();
    VS1003_SPI_ReadWriteByte(0x4D);
    VS1003_SPI_ReadWriteByte(0xEA);
    VS1003_SPI_ReadWriteByte(0x6D);
    VS1003_SPI_ReadWriteByte(0x54);
    VS1003_SPI_ReadWriteByte(0x00);
    VS1003_SPI_ReadWriteByte(0x00);
    VS1003_SPI_ReadWriteByte(0x00);
    VS1003_SPI_ReadWriteByte(0x00);
    VS1003_XDCS_High();
    
    HAL_Delay(100);
    
    /* 读取测试结果 */
    uint16_t result = VS1003_ReadReg(VS_HDAT0);
    
    /* 恢复正常模式 */
    VS1003_HardReset();
    VS1003_Init();
    
    printf("[VS1003] RAM test result: 0x%04X (0x83FF = PASS)\r\n", result);
    
    return (result == 0x83FF) ? 0 : 1;
}

