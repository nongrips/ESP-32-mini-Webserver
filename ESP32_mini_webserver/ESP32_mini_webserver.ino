#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>          // T2-D – Library Manager: "WiFiManager" by tzapu
#include <Preferences.h>          // T3-B – im ESP32-Core enthalten
#include <ArduinoOTA.h>           // T3-C – im ESP32-Core enthalten
#include <Wire.h>                 // T2-A – im ESP32-Core enthalten
#include <Adafruit_BME280.h>      // T2-A – Library Manager: "Adafruit BME280 Library"
#include <Adafruit_Sensor.h>      // T2-A – Library Manager: "Adafruit Unified Sensor"
#include <LittleFS.h>             // T2-B – im ESP32-Core enthalten
#include <WebSocketsServer.h>     // T2-C – Library Manager: "WebSockets" by Markus Sattler

// OTA-Passwort hier setzen (nie leer lassen!)
#define OTA_PASSWORD "esp32ota"

WebServer        server(80);
WebSocketsServer ws(81);     // T2-C: WebSocket auf Port 81
Preferences      prefs;

// --- BME280 Sensor -----------------------------------------------
Adafruit_BME280 bme;
bool    sensorOK  = false;
float   sensorTemp = NAN, sensorHum = NAN, sensorPres = NAN;
unsigned long lastSensorMs = 0;
// -----------------------------------------------------------------

// --- Pin-Konfiguration -------------------------------------------
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

// T2-B: Seite wird aus LittleFS geladen
void handleRoot() {
  if (LittleFS.exists("/index.html")) {
    File f = LittleFS.open("/index.html", "r");
    server.streamFile(f, "text/html");
    f.close();
  } else {
    server.send(500, "text/plain", "LittleFS: index.html nicht gefunden. Dateien hochladen!");
  }
}

void handleToggle() {
  int gpio = server.arg("pin").toInt();
  if (gpio == 0) gpio = PINS[0];
  int idx = pinIndex(gpio);
  if (idx >= 0) {
    pinStates[idx] = !pinStates[idx];
    digitalWrite(PINS[idx], pinStates[idx] ? HIGH : LOW);
    saveState(idx);
    broadcastState();
  }
  server.sendHeader("Location", "/");
  server.send(303);
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

  JsonObject sensor = doc["sensor"].to<JsonObject>();
  sensor["available"] = sensorOK;
  if (sensorOK) {
    sensor["temp_c"]   = serialized(String(sensorTemp, 1));
    sensor["humidity"] = serialized(String(sensorHum,  1));
    sensor["pressure"] = serialized(String(sensorPres, 1));
  }
}

// T2-C: aktuellen Status an alle verbundenen WebSocket-Clients senden
void broadcastState() {
  JsonDocument doc;
  buildStatusJson(doc);
  String json;
  serializeJson(doc, json);
  ws.broadcastTXT(json);
}

void handleApiSensor() {
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
  server.send(200, "application/json", json);
}

void handleApiStatus() {
  JsonDocument doc;
  buildStatusJson(doc);
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleApiToggle() {
  int gpio = server.arg("pin").toInt();
  if (gpio == 0) gpio = PINS[0];
  int idx = pinIndex(gpio);
  if (idx >= 0) {
    pinStates[idx] = !pinStates[idx];
    digitalWrite(PINS[idx], pinStates[idx] ? HIGH : LOW);
    saveState(idx);
    broadcastState();
  }

  JsonDocument doc;
  doc["gpio"]   = gpio;
  doc["state"]  = idx >= 0 ? pinStates[idx] : false;
  doc["led"]    = pinStates[0];
  doc["uptime"] = millis() / 1000;
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleDebug() {
  String txt = "=== ESP32 Debug ===\n";
  txt += "Uptime:      " + String(millis() / 1000)             + " s\n";
  txt += "Free Heap:   " + String(ESP.getFreeHeap())            + " bytes\n";
  txt += "CPU Freq:    " + String(ESP.getCpuFreqMHz())          + " MHz\n";
  txt += "Flash Size:  " + String(ESP.getFlashChipSize() / 1024) + " KB\n";
  txt += "IP:          " + WiFi.localIP().toString()             + "\n";
  txt += "MAC:         " + WiFi.macAddress()                     + "\n";
  txt += "SSID:        " + WiFi.SSID()                           + "\n";
  txt += "RSSI:        " + String(WiFi.RSSI())                   + " dBm\n";
  txt += "Hostname:    esp32.local\n";
  txt += "\nPins:\n";
  for (int i = 0; i < PIN_COUNT; i++)
    txt += "  GPIO " + String(PINS[i]) + " (" + String(PIN_NAMES[i]) + "): " + (pinStates[i] ? "AN" : "AUS") + "\n";
  server.send(200, "text/plain", txt);
}

// T2-D: WLAN-Einstellungen löschen und neu konfigurieren
void handleResetWifi() {
  server.send(200, "text/plain", "WLAN-Einstellungen werden zurückgesetzt. Board startet neu...");
  delay(1000);
  WiFiManager wm;
  wm.resetSettings();
  ESP.restart();
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Seite nicht gefunden.");
}

void setupOTA() {
  ArduinoOTA.setHostname("esp32-webserver");
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    Serial.println("OTA: Update startet...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA: Fertig. Neustart...");
  });
  ArduinoOTA.onError([](ota_error_t err) {
    Serial.printf("OTA Fehler [%u]\n", err);
  });

  ArduinoOTA.begin();
  Serial.println("OTA bereit (Passwort: " OTA_PASSWORD ")");
}

void setup() {
  Serial.begin(115200);

  // Pins initialisieren; gespeicherten Zustand laden
  prefs.begin("webserver", false);
  for (int i = 0; i < PIN_COUNT; i++) {
    pinStates[i] = prefs.getBool(("gpio" + String(PINS[i])).c_str(), false);
    pinMode(PINS[i], OUTPUT);
    digitalWrite(PINS[i], pinStates[i] ? HIGH : LOW);
  }

  // T2-D: WiFiManager – beim ersten Start öffnet sich ein Captive-Portal
  // ("ESP32-Setup"), danach werden Credentials im Flash gespeichert.
  WiFiManager wm;
  wm.setConfigPortalTimeout(120);
  if (!wm.autoConnect("ESP32-Setup")) {
    Serial.println("WiFiManager: Timeout, Neustart...");
    ESP.restart();
  }

  Serial.println("WLAN verbunden!");
  Serial.print("IP-Adresse: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp32")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS: http://esp32.local");
  }

  // T2-B: LittleFS mounten (true = bei Fehler neu formatieren)
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS: Fehler beim Mounten!");
  } else {
    Serial.println("LittleFS bereit.");
  }

  // T2-A: BME280 Sensor (I2C: SDA=GPIO21, SCL=GPIO22)
  Wire.begin();
  sensorOK = bme.begin(0x76) || bme.begin(0x77);
  Serial.println(sensorOK ? "BME280 gefunden." : "BME280 nicht gefunden (optional).");

  server.on("/",            handleRoot);
  server.on("/toggle",      handleToggle);
  server.on("/api/status",  handleApiStatus);
  server.on("/api/toggle",  handleApiToggle);
  server.on("/api/sensor",  handleApiSensor);
  server.on("/debug",       handleDebug);
  server.on("/reset-wifi",  handleResetWifi);
  // Statische Dateien aus LittleFS (CSS, JS, Bilder)
  server.serveStatic("/style.css", LittleFS, "/style.css");
  server.serveStatic("/app.js",    LittleFS, "/app.js");
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Webserver gestartet.");

  // T2-C: WebSocket-Server auf Port 81
  ws.begin();
  ws.onEvent([](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    if (type == WStype_CONNECTED) {
      broadcastState(); // neuen Client sofort mit aktuellem Zustand versorgen
    }
  });
  Serial.println("WebSocket gestartet (Port 81).");

  setupOTA();
}

void loop() {
  server.handleClient();
  ws.loop();
  ArduinoOTA.handle();

  // T2-A: Sensor alle 2 s lesen (nie im HTTP-Handler aufrufen!)
  if (sensorOK && millis() - lastSensorMs >= 2000) {
    sensorTemp = bme.readTemperature();
    sensorHum  = bme.readHumidity();
    sensorPres = bme.readPressure() / 100.0;
    lastSensorMs = millis();
  }
}
