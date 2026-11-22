#include "ui_telemetry.h"
#include "config.h"
#include "hal_drivers.h"
#include "sensors.h"
#include "charger_fsm.h"
#include "lab_psu.h"
#include "ir_tester.h"
#include <LiquidCrystal.h>

static LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

extern uint8_t uiScreen;
extern uint8_t mainIdx;
extern uint8_t srvIdx;
extern uint8_t fieldIdx;
extern uint8_t runPage;
extern bool    blinkPhase;
extern unsigned long lastBlinkMs;

static uint8_t  lastDrawnScr = 255;
static RunState lastDrawnSt  = (RunState)255;
static uint8_t  lastDrawnPg  = 255;
static unsigned long lastUiMs = 0;

void initUI() {
  pinMode(BTN_STOP, INPUT_PULLUP); pinMode(BTN_DEC, INPUT_PULLUP);
  pinMode(BTN_INC, INPUT_PULLUP);  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("  IMAX B6 PRO   ");
  lcd.setCursor(0, 1);
  lcd.print("PSU + IR TESTER ");
}

static void print2D(uint8_t val) { if (val < 10) lcd.print('0'); lcd.print(val); }

static void printTimeAndCapRow(unsigned long sec, float capMah) {
  lcd.setCursor(0, 1);
  print2D(sec / 3600UL); lcd.print(':');
  print2D((sec % 3600UL) / 60UL); lcd.print(':');
  print2D(sec % 60UL);
  lcd.print("        ");

  uint32_t cap = (capMah > 0.0f) ? (uint32_t)(capMah + 0.5f) : 0UL;
  if (cap > 999999UL) cap = 999999UL;

  uint8_t digits = 1;
  uint32_t tmp = cap;
  while (tmp >= 10) {
    tmp /= 10;
    digits++;
  }

  lcd.setCursor(16 - digits, 1);
  lcd.print(cap);
}

static void drawCursorLine(uint8_t row, const char* text14, bool selected) {
  lcd.setCursor(0, row);
  lcd.print(selected ? '>' : ' ');
  lcd.print(text14);
  lcd.print(selected ? '<' : ' ');
}

static const char* getMainLabel(uint8_t idx) {
  if (idx == 0) return "WORK TO CELLS ";
  if (idx == 1) return "   SERVICES   ";
  return "   SETTINGS   ";
}

static const char* getModeLongName(WorkMode m) {
  if (m == MODE_CHARGE)  return "CHARGE   ";
  if (m == MODE_STORAGE) return "STORAGE  ";
  if (m == MODE_DISCH)   return "DISCHARGE";
  return "BALANCE  ";
}

static const char* getModeShortTag(WorkMode m) {
  if (m == MODE_CHARGE)  return "CHG";
  if (m == MODE_STORAGE) return "STO";
  if (m == MODE_DISCH)   return "DSC";
  return "BAL";
}

void updateUI() {
  uint32_t now = millis();

  uint32_t blinkInterval = blinkPhase ? 750UL : 450UL;
  if (now - lastBlinkMs >= blinkInterval) {
    lastBlinkMs = now;
    blinkPhase = !blinkPhase;
  }

  if (now - lastUiMs < 200) return;
  lastUiMs = now;

  if (uiScreen != lastDrawnScr || sysState != lastDrawnSt || runPage != lastDrawnPg) {
    lcd.clear();
    lastDrawnScr = uiScreen;
    lastDrawnSt  = sysState;
    lastDrawnPg  = runPage;
  }

  if (sysState == STATE_IDLE) {
    if (uiScreen == 0) {
      uint8_t topIdx = (mainIdx == 2) ? 1 : 0;
      drawCursorLine(0, getMainLabel(topIdx),     mainIdx == topIdx);
      drawCursorLine(1, getMainLabel(topIdx + 1), mainIdx == topIdx + 1);
    }
    else if (uiScreen == 1) {
      lcd.setCursor(0, 0);
      if (fieldIdx == 0 && !blinkPhase) lcd.print("      ");
      else lcd.print((selChem == CHEM_LION) ? "Li-Ion" : "Li-Fe ");
      lcd.print("        ");
      if (fieldIdx == 1 && !blinkPhase) lcd.print("  ");
      else { lcd.print(selCells); lcd.print('S'); }

      lcd.setCursor(0, 1);
      if (fieldIdx == 2 && !blinkPhase) lcd.print("         ");
      else lcd.print(getModeLongName(selMode));
      lcd.print("   ");
      if (fieldIdx == 3 && !blinkPhase) lcd.print("    ");
      else { lcd.print(setAmps, 1); lcd.print('A'); }
    }
    else if (uiScreen == 2) {
      drawCursorLine(0, "LAB POWER SUP.", srvIdx == 0);
      drawCursorLine(1, "INTERNAL RES. ", srvIdx == 1);
    }
    else if (uiScreen == 3) {
      lcd.setCursor(0, 0);
      lcd.print("LAB POWER SUPPLY");

      lcd.setCursor(0, 1);
      if (fieldIdx == 0 && !blinkPhase) {
        lcd.print("     ");
      } else {
        lcd.print(setVolts, 1); lcd.print('V');
        if (setVolts < 10.0f) lcd.print(' ');
      }
      lcd.print("       ");
      if (fieldIdx == 1 && !blinkPhase) lcd.print("    ");
      else { lcd.print(setAmps, 1); lcd.print('A'); }
    }
    else if (uiScreen == 4) {
      lcd.setCursor(0, 0);
      lcd.print("INTERNAL RES.   ");
      lcd.setCursor(0, 1);
      lcd.print("CELLS: >"); lcd.print(selCells); lcd.print("S< START");
    }
    else if (uiScreen == 5) {
      drawCursorLine(0, buzzerEnabled ? " BUZZER: ON   " : " BUZZER: OFF  ", true);
      lcd.setCursor(0, 1);
      lcd.print("START:Toggle    ");
    }
    return;
  }

  if (sysState == STATE_RUNNING) {
    unsigned long sec = (now - jobStartMs) / 1000UL;
    if (selMode == MODE_PSU) {
      lcd.setCursor(0, 0); lcd.print("PSU "); lcd.print(psuCcActive ? "CC " : "CV ");
      if (measVout < 10.0f) lcd.print(' '); lcd.print(measVout, 2); lcd.print("V   ");
      lcd.setCursor(0, 1); lcd.print(measAmps, 2); lcd.print("A ");
      float w = measVout * measAmps; if (w < 10.0f) lcd.print(' '); lcd.print(w, 1); lcd.print("W ");
      print2D(sec / 60UL); lcd.print(':'); print2D(sec % 60UL);
      return;
    }

    if (runPage == 0) {
      lcd.setCursor(0, 0);
      lcd.print(getModeShortTag(selMode));
      lcd.print(' ');
      lcd.print(selCells);
      lcd.print("S  ");
      lcd.print((balMask != 0) ? 'B' : ' ');
      lcd.print(' ');
      if (measAmps < 10.0f) lcd.print(' ');
      lcd.print(measAmps, 2);
      lcd.print('A');

      printTimeAndCapRow(sec, capacityMah);
    } else if (runPage == 1) {
      lcd.setCursor(0, 0); lcd.print("B1:"); lcd.print(getCellVoltage(0), 2); lcd.print(" B2:"); lcd.print(cellV2, 2); lcd.print(" ");
      lcd.setCursor(0, 1); lcd.print("B3:"); lcd.print(cellV3, 2); lcd.print(" (INC=pg)");
    } else {
      lcd.setCursor(0, 0); lcd.print("B4:"); lcd.print(cellV4, 2); lcd.print(" B5:"); lcd.print(cellV5, 2); lcd.print(" ");
      lcd.setCursor(0, 1); lcd.print("B6:"); lcd.print(cellV6, 2); lcd.print(" (INC=pg)");
    }
  }
  else if (sysState == STATE_DONE) {
    if (selMode == MODE_IR) {
      uint16_t r0 = *(cellIrMohm + 0);
      if (selCells == 1) {
        lcd.setCursor(0, 0); if (useVoutAs1S) lcd.print("~"); lcd.print("IR 1S: "); lcd.print(r0); lcd.print(" mOhm  ");
        lcd.setCursor(0, 1); lcd.print(r0 <= 45 ? "HEALTH: GOOD    " : (r0 <= 90 ? "HEALTH: FAIR    " : "HEALTH: WEAK    "));
      } else if (runPage == 0) {
        lcd.setCursor(0, 0); if (useVoutAs1S) lcd.print("~"); lcd.print("PACK IR: "); lcd.print(totalPackIrMohm); lcd.print("mO  ");
        lcd.setCursor(0, 1); lcd.print("B1:"); lcd.print(*(cellIrMohm + 0)); lcd.print(" B2:"); lcd.print(*(cellIrMohm + 1)); lcd.print(" (INC)");
      } else {
        lcd.setCursor(0, 0); lcd.print("B3:"); lcd.print(*(cellIrMohm + 2)); lcd.print(" B4:"); lcd.print(*(cellIrMohm + 3)); lcd.print("  ");
        lcd.setCursor(0, 1); lcd.print("B5:"); lcd.print(*(cellIrMohm + 4)); lcd.print(" B6:"); lcd.print(*(cellIrMohm + 5)); lcd.print("  ");
      }
    } else {
      unsigned long sec = (jobEndMs >= jobStartMs) ? ((jobEndMs - jobStartMs) / 1000UL) : 0UL;
      lcd.setCursor(0, 0);
      lcd.print("COMPLETE! (STOP)");
      printTimeAndCapRow(sec, capacityMah);
    }
  } else {
    lcd.setCursor(0, 0); lcd.print("ERR: "); lcd.print(errText); lcd.print("      ");
    lcd.setCursor(0, 1); lcd.print("Press STOP      ");
  }
}
