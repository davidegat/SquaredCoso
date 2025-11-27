#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <Preferences.h>
#include "globals.h"

// ============================================================================
// SETTINGS HANDLER – Gestione configurazione, caricamento e salvataggio NVS
// ============================================================================
//
// Questo modulo gestisce:
// - parsing POST dalla WebUI
// - aggiornamento delle variabili globali
// - salvataggio su NVS (namespace "app")
// - caricamento su boot
// - maschera pagine visibili
//
// Il codice è ordinato in blocchi logici e i commenti spiegano il funzionamento
// ============================================================================


// ---------------------------------------------------------------------------
// CHIAVI NVS PER LE PAGINE
// ---------------------------------------------------------------------------
static const char* PAGE_KEYS[PAGES] = {
  "p_WEATHER", "p_AIR", "p_CLOCK", "p_BINARY", "p_CAL",
  "p_BTC", "p_QOD", "p_INFO", "p_COUNT", "p_FX",
  "p_T24", "p_SUN", "p_NEWS", "p_HA", "p_STELLAR",
  "p_NOTES", "p_CHRONOS"
};

// ---------------------------------------------------------------------------
// EXTERN dal core
// ---------------------------------------------------------------------------
extern WebServer   web;
extern Preferences prefs;

extern bool g_splash_enabled;
extern bool g_forceRedraw;

extern void ensureCurrentPageEnabled();
extern uint16_t pagesMaskFromArray();
extern void pagesArrayFromMask(uint16_t mask);


// ============================================================================
// handleSettings() — Gestione POST e salvataggio su NVS
// ============================================================================
void handleSettings() {

  bool saved = false;

  if (web.method() == HTTP_POST) {

    // ----------------------------------------------------------
    // PARAMETRI BASE
    // ----------------------------------------------------------
    String city = web.arg("city"); city.trim();
    String lang = web.arg("lang"); lang.trim();
    String ics  = web.arg("ics");  ics.trim();

    if (city.length()) g_city = city;
    g_lang = (lang == "it" || lang == "en") ? lang : "it";
    g_ics  = ics;

    // Intervallo pagina (5–600s)
    if (web.hasArg("page_s")) {
      long ps = web.arg("page_s").toInt();
      if (ps < 5)   ps = 5;
      if (ps > 600) ps = 600;
      PAGE_INTERVAL_MS = ps * 1000UL;
    }

    // -------- Post-it --------
    if (web.hasArg("note"))
      g_note = sanitizeText(web.arg("note"));
    else
      g_note = "";

    // -------- Fiat base --------
    if (web.hasArg("fiat")) {
      g_fiat = web.arg("fiat");
      g_fiat.trim();
      g_fiat.toUpperCase();
    }

    // ----------------------------------------------------------
    // HOME ASSISTANT
    // ----------------------------------------------------------
    if (web.hasArg("ha_ip")) {
      g_ha_ip = sanitizeText(web.arg("ha_ip"));
      g_ha_ip.trim();
      if (g_ha_ip.startsWith("http://"))  g_ha_ip.remove(0, 7);
      if (g_ha_ip.startsWith("https://")) g_ha_ip.remove(0, 8);
      if (g_ha_ip.endsWith("/"))         g_ha_ip.remove(g_ha_ip.length() - 1);
    }

    if (web.hasArg("ha_token")) {
      g_ha_token = sanitizeText(web.arg("ha_token"));
      g_ha_token.trim();
    }

    // ----------------------------------------------------------
    // BITCOIN
    // ----------------------------------------------------------
    if (web.hasArg("btc_owned")) {
      String s = web.arg("btc_owned");
      s.trim();
      g_btc_owned = s.length() ? s.toDouble() : NAN;
    }

    // ----------------------------------------------------------
    // OPENAI
    // ----------------------------------------------------------
    g_oa_key   = sanitizeText(web.arg("openai_key"));
    g_oa_topic = sanitizeText(web.arg("openai_topic"));

    // ----------------------------------------------------------
    // RSS
    // ----------------------------------------------------------
    if (web.hasArg("rss_url")) {
      g_rss_url = sanitizeText(web.arg("rss_url"));
      g_rss_url.trim();
    }

    // ----------------------------------------------------------
    // SPLASH IMAGES
    // ----------------------------------------------------------
    g_splash_enabled = web.hasArg("splash_enabled");

    // ----------------------------------------------------------
    // COUNTDOWN
    // ----------------------------------------------------------
    for (int i = 0; i < 8; i++) {
      char kn[6], kt[6];
      snprintf(kn, sizeof(kn), "cd%dn", i+1);
      snprintf(kt, sizeof(kt), "cd%dt", i+1);
      cd[i].name    = sanitizeText(web.arg(kn));
      cd[i].whenISO = sanitizeText(web.arg(kt));
    }

    // ----------------------------------------------------------
    // PAGINE VISIBILI
    // ----------------------------------------------------------
    for (int i = 0; i < PAGES; i++)
      g_show[i] = web.hasArg(PAGE_KEYS[i]);

    // Almeno una pagina deve restare attiva
    bool any = false;
    for (int i = 0; i < PAGES; i++)
      if (g_show[i]) { any = true; break; }

    if (!any)
      g_show[P_CLOCK] = true;

    // ----------------------------------------------------------
    // SALVATAGGIO SU NVS
    // ----------------------------------------------------------
    prefs.begin("app", false);

    prefs.putString("city", g_city);
    prefs.putString("lang", g_lang);
    prefs.putString("ics",  g_ics);
    prefs.putString("lat",  g_lat);
    prefs.putString("lon",  g_lon);

    prefs.putBool("splash", g_splash_enabled);

    prefs.putString("ha_ip",    g_ha_ip);
    prefs.putString("ha_token", g_ha_token);

    prefs.putULong("page_ms", PAGE_INTERVAL_MS);

    prefs.putString("fiat",      g_fiat);
    prefs.putString("rss_url",   g_rss_url);
    prefs.putString("oa_key",    g_oa_key);
    prefs.putString("oa_topic",  g_oa_topic);
    prefs.putString("note",      g_note);

    // BTC — salvato come stringa con precisione a 8 decimali
    prefs.putString("btc_owned", String(g_btc_owned, 8));

    // Countdown
    for (int i = 0; i < 8; i++) {
      char kn[6], kt[6];
      snprintf(kn, sizeof(kn), "cd%dn", i+1);
      snprintf(kt, sizeof(kt), "cd%dt", i+1);
      prefs.putString(kn, cd[i].name);
      prefs.putString(kt, cd[i].whenISO);
    }

    // Maschera pagine
    prefs.putUShort("pages_mask", pagesMaskFromArray());

    prefs.end();

    ensureCurrentPageEnabled();
    g_dataRefreshPending = true;
    saved = true;
  }

  web.send(200, "text/html; charset=utf-8", htmlSettings(saved, ""));
}



// ============================================================================
// loadAppConfig() — Caricamento completo configurazione NVS
// ============================================================================
void loadAppConfig() {

  prefs.begin("app", true);

  g_city = prefs.getString("city", g_city);
  g_lang = prefs.getString("lang", g_lang);
  g_ics  = prefs.getString("ics",  g_ics);
  g_lat  = prefs.getString("lat",  g_lat);
  g_lon  = prefs.getString("lon",  g_lon);

  g_note = prefs.getString("note", "");

  g_fiat = prefs.getString("fiat", "CHF");
  g_fiat.trim();
  g_fiat.toUpperCase();

  g_splash_enabled = prefs.getBool("splash", true);

  g_ha_ip    = prefs.getString("ha_ip", "");
  g_ha_token = prefs.getString("ha_token", "");

  g_ha_ip.trim();
  g_ha_token.trim();
  if (g_ha_ip.startsWith("http://"))  g_ha_ip.remove(0, 7);
  if (g_ha_ip.startsWith("https://")) g_ha_ip.remove(0, 8);
  if (g_ha_ip.endsWith("/"))          g_ha_ip.remove(g_ha_ip.length() - 1);

  PAGE_INTERVAL_MS = prefs.getULong("page_ms", PAGE_INTERVAL_MS);

  g_oa_key   = prefs.getString("oa_key",   g_oa_key);
  g_oa_topic = prefs.getString("oa_topic", g_oa_topic);

  g_rss_url = prefs.getString("rss_url", g_rss_url);
  g_rss_url.trim();

  // BTC — letto simmetricamente come stringa
  {
    String btc = prefs.getString("btc_owned", "");
    btc.trim();
    g_btc_owned = btc.length() ? btc.toDouble() : NAN;
  }

  // Countdown
  for (int i = 0; i < 8; i++) {
    char kn[6], kt[6];
    snprintf(kn, sizeof(kn), "cd%dn", i+1);
    snprintf(kt, sizeof(kt), "cd%dt", i+1);
    cd[i].name    = prefs.getString(kn, "");
    cd[i].whenISO = prefs.getString(kt, "");
  }

  // Pagine visibili
  uint16_t mask = prefs.getUShort("pages_mask", 0xFFFF);

  prefs.end();

  pagesArrayFromMask(mask);

  // intervallo pagina sicuro
  uint32_t s = PAGE_INTERVAL_MS / 1000;
  if (s < 5)   PAGE_INTERVAL_MS = 5000;
  if (s > 600) PAGE_INTERVAL_MS = 600000;

  g_oa_key.trim();
  g_oa_topic.trim();
}



// ============================================================================
// saveAppConfig() — Salvataggio rapido (usato raramente)
// ============================================================================
void saveAppConfig() {

  prefs.begin("app", false);

  prefs.putString("fiat", g_fiat);
  prefs.putString("city", g_city);
  prefs.putString("lang", g_lang);
  prefs.putString("ics",  g_ics);
  prefs.putString("lat",  g_lat);
  prefs.putString("lon",  g_lon);

  prefs.putString("rss_url",   g_rss_url);
  prefs.putString("oa_key",    g_oa_key);
  prefs.putString("oa_topic",  g_oa_topic);

  prefs.putString("ha_ip",    g_ha_ip);
  prefs.putString("ha_token", g_ha_token);

  prefs.putULong("page_ms", PAGE_INTERVAL_MS);

  // BTC simmetrico
  prefs.putString("btc_owned", String(g_btc_owned, 8));

  // Countdown
  for (int i = 0; i < 8; i++) {
    char kn[6], kt[6];
    snprintf(kn, sizeof(kn), "cd%dn", i + 1);
    snprintf(kt, sizeof(kt), "cd%dt", i + 1);
    prefs.putString(kn, cd[i].name);
    prefs.putString(kt, cd[i].whenISO);
  }

  prefs.putUShort("pages_mask", pagesMaskFromArray());

  prefs.end();
}


