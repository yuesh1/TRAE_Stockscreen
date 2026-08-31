// ============================================================
//  A股行情抓取与解析
//  主源:  东方财富批量接口  https://push2.eastmoney.com  (JSON, UTF-8)
//  备用:  腾讯行情          http://qt.gtimg.cn            (GBK 文本,
//         只提取 ASCII 数字字段，无需转码)
// ============================================================
#include "stocks.h"
#include "config.h"
#include "net_http.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <ctype.h>

#define EM_HOST "push2.eastmoney.com"
#define EM_UT   "fa5fd1943c7b386f172d6893dbfba10b"
#define QQ_HOST "qt.gtimg.cn"

// ---------------- 工具函数 ----------------

static bool isDigits(const char* s, size_t n) {
  for (size_t i = 0; i < n; i++)
    if (s[i] < '0' || s[i] > '9') return false;
  return true;
}

// "20260821161449"（北京时间）→ epoch
static uint32_t parseYmdHms(const char* s) {
  int y, mo, d, h, mi, se;
  if (sscanf(s, "%4d%2d%2d%2d%2d%2d", &y, &mo, &d, &h, &mi, &se) != 6) return 0;
  return (uint32_t)((int64_t)daysFromCivil(y, mo, d) * 86400
                    + h * 3600 + mi * 60 + se - 8 * 3600);
}

// HTTP 请求 / 响应头解析 / 日历换算已抽到 net_http.cpp（与 crypto.cpp 共用）

// ---------------- 东财（主源） ----------------

static void buildSecids(char* dst, size_t cap) {
  dst[0] = 0;
  for (size_t i = 0; i < WATCHLIST_COUNT; i++) {
    const char* code = WATCHLIST[i];
    if (i) strlcat(dst, ",", cap);
    strlcat(dst, code[0] == '6' ? "1." : "0.", cap);   // 6xx→沪(1)，其余按深(0)
    strlcat(dst, code, cap);
  }
}

static bool parseEastmoney(const char* body, Quote* out, size_t maxCount) {
  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;
  JsonArray diff = doc["data"]["diff"];
  size_t n = diff.size() < maxCount ? diff.size() : maxCount;
  for (size_t i = 0; i < n; i++) {
    JsonObject it = diff[i];
    strlcpy(out[i].code, it["f12"] | "", sizeof(out[i].code));
    strlcpy(out[i].name, it["f14"] | "", sizeof(out[i].name));
    out[i].valid = false;
    out[i].ts    = 0;
    // 停牌时 f2/f18 是 "-" 字符串，只有浮点数才算有效行情
    if (it["f2"].is<float>() && it["f18"].is<float>()) {
      out[i].price     = it["f2"].as<float>();
      out[i].prevClose = it["f18"].as<float>();
      out[i].change    = it["f4"].is<float>() ? it["f4"].as<float>()
                                              : out[i].price - out[i].prevClose;
      out[i].pct       = it["f3"].is<float>() ? it["f3"].as<float>() : 0;
      out[i].ts        = it["f124"].is<long>() ? it["f124"].as<long>() : 0;
      out[i].valid     = true;
    }
  }
  return n > 0;
}

// ---------------- 腾讯（备用源） ----------------

static bool parseTencent(const char* body, Quote* out, size_t maxCount) {
  size_t ok = 0;
  for (size_t i = 0; i < maxCount && i < WATCHLIST_COUNT; i++) {
    const char* code = WATCHLIST[i];
    char mark[20];
    snprintf(mark, sizeof(mark), "v_%s%s=\"", code[0] == '6' ? "sh" : "sz", code);
    const char* seg = strstr(body, mark);
    if (!seg) continue;
    const char* p   = seg + strlen(mark);
    const char* end = strchr(p, '"');
    if (!end) continue;

    Quote& q = out[i];
    strlcpy(q.code, code, sizeof(q.code));
    // 腾讯源是 GBK，名称无法解析，用 config.h 里的映射表兜底
    strlcpy(q.name, i < WATCH_NAMES_COUNT ? WATCH_NAMES[i] : "", sizeof(q.name));
    q.valid = false;
    q.ts = 0;
    q.price = q.prevClose = q.change = q.pct = 0;

    // 价格/昨收：段内第 1、2 个带小数点的纯 ASCII 字段
    //（字段[0]=市场标记、字段[1]=GBK 名称、字段[2]=代码 都会被过滤掉）
    float nums[2] = {0, 0};
    int nNum = 0;
    const char* f = p;
    while (f < end) {
      const char* fs = strchr(f, '~');
      if (!fs || fs > end) fs = end;
      if (fs > f && nNum < 2) {
        bool ascii = true, dot = false;
        for (const char* ch = f; ch < fs; ch++) {
          if ((uint8_t)*ch >= 0x80) { ascii = false; break; }
          if (*ch == '.') dot = true;
        }
        if (ascii && dot) nums[nNum++] = atof(f);
      }
      if (fs >= end) break;
      f = fs + 1;
    }
    q.price     = nums[0];
    q.prevClose = nums[1];

    // 时间戳 / 涨跌额 / 涨跌幅：定位 '~' + 14 位数字（YYYYMMDDHHMMSS），
    // 其后两个字段依次是涨跌额、涨跌幅
    for (const char* t = p; t + 15 <= end; t++) {
      if (*t == '~' && isDigits(t + 1, 14)) {
        q.ts = parseYmdHms(t + 1);
        const char* a = t + 15;              // 时间戳后的 '~'
        if (a < end && *a == '~') {
          const char* b = strchr(a + 1, '~');
          if (b && b < end) {
            q.change = atof(a + 1);
            const char* cc = strchr(b + 1, '~');
            if (cc && cc < end) q.pct = atof(b + 1);
          }
        }
        break;
      }
    }
    if (q.price > 0 && q.ts > 0) {
      q.change = q.change != 0 ? q.change : q.price - q.prevClose;
      q.pct    = q.pct    != 0 ? q.pct
                               : (q.prevClose > 0 ? (q.price - q.prevClose) / q.prevClose * 100 : 0);
      q.valid = true;
      ok++;
    }
  }
  return ok > 0;
}

// ---------------- 对外接口 ----------------

bool fetchQuotes(Quote* out, size_t maxCount, size_t* okCount, uint32_t* serverEpoch) {
  static char buf[2400];    // 全局缓冲：ESP32-C3 任务栈只有 8KB，避免栈溢出
  *okCount = 0;
  if (serverEpoch) *serverEpoch = 0;
  uint32_t se = 0;          // 本次请求的 HTTP Date（UTC epoch）
  uint32_t t0 = millis();
  bool parsed = false;
  log_i("[行情] 开始抓取，free heap %ukB", ESP.getFreeHeap() / 1024);

  // 1) 东财批量接口。
  // C3 + core 3.0.7 的 mbedTLS 有 PADLOCK 数据对齐 bug（"Input data should
  // be aligned"），TLS 握手必败；失败一次后熔断 10 分钟直接走腾讯源，
  // 避免每轮刷新都白等一次握手
  static uint32_t emSkipUntil = 0;
  if (millis() < emSkipUntil) {
    log_i("[行情] 东财熔断中，直接走腾讯源");
  } else {
    char secids[64];
    buildSecids(secids, sizeof(secids));
    char path[180];
    snprintf(path, sizeof(path),
             "/api/qt/ulist.np/get?secids=%s"
             "&fields=f2,f3,f4,f12,f14,f18,f124&fltt=2&invt=2&ut=%s",
             secids, EM_UT);

    WiFiClientSecure c;
    c.setInsecure();       // 公开行情数据 + 个人设备，免证书校验
    int code = httpGet(c, EM_HOST, 443, path, nullptr, buf, sizeof(buf), &se);
    if (code < 0) {
      char err[96] = {0};
      c.lastError(err, sizeof(err));
      log_i("[东财] TLS: %s", err[0] ? err : "(连接失败)");
    }
    log_i("[东财] HTTP %d (%lums)", code, millis() - t0);
    if (code == 200 && serverEpoch) *serverEpoch = se;
    parsed = (code == 200) && parseEastmoney(stripHeaders(buf), out, maxCount);
    if (!parsed) {
      emSkipUntil = millis() + 10UL * 60 * 1000;
      log_i("[行情] 东财失败，熔断 10 分钟，降级腾讯接口...");
    }
  }

  // 2) 腾讯（HTTP 明文，无 TLS 开销）
  if (!parsed) {
    char q[96];
    q[0] = 0;
    for (size_t i = 0; i < WATCHLIST_COUNT; i++) {
      if (i) strlcat(q, ",", sizeof(q));
      strlcat(q, WATCHLIST[i][0] == '6' ? "sh" : "sz", sizeof(q));
      strlcat(q, WATCHLIST[i], sizeof(q));
    }
    char path[128];
    snprintf(path, sizeof(path), "/q=%s", q);

    WiFiClient c;
    int code = httpGet(c, QQ_HOST, 80, path, "Referer: https://gu.qq.com/", buf, sizeof(buf), &se);
    log_i("[腾讯] HTTP %d (%lums)", code, millis() - t0);
    if (code == 200) {
      if (serverEpoch) *serverEpoch = se;
      parseTencent(stripHeaders(buf), out, maxCount);
    }
  }

  for (size_t i = 0; i < maxCount; i++)
    if (out[i].valid) (*okCount)++;
  log_i("[行情] 成功 %u/%u 只，耗时 %lums",
        (unsigned)*okCount, (unsigned)maxCount, millis() - t0);
  return *okCount > 0;
}

// ---------------- 时间接口 ----------------

bool isTradingTime(uint32_t epoch) {
  if (!epoch) return true;      // 还没有服务器时间时按交易时段刷新，尽快拿到首次数据
  uint32_t beijing = epoch + 8 * 3600;
  int32_t days = beijing / 86400;
  uint32_t sod  = beijing % 86400;
  int wd = (days + 4) % 7;      // 0=周日 ... 6=周六
  if (wd < 1 || wd > 5) return false;
  uint32_t mins = sod / 60;
  return (mins >= 9 * 60 + 15 && mins <= 11 * 60 + 35) ||
         (mins >= 12 * 60 + 55 && mins <= 15 * 60 + 5);
}

void epochToHMS(uint32_t epoch, char* buf) {
  if (!epoch) { strcpy(buf, "--:--:--"); return; }
  uint32_t sod = (epoch + 8 * 3600) % 86400;
  sprintf(buf, "%02u:%02u:%02u", sod / 3600, sod % 3600 / 60, sod % 60);
}

void epochToMDHM(uint32_t epoch, char* buf) {
  if (!epoch) { strcpy(buf, "--/-- --:--"); return; }
  uint32_t beijing = epoch + 8 * 3600;
  int y; unsigned mo, d;
  civilFromDays(beijing / 86400, y, mo, d);
  sprintf(buf, "%02u-%02u %02u:%02u", mo, d, beijing % 86400 / 3600, beijing % 86400 % 3600 / 60);
}
