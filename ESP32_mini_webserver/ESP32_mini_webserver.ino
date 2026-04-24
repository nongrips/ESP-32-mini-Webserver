#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>   // T2-D – Library Manager: "WiFiManager" by tzapu
#include <Preferences.h>   // T3-B – im ESP32-Core enthalten
#include <ArduinoOTA.h>    // T3-C – im ESP32-Core enthalten

// OTA-Passwort hier setzen (nie leer lassen!)
#define OTA_PASSWORD "esp32ota"

WebServer   server(80);
Preferences prefs;

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

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32 Webserver</title>";
  html += "<style>";
  html += "body{font-family:sans-serif;text-align:center;margin-top:40px;background:#1e1e1e;color:#fff;}";
  html += "h1{color:#00bcd4;}";
  html += ".row{display:flex;justify-content:space-between;align-items:center;";
  html += "background:#2d2d2d;padding:12px 20px;border-radius:8px;margin:10px auto;max-width:420px;}";
  html += "button{padding:10px 22px;font-size:1rem;border:none;border-radius:6px;cursor:pointer;color:#fff;}";
  html += ".on{background:#4CAF50;} .off{background:#f44336;}";
  html += ".links{margin-top:30px;font-size:.85rem;}";
  html += ".links a{color:#00bcd4;margin:0 10px;}";
  html += "</style></head><body>";
  html += "<h1>ESP32 Mini Webserver</h1>";

  for (int i = 0; i < PIN_COUNT; i++) {
    String cls = pinStates[i] ? "on" : "off";
    String lbl = pinStates[i] ? "AN" : "AUS";
    html += "<div class='row'>";
    html += "<span><b>" + String(PIN_NAMES[i]) + "</b> (GPIO&nbsp;" + String(PINS[i]) + "):&nbsp;" + lbl + "</span>";
    html += "<form action='/toggle' method='get' style='margin:0'>";
    html += "<input type='hidden' name='pin' value='" + String(PINS[i]) + "'>";
    html += "<button class='" + cls + "'>Umschalten</button>";
    html += "</form></div>";
  }

  html += "<div class='links'><a href='/debug'>Debug</a><a href='/reset-wifi'>WLAN zurücksetzen</a></div>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleToggle() {
  int gpio = server.arg("pin").toInt();
  if (gpio == 0) gpio = PINS[0];
  int idx = pinIndex(gpio);
  if (idx >= 0) {
    pinStates[idx] = !pinStates[idx];
    digitalWrite(PINS[idx], pinStates[idx] ? HIGH : LOW);
    saveState(idx);
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

  server.on("/",            handleRoot);
  server.on("/toggle",      handleToggle);
  server.on("/api/status",  handleApiStatus);
  server.on("/api/toggle",  handleApiToggle);
  server.on("/debug",       handleDebug);
  server.on("/reset-wifi",  handleResetWifi);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Webserver gestartet.");

  setupOTA();
}

void loop() {
  server.handleClient();
  ArduinoOTA.handle();
}
