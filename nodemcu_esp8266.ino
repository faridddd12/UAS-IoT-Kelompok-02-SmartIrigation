#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <SoftwareSerial.h>
#include "secrets.h"

SoftwareSerial unoSerial(D5, D6);
ESP8266WebServer server(80);

String dataJsonTerakhir = "{\"id\":\"smartirrigate-01\",\"tanah\":0,\"suhu\":0,\"udara\":0,\"cahaya\":0,\"pompa\":0,\"mode\":0,\"error\":0}";

// Variabel penyimpanan nilai terbaru untuk dikirim ke ThingSpeak
int valTanah = 0, valCahaya = 0, valPompa = 0, valError = 0;
float valSuhu = 0.0, valUdara = 0.0;
bool dataBaruMasuk = false;

unsigned long lastThingSpeak = 0;
const unsigned long intervalTS = 20000; // Kirim ke ThingSpeak tiap 20 detik
String statusCloud = "Menunggu pengiriman...";

const char HTML_DASHBOARD[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Smart Irrigation Dashboard</title>
  <style>
    * { box-sizing: border-box; font-family: 'Segoe UI', sans-serif; margin: 0; padding: 0; }
    body { background: #0f172a; color: #f8fafc; padding: 14px; display: flex; justify-content: center; }
    .container { width: 100%; max-width: 440px; }
    .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px; }
    h2 { color: #38bdf8; font-size: 1.25rem; }
    .badge { padding: 4px 10px; border-radius: 99px; font-size: 0.75rem; font-weight: bold; }
    .online { background: #14532d; color: #4ade80; border: 1px solid #22c55e; }
    .offline { background: #7f1d1d; color: #fca5a5; border: 1px solid #ef4444; }
    .meta { font-size: 0.75rem; color: #94a3b8; margin-bottom: 10px; display: flex; justify-content: space-between; }
    .cloud-bar { font-size: 0.75rem; background: #1e293b; color: #a5b4fc; padding: 7px 10px; border-radius: 8px; border: 1px solid #334155; margin-bottom: 12px; text-align: center; }
    .status-banner { padding: 11px; border-radius: 10px; text-align: center; font-weight: bold; margin-bottom: 14px; background: #1e293b; border: 1px solid #334155; font-size: 0.9rem; }
    .err { background: #7f1d1d !important; border-color: #ef4444 !important; color: #fecaca; }
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 14px; }
    .card { background: #1e293b; padding: 13px; border-radius: 12px; border: 1px solid #334155; text-align: center; }
    .card span { display: block; font-size: 0.75rem; color: #94a3b8; margin-bottom: 4px; }
    .card strong { font-size: 1.35rem; color: #f8fafc; }
    .chart-box { background: #1e293b; padding: 12px; border-radius: 12px; border: 1px solid #334155; margin-bottom: 14px; }
    .chart-title { font-size: 0.8rem; color: #cbd5e1; margin-bottom: 8px; }
    canvas { width: 100%; height: 110px; display: block; }
    .control-box { background: #1e293b; padding: 14px; border-radius: 12px; border: 1px solid #334155; text-align: center; }
    .confirm { font-size: 0.78rem; color: #38bdf8; margin-top: 6px; min-height: 18px; }
    .row { display: flex; gap: 8px; margin-top: 10px; margin-bottom: 8px; }
    button { flex: 1; padding: 12px; border: none; border-radius: 9px; font-size: 0.9rem; font-weight: bold; cursor: pointer; }
    .btn-on { background: #22c55e; color: #052e16; }
    .btn-off { background: #ef4444; color: #ffffff; }
    .btn-auto { width: 100%; background: #38bdf8; color: #0f172a; }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h2>Smart Irrigation</h2>
      <span id="connBadge" class="badge online">ONLINE</span>
    </div>
    <div class="meta">
      <span>ID: smartirrigate-01</span>
      <span id="lastTime">Update: --:--:--</span>
    </div>
    <div id="cloudStatus" class="cloud-bar">ThingSpeak Cloud: Menunggu sinkronisasi...</div>

    <div id="banner" class="status-banner">Status: Memuat data...</div>

    <div class="grid">
      <div class="card"><span>Kelembapan Tanah</span><strong id="tanah">-- %</strong></div>
      <div class="card"><span>Status Pompa</span><strong id="pompa">--</strong></div>
      <div class="card"><span>Suhu Udara</span><strong id="suhu">-- °C</strong></div>
      <div class="card"><span>Kelembapan Udara</span><strong id="udara">-- %</strong></div>
      <div class="card" style="grid-column: span 2;"><span>Intensitas Cahaya (LDR)</span><strong id="cahaya">--</strong></div>
    </div>

    <div class="chart-box">
      <div class="chart-title">Tren Kelembapan Tanah (Deret Waktu - 20 Data Terakhir)</div>
      <canvas id="trendChart" width="380" height="110"></canvas>
    </div>

    <div class="control-box">
      <div id="modeText" style="font-size: 0.85rem; color: #cbd5e1;">Mode: OTOMATIS (Histeresis &lt;40% ON, &ge;55% OFF)</div>
      <div id="confirmText" class="confirm">Konfirmasi: Sinkron dengan aktuator</div>
      <div class="row">
        <button class="btn-on" onclick="kirimPerintah('ON')">NYALAKAN</button>
        <button class="btn-off" onclick="kirimPerintah('OFF')">MATIKAN</button>
      </div>
      <button class="btn-auto" onclick="kirimPerintah('AUTO')">MODE OTOMATIS (SENSOR)</button>
    </div>
  </div>

  <script>
    let riwayatTanah = [];

    function gambarGrafik() {
      const c = document.getElementById('trendChart');
      const ctx = c.getContext('2d');
      ctx.clearRect(0, 0, c.width, c.height);

      const y40 = c.height - (40 / 100) * c.height;
      ctx.strokeStyle = '#ef4444';
      ctx.setLineDash([4, 4]);
      ctx.beginPath();
      ctx.moveTo(0, y40);
      ctx.lineTo(c.width, y40);
      ctx.stroke();
      ctx.setLineDash([]);

      if (riwayatTanah.length < 2) return;
      ctx.strokeStyle = '#38bdf8';
      ctx.lineWidth = 2.5;
      ctx.beginPath();
      riwayatTanah.forEach((val, idx) => {
        const x = (idx / (riwayatTanah.length - 1)) * c.width;
        const y = c.height - (val / 100) * c.height;
        if (idx === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      });
      ctx.stroke();
    }

    function updateData() {
      fetch('/data')
        .then(res => res.json())
        .then(d => {
          const badge = document.getElementById('connBadge');
          badge.innerText = 'ONLINE';
          badge.className = 'badge online';

          const now = new Date();
          document.getElementById('lastTime').innerText = 'Update: ' + now.toLocaleTimeString('id-ID');
          document.getElementById('cloudStatus').innerText = 'ThingSpeak Cloud: ' + d.cloud;

          document.getElementById('tanah').innerText  = d.tanah + ' %';
          document.getElementById('suhu').innerText   = d.suhu + ' °C';
          document.getElementById('udara').innerText  = d.udara + ' %';
          document.getElementById('cahaya').innerText = d.cahaya;
          document.getElementById('pompa').innerText  = d.pompa == 1 ? 'MENYALA' : 'MATI';
          document.getElementById('pompa').style.color = d.pompa == 1 ? '#4ade80' : '#f87171';

          const modeNama = d.mode == 1 ? 'MANUAL (PAKSA ON)' : (d.mode == 2 ? 'MANUAL (PAKSA OFF)' : 'OTOMATIS (HISTERESIS)');
          document.getElementById('modeText').innerText = 'Mode: ' + modeNama;
          document.getElementById('confirmText').innerText = 'Konfirmasi Status: Pompa ' + (d.pompa == 1 ? 'AKTIF (1)' : 'NONAKTIF (0)');

          const banner = document.getElementById('banner');
          if (d.error == 1) {
            banner.innerText = 'GANGGUAN: Sensor Tidak Valid (E) - Pompa Diamankan OFF';
            banner.classList.add('err');
          } else {
            banner.innerText = d.pompa == 1 ? 'Status: Menyiram Tanaman (1)' : 'Status: Kondisi Aman / Standby (0)';
            banner.classList.remove('err');
          }

          riwayatTanah.push(d.tanah);
          if (riwayatTanah.length > 20) riwayatTanah.shift();
          gambarGrafik();
        })
        .catch(() => {
          const badge = document.getElementById('connBadge');
          badge.innerText = 'OFFLINE';
          badge.className = 'badge offline';
        });
    }

    function kirimPerintah(aksi) {
      document.getElementById('confirmText').innerText = 'Mengirim perintah ' + aksi + '...';
      fetch('/pompa?state=' + aksi).then(() => setTimeout(updateData, 400));
    }

    setInterval(updateData, 2000);
    updateData();
  </script>
</body>
</html>
)rawliteral";

float ambilNilaiJson(String json, String key) {
  int idx = json.indexOf("\"" + key + "\":");
  if (idx == -1) return 0;
  int start = idx + key.length() + 3;
  int end = json.indexOf(",", start);
  if (end == -1) end = json.indexOf("}", start);
  return json.substring(start, end).toFloat();
}

void kirimKeThingSpeak() {
  if (WiFi.status() != WL_CONNECTED) {
    statusCloud = "Terputus (Wi-Fi Offline)";
    return;
  }

  WiFiClient client;
  HTTPClient http;
  String url = "http://api.thingspeak.com/update?api_key=" + String(TS_API_KEY) +
               "&field1=" + String(valTanah) +
               "&field2=" + String(valSuhu, 1) +
               "&field3=" + String(valUdara, 1) +
               "&field4=" + String(valCahaya) +
               "&field5=" + String(valPompa);

  http.begin(client, url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    if (payload.toInt() > 0) {
      statusCloud = "Terkirim (Entry #" + payload + ")";
    } else {
      statusCloud = "Menunggu interval 15d...";
    }
  } else {
    statusCloud = "Gagal kirim (Internet Mati)";
  }
  http.end();
}

void setup() {
  Serial.begin(9600);
  unoSerial.begin(9600);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  Serial.print("Dashboard HP: http://");
  Serial.println(WiFi.localIP());

  server.on("/", []() { server.send_P(200, "text/html", HTML_DASHBOARD); });
  server.on("/data", []() {
    String res = dataJsonTerakhir;
    res.remove(res.length() - 1);
    res += ",\"cloud\":\"" + statusCloud + "\"}";
    server.send(200, "application/json", res);
  });
  server.on("/pompa", []() {
    if (server.hasArg("state")) {
      String state = server.arg("state");
      if (state == "ON") unoSerial.println("PUMP_ON");
      else if (state == "OFF") unoSerial.println("PUMP_OFF");
      else unoSerial.println("PUMP_AUTO");
    }
    server.send(200, "text/plain", "OK");
  });

  server.begin();
}

void loop() {
  if (unoSerial.available()) {
    String masuk = unoSerial.readStringUntil('\n');
    masuk.trim();
    if (masuk.startsWith("{") && masuk.endsWith("}")) {
      dataJsonTerakhir = masuk;
      valTanah  = (int)ambilNilaiJson(masuk, "tanah");
      valSuhu   = ambilNilaiJson(masuk, "suhu");
      valUdara  = ambilNilaiJson(masuk, "udara");
      valCahaya = (int)ambilNilaiJson(masuk, "cahaya");
      valPompa  = (int)ambilNilaiJson(masuk, "pompa");
      dataBaruMasuk = true;
    }
  }

  if (dataBaruMasuk && (millis() - lastThingSpeak >= intervalTS)) {
    lastThingSpeak = millis();
    kirimKeThingSpeak();
  }

  server.handleClient();
}
