#pragma once
/*
===============================================================================
   SQUARED — HOME ASSISTANT PAGE (IP + TOKEN)
   - Filtra solo entità utili
   - Friendly name max 3 parole
   - Batterie marcate "(Batt.)"
   - SUN eliminato in qualsiasi forma (ID, friendly, device_class)
   - Badge solo ON/OFF (verde scuro OFF, verde chiaro ON)
   - Temp/Umidità → nessun badge
   - Max 17 entità
   - Aggiornamento LIVE ogni 1s
===============================================================================
*/

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <vector>

#include "../handlers/globals.h"

// ============================================================================
// EXTERN
// ============================================================================
extern Arduino_RGB_Display* gfx;
extern const uint16_t COL_BG, COL_TEXT, COL_ACCENT1, COL_DIVIDER;
extern const int PAGE_X, PAGE_Y, PAGE_W, PAGE_H;
extern const int BASE_CHAR_W, BASE_CHAR_H, TEXT_SCALE;
extern String sanitizeText(const String& in);

// ============================================================================
// PROTOTIPI
// ============================================================================
bool fetchHA();
void pageHA();
void tickHA();

// ============================================================================
// STRUTTURA
// ============================================================================
struct HAEntry {
  String name;     // friendly
  String state;    // stato normalizzato
  bool isBattery;  // battery flag
  bool isOnOff;    // on/off → badge
  bool isTempHum;  // temperatura/umidità
};

static bool ha_ready = false;
static String ha_ip;
static std::vector<HAEntry> ha_entries;

// colori badge
static const uint16_t HA_OFF_COLOR = 0x03E0;
static const uint16_t HA_ON_COLOR  = 0x07E0;

// refresh locale
static uint32_t ha_nextPollMs = 0;
static bool ha_dirty = true;


// ============================================================================
// HELPERS LEGGERI
// ============================================================================
static bool isBatteryId(const String& id) {
  String s = id; s.toLowerCase();
  return (s.indexOf("battery") >= 0 ||
          s.indexOf("batteria") >= 0 ||
          s.endsWith("_bat"));
}

static bool isBatteryState(String s) {
  s.toLowerCase();
  return (s == "low" || s == "medium" || s == "middle" || s == "high");
}

static bool isTempHum(const String& id) {
  if (!id.startsWith("sensor.")) return false;
  String s = id; s.toLowerCase();
  return (s.indexOf("temperature") >= 0 ||
          s.indexOf("temperatura") >= 0 ||
          s.indexOf("humidity") >= 0 ||
          s.indexOf("umid") >= 0);
}

// friendly → max 3 parole
static void trimName(String& s) {
  int sp = 0;
  for (int i = 0; i < s.length(); i++) {
    if (s[i] == ' ') {
      sp++;
      if (sp >= 3) {
        s = s.substring(0, i);
        return;
      }
    }
  }
}

// normalizza stato
static String normState(const String& raw) {
  String s = raw;

  if (s == "low")    return "Low";
  if (s == "high")   return "High";
  if (s == "medium") return "Middle";
  if (s == "middle") return "Middle";

  bool num = (s.length() > 0);
  for (int i = 0; i < s.length() && num; i++)
    if (!isdigit(s[i])) num = false;
  if (num) return s + "%";

  if (s.length()) s[0] = toupper(s[0]);
  return s;
}


// ============================================================================
// FILTRO ENTITÀ (ESCLUDE TUTTO CIÒ CHE È SUN)
// ============================================================================
static bool allowEntity(const String& id, const String& fname) {

  // escludi tutto ciò che contiene "sun" ovunque
  {
    String sid = id;     sid.toLowerCase();
    String sfn = fname;  sfn.toLowerCase();

    if (sid.indexOf("sun") >= 0) return false;
    if (sfn.indexOf("sun") >= 0) return false;
  }

  if (id.startsWith("light."))  return true;
  if (id.startsWith("switch.")) return true;

  if (id.indexOf("_plug")   >= 0) return true;
  if (id.indexOf("_outlet") >= 0) return true;

  if (id.startsWith("binary_sensor.") &&
      id.indexOf("movimento") >= 0)
    return true;

  if (isBatteryId(id)) return true;
  if (isTempHum(id))   return true;

  return false;
}


// ============================================================================
// DISCOVERY
// ============================================================================
static String discoverHA() {
  int n = MDNS.queryService("_home-assistant", "_tcp");
  return (n > 0 ? MDNS.IP(0).toString() : "");
}


// ============================================================================
// FETCH STATI
// ============================================================================
static bool fetchHAStates() {

  ha_entries.clear();
  ha_entries.reserve(17);
  ha_ready = false;

  if (!g_ha_token.length()) return false;

  ha_ip = g_ha_ip.length() ? g_ha_ip : discoverHA();
  if (!ha_ip.length()) return false;

  HTTPClient http;
  http.setTimeout(3000);

  http.begin("http://" + ha_ip + ":8123/api/states");
  http.addHeader("Authorization", "Bearer " + g_ha_token);

  int code = http.GET();
  String body = http.getString();
  http.end();

  if (code != 200 || body.length() < 30) return false;

  int pos = 0;

  while (true) {

    if (ha_entries.size() >= 17) break;

    int p = body.indexOf("\"entity_id\"", pos);
    if (p < 0) break;

    int a = body.indexOf('"', p + 12);
    int b = body.indexOf('"', a + 1);
    if (a < 0 || b < 0) break;

    String id = body.substring(a + 1, b);
    pos = b + 1;

    // --- STATE ---
    int sKey = body.indexOf("\"state\"", b);
    if (sKey < 0) continue;

    int s0 = body.indexOf('"', sKey + 7);
    int s1 = body.indexOf('"', s0 + 1);
    if (s0 < 0 || s1 < 0) continue;

    String rawState = sanitizeText(body.substring(s0 + 1, s1));
    if (rawState == "unknown" || rawState == "unavailable") continue;

    // --- FRIENDLY NAME ---
    String fname = id;
    int attrPos = body.indexOf("\"attributes\"", s1);
    if (attrPos > 0) {
      int fn = body.indexOf("\"friendly_name\"", attrPos);
      if (fn > 0) {
        int f0 = body.indexOf('"', fn + 16);
        int f1 = body.indexOf('"', f0 + 1);
        if (f0 > 0 && f1 > f0)
          fname = sanitizeText(body.substring(f0 + 1, f1));
      }
    }

    // filtro completo (ID + friendly)
    if (!allowEntity(id, fname)) continue;

    bool isBatt = isBatteryId(id);
    if (isBatteryState(rawState)) isBatt = true;

    if (isBatt) fname += " (Batt.)";
    trimName(fname);

    bool isOnOff = (rawState == "on" || rawState == "off");
    bool isTH    = isTempHum(id);

    // aggiungi entry
    HAEntry e;
    e.name      = fname;
    e.state     = normState(rawState);
    e.isBattery = isBatt;
    e.isOnOff   = isOnOff;
    e.isTempHum = isTH;
    ha_entries.push_back(e);
  }

  ha_ready = true;
  ha_dirty = true;
  return true;
}


// ============================================================================
// AUTO-REFRESH (1s)
// ============================================================================
static void autoRefresh() {
  uint32_t now = millis();
  if (now >= ha_nextPollMs) {
    ha_nextPollMs = now + 1000;
    if (fetchHAStates()) ha_dirty = true;
  }
}

void tickHA() {
  autoRefresh();
}


// ============================================================================
// DRAW BADGE
// ============================================================================
static void drawBadge(int x, int y, uint16_t col) {
  gfx->fillRoundRect(x, y, 32, BASE_CHAR_H * TEXT_SCALE, 4, col);
}


// ============================================================================
// RENDER PAGINA (senza header)
// ============================================================================
void pageHA() {

  if (!ha_ready) {
    gfx->setCursor(PAGE_X, PAGE_Y + 20);
    gfx->print("Ricerca...");
    return;
  }

  if (!ha_dirty) return;
  ha_dirty = false;

  gfx->setTextSize(TEXT_SCALE);
  gfx->setTextColor(COL_TEXT, COL_BG);

  int y = PAGE_Y;

  if (ha_entries.empty()) {
    gfx->setCursor(PAGE_X, y);
    gfx->print("Nessuna entita'");
    return;
  }

  for (auto &e : ha_entries) {

    if (y > PAGE_Y + PAGE_H - 20) break;

    gfx->setCursor(PAGE_X, y);
    gfx->print(e.name);

    if (e.isOnOff) {
      uint16_t col = (e.state == "On" ? HA_ON_COLOR : HA_OFF_COLOR);
      drawBadge(PAGE_X + PAGE_W - 40, y - 2, col);
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

