#pragma once
/*
===============================================================================
   SQUARED — PAGINA HOME ASSISTANT (IP + TOKEN)
   - Filtra solo entità utili
   - Friendly name max 3 parole
   - Batterie marcate con "(Batt.)"
   - SUN abbreviato (Above/Below/Rising/Setting)
   - Badge SOLO ON/OFF (verde scuro OFF, verde chiaro ON)
   - Temp/Umidità → NO badge
   - Max 17 entità
   - Aggiornamento LIVE ogni 1s mentre la pagina è visibile
===============================================================================
*/

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <Client.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <vector>

#include "../handlers/globals.h"

// ============================================================================
// EXTERN
// ============================================================================
extern Arduino_RGB_Display* gfx;

extern const uint16_t COL_BG;
extern const uint16_t COL_TEXT;
extern const uint16_t COL_ACCENT1;
extern const uint16_t COL_DIVIDER;

extern const int PAGE_X;
extern const int PAGE_Y;
extern const int PAGE_W;
extern const int PAGE_H;

extern const int BASE_CHAR_W;
extern const int BASE_CHAR_H;
extern const int TEXT_SCALE;

extern void drawHeader(const String& title);
extern void drawHLine(int y);
extern String sanitizeText(const String& in);

// ============================================================================
// PROTOTIPI
// ============================================================================
bool fetchHA();
void pageHA();


// ============================================================================
// STRUTTURA
// ============================================================================
struct HAEntry {
  String id;
  String name;
  String state;
  bool isBattery;
  bool isSun;
  bool isOnOff;
  bool isTempHum;
};

static bool ha_ready = false;
static String ha_ip;
static std::vector<HAEntry> ha_entries;

// Badge colors
static const uint16_t HA_OFF_COLOR = 0x03E0;  // verde scuro
static const uint16_t HA_ON_COLOR  = 0x07E0;  // verde chiaro

// Auto-refresh locale
static uint32_t ha_nextPollMs = 0;
static bool ha_dirty = true;   // forza ridisegno alla prima draw


// ============================================================================
// HELPERS
// ============================================================================
static bool ha_isBatteryId(const String& id) {
  String s = id; s.toLowerCase();
  return (s.indexOf("battery")  >= 0 ||
          s.indexOf("batteria") >= 0 ||
          s.endsWith("_bat"));
}

static bool ha_isBatteryState(String s) {
  s.toLowerCase();
  return (s == "low" || s == "middle" || s == "medium" || s == "high");
}

static bool ha_isTempHum(const String& id) {
  if (!id.startsWith("sensor.")) return false;
  String s = id; s.toLowerCase();
  return (s.indexOf("temperature")  >= 0 ||
          s.indexOf("temperatura") >= 0 ||
          s.indexOf("humidity")    >= 0 ||
          s.indexOf("umid")        >= 0);
}


// ============================================================================
// NORMALIZZAZIONE
// ============================================================================
static String ha_normalizeSun(const String& raw) {
  if (raw == "above_horizon") return "Above";
  if (raw == "below_horizon") return "Below";
  if (raw == "rising")        return "Rising";
  if (raw == "setting")       return "Setting";
  return raw;
}

static String ha_normalizeState(const String& raw) {
  String s = raw;

  if (s == "high")   return "High";
  if (s == "low")    return "Low";
  if (s == "medium") return "Middle";
  if (s == "middle") return "Middle";

  bool numeric = (s.length() > 0);
  for (int i = 0; i < s.length() && numeric; i++)
    if (!isdigit(s[i])) numeric = false;

  if (numeric) return s + "%";

  if (s.length()) s[0] = toupper(s[0]);
  return s;
}


// ============================================================================
// FILTRO ENTITÀ
// ============================================================================
static bool ha_allowEntity(const String& id) {

  // ESCLUDE qualsiasi entità legata al sole
  {
    String s = id; 
    s.toLowerCase();
    if (s.startsWith("sun.")      ||   // sun.*
        s.indexOf("sun") >= 0     ||   // qualunque cosa contenga "sun"
        s.indexOf("next_") >= 0)       // esclusi anche next_*
      return false;
  }

  if (id.startsWith("light."))  return true;
  if (id.startsWith("switch.")) return true;
  if (id.indexOf("_plug")   >= 0) return true;
  if (id.indexOf("_outlet") >= 0) return true;

  if (id.startsWith("binary_sensor.") &&
      id.indexOf("movimento") >= 0)
    return true;

  if (ha_isBatteryId(id)) return true;
  if (ha_isTempHum(id))   return true;

  return false;
}



// ============================================================================
// FRIENDLY NAME → max 3 parole
// ============================================================================
static void ha_trimFriendlyName(String& fname) {
  int words = 0;
  for (int i = 0; i < fname.length(); i++) {
    if (fname[i] == ' ') {
      words++;
      if (words >= 3) {
        fname = fname.substring(0, i);
        return;
      }
    }
  }
}


// ============================================================================
// DISCOVERY
// ============================================================================
static String findHomeAssistant() {
  int n = MDNS.queryService("_home-assistant", "_tcp");
  if (n > 0) return MDNS.IP(0).toString();
  return "";
}


// ============================================================================
// FETCH STATI
// ============================================================================
static bool fetchHAStates() {

  ha_entries.clear();
  ha_entries.reserve(17);
  ha_ready = false;

  if (!g_ha_token.length()) return false;

  ha_ip = g_ha_ip.length() ? g_ha_ip : findHomeAssistant();
  if (!ha_ip.length()) return false;

  HTTPClient http;
  http.setTimeout(4000);

  http.begin("http://" + ha_ip + ":8123/api/states");
  http.addHeader("Authorization", "Bearer " + g_ha_token);

  int code = http.GET();
  String body = http.getString();
  http.end();

  if (code != 200 || body.length() < 10) return false;

  int  pos       = 0;
  bool sunExists = false;

  while (true) {

    if (ha_entries.size() >= 17) break;

    int p = body.indexOf("\"entity_id\"", pos);
    if (p < 0) break;

    int a = body.indexOf('"', p + 12);
    int b = body.indexOf('"', a + 1);
    if (a < 0 || b < 0) break;

    String id = body.substring(a + 1, b);
    pos = b + 1;

    if (!ha_allowEntity(id)) continue;

    int sKey = body.indexOf("\"state\"", b);
    if (sKey < 0) continue;

    int s0 = body.indexOf('"', sKey + 7);
    int s1 = body.indexOf('"', s0 + 1);
    if (s0 < 0 || s1 < 0) continue;

    String rawState = sanitizeText(body.substring(s0 + 1, s1));
    if (rawState == "unknown" || rawState == "unavailable") continue;

    bool isSun     = (id == "sun.sun");
    bool isTempHum = ha_isTempHum(id);

    if (isSun) {
      if (rawState != "above_horizon" &&
          rawState != "below_horizon" &&
          rawState != "rising" &&
          rawState != "setting")
        continue;

      if (sunExists) continue;
      sunExists = true;
    }

    String fname = id;
    bool isBattery = ha_isBatteryId(id);

    int attrPos = body.indexOf("\"attributes\"", s1);
    if (attrPos > 0) {

      int fn = body.indexOf("\"friendly_name\"", attrPos);
      if (fn > 0) {
        int f0 = body.indexOf('"', fn + 16);
        int f1 = body.indexOf('"', f0 + 1);
        if (f0 > 0 && f1 > f0)
          fname = sanitizeText(body.substring(f0 + 1, f1));
      }

      int dc = body.indexOf("\"device_class\"", attrPos);
      if (dc > 0) {
        int d0 = body.indexOf('"', dc + 15);
        int d1 = body.indexOf('"', d0 + 1);
        if (d0 > 0 && d1 > d0) {
          String dcVal = body.substring(d0 + 1, d1);
          dcVal.toLowerCase();
          if (dcVal == "battery") isBattery = true;
        }
      }
    }

    if (ha_isBatteryState(rawState))
      isBattery = true;

    if (isBattery)
      fname += " (Batt.)";

    ha_trimFriendlyName(fname);

    String st = isSun ? ha_normalizeSun(rawState)
                      : ha_normalizeState(rawState);

    bool isOnOff = (rawState == "on" || rawState == "off");

    // === Inserisci entry ===
    HAEntry e;
    e.id        = id;
    e.name      = fname;
    e.state     = st;
    e.isBattery = isBattery;
    e.isSun     = isSun;
    e.isOnOff   = isOnOff;
    e.isTempHum = isTempHum;

    ha_entries.push_back(e);
  }

  ha_ready = true;
  ha_dirty = true;   // forza ridisegno
  return true;
}


// ============================================================================
// AUTO-REFRESH LIVE
// ============================================================================
static void ha_autoRefreshIfNeeded() {
  uint32_t now = millis();
  if (now >= ha_nextPollMs) {
    ha_nextPollMs = now + 1000;
    if (fetchHAStates()) {
      ha_dirty = true;
    }
  }
}


// ============================================================================
// DRAW BADGE
// ============================================================================
static void ha_drawBadge(int x, int y, uint16_t col) {
  gfx->fillRoundRect(x, y, 32, BASE_CHAR_H * TEXT_SCALE, 4, col);
}


// ============================================================================
// RENDER PAGINA
// ============================================================================
void pageHA() {

  ha_autoRefreshIfNeeded();

  if (!ha_dirty) return;   // evita ridisegni inutili
  ha_dirty = false;

  drawHeader("HOME ASSISTANT");
  gfx->setTextSize(TEXT_SCALE);
  gfx->setTextColor(COL_TEXT, COL_BG);

  if (!ha_ready) {
    gfx->setCursor(PAGE_X, PAGE_Y + 20);
    gfx->print("Ricerca...");
    return;
  }

  if (ha_entries.empty()) {
    gfx->setCursor(PAGE_X, PAGE_Y + 20);
    gfx->print("Nessuna entita'");
    return;
  }

  int y = PAGE_Y;

  for (auto &e : ha_entries) {
    if (y > PAGE_Y + PAGE_H - 20)
      break;

    gfx->setCursor(PAGE_X, y);
    gfx->print(e.name);

    if (e.isOnOff) {
      uint16_t col = (e.state == "On" ? HA_ON_COLOR : HA_OFF_COLOR);
      ha_drawBadge(PAGE_X + PAGE_W - 40, y - 2, col);
    } else {
      int tw = e.state.length() * BASE_CHAR_W * TEXT_SCALE;
      gfx->setCursor(PAGE_X + PAGE_W - tw, y);
      gfx->print(e.state);
    }

    y += BASE_CHAR_H * TEXT_SCALE + 8;
    drawHLine(y - 4);
  }
}


// ============================================================================
// WRAPPER
// ============================================================================
bool fetchHA() {
  ha_ready = false;
  ha_nextPollMs = 0;
  ha_dirty = true;
  return fetchHAStates();
}

