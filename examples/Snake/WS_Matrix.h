#ifndef _WS_Matrix_H_
#define _WS_Matrix_H_
#include <Adafruit_NeoPixel.h>

#define RGB_Control_PIN   14       
#define Matrix_Row        8     
#define Matrix_Col        8       
#define RGB_COUNT         64
#define MAX_SNAKE_LENGTH  64    // Maximum possible snake length (entire board)

// Function declarations
void RGB_Matrix();
void Matrix_Init();
void Snake_Init();
uint8_t MoveSnake(uint8_t direction);
void UpdateDisplay();
uint8_t GetSnakeLength();

#endif
