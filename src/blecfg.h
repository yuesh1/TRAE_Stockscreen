#pragma once
// ============================================================
//  BLE 配网 + 行情推送（NimBLE GATT 服务）
//  手机用 nRF Connect / LightBlue 等通用 BLE 工具：
//    - 写特征（FFF1）写入文本命令，与 USB 串口相同：wifi <SSID> <密码>
//    - 通知特征（FFF2）订阅后接收命令回应与每轮行情推送
// ============================================================
#include <Arduino.h>

void bleBegin();                       // 初始化并开始广播（BLE_ENABLE=0 时空实现）
bool bleConnected();                   // 是否有手机连接（深睡眠前检查）

// 取出手机写入的一行命令；无命令返回 false（在主循环里执行，避免阻塞 BLE 栈）
bool bleTakeCommand(String& line);

// 向已订阅的手机推送一段文本（自动按 MTU 分片）
void bleNotify(const char* text);
