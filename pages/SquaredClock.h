/*
===============================================================================
   SQUARED — PAGINA "CLOCK"
   Descrizione: Orologio fullscreen stile “Midnight”, con sfondo geometrico,
                arco decorativo, layout HH/MM gigante, badge giorno e data.
   Autore: Davide “gat” Nasato
   Repository: https://github.com/davidegat/SquaredCoso
   Licenza: CC BY-NC 4.0
===============================================================================
*/

#pragma once

#include <Arduino.h>
#include <time.h>

extern void drawBoldMain(int16_t x, int16_t y, const String &raw);
extern void drawBoldMain(int16_t x, int16_t y, const String &raw,
                         uint8_t scale);

extern const uint16_t COL_BG;
extern const uint16_t COL_TEXT;
extern const uint16_t COL_ACCENT1;

extern Arduino_RGB_Display *gfx;

extern const int PAGE_X;
extern const int PAGE_Y;
extern const int BASE_CHAR_W;
extern const int BASE_CHAR_H;
extern const int TEXT_SCALE;

extern void drawHLine(int y);

extern String g_lang;

// ---------------------------------------------------------------------------
// PALETTE
// ---------------------------------------------------------------------------
static const uint16_t CLK_BG_DARK = 0x0000;   // Nero
static const uint16_t CLK_BG_SUBTLE = 0x0841; // Grigio molto scuro per pattern
static const uint16_t CLK_WHITE = 0xFFFF;     // Bianco
static const uint16_t CLK_GRAY = 0x9CD3;      // Grigio chiaro
static const uint16_t CLK_GRAY_DIM = 0x4A69;  // Grigio medio
static const uint16_t CLK_ACCENT = 0xFE00;    // Arancione vivace
static const uint16_t CLK_ACCENT2 = 0x07FF;   // Cyan
static const uint16_t CLK_RING_BG = 0x18E3;   // Anello sfondo

// ============================================================================
// HELPER: Disegna sfondo con pattern geometrico
// ============================================================================
static void drawClockBackground() {
  // Sfondo nero solido
  gfx->fillScreen(CLK_BG_DARK);

  // Pattern: cerchi concentrici in basso a destra (decorativi)
  for (int i = 5; i >= 1; i--) {
    int radius = 60 + i * 45;
    gfx->drawCircle(480 + 40, 480 + 40, radius, CLK_BG_SUBTLE);
  }

  // Pattern: cerchi concentrici in alto a sinistra
  for (int i = 3; i >= 1; i--) {
    int radius = 30 + i * 35;
    gfx->drawCircle(-30, -30, radius, CLK_BG_SUBTLE);
  }

  // Linea diagonale decorativa sottile
  for (int i = 0; i < 3; i++) {
    gfx->drawLine(0, 350 + i * 40, 130 + i * 40, 480, CLK_BG_SUBTLE);
  }
}

// ============================================================================
// HELPER: Disegna arco decorativo attorno all'ora
// ============================================================================
static void drawDecorativeArc(int cx, int cy, int r, int thickness,
                              uint16_t color) {
  // Simula un arco nella parte superiore
  for (int t = 0; t < thickness; t++) {
    int currentR = r + t;
    // Disegna solo la parte superiore (da -45° a +45° circa)
    for (int angle = -50; angle <= 50; angle++) {
      float rad = angle * 3.14159 / 180.0;
      int x = cx + (int)(currentR * sin(rad));
      int y = cy - (int)(currentR * cos(rad));
      if (x >= 0 && x < 480 && y >= 0 && y < 480) {
        gfx->drawPixel(x, y, color);
      }
    }
  }
}

// ============================================================================
// CLOCK FULLSCREEN - MINIMAL BOLD LAYOUT
// ============================================================================
inline void pageClock() {
  time_t now;
  struct tm t;
  time(&now);
  localtime_r(&now, &t);

  // ---------------------------------------------------------------------------
  // buffer ora e data
  // ---------------------------------------------------------------------------
  char bufH[3];
  char bufM[3];
  char bufD[16];

  snprintf(bufH, sizeof(bufH), "%02d", t.tm_hour);
  snprintf(bufM, sizeof(bufM), "%02d", t.tm_min);

  snprintf(bufD, sizeof(bufD), "%02d/%02d/%04d", t.tm_mday, t.tm_mon + 1,
           t.tm_year + 1900);

  // ---------------------------------------------------------------------------
  // SFONDO CON PATTERN
  // ---------------------------------------------------------------------------
  drawClockBackground();

  // ---------------------------------------------------------------------------
  // ARCO DECORATIVO SUPERIORE
  // ---------------------------------------------------------------------------
  drawDecorativeArc(240, 200, 180, 3, CLK_ACCENT);
  drawDecorativeArc(240, 200, 190, 2, CLK_GRAY_DIM);

  // ---------------------------------------------------------------------------
  // OROLOGIO GIGANTE - Layout verticale HH / MM
  // ---------------------------------------------------------------------------
  const int scale = 14;
  const int cw = BASE_CHAR_W * scale;
  const int ch = BASE_CHAR_H * scale;

  // Centro schermo
  const int centerX = 240;

  // ORE - parte superiore
  const int hoursY = 70;
  gfx->setTextSize(scale);
  gfx->setTextColor(CLK_WHITE);

  int hoursW = 2 * cw;
  gfx->setCursor(centerX - hoursW / 2, hoursY);
  gfx->print(bufH);

  // Linea separatrice orizzontale con accent
  const int sepY = hoursY + ch + 8;
  gfx->fillRect(centerX - 100, sepY, 200, 4, CLK_ACCENT);

  // Pallini decorativi ai lati della linea
  gfx->fillCircle(centerX - 110, sepY + 2, 6, CLK_ACCENT);
  gfx->fillCircle(centerX + 110, sepY + 2, 6, CLK_ACCENT);

  // MINUTI - parte inferiore
  const int minsY = sepY + 16;
  gfx->setTextSize(scale);
  gfx->setTextColor(CLK_GRAY);

  gfx->setCursor(centerX - hoursW / 2, minsY);
  gfx->print(bufM);

  // ---------------------------------------------------------------------------
  // BADGE GIORNO SETTIMANA
  // ---------------------------------------------------------------------------
  static const char it_wd[7][12] PROGMEM = {"Domenica",  "Lunedi",  "Martedi",
                                            "Mercoledi", "Giovedi", "Venerdi",
                                            "Sabato"};
  static const char en_wd[7][12] PROGMEM = {"Sunday",    "Monday",   "Tuesday",
                                            "Wednesday", "Thursday", "Friday",
                                            "Saturday"};

  char wd[12];
  if (g_lang == "it")
    strcpy_P(wd, it_wd[t.tm_wday]);
  else
    strcpy_P(wd, en_wd[t.tm_wday]);

  const int wdScale = 2;
  const int wdW = strlen(wd) * BASE_CHAR_W * wdScale;
  const int badgePadX = 20;
  const int badgePadY = 10;
  const int badgeW = wdW + badgePadX * 2;
  const int badgeH = BASE_CHAR_H * wdScale + badgePadY * 2;
  const int badgeX = centerX - badgeW / 2;
  const int badgeY = 355;

  // Sfondo badge
  gfx->fillRoundRect(badgeX, badgeY, badgeW, badgeH, badgeH / 2, CLK_RING_BG);
  gfx->drawRoundRect(badgeX, badgeY, badgeW, badgeH, badgeH / 2, CLK_ACCENT);

  // Testo giorno
  gfx->setTextSize(wdScale);
  gfx->setTextColor(CLK_WHITE);
  gfx->setCursor(badgeX + badgePadX, badgeY + badgePadY);
  gfx->print(wd);

  // ---------------------------------------------------------------------------
  // DATA
  // ---------------------------------------------------------------------------
  const int dateY = 420;
  const int dateScale = 3;
  const int dateW = strlen(bufD) * BASE_CHAR_W * dateScale;

  gfx->setTextSize(dateScale);
  gfx->setTextColor(CLK_GRAY_DIM);
  gfx->setCursor(centerX - dateW / 2, dateY);
  gfx->print(bufD);
}
