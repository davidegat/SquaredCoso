#pragma once

#include <Arduino.h>
#include <time.h>
#include <math.h>
#include "../handlers/globals.h"

// ---------------------------------------------------------------------------
// SquaredCoso – Pagina SUN (alba/tramonto/durata luce + UV + )
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

extern void   drawHeader(const String& title);
extern void   drawBoldMain(int16_t x, int16_t y, const String& raw, uint8_t scale);
extern bool   httpGET(const String& url, String& body, uint32_t timeoutMs);
extern String sanitizeText(const String& in);

extern String g_lat, g_lon, g_lang;

// ===========================================================================
// CACHE
// ===========================================================================
static char  sun_rise[6];
static char  sun_set [6];
static char  sun_noon[6];
static char  sun_cb  [6];
static char  sun_ce  [6];
static char  sun_len [16];
static char  sun_uvi [8];

static float g_moon_phase01 = -1.0f;

// ===========================================================================
// timegm
// ===========================================================================
static time_t my_timegm(const struct tm *t)
{
  static const int mdays[12] =
    {31,28,31,30,31,30,31,31,30,31,30,31};

  int year  = t->tm_year + 1900;
  int month = t->tm_mon;
  int day   = t->tm_mday;

  long days = 0;

  for (int y = 1970; y < year; y++) {
    days += 365;
    if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))
      days++;
  }

  for (int m = 0; m < month; m++) {
    days += mdays[m];
    if (m == 1 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)))
      days++;
  }

  days += (day - 1);

  return (time_t)(
    days       * 86400L +
    t->tm_hour * 3600L +
    t->tm_min  * 60L +
    t->tm_sec
  );
}

// ===========================================================================
// ISO → HH:MM
// ===========================================================================
static inline void isoToHM(const String& iso, char out[6])
{
  int t = iso.indexOf('T');
  if (t < 0 || t + 5 >= iso.length()) {
    strcpy(out, "--:--");
    return;
  }
  out[0] = iso[t+1];
  out[1] = iso[t+2];
  out[2] = ':';
  out[3] = iso[t+4];
  out[4] = iso[t+5];
  out[5] = 0;
}

// ===========================================================================
// ISO → epoch
// ===========================================================================
static bool isoToEpoch(const String& s, time_t& out)
{
  if (s.length() < 19) return false;

  struct tm tt {};
  tt.tm_year = s.substring(0,4).toInt() - 1900;
  tt.tm_mon  = s.substring(5,7).toInt() - 1;
  tt.tm_mday = s.substring(8,10).toInt();
  tt.tm_hour = s.substring(11,13).toInt();
  tt.tm_min  = s.substring(14,16).toInt();
  tt.tm_sec  = s.substring(17,19).toInt();

  out = my_timegm(&tt);
  return true;
}

// ===========================================================================
// JSON "key":"value"
// ===========================================================================
static bool jsonKV(const String& body, const char* key, String& out)
{
  String k = "\""; k += key; k += "\"";
  int p = body.indexOf(k);
  if (p < 0) return false;

  p = body.indexOf('"', p + k.length());
  if (p < 0) return false;

  int q = body.indexOf('"', p + 1);
  if (q < 0) return false;

  out = body.substring(p+1, q);
  return true;
}

// ===========================================================================
// JSON – primo numero daily[key]
// ===========================================================================
static bool jsonDailyFirstNumber(const String& body, const char* key, float& out)
{
  String k = "\""; k += key; k += "\":[";
  int p = body.indexOf(k);
  if (p < 0) return false;

  p += k.length();
  int len = body.length();
  int start = -1;

  for (int i = p; i < len; i++) {
    char c = body[i];
    if ((c>='0' && c<='9') || c=='+' || c=='-') { start=i; break; }
    if (c=='n') return false;
  }
  if (start < 0) return false;

  int end = start+1;
  while (end < len) {
    char c = body[end];
    if ((c>='0'&&c<='9') || c=='.' || c=='e' || c=='E' || c=='+' || c=='-')
      end++;
    else break;
  }

  out = body.substring(start,end).toFloat();
  return true;
}

// ===========================================================================
// Fase lunare da time()
// ===========================================================================
static void computeMoonPhaseFromTime()
{
  time_t now = time(nullptr);
  if (now < 1000000000L) {
    g_moon_phase01 = -1;
    return;
  }

  double jd = now / 86400.0 + 2440587.5;
  double d  = jd - 2451550.1;
  d = fmod(d, 29.53058867);
  if (d < 0) d += 29.53058867;

  g_moon_phase01 = (float)(d / 29.53058867);
}

// ===========================================================================
// Fetch SUN
// ===========================================================================
bool fetchSun()
{
  strcpy(sun_rise,"--:--");
  strcpy(sun_set ,"--:--");
  strcpy(sun_noon,"--:--");
  strcpy(sun_len ,"--h --m");
  strcpy(sun_cb  ,"--:--");
  strcpy(sun_ce  ,"--:--");
  strcpy(sun_uvi ,"--");

  g_moon_phase01 = -1;

  if (g_lat.isEmpty() || g_lon.isEmpty())
    return false;

  // Sunrise-sunset
  String url =
    "https://api.sunrise-sunset.org/json?lat=" + g_lat +
    "&lng=" + g_lon + "&formatted=0";

  String body;
  if (!httpGET(url, body, 10000))
    return false;

  String sr,ss,sn,cb,ce;
  if (!jsonKV(body,"sunrise",sr)) return false;
  if (!jsonKV(body,"sunset", ss)) return false;

  jsonKV(body,"solar_noon",sn);
  jsonKV(body,"civil_twilight_begin",cb);
  jsonKV(body,"civil_twilight_end",ce);

  isoToHM(sr,sun_rise);
  isoToHM(ss,sun_set);
  isoToHM(sn,sun_noon);
  isoToHM(cb,sun_cb);
  isoToHM(ce,sun_ce);

  time_t a,b;
  if (isoToEpoch(sr,a) && isoToEpoch(ss,b) && b > a) {
    long d=b-a;
    snprintf(sun_len,sizeof(sun_len), "%02ldh %02ldm", d/3600,(d%3600)/60);
  }

  // UV index
  String url2 =
    "https://api.open-meteo.com/v1/forecast"
    "?latitude="  + g_lat +
    "&longitude=" + g_lon +
    "&timezone=auto"
    "&daily=uv_index_max"
    "&forecast_days=1";

  String body2;
  if (httpGET(url2, body2, 8000)) {
    float uvi=-1;
    if (jsonDailyFirstNumber(body2,"uv_index_max",uvi) && uvi>=0)
      snprintf(sun_uvi,sizeof(sun_uvi),"%.1f",uvi);
  }

  computeMoonPhaseFromTime();
  return true;
}

// ===========================================================================
// Riga testo
// ===========================================================================
static inline void sunRow(const char* label, const char* val, int& y)
{
  const int SZ = TEXT_SCALE+1;
  const int H  = BASE_CHAR_H*SZ + 8;

  gfx->setTextSize(SZ);

  gfx->setTextColor(COL_ACCENT1, COL_BG);
  gfx->setCursor(PAGE_X,y);
  gfx->print(label);

  gfx->setTextColor(COL_ACCENT2, COL_BG);

  int w = strlen(val)*BASE_CHAR_W*SZ;
  int x = gfx->width() - PAGE_X - w;
  if (x < PAGE_X) x = PAGE_X;

  gfx->setCursor(x, y);
  gfx->print(val);

  y += H;
}

// ===========================================================================
// Label fase lunare
// ===========================================================================
static void buildMoonPhaseLabel(bool it, char out[24])
{
  strcpy(out, it?"Luna --":"Moon --");
  if (g_moon_phase01 < 0) return;

  float p = g_moon_phase01;

  if      (p < 0.0625f || p >= 0.9375f) strcpy(out,it?"Luna nuova"      :"New moon");
  else if (p < 0.1875f)                 strcpy(out,it?"Falce crescente" :"Waxing crescent");
  else if (p < 0.3125f)                 strcpy(out,it?"Primo quarto"    :"First quarter");
  else if (p < 0.4375f)                 strcpy(out,it?"Gibbosa cresc."  :"Waxing gibbous");
  else if (p < 0.5625f)                 strcpy(out,it?"Luna piena"      :"Full moon");
  else if (p < 0.6875f)                 strcpy(out,it?"Gibbosa calante" :"Waning gibbous");
  else if (p < 0.8125f)                 strcpy(out,it?"Ultimo quarto"   :"Last quarter");
  else                                   strcpy(out,it?"Falce calante"  :"Waning crescent");
}

// ===========================================================================
// LUNA — terminatore corretto + falce NON invertita
// ===========================================================================
static void drawMoonPhaseGraphic(int16_t cx, int16_t cy, int16_t r)
{
  if (g_moon_phase01 < 0) return;

  const uint16_t COL_DARK = 0x0000;
  const uint16_t COL_LIT  = 0xFFFF;
  const uint16_t COL_EDGE = 0xC618;

  float phase = g_moon_phase01;
  float i = phase * TWO_PI;

  float si = sinf(i);
  float co = cosf(i);

  for (int y=-r; y<=r; y++) {
    for (int x=-r; x<=r; x++) {

      int d2 = x*x + y*y;
      if (d2 > r*r) continue;

      float X = x;
      float Y = y;

      float z = sqrtf((float)(r*r - X*X - Y*Y));

      // *** CORREZIONE QUI ***  
      bool lit = (X * co - z * si >= 0.0f);

      bool edge = (d2 >= (r-1)*(r-1));

      uint16_t col =
        edge ? COL_EDGE :
        lit  ? COL_LIT  :
               COL_DARK;

      gfx->drawPixel(cx+x, cy+y, col);
    }
  }
}

// ===========================================================================
// Page SUN
// ===========================================================================
void pageSun()
{
  const bool it = (g_lang=="it");

  drawHeader(it?"Ore di luce oggi":"Today's daylight");

  int y = PAGE_Y + 20;

  if (sun_rise[0]=='-') {
    drawBoldMain(PAGE_X,y,
      it?"Nessun dato disponibile":"No data available",
      TEXT_SCALE);
    return;
  }

  sunRow(it?"Alba"         :"Sunrise",    sun_rise,y);
  sunRow(it?"Tramonto"     :"Sunset",     sun_set ,y);
  sunRow(it?"Mezzogiorno"  :"Solar noon", sun_noon,y);
  sunRow(it?"Durata luce"  :"Day length", sun_len ,y);
  sunRow(it?"Civile inizio":"Civil begin",sun_cb  ,y);
  sunRow(it?"Civile fine"  :"Civil end",  sun_ce  ,y);
  sunRow(it?"Indice UV"    :"UV index",   sun_uvi ,y);

  char label[24];
  buildMoonPhaseLabel(it,label);

  gfx->setTextSize(TEXT_SCALE);
  gfx->setTextColor(COL_TEXT,COL_BG);
  gfx->setCursor(PAGE_X, y+4);
//  gfx->print(it?"Fase lunare":"Moon phase");

//  gfx->setTextSize(TEXT_SCALE-1);
//  gfx->setTextColor(COL_ACCENT1,COL_BG);
//  gfx->setCursor(PAGE_X, y+4 + BASE_CHAR_H*TEXT_SCALE + 4);
  gfx->print(label);

  // nuova posizione + r leggermente più grande + shift destra
  const int16_t margin = 30;
  const int16_t r      = 85;

  int16_t cx = gfx->width()/2 + 70;      // → spostata a destra
  int16_t cy = gfx->height() - margin - r;

  drawMoonPhaseGraphic(cx, cy, r);
}

