#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

const char* ssid     = "DEIN_WLAN";      // CHANGE ME
const char* password = "DEIN_PASSWORT";  // CHANGE ME

WebServer server(80);

// --- Pin-Konfiguration -------------------------------------------
// Pins und Namen hier anpassen. Beliebig viele Einträge möglich.
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
  html += "a{color:#00bcd4;font-size:.85rem;display:block;margin-top:30px;}";
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

  html += "<a href='/debug'>Debug-Info</a>";
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
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void buildStatusJson(JsonDocument& doc) {
  doc["led"]      = pinStates[0]; // backward compat
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
  }

  JsonDocument doc;
  doc["gpio"]   = gpio;
  doc["state"]  = idx >= 0 ? pinStates[idx] : false;
  doc["led"]    = pinStates[0]; // backward compat
  doc["uptime"] = millis() / 1000;
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// T1-D: Debug-Endpoint
void handleDebug() {
  String txt = "=== ESP32 Debug ===\n";
  txt += "Uptime:      " + String(millis() / 1000)          + " s\n";
  txt += "Free Heap:   " + String(ESP.getFreeHeap())         + " bytes\n";
  txt += "CPU Freq:    " + String(ESP.getCpuFreqMHz())       + " MHz\n";
  txt += "Flash Size:  " + String(ESP.getFlashChipSize()/1024) + " KB\n";
  txt += "IP:          " + WiFi.localIP().toString()          + "\n";
  txt += "MAC:         " + WiFi.macAddress()                  + "\n";
  txt += "SSID:        " + WiFi.SSID()                        + "\n";
  txt += "RSSI:        " + String(WiFi.RSSI())                + " dBm\n";
  txt += "Hostname:    esp32.local\n";
  txt += "\nPins:\n";
  for (int i = 0; i < PIN_COUNT; i++)
    txt += "  GPIO " + String(PINS[i]) + " (" + String(PIN_NAMES[i]) + "): " + (pinStates[i] ? "AN" : "AUS") + "\n";
  server.send(200, "text/plain", txt);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Seite nicht gefunden.");
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < PIN_COUNT; i++) {
    pinStates[i] = false;
    pinMode(PINS[i], OUTPUT);
    digitalWrite(PINS[i], LOW);
  }

  WiFi.begin(ssid, password);
  Serial.print("Verbinde mit WLAN");

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 30) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nVerbindung fehlgeschlagen! SSID/Passwort prüfen.");
    return;
  }

  Serial.println("\nVerbunden!");
  Serial.print("IP-Adresse: ");
  Serial.println(WiFi.localIP());

  server.on("/",           handleRoot);
  server.on("/toggle",     handleToggle);
  server.on("/api/status", handleApiStatus);
  server.on("/api/toggle", handleApiToggle);
  server.on("/debug",      handleDebug);
  server.onNotFound(handleNotFound);

  if (MDNS.begin("esp32")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS gestartet: http://esp32.local");
  } else {
    Serial.println("mDNS fehlgeschlagen.");
  }

  server.begin();
  Serial.println("Webserver gestartet.");
}

void loop() {
  server.handleClient();
}
