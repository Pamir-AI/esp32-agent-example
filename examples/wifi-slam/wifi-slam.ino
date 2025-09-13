#include <WiFi.h>
#include <FastLED.h>

// LED Matrix Configuration
#define LED_PIN     14      // Data pin for WS2812B matrix (GPIO 14 for Waveshare ESP32-S3-Matrix)
#define NUM_LEDS    64      // 8x8 matrix
#define MATRIX_WIDTH 8
#define MATRIX_HEIGHT 8
#define BRIGHTNESS  60      // Global brightness (0-255)

// WiFi Configuration
const char* TARGET_SSID = "HIDER";

// RSSI Configuration
#define RSSI_MIN    -80    // Weak signal (blue)
#define RSSI_MAX    -40    // Strong signal (red)
#define EMA_ALPHA   0.1    // Exponential moving average alpha (0-1) - lower = more smoothing

// Timing
#define SCAN_INTERVAL_MS  100    // Delay between scans
#define STATUS_BLINK_MS   500    // Blink interval for status colors

// State Machine
enum SystemState {
  STATE_DISCOVERY,      // Initial discovery scan
  STATE_SCANNING,       // Looking for network
  STATE_LOCKED,         // Network found and tracking
  STATE_LOST           // Network was found but now lost
};

// Global Variables
CRGB leds[NUM_LEDS];
SystemState currentState = STATE_DISCOVERY;
uint8_t targetBSSID[6] = {0};
uint8_t targetChannel = 0;
bool hasTarget = false;
float rssiEMA = RSSI_MIN;
bool firstReading = true;
unsigned long lastBlinkTime = 0;
bool blinkState = false;
int lostCounter = 0;

// Median filter for RSSI (to remove spikes)
#define MEDIAN_SAMPLES 5
int rssiBuffer[MEDIAN_SAMPLES];
int bufferIndex = 0;
bool bufferFull = false;

// Function to convert XY to LED index for serpentine wiring
uint16_t XY(uint8_t x, uint8_t y) {
  if (y & 0x01) {
    // Odd rows run backwards
    return (y * MATRIX_WIDTH) + (MATRIX_WIDTH - 1 - x);
  } else {
    // Even rows run forwards
    return (y * MATRIX_WIDTH) + x;
  }
}

// Fill entire matrix with single color
void fillMatrix(uint8_t r, uint8_t g, uint8_t b) {
  CRGB color = CRGB(r, g, b);
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = color;
  }
  FastLED.show();
}

// Convert RSSI to RGB color using full rainbow spectrum
void rssiToColor(float rssi, uint8_t* r, uint8_t* g, uint8_t* b) {
  // Normalize RSSI to 0-1 range
  float t = (rssi - RSSI_MIN) / float(RSSI_MAX - RSSI_MIN);
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  
  // Map to hue (0-240 degrees for blue to red through green/yellow/orange)
  // Blue (weak) = 240°, Cyan = 180°, Green = 120°, Yellow = 60°, Red (strong) = 0°
  float hue = (1.0 - t) * 240.0;  // Reverse so strong signal = red
  
  // Convert HSV to RGB (simplified for full saturation and value)
  float h = hue / 60.0;
  int hi = int(h) % 6;
  float f = h - int(h);
  
  uint8_t v = 255;
  uint8_t p = 0;
  uint8_t q = uint8_t(255 * (1 - f));
  uint8_t u = uint8_t(255 * f);
  
  switch(hi) {
    case 0: *r = v; *g = u; *b = p; break;  // Red to Yellow
    case 1: *r = q; *g = v; *b = p; break;  // Yellow to Green  
    case 2: *r = p; *g = v; *b = u; break;  // Green to Cyan
    case 3: *r = p; *g = q; *b = v; break;  // Cyan to Blue
    case 4: *r = u; *g = p; *b = v; break;  // Blue to Magenta
    case 5: *r = v; *g = p; *b = q; break;  // Magenta to Red
  }
}

// Get median value from array
int getMedian(int* arr, int size) {
  // Create a temporary array for sorting
  int temp[MEDIAN_SAMPLES];
  for (int i = 0; i < size; i++) {
    temp[i] = arr[i];
  }
  
  // Simple bubble sort for small array
  for (int i = 0; i < size - 1; i++) {
    for (int j = 0; j < size - i - 1; j++) {
      if (temp[j] > temp[j + 1]) {
        int swap = temp[j];
        temp[j] = temp[j + 1];
        temp[j + 1] = swap;
      }
    }
  }
  
  return temp[size / 2];
}

// Apply median filter then EMA smoothing to RSSI
float smoothRSSI(int rawRSSI) {
  // Add to circular buffer
  rssiBuffer[bufferIndex] = rawRSSI;
  bufferIndex = (bufferIndex + 1) % MEDIAN_SAMPLES;
  
  if (!bufferFull && bufferIndex == 0) {
    bufferFull = true;
  }
  
  // Get median value
  int medianValue;
  if (bufferFull) {
    medianValue = getMedian(rssiBuffer, MEDIAN_SAMPLES);
  } else {
    medianValue = getMedian(rssiBuffer, bufferIndex);
  }
  
  // Apply EMA to median value
  if (firstReading) {
    rssiEMA = medianValue;
    firstReading = false;
  } else {
    rssiEMA = (EMA_ALPHA * medianValue) + ((1 - EMA_ALPHA) * rssiEMA);
  }
  return rssiEMA;
}

// Display status colors based on state
void showStatusColor() {
  unsigned long currentTime = millis();
  
  switch (currentState) {
    case STATE_DISCOVERY:
    case STATE_SCANNING:
      // Dim amber for scanning
      if (currentTime - lastBlinkTime > STATUS_BLINK_MS) {
        blinkState = !blinkState;
        lastBlinkTime = currentTime;
      }
      if (blinkState) {
        fillMatrix(100, 50, 0);  // Amber
      } else {
        fillMatrix(50, 25, 0);   // Dim amber
      }
      break;
      
    case STATE_LOST:
      // Brief gray flash for lost signal
      fillMatrix(40, 40, 40);  // Gray
      break;
      
    case STATE_LOCKED:
      // Will be handled by RSSI color mapping
      break;
  }
}

// Perform discovery scan to find target network
bool performDiscoveryScan() {
  Serial.println("Starting discovery scan...");
  
  int numNetworks = WiFi.scanNetworks();
  if (numNetworks == 0) {
    Serial.println("No networks found");
    return false;
  }
  
  int bestRSSI = -127;
  int bestIndex = -1;
  
  // Find strongest network with target SSID
  for (int i = 0; i < numNetworks; i++) {
    if (WiFi.SSID(i) == TARGET_SSID) {
      Serial.printf("Found %s: RSSI=%d, Channel=%d\n", 
                    TARGET_SSID, WiFi.RSSI(i), WiFi.channel(i));
      
      if (WiFi.RSSI(i) > bestRSSI) {
        bestRSSI = WiFi.RSSI(i);
        bestIndex = i;
      }
    }
  }
  
  if (bestIndex >= 0) {
    // Store the BSSID and channel of strongest AP
    memcpy(targetBSSID, WiFi.BSSID(bestIndex), 6);
    targetChannel = WiFi.channel(bestIndex);
    hasTarget = true;
    
    Serial.printf("Locked to BSSID %02X:%02X:%02X:%02X:%02X:%02X on channel %d\n",
                  targetBSSID[0], targetBSSID[1], targetBSSID[2],
                  targetBSSID[3], targetBSSID[4], targetBSSID[5],
                  targetChannel);
    
    // Initialize EMA with first reading
    smoothRSSI(bestRSSI);
    
    return true;
  }
  
  Serial.println("Target network not found");
  return false;
}

// Scan for locked BSSID
int scanForTarget() {
  int numNetworks;
  
  // Try single-channel scan if we know the channel
  if (targetChannel > 0) {
    // Note: ESP32 Arduino doesn't directly support single-channel scan
    // We'll use regular scan but it will be faster with fewer networks
    numNetworks = WiFi.scanNetworks(false, false, false, 300, targetChannel);
  } else {
    numNetworks = WiFi.scanNetworks();
  }
  
  if (numNetworks == 0) {
    return -127;  // No signal
  }
  
  // Look for our specific BSSID
  for (int i = 0; i < numNetworks; i++) {
    uint8_t* bssid = WiFi.BSSID(i);
    if (memcmp(bssid, targetBSSID, 6) == 0) {
      return WiFi.RSSI(i);
    }
  }
  
  return -127;  // Target not found
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== WiFi Gradient Viewer ===");
  Serial.printf("Target SSID: %s\n", TARGET_SSID);
  Serial.printf("RSSI Range: %d to %d dBm\n", RSSI_MIN, RSSI_MAX);
  Serial.printf("EMA Alpha: %.2f\n", EMA_ALPHA);
  
  // Initialize LED Matrix
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();
  
  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);
  
  Serial.println("WiFi initialized in STA mode");
  
  // Show initialization pattern
  fillMatrix(0, 0, 100);  // Blue startup
  delay(500);
}

void loop() {
  switch (currentState) {
    case STATE_DISCOVERY:
      showStatusColor();
      
      if (performDiscoveryScan()) {
        currentState = STATE_LOCKED;
        lostCounter = 0;
      } else {
        // Stay in discovery, will retry
        delay(1000);
      }
      break;
      
    case STATE_SCANNING:
      showStatusColor();
      
      // Try to find network again
      if (performDiscoveryScan()) {
        currentState = STATE_LOCKED;
        lostCounter = 0;
      }
      break;
      
    case STATE_LOCKED:
      {
        int rssi = scanForTarget();
        
        if (rssi == -127) {
          // Target lost
          lostCounter++;
          if (lostCounter > 3) {
            Serial.println("Network lost!");
            currentState = STATE_LOST;
            lostCounter = 0;
          }
        } else {
          lostCounter = 0;
          
          // Apply smoothing
          float smoothed = smoothRSSI(rssi);
          
          // Convert to color
          uint8_t r, g, b;
          rssiToColor(smoothed, &r, &g, &b);
          
          // Update display
          fillMatrix(r, g, b);
          
          Serial.printf("RSSI: %d dBm (smoothed: %.1f) -> RGB(%d,%d,%d)\n",
                        rssi, smoothed, r, g, b);
        }
      }
      break;
      
    case STATE_LOST:
      showStatusColor();
      delay(200);  // Brief gray flash
      currentState = STATE_SCANNING;
      firstReading = true;  // Reset EMA for next lock
      bufferIndex = 0;      // Reset median filter
      bufferFull = false;
      break;
  }
  
  delay(SCAN_INTERVAL_MS);
}