#pragma once
#include <Arduino.h>

// 单只股票行情
struct Quote {
  char     code[8];      // 代码 "600519"
  char     name[24];     // 中文名称（UTF-8；东财源返回，腾讯源用 config.h 名称表兜底）
  float    price;        // 最新价
  float    prevClose;    // 昨收
  float    change;       // 涨跌额
  float    pct;          // 涨跌幅 %
  uint32_t ts;           // 行情时间戳（epoch，服务器时间）
  bool     valid;        // 停牌/解析失败时为 false
};

// 抓取自选股行情到 out（最多 maxCount 只），成功只数写入 okCount。
// 优先东财批量接口，失败自动降级腾讯。返回是否至少有一只成功。
// serverEpoch（可为 NULL）：HTTP 200 时把响应头 Date 解析成 UTC epoch 写入。
// 行情时间戳在休市时是旧值，做时钟应以 serverEpoch 为准。
bool fetchQuotes(Quote* out, size_t maxCount, size_t* okCount, uint32_t* serverEpoch);

// 时间戳是否处于 A股交易时段（北京时间，含集合竞价与尾盘收尾）
bool isTradingTime(uint32_t epoch);

// epoch → 北京时间 "HH:MM:SS"（buf ≥ 9 字节）；epoch 为 0 时输出 "--:--:--"
void epochToHMS(uint32_t epoch, char* buf);

// epoch → 北京时间 "MM-DD HH:MM"（buf ≥ 12 字节）
void epochToMDHM(uint32_t epoch, char* buf);
