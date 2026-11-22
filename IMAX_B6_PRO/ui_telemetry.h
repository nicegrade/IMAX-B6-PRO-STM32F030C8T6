#ifndef UI_TELEMETRY_H
#define UI_TELEMETRY_H

#include <Arduino.h>

void initUI();
void initTelemetry();
void handleButtons();
void updateUI();
void sendTelemetry();
void sendTelemetryNow(const char* overrideFlag = nullptr);

#endif
