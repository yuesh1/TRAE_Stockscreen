#pragma once
#include "User_Setup.h"   // 必须先于 TFT_eSPI.h（USER_SETUP_LOADED 模式）
#include <TFT_eSPI.h>
#include "stocks.h"
#include "crypto.h"

// 页面：0 = A股自选，1 = 加密货币市值前五
enum UiPage { UI_PAGE_STOCK = 0, UI_PAGE_CRYPTO = 1, UI_PAGE_COUNT = 2 };

// 启动/等待画面
void uiShowBoot(TFT_eSPI& tft, const char* line1, const char* line2);

// 全屏渲染（每次抓取后调用）；battery 为电量 0..100，<0 表示不显示；
// needConfig 为 true 时显示 USB 串口配网提示
void uiRender(TFT_eSPI& tft, const Quote* quotes, size_t count,
              bool wifiOk, bool fetchOk, bool haveData, uint32_t ts, int battery,
              bool needConfig = false);

// 加密货币页全屏渲染；ts 为该页数据的更新时间（epoch）
void uiRenderCrypto(TFT_eSPI& tft, const CryptoQuote* coins, size_t count,
                    bool wifiOk, bool fetchOk, bool haveData, uint32_t ts,
                    int battery);

// 设置当前页（影响局部刷新时底部状态的画法）
void uiSetPage(UiPage page);

// 局部刷新头部时钟与底部状态（每秒调用）
void uiRenderClock(TFT_eSPI& tft, uint32_t nowEpoch, int battery);

// 绘制 UTF-8 中文（16×16 点阵，字形来自 tools/gen_font.py 生成的 stock_font.h），
// 最多画 maxChars 个字；字库中没有的字会跳过
void drawCN(TFT_eSPI& tft, int16_t x, int16_t y, const char* utf8,
            uint16_t color, int maxChars = 64);
