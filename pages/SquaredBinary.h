#pragma once

#include <Arduino.h>
#include <time.h>
#include "../handlers/globals.h"

// ===========================================================================
// COLORI BINARIO
// - BIN_BG  : colore di sfondo della pagina
// - BIN_ON  : colore dei bit attivi
// - BIN_OFF : colore dei bit inattivi
// - BIN_TEXT: colore del testo (etichette, titoli)
// - BIN_DEC : colore delle cifre decimali sotto i quadrati (più scuro)
// ===========================================================================
static const uint16_t BIN_BG   = 0x0000;
static const uint16_t BIN_ON   = 0x07E0;
static const uint16_t BIN_OFF  = 0x0180;
static const uint16_t BIN_TEXT = 0x07E0;
static const uint16_t BIN_DEC  = 0x0240;   // numeri decimali leggermente più scuri

static bool binary_force_first = true;

// ===========================================================================
// DISEGNO DEL BORDO DI PAGINA
// - Bordo perimetrale sempre verde
// - Spessore 1 px per lato
// ===========================================================================
static inline void drawFixedBorder()
{
  gfx->drawFastHLine(0,   0,   480, BIN_ON); // bordo superiore
  gfx->drawFastHLine(0, 479,  480, BIN_ON); // bordo inferiore
  gfx->drawFastVLine(0,   0,   480, BIN_ON); // bordo sinistro
  gfx->drawFastVLine(479, 0,   480, BIN_ON); // bordo destro
}

// ===========================================================================
// DISEGNO DEL SINGOLO QUADRATO BINARIO
// - "on"  → quadrato verde
// - "off" → quadrato verde scuro
// - contorno sempre verde acceso
// ===========================================================================
static inline void drawBitBox(int x, int y, int size, bool on)
{
  uint16_t col = on ? BIN_ON : BIN_OFF;

  gfx->fillRect(x, y, size, size, col);   // quadrato pieno
  gfx->drawRect(x, y, size, size, BIN_ON); // bordo del quadrato
}

// ===========================================================================
// PAGINA CLOCK BINARIO 12H
// - Mostra ore in formato 12h (8-4-2-1)
// - Mostra minuti (32-16-8-4-2-1)
// - Mostra indicatori AM / PM
// - Ridisegna solo quando cambia ora/minuto (efficienza)
// ===========================================================================
inline void pageBinaryClock()
{
  static bool first_frame = true;
  static uint8_t prevH12 = 255;
  static uint8_t prevM   = 255;
  static uint8_t prevAM  = 255;

  // -------------------------------------------------------------------------
  // LETTURA ORA ATTUALE
  // -------------------------------------------------------------------------
  time_t now;
  struct tm t;
  time(&now);
  localtime_r(&now, &t);

  uint8_t H24 = t.tm_hour;
  uint8_t M   = t.tm_min;

  // Conversione 24h → 12h
  uint8_t H12 = H24 % 12;
  if (H12 == 0) H12 = 12;

  uint8_t isAM = (H24 < 12);

  // -------------------------------------------------------------------------
  // CONTROLLO NECESSITÀ DI RIDISEGNO
  // Ridisegniamo solo se cambia:
  // - ora 12h
  // - minuti
  // - AM/PM
  // - primo avvio
  // -------------------------------------------------------------------------
  bool needRedraw =
      first_frame || binary_force_first ||
      (H12 != prevH12) || (M != prevM) || (isAM != prevAM);

  if (!needRedraw) {
    drawFixedBorder();
    return;
  }

  // Aggiorno lo stato interno
  first_frame        = false;
  binary_force_first = false;
  prevH12            = H12;
  prevM              = M;
  prevAM             = isAM;

  // -------------------------------------------------------------------------
  // RENDER COMPLETO DELLA PAGINA
  // -------------------------------------------------------------------------
  gfx->fillScreen(BIN_BG);
  drawFixedBorder();

  const int box = 48;   // dimensione del quadrato binario
  const int gap = 20;   // spazio tra i quadrati

  // Y: posizioni verticali dei tre blocchi (ore, am/pm, minuti)
  const int hrsY   = 70;
  const int ampmY  = 210;
  const int minY   = 330;

  // Larghezze complessive (quadrati + spazi)
  const int totalW_hrs = 4 * box + 3 * gap;
  const int totalW_min = 6 * box + 5 * gap;

  // X: centrate orizzontalmente
  const int hrsX = (480 - totalW_hrs) / 2;
  const int minX = (480 - totalW_min) / 2;

  gfx->setTextSize(2);

  // -------------------------------------------------------------------------
  // TITOLO "hours"
  // -------------------------------------------------------------------------
  {
    const char* lbl = "hours";
    gfx->setTextColor(BIN_TEXT, BIN_BG);

    int16_t tw = strlen(lbl) * BASE_CHAR_W * 2; // larghezza testo
    int cx = hrsX + totalW_hrs/2 - tw/2;        // centratura orizzontale

    gfx->setCursor(cx, hrsY - 25);
    gfx->print(lbl);
  }

  // -------------------------------------------------------------------------
  // ORE: quadrati + valori 8 4 2 1
  // -------------------------------------------------------------------------
  gfx->setTextColor(BIN_DEC, BIN_BG); // colore decimale più scuro

  const int Hbits[4] = {8,4,2,1};

  for (int i = 0; i < 4; i++) {
    int x = hrsX + i * (box + gap);
    bool on = (H12 & Hbits[i]);

    drawBitBox(x, hrsY, box, on);

    gfx->setCursor(x + (box/2 - BASE_CHAR_W), hrsY + box + 12);
    gfx->print(Hbits[i]);
  }

  // -------------------------------------------------------------------------
  // BLOCCO AM / PM
  // -------------------------------------------------------------------------
  const int center_gap = 80;

  const int cx1 = (480 / 2) - box - (center_gap / 2); // posizione AM
  const int cx2 = (480 / 2) + (center_gap / 2);       // posizione PM

  drawBitBox(cx1, ampmY, box, isAM);
  drawBitBox(cx2, ampmY, box, !isAM);

  gfx->setTextColor(BIN_TEXT, BIN_BG);
  gfx->setCursor(cx1 - 32, ampmY + (box / 2 - 8));
  gfx->print("am");

  gfx->setCursor(cx2 + box + 8, ampmY + (box / 2 - 8));
  gfx->print("pm");

  // -------------------------------------------------------------------------
  // TITOLO "minutes"
  // -------------------------------------------------------------------------
  {
    const char* lbl = "minutes";
    gfx->setTextColor(BIN_TEXT, BIN_BG);

    int16_t tw = strlen(lbl) * BASE_CHAR_W * 2;
    int cx = minX + totalW_min/2 - tw/2;

    gfx->setCursor(cx, minY - 25);
    gfx->print(lbl);
  }

  // -------------------------------------------------------------------------
  // MINUTI: quadrati + valori 32 16 8 4 2 1
  // -------------------------------------------------------------------------
  gfx->setTextColor(BIN_DEC, BIN_BG);

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
// FUNZIONE DI RESET REDISEGNO
// - Forza un ridisegno completo al prossimo ciclo pagina
// ===========================================================================
inline void resetBinaryClockFirstDraw()
{
  binary_force_first = true;
}

