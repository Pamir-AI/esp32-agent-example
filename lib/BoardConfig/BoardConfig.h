// ESP32-S3-Matrix Board Configuration
// Waveshare ESP32-S3-Matrix with 8x8 WS2812B LED Matrix
// All games should include this file for consistent mapping

#pragma once

#include <FastLED.h>

// Hardware Pin Configuration
#define LED_PIN 14
#define BRIGHTNESS_LIMIT 60  // Safety limit to prevent overheating

// Panel geometry
#define MATRIX_WIDTH 8
#define MATRIX_HEIGHT 8
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

// LED color order for WS2812B on ESP32-S3-Matrix
#define COLOR_ORDER RGB

// Wiring configuration - ESP32-S3-Matrix uses serpentine wiring
// Row 0: 0→1→2→3→4→5→6→7
// Row 1: 15→14→13→12→11→10→9→8 (reversed)
// Row 2: 16→17→18→19→20→21→22→23
// etc.
#define PANEL_WIRING_SERPENTINE 0

// Orientation flags (adjust if your display appears flipped)
#define PANEL_FLIP_X 0
#define PANEL_FLIP_Y 0

// Rotation in degrees (0, 90, 180, 270)
#define PANEL_ROTATION 0

// Debug/Calibration mode
#define PANEL_CALIBRATION 0

// Frame rate for serial visualization (5-20 FPS recommended)
#define FRAME_RATE_MS 100  // 10 FPS

// XY coordinate mapping function for serpentine wiring
// This maps logical (x,y) coordinates to physical LED index
inline uint16_t XY(uint8_t x, uint8_t y) {
  // Bounds checking
  if (x >= MATRIX_WIDTH) x = MATRIX_WIDTH - 1;
  if (y >= MATRIX_HEIGHT) y = MATRIX_HEIGHT - 1;
  
  uint16_t index;
  
  #if PANEL_FLIP_Y
    y = (MATRIX_HEIGHT - 1) - y;
  #endif
  
  #if PANEL_FLIP_X
    x = (MATRIX_WIDTH - 1) - x;
  #endif
  
  #if PANEL_WIRING_SERPENTINE
    // Serpentine wiring: odd rows are reversed
    if (y & 0x01) {
      // Odd row: right to left
      index = (y * MATRIX_WIDTH) + (MATRIX_WIDTH - 1 - x);
    } else {
      // Even row: left to right
      index = (y * MATRIX_WIDTH) + x;
    }
  #else
    // Progressive wiring: simple row-major
    index = (y * MATRIX_WIDTH) + x;
  #endif
  
  return index;
}

// Helper function to send frame data for visualization
inline void sendFrameData(CRGB* leds) {
  Serial.print("FRAME:");
  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    for (int x = 0; x < MATRIX_WIDTH; x++) {
      CRGB color = leds[XY(x, y)];
      char buf[8];
      sprintf(buf, "%02X%02X%02X,", color.r, color.g, color.b);
      Serial.print(buf);
    }
  }
}

// Standard color definitions for consistency
namespace MatrixColors {
  const CRGB BLACK   = CRGB(0, 0, 0);
  const CRGB RED     = CRGB(60, 0, 0);
  const CRGB GREEN   = CRGB(0, 60, 0);
  const CRGB BLUE    = CRGB(0, 0, 60);
  const CRGB YELLOW  = CRGB(60, 60, 0);
  const CRGB CYAN    = CRGB(0, 60, 60);
  const CRGB MAGENTA = CRGB(60, 0, 60);
  const CRGB WHITE   = CRGB(60, 60, 60);
  const CRGB ORANGE  = CRGB(60, 30, 0);
  const CRGB PURPLE  = CRGB(30, 0, 60);
}
