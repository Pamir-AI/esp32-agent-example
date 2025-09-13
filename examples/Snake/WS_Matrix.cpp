#include "WS_Matrix.h"
#include <Arduino.h>

// English: Please note that the brightness of the lamp bead should not be too high, which can easily cause the temperature of the board to rise rapidly, thus damaging the board !!!
// Chinese: 请注意，灯珠亮度不要太高，容易导致板子温度急速上升，从而损坏板子!!! 

// LED strip object
Adafruit_NeoPixel pixels(RGB_COUNT, RGB_Control_PIN, NEO_RGB + NEO_KHZ800);

// Snake game data structures
struct Point {
  int8_t x;
  int8_t y;
};

Point snake[MAX_SNAKE_LENGTH];  // Snake body positions
uint8_t snakeLength = 3;        // Current snake length
Point food;                      // Food position
bool gameOver = false;

// Color definitions (keep brightness low to prevent overheating)
uint8_t headColor[3] = {0, 50, 0};   // Bright green for head
uint8_t bodyColor[3] = {0, 25, 0};   // Dimmer green for body  
uint8_t foodColor[3] = {40, 0, 0};   // Red for food

// Forward declaration
void GameOverAnimation();
void GenerateFood();

// Initialize the LED matrix
void Matrix_Init() {
  pixels.begin();
  // English: Please note that the brightness of the lamp bead should not be too high, which can easily cause the temperature of the board to rise rapidly, thus damaging the board !!!
  // Chinese: 请注意，灯珠亮度不要太高，容易导致板子温度急速上升，从而损坏板子!!! 
  pixels.setBrightness(50);  // Keep brightness low
  pixels.clear();
  pixels.show();
}

// Initialize snake game
void Snake_Init() {
  // Place snake in middle of board, moving right
  snakeLength = 3;
  snake[0] = {4, 4};  // Head
  snake[1] = {3, 4};  // Body
  snake[2] = {2, 4};  // Tail
  
  // Place initial food
  GenerateFood();
  
  gameOver = false;
}

// Generate new food position
void GenerateFood() {
  bool validPosition = false;
  
  while (!validPosition) {
    food.x = random(0, Matrix_Row);  // x is row
    food.y = random(0, Matrix_Col);  // y is column
    
    // Check if food overlaps with snake
    validPosition = true;
    for (uint8_t i = 0; i < snakeLength; i++) {
      if (snake[i].x == food.x && snake[i].y == food.y) {
        validPosition = false;
        break;
      }
    }
  }
}

// Get current snake length
uint8_t GetSnakeLength() {
  return snakeLength;
}

// Move snake in given direction
// Returns: 0 = game over, 1 = normal move, 2 = food eaten
uint8_t MoveSnake(uint8_t direction) {
  if (gameOver) return 0;
  
  // Calculate new head position
  Point newHead = snake[0];
  
  // In the matrix: x is row (0-7 top to bottom), y is column (0-7 left to right)
  switch(direction) {
    case 0: // Up (decrease row)
      newHead.x--;
      break;
    case 1: // Right (increase column)
      newHead.y++;
      break;
    case 2: // Down (increase row)
      newHead.x++;
      break;
    case 3: // Left (decrease column)
      newHead.y--;
      break;
  }
  
  // Check wall collision (wrap around edges for now)
  if (newHead.x < 0) newHead.x = Matrix_Row - 1;
  if (newHead.x >= Matrix_Row) newHead.x = 0;
  if (newHead.y < 0) newHead.y = Matrix_Col - 1;
  if (newHead.y >= Matrix_Col) newHead.y = 0;
  
  // Check if trying to move backwards into immediate body segment
  if (snakeLength > 1 && snake[1].x == newHead.x && snake[1].y == newHead.y) {
    // Trying to go backwards - just ignore this move and continue forward
    return 1;  // Return normal move but don't actually update position
  }
  
  // Check self collision with rest of body
  for (uint8_t i = 2; i < snakeLength; i++) {  // Start from i=2 to skip immediate body segment
    if (snake[i].x == newHead.x && snake[i].y == newHead.y) {
      gameOver = true;
      GameOverAnimation();
      return 0;  // Game over
    }
  }
  
  // Check if food is eaten
  bool foodEaten = (newHead.x == food.x && newHead.y == food.y);
  
  // Move snake body
  if (!foodEaten) {
    // Shift body segments (remove tail)
    for (int8_t i = snakeLength - 1; i > 0; i--) {
      snake[i] = snake[i - 1];
    }
  } else {
    // Grow snake (keep tail, shift everything else)
    if (snakeLength < MAX_SNAKE_LENGTH) {
      for (int8_t i = snakeLength; i > 0; i--) {
        snake[i] = snake[i - 1];
      }
      snakeLength++;
      GenerateFood();
    }
  }
  
  // Place new head
  snake[0] = newHead;
  
  return foodEaten ? 2 : 1;
}

// Update LED display
void UpdateDisplay() {
  pixels.clear();
  
  // Draw snake body (draw body first so head appears on top)
  // Using the original indexing: row*8+col where x=row, y=col
  for (uint8_t i = 1; i < snakeLength; i++) {
    uint8_t pixelIndex = snake[i].x * Matrix_Col + snake[i].y;
    pixels.setPixelColor(pixelIndex, pixels.Color(bodyColor[0], bodyColor[1], bodyColor[2]));
  }
  
  // Draw snake head (brighter)
  uint8_t headIndex = snake[0].x * Matrix_Col + snake[0].y;
  pixels.setPixelColor(headIndex, pixels.Color(headColor[0], headColor[1], headColor[2]));
  
  // Draw food
  uint8_t foodIndex = food.x * Matrix_Col + food.y;
  pixels.setPixelColor(foodIndex, pixels.Color(foodColor[0], foodColor[1], foodColor[2]));
  
  pixels.show();
}

// Game over animation
void GameOverAnimation() {
  // Flash red 3 times
  for (uint8_t flash = 0; flash < 3; flash++) {
    // All red
    for (uint8_t i = 0; i < RGB_COUNT; i++) {
      pixels.setPixelColor(i, pixels.Color(30, 0, 0));
    }
    pixels.show();
    delay(200);
    
    // Clear
    pixels.clear();
    pixels.show();
    delay(200);
  }
}