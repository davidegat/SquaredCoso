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

static inline bool jsonStringAfterKey(const String &src, const char *key,
                                      String &out, int from = 0) {
  String k = String('"') + key + '"';
  int p = src.indexOf(k, from);
  if (p < 0) return false;
  p = src.indexOf(':', p + k.length());
  if (p < 0) return false;
  int q = p + 1;
  while (q < src.length() && isspace((unsigned char)src[q])) q++;
  if (q >= src.length() || src[q] != '"') return false;
  q++;

  out = "";
  out.reserve(128);
  bool esc = false;
  for (int i = q; i < src.length(); i++) {
    char c = src[i];
    if (esc) {
      switch (c) {
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        case 'u': {
          if (i + 4 >= src.length()) return false;
          char hex[5] = {src[i+1], src[i+2], src[i+3], src[i+4], 0};
          long cp = strtol(hex, nullptr, 16);
          if (cp < 0x80) out += (char)cp;
          else if (cp < 0x800) {
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
          } else {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
          }
          i += 4;
          break;
        }
        default: out += c; break;
      }
      esc = false;
      continue;
    }
    if (c == '\\') { esc = true; continue; }
    if (c == '"') return true;
    out += c;
  }
  return false;
}
