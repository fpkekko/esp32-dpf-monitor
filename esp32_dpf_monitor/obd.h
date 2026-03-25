#pragma once

#include <Arduino.h>

bool connectOBD();
String sendOBD(const String& cmd, int ms = 1000);
String sendOBD(const char* cmd, int ms = 1000);
void initELM();
bool readDPF();
bool readBattery();
bool readEngineData();
void updateRegenLog();
