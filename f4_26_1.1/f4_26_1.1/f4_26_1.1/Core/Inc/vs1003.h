/**
 ******************************************************************************
 * @file    vs1003.h
 * @brief   VS1003B-L ???????????
 * @author  RideWind Team
 * @date    2025-12-29
 * @version 2.0
 * @note    ???????????????
 ******************************************************************************
 * ???? (?????):
 *   VS1003 Pin    STM32 Pin    ??
 *   ---------    ---------    ----
 *   SCLK (28)    PB3          SPI3_SCK
 *   SO   (30)    PB4          SPI3_MISO
 *   SI   (29)    PB5          SPI3_MOSI
 *   XCS  (23)    PB8          ???? (???)
 *   XDCS (13)    PC13         ???? (???)
 *   XRESET(3)    PB6          ???? (???)
 *   DREQ (8)     PB7          ???? (?=?????)
 ******************************************************************************
 */

#ifndef __VS1003_H
#define __VS1003_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "spi.h"
#include "gpio.h"

/* ======================== ?????? ======================== */
/* ??????main.h???? */

/* ???? XCS - PB9 (vx_cs_Pin) */
#define VS1003_XCS_PORT       GPIOB
#define VS1003_XCS_PIN        GPIO_PIN_9

/* ???? XDCS - PC13 (DC2) */
#define VS1003_XDCS_PORT      GPIOC
#define VS1003_XDCS_PIN       GPIO_PIN_13

/* ???? XRESET - PB6 (REST) */
#define VS1003_XRESET_PORT    GPIOB
#define VS1003_XRESET_PIN     GPIO_PIN_6

/* ???? DREQ - PB7 (??) */
#define VS1003_DREQ_PORT      GPIOB
#define VS1003_DREQ_PIN       GPIO_PIN_7

/* ======================== VS1003 ????? ======================== */

#define VS_MODE         0x00    /* ??????? */
#define VS_STATUS       0x01    /* ????? */
#define VS_BASS         0x02    /* ??/???? */
#define VS_CLOCKF       0x03    /* ???? + ??? */
#define VS_DECODE_TIME  0x04    /* ????(?) */
#define VS_AUDATA       0x05    /* ?????? */
#define VS_WRAM         0x06    /* RAM?? */
#define VS_WRAMADDR     0x07    /* RAM???? */
#define VS_HDAT0        0x08    /* ????0 */
#define VS_HDAT1        0x09    /* ????1 */
#define VS_AIADDR       0x0A    /* ???????? */
#define VS_VOL          0x0B    /* ???? */
#define VS_AICTRL0      0x0C    /* ??????0 */
#define VS_AICTRL1      0x0D    /* ??????1 */
#define VS_AICTRL2      0x0E    /* ??????2 */
#define VS_AICTRL3      0x0F    /* ??????3 */

/* ======================== VS_MODE ?????? ======================== */

#define SM_DIFF         0x0001  /* ???? */
#define SM_LAYER12      0x0002  /* ??MPEG Layer I & II */
#define SM_RESET        0x0004  /* ???? */
#define SM_CANCEL       0x0008  /* ???? */
#define SM_EARSPEAKER_LO 0x0010 /* EarSpeaker??? */
#define SM_TESTS        0x0020  /* ??SDI?? */
#define SM_STREAM       0x0040  /* ??? */
#define SM_EARSPEAKER_HI 0x0080 /* EarSpeaker??? */
#define SM_DACT         0x0100  /* DCLK???? */
#define SM_SDIORD       0x0200  /* SDI??? */
#define SM_SDISHARE     0x0400  /* ??SCI/SDI?? */
#define SM_SDINEW       0x0800  /* VS1002??SPI?? */
#define SM_ADPCM        0x1000  /* ADPCM?? */
#define SM_LINE1        0x4000  /* ???/???? */
#define SM_CLK_RANGE    0x8000  /* ?????? */

/* ======================== SPI ???? ======================== */

#define VS_WRITE_CMD    0x02    /* ?????? */
#define VS_READ_CMD     0x03    /* ?????? */

/* ======================== ?????? ======================== */

typedef enum {
    VS_IDLE = 0,        /* ???? */
    VS_PLAYING,         /* ???? */
    VS_PAUSED,          /* ???? */
    VS_STOPPED          /* ???? */
} VS1003_State_t;

/* ======================== ??????? ======================== */

typedef struct {
    uint16_t sampleRate;    /* ??? */
    uint8_t  channels;      /* ??? */
    uint8_t  bitsPerSample; /* ??? */
    uint32_t bitRate;       /* ??? */
    uint32_t decodeTime;    /* ?????(?) */
} VS1003_AudioInfo_t;

/* ======================== ???? ======================== */

/* ?????? */
void VS1003_Init(void);
void VS1003_HardReset(void);
void VS1003_SoftReset(void);
void VS1003_GPIO_Init(void);

/* SPI ?? */
uint16_t VS1003_ReadReg(uint8_t regAddr);
void VS1003_WriteReg(uint8_t regAddr, uint16_t regVal);
void VS1003_WriteCmd(uint8_t regAddr, uint16_t regVal);

/* ?????? */
void VS1003_SendMusicData(uint8_t *buf, uint16_t len);
void VS1003_SendMusicByte(uint8_t data);
uint8_t VS1003_SendZeros(uint16_t len);

/* ???? */
void VS1003_SetVolume(uint8_t left, uint8_t right);
void VS1003_SetVolumePercent(uint8_t percent);
void VS1003_SetBass(uint8_t bassAmp, uint8_t bassFreq, uint8_t trebleAmp, uint8_t trebleFreq);

/* ???? */
uint8_t VS1003_CheckDREQ(void);
void VS1003_WaitDREQ(void);
VS1003_State_t VS1003_GetState(void);
void VS1003_GetAudioInfo(VS1003_AudioInfo_t *info);

/* ???? */
void VS1003_SineTest(uint8_t idx);
void VS1003_SineTestOff(void);
uint8_t VS1003_RamTest(void);

/* ???? */
void VS1003_Play(void);
void VS1003_Pause(void);
void VS1003_Stop(void);
void VS1003_Resume(void);

/* ???? (????????) */
static inline void VS1003_XCS_Low(void) {
    HAL_GPIO_WritePin(VS1003_XCS_PORT, VS1003_XCS_PIN, GPIO_PIN_RESET);
}

static inline void VS1003_XCS_High(void) {
    HAL_GPIO_WritePin(VS1003_XCS_PORT, VS1003_XCS_PIN, GPIO_PIN_SET);
}

static inline void VS1003_XDCS_Low(void) {
    HAL_GPIO_WritePin(VS1003_XDCS_PORT, VS1003_XDCS_PIN, GPIO_PIN_RESET);
}

static inline void VS1003_XDCS_High(void) {
    HAL_GPIO_WritePin(VS1003_XDCS_PORT, VS1003_XDCS_PIN, GPIO_PIN_SET);
}

static inline void VS1003_XRESET_Low(void) {
    HAL_GPIO_WritePin(VS1003_XRESET_PORT, VS1003_XRESET_PIN, GPIO_PIN_RESET);
}

static inline void VS1003_XRESET_High(void) {
    HAL_GPIO_WritePin(VS1003_XRESET_PORT, VS1003_XRESET_PIN, GPIO_PIN_SET);
}

static inline uint8_t VS1003_DREQ_Read(void) {
    return HAL_GPIO_ReadPin(VS1003_DREQ_PORT, VS1003_DREQ_PIN);
}

#ifdef __cplusplus
}
#endif

#endif /* __VS1003_H */
