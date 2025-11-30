/*
===============================================================================
   SQUARED — PAGINA "COUNTDOWNS" + SNAKE PERIMETRALE
   Descrizione: Gestione countdown multipli con parsing ISO ottimizzato,
                layout a 2 colonne, ordinamento per data, delta compatto
                senza String persistenti, e animazione snake sui bordi.
   Autore: Davide “gat” Nasato
   Repository: https://github.com/davidegat/SquaredCoso
   Licenza: CC BY-NC 4.0
===============================================================================
*/

#pragma once

#include "../handlers/globals.h"
#include <Arduino.h>
#include <time.h>

extern Arduino_RGB_Display *gfx;

extern const uint16_t COL_BG;
extern const uint16_t COL_HEADER;
extern const uint16_t COL_ACCENT1;
extern const uint16_t COL_ACCENT2;

extern const int PAGE_X;
extern const int PAGE_Y;
extern const int TEXT_SCALE;
extern const int CHAR_H;

extern void drawHeader(const String &title);
extern void drawBoldMain(int16_t x, int16_t y, const String &raw,
                         uint8_t scale);
extern String sanitizeText(const String &in);

extern CDEvent cd[8];
extern String g_lang;

String formatShortDate(time_t t);

// ---------------------------------------------------------------------------
// Costanti
// ---------------------------------------------------------------------------
static constexpr int16_t CD_COL_W = 240;
static constexpr int16_t CD_DISP = 480;

// Snake constants
static constexpr int8_t SNK_W = 4;
static constexpr int8_t SNK_SPD = 4;
static constexpr int16_t SNK_LOOP = CD_DISP * 4; // perimetro
static constexpr uint8_t SNK_TRAIL = 120;
static constexpr uint8_t SNK_FADE = 4;

// ---------------------------------------------------------------------------
// Parsing ISO "YYYY-MM-DD HH:MM" → time_t (ottimizzato)
// ---------------------------------------------------------------------------
static bool parseISO(const String &iso, time_t &out) {
  if (iso.length() < 16)
    return false;

  const char *s = iso.c_str();

  struct tm t = {};
  t.tm_year = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 +
              (s[3] - '0') - 1900;
  t.tm_mon = (s[5] - '0') * 10 + (s[6] - '0') - 1;
  t.tm_mday = (s[8] - '0') * 10 + (s[9] - '0');
  t.tm_hour = (s[11] - '0') * 10 + (s[12] - '0');
  t.tm_min = (s[14] - '0') * 10 + (s[15] - '0');

  out = mktime(&t);
  return out > 0;
}

// ---------------------------------------------------------------------------
// Format delta in buffer
// ---------------------------------------------------------------------------
static void fmtDelta(time_t target, char *buf, uint8_t bufSize, bool it) {
  time_t now = time(nullptr);
  int32_t diff = (int32_t)(target - now);

  if (diff <= 0) {
    strcpy_P(buf, it ? PSTR("scaduto") : PSTR("expired"));
    return;
  }

  int16_t d = diff / 86400L;
  diff %= 86400L;
  int8_t h = diff / 3600;
  diff %= 3600;
  int8_t m = diff / 60;

  if (d > 0) {
    snprintf_P(buf, bufSize, PSTR("%dg %02dh %02dm"), d, h, m);
  } else {
    snprintf_P(buf, bufSize, PSTR("%02dh %02dm"), h, m);
  }
}

// ---------------------------------------------------------------------------
// Struttura Row
// ---------------------------------------------------------------------------
struct CDRow {
  int8_t idx; // indice in cd[]
  time_t when;
};

// ---------------------------------------------------------------------------
// Pagina COUNTDOWN
// ---------------------------------------------------------------------------
void pageCountdowns() {
  drawHeader(F("Countdown"));

  const bool it = (g_lang == "it");

  CDRow list[8];
  int8_t n = 0;

  // Raccolta countdown validi
  for (int8_t i = 0; i < 8; i++) {
    if (cd[i].name.isEmpty() || cd[i].whenISO.isEmpty())
      continue;

    time_t t;
    if (!parseISO(cd[i].whenISO, t))
      continue;

    list[n].idx = i;
    list[n].when = t;
    n++;
  }

  if (n == 0) {
    drawBoldMain(PAGE_X, PAGE_Y + CHAR_H,
                 it ? F("Nessun countdown") : F("No countdowns"), TEXT_SCALE);
    return;
  }

  // Sort per tempo crescente (insertion sort)
  for (int8_t i = 1; i < n; i++) {
    CDRow k = list[i];
    int8_t j = i - 1;
    while (j >= 0 && list[j].when > k.when) {
      list[j + 1] = list[j];
      j--;
    }
    list[j + 1] = k;
  }

  // Layout
  const int16_t CELL_H = CHAR_H * 4 + 14;
  const int16_t START_Y = PAGE_Y;

  gfx->setTextSize(TEXT_SCALE);
  gfx->drawFastVLine(CD_COL_W, START_Y, CD_DISP - START_Y - 10, COL_HEADER);

  char deltaBuf[16];

  for (int8_t i = 0; i < n && i < 8; i++) {
    int8_t col = (i < 4) ? 0 : 1;
    int8_t row = (i < 4) ? i : i - 4;

    int16_t colBaseX = col * CD_COL_W;
    int16_t x = colBaseX + PAGE_X;
    int16_t y = START_Y + row * CELL_H;

    // Riferimenti diretti a cd[] (evita copia String)
    const String &nm = cd[list[i].idx].name;
    String dt = formatShortDate(list[i].when);

    fmtDelta(list[i].when, deltaBuf, sizeof(deltaBuf), it);

    // 1) Nome
    gfx->setTextColor(COL_ACCENT1, COL_BG);
    gfx->setCursor(x, y + CHAR_H);
    gfx->print(sanitizeText(nm));

    // 2) Data
    int16_t y_date = y + CHAR_H * 2 + 2;
    gfx->setTextColor(COL_HEADER, COL_BG);
    gfx->setCursor(x, y_date);
    gfx->print('(');
    gfx->print(dt);
    gfx->print(')');

    // 3) Countdown (allineato a destra)
    int16_t y_delta = y + CHAR_H * 3 + 4;
    gfx->setTextColor(COL_ACCENT2, COL_BG);

    int16_t deltaW = strlen(deltaBuf) * 6 * TEXT_SCALE;
    int16_t x_delta = colBaseX + CD_COL_W - deltaW - PAGE_X;

    gfx->setCursor(x_delta, y_delta);
    gfx->print(deltaBuf);

    // Separatore
    if (row < 3) {
      gfx->drawFastHLine(colBaseX + PAGE_X, y + CELL_H - 4,
                         CD_COL_W - 2 * PAGE_X, COL_HEADER);
    }
  }
}

// ---------------------------------------------------------------------------
// Snake
// ---------------------------------------------------------------------------
struct SnkSeg {
  int16_t x, y;
  int8_t w, h;
  uint8_t life;
};

static SnkSeg trail[SNK_TRAIL];
static uint8_t trail_len = 0;
static int16_t snake_pos = 0;

// ---------------------------------------------------------------------------
// Fade color (ottimizzato)
// ---------------------------------------------------------------------------
static inline uint16_t fade565(uint16_t fg, uint8_t life) {
  // Estrai componenti con shift
  uint8_t fg_r = (fg >> 11) & 0x1F;
  uint8_t fg_g = (fg >> 5) & 0x3F;
  uint8_t fg_b = fg & 0x1F;

  uint8_t bg_r = (COL_BG >> 11) & 0x1F;
  uint8_t bg_g = (COL_BG >> 5) & 0x3F;
  uint8_t bg_b = COL_BG & 0x1F;

  // Interpolazione con moltiplicazione e shift
  uint8_t r = bg_r + (((fg_r - bg_r) * life) >> 8);
  uint8_t g = bg_g + (((fg_g - bg_g) * life) >> 8);
  uint8_t b = bg_b + (((fg_b - bg_b) * life) >> 8);

  return (r << 11) | (g << 5) | b;
}

// ---------------------------------------------------------------------------
// Posizione snake sul bordo
// ---------------------------------------------------------------------------
static void getSnakePos(int16_t p, int16_t &x, int16_t &y, int8_t &w,
                        int8_t &h) {
  if (p < CD_DISP) {
    // Top edge →
    x = p;
    y = 0;
    w = SNK_SPD;
    h = SNK_W;
  } else if (p < CD_DISP * 2) {
    // Right edge ↓
    x = CD_DISP - SNK_W;
    y = p - CD_DISP;
    w = SNK_W;
    h = SNK_SPD;
  } else if (p < CD_DISP * 3) {
    // Bottom edge ←
    x = CD_DISP * 3 - p;
    y = CD_DISP - SNK_W;
    w = SNK_SPD;
    h = SNK_W;
  } else {
    // Left edge ↑
    x = 0;
    y = CD_DISP * 4 - p - SNK_SPD;
    w = SNK_W;
    h = SNK_SPD;
  }
}

// ---------------------------------------------------------------------------
// Tick animazione snake
// ---------------------------------------------------------------------------
void tickCountdownSnake() {
  snake_pos += SNK_SPD;
  if (snake_pos >= SNK_LOOP)
    snake_pos = 0;

  int16_t x, y;
  int8_t w, h;
  getSnakePos(snake_pos, x, y, w, h);

  // Aggiungi nuovo segmento
  if (trail_len < SNK_TRAIL) {
    SnkSeg &s = trail[trail_len++];
    s.x = x;
    s.y = y;
    s.w = w;
    s.h = h;
    s.life = 255;
  } else {
    // Shift trail
    memmove(&trail[0], &trail[1], sizeof(SnkSeg) * (SNK_TRAIL - 1));

    SnkSeg &s = trail[SNK_TRAIL - 1];
    s.x = x;
    s.y = y;
    s.w = w;
    s.h = h;
    s.life = 255;
  }

  // Render e fade
  for (uint8_t i = 0; i < trail_len; i++) {
    SnkSeg &s = trail[i];

    if (s.life == 0) {
      gfx->fillRect(s.x, s.y, s.w, s.h, COL_BG);
      continue;
    }

    gfx->fillRect(s.x, s.y, s.w, s.h, fade565(COL_ACCENT1, s.life));

    s.life = (s.life > SNK_FADE) ? s.life - SNK_FADE : 0;
  }
}
