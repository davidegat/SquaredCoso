/*
===============================================================================
   SQUARED — PAGINA "CRYPTO – BITCOIN"
   Descrizione: Fetch da CoinGecko ottimizzato, parsing compatto,
                card prezzo + variazione 24h, portfolio BTC, icona custom,
                e layout moderno tema crypto.
   Autore: Davide “gat” Nasato
   Repository: https://github.com/davidegat/SquaredCoso
   Licenza: CC BY-NC 4.0
===============================================================================
*/

#pragma once

#include "../handlers/globals.h"
#include <Arduino.h>

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
extern const int CHAR_H;

extern String g_lang;
extern String g_fiat;
extern double g_btc_owned;

extern bool httpGET(const String &url, String &out, uint32_t timeout);
extern int indexOfCI(const String &src, const String &key, int from);
extern void drawHeader(const String &title);
extern bool jsonNum(const String &src, const char *key, float &out, int from);

// ---------------------------------------------------------------------------
// Colori tema crypto
// ---------------------------------------------------------------------------
constexpr uint16_t CR_BG_DARK = 0x0000; // Nero
constexpr uint16_t CR_CARD = 0x10A2;    // Grigio card
constexpr uint16_t CR_BORDER = 0x2945;  // Bordo
constexpr uint16_t CR_ORANGE = 0xFBE0;  // Bitcoin orange
constexpr uint16_t CR_GREEN = 0x07E0;   // Verde profit
constexpr uint16_t CR_RED = 0xF800;     // Rosso loss
constexpr uint16_t CR_GRAY = 0x7BEF;    // Grigio testo secondario

// ---------------------------------------------------------------------------
// Stato locale
// ---------------------------------------------------------------------------
static float cr_price = NAN;
static float cr_chg24 = NAN;
static uint32_t cr_last_update_ms = 0;

// ---------------------------------------------------------------------------
// Format importo fiat con separatori migliaia
// ---------------------------------------------------------------------------
static void crFmtFiat(float v, char *out, uint8_t outSize) {
  if (isnan(v)) {
    strncpy(out, "--.--", outSize);
    out[outSize - 1] = 0;
    return;
  }

  // Parte intera + centesimi
  int32_t ip = (int32_t)v;
  int16_t cents = (int16_t)((v - (float)ip) * 100.0f + 0.5f);
  if (cents >= 100) {
    ip++;
    cents -= 100;
  }

  // --- converti l'intero in stringa normale (senza separatori) ---
  char raw[16];
  uint8_t rawLen = 0;

  if (ip == 0) {
    raw[rawLen++] = '0';
  } else {
    int32_t n = ip;
    while (n > 0 && rawLen < sizeof(raw)) {
      raw[rawLen++] = '0' + (n % 10);
      n /= 10;
    }
    // inverti
    for (uint8_t i = 0, j = rawLen - 1; i < j; i++, j--) {
      char c = raw[i];
      raw[i] = raw[j];
      raw[j] = c;
    }
  }

  // --- inserisci separatori ogni 3 cifre (procedendo da destra) ---
  char tmp[24];
  uint8_t t = 0;
  uint8_t count = 0;

  for (int i = rawLen - 1; i >= 0; i--) {
    if (count == 3) {
      tmp[t++] = '\''; // separatore CH
      count = 0;
    }
    tmp[t++] = raw[i];
    count++;
  }

  // tmp è al contrario, rigira per ottenere l’ordine corretto
  for (uint8_t i = 0, j = t - 1; i < j; i++, j--) {
    char c = tmp[i];
    tmp[i] = tmp[j];
    tmp[j] = c;
  }
  tmp[t] = 0;

  // --- scrivi nell’output finale ---
  uint8_t pos = 0;
  for (uint8_t i = 0; i < t && pos < outSize - 1; i++) {
    out[pos++] = tmp[i];
  }

  // --- aggiungi decimali (solo se non sono zero) ---
  if (cents > 0 && pos < outSize - 3) {
    out[pos++] = '.';
    out[pos++] = '0' + (cents / 10);
    out[pos++] = '0' + (cents % 10);
  }

  out[pos] = 0;
}

// ---------------------------------------------------------------------------
// Format quantità BTC (ottimizzato)
// ---------------------------------------------------------------------------
static void crFmtBTC(double v, char *out, uint8_t outSize) {
  if (isnan(v)) {
    memcpy_P(out, PSTR("--"), 3);
    return;
  }

  dtostrf(v, 1, 8, out);

  // Sostituisci . con ,
  for (uint8_t i = 0; out[i] && i < outSize; i++) {
    if (out[i] == '.') {
      out[i] = ',';
      break;
    }
  }
}

// ---------------------------------------------------------------------------
// Disegna icona Bitcoin stilizzata
// ---------------------------------------------------------------------------
static void drawBitcoinIcon(int16_t cx, int16_t cy, int16_t r) {
  // Cerchio esterno con sfumatura
  gfx->fillCircle(cx, cy, r, CR_ORANGE);
  gfx->drawCircle(cx, cy, r, 0xC560);     // Bordo scuro
  gfx->drawCircle(cx, cy, r - 1, 0xFD20); // Highlight interno

  // "B" centrata
  gfx->setTextSize(3);
  gfx->setTextColor(CR_BG_DARK);

  // Char 'B' è largo ~18px a size 3, alto ~24px
  int16_t bx = cx - 7;
  int16_t by = cy - 11;
  gfx->setCursor(bx, by);
  gfx->print('B');
}

// ---------------------------------------------------------------------------
// Disegna freccia trend
// ---------------------------------------------------------------------------
static void drawTrendArrow(int16_t x, int16_t y, bool up, uint16_t col) {
  if (up) {
    // Freccia su ▲
    gfx->fillTriangle(x, y + 16, x + 10, y, x + 20, y + 16, col);
  } else {
    // Freccia giù ▼
    gfx->fillTriangle(x, y, x + 10, y + 16, x + 20, y, col);
  }
}

// ---------------------------------------------------------------------------
// Fetch da CoinGecko (ottimizzato)
// ---------------------------------------------------------------------------
static bool fetchCrypto() {
  if (!g_fiat.length())
    g_fiat = F("CHF");

  String fiat = g_fiat;
  fiat.toLowerCase();

  String url = F("https://api.coingecko.com/api/v3/simple/"
                 "price?ids=bitcoin&vs_currencies=");
  url += fiat;
  url += F("&include_24hr_change=true");

  String body;
  if (!httpGET(url, body, 10000))
    return false;

  int16_t b = indexOfCI(body, F("\"bitcoin\""), 0);
  if (b < 0)
    return false;

  float priceF = NAN;
  if (!jsonNum(body, fiat.c_str(), priceF, b))
    return false;

  cr_price = priceF;

  // 24h change
  char keyBuf[20];
  snprintf_P(keyBuf, sizeof(keyBuf), PSTR("%s_24h_change"), fiat.c_str());

  float pctF = NAN;
  jsonNum(body, keyBuf, pctF, b);
  cr_chg24 = pctF;

  cr_last_update_ms = millis();
  return true;
}

// ---------------------------------------------------------------------------
// Render pagina BTC (layout moderno)
// ---------------------------------------------------------------------------
static void pageCrypto() {
  const bool it = (g_lang == "it");

  gfx->fillScreen(CR_BG_DARK);

  // === HEADER con linea accent ===
  gfx->fillRect(0, 0, 480, 4, CR_ORANGE);

  gfx->setTextSize(2);
  gfx->setTextColor(COL_TEXT);
  gfx->setCursor(PAGE_X, 16);
  gfx->print(F("Bitcoin"));

  // Icona in alto a destra
  drawBitcoinIcon(440, 35, 25);

  // === CARD PREZZO PRINCIPALE ===
  const int16_t cardY = 70;
  const int16_t cardH = 140;

  gfx->fillRoundRect(PAGE_X, cardY, 480 - PAGE_X * 2, cardH, 12, CR_CARD);
  gfx->drawRoundRect(PAGE_X, cardY, 480 - PAGE_X * 2, cardH, 12, CR_BORDER);

  // Prezzo grande centrato
  char priceBuf[24];
  crFmtFiat(cr_price, priceBuf, sizeof(priceBuf));

  gfx->setTextSize(5);
  gfx->setTextColor(COL_TEXT);

  char fullPrice[32];
  snprintf_P(fullPrice, sizeof(fullPrice), PSTR("%s %s"), g_fiat.c_str(),
             priceBuf);

  int16_t tw = strlen(fullPrice) * BASE_CHAR_W * 5;
  gfx->setCursor((480 - tw) / 2, cardY + 25);
  gfx->print(fullPrice);

  // Variazione 24h con colore e freccia
  uint16_t chgCol = CR_GRAY;
  bool isUp = false;

  char chgBuf[16];
  if (isnan(cr_chg24)) {
    memcpy_P(chgBuf, PSTR("-- %"), 5);
  } else {
    snprintf_P(chgBuf, sizeof(chgBuf), PSTR("%+.2f%%"), cr_chg24);
    isUp = (cr_chg24 >= 0);
    chgCol = isUp ? CR_GREEN : CR_RED;
  }

  gfx->setTextSize(3);
  gfx->setTextColor(chgCol);

  int16_t chgW = strlen(chgBuf) * BASE_CHAR_W * 3;
  int16_t chgX = (480 - chgW - 28) / 2; // Spazio per freccia

  gfx->setCursor(chgX + 28, cardY + 90);
  gfx->print(chgBuf);

  // Freccia trend
  if (!isnan(cr_chg24)) {
    drawTrendArrow(chgX, cardY + 92, isUp, chgCol);
  }

  // Label "24h"
  gfx->setTextSize(1);
  gfx->setTextColor(CR_GRAY);
  gfx->setCursor(chgX + chgW + 32, cardY + 100);
  gfx->print(F("24h"));

  // === SEZIONE PORTFOLIO (se configurato) ===
  if (!isnan(g_btc_owned) && !isnan(cr_price) && g_btc_owned > 0) {
    const int16_t portY = cardY + cardH + 20;

    // Card portfolio
    gfx->fillRoundRect(PAGE_X, portY, 480 - PAGE_X * 2, 110, 12, CR_CARD);
    gfx->drawRoundRect(PAGE_X, portY, 480 - PAGE_X * 2, 110, 12, CR_BORDER);

    // Icona wallet stilizzata
    gfx->fillRoundRect(PAGE_X + 15, portY + 15, 36, 28, 4, CR_ORANGE);
    gfx->drawFastHLine(PAGE_X + 20, portY + 25, 26, CR_BG_DARK);
    gfx->drawFastHLine(PAGE_X + 20, portY + 31, 26, CR_BG_DARK);

    // Label "Il tuo portfolio"
    gfx->setTextSize(1);
    gfx->setTextColor(CR_GRAY);
    gfx->setCursor(PAGE_X + 60, portY + 12);
    gfx->print(it ? F("IL TUO PORTFOLIO") : F("YOUR PORTFOLIO"));

    // Quantità BTC
    char btcBuf[20];
    crFmtBTC(g_btc_owned, btcBuf, sizeof(btcBuf));

    gfx->setTextSize(2);
    gfx->setTextColor(COL_TEXT);
    gfx->setCursor(PAGE_X + 60, portY + 30);
    gfx->print(btcBuf);
    gfx->print(F(" BTC"));

    // Valore in fiat
    float tot = g_btc_owned * cr_price;
    char totBuf[24];
    crFmtFiat(tot, totBuf, sizeof(totBuf));

    gfx->setTextSize(3);
    gfx->setTextColor(CR_ORANGE);

    char totLine[32];
    snprintf_P(totLine, sizeof(totLine), PSTR("= %s %s"), g_fiat.c_str(),
               totBuf);

    int16_t totW = strlen(totLine) * BASE_CHAR_W * 3;
    gfx->setCursor((480 - totW) / 2, portY + 65);
    gfx->print(totLine);
  }

  // === FOOTER ===
  const int16_t footY = 420;

  // Linea separatore
  gfx->drawFastHLine(PAGE_X, footY, 480 - PAGE_X * 2, CR_BORDER);

  // Pallini decorativi
  gfx->fillCircle(PAGE_X, footY, 3, CR_ORANGE);
  gfx->fillCircle(480 - PAGE_X, footY, 3, CR_ORANGE);

  // Timestamp aggiornamento
  gfx->setTextSize(1);
  gfx->setTextColor(CR_GRAY);

  char footBuf[48];
  if (cr_last_update_ms) {
    uint16_t secs = (millis() - cr_last_update_ms) / 1000;
    if (it) {
      snprintf_P(footBuf, sizeof(footBuf),
                 PSTR("CoinGecko | Aggiornato %ds fa"), secs);
    } else {
      snprintf_P(footBuf, sizeof(footBuf), PSTR("CoinGecko | Updated %ds ago"),
                 secs);
    }
  } else {
    strcpy_P(footBuf, it ? PSTR("CoinGecko | Non disponibile")
                         : PSTR("CoinGecko | Not available"));
  }

  int16_t footW = strlen(footBuf) * 6;
  gfx->setCursor((480 - footW) / 2, footY + 12);
  gfx->print(footBuf);

  // Indicatore live
  static bool blink = false;
  blink = !blink;
  if (cr_last_update_ms && blink) {
    gfx->fillCircle(PAGE_X + 8, footY + 35, 4, CR_GREEN);
  }
  gfx->drawCircle(PAGE_X + 8, footY + 35, 4, CR_GRAY);

  gfx->setCursor(PAGE_X + 18, footY + 32);
  gfx->print(F("LIVE"));

  gfx->setTextSize(TEXT_SCALE);
}

// ---------------------------------------------------------------------------
// Wrapper
// ---------------------------------------------------------------------------
inline bool fetchCryptoWrapper() { return fetchCrypto(); }
inline void pageCryptoWrapper() { pageCrypto(); }
