#pragma once

// -----------------------------------------------------------------------------
//  SQUARED — QOD (Quote Of The Day)
//  Modulo: gestione frase del giorno (OpenAI → preferita, ZenQuotes → fallback)
//  - Cache giornaliera con controllo fonte (AI / fallback)
//  - Normalizzazione ASCII per compatibilità pannello
//  - Parsing leggero JSON text-only
//  - Rendering centrato a paragrafi
// -----------------------------------------------------------------------------

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "../handlers/globals.h"

// -----------------------------------------------------------------------------
// Risorse condivise dal core SquaredCoso
// -----------------------------------------------------------------------------
extern Arduino_RGB_Display* gfx;

extern const int PAGE_X, PAGE_Y, PAGE_W, PAGE_H;
extern const int CHAR_H, BASE_CHAR_W, BASE_CHAR_H, TEXT_SCALE;

extern const uint16_t COL_BG;
extern const uint16_t COL_ACCENT1;

extern String g_lang;
extern String g_oa_key;
extern String g_oa_topic;

extern bool   httpGET(const String&, String&, uint32_t);
//extern bool   isHttpOk(int);
extern int    indexOfCI(const String&, const String&, int);
extern bool   jsonFindStringKV(const String&, const String&, int, String&);
extern String jsonEscape(const String&);
extern String decodeJsonUnicode(const String&);
extern String sanitizeText(const String&);
extern void   todayYMD(String&);
extern void   drawHeader(const String&);
extern void   drawBoldMain(int16_t, int16_t, const String&, uint8_t);
extern void   drawParagraph(int16_t, int16_t, int16_t, const String&, uint8_t);

extern WebServer web;

// -----------------------------------------------------------------------------
// Cache locale QOD
//  - frase
//  - autore
//  - data cache (YYYYMMDD) → evita richieste ripetute
//  - flag fonte (AI / ZenQuotes)
// -----------------------------------------------------------------------------
static String qod_text;
static String qod_author;
static String qod_date_ymd;
static bool   qod_from_ai = false;

// -----------------------------------------------------------------------------
// Conversione virgolette / apici / trattini → ASCII per compatibilità display
// -----------------------------------------------------------------------------
static String qodSanitizeQuotes(const String& in) {
  String s = in;
  s.replace("“","\""); s.replace("”","\"");
  s.replace("„","\""); s.replace("«","\""); s.replace("»","\"");
  s.replace("‘","'");  s.replace("’","'");
  s.replace("—","-");  s.replace("–","-");
  return s;
}

// -----------------------------------------------------------------------------
// Fallback: ZenQuotes (API semplice, JSON prevedibile)
// -----------------------------------------------------------------------------
static bool fetchQOD_ZenQuotes() {
  String body;
  if (!httpGET("https://zenquotes.io/api/today", body, 10000))
    return false;

  String q, a;
  if (!jsonFindStringKV(body, "q", 0, q))
    return false;
  jsonFindStringKV(body, "a", 0, a);

  qod_text   = qodSanitizeQuotes(sanitizeText(q));
  qod_author = qodSanitizeQuotes(sanitizeText(a));

  if (qod_text.length() > 280)
    qod_text.remove(277);

  return true;
}

// -----------------------------------------------------------------------------
// Fetch via OpenAI Responses API (se configurata)
//  - prompt breve e vincolato
//  - estrazione della prima chiave "text"
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
  http.addHeader("Authorization", String("Bearer ") + g_oa_key);

  String prompt =
    (g_lang == "it")
      ? ("Scrivi una breve frase in stile " + g_oa_topic +
         " in buon italiano e senza errori. Solo la frase, sempre nuova.")
      : ("Write a short quote in the style of " + g_oa_topic +
         " in good English without errors. Only the quote, always new.");

  String body =
    "{\"model\":\"gpt-4.1-nano\","
    "\"max_output_tokens\":50,"
    "\"input\":\"" + jsonEscape(prompt) + "\"}";

  int code = http.POST(body);
  if (!isHttpOk(code)) {
    http.end();
    return false;
  }

  String resp = http.getString();
  http.end();

  int p = indexOfCI(resp, "\"text\"", 0);
  if (p < 0) return false;

  p = resp.indexOf('"', p + 6);
  if (p < 0) return false;

  int q = resp.indexOf('"', p + 1);
  if (q < 0) return false;

  String raw = resp.substring(p + 1, q);

  raw = decodeJsonUnicode(raw);
  raw.replace("\\n"," ");
  raw.replace("\\","");
  raw.trim();

  qod_text = qodSanitizeQuotes(sanitizeText(raw));

  if (qod_text.length() > 280)
    qod_text.remove(277);

  qod_author = "AI Generated";
  return qod_text.length() > 0;
}

// -----------------------------------------------------------------------------
// Logica master QOD
//  - controlla cache del giorno
//  - preferisce AI se disponibile
//  - fallback automatico ZenQuotes
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
      qod_from_ai  = true;
      return true;
    }
  }

  if (fetchQOD_ZenQuotes()) {
    qod_date_ymd = today;
    qod_from_ai  = false;
    return true;
  }

  return false;
}

// -----------------------------------------------------------------------------
// Handler Web: invalida cache e rigenera frase al volo
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
// Rendering pagina QOD
//  - titolo
//  - frase con word-wrap automatico
//  - autore centrato in basso
// -----------------------------------------------------------------------------
static void pageQOD() {

  drawHeader(g_lang == "it" ? "Frase del giorno"
                            : "Quote of the Day");
  int y = PAGE_Y;

  if (!qod_text.length()) {
    drawBoldMain(
      PAGE_X,
      y + CHAR_H,
      g_lang == "it" ? "Nessuna frase disponibile"
                     : "No quote available",
      TEXT_SCALE + 1
    );
    return;
  }

  const uint16_t L = qod_text.length();
  const uint8_t scale = (L < 80 ? 4 : (L < 160 ? 3 : 2));

  String full = "\"" + qod_text + "\"";
  drawParagraph(PAGE_X, y, PAGE_W, full, scale);

  String author =
    qod_author.length()
      ? ("- " + qod_author)
      : (g_lang == "it" ? "- sconosciuto"
                        : "- unknown");

  const uint8_t aScale = (author.length() < 18 ? 3 : 2);

  const int aW = author.length() * BASE_CHAR_W * aScale;
  const int aX = (480 - aW) / 2;
  const int aY =
    PAGE_Y + PAGE_H - (BASE_CHAR_H * aScale) - 8;

  gfx->setTextColor(COL_ACCENT1, COL_BG);
  gfx->setTextSize(aScale);
  gfx->setCursor(aX, aY);
  gfx->print(author);

  gfx->setTextSize(TEXT_SCALE);
}

