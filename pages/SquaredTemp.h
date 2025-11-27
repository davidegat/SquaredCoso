#pragma once

// ---------------------------------------------------------------------------
//  SQUARED — PAGINA "TEMPERATURA 24H"
//  - Fetch da Open-Meteo (7 valori giornalieri mean-temp)
//  - Interpolazione lineare a 24 campioni orari
//  - Grafico statico 24h (nessuna animazione, nessuna alloc dinamica)
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "../handlers/globals.h"
#include "../handlers/jsonhelpers.h"

// ---------------------------------------------------------------------------
// EXTERN dal core (display, layout, colori, helpers HTTP / geo)
// ---------------------------------------------------------------------------
extern Arduino_RGB_Display* gfx;

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

extern void drawHeader(const String& title);
extern void drawBoldMain(int16_t x, int16_t y, const String& raw);
extern void drawBoldMain(int16_t x, int16_t y, const String& raw, uint8_t scale);

extern int indexOfCI(const String&, const String&, int from);
extern bool httpGET(const String& url, String& out, uint32_t timeout);
extern bool geocodeIfNeeded();

extern String g_lang;
extern String g_lat;
extern String g_lon;

// ---------------------------------------------------------------------------
// Buffer campioni temperatura 24h (interpolati)
// ---------------------------------------------------------------------------
static float t24[24] = { NAN };

// ---------------------------------------------------------------------------
// Stub animazione (API compatibile, ma disattivata)
// ---------------------------------------------------------------------------
static int temp24_progress = 23;
static uint32_t temp24_last = 0;

void resetTemp24Anim() {
  temp24_progress = 23;
}

void tickTemp24Anim() {
  (void)temp24_last;  // animazione disattivata, manteniamo la firma
}


// ---------------------------------------------------------------------------
// Fetch da Open-Meteo: 7 valori giornalieri → 24 campioni interpolati
// ---------------------------------------------------------------------------
static bool fetchTemp24() {

  for (int i = 0; i < 24; i++) t24[i] = NAN;

  if (!geocodeIfNeeded()) return false;

  String url =
    "https://api.open-meteo.com/v1/forecast?"
    "latitude="
    + g_lat + "&longitude=" + g_lon + "&daily=temperature_2m_mean"
                                      "&forecast_days=7&timezone=auto";

  String body;
  if (!httpGET(url, body, 12000))
    return false;

  int posDaily = indexOfCI(body, "\"daily\"", 0);
  if (posDaily < 0) return false;

  int posMean = indexOfCI(body, "\"temperature_2m_mean\"", posDaily);
  if (posMean < 0) return false;

  int lb = body.indexOf('[', posMean);
  if (lb < 0) return false;

  int rb = findMatchingBracket(body, lb);
  if (rb < 0) return false;

  String arr = body.substring(lb + 1, rb);

  float seven[7];
  for (int i = 0; i < 7; i++) seven[i] = NAN;

  int idx = 0;
  int start = 0;
  const int L = arr.length();

  while (idx < 7 && start < L) {

    int s = start;
    while (s < L && !((arr[s] >= '0' && arr[s] <= '9') || arr[s] == '-'))
      s++;
    if (s >= L) break;

    int e = s;
    while (e < L && ((arr[e] >= '0' && arr[e] <= '9') || arr[e] == '.' || arr[e] == '-'))
      e++;

    seven[idx] = arr.substring(s, e).toFloat();
    idx++;
    start = e + 1;
  }

  if (!idx) return false;

  // distribuzione 7 anchor lungo i 24 campioni
  int anchors[7];
  for (int i = 0; i < 7; i++) {
    int a = (int)round(i * (24.0f / 6.0f));
    anchors[i] = (a > 23) ? 23 : a;
  }

  for (int i = 0; i < 7; i++)
    t24[anchors[i]] = seven[i];

  // interpolazione lineare tra anchor consecutivi
  for (int a = 0; a < 6; a++) {
    int x1 = anchors[a];
    int x2 = anchors[a + 1];
    float y1 = seven[a];
    float y2 = seven[a + 1];

    int dx = x2 - x1;
    if (dx < 1) continue;

    float dy = (y2 - y1) / dx;

    for (int k = 1; k < dx; k++)
      t24[x1 + k] = y1 + dy * k;
  }

  return true;
}

// ---------------------------------------------------------------------------
// Pagina GRAFICO 24H — statico, nessuna animazione
// ---------------------------------------------------------------------------
static void pageTemp24() {

  drawHeader(
    g_lang == "it" ? "Evoluzione temperatura"
                   : "Temperature trend");

  float mn = 9999.0f;
  float mx = -9999.0f;

  for (int i = 0; i < 24; i++) {
    float v = t24[i];
    if (!isnan(v)) {
      if (v < mn) mn = v;
      if (v > mx) mx = v;
    }
  }

  if (mn > mx || mn == 9999.0f) {
    drawBoldMain(
      PAGE_X,
      PAGE_Y + CHAR_H,
      (g_lang == "it" ? "Dati non disponibili"
                      : "Data not available"),
      TEXT_SCALE);
    return;
  }

  // range VA CALCOLATO QUI, PRIMA DI USARLO PER LE ETICHETTE
  float range = mx - mn;
  if (range < 0.1f) range = 0.1f;

  const int X = 20;
  const int W = 440;
  const int Y = PAGE_Y + 40;
  const int H = 300;

  gfx->fillRect(X - 2, Y - 2, W + 4, H + 60, COL_BG);
  gfx->drawRect(X, Y, W, H, COL_ACCENT2);

  // -----------------------------------------------------------
  // GRID: linee verticali per i 7 giorni (ancore 0–23)
  // -----------------------------------------------------------
  {
    const int anchors[7] = { 0, 4, 8, 12, 16, 20, 23 };
    gfx->startWrite();
    for (int i = 0; i < 7; i++) {
      int xx = X + (anchors[i] * W) / 23;
      gfx->drawLine(xx, Y, xx, Y + H, COL_ACCENT2);
    }
    gfx->endWrite();
  }

  // -----------------------------------------------------------
  // Etichette temperatura min/max + livelli intermedi
  // -----------------------------------------------------------
  {
    gfx->setTextSize(2);
    gfx->setTextColor(COL_TEXT, COL_BG);

    // Etichetta MIN
    String tMin = String((int)mn) + "c";
    gfx->setCursor(X - 40, Y + H - 10);
    gfx->print(tMin);

    // Etichetta MAX
    String tMax = String((int)mx) + "c";
    gfx->setCursor(X - 40, Y - 4);
    gfx->print(tMax);

    // Tre livelli intermedi (25%, 50%, 75%)
    float y25 = Y + H - (H * 0.25f);
    float y50 = Y + H - (H * 0.50f);
    float y75 = Y + H - (H * 0.75f);

    int mid25 = (int)(mn + range * 0.25f);
    int mid50 = (int)(mn + range * 0.50f);
    int mid75 = (int)(mn + range * 0.75f);

    gfx->setCursor(X - 40, (int)(y25 - 6));
    gfx->print(String(mid25) + "c");

    gfx->setCursor(X - 40, (int)(y50 - 6));
    gfx->print(String(mid50) + "c");

    gfx->setCursor(X - 40, (int)(y75 - 6));
    gfx->print(String(mid75) + "c");
  }

  // -----------------------------------------------------------
  // Etichette dei 7 giorni reali (IT/EN), basate sulla data locale
  // -----------------------------------------------------------
  {
    const char* days_it[7] = {
      "Lun", "Mar", "Mer", "Gio", "Ven", "Sab", "Dom"
    };
    const char* days_en[7] = {
      "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
    };

    const char** D = (g_lang == "it" ? days_it : days_en);

    time_t now = time(nullptr);
    struct tm info;
    localtime_r(&now, &info);

    int base = (info.tm_wday == 0 ? 6 : info.tm_wday - 1);

    const int anchors[7] = { 0, 4, 8, 12, 16, 20, 23 };

    gfx->setTextColor(COL_ACCENT2, COL_BG);
    gfx->setTextSize(1);

    for (int i = 0; i < 7; i++) {
      int dayIndex = (base + i) % 7;
      int xx = X + (anchors[i] * W) / 23;

      gfx->setCursor(xx - 10, Y + H + 48);
      gfx->print(D[dayIndex]);
    }

    gfx->setTextSize(TEXT_SCALE);
  }

  // LEGEND vecchia commentata: inutile ora
  // String legend =
  //   (g_lang == "it"
  //      ? String((int)mn) + " min - " + String((int)mx) + " max"
  //      : String((int)mn) + " low - " + String((int)mx) + " high");
  // drawBoldMain(PAGE_X, Y + H + 20, legend, TEXT_SCALE + 1);

  // range è già stato calcolato sopra

  for (int i = 0; i < 23; i++) {

    float v1 = t24[i];
    float v2 = t24[i + 1];
    if (isnan(v1) || isnan(v2)) continue;

    int x1 = X + (i * W) / 23;
    int x2 = X + ((i + 1) * W) / 23;

    int y1 = Y + H - (int)(((v1 - mn) / range) * H);
    int y2 = Y + H - (int)(((v2 - mn) / range) * H);

    gfx->drawLine(x1, y1, x2, y2, COL_ACCENT1);
  }
}
	
