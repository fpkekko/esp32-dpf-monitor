#pragma once

#include <Preferences.h>

// ─── BLE UUID ────────────────────────────────────────────────────────────────
// Vgate iCar Pro BLE (IOS-VLINK) — single characteristic for write + notify
#define BLE_SERVICE_UUID  "e7810a71-73ae-499d-8c15-faa9aef0c3f2"
#define BLE_WRITE_UUID    "bef8d6c9-9c21-4c9e-b632-bd58c1009f9f"
#define BLE_NOTIFY_UUID   "bef8d6c9-9c21-4c9e-b632-bd58c1009f9f"

// ─── Pins ────────────────────────────────────────────────────────────────────
#define PIN_BTN1      0
#define PIN_BTN2     14
#define PIN_POWER_ON 15   // display power — LILYGO T-Display-S3 — REQUIRED
#define PIN_TFT_BL   38   // backlight — LILYGO T-Display-S3

// ─── Display resolution (320x170 after setRotation(1)) ───────────────────────
#define SCREEN_W 320
#define SCREEN_H 170

// ─── WiFi AP ─────────────────────────────────────────────────────────────────
#define AP_SSID  "DPF-Monitor"
#define AP_PASS  "dpfmonitor"
#define AP_IP    "192.168.4.1"

// ─── Defaults ────────────────────────────────────────────────────────────────
#define POLL_MS_DEFAULT      2000
#define DPF_WARN_DEFAULT     70
#define DPF_CRIT_DEFAULT     85
#define DIFF_WARN_DEFAULT    30
#define DIFF_CRIT_DEFAULT    45
#define OIL_WARN_DEFAULT     110
#define OIL_CRIT_DEFAULT     120
#define COOLANT_WARN_DEFAULT 98
#define BATT_LOW_DEFAULT     11.8f
#define ALT_MIN_DEFAULT      12.8f

// ─── Colour palette ──────────────────────────────────────────────────────────
#define C_BG    0x0000
#define C_HDR   0x0841
#define C_WHITE 0xFFFF
#define C_GRAY  0x4A49
#define C_DGRAY 0x2104
#define C_GREEN 0x07E0
#define C_YELLOW 0xFFE0
#define C_ORANGE 0xFD20
#define C_RED   0xF800
#define C_BLUE  0x3D9F

// ─── Vehicle profiles ────────────────────────────────────────────────────────
struct CarProfile {
  const char* name;
  const char* model;
  const char* year;
  const char* ecu;
  const char* protocol;   // AT command for ELM327
  const char* header;     // ATSH engine ECU header
  // DPF PIDs
  const char* pidSoot;
  const char* pidRegen;
  const char* pidDpfTemp;
  const char* pidDiffPress;
  const char* pidKmRegen;
  // formulas (used by web page; parsing done in readDPF_byProfile)
  bool  hasAdblue;
  int   dpfMaxGrams;  // 0 = use % directly
};

const CarProfile PROFILES[] = {
  {
    "Citroën C4 Spacetourer",
    "2.0 BlueHDi 163cv",
    "2019",
    "Delphi DCM6.2A",
    "ATSP6",       // ISO 15765-4 CAN 11bit 500kbaud
    "7E0",
    "2102FF",      // soot % → A/255*100
    "2102FE",      // regen active → bit 0
    "210261",      // DPF temp °C → (A*256+B)-400  [verify live]
    "210262",      // diff pressure mbar → (A*256+B)/10 [verify]
    "21026D",      // km since regen → A*256+B
    true,
    0
  },
  {
    "Opel Astra J",
    "1.7 CDTI A17DTR 110cv",
    "2011",
    "DENSO (A17DTR)",
    "ATSP6",
    "7E0",
    "22336A",      // soot % → A/255*100  (0-255 → 0-100%)
    "223274",      // regen active → v>0 (0=off, 1-255=progress)
    "223279",      // DPF temp °C → A*5-40  (1-byte, -40..1235°C)
    "2220F4",      // diff pressure mbar → SIGNED(A)
    "223277",      // km since regen → A*65536+B*256+C  (3-byte)
    false,
    0
  }
};
const int NUM_PROFILES = 2;

// ─── Configuration (loaded/saved from NVS) ───────────────────────────────────
struct Config {
  int   carIndex       = 0;
  char  obdMac[18]     = "";
  int   pollMs         = POLL_MS_DEFAULT;
  int   dpfWarn        = DPF_WARN_DEFAULT;
  int   dpfCrit        = DPF_CRIT_DEFAULT;
  int   diffWarn       = DIFF_WARN_DEFAULT;
  int   diffCrit       = DIFF_CRIT_DEFAULT;
  int   oilWarn        = OIL_WARN_DEFAULT;
  int   oilCrit        = OIL_CRIT_DEFAULT;
  int   coolantWarn    = COOLANT_WARN_DEFAULT;
  float battLow        = BATT_LOW_DEFAULT;
  float altMin         = ALT_MIN_DEFAULT;
  int   startPage      = 0;
  int   brightness     = 80;
  bool  buzzer         = false;
};

extern Config cfg;
extern Preferences prefs;

void loadConfig();
void saveConfig();

// ─── Vehicle data ─────────────────────────────────────────────────────────────
struct VehicleData {
  int   dpfPct=0, dpfTempC=0, dpfDiffPress=0, dpfKmRegen=0;
  bool  dpfRegen=false;
  int   oilTempC=0, coolantTempC=0, rpm=0;
  float turboBar=0;
  float battV=0;
  bool     engineOn=false, altOk=true, btConnected=false, dataValid=false;
  uint32_t engineStartMs=0;      // millis() when engine last turned on
  volatile bool newData=false;   // set by OBD task, cleared by UI loop
};
extern VehicleData vd;

struct RegenEntry { uint16_t km; uint8_t durMin; bool forced; };
extern RegenEntry regenLog[10];
extern int regenCount, regenLogCount;
extern bool lastRegenState;
extern uint32_t regenStartMs;
