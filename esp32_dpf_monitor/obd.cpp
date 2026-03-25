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
    if (c != '\r' && c != '\n') bleRxBuf += c;
  }
}

class OBDScanCallback : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice dev) override {
    String name = dev.getName().c_str(); name.toUpperCase();
    if (name.indexOf("OBD")>=0 || name.indexOf("ELM")>=0 || name.indexOf("VGATE")>=0) {
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
  bleRxBuf.trim();
  return bleRxBuf;
}

String sendOBD(const char* cmd, int ms){ return sendOBD(String(cmd), ms); }

bool readBattery() {
  String r=sendOBD("ATRV"); r.replace("V",""); r.trim();
  float v=r.toFloat();
  if(v>8&&v<18){vd.battV=v;vd.engineOn=(v>13.0f);vd.altOk=!vd.engineOn||(v>=cfg.altMin);return true;}
  return false;
}

bool readDPF() {
  const CarProfile& car=PROFILES[cfg.carIndex];
  int idx=cfg.carIndex;

  // Soot %
  String r=sendOBD(car.pidSoot);
  if(r.length()>=8 && r[0]=='6') {
    int raw=strtol(r.substring(6,8).c_str(),nullptr,16);
    if(idx==0) vd.dpfPct=(int)(raw/255.0f*100.0f);  // PSA: A/255*100
    else       vd.dpfPct=raw;                          // GM:  A (already %)
  }

  // Regen active
  r=sendOBD(car.pidRegen);
  if(r.length()>=8 && r[0]=='6')
    vd.dpfRegen=strtol(r.substring(6,8).c_str(),nullptr,16)&0x01;

  // DPF temperature
  r=sendOBD(car.pidDpfTemp);
  if(r.length()>=10 && r[0]=='6'){
    int hi=strtol(r.substring(6,8).c_str(),nullptr,16);
    int lo=strtol(r.substring(8,10).c_str(),nullptr,16);
    int raw=hi*256+lo;
    if(idx==0) vd.dpfTempC=raw-400;          // PSA
    else       vd.dpfTempC=raw/10-40;         // GM
  }

  // Differential pressure
  r=sendOBD(car.pidDiffPress);
  if(r.length()>=10 && r[0]=='6'){
    int hi=strtol(r.substring(6,8).c_str(),nullptr,16);
    int lo=strtol(r.substring(8,10).c_str(),nullptr,16);
    vd.dpfDiffPress=(hi*256+lo)/10;
  }

  // Km since last regen
  r=sendOBD(car.pidKmRegen);
  if(r.length()>=10 && r[0]=='6'){
    int hi=strtol(r.substring(6,8).c_str(),nullptr,16);
    int lo=strtol(r.substring(8,10).c_str(),nullptr,16);
    vd.dpfKmRegen=hi*256+lo;
  }
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
