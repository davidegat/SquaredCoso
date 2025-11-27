#pragma once

#include <Arduino.h>
#include <time.h>

// font / rendering condivisi
extern void drawBoldMain(int16_t x, int16_t y, const String& raw);
extern void drawBoldMain(int16_t x, int16_t y, const String& raw, uint8_t scale);

extern const uint16_t COL_BG;
extern const uint16_t COL_TEXT;
extern const uint16_t COL_ACCENT1;

extern Arduino_RGB_Display* gfx;

extern const int PAGE_X;
extern const int PAGE_Y;
extern const int BASE_CHAR_W;
extern const int BASE_CHAR_H;
extern const int TEXT_SCALE;

extern void drawHLine(int y);

extern String g_lang;


// ============================================================================
// CLOCK FULLSCREEN 
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

  snprintf(bufD, sizeof(bufD), "%02d/%02d/%04d",
           t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);

  // ---------------------------------------------------------------------------
  // parametri grafici orologio gigante
  // ---------------------------------------------------------------------------
  const int scale  = 14;
  const int cw     = BASE_CHAR_W * scale;
  const int ch     = BASE_CHAR_H * scale;
  const int colonW = cw;
  const int totalW = cw * 2 + colonW + cw * 2;

  // HEIGHTS DEL LAYOUT
  const int H_clock = ch;
  const int H_sep   = 20 + 2;      // 20 px + linea
  const int H_date  = BASE_CHAR_H * 5;
  const int H_wday  = BASE_CHAR_H * 4;

  const int blockH = H_clock + H_sep + H_date + 28 + H_wday;

  // CENTRATURA VERTICALE
  const int blockY = (480 - blockH) >> 1;
  int y = blockY;

  gfx->fillScreen(COL_BG);

  // ---------------------------------------------------------------------------
  // OROLOGIO (HH:MM)
  // ---------------------------------------------------------------------------
  gfx->setTextSize(scale);
  gfx->setTextColor(COL_TEXT, COL_BG);

  const int clockX = (480 - totalW) >> 1;

  // ORE
  gfx->setCursor(clockX, y);
  gfx->print(bufH);

  // DUE PUNTI
  {
    const int colonX = clockX + cw * 2;
    const int dotW   = max(colonW / 6, 4);
    const int dotH   = max(ch / 8, 4);
    const int cx     = colonX + ((colonW - dotW) >> 1);

    const int upY = y + (ch * 27) / 100;
    const int dnY = y + (ch * 62) / 100;

    gfx->fillRect(cx, upY, dotW, dotH, COL_ACCENT1);
    gfx->fillRect(cx, dnY, dotW, dotH, COL_ACCENT1);
  }

  // MINUTI
  gfx->setCursor(clockX + cw * 2 + colonW, y);
  gfx->print(bufM);

  // AGGIORNA Y
  y += H_clock + 20;

  // ---------------------------------------------------------------------------
  // SEPARATORE
  // ---------------------------------------------------------------------------
  drawHLine(y);
  y += 14;

  // ---------------------------------------------------------------------------
  // DATA
  // ---------------------------------------------------------------------------
  gfx->setTextSize(5);
  gfx->setTextColor(COL_TEXT, COL_BG);

  const int dateW = strlen(bufD) * BASE_CHAR_W * 5;
  gfx->setCursor((480 - dateW) >> 1, y);
  gfx->print(bufD);

  y += H_date + 28;

  // ---------------------------------------------------------------------------
  // WEEKDAY
  // ---------------------------------------------------------------------------
  static const char it_wd[7][12] PROGMEM = {
    "Domenica","Lunedi","Martedi","Mercoledi",
    "Giovedi","Venerdi","Sabato"
  };
  static const char en_wd[7][12] PROGMEM = {
    "Sunday","Monday","Tuesday","Wednesday",
    "Thursday","Friday","Saturday"
  };

  char wd[12];
  if (g_lang == "it") strcpy_P(wd, it_wd[t.tm_wday]);
  else               strcpy_P(wd, en_wd[t.tm_wday]);

  gfx->setTextSize(4);
  const int wdW = strlen(wd) * BASE_CHAR_W * 4;

  gfx->setCursor((480 - wdW) >> 1, y);
  gfx->print(wd);

  gfx->setTextSize(TEXT_SCALE);
}


