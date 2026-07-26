// Requires the "SensorLib" library (by lewisxhe) for the QMI8658 IMU used by
// the Snake game's tilt control - install via Arduino IDE Library Manager.
#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include "WS_QMI8658.h"

#define MATRIX_WIDTH 8
#define MATRIX_HEIGHT 8
#define NUM_LEDS 64
#define LED_PIN 14

// WiFi Access Point credentials
const char* ap_ssid = "ESP32-LED-Matrix";
const char* ap_password = "ledmatrix123";

CRGB leds[NUM_LEDS];
WebServer server(80);

// Animation variables
unsigned long lastAnimationUpdate = 0;
int animationSpeed = 100; // ms between frames
uint8_t animationHue = 0;
int animationStep = 0;
bool animationRunning = false;
String currentAnimation = "none";

// Fire effect state (per-column heat)
uint8_t heat[MATRIX_WIDTH][MATRIX_HEIGHT];
// Matrix rain effect state (per-column drop row, -1 = no active drop)
int8_t rainDrop[MATRIX_WIDTH];

// Snake game state
#define MAX_SNAKE_LENGTH NUM_LEDS
struct SnakePoint { int8_t x; int8_t y; };
SnakePoint snakeBody[MAX_SNAKE_LENGTH];
uint8_t snakeLength = 3;
SnakePoint snakeFood;
bool snakeModeActive = false;
bool snakeGameOver = false;
uint8_t snakeDirection = 1; // 0=up, 1=right, 2=down, 3=left
unsigned long lastSnakeMove = 0;
unsigned long snakeMoveInterval = 300;
uint16_t snakeScore = 0;
bool imuReady = false;

// Tilt accumulation (mirrors the standalone Snake example's algorithm)
float tiltXA = 0, tiltXB = 0, tiltYA = 0, tiltYB = 0;

uint16_t XY(uint8_t x, uint8_t y) {
  if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT) return NUM_LEDS;

  uint16_t i;
  if (y & 0x01) {
    uint8_t reverseX = (MATRIX_WIDTH - 1) - x;
    i = (y * MATRIX_WIDTH) + reverseX;
  } else {
    i = (y * MATRIX_WIDTH) + x;
  }
  return i;
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 LED Matrix Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    * { box-sizing: border-box; }
    :root {
      --cell-size: min(48px, (100vw - 64px) / 8);
    }
    body {
      font-family: Arial, sans-serif;
      background: #1a1a1a;
      color: #fff;
      margin: 0;
      padding: clamp(8px, 4vw, 20px);
      display: flex;
      flex-direction: column;
      align-items: center;
      overflow-x: hidden;
    }
    h1 { margin-bottom: 10px; font-size: clamp(1.1rem, 5vw, 1.6rem); text-align: center; }
    .wifi-status {
      background: #333;
      padding: 8px 16px;
      border-radius: 5px;
      font-size: 12px;
      margin-bottom: 20px;
    }
    .matrix-container {
      display: grid;
      grid-template-columns: repeat(8, var(--cell-size));
      grid-gap: clamp(2px, 1vw, 4px);
      margin-bottom: 20px;
      padding: clamp(8px, 3vw, 20px);
      background: #2a2a2a;
      border-radius: 10px;
      max-width: 100%;
    }
    .led {
      width: var(--cell-size);
      height: var(--cell-size);
      border: 2px solid #444;
      border-radius: 5px;
      cursor: pointer;
      transition: transform 0.1s;
    }
    .led:hover {
      transform: scale(1.1);
      border-color: #fff;
    }
    .controls {
      background: #2a2a2a;
      padding: clamp(10px, 3vw, 20px);
      border-radius: 10px;
      display: flex;
      flex-direction: column;
      gap: 15px;
      max-width: 450px;
      width: 100%;
    }
    .control-row {
      display: flex;
      align-items: center;
      gap: 10px;
      flex-wrap: wrap;
    }
    label {
      font-weight: bold;
      min-width: 100px;
    }
    input[type="color"] {
      width: 60px;
      height: 40px;
      border: none;
      border-radius: 5px;
      cursor: pointer;
    }
    input[type="range"] {
      flex: 1;
      min-width: 80px;
    }
    .slider-value {
      min-width: 42px;
      text-align: right;
      font-variant-numeric: tabular-nums;
    }
    button {
      padding: 10px 20px;
      background: #4CAF50;
      color: white;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      font-size: 16px;
      transition: background 0.3s;
      flex: 1 1 auto;
    }
    button:hover {
      background: #45a049;
    }
    .clear-btn {
      background: #f44336;
    }
    .clear-btn:hover {
      background: #da190b;
    }
    .info {
      margin-top: 10px;
      padding: 10px;
      background: #333;
      border-radius: 5px;
      font-size: 14px;
    }
    .section-title {
      margin: 0 0 15px 0;
      text-align: center;
      color: #fff;
    }
    .divider {
      border-top: 2px solid #444;
      margin: 15px 0;
      padding-top: 15px;
    }
    .dpad {
      display: grid;
      grid-template-columns: repeat(3, 60px);
      grid-template-rows: repeat(2, 50px);
      grid-template-areas:
        ".  up   ."
        "left . right";
      gap: 6px;
      justify-content: center;
      margin: 10px 0;
    }
    .dpad button { flex: none; padding: 0; }
    .dpad .up { grid-area: up; }
    .dpad .left { grid-area: left; }
    .dpad .right { grid-area: right; }
    #snakeStatus { text-align: center; font-size: 14px; }
    @media (max-width: 400px) {
      button { padding: 8px 10px; font-size: 13px; }
      label { min-width: 80px; }
    }
  </style>
</head>
<body>
  <h1>&#127752; ESP32 LED Matrix Control</h1>
  <div class="wifi-status">AP Mode: ESP32-LED-Matrix</div>

  <div class="matrix-container" id="matrix"></div>

  <div class="controls">
    <div class="control-row">
      <label>Color:</label>
      <input type="color" id="colorPicker" value="#ff0000">
    </div>

    <div class="control-row">
      <button onclick="fillAll()">Fill All</button>
      <button class="clear-btn" onclick="clearAll()">Clear All</button>
    </div>

    <div class="control-row">
      <label>Speed:</label>
      <input type="range" id="speedSlider" min="20" max="500" step="10" value="100">
      <span class="slider-value" id="speedValue">100ms</span>
    </div>

    <div class="control-row">
      <label>Brightness:</label>
      <input type="range" id="brightnessSlider" min="10" max="255" step="5" value="100">
      <span class="slider-value" id="brightnessValue">100</span>
    </div>

    <div class="divider">
      <h3 class="section-title">Animation Presets</h3>

      <div class="control-row">
        <button onclick="startAnimation('rainbow')" style="background: linear-gradient(45deg, #ff0000, #ff7f00, #ffff00, #00ff00, #0000ff, #4b0082, #9400d3);">&#127752; Rainbow</button>
        <button onclick="startAnimation('spiral')" style="background: #8A2BE2;">&#127760; Spiral</button>
      </div>

      <div class="control-row">
        <button onclick="startAnimation('wave')" style="background: #1E90FF;">&#127754; Wave</button>
        <button onclick="startAnimation('pulse')" style="background: #FF69B4;">&#128147; Pulse</button>
      </div>

      <div class="control-row">
        <button onclick="startAnimation('fire')" style="background: #FF4500;">&#128293; Fire</button>
        <button onclick="startAnimation('twinkle')" style="background: #FFD700; color:#222;">&#10024; Twinkle</button>
      </div>

      <div class="control-row">
        <button onclick="startAnimation('chase')" style="background: #00CED1; color:#222;">&#128163; Chase</button>
        <button onclick="startAnimation('matrixrain')" style="background: #003300;">&#128421;&#65039; Matrix Rain</button>
      </div>

      <div class="control-row">
        <button onclick="stopAnimation()" class="clear-btn">&#9209;&#65039; Stop Animation</button>
      </div>
    </div>

    <div class="divider">
      <h3 class="section-title">&#128034; Snake (tilt the board to steer)</h3>

      <div class="control-row">
        <button onclick="startSnake()" style="background: #2E8B57;">&#9654;&#65039; Play Snake</button>
        <button onclick="stopSnake()" class="clear-btn">&#9209;&#65039; Stop Game</button>
      </div>

      <div class="dpad">
        <button class="up" onclick="snakeDir(0)">&#11014;&#65039;</button>
        <button class="left" onclick="snakeDir(3)">&#11013;&#65039;</button>
        <button class="right" onclick="snakeDir(1)">&#10145;&#65039;</button>
      </div>
      <div class="control-row" style="justify-content:center;">
        <button style="flex:none; width:60px;" onclick="snakeDir(2)">&#11015;&#65039;</button>
      </div>

      <div id="snakeStatus">Not running</div>
    </div>

    <div class="info">
      Click any LED to set its color, or use the presets and game above
    </div>
  </div>

  <script>
    const matrix = document.getElementById('matrix');
    const colorPicker = document.getElementById('colorPicker');
    const speedSlider = document.getElementById('speedSlider');
    const speedValue = document.getElementById('speedValue');
    const brightnessSlider = document.getElementById('brightnessSlider');
    const brightnessValue = document.getElementById('brightnessValue');
    const snakeStatus = document.getElementById('snakeStatus');
    let snakePollTimer = null;

    // Create 8x8 grid
    for (let y = 0; y < 8; y++) {
      for (let x = 0; x < 8; x++) {
        const led = document.createElement('div');
        led.className = 'led';
        led.style.backgroundColor = '#000';
        led.dataset.x = x;
        led.dataset.y = y;
        led.onclick = () => setLED(x, y, colorPicker.value);
        matrix.appendChild(led);
      }
    }

    function hexToRgb(hex) {
      const r = parseInt(hex.slice(1, 3), 16);
      const g = parseInt(hex.slice(3, 5), 16);
      const b = parseInt(hex.slice(5, 7), 16);
      return {r, g, b};
    }

    function setLED(x, y, color) {
      const rgb = hexToRgb(color);
      fetch(`/setled?x=${x}&y=${y}&r=${rgb.r}&g=${rgb.g}&b=${rgb.b}`)
        .then(response => response.text())
        .then(() => {
          const led = document.querySelector(`[data-x="${x}"][data-y="${y}"]`);
          led.style.backgroundColor = color;
        });
    }

    function fillAll() {
      const color = colorPicker.value;
      const rgb = hexToRgb(color);
      fetch(`/fillall?r=${rgb.r}&g=${rgb.g}&b=${rgb.b}`)
        .then(() => {
          document.querySelectorAll('.led').forEach(led => {
            led.style.backgroundColor = color;
          });
        });
    }

    function clearAll() {
      fetch('/clearall')
        .then(() => {
          document.querySelectorAll('.led').forEach(led => {
            led.style.backgroundColor = '#000';
          });
        });
    }

    function startAnimation(type) {
      stopSnakePolling();
      fetch(`/${type}`)
        .then(response => response.text())
        .then(data => {
          console.log(`Started ${type} animation:`, data);
        })
        .catch(error => {
          console.error('Error starting animation:', error);
        });
    }

    function stopAnimation() {
      fetch('/stop')
        .then(response => response.text())
        .then(data => {
          console.log('Stopped animation:', data);
        })
        .catch(error => {
          console.error('Error stopping animation:', error);
        });
    }

    let speedDebounce = null;
    speedSlider.oninput = () => {
      speedValue.textContent = `${speedSlider.value}ms`;
      clearTimeout(speedDebounce);
      speedDebounce = setTimeout(() => {
        fetch(`/setspeed?ms=${speedSlider.value}`);
      }, 100);
    };

    let brightnessDebounce = null;
    brightnessSlider.oninput = () => {
      brightnessValue.textContent = brightnessSlider.value;
      clearTimeout(brightnessDebounce);
      brightnessDebounce = setTimeout(() => {
        fetch(`/setbrightness?level=${brightnessSlider.value}`);
      }, 100);
    };

    function startSnake() {
      fetch('/snake/start')
        .then(response => response.text())
        .then(() => startSnakePolling())
        .catch(error => console.error('Error starting snake:', error));
    }

    function stopSnake() {
      stopSnakePolling();
      fetch('/snake/stop')
        .then(() => { snakeStatus.textContent = 'Not running'; })
        .catch(error => console.error('Error stopping snake:', error));
    }

    function snakeDir(d) {
      fetch(`/snake/dir?d=${d}`).catch(error => console.error('Error setting direction:', error));
    }

    function startSnakePolling() {
      stopSnakePolling();
      snakePollTimer = setInterval(pollSnakeState, 300);
      pollSnakeState();
    }

    function stopSnakePolling() {
      if (snakePollTimer) {
        clearInterval(snakePollTimer);
        snakePollTimer = null;
      }
    }

    function pollSnakeState() {
      fetch('/snake/state')
        .then(response => response.json())
        .then(state => {
          if (!state.active) {
            snakeStatus.textContent = 'Not running';
            stopSnakePolling();
            return;
          }
          let text = `Score: ${state.score}`;
          if (!state.imu) text += ' (no IMU found - use arrows)';
          if (state.gameOver) text += ' - Game Over, restarting...';
          snakeStatus.textContent = text;
        })
        .catch(error => console.error('Error polling snake state:', error));
    }
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handleSetLED() {
  if (server.hasArg("x") && server.hasArg("y") &&
      server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) {

    int x = server.arg("x").toInt();
    int y = server.arg("y").toInt();
    int r = server.arg("r").toInt();
    int g = server.arg("g").toInt();
    int b = server.arg("b").toInt();

    if (x >= 0 && x < MATRIX_WIDTH && y >= 0 && y < MATRIX_HEIGHT) {
      leds[XY(x, y)] = CRGB(r, g, b);
      FastLED.show();
      server.send(200, "text/plain", "OK");
      return;
    }
  }
  server.send(400, "text/plain", "Invalid parameters");
}

void handleFillAll() {
  if (server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) {
    int r = server.arg("r").toInt();
    int g = server.arg("g").toInt();
    int b = server.arg("b").toInt();

    fill_solid(leds, NUM_LEDS, CRGB(r, g, b));
    FastLED.show();
    server.send(200, "text/plain", "OK");
    return;
  }
  server.send(400, "text/plain", "Invalid parameters");
}

void handleClearAll() {
  animationRunning = false;
  snakeModeActive = false;
  currentAnimation = "none";
  FastLED.clear();
  FastLED.show();
  server.send(200, "text/plain", "OK");
}

void handleSetSpeed() {
  if (server.hasArg("ms")) {
    animationSpeed = constrain(server.arg("ms").toInt(), 20, 2000);
    server.send(200, "text/plain", "OK");
    return;
  }
  server.send(400, "text/plain", "Invalid parameters");
}

void handleSetBrightness() {
  if (server.hasArg("level")) {
    FastLED.setBrightness(constrain(server.arg("level").toInt(), 0, 255));
    FastLED.show();
    server.send(200, "text/plain", "OK");
    return;
  }
  server.send(400, "text/plain", "Invalid parameters");
}

// Animation Functions
void drawRainbow() {
  for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
      uint8_t hue = animationHue + (x * 32) + (y * 16);
      leds[XY(x, y)] = CHSV(hue, 255, 200);
    }
  }
  animationHue += 2;
}

void drawSpiral() {
  FastLED.clear();
  int centerX = MATRIX_WIDTH / 2;
  int centerY = MATRIX_HEIGHT / 2;
  float angle = (animationStep * 0.2) * (PI / 180.0);

  for (int r = 0; r < 4; r++) {
    float spiralAngle = angle + (r * PI / 2);
    int x = centerX + (r * cos(spiralAngle));
    int y = centerY + (r * sin(spiralAngle));

    if (x >= 0 && x < MATRIX_WIDTH && y >= 0 && y < MATRIX_HEIGHT) {
      uint8_t hue = animationHue + (r * 64);
      leds[XY(x, y)] = CHSV(hue, 255, 255);
    }
  }
  animationStep += 5;
  animationHue += 1;
}

void drawWave() {
  FastLED.clear();
  for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
    float wave = sin((x + animationStep) * 0.5) * 2 + 3;
    int y = constrain(wave, 0, MATRIX_HEIGHT - 1);

    uint8_t hue = animationHue + (x * 32);
    leds[XY(x, y)] = CHSV(hue, 255, 255);

    // Add trail effect
    if (y > 0) leds[XY(x, y-1)] = CHSV(hue, 255, 100);
    if (y < MATRIX_HEIGHT-1) leds[XY(x, y+1)] = CHSV(hue, 255, 100);
  }
  animationStep += 1;
  animationHue += 2;
}

void drawPulse() {
  FastLED.clear();
  int centerX = MATRIX_WIDTH / 2;
  int centerY = MATRIX_HEIGHT / 2;
  float pulseRadius = (sin(animationStep * 0.1) + 1) * 3;

  for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
      float distance = sqrt(pow(x - centerX, 2) + pow(y - centerY, 2));

      if (abs(distance - pulseRadius) < 1.0) {
        uint8_t brightness = 255 - (abs(distance - pulseRadius) * 255);
        leds[XY(x, y)] = CHSV(animationHue, 255, brightness);
      }
    }
  }
  animationStep += 1;
  animationHue += 1;
}

void drawFire() {
  // Cool down every cell a little
  for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
    for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
      heat[x][y] = qsub8(heat[x][y], random8(0, 50));
    }
  }

  // Heat rises: propagate upward (y=0 is the top row, fire climbs from the bottom)
  for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
    for (uint8_t y = 0; y < MATRIX_HEIGHT - 2; y++) {
      heat[x][y] = (heat[x][y+1] + heat[x][y+2] + heat[x][y+2]) / 3;
    }
  }

  // Ignite new sparks at the bottom row
  for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
    if (random8() < 120) {
      heat[x][MATRIX_HEIGHT-1] = qadd8(heat[x][MATRIX_HEIGHT-1], random8(160, 255));
    }
  }

  // Map heat to colors
  for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
    for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
      leds[XY(x, y)] = HeatColor(heat[x][y]);
    }
  }
}

void drawTwinkle() {
  fadeToBlackBy(leds, NUM_LEDS, 20);
  if (random8() < 80) {
    leds[random16(NUM_LEDS)] = CHSV(animationHue + random8(64), 200, 255);
  }
  animationHue += 1;
}

void drawChase() {
  fadeToBlackBy(leds, NUM_LEDS, 60);
  uint16_t pos = animationStep % NUM_LEDS;
  leds[pos] = CHSV(animationHue, 255, 255);
  animationStep += 1;
  animationHue += 3;
}

void drawMatrixRain() {
  fadeToBlackBy(leds, NUM_LEDS, 40);
  for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
    if (rainDrop[x] < 0) {
      if (random8() < 15) rainDrop[x] = 0;
    } else {
      leds[XY(x, rainDrop[x])] = CRGB(0, 255, 70);
      rainDrop[x]++;
      if (rainDrop[x] >= MATRIX_HEIGHT) rainDrop[x] = -1;
    }
  }
}

// Animation handlers
void startAnimationCommon(const char* name) {
  snakeModeActive = false;
  animationRunning = true;
  currentAnimation = name;
  animationStep = 0;
}

void handleRainbow() {
  startAnimationCommon("rainbow");
  server.send(200, "text/plain", "Rainbow animation started");
}

void handleSpiral() {
  startAnimationCommon("spiral");
  server.send(200, "text/plain", "Spiral animation started");
}

void handleWave() {
  startAnimationCommon("wave");
  server.send(200, "text/plain", "Wave animation started");
}

void handlePulse() {
  startAnimationCommon("pulse");
  server.send(200, "text/plain", "Pulse animation started");
}

void handleFire() {
  memset(heat, 0, sizeof(heat));
  startAnimationCommon("fire");
  server.send(200, "text/plain", "Fire animation started");
}

void handleTwinkle() {
  startAnimationCommon("twinkle");
  server.send(200, "text/plain", "Twinkle animation started");
}

void handleChase() {
  startAnimationCommon("chase");
  server.send(200, "text/plain", "Chase animation started");
}

void handleMatrixRain() {
  for (uint8_t x = 0; x < MATRIX_WIDTH; x++) rainDrop[x] = -1;
  startAnimationCommon("matrixrain");
  server.send(200, "text/plain", "Matrix rain animation started");
}

void handleStopAnimation() {
  animationRunning = false;
  currentAnimation = "none";
  server.send(200, "text/plain", "Animation stopped");
}

// Snake game
void snakeGenerateFood() {
  bool validPosition = false;
  while (!validPosition) {
    snakeFood.x = random(0, MATRIX_WIDTH);
    snakeFood.y = random(0, MATRIX_HEIGHT);

    validPosition = true;
    for (uint8_t i = 0; i < snakeLength; i++) {
      if (snakeBody[i].x == snakeFood.x && snakeBody[i].y == snakeFood.y) {
        validPosition = false;
        break;
      }
    }
  }
}

void snakeReset() {
  snakeLength = 3;
  snakeBody[0] = {4, 4};
  snakeBody[1] = {3, 4};
  snakeBody[2] = {2, 4};
  snakeDirection = 1;
  snakeMoveInterval = 300;
  snakeScore = 0;
  tiltXA = tiltXB = tiltYA = tiltYB = 0;
  snakeGenerateFood();
  snakeGameOver = false;
}

// Applies a direction change, ignoring reversals into the snake's own body
void snakeTrySetDirection(uint8_t newDirection) {
  if (snakeLength > 1 &&
      ((snakeDirection == 0 && newDirection == 2) ||
       (snakeDirection == 2 && newDirection == 0) ||
       (snakeDirection == 1 && newDirection == 3) ||
       (snakeDirection == 3 && newDirection == 1))) {
    return;
  }
  snakeDirection = newDirection;
}

// Returns: 0 = game over, 1 = normal move, 2 = food eaten
uint8_t snakeMove(uint8_t direction) {
  SnakePoint newHead = snakeBody[0];

  switch (direction) {
    case 0: newHead.y--; break; // Up
    case 1: newHead.x++; break; // Right
    case 2: newHead.y++; break; // Down
    case 3: newHead.x--; break; // Left
  }

  // Wrap around edges
  if (newHead.x < 0) newHead.x = MATRIX_WIDTH - 1;
  if (newHead.x >= MATRIX_WIDTH) newHead.x = 0;
  if (newHead.y < 0) newHead.y = MATRIX_HEIGHT - 1;
  if (newHead.y >= MATRIX_HEIGHT) newHead.y = 0;

  // Ignore a move that would immediately reverse into the neck
  if (snakeLength > 1 && snakeBody[1].x == newHead.x && snakeBody[1].y == newHead.y) {
    return 1;
  }

  // Self collision
  for (uint8_t i = 2; i < snakeLength; i++) {
    if (snakeBody[i].x == newHead.x && snakeBody[i].y == newHead.y) {
      return 0;
    }
  }

  bool foodEaten = (newHead.x == snakeFood.x && newHead.y == snakeFood.y);

  if (!foodEaten) {
    for (int8_t i = snakeLength - 1; i > 0; i--) {
      snakeBody[i] = snakeBody[i - 1];
    }
  } else if (snakeLength < MAX_SNAKE_LENGTH) {
    for (int8_t i = snakeLength; i > 0; i--) {
      snakeBody[i] = snakeBody[i - 1];
    }
    snakeLength++;
    snakeGenerateFood();
  }

  snakeBody[0] = newHead;
  return foodEaten ? 2 : 1;
}

void snakeUpdateDisplay() {
  FastLED.clear();

  for (uint8_t i = 1; i < snakeLength; i++) {
    leds[XY(snakeBody[i].x, snakeBody[i].y)] = CRGB(0, 100, 0);
  }
  leds[XY(snakeBody[0].x, snakeBody[0].y)] = CRGB(0, 200, 0);
  leds[XY(snakeFood.x, snakeFood.y)] = CRGB(160, 0, 0);

  FastLED.show();
}

void snakeGameOverFlash() {
  for (uint8_t flash = 0; flash < 3; flash++) {
    fill_solid(leds, NUM_LEDS, CRGB(120, 0, 0));
    FastLED.show();
    delay(200);
    FastLED.clear();
    FastLED.show();
    delay(200);
  }
}

void snakeUpdateTiltDirection() {
  if (Accel.x > 0.15 || Accel.x < 0 || Accel.y > 0.15 || Accel.y < 0) {
    if (Accel.x > 0.15) { tiltXA += Accel.x * 10; tiltXB = 0; }
    else if (Accel.x < 0) { tiltXB += fabs(Accel.x) * 10; tiltXA = 0; }
    else { tiltXA = 0; tiltXB = 0; }

    if (Accel.y > 0.15) { tiltYA += Accel.y * 10; tiltYB = 0; }
    else if (Accel.y < 0) { tiltYB += fabs(Accel.y) * 10; tiltYA = 0; }
    else { tiltYA = 0; tiltYB = 0; }

    uint8_t newDirection = snakeDirection;
    if (tiltXA >= 10) { newDirection = 2; tiltXA = 0; tiltXB = 0; }
    if (tiltXB >= 10) { newDirection = 0; tiltXA = 0; tiltXB = 0; }
    if (tiltYA >= 10) { newDirection = 3; tiltYA = 0; tiltYB = 0; }
    if (tiltYB >= 10) { newDirection = 1; tiltYA = 0; tiltYB = 0; }

    snakeTrySetDirection(newDirection);
  }
}

void handleSnakeStart() {
  animationRunning = false;
  currentAnimation = "none";
  snakeReset();
  snakeModeActive = true;
  lastSnakeMove = millis();
  server.send(200, "text/plain", "Snake started");
}

void handleSnakeStop() {
  snakeModeActive = false;
  FastLED.clear();
  FastLED.show();
  server.send(200, "text/plain", "Snake stopped");
}

void handleSnakeDir() {
  if (server.hasArg("d")) {
    int d = server.arg("d").toInt();
    if (d >= 0 && d <= 3) {
      snakeTrySetDirection((uint8_t)d);
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleSnakeState() {
  String json = "{";
  json += "\"active\":"; json += (snakeModeActive ? "true" : "false"); json += ",";
  json += "\"gameOver\":"; json += (snakeGameOver ? "true" : "false"); json += ",";
  json += "\"score\":"; json += String(snakeScore); json += ",";
  json += "\"imu\":"; json += (imuReady ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\nESP32 LED Matrix Web Control");
  Serial.println("=============================");

  // Initialize LEDs
  FastLED.addLeds<WS2812B, LED_PIN, RGB>(leds, NUM_LEDS);
  FastLED.setBrightness(100);
  FastLED.clear();
  FastLED.show();
  Serial.println("LEDs initialized");

  for (uint8_t x = 0; x < MATRIX_WIDTH; x++) rainDrop[x] = -1;

  // Initialize IMU for Snake tilt control (non-blocking - falls back to on-screen arrows)
  imuReady = QMI8658_Init();

  // Create WiFi Access Point
  Serial.println("Creating WiFi Access Point...");

  WiFi.mode(WIFI_AP);
  delay(100);

  bool ap_started = WiFi.softAP(ap_ssid, ap_password);

  if (ap_started) {
    Serial.println("Access Point created successfully!");
    Serial.print("  SSID: ");
    Serial.println(ap_ssid);
    Serial.print("  Password: ");
    Serial.println(ap_password);
    Serial.print("  IP address: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("\n>>> Connect to WiFi and go to http://192.168.4.1 <<<\n");
  } else {
    Serial.println("Failed to create Access Point!");
  }

  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/setled", handleSetLED);
  server.on("/fillall", handleFillAll);
  server.on("/clearall", handleClearAll);
  server.on("/setspeed", handleSetSpeed);
  server.on("/setbrightness", handleSetBrightness);
  server.on("/rainbow", handleRainbow);
  server.on("/spiral", handleSpiral);
  server.on("/wave", handleWave);
  server.on("/pulse", handlePulse);
  server.on("/fire", handleFire);
  server.on("/twinkle", handleTwinkle);
  server.on("/chase", handleChase);
  server.on("/matrixrain", handleMatrixRain);
  server.on("/stop", handleStopAnimation);
  server.on("/snake/start", handleSnakeStart);
  server.on("/snake/stop", handleSnakeStop);
  server.on("/snake/dir", handleSnakeDir);
  server.on("/snake/state", handleSnakeState);

  server.begin();
  Serial.println("Web server started");
  Serial.println("=============================\n");

  // Start default animation (rainbow)
  Serial.println("Starting default rainbow animation...");
  startAnimationCommon("rainbow");
}

void loop() {
  server.handleClient();

  if (snakeModeActive) {
    if (imuReady) {
      QMI8658_Loop();
      snakeUpdateTiltDirection();
    }

    if (millis() - lastSnakeMove >= snakeMoveInterval) {
      lastSnakeMove = millis();
      uint8_t status = snakeMove(snakeDirection);

      if (status == 0) {
        snakeGameOver = true;
        snakeGameOverFlash();
        snakeReset();
      } else if (status == 2) {
        snakeScore++;
        snakeMoveInterval = max(100UL, 300UL - (unsigned long)snakeScore * 10UL);
      }

      snakeUpdateDisplay();
    }
    return;
  }

  // Handle ambient animations
  if (animationRunning && millis() - lastAnimationUpdate > animationSpeed) {
    lastAnimationUpdate = millis();

    if (currentAnimation == "rainbow") {
      drawRainbow();
    } else if (currentAnimation == "spiral") {
      drawSpiral();
    } else if (currentAnimation == "wave") {
      drawWave();
    } else if (currentAnimation == "pulse") {
      drawPulse();
    } else if (currentAnimation == "fire") {
      drawFire();
    } else if (currentAnimation == "twinkle") {
      drawTwinkle();
    } else if (currentAnimation == "chase") {
      drawChase();
    } else if (currentAnimation == "matrixrain") {
      drawMatrixRain();
    }

    FastLED.show();
  }
}
