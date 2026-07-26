#ifndef _WS_QMI8658_H_
#define _WS_QMI8658_H_
#include <Arduino.h>
#include <Wire.h>
#include "SensorQMI8658.hpp"

// IMUdata is already defined in SensorQMI8658.hpp
extern IMUdata Accel;
extern IMUdata Gyro;

// Returns true if the IMU was found and configured. Never blocks -
// callers should fall back to on-screen controls when this returns false.
bool QMI8658_Init();
void QMI8658_Loop();
#endif
