#pragma once
// ============================================================
//  HTTP / 日历公共工具（stocks.cpp 与 crypto.cpp 共用）
// ============================================================
#include <Arduino.h>
#include <WiFiClient.h>

// 日历换算（Howard Hinnant civil-from-days 算法）
int32_t daysFromCivil(int y, unsigned m, unsigned d);
void    civilFromDays(int32_t z, int& y, unsigned& m, unsigned& d);

// "Fri, 22 Aug 2026 08:14:53 GMT" → UTC epoch；解析失败返回 0
uint32_t parseHttpDate(const char* s);

// 在长度为 len 的响应头里找 name:（大小写不敏感，name 用小写传入），返回值指针
const char* findHeader(const char* h, size_t len, const char* name);

// 通用 GET：整体读进 buf，返回 HTTP 状态码；网络失败返回 -1。
// serverEpoch（可为 NULL）：HTTP 200 时把响应头 Date 解析成 UTC epoch 写入
int httpGet(WiFiClient& c, const char* host, uint16_t port, const char* path,
            const char* extraHdr, char* buf, size_t cap, uint32_t* serverEpoch);

// buf 原地去掉响应头，指向 body；若响应头声明 Transfer-Encoding: chunked，
// 原地拼接各分块（CoinGecko 等 API 常用 chunked 编码）
char* stripHeaders(char* buf);
