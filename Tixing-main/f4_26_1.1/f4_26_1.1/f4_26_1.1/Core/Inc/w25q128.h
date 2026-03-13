/**
  ******************************************************************************
  * @file    w25q128.h
  * @brief   Header file for W25Q128 SPI Flash driver
  ******************************************************************************
  */

#ifndef __W25Q128_H
#define __W25Q128_H


#include "main.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"
#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include "spi.h"
#include "gpio.h"

/* W25Q128指令集 */
#define W25Q128_WRITE_ENABLE      0x06
#define W25Q128_WRITE_DISABLE     0x04
#define W25Q128_READ_STATUS_REG1  0x05
#define W25Q128_READ_STATUS_REG2  0x35
#define W25Q128_WRITE_STATUS_REG  0x01
#define W25Q128_PAGE_PROGRAM      0x02
#define W25Q128_QUAD_PAGE_PROG    0x32
#define W25Q128_BLOCK_ERASE_64K   0xD8
#define W25Q128_BLOCK_ERASE_32K   0x52
#define W25Q128_SECTOR_ERASE      0x20
#define W25Q128_CHIP_ERASE        0xC7
#define W25Q128_ERASE_SUSPEND     0x75
#define W25Q128_ERASE_RESUME      0x7A
#define W25Q128_POWER_DOWN        0xB9
#define W25Q128_HIGH_PERF_MODE    0xA3
#define W25Q128_CONTINUOUS_MODE   0x77
#define W25Q128_RELEASE_PD_ID     0xAB
#define W25Q128_MANUFACTURER_ID   0x90
#define W25Q128_JEDEC_ID          0x9F
#define W25Q128_READ_DATA         0x03
#define W25Q128_FAST_READ         0x0B
#define W25Q128_FAST_READ_DUAL    0x3B
#define W25Q128_FAST_READ_QUAD    0x6B

/* 状态寄存器位定义 */
#define W25Q128_SR_WIP            0x01  /* 写忙标志 */
#define W25Q128_SR_WEL            0x02  /* 写使能锁存器 */
#define W25Q128_SR_BP0            0x04  /* 块保护位0 */
#define W25Q128_SR_BP1            0x08  /* 块保护位1 */
#define W25Q128_SR_BP2            0x10  /* 块保护位2 */
#define W25Q128_SR_TB             0x20  /* 顶部/底部块保护 */
#define W25Q128_SR_SEC            0x40  /* 扇区保护 */
#define W25Q128_SR_SRP            0x80  /* 状态寄存器保护 */

/* W25Q128参数 */
#define W25Q128_PAGE_SIZE         256     /* 页大小（字节） */
#define W25Q128_SECTOR_SIZE       4096    /* 扇区大小（字节） */
#define W25Q128_BLOCK_SIZE        65536   /* 块大小（字节） */
#define W25Q128_TOTAL_SIZE        16777216 /* 总容量（16MB = 128Mb） */
#define W25Q128_PAGE_COUNT        (W25Q128_TOTAL_SIZE / W25Q128_PAGE_SIZE)
#define W25Q128_SECTOR_COUNT      (W25Q128_TOTAL_SIZE / W25Q128_SECTOR_SIZE)
#define W25Q128_BLOCK_COUNT       (W25Q128_TOTAL_SIZE / W25Q128_BLOCK_SIZE)

/* 函数声明 */
void W25Q128_Init(void);
uint8_t W25Q128_ReadID(void);
uint8_t W25Q128_ReadStatusReg1(void);
uint8_t W25Q128_ReadStatusReg2(void);
void W25Q128_WriteEnable(void);
void W25Q128_WriteDisable(void);
void W25Q128_WaitForWriteEnd(void);
void W25Q128_EraseSector(uint32_t SectorAddr);
void W25Q128_EraseBlock(uint32_t BlockAddr);
void W25Q128_EraseChip(void);
void W25Q128_PageWrite(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);
void W25Q128_BufferWrite(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);
void W25Q128_BufferRead(uint8_t* pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead);
void W25Q128_PowerDown(void);
void W25Q128_WakeUp(void);

#endif


