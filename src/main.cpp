#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <DNSServer.h>

// =====================================================
// PIN MAP ESP32
// =====================================================
#define LED_SYSTEM          2   
#define LED_FRONT_PRESSURE  5   
#define LED_REAR_PRESSURE   19  
#define LED_FRONT_SPEED     3   
#define LED_REAR_SPEED      22  

#define PIN_ADC_FRONT_PRESSURE 32
#define PIN_ADC_REAR_PRESSURE  33
#define PIN_FRONT_SPEED_SENSOR 26 
#define PIN_REAR_SPEED_SENSOR  27 

// =====================================================
// WIFI AP & CAPTIVE PORTAL SETTINGS
// =====================================================
const char* AP_SSID = "SMART BRAKE EDU";
const char* AP_PASS = "edubrake2026";
const char* OTA_HOSTNAME = "SMART-BRAKE-EDU";

const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer server(80);
Preferences prefs;

// =====================================================
// DATA STRUCTURES
// =====================================================
struct BrakeSetting {
  uint16_t numTooth;
  float diameterMM;
  float maxSpeedKMH;
  int minADC;
  int maxADC;
  float maxPressurePSI;
};

BrakeSetting frontSet = { 40, 588.0, 120.0, 990, 4095, 100.0 };
BrakeSetting rearSet  = { 40, 537.2, 120.0, 990, 4095, 100.0 };

// =====================================================
// GLOBAL MEASUREMENT VARIABLES
// =====================================================
volatile uint32_t frontPulseCount = 0;
volatile uint32_t rearPulseCount = 0;
volatile uint32_t lastFrontPulseUs = 0;
volatile uint32_t lastRearPulseUs = 0;

const uint32_t PULSE_DEBOUNCE_US = 15000;
uint32_t lastFrontPulseCount = 0;
uint32_t lastRearPulseCount = 0;
uint32_t lastSampleMs = 0;
uint32_t lastSpeedSampleMs = 0;

const uint32_t UI_INTERVAL_MS = 100;
const uint32_t SPEED_INTERVAL_MS = 1000;

float emaFrontADC = 0.0;
float emaRearADC = 0.0;
const float ADC_ALPHA = 0.3;

int rawFrontADC = 0;
int rawRearADC = 0;
float frontPressurePSI = 0.0;
float rearPressurePSI = 0.0;
float frontSpeedKMH = 0.0;
float rearSpeedKMH = 0.0;
float frontRatio = 0.0;
float rearRatio = 0.0;
bool frontPressureConnected = false;
bool rearPressureConnected = false;

// =====================================================
// LED MANAGEMENT VARIABLES
// =====================================================
unsigned long lastSystemBlink = 0;
bool systemLedState = false;
volatile bool frontPulseLed = false;
volatile bool rearPulseLed = false;
unsigned long frontPulseLedTimer = 0;
unsigned long rearPulseLedTimer = 0;
const unsigned long PULSE_LED_DURATION = 50;
unsigned long lastPressureLedUpdate = 0;
const unsigned long PRESSURE_LED_INTERVAL = 500;

// =====================================================
// SPEED SENSOR DISCONNECT DETECTION
// =====================================================
unsigned long lastFrontPulseTime = 0;
unsigned long lastRearPulseTime = 0;
const unsigned long SPEED_SENSOR_TIMEOUT = 3000;

// =====================================================
// HTML PAGE - FINAL v7.0
// =====================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">
<title>SMART BRAKE EDU v7.0</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<script src="https://cdn.sheetjs.com/xlsx-0.20.0/package/dist/xlsx.full.min.js"></script>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Nunito:wght@400;700;900&display=swap');
  :root { --blue: #0d6efd; --orange: #fd7e14; --bg: #f1f5f9; --dark: #0f172a; --green: #10b981; --red: #ef4444; }
  * { box-sizing: border-box; font-family: 'Nunito', sans-serif; margin: 0; padding: 0; user-select:none; }
  body { background: var(--bg); color: var(--dark); display: flex; flex-direction: column; height: 100vh; overflow-x:hidden; }
  
  .navbar { display: flex; justify-content: space-between; align-items: center; background: var(--dark); padding: 12px 20px; color: white; flex-wrap: wrap; gap: 10px; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.1); z-index: 10;}
  .logo { font-size: 22px; font-weight: 900; font-style: italic; letter-spacing: 1px;}
  .logo span { color: var(--red); }
  .tabs { display: flex; gap: 8px; flex-wrap: wrap; }
  .tab-btn { background: transparent; color: #cbd5e1; border: 2px solid transparent; padding: 8px 16px; border-radius: 8px; font-weight: 800; cursor: pointer; transition: all 0.3s ease; }
  .tab-btn.active { background: var(--blue); color: white; box-shadow: 0 4px 10px rgba(13,110,253,0.3); }
  .conn-status { display: flex; align-items: center; gap: 8px; font-size: 14px; font-weight: 800; background: rgba(0,0,0,0.3); padding: 6px 12px; border-radius: 20px;}
  .conn-dot { width: 10px; height: 10px; background: var(--red); border-radius: 50%; box-shadow: 0 0 8px var(--red); }

  .tab-content { display: none; padding: 20px; flex: 1; overflow-y: auto; }
  .tab-content.active { display: block; }
  .panel-card { background: white; border-radius: 16px; padding: 20px; box-shadow: 0 10px 15px -3px rgba(0,0,0,0.05); border: 1px solid #e2e8f0; }

  .dash-grid { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 20px; }
  .dash-panel { display: flex; flex-direction: column; align-items: center; justify-content: space-between; text-align:center; }
  .panel-title { font-weight: 900; font-size: 18px; letter-spacing: 1px; margin-bottom: 10px; text-transform: uppercase;}
  
  .gauge-svg { width: 100%; max-width: 200px; overflow: visible; filter: drop-shadow(0px 8px 6px rgba(0,0,0,0.1)); }
  .speed-text { margin-top: -40px; text-align: center; }
  .s-val { font-size: 55px; font-weight: 900; line-height: 1; }
  .s-unit { font-size: 16px; color: #64748b; font-weight: 800; text-transform: uppercase; }
  
  .bars-wrapper { display: flex; justify-content: center; gap: 40px; height: 180px; width: 100%; align-items: flex-end; margin: 20px 0; }
  .bar-col { display: flex; flex-direction: column; align-items: center; height: 100%; justify-content: flex-end;}
  .bar-bg { width: 55px; background: #f1f5f9; border-radius: 12px; height: 140px; position: relative; overflow: hidden; box-shadow: inset 0 4px 6px rgba(0,0,0,0.1); border: 1px solid #e2e8f0;}
  .bar-fill { position: absolute; bottom: 0; left: 0; right: 0; transition: height 0.15s ease-out; border-radius: 0 0 12px 12px;}
  .bar-f { background: linear-gradient(0deg, #1d4ed8, #60a5fa); }
  .bar-r { background: linear-gradient(0deg, #ea580c, #fcd34d); }
  .bar-label { font-weight: 900; font-size: 20px; margin-top: 10px; }

  .ratio-box { background: #f8fafc; padding: 10px 20px; border-radius: 12px; border: 1px dashed #cbd5e1; width: 100%;}
  
  .record-box { background: white; width: 100%; margin-bottom: 15px; }
  .btn-record { width:100%; padding:15px; font-size:18px; font-weight:900; border:none; border-radius:12px; background: var(--blue); color:white; cursor:pointer; transition:all 0.2s;}
  .btn-record.recording { background: var(--red); animation: pulse 1.5s infinite; }
  @keyframes pulse { 0% { box-shadow: 0 0 0 0 rgba(239,68,68,0.7); } 70% { box-shadow: 0 0 0 15px rgba(239,68,68,0); } 100% { box-shadow: 0 0 0 0 rgba(239,68,68,0); } }

  .rider-input { display:flex; gap:8px; margin-bottom:10px; }
  .rider-input input { flex:1; padding:10px; border:2px solid #e2e8f0; border-radius:8px; font-weight:800; font-size:14px; outline:none; }
  .rider-input button { padding:10px 15px; background:var(--green); color:white; border:none; border-radius:8px; font-weight:800; cursor:pointer; }

  .sensor-status { display: flex; justify-content: center; gap: 15px; margin-top: 10px; font-size: 11px; font-weight: 700; }
  .status-dot { display: inline-block; width: 8px; height: 8px; border-radius: 50%; margin-right: 4px; }
  .status-dot.on { background: var(--green); box-shadow: 0 0 6px var(--green); }
  .status-dot.off { background: var(--red); box-shadow: 0 0 6px var(--red); }

  #captiveWarning { animation: shake 0.5s ease-in-out; }
  @keyframes shake { 0%,100%{transform:translateX(0)} 25%{transform:translateX(-5px)} 75%{transform:translateX(5px)} }

  .summary-grid { display: grid; grid-template-columns: 1fr 1fr 1fr 1fr; gap: 15px; margin-bottom: 20px; }
  .summary-card { padding: 15px; border-radius: 12px; text-align: center; }
  .summary-card .label { font-size: 12px; font-weight: 800; color: #64748b; }
  .summary-card .value { font-size: 36px; font-weight: 900; }
  .summary-card .unit { font-size: 12px; color: #64748b; }
  .card-blue { background: linear-gradient(135deg,#eff6ff,#dbeafe); } .card-blue .value { color: var(--blue); }
  .card-red { background: linear-gradient(135deg,#fef2f2,#fee2e2); } .card-red .value { color: var(--red); }
  .card-green { background: linear-gradient(135deg,#f0fdf4,#dcfce7); } .card-green .value { color: var(--green); }
  .card-yellow { background: linear-gradient(135deg,#fefce8,#fef9c3); } .card-yellow .value { color: #854d0e; }

  .logger-layout { display: grid; grid-template-columns: 2fr 1fr; gap: 20px; }
  
  .session-mgr { background: white; padding: 15px 20px; border-radius: 12px; margin-bottom: 20px; display:flex; justify-content:space-between; align-items:center; border: 1px solid #e2e8f0; flex-wrap:wrap; gap:10px;}
  .session-select { padding: 8px 12px; border-radius: 8px; font-weight: 800; border: 2px solid #cbd5e1; outline:none; min-width:250px; }
  .btn-clear { background: #fee2e2; color: var(--red); border: none; padding: 10px 15px; border-radius: 8px; font-weight: 900; cursor: pointer; transition: 0.2s;}
  .btn-clear:hover { background: var(--red); color: white; }
  
  .graph-container { position: relative; width: 100%; height: 400px; background: white; border: 1px solid #e2e8f0; border-radius: 12px; padding: 15px; display: flex; flex-direction: column;}
  .legend-box { display: flex; justify-content: center; gap: 15px; font-size: 12px; font-weight: 800; margin-bottom: 10px; flex-wrap: wrap; background: #f8fafc; padding: 8px; border-radius: 8px;}
  .leg-item { display: flex; align-items: center; gap: 5px; color: #475569;}
  .l-sf { width: 15px; height: 3px; background: var(--blue); border-radius:2px;}
  .l-sr { width: 15px; height: 3px; background: var(--orange); border-radius:2px;}
  .l-pf { width: 15px; height: 0px; border-bottom: 2px dashed var(--blue); }
  .l-pr { width: 15px; height: 0px; border-bottom: 2px dashed var(--orange); }
  #logCanvas { width: 100%; flex: 1; }

  .timeline-box, .dist-box { background: #f8fafc; border-radius: 12px; padding: 15px; border: 1px solid #e2e8f0; font-size: 13px; margin-bottom: 15px; }
  .dist-box { max-height: 200px; overflow-y: auto; font-size: 12px; }
  
  .btn-action { padding: 10px 15px; font-size: 14px; font-weight: 900; color: white; border: none; border-radius: 8px; cursor: pointer; transition:0.2s; }

  #btnToggleGrade { transition: all 0.2s ease; }
  #btnToggleGrade:hover { transform: scale(1.1); box-shadow: 0 2px 8px rgba(0,0,0,0.15); }

  .profile-box { background: #f8fafc; border: 1px solid #e2e8f0; padding: 15px; border-radius: 12px; margin-bottom: 20px; }
  .profile-controls { display: flex; gap: 10px; margin-top: 10px;}
  .btn-prof { flex: 1; padding: 10px; border: none; border-radius: 8px; font-weight: 800; cursor: pointer; color: white; }
  
  .settings-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }
  .set-title { font-size: 18px; font-weight: 900; margin-bottom: 15px; border-bottom: 2px solid #e2e8f0; padding-bottom: 8px;}
  .input-group { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; background: #f8fafc; padding: 8px 12px; border-radius: 8px; border: 1px solid #e2e8f0;}
  .input-group label { font-weight: 800; font-size: 13px; color: #475569;}
  .input-group input { width: 100px; padding: 8px; border: 2px solid #cbd5e1; border-radius: 6px; text-align: center; font-weight: 900; font-size:14px; color: var(--blue); outline:none;}
  
  .calib-box { background: white; padding: 15px; border-radius: 12px; margin-top: 15px; border:2px dashed #cbd5e1;}
  .calib-btn { width: 100%; padding: 12px; border: none; border-radius: 8px; font-weight: 900; cursor: pointer; margin-bottom: 10px; color: white;}
  .btn-zero { background: #64748b; } .btn-zero:hover { background: #475569; }
  .btn-max { background: var(--red); } .btn-max:hover { background: #dc2626; }
  .btn-save-set { background: var(--green); color: white; border: none; padding: 15px; border-radius: 12px; font-weight: 900; font-size: 16px; cursor: pointer; margin-top: 20px; width: 100%; }

  .grade-circle { font-size: 45px; font-weight: 900; line-height: 1; }

  @media (max-width: 900px) {
    .dash-grid, .logger-layout, .settings-grid, .summary-grid { grid-template-columns: 1fr 1fr; }
    .summary-grid { grid-template-columns: 1fr 1fr; }
    .tabs { justify-content: center; width: 100%;}
  }
</style>
</head>
<body>

<div class="navbar">
  <div class="logo">SMART<span>BRAKE</span> EDU</div>
  <div class="tabs">
    <button class="tab-btn active" onclick="switchTab('userTab', this)">DASHBOARD</button>
    <button class="tab-btn" onclick="switchTab('logTab', this)">DATA LOGGER</button>
    <button class="tab-btn" onclick="switchTab('setTab', this)">SETTINGS</button>
  </div>
  <div class="conn-status"><div class="conn-dot" id="connDot"></div><span id="connText">Connecting...</span></div>
</div>

<!-- ==================== DASHBOARD TAB ==================== -->
<div id="userTab" class="tab-content active">
  <div id="captiveWarning" style="display:none; background:#fff3cd; border:2px solid #ff9800; padding:15px; border-radius:12px; margin-bottom:15px; text-align:center;">
    <div style="font-size:40px; margin-bottom:10px;">⚠️</div>
    <div style="font-weight:900; font-size:16px; margin-bottom:5px;">Mode Terbatas Terdeteksi!</div>
    <div style="font-size:13px; color:#64748b; margin-bottom:15px;">Fitur Export & History tidak tersedia di pop-up WiFi.</div>
    <button onclick="openInBrowser()" style="background:#0d6efd; color:white; border:none; padding:12px 25px; border-radius:8px; font-weight:900; font-size:16px; cursor:pointer; width:100%;">🚀 BUKA DI CHROME/SAFARI</button>
    <div style="font-size:11px; color:#64748b; margin-top:10px;">Atau ketik manual: <b>192.168.4.1</b></div>
  </div>

  <div class="dash-grid">
    <div class="panel-card dash-panel">
      <div class="panel-title" style="color:var(--blue);">FRONT SPEED</div>
      <svg viewBox="0 0 100 65" class="gauge-svg">
        <path d="M 10 60 A 40 40 0 0 1 90 60" fill="none" stroke="#e2e8f0" stroke-width="12" stroke-linecap="round"/>
        <path id="svgSpdF" d="M 10 60 A 40 40 0 0 1 90 60" fill="none" stroke="var(--blue)" stroke-width="12" stroke-linecap="round" stroke-dasharray="126" stroke-dashoffset="126" style="transition: stroke-dashoffset 0.2s;"/>
      </svg>
      <div class="speed-text"><div class="s-val" id="txtSpdF" style="color:var(--blue);">0</div><div class="s-unit">km/h</div></div>
    </div>
    
    <div class="panel-card dash-panel" style="justify-content: space-between;">
      <div class="rider-input">
        <input type="text" id="riderName" placeholder="Nama Pengendara (opsional)">
        <button onclick="saveRiderName()" title="Simpan Nama">💾</button>
      </div>
      <div class="record-box">
        <button id="btnRec" class="btn-record" onclick="toggleRecord()">🔴 START RECORD</button>
        <div id="recTime" style="font-weight:900; font-size:22px; color:var(--red); display:none; margin-top:10px;">00:00.0</div>
      </div>
      <div class="panel-title" style="color:#334155; margin-top:10px;">BRAKE PRESSURE</div>
      <div class="bars-wrapper">
        <div class="bar-col"><div class="bar-bg"><div id="barF" class="bar-fill bar-f" style="height:0%;"></div></div><div class="bar-label" style="color:var(--blue);" id="txtPercF">0%</div></div>
        <div class="bar-col"><div class="bar-bg"><div id="barR" class="bar-fill bar-r" style="height:0%;"></div></div><div class="bar-label" style="color:var(--orange);" id="txtPercR">0%</div></div>
      </div>
      <div class="ratio-box">
        <div style="font-weight:900; font-size:28px;"><span style="color:var(--blue)" id="ratF">0</span> : <span style="color:var(--orange)" id="ratR">0</span></div>
        <div style="font-size:12px; font-weight:800; color:#64748b;">RATIO ( F : R )</div>
      </div>
      <div class="sensor-status">
        <span><span class="status-dot" id="dotFS"></span> Speed F</span>
        <span><span class="status-dot" id="dotRS"></span> Speed R</span>
        <span><span class="status-dot" id="dotFP"></span> Press F</span>
        <span><span class="status-dot" id="dotRP"></span> Press R</span>
      </div>
    </div>

    <div class="panel-card dash-panel">
      <div class="panel-title" style="color:var(--orange);">REAR SPEED</div>
      <svg viewBox="0 0 100 65" class="gauge-svg">
        <path d="M 10 60 A 40 40 0 0 1 90 60" fill="none" stroke="#e2e8f0" stroke-width="12" stroke-linecap="round"/>
        <path id="svgSpdR" d="M 10 60 A 40 40 0 0 1 90 60" fill="none" stroke="var(--orange)" stroke-width="12" stroke-linecap="round" stroke-dasharray="126" stroke-dashoffset="126" style="transition: stroke-dashoffset 0.2s;"/>
      </svg>
      <div class="speed-text"><div class="s-val" id="txtSpdR" style="color:var(--orange);">0</div><div class="s-unit">km/h</div></div>
    </div>
  </div>
</div>

<!-- ==================== DATA LOGGER TAB v7.0 ==================== -->
<div id="logTab" class="tab-content">
  <div class="session-mgr">
    <div style="display:flex; align-items:center; gap:15px; flex:1;">
      <div style="font-weight:900; font-size:16px;">📋 BRAKE TEST HISTORY</div>
      <select id="sessionList" class="session-select" onchange="loadSession(this.value)" style="flex:1;">
        <option value="">-- Pilih Hasil Uji --</option>
      </select>
    </div>
    <div style="display:flex; gap:8px;">
      <button class="btn-clear" onclick="deleteSingleSession()">🗑️ Hapus Ini</button>
      <button class="btn-clear" onclick="clearAllSessions()" style="background:#fecaca;">💣 Hapus Semua</button>
    </div>
  </div>

  <div class="panel-card">
    <div class="summary-grid">
      <div class="summary-card card-blue"><div class="label">INITIAL SPEED</div><div class="value" id="sumSpd">0</div><div class="unit">km/h</div></div>
      <div class="summary-card card-red"><div class="label">STOPPING TIME</div><div class="value" id="sumTime">0.00</div><div class="unit">seconds</div></div>
      <div class="summary-card card-green"><div class="label">DISTANCE</div><div class="value" id="sumDist">0.0</div><div class="unit">meters</div></div>
      <div class="summary-card card-yellow"><div class="label">DISTRIBUTION</div><div class="value" id="sumRatio">0:0</div><div class="unit">F : R</div></div>
    </div>

    <div class="logger-layout">
      <div>
        <div style="font-weight:900; font-size:16px; margin-bottom:10px;">📈 BRAKE DYNAMICS GRAPH</div>
        <div class="graph-container">
          <div class="legend-box">
            <div class="leg-item"><div class="l-sf"></div> Speed F</div>
            <div class="leg-item"><div class="l-sr"></div> Speed R</div>
            <div class="leg-item"><div class="l-pf"></div> Press F (%)</div>
            <div class="leg-item"><div class="l-pr"></div> Press R (%)</div>
          </div>
          <canvas id="logCanvas"></canvas>
        </div>
      </div>
      
      <div>
        <div style="font-weight:900; font-size:16px; margin-bottom:10px;">⏱️ BRAKE EVENT TIMELINE</div>
        <div class="timeline-box" id="timelineBox">
          <div style="color:#64748b; text-align:center; padding:20px;">Pilih sesi untuk melihat kronologi</div>
        </div>

        <div style="font-weight:900; font-size:16px; margin:15px 0 10px;">📊 DISTRIBUTION HISTORY</div>
        <div class="dist-box" id="distHistoryBox">
          <div style="color:#64748b; text-align:center; padding:10px;">Data distribusi selama pengereman</div>
        </div>

        <div style="margin-top:15px; text-align:center; background:white; padding:15px; border-radius:12px; border:1px solid #e2e8f0; position:relative;">
          <button id="btnToggleGrade" onclick="toggleGrade()" style="position:absolute; top:8px; right:8px; background:#fee2e2; border:1px solid #e2e8f0; border-radius:6px; width:32px; height:32px; cursor:pointer; font-size:16px; display:flex; align-items:center; justify-content:center;" title="Show Grade">🚫</button>
          <div id="gradeContainer" style="display:none;">
            <div class="grade-circle" id="evGrade">-</div>
            <div style="font-weight:800; font-size:13px; color:#64748b; margin-top:8px;" id="evDesc">Lakukan uji pengereman dulu!</div>
          </div>
        </div>
      </div>
    </div>

    <div style="display:flex; gap:10px; margin-top:15px; flex-wrap:wrap;">
      <button class="btn-action" onclick="exportImage()" style="background:#6366f1; flex:1; min-width:100px;">📸 GRAFIK (PNG)</button>
      <button class="btn-action" onclick="exportCSV()" style="background:#217346; flex:1; min-width:100px;">📥 DATA (CSV)</button>
      <button class="btn-action" onclick="exportExcel()" style="background:#185a2e; flex:1; min-width:100px;">📊 EXCEL (XLSX)</button>
    </div>
  </div>
</div>

<!-- ==================== SETTINGS TAB ==================== -->
<div id="setTab" class="tab-content">
  <div class="profile-box">
    <div style="font-weight:900; font-size:16px; margin-bottom:5px;">🏍️ PROFILE KALIBRASI KENDARAAN</div>
    <div style="font-size:12px; color:#64748b;">Simpan berbagai settingan motor berbeda di HP/Laptop ini.</div>
    <div class="profile-controls">
      <select id="profileSelect" class="session-select" style="flex:2;" onchange="checkSportMode()"></select>
      <button class="btn-prof" style="background:var(--blue);" onclick="loadProfile()">📂 Load</button>
      <button class="btn-prof" style="background:var(--green);" onclick="saveNewProfile()">➕ New</button>
      <button class="btn-prof" style="background:var(--red);" onclick="deleteProfile()">🗑️ Delete</button>
    </div>
  </div>

  <div id="sportNotice" style="display:none; background:#e0f2fe; border:1px solid #0d6efd; padding:12px; border-radius:8px; margin-bottom:15px;">
    <div style="display:flex; align-items:center; gap:10px;">
      <span style="font-size:24px;">🏍️</span>
      <div>
        <div style="font-weight:900; color:#0d6efd;">Mode Motor Sport Terdeteksi</div>
        <div style="font-size:12px; color:#64748b;">Fitur pembacaan kopling masih dalam <b>tahap pengembangan</b>. Saat ini hanya membaca speed & pressure.</div>
      </div>
    </div>
  </div>

  <div class="panel-card settings-grid">
    <div>
      <div class="set-title">⚙️ RODA DEPAN</div>
      <div class="input-group"><label>Gigi Sensor/Magnet</label><input type="number" id="fTooth"></div>
      <div class="input-group"><label>Diameter Ban (mm)</label><input type="number" id="fDiam"></div>
      <div class="calib-box">
        <div style="font-size:12px; font-weight:900; margin-bottom:15px; color:#64748b; text-align:center;">KALIBRASI PRESSURE SENSOR</div>
        <div class="input-group" style="background:#e0f2fe; border-color:#bae6fd;"><label style="color:#0284c7;">ADC Filtered</label><input type="number" id="rawF" readonly style="background:transparent; border:none; font-weight:900; color:#0369a1;"></div>
        <button class="calib-btn btn-zero" onclick="doCalib('zero','F')">🔄 1. SET ZERO (Lepas Rem)</button>
        <button class="calib-btn btn-max" onclick="doCalib('max','F')">⚙️ 2. SET MAX (Tekan Mentok!)</button>
      </div>
    </div>
    <div>
      <div class="set-title">⚙️ RODA BELAKANG</div>
      <div class="input-group"><label>Gigi Sensor/Magnet</label><input type="number" id="rTooth"></div>
      <div class="input-group"><label>Diameter Ban (mm)</label><input type="number" id="rDiam"></div>
      <div class="calib-box">
        <div style="font-size:12px; font-weight:900; margin-bottom:15px; color:#64748b; text-align:center;">KALIBRASI PRESSURE SENSOR</div>
        <div class="input-group" style="background:#ffedd5; border-color:#fde68a;"><label style="color:#c2410c;">ADC Filtered</label><input type="number" id="rawR" readonly style="background:transparent; border:none; font-weight:900; color:#c2410c;"></div>
        <button class="calib-btn btn-zero" onclick="doCalib('zero','R')">🔄 1. SET ZERO (Lepas Rem)</button>
        <button class="calib-btn btn-max" onclick="doCalib('max','R')">⚙️ 2. SET MAX (Tekan Mentok!)</button>
      </div>
    </div>
  </div>
  <button class="btn-save-set" onclick="saveSettingsToDevice()">💾 UPLOAD PENGATURAN RODA KE ALAT (ROM)</button>
</div>

<script>
// ==========================================
// GLOBAL VARIABLES
// ==========================================
let startMs = null;
let isRecording = false;
let recStartMs = 0;
let tempLogData = []; 
let sessions = []; 
let activeSessionId = null;
const maxSpeed = 120;
const svgArcLength = 126;
const canvas = document.getElementById("logCanvas");
const ctx = canvas.getContext("2d");
let riderName = localStorage.getItem('riderName') || '';

// ==========================================
// PROFILE HONDA (DOUBLE CALIPER ONLY)
// ==========================================
let profiles = JSON.parse(localStorage.getItem('smartBrakeProfiles')) || {
  "Honda ADV 160 ABS": { fTooth: 2, fDiam: 588, rTooth: 2, rDiam: 537.2 },
  "Honda PCX 160 ABS": { fTooth: 2, fDiam: 535, rTooth: 2, rDiam: 520 },
  "Honda Vario 160 ABS": { fTooth: 2, fDiam: 520, rTooth: 2, rDiam: 500 },
  "Honda CBR150R ABS": { fTooth: 2, fDiam: 530, rTooth: 2, rDiam: 520 },
  "Honda CB150R ABS": { fTooth: 2, fDiam: 530, rTooth: 2, rDiam: 520 },
  "Honda CRF150L (Dual Disc)": { fTooth: 2, fDiam: 540, rTooth: 2, rDiam: 530 }
};

const sportBikes = ["Honda CBR150R ABS", "Honda CB150R ABS", "Honda CRF150L (Dual Disc)"];

function checkSportMode() {
  const sel = document.getElementById("profileSelect").value;
  document.getElementById("sportNotice").style.display = sportBikes.includes(sel) ? "block" : "none";
}

function detectCaptivePortal() {
  const isSmall = window.innerWidth < 500;
  const isStandalone = navigator.standalone || window.matchMedia('(display-mode: standalone)').matches;
  if(isSmall || isStandalone) document.getElementById("captiveWarning").style.display = "block";
}

function openInBrowser() {
  const url = "http://192.168.4.1";
  if(/Android/.test(navigator.userAgent)) {
    window.location.href = `intent://192.168.4.1#Intent;scheme=http;package=com.android.chrome;end`;
    setTimeout(() => { window.open(url, "_system"); }, 500);
  } else {
    window.open(url, "_blank", "width=1024,height=768");
  }
  alert("📱 Buka Chrome/Safari manual jika tidak otomatis.\n\nKetik: 192.168.4.1");
}

document.addEventListener("DOMContentLoaded", () => {
  document.getElementById("riderName").value = riderName || '';
  detectCaptivePortal();
  let saved = localStorage.getItem('smartBrakeSessions');
  if(saved) { sessions = JSON.parse(saved); if(sessions.length > 0) { updateSessionDropdown(); loadSession(sessions[sessions.length-1].id); } }
  updateProfileDropdown();
  resizeCanvas();
});

function saveRiderName() {
  riderName = document.getElementById("riderName").value || 'Anonymous';
  localStorage.setItem('riderName', riderName);
  document.getElementById("riderName").value = riderName;
}

let gradeVisible = false;
function toggleGrade() {
  gradeVisible = !gradeVisible;
  const gc = document.getElementById("gradeContainer");
  const btn = document.getElementById("btnToggleGrade");
  if(gradeVisible) { gc.style.display="block"; btn.innerHTML="👁️"; btn.style.background="#f1f5f9"; btn.title="Hide Grade"; }
  else { gc.style.display="none"; btn.innerHTML="🚫"; btn.style.background="#fee2e2"; btn.title="Show Grade"; }
}

// ==========================================
// FETCH DATA (HTTP POLLING)
// ==========================================
async function fetchData() {
    try {
        const response = await fetch("/data");
        if (!response.ok) throw new Error('Net error');
        const d = await response.json();
        if(startMs === null) { 
          startMs = d.ms; 
          setValue("fTooth", d.settings.fTooth); setValue("fDiam", d.settings.fDiam); 
          setValue("rTooth", d.settings.rTooth); setValue("rDiam", d.settings.rDiam); 
        }
        let sF = Math.round(d.frontSpeedKMH), sR = Math.round(d.rearSpeedKMH);
        setText("txtSpdF", sF); setText("txtSpdR", sR);
        document.getElementById("svgSpdF").style.strokeDashoffset = svgArcLength - ((Math.min(sF, maxSpeed) / maxSpeed) * svgArcLength);
        document.getElementById("svgSpdR").style.strokeDashoffset = svgArcLength - ((Math.min(sR, maxSpeed) / maxSpeed) * svgArcLength);
        let pF = d.frontPressurePSI, pR = d.rearPressurePSI;
        if(isRecording) {
            if(recStartMs === 0 && d.ms > 0) recStartMs = d.ms;
            if(recStartMs > 0) {
                let elapsed = d.ms - recStartMs;
                if (elapsed < 0) { recStartMs = d.ms; elapsed = 0; }
                tempLogData.push({ t: elapsed, sf: sF, sr: sR, pf: pF, pr: pR });
                setText("recTime", formatTime(elapsed));
            }
        }
        let maxP = 100;
        let percF = Math.min(100, Math.round((pF / maxP) * 100)), percR = Math.min(100, Math.round((pR / maxP) * 100));
        setText("txtPercF", percF + "%"); setText("txtPercR", percR + "%");
        document.getElementById("barF").style.height = percF + "%"; document.getElementById("barR").style.height = percR + "%";
        setText("ratF", Math.round(d.frontRatio)); setText("ratR", Math.round(d.rearRatio));
        setValue("rawF", d.rawF); setValue("rawR", d.rawR);
        document.getElementById("dotFP").className = "status-dot " + (d.fpConn ? "on" : "off");
        document.getElementById("dotRP").className = "status-dot " + (d.rpConn ? "on" : "off");
        document.getElementById("dotFS").className = "status-dot " + (d.fsConn ? "on" : "off");
        document.getElementById("dotRS").className = "status-dot " + (d.rsConn ? "on" : "off");
        let sysOk = d.fpConn && d.rpConn;
        document.getElementById("connDot").style.background = sysOk ? "var(--green)" : "var(--red)";
        setText("connText", sysOk ? "Connected" : "Sensor Error");
    } catch(e) {
        document.getElementById("connDot").style.background = "var(--red)";
        setText("connText", "Disconnected");
    } finally { setTimeout(fetchData, 200); }
}
window.addEventListener('load', fetchData);

function updateProfileDropdown() {
  const sel = document.getElementById("profileSelect"); sel.innerHTML = "";
  for(let key in profiles) sel.innerHTML += `<option value="${key}">${key}</option>`;
  checkSportMode();
}
function loadProfile() {
  const key = document.getElementById("profileSelect").value;
  if(profiles[key]) { setValue("fTooth", profiles[key].fTooth); setValue("fDiam", profiles[key].fDiam); setValue("rTooth", profiles[key].rTooth); setValue("rDiam", profiles[key].rDiam); }
  checkSportMode();
}
function saveNewProfile() {
  let name = prompt("Masukkan nama profil baru:");
  if(name) {
    profiles[name] = { fTooth: document.getElementById("fTooth").value || 2, fDiam: document.getElementById("fDiam").value || 588, rTooth: document.getElementById("rTooth").value || 2, rDiam: document.getElementById("rDiam").value || 537.2 };
    localStorage.setItem('smartBrakeProfiles', JSON.stringify(profiles));
    updateProfileDropdown(); document.getElementById("profileSelect").value = name;
  }
}
function deleteProfile() {
  const key = document.getElementById("profileSelect").value;
  if(confirm(`Yakin hapus profil ${key}?`)) { delete profiles[key]; localStorage.setItem('smartBrakeProfiles', JSON.stringify(profiles)); updateProfileDropdown(); }
}

function setText(id, value) { const el = document.getElementById(id); if (el) el.textContent = value; }
function setValue(id, value) { const el = document.getElementById(id); if (el) el.value = value; }
function resizeCanvas() {
  canvas.width = canvas.parentElement.clientWidth - 30;
  canvas.height = canvas.parentElement.clientHeight - 60; 
  if(activeSessionId) { let session = sessions.find(s => s.id == activeSessionId); if(session) generateEvaluation(session.data); }
}
window.addEventListener("resize", resizeCanvas);
function switchTab(tabId, element) {
  document.querySelectorAll('.tab-content').forEach(t => t.classList.remove('active'));
  document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
  document.getElementById(tabId).classList.add('active'); element.classList.add('active');
  if(tabId === 'logTab' && activeSessionId) setTimeout(resizeCanvas, 50); 
}
function formatTime(ms) {
  let totalS = Math.floor(ms / 1000), dec = Math.floor((ms % 1000) / 100); 
  let m = Math.floor(totalS / 60).toString().padStart(2, '0'), s = (totalS % 60).toString().padStart(2, '0');
  return `${m}:${s}.${dec}`;
}

function toggleRecord() {
  const btn = document.getElementById("btnRec"), td = document.getElementById("recTime");
  if(!isRecording) { isRecording = true; tempLogData = []; recStartMs = 0; btn.classList.add("recording"); btn.textContent = "⏹ STOP & ANALISA"; td.style.display = "block"; }
  else { isRecording = false; recStartMs = 0; btn.classList.remove("recording"); btn.textContent = "🔴 START RECORD"; td.style.display = "none"; processAndSaveSession(); }
}

function processAndSaveSession() {
  if(tempLogData.length === 0) return;
  let currentRider = document.getElementById("riderName").value || riderName || '';
  let confirmName = prompt("Nama Pengendara untuk tes ini:", currentRider || 'Anonymous');
  if(confirmName && confirmName.trim() !== '') {
    riderName = confirmName.trim();
    document.getElementById("riderName").value = riderName;
    localStorage.setItem('riderName', riderName);
  }
  let rider = riderName || 'Anonymous';
  let maxSpd = Math.max(...tempLogData.map(d => Math.max(d.sf, d.sr)));
  let maxPF = Math.max(...tempLogData.map(d => d.pf)), maxPR = Math.max(...tempLogData.map(d => d.pr));
  let totalP = maxPF + maxPR, ratF = totalP > 0 ? Math.round((maxPF / totalP) * 100) : 50;
  let grade = (ratF >= 60 && ratF <= 80) ? "✅" : "⚠️";
  let now = new Date();
  let dateStr = now.toLocaleDateString('id-ID', {day:'2-digit', month:'short', year:'numeric'});
  let timeStr = now.toLocaleTimeString('id-ID', {hour:'2-digit', minute:'2-digit'});
  let sessionName = `${rider} | Test #${sessions.length + 1} | ${dateStr} ${timeStr} | ${Math.round(maxSpd)}km/h ${grade}`;
  let newSession = { id: Date.now(), name: sessionName, rider: rider, data: tempLogData };
  sessions.push(newSession); localStorage.setItem('smartBrakeSessions', JSON.stringify(sessions));
  updateSessionDropdown(); loadSession(newSession.id);
  switchTab('logTab', document.querySelectorAll('.tab-btn')[1]); 
}

function updateSessionDropdown() {
  const select = document.getElementById("sessionList"); select.innerHTML = "";
  sessions.forEach(s => { let opt = document.createElement("option"); opt.value = s.id; opt.textContent = s.name; select.appendChild(opt); });
}
function loadSession(id) {
  activeSessionId = id; document.getElementById("sessionList").value = id;
  let session = sessions.find(s => s.id == id); if(session) generateEvaluation(session.data);
}
function deleteSingleSession() {
  if(!activeSessionId) { alert("Pilih sesi dulu!"); return; }
  let session = sessions.find(s => s.id == activeSessionId);
  if(!session) return;
  if(confirm(`Hapus sesi "${session.name}"?`)) {
    sessions = sessions.filter(s => s.id != activeSessionId);
    localStorage.setItem('smartBrakeSessions', JSON.stringify(sessions));
    activeSessionId = null;
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    setText("evGrade", "-"); setText("evDesc", "Lakukan uji pengereman dulu!");
    document.getElementById("evGrade").style.color = "var(--dark)";
    setText("sumSpd", "0"); setText("sumTime", "0.00"); setText("sumDist", "0.0"); setText("sumRatio", "0:0");
    document.getElementById("timelineBox").innerHTML = '<div style="color:#64748b; text-align:center; padding:20px;">Pilih sesi untuk melihat kronologi</div>';
    document.getElementById("distHistoryBox").innerHTML = '<div style="color:#64748b; text-align:center; padding:10px;">Data distribusi selama pengereman</div>';
    updateSessionDropdown();
    if(sessions.length > 0) { loadSession(sessions[sessions.length-1].id); }
  }
}
function clearAllSessions() {
  if(confirm("Yakin ingin menghapus SEMUA sesi?")) {
    sessions = []; localStorage.removeItem('smartBrakeSessions'); updateSessionDropdown(); activeSessionId = null;
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    setText("evGrade", "-"); setText("evDesc", "Lakukan uji pengereman dulu!");
    document.getElementById("evGrade").style.color = "var(--dark)";
    setText("sumSpd", "0"); setText("sumTime", "0.00"); setText("sumDist", "0.0"); setText("sumRatio", "0:0");
    document.getElementById("timelineBox").innerHTML = '<div style="color:#64748b; text-align:center; padding:20px;">Pilih sesi untuk melihat kronologi</div>';
    document.getElementById("distHistoryBox").innerHTML = '<div style="color:#64748b; text-align:center; padding:10px;">Data distribusi selama pengereman</div>';
  }
}

// ==========================================
// ENHANCED EVALUATION
// ==========================================
function generateEvaluation(dataArr) {
  if(!dataArr || dataArr.length === 0) return;
  let brakeStart = -1, peakFrontTime = 0, peakRearTime = 0, maxPF = 0, maxPR = 0;
  let stopTime = dataArr[dataArr.length-1].t;
  for(let i=0; i<dataArr.length; i++) {
    if(brakeStart === -1 && (dataArr[i].pf > 2 || dataArr[i].pr > 2)) brakeStart = dataArr[i].t;
    if(dataArr[i].pf > maxPF) { maxPF = dataArr[i].pf; peakFrontTime = dataArr[i].t; }
    if(dataArr[i].pr > maxPR) { maxPR = dataArr[i].pr; peakRearTime = dataArr[i].t; }
  }
  if(brakeStart === -1) brakeStart = dataArr[0].t;
  let durS = ((stopTime - brakeStart) / 1000.0).toFixed(2); if (durS <= 0) durS = 0.01;
  let maxSpdKmh = Math.max(...dataArr.map(d => Math.max(d.sf, d.sr)));
  let endSpdKmh = Math.max(dataArr[dataArr.length-1].sf, dataArr[dataArr.length-1].sr);
  let brakingDistance = 0;
  for(let i = 1; i < dataArr.length; i++) {
    let dt = (dataArr[i].t - dataArr[i-1].t) / 1000.0; 
    let v1 = Math.max(dataArr[i-1].sf, dataArr[i-1].sr) / 3.6, v2 = Math.max(dataArr[i].sf, dataArr[i].sr) / 3.6;     
    brakingDistance += ((v1 + v2) / 2) * dt;
  }
  let vAwalMS = maxSpdKmh / 3.6, vAkhirMS = endSpdKmh / 3.6;
  let deceleration = (vAwalMS - vAkhirMS) / durS, efficiency = (deceleration / 9.81) * 100;
  setText("sumSpd", Math.round(maxSpdKmh)); setText("sumTime", durS); setText("sumDist", brakingDistance.toFixed(1));
  let totalP = maxPF + maxPR, ratF = totalP > 0 ? Math.round((maxPF / totalP) * 100) : 50, ratR = 100 - ratF;
  setText("sumRatio", `${ratF}:${ratR}`);
  let timelineHTML = "";
  timelineHTML += `<div style="border-left:3px solid var(--blue); padding-left:12px; margin-bottom:8px;"><div style="font-weight:900; color:var(--blue);">▶ TEST START</div><div style="color:#64748b;">${(brakeStart/1000).toFixed(2)}s • ${Math.round(maxSpdKmh)} km/h</div></div>`;
  timelineHTML += `<div style="border-left:3px solid var(--orange); padding-left:12px; margin-bottom:8px;"><div style="font-weight:900; color:var(--orange);">🎯 BRAKE APPLIED</div><div style="color:#64748b;">${(brakeStart/1000).toFixed(2)}s</div></div>`;
  timelineHTML += `<div style="border-left:3px solid var(--blue); padding-left:12px; margin-bottom:8px;"><div style="font-weight:900; color:var(--blue);">⬆ PEAK FRONT</div><div style="color:#64748b;">${(peakFrontTime/1000).toFixed(2)}s • ${maxPF.toFixed(1)}%</div></div>`;
  timelineHTML += `<div style="border-left:3px solid var(--orange); padding-left:12px; margin-bottom:8px;"><div style="font-weight:900; color:var(--orange);">⬆ PEAK REAR</div><div style="color:#64748b;">${(peakRearTime/1000).toFixed(2)}s • ${maxPR.toFixed(1)}%</div></div>`;
  timelineHTML += `<div style="border-left:3px solid var(--red); padding-left:12px;"><div style="font-weight:900; color:var(--red);">⏹ VEHICLE STOP</div><div style="color:#64748b;">${(stopTime/1000).toFixed(2)}s • ${Math.round(endSpdKmh)} km/h</div></div>`;
  document.getElementById("timelineBox").innerHTML = timelineHTML;
  let distHTML = "", lastSampleTime = -500;
  for(let i=0; i<dataArr.length; i++) {
    if(dataArr[i].t - lastSampleTime >= 500) {
      lastSampleTime = dataArr[i].t; let pF = dataArr[i].pf, pR = dataArr[i].pr, total = pF + pR;
      if(total > 0.5) {
        let rF = Math.round((pF/total)*100), rR = 100 - rF;
        distHTML += `<div style="display:flex; justify-content:space-between; padding:4px 0; border-bottom:1px solid #e2e8f0;"><span style="font-weight:800;">${(dataArr[i].t/1000).toFixed(1)}s</span><span style="color:var(--blue);">${rF}%</span><span>:</span><span style="color:var(--orange);">${rR}%</span></div>`;
      }
    }
  }
  document.getElementById("distHistoryBox").innerHTML = distHTML || "Data tidak cukup";
  let grade = document.getElementById("evGrade"), desc = document.getElementById("evDesc");
  if(totalP < 5 || maxSpdKmh < 5) { grade.textContent = "❌"; grade.style.color = "var(--dark)"; desc.textContent = "Data tidak valid."; }
  else if(ratF >= 60 && ratF <= 80) { grade.textContent = "A"; grade.style.color = "var(--green)"; desc.textContent = "Distribusi IDEAL!"; }
  else if(ratF > 80) { grade.textContent = "C"; grade.style.color = "var(--orange)"; desc.textContent = "Dominan DEPAN."; }
  else if(ratF < 40) { grade.textContent = "C"; grade.style.color = "var(--red)"; desc.textContent = "Dominan BELAKANG."; }
  else { grade.textContent = "B"; grade.style.color = "var(--blue)"; desc.textContent = "Cukup baik."; }
  drawEnhancedGraph(dataArr, brakeStart, peakFrontTime, peakRearTime, stopTime);
}

// ==========================================
// ENHANCED GRAPH
// ==========================================
function drawEnhancedGraph(dataArr, brakeStart, peakFront, peakRear, stopTime) {
  if(!dataArr || dataArr.length === 0) return;
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  const w = canvas.width, h = canvas.height, padL = 50, padR = 50, padT = 25, padB = 35;
  const drawW = w - padL - padR, drawH = h - padT - padB, totalTime = dataArr[dataArr.length-1].t;
  const timeToX = (t) => padL + (t / totalTime) * drawW;
  ctx.fillStyle = "#dcfce7"; ctx.fillRect(padL, padT, timeToX(brakeStart) - padL, drawH);
  ctx.fillStyle = "#fef9c3"; ctx.fillRect(timeToX(brakeStart), padT, timeToX(stopTime) - timeToX(brakeStart), drawH);
  if(stopTime < totalTime) { ctx.fillStyle = "#fee2e2"; ctx.fillRect(timeToX(stopTime), padT, padL + drawW - timeToX(stopTime), drawH); }
  ctx.strokeStyle = "#e2e8f0"; ctx.lineWidth = 1; ctx.beginPath();
  for(let i=0; i<=5; i++) { let y = padT + (drawH * i / 5); ctx.moveTo(padL, y); ctx.lineTo(padL + drawW, y); }
  ctx.stroke();
  ctx.fillStyle = "#64748b"; ctx.font = "bold 11px Nunito"; ctx.textAlign = "right";
  ctx.fillText("Speed", padL - 5, padT - 5); ctx.fillText("120", padL - 5, padT + 12); ctx.fillText("0", padL - 5, padT + drawH + 5);
  ctx.textAlign = "left"; ctx.fillText("%", padL + drawW + 5, padT - 5); ctx.fillText("100", padL + drawW + 5, padT + 12); ctx.fillText("0", padL + drawW + 5, padT + drawH + 5);
  const plotLine = (key, color, maxVal, isDashed) => {
    ctx.strokeStyle = color; ctx.lineWidth = 3; if(isDashed) ctx.setLineDash([5, 5]); else ctx.setLineDash([]);
    ctx.beginPath();
    for(let i=0; i<dataArr.length; i++) {
      let x = timeToX(dataArr[i].t), val = Math.min(dataArr[i][key], maxVal), y = padT + drawH - ((val / maxVal) * drawH);
      if(i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
    }
    ctx.stroke();
  };
  plotLine('sf', '#0d6efd', 120, false); plotLine('sr', '#fd7e14', 120, false); 
  plotLine('pf', '#0d6efd', 100, true);  plotLine('pr', '#fd7e14', 100, true);  
  ctx.setLineDash([]); 
  const drawMarker = (t, label, color, yOffset = -25) => {
    let x = timeToX(t); ctx.strokeStyle = color; ctx.lineWidth = 2; ctx.setLineDash([3, 3]);
    ctx.beginPath(); ctx.moveTo(x, padT); ctx.lineTo(x, padT + drawH); ctx.stroke(); ctx.setLineDash([]);
    ctx.fillStyle = color; ctx.font = "bold 10px Nunito"; ctx.textAlign = "center"; ctx.fillText(label, x, padT + yOffset);
    ctx.beginPath(); ctx.moveTo(x, padT); ctx.lineTo(x - 5, padT - 7); ctx.lineTo(x + 5, padT - 7); ctx.closePath(); ctx.fill();
  };
  drawMarker(brakeStart, "BRAKE", "#ef4444", -20);
  drawMarker(peakFront, "PEAK F", "#0d6efd", -35);
  drawMarker(peakRear, "PEAK R", "#fd7e14", -50);
  drawMarker(stopTime, "STOP", "#ef4444", -20);
  if(sportBikes.includes(document.getElementById("profileSelect").value)) {
    ctx.fillStyle = "#64748b"; ctx.font = "bold 10px Nunito"; ctx.textAlign = "right";
    ctx.fillText("ⓘ Clutch: Belum Didukung (Dev)", padL + drawW - 10, padT - 5);
  }
}

// ==========================================
// EXPORT GRAFIK PNG
// ==========================================
function exportImage() {
  if(!activeSessionId) return;
  let session = sessions.find(s => s.id == activeSessionId);
  if(!session) return;
  let now = new Date();
  let dateStr = now.toLocaleDateString('id-ID', {day:'2-digit', month:'2-digit', year:'numeric'}).replace(/\//g, '-');
  let timeStr = now.toLocaleTimeString('id-ID', {hour:'2-digit', minute:'2-digit'}).replace(/:/g, '-');
  let testNum = sessions.indexOf(session) + 1;
  let filename = `Grafik_Test${testNum}_${dateStr}_${timeStr}`;
  canvas.toBlob(function(blob) {
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = `${filename}.png`;
    a.click();
  }, "image/png");
}

// ==========================================
// EXPORT CSV
// ==========================================
function exportCSV() {
  if(!activeSessionId) return;
  let session = sessions.find(s => s.id == activeSessionId);
  if(!session) return;
  let now = new Date();
  let dateStr = now.toLocaleDateString('id-ID', {day:'2-digit', month:'2-digit', year:'numeric'}).replace(/\//g, '-');
  let timeStr = now.toLocaleTimeString('id-ID', {hour:'2-digit', minute:'2-digit'}).replace(/:/g, '-');
  let testNum = sessions.indexOf(session) + 1;
  let filename = `Data_Test${testNum}_${dateStr}_${timeStr}`;
  let maxSpd = Math.max(...session.data.map(d => Math.max(d.sf, d.sr)));
  let maxPF = Math.max(...session.data.map(d => d.pf)), maxPR = Math.max(...session.data.map(d => d.pr));
  let totalP = maxPF + maxPR, ratF = totalP > 0 ? Math.round((maxPF / totalP) * 100) : 50;
  let durS = ((session.data[session.data.length-1].t - session.data[0].t) / 1000.0).toFixed(2);
  let brakingDistance = 0;
  for(let i = 1; i < session.data.length; i++) {
    let dt = (session.data[i].t - session.data[i-1].t) / 1000.0;
    let v1 = Math.max(session.data[i-1].sf, session.data[i-1].sr) / 3.6;
    let v2 = Math.max(session.data[i].sf, session.data[i].sr) / 3.6;
    brakingDistance += ((v1 + v2) / 2) * dt;
  }
  let deceleration = (maxSpd/3.6) / durS;
  let efficiency = (deceleration / 9.81) * 100;
  let gradeText = (ratF >= 60 && ratF <= 80) ? 'A (IDEAL)' : (ratF > 80) ? 'C (Dominan Depan)' : (ratF < 40) ? 'C (Dominan Belakang)' : 'B (Cukup)';
  let csv = "\uFEFF";
  csv += "═══════════════════════════════════════════════════════\n";
  csv += "  SMART BRAKE EDU v7.0 - LAPORAN HASIL UJI PENGEREMAN\n";
  csv += "═══════════════════════════════════════════════════════\n";
  csv += `  Nama Sesi    : ${session.name}\n`;
  csv += `  Tanggal      : ${now.toLocaleString('id-ID')}\n`;
  csv += `  Pengendara   : ${session.rider || 'N/A'}\n`;
  csv += "───────────────────────────────────────────────────────\n\n";
  csv += "No  | Waktu(s) | Speed F | Speed R | Press F | Press R | Distribusi\n";
  csv += "    |          | (km/h)  | (km/h)  | (%)     | (%)     | (F : R)\n";
  csv += "────┼──────────┼─────────┼─────────┼─────────┼─────────┼──────────\n";
  session.data.forEach((r, i) => {
    let tp = r.pf + r.pr;
    let rf = tp > 0 ? Math.round((r.pf / tp) * 100) : 50;
    csv += `${String(i+1).padStart(3)} | ${(r.t/1000).toFixed(2).padStart(8)} | ${String(r.sf).padStart(7)} | ${String(r.sr).padStart(7)} | ${r.pf.toFixed(1).padStart(7)} | ${r.pr.toFixed(1).padStart(7)} | ${rf}:${100-rf}\n`;
  });
  csv += "\n\n═══════════════════════════════════════════════════════\n";
  csv += "  RINGKASAN HASIL UJI\n";
  csv += "═══════════════════════════════════════════════════════\n";
  csv += `  Kecepatan Maksimal      : ${Math.round(maxSpd)} km/h\n`;
  csv += `  Durasi Pengereman       : ${durS} detik\n`;
  csv += `  Jarak Pengereman        : ${brakingDistance.toFixed(2)} meter\n`;
  csv += `  Perlambatan Rata-rata   : ${deceleration.toFixed(2)} m/s²\n`;
  csv += `  Efisiensi Pengereman    : ${efficiency.toFixed(1)} %\n`;
  csv += "───────────────────────────────────────────────────────\n";
  csv += `  Tekanan Depan Maks       : ${maxPF.toFixed(1)}%\n`;
  csv += `  Tekanan Belakang Maks    : ${maxPR.toFixed(1)}%\n`;
  csv += `  Distribusi (F : R)       : ${ratF} : ${100-ratF}\n`;
  csv += "───────────────────────────────────────────────────────\n";
  csv += `  GRADE                    : ${gradeText}\n`;
  csv += "═══════════════════════════════════════════════════════\n";
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([csv], {type: "text/csv;charset=utf-8"}));
  a.download = `${filename}.csv`;
  a.click();
}

// ==========================================
// EXPORT XLSX
// ==========================================
function exportExcel() {
  if(!activeSessionId) return;
  let session = sessions.find(s => s.id == activeSessionId);
  if(!session) return;
  if(typeof XLSX === 'undefined') { alert("⚠️ Library Excel tidak tersedia.\n\nMenggunakan CSV sebagai alternatif..."); exportCSV(); return; }
  let now = new Date();
  let dateStr = now.toLocaleDateString('id-ID', {day:'2-digit', month:'2-digit', year:'numeric'}).replace(/\//g, '-');
  let timeStr = now.toLocaleTimeString('id-ID', {hour:'2-digit', minute:'2-digit'}).replace(/:/g, '-');
  let testNum = sessions.indexOf(session) + 1;
  let filename = `Laporan_Test${testNum}_${dateStr}_${timeStr}`;
  let maxSpd = Math.max(...session.data.map(d => Math.max(d.sf, d.sr)));
  let maxPF = Math.max(...session.data.map(d => d.pf)), maxPR = Math.max(...session.data.map(d => d.pr));
  let totalP = maxPF + maxPR, ratF = totalP > 0 ? Math.round((maxPF / totalP) * 100) : 50;
  let durS = ((session.data[session.data.length-1].t - session.data[0].t) / 1000.0).toFixed(2);
  let brakingDistance = 0;
  for(let i = 1; i < session.data.length; i++) {
    let dt = (session.data[i].t - session.data[i-1].t) / 1000.0;
    let v1 = Math.max(session.data[i-1].sf, session.data[i-1].sr) / 3.6;
    let v2 = Math.max(session.data[i].sf, session.data[i].sr) / 3.6;
    brakingDistance += ((v1 + v2) / 2) * dt;
  }
  let deceleration = (maxSpd/3.6) / durS;
  let efficiency = (deceleration / 9.81) * 100;
  let gradeText = (ratF >= 60 && ratF <= 80) ? 'A (IDEAL)' : (ratF > 80) ? 'C (Dominan Depan)' : (ratF < 40) ? 'C (Dominan Belakang)' : 'B (Cukup)';
  let dataArr = [["No", "Waktu (s)", "Speed Depan (km/h)", "Speed Belakang (km/h)", "Tekanan Depan (%)", "Tekanan Belakang (%)", "Distribusi (F:R)"]];
  session.data.forEach((r, i) => {
    let tp = r.pf + r.pr;
    let rf = tp > 0 ? Math.round((r.pf / tp) * 100) : 50;
    dataArr.push([i+1, (r.t/1000).toFixed(2), r.sf, r.sr, r.pf.toFixed(1), r.pr.toFixed(1), `${rf}:${100-rf}`]);
  });
  let summaryArr = [
    ["SMART BRAKE EDU v7.0 - LAPORAN HASIL UJI PENGEREMAN"], [""],
    ["INFORMASI TES"], ["Nama Sesi", session.name], ["Tanggal Export", now.toLocaleString('id-ID')], ["Pengendara", session.rider || 'N/A'], [""],
    ["HASIL KINEMATIKA"], ["Kecepatan Maksimal", `${Math.round(maxSpd)} km/h`], ["Durasi Pengereman", `${durS} detik`], ["Jarak Pengereman", `${brakingDistance.toFixed(2)} meter`], ["Perlambatan Rata-rata", `${deceleration.toFixed(2)} m/s²`], ["Efisiensi Pengereman", `${efficiency.toFixed(1)} %`], [""],
    ["DATA TEKANAN"], ["Tekanan Depan Maks", `${maxPF.toFixed(1)}%`], ["Tekanan Belakang Maks", `${maxPR.toFixed(1)}%`], ["Distribusi (F:R)", `${ratF} : ${100-ratF}`], [""],
    ["GRADE", gradeText], [""],
    ["KETERANGAN"], ["A = Distribusi ideal (60-80% depan)"], ["B = Cukup baik (50-60% atau 80-85% depan)"], ["C = Berbahaya (<40% atau >85% depan)"], [""],
    ["© SMART BRAKE EDU - Precision Brake Testing for Education"]
  ];
  let wb = XLSX.utils.book_new();
  let wsData = XLSX.utils.aoa_to_sheet(dataArr);
  let wsSummary = XLSX.utils.aoa_to_sheet(summaryArr);
  wsData['!cols'] = [{wch:5},{wch:10},{wch:15},{wch:15},{wch:15},{wch:15},{wch:15}];
  wsSummary['!cols'] = [{wch:35},{wch:30}];
  XLSX.utils.book_append_sheet(wb, wsData, "Data Pengereman");
  XLSX.utils.book_append_sheet(wb, wsSummary, "Ringkasan");
  try { XLSX.writeFile(wb, `${filename}.xlsx`); }
  catch(e) { alert("⚠️ Gagal export Excel.\n\nMenggunakan CSV sebagai alternatif..."); exportCSV(); }
}

// ==========================================
// HTTP POST (Settings & Calibration)
// ==========================================
function saveSettingsToDevice() {
  const params = new URLSearchParams({ fTooth: document.getElementById("fTooth").value, fDiam: document.getElementById("fDiam").value, rTooth: document.getElementById("rTooth").value, rDiam: document.getElementById("rDiam").value });
  fetch("/settings", { method: "POST", headers: {"Content-Type": "application/x-www-form-urlencoded"}, body: params.toString() })
  .then(r => alert("Pengaturan tersimpan!"))
  .catch(e => alert("Gagal menyimpan!"));
}
function doCalib(type, wheel) {
  fetch(`/calib?type=${type}&wheel=${wheel}`, {method: 'POST'})
  .then(r => { alert(`Kalibrasi ${type.toUpperCase()} rem ${wheel} tersimpan!`); })
  .catch(e => alert("Gagal kalibrasi!"));
}
</script>
</body>
</html>
)rawliteral";

// =====================================================
// LED MANAGEMENT FUNCTIONS
// =====================================================
void updateLEDSystem() {
  unsigned long now = millis();
  unsigned long blinkInterval = (WiFi.softAPgetStationNum() > 0) ? 125 : 500;
  if (now - lastSystemBlink >= blinkInterval) {
    lastSystemBlink = now; systemLedState = !systemLedState;
    digitalWrite(LED_SYSTEM, systemLedState);
  }
  if (now - lastPressureLedUpdate >= PRESSURE_LED_INTERVAL) {
    lastPressureLedUpdate = now;
    digitalWrite(LED_FRONT_PRESSURE, frontPressureConnected ? HIGH : LOW);
    digitalWrite(LED_REAR_PRESSURE, rearPressureConnected ? HIGH : LOW);
  }
  if (frontPulseLed && (now - frontPulseLedTimer >= PULSE_LED_DURATION)) { digitalWrite(LED_FRONT_SPEED, LOW); frontPulseLed = false; }
  if (rearPulseLed && (now - rearPulseLedTimer >= PULSE_LED_DURATION)) { digitalWrite(LED_REAR_SPEED, LOW); rearPulseLed = false; }
}

// =====================================================
// INTERRUPT SERVICE ROUTINE
// =====================================================
void IRAM_ATTR frontSpeedISR() {
  uint32_t nowUs = micros();
  if (nowUs - lastFrontPulseUs > PULSE_DEBOUNCE_US) {
    frontPulseCount++; lastFrontPulseUs = nowUs;
    digitalWrite(LED_FRONT_SPEED, HIGH); frontPulseLed = true; frontPulseLedTimer = millis();
  }
}
void IRAM_ATTR rearSpeedISR() {
  uint32_t nowUs = micros();
  if (nowUs - lastRearPulseUs > PULSE_DEBOUNCE_US) {
    rearPulseCount++; lastRearPulseUs = nowUs;
    digitalWrite(LED_REAR_SPEED, HIGH); rearPulseLed = true; rearPulseLedTimer = millis();
  }
}

// =====================================================
// HELPER FUNCTIONS
// =====================================================
float clampFloat(float v, float minV, float maxV) { if(v<minV) return minV; if(v>maxV) return maxV; return v; }
int clampInt(int v, int minV, int maxV) { if(v<minV) return minV; if(v>maxV) return maxV; return v; }
float adcToPressurePSI(int adc, BrakeSetting s) {
  if(s.maxADC <= s.minADC) return 0.0;
  if(adc <= s.minADC + 30) return 0.0;
  return clampFloat(((float)(adc - s.minADC) * s.maxPressurePSI) / (float)(s.maxADC - s.minADC), 0.0, s.maxPressurePSI);
}
bool isPressureSensorConnected(int adc) { return (adc >= 200 && adc <= 4095); }
bool isSpeedSensorConnected(bool isFront) {
  unsigned long t = isFront ? lastFrontPulseTime : lastRearPulseTime;
  if(t == 0) return false;
  return (millis() - t <= SPEED_SENSOR_TIMEOUT);
}
void updateSpeedSensorActivity() {
  static uint32_t lf=0, lr=0;
  noInterrupts(); uint32_t cf=frontPulseCount, cr=rearPulseCount; interrupts();
  if(cf!=lf){ lastFrontPulseTime=millis(); lf=cf; }
  if(cr!=lr){ lastRearPulseTime=millis(); lr=cr; }
}
float pulseToSpeedKMH(uint32_t delta, BrakeSetting s, uint32_t dtMs) {
  if(s.numTooth==0 || dtMs==0) return 0.0;
  float rev = (float)delta / (float)s.numTooth;
  float circ = 3.1415926 * (s.diameterMM / 1000.0);
  return clampFloat((rev * circ) / (dtMs / 1000.0) * 3.6, 0.0, s.maxSpeedKMH);
}
void updateMeasurement() {
  uint32_t now = millis();
  updateSpeedSensorActivity();
  if(now - lastSampleMs >= UI_INTERVAL_MS) {
    lastSampleMs = now;
    int rf = analogRead(PIN_ADC_FRONT_PRESSURE), rr = analogRead(PIN_ADC_REAR_PRESSURE);
    emaFrontADC = (emaFrontADC * (1.0 - ADC_ALPHA)) + (rf * ADC_ALPHA);
    emaRearADC  = (emaRearADC * (1.0 - ADC_ALPHA))  + (rr * ADC_ALPHA);
    rawFrontADC = (int)emaFrontADC; rawRearADC = (int)emaRearADC;
    frontPressureConnected = isPressureSensorConnected(rawFrontADC);
    rearPressureConnected = isPressureSensorConnected(rawRearADC);
    frontPressurePSI = frontPressureConnected ? adcToPressurePSI(rawFrontADC, frontSet) : 0.0;
    rearPressurePSI = rearPressureConnected ? adcToPressurePSI(rawRearADC, rearSet) : 0.0;
    float tp = frontPressurePSI + rearPressurePSI;
    if(tp > 0.01) { frontRatio = (frontPressurePSI/tp)*100.0; rearRatio = (rearPressurePSI/tp)*100.0; }
    else { frontRatio=0.0; rearRatio=0.0; }
  }
  if(now - lastSpeedSampleMs >= SPEED_INTERVAL_MS) {
    uint32_t dt = now - lastSpeedSampleMs;
    noInterrupts(); uint32_t cf=frontPulseCount, cr=rearPulseCount; interrupts();
    frontSpeedKMH = isSpeedSensorConnected(true) ? pulseToSpeedKMH(cf - lastFrontPulseCount, frontSet, dt) : 0.0;
    rearSpeedKMH = isSpeedSensorConnected(false) ? pulseToSpeedKMH(cr - lastRearPulseCount, rearSet, dt) : 0.0;
    lastFrontPulseCount=cf; lastRearPulseCount=cr; lastSpeedSampleMs=now;
  }
}

// =====================================================
// SAVE TO ROM
// =====================================================
void savePrefs() {
  prefs.begin("brake", false);
  prefs.putBytes("front", &frontSet, sizeof(BrakeSetting));
  prefs.putBytes("rear", &rearSet, sizeof(BrakeSetting));
  prefs.end();
}

// =====================================================
// WEB SERVER HANDLERS
// =====================================================
void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }
void handleData() {
  char json[800];
  snprintf(json, sizeof(json), 
    "{\"ms\":%lu,\"rawF\":%d,\"rawR\":%d,\"fpConn\":%s,\"rpConn\":%s,\"fsConn\":%s,\"rsConn\":%s,"
    "\"frontPressurePSI\":%.2f,\"frontSpeedKMH\":%.1f,\"frontRatio\":%.1f,"
    "\"rearPressurePSI\":%.2f,\"rearSpeedKMH\":%.1f,\"rearRatio\":%.1f,"
    "\"settings\":{\"fTooth\":%u,\"fDiam\":%.1f,\"rTooth\":%u,\"rDiam\":%.1f}}",
    millis(), rawFrontADC, rawRearADC,
    (frontPressureConnected?"true":"false"), (rearPressureConnected?"true":"false"),
    (isSpeedSensorConnected(true)?"true":"false"), (isSpeedSensorConnected(false)?"true":"false"),
    frontPressurePSI, frontSpeedKMH, frontRatio, rearPressurePSI, rearSpeedKMH, rearRatio,
    frontSet.numTooth, frontSet.diameterMM, rearSet.numTooth, rearSet.diameterMM);
  server.send(200, "application/json", json);
}
void handleCalib() {
  String w = server.arg("wheel"), t = server.arg("type");
  if(w=="F"){ if(t=="zero") frontSet.minADC=rawFrontADC; if(t=="max") frontSet.maxADC=rawFrontADC; }
  if(w=="R"){ if(t=="zero") rearSet.minADC=rawRearADC; if(t=="max") rearSet.maxADC=rawRearADC; }
  savePrefs(); server.send(200, "text/plain", "OK");
}
void handleSettings() {
  if(server.hasArg("fTooth")) frontSet.numTooth=clampInt(server.arg("fTooth").toInt(),1,200);
  if(server.hasArg("fDiam")) frontSet.diameterMM=clampFloat(server.arg("fDiam").toFloat(),1.0,3000.0);
  if(server.hasArg("rTooth")) rearSet.numTooth=clampInt(server.arg("rTooth").toInt(),1,200);
  if(server.hasArg("rDiam")) rearSet.diameterMM=clampFloat(server.arg("rDiam").toFloat(),1.0,3000.0);
  savePrefs(); server.send(200, "text/plain", "OK");
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200); delay(1000);
  Serial.println("\nSMART BRAKE EDU v7.0 - FINAL\n");
  
  prefs.begin("brake", true);
  if(prefs.getBytesLength("front")==sizeof(BrakeSetting)) prefs.getBytes("front",&frontSet,sizeof(BrakeSetting));
  if(prefs.getBytesLength("rear")==sizeof(BrakeSetting)) prefs.getBytes("rear",&rearSet,sizeof(BrakeSetting));
  prefs.end();
  
  pinMode(LED_SYSTEM, OUTPUT); pinMode(LED_FRONT_PRESSURE, OUTPUT); pinMode(LED_REAR_PRESSURE, OUTPUT);
  pinMode(LED_FRONT_SPEED, OUTPUT); pinMode(LED_REAR_SPEED, OUTPUT);
  digitalWrite(LED_SYSTEM, HIGH); delay(300); digitalWrite(LED_SYSTEM, LOW);
  pinMode(PIN_FRONT_SPEED_SENSOR, INPUT_PULLUP); pinMode(PIN_REAR_SPEED_SENSOR, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_ADC_FRONT_PRESSURE, ADC_11db);
  analogSetPinAttenuation(PIN_ADC_REAR_PRESSURE, ADC_11db);
  attachInterrupt(digitalPinToInterrupt(PIN_FRONT_SPEED_SENSOR), frontSpeedISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_REAR_SPEED_SENSOR), rearSpeedISR, FALLING);
  
  WiFi.mode(WIFI_AP); WiFi.softAP(AP_SSID, AP_PASS);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  ArduinoOTA.setHostname(OTA_HOSTNAME); ArduinoOTA.begin();
  
  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.on("/calib", HTTP_POST, handleCalib);
  server.on("/settings", HTTP_POST, handleSettings);
  server.onNotFound([]() { server.sendHeader("Location", "http://" + WiFi.softAPIP().toString(), true); server.send(302, "text/plain", ""); });
  server.begin();
  
  emaFrontADC=analogRead(PIN_ADC_FRONT_PRESSURE); emaRearADC=analogRead(PIN_ADC_REAR_PRESSURE);
  lastSampleMs=millis(); lastSpeedSampleMs=millis();
  
  Serial.printf("\nWiFi: %s | Pass: %s | IP: %s\n", AP_SSID, AP_PASS, WiFi.softAPIP().toString().c_str());
}

// =====================================================
// MAIN LOOP
// =====================================================
void loop() {
  dnsServer.processNextRequest();
  ArduinoOTA.handle();
  server.handleClient();
  updateMeasurement();
  updateLEDSystem();
}