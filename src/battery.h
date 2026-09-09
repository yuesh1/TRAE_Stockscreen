#pragma once
// ============================================================
//  CW2017 电量计（I2C 0x63，与音频 codec 共用总线）
//  芯片不在位时 soc() 返回 -1，UI 自动隐藏电量显示
// ============================================================

#include <Arduino.h>

void batteryInit();   // Wire.begin + 唤醒芯片（确保退出睡眠/复位态）
int  batterySoc();    // 剩余电量 0..100，读失败返回 -1
int  batteryMv();     // 电池电压 mV，读失败返回 -1（备用，UI 暂未使用）

// 诊断：把 VERSION/CONFIG/SOC/电压与 I2C 错误码写入 out（串口/BLE 的 bat 命令用）
void batteryDiag(String& out);
