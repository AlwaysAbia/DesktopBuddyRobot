#include <FastLED.h>
#include <math.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// ==========================================
// WI-FI & OTA CONFIGURATION
// ==========================================
const char* ssid     = ""; 
const char* password = "";     
const char* otaHost  = "ESP32-SciFi-Eye";

// ==========================================
// HARDWARE & FASTLED CONFIGURATION
// ==========================================
#define DATA_PIN     16   // GPIO 16
#define NUM_LEDS     76
#define BRIGHTNESS   120
#define FPS          60

CRGB leds[NUM_LEDS];

struct Point2D {
  float x; // Horizontal in Visual Space
  float y; // Vertical in Visual Space
};

Point2D ledCoords[NUM_LEDS];

const int NUM_ROWS = 10;
const int rowLengths[NUM_ROWS] = {4, 6, 8, 10, 10, 10, 10, 8, 6, 4};
const bool IS_SERPENTINE = true; 

void buildCoordinateMap() {
  int ledIndex = 0;

  // Rotated angle (-135.0f) for matrix alignment
  const float rad = -135.0f * (M_PI / 180.0f); 
  const float cosA = cosf(rad);
  const float sinA = sinf(rad);

  for (int r = 0; r < NUM_ROWS; r++) {
    int count = rowLengths[r];
    float rawY = 1.0f - (2.0f * (r + 0.5f) / (float)NUM_ROWS);

    for (int c = 0; c < count; c++) {
      // Handle serpentine / zigzag row wiring direction
      int colIndex = (IS_SERPENTINE && (r % 2 != 0)) ? (count - 1 - c) : c;

      float rawX = -1.0f + (2.0f * (colIndex + 0.5f) / (float)count);

      rawX *= 0.95f;
      rawY *= 0.95f;

      // Rotational matrix transform
      float rotatedX = rawX * cosA - rawY * sinA;
      float rotatedY = rawX * sinA + rawY * cosA;

      ledCoords[ledIndex].x = rotatedX;
      ledCoords[ledIndex].y = rotatedY;

      ledIndex++;
    }
  }
}

// ==========================================
// COLOR PALETTES & ANIMATION STATES
// ==========================================
struct EyePalette {
  CRGB irisCore;
  CRGB irisEdge;
  CRGB eyelidEdge;
};

// ACTIVE PALETTE: Amber / Orange
const EyePalette AMBER_THEME = { CRGB(255, 140, 0), CRGB(120, 20, 0), CRGB(255, 200, 100) };

/* OTHER PALETTES (SAVED FOR FUTURE USE):
const EyePalette THEMES[4] = {
  { CRGB(0, 255, 255), CRGB(0, 30, 120), CRGB(180, 255, 255) }, // Cyan
  { CRGB(255, 140, 0), CRGB(120, 20, 0),  CRGB(255, 200, 100) }, // Amber
  { CRGB(0, 255, 100), CRGB(0, 80, 20),   CRGB(150, 255, 180) }, // Emerald
  { CRGB(255, 0, 150), CRGB(80, 0, 80),   CRGB(255, 160, 220) }  // Magenta
};
uint8_t currentTheme = 0;
unsigned long lastThemeChange = 0;
*/

float currentPupilX = 0.0f, currentPupilY = 0.0f;
float targetPupilX  = 0.0f, targetPupilY  = 0.0f;

unsigned long lastGazeChange = 0;
unsigned long gazeInterval   = 2000;

enum BlinkState { IDLE, CLOSING, CLOSED, OPENING };
BlinkState blinkState = IDLE;
float blinkProgress   = 0.0f;
unsigned long lastBlinkCheck = 0;
unsigned long nextBlinkTime  = 3000;

void updateGazeTarget() {
  if (millis() - lastGazeChange > gazeInterval) {
    float angle = (random(0, 360) * M_PI) / 180.0f;
    float dist  = (random(0, 100) / 100.0f) * 0.40f; 
    
    targetPupilX = cosf(angle) * dist;
    targetPupilY = sinf(angle) * dist;

    gazeInterval   = random(1200, 3500);
    lastGazeChange = millis();
  }

  currentPupilX += (targetPupilX - currentPupilX) * 0.08f;
  currentPupilY += (targetPupilY - currentPupilY) * 0.08f;
}

void updateBlinkAnimation() {
  unsigned long now = millis();

  switch (blinkState) {
    case IDLE:
      if (now - lastBlinkCheck > nextBlinkTime) {
        blinkState    = CLOSING;
        lastBlinkCheck = now;
      }
      break;

    case CLOSING:
      blinkProgress += 0.12f;
      if (blinkProgress >= 1.0f) {
        blinkProgress = 1.0f;
        blinkState    = CLOSED;
        lastBlinkCheck = now;
      }
      break;

    case CLOSED:
      if (now - lastBlinkCheck > 60) {
        blinkState = OPENING;
      }
      break;

    case OPENING:
      blinkProgress -= 0.10f;
      if (blinkProgress <= 0.0f) {
        blinkProgress  = 0.0f;
        blinkState     = IDLE;
        lastBlinkCheck = now;
        nextBlinkTime  = random(2000, 6000);
      }
      break;
  }
}

void renderEyeFrame() {
  /* AUTOMATIC THEME SWITCHING (COMMENTED OUT FOR NOW)
  if (millis() - lastThemeChange > 5000) {
    currentTheme = (currentTheme + 1) % 4;
    lastThemeChange = millis();
  }
  const EyePalette& theme = THEMES[currentTheme];
  */

  const EyePalette& theme = AMBER_THEME; 

  const float pupilRadius = 0.28f;
  const float irisRadius  = 1.05f; 

  const float glint1X = currentPupilX + 0.10f;
  const float glint1Y = currentPupilY + 0.10f;
  const float glint2X = currentPupilX - 0.08f;
  const float glint2Y = currentPupilY - 0.08f;

  float eyelidCutoffY = (1.0f - blinkProgress) * 0.95f;

  for (int i = 0; i < NUM_LEDS; i++) {
    float x = ledCoords[i].x;
    float y = ledCoords[i].y;

    // Eyelid horizontal cutoff
    float absY = fabsf(y);
    if (absY >= eyelidCutoffY) {
      leds[i] = CRGB::Black;

      if (absY < eyelidCutoffY + 0.20f && blinkProgress > 0.05f) {
        leds[i] = theme.eyelidEdge;
        leds[i].nscale8(200);
      }
      continue;
    }

    if (blinkProgress >= 0.92f && absY < 0.18f) {
      leds[i] = theme.eyelidEdge;
      continue;
    }

    // Radial rendering
    float dx = x - currentPupilX;
    float dy = y - currentPupilY;
    float distToPupil = sqrtf(dx * dx + dy * dy);

    // Primary Specular Glint
    float dGlint1 = sqrtf((x - glint1X) * (x - glint1X) + (y - glint1Y) * (y - glint1Y));
    if (dGlint1 < 0.08f) {
      leds[i] = CRGB::White;
      continue;
    }

    // Secondary Specular Glint
    float dGlint2 = sqrtf((x - glint2X) * (x - glint2X) + (y - glint2Y) * (y - glint2Y));
    if (dGlint2 < 0.05f) {
      leds[i] = CRGB(180, 180, 180);
      continue;
    }

    // Pupil
    if (distToPupil <= pupilRadius) {
      leds[i] = CRGB::Black;
    } 
    // Iris
    else if (distToPupil <= irisRadius) {
      float t = (distToPupil - pupilRadius) / (irisRadius - pupilRadius);
      t = constrain(t, 0.0f, 1.0f);

      CRGB color = blend(theme.irisCore, theme.irisEdge, uint8_t(t * 255.0f));

      if (t > 0.85f) {
        float rimFactor = (1.0f - t) / 0.15f;
        color.nscale8(uint8_t(rimFactor * 255.0f));
      }

      leds[i] = color;
    } 
    else {
      leds[i] = CRGB::Black;
    }
  }
}

// Helper function to translate status codes into text
const char* getWiFiStatusName(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:     return "IDLE_STATUS";
    case WL_NO_SSID_AVAIL:   return "NO_SSID_AVAIL (SSID not found)";
    case WL_SCAN_COMPLETED:  return "SCAN_COMPLETED";
    case WL_CONNECTED:       return "CONNECTED";
    case WL_CONNECT_FAILED:  return "CONNECT_FAILED (Wrong password/Security mismatch)";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED:    return "DISCONNECTED";
    default:                 return "UNKNOWN";
  }
}

// ==========================================
// OTA & WI-FI INITIALIZATION WITH LOGGING
// ==========================================
void setupOTA() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n==========================================");
  Serial.println("   ESP32 Sci-Fi Robot Eye Booting");
  Serial.println("==========================================");

  // 1. Fully disconnect and clear lingering state
  WiFi.disconnect(true);
  delay(100);

  // 2. Configure Station Mode & Disable Modem Sleep
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // Prevents dropouts on Android hotspots

  Serial.print("[WiFi] Attempting connection to SSID: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  // 3. Connection Handshake Loop with Detailed Debug Prints
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) { // 20-second window
    delay(500);
    Serial.print(".");
    
    // Print current status code every 5 seconds
    if (attempts > 0 && attempts % 10 == 0) {
      Serial.printf("\n[WiFi Handshake Status]: %s\n", getWiFiStatusName(WiFi.status()));
    }
    attempts++;
  }

  // 4. Verify Connection Result
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n\n[WiFi] CONNECTION SUCCESSFUL!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WiFi] Signal Strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

    // Configure ArduinoOTA
    ArduinoOTA.setHostname(otaHost);

    ArduinoOTA.onStart([]() {
      Serial.println("[OTA] Firmware update starting...");
      FastLED.clear(true);
    });

    ArduinoOTA.onEnd([]() {
      Serial.println("\n[OTA] Update Complete! Rebooting...");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("[OTA] Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

    ArduinoOTA.begin();
    Serial.println("[OTA] Service initialized and listening for updates.");
  } else {
    Serial.println("\n\n[WiFi] CONNECTION FAILED!");
    Serial.printf("[WiFi] Final Reason: %s\n", getWiFiStatusName(WiFi.status()));
    Serial.println("[WiFi] Proceeding to run animation in OFFLINE mode.");
  }
  Serial.println("==========================================\n");
}

// ==========================================
// MAIN SETUP & LOOP
// ==========================================
void setup() {
  // Initialize FastLED (LEDs remain OFF during WiFi setup to prevent brownout)
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear(true);

  // Pre-calculate 2D matrix map
  buildCoordinateMap();

  // Initialize WiFi and OTA
  setupOTA();
}

void loop() {
  // Check for wireless updates
  ArduinoOTA.handle();

  // Run render pipeline
  updateGazeTarget();
  updateBlinkAnimation();
  renderEyeFrame();

  FastLED.show();
  FastLED.delay(1000 / FPS);
}