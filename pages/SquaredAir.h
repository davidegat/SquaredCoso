#pragma once

/*
   SquaredCoso – Pagina "AIR QUALITY"
   - Legge qualità aria da Open-Meteo
   - Classifica in 5 categorie con testo IT/EN
   - Cambia colore sfondo globale g_air_bg
   - Mostra valori PM2.5 / PM10 / O3 / NO2
   - Effetto foglie animato solo parte bassa pagina
*/

#include <Arduino.h>
#include "../handlers/globals.h"
#include "../handlers/displayhelpers.h"
#include "../handlers/jsonhelpers.h"

// ---------------------------------------------------------------------------
// EXTERN (dal core di SquaredCoso)
// ---------------------------------------------------------------------------
extern Arduino_RGB_Display* gfx;
extern const uint16_t COL_BG;
extern const uint16_t COL_HEADER;
extern const uint16_t COL_TEXT;
extern const int PAGE_X;
extern const int PAGE_Y;
extern const int BASE_CHAR_W;
extern const int BASE_CHAR_H;
extern const int TEXT_SCALE;
extern const int CHAR_H;
extern void   drawHeader(const String& title);
extern void   drawBoldMain(int16_t x, int16_t y, const String&, uint8_t scale);
extern void   drawHLine(int y);
extern String sanitizeText(const String&);
extern int    indexOfCI(const String&, const String&, int from);
extern bool   httpGET(const String& url, String& out, uint32_t timeout);
extern bool   geocodeIfNeeded();
extern String g_city;
extern String g_lang;
extern uint16_t g_air_bg;

// ---------------------------------------------------------------------------
// SFONDI PER CATEGORIE ARIA (pagina)
// ---------------------------------------------------------------------------
#define AIR_BG_GOOD       0x232E
#define AIR_BG_FAIR       0xA7F0
#define AIR_BG_MODERATE   0xFFE0
#define AIR_BG_POOR       0xFD20
#define AIR_BG_VERYPOOR   0xF800

// ---------------------------------------------------------------------------
// COLORI PER I VALORI NUMERICI
// ---------------------------------------------------------------------------
#define AIR_VAL_GOOD      0x07E0  // verde neon
#define AIR_VAL_FAIR      0xFFE0  // giallo brillante
#define AIR_VAL_POOR      0xF800  // rosso vivo

// ---------------------------------------------------------------------------
// SOGLIE QUALITÀ ARIA (µg/m³) – modificabili via variabile
//  index: 0 = limite "buono", 1 = "discreto", 2 = "moderato", 3 = "scadente"
//  > soglia[3] = "pessimo"
// ---------------------------------------------------------------------------
static float PM25_THRESH[4] = { 10, 20, 25, 50 };
static float PM10_THRESH[4] = { 20, 40, 50, 100 };
static float O3_THRESH[4]   = { 80, 120, 180, 240 };
static float NO2_THRESH[4]  = { 40, 90, 120, 230 };

// ---------------------------------------------------------------------------
// CACHE VALORI ULTIMO FETCH
// ---------------------------------------------------------------------------
static float aq_pm25 = NAN;
static float aq_pm10 = NAN;
static float aq_o3   = NAN;
static float aq_no2  = NAN;



// Estrae il primo valore dell'array numerico "key": [ ... ]
static float parseFirstNumber(const String& obj, const char* key) {
  const String k = String("\"") + key + "\"";
  const int p = indexOfCI(obj, k, 0);
  if (p < 0) return NAN;

  const int a = obj.indexOf('[', p);
  if (a < 0) return NAN;

  int s = a + 1;
  const int len = obj.length();

  while (s < len && isspace(obj.charAt(s))) s++;
  if (s >= len || obj.charAt(s) == '"') return NAN;

  int e = s;
  while (e < len && obj.charAt(e) != ',' && obj.charAt(e) != ']') e++;

  return obj.substring(s, e).toFloat();
}

// ---------------------------------------------------------------------------
// CLASSIFICAZIONE QUALITÀ ARIA (0..4, -1 se NaN)
// ---------------------------------------------------------------------------
static int catFrom(float v, float a, float b, float c, float d) {
  if (isnan(v)) return -1;
  if (v <= a) return 0;
  if (v <= b) return 1;
  if (v <= c) return 2;
  if (v <= d) return 3;
  return 4;
}

static const char cat_it[][20] PROGMEM = {
  "Aria buona",
  "Aria discreta",
  "Aria moderata",
  "Aria scadente",
  "Aria pessima"
};

static const char cat_en[][20] PROGMEM = {
  "Good air",
  "Fair air",
  "Moderate air",
  "Poor air",
  "Very poor air"
};

static const char tip_it[][40] PROGMEM = {
  "tutto ok!",
  "nessuna precauzione.",
  "sensibili: limitare sforzo.",
  "evitare attivita fuori!",
  "rimanere al chiuso!"
};

static const char tip_en[][40] PROGMEM = {
  "all good!",
  "no precautions needed.",
  "sensitive: reduce effort.",
  "avoid long outdoor activity!",
  "stay indoors!"
};

// Ritorna frase riassuntiva e categoria "peggiore" tra i parametri
static String airVerdict(int& categoryOut) {
  int worst = -1;

  worst = max(worst, catFrom(aq_pm25,
                             PM25_THRESH[0], PM25_THRESH[1],
                             PM25_THRESH[2], PM25_THRESH[3]));
  worst = max(worst, catFrom(aq_pm10,
                             PM10_THRESH[0], PM10_THRESH[1],
                             PM10_THRESH[2], PM10_THRESH[3]));
  worst = max(worst, catFrom(aq_o3,
                             O3_THRESH[0], O3_THRESH[1],
                             O3_THRESH[2], O3_THRESH[3]));
  worst = max(worst, catFrom(aq_no2,
                             NO2_THRESH[0], NO2_THRESH[1],
                             NO2_THRESH[2], NO2_THRESH[3]));

  categoryOut = worst;

  if (worst < 0) {
    return (g_lang == "it")
      ? String("Aria: dati non disponibili")
      : String("Air: data unavailable");
  }

  char bufCat[20];
  char bufTip[40];

  if (g_lang == "it") {
    strcpy_P(bufCat, cat_it[worst]);
    strcpy_P(bufTip, tip_it[worst]);
  } else {
    strcpy_P(bufCat, cat_en[worst]);
    strcpy_P(bufTip, tip_en[worst]);
  }

  String s(bufCat);
  s += ", ";
  s += bufTip;
  return s;
}

// ---------------------------------------------------------------------------
// FETCH DATI AIR QUALITY DA OPEN-METEO
// ---------------------------------------------------------------------------
static bool fetchAir() {
  if (!geocodeIfNeeded()) return false;

  String url =
    String("https://air-quality-api.open-meteo.com/v1/air-quality?latitude=")
    + g_lat + "&longitude=" + g_lon
    + "&hourly=pm2_5,pm10,ozone,nitrogen_dioxide&timezone=auto";

  String body;
  if (!httpGET(url, body, 10000)) return false;

  String hourlyBlk;
  if (!extractObjectBlock(body, "hourly", hourlyBlk)) return false;

  aq_pm25 = parseFirstNumber(hourlyBlk, "pm2_5");
  aq_pm10 = parseFirstNumber(hourlyBlk, "pm10");
  aq_o3   = parseFirstNumber(hourlyBlk, "ozone");
  aq_no2  = parseFirstNumber(hourlyBlk, "nitrogen_dioxide");

  return true;
}

// ---------------------------------------------------------------------------
// PARTICLE SYSTEM – FOGLIE ORIZZONTALI NELLA PARTE BASSA
// ---------------------------------------------------------------------------
#define N_LEAVES     100
#define SIN_LUT_SIZE 64

static const int8_t SIN_LUT[SIN_LUT_SIZE] PROGMEM = {
  0,12,25,37,49,60,71,81,90,98,105,111,116,120,123,125,127,
  125,123,120,116,111,105,98,90,81,71,60,49,37,25,12,
  0,-12,-25,-37,-49,-60,-71,-81,-90,-98,-105,-111,-116,-120,-123,-125,
  -127,-125,-123,-120,-116,-111,-105,-98,-90,-81,-71,-60,-49,-37,-25,-12
};

static inline float fastSinPhase(float phaseIdx) {
  int idx = ((int)phaseIdx) & (SIN_LUT_SIZE - 1);
  int8_t v = pgm_read_byte(&SIN_LUT[idx]);
  return (float)v * (1.0f / 127.0f);
}

struct Leaf {
  float    x, y, baseY, phase, vx, amp;
  uint16_t color;
  int16_t  oldX, oldY;
  uint8_t  size;
};

static Leaf     leaves[N_LEAVES];
static bool     leavesInit   = false;
static uint32_t leavesLastMs = 0;
static const uint16_t LEAF_COLORS[2] PROGMEM = { 0xFD80, 0xA440 };

static inline void initLeafFX(int i) {
  Leaf& L = leaves[i];
  L.x     = (float)random(-40, -10);
  L.baseY = (float)random(310, 470);
  L.y     = L.baseY;
  L.vx    = 1.0f + (random(0, 20) * 0.1f);
  L.amp   = (float)(6 + random(0, 10));
  L.phase = (float)random(0, SIN_LUT_SIZE);
  L.size  = random(0, 3);
  L.color = pgm_read_word(&LEAF_COLORS[random(0, 2)]);
  L.oldX  = -1;
  L.oldY  = -1;
}

static inline void drawLeafFX(int x, int y, uint8_t s, uint16_t c) {
  gfx->drawPixel(x, y, c);
  if (s > 0) {
    gfx->drawPixel(x - 1, y, c);
    gfx->drawPixel(x + 1, y, c);
  }
  if (s > 1) {
    gfx->drawPixel(x, y - 1, c);
    gfx->drawPixel(x, y + 1, c);
    gfx->drawPixel(x - 1, y - 1, c);
    gfx->drawPixel(x + 1, y - 1, c);
    gfx->drawPixel(x - 1, y + 1, c);
    gfx->drawPixel(x + 1, y + 1, c);
  }
}

static void tickLeaves(uint16_t bg) {
  uint32_t now = millis();
  if (now - leavesLastMs < 33U) return;
  leavesLastMs = now;

  if (!leavesInit) {
    leavesInit = true;
    for (int i = 0; i < N_LEAVES; i++) initLeafFX(i);
  }

  gfx->startWrite();
  for (int i = 0; i < N_LEAVES; i++) {
    Leaf& L = leaves[i];

    if (L.oldY >= 300)
      drawLeafFX(L.oldX, L.oldY, L.size, bg);

    L.x     += L.vx;
    L.phase += 0.5f;
    if (L.phase >= SIN_LUT_SIZE) L.phase -= SIN_LUT_SIZE;

    float sinv = fastSinPhase(L.phase);
    L.y = L.baseY + sinv * L.amp;

    if (L.x > 500.0f) {
      initLeafFX(i);
      continue;
    }

    int ix = (int)L.x;
    int iy = (int)L.y;

    if (iy >= 300 && iy < 480 && ix >= -2 && ix < 482) {
      drawLeafFX(ix, iy, L.size, L.color);
    }

    L.oldX = ix;
    L.oldY = iy;
  }
  gfx->endWrite();
}

// ---------------------------------------------------------------------------
// STAMPA DI UNA SINGOLA RIGA (PM2.5 / PM10 / O3 / NO2)
// - label a sinistra (COL_TEXT)
// - valore a destra, colorato in base alla categoria
// ---------------------------------------------------------------------------
static void airRowPrint(const char* lbl, float value, int& y, uint16_t bg) {
  const uint8_t SZ = 3;

  // 1) Determina la categoria usando le stesse soglie globali dell'airVerdict
  int cat = -1;

  if (!isnan(value)) {
    // PM2.5
    if (lbl[0] == 'P' && lbl[1] == 'M' && lbl[2] == '2') {
      cat = catFrom(value,
                    PM25_THRESH[0], PM25_THRESH[1],
                    PM25_THRESH[2], PM25_THRESH[3]);
    }
    // PM10
    else if (lbl[0] == 'P' && lbl[1] == 'M' && lbl[2] == '1' && lbl[3] == '0') {
      cat = catFrom(value,
                    PM10_THRESH[0], PM10_THRESH[1],
                    PM10_THRESH[2], PM10_THRESH[3]);
    }
    // OZONO / OZONE
    else if (lbl[0] == 'O') {
      cat = catFrom(value,
                    O3_THRESH[0], O3_THRESH[1],
                    O3_THRESH[2], O3_THRESH[3]);
    }
    // NO2
    else if (lbl[0] == 'N' && lbl[1] == 'O' && lbl[2] == '2') {
      cat = catFrom(value,
                    NO2_THRESH[0], NO2_THRESH[1],
                    NO2_THRESH[2], NO2_THRESH[3]);
    }
  }

  uint16_t valColor = COL_TEXT;
  if (cat == 0 || cat == 1)          valColor = AIR_VAL_GOOD; // buono / discreto
  else if (cat == 2)                 valColor = AIR_VAL_FAIR; // moderato
  else if (cat == 3 || cat == 4)     valColor = AIR_VAL_POOR; // scadente / pessimo

  // 2) Label a sinistra
  gfx->setTextSize(SZ);
  gfx->setTextColor(COL_TEXT, bg);
  gfx->setCursor(PAGE_X, y);
  gfx->print(lbl);

  // 3) Valore numerico + unità
  char buf[16];
  if (isnan(value)) {
    strcpy(buf, "--");
  } else {
    bool pm25 = (lbl[0] == 'P' && lbl[1] == 'M' && lbl[2] == '2');
    dtostrf(value, 0, pm25 ? 1 : 0, buf);
  }

  const char* unit = (g_lang == "it")
    ? " ug/m3"
    : " \xC2\xB5""g/m\xC2\xB3";

  char out[24];
  snprintf(out, sizeof(out), "%s%s", buf, unit);

  int textW = strlen(out) * BASE_CHAR_W * SZ;
  int xr = 480 - PAGE_X - textW;
  if (xr < PAGE_X + 120 && textW < (480 - PAGE_X * 2))
    xr = PAGE_X + 120;

  gfx->setCursor(xr, y);
  gfx->setTextColor(valColor, bg);
  gfx->print(out);

  y += BASE_CHAR_H * SZ + 10;
}

// ---------------------------------------------------------------------------
// ENTRY POINT PAGINA AIR QUALITY
// ---------------------------------------------------------------------------
static void pageAir() {
  const bool it = (g_lang == "it");

  drawHeader(
    it ? String("Aria a ") + sanitizeText(g_city)
       : String("Air quality – ") + sanitizeText(g_city)
  );

  int cat = -1;
  String verdict = airVerdict(cat);

  // Selezione colore di sfondo in base alla categoria peggiore
  uint16_t bg = COL_BG;
  switch (cat) {
    case 0: bg = AIR_BG_GOOD;      break;
    case 1: bg = AIR_BG_FAIR;      break;
    case 2: bg = AIR_BG_MODERATE;  break;
    case 3: bg = AIR_BG_POOR;      break;
    case 4: bg = AIR_BG_VERYPOOR;  break;
  }

  g_air_bg = bg;

  // Riempie area contenuto (sotto l'header)
  gfx->fillRect(0, PAGE_Y, 480, 480 - PAGE_Y, bg);

  int y = PAGE_Y + 14;

  // Righe parametri
  airRowPrint("PM2.5:", aq_pm25, y, bg);
  airRowPrint("PM10 :", aq_pm10, y, bg);
  airRowPrint(it ? "OZONO:" : "OZONE", aq_o3, y, bg);
  airRowPrint("NO2  :", aq_no2, y, bg);

  // Separatore verso il testo riassuntivo
  drawHLine(y);
  y += 14;

  // Testo riassuntivo centrale (categoria + consiglio)
  gfx->setTextSize(TEXT_SCALE + 1);
  gfx->setTextColor(COL_TEXT, bg);

  int maxW = 480 - PAGE_X * 2;
  int maxC = maxW / (BASE_CHAR_W * (TEXT_SCALE + 1));
  if (maxC < 10) maxC = 10;

  String s = sanitizeText(verdict);
  s.trim();

  int start = 0;
  int slen  = s.length();
  y += 12;

  while (start < slen) {
    int len = slen - start;
    if (len > maxC) len = maxC;

    int cut = start + len;
    if (cut < slen) {
      int sp = s.lastIndexOf(' ', cut);
      if (sp > start) cut = sp;
    }

    String line = s.substring(start, cut);
    line.trim();

    int lineLen = line.length();
    if (lineLen > 0) {
      int w = lineLen * BASE_CHAR_W * (TEXT_SCALE + 1);
      int x = (480 - w) / 2;
      if (x < PAGE_X) x = PAGE_X;

      gfx->setCursor(x, y);
      gfx->print(line);
      y += BASE_CHAR_H * (TEXT_SCALE + 1) + 6;
    }

    if (cut >= slen) break;
    start = (s.charAt(cut) == ' ') ? (cut + 1) : cut;
    if (y > 420) break;
  }

  gfx->setTextSize(TEXT_SCALE);

  // Effetto foglie nella parte bassa
  tickLeaves(bg);
}

