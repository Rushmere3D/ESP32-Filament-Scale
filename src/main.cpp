
#include <Arduino.h>
#include <HX711.h>
#include <Preferences.h>
#include <WiFi.h>
#include <math.h>
#include <WebServer.h>

// -------------------- HX711 --------------------
constexpr uint8_t HX711_DOUT = 16;
constexpr uint8_t HX711_SCK = 17;
constexpr float DEFAULT_CALIBRATION = 422.6200f;
constexpr uint32_t REPORT_INTERVAL_MS = 250;

// -------------------- Wi-Fi --------------------
// Set Wi-Fi credentials in include/secrets.h (not committed to Git).
#include "secrets.h"

constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 10000;

// -------------------- Globals --------------------
HX711 scale;
Preferences prefs;
WebServer server(80);
float calibrationFactor = DEFAULT_CALIBRATION;
String commandBuffer;
uint32_t lastReportMs = 0;
float latestWeight = 0.0f;
uint32_t lastWeightMs = 0;
bool weightValid = false;
uint32_t lastWifiAttemptMs = 0;

bool ready(uint32_t timeoutMs = 1500) {
  if (scale.wait_ready_timeout(timeoutMs)) return true;
  Serial.println("[ERROR] HX711 not ready; check wiring.");
  return false;
}

void tareScale() {
  weightValid = false;
  if (!ready()) return;
  Serial.println("Keep the platform unloaded and still. Taring...");
  scale.tare(20);
  Serial.println("[OK] Tare complete.");
}

void calibrate(float knownGrams) {
  weightValid = false;
  if (!isfinite(knownGrams) || knownGrams <= 0.0f || knownGrams > 5000.0f) {
    Serial.println("[ERROR] Enter a known mass between 0 and 5000 g, e.g. CAL 200");
    return;
  }

  if (!ready()) return;

  Serial.println("Keep the known weight on the platform and still...");

  const float netCounts = (float)scale.get_value(20);

  if (!isfinite(netCounts) || fabsf(netCounts) < 100.0f) {
    Serial.println("[ERROR] Signal too small. TARE unloaded, then add the weight.");
    return;
  }

  const float candidate = netCounts / knownGrams;

  if (!isfinite(candidate) || fabsf(candidate) < 0.01f) {
    Serial.println("[ERROR] Invalid calibration factor; unchanged.");
    return;
  }

  calibrationFactor = candidate;
  scale.set_scale(calibrationFactor);
  prefs.putFloat("factor", calibrationFactor);

  Serial.printf(
    "[OK] Calibration factor saved: %.4f counts/g\n",
    calibrationFactor
  );
}

// -------------------- Wi-Fi --------------------

void connectWiFi() {
  Serial.printf("[WiFi] Connecting to %s...\n", WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  lastWifiAttemptMs = millis();
}

void maintainWiFi() {
  static bool wasConnected = false;

  if (WiFi.status() == WL_CONNECTED) {
    if (!wasConnected) {
      Serial.println("[WiFi] Connected!");
      Serial.print("[WiFi] IP address: ");
      Serial.println(WiFi.localIP());
      Serial.print("[WiFi] Signal strength: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
    }

    wasConnected = true;
    return;
  }

  if (wasConnected) {
    Serial.println("[WiFi] Connection lost.");
    wasConnected = false;
  }

  if (millis() - lastWifiAttemptMs >= WIFI_RETRY_INTERVAL_MS) {
    Serial.println("[WiFi] Retrying connection...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    lastWifiAttemptMs = millis();
  }
}

// -------------------- Serial commands --------------------

void processCommand(String command) {
  command.trim();
  command.toUpperCase();

  if (command == "TARE") {
    tareScale();
  } else if (command.startsWith("CAL ")) {
    String argument = command.substring(4);
    argument.trim();
    calibrate(argument.toFloat());
  } else if (command == "FACTOR") {
    Serial.printf(
      "Calibration factor: %.4f counts/g\n",
      calibrationFactor
    );
  } else if (command == "WIFI") {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[WiFi] Connected");
      Serial.print("[WiFi] IP address: ");
      Serial.println(WiFi.localIP());
      Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
    } else {
      Serial.println("[WiFi] Not connected");
    }
  } else if (command == "HELP") {
    Serial.println("Commands: TARE | CAL 200 | FACTOR | WIFI | HELP");
  } else if (command.length()) {
    Serial.println("Unknown command. Type HELP.");
  }
}

// -------------------- Setup --------------------

void handleWeightRequest() {
  if (!weightValid || millis() - lastWeightMs > 3000) {
    server.send(503, "application/json",
                "{\"error\":\"No recent weight reading\"}");
    return;
  }

  String json = "{";
  json += "\"weight\":" + String(latestWeight, 1) + ",";
  json += "\"unit\":\"g\",";
  json += "\"calibration\":" + String(calibrationFactor, 4);
  json += "}";

  server.send(200, "application/json", json);
}

void handleTareRequest()
{
    Serial.println("[HTTP] Remote TARE requested");

    // Prevent the weight API from returning an old reading.
    weightValid = false;

    // Check the HX711 before attempting tare.
    if (!scale.wait_ready_timeout(1500))
    {
        Serial.println("[HTTP] TARE failed: HX711 not ready");
        server.send(503, "application/json",
                    "{\"success\":false,\"error\":\"HX711 not ready\"}");
        return;
    }

    // Perform tare using the existing calibration and offset.
    tareScale();

    // The normal measurement loop will refresh the weight cache.
    server.send(200, "application/json",
                "{\"success\":true,\"message\":\"Tare complete\"}");

    Serial.println("[HTTP] Remote TARE complete");
}


// Stage 6B: calibration and information HTTP API.
// Calibration is intentionally synchronous: WebServer handles one request at a time.
void handleInfoRequest() {
  String json = "{\"firmware\":\"stage-6b\",\"calibration\":";
  json += String(calibrationFactor, 4);
  json += ",\"unit\":\"counts/g\",\"min_known_grams\":0.01,\"max_known_grams\":5000}";
  server.send(200, "application/json", json);
}

void handleCalibrationRequest() {
  // Strict numeric validation: reject empty, malformed, and non-finite inputs.
  if (!server.hasArg("grams")) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing grams parameter\"}");
    return;
  }
  const String arg = server.arg("grams");
  if (arg.isEmpty() || arg.length() > 16) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid grams parameter\"}");
    return;
  }
  char *end = nullptr;
  const float knownGrams = strtof(arg.c_str(), &end);
  if (end == arg.c_str() || *end != '\0' || !isfinite(knownGrams) ||
      knownGrams <= 0.0f || knownGrams > 5000.0f) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Known mass must be greater than 0 and at most 5000 g\"}");
    return;
  }
  weightValid = false;
  if (!scale.wait_ready_timeout(1500)) {
    server.send(503, "application/json", "{\"success\":false,\"error\":\"HX711 not ready\"}");
    return;
  }
  const float netCounts = (float)scale.get_value(20);
  if (!isfinite(netCounts) || fabsf(netCounts) < 100.0f) {
    server.send(422, "application/json", "{\"success\":false,\"error\":\"Signal too small; tare unloaded then place known mass\"}");
    return;
  }
  const float candidate = netCounts / knownGrams;
  if (!isfinite(candidate) || fabsf(candidate) < 0.01f || fabsf(candidate) >= 1000000.0f) {
    server.send(422, "application/json", "{\"success\":false,\"error\":\"Invalid calibration factor\"}");
    return;
  }
  // Save first, so a storage failure does not silently change calibration.
  const size_t saved = prefs.putFloat("factor", candidate);
  if (saved != sizeof(float)) {
    server.send(500, "application/json", "{\"success\":false,\"error\":\"Failed to save calibration\"}");
    return;
  }
  calibrationFactor = candidate;
  scale.set_scale(calibrationFactor);
  Serial.printf("[HTTP] Calibration saved: %.4f counts/g using %.2f g\\n", calibrationFactor, knownGrams);
  String json = "{\"success\":true,\"calibration\":";
  json += String(calibrationFactor, 4);
  json += ",\"unit\":\"counts/g\"}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  delay(1200);

  Serial.println("\n=== FILAMENT SCALE: WIFI TEST ===");

  prefs.begin("filament-scale", false);

  float stored = prefs.getFloat("factor", DEFAULT_CALIBRATION);

  if (isfinite(stored) &&
      fabsf(stored) >= 0.01f &&
      fabsf(stored) < 1000000.0f) {
    calibrationFactor = stored;
  }

  scale.begin(HX711_DOUT, HX711_SCK);
  scale.set_scale(calibrationFactor);

  Serial.printf(
    "Calibration factor: %.4f counts/g\n",
    calibrationFactor
  );

  Serial.println("Commands: TARE | CAL 200 | FACTOR | WIFI | HELP");

  if (ready(3000)) {
    Serial.println("Keep platform empty; initial tare in 3 seconds...");
    delay(3000);
    tareScale();
  }

  connectWiFi();

  server.on("/api/weight", HTTP_GET, handleWeightRequest);
  server.on("/api/tare", HTTP_POST, handleTareRequest);
  server.on("/api/info", HTTP_GET, handleInfoRequest);
  server.on("/api/calibrate", HTTP_POST, handleCalibrationRequest);
  server.begin();

  Serial.println("[HTTP] Weight API started on port 80");
}

// -------------------- Main loop --------------------

void loop() {
  while (Serial.available()) {
    const char c = (char)Serial.read();

    if (c == '\r' || c == '\n') {
      if (commandBuffer.length()) {
        processCommand(commandBuffer);
        commandBuffer = "";
      }
    } else if (commandBuffer.length() < 48) {
      commandBuffer += c;
    } else {
      commandBuffer = "";
      Serial.println("[ERROR] Command too long.");
    }
  }

  maintainWiFi();

  server.handleClient();


if (millis() - lastReportMs >= REPORT_INTERVAL_MS) {
  lastReportMs = millis();

  if (ready(1000)) {
    latestWeight = scale.get_units(2);
    lastWeightMs = millis();
    weightValid = true;

    Serial.printf("Weight: %.1f g\n", latestWeight);
  } else {
    weightValid = false;
  }
}

}
