#include "ui_telemetry.h"
#include "config.h"
#include "sensors.h"
#include "charger_fsm.h"
#include "ir_tester.h"
#include <SoftwareSerial.h>

static SoftwareSerial debugSerial(PIN_UART_RX, PIN_UART_TX);
static unsigned long lastSerialMs = 0;

void initTelemetry() {
  debugSerial.begin(9600);
}

static void printSysStatus(const char* overrideFlag) {
  const char* mTags[] = {"CHG", "STO", "DSC", "BAL", "PSU", "IR"};
  debugSerial.print(F(" SYS:"));
  if (overrideFlag != nullptr) {
    debugSerial.println(overrideFlag);
    return;
  }
  if (sysState == STATE_IDLE) {
    debugSerial.println(F("IDLE"));
  } else if (sysState == STATE_RUNNING) {
    debugSerial.print(F("RUN_"));
    debugSerial.print(mTags[selMode]);
    if (selMode != MODE_PSU) {
      debugSerial.print('_');
      debugSerial.print(selCells);
      debugSerial.print('S');
    }
    debugSerial.println();
  } else if (sysState == STATE_DONE) {
    debugSerial.print(F("DONE_"));
    debugSerial.print(mTags[selMode]);
    if (selMode == MODE_IR) {
      debugSerial.print(F(" IR:"));
      debugSerial.print(totalPackIrMohm);
      if (selCells > 1 && !useVoutAs1S) {
        for (uint8_t i = 0; i < selCells; i++) {
          debugSerial.print(F(" B"));
          debugSerial.print(i + 1);
          debugSerial.print(':');
          debugSerial.print(cellIrMohm[i]);
        }
      }
    }
    debugSerial.println();
  } else {
    debugSerial.println(F("ERR"));
  }
}

void sendTelemetryNow(const char* overrideFlag) {
  lastSerialMs = millis();

  if (sysState == STATE_IDLE && overrideFlag == nullptr) updateAllMeasurements();

  float packV = (sysState == STATE_IDLE) ? measVout : getTotalPackVoltage();
  uint16_t activePwm = (pwmDisch > 0) ? pwmDisch : pwmBuck;
  float pwmPct = (activePwm * 100.0f) / 1023.0f;

  debugSerial.print(F("V:"));  debugSerial.print(packV, 2);
  debugSerial.print(F(" I:")); debugSerial.print(measAmps, 2);
  debugSerial.print(F(" P:")); debugSerial.print(pwmPct, 1);
  debugSerial.print(F(" Q:")); debugSerial.print(capacityMah / 1000.0f, 3);
  debugSerial.print(F(" C1:")); debugSerial.print(getCellVoltage(0), 2);
  debugSerial.print(F(" C2:")); debugSerial.print(cellV2, 2);
  debugSerial.print(F(" C3:")); debugSerial.print(cellV3, 2);

  if (selCells > 3) {
    debugSerial.print(F(" C4:")); debugSerial.print(cellV4, 2);
    debugSerial.print(F(" C5:")); debugSerial.print(cellV5, 2);
    debugSerial.print(F(" C6:")); debugSerial.print(cellV6, 2);
  }

  printSysStatus(overrideFlag);
}

void sendTelemetry() {
  if (millis() - lastSerialMs < 1000) return;
  sendTelemetryNow(nullptr);
}
