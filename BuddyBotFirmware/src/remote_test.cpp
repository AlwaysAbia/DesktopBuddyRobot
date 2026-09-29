#include "remote_test.h"

#include <Arduino.h>

#include "clock_mode.h"
#include "display_modes.h"
#include "messages.h"
#include "tb_client.h"

namespace {

unsigned long rebootAt = 0;  // 0 = no reboot pending

// Every response carries the same snapshot, so the dashboard shows the result.
void fillState(JsonDocument& response) {
  response["mode"] = modes::name(modes::current());
  response["timeSynced"] = clock_mode::synced();
  response["messages"] = messages::count();
}

// RPC "nextMode" (no params): switch to the next display mode.
void onNextMode(JsonVariantConst const&, JsonDocument& response) {
  Serial.println("[RPC] nextMode");
  modes::next();
  fillState(response);
}

// RPC "clearMessages" (no params): delete the stored message history.
void onClearMessages(JsonVariantConst const&, JsonDocument& response) {
  Serial.println("[RPC] clearMessages");
  messages::clear();
  fillState(response);
}

// RPC "addMessage": params {"text": "..."} (or a plain JSON string). Stores it as the newest message.
void onAddMessage(JsonVariantConst const& params, JsonDocument& response) {
  const char* text = params.is<const char*>() ? params.as<const char*>() : params["text"].as<const char*>();
  Serial.printf("[RPC] addMessage %s\r\n", text ? text : "(missing text)");
  if (text != nullptr && text[0] != '\0') messages::add(text);
  fillState(response);
}

// RPC "reboot" (no params): restart shortly after the response has gone out.
void onReboot(JsonVariantConst const&, JsonDocument& response) {
  Serial.println("[RPC] reboot");
  rebootAt = millis() + 1500;
  fillState(response);
}

const RPC_Callback callbacks[] = {
  RPC_Callback("nextMode", &onNextMode),
  RPC_Callback("clearMessages", &onClearMessages),
  RPC_Callback("addMessage", &onAddMessage),
  RPC_Callback("reboot", &onReboot),
};

}  // namespace

namespace remote_test {

void begin() {
  if (!tb_client::rpcApi().RPC_Subscribe(std::begin(callbacks), std::end(callbacks))) {
    Serial.println("[RPC] Could not register test RPC methods");
  }
}

void loop() {
  if (rebootAt != 0 && (long)(millis() - rebootAt) >= 0) esp_restart();
}

}  // namespace remote_test
