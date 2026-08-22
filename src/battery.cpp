// ============================================================
//  CW2017 电量计驱动（寄存器协议照官方 bsp_battery.c 移植）
// ============================================================
#include "battery.h"
#include "config.h"
#include <Wire.h>

#define CW2017_ADDR 0x63

static bool cwRead(uint8_t reg, uint8_t* buf, size_t n) {
  Wire.beginTransmission(CW2017_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)CW2017_ADDR, (uint8_t)n) != n) return false;
  for (size_t i = 0; i < n; i++) buf[i] = (uint8_t)Wire.read();
  return true;
}

void batteryInit() {
  Wire.begin(BSP_I2C_SDA, BSP_I2C_SCL);

  uint8_t ver = 0;
  if (!cwRead(0x00, &ver, 1)) {   // VERSION 寄存器上电应答即代表芯片在位
    log_i("[电池] CW2017 无应答（0x%02X），隐藏电量", CW2017_ADDR);
    return;
  }
  Wire.beginTransmission(CW2017_ADDR);
  Wire.write(0x08);               // CONFIG 寄存器
  Wire.write(0x00);               // 0x00 = 正常模式（退出睡眠/复位态）
  Wire.endTransmission();
  delay(100);                     // 等首次 SOC 计算完成
  log_i("[电池] CW2017 VERSION=0x%02X", ver);
}

int batterySoc() {
  uint8_t b[2] = {0};
  if (!cwRead(0x04, b, 2)) return -1;   // SOC 高字节 = 整数百分比
  int soc = b[0];
  return soc <= 100 ? soc : -1;         // 未就绪时可能读到 0xFF
}

int batteryMv() {
  uint8_t b[2] = {0};
  if (!cwRead(0x02, b, 2)) return -1;
  uint32_t raw = ((uint32_t)b[0] << 8 | b[1]) & 0x3FFF;   // 14bit
  return (int)((raw * 3125) / 10000);                     // raw * 312.5uV → mV
}
