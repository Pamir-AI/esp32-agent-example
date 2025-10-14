ESP32‑S3 Matrix — Fast Path for New Games + Debug

This repo lets anyone build a new LED‑matrix game quickly, visualize it in a terminal, and keep hardware + tools in sync. Follow this flow and you're productive on day one.

1) Configure The Board Once
- Edit `lib/BoardConfig/BoardConfig.h` (single source of truth):
  - `MATRIX_WIDTH/HEIGHT` (default 8x8)
  - `LED_PIN` (default 14) and `BRIGHTNESS_LIMIT` (≤ 60)
  - `COLOR_ORDER` (use `RGB` for Waveshare ESP32‑S3‑Matrix)
  - Wiring/orientation: `PANEL_WIRING_SERPENTINE` (0=progressive row‑major), `PANEL_ROTATION`, `PANEL_FLIP_X/Y`
- All games include this file; change it once and everything follows.

2) Use The Shared Helpers
- Include in your sketch (`src/main.cpp`):
  ```cpp
  #include <FastLED.h>
  #include <BoardConfig.h>
  #include <MatrixUtil.h>
  ```
- What you get:
  - `MU_XY(x,y)`: stable XY→index mapping honoring the board profile
  - `MU_ADD_LEDS(DATA_PIN, leds, count)`: FastLED init using shared `COLOR_ORDER`
  - `MU_PrintMeta()`: prints a one‑time META line (size, wiring, rotation, flips)
  - `MU_SendFrameCSV(leds)`: prints one CSV‑hex frame compatible with the terminal visualizer
  - `MU_DrawCalibration(leds)`: corner markers TL=G, TR=R, BL=B, BR=W

Minimal New‑Game Template
```cpp
// src/main.cpp
#include <FastLED.h>
#include <BoardConfig.h>
#include <MatrixUtil.h>

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

3) Visualize In Terminal (Use only when trying to debug)
- Auto‑detect port, auto‑configure from META:
  - `python3 tools/led_matrix_viz.py --list-ports`
  - `python3 tools/led_matrix_viz.py -p /dev/ttyACM? -b 115200 --stats --verbose`
- If you don't print META, pass flags: `--width/--height --input-order xy --wiring progressive --rotate ...`
- Tips: `--ascii` for plain text, `--flip-x/--flip-y` for quick checks.

4) Build / Flash (ESP32‑S3) **PLATFORMIO**

## Installing PlatformIO (CM5/Raspberry Pi)

**Recommended: Use `uv` (fast & clean)**
```bash
# Install uv (modern Python package manager)
curl -LsSf https://astral.sh/uv/install.sh | sh
source ~/.bashrc  # Or restart terminal

# Install PlatformIO
uv tool install platformio

# Verify
pio --version
```

**Alternative: Quick install (works immediately)**
```bash
pip3 install platformio --break-system-packages
```

**Why uv?** 10-100x faster than pip, handles PATH automatically, keeps Python environment clean. It's the modern standard for installing Python CLI tools.

## Create New Game
```bash
# Create project structure
mkdir -p examples/MyGame/src
cd examples/MyGame
```

**Create platformio.ini:**
```ini
[env:waveshare-esp32s3-matrix]
platform = espressif32
board = adafruit_feather_esp32s3  # 4MB flash (matches Waveshare hardware)
framework = arduino
lib_deps =
    fastled/FastLED@^3.9.20
lib_extra_dirs = ../../lib      # Shared libs at repo root
monitor_speed = 115200
upload_speed = 921600
```

**Create src/main.cpp** - copy template above

## Build & Upload
```bash
# Build (first time downloads ~1.5GB ESP32 toolchain, 5-10 min)
pio run

# Upload (auto-detects port)
pio run -t upload

# Upload to specific port
pio run -t upload --upload-port /dev/ttyACM0

# Clean
pio run -t clean
```

## Upload Troubleshooting

### Normal Case (No Buttons Needed)
```bash
pio run -t upload  # Usually just works
```

You should see connecting dots then progress bars:
```
Connecting....
Writing at 0x00010000... (10 %)
Writing at 0x00020000... (20 %)
```
If this happens, you're done! No buttons needed.

### If Upload Fails ("Connecting..." Forever)

If you see endless dots with no progress:
```
Connecting........._____....._____
```

**Manual button sequence:**
1. Hold **BOOT** button
2. While holding BOOT, press **RESET** button once
3. Release both buttons
4. Run `pio run -t upload` again within 10 seconds

**Still failing after button sequence?**
- Check USB cable is plugged in firmly
- Confirm device detected: `lsusb | grep -i esp` should show "Espressif USB JTAG/serial debug unit"
- Try different USB port on your computer
- Check cable quality (some cables are power-only, no data)

### If Code Crashes or Acts Weird
- Press **RESET** button once (no BOOT needed)
- This restarts your code from the beginning

### Quick Reference

| Issue | Solution |
|-------|----------|
| Upload times out with dots | BOOT+RESET sequence → retry upload |
| Code frozen/not responding | Press RESET only |
| "Couldn't find board" | Check `lsusb`, check cable, try button sequence |
| First upload on new board | Often needs manual button sequence |

5) Proven Debug Workflow
- Keep Serial optional: short wait, then guard prints with `if (Serial)`.
- Use `MU_DrawCalibration(leds)` once to prove mapping (TL=G, TR=R, BL=B, BR=W).
- If colors are wrong on hardware, fix `COLOR_ORDER` in `lib/BoardConfig/BoardConfig.h` (Waveshare = `RGB`).
- If left/right swap on alternating rows, set `PANEL_WIRING_SERPENTINE` to `0` (progressive row‑major).

6) Quick Commands
- List ports: `python3 tools/led_matrix_viz.py --list-ports` or `lsusb | grep -i esp`
- Visualize: `python3 tools/led_matrix_viz.py -p /dev/ttyACM0 -b 115200 --stats`
- Raw monitor: `python3 tools/monitor_pong.py -p /dev/ttyACM0`
- Demo (no hardware): `python3 tools/led_matrix_viz.py --demo --width 8 --height 8`

7) Tips For The Next Dev (and agents)
- Start from the template; draw only via `MU_XY()`.
- Never hardcode edges; use `MATRIX_WIDTH/HEIGHT`.
- Limit debug frame rate (≈5–20 FPS) to keep serial stable.
- Update only `lib/BoardConfig/BoardConfig.h` for new panels/orientation; all games + tools follow.
- **Shared libs:** BoardConfig and MatrixUtil are at repo root (`lib/`), included as `<BoardConfig.h>` and `<MatrixUtil.h>`

Repo Highlights
- `lib/BoardConfig/` — board profile (geometry, color order, wiring/orientation, brightness) **SINGLE SOURCE OF TRUTH**
- `lib/MatrixUtil/` — mapping + serial frame helpers
- `tools/led_matrix_viz.py` — terminal visualizer (reads META to auto‑configure)
- `examples/RotatingDonut/` — reference PlatformIO project

DEBUGGING ISSUES
- **Flash size mismatch:** If you see "Detected size(4096k) smaller than (8192k)" on boot, use `board = adafruit_feather_esp32s3` in platformio.ini (NOT esp32-s3-devkitc-1)
- **Upload issues:** See "Upload Troubleshooting" section above for button sequences
- **Wrong device detected:** If `lsusb` shows "MicroPython" or "Pico" instead of "Espressif", that's the internal CM5 system. Replug the ESP32's USB cable.
- **Include errors:** Use `#include <BoardConfig.h>` and `#include <MatrixUtil.h>` (angle brackets, not quotes or paths)
- **Shared libs not found:** Add `lib_extra_dirs = ../../lib` to platformio.ini

Project Structure
```
esp32-agent-example/
├── lib/                      # SHARED LIBS (all games use these)
│   ├── BoardConfig/
│   │   ├── BoardConfig.h    # Single source of truth
│   │   └── library.json
│   └── MatrixUtil/
│       ├── MatrixUtil.h
│       └── library.json
├── examples/
│   ├── RotatingDonut/       # Example PlatformIO game
│   │   ├── platformio.ini
│   │   └── src/
│   │       └── main.cpp
│   └── MyGame/              # Your new game
│       ├── platformio.ini   # Copy from RotatingDonut
│       └── src/
│           └── main.cpp     # Your code here
└── tools/
    └── led_matrix_viz.py    # Terminal visualizer
```
