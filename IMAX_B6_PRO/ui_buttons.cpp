#include "ui_telemetry.h"
#include "config.h"
#include "hal_drivers.h"
#include "sensors.h"
#include "charger_fsm.h"
#include "lab_psu.h"
#include "ir_tester.h"

enum MenuScreen : uint8_t {
  SCR_MAIN = 0,
  SCR_CELLS = 1,
  SCR_SERVICES = 2,
  SCR_PSU_SET = 3,
  SCR_IR_SET = 4,
  SCR_SETTINGS = 5
};

uint8_t uiScreen = SCR_MAIN;
uint8_t mainIdx  = 0;
uint8_t srvIdx   = 0;
uint8_t fieldIdx = 0;
uint8_t runPage  = 0;

bool          blinkPhase  = true;
unsigned long lastBlinkMs = 0;

void resetBlink() {
  blinkPhase = true;
  lastBlinkMs = millis();
}

static uint8_t readRawButton() {
  if (!digitalRead(BTN_STOP))  return 1;
  if (!digitalRead(BTN_DEC))   return 2;
  if (!digitalRead(BTN_INC))   return 3;
  if (!digitalRead(BTN_START)) return 4;
  return 0;
}

void handleButtons() {
  static uint8_t  candBtn       = 0;
  static uint32_t candStartMs   = 0;
  static uint8_t  activeBtn     = 0;
  static uint32_t activePressMs = 0;
  static uint32_t lastRepeatMs  = 0;
  static bool     actionDone    = false;
  static uint32_t releaseMs     = 0;

  uint32_t now = millis();
  uint8_t raw = readRawButton();

  if (actionDone) {
    if (raw == 0) {
      if (releaseMs == 0) releaseMs = now;
      if (now - releaseMs >= 60) {
        actionDone = false;
        activeBtn = 0;
        candBtn = 0;
        releaseMs = 0;
      }
    } else {
      releaseMs = 0;
    }
    return;
  }

  if (raw != candBtn) {
    candBtn = raw;
    candStartMs = now;
  }
  if (candBtn == 0) {
    if (activeBtn == 1 && !actionDone) {
      bool isBlinkMenu = (sysState == STATE_IDLE) && (uiScreen == SCR_CELLS || uiScreen == SCR_PSU_SET);
      if (isBlinkMenu && (now - activePressMs >= 60)) {
        uint8_t maxFields = (uiScreen == SCR_CELLS) ? 4 : 2;
        fieldIdx = (fieldIdx + 1) % maxFields;
        resetBlink();
        softBeep(2200, 35);
      }
    }
    activeBtn = 0;
    return;
  }

  if (now - candStartMs < 60) return;

  bool isFirstPress = (activeBtn != candBtn);
  if (isFirstPress) {
    activeBtn = candBtn;
    activePressMs = now;
    lastRepeatMs = now;
  }

  bool bStop  = (activeBtn == 1);
  bool bDec   = (activeBtn == 2);
  bool bInc   = (activeBtn == 3);
  bool bStart = (activeBtn == 4);

  if (sysState == STATE_IDLE && (uiScreen == SCR_CELLS || uiScreen == SCR_PSU_SET)) {
    if (bStop) {
      if (now - activePressMs >= 800) {
        actionDone = true;
        uiScreen = (uiScreen == SCR_CELLS) ? SCR_MAIN : SCR_SERVICES;
        resetBlink();
        softBeep(1900, 80);
      }
      return;
    }
  }

  bool canRepeat = (bInc || bDec) && (
    (sysState == STATE_IDLE && (uiScreen == SCR_CELLS || uiScreen == SCR_PSU_SET || uiScreen == SCR_IR_SET)) ||
    (sysState == STATE_RUNNING && selMode == MODE_PSU)
  );

  if (!isFirstPress) {
    if (!canRepeat || (now - activePressMs < 450) || (now - lastRepeatMs < 220)) return;
    lastRepeatMs = now;
  } else if (!canRepeat) {
    actionDone = true;
  }

  if (sysState != STATE_IDLE) {
    if (bStop) {
      actionDone = true;
      stopJobManual();
      if (selMode == MODE_PSU) { uiScreen = SCR_PSU_SET; fieldIdx = 0; resetBlink(); }
      else if (selMode == MODE_IR) { uiScreen = SCR_IR_SET; }
      else { uiScreen = SCR_CELLS; fieldIdx = 0; resetBlink(); }
    } else if ((bInc || bDec) && selMode != MODE_PSU) {
      runPage = (runPage + 1) % 3;
      softBeep(2400, 35);
    } else if (selMode == MODE_PSU && (bInc || bDec)) {
      if (bInc && setVolts < PSU_MAX_VOLTS - 0.05f) setVolts += 0.10f;
      if (bDec && setVolts > PSU_MIN_VOLTS + 0.05f) setVolts -= 0.10f;
      softBeep(2500, 25);
    }
    return;
  }

  if (uiScreen == SCR_MAIN) {
    if (bInc) { mainIdx = (mainIdx + 1) % 3; softBeep(2400, 35); }
    else if (bDec) { mainIdx = (mainIdx + 2) % 3; softBeep(2400, 35); }
    else if (bStart) {
      if (mainIdx == 0) {
        uiScreen = SCR_CELLS;
        if (selMode >= MODE_PSU) selMode = MODE_CHARGE;
        fieldIdx = 0; resetBlink();
      } else if (mainIdx == 1) {
        uiScreen = SCR_SERVICES;
        srvIdx = 0;
      } else {
        uiScreen = SCR_SETTINGS;
      }
      softBeep(2600, 50);
    }
  }
  else if (uiScreen == SCR_CELLS) {
    if (bInc || bDec) {
      int8_t dir = bInc ? 1 : -1;
      resetBlink();
      softBeep(2400, 30);
      if (fieldIdx == 0) selChem = (selChem == CHEM_LION) ? CHEM_LIFE : CHEM_LION;
      else if (fieldIdx == 1) selCells = ((selCells - 1 + 6 + dir) % 6) + 1;
      else if (fieldIdx == 2) selMode = (WorkMode)((selMode + 4 + dir) % 4);
      else if (fieldIdx == 3) setAmps = constrain(setAmps + dir * 0.10f, 0.10f, 5.00f);
    } else if (bStart) {
      runPage = 0;
      startJob();
    }
  }
  else if (uiScreen == SCR_SERVICES) {
    if (bInc || bDec) { srvIdx = (srvIdx + 1) % 2; softBeep(2400, 35); }
    else if (bStop) { uiScreen = SCR_MAIN; softBeep(1900, 45); }
    else if (bStart) {
      if (srvIdx == 0) {
        selMode = MODE_PSU;
        uiScreen = SCR_PSU_SET;
        fieldIdx = 0; resetBlink();
      } else {
        selMode = MODE_IR;
        uiScreen = SCR_IR_SET;
      }
      softBeep(2600, 50);
    }
  }
  else if (uiScreen == SCR_PSU_SET) {
    if (bInc || bDec) {
      int8_t dir = bInc ? 1 : -1;
      resetBlink();
      softBeep(2400, 30);
      if (fieldIdx == 0) setVolts = constrain(setVolts + dir * 0.10f, PSU_MIN_VOLTS, PSU_MAX_VOLTS);
      else setAmps = constrain(setAmps + dir * 0.10f, 0.10f, 5.00f);
    } else if (bStart) {
      selMode = MODE_PSU;
      runPage = 0;
      startJob();
    }
  }
  else if (uiScreen == SCR_IR_SET) {
    if (bInc || bDec) {
      int8_t dir = bInc ? 1 : -1;
      selCells = ((selCells - 1 + 6 + dir) % 6) + 1;
      softBeep(2400, 35);
    } else if (bStop) {
      uiScreen = SCR_SERVICES;
      softBeep(1900, 45);
    } else if (bStart) {
      selMode = MODE_IR;
      runPage = 0;
      startJob();
    }
  }
  else if (uiScreen == SCR_SETTINGS) {
    if (bStart || bInc || bDec) {
      buzzerEnabled = !buzzerEnabled;
      softBeep(2500, 50);
    } else if (bStop) {
      uiScreen = SCR_MAIN;
      softBeep(1900, 45);
    }
  }
}
