#include "remote_test.h"

#include <Arduino.h>

#include "clock_mode.h"
#include "display_modes.h"
#include "messages.h"
#include "tb_client.h"

namespace {

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

const RPC_Callback callbacks[] = {
  RPC_Callback("nextMode", &onNextMode),
  RPC_Callback("clearMessages", &onClearMessages),
};

}  // namespace

namespace remote_test {

void begin() {
  if (!tb_client::rpcApi().RPC_Subscribe(std::begin(callbacks), std::end(callbacks))) {
    Serial.println("[RPC] Could not register test RPC methods");
  }
}

}  // namespace remote_test
