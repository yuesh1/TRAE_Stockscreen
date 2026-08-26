#pragma once
// ============================================================
//  WiFi 运行时配置（USB 串口 + NVS）
//  下载社区固件的用户无需改代码重编译：
//    USB 连接后打开串口（115200），输入  wifi <SSID> <密码>
//  凭据存入 NVS，断电不丢。优先级：NVS > 编译期默认值
//  （自己编译的用户仍可继续用 include/wifi.local.h）
// ============================================================
#include <Arduino.h>

// NVS 中是否已有保存的凭据
bool wifiConfigHaveCreds();

// 解析实际使用的凭据：NVS 优先，否则用编译期默认（wifi.local.h 或占位符）
void wifiConfigResolve(String& ssid, String& pass);

// 编译期默认值是否为占位符（即未配置）
bool wifiConfigIsPlaceholder();

// 初始化串口交互并打印简短使用说明（USB-Serial/JTAG）
void wifiConfigBegin();

// 轮询串口命令；凭据发生变更时返回 true（调用方应触发重连）
bool wifiConfigLoop();
