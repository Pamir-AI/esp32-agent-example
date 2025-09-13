// MatrixUtil.h - Shared helpers for LED matrix games
// Usage: include your board profile first (config/BoardConfig.h), then include this header.
// Provides:
//  - MU_XY(x,y): stable XY->index mapping honoring rotation, flips and wiring.
//  - MU_PrintMeta(): prints a single META line describing mapping for host tools.
//  - MU_SendFrameCSV(leds): emits one CSV-hex frame (FRAME:...) in XY scan order.
//  - MU_DrawCalibration(leds): draws corner markers (TL=G, TR=R, BL=B, BR=W).

#pragma once

#include <Arduino.h>
#include <FastLED.h>

// Expect the including sketch to define these in config/BoardConfig.h BEFORE this header.
#ifndef MATRIX_WIDTH
#error "Include config/BoardConfig.h before MatrixUtil.h"
#endif
#ifndef MATRIX_HEIGHT
#error "Include config/BoardConfig.h before MatrixUtil.h"
#endif
#ifndef COLOR_ORDER
#error "COLOR_ORDER not defined. Define in config/BoardConfig.h (e.g., RGB, GRB, ...)."
#endif

#ifndef PANEL_WIRING_SERPENTINE
#define PANEL_WIRING_SERPENTINE 0
#endif
#ifndef PANEL_ROTATION
#define PANEL_ROTATION 0
#endif
#ifndef PANEL_FLIP_X
#define PANEL_FLIP_X 0
#endif
#ifndef PANEL_FLIP_Y
#define PANEL_FLIP_Y 0
#endif

// XY mapping honoring rotation, flips and wiring
static inline uint16_t MU_XY(uint8_t x, uint8_t y) {
  if (x >= MATRIX_WIDTH)  x = MATRIX_WIDTH  - 1;
  if (y >= MATRIX_HEIGHT) y = MATRIX_HEIGHT - 1;

  // Apply rotation (clockwise)
  #if PANEL_ROTATION == 90
    uint8_t rx = MATRIX_WIDTH  - 1 - y;
    uint8_t ry = x;
    x = rx; y = ry;
  #elif PANEL_ROTATION == 180
    x = MATRIX_WIDTH  - 1 - x;
    y = MATRIX_HEIGHT - 1 - y;
  #elif PANEL_ROTATION == 270
    uint8_t rx2 = y;
    uint8_t ry2 = MATRIX_HEIGHT - 1 - x;
    x = rx2; y = ry2;
  #endif

  // Optional flips
  #if PANEL_FLIP_X
    x = MATRIX_WIDTH - 1 - x;
  #endif
  #if PANEL_FLIP_Y
    y = MATRIX_HEIGHT - 1 - y;
  #endif

  // Wiring
  #if PANEL_WIRING_SERPENTINE
    if (y & 0x01) {
      return (y * MATRIX_WIDTH) + (MATRIX_WIDTH - 1 - x);
    } else {
      return (y * MATRIX_WIDTH) + x;
    }
  #else
    return (y * MATRIX_WIDTH) + x;
  #endif
}

// Compile-time color order string for META line
static inline const char* MU_ColorOrderStr() {
  #if COLOR_ORDER == RGB
    return "RGB";
  #elif COLOR_ORDER == GRB
    return "GRB";
  #elif COLOR_ORDER == BRG
    return "BRG";
  #elif COLOR_ORDER == GBR
    return "GBR";
  #elif COLOR_ORDER == RBG
    return "RBG";
  #elif COLOR_ORDER == BGR
    return "BGR";
  #else
    return "UNK";
  #endif
}

// Print one-time mapping meta for host tools (e.g., led_matrix_viz.py)
static inline void MU_PrintMeta() {
  Serial.print("META:W="); Serial.print(MATRIX_WIDTH);
  Serial.print(",H="); Serial.print(MATRIX_HEIGHT);
  Serial.print(",ORDER=xy");
  Serial.print(",WIRING="); Serial.print(PANEL_WIRING_SERPENTINE ? "serpentine" : "progressive");
  Serial.print(",ROT="); Serial.print(PANEL_ROTATION);
  Serial.print(",FLIPX="); Serial.print((int)PANEL_FLIP_X);
  Serial.print(",FLIPY="); Serial.print((int)PANEL_FLIP_Y);
  Serial.print(",COLOR="); Serial.println(MU_ColorOrderStr());
}

// Emit one CSV-hex frame in XY scan order
static inline void MU_SendFrameCSV(const CRGB* leds) {
  Serial.print("FRAME:");
  for (int y = 0; y < MATRIX_HEIGHT; ++y) {
    for (int x = 0; x < MATRIX_WIDTH; ++x) {
      CRGB c = leds[MU_XY((uint8_t)x, (uint8_t)y)];
      char buf[8];
      sprintf(buf, "%02X%02X%02X,", c.r, c.g, c.b);
      Serial.print(buf);
    }
  }
  Serial.println();
}

// Draw static corner markers for quick alignment
static inline void MU_DrawCalibration(CRGB* leds) {
  for (int i = 0; i < MATRIX_WIDTH * MATRIX_HEIGHT; ++i) leds[i] = CRGB::Black;
  leds[MU_XY(0, 0)]                           = CRGB(0, 100, 0);
  leds[MU_XY(MATRIX_WIDTH-1, 0)]              = CRGB(100, 0, 0);
  leds[MU_XY(0, MATRIX_HEIGHT-1)]             = CRGB(0, 0, 100);
  leds[MU_XY(MATRIX_WIDTH-1, MATRIX_HEIGHT-1)] = CRGB(100, 100, 100);
}

// Helper macro to add LEDs with shared COLOR_ORDER; chipset fixed to WS2812B by default.
// You can override the chipset by defining MU_CHIPSET before including this header.
#ifndef MU_CHIPSET
#define MU_CHIPSET WS2812B
#endif

#define MU_ADD_LEDS(DATA_PIN, LED_ARRAY, COUNT) \
  FastLED.addLeds<MU_CHIPSET, DATA_PIN, COLOR_ORDER>(LED_ARRAY, COUNT)

