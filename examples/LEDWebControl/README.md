# LED Web Control

Web-based control panel for the 8×8 LED matrix. The ESP32 hosts its own WiFi
access point and a control page — no router or app required, just a browser.

## Features

- **Manual control**: click any LED to set its color, fill/clear the whole grid
- **Live sliders**: animation speed and brightness, applied instantly
- **Animation presets**: Rainbow, Spiral, Wave, Pulse, Fire, Twinkle, Chase, Matrix Rain
- **Snake game**: tilt the physical board to steer (onboard QMI8658 IMU), with
  on-screen arrow buttons as a fallback and a live score display
- Mobile-friendly layout — the matrix scales to fit narrow screens

## Requirements

- Board: Waveshare ESP32-S3-Matrix (or equivalent with an 8×8 WS2812B matrix on
  GPIO 14 and a QMI8658 IMU on I2C SDA=11/SCL=12)
- Arduino IDE with the ESP32 board package installed
- Libraries (Library Manager):
  - **FastLED**
  - **SensorLib** (by lewisxhe) — provides the QMI8658 IMU driver used by Snake

## Setup

1. Open `LEDWebControl.ino` in Arduino IDE (keep `WS_QMI8658.cpp`/`.h` in the
   same sketch folder — the IDE compiles them together automatically).
2. Select your board/port, then upload.
3. Open the Serial Monitor at 115200 baud to confirm the access point started
   and to see the assigned IP (should be `192.168.4.1`).
4. On your phone/laptop, connect to WiFi network **`ESP32-LED-Matrix`**
   (password `ledmatrix123`).
5. Visit `http://192.168.4.1` in a browser.

## Using It

- **Draw**: pick a color, click LEDs in the grid to light them individually.
- **Fill All / Clear All**: apply the picked color to every LED, or blank the grid.
- **Speed / Brightness sliders**: affect whichever animation is currently running.
- **Animation Presets**: tap a preset to start it; "Stop Animation" halts it.
- **Snake**: tap "Play Snake", then physically tilt the board to steer. Use the
  on-screen arrows if you're testing without tilting, or if no IMU is detected
  (the Serial Monitor and the status line under the game controls will say so).
  The snake wraps around the matrix edges; eating food increases speed and score.

## HTTP Endpoints

| Endpoint | Params | Description |
|---|---|---|
| `/setled` | `x,y,r,g,b` | Set a single LED |
| `/fillall` | `r,g,b` | Fill the whole matrix |
| `/clearall` | — | Clear matrix, stop any animation/game |
| `/setspeed` | `ms` | Set animation frame delay (20–2000ms) |
| `/setbrightness` | `level` | Set global brightness (0–255) |
| `/rainbow` `/spiral` `/wave` `/pulse` `/fire` `/twinkle` `/chase` `/matrixrain` | — | Start that animation |
| `/stop` | — | Stop the current animation |
| `/snake/start` | — | Start/restart Snake |
| `/snake/stop` | — | Stop Snake, clear matrix |
| `/snake/dir` | `d` (0=up,1=right,2=down,3=left) | Manually set snake direction |
| `/snake/state` | — | JSON: `{active, gameOver, score, imu}` |

## Troubleshooting

| Symptom | Fix |
|---|---|
| Serial says "QMI8658 IMU not found" | Snake still works via the on-screen arrows; check I2C wiring (SDA=11, SCL=12) if you expect tilt control |
| Can't reach `192.168.4.1` | Confirm you're connected to the `ESP32-LED-Matrix` WiFi network, not your home network |
| Colors look wrong | Adjust the `RGB` color order in the `FastLED.addLeds<WS2812B, LED_PIN, RGB>` line |
| Grid still cramped on a very small screen | Zoom out in the browser, or lower the `--cell-size` cap in the `<style>` block |
