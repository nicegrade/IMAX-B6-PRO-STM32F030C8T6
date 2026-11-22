#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

extern float shuntZero;
extern float measAmps;
extern float measVout;
extern float measVin;
extern float cellV1, cellV2, cellV3, cellV4, cellV5, cellV6;
extern uint16_t rawMuxY0, rawMuxY1, rawMuxY2;

void  updateAllMeasurements();
float getCellVoltage(uint8_t idx);
float getTotalPackVoltage();

#endif
