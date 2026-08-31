// ============================================================
//  240×320 竖屏 UI：状态栏 + 自选股卡片列表 + 底部状态
//  A股配色习惯：红涨、绿跌
// ============================================================
#include "ui.h"
#include "stock_font.h"
#include <stdio.h>

// ---- 配色 ----
static const uint16_t C_BG    = 0x0841;   // 深藏青背景
static const uint16_t C_HEAD  = 0x18E3;   // 状态栏
static const uint16_t C_CARD  = 0x10A2;   // 卡片底
static const uint16_t C_UP    = 0xF800;   // 涨-红
static const uint16_t C_DOWN  = 0x07E0;   // 跌-绿
static const uint16_t C_FLAT  = 0xFFFF;   // 平-白
static const uint16_t C_GREY  = 0x7BEF;
static const uint16_t C_WARN  = 0xFD20;   // 橙（断网/失败）

// ---- 全局渲染状态（供局部刷新使用）----
static uint32_t g_ts      = 0;
static bool     g_wifiOk  = false;
static bool     g_fetchOk = false;
static bool     g_have    = false;
static bool     g_needCfg = false;   // 需要 USB 串口配网
static UiPage   g_page    = UI_PAGE_STOCK;
static uint32_t g_cryptoTs = 0;      // 币市页数据更新时间
static bool     g_cryptoOk = false;
static bool     g_cryptoHave = false;

static uint16_t dirColor(float v) {
  if (v > 0.001f) return C_UP;
  if (v < -0.001f) return C_DOWN;
  return C_FLAT;
}

static void fmtPrice(char* buf, size_t cap, const Quote& q) {
  if (!q.valid) strlcpy(buf, "--", cap);
  else snprintf(buf, cap, "%.2f", q.price);
}

// ---------------- 中文绘制 ----------------

void drawCN(TFT_eSPI& tft, int16_t x, int16_t y, const char* utf8,
            uint16_t color, int maxChars) {
  int drawn = 0;
  while (*utf8 && drawn < maxChars) {
    const uint8_t* p = (const uint8_t*)utf8;
    int len;
    if (p[0] < 0x80) len = 1;
    else if ((p[0] & 0xE0) == 0xC0) len = 2;
    else if ((p[0] & 0xF0) == 0xE0) len = 3;
    else len = 4;

    const CnGlyph* g = nullptr;
    for (int i = 0; i < CN_FONT_COUNT; i++) {
      int glen = strlen(CN_FONT[i].utf8);
      if (glen == len && memcmp(CN_FONT[i].utf8, p, len) == 0) {
        g = &CN_FONT[i];
        break;
      }
    }
    if (g) {
      for (int yy = 0; yy < 16; yy++)
        for (int xx = 0; xx < 16; xx++)
          if (g->rows[yy] & (1 << (15 - xx)))
            tft.drawPixel(x + xx, y + yy, color);
      x += 16;
      drawn++;
    }
    utf8 += len;
  }
}

// ---------------- 底部状态 ----------------

// 页码指示：底部中央两个圆点，实心 = 当前页
static void drawPageDots(TFT_eSPI& tft, int y) {
  for (int i = 0; i < UI_PAGE_COUNT; i++) {
    int x = 120 + (i - (UI_PAGE_COUNT - 1)) * 12 + 6 * (UI_PAGE_COUNT - 1);
    if (i == (int)g_page) tft.fillCircle(x, y + 9, 3, TFT_WHITE);
    else                  tft.drawCircle(x, y + 9, 3, C_GREY);
  }
}

static void drawFooter(TFT_eSPI& tft, uint32_t nowEpoch) {
  int y = 300;
  // 左：更新时间
  char upd[20];
  if (g_fetchOk && g_have) {
    char md[12];
    epochToMDHM(g_ts, md);
    snprintf(upd, sizeof(upd), "UPD %s", md);
    tft.setTextColor(C_GREY, C_BG);
  } else {
    strlcpy(upd, "UPD FAIL", sizeof(upd));
    tft.setTextColor(C_WARN, C_BG);
  }
  tft.drawString(upd, 6, y + 6, 1);
  drawPageDots(tft, y);

  // 右：状态标签（中文）
  const char* tag; uint16_t tagColor;
  if (g_needCfg)      { tag = "配网";  tagColor = C_WARN; }
  else if (!g_wifiOk) { tag = "断网";  tagColor = C_WARN; }
  else if (!g_have)   { tag = "失败";  tagColor = C_WARN; }
  else if (!isTradingTime(nowEpoch)) { tag = "休市"; tagColor = C_GREY; }
  else                { tag = "交易中"; tagColor = C_UP; }
  drawCN(tft, 232 - 16 * (int)strlen(tag) / 3, y, tag, tagColor, 8);
}

// 币市页底部：更新时间 + 页码 + "24H" 标签（币市全天交易）
static void drawFooterCrypto(TFT_eSPI& tft) {
  int y = 300;
  char upd[20];
  if (g_cryptoOk && g_cryptoHave) {
    char md[12];
    epochToMDHM(g_cryptoTs, md);
    snprintf(upd, sizeof(upd), "UPD %s", md);
    tft.setTextColor(C_GREY, C_BG);
  } else {
    strlcpy(upd, "UPD FAIL", sizeof(upd));
    tft.setTextColor(C_WARN, C_BG);
  }
  tft.drawString(upd, 6, y + 6, 1);
  drawPageDots(tft, y);

  const char* tag; uint16_t tagColor;
  if (g_needCfg)      { tag = "配网"; tagColor = C_WARN; }
  else if (!g_wifiOk) { tag = "断网"; tagColor = C_WARN; }
  else if (!g_cryptoHave) { tag = "失败"; tagColor = C_WARN; }
  else { tag = nullptr; tagColor = C_GREY; }
  if (tag) drawCN(tft, 232 - 16 * (int)strlen(tag) / 3, y, tag, tagColor, 8);
  else { tft.setTextColor(C_GREY, C_BG); tft.drawRightString("24H", 232, y + 3, 2); }
}

// ---------------- 头部 ----------------

static void drawHeader(TFT_eSPI& tft, uint32_t nowEpoch, int battery) {
  tft.fillRect(0, 0, 240, 26, C_HEAD);
  // WiFi 状态点
  tft.fillCircle(10, 13, 4, g_wifiOk ? C_DOWN : C_WARN);
  // 时间
  char hms[9];
  epochToHMS(nowEpoch, hms);
  tft.setTextColor(TFT_WHITE, C_HEAD);
  tft.drawString(hms, 20, 5, 2);
  // 电量（右上角；CW2017 不在位时 battery < 0 不显示）
  // 颜色与涨跌无关：≥50 绿 / 20~49 橙 / <20 红
  if (battery >= 0) {
    char bat[8];
    snprintf(bat, sizeof(bat), "%d%%", battery);
    tft.setTextColor(battery < 20 ? C_UP : (battery < 50 ? C_WARN : C_DOWN), C_HEAD);
    tft.drawRightString(bat, 232, 5, 2);
  }
}

// ---------------- 股票卡片 ----------------

static void drawCard(TFT_eSPI& tft, int x, int y, int w, int h, const Quote& q) {
  uint16_t dir = q.valid ? dirColor(q.pct) : C_GREY;
  tft.fillRoundRect(x, y, w, h, 6, C_CARD);
  tft.fillRoundRect(x, y, 4, h, 2, dir);   // 左侧涨跌色条

  char tmp[16];
  if (h >= 58) {
    // 三行布局（≤4 只）
    if (q.name[0]) drawCN(tft, x + 12, y + 2, q.name, TFT_WHITE, 8);
    else { tft.setTextColor(TFT_WHITE, C_CARD); tft.drawString(q.code, x + 12, y + 2, 2); }
    // 价格大字
    fmtPrice(tmp, sizeof(tmp), q);
    tft.setTextColor(q.valid ? TFT_WHITE : C_GREY, C_CARD);
    tft.drawString(tmp, x + 12, y + 18, 4);
    // 涨跌幅 + 涨跌额
    tft.setTextColor(dir, C_CARD);
    if (q.valid) snprintf(tmp, sizeof(tmp), "%+.2f%%", q.pct);
    else strlcpy(tmp, "--", sizeof(tmp));
    tft.drawString(tmp, x + 12, y + 44, 2);
    if (q.valid) snprintf(tmp, sizeof(tmp), "%+.2f", q.change);
    else strlcpy(tmp, "--", sizeof(tmp));
    tft.drawRightString(tmp, x + w - 12, y + 44, 2);
  } else {
    // 紧凑两行布局（5 只）
    if (q.name[0]) drawCN(tft, x + 10, y + 3, q.name, TFT_WHITE, 4);
    else { tft.setTextColor(TFT_WHITE, C_CARD); tft.drawString(q.code, x + 10, y + 3, 2); }
    fmtPrice(tmp, sizeof(tmp), q);
    tft.setTextColor(q.valid ? TFT_WHITE : C_GREY, C_CARD);
    tft.drawRightString(tmp, x + w - 10, y + 3, 2);
    tft.setTextColor(dir, C_CARD);
    if (q.valid) snprintf(tmp, sizeof(tmp), "%+.2f%%", q.pct);
    else strlcpy(tmp, "--", sizeof(tmp));
    tft.drawString(tmp, x + 10, y + 25, 2);
    if (q.valid) snprintf(tmp, sizeof(tmp), "%+.2f", q.change);
    else strlcpy(tmp, "--", sizeof(tmp));
    tft.drawRightString(tmp, x + w - 10, y + 25, 2);
  }
}

// ---------------- 加密货币卡片 ----------------

// 价格自适应小数位：BTC 十万级到 SHIB 小数级都能放下
static void fmtCryptoPrice(char* buf, size_t cap, float p) {
  if (p >= 10000)     snprintf(buf, cap, "%.0f", p);
  else if (p >= 100)  snprintf(buf, cap, "%.1f", p);
  else if (p >= 1)    snprintf(buf, cap, "%.2f", p);
  else                snprintf(buf, cap, "%.4f", p);
}

// 市值缩写 "$2.31T" / "$180.5B"；备用源没有市值时显示 "-"
static void fmtMcap(char* buf, size_t cap, double m) {
  if (m >= 1e12)      snprintf(buf, cap, "$%.2fT", m / 1e12);
  else if (m >= 1e9)  snprintf(buf, cap, "$%.1fB", m / 1e9);
  else if (m >= 1e6)  snprintf(buf, cap, "$%.0fM", m / 1e6);
  else                strlcpy(buf, "-", cap);
}

static void drawCryptoCard(TFT_eSPI& tft, int x, int y, int w, int h,
                           const CryptoQuote& q) {
  uint16_t dir = q.valid ? dirColor(q.pct24) : C_GREY;
  tft.fillRoundRect(x, y, w, h, 6, C_CARD);
  tft.fillRoundRect(x, y, 4, h, 2, dir);   // 左侧涨跌色条，红涨绿跌与 A股页一致

  char tmp[20];
  // 行1：币种符号 + 美元价
  tft.setTextColor(TFT_WHITE, C_CARD);
  tft.drawString(q.sym, x + 10, y + 3, 2);
  if (q.valid) fmtCryptoPrice(tmp, sizeof(tmp), q.price);
  else strlcpy(tmp, "--", sizeof(tmp));
  tft.setTextColor(q.valid ? TFT_WHITE : C_GREY, C_CARD);
  tft.drawRightString(tmp, x + w - 10, y + 3, 2);
  // 行2：24h 涨跌幅 + 市值
  tft.setTextColor(dir, C_CARD);
  if (q.valid) snprintf(tmp, sizeof(tmp), "%+.2f%%", q.pct24);
  else strlcpy(tmp, "--", sizeof(tmp));
  tft.drawString(tmp, x + 10, y + 25, 2);
  tft.setTextColor(C_GREY, C_CARD);
  fmtMcap(tmp, sizeof(tmp), q.valid ? q.mcap : 0);
  tft.drawRightString(tmp, x + w - 10, y + 25, 2);
}

// ---------------- 对外接口 ----------------

void uiShowBoot(TFT_eSPI& tft, const char* line1, const char* line2) {
  tft.fillScreen(C_BG);
  tft.setTextColor(TFT_WHITE, C_BG);
  tft.drawString(line1, (240 - strlen(line1) * 16) / 2, 120, 2);
  tft.setTextColor(C_GREY, C_BG);
  tft.drawString(line2, (240 - strlen(line2) * 16) / 2, 150, 2);
}

void uiRender(TFT_eSPI& tft, const Quote* quotes, size_t count,
              bool wifiOk, bool fetchOk, bool haveData, uint32_t ts, int battery,
              bool needConfig) {
  g_ts = ts; g_wifiOk = wifiOk; g_fetchOk = fetchOk; g_have = haveData;
  g_needCfg = needConfig;

  tft.fillScreen(C_BG);
  drawHeader(tft, g_ts, battery);

  if (!haveData && !fetchOk) {
    if (g_needCfg) {
      // 配网提示：请用USB串口配网（字库含 请/用/串/口/配/网）
      // 整行 9 字符 × 16px = 144px，起始 x=48 居中
      tft.setTextColor(TFT_WHITE, C_BG);
      tft.drawString("A-SHARE TICKER", (240 - 13 * 16) / 2, 96, 2);
      drawCN(tft, 48, 130, "请用", TFT_WHITE, 4);
      tft.drawString("USB", 48 + 2 * 16, 130, 2);
      drawCN(tft, 48 + 5 * 16, 130, "串口配网", TFT_WHITE, 8);
      tft.setTextColor(C_GREY, C_BG);
      tft.drawString("wifi <SSID> <PASS>", (240 - 18 * 8) / 2, 164, 1);
      tft.drawString("baud 115200", (240 - 11 * 8) / 2, 186, 1);
    } else {
      uiShowBoot(tft, "NO DATA", wifiOk ? "waiting..." : "WIFI OFFLINE");
    }
    return;
  }

  int rowH = count <= 4 ? 60 : 50;   // 5 只时卡片区到 296px，底部状态栏从 300px 起
  int gap  = 4;
  for (size_t i = 0; i < count; i++) {
    int y = 30 + i * (rowH + gap);
    drawCard(tft, 4, y, 232, rowH, quotes[i]);
  }
  drawFooter(tft, g_ts);
}

void uiRenderCrypto(TFT_eSPI& tft, const CryptoQuote* coins, size_t count,
                    bool wifiOk, bool fetchOk, bool haveData, uint32_t ts,
                    int battery) {
  g_wifiOk = wifiOk; g_cryptoOk = fetchOk; g_cryptoHave = haveData;
  g_cryptoTs = ts;

  tft.fillScreen(C_BG);
  drawHeader(tft, g_ts, battery);

  if (!haveData) {
    uiShowBoot(tft, "CRYPTO TOP5", wifiOk ? "loading..." : "WIFI OFFLINE");
    drawHeader(tft, g_ts, battery);   // uiShowBoot 清屏后补回状态栏
    tft.fillRect(0, 300, 240, 20, C_BG);
    drawFooterCrypto(tft);
    return;
  }

  int rowH = count <= 4 ? 60 : 50;
  int gap  = 4;
  for (size_t i = 0; i < count; i++) {
    int y = 30 + i * (rowH + gap);
    drawCryptoCard(tft, 4, y, 232, rowH, coins[i]);
  }
  drawFooterCrypto(tft);
}

void uiSetPage(UiPage page) { g_page = page; }

void uiRenderClock(TFT_eSPI& tft, uint32_t nowEpoch, int battery) {
  drawHeader(tft, nowEpoch, battery);
  // 状态标签可能变化（如进入/退出交易时段），一并刷新
  tft.fillRect(0, 300, 240, 20, C_BG);
  if (g_page == UI_PAGE_CRYPTO) drawFooterCrypto(tft);
  else                          drawFooter(tft, nowEpoch);
}
