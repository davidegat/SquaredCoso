#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>

typedef struct {
  uint16_t color;
  uint16_t count;
} RLERun;

struct CosinoRLE {
  const RLERun *data;
  size_t runs;
};

String sanitizeText(const String &in);
String htmlSettings(bool saved, const String &msg);
extern String g_note;

enum Page {
  P_WEATHER = 0,
  P_AIR,
  P_CLOCK,
  P_BINARY,
  P_CAL,
  P_BTC,
  P_QOD,
  P_INFO,
  P_COUNT,
  P_FX,
  P_T24,
  P_SUN,
  P_NEWS,
  P_HA,
  P_STELLAR,
  P_NOTES,
  P_CHRONOS,
  PAGES
};

struct CDEvent {
  String name;
  String whenISO;
};
extern CDEvent cd[8];

extern String g_city;
extern String g_lang;
extern String g_ics;
extern String g_lat;
extern String g_lon;
extern String g_tz_rule;
extern uint8_t g_rotation;
extern uint32_t PAGE_INTERVAL_MS;

extern double g_btc_owned;
extern String g_fiat;

extern String g_oa_key;
extern String g_oa_topic;

extern String g_rss_url;

bool geocodeIfNeeded();
extern String formatShortDate(time_t t);
inline bool isHttpOk(int code) { return code >= 200 && code < 300; }

extern bool jsonFindStringKV(const String &body, const String &key, int from,
                             String &outVal);
extern String g_ha_ip;
extern String g_ha_token;
extern String g_ha_entities;

void softReboot();
void applyDisplayRotation();

extern const uint16_t COL_BG;
extern const uint16_t COL_HEADER;
extern const uint16_t COL_TEXT;
extern const uint16_t COL_ACCENT1;
extern const uint16_t COL_ACCENT2;
extern const uint16_t COL_DIVIDER;
extern const int HEADER_H;

extern const int PAGE_X;
extern const int PAGE_Y;
extern const int PAGE_W;
extern const int PAGE_H;

extern const int BASE_CHAR_W;
extern const int BASE_CHAR_H;
extern const int TEXT_SCALE;

extern class Arduino_RGB_Display *gfx;

extern bool g_show[PAGES];
extern int g_page;
extern uint16_t g_air_bg;
extern bool g_timeSynced;
extern bool g_splash_enabled;

uint32_t pagesMaskFromArray();
void pagesArrayFromMask(uint32_t mask);
int firstEnabledPage();
bool advanceToNextEnabled();
void ensureCurrentPageEnabled();
void drawCurrentPage();
void pageBinaryClock();
void pageChronos();

extern volatile bool g_dataRefreshPending;

enum RefreshStep {
  R_WEATHER,
  R_AIR,
  R_ICS,
  R_BTC,
  R_QOD,
  R_FX,
  R_T24,
  R_SUN,
  R_NEWS,
  R_HA,
  R_DONE
};

#endif
