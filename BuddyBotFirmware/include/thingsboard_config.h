// ThingsBoard connection settings (non-secret). The device access token is
// secret and lives in include/secrets.h as TB_ACCESS_TOKEN.
// Names on the ThingsBoard side are listed in interface-contract.md, section 3.
#pragma once

#define TB_HOST "eu.thingsboard.cloud"

// MQTT over TLS. Server certificate is checked against the core's built-in CA bundle.
#define TB_PORT 8883

// Minimum time between reconnect attempts while WiFi is up but MQTT is down.
// Each attempt blocks the loop for the TLS handshake (~1-2 s).
#define TB_RECONNECT_INTERVAL_MS 10000
