// ============================================================
// ESP32 Mini Webserver
// Benötigte Libraries (Library Manager):
//   - "ArduinoJson"              by Benoit Blanchon
//   - "WiFiManager"              by tzapu
//   - "Adafruit BME280 Library"  by Adafruit
//   - "Adafruit Unified Sensor"  by Adafruit
//   - "Adafruit BusIO"           by Adafruit
//   - "WebSockets"               by Markus Sattler
//   - "PubSubClient"             by Nick O'Leary
// Alle im ESP32-Core enthalten (kein Install):
//   ESPmDNS, Preferences, ArduinoOTA, LittleFS, Wire, WebServer
// ============================================================

#include <WiFi.h>
#include <WebServer.h>            // im ESP32-Core enthalten
#include <WebSocketsServer.h>     // Library Manager: "WebSockets" by Markus Sattler
#include <ESPmDNS.h>
#include <ArduinoJson.h>          // Library Manager: "ArduinoJson"
#include <WiFiManager.h>          // Library Manager: "WiFiManager" by tzapu
#include <Preferences.h>          // im ESP32-Core enthalten
#include <ArduinoOTA.h>           // im ESP32-Core enthalten
#include <Wire.h>                 // im ESP32-Core enthalten
#include <Adafruit_BME280.h>      // Library Manager: "Adafruit BME280 Library"
#include <Adafruit_Sensor.h>      // Library Manager: "Adafruit Unified Sensor"
#include <LittleFS.h>             // im ESP32-Core enthalten
#include <PubSubClient.h>         // Library Manager: "PubSubClient" by Nick O'Leary

#define OTA_PASSWORD "esp32ota"   // OTA-Passwort – bitte ändern!

// --- MQTT-Konfiguration ------------------------------------------
#define MQTT_ENABLED  false
#define MQTT_BROKER   "192.168.1.X"
#define MQTT_PORT     1883
#define MQTT_USER     ""
#define MQTT_PASS     ""
// -----------------------------------------------------------------

WebServer        server(80);
WebSocketsServer ws(81);
Preferences      prefs;

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

// T3-D
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);
String       mqttBase;

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

void broadcastState() {
  JsonDocument doc;
  buildStatusJson(doc);
  String json;
  serializeJson(doc, json);
  ws.broadcastTXT(json);
}

void mqttPublishPin(int idx) {
  if (!MQTT_ENABLED || !mqtt.connected()) return;
  String topic = mqttBase + "/pins/" + String(PINS[idx]) + "/state";
  mqtt.publish(topic.c_str(), pinStates[idx] ? "ON" : "OFF", true);
}

void doToggle(int gpio) {
  int idx = pinIndex(gpio);
  if (idx >= 0) {
    pinStates[idx] = !pinStates[idx];
    digitalWrite(PINS[idx], pinStates[idx] ? HIGH : LOW);
    saveState(idx);
    broadcastState();
    mqttPublishPin(idx);
  }
}

void handleRoot() {
  if (LittleFS.exists("/index.html")) {
    File f = LittleFS.open("/index.html", "r");
    server.streamFile(f, "text/html");
    f.close();
  } else {
    server.send(500, "text/plain", "LittleFS: index.html fehlt. Dateien hochladen!");
  }
}

void handleToggle() {
  int gpio = server.arg("pin").toInt();
  if (gpio == 0) gpio = PINS[0];
  doToggle(gpio);
  server.sendHeader("Location", "/");
  server.send(303);
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
  doToggle(gpio);
  int idx = pinIndex(gpio);
  JsonDocument doc;
  doc["gpio"]   = gpio;
  doc["state"]  = idx >= 0 ? pinStates[idx] : false;
  doc["led"]    = pinStates[0];
  doc["uptime"] = millis() / 1000;
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
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

void handleDebug() {
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
  server.send(200, "text/plain", txt);
}

void handleResetWifi() {
  server.send(200, "text/plain", "WLAN wird zurückgesetzt, Board startet neu...");
  delay(500);
  WiFiManager wm;
  wm.resetSettings();
  ESP.restart();
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String t = String(topic);
  String v = String((char*)payload).substring(0, len);
  int gpioStart = t.lastIndexOf("/pins/") + 6;
  int gpioEnd   = t.indexOf("/set", gpioStart);
  if (gpioStart > 5 && gpioEnd > gpioStart) {
    int gpio = t.substring(gpioStart, gpioEnd).toInt();
    int idx  = pinIndex(gpio);
    if (idx >= 0) {
      bool newState = (v == "ON") ? true : (v == "OFF") ? false : !pinStates[idx];
      if (newState != pinStates[idx]) doToggle(gpio);
    }
  }
}

void mqttReconnect() {
  if (!MQTT_ENABLED || mqtt.connected()) return;
  String lwt = mqttBase + "/status";
  if (mqtt.connect("esp32-webserver", MQTT_USER, MQTT_PASS,
                   lwt.c_str(), 1, true, "offline")) {
    mqtt.publish(lwt.c_str(), "online", true);
    mqtt.subscribe((mqttBase + "/pins/+/set").c_str());
    for (int i = 0; i < PIN_COUNT; i++) mqttPublishPin(i);
  }
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

  prefs.begin("webserver", false);
  for (int i = 0; i < PIN_COUNT; i++) {
    pinStates[i] = prefs.getBool(("gpio" + String(PINS[i])).c_str(), false);
    pinMode(PINS[i], OUTPUT);
    digitalWrite(PINS[i], pinStates[i] ? HIGH : LOW);
  }

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

  server.on("/",            handleRoot);
  server.on("/toggle",      handleToggle);
  server.on("/api/status",  handleApiStatus);
  server.on("/api/toggle",  handleApiToggle);
  server.on("/api/sensor",  handleApiSensor);
  server.on("/debug",       handleDebug);
  server.on("/reset-wifi",  handleResetWifi);
  server.serveStatic("/style.css", LittleFS, "/style.css");
  server.serveStatic("/app.js",    LittleFS, "/app.js");
  server.onNotFound([](){ server.send(404, "text/plain", "404: Nicht gefunden."); });
  server.begin();
  Serial.println("Webserver gestartet (Port 80).");

  ws.begin();
  ws.onEvent([](uint8_t num, WStype_t type, uint8_t*, size_t) {
    if (type == WStype_CONNECTED) broadcastState();
  });
  Serial.println("WebSocket gestartet (Port 81).");

  if (MQTT_ENABLED) {
    String mac = WiFi.macAddress(); mac.replace(":", ""); mac.toLowerCase();
    mqttBase = "esp32/" + mac;
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    mqttReconnect();
  }

  setupOTA();
}

void loop() {
  server.handleClient();
  ws.loop();
  ArduinoOTA.handle();

  if (MQTT_ENABLED) {
    if (!mqtt.connected()) mqttReconnect();
    mqtt.loop();
  }

  if (sensorOK && millis() - lastSensorMs >= 2000) {
    sensorTemp = bme.readTemperature();
    sensorHum  = bme.readHumidity();
    sensorPres = bme.readPressure() / 100.0;
    lastSensorMs = millis();
    broadcastState();
    if (MQTT_ENABLED && mqtt.connected()) {
      String base = mqttBase + "/sensor/";
      mqtt.publish((base + "temp_c").c_str(),  String(sensorTemp, 1).c_str());
      mqtt.publish((base + "humidity").c_str(), String(sensorHum,  1).c_str());
      mqtt.publish((base + "pressure").c_str(), String(sensorPres, 1).c_str());
    }
  }
}
