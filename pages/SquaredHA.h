#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include "../handlers/globals.h"
#include "../handlers/jsonhelpers.h"

extern Arduino_RGB_Display *gfx;
extern const uint16_t COL_BG, COL_TEXT, COL_ACCENT1, COL_DIVIDER;
extern const int PAGE_X, PAGE_Y, PAGE_W, PAGE_H;
extern const int BASE_CHAR_W, BASE_CHAR_H, TEXT_SCALE;
extern void drawHLine(int y);
extern void drawHeader(const String &title);

static constexpr uint8_t HA_MAX_ENTRIES = 17;
static constexpr uint8_t HA_NAME_LEN = 28;
static constexpr uint8_t HA_STATE_LEN = 12;
static constexpr uint16_t HA_OFF_COL = 0x03E0;
static constexpr uint16_t HA_ON_COL = 0x07E0;
static constexpr uint32_t HA_POLL_TARGETED_MS = 15000UL;
static constexpr uint32_t HA_POLL_AUTO_MS = 30000UL;

struct HAEntry {
  char name[HA_NAME_LEN];
  char state[HA_STATE_LEN];
  uint8_t flags;
};
static constexpr uint8_t HA_F_BATT = 0x01;
static constexpr uint8_t HA_F_ONOFF = 0x02;
static constexpr uint8_t HA_F_TEMPH = 0x04;

static HAEntry ha_entries[HA_MAX_ENTRIES];
static uint8_t ha_count = 0;
static uint32_t ha_nextPollMs = 0;
static char ha_host[48];
static struct { uint8_t ready:1; uint8_t dirty:1; uint8_t reserved:6; } ha_flags={0,1,0};

static bool hasCI(const char *hay, const char *needle) {
  if (!hay || !needle) return false;
  size_t nl=strlen(needle), hl=strlen(hay);
  if (!nl || nl>hl) return false;
  for (size_t i=0;i+nl<=hl;i++) {
    size_t j=0;
    while (j<nl && tolower((unsigned char)hay[i+j])==tolower((unsigned char)needle[j])) j++;
    if (j==nl) return true;
  }
  return false;
}

static bool isBattId(const char *id) {
  return hasCI(id,"battery") || hasCI(id,"batteria") || strstr(id,"_bat") != nullptr;
}
static bool isTempHum(const char *id) {
  if (strncmp(id,"sensor.",7)!=0) return false;
  return hasCI(id,"temperature") || hasCI(id,"temperatura") || hasCI(id,"humidity") || hasCI(id,"umid");
}
static bool allowEnt(const char *id, const char *fname) {
  if (hasCI(id,"sun.") || hasCI(fname,"sun")) return false;
  if (!strncmp(id,"light.",6) || !strncmp(id,"switch.",7)) return true;
  if (strstr(id,"_plug") || strstr(id,"_outlet")) return true;
  if (!strncmp(id,"binary_sensor.",14) && (hasCI(id,"motion") || hasCI(id,"movimento"))) return true;
  return isBattId(id) || isTempHum(id);
}

static void copyTrim3(char *dest, uint8_t size, const char *src) {
  if (!size) return;
  uint8_t di=0, spaces=0;
  while (*src && di<size-1) {
    if (*src==' ' && ++spaces>=3) break;
    dest[di++]=*src++;
  }
  dest[di]=0;
}

static void normState(char *dest, uint8_t size, const char *raw,
                      const char *unit, const char *entityId) {
  if (!size) return;
  if (!raw || !raw[0]) { strncpy(dest,"--",size); dest[size-1]=0; return; }
  size_t len=strlen(raw);
  if (!strcasecmp(raw,"low")) { strncpy(dest,"Low",size); dest[size-1]=0; return; }
  if (!strcasecmp(raw,"high")) { strncpy(dest,"High",size); dest[size-1]=0; return; }
  if (!strcasecmp(raw,"medium") || !strcasecmp(raw,"middle")) { strncpy(dest,"Middle",size); dest[size-1]=0; return; }

  bool numeric=true;
  for (size_t i=0;i<len;i++) {
    if (!isdigit((unsigned char)raw[i]) && raw[i]!='.' && raw[i]!='-' && raw[i]!='+') {
      numeric=false; break;
    }
  }

  if (numeric) {
    String shown(raw);
    String u = unit ? String(unit) : String();
    u.trim();

    u.replace("°C", "C");
    u.replace("°F", "F");
    if (!u.length() && entityId) {
      if (isBattId(entityId) || hasCI(entityId,"humidity") || hasCI(entityId,"umid")) u = "%";
      else if (hasCI(entityId,"temperature") || hasCI(entityId,"temperatura")) u = "C";
    }
    if (u.length()) {
      if (u == "%") shown += "%";
      else { shown += " "; shown += u; }
    }
    strncpy(dest, shown.c_str(), size-1); dest[size-1]=0;
    return;
  }

  len = len < (size_t)size-1 ? len : (size_t)size-1;
  memcpy(dest,raw,len); dest[len]=0;
  dest[0]=toupper((unsigned char)dest[0]);
}

static bool discoverHA() {
  int n=MDNS.queryService("_home-assistant","_tcp");
  if (n<=0) return false;
  IPAddress ip=MDNS.IP(0);
  snprintf(ha_host,sizeof(ha_host),"%u.%u.%u.%u",ip[0],ip[1],ip[2],ip[3]);
  return true;
}

static String haBaseURL() {
  String host=g_ha_ip;
  host.trim();
  if (!host.length()) host=ha_host;
  if (host.startsWith("http://") || host.startsWith("https://")) return host;
  String url=F("http://"); url += host;

  int firstColon=host.indexOf(':');
  if (firstColon < 0) url += F(":8123");
  return url;
}

static bool haGET(const String &path, String &body) {
  if (!g_ha_token.length()) return false;
  if (!g_ha_ip.length() && !ha_host[0] && !discoverHA()) return false;
  HTTPClient http;
  http.setTimeout(4000);
  http.setReuse(false);
  String url=haBaseURL()+path;
  if (!http.begin(url)) return false;
  http.addHeader(F("Authorization"), String(F("Bearer ")) + g_ha_token);
  int code=http.GET();
  if (code!=200) { http.end(); return false; }
  body=http.getString();
  http.end();
  return body.length()>2;
}

static bool addEntry(const String &id, const String &state, const String &fname,
                     const String &unit, bool forced) {
  if (ha_count>=HA_MAX_ENTRIES || !id.length() || !state.length()) return false;
  if (state=="unknown" || state=="unavailable") return false;
  if (!forced && !allowEnt(id.c_str(),fname.c_str())) return false;

  HAEntry &e=ha_entries[ha_count];
  e.flags=0;
  String display=fname.length()?fname:id;
  if (isBattId(id.c_str())) e.flags|=HA_F_BATT;
  if (isTempHum(id.c_str())) e.flags|=HA_F_TEMPH;
  if (state=="on" || state=="off") e.flags|=HA_F_ONOFF;
  copyTrim3(e.name,HA_NAME_LEN,display.c_str());
  normState(e.state,HA_STATE_LEN,state.c_str(),unit.c_str(),id.c_str());
  ha_count++;
  return true;
}

static bool parseSingleEntity(const String &body, bool forced) {
  String id,state,fname,unit;
  if (!jsonStringAfterKey(body,"entity_id",id)) return false;
  if (!jsonStringAfterKey(body,"state",state)) return false;
  jsonStringAfterKey(body,"friendly_name",fname);
  jsonStringAfterKey(body,"unit_of_measurement",unit);
  return addEntry(id,state,fname,unit,forced);
}

static uint8_t parseConfiguredIds(String ids[], uint8_t maxIds) {
  String src=g_ha_entities;
  uint8_t n=0;
  int pos=0;
  while (n<maxIds && pos<src.length()) {
    int comma=src.indexOf(',',pos);
    if (comma<0) comma=src.length();
    String id=src.substring(pos,comma); id.trim();
    if (id.length()) ids[n++]=id;
    pos=comma+1;
  }
  return n;
}

static bool fetchHAStates() {
  ha_count=0;
  ha_flags.ready=0;
  if (!g_ha_token.length()) return false;

  String ids[HA_MAX_ENTRIES];
  uint8_t idCount=parseConfiguredIds(ids,HA_MAX_ENTRIES);
  if (idCount) {
    uint8_t consecutiveFailures = 0;
    for (uint8_t i=0;i<idCount && ha_count<HA_MAX_ENTRIES;i++) {
      String body;
      String path=F("/api/states/"); path += ids[i];
      if (haGET(path,body)) {
        parseSingleEntity(body,true);
        consecutiveFailures = 0;
      } else if (++consecutiveFailures >= 3) {
        break;
      }
      delay(1);
    }
  } else {
    // Full-state discovery is kept only as a compatibility fallback.
    String body;
    if (!haGET(F("/api/states"),body)) return false;
    int pos=0;
    while (ha_count<HA_MAX_ENTRIES) {
      int ep=body.indexOf(F("\"entity_id\""),pos);
      if (ep<0) break;
      String id,state,fname,unit;
      if (!jsonStringAfterKey(body,"entity_id",id,ep)) break;
      int sp=body.indexOf(F("\"state\""),ep);
      if (sp<0 || !jsonStringAfterKey(body,"state",state,sp)) { pos=ep+11; continue; }
      int next=body.indexOf(F("\"entity_id\""),sp+1);
      int fp=body.indexOf(F("\"friendly_name\""),sp);
      if (fp>=0 && (next<0 || fp<next)) jsonStringAfterKey(body,"friendly_name",fname,fp);
      int up=body.indexOf(F("\"unit_of_measurement\""),sp);
      if (up>=0 && (next<0 || up<next)) jsonStringAfterKey(body,"unit_of_measurement",unit,up);
      addEntry(id,state,fname,unit,false);
      pos=sp+8;
    }
  }

  ha_flags.ready=1;
  ha_flags.dirty=1;
  return true;
}

void tickHA() {
  uint32_t now=millis();
  if ((int32_t)(now-ha_nextPollMs)>=0) {
    ha_nextPollMs=now+(g_ha_entities.length()?HA_POLL_TARGETED_MS:HA_POLL_AUTO_MS);
    fetchHAStates();
  }
}

void pageHA() {
  if (!ha_flags.ready) {
    drawHeader(F("Home Assistant"));
    gfx->setTextSize(TEXT_SCALE);
    gfx->setTextColor(COL_TEXT,COL_BG);
    gfx->setCursor(PAGE_X,PAGE_Y+20);
    gfx->print(F("Ricerca..."));
    return;
  }
  if (!ha_flags.dirty) return;
  ha_flags.dirty=0;

  drawHeader(F("Home Assistant"));
  gfx->fillRect(0,PAGE_Y,480,480-PAGE_Y,COL_BG);
  gfx->setTextSize(TEXT_SCALE);
  gfx->setTextColor(COL_TEXT,COL_BG);
  const int16_t lineH=BASE_CHAR_H*TEXT_SCALE;
  const int16_t yMax=PAGE_Y+PAGE_H-lineH;
  int16_t y=PAGE_Y+4;

  if (!ha_count) {
    gfx->setCursor(PAGE_X,y+20);
    gfx->print(F("Nessuna entita'"));
    return;
  }

  for (uint8_t i=0;i<ha_count && y<=yMax;i++) {
    const HAEntry &e=ha_entries[i];
    gfx->setCursor(PAGE_X,y); gfx->print(e.name);
    if (e.flags & HA_F_ONOFF) {
      uint16_t col=!strcasecmp(e.state,"On")?HA_ON_COL:HA_OFF_COL;
      gfx->fillRoundRect(PAGE_X+PAGE_W-40,y-2,32,lineH,4,col);
    } else {
      int16_t tw=strlen(e.state)*BASE_CHAR_W*TEXT_SCALE;
      gfx->setCursor(PAGE_X+PAGE_W-tw,y); gfx->print(e.state);
    }
    y += lineH+9;
    if (y<=yMax) drawHLine(y-4);
  }
}

bool fetchHA() {
  ha_flags.ready=0; ha_flags.dirty=1;
  ha_nextPollMs=millis()+(g_ha_entities.length()?HA_POLL_TARGETED_MS:HA_POLL_AUTO_MS);
  return fetchHAStates();
}
