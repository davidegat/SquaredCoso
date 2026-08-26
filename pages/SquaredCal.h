#pragma once

#include "../images/cal_icon.h"
#include <Arduino.h>
#include <time.h>

extern String g_lang;
extern String g_ics;
extern void todayYMD(String &ymd);
extern bool httpGET(const String &url, String &body, uint32_t timeoutMs);
extern int indexOfCI(const String &src, const String &key, int from);
extern void drawHeader(const String &title);
extern void drawBoldMain(int16_t x, int16_t y, const String &raw, uint8_t scale);
extern String sanitizeText(const String &in);
extern Arduino_RGB_Display *gfx;
extern const int PAGE_X, PAGE_Y, CHAR_H, TEXT_SCALE;
extern const uint16_t COL_TEXT, COL_BG;
extern void drawHLine(int y);

struct CalItem {
  char when[16];
  char summary[64];
  time_t ts;
  int32_t rank;
  bool allDay;
  bool used;
};
static CalItem cal[3];

static inline void resetCal() { memset(cal,0,sizeof(cal)); }

static inline String extractAfterColon(const String &src, int pos) {
  int c=src.indexOf(':',pos); if(c<0) return "";
  int e=src.indexOf('\n',c+1); if(e<0) e=src.length();
  String t=src.substring(c+1,e); t.trim(); return t;
}

static inline void copyToBuf(const String &s,char *buf,size_t n) {
  if(!n) return; size_t k=(size_t)s.length(); if(k>=n) k=n-1; memcpy(buf,s.c_str(),k); buf[k]=0;
}

static inline int64_t daysFromCivil(int y,unsigned m,unsigned d) {
  y -= m <= 2;
  const int era=(y>=0?y:y-399)/400;
  const unsigned yoe=(unsigned)(y-era*400);
  const unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;
  const unsigned doe=yoe*365+yoe/4-yoe/100+doy;
  return era*146097+(int)doe-719468;
}

static time_t utcStampToEpoch(const String &stamp) {
  if(stamp.length()<15) return 0;
  int y=stamp.substring(0,4).toInt();
  int mo=stamp.substring(4,6).toInt();
  int d=stamp.substring(6,8).toInt();
  int h=stamp.substring(9,11).toInt();
  int mi=stamp.substring(11,13).toInt();
  int se=stamp.substring(13,15).toInt();
  return (time_t)(daysFromCivil(y,mo,d)*86400LL+h*3600+mi*60+se);
}

static bool parseICSStart(const String &line, time_t &ts, bool &allDay, String &localYmd, String &hm) {
  String stamp=extractAfterColon(line,0);
  if(stamp.length()<8) return false;
  allDay = stamp.length()<15 || stamp[8]!='T';

  if(allDay) {
    localYmd=stamp.substring(0,8);
    hm=(g_lang=="it"?"tutto il giorno":"all day");
    struct tm tt={};
    tt.tm_year=stamp.substring(0,4).toInt()-1900;
    tt.tm_mon=stamp.substring(4,6).toInt()-1;
    tt.tm_mday=stamp.substring(6,8).toInt();
    tt.tm_isdst=-1;
    ts=mktime(&tt);
    return true;
  }

  bool utc=stamp.endsWith("Z") || line.indexOf("TZID=UTC")>=0;
  if(utc) {
    ts=utcStampToEpoch(stamp);
    struct tm lt; localtime_r(&ts,&lt);
    char ymd[9], hmb[6];
    snprintf(ymd,sizeof(ymd),"%04d%02d%02d",lt.tm_year+1900,lt.tm_mon+1,lt.tm_mday);
    snprintf(hmb,sizeof(hmb),"%02d:%02d",lt.tm_hour,lt.tm_min);
    localYmd=ymd; hm=hmb;
  } else {
    struct tm tt={};
    tt.tm_year=stamp.substring(0,4).toInt()-1900;
    tt.tm_mon=stamp.substring(4,6).toInt()-1;
    tt.tm_mday=stamp.substring(6,8).toInt();
    tt.tm_hour=stamp.substring(9,11).toInt();
    tt.tm_min=stamp.substring(11,13).toInt();
    tt.tm_sec=stamp.substring(13,15).toInt();
    tt.tm_isdst=-1;
    ts=mktime(&tt);
    localYmd=stamp.substring(0,8);
    hm=stamp.substring(9,11)+":"+stamp.substring(11,13);
  }
  return ts>0;
}

static String cleanICSText(String s) {
  s.replace("\\n"," "); s.replace("\\N"," ");
  s.replace("\\,",","); s.replace("\\;",";"); s.replace("\\\\","\\");
  return sanitizeText(s);
}

static void insertCandidate(const String &summary,const String &when,time_t ts,bool allDay,int32_t rank) {
  CalItem item{};
  copyToBuf(summary,item.summary,sizeof(item.summary));
  copyToBuf(when,item.when,sizeof(item.when));
  item.ts=ts; item.allDay=allDay; item.rank=rank; item.used=true;

  for(int i=0;i<3;i++) {
    if(!cal[i].used || rank<cal[i].rank) {
      for(int j=2;j>i;j--) cal[j]=cal[j-1];
      cal[i]=item; return;
    }
  }
}

bool fetchICS() {
  resetCal();
  if(!g_ics.length()) return true;
  String body;
  if(!httpGET(g_ics,body,15000)) return false;

  // RFC5545 folded lines continue when the next physical line starts with space/tab.
  body.replace("\r\n ",""); body.replace("\r\n\t",""); body.replace("\r","");
  String today; todayYMD(today);
  time_t now=time(nullptr);
  int p=0;
  while(true) {
    int b=body.indexOf("BEGIN:VEVENT",p); if(b<0) break;
    int e=body.indexOf("END:VEVENT",b); if(e<0) break;
    String blk=body.substring(b,e);
    p=e+10;

    int ds=indexOfCI(blk,"DTSTART",0); if(ds<0) continue;
    int lineEnd=blk.indexOf('\n',ds); if(lineEnd<0) lineEnd=blk.length();
    String dtline=blk.substring(ds,lineEnd);
    time_t ts=0; bool allDay=false; String ymd,hm;
    if(!parseICSStart(dtline,ts,allDay,ymd,hm) || ymd!=today) continue;

    int ss=indexOfCI(blk,"SUMMARY",0); if(ss<0) continue;
    String summary=cleanICSText(extractAfterColon(blk,ss));
    if(!summary.length()) continue;

    int32_t rank;
    if(allDay) rank=0;
    else {
      int64_t d=(int64_t)ts-(int64_t)now;
      if (d >= 0) rank = (d > INT32_MAX) ? INT32_MAX : (int32_t)d;
      else { int64_t past = -d; if (past > 86400LL) past = 86400LL; rank = (int32_t)(86400LL + past); }
    }
    insertCandidate(summary,hm,ts,allDay,rank);
  }
  return true;
}

void pageCalendar() {
  drawHeader(g_lang=="it"?"Oggi":"Today");
  const int ICON_MARGIN=20;
  int iconX=480-ICON_MARGIN-CAL_ICON_WIDTH;
  int iconY=PAGE_Y-5;
  drawRLE(iconX,iconY,CAL_ICON_WIDTH,CAL_ICON_HEIGHT,cal_icon,sizeof(cal_icon)/sizeof(RLERun));

  time_t now=time(nullptr); struct tm t; localtime_r(&now,&t);
  String dayStr=String(t.tm_mday);
  gfx->setTextSize(3); gfx->setTextColor(COL_BG,COL_TEXT);
  int textW=dayStr.length()*18;
  gfx->setCursor(iconX+CAL_ICON_WIDTH/2-textW/2,iconY+CAL_ICON_HEIGHT/2-8);
  gfx->print(dayStr); gfx->setTextSize(TEXT_SCALE);

  int y=PAGE_Y;
  if(!cal[0].used) {
    drawBoldMain(PAGE_X,y+CHAR_H,g_lang=="it"?"Nessun evento":"No events",TEXT_SCALE+1);
    return;
  }

  for(uint8_t i=0;i<3 && cal[i].used;i++) {
    CalItem &ev=cal[i];
    drawBoldMain(PAGE_X,y,String(ev.summary),TEXT_SCALE+1);
    y += CHAR_H*(TEXT_SCALE+1)-6;
    gfx->setTextSize(2); gfx->setTextColor(COL_TEXT,COL_BG); gfx->setCursor(PAGE_X,y); gfx->print(ev.when);
    y += CHAR_H*2+4; gfx->setTextSize(TEXT_SCALE);
    if(i<2 && cal[i+1].used) { drawHLine(y); y+=12; }
    if(y>450) break;
  }
}
