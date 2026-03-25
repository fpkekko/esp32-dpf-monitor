#include "config.h"

Config cfg;
Preferences prefs;
VehicleData vd;

RegenEntry regenLog[10];
int regenCount = 0, regenLogCount = 0;
bool lastRegenState = false;
uint32_t regenStartMs = 0;

void loadConfig() {
  prefs.begin("dpfmon", true);
  cfg.carIndex    = prefs.getInt("carIdx",   0);
  cfg.pollMs      = prefs.getInt("pollMs",   POLL_MS_DEFAULT);
  cfg.dpfWarn     = prefs.getInt("dpfWarn",  DPF_WARN_DEFAULT);
  cfg.dpfCrit     = prefs.getInt("dpfCrit",  DPF_CRIT_DEFAULT);
  cfg.diffWarn    = prefs.getInt("diffWarn", DIFF_WARN_DEFAULT);
  cfg.diffCrit    = prefs.getInt("diffCrit", DIFF_CRIT_DEFAULT);
  cfg.oilWarn     = prefs.getInt("oilWarn",  OIL_WARN_DEFAULT);
  cfg.oilCrit     = prefs.getInt("oilCrit",  OIL_CRIT_DEFAULT);
  cfg.coolantWarn = prefs.getInt("cwWarn",   COOLANT_WARN_DEFAULT);
  cfg.battLow     = prefs.getFloat("battLow", BATT_LOW_DEFAULT);
  cfg.altMin      = prefs.getFloat("altMin",  ALT_MIN_DEFAULT);
  cfg.startPage   = prefs.getInt("startPg",  0);
  cfg.brightness  = prefs.getInt("bright",   80);
  cfg.buzzer      = prefs.getBool("buzzer",  false);
  prefs.getString("obdMac", cfg.obdMac, sizeof(cfg.obdMac));
  prefs.end();
}

void saveConfig() {
  prefs.begin("dpfmon", false);
  prefs.putInt("carIdx",   cfg.carIndex);
  prefs.putInt("pollMs",   cfg.pollMs);
  prefs.putInt("dpfWarn",  cfg.dpfWarn);
  prefs.putInt("dpfCrit",  cfg.dpfCrit);
  prefs.putInt("diffWarn", cfg.diffWarn);
  prefs.putInt("diffCrit", cfg.diffCrit);
  prefs.putInt("oilWarn",  cfg.oilWarn);
  prefs.putInt("oilCrit",  cfg.oilCrit);
  prefs.putInt("cwWarn",   cfg.coolantWarn);
  prefs.putFloat("battLow",cfg.battLow);
  prefs.putFloat("altMin", cfg.altMin);
  prefs.putInt("startPg",  cfg.startPage);
  prefs.putInt("bright",   cfg.brightness);
  prefs.putBool("buzzer",  cfg.buzzer);
  prefs.putString("obdMac",cfg.obdMac);
  prefs.end();
}
