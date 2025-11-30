/*
===============================================================================
   SQUARED — PAGINA "HOME ASSISTANT"
   Descrizione: Scansione entità HA via API, filtro intelligente (no sun,
                battery/temp/humidity, on/off), parsing compatto JSON senza
                allocazioni inutili, rendering ottimizzato fino a 17 entità.
   Autore: Davide “gat” Nasato
   Repository: https://github.com/davidegat/SquaredCoso
   Licenza: CC BY-NC 4.0
===============================================================================
*/

#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "../handlers/globals.h"

// ============================================================================
// EXTERN
// ============================================================================
extern Arduino_RGB_Display *gfx;
extern const uint16_t COL_BG, COL_TEXT, COL_ACCENT1, COL_DIVIDER;
extern const int PAGE_X, PAGE_Y, PAGE_W, PAGE_H;
extern const int BASE_CHAR_W, BASE_CHAR_H, TEXT_SCALE;
extern void drawHLine(int y);

// ============================================================================
// COSTANTI
// ============================================================================
static constexpr uint8_t HA_MAX_ENTRIES = 17;
static constexpr uint8_t HA_NAME_LEN = 28;  // era 32
static constexpr uint8_t HA_STATE_LEN = 10; // era 12
static constexpr uint16_t HA_OFF_COL = 0x03E0;
static constexpr uint16_t HA_ON_COL = 0x07E0;

struct HAEntry {
  char name[HA_NAME_LEN];
  char state[HA_STATE_LEN];
  uint8_t flags;
};

// Flag bits
static constexpr uint8_t HA_F_BATT = 0x01;
static constexpr uint8_t HA_F_ONOFF = 0x02;
static constexpr uint8_t HA_F_TEMPH = 0x04;

// ============================================================================
// STATO GLOBALE
// ============================================================================
static HAEntry ha_entries[HA_MAX_ENTRIES];
static uint8_t ha_count = 0;
static uint32_t ha_nextPollMs = 0;
static char ha_ip[16];

// Bitfield per flag globali
static struct {
  uint8_t ready : 1;
  uint8_t dirty : 1;
  uint8_t reserved : 6;
} ha_flags = {0, 1, 0};

// ============================================================================
// HELPERS
// ============================================================================

// Case-insensitive search
static bool hasCI(const char *hay, const char *needle) {
  if (!hay || !needle)
    return false;

  const uint8_t nlen = strlen(needle);
  const uint8_t hlen = strlen(hay);
  if (nlen > hlen)
    return false;

  for (uint8_t i = 0; i <= hlen - nlen; i++) {
    uint8_t j = 0;
    while (j < nlen && tolower(hay[i + j]) == tolower(needle[j]))
      j++;
    if (j == nlen)
      return true;
  }
  return false;
}

static inline bool hasCI(const String &s, const char *needle) {
  return hasCI(s.c_str(), needle);
}

static bool isBattId(const char *id) {
  return hasCI(id, "battery") || hasCI(id, "batteria") ||
         strstr(id, "_bat") != nullptr;
}

static bool isBattState(const char *s) {
  if (!s || strlen(s) > 6)
    return false;

  char low[8];
  for (uint8_t i = 0; s[i] && i < 7; i++)
    low[i] = tolower(s[i]);
  low[strlen(s)] = 0;

  return strcmp(low, "low") == 0 || strcmp(low, "high") == 0 ||
         strcmp(low, "medium") == 0 || strcmp(low, "middle") == 0;
}

static bool isTempHum(const char *id) {
  if (strncmp(id, "sensor.", 7) != 0)
    return false;
  return hasCI(id, "temperature") || hasCI(id, "temperatura") ||
         hasCI(id, "humidity") || hasCI(id, "umid");
}

// Copia troncata a max 3 parole
static void copyTrim3(char *dest, uint8_t destSize, const char *src) {
  uint8_t di = 0, spaces = 0;

  while (*src && di < destSize - 1) {
    if (*src == ' ' && ++spaces >= 3)
      break;
    dest[di++] = *src++;
  }
  dest[di] = 0;
}

// Normalizza stato in buffer
static void normState(char *dest, uint8_t destSize, const char *raw) {
  uint8_t len = strlen(raw);

  // Stati speciali (PROGMEM)
  if (strcasecmp(raw, "low") == 0) {
    strcpy_P(dest, PSTR("Low"));
    return;
  }
  if (strcasecmp(raw, "high") == 0) {
    strcpy_P(dest, PSTR("High"));
    return;
  }
  if (strcasecmp(raw, "medium") == 0 || strcasecmp(raw, "middle") == 0) {
    strcpy_P(dest, PSTR("Middle"));
    return;
  }

  bool allNum = (len > 0);
  for (uint8_t i = 0; i < len && allNum; i++) {
    if (!isdigit(raw[i]))
      allNum = false;
  }

  if (allNum && len < destSize - 2) {
    strcpy(dest, raw);
    dest[len] = '%';
    dest[len + 1] = 0;
    return;
  }

  // Copia con prima maiuscola
  if (len >= destSize)
    len = destSize - 1;
  dest[0] = toupper(raw[0]);
  memcpy(dest + 1, raw + 1, len - 1);
  dest[len] = 0;
}

// ============================================================================
// FILTRO ENTITÀ
// ============================================================================
static bool allowEnt(const char *id, const char *fname) {
  // Escludi sun
  if (hasCI(id, "sun") || hasCI(fname, "sun"))
    return false;

  // Prefissi permessi
  if (strncmp(id, "light.", 6) == 0)
    return true;
  if (strncmp(id, "switch.", 7) == 0)
    return true;

  // Plug/outlet
  if (strstr(id, "_plug") || strstr(id, "_outlet"))
    return true;

  // Binary sensor movimento
  if (strncmp(id, "binary_sensor.", 14) == 0 && hasCI(id, "movimento"))
    return true;

  // Battery o Temp/Humidity
  if (isBattId(id))
    return true;
  if (isTempHum(id))
    return true;

  return false;
}

// ============================================================================
// DISCOVERY
// ============================================================================
static bool discoverHA() {
  int8_t n = MDNS.queryService("_home-assistant", "_tcp");
  if (n > 0) {
    IPAddress ip = MDNS.IP(0);
    snprintf_P(ha_ip, sizeof(ha_ip), PSTR("%d.%d.%d.%d"), ip[0], ip[1], ip[2],
               ip[3]);
    return true;
  }
  ha_ip[0] = 0;
  return false;
}

// ============================================================================
// JSON PARSING
// ============================================================================
static int16_t extractJson(const String &body, int16_t start, const char *key,
                           char *dest, uint8_t destSize) {
  int16_t p = body.indexOf(key, start);
  if (p < 0)
    return -1;

  int16_t q1 = body.indexOf('"', p + strlen(key));
  if (q1 < 0)
    return -1;

  int16_t q2 = body.indexOf('"', q1 + 1);
  if (q2 < 0)
    return -1;

  uint8_t len = q2 - q1 - 1;
  if (len >= destSize)
    len = destSize - 1;

  const char *src = body.c_str() + q1 + 1;
  memcpy(dest, src, len);
  dest[len] = 0;

  return q2 + 1;
}

// ============================================================================
// FETCH STATI
// ============================================================================
static bool fetchHAStates() {
  ha_count = 0;
  ha_flags.ready = 0;

  if (g_ha_token.length() == 0)
    return false;

  // IP
  if (g_ha_ip.length() > 0) {
    strncpy(ha_ip, g_ha_ip.c_str(), sizeof(ha_ip) - 1);
    ha_ip[sizeof(ha_ip) - 1] = 0;
  } else if (!discoverHA()) {
    return false;
  }

  if (ha_ip[0] == 0)
    return false;

  // HTTP
  HTTPClient http;
  http.setTimeout(3000);
  http.setReuse(false);

  char url[48];
  snprintf_P(url, sizeof(url), PSTR("http://%s:8123/api/states"), ha_ip);

  http.begin(url);

  String authHeader = F("Bearer ");
  authHeader += g_ha_token;
  http.addHeader(F("Authorization"), authHeader);

  int16_t code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  if (body.length() < 30)
    return false;

  // Buffer parsing
  char idBuf[48];
  char stateBuf[24];
  char fnameBuf[48];

  int16_t pos = 0;

  while (ha_count < HA_MAX_ENTRIES) {
    pos = extractJson(body, pos, "\"entity_id\"", idBuf, sizeof(idBuf));
    if (pos < 0)
      break;

    int16_t statePos =
        extractJson(body, pos, "\"state\"", stateBuf, sizeof(stateBuf));
    if (statePos < 0)
      continue;

    // Skip invalidi
    if (strcmp(stateBuf, "unknown") == 0 ||
        strcmp(stateBuf, "unavailable") == 0) {
      pos = statePos;
      continue;
    }

    // Friendly name
    strcpy(fnameBuf, idBuf);

    int16_t attrPos = body.indexOf(F("\"attributes\""), statePos);
    if (attrPos > 0 && attrPos < statePos + 500) {
      extractJson(body, attrPos, "\"friendly_name\"", fnameBuf,
                  sizeof(fnameBuf));
    }

    // Filtro
    if (!allowEnt(idBuf, fnameBuf)) {
      pos = statePos;
      continue;
    }

    // Entry
    HAEntry &e = ha_entries[ha_count];
    e.flags = 0;

    // Battery
    if (isBattId(idBuf) || isBattState(stateBuf)) {
      e.flags |= HA_F_BATT;
      // Aggiungi suffisso
      uint8_t flen = strlen(fnameBuf);
      if (flen < sizeof(fnameBuf) - 8) {
        strcpy_P(fnameBuf + flen, PSTR(" (Batt)"));
      }
    }

    // Temp/Hum
    if (isTempHum(idBuf))
      e.flags |= HA_F_TEMPH;

    // On/Off
    if (strcasecmp(stateBuf, "on") == 0 || strcasecmp(stateBuf, "off") == 0) {
      e.flags |= HA_F_ONOFF;
    }

    copyTrim3(e.name, HA_NAME_LEN, fnameBuf);
    normState(e.state, HA_STATE_LEN, stateBuf);

    ha_count++;
    pos = statePos;
  }

  ha_flags.ready = 1;
  ha_flags.dirty = 1;
  return true;
}

// ============================================================================
// TICK
// ============================================================================
void tickHA() {
  uint32_t now = millis();
  if (now >= ha_nextPollMs) {
    ha_nextPollMs = now + 1000;
    if (fetchHAStates())
      ha_flags.dirty = 1;
  }
}

// ============================================================================
// RENDER
// ============================================================================
void pageHA() {
  if (!ha_flags.ready) {
    gfx->setCursor(PAGE_X, PAGE_Y + 20);
    gfx->print(F("Ricerca..."));
    return;
  }

  if (!ha_flags.dirty)
    return;
  ha_flags.dirty = 0;

  gfx->setTextSize(TEXT_SCALE);
  gfx->setTextColor(COL_TEXT, COL_BG);

  const int16_t lineH = BASE_CHAR_H * TEXT_SCALE;
  const int16_t rowStep = lineH + 7;
  const int16_t yMax = PAGE_Y + PAGE_H - 20;
  int16_t y = PAGE_Y - 40;

  if (ha_count == 0) {
    gfx->setCursor(PAGE_X, y);
    gfx->print(F("Nessuna entita'"));
    return;
  }

  for (uint8_t i = 0; i < ha_count && y <= yMax; i++) {
    const HAEntry &e = ha_entries[i];

    gfx->setCursor(PAGE_X, y);
    gfx->print(e.name);

    if (e.flags & HA_F_ONOFF) {
      // Check secondo carattere: 'n' = On, 'f' = Off
      uint16_t col = (e.state[1] == 'n') ? HA_ON_COL : HA_OFF_COL;
      gfx->fillRoundRect(PAGE_X + PAGE_W - 40, y - 2, 32, lineH, 4, col);
    } else {
      int16_t tw = strlen(e.state) * BASE_CHAR_W * TEXT_SCALE;
      gfx->setCursor(PAGE_X + PAGE_W - tw, y);
      gfx->print(e.state);
    }

    y += lineH + 10;
    drawHLine(y - 4);
    y -= 3;
  }
}

// ============================================================================
// WRAPPER
// ============================================================================
bool fetchHA() {
  ha_flags.ready = 0;
  ha_flags.dirty = 1;
  ha_nextPollMs = 0;
  return fetchHAStates();
}
