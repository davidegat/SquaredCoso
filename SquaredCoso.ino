/*
   ============================================================================
   SquaredCoso – Gat Multi Ticker (ESP32-S3 + ST7701 480x480 RGB panel)
   ============================================================================
   Autore:     Davide "gat"
   Repository: https://github.com/davidegat/SquaredCoso
   Licenza:    Creative Commons BY-NC 4.0
*/

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Arduino_GFX_Library.h>
#include <time.h>
#include <math.h>

// =============================================================================
// FIRMWARE INFO
// =============================================================================
const char FW_NAME[] PROGMEM = "SquaredCoso";
const char FW_VERSION[] PROGMEM = "b1.2.0";

int indexOfCI(const String& src, const String& key, int from = 0);

// handlers
#include "handlers/settingshandler.h"
#include "handlers/displayhelpers.h"
#include "handlers/jsonhelpers.h"
#include "handlers/touch_menu.h"

// immagini
#include "images/SquaredCoso.h"
#include "images/cosino.h"
#include "images/cosino1.h"
#include "images/cosino2.h"

// pagine
#include "pages/SquaredCount.h"
#include "pages/SquaredCal.h"
#include "pages/SquaredMeteo.h"
#include "pages/SquaredTemp.h"
#include "pages/SquaredClock.h"
#include "pages/SquaredSay.h"
#include "pages/SquaredInfo.h"
#include "pages/SquaredLight.h"
#include "pages/SquaredCHF.h"
#include "pages/SquaredAir.h"
#include "pages/SquaredCrypto.h"
#include "pages/SquaredNews.h"
#include "pages/SquaredHA.h"
#include "pages/SquaredStellar.h"
#include "pages/SquaredBinary.h"
#include "pages/SquaredNotes.h"
#include "pages/SquaredChronos.h"

// =============================================================================
// CONFIG APPLICAZIONE
// =============================================================================

// --- Countdown ---------------------------------------------------------------
CDEvent cd[8];

// --- Rotazione pagine --------------------------------------------------------
uint32_t PAGE_INTERVAL_MS = 15000;
int g_page = 0;
bool g_cycleCompleted = false;

// --- Localizzazione ----------------------------------------------------------
String g_city = F("Bellinzona");
String g_lang = F("it");
String g_ics;
String g_lat;
String g_lon;

// --- API & integrazioni ------------------------------------------------------
String g_rss_url = F("https://feeds.bbci.co.uk/news/rss.xml");
String g_ha_ip;
String g_ha_token;
String g_oa_key;
String g_oa_topic;

// --- Note --------------------------------------------------------------------
String g_note;

// --- Bitcoin -----------------------------------------------------------------
double g_btc_owned = NAN;
String g_fiat = F("CHF");

// --- Stato pagine ------------------------------------------------------------
bool g_show[PAGES] = {
  true, true, true, true, true,
  true, true, true, true, true,
  true, true, true
};

bool g_pageDirty[PAGES] = { false };

// --- Flag runtime (extern in altri file - DEVONO essere bool normali) --------
volatile bool g_dataRefreshPending = false;
bool g_splash_enabled = true;
bool g_timeSynced = false;

// --- Solo interni (non usati altrove) -----------------------------------
static bool g_bootPhase = true;
static int8_t lastSecond = -1;

// =============================================================================
// GEOCODING OPEN-METEO
// =============================================================================
bool geocodeIfNeeded() {
  if (g_lat.length() && g_lon.length()) return true;

  String url = F("https://geocoding-api.open-meteo.com/v1/search?count=1&format=json&name=");
  url += g_city;
  url += F("&language=");
  url += g_lang;

  String body;
  if (!httpGET(url, body, 10000)) return false;

  int p = indexOfCI(body, F("\"latitude\""), 0);
  if (p < 0) return false;
  int c = body.indexOf(':', p);
  int e = body.indexOf(',', c + 1);
  g_lat = sanitizeText(body.substring(c + 1, e));

  p = indexOfCI(body, F("\"longitude\""), 0);
  if (p < 0) return false;
  c = body.indexOf(':', p);
  e = body.indexOf(',', c + 1);
  g_lon = sanitizeText(body.substring(c + 1, e));

  if (!g_lat.length() || !g_lon.length()) return false;

  saveAppConfig();
  return true;
}

// =============================================================================
// BUS GFX / PANNELLO RGB ST7701
// =============================================================================
Arduino_DataBus* bus = new Arduino_SWSPI(
  GFX_NOT_DEFINED, 39, 48, 47, GFX_NOT_DEFINED);

Arduino_ESP32RGBPanel* rgbpanel = new Arduino_ESP32RGBPanel(
  18, 17, 16, 21,
  11, 12, 13, 14, 0,
  8, 20, 3, 46, 9, 10,
  4, 5, 6, 7, 15,
  1, 10, 8, 50,
  1, 10, 8, 20,
  0, 12000000, false, 0, 0, 0);

Arduino_RGB_Display* gfx = new Arduino_RGB_Display(
  480, 480, rgbpanel, 0, true, bus, GFX_NOT_DEFINED,
  st7701_type9_init_operations, sizeof(st7701_type9_init_operations));

// =============================================================================
// TEMA COLORI
// =============================================================================
const uint16_t COL_BG = 0x1B70;
const uint16_t COL_HEADER = 0x2967;
const uint16_t COL_TEXT = 0xFFFF;
const uint16_t COL_SUBTEXT = 0xC618;
const uint16_t COL_DIVIDER = 0xFFE0;
const uint16_t COL_ACCENT1 = 0xFFFF;
const uint16_t COL_ACCENT2 = 0x07FF;
const uint16_t COL_GOOD = 0x07E0;
const uint16_t COL_WARN = 0xFFE0;
const uint16_t COL_BAD = 0xF800;
uint16_t g_air_bg = COL_BG;

// =============================================================================
// LAYOUT
// =============================================================================
const int HEADER_H = 50;
const int PAGE_X = 16;
const int PAGE_Y = HEADER_H + 12;
const int PAGE_W = 480 - 32;
const int PAGE_H = 480 - PAGE_Y - 16;
const int BASE_CHAR_W = 6;
const int BASE_CHAR_H = 8;
const int TEXT_SCALE = 2;
const int CHAR_H = BASE_CHAR_H * TEXT_SCALE;

// =============================================================================
// BACKLIGHT
// =============================================================================
#define GFX_BL 38
#define PWM_CHANNEL 0
#define PWM_FREQ 1000
#define PWM_BITS 8

// =============================================================================
// NTP
// =============================================================================
static const char NTP_SERVER[] PROGMEM = "pool.ntp.org";
static const long GMT_OFFSET_SEC = 3600;
static const int DAYLIGHT_OFFSET_SEC = 3600;

static bool waitForValidTime(uint16_t timeoutMs = 8000) {
  uint32_t t0 = millis();
  time_t now;
  struct tm info;

  while ((millis() - t0) < timeoutMs) {
    time(&now);
    localtime_r(&now, &info);
    if (info.tm_year + 1900 > 2020) return true;
    delay(100);
  }
  return false;
}

void syncTimeFromNTP() {
  if (g_timeSynced) return;
  char buf[20];
  strcpy_P(buf, NTP_SERVER);
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, buf);
  g_timeSynced = waitForValidTime(8000);
}

// =============================================================================
// NVS / WEB / DNS
// =============================================================================
Preferences prefs;
DNSServer dnsServer;
WebServer web(80);

String sta_ssid, sta_pass;
String ap_ssid, ap_pass;

const byte DNS_PORT = 53;

// =============================================================================
// SCHERMATA AP CONFIG
// =============================================================================
static void drawAPScreenOnce(const String& ssid, const String& pass) {
  gfx->fillScreen(COL_BG);
  drawBoldTextColored(16, 36, F("Connettiti all'AP:"), COL_TEXT, COL_BG);
  drawBoldTextColored(16, 66, ssid, COL_TEXT, COL_BG);

  gfx->setTextSize(1);
  gfx->setTextColor(COL_TEXT, COL_BG);
  gfx->setCursor(16, 96);
  gfx->print(F("Password: "));
  gfx->print(pass);
  gfx->setCursor(16, 114);
  gfx->print(F("Captive portal automatico."));
  gfx->setCursor(16, 126);
  gfx->print(F("Se non compare, apri l'IP dell'AP."));
  gfx->setTextSize(TEXT_SCALE);
}

// =============================================================================
// ACCESS-POINT + CAPTIVE PORTAL
// =============================================================================
static void startAPWithPortal() {
  uint8_t mac[6];
  WiFi.macAddress(mac);

  char ssidbuf[20];
  snprintf_P(ssidbuf, sizeof(ssidbuf), PSTR("PANEL-%02X%02X"), mac[4], mac[5]);

  ap_ssid = ssidbuf;
  ap_pass = F("panelsetup");

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAP(ap_ssid.c_str(), ap_pass.c_str());
  delay(100);

  startDNSCaptive();
  startAPPortal();
  drawAPScreenOnce(ap_ssid, ap_pass);
}

// =============================================================================
// CONNESSIONE STA
// =============================================================================
static bool tryConnectSTA(uint16_t timeoutMs = 8000) {
  prefs.begin("wifi", true);
  sta_ssid = prefs.getString("ssid", "");
  sta_pass = prefs.getString("pass", "");
  prefs.end();

  if (sta_ssid.isEmpty()) return false;

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < timeoutMs) {
    delay(100);
    yield();
  }

  return WiFi.status() == WL_CONNECTED;
}

bool httpGET(const String& url, String& body, uint32_t timeoutMs = 10000);

// =============================================================================
// SOFT REBOOT
// =============================================================================
static void showSettingsOK() {
  gfx->fillScreen(COL_BG);
  gfx->setTextSize(3);
  gfx->setTextColor(COL_TEXT, COL_BG);
  gfx->setCursor(150, 225);
  gfx->print(F("Updating..."));
}

void softReboot() {
  showSettingsOK();
  delay(600);
  quickFadeOut();

  ensureCurrentPageEnabled();
  g_page = firstEnabledPage();
  lastPageSwitch = millis();
  g_dataRefreshPending = true;

  gfx->fillScreen(COL_BG);
  drawCurrentPage();
  quickFadeIn();
}

// =============================================================================
// HELPER PAGINE
// =============================================================================
static uint8_t countEnabledPages() {
  uint8_t c = 0;
  for (uint8_t i = 0; i < (uint8_t)PAGES; i++)
    if (g_show[i]) c++;
  return c;
}

// =============================================================================
// REFRESH TIMING
// =============================================================================
static uint32_t lastRefresh = 0;
static const uint32_t REFRESH_MS = 600000UL;
uint32_t lastPageSwitch = 0;

// =============================================================================
// SCHEDULER REFRESH
// =============================================================================
RefreshStep refreshStep = R_DONE;
static uint32_t refreshDelay = 0;

void pageCountdowns();

void drawCurrentPage() {
  ensureCurrentPageEnabled();
  gfx->fillScreen(COL_BG);

  switch (g_page) {
    case P_WEATHER: pageWeather(); break;
    case P_AIR: pageAir(); break;
    case P_CLOCK: pageClock(); break;
    case P_BINARY:
      resetBinaryClockFirstDraw();
      pageBinaryClock();
      break;
    case P_CAL: pageCalendar(); break;
    case P_BTC: pageCryptoWrapper(); break;
    case P_QOD: pageQOD(); break;
    case P_INFO: pageInfo(); break;
    case P_COUNT: pageCountdowns(); break;
    case P_FX: pageFX(); break;
    case P_T24: pageTemp24(); break;
    case P_SUN: pageSun(); break;
    case P_NEWS: pageNews(); break;
    case P_HA: pageHA(); break;
    case P_STELLAR: pageStellar(); break;
    case P_NOTES: pageNotes(); break;
    case P_CHRONOS: pageChronos(); break;
  }
}

// =============================================================================
// REFRESH DATI
// =============================================================================
void refreshAll() {
  if (g_show[P_WEATHER]) {
    fetchWeather();
    g_pageDirty[P_WEATHER] = true;
  }

  if (g_show[P_AIR] && fetchAir())
    g_pageDirty[P_AIR] = true;

  fetchICS();
  g_pageDirty[P_CAL] = true;

  if (g_show[P_BTC] && fetchCryptoWrapper())
    g_pageDirty[P_BTC] = true;

  if (g_show[P_QOD] && fetchQOD())
    g_pageDirty[P_QOD] = true;

  if (g_show[P_FX] && fetchFX())
    g_pageDirty[P_FX] = true;

  if (g_show[P_T24] && fetchTemp24())
    g_pageDirty[P_T24] = true;

  if (g_show[P_SUN] && fetchSun())
    g_pageDirty[P_SUN] = true;

  if (g_show[P_NEWS] && fetchNews())
    g_pageDirty[P_NEWS] = true;

  if (g_show[P_HA] && fetchHA())
    g_pageDirty[P_HA] = true;

  if (g_bootPhase) {
    gfx->fillScreen(COL_BG);
    drawCurrentPage();
    lastPageSwitch = millis();
    return;
  }

  if (g_pageDirty[g_page]) {
    g_pageDirty[g_page] = false;
    gfx->fillScreen(COL_BG);
    drawCurrentPage();
    lastPageSwitch = millis();
  }
}

// =============================================================================
// COSINO RANDOM
// =============================================================================
static CosinoRLE pickRandomCosino() {
  switch (random(0, 3)) {
    case 0: return { cosino, sizeof(cosino) / sizeof(RLERun) };
    case 1: return { cosino1, sizeof(cosino1) / sizeof(RLERun) };
    default: return { cosino2, sizeof(cosino2) / sizeof(RLERun) };
  }
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  panelKickstart();
  showSplashFadeInOnly(SquaredCoso, SquaredCoso_count, 2000);

  // Versione firmware
  gfx->setTextSize(2);
  gfx->setTextColor(COL_TEXT);

  char verBuf[12];
  strcpy_P(verBuf, FW_VERSION);
  int fwLen = strlen(verBuf) * BASE_CHAR_W * 2;
  gfx->setCursor(480 - fwLen - 38, 480 - BASE_CHAR_H * 2 - 8);
  gfx->print(verBuf);

  loadAppConfig();
  touchInit();

  if (!tryConnectSTA(8000)) {
    startAPWithPortal();
  } else {
    startSTAWeb();
    syncTimeFromNTP();
    g_dataRefreshPending = true;
  }

  refreshAll();
  g_pageDirty[g_page] = true;

  if (countEnabledPages() == 1) {
    g_bootPhase = false;
    g_page = firstEnabledPage();
    fadeInUI();
    gfx->fillScreen(COL_BG);
    drawCurrentPage();
    lastPageSwitch = millis();
    return;
  }

  fadeInUI();
  g_bootPhase = false;
  gfx->fillScreen(COL_BG);
  drawCurrentPage();
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  // AP mode
  if (WiFi.getMode() == WIFI_AP) {
    dnsServer.processNextRequest();
    web.handleClient();
    delay(10);
    return;
  }

  web.handleClient();

  // Riconnessione WiFi
  if (WiFi.status() != WL_CONNECTED) {
    static uint32_t nextTry = 0;
    if (millis() > nextTry) {
      nextTry = millis() + 5000;
      if (tryConnectSTA(5000)) {
        if (!g_timeSynced) syncTimeFromNTP();
        g_dataRefreshPending = true;
      }
    }
    delay(30);
    return;
  }

  // Refresh immediato
  if (g_dataRefreshPending) {
    g_dataRefreshPending = false;
    refreshAll();
  }

  // Scheduler refresh
  if (millis() - lastRefresh >= REFRESH_MS) {
    lastRefresh = millis();
    refreshStep = R_WEATHER;
    refreshDelay = millis() + 200;
  }

  // Step refresh distribuito
  if (refreshStep != R_DONE && millis() > refreshDelay) {
    switch (refreshStep) {
      case R_WEATHER:
        if (g_show[P_WEATHER]) {
          fetchWeather();
          g_pageDirty[P_WEATHER] = true;
        }
        refreshStep = R_AIR;
        break;
      case R_AIR:
        if (g_show[P_AIR] && fetchAir()) g_pageDirty[P_AIR] = true;
        refreshStep = R_ICS;
        break;
      case R_ICS:
        fetchICS();
        g_pageDirty[P_CAL] = true;
        refreshStep = R_BTC;
        break;
      case R_BTC:
        if (g_show[P_BTC] && fetchCryptoWrapper()) g_pageDirty[P_BTC] = true;
        refreshStep = R_QOD;
        break;
      case R_QOD:
        if (g_show[P_QOD] && fetchQOD()) g_pageDirty[P_QOD] = true;
        refreshStep = R_FX;
        break;
      case R_FX:
        if (g_show[P_FX] && fetchFX()) g_pageDirty[P_FX] = true;
        refreshStep = R_T24;
        break;
      case R_T24:
        if (g_show[P_T24] && fetchTemp24()) g_pageDirty[P_T24] = true;
        refreshStep = R_SUN;
        break;
      case R_SUN:
        if (g_show[P_SUN] && fetchSun()) g_pageDirty[P_SUN] = true;
        refreshStep = R_NEWS;
        break;
      case R_NEWS:
        if (g_show[P_NEWS] && fetchNews()) g_pageDirty[P_NEWS] = true;
        refreshStep = R_HA;
        break;
      case R_HA:
        if (g_show[P_HA] && fetchHA()) g_pageDirty[P_HA] = true;
        refreshStep = R_DONE;
        break;
      default:
        break;
    }
    refreshDelay = millis() + 200;
  }

  // Rotazione pagine
  if (!touchPaused && millis() - lastPageSwitch >= PAGE_INTERVAL_MS) {
    uint8_t enabled = countEnabledPages();

    if (enabled <= 1) {
      time_t now = time(nullptr);
      struct tm ti;
      localtime_r(&now, &ti);

      if (ti.tm_sec != lastSecond) {
        lastSecond = ti.tm_sec;
        if (g_page == P_CLOCK) g_pageDirty[g_page] = true;
        if (g_page == P_BINARY) pageBinaryClock();
      }

      if (g_pageDirty[g_page]) {
        g_pageDirty[g_page] = false;
        gfx->fillScreen(COL_BG);
        drawCurrentPage();
      }

      lastPageSwitch = millis();
      return;
    }

    quickFadeOut();
    int oldPage = g_page;
    bool ok = advanceToNextEnabled();

    if (ok && g_page == P_T24 && oldPage != P_T24)
      resetTemp24Anim();

    if (!ok || g_page <= oldPage)
      g_cycleCompleted = true;

    if (g_cycleCompleted) {
      if (g_splash_enabled) {
        CosinoRLE c = pickRandomCosino();
        showCycleSplash(c.data, c.runs, 1500);
        splashFadeOut();
      }

      int first = firstEnabledPage();
      g_page = (first < 0) ? P_CLOCK : first;
      g_cycleCompleted = false;

      drawCurrentPage();
      fadeInUI();
      lastPageSwitch = millis();
      return;
    }

    drawCurrentPage();
    quickFadeIn();
    lastPageSwitch = millis();
  }

  // Touch
  touchLoop();

  if (touchPaused) {
    lastPageSwitch = millis();
    delay(5);
    return;
  }

  // Animazioni pagina corrente
  switch (g_page) {
    case P_HA:
      tickHA();
      pageHA();
      break;

    case P_WEATHER:
      pageWeatherParticlesTick();
      if (g_pageDirty[P_WEATHER]) {
        g_pageDirty[P_WEATHER] = false;
        gfx->fillScreen(COL_BG);
        pageWeather();
      }
      break;

    case P_AIR:
      tickLeaves(g_air_bg);
      break;

    case P_FX:
      tickFXDataStream(COL_BG);
      break;

    case P_COUNT:
      tickCountdownSnake();
      break;

    case P_STELLAR:
      tickStellar();
      break;

    case P_T24:
      {
        int old = temp24_progress;
        tickTemp24Anim();
        if (temp24_progress != old) {
          gfx->fillScreen(COL_BG);
          pageTemp24();
        }
        break;
      }

    default:
      break;
  }

  delay(5);
}