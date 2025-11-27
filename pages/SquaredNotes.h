#pragma once
/*
   SquaredCoso – Pagina "SquaredNotes" (post-it)
   - Sfondo giallo
   - Testo nero stile scritto a mano (IndieFlower)
   - Header giallo medio-chiaro
   - Triangolo decorativo
   - Word-wrap automatico + auto-scaling con GFXfont
*/

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "../handlers/globals.h"
#include "../fonts/IndieFlower_Regular20pt7b.h"

extern Arduino_RGB_Display* gfx;

extern const int PAGE_W;
extern const int PAGE_H;
extern const int TEXT_SCALE;

extern String g_note;
extern String sanitizeText(const String& in);

// ---------------------------------------------------------------------------
// Colori post-it
// ---------------------------------------------------------------------------
static const uint16_t NOTE_BG   = 0xFFE0; // giallo chiaro (sfondo)
static const uint16_t NOTE_DARK = 0xFEC0; // giallo medio-chiaro header/triangolo
static const uint16_t NOTE_TEXT = 0x0000; // nero

// Altezza header
static const int NOTE_HEADER_H = 40;


// ---------------------------------------------------------------------------
// Calcolo bounding box (con GFXfont serve SEMPRE)
// ---------------------------------------------------------------------------
static void measureText(const String& s, int& w, int& h) {
  int16_t x1, y1;
  uint16_t ww, hh;
  gfx->getTextBounds(s, 0, 0, &x1, &y1, &ww, &hh);
  w = ww;
  h = hh;
}


// ---------------------------------------------------------------------------
// Pagina NOTA
// ---------------------------------------------------------------------------
void pageNotes()
{
  // sfondo
  gfx->fillScreen(NOTE_BG);

  // header
  gfx->fillRect(0, 0, 480, NOTE_HEADER_H, NOTE_DARK);

  // triangolo decorativo
  int x0 = 0,   y0 = 480;
  int x1 = 0,   y1 = 380;
  int x2 = 100, y2 = 480;

  for (int y = y1; y <= y0; y++) {
    float t = float(y - y1) / float(y0 - y1);
    int xm = x1 + int((x2 - x1) * t);
    gfx->drawFastHLine(x1, y, xm, NOTE_DARK);
  }

  // testo
  String txt = sanitizeText(g_note);
  if (!txt.length()) txt = (g_lang == "it" ? "(vuoto)" : "(empty)");

  // imposta font IndieFlower
  gfx->setFont(&IndieFlower_Regular20pt7b);
  gfx->setTextColor(NOTE_TEXT, NOTE_BG);
  gfx->setTextSize(1);   // SEMPRE 1 con GFXfont

  // area disponibile
  const int MAX_W = PAGE_W - 40;
  const int MAX_H = PAGE_H - NOTE_HEADER_H - 40;

  // tentiamo varie scale virtuali (simulazione ingrandimento)
  // NB: Siccome GFXfont non supporta scaling, simuliamo usando più font oppure scale=1.
  // Qui lasciamo scale=1 fisso e adattiamo word-wrap.
  //
  // Se davvero vuoi scaling fluido, devo convertire 3 versioni del font:
  //  - 18pt
  //  - 20pt
  //  - 24pt
  // e fare auto-scelta. Posso farlo, dimmelo.

  // word-wrap
  std::vector<String> lines;
  {
    std::vector<String> words;
    int start = 0;
    while (true) {
      int sp = txt.indexOf(' ', start);
      if (sp < 0) { words.push_back(txt.substring(start)); break; }
      words.push_back(txt.substring(start, sp));
      start = sp + 1;
    }

    String current;
    for (size_t i = 0; i < words.size(); i++) {
      String test = current.length() ? current + " " + words[i] : words[i];

      int w,h;
      measureText(test, w, h);

      if (w > MAX_W && current.length()) {
        lines.push_back(current);
        current = words[i];
      } else {
        current = test;
      }
    }
    if (current.length()) lines.push_back(current);
  }

  // misura altezza totale
  int line_w, line_h;
  measureText("Ag", line_w, line_h);
  int total_h = line_h * lines.size();

  // centratura verticale
  int start_y = NOTE_HEADER_H + (MAX_H - total_h) / 2;
  if (start_y < NOTE_HEADER_H + 20) start_y = NOTE_HEADER_H + 20;

  // stampa
  int y = start_y;
  for (auto& L : lines) {
    int w,h;
    measureText(L, w, h);
    int x = (480 - w) / 2;

    gfx->setCursor(x, y + h);
    gfx->print(L);

    y += h;
    if (y > 470) break;
  }

  // ripristina font di default
  gfx->setFont(NULL);
}

