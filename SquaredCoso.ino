/*
   ============================================================================
   SquaredCoso – Gat Multi Ticker (ESP32-S3 + ST7701 480x480 RGB panel)
   ============================================================================
   Autore:     Davide "gat"
   Repository: https://github.com/davidegat/SquaredCoso

   DESCRIZIONE
   -----------
   Piattaforma informativa modulare per pannello 480x480 basato su ESP32-S3:
   rotazione automatica di pagine meteo, aria, orologio, calendario ICS,
   Bitcoin, cambi valutari, countdown multipli, ore di luce, RSS news,
   Quote Of the Day e info di sistema.

   Questo file è il "core" del firmware:
   - inizializza il pannello RGB ST7701 (Panel-4848S040)
   - gestisce Wi-Fi, NTP e WebUI (AP/STA + captive portal)
   - carica/salva configurazioni da NVS
   - orchestra il fetch periodico dei dati remoti
   - controlla la rotazione delle pagine e le transizioni fade/splash

   LICENZA
   -------
   Creative Commons BY-NC 4.0
   - Uso consentito solo non-commerciale
   - Ridistribuzione ammessa con attribuzione a "davidegat"
   - Modifiche consentite mantenendo la stessa licenza
   - Il codice derivato da questo progetto deve mantenere riferimento
     al repository ufficiale SquaredCoso
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

int indexOfCI(const String& src, const String& key, int from = 0);

// handler di base (config, UI, layout, helpers)
#include "handlers/globals.h"
#include "handlers/settingshandler.h"
#include "handlers/displayhelpers.h"

// immagini e cicli RLE
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

// -----------------------------------------------------------------------------
// CONFIG APPLICAZIONE
// -----------------------------------------------------------------------------
String g_rss_url = "https://feeds.bbci.co.uk/news/rss.xml";
String g_city = "Bellinzona";
String g_lang = "it";
String g_ics = "";
String g_lat = "";
String g_lon = "";
uint32_t PAGE_INTERVAL_MS = 15000;
String g_ha_ip = "";
String g_ha_token = "";

CDEvent cd[8];

String g_oa_key = "";
String g_oa_topic = "";

int g_page = 0;
bool g_cycleCompleted = false;

bool g_show[PAGES] = {
  true, true, true, true, true,
  true, true, true, true, true,
  true, true, true
};
bool g_pageDirty[PAGES] = { false };
static int lastSecond = -1;

double g_btc_owned = NAN;
String g_fiat = "CHF";

volatile bool g_dataRefreshPending = false;
bool g_splash_enabled = true;

// -----------------------------------------------------------------------------
// GEOCODING OPEN-METEO (auto-lat/lon se mancano in config)
// -----------------------------------------------------------------------------
bool geocodeIfNeeded() {
  if (g_lat.length() && g_lon.length()) return true;

  String url =
    "https://geocoding-api.open-meteo.com/v1/search?count=1&format=json"
    "&name="
    + g_city + "&language=" + g_lang;

  String body;
  if (!httpGET(url, body, 10000)) return false;

  int p = indexOfCI(body, "\"latitude\"", 0);
  if (p < 0) return false;
  int c = body.indexOf(':', p);
  int e = body.indexOf(',', c + 1);
  g_lat = sanitizeText(body.substring(c + 1, e));

  p = indexOfCI(body, "\"longitude\"", 0);
  if (p < 0) return false;
  c = body.indexOf(':', p);
  e = body.indexOf(',', c + 1);
  g_lon = sanitizeText(body.substring(c + 1, e));

  if (!g_lat.length() || !g_lon.length()) return false;

  saveAppConfig();
  return true;
}

// -----------------------------------------------------------------------------
// BUS GFX / PANNELLO RGB ST7701
// -----------------------------------------------------------------------------
Arduino_DataBus* bus = new Arduino_SWSPI(
  GFX_NOT_DEFINED,
  39,
  48,
  47,
  GFX_NOT_DEFINED);

Arduino_ESP32RGBPanel* rgbpanel = new Arduino_ESP32RGBPanel(
  18, 17, 16, 21,       // DE, VSYNC, HSYNC, PCLK
  11, 12, 13, 14, 0,    // R0..R4
  8, 20, 3, 46, 9, 10,  // G0..G5
  4, 5, 6, 7, 15,       // B0..B4
  1, 10, 8, 50,         // HSYNC: pol, fp, pw, bp
  1, 10, 8, 20,         // VSYNC: pol, fp, pw, bp
  0,
  12000000,
  false, 0, 0, 0);

Arduino_RGB_Display* gfx = new Arduino_RGB_Display(
  480, 480,
  rgbpanel, 0, true,
  bus, GFX_NOT_DEFINED,
  st7701_type9_init_operations, sizeof(st7701_type9_init_operations));

// -----------------------------------------------------------------------------
// TEMA COLORE PRINCIPALE (Aurora / Neon dark)
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// LAYOUT TESTO / AREA PAGINE
// -----------------------------------------------------------------------------
static const int HEADER_H = 50;
const int PAGE_X = 16;
const int PAGE_Y = HEADER_H + 12;
const int PAGE_W = 480 - 32;
const int PAGE_H = 480 - PAGE_Y - 16;
const int BASE_CHAR_W = 6;
const int BASE_CHAR_H = 8;
const int TEXT_SCALE = 2;
const int CHAR_H = BASE_CHAR_H * TEXT_SCALE;

// -----------------------------------------------------------------------------
// BACKLIGHT PWM (gestito da displayhelpers, qui solo pin/canale)
// -----------------------------------------------------------------------------
#define GFX_BL 38
#define PWM_CHANNEL 0
#define PWM_FREQ 1000
#define PWM_BITS 8

// -----------------------------------------------------------------------------
// NTP / ORA LOCALE
// -----------------------------------------------------------------------------
static const char* NTP_SERVER = "pool.ntp.org";
static const long GMT_OFFSET_SEC = 3600;
static const int DAYLIGHT_OFFSET_SEC = 3600;

bool g_timeSynced = false;

static bool waitForValidTime(uint32_t timeoutMs = 8000) {
  uint32_t t0 = millis();
  time_t now = 0;
  struct tm info;

  while ((millis() - t0) < timeoutMs) {
    time(&now);
    localtime_r(&now, &info);
    if (info.tm_year + 1900 > 2020) return true;
    delay(100);
  }
  return false;
}

static void syncTimeFromNTP() {
  if (g_timeSynced) return;
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  g_timeSynced = waitForValidTime(8000);
}

// -----------------------------------------------------------------------------
// NVS / WEB / DNS
// -----------------------------------------------------------------------------
Preferences prefs;
DNSServer dnsServer;
WebServer web(80);

String sta_ssid, sta_pass;
String ap_ssid, ap_pass;

const byte DNS_PORT = 53;

// -----------------------------------------------------------------------------
// SCHERMATA INFO AP CONFIG (quando parte il captive portal)
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// AVVIO ACCESS-POINT + DNS CAPTIVE + WebUI setup
// -----------------------------------------------------------------------------
static void startAPWithPortal() {
  uint8_t mac[6];
  WiFi.macAddress(mac);

  char ssidbuf[32];
  snprintf(ssidbuf, sizeof(ssidbuf), "PANEL-%02X%02X", mac[4], mac[5]);

  ap_ssid = ssidbuf;
  ap_pass = "panelsetup";

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAP(ap_ssid.c_str(), ap_pass.c_str());
  delay(100);

  startDNSCaptive();
  startAPPortal();
  drawAPScreenOnce(ap_ssid, ap_pass);
}

// -----------------------------------------------------------------------------
// TENTATIVO CONNESSIONE STA (SSID salvato in NVS)
// -----------------------------------------------------------------------------
static bool tryConnectSTA(uint32_t timeoutMs = 8000) {
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


// -----------------------------------------------------------------------------
// REFRESH DATI PERIODICO
// -----------------------------------------------------------------------------
static int countEnabledPages() {
  int c = 0;
  for (int i = 0; i < PAGES; i++)
    if (g_show[i]) c++;
  return c;
}

static uint32_t lastRefresh = 0;
static const uint32_t REFRESH_MS = 10UL * 60UL * 1000UL;
static uint32_t lastPageSwitch = 0;

// -----------------------------------------------------------------------------
// SCHEDULER REFRESH (distribuisce i fetch nel tempo)
// -----------------------------------------------------------------------------
RefreshStep refreshStep = R_DONE;
uint32_t refreshDelay = 0;

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
  }
}

static bool g_bootPhase = true;
static bool force_page_full_redraw = false;

// -----------------------------------------------------------------------------
// REFRESH DI TUTTI I DATI (meteo, aria, BTC, FX, QOD, T24, SUN, NEWS, ICS)
// -----------------------------------------------------------------------------
void refreshAll() {

  // METEO
  if (g_show[P_WEATHER]) {
    fetchWeather();                 // sempre
    g_pageDirty[P_WEATHER] = true;  // sempre
  }

  // ARIA
  if (g_show[P_AIR] && fetchAir())
    g_pageDirty[P_AIR] = true;

  // CALENDARIO (ICS cambia sempre → sempre dirty)
  fetchICS();
  g_pageDirty[P_CAL] = true;

  // BTC
  if (g_show[P_BTC] && fetchCryptoWrapper())
    g_pageDirty[P_BTC] = true;

  // QOD
  if (g_show[P_QOD] && fetchQOD())
    g_pageDirty[P_QOD] = true;

  // FX
  if (g_show[P_FX] && fetchFX())
    g_pageDirty[P_FX] = true;

  // T24 (grafico)
  if (g_show[P_T24] && fetchTemp24())
    g_pageDirty[P_T24] = true;

  // SUN (alba/tramonto)
  if (g_show[P_SUN] && fetchSun())
    g_pageDirty[P_SUN] = true;

  // NEWS
  if (g_show[P_NEWS] && fetchNews())
    g_pageDirty[P_NEWS] = true;

  // HOME ASSISTANT
  if (g_show[P_HA] && fetchHA())
    g_pageDirty[P_HA] = true;

  // Durante il boot evita di ridisegnare inutilmente
  // se siamo nel boot, dopo refresh dobbiamo SEMPRE ridisegnare la pagina corrente
  if (g_bootPhase) {
    gfx->fillScreen(COL_BG);
    drawCurrentPage();
    lastPageSwitch = millis();
    return;
  }

  // fuori dal boot, ridisegniamo solo se la pagina corrente è diventata dirty
  if (g_pageDirty[g_page]) {
    g_pageDirty[g_page] = false;
    gfx->fillScreen(COL_BG);
    drawCurrentPage();
    lastPageSwitch = millis();
  }
}

// -----------------------------------------------------------------------------
// SELEZIONE RLE "COSINO" RANDOM PER SPLASH DI CICLO
// -----------------------------------------------------------------------------
static CosinoRLE pickRandomCosino() {
  int r = random(0, 3);

  switch (r) {
    case 0: return { cosino, sizeof(cosino) / sizeof(RLERun) };
    case 1: return { cosino1, sizeof(cosino1) / sizeof(RLERun) };
    case 2: return { cosino2, sizeof(cosino2) / sizeof(RLERun) };
  }
  return { cosino, sizeof(cosino) / sizeof(RLERun) };
}

void setup() {
  panelKickstart();

  // splash iniziale (fade-in)
  showSplashFadeInOnly(SquaredCoso, SquaredCoso_count, 2000);

  loadAppConfig();

  if (!tryConnectSTA(8000)) {
    startAPWithPortal();
  } else {
    startSTAWeb();
    syncTimeFromNTP();
    g_dataRefreshPending = true;
  }

  refreshAll();

  // ---- se esiste UNA sola pagina attiva → niente splashFadeOut ----
  if (countEnabledPages() == 1) {
    g_bootPhase = false;

    g_page = firstEnabledPage();
    gfx->fillScreen(COL_BG);
    drawCurrentPage();

    lastPageSwitch = millis();
    return;  // << STOP QUI, nessun fade-out deve eseguire
  }

  // ---- 2+ pagine → comportamento normale ----
  splashFadeOut();
  g_bootPhase = false;

  drawCurrentPage();
  fadeInUI();
}

// -----------------------------------------------------------------------------
// LOOP PRINCIPALE: Wi-Fi, refresh dati, rotazione pagine, FX animazioni
// -----------------------------------------------------------------------------
void loop() {

  // ---------------------------------------------------------------------------
  // MODALITÀ AP (captive portal attivo)
  // ---------------------------------------------------------------------------
  if (WiFi.getMode() == WIFI_AP) {
    dnsServer.processNextRequest();
    web.handleClient();
    delay(10);
    return;
  }

  // ---------------------------------------------------------------------------
  // Web server (STA mode)
  // ---------------------------------------------------------------------------
  web.handleClient();

  // ---------------------------------------------------------------------------
  // GESTIONE RICONNESSIONE Wi-Fi
  // ---------------------------------------------------------------------------
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

  // ---------------------------------------------------------------------------
  // REFRESH IMMEDIATO A RICHIESTA (es. dopo connessione Wi-Fi)
  // ---------------------------------------------------------------------------
  if (g_dataRefreshPending) {
    g_dataRefreshPending = false;
    refreshAll();
  }

  // ---------------------------------------------------------------------------
  // SCHEDULER: INIZIO CICLO DI REFRESH DISTRIBUITO
  // ---------------------------------------------------------------------------
  if (millis() - lastRefresh >= REFRESH_MS) {
    lastRefresh = millis();
    refreshStep = R_WEATHER;
    refreshDelay = millis() + 200;
  }

  // ---------------------------------------------------------------------------
  // SCHEDULER: ESECUZIONE STEP ATTUALE (uno per volta)
  // ---------------------------------------------------------------------------
  // ---------------------------------------------------------------------------
  // SCHEDULER: ESECUZIONE STEP ATTUALE (uno per volta)
  // ---------------------------------------------------------------------------
  if (refreshStep != R_DONE && millis() > refreshDelay) {

    switch (refreshStep) {

      case R_WEATHER:
        if (g_show[P_WEATHER]) {
          fetchWeather();                 // sempre
          g_pageDirty[P_WEATHER] = true;  // sempre
        }
        refreshStep = R_AIR;
        break;



      case R_AIR:
        if (g_show[P_AIR] && fetchAir())
          g_pageDirty[P_AIR] = true;
        refreshStep = R_ICS;
        break;

      case R_ICS:
        // ICS sempre considerato "dirty" per il calendario
        fetchICS();
        g_pageDirty[P_CAL] = true;
        refreshStep = R_BTC;
        break;

      case R_BTC:
        if (g_show[P_BTC] && fetchCryptoWrapper())
          g_pageDirty[P_BTC] = true;
        refreshStep = R_QOD;
        break;

      case R_QOD:
        if (g_show[P_QOD] && fetchQOD())
          g_pageDirty[P_QOD] = true;
        refreshStep = R_FX;
        break;

      case R_FX:
        if (g_show[P_FX] && fetchFX())
          g_pageDirty[P_FX] = true;
        refreshStep = R_T24;
        break;

      case R_T24:
        if (g_show[P_T24] && fetchTemp24())
          g_pageDirty[P_T24] = true;
        refreshStep = R_SUN;
        break;

      case R_SUN:
        if (g_show[P_SUN] && fetchSun())
          g_pageDirty[P_SUN] = true;
        refreshStep = R_NEWS;
        break;

      case R_NEWS:
        if (g_show[P_NEWS] && fetchNews())
          g_pageDirty[P_NEWS] = true;
        refreshStep = R_HA;
        break;

      case R_HA:
        if (g_show[P_HA] && fetchHA())
          g_pageDirty[P_HA] = true;
        refreshStep = R_DONE;
        break;

      case R_DONE:
      default:
        break;
    }

    // prossimo step → ~200ms
    refreshDelay = millis() + 200;
  }

  // ---------------------------------------------------------------------------
  // ROTAZIONE PAGINE AUTOMATICA
  // ---------------------------------------------------------------------------
  if (millis() - lastPageSwitch >= PAGE_INTERVAL_MS) {

    int enabled = countEnabledPages();

    // ------------------------------------------------------------
    // UNA SOLA PAGINA ATTIVA → nessuna rotazione, nessuna transizione
    // ------------------------------------------------------------
    if (enabled <= 1) {

      // --- tick orologi ---
      time_t now = time(nullptr);
      struct tm ti;
      localtime_r(&now, &ti);

      if (ti.tm_sec != lastSecond) {
        lastSecond = ti.tm_sec;

        // CLOCK digitale → redraw completo (serve)
        if (g_page == P_CLOCK) {
          g_pageDirty[g_page] = true;
        }

        // CLOCK binario → aggiornamento locale (nessun fillScreen)
        if (g_page == P_BINARY) {
          pageBinaryClock();  // aggiornamento istantaneo, NO flicker
        }
      }

      // --- redraw solo quando serve (solo per CLOCK digitale) ---
      if (g_pageDirty[g_page]) {
        g_pageDirty[g_page] = false;
        gfx->fillScreen(COL_BG);
        drawCurrentPage();
      }

      lastPageSwitch = millis();
      return;  // nessuna transizione
    }

    // ------------------------------------------------------------
    // ROTAZIONE CLASSICA (fade, splash, transizioni)
    // ------------------------------------------------------------
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
      if (first < 0) first = P_CLOCK;

      g_page = first;
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

  // ---------------------------------------------------------------------------
  // ANIMAZIONI DELLA PAGINA CORRENTE
  // ---------------------------------------------------------------------------
  if (g_page == P_HA) {
    tickHA();  // aggiorna gli stati ogni 1s
    pageHA();  // ridisegna solo se ha_dirty=true
    delay(5);
    return;  // evita che sotto qualcosa sovrascriva la pagina
  }

  if (g_page == P_WEATHER) {

    // tick particelle
    pageWeatherParticlesTick();

    // se la pagina è dirty → ridisegna tutto
    if (g_pageDirty[P_WEATHER]) {
      g_pageDirty[P_WEATHER] = false;
      gfx->fillScreen(COL_BG);
      pageWeather();  // <-- REDRAW CORRETTO
    }

    delay(5);
    return;
  }

  if (g_page == P_AIR)
    tickLeaves(g_air_bg);

  if (g_page == P_FX)
    tickFXDataStream(COL_BG);

  if (g_page == P_COUNT)
    tickCountdownSnake();

  if (g_page == P_STELLAR) {
    // Solo cometa e piccoli effetti: niente redraw completo
    tickStellar();
    delay(5);
    return;  // blocca altre animazioni che potrebbero sovrascrivere
  }

  if (g_page == P_T24) {
    int old = temp24_progress;
    tickTemp24Anim();

    if (temp24_progress != old) {
      gfx->fillScreen(COL_BG);
      pageTemp24();
    }
  }

  delay(5);
}