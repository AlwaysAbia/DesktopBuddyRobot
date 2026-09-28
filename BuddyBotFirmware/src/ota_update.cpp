#include "ota_update.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <Espressif_Updater.h>

#include "ota_config.h"
#include "tb_client.h"
#include "thingsboard_config.h"

// ==========================================
// ROLLBACK
// The prebuilt arduino-esp32 core has CONFIG_APP_ROLLBACK_ENABLE=y, but by
// default initArduino() marks a freshly OTA'd image valid before setup() even
// runs, which makes rollback useless. Overriding this weak hook to return true
// hands that decision to us: ota::confirmRunningFirmware().
// Applies to every OTA path (ThingsBoard OTA and ArduinoOTA pushes alike).
// ==========================================
extern "C" bool verifyRollbackLater() {
  return true;
}

namespace {

// NVS bookkeeping so a rolled-back version isn't re-downloaded forever.
// Keys are listed in interface-contract.md, section 2.
const char* NVS_NAMESPACE = "ota";
const char* KEY_PENDING   = "pending";  // version we just flashed, cleared on next boot
const char* KEY_BAD       = "bad";      // version that failed to boot (rolled back)

// Firmware chunk download over MQTT.
constexpr uint8_t  CHUNK_RETRIES      = 12U;
constexpr uint16_t CHUNK_SIZE         = 4096U;
constexpr uint64_t REQUEST_TIMEOUT_US = 10ULL * 1000ULL * 1000ULL;

// ThingsBoard-defined shared attributes describing the assigned OTA package.
constexpr char FW_TITLE_KEY[]   = "fw_title";
constexpr char FW_VERSION_KEY[] = "fw_version";
constexpr const char* ASSIGNED_KEYS[] = {FW_TITLE_KEY, FW_VERSION_KEY};

// Client attributes this device reports (interface-contract.md, section 3).
constexpr char ATTR_CURRENT_TITLE[]   = "current_fw_title";
constexpr char ATTR_CURRENT_VERSION[] = "current_fw_version";

enum class Stage { Idle, WaitingForConnection, WaitingForAttributes, Downloading };

Stage stage = Stage::Idle;
bool forceInstall = false;
Espressif_Updater<> updater;

// Filled from the attribute response (tb.loop() context) or timeout (esp_timer task).
volatile bool attributesReceived = false;
volatile bool attributesTimedOut = false;
char assignedTitle[48]   = "";
char assignedVersion[32] = "";
String targetVersion;

// Result of the previous OTA, determined at boot, reported once ThingsBoard is up.
enum class BootReport { None, Updated, RolledBack };
BootReport bootReport = BootReport::None;
String rolledBackVersion;

// Preferences::getString() logs an error for a missing key; check first.
String readKey(Preferences& prefs, const char* key) {
  return prefs.isKey(key) ? prefs.getString(key, "") : String();
}

const char* stateName(esp_ota_img_states_t state) {
  switch (state) {
    case ESP_OTA_IMG_NEW:            return "NEW";
    case ESP_OTA_IMG_PENDING_VERIFY: return "PENDING_VERIFY";
    case ESP_OTA_IMG_VALID:          return "VALID";
    case ESP_OTA_IMG_INVALID:        return "INVALID";
    case ESP_OTA_IMG_ABORTED:        return "ABORTED";
    case ESP_OTA_IMG_UNDEFINED:      return "UNDEFINED";
    default:                         return "UNKNOWN";
  }
}

void clearPending() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  if (prefs.isKey(KEY_PENDING)) prefs.remove(KEY_PENDING);
  prefs.end();
}

// ---------- SDK callbacks ----------

void onAttributesReceived(JsonObjectConst const& data) {
  strlcpy(assignedTitle, data[FW_TITLE_KEY] | "", sizeof(assignedTitle));
  strlcpy(assignedVersion, data[FW_VERSION_KEY] | "", sizeof(assignedVersion));
  attributesReceived = true;
}

void onAttributesTimeout() {
  attributesTimedOut = true;  // esp_timer task: only set a flag
}

void onUpdateStarting() {
  // Recorded before the first byte is written, so a rollback can be detected on next boot.
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString(KEY_PENDING, targetVersion);
  prefs.end();
  Serial.printf("[OTA] Downloading %s %s from ThingsBoard\r\n", FIRMWARE_TITLE, targetVersion.c_str());
}

void onProgress(size_t const& current, size_t const& total) {
  static int lastDecile = -1;
  if (total == 0) return;
  int decile = (int)(current * 10 / total);
  if (current <= 1) lastDecile = -1;
  if (decile != lastDecile) {
    lastDecile = decile;
    Serial.printf("[OTA] Progress: %d%%\r\n", decile * 10);
  }
}

void onFinished(bool const& success) {
  if (success) {
    // SDK has already verified the checksum and set the boot partition.
    Serial.printf("[OTA] Flashed %s - rebooting\r\n", targetVersion.c_str());
    Serial.flush();
    delay(300);
    esp_restart();
  }
  Serial.println("[OTA] Update failed - still running " FIRMWARE_VERSION " (state reported to ThingsBoard)");
  clearPending();
  stage = Stage::Idle;
}

// ---------- check flow ----------

void sendAttributeRequest() {
  attributesReceived = false;
  attributesTimedOut = false;
  const Attribute_Request_Callback<2U> callback(&onAttributesReceived, REQUEST_TIMEOUT_US, &onAttributesTimeout,
                                                ASSIGNED_KEYS + 0U, ASSIGNED_KEYS + 2U);
  if (!tb_client::attributeRequestApi().Shared_Attributes_Request(callback)) {
    Serial.println("[OTA] Could not request assigned firmware from ThingsBoard");
    stage = Stage::Idle;
    return;
  }
  Serial.println("[OTA] Asking ThingsBoard which firmware is assigned ...");
  stage = Stage::WaitingForAttributes;
}

void evaluateAssignment() {
  auto& otaApi = tb_client::otaApi();
  stage = Stage::Idle;

  if (assignedVersion[0] == '\0') {
    Serial.println("[OTA] No firmware package assigned to this device (or its profile) in ThingsBoard");
    return;
  }
  Serial.printf("[OTA] Running %s %s, ThingsBoard assigns %s %s\r\n", FIRMWARE_TITLE, FIRMWARE_VERSION,
                assignedTitle, assignedVersion);

  if (strcmp(assignedTitle, FIRMWARE_TITLE) != 0) {
    Serial.printf("[OTA] Package title \"%s\" is not \"%s\" - ignoring\r\n", assignedTitle, FIRMWARE_TITLE);
    otaApi.Firmware_Send_State(FW_STATE_FAILED, "Package title does not match device firmware title");
    return;
  }
  if (strcmp(assignedVersion, FIRMWARE_VERSION) == 0) {
    Serial.println("[OTA] Already up to date");
    otaApi.Firmware_Send_State(FW_STATE_UPDATED);
    return;
  }

  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);
  String bad = readKey(prefs, KEY_BAD);
  prefs.end();
  if (!forceInstall && bad == assignedVersion) {
    Serial.printf("[OTA] %s failed to boot before - skipping (use 'ota force' to retry)\r\n", assignedVersion);
    String error = String(assignedVersion) + " previously failed to boot and was rolled back; not retrying";
    otaApi.Firmware_Send_State(FW_STATE_FAILED, error.c_str());
    return;
  }

  targetVersion = assignedVersion;
  const OTA_Update_Callback callback(FIRMWARE_TITLE, FIRMWARE_VERSION, &updater, &onFinished, &onProgress,
                                     &onUpdateStarting, CHUNK_RETRIES, CHUNK_SIZE, REQUEST_TIMEOUT_US);
  if (!otaApi.Start_Firmware_Update(callback)) {
    Serial.println("[OTA] Could not start firmware update");
    return;
  }
  stage = Stage::Downloading;
}

// Runs once after every ThingsBoard (re)connect.
void onTbConnected() {
  auto& tb = tb_client::tb();
  auto& otaApi = tb_client::otaApi();

  // current_fw_* as telemetry is what ThingsBoard's OTA dashboard reads;
  // the same keys as client attributes are our stable "what is running" record.
  otaApi.Firmware_Send_Info(FIRMWARE_TITLE, FIRMWARE_VERSION);
  tb.sendAttributeData(ATTR_CURRENT_TITLE, FIRMWARE_TITLE);
  tb.sendAttributeData(ATTR_CURRENT_VERSION, FIRMWARE_VERSION);
  Serial.printf("[TB] Reported %s %s\r\n", FIRMWARE_TITLE, FIRMWARE_VERSION);

  if (bootReport == BootReport::Updated) {
    otaApi.Firmware_Send_State(FW_STATE_UPDATED);
    Serial.println("[TB] Reported fw_state UPDATED");
  } else if (bootReport == BootReport::RolledBack) {
    String error = rolledBackVersion + " did not boot; rolled back to " FIRMWARE_VERSION;
    otaApi.Firmware_Send_State(FW_STATE_FAILED, error.c_str());
    Serial.println("[TB] Reported fw_state FAILED (rollback)");
  }
  bootReport = BootReport::None;
}

}  // namespace

namespace ota {

void confirmRunningFirmware() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  bool hasState = esp_ota_get_state_partition(running, &state) == ESP_OK;

  if (hasState && state == ESP_OTA_IMG_PENDING_VERIFY) {
#if OTA_TEST_CRASH_BEFORE_VALID
    Serial.println("[OTA] TEST: crashing before marking image valid - expect rollback");
    Serial.flush();
    abort();
#endif
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
      Serial.printf("[OTA] New firmware %s marked valid (rollback cancelled)\r\n", FIRMWARE_VERSION);
    } else {
      Serial.println("[OTA] ERROR: could not mark firmware valid");
    }
  }

  // Did the last OTA we started actually end up running?
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  String pending = readKey(prefs, KEY_PENDING);
  if (!pending.isEmpty()) {
    if (pending == FIRMWARE_VERSION) {
      Serial.printf("[OTA] Update to %s succeeded\r\n", pending.c_str());
      if (prefs.isKey(KEY_BAD)) prefs.remove(KEY_BAD);
      bootReport = BootReport::Updated;
    } else {
      // Rolled back (or power was lost before reboot). Don't auto-retry it.
      Serial.printf("[OTA] Update to %s did not stick - still on %s. Marking %s as bad "
                    "(use 'ota force' to retry)\r\n",
                    pending.c_str(), FIRMWARE_VERSION, pending.c_str());
      prefs.putString(KEY_BAD, pending);
      bootReport = BootReport::RolledBack;
      rolledBackVersion = pending;
    }
    prefs.remove(KEY_PENDING);
  }
  prefs.end();
}

void begin() {
  tb_client::onConnected(&onTbConnected);
}

void requestCheck(bool force) {
  if (stage != Stage::Idle) {
    Serial.println("[OTA] A check/update is already in progress");
    return;
  }
  forceInstall = force;
  stage = Stage::WaitingForConnection;
  if (!tb_client::connected()) Serial.println("[OTA] Check queued until ThingsBoard is connected");
}

void loop() {
  switch (stage) {
    case Stage::WaitingForConnection:
      if (tb_client::connected()) sendAttributeRequest();
      break;
    case Stage::WaitingForAttributes:
      if (attributesReceived) {
        evaluateAssignment();
      } else if (attributesTimedOut) {
        // Treat no answer like "nothing assigned".
        assignedTitle[0] = '\0';
        assignedVersion[0] = '\0';
        evaluateAssignment();
      }
      break;
    default:
      break;
  }
}

bool isUpdating() {
  return stage == Stage::Downloading;
}

void printStatus() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* next = esp_ota_get_next_update_partition(NULL);
  esp_ota_img_states_t state;
  bool hasState = esp_ota_get_state_partition(running, &state) == ESP_OK;

  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);
  String bad = readKey(prefs, KEY_BAD);
  prefs.end();

  Serial.println("---------- OTA status ----------");
  Serial.printf("Firmware         : %s %s\r\n", FIRMWARE_TITLE, FIRMWARE_VERSION);
  Serial.printf("Running partition: %s @ 0x%06x (state: %s)\r\n", running->label, (unsigned)running->address,
                hasState ? stateName(state) : "n/a");
  Serial.printf("Next OTA slot    : %s\r\n", next ? next->label : "none");
  Serial.printf("Rollback possible: %s\r\n", esp_ota_check_rollback_is_possible() ? "yes" : "no");
  Serial.printf("Bad version      : %s\r\n", bad.isEmpty() ? "-" : bad.c_str());
  Serial.printf("ThingsBoard      : %s (%s)\r\n", TB_HOST, tb_client::connected() ? "connected" : "not connected");
  Serial.printf("Last assigned    : %s\r\n", assignedVersion[0] ? assignedVersion : "-");
  Serial.println("--------------------------------");
}

}  // namespace ota
