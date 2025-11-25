#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>

/* ============================================================================
   STRUTTURE RLE
============================================================================ */
typedef struct {
  uint16_t color;
  uint16_t count;
} RLERun;

struct CosinoRLE {
  const RLERun* data;
  size_t runs;
};

/* ============================================================================
   FUNZIONI GLOBALI
============================================================================ */
String sanitizeText(const String& in);
String htmlSettings(bool saved, const String& msg);

/* ============================================================================
   ENUM PAGINE
============================================================================ */
enum Page {
  P_WEATHER = 0,
  P_AIR,        // 1
  P_CLOCK,      // 2
  P_BINARY,     // 3
  P_CAL,        // 4
  P_BTC,        // 5
  P_QOD,        // 6
  P_INFO,       // 7
  P_COUNT,      // 8
  P_FX,         // 9
  P_T24,        // 10
  P_SUN,        // 11
  P_NEWS,       // 12
  P_HA,         // 13
  P_STELLAR,    // 14
  PAGES
};




/* ============================================================================
   COUNTDOWN
============================================================================ */
struct CDEvent {
  String name;
  String whenISO;
};
extern CDEvent cd[8];

/* ============================================================================
   CONFIG GLOBALI (DEFINITE NEL .INO)
============================================================================ */
extern String g_city;
extern String g_lang;
extern String g_ics;
extern String g_lat;
extern String g_lon;
extern uint32_t PAGE_INTERVAL_MS;

extern String g_from_station;
extern String g_to_station;

extern double g_btc_owned;
extern String g_fiat;

extern String g_oa_key;
extern String g_oa_topic;

extern String g_rss_url;

bool geocodeIfNeeded();
extern String formatShortDate(time_t t);
inline bool isHttpOk(int code) {
  return code >= 200 && code < 300;
}

extern bool jsonFindStringKV(const String& body,
                             const String& key,
                             int from,
                             String& outVal);
extern String g_ha_ip;
extern String g_ha_token;


/* ============================================================================
   VARIABILI GRAFICHE (DEFINITE COME CONST NEL .INO)
============================================================================ */
extern const uint16_t COL_BG;
extern const uint16_t COL_HEADER;
extern const uint16_t COL_TEXT;
extern const uint16_t COL_ACCENT1;
extern const uint16_t COL_ACCENT2;
extern const uint16_t COL_DIVIDER;

extern const int PAGE_X;
extern const int PAGE_Y;
extern const int PAGE_W;
extern const int PAGE_H;

extern const int BASE_CHAR_W;
extern const int BASE_CHAR_H;
extern const int TEXT_SCALE;

extern class Arduino_RGB_Display* gfx;

/* ============================================================================
   PAGINE / ROTAZIONE
============================================================================ */
extern bool g_show[PAGES];
extern int g_page;
extern uint16_t g_air_bg;
extern bool g_timeSynced;
extern bool g_splash_enabled;

uint16_t pagesMaskFromArray();
void pagesArrayFromMask(uint16_t m);
int firstEnabledPage();
bool advanceToNextEnabled();
void ensureCurrentPageEnabled();
void drawCurrentPage();
void pageBinaryClock();

/* ============================================================================
   REFRESH FLAG
============================================================================ */
extern volatile bool g_dataRefreshPending;

/* ============================================================================
   REFRESH DISTRIBUITO (scheduler)
============================================================================ */
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
  R_HA,     // <-- NUOVO STEP Home Assistant
  R_STELLAR,
  R_DONE
};

#endif // GLOBALS_H

