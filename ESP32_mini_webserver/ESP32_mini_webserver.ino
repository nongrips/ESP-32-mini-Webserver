#include <WiFi.h>
#include <WebServer.h>

const char* ssid     = "DEIN_WLAN";      // CHANGE ME
const char* password = "DEIN_PASSWORT";  // CHANGE ME

WebServer server(80);

const int ledPin  = 2;
bool      ledState = false;

void handleRoot() {
  String ledStatus = ledState ? "AN" : "AUS";
  String btnColor  = ledState ? "#4CAF50" : "#f44336";

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32 LED Control</title>";
  html += "<style>";
  html += "body{font-family:sans-serif;text-align:center;margin-top:50px;background:#1e1e1e;color:#fff;}";
  html += "h1{color:#00bcd4;}";
  html += "p{font-size:1.2rem;}";
  html += "button{padding:16px 40px;font-size:1.1rem;border:none;border-radius:8px;cursor:pointer;";
  html += "background:" + btnColor + ";color:#fff;transition:opacity .2s;}";
  html += "button:hover{opacity:0.85;}";
  html += "</style></head><body>";
  html += "<h1>ESP32 Mini Webserver</h1>";
  html += "<p>LED (Pin 2) ist aktuell: <strong>" + ledStatus + "</strong></p>";
  html += "<form action='/toggle' method='get'>";
  html += "<button type='submit'>LED umschalten</button>";
  html += "</form>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleToggle() {
  ledState = !ledState;
  digitalWrite(ledPin, ledState ? HIGH : LOW);
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Seite nicht gefunden.");
}

void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

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

  server.on("/",       handleRoot);
  server.on("/toggle", handleToggle);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Webserver gestartet.");
}

void loop() {
  server.handleClient();
}
