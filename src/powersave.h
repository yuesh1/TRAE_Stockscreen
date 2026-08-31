#pragma once
// ============================================================
//  夜间深睡眠省电：可配置时段窗口内息屏后深睡，
//  RTC 定时器睡到窗口结束，任意实体键（GPIO0 拉低）随时唤醒
// ============================================================
#include <Arduino.h>
#include "User_Setup.h"   // 必须先于 TFT_eSPI.h（USER_SETUP_LOADED 模式）
#include <TFT_eSPI.h>

// 开机时调用：打印唤醒原因（深睡定时/按键/上电复位）
void powersaveBootReport();

// 本次开机的唤醒原因文本（开机早期的日志常在 USB 重枚举前丢失，
// 供主循环稍后补打一次）
const char* powersaveWakeCause();

// epoch（UTC）是否落在深睡窗口内（按北京时间判断）
bool powersaveInWindow(uint32_t epoch);

// 满足条件则进入深睡眠（函数不返回）：
//   窗口内 + 已息屏 + 无 BLE 连接。tft 用于睡前发面板睡眠命令省电
void powersaveMaybeSleep(TFT_eSPI& tft, uint32_t epoch, bool screenOn, bool bleConn);
