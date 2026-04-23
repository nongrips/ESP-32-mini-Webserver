# ESP32 Mini Webserver

Ein einfacher Webserver auf dem ESP32, der eine LED über den Browser ein- und ausschalten lässt.  
*A simple ESP32 webserver that lets you toggle an LED via your browser.*

> **Board:** ESP-32D (HW-394) · WiFi + Bluetooth · USB-C

---

## Inhaltsverzeichnis / Table of Contents

- [Deutsch](#deutsch)
  - [Was du brauchst](#was-du-brauchst)
  - [Arduino IDE einrichten](#arduino-ide-einrichten)
  - [CachyOS / Arch Linux – Besonderheiten](#cachyos--arch-linux--besonderheiten)
  - [Board-Treiber installieren](#board-treiber-installieren)
  - [Code anpassen & hochladen](#code-anpassen--hochladen)
  - [Webserver aufrufen](#webserver-aufrufen)
  - [JSON API](#json-api)
  - [Mögliche Einsatzzwecke](#mögliche-einsatzzwecke)
  - [Häufige Fehler & Lösungen](#häufige-fehler--lösungen)
- [English](#english)
  - [What you need](#what-you-need)
  - [Set up Arduino IDE](#set-up-arduino-ide)
  - [CachyOS / Arch Linux – Special notes](#cachyos--arch-linux--special-notes)
  - [Install board support](#install-board-support)
  - [Configure & upload the code](#configure--upload-the-code)
  - [Access the webserver](#access-the-webserver)
  - [JSON API](#json-api-1)
  - [Possible use cases](#possible-use-cases)
  - [Common errors & fixes](#common-errors--fixes)

---

# Deutsch

## Was du brauchst

| Komponente | Details |
|---|---|
| ESP32-Board | ESP-32D (HW-394) oder kompatibel |
| USB-C Kabel | Datenübertragung (kein reines Ladekabel!) |
| PC/Mac/Linux | Mit Arduino IDE |
| WLAN-Netzwerk | 2,4 GHz (ESP32 unterstützt kein 5 GHz) |

---

## Arduino IDE einrichten

### 1. Arduino IDE herunterladen

Lade die **Arduino IDE 2.x** herunter: https://www.arduino.cc/en/software

### 2. ESP32-Boardunterstützung hinzufügen

Die Arduino IDE kennt den ESP32 standardmäßig nicht. Das wird über den **Boards Manager** nachgerüstet:

1. Öffne Arduino IDE
2. Gehe zu **Datei → Einstellungen** (Windows/Linux) oder **Arduino IDE → Einstellungen** (Mac)
3. Füge folgende URL im Feld **„Zusätzliche Boardverwalter-URLs"** ein:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Klicke **OK**
5. Gehe zu **Werkzeuge → Board → Boardverwalter**
6. Suche nach `esp32`
7. Installiere **„esp32" von Espressif Systems** (Version 2.x oder höher)

> Die Installation kann einige Minuten dauern.

### 3. CP210x / CH340 USB-Treiber (falls nötig)

Das Board HW-394 nutzt oft einen **CH340**- oder **CP2102**-USB-Chip. Prüfe, ob dein Computer das Board erkennt:

- **Windows:** Gerätemanager → Anschlüsse (COM & LPT) → neuer COM-Port muss erscheinen
- **Mac/Linux:** `ls /dev/tty.*` oder `ls /dev/ttyUSB*` im Terminal

Falls kein Port erscheint:
- **CH340-Treiber:** https://www.wch-ic.com/downloads/CH341SER_EXE.html
- **CP210x-Treiber:** https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers

---

## CachyOS / Arch Linux – Besonderheiten

> CachyOS basiert auf Arch Linux. Es gibt einige wichtige Unterschiede zu Windows und Debian/Ubuntu-basierten Systemen.

### 1. Arduino IDE installieren

Auf CachyOS **nicht** manuell herunterladen – stattdessen über AUR installieren:

```bash
# Mit yay
yay -S arduino-ide-bin

# Oder mit paru
paru -S arduino-ide-bin
```

Alternativ als **Flatpak** (aber Achtung – siehe Abschnitt unten):

```bash
flatpak install flathub cc.arduino.IDE2
```

Oder als **AppImage** von der Arduino-Website – dann zuerst ausführbar machen:

```bash
chmod +x arduino-ide_*.AppImage
./arduino-ide_*.AppImage
```

> Falls das AppImage nicht startet: `sudo pacman -S fuse2`

---

### 2. USB-Treiber – kein manueller Download nötig

Der CH340- und CP210x-Kernel-Modul ist im CachyOS-Kernel bereits enthalten. Nach dem Einstecken des Boards wird es automatisch erkannt.

Prüfen, ob das Modul geladen ist:

```bash
# Für CH340-Chip (häufigster bei günstigen ESP32-Boards)
lsmod | grep ch341

# Für CP2102-Chip
lsmod | grep cp210x
```

Falls kein Modul geladen ist, manuell laden:

```bash
sudo modprobe ch341
# oder
sudo modprobe cp210x
```

Nach dem Einstecken des Boards den Port prüfen:

```bash
ls /dev/ttyUSB* /dev/ttyACM*
# oder
dmesg | tail -15
```

Das ESP32 erscheint typischerweise als:
- `/dev/ttyUSB0` – CH340-Chip
- `/dev/ttyACM0` – CP2102-Chip

---

### 3. Seriellen Port freischalten – `uucp`-Gruppe (wichtig!)

Das ist der **häufigste Fehler auf Arch/CachyOS**: Ohne Gruppenzugehörigkeit kann die Arduino IDE nicht auf den Port zugreifen.

Auf **Arch/CachyOS** heißt die Gruppe `uucp` (nicht `dialout` wie bei Ubuntu/Debian).

```bash
# Benutzer zur uucp-Gruppe hinzufügen
sudo usermod -aG uucp $USER
```

Danach **ausloggen und neu einloggen** (oder neu starten). Prüfen ob es geklappt hat:

```bash
groups
# Ausgabe muss "uucp" enthalten
```

Direkter Test ohne Neuanmeldung (nur für die aktuelle Session):

```bash
newgrp uucp
```

> Ohne diesen Schritt erscheint in der Arduino IDE: **„Error opening serial port"** oder der Upload schlägt mit einem Berechtigungsfehler fehl.

---

### 4. Flatpak-Einschränkung (Sandbox-Problem)

Wenn die Arduino IDE über **Flatpak** installiert wurde, kann sie standardmäßig **nicht auf `/dev/ttyUSB0` zugreifen**, da Flatpak in einer Sandbox läuft.

Freigabe erteilen:

```bash
flatpak override --user --filesystem=/dev/ttyUSB0 cc.arduino.IDE2
flatpak override --user --filesystem=/dev/ttyACM0 cc.arduino.IDE2
```

Dauerhaftere Alternative: Die **AUR-Version** (`arduino-ide-bin`) läuft ohne Sandbox und hat dieses Problem nicht.

---

### 5. python-pyserial (falls esptool Fehler auftreten)

```bash
sudo pacman -S python-pyserial
```

---

## Board-Treiber installieren

Nach dem Anschließen des Boards über USB-C:

1. Gehe zu **Werkzeuge → Board → esp32**
2. Wähle **„ESP32 Dev Module"** (passt für die meisten ESP32-D Boards)
3. Gehe zu **Werkzeuge → Port** und wähle den COM-Port deines Boards aus
4. Stelle folgende Einstellungen sicher:

| Einstellung | Wert |
|---|---|
| Board | ESP32 Dev Module |
| Upload Speed | 921600 |
| Flash Frequency | 80 MHz |
| Flash Mode | QIO |
| Partition Scheme | Default 4MB with spiffs |
| Core Debug Level | None |

---

## Code anpassen & hochladen

### 1. Sketch öffnen

Öffne die Datei `ESP32_mini_webserver/ESP32_mini_webserver.ino` in der Arduino IDE.

### 2. WLAN-Zugangsdaten eintragen

Passe diese beiden Zeilen am Anfang des Sketches an:

```cpp
const char* ssid     = "DEIN_WLAN";      // Name deines WLANs (2,4 GHz)
const char* password = "DEIN_PASSWORT";  // Passwort deines WLANs
```

> **Wichtig:** Das ESP32 unterstützt nur **2,4-GHz-Netzwerke**, kein 5 GHz!

### 3. Code hochladen

1. Klicke auf den **Pfeil (→) „Hochladen"** in der Arduino IDE
2. Halte ggf. den **BOOT-Knopf** auf dem Board gedrückt, bis der Upload startet  
   *(beim HW-394 oft nötig, wenn „Connecting…" erscheint)*
3. Warte, bis in der Konsole **„Leaving… Hard resetting via RTS pin…"** erscheint

### 4. IP-Adresse herausfinden

1. Gehe zu **Werkzeuge → Serieller Monitor**
2. Stelle die Baudrate auf **115200** ein
3. Drücke den **EN/Reset-Knopf** auf dem Board
4. Im Monitor erscheint die IP-Adresse, z. B.:
   ```
   Verbinde mit WLAN..........
   Verbunden!
   IP-Adresse: 192.168.1.42
   mDNS gestartet: http://esp32.local
   Webserver gestartet.
   ```

---

## Webserver aufrufen

1. Öffne einen Browser auf einem Gerät im **selben WLAN**
2. Gib die IP-Adresse ein, z. B.: `http://192.168.1.42`  
   **Oder:** Nutze den mDNS-Hostname: `http://esp32.local` *(kein IP-Nachschlagen nötig)*
3. Die Weboberfläche erscheint mit einem Button zum Umschalten der LED (Pin 2 / blaue Onboard-LED)

> **mDNS-Hinweise:**  
> - Linux & Android: funktioniert sofort (Avahi-Daemon ist meist vorinstalliert)  
> - Windows: benötigt [Bonjour](https://support.apple.com/kb/DL999) (oder iTunes installieren)  
> - **CachyOS:** `sudo pacman -S avahi nss-mdns` – danach einmalig in `/etc/nsswitch.conf` bei `hosts:` den Eintrag `mdns_minimal` vor `resolve` setzen

---

## JSON API

Der Webserver stellt zwei JSON-Endpunkte bereit – nützlich für Home Assistant, Node-RED, Shell-Skripte oder andere Microcontroller.

> **Library:** `ArduinoJson` by Benoit Blanchon – einmalig in der Arduino IDE installieren:  
> **Werkzeuge → Bibliotheken verwalten → „ArduinoJson" suchen → Installieren**

### `GET /api/status`

Gibt den aktuellen Zustand des Boards zurück.

```bash
curl http://esp32.local/api/status
```

Antwort:
```json
{
  "led": false,
  "uptime": 142,
  "ip": "192.168.1.42",
  "hostname": "esp32.local",
  "rssi": -58
}
```

| Feld | Bedeutung |
|---|---|
| `led` | `true` = AN, `false` = AUS |
| `uptime` | Sekunden seit letztem Neustart |
| `ip` | IP-Adresse im lokalen Netz |
| `hostname` | mDNS-Hostname |
| `rssi` | WLAN-Signalstärke in dBm (näher an 0 = besser) |

### `GET /api/toggle`

Schaltet die LED um und gibt den neuen Zustand zurück. Ideal für curl-Skripte oder Automationen.

```bash
curl http://esp32.local/api/toggle
```

Antwort:
```json
{
  "led": true,
  "uptime": 145
}
```

### Home Assistant – REST-Integration

```yaml
# configuration.yaml
switch:
  - platform: rest
    name: ESP32 LED
    resource: http://esp32.local/api/toggle
    state_resource: http://esp32.local/api/status
    body_on: ""
    body_off: ""
    is_on_template: "{{ value_json.led }}"
    method: GET
```

---

## Mögliche Einsatzzwecke

### Heimautomatisierung
- Steckdosen, Relais oder Lampen per Browser oder Smartphone schalten
- Rolladen, Ventilatoren oder Heizkörper fernsteuern

### Sensormonitoring
- Temperatur/Luftfeuchtigkeit (DHT22, BME280) live im Browser anzeigen
- Bodenfeuchtigkeit für Pflanzen überwachen

### Prototypen & Bastelprojekte
- 3D-Drucker-Status anzeigen
- Aquarium-Steuerung (Pumpen, Beleuchtung)
- Türklingel mit Kameraintegration

### Lehr- & Lernprojekte
- Einstieg in IoT und Webentwicklung
- REST-API-Grundlagen verstehen
- Netzwerkprogrammierung mit C++ üben

---

## Häufige Fehler & Lösungen

| Fehler | Lösung |
|---|---|
| `Connecting…` bleibt hängen | BOOT-Knopf gedrückt halten während Upload |
| Kein COM-Port sichtbar | USB-Treiber (CH340/CP210x) installieren, anderes Kabel probieren |
| WLAN-Verbindung schlägt fehl | Nur 2,4-GHz-WLAN verwenden, SSID/Passwort prüfen |
| Seite lädt nicht | Prüfe, ob PC und ESP32 im selben Netz sind; Firewall prüfen |
| `esptool.py` Fehler | Upload Speed auf `115200` reduzieren |
| Board wird heiß | Kurzschluss prüfen, Stromversorgung kontrollieren |
| **[CachyOS]** „Error opening serial port" | `sudo usermod -aG uucp $USER` → ausloggen → neu einloggen |
| **[CachyOS]** Flatpak sieht keinen Port | `flatpak override --user --filesystem=/dev/ttyUSB0 cc.arduino.IDE2` |
| **[CachyOS]** AppImage startet nicht | `sudo pacman -S fuse2` |
| **[CachyOS]** `ch341`-Modul fehlt | `sudo modprobe ch341` oder `sudo modprobe cp210x` |

---

---

# English

## What you need

| Component | Details |
|---|---|
| ESP32 board | ESP-32D (HW-394) or compatible |
| USB-C cable | Data cable (not a charge-only cable!) |
| PC/Mac/Linux | With Arduino IDE installed |
| WiFi network | 2.4 GHz (ESP32 does not support 5 GHz) |

---

## Set up Arduino IDE

### 1. Download Arduino IDE

Download **Arduino IDE 2.x**: https://www.arduino.cc/en/software

### 2. Add ESP32 board support

The Arduino IDE doesn't support ESP32 out of the box. Add it via the **Boards Manager**:

1. Open Arduino IDE
2. Go to **File → Preferences** (Windows/Linux) or **Arduino IDE → Preferences** (Mac)
3. In the field **"Additional Boards Manager URLs"**, paste:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Click **OK**
5. Go to **Tools → Board → Boards Manager**
6. Search for `esp32`
7. Install **"esp32" by Espressif Systems** (version 2.x or later)

> Installation may take a few minutes.

### 3. CP210x / CH340 USB driver (if needed)

The HW-394 board typically uses a **CH340** or **CP2102** USB chip. Check if your computer recognizes the board:

- **Windows:** Device Manager → Ports (COM & LPT) → a new COM port should appear
- **Mac/Linux:** Run `ls /dev/tty.*` or `ls /dev/ttyUSB*` in Terminal

If no port appears:
- **CH340 driver:** https://www.wch-ic.com/downloads/CH341SER_EXE.html
- **CP210x driver:** https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers

---

## CachyOS / Arch Linux – Special notes

> CachyOS is Arch-based. There are a few important differences from Windows and Debian/Ubuntu systems.

### 1. Install Arduino IDE

On CachyOS, **don't download manually** – install via AUR instead:

```bash
# Using yay
yay -S arduino-ide-bin

# Or using paru
paru -S arduino-ide-bin
```

Alternatively as a **Flatpak** (but read the caveat below):

```bash
flatpak install flathub cc.arduino.IDE2
```

Or use the **AppImage** from the Arduino website – make it executable first:

```bash
chmod +x arduino-ide_*.AppImage
./arduino-ide_*.AppImage
```

> If the AppImage doesn't launch: `sudo pacman -S fuse2`

---

### 2. USB drivers – no manual installation needed

The CH340 (`ch341`) and CP210x kernel modules are already included in the CachyOS kernel. The board is detected automatically when plugged in.

Check if the module is loaded:

```bash
# For CH340 chip (most common on budget ESP32 boards)
lsmod | grep ch341

# For CP2102 chip
lsmod | grep cp210x
```

Load manually if needed:

```bash
sudo modprobe ch341
# or
sudo modprobe cp210x
```

After plugging in the board, check the port:

```bash
ls /dev/ttyUSB* /dev/ttyACM*
# or
dmesg | tail -15
```

The ESP32 typically appears as:
- `/dev/ttyUSB0` – CH340 chip
- `/dev/ttyACM0` – CP2102 chip

---

### 3. Unlock serial port access – `uucp` group (critical!)

This is the **most common mistake on Arch/CachyOS**: without group membership, Arduino IDE cannot access the serial port.

On **Arch/CachyOS** the group is called `uucp` (not `dialout` as on Ubuntu/Debian).

```bash
# Add your user to the uucp group
sudo usermod -aG uucp $USER
```

Then **log out and log back in** (or reboot). Verify it worked:

```bash
groups
# Output must contain "uucp"
```

Quick test without re-login (current session only):

```bash
newgrp uucp
```

> Without this step, Arduino IDE will show **"Error opening serial port"** or uploads will fail with a permissions error.

---

### 4. Flatpak sandbox limitation

If Arduino IDE was installed via **Flatpak**, it cannot access `/dev/ttyUSB0` by default because Flatpak runs in a sandbox.

Grant access:

```bash
flatpak override --user --filesystem=/dev/ttyUSB0 cc.arduino.IDE2
flatpak override --user --filesystem=/dev/ttyACM0 cc.arduino.IDE2
```

Better alternative: the **AUR version** (`arduino-ide-bin`) runs without a sandbox and doesn't have this problem.

---

### 5. python-pyserial (if esptool errors occur)

```bash
sudo pacman -S python-pyserial
```

---

## Install board support

After connecting the board via USB-C:

1. Go to **Tools → Board → esp32**
2. Select **"ESP32 Dev Module"** (works for most ESP32-D boards)
3. Go to **Tools → Port** and select your board's COM port
4. Verify these settings:

| Setting | Value |
|---|---|
| Board | ESP32 Dev Module |
| Upload Speed | 921600 |
| Flash Frequency | 80 MHz |
| Flash Mode | QIO |
| Partition Scheme | Default 4MB with spiffs |
| Core Debug Level | None |

---

## Configure & upload the code

### 1. Open the sketch

Open `ESP32_mini_webserver/ESP32_mini_webserver.ino` in Arduino IDE.

### 2. Enter your WiFi credentials

Edit these two lines at the top of the sketch:

```cpp
const char* ssid     = "YOUR_WIFI";      // Your WiFi network name (2.4 GHz)
const char* password = "YOUR_PASSWORD";  // Your WiFi password
```

> **Important:** ESP32 only supports **2.4 GHz networks**, not 5 GHz!

### 3. Upload the code

1. Click the **arrow (→) "Upload"** button in Arduino IDE
2. If you see **"Connecting…"**, hold the **BOOT button** on the board until the upload starts  
   *(commonly required on HW-394)*
3. Wait until the console shows **"Leaving… Hard resetting via RTS pin…"**

### 4. Find the IP address

1. Go to **Tools → Serial Monitor**
2. Set baud rate to **115200**
3. Press the **EN/Reset button** on the board
4. The IP address will appear in the monitor, e.g.:
   ```
   Verbinde mit WLAN..........
   Verbunden!
   IP-Adresse: 192.168.1.42
   mDNS gestartet: http://esp32.local
   Webserver gestartet.
   ```

---

## Access the webserver

1. Open a browser on a device connected to the **same WiFi network**
2. Enter the IP address, e.g.: `http://192.168.1.42`  
   **Or:** Use the mDNS hostname: `http://esp32.local` *(no IP lookup needed)*
3. The web interface loads with a toggle button to control the LED (Pin 2 / onboard blue LED)

> **mDNS notes:**  
> - Linux & Android: works out of the box (Avahi daemon is usually pre-installed)  
> - Windows: requires [Bonjour](https://support.apple.com/kb/DL999) (or install iTunes)  
> - **CachyOS:** `sudo pacman -S avahi nss-mdns` – then add `mdns_minimal` before `resolve` in the `hosts:` line of `/etc/nsswitch.conf`

---

## JSON API

The webserver exposes two JSON endpoints — useful for Home Assistant, Node-RED, shell scripts, or other microcontrollers.

> **Library:** `ArduinoJson` by Benoit Blanchon – install once in Arduino IDE:  
> **Tools → Manage Libraries → search "ArduinoJson" → Install**

### `GET /api/status`

Returns the current state of the board.

```bash
curl http://esp32.local/api/status
```

Response:
```json
{
  "led": false,
  "uptime": 142,
  "ip": "192.168.1.42",
  "hostname": "esp32.local",
  "rssi": -58
}
```

| Field | Meaning |
|---|---|
| `led` | `true` = ON, `false` = OFF |
| `uptime` | Seconds since last reboot |
| `ip` | Local network IP address |
| `hostname` | mDNS hostname |
| `rssi` | WiFi signal strength in dBm (closer to 0 = better) |

### `GET /api/toggle`

Toggles the LED and returns the new state. Ideal for curl scripts or automations.

```bash
curl http://esp32.local/api/toggle
```

Response:
```json
{
  "led": true,
  "uptime": 145
}
```

### Home Assistant – REST integration

```yaml
# configuration.yaml
switch:
  - platform: rest
    name: ESP32 LED
    resource: http://esp32.local/api/toggle
    state_resource: http://esp32.local/api/status
    body_on: ""
    body_off: ""
    is_on_template: "{{ value_json.led }}"
    method: GET
```

---

## Possible use cases

### Home automation
- Control outlets, relays, or lights from a browser or smartphone
- Remotely manage blinds, fans, or radiators

### Sensor monitoring
- Display live temperature/humidity readings (DHT22, BME280) in the browser
- Monitor soil moisture for plants

### Prototyping & hobby projects
- Show 3D printer status
- Aquarium controller (pumps, lighting)
- Doorbell with camera integration

### Education & learning
- Introduction to IoT and web development
- Understand REST API fundamentals
- Practice network programming with C++

---

## Common errors & fixes

| Error | Solution |
|---|---|
| `Connecting…` hangs | Hold the BOOT button while uploading |
| No COM port visible | Install USB driver (CH340/CP210x), try a different cable |
| WiFi connection fails | Use a 2.4 GHz network only; double-check SSID & password |
| Page doesn't load | Confirm PC and ESP32 are on the same network; check firewall |
| `esptool.py` error | Reduce Upload Speed to `115200` |
| Board gets hot | Check for short circuits; verify power supply |
| **[CachyOS]** "Error opening serial port" | `sudo usermod -aG uucp $USER` → log out → log back in |
| **[CachyOS]** Flatpak can't see the port | `flatpak override --user --filesystem=/dev/ttyUSB0 cc.arduino.IDE2` |
| **[CachyOS]** AppImage won't launch | `sudo pacman -S fuse2` |
| **[CachyOS]** `ch341` module missing | `sudo modprobe ch341` or `sudo modprobe cp210x` |

---

## Project structure

```
ESP-32-mini-Webserver/
├── ESP32_mini_webserver/
│   └── ESP32_mini_webserver.ino   # Main Arduino sketch
└── README.md
```

## License

MIT – feel free to use, modify, and share.
