// ============================================================
//  CW2017 电量计驱动（寄存器协议照官方 bsp_battery.c 移植）
//  芯片掉电（电池深放/断开）会丢失 RAM 里的电池 profile，此后 SOC
//  永远读到 0xFE/0xFF 无效值——初始化必须校验 profile 并在丢失时
//  重写（官方优特利 520mAh 参数），否则电量显示会"突然消失"
// ============================================================
#include "battery.h"
#include "config.h"
#include <Wire.h>

#define CW2017_ADDR      0x63
#define CW_REG_VERSION   0x00   // 版本号，上电应答即代表芯片在位
#define CW_REG_VCELL_H   0x02   // 14bit 电压，V(uV) = raw * 312.5
#define CW_REG_SOC_H     0x04   // 高字节 = 整数百分比；低字节(0x05)= 1/256 %
#define CW_REG_CONFIG    0x08   // 0xF0=睡眠 / 0x30=复位态 / 0x00=正常
#define CW_REG_SOC_ALERT 0x0B   // bit7=profile UPDATE_FLAG；bit6:0=SOC 告警阈值
#define CW_REG_PROFILE   0x10   // 80 字节电池 profile 起始地址

#define CW_CONFIG_ACTIVE  0x00
#define CW_CONFIG_RESTART 0x30
#define CW_CONFIG_SLEEP   0xF0
#define CW_UPDATE_FLAG    0x80
#define CW_PROFILE_SIZE   80

// 官方优特利 520mAh 电芯 profile（来自 ai-passport bsp_battery.c）
static const uint8_t PROFILE[CW_PROFILE_SIZE] = {
    0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xAD, 0xC7, 0xC8, 0xCA, 0xBD, 0xB1, 0xC1, 0x94,
    0x88, 0xD1, 0xBD, 0x97, 0x88, 0x66, 0x56, 0x4A,
    0x3F, 0x33, 0x26, 0x5C, 0x37, 0xD1, 0x27, 0xD8,
    0xCC, 0xB7, 0xCF, 0xB3, 0xB2, 0xAE, 0xA6, 0x9E,
    0x99, 0x97, 0x9B, 0x86, 0x47, 0x1E, 0x17, 0x26,
    0x49, 0x96, 0xD9, 0xE1, 0xDD, 0xDC, 0xD4, 0x59,
    0x00, 0x00, 0x90, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5C,
};

static bool cwRead(uint8_t reg, uint8_t* buf, size_t n) {
  Wire.beginTransmission(CW2017_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)CW2017_ADDR, (uint8_t)n) != n) return false;
  for (size_t i = 0; i < n; i++) buf[i] = (uint8_t)Wire.read();
  return true;
}

static bool cwWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(CW2017_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

// CONFIG 切换必须经过复位态（0x30），时序照官方
static bool cwEnterSleep() {
  if (!cwWrite(CW_REG_CONFIG, CW_CONFIG_RESTART)) return false;
  delay(20);
  if (!cwWrite(CW_REG_CONFIG, CW_CONFIG_SLEEP)) return false;
  delay(10);
  return true;
}

static bool cwEnterActive() {
  if (!cwWrite(CW_REG_CONFIG, CW_CONFIG_RESTART)) return false;
  delay(20);
  if (!cwWrite(CW_REG_CONFIG, CW_CONFIG_ACTIVE)) return false;
  delay(10);
  return true;
}

// 同时检查 UPDATE_FLAG 和 80 字节内容，避免只凭标志误用旧电芯参数
static bool cwProfileMatches(bool* matches) {
  uint8_t val = 0;
  *matches = false;
  if (!cwRead(CW_REG_SOC_ALERT, &val, 1)) return false;
  if ((val & CW_UPDATE_FLAG) == 0) return true;
  for (size_t i = 0; i < CW_PROFILE_SIZE; i++) {
    if (!cwRead((uint8_t)(CW_REG_PROFILE + i), &val, 1)) return false;
    if (val != PROFILE[i]) return true;
  }
  *matches = true;
  return true;
}

// Profile 必须在睡眠态逐字节写入，读回校验后置 UPDATE_FLAG，再重启计算
static bool cwUpdateProfile() {
  uint8_t val = 0;
  if (!cwEnterSleep()) return false;
  for (size_t i = 0; i < CW_PROFILE_SIZE; i++)
    if (!cwWrite((uint8_t)(CW_REG_PROFILE + i), PROFILE[i])) {
      log_e("[电池] 写 profile 失败: index=%u", (unsigned)i);
      return false;
    }
  for (size_t i = 0; i < CW_PROFILE_SIZE; i++) {
    if (!cwRead((uint8_t)(CW_REG_PROFILE + i), &val, 1)) return false;
    if (val != PROFILE[i]) {
      log_e("[电池] profile 校验失败: index=%u 期望=0x%02X 实际=0x%02X",
            (unsigned)i, PROFILE[i], val);
      return false;
    }
  }
  if (!cwRead(CW_REG_SOC_ALERT, &val, 1)) return false;
  if (!cwWrite(CW_REG_SOC_ALERT, val | CW_UPDATE_FLAG)) return false;
  return cwEnterActive();
}

// 首次计算期间 SOC 可能暂时大于 100；最多等待 5 秒
static bool cwWaitSocReady() {
  for (int retry = 0; retry < 50; retry++) {
    uint8_t soc = 0;
    delay(100);
    if (cwRead(CW_REG_SOC_H, &soc, 1) && soc <= 100) return true;
  }
  return false;
}

void batteryInit() {
  Wire.begin(BSP_I2C_SDA, BSP_I2C_SCL);

  uint8_t ver = 0;
  if (!cwRead(CW_REG_VERSION, &ver, 1)) {   // 上电应答即代表芯片在位
    log_i("[电池] CW2017 无应答（0x%02X），隐藏电量", CW2017_ADDR);
    return;
  }
  log_i("[电池] CW2017 VERSION=0x%02X", ver);

  bool matches = false;
  if (!cwProfileMatches(&matches)) {
    log_e("[电池] 读取电池 profile 失败");
    return;
  }
  if (!matches) {
    // 芯片掉过电（电池深放/断开），profile 丢失 → 重写并重启量测
    log_i("[电池] profile 丢失/不匹配，写入优特利 520mAh 参数...");
    if (!cwUpdateProfile()) {
      log_e("[电池] profile 写入失败");
      return;
    }
  } else {
    uint8_t config = 0;
    if (cwRead(CW_REG_CONFIG, &config, 1) && config != CW_CONFIG_ACTIVE)
      cwEnterActive();
  }
  if (!cwWaitSocReady())
    log_w("[电池] 等待首次 SOC 计算超时（充电中会自行恢复）");
}

int batterySoc() {
  uint8_t b[2] = {0};
  if (!cwRead(CW_REG_SOC_H, b, 2)) return -1;   // SOC 高字节 = 整数百分比
  int soc = b[0];
  return soc <= 100 ? soc : -1;                 // 未就绪时可能读到 0xFF
}

int batteryMv() {
  uint8_t b[2] = {0};
  if (!cwRead(CW_REG_VCELL_H, b, 2)) return -1;
  uint32_t raw = ((uint32_t)b[0] << 8 | b[1]) & 0x3FFF;   // 14bit
  return (int)((raw * 3125) / 10000);                     // raw * 312.5uV → mV
}

// 逐寄存器读并报告 I2C 层错误码，区分"芯片不应答"和"数据异常"
void batteryDiag(String& out) {
  char line[64];
  Wire.beginTransmission(CW2017_ADDR);
  int err = Wire.endTransmission();   // 空探测：0=ACK
  snprintf(line, sizeof(line), "[电池] I2C 0x%02X 探测: %s(err=%d)\r\n",
           CW2017_ADDR, err == 0 ? "ACK" : "无应答", err);
  out += line;
  if (err != 0) return;

  struct { uint8_t reg; const char* name; } regs[] = {
      {CW_REG_VERSION, "VERSION"}, {CW_REG_CONFIG, "CONFIG"},
      {CW_REG_SOC_ALERT, "SOC_ALERT"}, {CW_REG_SOC_H, "SOC_H"}};
  for (auto& r : regs) {
    uint8_t v = 0;
    if (cwRead(r.reg, &v, 1))
      snprintf(line, sizeof(line), "[电池] %s(0x%02X)=0x%02X\r\n", r.name, r.reg, v);
    else
      snprintf(line, sizeof(line), "[电池] %s(0x%02X) 读失败\r\n", r.name, r.reg);
    out += line;
  }
  snprintf(line, sizeof(line), "[电池] soc=%d%% 电压=%dmV\r\n", batterySoc(), batteryMv());
  out += line;
}
