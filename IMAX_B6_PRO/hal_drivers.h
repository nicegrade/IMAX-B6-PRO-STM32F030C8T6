#ifndef HAL_DRIVERS_H
#define HAL_DRIVERS_H

#include <Arduino.h>

extern bool buzzerEnabled;

void initHardwarePinsSafe();
void initHardwareADC();
void initUltrasonicPWM();

uint16_t readAdcAvg16(uint8_t ch);
uint16_t readMuxAvg16(uint8_t muxCh);
float    readShuntOversampled64();

void softBeep(uint16_t freqHz, uint16_t durationMs, bool forceAlarm = false);
void applyBalanceMask(uint8_t mask);
void hardwareAllOff();

#endif
