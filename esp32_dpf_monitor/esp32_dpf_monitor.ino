/*
 * esp32-dpf-monitor v1.0 — WiFi config + 2 vehicle profiles
 *
 * Features:
 *   - Integrated WiFi Access Point (192.168.4.1)
 *   - Browser-based configuration page (no app required)
 *   - Citroën C4 Spacetourer 2019 2.0 BlueHDi profile (ECU Delphi DCM6.2A)
 *   - Opel Astra J 2011 1.7 CDTI A17DTR profile (ECU Delco E87)
 *   - Settings saved to NVS (persist across reboots)
 *
 * Libraries: BLEDevice, TFT_eSPI, WiFi, WebServer, Preferences
 */

#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <TFT_eSPI.h>
#include <FS.h>
using namespace fs;
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

#include "config.h"
#include "obd.h"
#include "display.h"
#include "webui.h"

// ─── Global instances ────────────────────────────────────────────────────────
TFT_eSPI tft = TFT_eSPI();
WebServer server(80);

int  currentPage = 0;
bool btn1Last = HIGH, btn2Last = HIGH;
bool webConfigChanged = false;
bool needsRedraw      = true;

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);

  loadConfig();
  currentPage = cfg.startPage;

  // Display power — must be the very first command
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);

  // Backlight before init to avoid black flash
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(C_BG);

  // Splash screen
  tft.setTextColor(C_WHITE, C_BG); tft.setTextSize(2);
  tft.setCursor(20, 50); tft.print("DPF Monitor");
  const CarProfile& car = PROFILES[cfg.carIndex];
  tft.setTextSize(1); tft.setTextColor(C_BLUE, C_BG);
  tft.setCursor(20, 80); tft.print(car.name);
  tft.setCursor(20, 95); tft.print(car.model);

  // Start WiFi AP
  tft.setTextColor(C_GRAY, C_BG);
  tft.setCursor(20, 118); tft.print("WiFi AP: " AP_SSID);
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress apIP(192,168,4,1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255,255,255,0));

  initWebServer();
  server.begin();
  tft.setCursor(20, 134); tft.print("IP: 192.168.4.1");

  // Start BLE OBD
  tft.setCursor(20, 152); tft.print("Connecting OBD BLE...");
  BLEDevice::init("ESP32_DPF");
  if (!connectOBD()) {
    tft.setTextColor(C_ORANGE, C_BG);
    tft.setCursor(20, 168); tft.print("OBD not found (BLE)");
    tft.setCursor(20, 184); tft.print("Display active, WiFi config OK");
  } else {
    initELM();
  }
  delay(1500);
  tft.fillScreen(C_BG);
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();

  // Reload config if changed from web UI
  if (webConfigChanged) {
    webConfigChanged = false;
    needsRedraw = true;
  }

  // Buttons
  bool b1=digitalRead(PIN_BTN1), b2=digitalRead(PIN_BTN2);
  if (btn1Last==HIGH && b1==LOW) { currentPage=(currentPage-1+4)%4; tft.fillScreen(C_BG); needsRedraw=true; delay(30); }
  if (btn2Last==HIGH && b2==LOW) { currentPage=(currentPage+1)%4;   tft.fillScreen(C_BG); needsRedraw=true; delay(30); }
  btn1Last=b1; btn2Last=b2;

  // Poll OBD
  static uint32_t lastPoll=0;
  if (vd.btConnected && millis()-lastPoll >= (uint32_t)cfg.pollMs) {
    lastPoll=millis();
    readBattery(); readDPF(); readEngineData();
    updateRegenLog();
    vd.dataValid=true;
    needsRedraw=true;
  }

  // Alarm blink tick (500 ms)
  static uint32_t lastAlarmTick=0;
  bool hasAlarm = vd.dataValid && (vd.dpfPct>=cfg.dpfCrit || vd.dpfRegen ||
                  vd.oilTempC>=cfg.oilCrit || vd.turboBar>2.2f ||
                  (!vd.altOk&&vd.engineOn) || vd.battV<cfg.battLow);
  if (hasAlarm && millis()-lastAlarmTick>=500) {
    lastAlarmTick=millis();
    needsRedraw=true;
  }

  // Redraw only when needed
  if (needsRedraw) {
    needsRedraw=false;
    switch(currentPage){
      case 0: drawPage0(); break;
      case 1: drawPage1(); break;
      case 2: drawPage2(); break;
      case 3: drawPage3(); break;
    }
    drawGlobalAlarm();
  }
  delay(100);
}
