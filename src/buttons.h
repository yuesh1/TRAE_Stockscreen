#pragma once
// ============================================================
//  三键输入：UP / DOWN / OK 共用 GPIO0 ADC 分压
//  电压窗口来自官方仓库 bsp_pins.h（松开约 3300mV）：
//    UP 0~150mV，DOWN 150~447mV，OK 447~1900mV
// ============================================================
#include <Arduino.h>

enum BtnKey { BTN_NONE = 0, BTN_UP, BTN_DOWN, BTN_OK };

void btnInit();

// 在 loop 里高频调用；松开瞬间返回一次该键的点击事件，其余时刻返回 BTN_NONE
BtnKey btnPoll();

// 当前是否有任意键处于按下状态（用于亮屏保持）
bool btnAnyPressed();
