/*
===============================================================================
   SQUARED — PAGINA "CALENDAR ICS"
   Descrizione: Parser ICS leggero (solo eventi di oggi), slot compatti
                senza allocazioni persistenti, rendering max 3 eventi
                ordinati per prossimità temporale.

                La pagina è in stato sperimentale e potrebbe non funzionare
                con tutti i formati di calendario.

   Autore: Davide “gat” Nasato
   Repository: https://github.com/davidegat/SquaredCoso
   Licenza: CC BY-NC 4.0
===============================================================================
*/

#pragma once

#include "../images/cal_icon.h"
#include <Arduino.h>
#include <time.h>

// configurazione globale e helper
extern String g_lang;
extern String g_ics;

extern void todayYMD(String &ymd);
extern bool httpGET(const String &url, String &body, uint32_t timeoutMs);
extern int indexOfCI(const String &src, const String &key, int from);

extern void drawHeader(const String &title);
extern void drawBoldMain(int16_t x, int16_t y, const String &raw,
                         uint8_t scale);
extern String sanitizeText(const String &in);

extern Arduino_RGB_Display *gfx;

extern const int PAGE_X;
extern const int PAGE_Y;
extern const int CHAR_H;
extern const int TEXT_SCALE;

extern const uint16_t COL_TEXT;
extern const uint16_t COL_BG;

extern void drawHLine(int y);

// ---------------------------------------------------------------------------
// Struttura evento ICS
// ---------------------------------------------------------------------------
struct CalItem {
  char when[16];
  char summary[64];
  time_t ts;
  bool allDay;
  bool used;
};

static CalItem cal[3];

// ---------------------------------------------------------------------------
// Reset degli slot eventi
// ---------------------------------------------------------------------------
static inline void resetCal() {
  for (uint8_t i = 0; i < 3; i++) {
    cal[i].when[0] = '\0';
    cal[i].summary[0] = '\0';
    cal[i].ts = 0;
    cal[i].allDay = false;
    cal[i].used = false;
  }
}

// ---------------------------------------------------------------------------
// Estrae il testo dopo ':' fino a fine riga, con trim
// ---------------------------------------------------------------------------
static inline String extractAfterColon(const String &src, int pos) {
  int c = src.indexOf(':', pos);
  if (c < 0)
    return "";
  int e = src.indexOf('\n', c + 1);
  if (e < 0)
    e = src.length();

  String t = src.substring(c + 1, e);
  t.trim();
  return t;
}

// ---------------------------------------------------------------------------
// Verifica se un timestamp ICS (YYYYMMDD...) è di oggi
// ---------------------------------------------------------------------------
static inline bool isTodayStamp(const String &dtstamp, const String &todayYmd) {
  return (dtstamp.length() >= 8) && dtstamp.startsWith(todayYmd);
}

// ---------------------------------------------------------------------------
// Converte stampo ICS in stringa orario human-readable o "all day"
// ---------------------------------------------------------------------------
static inline void humanTimeFromStamp(const String &stamp, String &out) {
  if (stamp.length() >= 15 && stamp[8] == 'T') {
    out = stamp.substring(9, 11) + ":" + stamp.substring(11, 13);
  } else {
    out = (g_lang == "it" ? "tutto il giorno" : "all day");
  }
}

// ---------------------------------------------------------------------------
// Copia String in buffer char[] con troncamento sicuro
// ---------------------------------------------------------------------------
static inline void copyToBuf(const String &s, char *buf, size_t bufLen) {
  if (!bufLen)
    return;
  size_t n = s.length();
  if (n >= bufLen)
    n = bufLen - 1;
  for (size_t i = 0; i < n; i++)
    buf[i] = (char)s[i];
  buf[n] = '\0';
}

// ---------------------------------------------------------------------------
// fetchICS
// - scarica il file ICS da g_ics
// - raccoglie solo gli eventi di oggi in cal[0..2]
// ---------------------------------------------------------------------------
bool fetchICS() {
  resetCal();

  if (!g_ics.length())
    return true;

  String body;
  if (!httpGET(g_ics, body, 15000))
    return false;

  String today;
  todayYMD(today);

  uint8_t idx = 0;
  int p = 0;

  while (idx < 3) {
    int b = body.indexOf("BEGIN:VEVENT", p);
    if (b < 0)
      break;

    int e = body.indexOf("END:VEVENT", b);
    if (e < 0)
      break;

    String blk = body.substring(b, e);

    int ds = indexOfCI(blk, "DTSTART", 0);
    if (ds < 0) {
      p = e + 10;
      continue;
    }

    String rawStart = extractAfterColon(blk, ds);
    if (!isTodayStamp(rawStart, today)) {
      p = e + 10;
      continue;
    }

    int ss = indexOfCI(blk, "SUMMARY", 0);
    String summary = (ss >= 0) ? extractAfterColon(blk, ss) : "";
    if (!summary.length()) {
      p = e + 10;
      continue;
    }

    String whenStr;
    humanTimeFromStamp(rawStart, whenStr);

    summary = sanitizeText(summary);
    whenStr = sanitizeText(whenStr);

    struct tm tt = {};
    if (rawStart.length() >= 8) {
      tt.tm_year = rawStart.substring(0, 4).toInt() - 1900;
      tt.tm_mon = rawStart.substring(4, 6).toInt() - 1;
      tt.tm_mday = rawStart.substring(6, 8).toInt();
    }

    bool hasTime = (rawStart.length() >= 15 && rawStart[8] == 'T');
    if (hasTime) {
      tt.tm_hour = rawStart.substring(9, 11).toInt();
      tt.tm_min = rawStart.substring(11, 13).toInt();
    } else {
      tt.tm_hour = 0;
      tt.tm_min = 0;
    }

    copyToBuf(whenStr, cal[idx].when, sizeof(cal[idx].when));
    copyToBuf(summary, cal[idx].summary, sizeof(cal[idx].summary));
    cal[idx].ts = mktime(&tt);
    cal[idx].allDay = !hasTime;
    cal[idx].used = true;

    idx++;
    p = e + 10;
  }

  return true;
}

// ---------------------------------------------------------------------------
// pageCalendar
// - mostra max 3 eventi di oggi in ordine di “prossimità” temporale
// - usa solo indici su cal[] per ridurre RAM
// ---------------------------------------------------------------------------
void pageCalendar() {
  drawHeader(g_lang == "it" ? "Oggi" : "Today");

  // ---------------------------------------------------
  // Icona calendario (RLE)
  // ---------------------------------------------------
  const int ICON_MARGIN = 20;
  int iconX = 480 - ICON_MARGIN - CAL_ICON_WIDTH;
  int iconY = PAGE_Y - 5;

  drawRLE(iconX, iconY, CAL_ICON_WIDTH, CAL_ICON_HEIGHT, cal_icon,
          sizeof(cal_icon) / sizeof(RLERun));

  // ---------------------------------------------------
  // Numero del giorno centrato nell'icona (TextSize 3, sfondo bianco)
  // ---------------------------------------------------
  time_t now;
  struct tm t;
  time(&now);
  localtime_r(&now, &t);

  int day = t.tm_mday;
  String dayStr = String(day);

  // dimensione carattere
  gfx->setTextSize(3);
  gfx->setTextColor(COL_BG, COL_TEXT); // testo colore tema, sfondo bianco

  // centro dell'icona
  int iconCenterX = iconX + (CAL_ICON_WIDTH / 2);
  int iconCenterY = iconY + (CAL_ICON_HEIGHT / 2);

  int textW = dayStr.length() * 18; // 6px * 3
  int textH = 24;                   // circa 8px * 3

  int textX = iconCenterX - (textW / 2);
  int textY = iconCenterY - (textH / 2) + 4;

  gfx->setCursor(textX, textY);
  gfx->print(dayStr);

  // reset dimensione testo
  gfx->setTextSize(TEXT_SCALE);

  int y = PAGE_Y;

  struct Row {
    uint8_t idx;
    time_t ts;
    long delta;
    bool allDay;
  };

  Row rows[3];
  uint8_t n = 0;

  for (uint8_t i = 0; i < 3; i++) {
    if (!cal[i].used || cal[i].summary[0] == '\0')
      continue;

    long d = cal[i].allDay ? 0 : (long)difftime(cal[i].ts, now);
    if (d < 0)
      d = 86400;

    rows[n].idx = i;
    rows[n].ts = cal[i].ts;
    rows[n].delta = d;
    rows[n].allDay = cal[i].allDay;
    n++;
  }

  if (!n) {
    drawBoldMain(PAGE_X, y + CHAR_H,
                 (g_lang == "it" ? "Nessun evento" : "No events"),
                 TEXT_SCALE + 1);
    return;
  }

  for (uint8_t i = 1; i < n; i++) {
    Row key = rows[i];
    int8_t j = i - 1;
    while (j >= 0 && rows[j].delta > key.delta) {
      rows[j + 1] = rows[j];
      j--;
    }
    rows[j + 1] = key;
  }

  for (uint8_t i = 0; i < n; i++) {
    CalItem &ev = cal[rows[i].idx];

    drawBoldMain(PAGE_X, y, String(ev.summary), TEXT_SCALE + 1);

    y += (CHAR_H * (TEXT_SCALE + 1)) - 6;

    gfx->setTextSize(2);
    gfx->setTextColor(COL_TEXT, COL_BG);
    gfx->setCursor(PAGE_X, y);
    gfx->print(ev.when);

    y += CHAR_H * 2 + 4;
    gfx->setTextSize(TEXT_SCALE);

    if (i < n - 1) {
      drawHLine(y);
      y += 12;
    }

    if (y > 450)
      break;
  }
}
