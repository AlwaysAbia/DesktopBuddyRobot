#include "remote_ops.h"

#include <Arduino.h>
#include <WiFi.h>

#include "ota_update.h"
#include "tb_client.h"

namespace {

constexpr unsigned long TELEMETRY_INTERVAL_MS = 60UL * 1000UL;
constexpr unsigned long ATTRIBUTE_CHECK_MS    = 5UL * 1000UL;

// RPC "checkForUpdate": params {"force": true} (optional, also accepts a plain true/false).
// Starts the same check as the serial command "ota" / "ota force" and answers at once;
// progress shows in the fw_state telemetry.
void onCheckForUpdate(JsonVariantConst const& params, JsonDocument& response) {
  bool force = false;
  if (params.is<bool>()) force = params.as<bool>();
  else if (params.is<JsonObjectConst>()) force = params["force"] | false;

  bool started = ota::requestCheck(force);
  Serial.printf("[RPC] checkForUpdate force=%d -> %s\r\n", force, started ? "started" : "busy");
  response["accepted"] = started;  // false = a check/update is already running
  response["force"] = force;
}

const RPC_Callback callbacks[] = {
  RPC_Callback("checkForUpdate", &onCheckForUpdate),
};

// Total RPC methods across remote_ops and remote_test must stay <= tb_client::MAX_RPC_METHODS
// (remote_test.cpp has 4).
static_assert(4 + sizeof(callbacks) / sizeof(callbacks[0]) <= tb_client::MAX_RPC_METHODS,
              "more RPC methods than tb_client::MAX_RPC_METHODS: none would register");

bool wasConnected = false;
unsigned long lastTelemetry = 0, lastAttrCheck = 0;
String lastAttrKey;  // fingerprint of the attributes last sent

const char* orDash(const String& s) {
  return s.isEmpty() ? "-" : s.c_str();
}

void sendTelemetry() {
  auto& tb = tb_client::tb();
  tb.sendTelemetryData("rssi", WiFi.RSSI());
  tb.sendTelemetryData("uptime_s", (uint32_t)(millis() / 1000UL));
  tb.sendTelemetryData("heap_free_kb", (uint32_t)(ESP.getFreeHeap() / 1024));
  tb.sendTelemetryData("heap_min_kb", (uint32_t)(ESP.getMinFreeHeap() / 1024));
}

// Sends the OTA attributes when they differ from what was last sent (or force = true).
void sendAttributes(bool force) {
  ota::Info info = ota::info();
  String key = String(info.partition) + '|' + info.imageState + '|' + info.rollbackPossible + '|' +
               info.awaitingConfirm + '|' + info.badVersion + '|' + info.lastError;
  if (!force && key == lastAttrKey) return;
  lastAttrKey = key;

  auto& tb = tb_client::tb();
  tb.sendAttributeData("ota_partition", info.partition);
  tb.sendAttributeData("ota_image_state", info.imageState);
  tb.sendAttributeData("ota_rollback_possible", info.rollbackPossible);
  tb.sendAttributeData("ota_awaiting_confirm", info.awaitingConfirm);
  tb.sendAttributeData("ota_bad_version", orDash(info.badVersion));
  tb.sendAttributeData("ota_last_error", orDash(info.lastError));
}

}  // namespace

namespace remote_ops {

void begin() {
  if (!tb_client::rpcApi().RPC_Subscribe(std::begin(callbacks), std::end(callbacks))) {
    Serial.println("[RPC] Could not register remote_ops RPC methods");
  }
}

void loop() {
  bool connected = tb_client::connected();
  if (!connected) {
    wasConnected = false;
    return;
  }

  unsigned long now = millis();
  if (!wasConnected) {  // (re)connected: send everything once
    wasConnected = true;
    sendAttributes(true);
    sendTelemetry();
    lastTelemetry = lastAttrCheck = now;
    Serial.println("[TB] Reported device status");
    return;
  }
  if (now - lastAttrCheck >= ATTRIBUTE_CHECK_MS) {
    lastAttrCheck = now;
    sendAttributes(false);
  }
  if (now - lastTelemetry >= TELEMETRY_INTERVAL_MS) {
    lastTelemetry = now;
    sendTelemetry();
  }
}

}  // namespace remote_ops
