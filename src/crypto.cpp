// ============================================================
//  加密货币行情抓取
//  主源: CoinGecko /api/v3/coins/markets —— 按市值降序返回，动态跟踪
//        排名变化（HTTPS，大陆网络可能需要在 config.h 换镜像域名）
//  备用: OKX /api/v5/market/ticker —— 固定列表逐个查询，只有价格与
//        24h 开盘价（市值显示 "-"）
//  两源都是公开只读行情接口，与东财源一样用 setInsecure 免证书校验
// ============================================================
#include "crypto.h"
#include "config.h"
#include "net_http.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// 响应缓冲：CoinGecko 8 币全字段约 7KB，取 12KB 冗余；用堆避免占静态内存
#define CRYPTO_BUF_SIZE 12288

// ---------------- CoinGecko（主源，动态市值排名） ----------------

static bool isExcluded(const char* sym) {
  static const char* ex[] = CRYPTO_EXCLUDE;
  for (size_t i = 0; i < sizeof(ex) / sizeof(ex[0]); i++)
    if (strcasecmp(sym, ex[i]) == 0) return true;
  return false;
}

static bool fetchCoinGecko(char* buf, CryptoQuote* out, size_t maxCount, size_t* okCount) {
  // 多拉几只，过滤稳定币后仍能凑满 maxCount
  char path[160];
  snprintf(path, sizeof(path),
           "/api/v3/coins/markets?vs_currency=usd&order=market_cap_desc"
           "&per_page=%u&page=1&sparkline=false",
           (unsigned)(maxCount + 4));

  WiFiClientSecure c;
  c.setInsecure();
  uint32_t t0 = millis();
  int code = httpGet(c, CRYPTO_CG_HOST, 443, path,
                     "Accept: application/json", buf, CRYPTO_BUF_SIZE, nullptr);
  if (code < 0) {
    char err[96] = {0};
    c.lastError(err, sizeof(err));
    log_i("[CoinGecko] TLS: %s", err[0] ? err : "(连接失败)");
  }
  log_i("[CoinGecko] HTTP %d (%lums)", code, millis() - t0);
  if (code != 200) return false;

  // 只保留用得到的 4 个字段，避免整包 JSON 撑爆内存
  JsonDocument filter;
  filter[0]["symbol"] = true;
  filter[0]["current_price"] = true;
  filter[0]["price_change_percentage_24h"] = true;
  filter[0]["market_cap"] = true;

  JsonDocument doc;
  if (deserializeJson(doc, stripHeaders(buf), DeserializationOption::Filter(filter)))
    return false;

  size_t n = 0;
  for (JsonObject it : doc.as<JsonArray>()) {
    if (n >= maxCount) break;
    const char* sym = it["symbol"] | "";
    if (!sym[0] || isExcluded(sym)) continue;
    CryptoQuote& q = out[n];
    strlcpy(q.sym, sym, sizeof(q.sym));
    for (char* p = q.sym; *p; p++) *p = toupper((unsigned char)*p);
    q.price = it["current_price"] | 0.0f;
    q.pct24 = it["price_change_percentage_24h"] | 0.0f;
    q.mcap  = it["market_cap"] | 0.0;
    q.valid = q.price > 0;
    if (q.valid) n++;
  }
  *okCount = n;
  return n > 0;
}

// ---------------- OKX（备用源，固定列表） ----------------

static bool fetchOkxOne(char* buf, const char* sym, CryptoQuote& q) {
  char path[64];
  snprintf(path, sizeof(path), "/api/v5/market/ticker?instId=%s-USDT", sym);

  WiFiClientSecure c;
  c.setInsecure();
  int code = httpGet(c, CRYPTO_OKX_HOST, 443, path,
                     "Accept: application/json", buf, CRYPTO_BUF_SIZE, nullptr);
  if (code != 200) {
    log_i("[OKX] %s HTTP %d", sym, code);
    return false;
  }
  JsonDocument doc;
  if (deserializeJson(doc, stripHeaders(buf))) return false;
  JsonObject d = doc["data"][0];
  float last = atof(d["last"] | "0");
  float open = atof(d["open24h"] | "0");   // OKX 数值都是字符串
  if (last <= 0) return false;

  strlcpy(q.sym, sym, sizeof(q.sym));
  q.price = last;
  q.pct24 = open > 0 ? (last - open) / open * 100 : 0;
  q.mcap  = 0;      // 单币行情接口无市值
  q.valid = true;
  return true;
}

static bool fetchOkx(char* buf, CryptoQuote* out, size_t maxCount, size_t* okCount) {
  static const char* list[] = CRYPTO_FALLBACK_LIST;
  size_t total = sizeof(list) / sizeof(list[0]);
  size_t n = 0;
  for (size_t i = 0; i < total && n < maxCount; i++)
    if (fetchOkxOne(buf, list[i], out[n])) n++;
  *okCount = n;
  return n > 0;
}

// ---------------- 对外接口 ----------------

bool fetchCrypto(CryptoQuote* out, size_t maxCount, size_t* okCount) {
  *okCount = 0;
  char* buf = (char*)malloc(CRYPTO_BUF_SIZE);
  if (!buf) {
    log_e("[币市] 缓冲分配失败，free heap %ukB", ESP.getFreeHeap() / 1024);
    return false;
  }
  uint32_t t0 = millis();
  memset(out, 0, sizeof(CryptoQuote) * maxCount);

  // CoinGecko 失败后熔断 10 分钟直接走 OKX，与东财源同样的策略
  static uint32_t cgSkipUntil = 0;
  bool ok = false;
  if (millis() >= cgSkipUntil) {
    ok = fetchCoinGecko(buf, out, maxCount, okCount);
    if (!ok) {
      cgSkipUntil = millis() + 10UL * 60 * 1000;
      log_i("[币市] CoinGecko 失败，熔断 10 分钟，降级 OKX...");
    }
  } else {
    log_i("[币市] CoinGecko 熔断中，直接走 OKX");
  }
  if (!ok) ok = fetchOkx(buf, out, maxCount, okCount);

  free(buf);
  log_i("[币市] 成功 %u/%u 只，耗时 %lums",
        (unsigned)*okCount, (unsigned)maxCount, millis() - t0);
  return ok;
}
