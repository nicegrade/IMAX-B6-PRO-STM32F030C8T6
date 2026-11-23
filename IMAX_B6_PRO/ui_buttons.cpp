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
  SCR_DIAGNOSTICS = 2,
  SCR_PSU_SET = 3,
  SCR_IR_SET = 4,
  SCR_SETTINGS = 5
};

uint8_t uiScreen    = SCR_MAIN;
uint8_t mainIdx     = 0;
uint8_t diagIdx     = 0;
uint8_t fieldIdx    = 0;
uint8_t psuRunField = 0; // 0 = Напряжение (V), 1 = Ток (A) в режиме RUN PSU
uint8_t runPage     = 0;

bool          blinkPhase    = true;
unsigned long lastBlinkMs   = 0;
unsigned long lastPsuEditMs = 0; // Время последнего изменения уставки в RUN PSU

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

  const uint16_t DEBOUNCE_PRESS_MS   = 22;
  const uint16_t DEBOUNCE_RELEASE_MS = 22;
  const uint16_t LONG_PRESS_MS       = 500;

  uint32_t now = millis();
  uint8_t raw = readRawButton();

  if (actionDone) {
    if (raw == 0) {
      if (releaseMs == 0) releaseMs = now;
      if (now - releaseMs >= DEBOUNCE_RELEASE_MS) {
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
      if (isBlinkMenu && (now - activePressMs >= DEBOUNCE_PRESS_MS)) {
        uint8_t maxFields = (uiScreen == SCR_CELLS) ? 4 : 2;
        fieldIdx = (fieldIdx + 1) % maxFields;
        resetBlink();
        softBeep(2200, 25);
      }
    }
    activeBtn = 0;
    return;
  }

  if (now - candStartMs < DEBOUNCE_PRESS_MS) return;

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
      if (now - activePressMs >= LONG_PRESS_MS) {
        actionDone = true;
        uiScreen = (uiScreen == SCR_CELLS) ? SCR_MAIN : SCR_DIAGNOSTICS;
        resetBlink();
        softBeep(1900, 65);
      }
      return;
    }
  }

  bool canRepeat = (bInc || bDec) && (
    (sysState == STATE_IDLE && (uiScreen == SCR_CELLS || uiScreen == SCR_PSU_SET || uiScreen == SCR_IR_SET)) ||
    (sysState == STATE_RUNNING && selMode == MODE_PSU)
  );

  uint8_t accelMult = 1;
  if (!isFirstPress) {
    if (!canRepeat) return;
    uint32_t heldMs = now - activePressMs;
    if (heldMs < 320) return;

    uint16_t repInterval = 140;
    if (heldMs >= 1900) {
      repInterval = 45;
      accelMult = 5;
    } else if (heldMs >= 1000) {
      repInterval = 75;
      accelMult = 2;
    }

    if (now - lastRepeatMs < repInterval) return;
    lastRepeatMs = now;
  } else if (!canRepeat) {
    actionDone = true;
  }

  if (sysState != STATE_IDLE) {
    if (bStop) {
      actionDone = true;
      stopJobManual();
      if (selMode == MODE_PSU) {
        uiScreen = SCR_PSU_SET;
        fieldIdx = 0;
        resetBlink();
      } else if (selMode == MODE_IR) {
        uiScreen = SCR_IR_SET;
        resetBlink();
      } else {
        uiScreen = SCR_CELLS;
        resetBlink();
      }
    } else if (selMode == MODE_PSU && sysState == STATE_RUNNING) {
      if (bStart) {
        psuRunField = (psuRunField + 1) % 2;
        lastPsuEditMs = now;
        resetBlink();
        softBeep(2300, 25);
      } else if (bInc || bDec) {
        int8_t dir = bInc ? 1 : -1;
        lastPsuEditMs = now;
        resetBlink();
        if (psuRunField == 0) {
          float stepV = 0.10f * accelMult;
          setVolts = constrain(setVolts + dir * stepV, PSU_MIN_VOLTS, PSU_MAX_VOLTS);
        } else {
          float stepA = (accelMult >= 5) ? 0.20f : 0.10f;
          setAmps = constrain(setAmps + dir * stepA, 0.10f, 5.00f);
        }
        softBeep(2500, 18);
      }
    } else if ((bInc || bDec) && selMode != MODE_PSU) {
      runPage = (runPage + 1) % 3;
      softBeep(2400, 25);
    }
    return;
  }

  if (uiScreen == SCR_MAIN) {
    if (bInc) { mainIdx = (mainIdx + 1) % 3; resetBlink(); softBeep(2400, 25); }
    else if (bDec) { mainIdx = (mainIdx + 2) % 3; resetBlink(); softBeep(2400, 25); }
    else if (bStart) {
      if (mainIdx == 0) {
        uiScreen = SCR_CELLS;
        if (selMode >= MODE_PSU) selMode = MODE_CHARGE;
        resetBlink();
      } else if (mainIdx == 1) {
        uiScreen = SCR_DIAGNOSTICS;
        diagIdx = 0;
        resetBlink();
      } else {
        uiScreen = SCR_SETTINGS;
        resetBlink();
      }
      softBeep(2600, 40);
    }
  }
  else if (uiScreen == SCR_CELLS) {
    if (bInc || bDec) {
      int8_t dir = bInc ? 1 : -1;
      resetBlink();
      softBeep(2400, 20);
      if (fieldIdx == 0) selChem = (selChem == CHEM_LION) ? CHEM_LIFE : CHEM_LION;
      else if (fieldIdx == 1) selCells = ((selCells - 1 + 6 + dir) % 6) + 1;
      else if (fieldIdx == 2) selMode = (WorkMode)((selMode + 4 + dir) % 4);
      else if (fieldIdx == 3) {
        float stepA = (accelMult >= 5) ? 0.20f : 0.10f;
        setAmps = constrain(setAmps + dir * stepA, 0.10f, 5.00f);
      }
    } else if (bStart) {
      runPage = 0;
      startJob();
    }
  }
  else if (uiScreen == SCR_DIAGNOSTICS) {
    if (bInc || bDec) { diagIdx = (diagIdx + 1) % 2; resetBlink(); softBeep(2400, 25); }
    else if (bStop) { uiScreen = SCR_MAIN; resetBlink(); softBeep(1900, 35); }
    else if (bStart) {
      if (diagIdx == 0) {
        selMode = MODE_PSU;
        uiScreen = SCR_PSU_SET;
        fieldIdx = 0;
        resetBlink();
      } else {
        selMode = MODE_IR;
        uiScreen = SCR_IR_SET;
        resetBlink();
      }
      softBeep(2600, 40);
    }
  }
  else if (uiScreen == SCR_PSU_SET) {
    if (bInc || bDec) {
      int8_t dir = bInc ? 1 : -1;
      resetBlink();
      softBeep(2400, 20);
      if (fieldIdx == 0) {
        float stepV = 0.10f * accelMult;
        setVolts = constrain(setVolts + dir * stepV, PSU_MIN_VOLTS, PSU_MAX_VOLTS);
      } else {
        float stepA = (accelMult >= 5) ? 0.20f : 0.10f;
        setAmps = constrain(setAmps + dir * stepA, 0.10f, 5.00f);
      }
    } else if (bStart) {
      selMode = MODE_PSU;
      psuRunField = 0;
      lastPsuEditMs = 0;
      runPage = 0;
      resetBlink();
      startJob();
    }
  }
  else if (uiScreen == SCR_IR_SET) {
    if (bInc || bDec) {
      int8_t dir = bInc ? 1 : -1;
      selCells = ((selCells - 1 + 6 + dir) % 6) + 1;
      resetBlink();
      softBeep(2400, 25);
    } else if (bStop) {
      uiScreen = SCR_DIAGNOSTICS;
      resetBlink();
      softBeep(1900, 35);
    } else if (bStart) {
      selMode = MODE_IR;
      runPage = 0;
      startJob();
    }
  }
  else if (uiScreen == SCR_SETTINGS) {
    if (bStart || bInc || bDec) {
      buzzerEnabled = !buzzerEnabled;
      resetBlink();
      softBeep(2500, 40);
    } else if (bStop) {
      uiScreen = SCR_MAIN;
      resetBlink();
      softBeep(1900, 35);
    }
  }
}
