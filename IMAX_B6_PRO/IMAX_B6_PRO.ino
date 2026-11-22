#include <Arduino.h>
#include "config.h"
#include "hal_drivers.h"
#include "charger_fsm.h"
#include "ui_telemetry.h"

void setup() {
  // 1. Мгновенная блокировка силовой части и балансира
  initHardwarePinsSafe();

  // 2. Обязательное окно 3 секунды для прошивки через ST-Link V2 (до старта UART на PA_14)
  delay(3000);

  // 3. Инициализация АЦП 12 бит (HSI14) и ШИМ 20 кГц (10 бит)
  initHardwareADC();
  initUltrasonicPWM();

  // 4. Запуск дисплея, кнопок и телеметрии/графиков
  initUI();
  initTelemetry();

  softBeep(2400, 100);
  delay(600);
}

void loop() {
  handleButtons();
  runControlStep();
  updateUI();
  sendTelemetry();
}
