const pinsEl   = document.getElementById('pins');
const sensorEl = document.getElementById('sensor');
const errorEl  = document.getElementById('error');

function renderPins(pins) {
  pinsEl.innerHTML = pins.map(p => `
    <div class="card row">
      <span><b>${p.name}</b> (GPIO&nbsp;${p.gpio}):&nbsp;${p.state ? 'AN' : 'AUS'}</span>
      <button class="${p.state ? 'on' : 'off'}" onclick="toggle(${p.gpio})">Umschalten</button>
    </div>`).join('');
}

function renderSensor(s) {
  if (!s || !s.available) { sensorEl.innerHTML = ''; return; }
  sensorEl.innerHTML = `
    <div class="card" style="margin-top:20px">
      <b style="color:#00bcd4">Sensor (BME280)</b>
      <div class="sensor-grid" style="margin-top:12px">
        <div><div class="sensor-value">${s.temp_c}&nbsp;°C</div><div class="sensor-label">Temperatur</div></div>
        <div><div class="sensor-value">${s.humidity}&nbsp;%</div><div class="sensor-label">Luftfeuchtigkeit</div></div>
        <div><div class="sensor-value">${s.pressure}</div><div class="sensor-label">hPa</div></div>
      </div>
    </div>`;
}

function applyData(data) {
  renderPins(data.pins);
  renderSensor(data.sensor);
  errorEl.textContent = '';
}

// T3-A: WebSocket läuft jetzt als Pfad /ws auf Port 80 (kein eigener Port mehr)
const wsProto = location.protocol === 'https:' ? 'wss' : 'ws';
const socket  = new WebSocket(`${wsProto}://${location.hostname}/ws`);

socket.onmessage = e => applyData(JSON.parse(e.data));

socket.onclose = () => {
  errorEl.textContent = 'WebSocket getrennt – Seite neu laden.';
};

socket.onerror = () => {
  // Fallback: einmalig per HTTP laden
  fetch('/api/status').then(r => r.json()).then(applyData).catch(() => {
    errorEl.textContent = 'Verbindung zum ESP32 unterbrochen.';
  });
};

async function toggle(gpio) {
  await fetch('/api/toggle?pin=' + gpio);
  // Update kommt automatisch per WebSocket-Broadcast vom Server
}
