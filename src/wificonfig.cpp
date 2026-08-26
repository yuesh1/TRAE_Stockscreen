// ============================================================
//  WiFi 运行时配置：USB-Serial/JTAG 串口命令 + NVS 持久化
//  命令：
//    wifi <SSID> <密码>   保存凭据并立即生效
//    wifi-status          查看当前凭据来源（不打印密码）
//    wifi-clear           清除已保存的凭据，回退编译期默认值
//    help                 打印帮助
//  输入输出走 USB-Serial/JTAG（与 log_i 日志同一通道，115200）
// ============================================================
#include "wificonfig.h"
#include "config.h"
#include <Preferences.h>
#include <stdarg.h>
#include "driver/usb_serial_jtag.h"

static const char* NVS_NS = "wifi";

static const char* USAGE =
  "\r\n=== WiFi 配置 ===\r\n"
  "  wifi <SSID> <密码>  保存凭据并重连\r\n"
  "  wifi-status         查看凭据来源\r\n"
  "  wifi-clear          清除 NVS 凭据\r\n"
  "  help                打印本帮助\r\n";

// ---- 控制台输出 / 输入（USB-Serial/JTAG 驱动）----

static void cfgWrite(const char* s) {
  usb_serial_jtag_write_bytes(s, strlen(s), pdMS_TO_TICKS(200));
}

static void cfgPrintf(const char* fmt, ...) {
  char buf[128];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  cfgWrite(buf);
}

// 返回读到的字节，无数据返回 -1
static int cfgReadByte() {
  uint8_t b;
  int n = usb_serial_jtag_read_bytes(&b, 1, 0);
  return n > 0 ? b : -1;
}

// ---- NVS ----

static bool readNvs(String& ssid, String& pass) {
  Preferences pref;
  if (!pref.begin(NVS_NS, true)) return false;   // 只读打开
  ssid = pref.getString("ssid", "");
  pass = pref.getString("pass", "");
  pref.end();
  return ssid.length() > 0 && pass.length() > 0;
}

bool wifiConfigHaveCreds() {
  String s, p;
  return readNvs(s, p);
}

void wifiConfigResolve(String& ssid, String& pass) {
  if (readNvs(ssid, pass)) return;      // NVS 优先
  ssid = WIFI_SSID;                     // 否则编译期默认值
  pass = WIFI_PASS;
}

bool wifiConfigIsPlaceholder() {
  return strcmp(WIFI_SSID, "YOUR_WIFI_SSID") == 0;
}

static bool saveNvs(const String& ssid, const String& pass) {
  Preferences pref;
  if (!pref.begin(NVS_NS, false)) return false;  // 读写打开
  pref.putString("ssid", ssid);
  pref.putString("pass", pass);
  pref.end();
  return true;
}

static void clearNvs() {
  Preferences pref;
  if (pref.begin(NVS_NS, false)) {
    pref.remove("ssid");
    pref.remove("pass");
    pref.end();
  }
}

// ---- 命令处理 ----

void wifiConfigBegin() {
  cfgWrite("\r\n[WiFi] USB 串口配网已就绪（help 查看命令）\r\n");
}

// 去掉行尾 \r\n 与首尾空格
static String trimLine(const String& s) {
  String t = s;
  while (t.length() && (t[t.length() - 1] == '\r' || t[t.length() - 1] == '\n' || t[t.length() - 1] == ' '))
    t.remove(t.length() - 1);
  while (t.length() && t[0] == ' ') t.remove(0, 1);
  return t;
}

static void printStatus() {
  String s, p;
  if (readNvs(s, p)) {
    cfgPrintf("[WiFi] 当前凭据：NVS 已保存（SSID: %s，密码已隐藏）\r\n", s.c_str());
  } else {
    cfgPrintf("[WiFi] 当前凭据：编译期默认值（SSID: %s）%s\r\n", WIFI_SSID,
              wifiConfigIsPlaceholder() ? "，未配置！请用 wifi 命令设置" : "");
  }
}

// 解析一行命令，凭据变更返回 true
static bool handleLine(const String& raw) {
  String line = trimLine(raw);
  if (line.length() == 0) return false;
  if (line == "help" || line == "?") {
    cfgWrite(USAGE);
  } else if (line == "wifi-status") {
    printStatus();
  } else if (line == "wifi-clear") {
    clearNvs();
    cfgWrite("[WiFi] 已清除 NVS 凭据，将回退编译期默认值\r\n");
    return true;
  } else if (line.startsWith("wifi ")) {
    String rest = line.substring(5);
    int sp = rest.indexOf(' ');
    if (sp <= 0) {
      cfgWrite("[WiFi] 格式：wifi <SSID> <密码>（SSID 和密码不要带空格）\r\n");
      return false;
    }
    String ssid = rest.substring(0, sp);
    String pass = rest.substring(sp + 1);
    if (!saveNvs(ssid, pass)) {
      cfgWrite("[WiFi] NVS 保存失败\r\n");
      return false;
    }
    cfgPrintf("[WiFi] 已保存（SSID: %s，密码已隐藏），正在重连...\r\n", ssid.c_str());
    return true;
  } else {
    cfgWrite("[WiFi] 未知命令，输入 help 查看帮助\r\n");
  }
  return false;
}

bool wifiConfigLoop() {
  static String buf;
  bool changed = false;
  int c;
  while ((c = cfgReadByte()) >= 0) {
    if (c == '\n' || c == '\r') {
      if (buf.length()) changed = handleLine(buf) || changed;
      buf = "";
    } else if (buf.length() < 96) {
      buf += (char)c;
    }
  }
  return changed;
}
