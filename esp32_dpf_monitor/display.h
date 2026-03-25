#pragma once

#include <Arduino.h>

void drawPage0();
void drawPage1();
void drawPage2();
void drawPage3();
void drawHeader(const char* t);
void drawBar(int x, int y, int w, int h, int pct, uint16_t col);
void drawMiniCard(int x, int y, int w, const char* lbl, const char* val, uint16_t col);
void drawAlarm(const char* msg);
void drawGlobalAlarm();
uint16_t dpfColor(int p);
uint16_t tempColor(int t, int w, int c);
uint16_t battColor(float v);
