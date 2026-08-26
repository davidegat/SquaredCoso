#pragma once

#include "globals.h"
#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <time.h>
#include <stdlib.h>

static const char *PAGE_KEYS[PAGES] = {
    "p_WEATHER", "p_AIR",  "p_CLOCK",   "p_BINARY", "p_CAL",    "p_BTC",
    "p_QOD",     "p_INFO", "p_COUNT",   "p_FX",     "p_T24",    "p_SUN",
    "p_NEWS",    "p_HA",   "p_STELLAR", "p_NOTES",  "p_CHRONOS"};

extern WebServer web;
extern Preferences prefs;
extern bool g_splash_enabled;
extern void ensureCurrentPageEnabled();
extern uint32_t pagesMaskFromArray();
extern void pagesArrayFromMask(uint32_t mask);

static inline String cleanHAHost(String s) {
  s.trim();
  if (s.startsWith("http://")) s.remove(0, 7);
  if (s.startsWith("https://")) s.remove(0, 8);
  while (s.endsWith("/")) s.remove(s.length() - 1);
  return s;
}

static inline String cleanEntityList(String s) {
  s.replace("\r", "");
  s.replace("\n", ",");
  s.replace(";", ",");
  while (s.indexOf("  ") >= 0) s.replace("  ", " ");
  s.trim();
  return s;
}

void saveAppConfig() {
  prefs.begin("app", false);
  prefs.putString("city", g_city);
  prefs.putString("lang", g_lang);
  prefs.putString("ics", g_ics);
  prefs.putString("lat", g_lat);
  prefs.putString("lon", g_lon);
  prefs.putString("tz_rule", g_tz_rule);
  prefs.putUChar("rotation", g_rotation & 3);
  prefs.putBool("splash", g_splash_enabled);
  prefs.putString("ha_ip", g_ha_ip);
  prefs.putString("ha_token", g_ha_token);
  prefs.putString("ha_entities", g_ha_entities);
  prefs.putULong("page_ms", PAGE_INTERVAL_MS);
  prefs.putString("fiat", g_fiat);
  prefs.putString("rss_url", g_rss_url);
  prefs.putString("oa_key", g_oa_key);
  prefs.putString("oa_topic", g_oa_topic);
  prefs.putString("note", g_note);
  prefs.putString("btc_owned", isnan(g_btc_owned) ? "" : String(g_btc_owned, 8));

  for (int i = 0; i < 8; i++) {
    char kn[6], kt[6];
    snprintf(kn, sizeof(kn), "cd%dn", i + 1);
    snprintf(kt, sizeof(kt), "cd%dt", i + 1);
    prefs.putString(kn, cd[i].name);
    prefs.putString(kt, cd[i].whenISO);
  }
  prefs.putUInt("pages_mask", pagesMaskFromArray());
  prefs.end();
}

void handleSettings() {
  bool saved = false;

  if (web.method() == HTTP_POST) {
    String city = web.arg("city"); city.trim();
    String lang = web.arg("lang"); lang.trim();
    String ics = web.arg("ics"); ics.trim();

    if (city.length() && city != g_city) {
      g_city = city;
      g_lat = "";
      g_lon = "";
    }
    g_lang = (lang == "it" || lang == "en") ? lang : "it";
    g_ics = ics;

    if (web.hasArg("tz_preset")) {
      String preset = web.arg("tz_preset"); preset.trim();
      if (preset == "europe") {
        g_tz_rule = "CET-1CEST,M3.5.0,M10.5.0/3";
      } else if (preset == "utc") {
        g_tz_rule = "UTC0";
      } else if (preset == "custom" && web.hasArg("tz_custom")) {
        String tz = web.arg("tz_custom"); tz.trim();
        if (tz.length() >= 3 && tz.length() <= 96) g_tz_rule = tz;
      }
    } else if (web.hasArg("tz_rule")) {

      String tz = web.arg("tz_rule"); tz.trim();
      if (tz.length() >= 3 && tz.length() <= 96) g_tz_rule = tz;
    }

    if (web.hasArg("rotation")) {
      int r = web.arg("rotation").toInt();
      g_rotation = (uint8_t)constrain(r, 0, 3);
    }

    if (web.hasArg("page_s")) {
      long ps = constrain(web.arg("page_s").toInt(), 5L, 600L);
      PAGE_INTERVAL_MS = (uint32_t)ps * 1000UL;
    }

    g_note = web.hasArg("note") ? web.arg("note") : "";

    if (web.hasArg("fiat")) {
      g_fiat = web.arg("fiat"); g_fiat.trim(); g_fiat.toUpperCase();
      if (g_fiat.length() != 3) g_fiat = "CHF";
    }

    if (web.hasArg("ha_ip")) g_ha_ip = cleanHAHost(web.arg("ha_ip"));
    if (web.hasArg("ha_entities")) g_ha_entities = cleanEntityList(web.arg("ha_entities"));

    // Blank secret fields keep the stored value; explicit clear flags remove it.
    if (web.hasArg("clear_ha_token")) {
      g_ha_token = "";
    } else if (web.hasArg("ha_token")) {
      String v = web.arg("ha_token"); v.trim();
      if (v.length()) g_ha_token = v;
    }

    if (web.hasArg("btc_owned")) {
      String v = web.arg("btc_owned"); v.trim();
      g_btc_owned = v.length() ? v.toDouble() : NAN;
    }

    if (web.hasArg("clear_openai_key")) {
      g_oa_key = "";
    } else if (web.hasArg("openai_key")) {
      String v = web.arg("openai_key"); v.trim();
      if (v.length()) g_oa_key = v;
    }
    if (web.hasArg("openai_topic")) {
      g_oa_topic = web.arg("openai_topic"); g_oa_topic.trim();
    }

    if (web.hasArg("rss_url")) {
      g_rss_url = web.arg("rss_url"); g_rss_url.trim();
    }

    g_splash_enabled = web.hasArg("splash_enabled");

    setenv("TZ", g_tz_rule.c_str(), 1);
    tzset();

    for (int i = 0; i < 8; i++) {
      char kn[6], kt[6];
      snprintf(kn, sizeof(kn), "cd%dn", i + 1);
      snprintf(kt, sizeof(kt), "cd%dt", i + 1);
      cd[i].name = web.arg(kn); cd[i].name.trim();
      cd[i].whenISO = web.arg(kt); cd[i].whenISO.trim();
    }

    for (int i = 0; i < PAGES; i++) g_show[i] = web.hasArg(PAGE_KEYS[i]);
    bool any = false;
    for (int i = 0; i < PAGES; i++) any |= g_show[i];
    if (!any) g_show[P_CLOCK] = true;

    saveAppConfig();
    ensureCurrentPageEnabled();
    applyDisplayRotation();
    softReboot();
    saved = true;
  }

  web.send(200, "text/html; charset=utf-8", htmlSettings(saved, ""));
}

void loadAppConfig() {
  prefs.begin("app", true);
  g_city = prefs.getString("city", g_city);
  g_lang = prefs.getString("lang", g_lang);
  g_ics = prefs.getString("ics", g_ics);
  g_lat = prefs.getString("lat", g_lat);
  g_lon = prefs.getString("lon", g_lon);
  g_tz_rule = prefs.getString("tz_rule", g_tz_rule);
  g_rotation = prefs.getUChar("rotation", 0) & 3;
  g_note = prefs.getString("note", "");
  g_fiat = prefs.getString("fiat", "CHF");
  g_splash_enabled = prefs.getBool("splash", true);
  g_ha_ip = prefs.getString("ha_ip", "");
  g_ha_token = prefs.getString("ha_token", "");
  g_ha_entities = prefs.getString("ha_entities", "");
  PAGE_INTERVAL_MS = prefs.getULong("page_ms", PAGE_INTERVAL_MS);
  g_oa_key = prefs.getString("oa_key", g_oa_key);
  g_oa_topic = prefs.getString("oa_topic", g_oa_topic);
  g_rss_url = prefs.getString("rss_url", g_rss_url);

  String btc = prefs.getString("btc_owned", "");
  btc.trim();
  g_btc_owned = btc.length() ? btc.toDouble() : NAN;

  for (int i = 0; i < 8; i++) {
    char kn[6], kt[6];
    snprintf(kn, sizeof(kn), "cd%dn", i + 1);
    snprintf(kt, sizeof(kt), "cd%dt", i + 1);
    cd[i].name = prefs.getString(kn, "");
    cd[i].whenISO = prefs.getString(kt, "");
  }
  uint32_t mask = prefs.getUInt("pages_mask", 0xFFFFFFFFUL);
  prefs.end();

  pagesArrayFromMask(mask);
  PAGE_INTERVAL_MS = constrain(PAGE_INTERVAL_MS, 5000UL, 600000UL);
  g_fiat.trim(); g_fiat.toUpperCase();
  if (g_fiat.length() != 3) g_fiat = "CHF";
  g_ha_ip = cleanHAHost(g_ha_ip);
  g_ha_token.trim();
  g_ha_entities = cleanEntityList(g_ha_entities);
  g_oa_key.trim();
  g_oa_topic.trim();
  g_rss_url.trim();
  if (g_tz_rule.length() < 3) g_tz_rule = "CET-1CEST,M3.5.0,M10.5.0/3";
}
