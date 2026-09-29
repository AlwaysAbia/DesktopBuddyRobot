// The one persistent MQTT connection to ThingsBoard. Owns the SDK instance and
// every API implementation registered on it (OTA today, messaging later).
#pragma once

#include <Attribute_Request.h>
#include <OTA_Firmware_Update.h>
#include <Server_Side_RPC.h>
#include <ThingsBoard.h>

namespace tb_client {

// Our own shared-attribute request: 1 request in flight, up to 2 keys.
using AttributeRequestApi = Attribute_Request<1U, 2U>;

// Server-side RPC: up to MAX_RPC_METHODS methods, responses with up to 4 JSON fields.
// RPC_Subscribe() fails as a whole (no method works) if given more methods than this.
constexpr size_t MAX_RPC_METHODS = 8U;
using RpcApi = Server_Side_RPC<MAX_RPC_METHODS, 4U>;

// Connect (and reconnect, rate-limited) whenever WiFi is up. Call every loop.
void loop();

bool connected();

// Called once after every successful (re)connect, from inside loop().
void onConnected(void (*callback)());

ThingsBoard& tb();
OTA_Firmware_Update<>& otaApi();
AttributeRequestApi& attributeRequestApi();

// Methods subscribed here are re-subscribed by the SDK after every reconnect.
RpcApi& rpcApi();

}  // namespace tb_client
