#include "WS_QMI8658.h"
#include "WS_Matrix.h"

// English: Please note that the brightness of the lamp bead should not be too high, which can easily cause the temperature of the board to rise rapidly, thus damaging the board !!!
// Chinese: 请注意，灯珠亮度不要太高，容易导致板子温度急速上升，从而损坏板子!!! 

extern IMUdata Accel;
unsigned long lastMoveTime = 0;
unsigned long moveInterval = 300; // Snake speed in milliseconds

void setup()
{
  Serial.begin(115200);
  QMI8658_Init();
  Matrix_Init();
  Snake_Init();
  Serial.println("Snake Game Started!");
  Serial.println("Tilt the board to control the snake");
}

// Direction tracking
uint8_t currentDirection = 1; // 0=up, 1=right, 2=down, 3=left
uint8_t Time_X_A = 0, Time_X_B = 0, Time_Y_A = 0, Time_Y_B = 0;

void loop()
{
  unsigned long currentTime = millis();
  
  // Read IMU continuously
  QMI8658_Loop();
  
  // Use the original game's IMU processing logic
  if(Accel.x > 0.15 || Accel.x < 0 || Accel.y > 0.15 || Accel.y < 0){
    // Process X-axis (controls up/down movement)
    if(Accel.x > 0.15){
      Time_X_A = Time_X_A + Accel.x * 10;
      Time_X_B = 0;
    }
    else if(Accel.x < 0){
      Time_X_B = Time_X_B + abs(Accel.x) * 10;
      Time_X_A = 0;
    }
    else{
      Time_X_A = 0;
      Time_X_B = 0;
    }
    
    // Process Y-axis (controls left/right movement)
    if(Accel.y > 0.15){
      Time_Y_A = Time_Y_A + Accel.y * 10;
      Time_Y_B = 0;
    }
    else if(Accel.y < 0){
      Time_Y_B = Time_Y_B + abs(Accel.y) * 10;
      Time_Y_A = 0;
    }
    else{
      Time_Y_A = 0;
      Time_Y_B = 0;
    }
    
    // Determine direction based on accumulated time
    uint8_t newDirection = currentDirection;
    
    if(Time_X_A >= 10){
      newDirection = 2; // Tilt forward (positive X) = move down
      Time_X_A = 0;
      Time_X_B = 0;
    }
    if(Time_X_B >= 10){
      newDirection = 0; // Tilt backward (negative X) = move up
      Time_X_A = 0;
      Time_X_B = 0;
    }
    if(Time_Y_A >= 10){
      newDirection = 3; // Tilt left (positive Y) = move left
      Time_Y_A = 0;
      Time_Y_B = 0;
    }
    if(Time_Y_B >= 10){
      newDirection = 1; // Tilt right (negative Y) = move right
      Time_Y_A = 0;
      Time_Y_B = 0;
    }
    
    // Prevent 180-degree turns (can't go back into yourself)
    if (GetSnakeLength() > 1) {
      if ((currentDirection == 0 && newDirection == 2) || // Up to down
          (currentDirection == 2 && newDirection == 0) || // Down to up
          (currentDirection == 1 && newDirection == 3) || // Right to left
          (currentDirection == 3 && newDirection == 1)) { // Left to right
        // Invalid move, keep current direction
      } else {
        currentDirection = newDirection;
      }
    } else {
      currentDirection = newDirection;
    }
  }
  
  // Move snake at regular intervals
  if (currentTime - lastMoveTime >= moveInterval) {
    uint8_t gameStatus = MoveSnake(currentDirection);
    
    if (gameStatus == 0) {
      // Game over
      Serial.print("Game Over! Score: ");
      Serial.println(GetSnakeLength() - 3);
      delay(2000);
      
      // Reset game
      Snake_Init();
      currentDirection = 1;
      Time_X_A = 0;
      Time_X_B = 0;
      Time_Y_A = 0;
      Time_Y_B = 0;
      Serial.println("New Game Started!");
    } else if (gameStatus == 2) {
      // Food eaten, increase speed slightly
      Serial.print("Score: ");
      Serial.println(GetSnakeLength() - 3);
      
      // Speed up as snake grows (minimum 100ms)
      moveInterval = max(100, 300 - (GetSnakeLength() - 3) * 10);
    }
    
    UpdateDisplay();
    lastMoveTime = currentTime;
  }
  
  delay(10);
}