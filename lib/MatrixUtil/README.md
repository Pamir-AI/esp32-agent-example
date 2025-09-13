MatrixUtil — Shared helpers for LED matrix games

Overview
- Header-only helpers so every sketch can share one mapping and serial framing logic.
- Works with `config/BoardConfig.h` as the single source of truth for geometry, wiring, rotation, flips, and color order.

Provided API (include `config/BoardConfig.h` first)
- `uint16_t MU_XY(uint8_t x, uint8_t y)` — Maps logical XY to LED index (honors wiring/rotation/flips).
- `void MU_PrintMeta()` — Prints one `META:` line with mapping info; the terminal visualizer auto-configures from this.
- `void MU_SendFrameCSV(const CRGB* leds)` — Emits one CSV-hex frame (`FRAME:`) in XY order for the visualizer.
- `void MU_DrawCalibration(CRGB* leds)` — Writes corner markers to `leds` (TL=G, TR=R, BL=B, BR=W).
- `MU_ADD_LEDS(DATA_PIN, leds, count)` — Macro wrapping `FastLED.addLeds<..., COLOR_ORDER>`.

Usage in a sketch
```
#include <FastLED.h>
#include "config/BoardConfig.h"
#include "lib/MatrixUtil/MatrixUtil.h"

#define LED_PIN 14
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)
CRGB leds[NUM_LEDS];

void setup() {
  Serial.begin(115200);
  delay(300);
  MU_PrintMeta();
  MU_ADD_LEDS(LED_PIN, leds, NUM_LEDS);
  FastLED.setBrightness(60);
  FastLED.clear(); FastLED.show();
}

void loop() {
  // Draw something using MU_XY(x,y)
  FastLED.clear();
  for (uint8_t y = 0; y < MATRIX_HEIGHT; ++y)
    for (uint8_t x = 0; x < MATRIX_WIDTH; ++x)
      if ((x + y) % 2 == 0) leds[MU_XY(x,y)] = CRGB(30,30,30);
  FastLED.show();

  // Send one debug frame for the terminal visualizer
  MU_SendFrameCSV(leds);
  delay(100);
}
```

Notes
- Keep `BoardConfig.h` accurate for your panel; then all sketches behave consistently.
- The `META:` line is optional but recommended; it helps host tools detect config.
- If your panel uses a different chipset, `#define MU_CHIPSET <your chipset>` before including this header.

