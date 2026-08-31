// ============================================================
//  HTTP / 日历公共工具实现（从 stocks.cpp 抽出，供 crypto.cpp 复用）
// ============================================================
#include "net_http.h"
#include <ctype.h>

// ---------------- 日历 ----------------

int32_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  int era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = y - era * 400;
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int)doe - 719468;
}

void civilFromDays(int32_t z, int& y, unsigned& m, unsigned& d) {
  z += 719468;
  int era = (z >= 0 ? z : z - 146096) / 146097;
  unsigned doe = z - era * 146097;
  unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  y = (int)yoe + era * 400;
  unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  unsigned mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp + (mp < 10 ? 3 : -9);
  y += (m <= 2);
}

uint32_t parseHttpDate(const char* s) {
  int d, h, mi, se, y;
  char mon[4] = {0};
  if (sscanf(s, "%*3s, %d %3s %d %d:%d:%d", &d, mon, &y, &h, &mi, &se) != 6) return 0;
  unsigned m = 0;
  for (unsigned i = 0; i < 12; i++)
    if (!memcmp(mon, "JanFebMarAprMayJunJulAugSepOctNovDec" + i * 3, 3)) { m = i + 1; break; }
  if (!m) return 0;
  return (uint32_t)((int64_t)daysFromCivil(y, m, d) * 86400 + h * 3600 + mi * 60 + se);
}

const char* findHeader(const char* h, size_t len, const char* name) {
  size_t nl = strlen(name);
  for (const char* p = h; p + nl + 1 <= h + len; p++) {
    if (p != h && p[-1] != '\n') continue;    // 只匹配行首
    bool ok = true;
    for (size_t k = 0; k < nl; k++)
      if (tolower((unsigned char)p[k]) != name[k]) { ok = false; break; }
    if (!ok) continue;
    const char* v = p + nl;
    while (v < h + len && (*v == ' ' || *v == '\t')) v++;
    return v < h + len ? v : nullptr;
  }
  return nullptr;
}

// ---------------- HTTP ----------------

int httpGet(WiFiClient& c, const char* host, uint16_t port, const char* path,
            const char* extraHdr, char* buf, size_t cap, uint32_t* serverEpoch) {
  c.setTimeout(8000);
  if (!c.connect(host, port)) return -1;
  c.print(F("GET ")); c.print(path);
  c.print(F(" HTTP/1.1\r\nHost: ")); c.print(host);
  c.print(F("\r\nConnection: close\r\nUser-Agent: Mozilla/5.0\r\n"));
  if (extraHdr) { c.print(extraHdr); c.print(F("\r\n")); }
  c.print(F("\r\n"));

  unsigned long t0 = millis();
  size_t n = 0;
  while (c.connected() || c.available()) {
    if (n >= cap - 1) break;
    int r = c.read((uint8_t*)buf + n, cap - 1 - n);
    if (r > 0) n += r;
    if (millis() - t0 > 9000) { log_i("[http] 读取超时"); break; }
  }
  c.stop();
  buf[n] = 0;
  if (strncmp(buf, "HTTP/", 5) != 0) return -1;
  if (serverEpoch) {
    *serverEpoch = 0;
    char* hend = strstr(buf, "\r\n\r\n");     // 只在响应头里找，避免扫到 body
    if (hend) {
      const char* date = findHeader(buf, (size_t)(hend - buf), "date:");
      if (date) *serverEpoch = parseHttpDate(date);
    }
  }
  return atoi(buf + 9);
}

// chunked body 原地拼接：body 由若干 "<hex长度>\r\n<数据>\r\n" 组成，
// 去掉长度行后把数据段依次前移，返回后 body 是连续明文
static void dechunk(char* body) {
  char* src = body;
  char* dst = body;
  for (;;) {
    long len = strtol(src, &src, 16);      // 分块长度（十六进制）
    if (len <= 0) break;
    while (*src && *src != '\n') src++;    // 跳过长度行余下部分（含扩展）
    if (*src == '\n') src++;
    memmove(dst, src, len);
    dst += len;
    src += len;
    while (*src == '\r' || *src == '\n') src++;   // 分块尾 CRLF
  }
  *dst = 0;
}

char* stripHeaders(char* buf) {
  char* p = strstr(buf, "\r\n\r\n");
  if (!p) return buf;
  char* body = p + 4;
  const char* te = findHeader(buf, (size_t)(p - buf), "transfer-encoding:");
  if (te && strncasecmp(te, "chunked", 7) == 0) dechunk(body);
  return body;
}
