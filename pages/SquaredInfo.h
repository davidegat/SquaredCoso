#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include "../handlers/globals.h"

// ============================================================================
// PAGE INFO – OTTIMIZZATA
// ============================================================================

// extern
extern Arduino_RGB_Display* gfx;

extern const uint16_t COL_BG;
extern const uint16_t COL_TEXT;
extern const uint16_t COL_ACCENT1;
extern const uint16_t COL_ACCENT2;
extern const uint16_t COL_DIVIDER;
extern const uint16_t COL_HEADER;
extern const uint16_t COL_GOOD;
extern const uint16_t COL_WARN;
extern const uint16_t COL_BAD;

extern const int PAGE_X;
extern const int PAGE_Y;
extern const int BASE_CHAR_H;
extern const int TEXT_SCALE;
extern const int CHAR_H;

extern void drawHeader(const String& title);
extern void drawHLine(int y);
extern String sanitizeText(const String& in);

extern bool g_show[];

#define INFO_BADGE_W  28
#define INFO_BADGE_H  18

// ============================================================================
// BADGE (molto più leggero)
// ============================================================================
static inline void drawBadge(int16_t x, int16_t y, uint8_t v) {
  uint16_t col =
      (v < 40) ? 0x07E0 :      // verde
      (v < 70) ? 0xFFE0 :      // giallo
                 0xF800;       // rosso
  gfx->fillRoundRect(x, y, INFO_BADGE_W, INFO_BADGE_H, 4, col);
}

// ============================================================================
// FORMAT BYTES (zero String temporanee inutili)
// ============================================================================
static inline String formatBytes(size_t b) {
  char buf[20];
  if (b >= (1 << 20)) snprintf(buf, sizeof(buf), "%u MB", (unsigned)(b >> 20));
  else if (b >= (1 << 10)) snprintf(buf, sizeof(buf), "%u KB", (unsigned)(b >> 10));
  else snprintf(buf, sizeof(buf), "%u B", (unsigned)b);
  return String(buf);
}

// ============================================================================
// UPTIME leggibile – zero sprechi
// ============================================================================
static inline String formatUptime() {
  unsigned long sec = millis() / 1000UL;

  unsigned d = sec / 86400UL; sec %= 86400UL;
  unsigned h = sec / 3600UL;  sec %= 3600UL;
  unsigned m = sec / 60UL;    sec %= 60UL;

  char buf[20];
  snprintf(buf, sizeof(buf), "%ud %02u:%02u:%02u", d, h, m, (unsigned)sec);
  return String(buf);
}

// ============================================================================
// STIMA CPU — ottimizzata (loop più efficiente)
// ============================================================================
static uint8_t estimateCPU() {

  const uint32_t win = 200;
  volatile uint32_t sink = 0;

  const uint32_t t0 = millis();
  const uint32_t u0 = micros();

  // loop più denso (meno overhead)
  while (millis() - t0 < win) {
    sink += 7;
  }

  uint32_t dt = micros() - u0;
  if (dt > 250000) dt = 250000;
  if (dt <  80000) dt = 80000;

  return (uint8_t)constrain(map(dt, 80000, 250000, 5, 99), 5, 99);
}

// ============================================================================
// PAGINA INFO — versione ottimizzata
// ============================================================================
static void pageInfo() {

  drawHeader("Device info");
  int y = PAGE_Y;

  gfx->setTextSize(TEXT_SCALE);
  gfx->setTextColor(COL_TEXT, COL_BG);

  // -------------------------------------------------------------------------
  // URL SETTINGS
  // -------------------------------------------------------------------------
  const bool sta = (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED);

  char urlbuf[64];
  if (sta) {
    snprintf(urlbuf, sizeof(urlbuf), "http://%s/settings",
             WiFi.localIP().toString().c_str());
  } else if (WiFi.getMode() == WIFI_AP) {
    snprintf(urlbuf, sizeof(urlbuf), "http://%s/settings",
             WiFi.softAPIP().toString().c_str());
  } else {
    snprintf(urlbuf, sizeof(urlbuf), "n/a");
  }

  gfx->setCursor(PAGE_X, y + CHAR_H);
  gfx->print(urlbuf);

  y += CHAR_H * 2 + 10;
  drawHLine(y);
  y += 10;

  // -------------------------------------------------------------------------
  // CPU
  // -------------------------------------------------------------------------
  uint8_t cpu = estimateCPU();

  gfx->setCursor(PAGE_X, y + CHAR_H);
  gfx->print("CPU: ");
  gfx->print((int)cpu);
  gfx->print("%");

  drawBadge(420, y + (CHAR_H/2), cpu);

  y += CHAR_H * 2 + 10;

  // -------------------------------------------------------------------------
  // RAM
  // -------------------------------------------------------------------------
  size_t freeHeap  = ESP.getFreeHeap();
  size_t totalHeap = ESP.getHeapSize();

  gfx->setCursor(PAGE_X, y + CHAR_H);
  gfx->print("RAM: ");
  gfx->print(formatBytes(freeHeap));
  gfx->print(" / ");
  gfx->print(formatBytes(totalHeap));

  uint8_t ramPct =
      (uint8_t)((100 * (totalHeap - freeHeap)) /
                (totalHeap ? totalHeap : 1));

  drawBadge(420, y + (CHAR_H/2), ramPct);

  y += CHAR_H * 2 + 10;

  // -------------------------------------------------------------------------
  // PAGINE ATTIVE
  // -------------------------------------------------------------------------
  int enabled = 0;
  for (int i = 0; i < PAGES; i++) enabled += g_show[i] ? 1 : 0;

  gfx->setCursor(PAGE_X, y + CHAR_H);
  gfx->print("Pages: ");
  gfx->print(enabled);
  gfx->print(" / ");
  gfx->print((int)PAGES);

  uint8_t pgPct = (uint8_t)((100 * enabled) / (PAGES ? PAGES : 1));
  drawBadge(420, y + (CHAR_H/2), pgPct);

  y += CHAR_H * 2 + 14;
  drawHLine(y);
  y += 10;

  // -------------------------------------------------------------------------
  // INFO FIRMWARE
  // -------------------------------------------------------------------------
  gfx->setCursor(PAGE_X, y + CHAR_H);
  gfx->print("Firmware: ");
  gfx->print(FW_NAME);
  gfx->print(" ");
  gfx->print(FW_VERSION);


  y += CHAR_H * 2 + 10;

  gfx->setCursor(PAGE_X, y + CHAR_H);
  gfx->print("          github.com/davidegat");

  y += CHAR_H * 2 + 10;

  gfx->setCursor(PAGE_X, y + CHAR_H);
  gfx->print("Model:    ESP32-S3 Panel 4848S040");

}

