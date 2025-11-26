#pragma once

#include <Arduino.h>
#include <time.h>
#include <math.h>
#include "../handlers/globals.h"

// ---------------------------------------------------------------------------
// IMMAGINE LUNA RLE
// ---------------------------------------------------------------------------
#include "../images/luna.h"  // definisce: LUNA_WIDTH, LUNA_HEIGHT, luna[] (RLERun)

// Wrapper CosinoRLE per la nostra luna RLE
static const CosinoRLE LUNA_ART = {
  luna,
  sizeof(luna) / sizeof(RLERun)
};

// ---------------------------------------------------------------------------
// SquaredCoso – Pagina SUN
// ---------------------------------------------------------------------------

extern Arduino_RGB_Display* gfx;

extern const uint16_t COL_BG;
extern const uint16_t COL_TEXT;
extern const uint16_t COL_ACCENT1;
extern const uint16_t COL_ACCENT2;

extern const int PAGE_X;
extern const int PAGE_Y;
extern const int BASE_CHAR_W;
extern const int BASE_CHAR_H;
extern const int TEXT_SCALE;

extern void drawHeader(const String& title);
extern void drawBoldMain(int16_t x, int16_t y, const String& raw, uint8_t scale);
extern bool httpGET(const String& url, String& body, uint32_t timeoutMs);

extern String g_lat, g_lon, g_lang;

// ---------------------------------------------------------------------------
// Cache
// ---------------------------------------------------------------------------
static char sun_rise[6];
static char sun_set[6];
static char sun_noon[6];
static char sun_cb[6];
static char sun_ce[6];
static char sun_len[16];
static char sun_uvi[8];

static float g_moon_phase_days = -1.0f;
static float g_moon_illum01 = -1.0f;
static bool g_moon_waxing = true;
static uint8_t g_moon_phase_idx = 0;

// ---------------------------------------------------------------------------
// TEXTURE LUNA + SHADOW (precompute)
// ---------------------------------------------------------------------------
static uint16_t g_moonTex[LUNA_WIDTH * LUNA_HEIGHT];
static bool g_moonTexReady = false;

static uint16_t g_shadowTex[LUNA_WIDTH * LUNA_HEIGHT];
static bool g_shadowReady = false;
static int g_shadowPhaseBucket = -1;

// ============================================================================
// TIME & JSON UTILITY
// ============================================================================

static long calcTZ() {
  time_t now = time(nullptr);
  struct tm lo {
  }, gm{};
  localtime_r(&now, &lo);
  gmtime_r(&now, &gm);
  long d = lo.tm_hour - gm.tm_hour;
  if (d > 12) d -= 24;
  if (d < -12) d += 24;
  return d;
}

static inline void isoToHM(const String& iso, char out[6]) {
  int t = iso.indexOf('T');
  if (t < 0 || t + 5 >= iso.length()) {
    strcpy(out, "--:--");
    return;
  }
  out[0] = iso[t + 1];
  out[1] = iso[t + 2];
  out[2] = ':';
  out[3] = iso[t + 4];
  out[4] = iso[t + 5];
  out[5] = 0;
}

static bool isoToEpoch(const String& s, time_t& out) {
  if (s.length() < 19) return false;
  struct tm tt {};
  tt.tm_year = s.substring(0, 4).toInt() - 1900;
  tt.tm_mon = s.substring(5, 7).toInt() - 1;
  tt.tm_mday = s.substring(8, 10).toInt();
  tt.tm_hour = s.substring(11, 13).toInt();
  tt.tm_min = s.substring(14, 16).toInt();
  tt.tm_sec = s.substring(17, 19).toInt();
  time_t local = mktime(&tt);
  out = local - calcTZ() * 3600;
  return true;
}

static bool jsonKV(const String& body, const char* key, String& out) {
  String k = "\"";
  k += key;
  k += "\"";
  int p = body.indexOf(k);
  if (p < 0) return false;
  p = body.indexOf('"', p + k.length());
  if (p < 0) return false;
  int q = body.indexOf('"', p + 1);
  if (q < 0) return false;
  out = body.substring(p + 1, q);
  return true;
}

static bool jsonDailyFirstNumber(const String& body, const char* key, float& out) {
  String k = "\"";
  k += key;
  k += "\":[";
  int p = body.indexOf(k);
  if (p < 0) return false;

  p += k.length();
  int start = -1;
  int len = body.length();

  for (int i = p; i < len; i++) {
    char c = body[i];
    if ((c >= '0' && c <= '9') || c == '+' || c == '-') {
      start = i;
      break;
    }
    if (c == 'n') return false;
  }
  if (start < 0) return false;

  int end = start + 1;
  while (end < len) {
    char c = body[end];
    if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') end++;
    else break;
  }

  out = body.substring(start, end).toFloat();
  return true;
}

// ============================================================================
// PHASE
// ============================================================================
static float calculateMoonPhaseDays(int y, int m, int d) {
  if (m < 3) {
    y--;
    m += 12;
  }
  int A = y / 100;
  int B = A / 4;
  int C = 2 - A + B;
  float E = floorf(365.25f * (y + 4716));
  float F = floorf(30.6001f * (m + 1));
  float jd = C + d + E + F - 1524.5f;
  float days = jd - 2451549.5f;
  float nm = days / 29.53f;
  float ph = (nm - (int)nm) * 29.53f;
  if (ph < 0) ph += 29.53f;
  return ph;
}

// ============================================================================
// DECODE RLE → RAM (UNA VOLTA)
// ============================================================================
static void ensureMoonTextureDecoded() {
  if (g_moonTexReady) return;

  int32_t total = LUNA_WIDTH * LUNA_HEIGHT;
  int32_t idx = 0;

  for (size_t i = 0; i < LUNA_ART.runs && idx < total; i++) {

    uint16_t col = LUNA_ART.data[i].color;  // FIX
    uint16_t count = LUNA_ART.data[i].count;

    for (uint16_t k = 0; k < count && idx < total; k++) {
      g_moonTex[idx++] = col;
    }
  }

  g_moonTexReady = true;
}


// ============================================================================
// PRECOMPUTE SHADOW (TRASPARENTE) UNA VOLTA PER FASE
// ============================================================================
static void buildShadowTexture(int r) {
  // bucket fase → 8 sezioni
  int bucket = (int)((g_moon_phase_days / 29.53f) * 8.0f);
  if (bucket < 0) bucket = 0;
  if (bucket > 7) bucket = 7;

  if (bucket == g_shadowPhaseBucket) {
    g_shadowReady = true;
    return;
  }

  g_shadowPhaseBucket = bucket;

  float illum = g_moon_illum01;
  float phase = g_moon_phase_days;

  const float ALPHA = 0.35f;
  float k = (illum * 2.0f) - 1.0f;
  float shift = (phase < 14.765f) ? (-fabs(k) * r) : (+fabs(k) * r);

  for (int y = -r; y <= r; y++) {

    int y2 = y + r;
    int row = y2 * LUNA_WIDTH;

    int big2 = r * r - y * y;
    if (big2 < 0) continue;

    int xlim = (int)sqrtf(big2);

    for (int x = -xlim; x <= xlim; x++) {

      int px = x + r;
      if (px < 0 || px >= LUNA_WIDTH) continue;

      uint16_t base = g_moonTex[row + px];

      float dx = x - shift;
      float dist2 = dx * dx + y * y;

      if (dist2 <= r * r) {
        // blend trasparente
        uint8_t R = (base >> 11) & 0x1F;
        uint8_t G = (base >> 5) & 0x3F;
        uint8_t B = base & 0x1F;

        R = (uint8_t)(R * ALPHA);
        G = (uint8_t)(G * ALPHA);
        B = (uint8_t)(B * ALPHA);

        g_shadowTex[row + px] = (R << 11) | (G << 5) | B;
      } else {
        g_shadowTex[row + px] = base;
      }
    }
  }

  g_shadowReady = true;
}

// ============================================================================
// Fetch SUN + MOON
// ============================================================================
bool fetchSun() {
  strcpy(sun_rise, "--:--");
  strcpy(sun_set, "--:--");
  strcpy(sun_noon, "--:--");
  strcpy(sun_cb, "--:--");
  strcpy(sun_ce, "--:--");
  strcpy(sun_len, "--h --m");
  strcpy(sun_uvi, "--");

  g_moon_phase_days = -1;
  g_moon_illum01 = -1;
  g_moon_phase_idx = 0;
  g_moon_waxing = true;

  if (g_lat.isEmpty() || g_lon.isEmpty()) return false;

  time_t now = time(nullptr);
  struct tm t {};
  localtime_r(&now, &t);

  // --- SUN ---
  String body;
  String url = "https://api.sunrise-sunset.org/json?lat=" + g_lat + "&lng=" + g_lon + "&formatted=0";
  if (!httpGET(url, body, 10000)) return false;

  String sr, ss, sn, cb, ce;
  if (!jsonKV(body, "sunrise", sr)) return false;
  if (!jsonKV(body, "sunset", ss)) return false;

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
    snprintf(sun_len, sizeof(sun_len), "%02ldh %02ldm", d / 3600, (d % 3600) / 60);
  }

  // --- UV ---
  String bodyUV;
  String urlUV = String("https://api.open-meteo.com/v1/forecast") + "?latitude=" + g_lat + "&longitude=" + g_lon + "&timezone=auto&daily=uv_index_max&forecast_days=1";

  if (httpGET(urlUV, bodyUV, 8000)) {
    float uvi = -1;
    if (jsonDailyFirstNumber(bodyUV, "uv_index_max", uvi) && uvi >= 0)
      snprintf(sun_uvi, sizeof(sun_uvi), "%.1f", uvi);
  }

  // --- MOON ---
  g_moon_phase_days = calculateMoonPhaseDays(
    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

  g_moon_phase_idx = (uint8_t)((g_moon_phase_days / 29.53f) * 8.0f) % 8;

  g_moon_illum01 = (1.0f - cosf((g_moon_phase_days / 29.53f) * 2.0f * PI)) * 0.5f;

  g_moon_waxing = (g_moon_phase_days < 14.765f);

  // invalida ombra, verrà rigenerata
  g_shadowReady = false;

  return true;
}

// ============================================================================
// Riga di testo sole
// ============================================================================
static inline void sunRow(const char* label, const char* val, int& y) {
  const int SZ = TEXT_SCALE + 1;
  const int H = BASE_CHAR_H * SZ + 8;

  gfx->setTextSize(SZ);
  gfx->setTextColor(COL_ACCENT1);
  gfx->setCursor(PAGE_X, y);
  gfx->print(label);

  gfx->setTextColor(COL_ACCENT2);

  int w = strlen(val) * BASE_CHAR_W * SZ;
  int x = gfx->width() - PAGE_X - w;
  if (x < PAGE_X) x = PAGE_X;

  gfx->setCursor(x, y);
  gfx->print(val);

  y += H;
}

// ============================================================================
// Etichetta fase lunare
// ============================================================================
static void buildMoonPhaseLabel(bool it, char out[32]) {
  if (g_moon_illum01 < 0) {
    strcpy(out, it ? "Luna --" : "Moon --");
    return;
  }

  static const char* en[8] = {
    "New moon", "Waxing crescent", "First quarter", "Waxing gibbous",
    "Full moon", "Waning gibbous", "Last quarter", "Waning crescent"
  };

  static const char* itn[8] = {
    "Luna nuova", "Falce crescente", "Primo quarto", "Gibbosa crescente",
    "Luna piena", "Gibbosa calante", "Ultimo quarto", "Falce calante"
  };

  uint8_t idx = g_moon_phase_idx;
  if (idx > 7) idx = 7;
  snprintf(out, 32, "%s", it ? itn[idx] : en[idx]);
}

// ============================================================================
// Disegno luna con ombra TRASPARENTE precompute
// ============================================================================
static void drawMoonPhaseGraphic(int16_t cx, int16_t cy, int16_t r) {
  if (g_moon_illum01 < 0) return;

  ensureMoonTextureDecoded();
  buildShadowTexture(r);  // solo se cambia fase

  // copia RAM → schermo
  for (int y = -r; y <= r; y++) {
    int big2 = r * r - y * y;
    if (big2 < 0) continue;

    int xlim = (int)sqrtf(big2);

    int row = (y + r) * LUNA_WIDTH;

    for (int x = -xlim; x <= xlim; x++) {
      int px = x + r;
      if (px < 0 || px >= LUNA_WIDTH) continue;

      uint16_t col = g_shadowTex[row + px];

      gfx->drawPixel(cx + x, cy + y, col);
    }
  }

  gfx->drawCircle(cx, cy, r, 0x0000);
}

// ============================================================================
// Page SUN
// ============================================================================
void pageSun() {
  const bool it = (g_lang == "it");
  gfx->fillScreen(0x0000);

  drawHeader(it ? "Ore di luce oggi" : "Today's daylight");

  int y = PAGE_Y + 20;

  if (sun_rise[0] == '-') {
    drawBoldMain(PAGE_X, y, it ? "Nessun dato disponibile" : "No data available", TEXT_SCALE);
    return;
  }

  sunRow(it ? "Alba" : "Sunrise", sun_rise, y);
  sunRow(it ? "Tramonto" : "Sunset", sun_set, y);
  sunRow(it ? "Mezzogiorno" : "Solar noon", sun_noon, y);
  sunRow(it ? "Durata luce" : "Day length", sun_len, y);
  sunRow(it ? "Civile inizio" : "Civil begin", sun_cb, y);
  sunRow(it ? "Civile fine" : "Civil end", sun_ce, y);
  sunRow(it ? "Indice UV" : "UV index", sun_uvi, y);

  char label[32];
  buildMoonPhaseLabel(it, label);

  // --- Label fase lunare su due righe ---
  gfx->setTextSize(TEXT_SCALE);
  gfx->setTextColor(COL_TEXT);

  // spezza al primo spazio
  char* spacePtr = strchr(label, ' ');
  if (spacePtr) {
    *spacePtr = 0;  // chiude la prima parola
    const char* w1 = label;
    const char* w2 = spacePtr + 1;

    gfx->setCursor(PAGE_X, y + 10);
    gfx->print(w1);

    gfx->setCursor(PAGE_X, y + 4 + BASE_CHAR_H * TEXT_SCALE + 4);
    gfx->print(w2);
  } else {
    // fallback: una sola parola
    gfx->setCursor(PAGE_X, y + 10);
    gfx->print(label);
  }


  const int16_t r = 85;
  const int16_t margin = 30;

  int16_t cx = gfx->width() / 2 + 70;
  int16_t cy = gfx->height() - margin - r;

  drawMoonPhaseGraphic(cx, cy, r);
}
