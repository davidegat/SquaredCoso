#pragma once
#include <Arduino.h>

/*
  ============================================================
  JSON HELPERS – Header-only per SquaredCoso
  ============================================================
  Raccolta di funzioni ultra-leggere per il parsing JSON:
  lavorano direttamente sulle String Arduino, senza allocazioni,
  senza dipendenze esterne e pensate per le risposte HTTP delle API.

  ------------------------------------------------------------
  ELENCO FUNZIONI PRINCIPALI
  ------------------------------------------------------------

  • extractJsonNumber()
      Cerca una chiave generica e legge il numero successivo.
      Utile per JSON semplici o quando la chiave non è nota in anticipo.

  • extractJsonString2()
      Estrae la stringa associata a "key": "value".

  • jsonNum()
      Lettura standard: "key": NUMBER.
      È la funzione consigliata per tutti i moduli.

  • jsonKV()
      Lettura stringa: "key": "value".
      Meno fragile rispetto a estrazioni manuali.

  • jsonArrFirstNumber()
      Legge il primo numero di array: "key":[ NUM, ... ].

  • extractObjectBlock()
      Estrae un intero oggetto annidato { ... } relativo a una chiave.

  • jsonDailyFirstNumber()
      Variante per Open-Meteo: legge il primo valore di array giornalieri.

  • findMatchingBracket()
      Trova la ']' che chiude la '[' data, utile per array più complessi.

  ------------------------------------------------------------
  NOTE
  ------------------------------------------------------------
  - Usare sempre jsonNum(), jsonKV() e jsonArrFirstNumber()
    dove possibile.
  - Tutti i parser assumono JSON ben formati (no protezione completa).
  - Sono progettati per essere veloci e compatti su ESP32-S3.

  ============================================================
*/



// ============================================================
// 1) PRIMITIVE – estrazione numerica generica
// ============================================================
static inline bool extractJsonNumber(const String& body,
                                     const String& key,
                                     float &out,
                                     int from = 0)
{
    int p = body.indexOf(key, from);
    if (p < 0) return false;

    p = body.indexOf(':', p);
    if (p < 0) return false;

    const int len = body.length();
    int s = p + 1;

    while (s < len &&
          (body[s] == ' ' || body[s] == '\"'))
        s++;

    int e = s;

    while (e < len &&
          (isdigit(body[e]) || body[e] == '-' ||
           body[e] == '+'   || body[e] == '.'))
        e++;

    if (e <= s) return false;

    out = body.substring(s, e).toFloat();
    return true;
}



// ============================================================
// 2) PRIMITIVE – estrazione stringa "key": "value"
// ============================================================
static inline bool extractJsonString2(const String& body,
                                      const String& key,
                                      String &out)
{
    int p = body.indexOf(key);
    if (p < 0) return false;

    p = body.indexOf(':', p);
    if (p < 0) return false;

    int s = body.indexOf('\"', p);
    if (s < 0) return false;
    s++;

    int e = body.indexOf('\"', s);
    if (e < 0) return false;

    out = body.substring(s, e);
    out.trim();
    return out.length() > 0;
}



// ============================================================
// 3) jsonNum – "key": NUMBER
// ============================================================
static inline bool jsonNum(const String& src,
                           const char* key,
                           float& out,
                           int from = 0)
{
    String k;
    k.reserve(strlen(key) + 2);
    k = '\"';
    k += key;
    k += '\"';

    int p = src.indexOf(k, from);
    if (p < 0) return false;

    p = src.indexOf(':', p);
    if (p < 0) return false;

    const int N = src.length();
    int s = p + 1;

    while (s < N && (src[s] == ' ' || src[s] == '\"'))
        s++;

    int e = s;

    while (e < N &&
          (isdigit(src[e]) || src[e] == '-' ||
           src[e] == '+'   || src[e] == '.'))
        e++;

    if (e <= s) return false;

    out = src.substring(s, e).toFloat();
    return true;
}



// ============================================================
// 4) jsonKV – "key": "value"
// ============================================================
static inline bool jsonKV(const String& src,
                          const char* key,
                          String& out)
{
    String k;
    k.reserve(strlen(key) + 2);
    k = '\"';
    k += key;
    k += '\"';

    int p = src.indexOf(k);
    if (p < 0) return false;

    p = src.indexOf('\"', p + k.length());
    if (p < 0) return false;

    int q = src.indexOf('\"', p + 1);
    if (q < 0) return false;

    out = src.substring(p + 1, q);
    return true;
}



// ============================================================
// 5) jsonArrFirstNumber – "key":[ NUMBER, ... ]
// ============================================================
static inline bool jsonArrFirstNumber(const String& src,
                                      const char* key,
                                      float& out)
{
    String k;
    k.reserve(strlen(key) + 3);
    k = '\"';
    k += key;
    k += "\":[";

    int p = src.indexOf(k);
    if (p < 0) return false;

    p += k.length();
    const int N = src.length();

    int s = p;

    while (s < N &&
          !(isdigit(src[s]) || src[s] == '-' ||
            src[s] == '+'   || src[s] == '.'))
    {
        if (src[s] == ']' || src[s] == 'n')
            return false;
        s++;
    }
    if (s >= N) return false;

    int e = s;

    while (e < N &&
          (isdigit(src[e]) || src[e] == '-' ||
           src[e] == '+'   || src[e] == '.'))
        e++;

    out = src.substring(s, e).toFloat();
    return true;
}



// ============================================================
// 6) extractObjectBlock – trova oggetto { ... } dopo una chiave
// ============================================================
static inline bool extractObjectBlock(const String& body,
                                      const char* key,
                                      String& out)
{
    const String k = String('\"') + key + '\"';
    const int p = indexOfCI(body, k, 0);
    if (p < 0) return false;

    const int b = body.indexOf('{', p);
    if (b < 0) return false;

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
static inline bool jsonDailyFirstNumber(const String& src,
                                        const char* key,
                                        float& out)
{
    String k;
    k.reserve(strlen(key) + 4);
    k = '\"';
    k += key;
    k += "\":[";

    int p = src.indexOf(k);
    if (p < 0) return false;

    p += k.length();
    const int N = src.length();

    int start = -1;

    for (int i = p; i < N; i++) {
        char c = src[i];
        if ((c >= '0' && c <= '9') || c == '+' || c == '-') {
            start = i;
            break;
        }
        if (c == 'n') return false;
    }
    if (start < 0) return false;

    int end = start + 1;

    while (end < N) {
        char c = src[end];
        if ((c >= '0' && c <= '9') || c == '.' ||
            c == 'e' || c == 'E'   ||
            c == '+' || c == '-') {
            end++;
        } else break;
    }

    out = src.substring(start, end).toFloat();
    return true;
}



// ============================================================
// 8) findMatchingBracket – chiusura di '['
// ============================================================
static inline int findMatchingBracket(const String& src, int start)
{
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

