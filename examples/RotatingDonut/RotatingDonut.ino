#include <FastLED.h>
#include "../config/BoardConfig.h"
#include "../lib/MatrixUtil/MatrixUtil.h"

#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)
CRGB leds[NUM_LEDS];

// 3D donut parameters
float angleA = 0, angleB = 0;
const float R1 = 0.6;  // Smaller tube = bigger hole
const float R2 = 2.2;  // Overall donut radius  
const float K1 = 6;    // Screen distance (zoomed in)
const float K2 = 5;    // Viewer distance

// Z-buffer for depth
float zBuffer[NUM_LEDS];

void setup() {
  MU_ADD_LEDS(LED_PIN, leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS_LIMIT);
  FastLED.clear(); 
  FastLED.show();
}

void renderDonut() {
  // Clear screen and z-buffer
  FastLED.clear();
  for (int i = 0; i < NUM_LEDS; i++) {
    zBuffer[i] = 0;
  }

  // Precompute sines and cosines
  float cosA = cos(angleA), sinA = sin(angleA);
  float cosB = cos(angleB), sinB = sin(angleB);

  // Generate donut points
  for (float theta = 0; theta < 2 * PI; theta += 0.3) {
    float cosTheta = cos(theta), sinTheta = sin(theta);
    
    for (float phi = 0; phi < 2 * PI; phi += 0.2) {
      float cosPhi = cos(phi), sinPhi = sin(phi);
      
      // 3D coordinates of the point on the torus
      float circleX = R2 + R1 * cosTheta;
      float circleY = R1 * sinTheta;
      
      // Rotate around Y axis (angleB) and X axis (angleA)
      float x = circleX * (cosB * cosPhi + sinA * sinB * sinPhi) - circleY * cosA * sinB;
      float y = circleX * (sinB * cosPhi - sinA * cosB * sinPhi) + circleY * cosA * cosB;
      float z = K2 + cosA * circleX * sinPhi + circleY * sinA;
      float ooz = 1 / z;  // One over z
      
      // Project to 2D
      int xp = (int)(MATRIX_WIDTH / 2 + K1 * ooz * x);
      int yp = (int)(MATRIX_HEIGHT / 2 - K1 * ooz * y);
      
      // Check bounds
      if (xp >= 0 && xp < MATRIX_WIDTH && yp >= 0 && yp < MATRIX_HEIGHT) {
        int pixelIndex = MU_XY(xp, yp);
        
        // Only render if this point is closer than what's already there
        if (ooz > zBuffer[pixelIndex]) {
          zBuffer[pixelIndex] = ooz;
          
          // Calculate lighting based on surface normal
          float L = cosPhi * cosTheta * sinB - cosA * cosTheta * sinPhi - sinA * sinTheta + 
                   cosB * (cosA * sinTheta - cosTheta * sinA * sinPhi);
          
          if (L > 0) {
            // Simple grayscale depth - far = dark grey, close = white
            // This gives pure depth visualization
            
            // Normalize depth
            float depth = (ooz - 0.1) * 7;  
            depth = max(0.0f, min(1.0f, depth));
            
            // Apply lighting (surface angle affects brightness)
            float lit = L * 0.7 + 0.3;  // Never fully dark from lighting
            
            // Combine depth and lighting
            float brightness = depth * lit;
            
            // Map to grayscale: dark grey (far) to white (close)
            // Range from 15 (nearly black) to 255 (white)
            uint8_t intensity = 15 + (uint8_t)(brightness * 240);
            
            // Pure grayscale - all RGB channels the same
            leds[pixelIndex] = CRGB(intensity, intensity, intensity);
            
            // Optional: Add slight blue tint for style
            // leds[pixelIndex] = CRGB(
            //   intensity * 0.9,        // Slightly less red
            //   intensity * 0.95,       // Slightly less green  
            //   intensity               // Full blue for cool tint
            // );
          }
        }
      }
    }
  }
}

void loop() {
  renderDonut();
  FastLED.show();
  
  // Rotate the donut
  angleA += 0.07;
  angleB += 0.13;
  
  delay(20);  // 50 FPS
}