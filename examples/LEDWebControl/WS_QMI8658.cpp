#include "WS_QMI8658.h"

#define I2C_SDA 11
#define I2C_SCL 12

SensorQMI8658 QMI;

IMUdata Accel;
IMUdata Gyro;

bool QMI8658_Init() {
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!QMI.begin(Wire, QMI8658_L_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
    Serial.println("QMI8658 IMU not found - tilt control disabled, use the on-screen arrows instead");
    return false;
  }

  Serial.print("IMU Device ID: 0x");
  Serial.println(QMI.getChipID(), HEX);

  QMI.configAccelerometer(
      SensorQMI8658::ACC_RANGE_4G,
      SensorQMI8658::ACC_ODR_1000Hz,
      SensorQMI8658::LPF_MODE_0);
  QMI.configGyroscope(
      SensorQMI8658::GYR_RANGE_64DPS,
      SensorQMI8658::GYR_ODR_896_8Hz,
      SensorQMI8658::LPF_MODE_3);

  QMI.enableGyroscope();
  QMI.enableAccelerometer();
  return true;
}

void QMI8658_Loop() {
  if (QMI.getDataReady()) {
    QMI.getAccelerometer(Accel.x, Accel.y, Accel.z);
    QMI.getGyroscope(Gyro.x, Gyro.y, Gyro.z);
  }
}
