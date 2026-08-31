// ============================================================
//  三键 ADC 输入：25ms 采样 + 两次一致去抖，松开时上报点击
// ============================================================
#include "buttons.h"
#include "config.h"

static BtnKey   sHeld     = BTN_NONE;   // 去抖后的当前按住键
static BtnKey   sCand     = BTN_NONE;   // 候选（等待第二次采样确认）
static uint32_t sLastMs   = 0;

static BtnKey readRaw() {
#if BTN_ADC_PIN >= 0
  int mv = analogReadMilliVolts(BTN_ADC_PIN);
  if (mv < BTN_UP_MAX_MV)   return BTN_UP;
  if (mv < BTN_DOWN_MAX_MV) return BTN_DOWN;
  if (mv < BTN_PRESS_MV)    return BTN_OK;
#endif
  return BTN_NONE;
}

void btnInit() {
#if BTN_ADC_PIN >= 0
  analogReadResolution(12);
  analogSetPinAttenuation(BTN_ADC_PIN, ADC_11db);
#endif
}

bool btnAnyPressed() { return sHeld != BTN_NONE; }

BtnKey btnPoll() {
  uint32_t now = millis();
  if (now - sLastMs < 25) return BTN_NONE;
  sLastMs = now;

  BtnKey raw = readRaw();
  if (raw != sCand) {        // 第一次看到新状态，等下一轮确认
    sCand = raw;
    return BTN_NONE;
  }
  if (raw == sHeld) return BTN_NONE;   // 状态未变

  BtnKey released = BTN_NONE;
  if (sHeld != BTN_NONE && raw == BTN_NONE) released = sHeld;  // 松开=点击
  sHeld = raw;
  return released;
}
