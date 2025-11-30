/*
===============================================================================
   SQUARED — PAGINA "TEMP24" (Grafico Temperatura 24h)
   Descrizione: Download dati Open-Meteo, parsing leggero dell’array daily,
                ricostruzione curva 24h tramite 7 anchor + interpolazione
                lineare, griglia tratteggiata, etichette dinamiche IT/EN,
                curva spessa antialias simulato e punti con alone.
   Autore: Davide “gat” Nasato
   Repository: https://github.com/davidegat/SquaredCoso
   Licenza: CC BY-NC 4.0
===============================================================================
*/

#pragma once

#include "../handlers/globals.h"
#include "../handlers/jsonhelpers.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// ---------------------------------------------------------------------------
// EXTERN
// ---------------------------------------------------------------------------
extern Arduino_RGB_Display *gfx;

extern const uint16_t COL_BG;
extern const uint16_t COL_HEADER;
extern const uint16_t COL_TEXT;
extern const uint16_t COL_ACCENT1;
extern const uint16_t COL_ACCENT2;

extern const int PAGE_X;
extern const int PAGE_Y;
extern const int TEXT_SCALE;
extern const int CHAR_H;
extern const int BASE_CHAR_W;
extern const int BASE_CHAR_H;

extern void drawHeader(const String &title);
extern void drawBoldMain(int16_t x, int16_t y, const String &raw,
                         uint8_t scale);

extern int indexOfCI(const String &, const String &, int from);
extern bool httpGET(const String &url, String &out, uint32_t timeout);
extern bool geocodeIfNeeded();

extern String g_lang;
extern String g_lat;
extern String g_lon;

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------
static constexpr int16_t T24_MARGIN = 24;
static constexpr int16_t T24_GRAPH_X = 60;
static constexpr int16_t T24_GRAPH_Y = 70;
static constexpr int16_t T24_GRAPH_W = 396;
static constexpr int16_t T24_GRAPH_H = 280;
static constexpr int16_t T24_LABEL_Y = T24_GRAPH_Y + T24_GRAPH_H + 24;

// Colori grafici
static constexpr uint16_t T24_LINE_COL = 0x07FF;  // Cyan
static constexpr uint16_t T24_GRID_COL = 0x2945;  // Grigio scuro
static constexpr uint16_t T24_POINT_COL = 0xFFE0; // Giallo

// ---------------------------------------------------------------------------
// Buffer temperatura 24h
// ---------------------------------------------------------------------------
static float t24[24];

// ---------------------------------------------------------------------------
// Stub animazione (compatibilità API)
// ---------------------------------------------------------------------------
void resetTemp24Anim() {}
void tickTemp24Anim() {}

// ---------------------------------------------------------------------------
// Fetch da Open-Meteo
// ---------------------------------------------------------------------------
static bool fetchTemp24() {
  for (uint8_t i = 0; i < 24; i++)
    t24[i] = NAN;

  if (!geocodeIfNeeded())
    return false;

  String url = F("https://api.open-meteo.com/v1/forecast?latitude=");
  url += g_lat;
  url += F("&longitude=");
  url += g_lon;
  url += F("&daily=temperature_2m_mean&forecast_days=7&timezone=auto");

  String body;
  if (!httpGET(url, body, 12000))
    return false;

  int16_t posDaily = indexOfCI(body, F("\"daily\""), 0);
  if (posDaily < 0)
    return false;

  int16_t posMean = indexOfCI(body, F("\"temperature_2m_mean\""), posDaily);
  if (posMean < 0)
    return false;

  int16_t lb = body.indexOf('[', posMean);
  if (lb < 0)
    return false;

  int16_t rb = findMatchingBracket(body, lb);
  if (rb < 0)
    return false;

  String arr = body.substring(lb + 1, rb);

  float seven[7];
  for (uint8_t i = 0; i < 7; i++)
    seven[i] = NAN;

  uint8_t idx = 0;
  int16_t start = 0;
  const int16_t L = arr.length();

  while (idx < 7 && start < L) {
    int16_t s = start;
    while (s < L && !((arr[s] >= '0' && arr[s] <= '9') || arr[s] == '-'))
      s++;
    if (s >= L)
      break;

    int16_t e = s;
    while (e < L &&
           ((arr[e] >= '0' && arr[e] <= '9') || arr[e] == '.' || arr[e] == '-'))
      e++;

    seven[idx++] = arr.substring(s, e).toFloat();
    start = e + 1;
  }

  if (!idx)
    return false;

  // Anchor positions per 7 valori su 24 slot
  static const uint8_t anchors[7] PROGMEM = {0, 4, 8, 12, 16, 20, 23};

  for (uint8_t i = 0; i < 7; i++) {
    t24[pgm_read_byte(&anchors[i])] = seven[i];
  }

  // Interpolazione lineare
  for (uint8_t a = 0; a < 6; a++) {
    uint8_t x1 = pgm_read_byte(&anchors[a]);
    uint8_t x2 = pgm_read_byte(&anchors[a + 1]);
    float y1 = seven[a];
    float y2 = seven[a + 1];

    uint8_t dx = x2 - x1;
    if (dx < 1)
      continue;

    float dy = (y2 - y1) / dx;
    for (uint8_t k = 1; k < dx; k++) {
      t24[x1 + k] = y1 + dy * k;
    }
  }

  return true;
}

// ---------------------------------------------------------------------------
// Helper: formatta temperatura con simbolo gradi
// ---------------------------------------------------------------------------
static void fmtTemp(char *buf, uint8_t bufSize, int16_t temp) {
  snprintf_P(buf, bufSize, PSTR("%dc"), temp, 0xB0); // 0xB0 = °
}

// ---------------------------------------------------------------------------
// Helper: disegna linea griglia orizzontale tratteggiata
// ---------------------------------------------------------------------------
static void drawGridLineH(int16_t y) {
  for (int16_t x = T24_GRAPH_X; x < T24_GRAPH_X + T24_GRAPH_W; x += 6) {
    gfx->drawPixel(x, y, T24_GRID_COL);
    gfx->drawPixel(x + 1, y, T24_GRID_COL);
  }
}

// ---------------------------------------------------------------------------
// Pagina grafico 24H
// ---------------------------------------------------------------------------
static void pageTemp24() {
  const bool it = (g_lang == "it");

  gfx->fillScreen(COL_BG);
  drawHeader(it ? F("Trend Temperatura") : F("Temperature Trend"));

  // Trova min/max
  float mn = 999.0f, mx = -999.0f;
  for (uint8_t i = 0; i < 24; i++) {
    float v = t24[i];
    if (!isnan(v)) {
      if (v < mn)
        mn = v;
      if (v > mx)
        mx = v;
    }
  }

  if (mn > mx) {
    drawBoldMain(PAGE_X, PAGE_Y + 40,
                 it ? F("Dati non disponibili") : F("Data not available"),
                 TEXT_SCALE);
    return;
  }

  // Arrotonda range per etichette pulite
  int16_t minT = (int16_t)floorf(mn);
  int16_t maxT = (int16_t)ceilf(mx);
  if (maxT - minT < 4) {
    minT -= 2;
    maxT += 2;
  }
  float range = (float)(maxT - minT);

  // === AREA GRAFICO ===

  // Bordo grafico
  gfx->drawRect(T24_GRAPH_X, T24_GRAPH_Y, T24_GRAPH_W, T24_GRAPH_H,
                T24_GRID_COL);

  // Linee griglia orizzontali (5 livelli)
  for (uint8_t i = 1; i < 5; i++) {
    int16_t y = T24_GRAPH_Y + (T24_GRAPH_H * i) / 5;
    drawGridLineH(y);
  }

  // Linee griglia verticali (7 giorni)
  static const uint8_t anchors[7] PROGMEM = {0, 4, 8, 12, 16, 20, 23};
  for (uint8_t i = 0; i < 7; i++) {
    uint8_t anchor = pgm_read_byte(&anchors[i]);
    int16_t x = T24_GRAPH_X + (anchor * T24_GRAPH_W) / 23;

    for (int16_t y = T24_GRAPH_Y; y < T24_GRAPH_Y + T24_GRAPH_H; y += 6) {
      gfx->drawPixel(x, y, T24_GRID_COL);
      gfx->drawPixel(x, y + 1, T24_GRID_COL);
    }
  }

  // === ETICHETTE TEMPERATURA (asse Y) ===
  gfx->setTextSize(2);
  gfx->setTextColor(COL_TEXT);

  char buf[8];

  for (uint8_t i = 0; i <= 4; i++) {
    int16_t temp = minT + ((maxT - minT) * (4 - i)) / 4;
    int16_t y = T24_GRAPH_Y + (T24_GRAPH_H * i) / 4;

    fmtTemp(buf, sizeof(buf), temp);

    int16_t tw = strlen(buf) * 6;
    gfx->setCursor(T24_GRAPH_X - tw - 6, y - 3);
    gfx->print(buf);
  }

  // === ETICHETTE GIORNI (asse X) ===
  static const char D_IT[7][4] PROGMEM = {"Lu", "Ma", "Me", "Gi",
                                          "Ve", "Sa", "Do"};
  static const char D_EN[7][4] PROGMEM = {"Mo", "Tu", "We", "Th",
                                          "Fr", "Sa", "Su"};

  time_t now = time(nullptr);
  struct tm info;
  localtime_r(&now, &info);
  uint8_t baseDay = (info.tm_wday == 0) ? 6 : info.tm_wday - 1;

  gfx->setTextColor(COL_ACCENT2);

  for (uint8_t i = 0; i < 7; i++) {
    uint8_t dayIdx = (baseDay + i) % 7;
    uint8_t anchor = pgm_read_byte(&anchors[i]);
    int16_t x = T24_GRAPH_X + (anchor * T24_GRAPH_W) / 23;

    char dayBuf[4];
    if (it) {
      memcpy_P(dayBuf, D_IT[dayIdx], 4);
    } else {
      memcpy_P(dayBuf, D_EN[dayIdx], 4);
    }

    gfx->setCursor(x - 9, T24_LABEL_Y);
    gfx->print(dayBuf);
  }

  // === CURVA TEMPERATURA ===
  gfx->startWrite();

  int16_t prevX = -1, prevY = -1;

  for (uint8_t i = 0; i < 24; i++) {
    float v = t24[i];
    if (isnan(v)) {
      prevX = -1;
      continue;
    }

    int16_t x = T24_GRAPH_X + (i * T24_GRAPH_W) / 23;
    int16_t y = T24_GRAPH_Y + T24_GRAPH_H -
                (int16_t)(((v - minT) / range) * T24_GRAPH_H);

    // Clamp dentro l'area
    if (y < T24_GRAPH_Y)
      y = T24_GRAPH_Y;
    if (y > T24_GRAPH_Y + T24_GRAPH_H)
      y = T24_GRAPH_Y + T24_GRAPH_H;

    // Linea dal punto precedente
    if (prevX >= 0) {

      gfx->drawLine(prevX, prevY, x, y, T24_LINE_COL);
      gfx->drawLine(prevX, prevY - 1, x, y - 1, T24_LINE_COL);
      gfx->drawLine(prevX, prevY + 1, x, y + 1, T24_LINE_COL);
    }

    prevX = x;
    prevY = y;
  }

  gfx->endWrite();

  // === PUNTI DATI (solo sugli anchor) ===
  for (uint8_t i = 0; i < 7; i++) {
    uint8_t anchor = pgm_read_byte(&anchors[i]);
    float v = t24[anchor];
    if (isnan(v))
      continue;

    int16_t x = T24_GRAPH_X + (anchor * T24_GRAPH_W) / 23;
    int16_t y = T24_GRAPH_Y + T24_GRAPH_H -
                (int16_t)(((v - minT) / range) * T24_GRAPH_H);

    if (y < T24_GRAPH_Y)
      y = T24_GRAPH_Y;
    if (y > T24_GRAPH_Y + T24_GRAPH_H)
      y = T24_GRAPH_Y + T24_GRAPH_H;

    // Punto con alone
    gfx->fillCircle(x, y, 5, T24_POINT_COL);
    gfx->drawCircle(x, y, 6, T24_LINE_COL);
  }

  // === LEGENDA MIN/MAX ===
  gfx->setTextSize(TEXT_SCALE);
  gfx->setTextColor(COL_TEXT);

  char legendBuf[32];
  fmtTemp(buf, sizeof(buf), (int16_t)mn);

  if (it) {
    snprintf_P(legendBuf, sizeof(legendBuf), PSTR("Min %s  Max "), buf);
  } else {
    snprintf_P(legendBuf, sizeof(legendBuf), PSTR("Low %s  High "), buf);
  }

  fmtTemp(buf, sizeof(buf), (int16_t)mx);
  strcat(legendBuf, buf);

  int16_t legendW = strlen(legendBuf) * BASE_CHAR_W * TEXT_SCALE;
  gfx->setCursor((480 - legendW) / 2, T24_LABEL_Y + 24);
  gfx->print(legendBuf);
}
