/*
===============================================================================
   SQUARED — PAGINA "STELLAR" (Terra–Sole + Stagioni)
   Descrizione: Visualizzazione orbitale Terra–Sole con:
                - orbita annuale e posizione attuale della Terra
                - solstizi ed equinozi marcati
                - simulazione luce/ombra sulla Terra
                - stelle pulsanti di sfondo e label auto-disposte
   Autore: Davide “gat” Nasato
   Repository: https://github.com/davidegat/SquaredCoso
   Licenza: CC BY-NC 4.0
===============================================================================
*/

#pragma once

#include "../handlers/globals.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <math.h>
#include <time.h>

extern Arduino_RGB_Display *gfx;
extern const uint16_t COL_TEXT, COL_ACCENT1, COL_ACCENT2, COL_DIVIDER;
extern const int PAGE_X, PAGE_Y, PAGE_W, PAGE_H, TEXT_SCALE;
extern String g_lang;
extern void drawHeader(const String &);

// ---------------------------------------------------------------------------
// Costanti colore
// ---------------------------------------------------------------------------
static const uint16_t BG = 0x0000;
static const uint16_t SUN = 0xFFE0;
static const uint16_t EAR_BLUE = 0x1B70;
static const uint16_t EAR_GREEN = 0x07E0;
static const uint16_t STAR_COL[3] = {0x8410, 0xC618, 0xFFFF};

// Sfumatura sole
static const uint16_t SUN_GLOW[] PROGMEM = {0x4200, 0x6320, 0x8440, 0xC5A0,
                                            0xE6C0};

// ---------------------------------------------------------------------------
// Stelle
// ---------------------------------------------------------------------------
static uint32_t nextPulse = 0;
static const uint8_t NS = 80;
static int16_t sx[NS], sy[NS];
static uint8_t sp[NS];

static void initStars() {
  randomSeed(millis() ^ 0x12345678);
  const int16_t w = gfx->width();
  const int16_t h = gfx->height();

  for (uint8_t i = 0; i < NS; i++) {
    sx[i] = random(w);
    sy[i] = random(PAGE_Y, h);
    sp[i] = random(3);
  }
}

static void drawStars() {
  for (uint8_t i = 0; i < NS; i++) {
    gfx->drawPixel(sx[i], sy[i], STAR_COL[sp[i]]);
  }
}

void tickStellar() {
  const uint32_t t = millis();
  if (t < nextPulse)
    return;
  nextPulse = t + 1000;

  for (uint8_t i = 0; i < NS; i++) {
    sp[i] = (sp[i] + 1) % 3;
  }
  drawStars();
}

// ---------------------------------------------------------------------------
// Utility collisioni label
// ---------------------------------------------------------------------------
static inline bool rectOverlap(int16_t x1, int16_t y1, int16_t w1, int16_t h1,
                               int16_t x2, int16_t y2, int16_t w2, int16_t h2) {
  return !((x1 + w1 < x2) | (x2 + w2 < x1) | (y1 + h1 < y2) | (y2 + h2 < y1));
}

// Usa distanza al quadrato per evitare sqrtf
static void avoidCircle(int16_t &lx, int16_t &ly, int16_t lw, int16_t lh,
                        float cx, float cy, float crSq, float margin) {
  const float lcx = lx + lw * 0.5f;
  const float lcy = ly + lh * 0.5f;
  const float dx = lcx - cx;
  const float dy = lcy - cy;
  const float distSq = dx * dx + dy * dy;

  const float cr = sqrtf(crSq);
  const float minDist = cr + ((lw > lh) ? lw : lh) * 0.5f + margin;
  const float minDistSq = minDist * minDist;

  if (distSq < minDistSq && distSq > 0.01f) {
    const float dist = sqrtf(distSq);
    const float push = (minDist - dist) / dist;
    lx += (int16_t)(dx * push);
    ly += (int16_t)(dy * push);
  }
}

static inline void clampToScreen(int16_t &lx, int16_t &ly, int16_t lw,
                                 int16_t lh, int16_t screenW, int16_t screenH,
                                 int16_t minY) {
  const int16_t M = 4;
  if (lx < M)
    lx = M;
  if (ly < minY + M)
    ly = minY + M;
  if (lx + lw > screenW - M)
    lx = screenW - M - lw;
  if (ly + lh > screenH - M)
    ly = screenH - M - lh;
}

static void pushApart(int16_t &ax, int16_t &ay, int16_t aw, int16_t ah,
                      int16_t &bx, int16_t &by, int16_t bw, int16_t bh) {
  if (!rectOverlap(ax, ay, aw, ah, bx, by, bw, bh))
    return;

  const float acx = ax + aw * 0.5f, acy = ay + ah * 0.5f;
  const float bcx = bx + bw * 0.5f, bcy = by + bh * 0.5f;
  float dx = acx - bcx, dy = acy - bcy;
  float distSq = dx * dx + dy * dy;

  if (distSq < 1.0f) {
    dx = 1.0f;
    dy = 0.0f;
    distSq = 1.0f;
  }

  const float overlapX = (aw + bw) * 0.5f - fabsf(acx - bcx);
  const float overlapY = (ah + bh) * 0.5f - fabsf(acy - bcy);
  const float k =
      ((overlapX > overlapY) ? overlapX : overlapY) * 0.6f / sqrtf(distSq);

  ax += (int16_t)(dx * k);
  ay += (int16_t)(dy * k);
  bx -= (int16_t)(dx * k);
  by -= (int16_t)(dy * k);
}

// Struct per label
struct Label {
  int16_t x, y, w, h;
};

static void resolveLabelConstraints(Label *labels, uint8_t count, float ex,
                                    float ey, float erSq, float sx_c,
                                    float sy_c, float sunRSq, int16_t screenW,
                                    int16_t screenH, int16_t minY) {
  const float EARTH_MARGIN = 8.0f;
  const float SUN_MARGIN = 12.0f;

  // 15 iterazioni
  for (uint8_t iter = 0; iter < 15; iter++) {
    // Evita Terra (skip label 0 = Terra)
    for (uint8_t i = 1; i < count; i++) {
      if (i != 1) // skip label Sole per Terra
        avoidCircle(labels[i].x, labels[i].y, labels[i].w, labels[i].h, ex, ey,
                    erSq, EARTH_MARGIN);
    }

    // Evita Sole (skip label 1 = Sole)
    for (uint8_t i = 0; i < count; i++) {
      if (i != 1)
        avoidCircle(labels[i].x, labels[i].y, labels[i].w, labels[i].h, sx_c,
                    sy_c, sunRSq, SUN_MARGIN);
    }

    // Collisioni tra label
    for (uint8_t i = 0; i < count; i++) {
      for (uint8_t j = i + 1; j < count; j++) {
        pushApart(labels[i].x, labels[i].y, labels[i].w, labels[i].h,
                  labels[j].x, labels[j].y, labels[j].w, labels[j].h);
      }
    }

    // Clamp
    for (uint8_t i = 0; i < count; i++) {
      clampToScreen(labels[i].x, labels[i].y, labels[i].w, labels[i].h, screenW,
                    screenH, minY);
    }
  }
}

// ---------------------------------------------------------------------------
// Sfumatura Sole
// ---------------------------------------------------------------------------
static void drawSunGlow(int16_t cx, int16_t cy, int16_t sunR) {
  for (int8_t level = 0; level < 5; level++) {
    const uint16_t col = pgm_read_word(&SUN_GLOW[level]);
    const int16_t rStart = sunR + 10 - (level << 1);
    const int16_t rEnd = rStart - 2;
    for (int16_t r = rStart; r > rEnd; r--) {
      gfx->drawCircle(cx, cy, r, col);
    }
  }
}

// ---------------------------------------------------------------------------
// Terra
// ---------------------------------------------------------------------------
static void drawEarth(float ex, float ey, int16_t er, float lx, float ly) {
  const int16_t erSq = er * er;

  for (int16_t dy = -er; dy <= er; dy++) {
    const int16_t dy2 = dy * dy;
    for (int16_t dx = -er; dx <= er; dx++) {
      if (dx * dx + dy2 > erSq)
        continue;

      const float nx = dx / (float)er;
      const float ny = dy / (float)er;
      const float dot = nx * lx + ny * ly;

      uint16_t c;
      if (dot > -0.2f) {
        // Lato illuminato
        if (dot > 0.2f && (random(100) < 15)) {
          c = EAR_GREEN;
        } else {
          c = EAR_BLUE;
          if (dot > 0.4f)
            c |= 0x4208;
        }
      } else {
        // Lato in ombra
        c = EAR_BLUE & 0x7BEF;
      }

      gfx->drawPixel((int16_t)ex + dx, (int16_t)ey + dy, c);
    }
  }
}

// ---------------------------------------------------------------------------
// Funzioni helper
// ---------------------------------------------------------------------------
static inline float normAng(float a) {
  while (a < 0)
    a += TWO_PI;
  while (a >= TWO_PI)
    a -= TWO_PI;
  return a;
}

static void orbitPos(int16_t cx, int16_t cy, float r, float f, float ps,
                     int16_t &x, int16_t &y) {
  float p = f + ps;
  p -= floorf(p);
  const float ang = p * TWO_PI;
  x = cx + (int16_t)(r * cosf(ang));
  y = cy + (int16_t)(r * sinf(ang));
}

static void drawTick(int16_t px, int16_t py, int16_t cx, int16_t cy,
                     int16_t len) {
  const float dx = px - cx;
  const float dy = py - cy;
  const float invDist = 1.0f / sqrtf(dx * dx + dy * dy + 0.001f);
  const float nx = dx * invDist;
  const float ny = dy * invDist;
  gfx->drawLine(px - (int16_t)(nx * len), py - (int16_t)(ny * len),
                px + (int16_t)(nx * len), py + (int16_t)(ny * len),
                COL_DIVIDER);
}

static void radialLabelPos(int16_t px, int16_t py, int16_t cx, int16_t cy,
                           int16_t lw, int16_t lh, int16_t &lx, int16_t &ly) {
  const float dx = px - cx;
  const float dy = py - cy;
  const float invLen = 1.0f / sqrtf(dx * dx + dy * dy + 0.001f);
  lx = px + (int16_t)(dx * invLen * 16) - (lw >> 1);
  ly = py + (int16_t)(dy * invLen * 16) - (lh >> 1);
}

// ===========================================================================
// PAGE
// ===========================================================================
void pageStellar() {
  const bool isItalian = (g_lang == "it");
  drawHeader(isItalian ? F("Sistema Solare") : F("Solar system"));

  const int16_t w = gfx->width();
  const int16_t h = gfx->height();

  gfx->fillRect(0, PAGE_Y, w, h - PAGE_Y, BG);

  initStars();
  drawStars();

  const int16_t cx = w >> 1;
  const int16_t cy = PAGE_Y + ((h - PAGE_Y) >> 1) + 10;

  // Orbita
  const float r = ((w < (h - PAGE_Y)) ? w : (h - PAGE_Y)) * 0.40f;

  for (int16_t d = 0; d < 360; d++) {
    const float ang = d * DEG_TO_RAD;
    gfx->drawPixel(cx + (int16_t)(r * cosf(ang)), cy + (int16_t)(r * sinf(ang)),
                   COL_DIVIDER);
  }

  // Costanti stagionali
  static const float D_SPRING = 79.0f, D_SUMMER = 172.0f, D_AUTUMN = 266.0f,
                     D_WINTER = 355.0f;
  static const float ps = -3.0f / 365.25f;

  const float F1 = D_SPRING / 365.25f;
  const float F2 = D_SUMMER / 365.25f;
  const float F3 = D_AUTUMN / 365.25f;
  const float F4 = D_WINTER / 365.25f;

  int16_t xS1, yS1, xS2, yS2, xS3, yS3, xS4, yS4;
  orbitPos(cx, cy, r, F1, ps, xS1, yS1);
  orbitPos(cx, cy, r, F2, ps, xS2, yS2);
  orbitPos(cx, cy, r, F3, ps, xS3, yS3);
  orbitPos(cx, cy, r, F4, ps, xS4, yS4);

  drawTick(xS1, yS1, cx, cy, 6);
  drawTick(xS2, yS2, cx, cy, 6);
  drawTick(xS3, yS3, cx, cy, 6);
  drawTick(xS4, yS4, cx, cy, 6);

  // Sole
  const int16_t scx = cx, scy = cy;
  const int16_t sunR = 26;

  drawSunGlow(scx, scy, sunR);
  gfx->fillCircle(scx, scy, sunR, SUN);
  gfx->drawCircle(scx, scy, sunR + 1, COL_DIVIDER);

  // Angoli stagioni
  const float angSPR = normAng(atan2f((float)(yS4 - cy), (float)(xS4 - cx)));
  const float angSUM = normAng(atan2f((float)(yS1 - cy), (float)(xS1 - cx)));
  const float angAUT = normAng(atan2f((float)(yS2 - cy), (float)(xS2 - cx)));
  const float angWIN = normAng(atan2f((float)(yS3 - cy), (float)(xS3 - cx)));

  // Giorno anno
  time_t now;
  time(&now);
  struct tm lt;
  localtime_r(&now, &lt);

  const int year = lt.tm_year + 1900;
  const bool leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
  const int16_t daysInYear = leap ? 366 : 365;
  const float day = (float)lt.tm_yday;

  // Interpolazione angolo Terra
  const float kd[5] = {D_WINTER - daysInYear, D_SPRING, D_SUMMER, D_AUTUMN,
                       D_WINTER};
  const float ka[5] = {angWIN - TWO_PI, angSPR, angSUM, angAUT, angWIN};

  float angEarth = angWIN;
  for (uint8_t i = 0; i < 4; i++) {
    if (day >= kd[i] && day <= kd[i + 1]) {
      const float t = (day - kd[i]) / (kd[i + 1] - kd[i]);
      angEarth = ka[i] + t * (ka[i + 1] - ka[i]);
      break;
    }
  }
  angEarth = normAng(angEarth);

  // Terra
  const float ex = cx + r * cosf(angEarth);
  const float ey = cy + r * sinf(angEarth);
  const int16_t er = 15;

  float lx = scx - ex, ly = scy - ey;
  const float ll = sqrtf(lx * lx + ly * ly) + 0.001f;
  lx /= ll;
  ly /= ll;

  drawEarth(ex, ey, er, lx, ly);

  // Label
  gfx->setTextSize(1);
  gfx->setTextColor(COL_TEXT, BG);

  // Stringhe (stack locale per efficienza)
  const char *const terra = isItalian ? "Terra" : "Earth";
  const char *const sole = isItalian ? "Sole" : "Sun";
  const char *const S1s = isItalian ? "Sol. Estate" : "Summer Sol.";
  const char *const S2s = isItalian ? "Eq. Autunno" : "Autumn Eq.";
  const char *const S3s = isItalian ? "Sol. Inverno" : "Winter Sol.";
  const char *const S4s = isItalian ? "Eq. Primavera" : "Spring Eq.";

  // Array di label
  Label labels[6];

  // Terra (radiale verso esterno)
  const float earthOutInv =
      1.0f /
      (sqrtf((ex - scx) * (ex - scx) + (ey - scy) * (ey - scy)) + 0.001f);
  labels[0].w = 6 * strlen(terra);
  labels[0].h = 8;
  labels[0].x =
      (int16_t)(ex + (ex - scx) * earthOutInv * (er + 8)) - (labels[0].w >> 1);
  labels[0].y =
      (int16_t)(ey + (ey - scy) * earthOutInv * (er + 8)) - (labels[0].h >> 1);

  // Sole
  labels[1].w = 6 * strlen(sole);
  labels[1].h = 8;
  labels[1].x = scx + sunR + 14;
  labels[1].y = scy - (labels[1].h >> 1);

  // Stagioni
  labels[2].w = 6 * strlen(S1s);
  labels[2].h = 8;
  labels[3].w = 6 * strlen(S2s);
  labels[3].h = 8;
  labels[4].w = 6 * strlen(S3s);
  labels[4].h = 8;
  labels[5].w = 6 * strlen(S4s);
  labels[5].h = 8;

  radialLabelPos(xS1, yS1, cx, cy, labels[2].w, labels[2].h, labels[2].x,
                 labels[2].y);
  radialLabelPos(xS2, yS2, cx, cy, labels[3].w, labels[3].h, labels[3].x,
                 labels[3].y);
  radialLabelPos(xS3, yS3, cx, cy, labels[4].w, labels[4].h, labels[4].x,
                 labels[4].y);
  radialLabelPos(xS4, yS4, cx, cy, labels[5].w, labels[5].h, labels[5].x,
                 labels[5].y);

  // Risolvi collisioni
  const float sunRadiusSq = (sunR + 10) * (sunR + 10);
  const float erSq = (float)(er * er);
  resolveLabelConstraints(labels, 6, ex, ey, erSq, (float)scx, (float)scy,
                          sunRadiusSq, w, h, PAGE_Y);

  // Disegna label
  gfx->setCursor(labels[0].x, labels[0].y);
  gfx->print(terra);
  gfx->setCursor(labels[1].x, labels[1].y);
  gfx->print(sole);
  gfx->setCursor(labels[2].x, labels[2].y);
  gfx->print(S1s);
  gfx->setCursor(labels[3].x, labels[3].y);
  gfx->print(S2s);
  gfx->setCursor(labels[4].x, labels[4].y);
  gfx->print(S3s);
  gfx->setCursor(labels[5].x, labels[5].y);
  gfx->print(S4s);

  gfx->setTextSize(TEXT_SCALE);
}
