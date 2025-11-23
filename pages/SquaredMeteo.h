#pragma once
/*
   SquaredCoso – Pagina METEO con particelle full-screen e icone RLE
   - Fetch da wttr.in (JSON j1), bilingue via g_lang ("it"/"en")
   - Testi previsioni e condizioni: direttamente dall'API, poi sanitizeText()
   - Particelle che rimbalzano su bordi e UI, senza sporcare overlay
   - Icona meteo RLE (sole/nuvole/pioggia) in basso a destra
*/

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "../handlers/globals.h"

extern bool jsonFindStringKV(const String& body, const String& key, int from, String& outVal);

extern Arduino_RGB_Display* gfx;

extern const uint16_t COL_BG;
extern const uint16_t COL_ACCENT1;
extern const uint16_t COL_ACCENT2;
extern const int PAGE_X;
extern const int PAGE_Y;

extern void   drawHeader(const String& title);
extern void   drawBoldMain(int16_t x, int16_t y, const String& raw, uint8_t scale);
extern void   drawHLine(int y);
extern bool   httpGET(const String& url, String& body, uint32_t timeoutMs);
extern String sanitizeText(const String& in);
extern int    indexOfCI(const String& src, const String& key, int from);

extern String g_city;
extern String g_lang;

extern void drawRLE(int x, int y, int w, int h, const RLERun *data, size_t runs);

#include "../images/sole.h"
#include "../images/nuvole.h"
#include "../images/pioggia.h"

// stato meteo corrente + descrizioni previste (testo già bilingue da API)
static float  w_now_tempC = NAN;
static String w_now_desc;
static String w_desc[3];

// ---------------------------------------------------------------------------
// Fetch meteo da wttr.in (j1) usando g_city e g_lang
// ---------------------------------------------------------------------------
bool fetchWeather() {
  String url = "https://wttr.in/" + g_city + "?format=j1&lang=" + g_lang;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  https.begin(client, url);
  https.addHeader("User-Agent", "curl");
  https.addHeader("Accept", "application/json");

  if (https.GET() != 200) {
    https.end();
    return false;
  }

  String body = https.getString();
  https.end();

  int cc = indexOfCI(body, "\"current_condition\"", 0);

  if (cc >= 0) {
    String t;

    if (jsonFindStringKV(body, "temp_C", cc, t))
      w_now_tempC = t.toFloat();

    if (jsonFindStringKV(
          body,
          "value",
          indexOfCI(body, "\"weatherDesc\"", cc),
          w_now_desc))
    {
      w_now_desc = sanitizeText(w_now_desc);
    }
  }

  for (int i = 0; i < 3; i++) {
    int pos = indexOfCI(body, "\"weatherDesc\"", cc + i * 60);
    String v;
    if (pos >= 0 && jsonFindStringKV(body, "value", pos, v)) {
      w_desc[i] = sanitizeText(v);
    }
  }

  return true;
}

// ---------------------------------------------------------------------------
// Scelta icona in base alla descrizione (supporta IT/EN)
// ---------------------------------------------------------------------------
static inline int pickWeatherIcon(const String& d) {
  String s = d;
  s.toLowerCase();
  if (s.indexOf("sun")   >= 0 || s.indexOf("sole")  >= 0) return 0;
  if (s.indexOf("rain")  >= 0 || s.indexOf("piogg") >= 0) return 2;
  return 1;
}

// ---------------------------------------------------------------------------
// Particelle di polvere che rimbalzano su UI, bordi e linee orizzontali
// ---------------------------------------------------------------------------
#define N_DUST 80

struct Particle {
  int16_t x, y;
  int16_t ox, oy;
  int8_t  vx, vy;
  uint8_t layer;
};

static Particle  dust[N_DUST];
static bool      dustInit = false;
static uint32_t  dustLast = 0;

constexpr uint16_t dustCol[3] = {
  0x7BEF,
  0xAD75,
  0xFFFF
};

// zone protette dalla UI: header, testo, linee, temp, icona
static inline bool isUI(int x, int y) {
  if (y < PAGE_Y + 58) return true;

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
  if (x >= ix - 10 && x < ix + NUVOLE_WIDTH + 10 &&
      y >= iy - 10 && y < iy + NUVOLE_HEIGHT + 10)
  {
    return true;
  }

  return false;
}

// posizionamento iniziale particella in zona non-UI con velocità compatibile
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

  if (i < 20)      p.layer = 0;
  else if (i < 45) p.layer = 1;
  else             p.layer = 2;

  int base = (p.layer == 0 ? 1 : (p.layer == 1 ? 2 : 3));

  do {
    p.vx = random(-base, base + 1);
    p.vy = random(-base, base + 1);
  } while (p.vx == 0 && p.vy == 0);
}

// tick animazione particelle, con rimbalzo su bordi e linee UI
void pageWeatherParticlesTick() {
  uint32_t now = millis();
  if (now - dustLast < 40) return;
  dustLast = now;

  if (!dustInit) {
    dustInit = true;
    for (int i = 0; i < N_DUST; i++) {
      initDust(i);
    }
  }

  const int UI_BARRIER_Y[3] = {
    PAGE_Y + 60,
    PAGE_Y + 131,
    PAGE_Y + 200
  };

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

// ---------------------------------------------------------------------------
// Rendering pagina METEO: particelle + testi + icona + temp grande
// ---------------------------------------------------------------------------
void pageWeather() {
  pageWeatherParticlesTick();

  drawHeader(
    g_lang == "it"
      ? "Meteo per " + sanitizeText(g_city)
      : "Weather in " + sanitizeText(g_city));

  int y = PAGE_Y;

  String line =
    (!isnan(w_now_tempC) && w_now_desc.length())
      ? String((int)round(w_now_tempC)) + "c  " + w_now_desc
      : (g_lang == "it" ? "Sto aggiornando..." : "Updating...");

  drawBoldMain(PAGE_X, y + 20, line, 2);
  y += 60;
  drawHLine(y);
  y += 16;

  static const char* const it_days[3] = { "oggi", "domani", "fra 2 giorni" };
  static const char* const en_days[3] = { "today", "tomorrow", "in 2 days" };

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
      drawRLE(ix, iy, SOLE_WIDTH, SOLE_HEIGHT,
              sole, sizeof(sole) / sizeof(RLERun));
      break;

    case 1:
      drawRLE(ix, iy, NUVOLE_WIDTH, NUVOLE_HEIGHT,
              nuvole, sizeof(nuvole) / sizeof(RLERun));
      break;

    default:
      drawRLE(ix, iy, PIOGGIA_WIDTH, PIOGGIA_HEIGHT,
              pioggia, sizeof(pioggia) / sizeof(RLERun));
      break;
  }

  if (!isnan(w_now_tempC)) {
    gfx->setTextColor(COL_ACCENT1, COL_BG);
    gfx->setTextSize(9);
    gfx->setCursor(PAD, 480 - 90);
    gfx->print((int)round(w_now_tempC));
    gfx->print("c");
    gfx->setTextSize(2);
  }
}

