#include "tb_client.h"

#include <Arduino.h>
#include <WiFi.h>
#include <NetworkClientSecure.h>
#include <Arduino_MQTT_Client.h>

#include "secrets_loader.h"
#include "thingsboard_config.h"

// Mozilla root CA bundle compiled into the core (CONFIG_MBEDTLS_CERTIFICATE_BUNDLE).
extern const uint8_t ca_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t ca_bundle_end[]   asm("_binary_x509_crt_bundle_end");

namespace {

// Attribute responses and our own messages are small; the OTA API temporarily
// grows the receive buffer to fit a firmware chunk while downloading.
constexpr uint16_t MAX_MESSAGE_RECEIVE_SIZE = 1024U;
constexpr uint16_t MAX_MESSAGE_SEND_SIZE    = 512U;

NetworkClientSecure tlsClient;
Arduino_MQTT_Client mqttClient(tlsClient);

OTA_Firmware_Update<> otaApiInstance;
tb_client::AttributeRequestApi attributeRequestApiInstance;
tb_client::RpcApi rpcApiInstance;
const std::array<IAPI_Implementation*, 3U> apis = {
  &otaApiInstance,
  &attributeRequestApiInstance,
  &rpcApiInstance,
};

ThingsBoard tbInstance(mqttClient, MAX_MESSAGE_RECEIVE_SIZE, MAX_MESSAGE_SEND_SIZE, Default_Max_Stack_Size, apis);

void (*connectedCallback)() = nullptr;
bool caBundleSet = false;
bool warnedNoToken = false;
bool wasConnected = false;
unsigned long lastAttempt = 0;
bool attemptedOnce = false;

}  // namespace

namespace tb_client {

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    wasConnected = false;
    return;
  }

  if (tbInstance.connected()) {
    tbInstance.loop();
    return;
  }

  if (wasConnected) {
    Serial.println("[TB] Connection lost");
    wasConnected = false;
  }

  if (strlen(TB_ACCESS_TOKEN) == 0) {
    if (!warnedNoToken) {
      Serial.println("[TB] No TB_ACCESS_TOKEN in include/secrets.h - ThingsBoard disabled");
      warnedNoToken = true;
    }
    return;
  }

  if (attemptedOnce && millis() - lastAttempt < TB_RECONNECT_INTERVAL_MS) return;
  attemptedOnce = true;
  lastAttempt = millis();

  if (!caBundleSet) {
    tlsClient.setCACertBundle(ca_bundle_start, ca_bundle_end - ca_bundle_start);
    caBundleSet = true;
  }

  Serial.printf("[TB] Connecting to %s:%u ...\r\n", TB_HOST, (unsigned)TB_PORT);
  if (!tbInstance.connect(TB_HOST, TB_ACCESS_TOKEN, TB_PORT)) {
    Serial.printf("[TB] Connect failed, retrying in %u s\r\n", (unsigned)(TB_RECONNECT_INTERVAL_MS / 1000));
    return;
  }

  Serial.println("[TB] Connected");
  wasConnected = true;
  if (connectedCallback) connectedCallback();
}

bool connected() {
  return tbInstance.connected();
}

void onConnected(void (*callback)()) {
  connectedCallback = callback;
}

ThingsBoard& tb() {
  return tbInstance;
}

OTA_Firmware_Update<>& otaApi() {
  return otaApiInstance;
}

AttributeRequestApi& attributeRequestApi() {
  return attributeRequestApiInstance;
}

RpcApi& rpcApi() {
  return rpcApiInstance;
}

}  // namespace tb_client
