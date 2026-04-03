#include "obd.h"
#include "config.h"

#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// ─── BLE globals ─────────────────────────────────────────────────────────────
static BLEClient*               bleClient    = nullptr;
static BLERemoteCharacteristic* bleWriteChar = nullptr;
static BLEAdvertisedDevice*     bleFoundDev  = nullptr;
static String                   bleRxBuf     = "";
static volatile bool            bleRxReady   = false;

static void bleNotifyCallback(BLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
  for (size_t i = 0; i < len; i++) {
    char c = (char)data[i];
    if (c == '>') { bleRxReady = true; return; }
    if (c == '\r') {
      if (bleRxBuf.length() > 0) { bleRxReady = true; return; }  // IOS-VLINK uses \r as terminator
    } else if (c != '\n') {
      bleRxBuf += c;
    }
  }
}

class OBDScanCallback : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice dev) override {
    String name = dev.getName().c_str(); name.toUpperCase();
    if (name.indexOf("OBD")>=0 || name.indexOf("ELM")>=0 || name.indexOf("VGATE")>=0 || name.indexOf("VLINK")>=0) {
      bleFoundDev = new BLEAdvertisedDevice(dev);
      BLEDevice::getScan()->stop();
    }
  }
};

static bool bleAttach(BLEAdvertisedDevice* dev) {
  bleClient = BLEDevice::createClient();
  if (!bleClient->connect(dev)) { delete bleClient; bleClient=nullptr; return false; }
  BLERemoteService* svc = bleClient->getService(BLE_SERVICE_UUID);
  if (!svc) { bleClient->disconnect(); delete bleClient; bleClient=nullptr; return false; }
  bleWriteChar = svc->getCharacteristic(BLE_WRITE_UUID);
  BLERemoteCharacteristic* notifyChar = svc->getCharacteristic(BLE_NOTIFY_UUID);
  if (!bleWriteChar || !notifyChar) {
    bleClient->disconnect(); delete bleClient; bleClient=nullptr; return false;
  }
  notifyChar->registerForNotify(bleNotifyCallback);
  vd.btConnected = true;
  return true;
}

bool connectOBD() {
  // Direct connection via MAC if configured in web UI
  if (strlen(cfg.obdMac)==17) {
    BLEAdvertisedDevice direct;
    // use BLEAddress for direct connection
    bleClient = BLEDevice::createClient();
    if (bleClient->connect(BLEAddress(cfg.obdMac))) {
      BLERemoteService* svc = bleClient->getService(BLE_SERVICE_UUID);
      if (svc) {
        bleWriteChar = svc->getCharacteristic(BLE_WRITE_UUID);
        BLERemoteCharacteristic* nc = svc->getCharacteristic(BLE_NOTIFY_UUID);
        if (bleWriteChar && nc) { nc->registerForNotify(bleNotifyCallback); vd.btConnected=true; return true; }
      }
      bleClient->disconnect();
    }
    delete bleClient; bleClient=nullptr;
    return false;
  }
  // Automatic scan by name
  bleFoundDev = nullptr;
  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new OBDScanCallback());
  scan->setActiveScan(true);
  scan->start(8, false);
  if (bleFoundDev) return bleAttach(bleFoundDev);
  return false;
}

void initELM() {
  sendOBD("ATZ",1500); delay(500);
  sendOBD("ATE0"); sendOBD("ATL0"); sendOBD("ATS0"); sendOBD("ATH0");
  const CarProfile& car=PROFILES[cfg.carIndex];
  sendOBD(car.protocol);
  String sh="ATSH "; sh+=car.header;
  sendOBD(sh);
  sendOBD("0100",2000);
}

String sendOBD(const String& cmd, int ms) {
  if (!bleWriteChar) return "";
  bleRxBuf   = "";
  bleRxReady = false;
  String tx = cmd + "\r";
  bleWriteChar->writeValue((uint8_t*)tx.c_str(), tx.length(), true);
  uint32_t t0 = millis();
  while (!bleRxReady && millis()-t0 < (uint32_t)ms) delay(5);
  bleRxBuf.replace(" ", "");   // remove spaces — some BLE adapters add them
  bleRxBuf.trim();
  // strip command echo if ATE0 was not honored by the adapter
  String cmdUpper = cmd; cmdUpper.toUpperCase();
  String bufUpper = bleRxBuf; bufUpper.toUpperCase();
  if (bufUpper.startsWith(cmdUpper)) bleRxBuf = bleRxBuf.substring(cmd.length());
  bleRxBuf.trim();
  return bleRxBuf;
}

String sendOBD(const char* cmd, int ms){ return sendOBD(String(cmd), ms); }

bool readBattery() {
  String r=sendOBD("ATRV"); r.replace("V",""); r.trim();
  float v=r.toFloat();
  if(v>8&&v<18){
    bool wasOn = vd.engineOn;
    vd.battV   = v;
    vd.engineOn= (v>13.0f);
    if(vd.engineOn && !wasOn) vd.engineStartMs = millis(); // record engine start
    bool altWarmup = vd.engineOn && (millis()-vd.engineStartMs < 60000UL);
    vd.altOk   = !vd.engineOn || altWarmup || (v>=cfg.altMin);
    return true;
  }
  return false;
}

bool readDPF() {
  const CarProfile& car=PROFILES[cfg.carIndex];
  int idx=cfg.carIndex;

  // Check if string is valid hex (not "NO DATA", "ERROR", "?", etc.)
  auto isHexStr = [](const String& s) -> bool {
    if(s.length()==0) return false;
    for(char c : s) if(!isxdigit((unsigned char)c)) return false;
    return true;
  };
  // Parse 1 byte: try mode 22 prefix ("62"), mode 21 prefix ("61"), then raw offset 0
  auto parse1 = [&isHexStr](const String& s) -> int {
    int p = s.indexOf("62");
    if(p>=0 && s.length()>=(unsigned)(p+8))
      return strtol(s.substring(p+6,p+8).c_str(),nullptr,16);
    p = s.indexOf("61");
    if(p>=0 && s.length()>=(unsigned)(p+8))
      return strtol(s.substring(p+6,p+8).c_str(),nullptr,16);
    if(isHexStr(s) && s.length()>=2)
      return strtol(s.substring(0,2).c_str(),nullptr,16);
    return -1;
  };
  // Parse 2 bytes: same fallback logic
  auto parse2 = [&isHexStr](const String& s) -> int {
    int p = s.indexOf("62");
    if(p>=0 && s.length()>=(unsigned)(p+10)){
      int hi=strtol(s.substring(p+6,p+8).c_str(),nullptr,16);
      int lo=strtol(s.substring(p+8,p+10).c_str(),nullptr,16);
      return hi*256+lo;
    }
    p = s.indexOf("61");
    if(p>=0 && s.length()>=(unsigned)(p+10)){
      int hi=strtol(s.substring(p+6,p+8).c_str(),nullptr,16);
      int lo=strtol(s.substring(p+8,p+10).c_str(),nullptr,16);
      return hi*256+lo;
    }
    if(isHexStr(s) && s.length()>=4){
      int hi=strtol(s.substring(0,2).c_str(),nullptr,16);
      int lo=strtol(s.substring(2,4).c_str(),nullptr,16);
      return hi*256+lo;
    }
    return -1;
  };

  // Parse 3 bytes: same fallback logic as parse2 but with extra byte C
  auto parse3 = [&isHexStr](const String& s) -> int {
    int p = s.indexOf("62");
    if(p>=0 && s.length()>=(unsigned)(p+12)){
      int a=strtol(s.substring(p+6,p+8).c_str(),nullptr,16);
      int b=strtol(s.substring(p+8,p+10).c_str(),nullptr,16);
      int c=strtol(s.substring(p+10,p+12).c_str(),nullptr,16);
      return a*65536+b*256+c;
    }
    p = s.indexOf("61");
    if(p>=0 && s.length()>=(unsigned)(p+12)){
      int a=strtol(s.substring(p+6,p+8).c_str(),nullptr,16);
      int b=strtol(s.substring(p+8,p+10).c_str(),nullptr,16);
      int c=strtol(s.substring(p+10,p+12).c_str(),nullptr,16);
      return a*65536+b*256+c;
    }
    if(isHexStr(s) && s.length()>=6){
      int a=strtol(s.substring(0,2).c_str(),nullptr,16);
      int b=strtol(s.substring(2,4).c_str(),nullptr,16);
      int c=strtol(s.substring(4,6).c_str(),nullptr,16);
      return a*65536+b*256+c;
    }
    return -1;
  };

  // Soot % — both profiles: raw byte 0-255 → 0-100%
  String r=sendOBD(car.pidSoot);
  { int v=parse1(r); if(v>=0) vd.dpfPct=(int)(v/255.0f*100.0f); }

  // Regen active
  // Citroën: bit 0 of status byte | Opel: 223274 → 0=off, >0=regen progress
  r=sendOBD(car.pidRegen);
  { int v=parse1(r); if(v>=0) vd.dpfRegen=(idx==0)?(v&0x01):(v>0); }

  // DPF temperature
  // Citroën: (A*256+B)-400 (2-byte) | Opel: A*5-40 (1-byte, PID 223279)
  r=sendOBD(car.pidDpfTemp);
  if(idx==0){ int v=parse2(r); if(v>=0) vd.dpfTempC=v-400; }
  else       { int v=parse1(r); if(v>=0) vd.dpfTempC=v*5-40; }

  // Differential pressure
  // Citroën: (A*256+B)/10 mbar | Opel: SIGNED(A) mbar (PID 2220F4)
  r=sendOBD(car.pidDiffPress);
  if(idx==0){ int v=parse2(r); if(v>=0) vd.dpfDiffPress=v/10; }
  else       { int v=parse1(r); if(v>=0) vd.dpfDiffPress=abs(v>127?v-256:v); }

  // Km since last regen
  // Citroën: A*256+B (2-byte) | Opel: A*65536+B*256+C (3-byte, PID 223277)
  r=sendOBD(car.pidKmRegen);
  if(idx==0){ int v=parse2(r); if(v>=0) vd.dpfKmRegen=v; }
  else       { int v=parse3(r); if(v>=0) vd.dpfKmRegen=v; }
  return true;
}

bool readEngineData() {
  String r=sendOBD("015C");
  if(r.startsWith("41")&&r.length()>=6) vd.oilTempC=strtol(r.substring(4,6).c_str(),nullptr,16)-40;
  r=sendOBD("0105");
  if(r.startsWith("41")&&r.length()>=6) vd.coolantTempC=strtol(r.substring(4,6).c_str(),nullptr,16)-40;
  r=sendOBD("010C");
  if(r.startsWith("41")&&r.length()>=8){
    int A=strtol(r.substring(4,6).c_str(),nullptr,16);
    int B=strtol(r.substring(6,8).c_str(),nullptr,16);
    vd.rpm=(A*256+B)/4;
  }
  r=sendOBD("010B");
  if(r.startsWith("41")&&r.length()>=6){
    int kpa=strtol(r.substring(4,6).c_str(),nullptr,16);
    vd.turboBar=max(0.0f,(kpa-101)/100.0f);
  }
  return true;
}

void updateRegenLog(){
  if(vd.dpfRegen&&!lastRegenState){regenStartMs=millis();}
  if(!vd.dpfRegen&&lastRegenState){
    int idx=regenCount%10;            // circular buffer: overwrites oldest entry
    regenLog[idx]={
      (uint16_t)vd.dpfKmRegen,
      (uint8_t)((millis()-regenStartMs)/60000UL),
      vd.dpfPct>=(uint8_t)cfg.dpfCrit
    };
    if(regenLogCount<10) regenLogCount++;
    regenCount++;
  }
  lastRegenState=vd.dpfRegen;
}

// ─── OBD background task (core 0) ────────────────────────────────────────────
static void obdTask(void* pv) {
  for (;;) {
    if (vd.btConnected) {
      readBattery();
      readDPF();
      readEngineData();
      updateRegenLog();
      vd.dataValid = true;
      vd.newData   = true;   // signal UI loop to redraw
    }
    vTaskDelay(pdMS_TO_TICKS(cfg.pollMs));
  }
}

void startOBDTask() {
  // OBD on core 0 — UI (loop) stays on core 1
  xTaskCreatePinnedToCore(obdTask, "OBD", 8192, nullptr, 1, nullptr, 0);
}
