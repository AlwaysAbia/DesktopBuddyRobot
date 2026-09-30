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
// Applies to any image installed by OTA.
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

// A freshly OTA-installed image stays PENDING_VERIFY until the first successful
// ThingsBoard connection (onTbConnected). If that takes longer than
// OTA_CONFIRM_TIMEOUT_S the bootloader is told to roll back.
bool awaitingConfirm = false;
unsigned long confirmStartedAt = 0;

// Last OTA problem, reported to ThingsBoard as an attribute. Cleared by a successful update.
String lastError;

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

// Did the last OTA we started actually end up running? Runs at boot, or (for a
// freshly installed image) once the image has been confirmed.
void settlePending() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  String pending = readKey(prefs, KEY_PENDING);
  if (!pending.isEmpty()) {
    if (pending == FIRMWARE_VERSION) {
      Serial.printf("[OTA] Update to %s succeeded\r\n", pending.c_str());
      if (prefs.isKey(KEY_BAD)) prefs.remove(KEY_BAD);
      bootReport = BootReport::Updated;
      lastError = "";
    } else {
      // Rolled back (or power was lost before reboot). Don't auto-retry it.
      Serial.printf("[OTA] Update to %s did not stick - still on %s. Marking %s as bad "
                    "(use 'ota force' to retry)\r\n",
                    pending.c_str(), FIRMWARE_VERSION, pending.c_str());
      prefs.putString(KEY_BAD, pending);
      bootReport = BootReport::RolledBack;
      rolledBackVersion = pending;
      lastError = pending + " did not boot; rolled back";
    }
    prefs.remove(KEY_PENDING);
  }
  prefs.end();
}

// Marks the running image valid (cancels the rollback) and settles the bookkeeping.
void confirmNow() {
#if OTA_TEST_SKIP_CONFIRM
  return;  // TEST ONLY: never confirm, so the confirm timeout rolls back
#endif
  if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
    Serial.printf("[OTA] New firmware %s marked valid after connecting to ThingsBoard (rollback cancelled)\r\n",
                  FIRMWARE_VERSION);
  } else {
    Serial.println("[OTA] ERROR: could not mark firmware valid");
  }
  awaitingConfirm = false;
  settlePending();
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
  lastError = "Download or flash of " + targetVersion + " failed";
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
    lastError = "Could not request assigned firmware";
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
    lastError = "Package title does not match device firmware title";
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
    lastError = error;
    otaApi.Firmware_Send_State(FW_STATE_FAILED, error.c_str());
    return;
  }

  targetVersion = assignedVersion;
  const OTA_Update_Callback callback(FIRMWARE_TITLE, FIRMWARE_VERSION, &updater, &onFinished, &onProgress,
                                     &onUpdateStarting, CHUNK_RETRIES, CHUNK_SIZE, REQUEST_TIMEOUT_US);
  if (!otaApi.Start_Firmware_Update(callback)) {
    Serial.println("[OTA] Could not start firmware update");
    lastError = "Could not start firmware update";
    return;
  }
  stage = Stage::Downloading;
}

// Runs once after every ThingsBoard (re)connect.
void onTbConnected() {
  auto& tb = tb_client::tb();
  auto& otaApi = tb_client::otaApi();

  // First successful ThingsBoard connection proves WiFi, TLS and the token work
  // in this build: only now keep a freshly installed image.
  if (awaitingConfirm) confirmNow();

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
    // Not marked valid yet: a crash or reset before the first ThingsBoard
    // connection makes the bootloader roll back. onTbConnected() confirms,
    // ota::loop() rolls back on timeout. The pending/bad bookkeeping waits for that.
    awaitingConfirm = true;
    confirmStartedAt = millis();
    Serial.printf("[OTA] New firmware %s runs unconfirmed: needs a ThingsBoard connection within %d s "
                  "or it rolls back\r\n", FIRMWARE_VERSION, OTA_CONFIRM_TIMEOUT_S);
    return;
  }
  settlePending();
}

void begin() {
  tb_client::onConnected(&onTbConnected);
}

bool requestCheck(bool force) {
  if (stage != Stage::Idle) {
    Serial.println("[OTA] A check/update is already in progress");
    return false;
  }
  forceInstall = force;
  stage = Stage::WaitingForConnection;
  if (!tb_client::connected()) Serial.println("[OTA] Check queued until ThingsBoard is connected");
  return true;
}

void loop() {
  if (awaitingConfirm && millis() - confirmStartedAt > (unsigned long)OTA_CONFIRM_TIMEOUT_S * 1000UL) {
    Serial.printf("[OTA] No ThingsBoard connection within %d s of booting %s - rolling back\r\n",
                  OTA_CONFIRM_TIMEOUT_S, FIRMWARE_VERSION);
    Serial.flush();
    esp_ota_mark_app_invalid_rollback_and_reboot();  // only returns on error
    Serial.println("[OTA] ERROR: rollback failed");
    awaitingConfirm = false;
  }
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

Info info() {
  Info out;
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  out.partition = running->label;
  out.imageState = esp_ota_get_state_partition(running, &state) == ESP_OK ? stateName(state) : "n/a";
  out.rollbackPossible = esp_ota_check_rollback_is_possible();
  out.awaitingConfirm = awaitingConfirm;
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);
  out.badVersion = readKey(prefs, KEY_BAD);
  prefs.end();
  out.lastError = lastError;
  return out;
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
  Serial.printf("Awaiting confirm : %s\r\n", awaitingConfirm ? "yes (needs ThingsBoard connection)" : "no");
  Serial.printf("Last OTA error   : %s\r\n", lastError.isEmpty() ? "-" : lastError.c_str());
  Serial.printf("ThingsBoard      : %s (%s)\r\n", TB_HOST, tb_client::connected() ? "connected" : "not connected");
  Serial.printf("Last assigned    : %s\r\n", assignedVersion[0] ? assignedVersion : "-");
  Serial.println("--------------------------------");
}

}  // namespace ota
