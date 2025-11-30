/*
===============================================================================
   SQUARED — DISPLAY HELPERS (Core grafico)
   Descrizione: Inizializzazione pannello ST7701, gestione backlight con
                fade-in/out, splash RLE 480×480, rendering testi (bold,
                header, paragraph), sanitizzazione UTF-8, escape JSON,
                helper data/ora, mini-parser JSON KV e funzioni paging
                (mask ↔ array) condivise da tutte le pagine.
   Autore: Davide “gat” Nasato
   Repository: https://github.com/davidegat/SquaredCoso
   Licenza: CC BY-NC 4.0
===============================================================================
*/

#pragma once

#include "globals.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// Display condiviso
extern Arduino_RGB_Display *gfx;

/* ============================================================================
   BACKLIGHT (PWM su pin 38)
============================================================================ */
#define GFX_BL 38
#define PWM_CHANNEL 0
#define PWM_FREQ 1000
#define PWM_BITS 8

/* ============================================================================
   drawRLE — Decodifica immagini RLE 480×480 RGB565 (splash/logo)
   Ogni "run" contiene: colore + conteggio pixel consecutivi
============================================================================ */
inline void drawRLE(int x, int y, int w, int h, const RLERun *data,
                    size_t runs) {
  static uint16_t lineBuf[480];

  int runIndex = 0;
  int runOffset = 0;

  for (int py = 0; py < h; py++) {
    int filled = 0;

    while (filled < w && runIndex < (int)runs) {
      uint16_t col = data[runIndex].color;
      uint16_t cnt = data[runIndex].count - runOffset;
      int toCopy = min<int>(cnt, w - filled);

      for (int i = 0; i < toCopy; i++)
        lineBuf[filled + i] = col;

      filled += toCopy;

      if (toCopy == cnt) {
        runIndex++;
        runOffset = 0;
      } else
        runOffset += toCopy;
    }

    gfx->draw16bitRGBBitmap(x, y + py, lineBuf, w, 1);
  }
}

/* ============================================================================
   FADE VELOCI (transizioni pagina)
============================================================================ */
inline void quickFadeOut() {
  const int maxDuty = (1 << PWM_BITS) - 1;
  for (int i = 50; i >= 0; i--) {
    ledcWrite(PWM_CHANNEL, (maxDuty * i) / 50);
    delay(2);
  }
}

inline void quickFadeIn() {
  const int maxDuty = (1 << PWM_BITS) - 1;
  for (int i = 0; i <= 50; i++) {
    ledcWrite(PWM_CHANNEL, (maxDuty * i) / 50);
    delay(2);
  }
}

/* ============================================================================
   Kickstart pannello ST7701 (inizializzazione sequenza)
============================================================================ */
inline void panelKickstart() {
  delay(50);
  gfx->begin();
  gfx->setRotation(0);
  delay(120);
  gfx->displayOn();
  delay(20);
}

/* ============================================================================
   SPLASH pieni
============================================================================ */
inline void showSplashFadeInOnly(const RLERun *rle, size_t runs,
                                 uint16_t holdMs) {
  gfx->fillScreen(RGB565_BLACK);
  drawRLE(0, 0, 480, 480, rle, runs);

  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_BITS);
  ledcAttachPin(GFX_BL, PWM_CHANNEL);

  const int maxDuty = (1 << PWM_BITS) - 1;

  // fade-in lento
  for (int i = 0; i <= 500; i++) {
    ledcWrite(PWM_CHANNEL, (maxDuty * i) / 500);
    delay(5);
  }

  delay(holdMs);
}

inline void splashFadeOut() {
  const int maxDuty = (1 << PWM_BITS) - 1;

  for (int i = 250; i >= 0; i--) {
    ledcWrite(PWM_CHANNEL, (maxDuty * i) / 250);
    delay(5);
  }

  gfx->fillScreen(COL_BG);
}

/* ============================================================================
   FADE-IN UI (dopo splash o cambio pagina)
============================================================================ */
inline void fadeInUI() {
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_BITS);
  ledcAttachPin(GFX_BL, PWM_CHANNEL);

  const int maxDuty = (1 << PWM_BITS) - 1;

  ledcWrite(PWM_CHANNEL, 0);
  for (int i = 0; i <= 250; i++) {
    ledcWrite(PWM_CHANNEL, (maxDuty * i) / 250);
    delay(5);
  }
}

/* ============================================================================
   Splash “cosino” per rotazione pagina (fade-in + hold)
============================================================================ */
inline void showCycleSplash(const RLERun *data, size_t runs, uint16_t holdMs) {
  gfx->fillScreen(RGB565_BLACK);
  drawRLE(0, 0, 480, 480, data, runs);

  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_BITS);
  ledcAttachPin(GFX_BL, PWM_CHANNEL);

  const int maxDuty = (1 << PWM_BITS) - 1;

  for (int i = 0; i <= 500; i++) {
    ledcWrite(PWM_CHANNEL, (maxDuty * i) / 500);
    delay(5);
  }

  delay(holdMs);
}

/* ============================================================================
   sanitizeText — converte UTF-8 accentate in ASCII safe
   Usato da: News, Meteo, QOD
============================================================================ */
inline String sanitizeText(const String &in) {
  String out;
  out.reserve(in.length());

  for (int i = 0; i < in.length();) {
    uint8_t c = (uint8_t)in[i];

    if (c < 0x80) {
      out += (char)c;
      i++;
      continue;
    }

    if (c == 0xC3 && i + 1 < in.length()) {
      uint8_t c2 = (uint8_t)in[i + 1];
      switch (c2) {
      case 0xA0:
      case 0xA1:
        out += 'a';
        break;
      case 0xA8:
      case 0xA9:
        out += 'e';
        break;
      case 0xAC:
      case 0xAD:
        out += 'i';
        break;
      case 0xB2:
      case 0xB3:
        out += 'o';
        break;
      case 0xB9:
      case 0xBA:
        out += 'u';
        break;
      default:
        out += '?';
      }
      i += 2;
      continue;
    }

    out += '?';
    i++;
  }

  while (out.indexOf("  ") >= 0)
    out.replace("  ", " ");
  out.trim();
  return out;
}

/* ============================================================================
   decodeJsonUnicode — converte sequenze \uXXXX in UTF-8 reale
   Usato nel parsing OpenAI JSON
============================================================================ */
inline String decodeJsonUnicode(const String &s) {
  String out;
  out.reserve(s.length());

  for (int i = 0; i < s.length(); i++) {
    if (s[i] == '\\' && i + 5 < s.length() && s[i + 1] == 'u') {
      int code = strtol(s.substring(i + 2, i + 6).c_str(), nullptr, 16);

      if (code < 0x80)
        out += char(code);
      else if (code < 0x800) {
        out += char(0xC0 | (code >> 6));
        out += char(0x80 | (code & 0x3F));
      } else {
        out += char(0xE0 | (code >> 12));
        out += char(0x80 | ((code >> 6) & 0x3F));
        out += char(0x80 | (code & 0x3F));
      }

      i += 5;
    } else
      out += s[i];
  }
  return out;
}

/* ============================================================================
   jsonEscape — escape per body OpenAI
============================================================================ */
inline String jsonEscape(const String &s) {
  String o;
  o.reserve(s.length() + 8);

  for (char c : s) {
    if (c == '\\')
      o += "\\\\";
    else if (c == '\"')
      o += "\\\"";
    else if (c == '\n')
      o += "\\n";
    else if (c == '\r')
      o += "\\r";
    else
      o += c;
  }
  return o;
}

/* ============================================================================
   TEXT RENDERING — bold offset 4px e paragrafi word-wrap
============================================================================ */
inline void drawBoldTextColored(int16_t x, int16_t y, const String &raw,
                                uint16_t fg, uint16_t bg,
                                uint8_t scale = TEXT_SCALE) {
  String s = sanitizeText(raw);

  gfx->setTextSize(scale);
  gfx->setTextColor(fg, bg);

  gfx->setCursor(x + 1, y);
  gfx->print(s);
  gfx->setCursor(x, y + 1);
  gfx->print(s);
  gfx->setCursor(x + 1, y + 1);
  gfx->print(s);
  gfx->setCursor(x, y);
  gfx->print(s);
}

inline void drawBoldMain(int16_t x, int16_t y, const String &raw,
                         uint8_t scale) {
  drawBoldTextColored(x, y, raw, COL_TEXT, COL_BG, scale);
}
inline void drawBoldMain(int16_t x, int16_t y, const String &raw) {
  drawBoldMain(x, y, raw, TEXT_SCALE);
}

/* ============================================================================
   drawHeader — barra superiore con titolo pagina + ora corrente
============================================================================ */
inline String getFormattedDateTime(); // forward

inline void drawHeader(const String &titleRaw) {
  gfx->fillRect(0, 0, 480, 50, COL_HEADER);

  drawBoldTextColored(16, 20, sanitizeText(titleRaw), COL_TEXT, COL_HEADER);

  String dt = getFormattedDateTime();
  if (dt.length()) {
    int tw = dt.length() * BASE_CHAR_W * TEXT_SCALE;
    drawBoldTextColored(480 - tw - 16, 20, dt, COL_ACCENT1, COL_HEADER);
  }
}

/* ============================================================================
   drawHLine — linea orizzontale divisoria UI
============================================================================ */
inline void drawHLine(int y) {
  gfx->drawLine(PAGE_X, y, PAGE_X + PAGE_W, y, COL_DIVIDER);
}

/* ============================================================================
   drawParagraph — testo multi-linea con word-wrap
   NO background, solo colore testo
============================================================================ */
inline void drawParagraph(int16_t x, int16_t y, int16_t w, const String &text,
                          uint8_t scale) {
  int maxChars = w / (BASE_CHAR_W * scale);
  if (maxChars < 8)
    maxChars = 8;

  gfx->setTextSize(scale);
  gfx->setTextColor(COL_TEXT, COL_TEXT); // nessuno sfondo visibile

  String s = sanitizeText(text);
  int start = 0;

  while (start < s.length()) {
    int len = min(maxChars, (int)s.length() - start);
    int cut = start + len;

    // cerca spazio precedente per evitare spezzature brutte
    if (cut < s.length()) {
      int sp = s.lastIndexOf(' ', cut);
      if (sp > start)
        cut = sp;
    }

    String line = s.substring(start, cut);
    line.trim();

    gfx->setCursor(x, y);
    gfx->print(line);

    y += BASE_CHAR_H * scale + 6;
    start = (cut < s.length() && s[cut] == ' ') ? cut + 1 : cut;

    if (y > 470)
      break;
  }
}

/* ============================================================================
   DATE HELPERS
============================================================================ */
inline String getFormattedDateTime() {
  extern bool g_timeSynced;
  if (!g_timeSynced)
    return "";

  time_t now;
  struct tm t;
  time(&now);
  localtime_r(&now, &t);

  char buf[32];
  snprintf(buf, sizeof(buf), "%02d/%02d - %02d:%02d", t.tm_mday, t.tm_mon + 1,
           t.tm_hour, t.tm_min);
  return String(buf);
}

inline void todayYMD(String &ymd) {
  time_t now;
  struct tm t;
  time(&now);
  localtime_r(&now, &t);

  char buf[16];
  snprintf(buf, sizeof(buf), "%04d%02d%02d", t.tm_year + 1900, t.tm_mon + 1,
           t.tm_mday);
  ymd = buf;
}

inline String formatShortDate(time_t tt) {
  struct tm t;
  localtime_r(&tt, &t);
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d/%02d", t.tm_mday, t.tm_mon + 1);
  return String(buf);
}

/* ============================================================================
   JSON HELPERS — mini-parser manuali
============================================================================ */
inline bool jsonFindStringKV(const String &body, const String &key, int from,
                             String &outVal) {
  String k = "\"" + key + "\"";
  int p = body.indexOf(k, from);
  if (p < 0)
    return false;
  p = body.indexOf(':', p);
  if (p < 0)
    return false;

  int q1 = body.indexOf('"', p + 1);
  if (q1 < 0)
    return false;
  int q2 = body.indexOf('"', q1 + 1);
  if (q2 < 0)
    return false;

  outVal = body.substring(q1 + 1, q2);
  return true;
}

inline bool jsonFindIntKV(const String &body, const String &key, int from,
                          int &outVal) {
  String k = "\"" + key + "\"";
  int p = body.indexOf(k, from);
  if (p < 0)
    return false;
  p = body.indexOf(':', p);
  if (p < 0)
    return false;

  int s = p + 1;
  while (s < body.length() && isspace(body[s]))
    s++;

  int e = s;
  while (e < body.length() && isdigit(body[e]))
    e++;

  if (e <= s)
    return false;

  outVal = body.substring(s, e).toInt();
  return true;
}

/* ============================================================================
   PAGING HELPERS — gestione pagine attive
============================================================================ */
extern bool g_show[];
extern int g_page;

inline uint32_t pagesMaskFromArray() {
  uint32_t m = 0;
  for (int i = 0; i < PAGES; i++)
    if (g_show[i])
      m |= (1UL << i);
  return m;
}

inline void pagesArrayFromMask(uint32_t m) {
  for (int i = 0; i < PAGES; i++)
    g_show[i] = (m & (1UL << i)) != 0;

  bool any = false;
  for (int i = 0; i < PAGES; i++)
    if (g_show[i]) {
      any = true;
      break;
    }

  if (!any) {
    for (int i = 0; i < PAGES; i++)
      g_show[i] = false;
    g_show[P_CLOCK] = true;
  }
}

inline int firstEnabledPage() {
  for (int i = 0; i < PAGES; i++)
    if (g_show[i])
      return i;
  return -1;
}

inline bool advanceToNextEnabled() {
  int f = firstEnabledPage();
  if (f < 0)
    return false;

  for (int k = 0; k < PAGES; k++) {
    g_page = (g_page + 1) % PAGES;
    if (g_show[g_page])
      return true;
  }
  return false;
}

inline void ensureCurrentPageEnabled() {
  if (g_show[g_page])
    return;

  int f = firstEnabledPage();
  if (f >= 0)
    g_page = f;
  else {
    g_page = P_CLOCK;
    for (int i = 0; i < PAGES; i++)
      g_show[i] = false;
    g_show[P_CLOCK] = true;
  }
}
