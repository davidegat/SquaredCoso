// Firmware entry point for ESP32-S3 Panel-4848S040 (ST7701 display + GT911 touch).
#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <Arduino_GFX_Library.h>
#include <time.h>
#include <math.h>

const char FW_NAME[] PROGMEM = "SquaredCoso";
const char FW_VERSION[] PROGMEM = "b1.3.0";

int indexOfCI(const String& src, const String& key, int from = 0);

#include "handlers/settingshandler.h"
#include "handlers/displayhelpers.h"
#include "handlers/jsonhelpers.h"
#include "handlers/touch_menu.h"

#include "images/SquaredCoso.h"
#include "images/cosino.h"
#include "images/cosino1.h"
#include "images/cosino2.h"

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

CDEvent cd[8];

uint32_t PAGE_INTERVAL_MS = 15000;
int g_page = 0;
bool g_cycleCompleted = false;

String g_city = F("Bellinzona");
String g_lang = F("it");
String g_ics;
String g_lat;
String g_lon;
String g_tz_rule = F("CET-1CEST,M3.5.0,M10.5.0/3");
uint8_t g_rotation = 0;

String g_rss_url = F("https://feeds.bbci.co.uk/news/rss.xml");
String g_ha_ip;
String g_ha_token;
String g_ha_entities;
String g_oa_key;
String g_oa_topic;

String g_note;

double g_btc_owned = NAN;
String g_fiat = F("CHF");

bool g_show[PAGES] = {
  true, true, true, true, true,
  true, true, true, true, true,
  true, true, true
};

bool g_pageDirty[PAGES] = { false };

volatile bool g_dataRefreshPending = false;
bool g_splash_enabled = true;
bool g_timeSynced = false;

static int8_t lastSecond = -1;

static String urlEncodeComponent(const String &src) {
  static const char hex[] = "0123456789ABCDEF";
  String out; out.reserve(src.length()+8);
  for (int i = 0; i < src.length(); i++) {
    uint8_t c=(uint8_t)src[i];
    if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~') out+=(char)c;
    else if (c==' ') out += F("%20");
    else { out += '%'; out += hex[c>>4]; out += hex[c&15]; }
  }
  return out;
}

bool geocodeIfNeeded() {
  if (g_lat.length() && g_lon.length()) return true;

  String url = F("https://geocoding-api.open-meteo.com/v1/search?count=1&format=json&name=");
  url += urlEncodeComponent(g_city);
  url += F("&language=");
  url += g_lang;

  String body;
  if (!httpGET(url, body, 10000)) return false;
  float lat=NAN, lon=NAN;
  if (!jsonNum(body,"latitude",lat) || !jsonNum(body,"longitude",lon)) return false;
  g_lat=String(lat,6);
  g_lon=String(lon,6);
  saveAppConfig();
  return true;
}

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

const uint16_t COL_BG = 0x1B70;
const uint16_t COL_HEADER = 0x2967;
const uint16_t COL_TEXT = 0xFFFF;
const uint16_t COL_DIVIDER = 0xFFE0;
const uint16_t COL_ACCENT1 = 0xFFFF;
const uint16_t COL_ACCENT2 = 0x07FF;
const uint16_t COL_GOOD = 0x07E0;
const uint16_t COL_WARN = 0xFFE0;
const uint16_t COL_BAD = 0xF800;
uint16_t g_air_bg = COL_BG;

const int HEADER_H = 50;
const int PAGE_X = 16;
const int PAGE_Y = HEADER_H + 12;
const int PAGE_W = 480 - 32;
const int PAGE_H = 480 - PAGE_Y - 16;
const int BASE_CHAR_W = 6;
const int BASE_CHAR_H = 8;
const int TEXT_SCALE = 2;
const int CHAR_H = BASE_CHAR_H * TEXT_SCALE;

#define GFX_BL 38
#define PWM_CHANNEL 0
#define PWM_FREQ 1000
#define PWM_BITS 8

static const char NTP_SERVER[] PROGMEM = "pool.ntp.org";

static bool waitForValidTime(uint16_t timeoutMs = 8000) {
  uint32_t t0 = millis();
  time_t now;
  struct tm info;
  while ((uint32_t)(millis() - t0) < timeoutMs) {
    time(&now);
    localtime_r(&now, &info);
    if (info.tm_year + 1900 > 2020) return true;
    delay(100);
  }
  return false;
}

void syncTimeFromNTP() {
  if (g_timeSynced) return;
  char ntp[20];
  strcpy_P(ntp, NTP_SERVER);
  configTzTime(g_tz_rule.c_str(), ntp);
  g_timeSynced = waitForValidTime(8000);
}

static inline bool deadlineReached(uint32_t now, uint32_t deadline) {
  return (int32_t)(now - deadline) >= 0;
}

Preferences prefs;
DNSServer dnsServer;
WebServer web(80);

String sta_ssid, sta_pass;
String ap_ssid, ap_pass;

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
  gfx->print(F("Se non compare, apri:"));
  gfx->setCursor(16, 138);
  gfx->print(F("http://"));
  gfx->print(WiFi.softAPIP());
  gfx->print(F("/"));
  gfx->setTextSize(TEXT_SCALE);
}

static void startDNSCaptive();
static void startAPPortal();
static void startSTAWeb();

static void startAPWithPortal() {
  uint8_t mac[6];
  WiFi.macAddress(mac);

  char ssidbuf[20];
  snprintf_P(ssidbuf, sizeof(ssidbuf), PSTR("PANEL-%02X%02X"), mac[4], mac[5]);

  ap_ssid = ssidbuf;
  char passbuf[20];
  snprintf_P(passbuf, sizeof(passbuf), PSTR("sq-%02X%02X%02X%02X"),
             mac[2], mac[3], mac[4], mac[5]);
  ap_pass = passbuf;

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAP(ap_ssid.c_str(), ap_pass.c_str());
  delay(100);

  startDNSCaptive();
  startAPPortal();
  drawAPScreenOnce(ap_ssid, ap_pass);
}

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

void applyDisplayRotation() {
  if (!gfx) return;
  gfx->setRotation(g_rotation & 3);
}

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

static uint8_t countEnabledPages() {
  uint8_t c = 0;
  for (uint8_t i = 0; i < (uint8_t)PAGES; i++)
    if (g_show[i]) c++;
  return c;
}

static uint32_t lastRefresh = 0;
static const uint32_t REFRESH_MS = 600000UL;
uint32_t lastPageSwitch = 0;

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

void refreshAll() {
  // Remote sources are fetched one per scheduler step to avoid long blocking chains.
  refreshStep = R_WEATHER;
  refreshDelay = millis();
}

static void redrawDirtyCurrentPage() {
  if (g_page >= 0 && g_page < PAGES && g_pageDirty[g_page]) {
    g_pageDirty[g_page] = false;
    gfx->fillScreen(COL_BG);
    drawCurrentPage();
  }
}

static CosinoRLE pickRandomCosino() {
  switch (random(0, 3)) {
    case 0: return { cosino, sizeof(cosino) / sizeof(RLERun) };
    case 1: return { cosino1, sizeof(cosino1) / sizeof(RLERun) };
    default: return { cosino2, sizeof(cosino2) / sizeof(RLERun) };
  }
}

void setup() {
  // Load rotation before panel startup so splash and touch use the saved orientation.
  loadAppConfig();
  panelKickstart();
  showSplashFadeInOnly(SquaredCoso, SquaredCoso_count, 1200);

  gfx->setTextSize(2);
  gfx->setTextColor(COL_TEXT);
  char verBuf[20];
  strcpy_P(verBuf, FW_VERSION);
  int fwLen = strlen(verBuf) * BASE_CHAR_W * 2;
  gfx->setCursor(480 - fwLen - 20, 480 - BASE_CHAR_H * 2 - 8);
  gfx->print(verBuf);

  if (!tryConnectSTA(8000)) {
    startAPWithPortal();
    return;
  }

  MDNS.begin("squaredcoso");
  touchInit();
  startSTAWeb();
  syncTimeFromNTP();

  g_page = firstEnabledPage();
  if (g_page < 0) g_page = P_CLOCK;
  fadeInUI();
  gfx->fillScreen(COL_BG);
  drawCurrentPage();
  lastPageSwitch = millis();

  g_dataRefreshPending = true;
  lastRefresh = millis();
}

void loop() {
  const uint32_t nowMs = millis();

  if (WiFi.getMode() == WIFI_AP) {
    dnsServer.processNextRequest();
    web.handleClient();
    delay(10);
    return;
  }

  web.handleClient();

  if (WiFi.status() != WL_CONNECTED) {
    static uint32_t nextTry = 0;
    if (deadlineReached(nowMs, nextTry)) {
      nextTry = nowMs + 5000U;
      if (tryConnectSTA(5000)) {
        if (!g_timeSynced) syncTimeFromNTP();
        g_dataRefreshPending = true;
      }
    }
    delay(30);
    return;
  }

  if (g_dataRefreshPending) {
    g_dataRefreshPending = false;
    refreshAll();
  }

  if ((uint32_t)(nowMs - lastRefresh) >= REFRESH_MS && refreshStep == R_DONE) {
    lastRefresh = nowMs;
    refreshStep = R_WEATHER;
    refreshDelay = nowMs;
  }

  if (refreshStep != R_DONE && deadlineReached(nowMs, refreshDelay)) {
    switch (refreshStep) {
      case R_WEATHER:
        if (g_show[P_WEATHER] && fetchWeather()) g_pageDirty[P_WEATHER] = true;
        refreshStep = R_AIR; break;
      case R_AIR:
        if (g_show[P_AIR] && fetchAir()) g_pageDirty[P_AIR] = true;
        refreshStep = R_ICS; break;
      case R_ICS:
        if (g_show[P_CAL] && fetchICS()) g_pageDirty[P_CAL] = true;
        refreshStep = R_BTC; break;
      case R_BTC:
        if (g_show[P_BTC] && fetchCryptoWrapper()) g_pageDirty[P_BTC] = true;
        refreshStep = R_QOD; break;
      case R_QOD:
        if (g_show[P_QOD] && fetchQOD()) g_pageDirty[P_QOD] = true;
        refreshStep = R_FX; break;
      case R_FX:
        if (g_show[P_FX] && fetchFX()) g_pageDirty[P_FX] = true;
        refreshStep = R_T24; break;
      case R_T24:
        if (g_show[P_T24] && fetchTemp24()) g_pageDirty[P_T24] = true;
        refreshStep = R_SUN; break;
      case R_SUN:
        if (g_show[P_SUN] && fetchSun()) g_pageDirty[P_SUN] = true;
        refreshStep = R_NEWS; break;
      case R_NEWS:
        if (g_show[P_NEWS] && fetchNews()) g_pageDirty[P_NEWS] = true;
        refreshStep = R_HA; break;
      case R_HA:
        if (g_show[P_HA] && fetchHA()) g_pageDirty[P_HA] = true;
        refreshStep = R_DONE; break;
      default:
        refreshStep = R_DONE; break;
    }
    refreshDelay = millis() + 100U;
    redrawDirtyCurrentPage();
  }

  time_t now = time(nullptr);
  struct tm ti;
  localtime_r(&now, &ti);
  if (ti.tm_sec != lastSecond) {
    lastSecond = ti.tm_sec;
    if (!touchPaused && g_page == P_CLOCK) {
      tickClock();
    } else if (!touchPaused && g_page == P_BINARY) {
      pageBinaryClock();
    }
  }

  if (!touchPaused && (uint32_t)(millis() - lastPageSwitch) >= PAGE_INTERVAL_MS) {
    const uint8_t enabled = countEnabledPages();
    if (enabled > 1) {
      quickFadeOut();
      int oldPage = g_page;
      bool ok = advanceToNextEnabled();
      if (!ok || g_page <= oldPage) g_cycleCompleted = true;

      if (g_cycleCompleted) {
        if (g_splash_enabled) {
          CosinoRLE c = pickRandomCosino();
          showCycleSplash(c.data, c.runs, 900);
          splashFadeOut();
        }
        int first = firstEnabledPage();
        g_page = (first < 0) ? P_CLOCK : first;
        g_cycleCompleted = false;
        drawCurrentPage();
        fadeInUI();
      } else {
        drawCurrentPage();
        quickFadeIn();
      }
    }
    lastPageSwitch = millis();
  }

  touchLoop();
  if (touchPaused) {
    lastPageSwitch = millis();
    delay(5);
    return;
  }

  switch (g_page) {
    case P_HA:
      tickHA();
      pageHA();
      break;
    case P_WEATHER:
      pageWeatherParticlesTick();
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
    default:
      break;
  }

  redrawDirtyCurrentPage();
  delay(5);
}
