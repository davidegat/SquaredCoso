#pragma once

#include "../handlers/displayhelpers.h"
#include "../handlers/globals.h"
#include "../handlers/jsonhelpers.h"
#include <Arduino.h>

extern Arduino_RGB_Display *gfx;
extern const uint16_t COL_BG, COL_HEADER, COL_TEXT;
extern const int PAGE_X, PAGE_Y, BASE_CHAR_W, BASE_CHAR_H, TEXT_SCALE, CHAR_H;
extern void drawHeader(const String &title);
extern void drawBoldMain(int16_t x, int16_t y, const String &, uint8_t scale);
extern void drawHLine(int y);
extern String sanitizeText(const String &);
extern int indexOfCI(const String &, const String &, int from);
extern bool httpGET(const String &url, String &out, uint32_t timeout);
extern bool geocodeIfNeeded();
extern String g_city, g_lang;
extern uint16_t g_air_bg;

static const uint16_t AIR_BG[] PROGMEM = {
    0x1B70,
    0x3C90,
    0x548C,
    0x53EC,
    0x528C,
    0x328C
};

static const uint16_t AIR_VAL_GOOD = 0x07E0;
static const uint16_t AIR_VAL_FAIR = 0xFFE0;
static const uint16_t AIR_VAL_POOR = 0xF800;

static const float THRESH_PM25[] PROGMEM = {5, 15, 50, 90, 140};
static const float THRESH_PM10[] PROGMEM = {15, 45, 120, 195, 270};
static const float THRESH_O3[] PROGMEM = {60, 100, 120, 160, 180};
static const float THRESH_NO2[] PROGMEM = {10, 25, 60, 100, 150};

static float aq_val[4];

#define AQ_PM25 0
#define AQ_PM10 1
#define AQ_O3 2
#define AQ_NO2 3

static const char PROGMEM cat_it_0[] = "Aria buona";
static const char PROGMEM cat_it_1[] = "Aria discreta";
static const char PROGMEM cat_it_2[] = "Aria moderata";
static const char PROGMEM cat_it_3[] = "Aria scadente";
static const char PROGMEM cat_it_4[] = "Aria pessima";
static const char PROGMEM cat_it_5[] = "Aria estrema";

static const char PROGMEM cat_en_0[] = "Good air";
static const char PROGMEM cat_en_1[] = "Fair air";
static const char PROGMEM cat_en_2[] = "Moderate air";
static const char PROGMEM cat_en_3[] = "Poor air";
static const char PROGMEM cat_en_4[] = "Very poor air";
static const char PROGMEM cat_en_5[] = "Extremely poor";

static const char *const cat_it[] PROGMEM = {cat_it_0, cat_it_1, cat_it_2,
                                             cat_it_3, cat_it_4, cat_it_5};
static const char *const cat_en[] PROGMEM = {cat_en_0, cat_en_1, cat_en_2,
                                             cat_en_3, cat_en_4, cat_en_5};

static const char PROGMEM tip_it_0[] = "tutto ok!";
static const char PROGMEM tip_it_1[] = "nessuna precauzione.";
static const char PROGMEM tip_it_2[] = "sensibili: limitare sforzo.";
static const char PROGMEM tip_it_3[] = "evitare attivita fuori!";
static const char PROGMEM tip_it_4[] = "rimanere al chiuso!";
static const char PROGMEM tip_it_5[] = "evitare esposizione esterna!";

static const char PROGMEM tip_en_0[] = "all good!";
static const char PROGMEM tip_en_1[] = "no precautions needed.";
static const char PROGMEM tip_en_2[] = "sensitive: reduce effort.";
static const char PROGMEM tip_en_3[] = "avoid long outdoor activity!";
static const char PROGMEM tip_en_4[] = "stay indoors!";
static const char PROGMEM tip_en_5[] = "avoid outdoor exposure!";

static const char *const tip_it[] PROGMEM = {tip_it_0, tip_it_1, tip_it_2,
                                             tip_it_3, tip_it_4, tip_it_5};
static const char *const tip_en[] PROGMEM = {tip_en_0, tip_en_1, tip_en_2,
                                             tip_en_3, tip_en_4, tip_en_5};

static inline int8_t catFrom(float v, const float *thresh) {
  if (isnan(v))
    return -1;
  if (v <= pgm_read_float(&thresh[0]))
    return 0;
  if (v <= pgm_read_float(&thresh[1]))
    return 1;
  if (v <= pgm_read_float(&thresh[2]))
    return 2;
  if (v <= pgm_read_float(&thresh[3]))
    return 3;
  if (v <= pgm_read_float(&thresh[4]))
    return 4;
  return 5;
}

static int8_t worstCategory() {
  int8_t worst = -1;
  worst = max(worst, catFrom(aq_val[AQ_PM25], THRESH_PM25));
  worst = max(worst, catFrom(aq_val[AQ_PM10], THRESH_PM10));
  worst = max(worst, catFrom(aq_val[AQ_O3], THRESH_O3));
  worst = max(worst, catFrom(aq_val[AQ_NO2], THRESH_NO2));
  return worst;
}

static bool fetchAir() {
  if (!geocodeIfNeeded()) return false;

  String url = F("https://air-quality-api.open-meteo.com/v1/air-quality?latitude=");
  url += g_lat;
  url += F("&longitude=");
  url += g_lon;
  url += F("&current=pm2_5,pm10,ozone,nitrogen_dioxide&timezone=auto");

  String body;
  if (!httpGET(url, body, 10000)) return false;

  String cur;
  if (!extractObjectBlock(body, "current", cur)) return false;

  float v;
  aq_val[AQ_PM25] = jsonNum(cur, "pm2_5", v) ? v : NAN;
  aq_val[AQ_PM10] = jsonNum(cur, "pm10", v) ? v : NAN;
  aq_val[AQ_O3]   = jsonNum(cur, "ozone", v) ? v : NAN;
  aq_val[AQ_NO2]  = jsonNum(cur, "nitrogen_dioxide", v) ? v : NAN;

  return !(isnan(aq_val[AQ_PM25]) && isnan(aq_val[AQ_PM10]) &&
           isnan(aq_val[AQ_O3]) && isnan(aq_val[AQ_NO2]));
}

#define N_LEAVES 100
#define SIN_LUT_SIZE 64

static const int8_t SIN_LUT[SIN_LUT_SIZE] PROGMEM = {
    0,    12,   25,   37,   49,   60,   71,   81,   90,   98,   105,
    111,  116,  120,  123,  125,  127,  125,  123,  120,  116,  111,
    105,  98,   90,   81,   71,   60,   49,   37,   25,   12,   0,
    -12,  -25,  -37,  -49,  -60,  -71,  -81,  -90,  -98,  -105, -111,
    -116, -120, -123, -125, -127, -125, -123, -120, -116, -111, -105,
    -98,  -90,  -81,  -71,  -60,  -49,  -37,  -25,  -12};

// Leaf X uses 4-bit fixed point for smooth motion without float-heavy updates.
struct Leaf {
  int16_t x;
  int16_t baseY;
  int16_t oldX, oldY;
  uint8_t phase;
  uint8_t vx;
  uint8_t amp;
  uint8_t size;
  uint16_t color;
};

static Leaf leaves[N_LEAVES];
static bool leavesInit = false;
static uint32_t leavesLastMs = 0;

static const uint16_t LEAF_COLORS[] PROGMEM = {0xFD80, 0xA440};

static void initLeafFX(uint8_t i, bool spreadOnScreen = false) {
  Leaf &L = leaves[i];

  const int16_t startX = spreadOnScreen ? random(-20, 480) : random(-40, -10);
  L.x = startX << 4;
  L.baseY = random(310, 470);
  L.vx = 16 + random(0, 32);
  L.amp = 6 + random(0, 10);
  L.phase = random(0, SIN_LUT_SIZE);
  L.size = random(0, 3);
  L.color = pgm_read_word(&LEAF_COLORS[random(0, 2)]);
  L.oldX = -100;
  L.oldY = -100;
}

static inline void drawLeafShape(int16_t x, int16_t y, uint8_t s, uint16_t c) {
  gfx->drawPixel(x, y, c);
  if (s > 0) {
    gfx->drawPixel(x - 1, y, c);
    gfx->drawPixel(x + 1, y, c);
  }
  if (s > 1) {
    gfx->drawPixel(x, y - 1, c);
    gfx->drawPixel(x, y + 1, c);
    gfx->drawPixel(x - 1, y - 1, c);
    gfx->drawPixel(x + 1, y - 1, c);
    gfx->drawPixel(x - 1, y + 1, c);
    gfx->drawPixel(x + 1, y + 1, c);
  }
}

static void tickLeaves(uint16_t bg) {
  uint32_t now = millis();
  if (now - leavesLastMs < 33U)
    return;
  leavesLastMs = now;

  if (!leavesInit) {
    leavesInit = true;
    for (uint8_t i = 0; i < N_LEAVES; i++)
      initLeafFX(i, true);
  }

  gfx->startWrite();

  for (uint8_t i = 0; i < N_LEAVES; i++) {
    Leaf &L = leaves[i];

    if (L.oldY >= 300) {
      drawLeafShape(L.oldX, L.oldY, L.size, bg);
    }

    L.x += L.vx;
    L.phase = (L.phase + 1) & (SIN_LUT_SIZE - 1);

    int8_t sinVal = pgm_read_byte(&SIN_LUT[L.phase]);
    int16_t iy = L.baseY + ((sinVal * L.amp) >> 7);
    int16_t ix = L.x >> 4;

    if (ix > 500) {
      initLeafFX(i);
      continue;
    }

    if (iy >= 300 && iy < 480 && ix >= -2 && ix < 482) {
      drawLeafShape(ix, iy, L.size, L.color);
    }

    L.oldX = ix;
    L.oldY = iy;
  }

  gfx->endWrite();
}

static void airRowPrint(uint8_t paramIdx, const char *label, int16_t &y,
                        uint16_t bg) {
  const uint8_t SZ = 3;
  float value = aq_val[paramIdx];

  const float *thresh;
  switch (paramIdx) {
  case AQ_PM25:
    thresh = THRESH_PM25;
    break;
  case AQ_PM10:
    thresh = THRESH_PM10;
    break;
  case AQ_O3:
    thresh = THRESH_O3;
    break;
  default:
    thresh = THRESH_NO2;
    break;
  }

  int8_t cat = catFrom(value, thresh);

  uint16_t valColor = COL_TEXT;
  if (cat == 0 || cat == 1)
    valColor = AIR_VAL_GOOD;
  else if (cat == 2)
    valColor = AIR_VAL_FAIR;
  else if (cat >= 3)
    valColor = AIR_VAL_POOR;

  gfx->setTextSize(SZ);
  gfx->setTextColor(COL_TEXT, bg);
  gfx->setCursor(PAGE_X, y);
  gfx->print(label);

  char buf[24];
  if (isnan(value)) {
    strcpy(buf, "-- ");
  } else {
    dtostrf(value, 0, (paramIdx == AQ_PM25) ? 1 : 0, buf);
    strcat(buf, " ");
  }

  strcat(buf, "ug/m3");

  int16_t textW = strlen(buf) * BASE_CHAR_W * SZ;
  int16_t xr = 480 - PAGE_X - textW;
  if (xr < PAGE_X + 120)
    xr = PAGE_X + 120;

  gfx->setCursor(xr, y);
  gfx->setTextColor(valColor, bg);
  gfx->print(buf);

  y += BASE_CHAR_H * SZ + 10;
}

static void pageAir() {
  const bool isIt = (g_lang[0] == 'i');

  leavesLastMs = millis() - 34U;

  if (isIt) {
    String h = F("Aria a ");
    h += sanitizeText(g_city);
    drawHeader(h);
  } else {
    String h = F("Air quality - ");
    h += sanitizeText(g_city);
    drawHeader(h);
  }

  int8_t cat = worstCategory();

  uint16_t bg = COL_BG;
  if (cat >= 0 && cat <= 5) {
    bg = pgm_read_word(&AIR_BG[cat]);
  }
  g_air_bg = bg;

  gfx->fillRect(0, PAGE_Y, 480, 480 - PAGE_Y, bg);

  int16_t y = PAGE_Y + 14;

  airRowPrint(AQ_PM25, "PM2.5:", y, bg);
  airRowPrint(AQ_PM10, "PM10 :", y, bg);
  airRowPrint(AQ_O3, isIt ? "OZONO:" : "OZONE:", y, bg);
  airRowPrint(AQ_NO2, "NO2  :", y, bg);

  drawHLine(y);
  y += 14;

  gfx->setTextSize(TEXT_SCALE + 1);
  gfx->setTextColor(COL_TEXT, bg);

  char catBuf[24], tipBuf[40];

  if (cat < 0) {
    strcpy_P(catBuf,
             isIt ? PSTR("Dati non disponibili") : PSTR("Data unavailable"));
    tipBuf[0] = '\0';
  } else {
    strcpy_P(catBuf,
             (const char *)pgm_read_ptr(isIt ? &cat_it[cat] : &cat_en[cat]));
    strcpy_P(tipBuf,
             (const char *)pgm_read_ptr(isIt ? &tip_it[cat] : &tip_en[cat]));
  }

  const int16_t charW = BASE_CHAR_W * (TEXT_SCALE + 1);

  y += 12;

  int16_t catLen = strlen(catBuf);
  int16_t catW = catLen * charW;
  int16_t catX = (480 - catW) >> 1;
  if (catX < PAGE_X)
    catX = PAGE_X;

  gfx->setCursor(catX, y);
  gfx->print(catBuf);
  y += BASE_CHAR_H * (TEXT_SCALE + 1) + 6;

  if (tipBuf[0] != '\0') {
    int16_t tipLen = strlen(tipBuf);
    int16_t tipW = tipLen * charW;
    int16_t tipX = (480 - tipW) >> 1;
    if (tipX < PAGE_X)
      tipX = PAGE_X;

    gfx->setCursor(tipX, y);
    gfx->print(tipBuf);
  }

  gfx->setTextSize(TEXT_SCALE);

  tickLeaves(bg);
}
