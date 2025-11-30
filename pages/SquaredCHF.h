/*
===============================================================================
   SQUARED — PAGINA "FOREX" + MONEY RAIN FX
   Descrizione: Fetch tassi di cambio via API Frankfurter, rendering compatto
                più animazione Money Rain per la sezione grafica a destra.
   Autore: Davide “gat” Nasato
   Repository: https://github.com/gatonegro/SquaredCoso
   Licenza: CC BY-NC 4.0
===============================================================================
*/

#pragma once

#include "../handlers/globals.h"
#include <Arduino.h>

extern Arduino_RGB_Display *gfx;

extern const uint16_t COL_BG;
extern const uint16_t COL_TEXT;

extern const int PAGE_X;
extern const int PAGE_Y;
extern const int TEXT_SCALE;
extern const int CHAR_H;

extern String g_fiat;

extern void drawHeader(const String &title);
extern String sanitizeText(const String &);
extern bool httpGET(const String &, String &, uint32_t);

extern bool jsonNum(const String &src, const char *key, float &out, int from);

// ---------------------------------------------------------------------------
// Stato FX globale
// ---------------------------------------------------------------------------
double fx_eur = NAN, fx_usd = NAN, fx_gbp = NAN, fx_jpy = NAN;
double fx_cad = NAN, fx_cny = NAN, fx_inr = NAN, fx_chf = NAN;

double fx_prev_eur = NAN, fx_prev_usd = NAN, fx_prev_gbp = NAN;
double fx_prev_jpy = NAN, fx_prev_cad = NAN;
double fx_prev_cny = NAN, fx_prev_inr = NAN, fx_prev_chf = NAN;

// ---------------------------------------------------------------------------
// Formatter numerico compatto
// ---------------------------------------------------------------------------
static inline const char *fmtDoubleBuf(double v, uint8_t dec, char *out) {
  dtostrf(v, 0, dec, out);
  return out;
}

// ---------------------------------------------------------------------------
// Fetch tassi FX dalla REST API (usa jsonNum degli helpers)
// ---------------------------------------------------------------------------
bool fetchFX() {
  if (!g_fiat.length())
    g_fiat = "CHF";

  String body;
  if (!httpGET("https://api.frankfurter.app/latest?from=" + g_fiat +
                   "&to=CHF,EUR,USD,GBP,JPY,CAD,CNY,INR",
               body, 10000))
    return false;

  int base = 0; // non serve cercare un blocco specifico

  auto get = [&](const char *code, double &out) {
    float f = NAN;
    if (jsonNum(body, code, f, base))
      out = (double)f;
    else
      out = NAN;
  };

  get("EUR", fx_eur);
  get("USD", fx_usd);
  get("GBP", fx_gbp);
  get("JPY", fx_jpy);
  get("CAD", fx_cad);
  get("CNY", fx_cny);
  get("INR", fx_inr);
  get("CHF", fx_chf);

  return true;
}

// ===========================================================================
// MONEY RAIN
// ===========================================================================

#define MONEY_COUNT 22

#define AREA_X_MIN 260
#define AREA_X_MAX 479
#define AREA_Y_MIN 80
#define AREA_Y_MAX 479

#define SAFE_M 3

struct MoneyDrop {
  float x, y;
  float vy;
  float phase, amp;
  float rot, drot;
  uint8_t size;
  uint16_t color;
};

static MoneyDrop money[MONEY_COUNT];
static bool moneyInit = false;
static uint32_t moneyLast = 0;

static const uint16_t MONEY_COL[2] = {0x07E0, 0x03E0};

// ---------------------------------------------------------------------------
// Raggio
// ---------------------------------------------------------------------------
static inline int moneyRadius(uint8_t s) {
  const int w = 6 + s * 2;
  const int h = 8 + s * 2;
  float diag = sqrtf(float(w * w + h * h));
  return int(diag * 0.5f) + SAFE_M;
}

// ---------------------------------------------------------------------------
// Draw banconota
// ---------------------------------------------------------------------------
static inline void drawMoney(int cx, int cy, uint8_t s, float rot,
                             uint16_t col) {
  const int w = 6 + s * 2;
  const int h = 8 + s * 2;
  const int hw = w >> 1;
  const int hh = h >> 1;

  float cs = cosf(rot);
  float sn = sinf(rot);

  for (int dy = -hh; dy <= hh; dy++) {

    float ryx = dy * sn;
    float ryy = dy * cs;

    for (int dx = -hw; dx <= hw; dx++) {

      int px = cx + int(dx * cs - ryx);
      int py = cy + int(dx * sn + ryy);

      if (px < AREA_X_MIN || px > AREA_X_MAX)
        continue;
      if (py < AREA_Y_MIN || py > AREA_Y_MAX)
        continue;

      gfx->drawPixel(px, py, col);
    }
  }
}

// ---------------------------------------------------------------------------
// Init singolo money drop
// ---------------------------------------------------------------------------
static inline void moneyInitOne(int i) {
  MoneyDrop &m = money[i];

  m.size = uint8_t(random(0, 3));
  int r = moneyRadius(m.size);

  m.x = random(AREA_X_MIN + r, AREA_X_MAX - r);
  m.y = AREA_Y_MIN - random(40, 140) - r;

  m.vy = 1.2f + random(0, 14) * 0.08f;
  m.amp = 3 + random(0, 6);
  m.phase = random(0, 628) * 0.01f;

  m.rot = random(0, 628) * 0.01f;
  m.drot = 0.03f + random(0, 15) * 0.01f;

  m.color = MONEY_COL[random(0, 2)];
}

// ---------------------------------------------------------------------------
// Tick animazione
// ---------------------------------------------------------------------------
void tickFXDataStream(uint16_t bg) {
  uint32_t now = millis();
  if (now - moneyLast < 33)
    return;
  moneyLast = now;

  if (!moneyInit) {
    moneyInit = true;
    for (int i = 0; i < MONEY_COUNT; i++)
      moneyInitOne(i);
  }

  for (int i = 0; i < MONEY_COUNT; i++) {

    MoneyDrop &m = money[i];
    int r = moneyRadius(m.size);

    drawMoney(int(m.x), int(m.y), m.size, m.rot, bg);

    m.phase += 0.035f;
    float dx = sinf(m.phase) * m.amp;

    m.y += m.vy;
    m.rot += m.drot;

    float nx = m.x + dx;
    float ny = m.y;

    if (ny - r > AREA_Y_MAX) {
      moneyInitOne(i);
      continue;
    }

    float safeL = AREA_X_MIN + r + m.amp;
    float safeR = AREA_X_MAX - r - m.amp;

    if (nx < safeL) {
      nx = safeL;
      m.phase += 0.3f;
    }
    if (nx > safeR) {
      nx = safeR;
      m.phase += 0.3f;
    }

    if (nx < AREA_X_MIN + r)
      nx = AREA_X_MIN + r;
    if (nx > AREA_X_MAX - r)
      nx = AREA_X_MAX - r;

    if (ny < AREA_Y_MIN - r)
      ny = AREA_Y_MIN - r;

    drawMoney(int(nx), int(ny), m.size, m.rot, m.color);

    m.x = nx;
    m.y = ny;
  }
}

// ---------------------------------------------------------------------------
// Stampa una riga FX
// ---------------------------------------------------------------------------
static inline void fxPrintRow(const char *lbl, double prev, double val,
                              uint8_t dec, int &y, uint8_t scale,
                              uint16_t colUp, uint16_t colDown,
                              uint16_t colNeut, char *buf) {
  gfx->setCursor(PAGE_X, y + CHAR_H);
  gfx->setTextColor(COL_TEXT, COL_BG);
  gfx->print(lbl);

  uint16_t col = colNeut;
  if (!isnan(prev) && !isnan(val)) {
    if (val > prev)
      col = colUp;
    else if (val < prev)
      col = colDown;
  }

  gfx->setTextColor(col, COL_BG);
  gfx->setCursor(PAGE_X + 70, y + CHAR_H);

  if (isnan(val))
    gfx->print("--");
  else
    gfx->print(fmtDoubleBuf(val, dec, buf));

  y += CHAR_H * scale + 6;
}

// ---------------------------------------------------------------------------
// Pagina FX
// ---------------------------------------------------------------------------
void pageFX() {
  drawHeader("Exchange " + g_fiat);

  gfx->fillRect(AREA_X_MIN, AREA_Y_MIN, AREA_X_MAX - AREA_X_MIN + 1,
                AREA_Y_MAX - AREA_Y_MIN + 1, COL_BG);

  int y = PAGE_Y;
  const uint8_t scale = TEXT_SCALE + 1;

  const uint16_t COL_UP = 0x0340;
  const uint16_t COL_DOWN = 0x9000;
  const uint16_t COL_NEUT = COL_TEXT;

  gfx->setTextSize(scale);
  char buf[32];

  if (g_fiat != "CHF")
    fxPrintRow("CHF:", fx_prev_chf, fx_chf, 3, y, scale, COL_UP, COL_DOWN,
               COL_NEUT, buf);
  if (g_fiat != "EUR")
    fxPrintRow("EUR:", fx_prev_eur, fx_eur, 3, y, scale, COL_UP, COL_DOWN,
               COL_NEUT, buf);
  if (g_fiat != "USD")
    fxPrintRow("USD:", fx_prev_usd, fx_usd, 3, y, scale, COL_UP, COL_DOWN,
               COL_NEUT, buf);
  if (g_fiat != "GBP")
    fxPrintRow("GBP:", fx_prev_gbp, fx_gbp, 3, y, scale, COL_UP, COL_DOWN,
               COL_NEUT, buf);
  if (g_fiat != "JPY")
    fxPrintRow("JPY:", fx_prev_jpy, fx_jpy, 1, y, scale, COL_UP, COL_DOWN,
               COL_NEUT, buf);
  if (g_fiat != "CAD")
    fxPrintRow("CAD:", fx_prev_cad, fx_cad, 3, y, scale, COL_UP, COL_DOWN,
               COL_NEUT, buf);
  if (g_fiat != "CNY")
    fxPrintRow("CNY:", fx_prev_cny, fx_cny, 3, y, scale, COL_UP, COL_DOWN,
               COL_NEUT, buf);
  if (g_fiat != "INR")
    fxPrintRow("INR:", fx_prev_inr, fx_inr, 3, y, scale, COL_UP, COL_DOWN,
               COL_NEUT, buf);

  fx_prev_chf = fx_chf;
  fx_prev_eur = fx_eur;
  fx_prev_usd = fx_usd;
  fx_prev_gbp = fx_gbp;
  fx_prev_jpy = fx_jpy;
  fx_prev_cad = fx_cad;
  fx_prev_cny = fx_cny;
  fx_prev_inr = fx_inr;
}
