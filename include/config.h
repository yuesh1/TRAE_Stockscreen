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
static const char* WATCHLIST[] = {"603986", "002008", "159583"};
#define WATCHLIST_COUNT (sizeof(WATCHLIST) / sizeof(WATCHLIST[0]))

// 与 WATCHLIST 一一对应的中文名称。
// 东财主源会返回名称；腾讯备用源是 GBK 无法解析，用这张表兜底（改自选股时同步改）
static const char* WATCH_NAMES[] = {"兆易创新", "大族激光", "富国通信"};
#define WATCH_NAMES_COUNT (sizeof(WATCH_NAMES) / sizeof(WATCH_NAMES[0]))

// ---------- 加密货币页（UP/DOWN 键在 A股页与币市页之间切换）----------
#define CRYPTO_ENABLE     1        // 0 = 关闭币市页
#define CRYPTO_COUNT      5        // 显示市值排名前几的币（最多 5 只显示效果最好）
#define CRYPTO_REFRESH_MS 45000    // 刷新间隔（CoinGecko 免费接口有频率限制，勿低于 30s）
// 主源 CoinGecko：动态按市值降序取前几名（HTTPS）。
// 大陆网络访问不到时可改成可用的镜像域名（路径需兼容 /api/v3）
#define CRYPTO_CG_HOST  "api.coingecko.com"
// 备用源 OKX：固定列表逐个查询（无市值数据）。大陆网络可改 "aws.okx.com"
#define CRYPTO_OKX_HOST "www.okx.com"
// 市值排名里跳过的稳定币/包装币（用户要看的是"流动"，这些不动）
#define CRYPTO_EXCLUDE  {"USDT", "USDC", "STETH", "WBTC", "WETH", "WSTETH", "BSC-USD"}
// CoinGecko 不可用时 OKX 备用固定列表（按当前市值排名手工维护）
#define CRYPTO_FALLBACK_LIST {"BTC", "ETH", "XRP", "BNB", "SOL"}

// ---------- 夜间深睡眠省电 ----------
// 北京时间窗口内且已息屏、无 BLE 连接时进入深睡眠（<0.1mA），
// RTC 定时睡到窗口结束自动唤醒；按任意实体键随时唤醒（唤醒=重启，需数秒联网）
#define DEEP_SLEEP_ENABLE    1                  // 0 = 关闭
#define DEEP_SLEEP_START_MIN (23 * 60 + 30)     // 23:30 开始
#define DEEP_SLEEP_END_MIN   (7 * 60)           // 07:00 结束（可跨零点）

// ---------- BLE 配网 / 行情推送 ----------
// 手机装 nRF Connect / LightBlue，连接设备后：
//   向特征 FFF1 写文本 "wifi <SSID> <密码>" 即可配网（与串口命令一致）
//   订阅特征 FFF2 可收到命令回应和每轮行情推送
#define BLE_ENABLE      1                  // 0 = 关闭（可省约 50KB 内存）
#define BLE_DEVICE_NAME "StockScreen"      // 手机扫描到的设备名

// ---------- 中文字库字符集（配合 tools/gen_font.py 使用）----------
// 把自选股中文名称里出现的字都写进来；改了之后运行：
//   python3 tools/gen_font.py
// 重新生成 include/stock_font.h
#define CN_CHARSET "兆易创新大族激光富国通信交易中断网失败休市股票请用串口配"

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

// ---------- 按键与自动息屏 ----------
// TRAE AI 通行证三键共用 GPIO0 ADC 分压，电压窗口来自官方仓库 bsp_pins.h：
//   UP 0~150mV / DOWN 150~447mV / OK 447~1900mV / 松开约 3300mV
// UP/DOWN 切换页面（A股 ⇄ 币市），OK 立即刷新当前页，任意键唤醒屏幕
#define BTN_ADC_PIN        0
#define BTN_UP_MAX_MV      150
#define BTN_DOWN_MAX_MV    447
#define BTN_PRESS_MV       1900
#define SCREEN_TIMEOUT_MS  20000

// ---------- 刷新频率 ----------
#define REFRESH_TRADING_MS 4000    // 交易时段刷新间隔
#define REFRESH_IDLE_MS    60000   // 非交易时段（休市）刷新间隔
