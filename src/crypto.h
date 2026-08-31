#pragma once
// ============================================================
//  加密货币行情：动态抓取市值前五大币（CoinGecko 主源，OKX 备用）
// ============================================================
#include <Arduino.h>

struct CryptoQuote {
  char   sym[12];    // 币种符号大写 "BTC"
  float  price;      // 最新价（USD）
  float  pct24;      // 24 小时涨跌幅 %
  double mcap;       // 市值（USD）；备用源拿不到时为 0
  bool   valid;
};

// 抓取市值排名前 maxCount 的币种（已按 config.h 的 CRYPTO_EXCLUDE 过滤稳定币）。
// 主源 CoinGecko /coins/markets（动态市值排名），失败降级 OKX 固定列表。
// 返回是否至少一只成功，成功只数写入 okCount。
bool fetchCrypto(CryptoQuote* out, size_t maxCount, size_t* okCount);
