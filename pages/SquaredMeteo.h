#pragma once

#include "../handlers/globals.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <HTTPClient.h>

extern Arduino_RGB_Display *gfx;

extern const uint16_t COL_BG;
extern const uint16_t COL_ACCENT1;
extern const uint16_t COL_ACCENT2;
extern const int PAGE_X;
extern const int PAGE_Y;

extern void drawHeader(const String &title);
extern void drawBoldMain(int16_t x, int16_t y, const String &raw,
                         uint8_t scale);
extern void drawHLine(int y);
extern bool httpGET(const String &url, String &body, uint32_t timeoutMs);
extern String sanitizeText(const String &in);

extern String g_city;
extern String g_lang;
extern String g_lat;
extern String g_lon;
extern bool geocodeIfNeeded();

extern void drawRLE(int x, int y, int w, int h, const RLERun *data,
                    size_t runs);

#include "../images/nuvole.h"
#include "../images/pioggia.h"
#include "../images/sole.h"

static float w_now_tempC = NAN;
static String w_now_desc;
static String w_desc[3];

static bool fetchLatLon(float &lat, float &lon) {
  if (!geocodeIfNeeded()) return false;
  lat = g_lat.toFloat();
  lon = g_lon.toFloat();
  return !isnan(lat) && !isnan(lon);
}

static String mapWeatherCodeToDesc(int code, const String &lang) {
  String d = "Cloudy";

  if (code == 0) {
    d = "Clear sky";
  } else if (code == 1 || code == 2 || code == 3) {
    d = "Partly cloudy";
  } else if (code == 45 || code == 48) {
    d = "Fog";
  } else if ((code >= 51 && code <= 57) || (code >= 61 && code <= 67)) {
    d = "Rain";
  } else if ((code >= 71 && code <= 77)) {
    d = "Snow";
  } else if (code >= 80 && code <= 82) {
    d = "Rain shower";
  } else if (code >= 85 && code <= 86) {
    d = "Snow shower";
  } else if (code >= 95 && code <= 99) {
    d = "Thunderstorm";
  }

  if (lang == "it") {
    if (d == "Clear sky")
      d = "Sereno";
    else if (d == "Partly cloudy")
      d = "Nuvoloso";
    else if (d == "Cloudy")
      d = "Coperto";
    else if (d == "Fog")
      d = "Nebbia";
    else if (d == "Rain")
      d = "Pioggia";
    else if (d == "Snow")
      d = "Neve";
    else if (d == "Rain shower")
      d = "Rovesci";
    else if (d == "Snow shower")
      d = "Nevischio";
    else if (d == "Thunderstorm")
      d = "Temporali";
  }

  return d;
}

static bool extractDailyWeathercode(const String &body, int index,
                                    int &codeOut) {
  int dailyPos = body.indexOf("\"daily\"");
  if (dailyPos < 0)
    return false;

  int wcPos = body.indexOf("\"weathercode\"", dailyPos);
  if (wcPos < 0)
    return false;

  int bracket = body.indexOf('[', wcPos);
  if (bracket < 0)
    return false;

  int len = body.length();
  int p = bracket + 1;
  int currentIndex = 0;

  while (p < len) {

    while (p < len && (body[p] == ' ' || body[p] == ',' || body[p] == '\n' ||
                       body[p] == '\r' || body[p] == '\t'))
      p++;

    if (p >= len || body[p] == ']')
      break;

    int start = p;
    while (p < len && (body[p] == '-' || body[p] == '+' || isdigit(body[p])))
      p++;

    if (currentIndex == index) {
      codeOut = body.substring(start, p).toInt();
      return true;
    }

    currentIndex++;
  }

  return false;
}

bool fetchWeather() {

  w_now_tempC = NAN;
  w_now_desc = "";
  for (int i = 0; i < 3; i++) {
    w_desc[i] = "";
  }

  float lat = NAN, lon = NAN;
  if (!fetchLatLon(lat, lon))
    return false;

  String url = "https://api.open-meteo.com/v1/forecast"
               "?latitude=" +
               String(lat, 6) + "&longitude=" + String(lon, 6) +
               "&current_weather=true"
               "&daily=weathercode"
               "&timezone=auto";

  String body;
  if (!httpGET(url, body, 10000))
    return false;

  bool ok = false;

  int cwPos = body.indexOf("\"current_weather\"");
  if (cwPos >= 0) {

    float t;
    if (extractJsonNumber(body, "\"temperature\"", t, cwPos)) {
      w_now_tempC = t;
      ok = true;
    }

    float codef;
    if (extractJsonNumber(body, "\"weathercode\"", codef, cwPos)) {
      int code = (int)codef;
      String d = mapWeatherCodeToDesc(code, g_lang);
      w_now_desc = sanitizeText(d);
      if (w_now_desc.length())
        ok = true;
    }
  }

  for (int i = 0; i < 3; i++) {
    int code;
    if (!extractDailyWeathercode(body, i, code))
      break;

    String d = mapWeatherCodeToDesc(code, g_lang);
    w_desc[i] = sanitizeText(d);
    if (w_desc[i].length())
      ok = true;
  }

  return ok;
}

static inline int pickWeatherIcon(const String &d) {
  String s = d;
  s.toLowerCase();
  if (s.indexOf("clear") >= 0 || s.indexOf("seren") >= 0)
    return 0;
  if (s.indexOf("rain") >= 0 || s.indexOf("piogg") >= 0)
    return 2;
  return 1;
}

#define N_DUST 80

struct Particle {
  int16_t x, y;
  int16_t ox, oy;
  int8_t vx, vy;
  uint8_t layer;
};

static Particle dust[N_DUST];
static bool dustInit = false;
static uint32_t dustLast = 0;

constexpr uint16_t dustCol[3] = {0x7BEF, 0xAD75, 0xFFFF};

static inline bool isUI(int x, int y) {
  if (y < PAGE_Y + 58)
    return true;

  const int SEP0 = PAGE_Y + 60;
  const int SEP1 = PAGE_Y + 131;
  const int SEP2 = PAGE_Y + 200;

  if (y == SEP0 || y == SEP1 || y == SEP2) {
    return true;
  }

  if (x < PAGE_X + 350 && y >= PAGE_Y + 58 && y < 360) {
    return true;
  }

  if (x < 220 && y > 340) {
    return true;
  }

  int PAD = 24;
  int ix = 480 - NUVOLE_WIDTH - PAD;
  int iy = 480 - NUVOLE_HEIGHT - PAD;
  if (x >= ix - 10 && x < ix + NUVOLE_WIDTH + 10 && y >= iy - 10 &&
      y < iy + NUVOLE_HEIGHT + 10) {
    return true;
  }

  return false;
}

static inline void initDust(int i) {
  Particle &p = dust[i];

  int attempts = 0;
  do {
    p.x = random(0, 480);
    p.y = random(0, 480);
    attempts++;
  } while (isUI(p.x, p.y) && attempts < 16);

  if (isUI(p.x, p.y)) {
    p.x = 240;
    p.y = 240;
  }

  p.ox = p.x;
  p.oy = p.y;

  if (i < 20)
    p.layer = 0;
  else if (i < 45)
    p.layer = 1;
  else
    p.layer = 2;

  int base = (p.layer == 0 ? 1 : (p.layer == 1 ? 2 : 3));

  do {
    p.vx = random(-base, base + 1);
    p.vy = random(-base, base + 1);
  } while (p.vx == 0 && p.vy == 0);
}

void pageWeatherParticlesTick() {
  uint32_t now = millis();
  if (now - dustLast < 40)
    return;
  dustLast = now;

  if (!dustInit) {
    dustInit = true;
    for (int i = 0; i < N_DUST; i++) {
      initDust(i);
    }
  }

  const int UI_BARRIER_Y[3] = {PAGE_Y + 60, PAGE_Y + 131, PAGE_Y + 200};

  for (int i = 0; i < N_DUST; i++) {

    Particle &p = dust[i];

    if (!isUI(p.ox, p.oy)) {
      gfx->drawPixel(p.ox, p.oy, COL_BG);
    }

    int16_t nx = p.x + p.vx;
    int16_t ny = p.y + p.vy;

    if (nx < 0 || nx >= 480) {
      p.vx = -p.vx;
      nx = p.x + p.vx;
    }
    if (ny < 0 || ny >= 480) {
      p.vy = -p.vy;
      ny = p.y + p.vy;
    }

    for (int b = 0; b < 3; b++) {
      int by = UI_BARRIER_Y[b];

      if (p.y < by && ny >= by) {
        p.vy = -p.vy;
        ny = p.y + p.vy;
      }

      if (p.y > by && ny <= by) {
        p.vy = -p.vy;
        ny = p.y + p.vy;
      }
    }

    if (isUI(nx, ny)) {
      initDust(i);
      continue;
    }

    gfx->drawPixel(nx, ny, dustCol[p.layer]);

    p.ox = p.x = nx;
    p.oy = p.y = ny;
  }
}

void pageWeather() {
  pageWeatherParticlesTick();

  drawHeader(g_lang == "it" ? "Meteo per " + sanitizeText(g_city)
                            : "Weather in " + sanitizeText(g_city));

  int y = PAGE_Y;

  String line;
  if (!isnan(w_now_tempC) && w_now_desc.length()) {
    line = String((int)round(w_now_tempC)) + "c  " + w_now_desc;
  } else if (w_now_desc.length()) {
    line = w_now_desc;
  } else {
    line = (g_lang == "it" ? "Sto aggiornando..." : "Updating...");
  }

  drawBoldMain(PAGE_X, y + 20, line, 2);
  y += 60;
  drawHLine(y);
  y += 16;

  static const char *const it_days[3] = {"oggi", "domani", "fra 2 giorni"};
  static const char *const en_days[3] = {"today", "tomorrow", "in 2 days"};

  for (int i = 0; i < 3; i++) {
    gfx->setCursor(PAGE_X, y);
    gfx->setTextColor(COL_ACCENT1, COL_BG);
    gfx->print(g_lang == "it" ? it_days[i] : en_days[i]);

    gfx->setCursor(PAGE_X, y + 20);
    gfx->setTextColor(COL_ACCENT2, COL_BG);
    gfx->print(w_desc[i]);

    y += (i < 2 ? 69 : 55);
    if (i < 2) {
      drawHLine(y - 14);
    }
  }

  int PAD = 24;
  int ix = 480 - NUVOLE_WIDTH - PAD;
  int iy = 480 - NUVOLE_HEIGHT - PAD;

  switch (pickWeatherIcon(w_now_desc)) {
  case 0:
    drawRLE(ix, iy, SOLE_WIDTH, SOLE_HEIGHT, sole,
            sizeof(sole) / sizeof(RLERun));
    break;

  case 1:
    drawRLE(ix, iy, NUVOLE_WIDTH, NUVOLE_HEIGHT, nuvole,
            sizeof(nuvole) / sizeof(RLERun));
    break;

  default:
    drawRLE(ix, iy, PIOGGIA_WIDTH, PIOGGIA_HEIGHT, pioggia,
            sizeof(pioggia) / sizeof(RLERun));
    break;
  }

  if (!isnan(w_now_tempC)) {
    gfx->setTextColor(COL_ACCENT1, COL_BG);
    gfx->setTextSize(9);
    gfx->setCursor(PAD, 480 - 115);
    gfx->print((int)round(w_now_tempC));
    gfx->print("c");
    gfx->setTextSize(2);
  }
}
