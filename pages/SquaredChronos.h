#pragma once

#include "../handlers/globals.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <time.h>

extern Arduino_RGB_Display *gfx;
extern const uint16_t COL_TEXT, COL_ACCENT1, COL_ACCENT2, COL_DIVIDER;
extern const int PAGE_X, PAGE_Y, TEXT_SCALE;
extern void drawHeader(const String &);

static constexpr int16_t CH_LEFT = 24;
static constexpr int16_t CH_RIGHT = 456;
static constexpr int16_t CH_BAR_W = CH_RIGHT - CH_LEFT;
static constexpr int16_t CH_BAR_H = 14;
static constexpr int16_t CH_ROW_H = 36;
static constexpr int16_t CH_ROW_H_C = 58;

static constexpr int16_t SEC_HUMAN_Y = 70;
static constexpr int16_t SEC_COSMIC_Y = 270;
static constexpr int16_t SEC_UNIV_Y = 400;

static constexpr uint16_t CH_BG = 0x0000;
static constexpr uint16_t CH_BAR_BG = 0x18C3;
static constexpr uint16_t CH_SUBTLE = 0x4A49;

static constexpr float AGE_UNIVERSE_GA = 13.8f;
static constexpr float AGE_EARTH_GA = 4.54f;
static constexpr float AGE_SUN_GA = 4.60f;
static constexpr float SUN_MAIN_SEQUENCE_GA = 10.0f;

static constexpr float PCT_EARTH = AGE_EARTH_GA / AGE_UNIVERSE_GA;
static constexpr float PCT_SUN = AGE_SUN_GA / SUN_MAIN_SEQUENCE_GA;

static const uint8_t DAYS_MONTH[] PROGMEM = {31, 28, 31, 30, 31, 30,
                                             31, 31, 30, 31, 30, 31};

struct ChProg {
  float day, week, month, year, century;
};

static void calcProgress(ChProg &p) {
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);

  const int16_t year = t.tm_year + 1900;
  const bool leap = ((year & 3) == 0 && (year % 100 != 0)) || (year % 400 == 0);

  const int32_t secDay = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
  p.day = secDay * (1.0f / 86400.0f);

  int8_t dow = t.tm_wday ? t.tm_wday : 7;
  p.week = ((dow - 1) + p.day) * (1.0f / 7.0f);

  uint8_t daysM = pgm_read_byte(&DAYS_MONTH[t.tm_mon]);
  if (t.tm_mon == 1 && leap)
    daysM = 29;
  p.month = ((t.tm_mday - 1) + p.day) / (float)daysM;

  p.year = (t.tm_yday + p.day) / (leap ? 366.0f : 365.0f);

  // Civil centuries start in years ending in 01, not 00.
  const int16_t centuryStart = ((year - 1) / 100) * 100 + 1;
  p.century = ((year - centuryStart) + p.year) * 0.01f;
}

static inline int16_t txtW(uint8_t len, uint8_t sz) { return len * 6 * sz; }

static inline void pctStr(char *buf, float pct, bool dec) {
  int16_t v = (int16_t)(pct * (dec ? 1000.0f : 100.0f));
  if (dec) {
    buf[0] = '0' + (v / 100);
    buf[1] = '0' + ((v / 10) % 10);
    buf[2] = '.';
    buf[3] = '0' + (v % 10);
    buf[4] = '%';
    buf[5] = 0;
  } else {
    if (v >= 100) {
      buf[0] = '1';
      buf[1] = '0';
      buf[2] = '0';
      buf[3] = '%';
      buf[4] = 0;
    } else if (v >= 10) {
      buf[0] = '0' + (v / 10);
      buf[1] = '0' + (v % 10);
      buf[2] = '%';
      buf[3] = 0;
    } else {
      buf[0] = '0' + v;
      buf[1] = '%';
      buf[2] = 0;
    }
  }
}

static void drawBar(int16_t x, int16_t y, int16_t w, int16_t h, float pct,
                    uint16_t col) {
  gfx->fillRoundRect(x, y, w, h, 4, CH_BAR_BG);

  int16_t fill = (int16_t)(pct * (w - 4));
  if (fill > 0) {
    gfx->fillRoundRect(x + 2, y + 2, fill, h - 4, 3, col);
  }

  gfx->drawRoundRect(x, y, w, h, 4, CH_SUBTLE);
}

static void drawRowH(int16_t y, const __FlashStringHelper *label, float pct,
                     uint16_t col) {
  char buf[8];
  pctStr(buf, pct, false);

  gfx->setTextColor(COL_TEXT);
  gfx->setCursor(CH_LEFT, y);
  gfx->print(label);

  gfx->setTextColor(col);
  gfx->setCursor(CH_RIGHT - txtW(strlen(buf), TEXT_SCALE), y);
  gfx->print(buf);

  drawBar(CH_LEFT, y + 16, CH_BAR_W, CH_BAR_H, pct, col);
}

static void drawRowC(int16_t y, const __FlashStringHelper *label,
                     const __FlashStringHelper *sub, float pct, uint16_t col) {
  char buf[8];
  pctStr(buf, pct, true);

  gfx->setTextColor(COL_TEXT);
  gfx->setCursor(CH_LEFT, y);
  gfx->print(label);

  gfx->setTextColor(col);
  gfx->setCursor(CH_RIGHT - txtW(strlen(buf), TEXT_SCALE), y);
  gfx->print(buf);

  drawBar(CH_LEFT, y + 16, CH_BAR_W, CH_BAR_H + 2, pct, col);

  gfx->setTextColor(CH_SUBTLE);
  gfx->setCursor(CH_LEFT, y + 38);
  gfx->print(sub);
}

static void drawHuman(const ChProg &p) {
  gfx->setTextSize(TEXT_SCALE);

  int16_t y = SEC_HUMAN_Y;
  drawRowH(y, F("Today"), p.day, COL_ACCENT1);
  drawRowH(y + CH_ROW_H, F("Week"), p.week, COL_ACCENT1);
  drawRowH(y + CH_ROW_H * 2, F("Month"), p.month, COL_ACCENT1);
  drawRowH(y + CH_ROW_H * 3, F("Year"), p.year, COL_ACCENT2);
  drawRowH(y + CH_ROW_H * 4, F("Century"), p.century, COL_ACCENT2);
}

static void drawCosmic() {
  gfx->setTextSize(TEXT_SCALE);

  drawRowC(SEC_COSMIC_Y, F("Earth / Universe"), F("4.54 / 13.8 Ga"),
           PCT_EARTH, COL_ACCENT2);
  drawRowC(SEC_COSMIC_Y + CH_ROW_H_C, F("Sun main seq."), F("4.6 / ~10 Ga"),
           PCT_SUN, COL_ACCENT1);
}

static void drawUniverse() {
  const int16_t y = SEC_UNIV_Y;

  gfx->setTextSize(TEXT_SCALE);
  gfx->setTextColor(COL_TEXT);
  gfx->setCursor(CH_LEFT, y);
  gfx->print(F("Universe Timeline"));

  gfx->setTextColor(CH_SUBTLE);
  gfx->setCursor(CH_RIGHT - txtW(7, TEXT_SCALE), y);
  gfx->print(F("13.8 Ga"));

  const int16_t lineY = y + 30;
  gfx->fillRoundRect(CH_LEFT, lineY, CH_BAR_W, 8, 3, CH_BAR_BG);
  gfx->drawRoundRect(CH_LEFT, lineY, CH_BAR_W, 8, 3, CH_SUBTLE);

  const int16_t mBigBang = CH_LEFT + 4;
  const float solarSystemFrac = (AGE_UNIVERSE_GA - AGE_SUN_GA) / AGE_UNIVERSE_GA;
  const int16_t mSolar = CH_LEFT + (int16_t)(CH_BAR_W * solarSystemFrac);
  const int16_t mNow = CH_RIGHT - 8;

  const int16_t my = lineY + 4;

  gfx->fillCircle(mBigBang, my, 5, COL_ACCENT1);
  gfx->fillCircle(mSolar, my, 4, COL_ACCENT2);
  gfx->fillCircle(mNow, my, 6, COL_ACCENT1);
  gfx->drawCircle(mNow, my, 8, COL_ACCENT1);

  const int16_t labelY = lineY + 18;
  gfx->setTextSize(1);

  gfx->setTextColor(COL_ACCENT1);
  gfx->setCursor(mBigBang - 4, labelY);
  gfx->print(F("Big Bang"));

  gfx->setTextColor(COL_ACCENT2);
  gfx->setCursor(mSolar - 28, labelY);
  gfx->print(F("Solar sys."));

  gfx->setTextColor(COL_ACCENT1);
  gfx->setCursor(mNow - 10, labelY);
  gfx->print(F("Now"));
}

inline void pageChronos() {
  gfx->fillScreen(CH_BG);
  drawHeader(F("CHRONOS"));

  gfx->setTextSize(TEXT_SCALE);

  ChProg p;
  calcProgress(p);

  drawHuman(p);
  drawCosmic();
  drawUniverse();
}
