// ============================================================
//  ESP32-C3 A股行情屏 —— 主固件
//  流程: WiFi 连接 → 周期抓取行情（东财/腾讯）→ 渲染 240×320 列表
//  刷新节奏用行情服务器时间戳判断交易时段，无需 NTP/RTC
// ============================================================
#include <Arduino.h>
#include "User_Setup.h"   // 必须先于 TFT_eSPI.h（USER_SETUP_LOADED 模式）
#include <TFT_eSPI.h>
#include <WiFi.h>
#include "config.h"
#include "stocks.h"
#include "ui.h"
#include "battery.h"
#include "wificonfig.h"

static TFT_eSPI tft;
static Quote quotes[WATCHLIST_COUNT];

static bool gNeedConfig = false;  // 编译期占位符且 NVS 无凭据：需要串口配网

static bool     haveData   = false;   // 是否已成功拿到过数据
static bool     lastFetchOk= false;
static uint32_t lastTs     = 0;       // 最近一次行情的服务器时间戳
static uint32_t fetchAtMs  = 0;       // 抓取成功时刻（millis），用于推算当前时间
static uint32_t lastFetchMs= 0;
static uint32_t lastClockMs= 0;

static int      gBattSoc   = -1;   // 电量 0..100，-1 = 电量计不在位
static uint32_t lastBattMs = 0;

static bool     screenOn      = true;
static uint32_t lastActivityMs= 0;
static uint32_t lastButtonMs  = 0;

// 屏幕时钟：服务器时间 + 本地经过时间（秒级足够）
static uint32_t nowEpoch() {
  if (!haveData || !lastTs) return 0;
  return lastTs + (millis() - fetchAtMs) / 1000;
}

static bool isKeyPressed() {
#if BTN_ADC_PIN >= 0
  return analogReadMilliVolts(BTN_ADC_PIN) < BTN_PRESS_MV;
#else
  return false;
#endif
}

static void setScreenOn(bool on) {
#if TFT_BL_PIN >= 0
  digitalWrite(TFT_BL_PIN, on ? HIGH : LOW);
  screenOn = on;
#else
  // 没有可控背光时保持渲染，避免逻辑息屏后无法唤醒画面。
  screenOn = true;
#endif
}

static void renderScreen() {
  if (!screenOn) return;
  uiRender(tft, quotes, WATCHLIST_COUNT,
           WiFi.status() == WL_CONNECTED, lastFetchOk, haveData,
           lastTs, gBattSoc, gNeedConfig);
  // 全屏渲染保留 lastTs 作为行情更新时间；头部时钟单独显示当前推算时间。
  uiRenderClock(tft, nowEpoch(), gBattSoc);
}

static void connectWiFi() {
  String ssid, pass;
  wifiConfigResolve(ssid, pass);   // NVS 优先，否则编译期默认值
  log_i("[WiFi] 连接 %s ...", ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), pass.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(200);
    yield();
  }
  if (WiFi.status() == WL_CONNECTED) log_i("[WiFi] 已连接");
  else log_i("[WiFi] 连接失败（USB 串口输入 wifi <SSID> <密码> 可重新配置）");
}

static void doFetch() {
  size_t ok = 0;
  uint32_t serverEpoch = 0;
  bool okF = fetchQuotes(quotes, WATCHLIST_COUNT, &ok, &serverEpoch);
  lastFetchOk = okF;
  if (ok > 0) {
    haveData = true;
    fetchAtMs = millis();
    // 时钟优先用服务器真实时间（HTTP Date 头）：休市时行情时间戳是
    // 昨日的旧值，不能当"现在"用；拿不到 Date 头才退回行情时间戳
    if (serverEpoch) {
      lastTs = serverEpoch;
      log_i("[时钟] 已用服务器时间校准 %lu", (unsigned long)serverEpoch);
    } else {
      uint32_t maxTs = 0;   // 各股时间戳最大值（停牌股时间戳偏旧）
      for (size_t i = 0; i < WATCHLIST_COUNT; i++)
        if (quotes[i].valid && quotes[i].ts > maxTs) maxTs = quotes[i].ts;
      lastTs = maxTs;
    }
  }
  renderScreen();
}

void setup() {
  delay(300);
  log_i("=== stockscreen boot ===");
  disableLoopWDT();   // 抓取可能阻塞数秒，避免任务看门狗误杀

#if TFT_BL_PIN >= 0
  pinMode(TFT_BL_PIN, OUTPUT);
  setScreenOn(true);
#endif
#if BTN_ADC_PIN >= 0
  analogReadResolution(12);
  analogSetPinAttenuation(BTN_ADC_PIN, ADC_11db);
#endif
  tft.init();
  tft.setRotation(0);       // 画面上下颠倒/镜像时改 1/2/3
  tft.fillScreen(TFT_BLACK);
  uiShowBoot(tft, "A-SHARE TICKER", "ESP32-C3");

  batteryInit();
  wifiConfigBegin();
  gNeedConfig = wifiConfigIsPlaceholder() && !wifiConfigHaveCreds();
  if (gNeedConfig) {
    log_i("[配网] 未配置 WiFi：USB 串口输入 wifi <SSID> <密码>，help 查看帮助");
  }
  connectWiFi();
  lastFetchMs = millis() - REFRESH_TRADING_MS;   // 启动后立刻抓一次
  lastActivityMs = millis();
}

void loop() {
  uint32_t now = millis();
  bool wifiOk = WiFi.status() == WL_CONNECTED;

  // 三键共用 GPIO0 ADC 分压；任意键按下都算一次屏幕操作。
  if (now - lastButtonMs >= 100) {
    lastButtonMs = now;
    if (isKeyPressed()) {
      lastActivityMs = now;
      if (!screenOn) {
        setScreenOn(true);
        renderScreen();   // 先显示息屏期间缓存的数据，不等待网络请求
        lastClockMs = now;
        log_i("[息屏] 按键唤醒");
      }
    }
  }

#if TFT_BL_PIN >= 0
  if (screenOn && now - lastActivityMs >= SCREEN_TIMEOUT_MS) {
    setScreenOn(false);
    log_i("[息屏] %lus 无操作，关闭背光", (unsigned long)SCREEN_TIMEOUT_MS / 1000);
  }
#endif

  // 串口配网：凭据变更后立即重连
  if (wifiConfigLoop()) {
    gNeedConfig = wifiConfigIsPlaceholder() && !wifiConfigHaveCreds();
    WiFi.disconnect();
    lastFetchMs = 0;
    connectWiFi();
  }

  // 未配置时不做无谓的自动重连（占位符连不上，等用户串口配网）
  if (!wifiOk && !gNeedConfig && now - lastFetchMs > 10000) {
    lastFetchMs = now;
    log_i("[WiFi] 断线，重连...");
    WiFi.disconnect();
    WiFi.reconnect();
  }

  if (wifiOk) {
    // 刷新间隔：按服务器时间判断交易时段；没有数据时用快节奏先拿到首帧
    uint32_t interval = haveData
        ? (isTradingTime(lastTs) ? REFRESH_TRADING_MS : REFRESH_IDLE_MS)
        : 4000;
    if (now - lastFetchMs >= interval) {
      lastFetchMs = now;
      doFetch();
    }
  }

  // 每 5 秒采样电量（I2C 读很快，但没必要每秒读）
  if (now - lastBattMs >= 5000) {
    lastBattMs = now;
    int soc = batterySoc();
    if (soc != gBattSoc) {
      log_i("[电池] SOC %d%%", soc);
      gBattSoc = soc;
    }
  }

  // 每秒局部刷新时钟与状态标签；息屏时行情抓取、电量采样仍继续
  if (screenOn && now - lastClockMs >= 1000) {
    lastClockMs = now;
    uiRenderClock(tft, nowEpoch(), gBattSoc);
  }

  delay(20);
}
