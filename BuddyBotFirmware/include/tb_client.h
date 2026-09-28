// The one persistent MQTT connection to ThingsBoard. Owns the SDK instance and
// every API implementation registered on it (OTA today, messaging later).
#pragma once

#include <Attribute_Request.h>
#include <OTA_Firmware_Update.h>
#include <ThingsBoard.h>

namespace tb_client {

// Our own shared-attribute request: 1 request in flight, up to 2 keys.
using AttributeRequestApi = Attribute_Request<1U, 2U>;

// Connect (and reconnect, rate-limited) whenever WiFi is up. Call every loop.
void loop();

bool connected();

// Called once after every successful (re)connect, from inside loop().
void onConnected(void (*callback)());

ThingsBoard& tb();
OTA_Firmware_Update<>& otaApi();
AttributeRequestApi& attributeRequestApi();

}  // namespace tb_client
