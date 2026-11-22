#include "sensors.h"
#include "config.h"
#include "hal_drivers.h"
#include "charger_fsm.h"

float shuntZero = 1.55f;
float measAmps  = 0.0f;
float measVout  = 0.0f;
float measVin   = 12.0f;
float cellV1 = 0, cellV2 = 0, cellV3 = 0, cellV4 = 0, cellV5 = 0, cellV6 = 0;
uint16_t rawMuxY0 = 0, rawMuxY1 = 0, rawMuxY2 = 0;

void updateAllMeasurements() {
  uint16_t rVout = readAdcAvg16(7); // PA_7
  measVout = (rVout > 12) ? (rVout * K_VOUT) : 0.0f;

  if (selMode != MODE_PSU) {
    if (balMask != 0) {
      applyBalanceMask(0);
      delay(3);
    }

    float s1 = readAdcAvg16(1) * K_B1;
    float s2 = readAdcAvg16(2) * K_B2;
    float s3 = readAdcAvg16(3) * K_B3;
    float s4 = readAdcAvg16(4) * K_B4;
    float s5 = readAdcAvg16(5) * K_B5;
    float s6 = readAdcAvg16(6) * K_B6;

    if (balMask != 0) {
      applyBalanceMask(balMask);
    }

    cellV1 = s1;
    cellV2 = (s2 > s1 + 0.1f) ? (s2 - s1) : 0.0f;
    cellV3 = (s3 > s2 + 0.1f) ? (s3 - s2) : 0.0f;
    cellV4 = (s4 > s3 + 0.1f) ? (s4 - s3) : 0.0f;
    cellV5 = (s5 > s4 + 0.1f) ? (s5 - s4) : 0.0f;
    cellV6 = (s6 > s5 + 0.1f) ? (s6 - s5) : 0.0f;
  } else {
    cellV1 = cellV2 = cellV3 = cellV4 = cellV5 = cellV6 = 0.0f;
  }

  rawMuxY0 = readMuxAvg16(0); // Y0: Входное напряжение V_IN
  rawMuxY1 = readMuxAvg16(1); // Y1: Термодатчик CN1
  rawMuxY2 = readMuxAvg16(2); // Y2: Аппаратный датчик тока разряда (V5/C3)

  measVin = rawMuxY0 * K_VIN;

  if (pwmDisch > 0) {
    measAmps = (rawMuxY2 > 2) ? (rawMuxY2 * K_DISCH_ADC) : 0.0f;
  } else {
    float rSh = readShuntOversampled64();
    float rawDiff = rSh - shuntZero;
    measAmps = (rawDiff > 0.12f) ? (rawDiff * K_SHUNT_A) : 0.0f;
  }
}

float getCellVoltage(uint8_t idx) {
  if (useVoutAs1S) {
    return (selCells > 0) ? (measVout / (float)selCells) : measVout;
  }
  switch (idx) {
    case 0: return cellV1;
    case 1: return cellV2;
    case 2: return cellV3;
    case 3: return cellV4;
    case 4: return cellV5;
    case 5: return cellV6;
    default: return 0.0f;
  }
}

float getTotalPackVoltage() {
  if (selMode == MODE_PSU || useVoutAs1S) return measVout;
  float sum = 0.0f;
  for (uint8_t i = 0; i < selCells; i++) {
    sum += getCellVoltage(i);
  }
  return sum;
}
