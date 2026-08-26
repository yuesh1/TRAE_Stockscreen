#pragma once
// ============================================================
//  用户配置 —— 所有需要修改的地方都在这一个文件里
// ============================================================

// ---------- WiFi ----------
// 本机凭据放在 include/wifi.local.h（已被 .gitignore 忽略，不会上传到 GitHub）：
//    #define WIFI_SSID "你的WiFi名"
//    #define WIFI_PASS "你的WiFi密码"
// 没有该文件时用下面的占位符（别人 clone 仓库后改这里）
#if __has_include("wifi.local.h")
#include "wifi.local.h"
#else
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASS "YOUR_WIFI_PASSWORD"
#endif

// ---------- 自选股（A股 6 位代码，建议 3~4 只显示效果最好，最多 5 只）----------
// 市场前缀自动推导：6xx → 上海，0xx/3xx → 深圳
static const char* WATCHLIST[] = {"600104", "001696", "601179"};
#define WATCHLIST_COUNT (sizeof(WATCHLIST) / sizeof(WATCHLIST[0]))

// 与 WATCHLIST 一一对应的中文名称。
// 东财主源会返回名称；腾讯备用源是 GBK 无法解析，用这张表兜底（改自选股时同步改）
static const char* WATCH_NAMES[] = {"上汽集团", "宗申动力", "中国西电"};
#define WATCH_NAMES_COUNT (sizeof(WATCH_NAMES) / sizeof(WATCH_NAMES[0]))

// ---------- 中文字库字符集（配合 tools/gen_font.py 使用）----------
// 把自选股中文名称里出现的字都写进来；改了之后运行：
//   python3 tools/gen_font.py
// 重新生成 include/stock_font.h
#define CN_CHARSET "上汽集团宗申动力中国西电交易中断网失败休市股票请用串口配"

// ---------- I2C（电池电量计 CW2017，与音频 codec 共用总线） ----------
#define BSP_I2C_SDA 10
#define BSP_I2C_SCL 7

// ---------- 屏幕引脚 ----------
// TRAE AI 通行证（FoloToy）官方引脚，来源 folotoy/ai-passport 仓库 bsp_pins.h：
//   屏幕 ST7789P3 240x320，RST 未接 MCU（硬接 3.3V，走软件复位），背光 GPIO21
#define TFT_SCLK 8
#define TFT_MOSI 9
#define TFT_CS   1
#define TFT_DC   20
#define TFT_RST  -1   // 官方板复位脚硬接 3.3V，不接 MCU
#define TFT_BL_PIN 21 // 背光控制引脚，没有则填 -1（常亮）

// ---------- 刷新频率 ----------
#define REFRESH_TRADING_MS 4000    // 交易时段刷新间隔
#define REFRESH_IDLE_MS    60000   // 非交易时段（休市）刷新间隔
