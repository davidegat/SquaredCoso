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

static const uint16_t CLK_BG_DARK = 0x0000;
static const uint16_t CLK_BG_SUBTLE = 0x0841;
static const uint16_t CLK_WHITE = 0xFFFF;
static const uint16_t CLK_GRAY = 0x9CD3;
static const uint16_t CLK_GRAY_DIM = 0x4A69;
static const uint16_t CLK_ACCENT = 0xFE00;
static const uint16_t CLK_ACCENT2 = 0x07FF;
static const uint16_t CLK_RING_BG = 0x18E3;

static void drawClockBackground() {

  gfx->fillScreen(CLK_BG_DARK);

  for (int i = 5; i >= 1; i--) {
    int radius = 60 + i * 45;
    gfx->drawCircle(480 + 40, 480 + 40, radius, CLK_BG_SUBTLE);
  }

  for (int i = 3; i >= 1; i--) {
    int radius = 30 + i * 35;
    gfx->drawCircle(-30, -30, radius, CLK_BG_SUBTLE);
  }

  for (int i = 0; i < 3; i++) {
    gfx->drawLine(0, 350 + i * 40, 130 + i * 40, 480, CLK_BG_SUBTLE);
  }
}

static void drawDecorativeArc(int cx, int cy, int r, int thickness,
                              uint16_t color) {

  for (int t = 0; t < thickness; t++) {
    int currentR = r + t;

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

static int8_t clock_prev_hour = -1;
static int8_t clock_prev_min = -1;
static int8_t clock_prev_mday = -1;
static int8_t clock_prev_mon = -1;
static int16_t clock_prev_year = -1;

static inline void drawClockHourDigits(uint8_t hour) {
  const int scale = 14;
  const int cw = BASE_CHAR_W * scale;
  const int ch = BASE_CHAR_H * scale;
  const int centerX = 240;
  const int hoursY = 70;
  const int hoursW = 2 * cw;

  gfx->fillRect(centerX - hoursW / 2 - 4, hoursY - 4,
                hoursW + 8, ch + 8, CLK_BG_DARK);
  drawDecorativeArc(240, 200, 180, 3, CLK_ACCENT);
  drawDecorativeArc(240, 200, 190, 2, CLK_GRAY_DIM);

  char buf[3];
  snprintf(buf, sizeof(buf), "%02u", (unsigned)hour);
  gfx->setTextSize(scale);
  gfx->setTextColor(CLK_WHITE, CLK_BG_DARK);
  gfx->setCursor(centerX - hoursW / 2, hoursY);
  gfx->print(buf);
}

static inline void drawClockMinuteDigits(uint8_t minute) {
  const int scale = 14;
  const int cw = BASE_CHAR_W * scale;
  const int ch = BASE_CHAR_H * scale;
  const int centerX = 240;
  const int hoursY = 70;
  const int hoursW = 2 * cw;
  const int sepY = hoursY + ch + 8;
  const int minsY = sepY + 16;

  gfx->fillRect(centerX - hoursW / 2 - 4, minsY - 4,
                hoursW + 8, ch + 8, CLK_BG_DARK);

  char buf[3];
  snprintf(buf, sizeof(buf), "%02u", (unsigned)minute);
  gfx->setTextSize(scale);
  gfx->setTextColor(CLK_GRAY, CLK_BG_DARK);
  gfx->setCursor(centerX - hoursW / 2, minsY);
  gfx->print(buf);
}

inline void pageClock() {
  time_t now;
  struct tm t;
  time(&now);
  localtime_r(&now, &t);

  char bufH[3];
  char bufM[3];
  char bufD[16];

  snprintf(bufH, sizeof(bufH), "%02d", t.tm_hour);
  snprintf(bufM, sizeof(bufM), "%02d", t.tm_min);
  snprintf(bufD, sizeof(bufD), "%02d/%02d/%04d", t.tm_mday, t.tm_mon + 1,
           t.tm_year + 1900);

  drawClockBackground();
  drawDecorativeArc(240, 200, 180, 3, CLK_ACCENT);
  drawDecorativeArc(240, 200, 190, 2, CLK_GRAY_DIM);

  const int scale = 14;
  const int cw = BASE_CHAR_W * scale;
  const int ch = BASE_CHAR_H * scale;
  const int centerX = 240;
  const int hoursY = 70;
  const int hoursW = 2 * cw;

  gfx->setTextSize(scale);
  gfx->setTextColor(CLK_WHITE, CLK_BG_DARK);
  gfx->setCursor(centerX - hoursW / 2, hoursY);
  gfx->print(bufH);

  const int sepY = hoursY + ch + 8;
  gfx->fillRect(centerX - 100, sepY, 200, 4, CLK_ACCENT);
  gfx->fillCircle(centerX - 110, sepY + 2, 6, CLK_ACCENT);
  gfx->fillCircle(centerX + 110, sepY + 2, 6, CLK_ACCENT);

  const int minsY = sepY + 16;
  gfx->setTextSize(scale);
  gfx->setTextColor(CLK_GRAY, CLK_BG_DARK);
  gfx->setCursor(centerX - hoursW / 2, minsY);
  gfx->print(bufM);

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

  gfx->fillRoundRect(badgeX, badgeY, badgeW, badgeH, badgeH / 2, CLK_RING_BG);
  gfx->drawRoundRect(badgeX, badgeY, badgeW, badgeH, badgeH / 2, CLK_ACCENT);
  gfx->setTextSize(wdScale);
  gfx->setTextColor(CLK_WHITE, CLK_RING_BG);
  gfx->setCursor(badgeX + badgePadX, badgeY + badgePadY);
  gfx->print(wd);

  const int dateY = 420;
  const int dateScale = 3;
  const int dateW = strlen(bufD) * BASE_CHAR_W * dateScale;
  gfx->setTextSize(dateScale);
  gfx->setTextColor(CLK_GRAY_DIM, CLK_BG_DARK);
  gfx->setCursor(centerX - dateW / 2, dateY);
  gfx->print(bufD);

  clock_prev_hour = t.tm_hour;
  clock_prev_min = t.tm_min;
  clock_prev_mday = t.tm_mday;
  clock_prev_mon = t.tm_mon;
  clock_prev_year = t.tm_year;
  gfx->setTextSize(TEXT_SCALE);
}

inline void tickClock() {
  time_t now;
  struct tm t;
  time(&now);
  localtime_r(&now, &t);

  if (clock_prev_hour < 0 || clock_prev_min < 0) {
    pageClock();
    return;
  }

  if (t.tm_mday != clock_prev_mday || t.tm_mon != clock_prev_mon ||
      t.tm_year != clock_prev_year) {
    pageClock();
    return;
  }

  if (t.tm_hour != clock_prev_hour) {
    drawClockHourDigits((uint8_t)t.tm_hour);
    clock_prev_hour = t.tm_hour;
  }

  if (t.tm_min != clock_prev_min) {
    drawClockMinuteDigits((uint8_t)t.tm_min);
    clock_prev_min = t.tm_min;
  }

  gfx->setTextSize(TEXT_SCALE);
}
