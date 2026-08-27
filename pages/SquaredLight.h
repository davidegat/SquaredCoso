#pragma once

#include "../handlers/globals.h"
#include "../handlers/jsonhelpers.h"
#include <Arduino.h>
#include <math.h>
#include <time.h>

static inline int64_t sunDaysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int)doe - 719468;
}

static time_t timegm_esp32(struct tm *tm) {
  return (time_t)(sunDaysFromCivil(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday) * 86400LL +
                  tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec);
}

#include "../images/luna.h"

static const CosinoRLE LUNA_ART = {luna, sizeof(luna) / sizeof(RLERun)};

extern Arduino_RGB_Display *gfx;

extern const uint16_t COL_BG;
extern const uint16_t COL_TEXT;
extern const uint16_t COL_ACCENT1;
extern const uint16_t COL_ACCENT2;

extern const int PAGE_X;
extern const int PAGE_Y;
extern const int BASE_CHAR_W;
extern const int BASE_CHAR_H;
extern const int TEXT_SCALE;

extern String g_city;
extern String g_lat;
extern String g_lon;
extern String g_lang;

extern void drawHeader(const String &title);
extern void drawBoldMain(int16_t x, int16_t y, const String &raw,
                         uint8_t scale);
extern bool httpGET(const String &url, String &body, uint32_t timeoutMs);

static char sun_rise[6];
static char sun_set[6];
static char sun_noon[6];
static char sun_cb[6];
static char sun_ce[6];
static char sun_len[16];
static char sun_uvi[8];

static float g_moon_illum01 = -1.0f;
static bool g_moon_waxing = true;
static float g_moon_phase01 = -1.0f;

static uint8_t g_moon_phase_idx = 0;

static uint16_t g_moonTex[LUNA_WIDTH * LUNA_HEIGHT];
static bool g_moonTexReady = false;

static uint16_t g_shadowTex[LUNA_WIDTH * LUNA_HEIGHT];
static bool g_shadowReady = false;
static int g_shadowPhaseBucket = -1;

static inline bool isoToEpoch(const String &s, time_t &out) {
  if (s.length() < 19) return false;
  int y = s.substring(0, 4).toInt();
  int mo = s.substring(5, 7).toInt();
  int d = s.substring(8, 10).toInt();
  int h = s.substring(11, 13).toInt();
  int mi = s.substring(14, 16).toInt();
  int se = s.substring(17, 19).toInt();
  out = (time_t)(sunDaysFromCivil(y, mo, d) * 86400LL + h * 3600 + mi * 60 + se);
  return true;
}

static inline void isoToHM(const String &iso, char out[6]) {
  time_t tt;
  if (!isoToEpoch(iso, tt)) { strcpy(out, "--:--"); return; }
  struct tm local{};
  localtime_r(&tt, &local);
  snprintf(out, 6, "%02d:%02d", local.tm_hour, local.tm_min);
}

static void ensureMoonTextureDecoded() {
  if (g_moonTexReady)
    return;

  int32_t total = LUNA_WIDTH * LUNA_HEIGHT;
  int32_t idx = 0;

  for (size_t i = 0; i < LUNA_ART.runs && idx < total; i++) {
    uint16_t col = LUNA_ART.data[i].color;
    uint16_t cnt = LUNA_ART.data[i].count;
    for (uint16_t k = 0; k < cnt && idx < total; k++)
      g_moonTex[idx++] = col;
  }
  g_moonTexReady = true;
}

static void buildShadowTexture(int r) {
  if (g_shadowReady && g_shadowPhaseBucket == g_moon_phase_idx)
    return;

  g_shadowPhaseBucket = g_moon_phase_idx;
  ensureMoonTextureDecoded();

  float illum = g_moon_illum01;
  if (illum < 0)
    illum = 0.0f;
  if (illum > 1)
    illum = 1.0f;

  float terminator_k = cosf(M_PI * illum);

  if (!g_moon_waxing) {
    terminator_k = -terminator_k;
  }

  for (int y = -r; y <= r; y++) {
    int row = (y + r) * LUNA_WIDTH;

    int r2 = r * r;
    int y2 = y * y;
    if (y2 > r2)
      continue;

    float x_edge = sqrtf((float)(r2 - y2));
    int xlim = (int)x_edge;

    float x_terminator = terminator_k * x_edge;

    for (int x = -xlim; x <= xlim; x++) {
      int px = x + r;
      if (px < 0 || px >= LUNA_WIDTH)
        continue;

      uint16_t base = g_moonTex[row + px];

      bool lit;
      if (g_moon_waxing) {

        lit = ((float)x > x_terminator);
      } else {

        lit = ((float)x < x_terminator);
      }

      if (!lit) {

        uint8_t R = ((base >> 11) & 0x1F) >> 2;
        uint8_t G = ((base >> 5) & 0x3F) >> 2;
        uint8_t B = (base & 0x1F) >> 2;
        g_shadowTex[row + px] = (R << 11) | (G << 5) | B;
      } else {

        g_shadowTex[row + px] = base;
      }
    }
  }

  g_shadowReady = true;
}

struct MoonEvent {
  time_t ts;
  uint8_t phase4;
  bool valid;
};

static uint8_t uidToPhase4(const String &uid) {
  if (uid.indexOf("newmoon") >= 0)
    return 0;
  if (uid.indexOf("first-quarter") >= 0)
    return 1;
  if (uid.indexOf("fullmoon") >= 0)
    return 2;
  if (uid.indexOf("last-quarter") >= 0)
    return 3;
  return 255;
}

static time_t parseDate(const String &line) {
  int p = line.indexOf(':');
  if (p < 0)
    return 0;

  String d = line.substring(p + 1);
  String y = d.substring(0, 4);
  String m = d.substring(4, 6);
  String dd = d.substring(6, 8);

  struct tm tt {};
  tt.tm_year = y.toInt() - 1900;
  tt.tm_mon = m.toInt() - 1;
  tt.tm_mday = dd.toInt();
  tt.tm_hour = 12;
  tt.tm_min = 0;
  tt.tm_sec = 0;

  return timegm_esp32(&tt);
}

static void interpolatePhase(const MoonEvent &prev, const MoonEvent &next,
                             time_t nowUTC) {
  if (!prev.valid || !next.valid) {
    g_moon_illum01 = -1;
    g_moon_phase_idx = 0;
    g_moon_waxing = true;
    g_moon_phase01 = -1.0f;
    return;
  }

  double total = difftime(next.ts, prev.ts);
  if (total <= 0) {
    g_moon_illum01 = -1;
    g_moon_phase_idx = 0;
    g_moon_waxing = true;
    g_moon_phase01 = -1.0f;
    return;
  }

  double pos = difftime(nowUTC, prev.ts) / total;
  if (pos < 0)
    pos = 0;
  if (pos > 1)
    pos = 1;

  uint8_t p0 = prev.phase4 * 2;
  uint8_t p1 = next.phase4 * 2;
  if (p1 <= p0)
    p1 += 8;

  float pf = p0 + pos * (p1 - p0);
  g_moon_phase_idx = ((uint8_t)roundf(pf)) % 8;
  g_moon_phase01 = fmodf(pf * (1.0f / 8.0f), 1.0f);
  if (g_moon_phase01 < 0.0f)
    g_moon_phase01 += 1.0f;

  if (prev.phase4 == 0 && next.phase4 == 1) {

    g_moon_waxing = true;
    g_moon_illum01 = pos * 0.5f;
  } else if (prev.phase4 == 1 && next.phase4 == 2) {

    g_moon_waxing = true;
    g_moon_illum01 = 0.5f + (pos * 0.5f);
  } else if (prev.phase4 == 2 && next.phase4 == 3) {

    g_moon_waxing = false;
    g_moon_illum01 = 1.0f - (pos * 0.5f);
  } else if (prev.phase4 == 3 && next.phase4 == 0) {

    g_moon_waxing = false;
    g_moon_illum01 = 0.5f - (pos * 0.5f);
  } else if (prev.phase4 == 3 && next.phase4 == 1) {

    g_moon_waxing = (pos > 0.5f);
    if (pos < 0.5f) {

      g_moon_illum01 = 0.5f - (pos * 1.0f);
    } else {

      g_moon_illum01 = (pos - 0.5f) * 1.0f;
    }
  } else if (prev.phase4 == 1 && next.phase4 == 3) {

    g_moon_waxing = (pos < 0.5f);
    if (pos < 0.5f) {

      g_moon_illum01 = 0.5f + (pos * 1.0f);
    } else {

      g_moon_illum01 = 1.0f - ((pos - 0.5f) * 1.0f);
    }
  } else {

    float illumPrev = (prev.phase4 == 0 ? 0 : prev.phase4 == 2 ? 1 : 0.5);
    float illumNext = (next.phase4 == 0 ? 0 : next.phase4 == 2 ? 1 : 0.5);
    g_moon_illum01 = illumPrev + pos * (illumNext - illumPrev);
    g_moon_waxing = (illumNext > illumPrev);
  }
}

bool fetchSun() {

  strcpy(sun_rise, "--:--");
  strcpy(sun_set, "--:--");
  strcpy(sun_noon, "--:--");
  strcpy(sun_cb, "--:--");
  strcpy(sun_ce, "--:--");
  strcpy(sun_len, "--h --m");
  strcpy(sun_uvi, "--");

  g_moon_illum01 = -1;
  g_moon_phase_idx = 0;
  g_moon_waxing = true;
  g_moon_phase01 = -1.0f;

  float lat, lon;
  if (!fetchLatLon(lat, lon))
    return false;

  g_lat = String(lat, 6);
  g_lon = String(lon, 6);

  {
    String body;
    String url = "https://api.sunrise-sunset.org/json?lat=" + g_lat +
                 "&lng=" + g_lon + "&formatted=0";

    if (!httpGET(url, body, 10000))
      return false;

    String sr, ss, sn, cb, ce;
    if (!jsonKV(body, "sunrise", sr))
      return false;
    if (!jsonKV(body, "sunset", ss))
      return false;

    jsonKV(body, "solar_noon", sn);
    jsonKV(body, "civil_twilight_begin", cb);
    jsonKV(body, "civil_twilight_end", ce);

    isoToHM(sr, sun_rise);
    isoToHM(ss, sun_set);
    isoToHM(sn, sun_noon);
    isoToHM(cb, sun_cb);
    isoToHM(ce, sun_ce);

    time_t a, b;
    if (isoToEpoch(sr, a) && isoToEpoch(ss, b) && b > a) {
      long d = b - a;
      snprintf(sun_len, sizeof(sun_len), "%02ldh %02ldm", d / 3600,
               (d % 3600) / 60);
    }
  }

  {
    String bodyUV;
    String urlUV = String("https://api.open-meteo.com/v1/forecast") +
                   "?latitude=" + g_lat + "&longitude=" + g_lon +
                   "&timezone=auto&daily=uv_index_max&forecast_days=1";

    if (httpGET(urlUV, bodyUV, 8000)) {
      float uvi = -1;
      if (jsonDailyFirstNumber(bodyUV, "uv_index_max", uvi) && uvi >= 0)
        snprintf(sun_uvi, sizeof(sun_uvi), "%.1f", uvi);
    }
  }

  {
    String bodyICS;

    String urlICS = "https://mooncal.ch/mooncal.ics?created=1&lang=en"
                    "&phases[full]=true&phases[new]=true&phases[quarter]=true"
                    "&phases[daily]=false&style=fullmoon"
                    "&events[lunareclipse]=false&events[solareclipse]=false&"
                    "events[moonlanding]=false"
                    "&before=P45D&after=P45D&zone=Europe/Zurich";

    if (httpGET(urlICS, bodyICS, 12000)) {

      MoonEvent prev{0, 0, false};
      MoonEvent next{0, 0, false};

      time_t nowUTC = time(nullptr);

      int pos = 0;
      while (true) {
        int evStart = bodyICS.indexOf("BEGIN:VEVENT", pos);
        if (evStart < 0)
          break;

        int evEnd = bodyICS.indexOf("END:VEVENT", evStart);
        if (evEnd < 0)
          break;

        String ev = bodyICS.substring(evStart, evEnd);

        int pdt = ev.indexOf("DTSTART");
        if (pdt < 0) {
          pos = evEnd;
          continue;
        }
        int ln = ev.indexOf('\n', pdt);
        if (ln < 0)
          ln = ev.length();
        String dtline = ev.substring(pdt, ln);

        time_t ts = parseDate(dtline);

        int pu = ev.indexOf("UID:");
        if (pu < 0) {
          pos = evEnd;
          continue;
        }
        int eu = ev.indexOf('\n', pu);
        if (eu < 0)
          eu = ev.length();
        String uid = ev.substring(pu, eu);

        uint8_t ph4 = uidToPhase4(uid);
        if (ph4 == 255) {
          pos = evEnd;
          continue;
        }

        if (ts <= nowUTC) {
          prev.ts = ts;
          prev.phase4 = ph4;
          prev.valid = true;
        } else {
          next.ts = ts;
          next.phase4 = ph4;
          next.valid = true;
          break;
        }

        pos = evEnd;
      }

      interpolatePhase(prev, next, nowUTC);
    }
  }

  g_shadowReady = false;
  return true;
}

static inline void sunRowColored(const char *label, const char *val, int &y,
                                 uint16_t labelColor) {
  const int SZ = TEXT_SCALE + 1;
  const int H = BASE_CHAR_H * SZ + 8;

  gfx->setTextSize(SZ);
  gfx->setTextColor(labelColor);
  gfx->setCursor(PAGE_X, y);
  gfx->print(label);

  gfx->setTextColor(COL_ACCENT2);
  int w = strlen(val) * BASE_CHAR_W * SZ;
  int x = gfx->width() - PAGE_X - w;
  if (x < PAGE_X)
    x = PAGE_X;
  gfx->setCursor(x, y);
  gfx->print(val);

  y += H;
}

static inline void sunRow(const char *label, const char *val, int &y) {
  sunRowColored(label, val, y, COL_ACCENT1);
}

static uint16_t uvLabelColor() {
  if (sun_uvi[0] == '-')
    return COL_ACCENT1;

  const float uv = atof(sun_uvi);
  if (uv < 3.0f)
    return 0x07E0; // verde: basso (0-2)
  if (uv < 6.0f)
    return 0xFFE0; // giallo: moderato (3-5)
  return 0xF800;   // rosso: alto o superiore (6+)
}

static void buildMoonPhaseLabel(bool it, char out[32]) {

  if (g_moon_illum01 < 0) {
    strcpy(out, it ? "Dati non disponibili" : "No data");
    return;
  }

  float illum = g_moon_illum01;
  bool w = g_moon_waxing;

  const char *en, *itn;

  if (illum < 0.03f) {
    en = "New Moon";
    itn = "Luna nuova";
  } else if (illum < 0.47f) {
    en = w ? "Waxing Crescent" : "Waning Crescent";
    itn = w ? "Falce crescente" : "Falce calante";
  } else if (illum < 0.53f) {
    en = w ? "First Quarter" : "Last Quarter";
    itn = w ? "Primo quarto" : "Ultimo quarto";
  } else if (illum < 0.97f) {
    en = w ? "Waxing Gibbous" : "Waning Gibbous";
    itn = w ? "Gibbosa crescente" : "Gibbosa calante";
  } else {
    en = "Full Moon";
    itn = "Luna piena";
  }

  strcpy(out, it ? itn : en);
}

static void drawMoonPhaseGraphic(int16_t cx, int16_t cy, int16_t r) {
  if (g_moon_illum01 < 0)
    return;

  ensureMoonTextureDecoded();
  buildShadowTexture(r);

  for (int y = -r; y <= r; y++) {
    int big2 = r * r - y * y;
    if (big2 < 0)
      continue;
    int xlim = (int)sqrtf(big2);
    int row = (y + r) * LUNA_WIDTH;

    for (int x = -xlim; x <= xlim; x++) {
      int px = x + r;
      if (px < 0 || px >= LUNA_WIDTH)
        continue;
      gfx->drawPixel(cx + x, cy + y, g_shadowTex[row + px]);
    }
  }

  gfx->drawCircle(cx, cy, r, 0x0000);
}

void pageSun() {
  const bool it = (g_lang == "it");

  gfx->fillScreen(0x0000);
  drawHeader(it ? "Ore di luce oggi" : "Today's daylight");

  int y = PAGE_Y + 20;

  if (sun_rise[0] == '-') {
    drawBoldMain(PAGE_X, y,
                 it ? "Nessun dato disponibile" : "No data available",
                 TEXT_SCALE);
    return;
  }

  sunRow(it ? "Alba" : "Sunrise", sun_rise, y);
  sunRow(it ? "Tramonto" : "Sunset", sun_set, y);
  sunRow(it ? "Mezzogiorno" : "Solar noon", sun_noon, y);
  sunRow(it ? "Durata luce" : "Day length", sun_len, y);
  sunRow(it ? "Civile inizio" : "Civil begin", sun_cb, y);
  sunRow(it ? "Civile fine" : "Civil end", sun_ce, y);
  sunRowColored(it ? "Indice UV" : "UV index", sun_uvi, y, uvLabelColor());

  char label[32];
  buildMoonPhaseLabel(it, label);

  gfx->setTextSize(TEXT_SCALE);
  gfx->setTextColor(COL_TEXT);

  gfx->setCursor(PAGE_X, y + 10);
  gfx->print(label);

  const int16_t r = 85;
  const int16_t margin = 30;

  int16_t cx = gfx->width() / 2 + 70;
  int16_t cy = gfx->height() - margin - r;

  drawMoonPhaseGraphic(cx, cy, r);
}
