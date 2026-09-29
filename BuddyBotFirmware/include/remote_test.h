// TEMPORARY test controls over ThingsBoard RPC, so display modes can be
// tested from the dashboard without serial access. Placeholders until BLE
// mode control (next session) and Phase 4 messaging replace them; delete this
// module (and its RPCs in interface-contract.md) then.
#pragma once

namespace remote_test {

// Registers the RPC methods. Safe to call before ThingsBoard is connected.
void begin();

}  // namespace remote_test
