#ifndef __rx_H
#define __rx_H

#include "main.h"
#include "spi.h"
#include "tim.h"    // htim3 声明
#include "gpio.h"
#include "usart.h"  // huart2 声明
#include "lcd.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>  // printf

// ╔══════════════════════════════════════════════════════════════╗
// ║          蓝牙命令接收与解析模块                               ║
// ║          协议格式: CMD:PARAM\n                               ║
// ╚══════════════════════════════════════════════════════════════╝

#define USART5_BUFFER_SIZE  128
#define RX_BUFFER_SIZE      2048  // 🔥 增大缓冲区以支持Logo上传（约40个LOGO_DATA命令）

// 函数声明
void RX_proc(void);
void BLE_ParseCommand(char* cmd);

// 🆕 蓝牙主动上报函数
void BLE_SendString(const char* str);
void BLE_ReportKnobDelta(int16_t delta);
void BLE_ReportButtonEvent(const char* type, const char* action);
void BLE_ReportSpeed(int16_t speed, uint8_t unit);
void BLE_ReportThrottle(uint8_t state);
void BLE_ReportUnit(uint8_t unit);  // 🆕 上报单位切换
void BLE_ReportPreset(uint8_t preset);  // 🆕 上报颜色预设切换

// 外部变量声明
extern uint16_t rx_pointer;  // 🆕 改为 uint16_t 以支持更大的缓冲区
extern u8 rx_data;
extern u8 rx_buff[RX_BUFFER_SIZE];

#endif
