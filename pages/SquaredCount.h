#pragma once

#include <Arduino.h>
#include <time.h>
#include "../handlers/globals.h"

// ---------------------------------------------------------------------------
// SquaredCoso – Pagina "COUNTDOWN MULTIPLI" + snake perimetrale
// - pageCountdowns()    → elenca fino a 8 countdown ordinati per data
// - tickCountdownSnake()→ anima il bordo del pannello con scia luminosa
// ---------------------------------------------------------------------------

// display e palette condivisi
extern Arduino_RGB_Display* gfx;

extern const uint16_t COL_BG;
extern const uint16_t COL_HEADER;
extern const uint16_t COL_ACCENT1;
extern const uint16_t COL_ACCENT2;

extern const int PAGE_X;
extern const int PAGE_Y;
extern const int TEXT_SCALE;
extern const int CHAR_H;

extern void   drawHeader(const String& title);
extern void   drawBoldMain(int16_t x, int16_t y, const String& raw, uint8_t scale);
extern String sanitizeText(const String& in);

extern CDEvent cd[8];
extern String  g_lang;

String formatShortDate(time_t t);

// ---------------------------------------------------------------------------
// Parsing ISO "YYYY-MM-DD HH:MM" → time_t locale
// ---------------------------------------------------------------------------
static bool parseISOToTimeT(const String& iso, time_t& out)
{
  if (iso.length() < 16) return false;

  struct tm t = {};

  t.tm_year =
      (iso.charAt(0) - '0') * 1000 +
      (iso.charAt(1) - '0') * 100  +
      (iso.charAt(2) - '0') * 10   +
       iso.charAt(3) - '0'         - 1900;

  t.tm_mon  = (iso.charAt(5) - '0') * 10 + (iso.charAt(6) - '0') - 1;
  t.tm_mday = (iso.charAt(8) - '0') * 10 + (iso.charAt(9) - '0');

  t.tm_hour = (iso.charAt(11) - '0') * 10 + (iso.charAt(12) - '0');
  t.tm_min  = (iso.charAt(14) - '0') * 10 + (iso.charAt(15) - '0');

  out = mktime(&t);
  return out > 0;
}

// ---------------------------------------------------------------------------
// Format del tempo rimanente in stringa compatta (IT/EN)
// ---------------------------------------------------------------------------
static String formatDelta(time_t target)
{
  time_t now;
  time(&now);

  long diff = long(target - now);

  if (diff <= 0)
    return (g_lang == "it" ? "scaduto" : "expired");

  long d = diff / 86400L; diff %= 86400L;
  long h = diff / 3600L;  diff %= 3600L;
  long m = diff / 60L;

  char buf[24];
  if (d > 0)
    snprintf(buf, sizeof(buf), "%ldg %02ldh %02ldm", d, h, m);
  else
    snprintf(buf, sizeof(buf), "%02ldh %02ldm", h, m);

  return String(buf);
}

// ---------------------------------------------------------------------------
// Pagina COUNTDOWN: raccoglie, ordina e rende visivamente gli eventi
// ---------------------------------------------------------------------------
void pageCountdowns()
{
  drawHeader("Countdown");
  int y = PAGE_Y;

  struct Row {
    const char* name;
    time_t      when;
  };

  Row list[8];
  int n = 0;

  // raccolta countdown validi
  for (int i = 0; i < 8; i++) {
    if (cd[i].name.isEmpty() || cd[i].whenISO.isEmpty()) continue;

    time_t t;
    if (!parseISOToTimeT(cd[i].whenISO, t)) continue;

    list[n].name = cd[i].name.c_str();
    list[n].when = t;
    n++;
  }

  if (n == 0) {
    drawBoldMain(
      PAGE_X,
      y + CHAR_H,
      (g_lang == "it" ? "Nessun countdown" : "No countdowns"),
      TEXT_SCALE
    );
    return;
  }

  // insertion sort per data crescente (pochi elementi → costo minimo)
  for (int i = 1; i < n; i++) {
    Row key = list[i];
    int j = i - 1;
    while (j >= 0 && list[j].when > key.when) {
      list[j + 1] = list[j];
      j--;
    }
    list[j + 1] = key;
  }

  gfx->setTextSize(TEXT_SCALE);

  // rendering righe countdown (nome + data corta + delta)
  for (int i = 0; i < n; i++) {

    String nm = sanitizeText(list[i].name);
    String dt = formatShortDate(list[i].when);
    String dl = formatDelta(list[i].when);

    gfx->setTextColor(COL_ACCENT1, COL_BG);
    gfx->setCursor(PAGE_X, y + CHAR_H);
    gfx->print(nm);

    int y2 = y + CHAR_H * 2 + 4;
    gfx->setTextColor(COL_HEADER, COL_BG);
    gfx->setCursor(PAGE_X, y2);
    gfx->print("(");
    gfx->print(dt);
    gfx->print(")");

    gfx->setTextColor(COL_ACCENT2, COL_BG);
    gfx->setCursor(PAGE_X + 180, y2);
    gfx->print(dl);

    y += CHAR_H * 3 + 10;

    if (i < n - 1)
      gfx->drawFastHLine(PAGE_X, y, 440, COL_HEADER);

    if (y > 460) break;
  }
}

// ---------------------------------------------------------------------------
// Snake perimetrale con scia (bordo 480x480, spessore 4px)
// ---------------------------------------------------------------------------
static const int snake_w   = 4;
static const int snake_spd = 4;

static const int disp_w = 480;
static const int disp_h = 480;

static const int snake_loop =
    (disp_w * 2) + (disp_h * 2);

struct SnakeSeg {
  int     x, y, w, h;
  uint8_t life;
};

static const int MAX_TRAIL = 120;
static SnakeSeg  trail[MAX_TRAIL];
static int       trail_len = 0;
static int       snake_pos = 0;

// ---------------------------------------------------------------------------
// Fading di un colore RGB565 in base a life (0–255)
// ---------------------------------------------------------------------------
// life: 0-255  (255 = colore pieno, 0 = colore di sfondo)
static uint16_t fade565(uint16_t fg, uint8_t life)
{
  uint16_t bg = COL_BG;

  uint8_t fg_r = (fg >> 11) & 0x1F;
  uint8_t fg_g = (fg >> 5)  & 0x3F;
  uint8_t fg_b =  fg        & 0x1F;

  uint8_t bg_r = (bg >> 11) & 0x1F;
  uint8_t bg_g = (bg >> 5)  & 0x3F;
  uint8_t bg_b =  bg        & 0x1F;

  uint8_t r = bg_r + (((fg_r - bg_r) * life) >> 8);
  uint8_t g = bg_g + (((fg_g - bg_g) * life) >> 8);
  uint8_t b = bg_b + (((fg_b - bg_b) * life) >> 8);

  return (r << 11) | (g << 5) | b;
}


// ---------------------------------------------------------------------------
// Calcola rettangolo corrente del serpente lungo il bordo dello schermo
// ---------------------------------------------------------------------------
static void getSnakePos(int p, int &x, int &y, int &w, int &h)
{
  if (p < disp_w) {
    x = p;
    y = 0;
    w = snake_spd;
    h = snake_w;
  }
  else if (p < disp_w + disp_h) {
    x = disp_w - snake_w;
    y = p - disp_w;
    w = snake_w;
    h = snake_spd;
  }
  else if (p < disp_w * 2 + disp_h) {
    x = (disp_w * 2 + disp_h) - p;
    y = disp_h - snake_w;
    w = snake_spd;
    h = snake_w;
  }
  else {
    int t = p - (disp_w * 2 + disp_h);
    x = 0;
    y = disp_h - t - snake_spd;
    w = snake_w;
    h = snake_spd;
  }
}

// ---------------------------------------------------------------------------
// Tick animazione snake: avanza, aggiorna coda, applica fading
// ---------------------------------------------------------------------------
void tickCountdownSnake()
{
  snake_pos += snake_spd;
  if (snake_pos >= snake_loop) snake_pos = 0;

  int x, y, w, h;
  getSnakePos(snake_pos, x, y, w, h);

  if (trail_len < MAX_TRAIL) {
    trail[trail_len].x    = x;
    trail[trail_len].y    = y;
    trail[trail_len].w    = w;
    trail[trail_len].h    = h;
    trail[trail_len].life = 255;
    trail_len++;
  } else {
    for (int i = 1; i < MAX_TRAIL; i++)
      trail[i - 1] = trail[i];

    trail[MAX_TRAIL - 1].x    = x;
    trail[MAX_TRAIL - 1].y    = y;
    trail[MAX_TRAIL - 1].w    = w;
    trail[MAX_TRAIL - 1].h    = h;
    trail[MAX_TRAIL - 1].life = 255;
  }

  for (int i = 0; i < trail_len; i++) {

    SnakeSeg &s = trail[i];

    if (s.life == 0) {
      gfx->fillRect(s.x, s.y, s.w, s.h, COL_BG);
      continue;
    }

    uint16_t col = fade565(COL_ACCENT1, s.life);

    gfx->fillRect(s.x, s.y, s.w, s.h, col);

    if (s.life > 4)
      s.life -= 4;
    else
      s.life = 0;
  }
}

