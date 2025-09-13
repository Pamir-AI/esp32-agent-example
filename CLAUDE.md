ESP32‑S3 Matrix — Fast Path for New Games + Debug

This repo lets anyone build a new LED‑matrix game quickly, visualize it in a terminal, and keep hardware + tools in sync. Follow this flow and you’re productive on day one.

1) Configure The Board Once
- Edit `config/BoardConfig.h` (single source of truth):
  - `MATRIX_WIDTH/HEIGHT` (default 8x8)
  - `LED_PIN` (default 14) and `BRIGHTNESS_LIMIT` (≤ 60)
  - `COLOR_ORDER` (use `RGB` for Waveshare ESP32‑S3‑Matrix)
  - Wiring/orientation: `PANEL_WIRING_SERPENTINE` (0=progressive row‑major), `PANEL_ROTATION`, `PANEL_FLIP_X/Y`
- All games include this file; change it once and everything follows.

2) Use The Shared Helpers
- Include in your sketch:
  - `#include "config/BoardConfig.h"`
  - `#include "lib/MatrixUtil/MatrixUtil.h"`
- What you get:
  - `MU_XY(x,y)`: stable XY→index mapping honoring the board profile
  - `MU_ADD_LEDS(DATA_PIN, leds, count)`: FastLED init using shared `COLOR_ORDER`
  - `MU_PrintMeta()`: prints a one‑time META line (size, wiring, rotation, flips)
  - `MU_SendFrameCSV(leds)`: prints one CSV‑hex frame compatible with the terminal visualizer
  - `MU_DrawCalibration(leds)`: corner markers TL=G, TR=R, BL=B, BR=W

Minimal New‑Game Template
```cpp
#include <FastLED.h>
#include "config/BoardConfig.h"
#include "lib/MatrixUtil/MatrixUtil.h"

#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)
CRGB leds[NUM_LEDS];

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 1500) { delay(10); } // non‑blocking
  if (Serial) MU_PrintMeta();

  MU_ADD_LEDS(LED_PIN, leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS_LIMIT);
  FastLED.clear(); FastLED.show();
}

void loop() {
  FastLED.clear();
  for (uint8_t y=0; y<MATRIX_HEIGHT; ++y)
    for (uint8_t x=0; x<MATRIX_WIDTH; ++x)
      if ((x+y)&1) leds[MU_XY(x,y)] = CRGB(30,30,30);
  FastLED.show();

  if (Serial) MU_SendFrameCSV(leds); // terminal visualization
  delay(100);
}
```

3) Visualize In Terminal (No GUI)
- Auto‑detect port, auto‑configure from META:
  - `python3 tools/led_matrix_viz.py --list-ports`
  - `python3 tools/led_matrix_viz.py -p /dev/ttyACM? -b 115200 --stats --verbose`
- If you don’t print META, pass flags: `--width/--height --input-order xy --wiring progressive --rotate ...`
- Tips: `--ascii` for plain text, `--flip-x/--flip-y` for quick checks.

4) Build / Flash (ESP32‑S3)
- In Arduino IDE: Tools → USB CDC On Boot = Enabled (prevents Serial from blocking).
- Use the bundled CLI at `./bin/arduino-cli`:
  - First‑time setup (once):
    - `./bin/arduino-cli config init`
    - `./bin/arduino-cli core update-index`
    - `./bin/arduino-cli core install esp32:esp32`
  - Compile (example: Snake):
    - `./bin/arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc examples/Snake`
  - Upload:
    - `./bin/arduino-cli upload --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc --port /dev/ttyACM0 examples/Snake`
  - Monitor:
    - `./bin/arduino-cli monitor --port /dev/ttyACM0 --config baudrate=115200`

5) Proven Debug Workflow
- Keep Serial optional: short wait, then guard prints with `if (Serial)`.
- Use `MU_DrawCalibration(leds)` once to prove mapping (TL=G, TR=R, BL=B, BR=W).
- If colors are wrong on hardware, fix `COLOR_ORDER` in `BoardConfig.h` (Waveshare = `RGB`).
- If left/right swap on alternating rows, set `PANEL_WIRING_SERPENTINE` to `0` (progressive row‑major).

6) Quick Commands
- List ports: `python3 tools/led_matrix_viz.py --list-ports`
- Visualize: `python3 tools/led_matrix_viz.py -p /dev/ttyACM0 -b 115200 --stats`
- Raw monitor: `python3 tools/monitor_pong.py -p /dev/ttyACM0`
- Demo (no hardware): `python3 tools/led_matrix_viz.py --demo --width 8 --height 8`

7) Tips For The Next Dev (and agents)
- Start from the template; draw only via `MU_XY()`.
- Never hardcode edges; use `MATRIX_WIDTH/HEIGHT`.
- Limit debug frame rate (≈5–20 FPS) to keep serial stable.
- Update only `BoardConfig.h` for new panels/orientation; all games + tools follow.

Repo Highlights
- `config/BoardConfig.h` — board profile (geometry, color order, wiring/orientation, brightness)
- `lib/MatrixUtil/MatrixUtil.h` — mapping + serial frame helpers
- `tools/led_matrix_viz.py` — terminal visualizer (reads META to auto‑configure)
- `examples/` — reference sketches (Snake, tilt‑demo, wifi‑slam)

DEBUGING ISSUES
- when automatic upload fails, always ask for user to manual put device into bootloader mode, then wait for confirm before reflush again
- MicroPython Board in FS mode is a pico device (which belong to the internal system), you should see ESP devie when you try lsusb, if not remind user to try replug in usb