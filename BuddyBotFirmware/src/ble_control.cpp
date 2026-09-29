#include "ble_control.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <atomic>

#include "ble_config.h"
#include "display_modes.h"
#include "eye_mode.h"
#include "ota_config.h"
#include "ota_update.h"
#include "tb_client.h"
#include "wifi_manager.h"

namespace {

// Service + characteristic UUIDs. Keep in step with interface-contract.md, section 1.
const char* UUID_SERVICE  = "9b370000-a32f-4baf-8406-88e9298fd20d";
const char* UUID_WIFI_CFG = "9b370001-a32f-4baf-8406-88e9298fd20d";
const char* UUID_EYE      = "9b370002-a32f-4baf-8406-88e9298fd20d";
const char* UUID_MODE     = "9b370003-a32f-4baf-8406-88e9298fd20d";
const char* UUID_OTA      = "9b370004-a32f-4baf-8406-88e9298fd20d";
const char* UUID_STATUS   = "9b370005-a32f-4baf-8406-88e9298fd20d";

constexpr size_t MAX_WIFI_PAYLOAD = 200;  // {"ssid":32 bytes,"password":64 bytes} plus JSON escapes fits easily

NimBLEServer* server = nullptr;
NimBLECharacteristic* chrWifi = nullptr;
NimBLECharacteristic* chrEye = nullptr;
NimBLECharacteristic* chrMode = nullptr;
NimBLECharacteristic* chrOta = nullptr;
NimBLECharacteristic* chrStatus = nullptr;

// Written by the BLE host task, consumed by ble_control::loop() on the main task.
std::atomic<int> pendingMode{-1};
std::atomic<int> pendingEye{-1};
std::atomic<bool> pendingOta{false};
std::atomic<bool> forceNotify{false};
std::atomic<uint16_t> clientHandle{BLE_HS_CONN_HANDLE_NONE};
std::atomic<int> clientCount{0};

// Fills buf with the Status JSON (interface-contract.md). Returns its length.
size_t buildStatus(char* buf, size_t size) {
  StaticJsonDocument<256> doc;
  doc["wifi"] = wifi_mgr::stateName(wifi_mgr::state());
  const char* reason = wifi_mgr::failReason();
  if (reason[0]) doc["wifiErr"] = reason;
  doc["mode"] = (uint8_t)modes::current();
  doc["eye"] = eye::theme();
  doc["tb"] = tb_client::connected();
  doc["fw"] = FIRMWARE_VERSION;
  doc["heapMinKb"] = ESP.getMinFreeHeap() / 1024;
  return serializeJson(doc, buf, size);
}

void handleWifiWrite(const uint8_t* data, size_t len) {
  if (len == 0 || len > MAX_WIFI_PAYLOAD) {
    Serial.printf("[BLE] WiFi config rejected: bad length %u\r\n", (unsigned)len);
    return;
  }
  StaticJsonDocument<384> doc;
  if (deserializeJson(doc, data, len) != DeserializationError::Ok || !doc["ssid"].is<const char*>()) {
    Serial.println("[BLE] WiFi config rejected: not JSON with a string \"ssid\"");
    return;
  }
  const char* ssid = doc["ssid"];
  const char* password = doc["password"].is<const char*>() ? doc["password"].as<const char*>() : "";
  if (!wifi_mgr::queueCredentials(ssid, password)) {
    Serial.println("[BLE] WiFi config rejected: invalid SSID (max 32) or password (empty, 8-63 chars or 64 hex)");
    return;
  }
  Serial.println("[BLE] WiFi config accepted");
}

class ChrCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* chr, NimBLEConnInfo&) override {
    NimBLEAttValue value = chr->getValue();
    if (chr == chrWifi) {
      handleWifiWrite(value.data(), value.size());
    } else if (chr == chrEye) {
      if (value.size() == 1 && value.data()[0] < eye::THEME_COUNT) pendingEye = value.data()[0];
      else Serial.println("[BLE] Eye color rejected: need 1 byte 0-3");
    } else if (chr == chrMode) {
      if (value.size() == 1 && value.data()[0] < modes::MODE_COUNT) pendingMode = value.data()[0];
      else Serial.println("[BLE] Mode rejected: need 1 byte 0-2");
    } else if (chr == chrOta) {
      pendingOta = true;
    }
    forceNotify = true;
  }

  // Reads always return the live value, not what was last written.
  void onRead(NimBLECharacteristic* chr, NimBLEConnInfo&) override {
    if (chr == chrEye) {
      chr->setValue(eye::theme());
    } else if (chr == chrMode) {
      chr->setValue((uint8_t)modes::current());
    } else if (chr == chrStatus) {
      char buf[192];
      size_t n = buildStatus(buf, sizeof(buf));
      chr->setValue((const uint8_t*)buf, n);
    }
  }
} chrCallbacks;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo& info) override {
    clientHandle = info.getConnHandle();
    clientCount++;
    Serial.printf("[BLE] Client connected (%s)\r\n", info.getAddress().toString().c_str());
  }

  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override {
    clientHandle = BLE_HS_CONN_HANDLE_NONE;
    if (clientCount > 0) clientCount--;
    Serial.printf("[BLE] Client disconnected, reason 0x%02x\r\n", reason);
  }

  void onMTUChange(uint16_t mtu, NimBLEConnInfo&) override {
    Serial.printf("[BLE] MTU %u\r\n", (unsigned)mtu);
  }

  void onAuthenticationComplete(NimBLEConnInfo& info) override {
    Serial.printf("[BLE] Link %s\r\n", info.isEncrypted() ? "encrypted" : "NOT encrypted");
  }
} serverCallbacks;

// The values that make a Status notification worth sending.
struct StatusKey {
  uint8_t wifi;
  uint8_t mode;
  uint8_t eye;
  bool tb;
  bool operator!=(const StatusKey& o) const {
    return wifi != o.wifi || mode != o.mode || eye != o.eye || tb != o.tb;
  }
};

StatusKey currentKey() {
  return {(uint8_t)wifi_mgr::state(), (uint8_t)modes::current(), eye::theme(), tb_client::connected()};
}

}  // namespace

namespace ble_control {

void begin() {
  NimBLEDevice::init(BLE_DEVICE_NAME);
  // "Just Works": no input/output, no MITM protection, no bonding, no PIN.
  NimBLEDevice::setSecurityAuth(false, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

  server = NimBLEDevice::createServer();
  server->setCallbacks(&serverCallbacks);
  server->advertiseOnDisconnect(true);

  NimBLEService* service = server->createService(UUID_SERVICE);

  uint32_t wifiProps = NIMBLE_PROPERTY::WRITE;
#if BLE_WIFI_CONFIG_REQUIRES_ENCRYPTION
  wifiProps |= NIMBLE_PROPERTY::WRITE_ENC;
#endif
  chrWifi = service->createCharacteristic(UUID_WIFI_CFG, wifiProps);
  chrEye = service->createCharacteristic(UUID_EYE, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  chrMode = service->createCharacteristic(UUID_MODE, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  chrOta = service->createCharacteristic(UUID_OTA, NIMBLE_PROPERTY::WRITE);
  chrStatus = service->createCharacteristic(UUID_STATUS, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

  for (NimBLECharacteristic* c : {chrWifi, chrEye, chrMode, chrOta, chrStatus}) c->setCallbacks(&chrCallbacks);

  chrEye->setValue(eye::theme());
  chrMode->setValue((uint8_t)modes::current());
  char buf[192];
  size_t n = buildStatus(buf, sizeof(buf));
  chrStatus->setValue((const uint8_t*)buf, n);

  service->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->enableScanResponse(true);   // the 128-bit UUID and the name would not both fit in one packet
  adv->setName(BLE_DEVICE_NAME);
  adv->addServiceUUID(UUID_SERVICE);
  if (adv->start()) {
    Serial.printf("[BLE] Advertising as \"%s\"\r\n", BLE_DEVICE_NAME);
  } else {
    Serial.println("[BLE] ERROR: could not start advertising");
  }
  Serial.printf("[BLE] Free heap after BLE init: %u bytes\r\n", (unsigned)ESP.getFreeHeap());
}

void loop() {
  if (server == nullptr) return;

  wifi_mgr::setBleConnected(clientCount > 0);

  int newMode = pendingMode.exchange(-1);
  if (newMode >= 0) modes::set((modes::Mode)newMode);
  int newEye = pendingEye.exchange(-1);
  if (newEye >= 0) eye::setTheme((uint8_t)newEye);
  if (pendingOta.exchange(false)) {
    Serial.println("[BLE] OTA check requested");
    ota::requestCheck(false);
  }

  // Notify on real changes, and after every write so the app can time a round trip.
  static StatusKey lastKey = {};
  static bool first = true;
  StatusKey key = currentKey();
  bool forced = forceNotify.exchange(false);
  if (!first && !forced && !(key != lastKey)) return;
  first = false;
  lastKey = key;

  uint16_t handle = clientHandle;
  if (handle == BLE_HS_CONN_HANDLE_NONE) return;
  char buf[192];
  size_t n = buildStatus(buf, sizeof(buf));
  chrStatus->setValue((const uint8_t*)buf, n);
  // A notification longer than MTU-3 would be cut off mid-JSON: skip it; the app can still read.
  if (server->getPeerMTU(handle) < n + 3) {
    Serial.printf("[BLE] Status not notified: MTU %u too small for %u bytes\r\n",
                  (unsigned)server->getPeerMTU(handle), (unsigned)n);
    return;
  }
  chrStatus->notify(handle);
}

}  // namespace ble_control
