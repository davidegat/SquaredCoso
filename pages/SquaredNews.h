#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "../handlers/globals.h"

// ---------------------------------------------------------------------------
// SquaredCoso – Pagina NEWS (RSS, prime 5 notizie)
// - fetchNews()  → scarica RSS, estrae i primi titoli validi (max 5)
// - pageNews()   → mostra i titoli con word-wrap compatto, IT/EN
// ---------------------------------------------------------------------------

extern Arduino_RGB_Display* gfx;
extern const uint16_t COL_BG, COL_TEXT, COL_ACCENT1;
extern const int PAGE_X, PAGE_Y, CHAR_H, BASE_CHAR_W, BASE_CHAR_H, TEXT_SCALE;

extern String g_lang;
extern String g_rss_url;

extern bool   httpGET(const String&, String&, uint32_t);
extern String sanitizeText(const String&);
extern void   drawHeader(const String&);
extern void   drawHLine(int y);

// ---------------------------------------------------------------------------
// Buffer titoli (solo testo pulito, niente descrizioni HTML)
// ---------------------------------------------------------------------------
static const uint8_t NEWS_MAX = 5;
static String news_title[NEWS_MAX];

// ---------------------------------------------------------------------------
// Rimozione tag HTML/XML da un frammento (mantiene solo testo)
// ---------------------------------------------------------------------------
static inline String stripTags(const String& s) {
  const uint16_t L = s.length();
  String out;
  out.reserve(L);

  bool inTag = false;

  for (uint16_t i = 0; i < L; i++) {
    char c = s[i];

    if (c == '<') { inTag = true;  continue; }
    if (c == '>') { inTag = false; continue; }

    if (!inTag) out.concat(c);  // concat = molto più veloce di +=
  }
  return out;
}

// ---------------------------------------------------------------------------
// Decodifica entità HTML più comuni in-place
// ---------------------------------------------------------------------------
static inline void decodeEntities(String& s) {
  s.replace("&amp;", "&");
  s.replace("&lt;", "<");
  s.replace("&gt;", ">");
  s.replace("&quot;", "\"");
  s.replace("&apos;", "'");
  s.replace("&#39;", "'");
}

// ---------------------------------------------------------------------------
// Normalizza il titolo: CDATA → testo, rimozione tag, entità e sanitize
// ---------------------------------------------------------------------------
static String cleanTitle(const String& raw) {
  String s = raw;

  if (s.startsWith("<![CDATA[")) {
    s.remove(0, 9);
    int p = s.indexOf("]]>");
    if (p > 0) s = s.substring(0, p);
  }

  s = stripTags(s);
  decodeEntities(s);
  s = sanitizeText(s);
  s.trim();

  return s;
}

// ---------------------------------------------------------------------------
// Estrae contenuto <tag>...</tag> se presente
// ---------------------------------------------------------------------------
static inline bool getTag(const String& src, const char* tag, String& out) {
  String open  = "<";  open  += tag; open  += ">";
  String close = "</"; close += tag; close += ">";

  int p = src.indexOf(open);
  if (p < 0) return false;

  int s = p + open.length();
  int e = src.indexOf(close, s);
  if (e < 0) return false;

  out = src.substring(s, e);
  out.trim();
  return true;
}

// ---------------------------------------------------------------------------
// fetchNews: scarica RSS, popola news_title[] con i primi titoli puliti
// ---------------------------------------------------------------------------
bool fetchNews() {

  // Reset buffer finale (5 titoli da mostrare)
  for (uint8_t i = 0; i < NEWS_MAX; i++)
    news_title[i] = "";

  // Buffer temporaneo (10 titoli non ancora randomizzati)
  String raw_title[10];
  uint8_t found = 0;

  // URL effettivo
  const String url = g_rss_url.length()
                       ? g_rss_url
                       : "https://feeds.bbci.co.uk/news/rss.xml";

  // Scarichiamo il feed (ma lavoriamo solo sulle prime parti)
  String body;
  if (!httpGET(url, body, 8000)) return false;
  if (!body.length()) return false;

  // Parsing leggero: cerchiamo solo <item> ... </item> e dentro <title>
  uint16_t pos = 0;

  while (found < 10) {

    int a = body.indexOf("<item", pos);
    if (a < 0) break;

    int openEnd = body.indexOf('>', a);
    if (openEnd < 0) break;

    int b = body.indexOf("</item>", openEnd);
    if (b < 0) break;

    // Contenuto interno del blocco <item>
    String blk = body.substring(openEnd + 1, b);
    pos = b + 7;

    // Cerchiamo solo <title>...</title>
    String title;
    if (!getTag(blk, "title", title)) continue;

    title = cleanTitle(title);
    if (!title.length()) continue;

    raw_title[found++] = title;
  }

  if (found == 0) return false;

  // ---------------------------------------------------
  // RANDOM PICK: 5 titoli scelti dai 10 disponibili
  // (o meno, se il feed ne aveva meno)
  // ---------------------------------------------------
  uint8_t pickCount = min((uint8_t)NEWS_MAX, found);

  // Fisher–Yates shuffle sui primi `found` titoli
  for (uint8_t i = found - 1; i > 0; i--) {
    uint8_t j = random(0, i + 1);
    String tmp = raw_title[i];
    raw_title[i] = raw_title[j];
    raw_title[j] = tmp;
  }

  // Copiamo i primi 5 risultato randomizzato
  for (uint8_t i = 0; i < pickCount; i++)
    news_title[i] = raw_title[i];

  return true;
}


// ---------------------------------------------------------------------------
// pageNews: header + elenco titoli con word-wrap e separatori
// ---------------------------------------------------------------------------
void pageNews() {

  const bool it = (g_lang == "it");
  drawHeader("News");

  const uint8_t SZ = TEXT_SCALE;
  gfx->setTextSize(SZ);

  const int leftPad  = PAGE_X + 10;
  const int rightPad = 470;

  const int charW    = BASE_CHAR_W * SZ;
  const int maxChars = (rightPad - leftPad) / charW;

  int y = PAGE_Y + 8;
  const int lineH = (CHAR_H * SZ) + 6;

  if (!news_title[0].length()) {
    gfx->setCursor(leftPad, y + lineH);
    gfx->setTextColor(COL_ACCENT1, COL_BG);
    gfx->print(it ? "Nessuna notizia" : "No news available");
    return;
  }

  for (uint8_t i = 0; i < NEWS_MAX; i++) {

    if (!news_title[i].length()) continue;

    const String& text = news_title[i];
    const int len = text.length();
    int start = 0;

    while (start < len) {

      int remaining = len - start;
      int take      = min(remaining, maxChars);
      int cut       = start + take;

      if (cut < len) {
        int lastSpace = text.lastIndexOf(' ', cut);
        if (lastSpace > start) cut = lastSpace;
      }

      String line = text.substring(start, cut);
      line.trim();

      gfx->setCursor(leftPad, y);
      gfx->setTextColor(COL_TEXT, COL_BG);
      gfx->print(line);

      y += lineH;
      if (y > 440) break;

      start = (cut < len && text[cut] == ' ') ? (cut + 1) : cut;
    }

    if (i < NEWS_MAX - 1 && y < 430) {
      drawHLine(y - 3);
      y += 4;
    }

    if (y > 440) break;
  }

  gfx->setTextSize(TEXT_SCALE);
}

