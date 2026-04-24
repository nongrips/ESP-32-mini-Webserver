// ============================================================
// ESP32 Mini Webserver
// Bibliotheken (Library Manager sofern nicht anders angegeben):
//   - "WiFiManager" by tzapu
//   - "ArduinoJson" by Benoit Blanchon
//   - "Adafruit BME280 Library" + "Adafruit Unified Sensor"
//   - "WebSockets" by Markus Sattler (nur noch als Dep. verbleibend)
// T3-A – ESPAsyncWebServer: GitHub-ZIP installieren:
//   https://github.com/mathieucarbou/ESPAsyncWebServer
//   https://github.com/mathieucarbou/AsyncTCP  (Dependency)
// ============================================================

#include <WiFi.h>
#include <ESPAsyncWebServer.h>    // T3-A – GitHub ZIP (mathieucarbou fork)
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>          // Library Manager: "WiFiManager" by tzapu
#include <Preferences.h>          // im ESP32-Core enthalten
#include <ArduinoOTA.h>           // im ESP32-Core enthalten
#include <Wire.h>                 // im ESP32-Core enthalten
#include <Adafruit_BME280.h>      // Library Manager: "Adafruit BME280 Library"
#include <Adafruit_Sensor.h>      // Library Manager: "Adafruit Unified Sensor"
#include <LittleFS.h>             // im ESP32-Core enthalten

#define OTA_PASSWORD "esp32ota"   // OTA-Passwort – bitte ändern!

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");         // T3-A: WebSocket als Pfad, kein eigener Port mehr
Preferences    prefs;

// --- BME280 Sensor -----------------------------------------------
Adafruit_BME280 bme;
bool    sensorOK   = false;
float   sensorTemp = NAN, sensorHum = NAN, sensorPres = NAN;
unsigned long lastSensorMs = 0;
// -----------------------------------------------------------------

// --- Pin-Konfiguration (nach Bedarf anpassen) --------------------
const int   PINS[]      = {2, 4, 5};
const char* PIN_NAMES[] = {"LED (Onboard)", "Ausgang 2", "Ausgang 3"};
const int   PIN_COUNT   = sizeof(PINS) / sizeof(PINS[0]);
bool        pinStates[PIN_COUNT];
// -----------------------------------------------------------------

int pinIndex(int gpio) {
  for (int i = 0; i < PIN_COUNT; i++)
    if (PINS[i] == gpio) return i;
  return -1;
}

void saveState(int idx) {
  prefs.putBool(("gpio" + String(PINS[idx])).c_str(), pinStates[idx]);
}

void buildStatusJson(JsonDocument& doc) {
  doc["led"]      = pinStates[0];
  doc["uptime"]   = millis() / 1000;
  doc["ip"]       = WiFi.localIP().toString();
  doc["hostname"] = "esp32.local";
  doc["rssi"]     = WiFi.RSSI();

  JsonArray arr = doc["pins"].to<JsonArray>();
  for (int i = 0; i < PIN_COUNT; i++) {
    JsonObject p = arr.add<JsonObject>();
    p["gpio"]  = PINS[i];
    p["name"]  = PIN_NAMES[i];
    p["state"] = pinStates[i];
  }

  JsonObject s = doc["sensor"].to<JsonObject>();
  s["available"] = sensorOK;
  if (sensorOK) {
    s["temp_c"]   = serialized(String(sensorTemp, 1));
    s["humidity"] = serialized(String(sensorHum,  1));
    s["pressure"] = serialized(String(sensorPres, 1));
  }
}

// T2-C / T3-A: Status-JSON an alle WebSocket-Clients senden
void broadcastState() {
  JsonDocument doc;
  buildStatusJson(doc);
  String json;
  serializeJson(doc, json);
  ws.textAll(json);   // AsyncWebSocket API
}

void doToggle(int gpio) {
  int idx = pinIndex(gpio);
  if (idx >= 0) {
    pinStates[idx] = !pinStates[idx];
    digitalWrite(PINS[idx], pinStates[idx] ? HIGH : LOW);
    saveState(idx);
    broadcastState();
  }
}

// --- WebSocket-Event-Handler ------------------------------------
void onWsEvent(AsyncWebSocket*, AsyncWebSocketClient* client,
               AwsEventType type, void*, uint8_t*, size_t) {
  if (type == WS_EVT_CONNECT) {
    // neuen Client sofort mit aktuellem Zustand versorgen
    JsonDocument doc;
    buildStatusJson(doc);
    String json;
    serializeJson(doc, json);
    client->text(json);
  }
}
// ----------------------------------------------------------------

void setupRoutes() {
  // Statische Dateien aus LittleFS; index.html für "/"
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // Browser-Toggle (Redirect zurück zur Seite)
  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest* req) {
    int gpio = req->hasArg("pin") ? req->arg("pin").toInt() : PINS[0];
    doToggle(gpio);
    req->redirect("/");
  });

  // JSON API
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    buildStatusJson(doc);
    String json;
    serializeJson(doc, json);
    req->send(200, "application/json", json);
  });

  server.on("/api/toggle", HTTP_GET, [](AsyncWebServerRequest* req) {
    int gpio = req->hasArg("pin") ? req->arg("pin").toInt() : PINS[0];
    doToggle(gpio);
    int idx = pinIndex(gpio);
    JsonDocument doc;
    doc["gpio"]   = gpio;
    doc["state"]  = idx >= 0 ? pinStates[idx] : false;
    doc["led"]    = pinStates[0];
    doc["uptime"] = millis() / 1000;
    String json;
    serializeJson(doc, json);
    req->send(200, "application/json", json);
  });

  server.on("/api/sensor", HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["available"] = sensorOK;
    if (sensorOK) {
      doc["temp_c"]   = serialized(String(sensorTemp, 1));
      doc["humidity"] = serialized(String(sensorHum,  1));
      doc["pressure"] = serialized(String(sensorPres, 1));
    } else {
      doc["error"] = "Kein BME280 gefunden (SDA=GPIO21, SCL=GPIO22)";
    }
    String json;
    serializeJson(doc, json);
    req->send(200, "application/json", json);
  });

  server.on("/debug", HTTP_GET, [](AsyncWebServerRequest* req) {
    String txt = "=== ESP32 Debug ===\n";
    txt += "Uptime:      " + String(millis() / 1000)              + " s\n";
    txt += "Free Heap:   " + String(ESP.getFreeHeap())             + " bytes\n";
    txt += "CPU Freq:    " + String(ESP.getCpuFreqMHz())           + " MHz\n";
    txt += "Flash Size:  " + String(ESP.getFlashChipSize() / 1024) + " KB\n";
    txt += "IP:          " + WiFi.localIP().toString()              + "\n";
    txt += "MAC:         " + WiFi.macAddress()                      + "\n";
    txt += "SSID:        " + WiFi.SSID()                            + "\n";
    txt += "RSSI:        " + String(WiFi.RSSI())                    + " dBm\n";
    txt += "Hostname:    esp32.local\n\nPins:\n";
    for (int i = 0; i < PIN_COUNT; i++)
      txt += "  GPIO " + String(PINS[i]) + " (" + String(PIN_NAMES[i]) + "): "
           + (pinStates[i] ? "AN" : "AUS") + "\n";
    req->send(200, "text/plain", txt);
  });

  server.on("/reset-wifi", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/plain", "WLAN-Einstellungen werden zurückgesetzt. Board startet neu...");
    delay(500);
    WiFiManager wm;
    wm.resetSettings();
    ESP.restart();
  });

  // WebSocket registrieren
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // 404
  server.onNotFound([](AsyncWebServerRequest* req) {
    req->send(404, "text/plain", "404: Nicht gefunden.");
  });
}

void setupOTA() {
  ArduinoOTA.setHostname("esp32-webserver");
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]()  { Serial.println("OTA: Update startet..."); });
  ArduinoOTA.onEnd([]()    { Serial.println("\nOTA: Fertig."); });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("OTA Fehler [%u]\n", e); });
  ArduinoOTA.begin();
  Serial.println("OTA bereit.");
}

void setup() {
  Serial.begin(115200);

  // Gespeicherten Pin-Zustand laden
  prefs.begin("webserver", false);
  for (int i = 0; i < PIN_COUNT; i++) {
    pinStates[i] = prefs.getBool(("gpio" + String(PINS[i])).c_str(), false);
    pinMode(PINS[i], OUTPUT);
    digitalWrite(PINS[i], pinStates[i] ? HIGH : LOW);
  }

  // WiFiManager: beim ersten Start Captive-Portal "ESP32-Setup" öffnen
  WiFiManager wm;
  wm.setConfigPortalTimeout(120);
  if (!wm.autoConnect("ESP32-Setup")) {
    Serial.println("WiFiManager: Timeout, Neustart...");
    ESP.restart();
  }
  Serial.print("WLAN verbunden! IP: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp32")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS: http://esp32.local");
  }

  if (!LittleFS.begin(true))
    Serial.println("LittleFS: Fehler beim Mounten!");
  else
    Serial.println("LittleFS bereit.");

  Wire.begin();
  sensorOK = bme.begin(0x76) || bme.begin(0x77);
  Serial.println(sensorOK ? "BME280 gefunden." : "BME280 nicht gefunden (optional).");

  setupRoutes();
  server.begin();
  Serial.println("AsyncWebServer + WebSocket (/ws) gestartet.");

  setupOTA();
}

void loop() {
  ArduinoOTA.handle();
  // server.handleClient() entfällt – AsyncWebServer läuft auf eigenem Task
  // ws.loop() entfällt – AsyncWebSocket integriert

  if (sensorOK && millis() - lastSensorMs >= 2000) {
    sensorTemp = bme.readTemperature();
    sensorHum  = bme.readHumidity();
    sensorPres = bme.readPressure() / 100.0;
    lastSensorMs = millis();
    broadcastState(); // Sensor-Update auch per WebSocket pushen
  }
}
