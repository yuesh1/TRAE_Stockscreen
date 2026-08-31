// ============================================================
//  BLE 配网 + 行情推送实现（NimBLE-Arduino 1.4.x）
//  选 NimBLE 而非 Bluedroid：省约 100KB RAM，C3 上与 WiFi 共存更稳
// ============================================================
#include "blecfg.h"
#include "config.h"

#if BLE_ENABLE

#include <NimBLEDevice.h>

#define SVC_UUID  "FFF0"
#define CHR_CMD   "FFF1"   // 手机 → 设备：文本命令
#define CHR_PUSH  "FFF2"   // 设备 → 手机：命令回应 + 行情推送

static NimBLECharacteristic* sPushChr = nullptr;
static volatile bool sConnected = false;

// 命令在 BLE 栈任务里写入、主循环里取走执行；单槽足够（人手速远慢于主循环）
static String        sPendingCmd;
static volatile bool sHasCmd = false;

class ServerCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s) override {
    sConnected = true;
    log_i("[BLE] 手机已连接");
  }
  void onDisconnect(NimBLEServer* s) override {
    sConnected = false;
    log_i("[BLE] 连接断开，恢复广播");
    NimBLEDevice::startAdvertising();
  }
};

class CmdCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override {
    std::string v = c->getValue();
    if (v.empty() || sHasCmd) return;   // 上一条还没被主循环取走时丢弃
    sPendingCmd = String(v.c_str());
    sHasCmd = true;
  }
};

void bleBegin() {
  NimBLEDevice::init(BLE_DEVICE_NAME);
  NimBLEDevice::setMTU(256);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCB());

  NimBLEService* svc = server->createService(SVC_UUID);
  NimBLECharacteristic* cmd = svc->createCharacteristic(
      CHR_CMD, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  cmd->setCallbacks(new CmdCB());
  sPushChr = svc->createCharacteristic(
      CHR_PUSH, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  svc->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(SVC_UUID);
  adv->setScanResponse(true);
  adv->start();
  log_i("[BLE] 广播中：%s（服务 FFF0，写 FFF1 / 订阅 FFF2）", BLE_DEVICE_NAME);
}

bool bleConnected() { return sConnected; }

bool bleTakeCommand(String& line) {
  if (!sHasCmd) return false;
  line = sPendingCmd;
  sHasCmd = false;
  return true;
}

void bleNotify(const char* text) {
  if (!sPushChr || !sConnected) return;
  // 按协商 MTU 分片（notify 载荷 = MTU - 3）；UTF-8 中文避免截在多字节中间
  size_t maxChunk = NimBLEDevice::getMTU() - 3;
  if (maxChunk < 20) maxChunk = 20;
  size_t len = strlen(text);
  size_t off = 0;
  while (off < len) {
    size_t n = len - off < maxChunk ? len - off : maxChunk;
    while (n > 1 && (text[off + n] & 0xC0) == 0x80) n--;   // 不截断 UTF-8 序列
    sPushChr->setValue((uint8_t*)(text + off), n);
    sPushChr->notify();
    off += n;
    delay(10);   // 连续分片给协议栈喘息，避免丢包
  }
}

#else  // BLE_ENABLE == 0：空实现

void bleBegin() {}
bool bleConnected() { return false; }
bool bleTakeCommand(String&) { return false; }
void bleNotify(const char*) {}

#endif
