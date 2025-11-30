/*
===============================================================================
   SQUARED — TOUCH HANDLER + PAGE MANAGER
   Descrizione: Gestione touch GT911, mappatura coordinate,
                overlay menu pagine, paginazione e pulsanti azione.
   Autore: Davide “gat” Nasato
   Repository: https://github.com/davidegat/SquaredCoso
   Licenza: CC BY-NC 4.0
===============================================================================
*/

#pragma once
#include <Arduino.h>
#include <TAMC_GT911.h>

#define TOUCH_SDA 19
#define TOUCH_SCL 45
#define TOUCH_INT -1
#define TOUCH_RST -1

#define TOUCH_WIDTH 480
#define TOUCH_HEIGHT 480

extern uint32_t lastPageSwitch;

class Arduino_RGB_Display;
extern Arduino_RGB_Display *gfx;

extern const uint16_t COL_BG;
extern const uint16_t COL_ACCENT2;

// ============================================================================
// PAGE LABELS BILINGUE
// ============================================================================
static const char *PAGE_LABELS_IT[PAGES] = {
    "Meteo", "Aria",    "Orologio",  "Binario", "Calendario", "Bitcoin",
    "Frase", "Info",    "Countdown", "Forex",   "Temp.7gg",   "Luce/Luna",
    "News",  "HomeAss", "Stellar",   "Note",    "Chronos"};

static const char *PAGE_LABELS_EN[PAGES] = {
    "Weather", "Air",     "Clock",     "Binary", "Calendar", "Bitcoin",
    "Quote",   "Info",    "Countdown", "Forex",  "Temp.7d",  "Light/Moon",
    "News",    "HomeAss", "Stellar",   "Notes",  "Chronos"};

// ============================================================================
// FUNZIONE LABEL BILINGUE
// ============================================================================
static inline const char *pageLabel(int i) {
  return (g_lang == "en") ? PAGE_LABELS_EN[i] : PAGE_LABELS_IT[i];
}

// ---------------------------------------------------------------------------
// COLORI TEMA SCURO
// ---------------------------------------------------------------------------
constexpr uint16_t MENU_BG = 0x0841;
constexpr uint16_t MENU_CARD = 0x18C3;
constexpr uint16_t MENU_BORDER = 0x2945;
constexpr uint16_t MENU_TEXT = 0xE71C;
constexpr uint16_t MENU_TEXT_DIM = 0x7BEF;
constexpr uint16_t MENU_ACCENT = 0x07FF;
constexpr uint16_t MENU_ON_BG = 0x0320;
constexpr uint16_t MENU_ARROW_DIS = 0x1082;

// ---------------------------------------------------------------------------
// OGGETTO TOUCH
// ---------------------------------------------------------------------------
static TAMC_GT911 ts(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, TOUCH_WIDTH,
                     TOUCH_HEIGHT);

// ---------------------------------------------------------------------------
// STATO TOUCH
// ---------------------------------------------------------------------------
static struct {
  uint8_t ready : 1;
  uint8_t paused : 1;
  uint8_t active : 1;
  uint8_t reserved : 5;
} touchState = {0, 0, 0, 0};

#define touchReady touchState.ready
#define touchPaused touchState.paused
#define menuActive touchState.active

static uint32_t lastTouchMs = 0;
static uint8_t lastTouches = 0;

// ---------------------------------------------------------------------------
// PAGINAZIONE
// ---------------------------------------------------------------------------
constexpr uint8_t ITEMS_PER_PAGE = 8;
static uint8_t menuPage = 0;
static uint8_t totalMenuPages = 1;

// ---------------------------------------------------------------------------
// LAYOUT (
// ---------------------------------------------------------------------------
constexpr int16_t M_MARGIN = 16;
constexpr int16_t M_HEADER_H = 58;
constexpr int16_t M_FOOTER_H = 82;

constexpr uint8_t M_COLS = 2;
constexpr uint8_t M_ROWS = 4;
constexpr int16_t M_GAP_X = 12;
constexpr int16_t M_GAP_Y = 10;

constexpr int16_t M_GRID_Y = M_HEADER_H;
constexpr int16_t M_GRID_H = 480 - M_HEADER_H - M_FOOTER_H;

constexpr int16_t M_BTN_W = (480 - M_MARGIN * 2 - M_GAP_X) / M_COLS;
constexpr int16_t M_BTN_H = (M_GRID_H - M_GAP_Y * (M_ROWS - 1)) / M_ROWS;

constexpr int16_t ARROW_SZ = 56;
constexpr int16_t ARROW_Y = 480 - M_MARGIN - ARROW_SZ;
constexpr int16_t ARROW_L_X = M_MARGIN;
constexpr int16_t ARROW_R_X = 480 - M_MARGIN - ARROW_SZ;

constexpr int16_t ACT_GAP = 12;
constexpr int16_t ACT_AREA_X = ARROW_L_X + ARROW_SZ + ACT_GAP;
constexpr int16_t ACT_AREA_W = ARROW_R_X - ACT_AREA_X - ACT_GAP;
constexpr int16_t ACT_BTN_W = (ACT_AREA_W - ACT_GAP) / 2;
constexpr int16_t ACT_BTN_H = ARROW_SZ;
constexpr int16_t ACT_BTN_Y = ARROW_Y;

constexpr int16_t BTN_CANCEL_X = ACT_AREA_X;
constexpr int16_t BTN_OK_X = ACT_AREA_X + ACT_BTN_W + ACT_GAP;

// ---------------------------------------------------------------------------
// CACHE posizioni
// ---------------------------------------------------------------------------
static int16_t slotX[ITEMS_PER_PAGE];  // era int (32 → 16 byte)
static int16_t slotY[ITEMS_PER_PAGE];  // era int (32 → 16 byte)
static int8_t slotIdx[ITEMS_PER_PAGE]; // era int (32 → 8 byte)
static uint8_t visibleCount = 0;       // era int (4 → 1 byte)

// ---------------------------------------------------------------------------
// Calcola pagine totali
// ---------------------------------------------------------------------------
static void calcMenuPages() {
  uint8_t n = (uint8_t)PAGES;
  totalMenuPages = (n + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
  if (totalMenuPages < 1)
    totalMenuPages = 1;
}

// ---------------------------------------------------------------------------
// Disegna singolo bottone pagina
// ---------------------------------------------------------------------------
static void drawPageBtn(uint8_t slot, uint8_t pageIdx, int16_t x, int16_t y,
                        bool isOn) {
  constexpr uint8_t R = 8;

  uint16_t bgCol = isOn ? MENU_ON_BG : MENU_CARD;
  uint16_t borderCol = isOn ? MENU_ACCENT : MENU_BORDER;
  uint16_t textCol = isOn ? MENU_TEXT : MENU_TEXT_DIM;

  gfx->fillRoundRect(x, y, M_BTN_W, M_BTN_H, R, bgCol);
  gfx->drawRoundRect(x, y, M_BTN_W, M_BTN_H, R, borderCol);

  // Indicatore stato
  int16_t indX = x + M_BTN_W - 22;
  int16_t indY = y + M_BTN_H / 2;
  if (isOn) {
    gfx->fillCircle(indX, indY, 7, MENU_ACCENT);
  } else {
    gfx->drawCircle(indX, indY, 7, MENU_TEXT_DIM);
  }

  // Label
  gfx->setTextSize(2);
  gfx->setTextColor(textCol);
  gfx->setCursor(x + 14, y + (M_BTN_H - 16) / 2);
  gfx->print(pageLabel(pageIdx));
}

// ---------------------------------------------------------------------------
// Disegna freccia
// ---------------------------------------------------------------------------
static void drawArrowBtn(int16_t x, int16_t y, bool isLeft, bool enabled) {
  uint16_t bgCol = enabled ? MENU_CARD : MENU_ARROW_DIS;
  uint16_t borderCol = enabled ? MENU_ACCENT : MENU_BORDER;
  uint16_t arrowCol = enabled ? MENU_TEXT : MENU_TEXT_DIM;

  gfx->fillRoundRect(x, y, ARROW_SZ, ARROW_SZ, 8, bgCol);
  gfx->drawRoundRect(x, y, ARROW_SZ, ARROW_SZ, 8, borderCol);

  int16_t cx = x + ARROW_SZ / 2;
  int16_t cy = y + ARROW_SZ / 2;
  constexpr int8_t sz = 12;

  if (isLeft) {
    gfx->fillTriangle(cx - sz + 2, cy, cx + sz - 6, cy - sz, cx + sz - 6,
                      cy + sz, arrowCol);
  } else {
    gfx->fillTriangle(cx + sz - 2, cy, cx - sz + 6, cy - sz, cx - sz + 6,
                      cy + sz, arrowCol);
  }
}

// ---------------------------------------------------------------------------
// Disegna pulsante azione
// ---------------------------------------------------------------------------
static void drawActionBtn(int16_t x, int16_t y,
                          const __FlashStringHelper *label, bool filled) {
  uint16_t bgCol = filled ? MENU_ACCENT : MENU_CARD;
  uint16_t textCol = filled ? MENU_BG : MENU_TEXT;
  uint16_t borderCol = filled ? MENU_ACCENT : MENU_BORDER;

  gfx->fillRoundRect(x, y, ACT_BTN_W, ACT_BTN_H, 8, bgCol);
  gfx->drawRoundRect(x, y, ACT_BTN_W, ACT_BTN_H, 8, borderCol);

  gfx->setTextSize(2);
  gfx->setTextColor(textCol);

  // Calcola larghezza stringa PROGMEM
  PGM_P p = reinterpret_cast<PGM_P>(label);
  uint8_t len = strlen_P(p);
  int16_t textW = len * 12;
  int16_t textX = x + (ACT_BTN_W - textW) / 2;
  int16_t textY = y + (ACT_BTN_H - 14) / 2;

  gfx->setCursor(textX, textY);
  gfx->print(label);
}

// ---------------------------------------------------------------------------
// Header
// ---------------------------------------------------------------------------
static void drawMenuHeader() {
  gfx->fillRect(0, 0, 480, 3, MENU_ACCENT);

  gfx->setTextSize(2);
  gfx->setTextColor(MENU_TEXT);
  gfx->setCursor(M_MARGIN, 16);
  gfx->print(F("Gestione Pagine"));

  uint8_t activeCount = 0;
  for (uint8_t i = 0; i < (uint8_t)PAGES; i++) {
    if (g_show[i])
      activeCount++;
  }

  gfx->setTextSize(1);
  gfx->setTextColor(MENU_TEXT_DIM);
  gfx->setCursor(M_MARGIN, 40);
  gfx->print(activeCount);
  gfx->print(F("/"));
  gfx->print((uint8_t)PAGES);
  gfx->print(F(" attive"));

  char buf[6];
  snprintf_P(buf, sizeof(buf), PSTR("%d/%d"), menuPage + 1, totalMenuPages);
  uint8_t w = strlen(buf) * 6;
  gfx->setCursor(480 - M_MARGIN - w, 40);
  gfx->print(buf);

  gfx->drawFastHLine(M_MARGIN, M_HEADER_H - 4, 480 - M_MARGIN * 2, MENU_BORDER);
}

// ---------------------------------------------------------------------------
// Aggiorna contatore
// ---------------------------------------------------------------------------
static void updateMenuCounter() {
  gfx->fillRect(M_MARGIN, 38, 100, 14, MENU_BG);
  gfx->fillRect(400, 38, 70, 14, MENU_BG);

  uint8_t activeCount = 0;
  for (uint8_t i = 0; i < (uint8_t)PAGES; i++) {
    if (g_show[i])
      activeCount++;
  }

  gfx->setTextSize(1);
  gfx->setTextColor(MENU_TEXT_DIM);
  gfx->setCursor(M_MARGIN, 40);
  gfx->print(activeCount);
  gfx->print(F("/"));
  gfx->print((uint8_t)PAGES);
  gfx->print(F(" attive"));

  char buf[6];
  snprintf_P(buf, sizeof(buf), PSTR("%d/%d"), menuPage + 1, totalMenuPages);
  uint8_t w = strlen(buf) * 6;
  gfx->setCursor(480 - M_MARGIN - w, 40);
  gfx->print(buf);
}

// ---------------------------------------------------------------------------
// Disegna griglia
// ---------------------------------------------------------------------------
static void drawMenuGrid() {
  gfx->fillRect(0, M_HEADER_H, 480, M_GRID_H, MENU_BG);

  uint8_t startIdx = menuPage * ITEMS_PER_PAGE;
  uint8_t endIdx = startIdx + ITEMS_PER_PAGE;
  if (endIdx > (uint8_t)PAGES)
    endIdx = (uint8_t)PAGES;
  visibleCount = endIdx - startIdx;

  uint8_t slot = 0;
  for (uint8_t i = startIdx; i < endIdx; i++) {
    uint8_t col = slot % M_COLS;
    uint8_t row = slot / M_COLS;

    int16_t x = M_MARGIN + col * (M_BTN_W + M_GAP_X);
    int16_t y = M_GRID_Y + row * (M_BTN_H + M_GAP_Y);

    slotX[slot] = x;
    slotY[slot] = y;
    slotIdx[slot] = i;

    drawPageBtn(slot, i, x, y, g_show[i]);
    slot++;
  }

  drawArrowBtn(ARROW_L_X, ARROW_Y, true, menuPage > 0);
  drawArrowBtn(ARROW_R_X, ARROW_Y, false, menuPage < totalMenuPages - 1);
  updateMenuCounter();
}

// ---------------------------------------------------------------------------
// MENU COMPLETO
// ---------------------------------------------------------------------------
static void drawPauseOverlay() {
  calcMenuPages();
  menuPage = 0;

  gfx->fillScreen(MENU_BG);
  drawMenuHeader();
  drawActionBtn(BTN_CANCEL_X, ACT_BTN_Y, F("ANNULLA"), false);
  drawActionBtn(BTN_OK_X, ACT_BTN_Y, F("OK"), true);
  drawMenuGrid();
}

// ---------------------------------------------------------------------------
// CLEAR overlay
// ---------------------------------------------------------------------------
static void clearPauseOverlay() {
  gfx->fillScreen(COL_BG);
  drawCurrentPage();
}

// ---------------------------------------------------------------------------
// INIT TOUCH
// ---------------------------------------------------------------------------
static bool touchInit() {
  ts.begin();
  ts.setRotation(0);
  ts.read();
  touchReady = true;
  return true;
}

// ---------------------------------------------------------------------------
// MAP COORDINATE
// ---------------------------------------------------------------------------
static inline void mapTouch(int16_t &x, int16_t &y) {
  x = ts.points[0].y;
  y = 480 - ts.points[0].x;
}

// ---------------------------------------------------------------------------
// LOOP TOUCH
// ---------------------------------------------------------------------------
static void touchLoop() {
  if (!touchReady)
    return;
  ts.read();

  bool nowDown = (ts.touches > 0);
  bool prevDown = (lastTouches > 0);
  lastTouches = ts.touches;

  if (!nowDown || prevDown)
    return;

  uint32_t now = millis();
  if (now - lastTouchMs < 200)
    return;
  lastTouchMs = now;

  int16_t x, y;
  mapTouch(x, y);

  // =========================================================================
  // MENU APERTO
  // =========================================================================
  if (menuActive) {

    // FRECCIA SINISTRA
    if (x >= ARROW_L_X && x <= ARROW_L_X + ARROW_SZ && y >= ARROW_Y &&
        y <= ARROW_Y + ARROW_SZ) {
      if (menuPage > 0) {
        menuPage--;
        drawMenuGrid();
      }
      return;
    }

    // FRECCIA DESTRA
    if (x >= ARROW_R_X && x <= ARROW_R_X + ARROW_SZ && y >= ARROW_Y &&
        y <= ARROW_Y + ARROW_SZ) {
      if (menuPage < totalMenuPages - 1) {
        menuPage++;
        drawMenuGrid();
      }
      return;
    }

    // ANNULLA
    if (x >= BTN_CANCEL_X && x <= BTN_CANCEL_X + ACT_BTN_W && y >= ACT_BTN_Y &&
        y <= ACT_BTN_Y + ACT_BTN_H) {
      menuActive = false;
      touchPaused = false;
      clearPauseOverlay();
      return;
    }

    // CONFERMA
    if (x >= BTN_OK_X && x <= BTN_OK_X + ACT_BTN_W && y >= ACT_BTN_Y &&
        y <= ACT_BTN_Y + ACT_BTN_H) {
      prefs.begin("app", false);
      prefs.putUInt("pages_mask", pagesMaskFromArray());
      prefs.end();

      menuActive = false;
      touchPaused = false;
      softReboot();
      return;
    }

    // TOGGLE PAGINE
    for (uint8_t s = 0; s < visibleCount; s++) {
      if (x >= slotX[s] && x <= slotX[s] + M_BTN_W && y >= slotY[s] &&
          y <= slotY[s] + M_BTN_H) {

        int8_t idx = slotIdx[s];
        g_show[idx] = !g_show[idx];
        drawPageBtn(s, idx, slotX[s], slotY[s], g_show[idx]);
        updateMenuCounter();
        return;
      }
    }

    return;
  }

  // =========================================================================
  // MENU CHIUSO → Apri
  // =========================================================================
  touchPaused = true;
  menuActive = true;
  drawPauseOverlay();
}
