#include "hw.h"
#include "clsPCA9555.h"
#include <Arduino.h>

extern PCA9555 io_exp;

void reset_sensor_vdd(void)
{
  io_exp.pinMode(SENSOR_VDD_EN_PIN_IO_EXP, INPUT);
  delay(200);
  io_exp.pinMode(SENSOR_VDD_EN_PIN_IO_EXP, OUTPUT);
  delay(100);
}


