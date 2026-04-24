const pinsEl  = document.getElementById('pins');
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

async function loadStatus() {
  try {
    const res  = await fetch('/api/status');
    const data = await res.json();
    renderPins(data.pins);
    renderSensor(data.sensor);
    errorEl.textContent = '';
  } catch (e) {
    errorEl.textContent = 'Verbindung zum ESP32 unterbrochen.';
  }
}

async function toggle(gpio) {
  await fetch('/api/toggle?pin=' + gpio);
  loadStatus();
}

// Initialer Ladevorgang + Polling alle 3 s (wird durch WebSocket in T2-C ersetzt)
loadStatus();
setInterval(loadStatus, 3000);
