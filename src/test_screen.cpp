// ============================================================
//  屏幕测试固件 v2（TRAE AI 通行证官方引脚专用）
//  两种 SPI 实现轮流跑同一套满屏色块测试：
//    Cycle A —— Arduino SPI 库（SPI.begin(8,-1,9,1)）
//    Cycle B —— GPIO 寄存器直驱 bit-bang（绕开 SPI 驱动，任何板子必通）
//  每轮 红/绿/蓝/白/黑 五种满屏颜色各 2.5 秒，左上角字标说明当前状态。
//
//  判读：
//    A 有颜色、B 有颜色 → 屏幕与引脚全对，直接烧主固件
//    A 全黑、B 有颜色   → Arduino SPI 驱动在 C3 上有问题，主固件换直驱
//    A、B 全黑          → 引脚/面板问题，再查硬件
//  （颜色黑白互换 = INVON 极性反；字颠倒 = MADCTL 方向）
// ============================================================
#include <Arduino.h>
#include <SPI.h>
#include <esp32-hal-gpio.h>   // GPIO.out_w1ts / out_w1tc 寄存器直写（bit-bang 用）

// ---------- 官方引脚（folotoy/ai-passport bsp_pins.h）----------
#define PIN_SCLK 8
#define PIN_MOSI 9
#define PIN_CS   1
#define PIN_DC   20
#define PIN_BL   21

static bool s_bb = false;   // false = Arduino SPI，true = bit-bang 直驱

// ---------------- 低层写总线（两种实现）----------------

static void bbWrite(uint8_t d) {
  // SPI mode 0：上升沿采样 → 时钟低时摆好 MOSI，再拉高时钟
  for (int i = 7; i >= 0; i--) {
    GPIO.out_w1tc.val = (1UL << PIN_SCLK);
    if (d & (1 << i)) GPIO.out_w1ts.val = (1UL << PIN_MOSI);
    else              GPIO.out_w1tc.val = (1UL << PIN_MOSI);
    GPIO.out_w1ts.val = (1UL << PIN_SCLK);
  }
  GPIO.out_w1tc.val = (1UL << PIN_SCLK);
}

static void bbCmd(uint8_t c) {
  GPIO.out_w1tc.val = (1UL << PIN_DC);
  GPIO.out_w1tc.val = (1UL << PIN_CS);
  bbWrite(c);
  GPIO.out_w1ts.val = (1UL << PIN_CS);
}

static void bbDataN(const uint8_t* d, size_t n) {
  GPIO.out_w1ts.val = (1UL << PIN_DC);
  GPIO.out_w1tc.val = (1UL << PIN_CS);
  while (n--) bbWrite(*d++);
  GPIO.out_w1ts.val = (1UL << PIN_CS);
}

static void cmd(uint8_t c) {
  if (s_bb) { bbCmd(c); return; }
  digitalWrite(PIN_DC, LOW);
  SPI.beginTransaction(SPISettings(27000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(c);
  SPI.endTransaction();
}

static void dataN(const uint8_t* d, size_t n) {
  // 不能用 SPI.transfer(buf,n) —— ESP32 上全双工会把接收数据写回 buf，
  // Flash 只读区（DROM）常量表会被回写触发 Store fault
  if (s_bb) { bbDataN(d, n); return; }
  digitalWrite(PIN_DC, HIGH);
  SPI.beginTransaction(SPISettings(27000000, MSBFIRST, SPI_MODE0));
  SPI.writeBytes(d, n);
  SPI.endTransaction();
}

static void data8(uint8_t d) { dataN(&d, 1); }

static void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  cmd(0x2A); data8(x0 >> 8); data8(x0); data8(x1 >> 8); data8(x1);
  cmd(0x2B); data8(y0 >> 8); data8(y0); data8(y1 >> 8); data8(y1);
  cmd(0x2C);   // RAMWR
}

static void fillRectRaw(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
  setWindow(x, y, x + w - 1, y + h - 1);
  uint32_t n = (uint32_t)w * h;
  if (s_bb) {
    GPIO.out_w1ts.val = (1UL << PIN_DC);
    GPIO.out_w1tc.val = (1UL << PIN_CS);
    while (n--) { bbWrite(color >> 8); bbWrite(color & 0xFF); }
    GPIO.out_w1ts.val = (1UL << PIN_CS);
  } else {
    digitalWrite(PIN_DC, HIGH);
    SPI.beginTransaction(SPISettings(27000000, MSBFIRST, SPI_MODE0));
    while (n--) SPI.transfer16(color);
    SPI.endTransaction();
  }
}

// ---------------- ST7789P3 初始化（厂商序列）----------------

static void initSt7789() {
  // 厂商初始化序列（folotoy/ai-passport bsp_display.c 原值），
  // 末尾 0x21 INVON 反色必加，否则这块屏呈负片
  static const uint8_t pB2[]  = {0x05, 0x05, 0x00, 0x33, 0x33};
  static const uint8_t pD0a[] = {0xA7, 0xA1};
  static const uint8_t pD0b[] = {0xA4, 0xA1};
  static const uint8_t pE0[]  = {0xD0,0x04,0x08,0x0A,0x09,0x05,0x2D,0x43,0x49,0x09,0x16,0x15,0x26,0x2B};
  static const uint8_t pE1[]  = {0xD0,0x03,0x09,0x0A,0x0A,0x06,0x2E,0x44,0x40,0x3A,0x15,0x15,0x26,0x2A};
  cmd(0x01); delay(150);        // SWRESET（官方板 RST 未接 MCU，只能软复位）
  cmd(0x11); delay(120);        // SLPOUT
  cmd(0x36); data8(0x00);       // MADCTL
  cmd(0x3A); data8(0x55);       // COLMOD 16bit
  cmd(0xB2); dataN(pB2, 5);     // PORCTRL
  cmd(0xB7); data8(0x35);       // GCTRL
  cmd(0xBB); data8(0x21);       // VCOMS
  cmd(0xC0); data8(0x2C);       // LCMCTRL
  cmd(0xC2); data8(0x01);       // VDVVRHEN
  cmd(0xC3); data8(0x0B);       // VRHS
  cmd(0xC4); data8(0x20);       // VDVSET
  cmd(0xC6); data8(0x0F);       // FRCTRL2
  cmd(0xD0); dataN(pD0a, 2);    // PWCTRL1
  cmd(0xD0); dataN(pD0b, 2);    // PWCTRL1（厂商例程重发，覆盖上一条）
  cmd(0xD6); data8(0xA1);       // 低功耗模式
  cmd(0xE0); dataN(pE0, 14);    // PVGAMCTRL
  cmd(0xE1); dataN(pE1, 14);    // NVGAMCTRL
  cmd(0x21);                    // INVON 反色（必加）
  cmd(0x29); delay(30);         // DISPON
}

// ---------------- 5×7 ASCII 字体（0x20..0x5A）----------------

static const uint8_t F5X7[59][5] = {
  {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00},
  {0x14,0x7F,0x14,0x7F,0x14}, {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62},
  {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00}, {0x00,0x1C,0x22,0x41,0x00},
  {0x00,0x41,0x22,0x1C,0x00}, {0x14,0x08,0x3E,0x08,0x14}, {0x08,0x08,0x3E,0x08,0x08},
  {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00},
  {0x20,0x10,0x08,0x04,0x02}, {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
  {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31}, {0x18,0x14,0x12,0x7F,0x10},
  {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
  {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}, {0x00,0x36,0x36,0x00,0x00},
  {0x00,0x56,0x36,0x00,0x00}, {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14},
  {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06}, {0x32,0x49,0x79,0x41,0x3E},
  {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
  {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
  {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00},
  {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
  {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
  {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
  {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F},
  {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
  {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43},
};

static void drawGlyph(int x, int y, char ch, uint16_t color, int scale) {
  if (ch < 0x20 || ch > 0x5A) ch = '?';
  const uint8_t* g = F5X7[ch - 0x20];
  for (int r = 0; r < 7; r++)
    for (int c = 0; c < 5; c++)
      if (g[r] & (1 << (4 - c)))
        fillRectRaw(x + c * scale, y + r * scale, scale, scale, color);
}

static void drawText(int x, int y, const char* s, uint16_t color, int scale) {
  while (*s) { drawGlyph(x, y, *s++, color, scale); x += 6 * scale; }
}

// ---------------- 测试循环 ----------------

struct Phase { uint16_t color; uint16_t textColor; const char* label; };

static void runCycle(const char* tag) {
  static const Phase PHASES[] = {
    {0xF800, 0xFFFF, " RED"}, {0x07E0, 0x0000, " GREEN"},
    {0x001F, 0xFFFF, " BLUE"}, {0xFFFF, 0x0000, " WHITE"},
    {0x0000, 0xFFFF, " BLACK"},
  };
  for (size_t i = 0; i < sizeof(PHASES) / sizeof(PHASES[0]); i++) {
    const Phase& p = PHASES[i];
    fillRectRaw(0, 0, 240, 320, p.color);
    drawText(4, 8, tag, p.textColor, 3);
    drawText(4, 40, p.label, p.textColor, 2);
    log_i("[%s] %s", tag, p.label);
    delay(2500);
  }
}

void setup() {
  delay(300);
  log_i("=== 屏幕测试 v2（TRAE AI 通行证官方引脚）===");
  log_i("Cycle A = Arduino SPI；Cycle B = GPIO 直驱。红/绿/蓝/白/黑 各 2.5 秒循环。");
  log_i("判读：A/B 都有色=全部正常；A 黑 B 有色=SPI 驱动问题；全黑=查硬件。");

  pinMode(PIN_BL, OUTPUT);
  digitalWrite(PIN_BL, HIGH);   // 背光常亮（官方 LEDC 调光，测试先拉满）
  pinMode(PIN_CS, OUTPUT); digitalWrite(PIN_CS, HIGH);
  pinMode(PIN_DC, OUTPUT); digitalWrite(PIN_DC, HIGH);
}

void loop() {
  // Cycle A：Arduino SPI
  s_bb = false;
  SPI.end();
  SPI.begin(PIN_SCLK, -1, PIN_MOSI, PIN_CS);
  initSt7789();
  runCycle("SPI-A");

  // Cycle B：GPIO 寄存器直驱
  s_bb = true;
  SPI.end();
  pinMode(PIN_SCLK, OUTPUT);
  pinMode(PIN_MOSI, OUTPUT);
  digitalWrite(PIN_SCLK, LOW);
  digitalWrite(PIN_MOSI, LOW);
  initSt7789();
  runCycle("BB-B");
}
