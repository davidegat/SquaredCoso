#pragma once

#include "../handlers/globals.h"
#include <Arduino.h>
#include <WiFi.h>

extern Arduino_RGB_Display *gfx;
extern const uint16_t COL_BG;
extern const uint16_t COL_TEXT;
extern const uint16_t COL_ACCENT2;
extern const int PAGE_X;
extern const int PAGE_Y;
extern const int TEXT_SCALE;
extern const int CHAR_H;
extern void drawHeader(const String &title);
extern void drawHLine(int y);

static void pageInfo() {
  drawHeader("Device info");

  gfx->setTextSize(TEXT_SCALE);
  gfx->setTextColor(COL_TEXT, COL_BG);

  int y = PAGE_Y + CHAR_H;
  gfx->setCursor(PAGE_X, y);
  gfx->print("SquaredCoso");

  y += CHAR_H * 2 + 10;
  drawHLine(y);

  y += CHAR_H + 12;
  gfx->setCursor(PAGE_X, y);
  gfx->print("Firmware: b1.3.0");

  y += CHAR_H * 2;
  gfx->setCursor(PAGE_X, y);
  gfx->print("ESP32-S3 Panel 4848S040");

  y += CHAR_H * 2;
  IPAddress ip = WiFi.localIP();
  char ipBuf[24];
  snprintf(ipBuf, sizeof(ipBuf), "IP: %u.%u.%u.%u",
           (unsigned)ip[0], (unsigned)ip[1],
           (unsigned)ip[2], (unsigned)ip[3]);
  gfx->setTextColor(COL_ACCENT2, COL_BG);
  gfx->setCursor(PAGE_X, y);
  gfx->print(ipBuf);
}
