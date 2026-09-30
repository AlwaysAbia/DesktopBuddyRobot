// Permanent remote-operation features over ThingsBoard: the checkForUpdate RPC and
// device status reporting (attributes + telemetry) so nothing needs the serial port.
// Keys and method names: interface-contract.md, section 3.
#pragma once

namespace remote_ops {

// Registers the RPC methods. Call once in setup(), after ota::begin().
void begin();

// Sends status on every (re)connect, telemetry every REPORT_INTERVAL_MS, and
// attributes whenever they change. Call every loop, after tb_client::loop().
void loop();

}  // namespace remote_ops
