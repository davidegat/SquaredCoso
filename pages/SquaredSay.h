/*
===============================================================================
   SQUARED — PAGINA "QOD" (Quote of the Day)
   Descrizione: Generatore frase del giorno con doppia sorgente
                (OpenAI o ZenQuotes), cache giornaliera, sanitizzazione
                UTF-8, normalizzazione Unicode, word-wrap avanzato e
                rendering su sfondo RLE personalizzato.
   Autore: Davide “gat” Nasato
   Repository: https://github.com/davidegat/SquaredCoso
   Licenza: CC BY-NC 4.0
===============================================================================
*/

#pragma once

#include "../handlers/globals.h"
#include "../images/qod.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// -----------------------------------------------------------------------------
// Risorse core
// -----------------------------------------------------------------------------
extern Arduino_RGB_Display *gfx;

extern const int PAGE_X, PAGE_Y, PAGE_W, PAGE_H;
extern const int CHAR_H, BASE_CHAR_W, BASE_CHAR_H, TEXT_SCALE;

extern String g_lang;
extern String g_oa_key;
extern String g_oa_topic;

extern bool httpGET(const String &, String &, uint32_t);
extern int indexOfCI(const String &, const String &, int);

extern bool jsonKV(const String &, const char *, String &);

extern String jsonEscape(const String &);
extern String decodeJsonUnicode(const String &);
extern String sanitizeText(const String &);
extern void todayYMD(String &);
extern void drawHeader(const String &);
extern void drawBoldMain(int16_t, int16_t, const String &, uint8_t);
extern void drawParagraph(int16_t, int16_t, int16_t, const String &, uint8_t);
extern void drawCurrentPage();

extern WebServer web;

// -----------------------------------------------------------------------------
// Cache QOD
// -----------------------------------------------------------------------------
static String qod_text;
static String qod_author;
static String qod_date_ymd;
static bool qod_from_ai = false;

// -----------------------------------------------------------------------------
// Sanitize virgolette + normalizzazione Unicode
// -----------------------------------------------------------------------------
static String qodSanitizeQuotes(const String &in) {
  String s = in;

  s.replace("“", "\"");
  s.replace("”", "\"");
  s.replace("„", "\"");
  s.replace("«", "\"");
  s.replace("»", "\"");
  s.replace("‘", "'");
  s.replace("’", "'");
  s.replace("—", "-");
  s.replace("–", "-");

  s.replace("à", "a");
  s.replace("á", "a");
  s.replace("è", "e");
  s.replace("é", "e");
  s.replace("ê", "e");
  s.replace("ì", "i");
  s.replace("ò", "o");
  s.replace("ó", "o");
  s.replace("ù", "u");
  s.replace("ú", "u");

  return s;
}

// -----------------------------------------------------------------------------
// Fallback ZenQuotes
// -----------------------------------------------------------------------------
static bool fetchQOD_ZenQuotes() {
  String body;
  if (!httpGET("https://zenquotes.io/api/today", body, 10000))
    return false;

  String q, a;

  if (!jsonKV(body, "q", q))
    return false;

  jsonKV(body, "a", a);

  qod_text = qodSanitizeQuotes(sanitizeText(q));
  qod_author = qodSanitizeQuotes(sanitizeText(a));

  if (qod_text.length() > 280)
    qod_text.remove(277);

  return true;
}

// -----------------------------------------------------------------------------
// OpenAI
// -----------------------------------------------------------------------------
static bool fetchQOD_OpenAI() {

  if (!g_oa_key.length() || !g_oa_topic.length())
    return false;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(client, "https://api.openai.com/v1/responses"))
    return false;

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + g_oa_key);

  String prompt = (g_lang == "it")
                      ? ("Scrivi una frase breve, originale e logicamente "
                         "coerente nello stile di \"" +
                         g_oa_topic +
                         "\". Mantieni il tono caratteristico ma evita "
                         "assurdità o parole fuori contesto. Solo la frase.")
                      : ("Write a short, original and coherent sentence in the "
                         "style of \"" +
                         g_oa_topic +
                         "\". Keep the tone but avoid absurdity or noise. Only "
                         "the sentence.");

  String body = "{\"model\":\"gpt-4.1-nano\","
                "\"max_output_tokens\":50,"
                "\"input\":\"" +
                jsonEscape(prompt) + "\"}";

  int code = http.POST(body);
  if (!(code >= 200 && code < 300)) {
    http.end();
    return false;
  }

  String resp = http.getString();
  http.end();

  int p = indexOfCI(resp, "\"text\"", 0);
  if (p < 0)
    return false;

  p = resp.indexOf('"', p + 6);
  if (p < 0)
    return false;

  int q = resp.indexOf('"', p + 1);
  if (q < 0)
    return false;

  String raw = resp.substring(p + 1, q);

  raw = decodeJsonUnicode(raw);
  raw.replace("\\n", " ");
  raw.replace("\\", "");
  raw.trim();

  qod_text = qodSanitizeQuotes(sanitizeText(raw));

  if (qod_text.length() > 280)
    qod_text.remove(277);

  qod_author = "AI Generated";
  return qod_text.length() > 0;
}

// -----------------------------------------------------------------------------
// Master fetch
// -----------------------------------------------------------------------------
static bool fetchQOD() {

  String today;
  todayYMD(today);

  const bool wantAI = (g_oa_key.length() && g_oa_topic.length());

  if (qod_text.length() && qod_date_ymd == today) {
    if ((wantAI && qod_from_ai) || (!wantAI && !qod_from_ai))
      return true;
  }

  qod_text.clear();
  qod_author.clear();

  if (wantAI) {
    if (fetchQOD_OpenAI()) {
      qod_date_ymd = today;
      qod_from_ai = true;
      return true;
    }
  }

  if (fetchQOD_ZenQuotes()) {
    qod_date_ymd = today;
    qod_from_ai = false;
    return true;
  }

  return false;
}

// -----------------------------------------------------------------------------
// WebUI: forza rigenerazione
// -----------------------------------------------------------------------------
static void handleForceQOD() {

  qod_text.clear();
  qod_author.clear();
  qod_date_ymd.clear();
  qod_from_ai = false;

  fetchQOD();
  drawCurrentPage();

  web.send(200, "text/html; charset=utf-8",
           "<!doctype html><meta charset='utf-8'><body>"
           "<h3>Frase rigenerata</h3>"
           "<p><a href='/settings'>Back</a></p>"
           "</body>");
}

// -----------------------------------------------------------------------------
// Rendering pagina
// -----------------------------------------------------------------------------
static void pageQOD() {

  // SFONDO RLE
  drawRLE(0, 0, QOD_IMG_WIDTH, QOD_IMG_HEIGHT, qod_img, qod_img_count);

  drawHeader(g_lang == "it" ? "Frase del giorno" : "Quote of the Day");

  int y = PAGE_Y;

  if (!qod_text.length()) {
    drawBoldMain(PAGE_X, y + CHAR_H,
                 g_lang == "it" ? "Nessuna frase disponibile"
                                : "No quote available",
                 TEXT_SCALE + 1);
    return;
  }

  gfx->setTextColor(0x0000);
  String full = "\"" + qod_text + "\"";

  const uint16_t L = qod_text.length();
  const uint8_t scale = (L < 80 ? 4 : (L < 160 ? 3 : 2));

  drawParagraph(PAGE_X, y, PAGE_W, full, scale);

  String author = qod_author.length()
                      ? ("- " + qod_author)
                      : (g_lang == "it" ? "- sconosciuto" : "- unknown");

  const uint8_t aScale = (author.length() < 18 ? 3 : 2);
  const int padding = 20;
  const int aW = author.length() * BASE_CHAR_W * aScale;
  const int aX = 480 - aW - padding;
  const int aY = PAGE_Y + PAGE_H - (BASE_CHAR_H * aScale) - 8;

  gfx->setTextColor(0x0000);
  gfx->setTextSize(aScale);
  gfx->setCursor(aX, aY);
  gfx->print(author);
  gfx->setTextSize(TEXT_SCALE);
}
