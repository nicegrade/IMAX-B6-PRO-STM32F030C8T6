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
extern uint8_t diagIdx;
extern uint8_t fieldIdx;
extern uint8_t psuRunField;
extern uint8_t runPage;
extern bool    blinkPhase;
extern unsigned long lastBlinkMs;
extern unsigned long lastPsuEditMs;

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

static void drawCursorLine(uint8_t row, const char* text14, bool selected, bool visible) {
  lcd.setCursor(0, row);
  if (selected && !visible) {
    lcd.print("                ");
    return;
  }
  lcd.print(selected ? '>' : ' ');
  lcd.print(text14);
  lcd.print(selected ? '<' : ' ');
}

static const char* getMainLabel(uint8_t idx) {
  if (idx == 0) return "WORK TO CELLS ";
  if (idx == 1) return " DIAGNOSTICS  ";
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

  // Медленное комфортное мигание во всех меню и в рабочем режиме PSU
  uint32_t onMs  = 1150UL;
  uint32_t offMs = 350UL;
  if (sysState == STATE_RUNNING && selMode == MODE_PSU) {
    onMs  = 1300UL;
    offMs = 350UL;
  } else if (sysState == STATE_IDLE && (uiScreen == 1 || uiScreen == 3 || uiScreen == 4)) {
    onMs  = 950UL;
    offMs = 350UL;
  }

  uint32_t blinkInterval = blinkPhase ? onMs : offMs;
  if (now - lastBlinkMs >= blinkInterval) {
    lastBlinkMs = now;
    blinkPhase = !blinkPhase;
  }

  if (now - lastUiMs < 120) return;
  lastUiMs = now;

  if (uiScreen != lastDrawnScr || sysState != lastDrawnSt || runPage != lastDrawnPg) {
    lcd.clear();
    lastDrawnScr = uiScreen;
    lastDrawnSt  = sysState;
    lastDrawnPg  = runPage;
  }

  if (sysState == STATE_IDLE) {
    if (uiScreen == 0) { // SCR_MAIN
      uint8_t topIdx = (mainIdx == 2) ? 1 : 0;
      drawCursorLine(0, getMainLabel(topIdx),     mainIdx == topIdx,     blinkPhase);
      drawCursorLine(1, getMainLabel(topIdx + 1), mainIdx == topIdx + 1, blinkPhase);
    }
    else if (uiScreen == 1) { // SCR_CELLS
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
    else if (uiScreen == 2) { // SCR_DIAGNOSTICS
      drawCursorLine(0, "LAB POWER SUP.", diagIdx == 0, blinkPhase);
      drawCursorLine(1, "INTERNAL RES. ", diagIdx == 1, blinkPhase);
    }
    else if (uiScreen == 3) { // SCR_PSU_SET
      lcd.setCursor(0, 0);
      lcd.print("LAB POWER SUPPLY");

      // Округляем до десятых, чтобы 10.0V (9.999999f) не вызывало лишний пробел и сдвиг тока!
      int16_t vTenths = (int16_t)(setVolts * 10.0f + 0.5f);

      lcd.setCursor(0, 1);
      if (fieldIdx == 0 && !blinkPhase) {
        lcd.print("     ");
      } else {
        lcd.print(vTenths / 10);
        lcd.print('.');
        lcd.print(vTenths % 10);
        lcd.print('V');
        if (vTenths < 100) lcd.print(' ');
      }
      lcd.print("       ");
      if (fieldIdx == 1 && !blinkPhase) lcd.print("    ");
      else { lcd.print(setAmps, 1); lcd.print('A'); }
    }
    else if (uiScreen == 4) { // SCR_IR_SET
      lcd.setCursor(0, 0);
      lcd.print("INTERNAL RESIST ");
      lcd.setCursor(0, 1);
      lcd.print("CELLS: ");
      if (!blinkPhase) lcd.print("  ");
      else { lcd.print(selCells); lcd.print('S'); }
      lcd.print("  START");
    }
    else if (uiScreen == 5) { // SCR_SETTINGS
      drawCursorLine(0, buzzerEnabled ? " BUZZER: ON   " : " BUZZER: OFF  ", true, blinkPhase);
      lcd.setCursor(0, 1);
      lcd.print("START:Toggle    ");
    }
    return;
  }

  if (sysState == STATE_RUNNING) {
    unsigned long sec = (now - jobStartMs) / 1000UL;
    if (selMode == MODE_PSU) {
      // В течение 1.5 сек после нажатия кнопок в RUN PSU показываем целевую уставку (setVolts / setAmps)
      bool showTarget = (lastPsuEditMs != 0) && (now - lastPsuEditMs < 1500UL);
      float dispV = (showTarget && psuRunField == 0) ? setVolts : measVout;
      float dispA = (showTarget && psuRunField == 1) ? setAmps  : measAmps;

      int16_t vHundredths = (int16_t)(dispV * 100.0f + 0.5f);
      if (vHundredths < 0) vHundredths = 0;

      // Строка 1 (16 симв.): "PSU 12.30V 6.56A"
      lcd.setCursor(0, 0);
      lcd.print("PSU ");
      if (psuRunField == 0 && !blinkPhase) {
        lcd.print("      ");
      } else {
        if (vHundredths < 1000) lcd.print(' ');
        lcd.print(vHundredths / 100);
        lcd.print('.');
        print2D(vHundredths % 100);
        lcd.print('V');
      }
      lcd.print(' ');
      if (psuRunField == 1 && !blinkPhase) {
        lcd.print("     ");
      } else {
        lcd.print(dispA, 2);
        lcd.print('A');
      }

      // Строка 2 (16 симв.): Ватты слева ("12.4W   "), время hh:mm:ss справа ("00:01:23")
      lcd.setCursor(0, 1);
      float w = measVout * measAmps;
      lcd.print(w, 1);
      lcd.print("W       ");

      lcd.setCursor(8, 1);
      print2D(sec / 3600UL); lcd.print(':');
      print2D((sec % 3600UL) / 60UL); lcd.print(':');
      print2D(sec % 60UL);
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
