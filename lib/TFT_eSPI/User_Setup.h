// ============================================================
//  TFT_eSPI 配置（platformio.ini 里 -DUSER_SETUP_LOADED=1 启用）
//  引脚定义来自 include/config.h
// ============================================================
#pragma once
#include "config.h"

// 2.0寸 240x320 屏幕常见驱动是 ST7789；
// 如果 test_screen 确认你的屏是 ILI9341，改成 ILI9341_DRIVER（其他不用动）
#define ST7789_DRIVER

// ST7789P3 面板必须反色显示（官方 bsp_pins.h: BSP_LCD_INVERT_COLOR=1），
// 不加这行画面会呈负片
#define TFT_INVERSION_ON

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_MISO -1     // 屏幕只写不读
#define TOUCH_CS -1     // 无触摸屏

// 花屏时把 27 降到 20（MHz）
#define SPI_FREQUENCY 27000000

// 本面板红蓝通道反序（涨色 0xF800 显示成蓝色），已启用 BGR 顺序
#define TFT_RGB_ORDER TFT_BGR

// arduino-esp32 3.x：让 TFT_eSPI 用 SPISettings 事务（SPI_FREQUENCY 才生效，
// 否则 spi 传输走总线默认低速，满屏填充会慢到触发看门狗）
#define SUPPORT_TRANSACTIONS

// 背光：config.h 中 TFT_BL_PIN >= 0 时由 TFT_eSPI 初始化时自动点亮
#if TFT_BL_PIN >= 0
#define TFT_BL TFT_BL_PIN
#define TFT_BACKLIGHT_ON HIGH
#endif

// 用到的字体
#define LOAD_GLCD    // Font 1: 8px 小字
#define LOAD_FONT2   // Font 2: 16px
#define LOAD_FONT4   // Font 4: 26px 大数字
