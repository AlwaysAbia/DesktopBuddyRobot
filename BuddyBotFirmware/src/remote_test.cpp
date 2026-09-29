#include "remote_test.h"

#include <Arduino.h>

#include "clock_mode.h"
#include "display_modes.h"
#include "led_matrix.h"
#include "led_text.h"
#include "messages.h"
#include "tb_client.h"

namespace {

unsigned long rebootAt = 0;  // 0 = no reboot pending
bool testRows = false;       // wiring test pattern instead of the current mode

// Reads an integer RPC parameter however the dashboard sent it: an object
// {"key": 1}, a bare number 1, a bare bool, or text ("1" or '{"key": 1}').
// Returns fallback if none of those fit.
int intParam(JsonVariantConst params, const char* key, int fallback) {
  if (params.is<JsonObjectConst>()) return params[key] | fallback;
  if (params.is<int>()) return params.as<int>();
  if (params.is<bool>()) return params.as<bool>() ? 1 : 0;
  if (params.is<const char*>()) {
    const char* text = params.as<const char*>();
    if (text == nullptr) return fallback;
    StaticJsonDocument<96> doc;
    if (deserializeJson(doc, text) == DeserializationError::Ok) {
      if (doc.is<JsonObject>()) return doc[key] | fallback;
      if (doc.is<int>()) return doc.as<int>();
      if (doc.is<bool>()) return doc.as<bool>() ? 1 : 0;
    }
  }
  return fallback;
}

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

// RPC "ledTest": params {"on": true|false}. Shows the wiring test pattern until
// switched off (or the next reboot).
void onLedTest(JsonVariantConst const& params, JsonDocument& response) {
  testRows = intParam(params, "on", testRows ? 0 : 1) != 0;
  Serial.printf("[RPC] ledTest %d\r\n", (int)testRows);
  fillState(response);
  response["ledTest"] = testRows;
}

// RPC "textStyle": params {"style": 0|1|2} = smooth scroll / stepped scroll / one letter at a time.
void onTextStyle(JsonVariantConst const& params, JsonDocument& response) {
  int style = intParam(params, "style", -1);
  if (style >= 0 && style < led_text::STYLE_COUNT) led_text::setStyle((led_text::Style)style);
  Serial.printf("[RPC] textStyle %d\r\n", (int)led_text::style());
  fillState(response);
  response["textStyle"] = (int)led_text::style();
  response["got"] = style;  // what was parsed from the params (-1 = nothing usable)
}

const RPC_Callback callbacks[] = {
  RPC_Callback("nextMode", &onNextMode),
  RPC_Callback("clearMessages", &onClearMessages),
  RPC_Callback("addMessage", &onAddMessage),
  RPC_Callback("reboot", &onReboot),
  RPC_Callback("ledTest", &onLedTest),
  RPC_Callback("textStyle", &onTextStyle),
};

static_assert(sizeof(callbacks) / sizeof(callbacks[0]) <= tb_client::MAX_RPC_METHODS,
              "more RPC methods than tb_client::MAX_RPC_METHODS: none would register");

}  // namespace

namespace remote_test {

void begin() {
  if (!tb_client::rpcApi().RPC_Subscribe(std::begin(callbacks), std::end(callbacks))) {
    Serial.println("[RPC] Could not register test RPC methods");
  }
}

bool render() {
  if (!testRows) return false;
  led_matrix::renderTestRows();
  return true;
}

void loop() {
  if (rebootAt != 0 && (long)(millis() - rebootAt) >= 0) esp_restart();
}

}  // namespace remote_test
