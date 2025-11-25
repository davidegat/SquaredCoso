#pragma once

#include <Arduino.h>
#include <time.h>
#include "../handlers/globals.h"

// ===========================================================================
// COLORI BINARIO
// ===========================================================================
static const uint16_t BIN_BG   = 0x0000;
static const uint16_t BIN_ON   = 0x07E0;
static const uint16_t BIN_OFF  = 0x0180;
static const uint16_t BIN_TEXT = 0x07E0;

static bool binary_force_first = true;

// ===========================================================================
// BORDO FISSO VERDE
// ===========================================================================
static inline void drawFixedBorder()
{
  gfx->drawFastHLine(0,   0,   480, BIN_ON);
  gfx->drawFastHLine(0, 479,  480, BIN_ON);
  gfx->drawFastVLine(0,   0,   480, BIN_ON);
  gfx->drawFastVLine(479, 0,   480, BIN_ON);
}

// ===========================================================================
// QUADRATO BINARIO
// ===========================================================================
static inline void drawBitBox(int x, int y, int size, bool on)
{
  uint16_t col = on ? BIN_ON : BIN_OFF;
  gfx->fillRect(x, y, size, size, col);
  gfx->drawRect(x, y, size, size, BIN_ON);
}

// ===========================================================================
// CLOCK BINARIO 12H
// ===========================================================================
inline void pageBinaryClock()
{
  static bool first_frame = true;
  static uint8_t prevH12 = 255;
  static uint8_t prevM   = 255;
  static uint8_t prevAM  = 255;

  // -------------------------
  // ORA ATTUALE
  // -------------------------
  time_t now;
  struct tm t;
  time(&now);
  localtime_r(&now, &t);

  uint8_t H24 = t.tm_hour;
  uint8_t M   = t.tm_min;

  uint8_t H12 = H24 % 12;
  if (H12 == 0) H12 = 12;

  uint8_t isAM = (H24 < 12) ? 1 : 0;

  bool needRedraw =
      first_frame || binary_force_first ||
      (H12 != prevH12) || (M != prevM) || (isAM != prevAM);

  if (!needRedraw) {
    drawFixedBorder();
    return;
  }

  first_frame        = false;
  binary_force_first = false;
  prevH12            = H12;
  prevM              = M;
  prevAM             = isAM;

  // -------------------------
  // RENDER COMPLETO
  // -------------------------
  gfx->fillScreen(BIN_BG);

  drawFixedBorder();

  const int box = 48;
  const int gap = 20;

  const int hrsY   = 70;    
  const int ampmY  = 210;   
  const int minY   = 330;   

  const int totalW_hrs = 4 * box + 3 * gap;
  const int totalW_min = 6 * box + 5 * gap;

  const int hrsX = (480 - totalW_hrs) / 2;
  const int minX = (480 - totalW_min) / 2;

  gfx->setTextColor(BIN_TEXT, BIN_BG);
  gfx->setTextSize(2);

  // -------------------------
  // TITOLO "hours"
  // -------------------------
  {
    const char* lbl = "hours";
    int16_t tw = strlen(lbl) * BASE_CHAR_W * 2;
    int cx = hrsX + totalW_hrs/2 - tw/2;
    gfx->setCursor(cx, hrsY - 25);
    gfx->print(lbl);
  }

  // -------------------------
  // ORE (quadrati + cifre sotto)
  // -------------------------
  const int Hbits[4] = {8,4,2,1};

  for (int i = 0; i < 4; i++) {
    int x = hrsX + i * (box + gap);
    bool on = (H12 & Hbits[i]);
    drawBitBox(x, hrsY, box, on);

    gfx->setCursor(x + (box/2 - BASE_CHAR_W), hrsY + box + 12);
    gfx->print(Hbits[i]);
  }

  // -------------------------
  // CENTRO AM / PM
  // -------------------------
  const int center_gap = 80;

  const int cx1 = (480 / 2) - box - (center_gap / 2);
  const int cx2 = (480 / 2) + (center_gap / 2);

  drawBitBox(cx1, ampmY, box, isAM == 1);
  drawBitBox(cx2, ampmY, box, isAM == 0);

  gfx->setCursor(cx1 - 32, ampmY + (box / 2 - 8));
  gfx->print("am");

  gfx->setCursor(cx2 + box + 8, ampmY + (box / 2 - 8));
  gfx->print("pm");

  // -------------------------
  // TITOLO "minutes"
  // -------------------------
  {
    const char* lbl = "minutes";
    int16_t tw = strlen(lbl) * BASE_CHAR_W * 2;
    int cx = minX + totalW_min/2 - tw/2;
    gfx->setCursor(cx, minY - 25);
    gfx->print(lbl);
  }

  // -------------------------
  // MINUTI (quadrati + cifre sotto)
// -------------------------
  const int Mbits[6] = {32,16,8,4,2,1};

  for (int i = 0; i < 6; i++) {
    int x = minX + i * (box + gap);
    bool on = (M & Mbits[i]);
    drawBitBox(x, minY, box, on);

    gfx->setCursor(x + (box/2 - BASE_CHAR_W), minY + box + 12);
    gfx->print(Mbits[i]);
  }

  drawFixedBorder();
}

// ===========================================================================
// RESET FLAG DI RIDISEGNO FORZATO
// ===========================================================================
inline void resetBinaryClockFirstDraw()
{
  binary_force_first = true;
}


