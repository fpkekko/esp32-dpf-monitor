#include "display.h"
#include "config.h"

#include <TFT_eSPI.h>

extern TFT_eSPI tft;
extern int currentPage;

// ─── Colour helpers ──────────────────────────────────────────────────────────
uint16_t dpfColor(int p){
  return p>=cfg.dpfCrit?C_RED:p>=cfg.dpfWarn?C_ORANGE:p>=50?C_YELLOW:C_GREEN;
}
uint16_t tempColor(int t,int w,int c){
  return t>=c?C_RED:t>=w?C_ORANGE:C_GREEN;
}
uint16_t battColor(float v){
  return v<cfg.battLow?C_RED:v<12.4f?C_YELLOW:v>14.8f?C_ORANGE:C_GREEN;
}

// ─── Primitives ──────────────────────────────────────────────────────────────
void drawHeader(const char* t) {
  tft.fillRect(0,0,SCREEN_W,18,C_HDR);
  tft.setTextColor(C_WHITE,C_HDR); tft.setTextSize(1);
  tft.setCursor(8,5); tft.print(t);
  for(int i=0;i<4;i++){
    tft.fillCircle(146+i*12,9,3,i==currentPage?C_BLUE:C_DGRAY);
  }
  tft.setTextColor(0x07E0,C_HDR);
  tft.setCursor(282,5); tft.print("WiFi");
  tft.drawFastHLine(0,18,SCREEN_W,C_DGRAY);
}

void drawBar(int x,int y,int w,int h,int pct,uint16_t col){
  tft.drawRect(x,y,w,h,C_DGRAY);
  tft.fillRect(x+1,y+1,w-2,h-2,0x0841);
  int f=max(0,(w-2)*pct/100);
  if(f>0) tft.fillRect(x+1,y+1,f,h-2,col);
}

void drawMiniCard(int x,int y,int w,const char* lbl,const char* val,uint16_t col){
  tft.fillRect(x,y,w,32,0x0841); tft.drawRect(x,y,w,32,C_DGRAY);
  tft.setTextColor(C_GRAY,0x0841); tft.setTextSize(1);
  tft.setCursor(x+w/2-strlen(lbl)*3,y+5); tft.print(lbl);
  tft.setTextColor(col,0x0841);
  tft.setCursor(x+w/2-strlen(val)*4,y+18); tft.print(val);
}

void drawAlarm(const char* msg){
  static bool flip=false; flip=!flip;
  tft.fillRect(0,150,SCREEN_W,20,flip?0x7800:0x4000);
  tft.setTextColor(0xFCA5,flip?0x7800:0x4000); tft.setTextSize(1);
  tft.setCursor(max(0,160-(int)strlen(msg)*3),156); tft.print(msg);
}

// ─── Global alarms — visible on all pages ────────────────────────────────────
void drawGlobalAlarm() {
  if (!vd.dataValid) { tft.fillRect(0,150,320,20,C_BG); return; }
  bool regenForced = regenLogCount>=2 &&
                     regenLog[(regenCount-1)%10].forced &&
                     regenLog[(regenCount-2+10)%10].forced;
  if      (vd.dpfPct>=cfg.dpfCrit)   drawAlarm("DPF FULL - REGENERATION NEEDED");
  else if (vd.dpfRegen)               drawAlarm("REGEN IN PROGRESS - KEEP 60+ km/h");
  else if (vd.oilTempC>=cfg.oilCrit) drawAlarm("OIL CRITICAL - STOP THE VEHICLE");
  else if (vd.turboBar>2.2f)          drawAlarm("TURBO OVERPRESSURE");
  else if (!vd.altOk&&vd.engineOn)    drawAlarm("ALTERNATOR FAULT - SEE A MECHANIC");
  else if (vd.battV<cfg.battLow && millis()-vd.engineStartMs>60000UL) drawAlarm("BATTERY LOW");
  else if (regenForced)               drawAlarm("REPEATED FORCED REGENS - CHECK DPF");
  else                                tft.fillRect(0,150,320,20,C_BG);
}

// ─── Pages ───────────────────────────────────────────────────────────────────
void drawPage0(){
  const CarProfile& car=PROFILES[cfg.carIndex];
  drawHeader(car.name);
  uint16_t col=dpfColor(vd.dpfPct);
  char buf[16];
  snprintf(buf,sizeof(buf),"%d%%",vd.dpfPct);
  tft.setTextColor(col,C_BG); tft.setTextSize(3);
  tft.setCursor(96,22); tft.print(buf);
  tft.setTextColor(C_GRAY,C_BG); tft.setTextSize(1);
  tft.setCursor(10,50); tft.print("DPF soot level");
  drawBar(10,58,300,10,vd.dpfPct,col);
  snprintf(buf,sizeof(buf),"%d C",vd.dpfTempC);
  drawMiniCard(8,72,96,"TEMP DPF",buf,tempColor(vd.dpfTempC,480,600));
  snprintf(buf,sizeof(buf),"%d mb",vd.dpfDiffPress);
  drawMiniCard(112,72,96,"PRESS DIFF",buf,
    vd.dpfDiffPress>=cfg.diffCrit?C_RED:vd.dpfDiffPress>=cfg.diffWarn?C_ORANGE:C_GREEN);
  snprintf(buf,sizeof(buf),"%d km",vd.dpfKmRegen);
  drawMiniCard(216,72,96,"KM SINCE REGEN",buf,
    vd.dpfKmRegen>600?C_RED:vd.dpfKmRegen>400?C_ORANGE:C_GREEN);
  uint16_t rCol=vd.dpfRegen?C_ORANGE:C_DGRAY;
  tft.fillRect(10,110,300,24,vd.dpfRegen?0x2000:0x0841);
  tft.drawRect(10,110,300,24,rCol);
  tft.setTextColor(rCol,vd.dpfRegen?0x2000:0x0841); tft.setTextSize(1);
  tft.setCursor(18,119);
  tft.print(vd.dpfRegen?"REGENERATION ACTIVE - DO NOT SWITCH OFF":"regeneration: inactive");
}

void drawPage1(){
  drawHeader("Regen History");
  char buf[24];
  snprintf(buf,sizeof(buf),"%d",regenCount);
  tft.setTextColor(C_GREEN,C_BG); tft.setTextSize(2);
  tft.setCursor(10,22); tft.print(buf);
  tft.setTextColor(C_GRAY,C_BG); tft.setTextSize(1);
  tft.setCursor(36,26); tft.print("total");
  if(regenLogCount>0){
    uint32_t sk=0,sd=0; int fc=0;
    for(int i=0;i<regenLogCount;i++){sk+=regenLog[i].km;sd+=regenLog[i].durMin;if(regenLog[i].forced)fc++;}
    snprintf(buf,sizeof(buf),"avg: %u km  %u min",(unsigned)(sk/regenLogCount),(unsigned)(sd/regenLogCount));
    tft.setTextColor(C_BLUE,C_BG); tft.setCursor(100,22); tft.print(buf);
    snprintf(buf,sizeof(buf),"forced: %d/%d",fc,regenLogCount);
    tft.setTextColor(fc>regenLogCount/2?C_ORANGE:C_GRAY,C_BG);
    tft.setCursor(100,34); tft.print(buf);
  }
  tft.drawFastHLine(0,44,SCREEN_W,C_DGRAY);
  tft.setTextColor(C_GRAY,C_BG);
  tft.setCursor(8,48);  tft.print("#");
  tft.setCursor(28,48); tft.print("km");
  tft.setCursor(80,48); tft.print("duration");
  tft.setCursor(150,48);tft.print("type");
  tft.drawFastHLine(0,57,SCREEN_W,C_DGRAY);
  int sh=min(regenLogCount,6);
  for(int i=0;i<sh;i++){
    int y=62+i*14;
    RegenEntry& e=regenLog[(regenCount-1-i+10)%10];
    uint16_t bg=(i%2==0)?0x0421:C_BG;
    tft.fillRect(0,y-2,SCREEN_W,14,bg);
    tft.setTextColor(C_WHITE,bg);
    snprintf(buf,sizeof(buf),"%d",regenLogCount-i); tft.setCursor(8,y); tft.print(buf);
    snprintf(buf,sizeof(buf),"%d",e.km);             tft.setCursor(28,y); tft.print(buf);
    snprintf(buf,sizeof(buf),"%d min",e.durMin);     tft.setCursor(80,y); tft.print(buf);
    tft.setTextColor(e.forced?C_ORANGE:C_GREEN,bg);  tft.setCursor(150,y);
    tft.print(e.forced?"forced":"routine");
  }
  if(regenLogCount==0){
    tft.setTextColor(C_GRAY,C_BG);
    tft.setCursor(60,90); tft.print("no regen recorded");
  }
}

void drawPage2(){
  drawHeader("Engine & Turbo");
  char buf[16];
  uint16_t oCol=tempColor(vd.oilTempC,cfg.oilWarn,cfg.oilCrit);
  uint16_t wCol=tempColor(vd.coolantTempC,cfg.coolantWarn,cfg.coolantWarn+7);
  tft.setTextColor(C_GRAY,C_BG); tft.setTextSize(1);
  tft.setCursor(10,22); tft.print("OIL TEMP");
  snprintf(buf,sizeof(buf),"%d C",vd.oilTempC);
  tft.setTextColor(oCol,C_BG); tft.setTextSize(2); tft.setCursor(10,32); tft.print(buf);
  drawBar(10,50,148,6,min(100,vd.oilTempC*100/130),oCol);
  tft.setTextColor(C_GRAY,C_BG); tft.setTextSize(1);
  tft.setCursor(170,22); tft.print("COOLANT TEMP");
  snprintf(buf,sizeof(buf),"%d C",vd.coolantTempC);
  tft.setTextColor(wCol,C_BG); tft.setTextSize(2); tft.setCursor(170,32); tft.print(buf);
  drawBar(170,50,140,6,min(100,vd.coolantTempC*100/110),wCol);
  tft.drawFastHLine(0,62,SCREEN_W,C_DGRAY);
  uint16_t tCol=vd.turboBar>2.2f?C_RED:vd.turboBar>1.8f?C_ORANGE:vd.turboBar>0.3f?C_GREEN:C_GRAY;
  tft.fillRect(8,66,148,56,0x0841); tft.drawRect(8,66,148,56,C_DGRAY);
  tft.setTextColor(C_GRAY,0x0841); tft.setTextSize(1);
  tft.setCursor(42,72); tft.print("TURBO BAR");
  snprintf(buf,sizeof(buf),"%.1f b",vd.turboBar);
  tft.setTextColor(tCol,0x0841); tft.setTextSize(2); tft.setCursor(30,84); tft.print(buf);
  drawBar(16,108,132,6,min(100,(int)(vd.turboBar/2.5f*100)),tCol);
  uint16_t rCol=vd.rpm>4000?C_ORANGE:vd.rpm>2500?C_YELLOW:C_GREEN;
  tft.fillRect(164,66,148,56,0x0841); tft.drawRect(164,66,148,56,C_DGRAY);
  tft.setTextColor(C_GRAY,0x0841); tft.setTextSize(1); tft.setCursor(190,72); tft.print("ENGINE RPM");
  snprintf(buf,sizeof(buf),"%d",vd.rpm);
  tft.setTextColor(rCol,0x0841); tft.setTextSize(2); tft.setCursor(176,84); tft.print(buf);
  drawBar(172,108,132,6,min(100,vd.rpm/50),rCol);
  bool issue=vd.oilTempC>=cfg.oilWarn||vd.coolantTempC>=cfg.coolantWarn||vd.turboBar>2.0f;
  tft.fillRect(8,126,304,20,issue?0x2000:0x0421); tft.drawRect(8,126,304,20,issue?C_ORANGE:C_DGRAY);
  tft.setTextColor(issue?C_ORANGE:C_GREEN,issue?0x2000:0x0421); tft.setTextSize(1);
  tft.setCursor(16,133);
  if(vd.oilTempC>=cfg.oilCrit)               tft.print("OIL OVERHEATING");
  else if(vd.coolantTempC>=cfg.coolantWarn+7) tft.print("COOLANT OVERHEATING");
  else if(vd.turboBar>2.2f)                   tft.print("TURBO OVERPRESSURE");
  else if(issue)                              tft.print("temperature rising");
  else                                        tft.print("all parameters normal");
}

void drawPage3(){
  drawHeader("Battery & Alternator");
  char buf[16];
  uint16_t bCol=battColor(vd.battV);
  snprintf(buf,sizeof(buf),"%.1fV",vd.battV);
  tft.setTextColor(bCol,C_BG); tft.setTextSize(3); tft.setCursor(88,22); tft.print(buf);
  tft.setTextColor(C_GRAY,C_BG); tft.setTextSize(1); tft.setCursor(10,50); tft.print("battery voltage");
  int pct=constrain((int)((vd.battV-10.5f)/5.5f*100.0f),0,100);
  drawBar(10,58,300,10,pct,bCol);
  tft.setTextColor(C_GRAY,C_BG);
  tft.setCursor(10,72);  tft.print("10.5");
  tft.setCursor(66,72);  tft.print("11.8");
  tft.setCursor(122,72); tft.print("12.6");
  tft.setCursor(178,72); tft.print("13.8");
  tft.setCursor(245,72); tft.print("14.8V");
  tft.drawFastHLine(0,84,SCREEN_W,C_DGRAY);
  uint16_t aCol=!vd.engineOn?C_GRAY:vd.altOk?C_GREEN:C_RED;
  uint16_t aBg =!vd.engineOn?0x0841:vd.altOk?0x0421:0x2000;
  tft.fillRect(10,90,300,28,aBg); tft.drawRect(10,90,300,28,aCol);
  tft.setTextColor(aCol,aBg); tft.setTextSize(1); tft.setCursor(18,101);
  if(!vd.engineOn)    tft.print("engine off - alternator n/a");
  else if(vd.altOk)   tft.print("ALTERNATOR OK");
  else                tft.print("ALTERNATOR NOT CHARGING!");
  if(vd.engineOn){snprintf(buf,sizeof(buf),"%.1fV",vd.battV);tft.setCursor(200,101);tft.print(buf);}
  tft.setCursor(10,124);
  if(!vd.engineOn){
    int rng=max(0,(int)((vd.battV-10.5f)/5.5f*800.0f));
    snprintf(buf,sizeof(buf),"estimated range: ~%d km",rng);
    tft.setTextColor(C_GRAY,C_BG);
  } else {
    snprintf(buf,sizeof(buf),"range: n/a (alternator active)");
    tft.setTextColor(C_DGRAY,C_BG);
  }
  tft.print(buf);
}
