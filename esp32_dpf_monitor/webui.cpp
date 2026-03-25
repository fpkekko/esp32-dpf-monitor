#include "webui.h"
#include "config.h"

#include <FS.h>
using namespace fs;

static void handleData();
static void handleSave();
static void handleReset();
static void handleRoot();

void initWebServer() {
  server.on("/",        HTTP_GET,  handleRoot);
  server.on("/save",    HTTP_POST, handleSave);
  server.on("/data",    HTTP_GET,  handleData);
  server.on("/reset",   HTTP_POST, handleReset);
  server.onNotFound([](){server.send(404,"text/plain","Not found");});
}

static void handleData() {
  // Live JSON data for optional web dashboard
  char buf[512];
  const CarProfile& car=PROFILES[cfg.carIndex];
  snprintf(buf,sizeof(buf),
    "{\"car\":\"%s\",\"dpfPct\":%d,\"dpfTemp\":%d,\"dpfDiff\":%d,"
    "\"dpfRegen\":%s,\"dpfKm\":%d,\"oilTemp\":%d,\"coolant\":%d,"
    "\"turbo\":%.2f,\"rpm\":%d,\"battV\":%.2f,\"altOk\":%s,\"engine\":%s}",
    car.name, vd.dpfPct, vd.dpfTempC, vd.dpfDiffPress,
    vd.dpfRegen?"true":"false", vd.dpfKmRegen,
    vd.oilTempC, vd.coolantTempC,
    vd.turboBar, vd.rpm, vd.battV,
    vd.altOk?"true":"false", vd.engineOn?"true":"false");
  server.send(200,"application/json",buf);
}

static void handleSave() {
  if(server.hasArg("carIdx"))   cfg.carIndex    = server.arg("carIdx").toInt();
  if(server.hasArg("dpfWarn"))  cfg.dpfWarn     = server.arg("dpfWarn").toInt();
  if(server.hasArg("dpfCrit"))  cfg.dpfCrit     = server.arg("dpfCrit").toInt();
  if(server.hasArg("diffWarn")) cfg.diffWarn    = server.arg("diffWarn").toInt();
  if(server.hasArg("diffCrit")) cfg.diffCrit    = server.arg("diffCrit").toInt();
  if(server.hasArg("oilWarn"))  cfg.oilWarn     = server.arg("oilWarn").toInt();
  if(server.hasArg("oilCrit"))  cfg.oilCrit     = server.arg("oilCrit").toInt();
  if(server.hasArg("cwWarn"))   cfg.coolantWarn = server.arg("cwWarn").toInt();
  if(server.hasArg("battLow"))  cfg.battLow     = server.arg("battLow").toFloat();
  if(server.hasArg("altMin"))   cfg.altMin      = server.arg("altMin").toFloat();
  if(server.hasArg("pollMs"))   cfg.pollMs      = server.arg("pollMs").toInt();
  if(server.hasArg("startPg"))  cfg.startPage   = server.arg("startPg").toInt();
  if(server.hasArg("bright"))   cfg.brightness  = server.arg("bright").toInt();
  if(server.hasArg("obdMac"))   server.arg("obdMac").toCharArray(cfg.obdMac,18);
  cfg.buzzer = server.hasArg("buzzer");
  saveConfig();
  webConfigChanged=true;
  server.sendHeader("Location","/"); server.send(303);
}

static void handleReset() {
  prefs.begin("dpfmon",false); prefs.clear(); prefs.end();
  cfg=Config(); saveConfig();
  webConfigChanged=true;
  server.sendHeader("Location","/"); server.send(303);
}

static void handleRoot() {
  const CarProfile& car=PROFILES[cfg.carIndex];
  const CarProfile& other=PROFILES[1-cfg.carIndex];

  String html = F("<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>DPF Monitor</title><style>"
    "body{font-family:-apple-system,sans-serif;background:#000;color:#e5e7eb;margin:0;padding:0}"
    "h1{font-size:18px;font-weight:500;margin:0}"
    ".bar{background:#111;padding:14px 16px;border-bottom:1px solid #1f1f1f;display:flex;justify-content:space-between;align-items:center}"
    ".live{font-size:11px;color:#22c55e}"
    "nav{display:flex;background:#111;border-bottom:1px solid #1f1f1f}"
    "nav a{flex:1;padding:10px 4px;font-size:12px;color:#6b7280;text-align:center;text-decoration:none;cursor:pointer}"
    "nav a.on{color:#3b82f6;border-bottom:2px solid #3b82f6}"
    ".sec{padding:14px 14px 4px;border-bottom:1px solid #111}"
    ".sec-t{font-size:10px;color:#6b7280;letter-spacing:.05em;text-transform:uppercase;margin-bottom:10px}"
    ".row{display:flex;justify-content:space-between;align-items:center;padding:8px 0;border-bottom:1px solid #111}"
    ".row:last-child{border:none}"
    ".lbl{font-size:13px}.sub{font-size:10px;color:#6b7280}"
    "input[type=number],input[type=text]{background:#1a1a1a;border:1px solid #333;color:#e5e7eb;"
    "border-radius:6px;padding:5px 8px;font-size:13px;width:90px;text-align:right}"
    "input[type=range]{width:100px}"
    ".car-btn{background:#111;border:1px solid #333;border-radius:10px;padding:10px 14px;"
    "color:#e5e7eb;font-size:13px;width:100%;text-align:left;cursor:pointer;margin-bottom:8px;display:block}"
    ".car-btn.sel{border-color:#3b82f6;background:#0a1628;color:#93c5fd}"
    ".save{background:#3b82f6;color:#fff;border:none;border-radius:10px;padding:13px;"
    "font-size:14px;font-weight:500;width:calc(100% - 28px);margin:14px;cursor:pointer}"
    ".badge{font-size:9px;padding:2px 6px;border-radius:10px;margin-left:6px}"
    ".b-psa{background:#1e3a5f;color:#60a5fa}"
    ".b-gm{background:#1a2e1a;color:#4ade80}"
    ".stat{font-size:11px;color:#3b82f6;font-weight:500}"
    "table{width:100%;border-collapse:collapse;font-size:11px}"
    "th{color:#6b7280;font-weight:400;padding:4px;text-align:left;border-bottom:1px solid #1f1f1f}"
    "td{padding:5px 4px;border-bottom:1px solid #111;color:#d1d5db;font-family:monospace}"
    ".ok{color:#22c55e}.unk{color:#f59e0b}"
    ".info{background:#0a1628;border:1px solid #1e3a5f;border-radius:8px;padding:10px 12px;"
    "font-size:12px;color:#93c5fd;margin:10px 14px;line-height:1.7}"
    "</style></head><body>");

  // Header
  html += "<div class='bar'><h1>DPF Monitor</h1>";
  html += "<span class='live'>WiFi: 192.168.4.1</span></div>";
  html += "<nav>"
          "<a class='on' href='#vehicle'>Vehicle</a>"
          "<a href='#thresholds'>Thresholds</a>"
          "<a href='#pid'>PIDs</a>"
          "<a href='#advanced'>Advanced</a>"
          "</nav>";

  html += "<form method='POST' action='/save'>";

  // ── Section: vehicle selection
  html += "<div class='sec' id='vehicle'><div class='sec-t'>Select vehicle</div>";
  for(int i=0;i<NUM_PROFILES;i++){
    const CarProfile& p=PROFILES[i];
    bool sel=(i==cfg.carIndex);
    html += "<button type='button' class='car-btn";
    if(sel) html+=" sel";
    html+="' onclick='document.getElementById(\"carIdx\").value=";
    html+=String(i);
    html+=";document.querySelectorAll(\".car-btn\").forEach(b=>b.classList.remove(\"sel\"));this.classList.add(\"sel\")'>";
    html+=p.name;
    html+="<span class='badge ";
    html+=(i==0?"b-psa":"b-gm");
    html+="'>";html+=p.ecu;html+="</span><br>";
    html+="<span style='font-size:11px;color:#6b7280'>";
    html+=p.model;html+=" — ";html+=p.year;
    html+=" — AdBlue: ";html+=(p.hasAdblue?"Yes":"No");
    html+="</span></button>";
  }
  html+="<input type='hidden' id='carIdx' name='carIdx' value='";
  html+=String(cfg.carIndex);html+="'>";

  // Live data box
  html+="<div class='info' id='liveBox'>Live data: <span id='liveData'>loading...</span></div>";

  html+="</div>";

  // ── Section: thresholds
  html+="<div class='sec' id='thresholds'><div class='sec-t'>DPF / FAP</div>";
  auto row=[&](const char* lbl, const char* sub, const char* name, int val){
    html+="<div class='row'><div><div class='lbl'>"; html+=lbl;
    html+="</div><div class='sub'>"; html+=sub;
    html+="</div></div><input type='number' name='"; html+=name;
    html+="' value='"; html+=String(val); html+="'></div>";
  };
  auto rowf=[&](const char* lbl, const char* name, float val){
    html+="<div class='row'><div class='lbl'>"; html+=lbl;
    html+="</div><input type='number' step='0.1' name='"; html+=name;
    html+="' value='"; html+=String(val,1); html+="'></div>";
  };
  row("Soot warning","yellow blink","dpfWarn",cfg.dpfWarn);
  row("Critical alarm","red blink","dpfCrit",cfg.dpfCrit);
  row("Diff. pressure warning","mbar","diffWarn",cfg.diffWarn);
  row("Diff. pressure critical","mbar","diffCrit",cfg.diffCrit);
  html+="</div><div class='sec'><div class='sec-t'>Engine</div>";
  row("Oil temp warning","°C","oilWarn",cfg.oilWarn);
  row("Oil temp critical","°C","oilCrit",cfg.oilCrit);
  row("Coolant temp warning","°C","cwWarn",cfg.coolantWarn);
  html+="</div><div class='sec'><div class='sec-t'>Battery</div>";
  rowf("Low voltage (V)","battLow",cfg.battLow);
  rowf("Alternator min (V)","altMin",cfg.altMin);
  html+="</div>";

  // ── Section: PID info
  html+="<div class='sec' id='pid'><div class='sec-t'>";
  html+=PROFILES[cfg.carIndex].name;
  html+=" — configured PIDs</div>";
  html+="<table><tr><th>Parameter</th><th>PID</th><th>Formula</th><th></th></tr>";
  auto pidRow=[&](const char* par, const char* pid, const char* formula, bool ok){
    html+="<tr><td>"; html+=par;
    html+="</td><td style='color:#a78bfa'>"; html+=pid;
    html+="</td><td>"; html+=formula;
    html+="</td><td class='"; html+=(ok?"ok":"unk"); html+="'>";
    html+=(ok?"✓ OK":"? test"); html+="</td></tr>";
  };
  if(cfg.carIndex==0){
    pidRow("Soot %",   "2102FF","A/255×100",true);
    pidRow("Regen",    "2102FE","bit 0",true);
    pidRow("DPF Temp", "210261","(A*256+B)-400",false);
    pidRow("Diff press","210262","(A*256+B)/10",false);
    pidRow("Km regen", "21026D","A*256+B",true);
  } else {
    pidRow("Soot %",   "22336A","A (0-100%)",true);
    pidRow("Regen",    "2220FA","bit 0",false);
    pidRow("DPF Temp", "222010","(A*256+B)/10-40",false);
    pidRow("Km regen", "22336B","A*256+B",true);
  }
  html+="</table>";
  html+="<div class='info'>PIDs marked <span class='unk'>? test</span> need live verification. "
        "Formulas may vary by year/variant.<br>"
        "Use <a href='/data' style='color:#60a5fa'>192.168.4.1/data</a> for live JSON.</div></div>";

  // ── Section: advanced
  html+="<div class='sec' id='advanced'><div class='sec-t'>OBD Connection</div>";
  html+="<div class='row'><div><div class='lbl'>Adapter MAC</div>"
        "<div class='sub'>empty = auto scan</div></div>"
        "<input type='text' name='obdMac' value='"; html+=cfg.obdMac; html+="' placeholder='auto'></div>";
  row("Poll interval","ms","pollMs",cfg.pollMs);
  html+="</div><div class='sec'><div class='sec-t'>Display</div>";
  row("Boot page","0–3","startPg",cfg.startPage);
  html+="<div class='row'><div class='lbl'>Brightness</div>"
        "<input type='range' name='bright' min='10' max='100' value='";
  html+=String(cfg.brightness); html+="'></div>";
  html+="<div class='row'><div class='lbl'>Alarm buzzer</div>"
        "<input type='checkbox' name='buzzer'"; if(cfg.buzzer) html+=" checked"; html+="></div>";
  html+="</div><div class='sec'><div class='sec-t'>Wi-Fi AP</div>";
  html+="<div class='row'><div class='lbl'>SSID</div><span class='stat'>" AP_SSID "</span></div>";
  html+="<div class='row'><div class='lbl'>IP</div><span class='stat'>192.168.4.1</span></div>";
  html+="<div class='row'><div class='lbl'>Version</div><span class='stat'>v1.0</span></div>";
  html+="<div class='row'><div class='lbl' style='color:#ef4444'>Reset config</div>"
        "<button type='button' style='background:#7f1d1d;color:#fca5a5;border:none;"
        "border-radius:6px;padding:5px 12px;cursor:pointer' "
        "onclick='if(confirm(\"Reset?\"))fetch(\"/reset\",{method:\"POST\"}).then(()=>location.reload())'>Reset</button></div>";
  html+="</div>";

  html+="<button type='submit' class='save'>Save configuration</button></form>";

  // JS live data fetch
  html+="<script>"
        "function fetchLive(){"
        "fetch('/data').then(r=>r.json()).then(d=>{"
        "document.getElementById('liveData').innerHTML="
        "'DPF: '+d.dpfPct+'% | Temp: '+d.dpfTemp+'°C | Batt: '+d.battV+'V | '"
        "+(d.dpfRegen?'<span style=color:#f97316>REGEN ACTIVE</span>':'OK');"
        "}).catch(()=>{})}"
        "setInterval(fetchLive,3000);fetchLive();"
        // Tab switching via hash
        "document.querySelectorAll('nav a').forEach(a=>a.addEventListener('click',function(){"
        "document.querySelectorAll('nav a').forEach(x=>x.classList.remove('on'));"
        "this.classList.add('on');"
        "}));"
        "</script></body></html>";

  server.send(200,"text/html",html);
}
