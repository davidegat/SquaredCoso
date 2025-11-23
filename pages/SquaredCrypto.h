#pragma once

#include <Arduino.h>
#include "../handlers/globals.h"

// ---------------------------------------------------------------------------
// SquaredCoso – Pagina "CRYPTO – BITCOIN"
// - fetchCryptoWrapper() → aggiorna prezzo BTC e variazione 24h da CoinGecko
// - pageCryptoWrapper()  → rende prezzo, change 24h e valore totale posseduto
//   Parsing JSON minimale, nessuna alloc dinamica extra, solo stato statico.
// ---------------------------------------------------------------------------

// risorse condivise dal core
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
extern const int CHAR_H;

extern String g_lang;
extern String g_fiat;
extern double g_btc_owned;

extern bool   httpGET(const String& url, String& out, uint32_t timeout);
extern int    indexOfCI(const String& src, const String& key, int from);
extern void   drawHeader(const String& title);
extern void   drawHLine(int y);
extern void   drawBoldMain(int16_t x, int16_t y, const String& raw, uint8_t scale);

// ---------------------------------------------------------------------------
// Stato locale del modulo crypto (prezzo, change 24h, timestamp ultimo fetch)
// ---------------------------------------------------------------------------
static double   cr_price          = NAN;
static double   cr_prev_price     = NAN;
static double   cr_chg24          = NAN;
static uint32_t cr_last_update_ms = 0;

// ---------------------------------------------------------------------------
// Parsing JSON: cerca "key": NUMBER all'interno del body
// ---------------------------------------------------------------------------
static bool crFindNumberKV(const String& body,
                           const char* key,
                           int from,
                           double& outVal)
{
  char kbuf[48];
  snprintf(kbuf, sizeof(kbuf), "\"%s\"", key);

  const char* base  = body.c_str();
  const char* start = base + from;

  const char* p = strstr(start, kbuf);
  if (!p) return false;

  const char* colon = strchr(p, ':');
  if (!colon) return false;

  const char* s = colon + 1;

  while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') s++;

  char*  endptr = nullptr;
  double val    = strtod(s, &endptr);
  if (endptr == s) return false;

  outVal = val;
  return true;
}

// ---------------------------------------------------------------------------
// Format importo fiat con separatori migliaia, centesimi opzionali
// ---------------------------------------------------------------------------
static String crFmtFiat(double v)
{
  if (isnan(v)) return "--.--";

  long long ip = (long long)v;
  double    f  = v - (double)ip;
  if (f < 0) f = 0;

  int cents = (int)(f * 100.0 + 0.5);
  if (cents >= 100) {
    ip++;
    cents -= 100;
  }

  char buf[32];
  snprintf(buf, sizeof(buf), "%lld", ip);
  String sInt = buf;

  String out;
  out.reserve(sInt.length() + 4);
  int cnt = 0;

  for (int i = sInt.length() - 1; i >= 0; --i) {
    out = sInt.charAt(i) + out;
    if (++cnt == 3 && i > 0) {
      out = "'" + out;
      cnt = 0;
    }
  }

  if (cents == 0) return out;

  char cb[8];
  snprintf(cb, sizeof(cb), "%02d", cents);
  return out + "." + cb;
}

// ---------------------------------------------------------------------------
// Format quantità BTC con 8 decimali, notazione europea (virgola)
// ---------------------------------------------------------------------------
static String crFmtBTC(double v)
{
  if (isnan(v)) return "--";
  String s = String(v, 8);
  s.replace('.', ',');
  return s;
}

// ---------------------------------------------------------------------------
// Fetch da CoinGecko: prezzo BTC in g_fiat + variazione 24h
// ---------------------------------------------------------------------------
static bool fetchCrypto()
{
  if (!g_fiat.length())
    g_fiat = "CHF";

  String fiat = g_fiat;
  fiat.toLowerCase();

  String url =
    "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin"
    "&vs_currencies=" + fiat +
    "&include_24hr_change=true";

  String body;
  if (!httpGET(url, body, 10000))
    return false;

  int b = indexOfCI(body, "\"bitcoin\"", 0);
  if (b < 0) return false;

  double price = NAN;
  if (!crFindNumberKV(body, fiat.c_str(), b, price))
    return false;

  double pct = NAN;
  {
    String k = fiat + "_24h_change";
    (void)crFindNumberKV(body, k.c_str(), b, pct);
  }

  cr_prev_price      = cr_price;
  cr_price           = price;
  cr_chg24           = pct;
  cr_last_update_ms  = millis();

  return true;
}

// ---------------------------------------------------------------------------
// Render pagina: prezzo grande, change 24h e valore totale BTC posseduto
// ---------------------------------------------------------------------------
static void pageCrypto()
{
  const bool it = (g_lang == "it");

  drawHeader("Bitcoin");
  int y = PAGE_Y;

  gfx->setTextSize(6);
  gfx->setTextColor(COL_TEXT, COL_BG);

  String sPrice =
    isnan(cr_price)
      ? "--.--"
      : (g_fiat + " " + crFmtFiat(cr_price));

  int tw = sPrice.length() * BASE_CHAR_W * 6;
  gfx->setCursor((480 - tw) / 2, y + 70);
  gfx->print(sPrice);

  gfx->setTextSize(3);

  String   s24 = "--%";
  uint16_t col = COL_TEXT;

  if (!isnan(cr_chg24)) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%.2f%%", cr_chg24);
    s24 = buf;
    col = (cr_chg24 >= 0.0) ? 0x0000 : 0x9000;
  }

  gfx->setTextColor(col, COL_BG);
  int wtxt = s24.length() * BASE_CHAR_W * 3;
  gfx->setCursor((480 - wtxt) / 2, y + 150);
  gfx->print(s24);

  if (!isnan(g_btc_owned) && !isnan(cr_price)) {

    gfx->setTextSize(3);

    String L1 = crFmtBTC(g_btc_owned) + " BTC";
    int w1 = L1.length() * BASE_CHAR_W * 3;
    gfx->setCursor((480 - w1) / 2, y + 240);
    gfx->print(L1);

    double tot = g_btc_owned * cr_price;
    String L2 = "= " + g_fiat + " " + crFmtFiat(tot);
    int w2 = L2.length() * BASE_CHAR_W * 3;
    gfx->setCursor((480 - w2) / 2, y + 280);
    gfx->print(L2);
  }

  gfx->setTextSize(2);
  int fy = y + 340;
  drawHLine(fy);
  fy += 12;

  String sub = it
    ? "Fonte: CoinGecko | Aggiornato "
    : "Source: CoinGecko | Updated ";

  if (cr_last_update_ms) {
    unsigned long s = (millis() - cr_last_update_ms) / 1000;
    sub += String(s) + (it ? "s fa" : "s ago");
  } else {
    sub += (it ? "n/d" : "n/a");
  }

  drawBoldMain(PAGE_X, fy + CHAR_H, sub, TEXT_SCALE);

  gfx->setTextSize(TEXT_SCALE);
}

// ---------------------------------------------------------------------------
// Wrapper usati dal main (nomi stabili per il page-router)
// ---------------------------------------------------------------------------
inline bool fetchCryptoWrapper() { return fetchCrypto(); }
inline void pageCryptoWrapper()  { pageCrypto(); }

