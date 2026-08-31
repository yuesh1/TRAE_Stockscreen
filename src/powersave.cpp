// ============================================================
//  夜间深睡眠实现
//  参考官方 ai-passport demo_low_power.c（RTC 定时唤醒）；
//  按键唤醒利用三键分压电路：任意键按下把 GPIO0 拉到 <600mV（数字低电平），
//  外部 10k 上拉接常供电的 3.3V，深睡期间仍有效。
//  注意：深睡唤醒 = 重新启动，唤醒后重连 WiFi 抓一轮数据（约 5~10 秒）。
// ============================================================
#include "powersave.h"
#include "config.h"
#include "esp_sleep.h"
#include "driver/gpio.h"

void powersaveBootReport() {
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_TIMER:
      log_i("[深睡] 定时唤醒（睡眠窗口结束）");
      break;
    case ESP_SLEEP_WAKEUP_GPIO:
      log_i("[深睡] 按键唤醒");
      break;
    default:
      log_i("[深睡] 正常上电/复位启动");
      break;
  }
  // 上一轮深睡为压住背光启用了 GPIO 保持，启动后先解除才能重新控制
#if TFT_BL_PIN >= 0
  gpio_hold_dis((gpio_num_t)TFT_BL_PIN);
#endif
  gpio_deep_sleep_hold_dis();
}

bool powersaveInWindow(uint32_t epoch) {
#if !DEEP_SLEEP_ENABLE
  return false;
#else
  if (!epoch) return false;                    // 没有服务器时间无法判断
  uint32_t mins = (epoch + 8 * 3600) % 86400 / 60;   // 北京时间当日分钟
  if (DEEP_SLEEP_START_MIN <= DEEP_SLEEP_END_MIN)    // 窗口不跨零点
    return mins >= DEEP_SLEEP_START_MIN && mins < DEEP_SLEEP_END_MIN;
  return mins >= DEEP_SLEEP_START_MIN || mins < DEEP_SLEEP_END_MIN;  // 跨零点
#endif
}

void powersaveMaybeSleep(TFT_eSPI& tft, uint32_t epoch, bool screenOn, bool bleConn) {
#if DEEP_SLEEP_ENABLE
  if (screenOn || bleConn || !powersaveInWindow(epoch)) return;

  // 睡到窗口结束（RTC 时钟有 ±5% 漂移，提前醒会再次进入本函数补睡）
  uint32_t mins = (epoch + 8 * 3600) % 86400 / 60;
  uint32_t remainMin = mins < DEEP_SLEEP_END_MIN
      ? DEEP_SLEEP_END_MIN - mins
      : 24 * 60 - mins + DEEP_SLEEP_END_MIN;
  if (remainMin < 2) return;
  log_i("[深睡] 进入深睡眠 %lu 分钟（按任意键唤醒）", (unsigned long)remainMin);
  delay(50);   // 让日志发完

  // 屏幕面板睡眠（DISPOFF + SLPIN），背光引脚压低并在深睡期间保持
  tft.writecommand(0x28);
  tft.writecommand(0x10);
  delay(120);
#if TFT_BL_PIN >= 0
  digitalWrite(TFT_BL_PIN, LOW);
  gpio_hold_en((gpio_num_t)TFT_BL_PIN);
#endif
  gpio_deep_sleep_hold_en();

  esp_sleep_enable_timer_wakeup((uint64_t)remainMin * 60ULL * 1000000ULL);
#if BTN_ADC_PIN >= 0 && BTN_ADC_PIN <= 5   // C3 仅 GPIO0~5 支持深睡 GPIO 唤醒
  esp_deep_sleep_enable_gpio_wakeup(1ULL << BTN_ADC_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
#endif
  esp_deep_sleep_start();   // 不返回；唤醒即重启
#endif
}
