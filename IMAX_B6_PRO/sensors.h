#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

extern float shuntZero;
extern float measAmps;
extern float measVout;
extern float cellV1, cellV2, cellV3, cellV4, cellV5, cellV6;

void  updateAllMeasurements();
float getCellVoltage(uint8_t idx);
float getTotalPackVoltage();

#endif
