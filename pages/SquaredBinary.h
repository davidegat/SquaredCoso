#pragma once

#include <Arduino.h>
#include <time.h>
#include "../handlers/globals.h"

// Colori binario
static const uint16_t BIN_BG   = 0x0000; // nero puro
static const uint16_t BIN_ON   = 0x07E0; // verde acceso terminale
static const uint16_t BIN_OFF  = 0x0180; // verde molto scuro
static const uint16_t BIN_TEXT = 0x07E0;

// ---------------------------------------------------------------------------
// Disegna un quadrato binario
// ---------------------------------------------------------------------------
static inline void drawBitBox(int x, int y, int size, bool on)
{
  uint16_t col = on ? BIN_ON : BIN_OFF;
  gfx->fillRect(x, y, size, size, col);
  gfx->drawRect(x, y, size, size, BIN_ON);
}

// ---------------------------------------------------------------------------
// Pagina Clock Binario
// ---------------------------------------------------------------------------
inline void pageBinaryClock()
{
  // ----------- ORA LOCALE -----------
  time_t now;
  struct tm t;
  time(&now);
  localtime_r(&now, &t);

  uint8_t H = t.tm_hour;
  uint8_t M = t.tm_min;

  // ----------- SFONDO -----------
  gfx->fillScreen(BIN_BG);

  // ----------- PARAMETRI -----------
  const int box   = 48;    // dimensione quadrati
  const int gap   = 20;
  const int totalW_hrs = 4 * box + 3 * gap;
  const int totalW_min = 6 * box + 5 * gap;

  const int hrsX = (480 - totalW_hrs) / 2;
  const int minX = (480 - totalW_min) / 2;

  const int hrsY = 90;     // metà superiore
  const int minY = 270;    // metà inferiore

  // ----------- ORE (4 bit: 8 4 2 1) -----------
  const int Hbits[4] = {8,4,2,1};

  for (int i = 0; i < 4; i++) {
    int x = hrsX + i * (box + gap);
    bool on = H & Hbits[i];
    drawBitBox(x, hrsY, box, on);

    // etichetta decimale
    gfx->setTextColor(BIN_TEXT, BIN_BG);
    gfx->setTextSize(2);
    gfx->setCursor(x + (box/2 - BASE_CHAR_W), hrsY + box + 12);
    gfx->print(Hbits[i]);
  }

  // ----------- MINUTI (6 bit: 32 16 8 4 2 1) -----------
  const int Mbits[6] = {32,16,8,4,2,1};

  for (int i = 0; i < 6; i++) {
    int x = minX + i * (box + gap);
    bool on = M & Mbits[i];
    drawBitBox(x, minY, box, on);

    gfx->setTextColor(BIN_TEXT, BIN_BG);
    gfx->setTextSize(2);
    gfx->setCursor(x + (box/2 - BASE_CHAR_W), minY + box + 12);
    gfx->print(Mbits[i]);
  }
}

