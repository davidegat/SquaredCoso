#pragma once

#include "../handlers/globals.h"
#include "../handlers/tls_roots.h"
#include "../images/qod.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

extern Arduino_RGB_Display *gfx;

extern const int PAGE_X, PAGE_Y, PAGE_W, PAGE_H;
extern const int CHAR_H, BASE_CHAR_W, BASE_CHAR_H, TEXT_SCALE;

extern String g_lang;
extern String g_oa_key;
extern String g_oa_topic;

extern bool httpGET(const String &, String &, uint32_t);
extern int indexOfCI(const String &, const String &, int);

extern bool jsonKV(const String &, const char *, String &);
extern bool jsonStringAfterKey(const String &, const char *, String &, int);

extern String jsonEscape(const String &);
extern String decodeJsonUnicode(const String &);
extern String sanitizeText(const String &);
extern void todayYMD(String &);
extern void drawHeader(const String &);
extern void drawBoldMain(int16_t, int16_t, const String &, uint8_t);
extern void drawParagraph(int16_t, int16_t, int16_t, const String &, uint8_t);
extern void drawCurrentPage();

extern WebServer web;

static String qod_text;
static String qod_author;
static String qod_date_ymd;
static bool qod_from_ai = false;

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

static bool fetchQOD_OpenAI() {

  if (!g_oa_key.length() || !g_oa_topic.length())
    return false;

  WiFiClientSecure client;
  client.setCACert(OPENAI_ROOT_CA);

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

  // Parse the output_text item with escape-aware JSON string extraction.
  int p = indexOfCI(resp, "\"output_text\"", 0);
  if (p < 0) p = 0;

  String raw;
  if (!jsonStringAfterKey(resp, "text", raw, p)) return false;
  raw.replace("\n", " ");
  raw.replace("\r", " ");
  raw.trim();

  qod_text = qodSanitizeQuotes(sanitizeText(raw));

  if (qod_text.length() > 280)
    qod_text.remove(277);

  qod_author = "AI Generated";
  return qod_text.length() > 0;
}

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

static void handleForceQOD() {

  qod_text.clear();
  qod_author.clear();
  qod_date_ymd.clear();
  qod_from_ai = false;

  bool ok = fetchQOD();
  if (ok && g_page == P_QOD) drawCurrentPage();

  const bool it = (g_lang == "it");
  web.send(ok ? 200 : 503, "text/plain; charset=utf-8",
           ok ? (it ? "Nuova frase generata" : "New quote generated")
              : (it ? "Generazione frase non riuscita" : "Quote generation failed"));
}

static void pageQOD() {

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

  drawParagraph(PAGE_X, y, PAGE_W, full, scale, 0x0000, 0x0000);

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
