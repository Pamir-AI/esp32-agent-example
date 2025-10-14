// Falling Stars - Colorful particles with gravity and collision
#include <FastLED.h>
#include <BoardConfig.h>
#include <MatrixUtil.h>

#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)
#define MAX_PARTICLES 12

CRGB leds[NUM_LEDS];

struct Particle {
  float x, y;
  float vy;
  CRGB color;
  bool active;
};

Particle particles[MAX_PARTICLES];
uint8_t grid[MATRIX_WIDTH][MATRIX_HEIGHT];

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 1500) { delay(10); }
  if (Serial) MU_PrintMeta();

  MU_ADD_LEDS(LED_PIN, leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS_LIMIT);
  FastLED.clear(); FastLED.show();

  // Initialize particles
  for (int i = 0; i < MAX_PARTICLES; i++) {
    particles[i].active = false;
  }

  // Clear grid
  for (int x = 0; x < MATRIX_WIDTH; x++) {
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
      grid[x][y] = 0;
    }
  }
}

void spawnParticle() {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!particles[i].active) {
      particles[i].x = random(0, MATRIX_WIDTH);
      particles[i].y = 0;
      particles[i].vy = 0;
      particles[i].active = true;

      // Random vibrant colors
      uint8_t colorChoice = random(0, 6);
      switch(colorChoice) {
        case 0: particles[i].color = CRGB(255, 0, 0); break;   // Red
        case 1: particles[i].color = CRGB(0, 255, 0); break;   // Green
        case 2: particles[i].color = CRGB(0, 0, 255); break;   // Blue
        case 3: particles[i].color = CRGB(255, 255, 0); break; // Yellow
        case 4: particles[i].color = CRGB(255, 0, 255); break; // Magenta
        case 5: particles[i].color = CRGB(0, 255, 255); break; // Cyan
      }
      break;
    }
  }
}

void loop() {
  // Spawn new particle randomly
  if (random(0, 100) < 30) {
    spawnParticle();
  }

  // Update physics
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles[i].active) {
      particles[i].vy += 0.3; // Gravity
      particles[i].y += particles[i].vy;

      int ix = (int)particles[i].x;
      int iy = (int)particles[i].y;

      // Check collision with bottom or other particles
      if (iy >= MATRIX_HEIGHT - 1) {
        grid[ix][MATRIX_HEIGHT - 1] = i + 1;
        particles[i].active = false;
      } else if (iy >= 0 && iy < MATRIX_HEIGHT - 1) {
        if (grid[ix][iy + 1] != 0) {
          grid[ix][iy] = i + 1;
          particles[i].active = false;
        }
      }
    }
  }

  // Render
  FastLED.clear();

  // Draw settled particles
  for (int x = 0; x < MATRIX_WIDTH; x++) {
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
      if (grid[x][y] != 0) {
        uint8_t idx = grid[x][y] - 1;
        leds[MU_XY(x, y)] = particles[idx].color;
      }
    }
  }

  // Draw falling particles
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles[i].active) {
      int ix = (int)particles[i].x;
      int iy = (int)particles[i].y;
      if (ix >= 0 && ix < MATRIX_WIDTH && iy >= 0 && iy < MATRIX_HEIGHT) {
        leds[MU_XY(ix, iy)] = particles[i].color;
      }
    }
  }

  FastLED.show();
  if (Serial) MU_SendFrameCSV(leds);
  delay(50);
}
