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

  // 右：状态标签（中文）
  const char* tag; uint16_t tagColor;
  if (!g_wifiOk)      { tag = "断网";  tagColor = C_WARN; }
  else if (!g_have)   { tag = "失败";  tagColor = C_WARN; }
  else if (!isTradingTime(nowEpoch)) { tag = "休市"; tagColor = C_GREY; }
  else                { tag = "交易中"; tagColor = C_UP; }
  drawCN(tft, 232 - 16 * (int)strlen(tag) / 3, y, tag, tagColor, 8);
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

// ---------------- 对外接口 ----------------

void uiShowBoot(TFT_eSPI& tft, const char* line1, const char* line2) {
  tft.fillScreen(C_BG);
  tft.setTextColor(TFT_WHITE, C_BG);
  tft.drawString(line1, (240 - strlen(line1) * 16) / 2, 120, 2);
  tft.setTextColor(C_GREY, C_BG);
  tft.drawString(line2, (240 - strlen(line2) * 16) / 2, 150, 2);
}

void uiRender(TFT_eSPI& tft, const Quote* quotes, size_t count,
              bool wifiOk, bool fetchOk, bool haveData, uint32_t ts, int battery) {
  g_ts = ts; g_wifiOk = wifiOk; g_fetchOk = fetchOk; g_have = haveData;

  tft.fillScreen(C_BG);
  drawHeader(tft, g_ts, battery);

  if (!haveData && !fetchOk) {
    uiShowBoot(tft, "NO DATA", wifiOk ? "waiting..." : "WIFI OFFLINE");
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

void uiRenderClock(TFT_eSPI& tft, uint32_t nowEpoch, int battery) {
  drawHeader(tft, nowEpoch, battery);
  // 状态标签可能变化（如进入/退出交易时段），一并刷新
  tft.fillRect(0, 300, 240, 20, C_BG);
  drawFooter(tft, nowEpoch);
}
