#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <time.h>
#include "../handlers/globals.h"

// ============================================================================
// SQUAREDCHRONOS — Tempo umano / terrestre / cosmico
// Layout completo + testi + barre + timeline
// ============================================================================

// ---------------------------------------------------------------------------
// Override locale: sfondo più scuro (0x0004 invece del COL_BG globale)
// ---------------------------------------------------------------------------
static const uint16_t CHRONOS_BG = 0x0004;

// ---------------------------------------------------------------------------
// Esterni globali da SquaredCoso
// ---------------------------------------------------------------------------
extern Arduino_RGB_Display* gfx;

extern const uint16_t COL_TEXT;
extern const uint16_t COL_ACCENT1;
extern const uint16_t COL_ACCENT2;
extern const uint16_t COL_DIVIDER;

extern const int PAGE_X;
extern const int PAGE_Y;
extern const int PAGE_W;
extern const int PAGE_H;
extern const int TEXT_SCALE;

extern void drawHeader(const String&);

// ============================================================================
// Costanti cosmiche (variazione trascurabile su scale umane)
// ============================================================================
static constexpr float EARTH_AGE_GA = 4.54f;     // miliardi di anni
static constexpr float EARTH_END_GA = 10.0f;     // durata stimata abitabilità

static constexpr float SUN_AGE_GA   = 4.6f;      // età attuale del Sole
static constexpr float SUN_LIFE_GA  = 10.0f;     // vita totale prima di gigante rossa

static constexpr float UNIVERSE_AGE_GA = 13.8f;  // miliardi di anni

// ============================================================================
// Calcoli tempo umano — tutti gli algoritmi funzionano per qualsiasi data
// ============================================================================
static inline float dayProgress() {
    time_t now;
    struct tm t;
    time(&now);
    localtime_r(&now, &t);

    int sec = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
    return sec / 86400.0f;
}

static inline float weekProgress() {
    time_t now;
    struct tm t;
    time(&now);
    localtime_r(&now, &t);

    int dow = t.tm_wday;      // 0 = domenica
    if (dow == 0) dow = 7;    // domenica come 7
    float dayFrac = dayProgress();
    return (dow - 1 + dayFrac) / 7.0f;
}

static inline int daysInMonth(int year, int month) {
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month == 2) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    return days[month - 1];
}

static inline float monthProgress() {
    time_t now;
    struct tm t;
    time(&now);
    localtime_r(&now, &t);

    int day = t.tm_mday;
    int totalDays = daysInMonth(t.tm_year + 1900, t.tm_mon + 1);
    float dayFrac = dayProgress();
    
    return (day - 1 + dayFrac) / totalDays;
}

static inline float yearProgress() {
    time_t now;
    struct tm t;
    time(&now);
    localtime_r(&now, &t);

    int year = t.tm_year + 1900;
    bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    float daysInYear = leap ? 366.0f : 365.0f;
    
    float dayFrac = dayProgress();
    return (t.tm_yday + dayFrac) / daysInYear;
}

static inline float centuryProgress() {
    time_t now;
    struct tm t;
    time(&now);
    localtime_r(&now, &t);

    int year = t.tm_year + 1900;
    int centuryStart = (year / 100) * 100;  // calcola inizio secolo dinamicamente
    float yearsIn = (year - centuryStart) + yearProgress();
    return yearsIn / 100.0f;
}

// ============================================================================
// DRAW HELPERS
// ============================================================================
static void drawLabel(int x, int y, const String& s) {
    gfx->setCursor(x, y);
    gfx->setTextColor(COL_TEXT);
    gfx->setTextSize(TEXT_SCALE);
    gfx->print(s);
}

static void drawValueR(int rightX, int y, const String& s) {
    int16_t x1, y1;
    uint16_t w, h;
    gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
    gfx->setCursor(rightX - w, y);
    gfx->setTextColor(COL_ACCENT2);
    gfx->setTextSize(TEXT_SCALE);
    gfx->print(s);
}

static void drawBarFull(int x, int y, int w, int h, float pct, uint16_t col) {
    gfx->drawRect(x, y, w, h, COL_DIVIDER);
    int fill = int(pct * (w - 2));
    gfx->fillRect(x + 1, y + 1, fill, h - 2, col);
}

// ============================================================================
// TEMPO UMANO — Today / Week / Month / Year / Century
// ============================================================================
static void drawChronosHuman() {

    float d = dayProgress();
    float w = weekProgress();
    float m = monthProgress();
    float y = yearProgress();
    float c = centuryProgress();

    int x = PAGE_X + 30;
    int rightX = PAGE_X + PAGE_W - 30;
    int barW = PAGE_W - 60;
    int barH = 14;

    // TODAY
    drawLabel(x, 55, "Today");
    drawValueR(rightX, 55, String(int(d * 100)) + "%");
    drawBarFull(x, 70, barW, barH, d, COL_ACCENT1);

    // WEEK
    drawLabel(x, 90, "Week");
    drawValueR(rightX, 90, String(int(w * 100)) + "%");
    drawBarFull(x, 105, barW, barH, w, COL_ACCENT1);

    // MONTH
    drawLabel(x, 125, "Month");
    drawValueR(rightX, 125, String(int(m * 100)) + "%");
    drawBarFull(x, 140, barW, barH, m, COL_ACCENT1);

    // YEAR
    drawLabel(x, 160, "Year");
    drawValueR(rightX, 160, String(int(y * 100)) + "%");
    drawBarFull(x, 175, barW, barH, y, COL_ACCENT2);

    // CENTURY
    drawLabel(x, 195, "Century");
    drawValueR(rightX, 195, String(c * 100, 1) + "%");
    drawBarFull(x, 210, barW, barH, c, COL_ACCENT2);
}

// ============================================================================
// TEMPO COSMICO — Earth age + Sun age
// ============================================================================
static void drawChronosCosmic() {

    float pEarth = EARTH_AGE_GA / EARTH_END_GA;
    float pSun   = SUN_AGE_GA / SUN_LIFE_GA;

    int x = PAGE_X + 30;
    int rightX = PAGE_X + PAGE_W - 30;
    int barW = PAGE_W - 60;
    int barH = 16;

    // EARTH AGE
    drawLabel(x, 240, "Earth");
    drawValueR(rightX, 240, String(pEarth * 100, 1) + "%");
    drawBarFull(x, 255, barW, barH, pEarth, COL_ACCENT2);

    gfx->setCursor(x, 275);
    gfx->setTextColor(COL_DIVIDER);
    gfx->setTextSize(TEXT_SCALE);
    gfx->print("4.54 / 10 Ga");

    // SUN AGE
    drawLabel(x, 295, "Sun");
    drawValueR(rightX, 295, String(pSun * 100, 1) + "%");
    drawBarFull(x, 310, barW, barH, pSun, COL_ACCENT1);

    gfx->setCursor(x, 330);
    gfx->setTextColor(COL_DIVIDER);
    gfx->setTextSize(TEXT_SCALE);
    gfx->print("4.6 / 10 Ga");
}

// ============================================================================
// TIMELINE COSMICA
// ============================================================================
static void drawChronosUniverse() {

    int x = PAGE_X + 30;
    int rightX = PAGE_X + PAGE_W - 30;
    int w = PAGE_W - 60;

    drawLabel(x, 360, "Universe");

    // linea base
    gfx->drawFastHLine(x, 385, w, COL_TEXT);

    // tappe (logaritmiche semplificate)
    const int t1 = x + (w * 0.02);  // Big Bang
    const int t2 = x + (w * 0.10);  // CMB
    const int t3 = x + (w * 0.25);  // first stars
    const int t4 = x + (w * 0.60);  // Milky Way
    const int t5 = x + (w * 0.99);  // Today

    gfx->drawFastVLine(t1, 380, 10, COL_DIVIDER);
    gfx->drawFastVLine(t2, 380, 10, COL_DIVIDER);
    gfx->drawFastVLine(t3, 380, 10, COL_DIVIDER);
    gfx->drawFastVLine(t4, 380, 10, COL_DIVIDER);

    // marker oggi
    gfx->drawFastVLine(t5, 376, 16, COL_ACCENT1);

    // testo micro
    gfx->setCursor(x, 405);
    gfx->setTextColor(COL_TEXT);
    gfx->setTextSize(TEXT_SCALE);
    gfx->print("13.8 billion years");
}

// ============================================================================
// Pagina principale
// ============================================================================
inline void pageChronos() {

    // sfondo più scuro dedicato
    gfx->fillScreen(CHRONOS_BG);

    drawHeader("CHRONOS");

    drawChronosHuman();
    drawChronosCosmic();
    drawChronosUniverse();
}
