/*
===============================================================================
   SQUARED — JSON HELPERS (Header-only)
   Parser JSON ultra-leggero per ESP32-S3 basato su String Arduino.
   Nessuna allocazione dinamica, nessuna dipendenza esterna, funzioni pensate
   per risposte compatte dalle API (Open-Meteo, ZenQuotes, Home Assistant, BTC,
   FX, RSS, ICS, ecc.).
   Repository: https://github.com/davidegat/SquaredCoso
   Licenza: CC BY-NC 4.0
===============================================================================

   ------------------------------------------------------------
   ELENCO FUNZIONI E UTILIZZO
   ------------------------------------------------------------

   • extractJsonNumber(body, key, out [,from])
       Cerca "key" nel JSON e legge il numero subito dopo il ':'.
       Utile per JSON molto semplici o cerchi generici.

   • extractJsonString2(body, key, out)
       Estrae la stringa associata a "key": "value" con parsing lineare.
       Funziona anche con leggeri formati non standard.

   • jsonNum(src, "key", out [,from])
       Metodo principale raccomandato per leggere "key": NUMBER.
       Robusto, veloce, usa ricerca con "key" racchiusa tra doppi apici.

   • jsonKV(src, "key", out)
       Estrazione sicura di "key": "value".
       Usato ovunque serva solo testo (RSS, QOD, Home Assistant, ecc.).

   • jsonArrFirstNumber(src, "key", out)
       Raccoglie il primo numero dopo "key":[…].
       Fondamentale per API tipo Open-Meteo o forecast a grana fine.

   • extractObjectBlock(body, "key", out)
       Trova "key":{ ... } e restituisce l’intero oggetto annidato.
       Usato per analisi più strutturate senza JSON parser pesanti.

   • jsonDailyFirstNumber(src, "key", out)
       Variante per Open-Meteo: gestisce array giornalieri più complessi
       (temperature, UV, AQI, precipitazioni).

   • findMatchingBracket(src, pos)
       Trova la ']' corrispondente alla '[' iniziale.
       Essenziale per array lunghi o contenenti più blocchi
       (anch’essa usata nella pagina Temperature 24h).

   ------------------------------------------------------------
   NOTE IMPORTANTI
   ------------------------------------------------------------

   • Le funzioni NON sono un parser JSON formale, ma una soluzione ultra-veloce
     adatta a dispositivi embedded (ESP32-S3 con display RGB DMA).

   • Tutti i parser assumono JSON ben formati e non protetti contro contenuti
     volutamente corrotti (perfettamente adeguato per API standard).

   • Usare SEMPRE:
       - jsonNum()      per numeri
       - jsonKV()       per stringhe
       - jsonArrFirstNumber() per il primo valore in array
       - jsonDailyFirstNumber() per dati giornalieri Open-Meteo

   • Tutto è header-only: nessun .cpp, nessun overhead, inline ovunque.

===============================================================================
*/

#pragma once

#include <Arduino.h>

static inline bool extractJsonNumber(const String &body, const String &key,
                                     float &out, int from = 0) {
  int p = body.indexOf(key, from);
  if (p < 0)
    return false;

  p = body.indexOf(':', p);
  if (p < 0)
    return false;

  const int len = body.length();
  int s = p + 1;

  while (s < len && (body[s] == ' ' || body[s] == '\"'))
    s++;

  int e = s;

  while (e < len && (isdigit(body[e]) || body[e] == '-' || body[e] == '+' ||
                     body[e] == '.'))
    e++;

  if (e <= s)
    return false;

  out = body.substring(s, e).toFloat();
  return true;
}

// ============================================================
// 2) PRIMITIVE – estrazione stringa "key": "value"
// ============================================================
static inline bool extractJsonString2(const String &body, const String &key,
                                      String &out) {
  int p = body.indexOf(key);
  if (p < 0)
    return false;

  p = body.indexOf(':', p);
  if (p < 0)
    return false;

  int s = body.indexOf('\"', p);
  if (s < 0)
    return false;
  s++;

  int e = body.indexOf('\"', s);
  if (e < 0)
    return false;

  out = body.substring(s, e);
  out.trim();
  return out.length() > 0;
}

// ============================================================
// 3) jsonNum – "key": NUMBER
// ============================================================
static inline bool jsonNum(const String &src, const char *key, float &out,
                           int from = 0) {
  String k;
  k.reserve(strlen(key) + 2);
  k = '\"';
  k += key;
  k += '\"';

  int p = src.indexOf(k, from);
  if (p < 0)
    return false;

  p = src.indexOf(':', p);
  if (p < 0)
    return false;

  const int N = src.length();
  int s = p + 1;

  while (s < N && (src[s] == ' ' || src[s] == '\"'))
    s++;

  int e = s;

  while (e < N &&
         (isdigit(src[e]) || src[e] == '-' || src[e] == '+' || src[e] == '.'))
    e++;

  if (e <= s)
    return false;

  out = src.substring(s, e).toFloat();
  return true;
}

// ============================================================
// 4) jsonKV – "key": "value"
// ============================================================
static inline bool jsonKV(const String &src, const char *key, String &out) {
  String k;
  k.reserve(strlen(key) + 2);
  k = '\"';
  k += key;
  k += '\"';

  int p = src.indexOf(k);
  if (p < 0)
    return false;

  p = src.indexOf('\"', p + k.length());
  if (p < 0)
    return false;

  int q = src.indexOf('\"', p + 1);
  if (q < 0)
    return false;

  out = src.substring(p + 1, q);
  return true;
}

// ============================================================
// 5) jsonArrFirstNumber – "key":[ NUMBER, ... ]
// ============================================================
static inline bool jsonArrFirstNumber(const String &src, const char *key,
                                      float &out) {
  String k;
  k.reserve(strlen(key) + 3);
  k = '\"';
  k += key;
  k += "\":[";

  int p = src.indexOf(k);
  if (p < 0)
    return false;

  p += k.length();
  const int N = src.length();

  int s = p;

  while (s < N && !(isdigit(src[s]) || src[s] == '-' || src[s] == '+' ||
                    src[s] == '.')) {
    if (src[s] == ']' || src[s] == 'n')
      return false;
    s++;
  }
  if (s >= N)
    return false;

  int e = s;

  while (e < N &&
         (isdigit(src[e]) || src[e] == '-' || src[e] == '+' || src[e] == '.'))
    e++;

  out = src.substring(s, e).toFloat();
  return true;
}

// ============================================================
// 6) extractObjectBlock – trova oggetto { ... } dopo una chiave
// ============================================================
static inline bool extractObjectBlock(const String &body, const char *key,
                                      String &out) {
  const String k = String('\"') + key + '\"';
  const int p = indexOfCI(body, k, 0);
  if (p < 0)
    return false;

  const int b = body.indexOf('{', p);
  if (b < 0)
    return false;

  int depth = 0;
  const int len = body.length();

  for (int i = b; i < len; i++) {
    char c = body.charAt(i);

    if (c == '{')
      depth++;
    else if (c == '}') {
      depth--;
      if (depth == 0) {
        out = body.substring(b, i + 1);
        return true;
      }
    }
  }
  return false;
}

// ============================================================
// 7) jsonDailyFirstNumber – variante per Open-Meteo
// ============================================================
static inline bool jsonDailyFirstNumber(const String &src, const char *key,
                                        float &out) {
  String k;
  k.reserve(strlen(key) + 4);
  k = '\"';
  k += key;
  k += "\":[";

  int p = src.indexOf(k);
  if (p < 0)
    return false;

  p += k.length();
  const int N = src.length();

  int start = -1;

  for (int i = p; i < N; i++) {
    char c = src[i];
    if ((c >= '0' && c <= '9') || c == '+' || c == '-') {
      start = i;
      break;
    }
    if (c == 'n')
      return false;
  }
  if (start < 0)
    return false;

  int end = start + 1;

  while (end < N) {
    char c = src[end];
    if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' ||
        c == '+' || c == '-') {
      end++;
    } else
      break;
  }

  out = src.substring(start, end).toFloat();
  return true;
}

// ============================================================
// 8) findMatchingBracket – chiusura di '['
// ============================================================
static inline int findMatchingBracket(const String &src, int start) {
  const int N = src.length();
  if (start < 0 || start >= N || src[start] != '[')
    return -1;

  int depth = 0;

  for (int i = start; i < N; i++) {
    char c = src[i];

    if (c == '[')
      depth++;
    else if (c == ']') {
      depth--;
      if (depth == 0)
        return i;
    }
  }

  return -1;
}
