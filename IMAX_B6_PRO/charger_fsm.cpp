#include "charger_fsm.h"
#include "config.h"
#include "hal_drivers.h"
#include "sensors.h"
#include "lab_psu.h"
#include "ir_tester.h"
#include "ui_telemetry.h"

ChemType selChem  = CHEM_LION;
WorkMode selMode  = MODE_CHARGE;
uint8_t  selCells = 1;
float    setAmps  = 0.50f;

RunState    sysState    = STATE_IDLE;
const char* errText     = "";
bool        useVoutAs1S = false;
float       vinMinLimit = VIN_12V_SAG_LIMIT;

uint16_t pwmBuck  = 0;
uint16_t pwmBoost = 0;
uint16_t pwmDisch = 0;
uint8_t  balMask  = 0;

float         capacityMah = 0.0f;
unsigned long jobStartMs  = 0;
unsigned long jobEndMs    = 0;

static unsigned long lastCapMs   = 0;
static unsigned long lastCtrlMs  = 0;
static uint8_t       cvDoneTicks = 0;

static float getTargetChargeV() { return (selChem == CHEM_LION) ? LION_CHG_V : LIFE_CHG_V; }
static float getTargetStoreV()  { return (selChem == CHEM_LION) ? LION_STO_V : LIFE_STO_V; }
static float getTargetDischV()  { return (selChem == CHEM_LION) ? LION_DSC_V : LIFE_DSC_V; }
static float getMaxSafeCellV()  { return (selChem == CHEM_LION) ? LION_MAX_V : LIFE_MAX_V; }

static void resetPwmVars() {
  pwmBuck = pwmBoost = pwmDisch = 0;
  balMask = 0;
  resetLabPsuState();
  hardwareAllOff();
}

void abortJobWithError(const char* msg) {
  resetPwmVars();
  measAmps = 0.0f;
  errText = msg;
  sysState = STATE_ERROR;
  sendTelemetryNow("ERR");
  softBeep(1200, 200, true); delay(80); softBeep(1200, 200, true);
}

static void triggerDone() {
  resetPwmVars();
  measAmps = 0.0f;
  jobEndMs = millis();
  sysState = STATE_DONE;
  sendTelemetryNow(nullptr);
  softBeep(2200, 120); delay(60); softBeep(2600, 120); delay(60); softBeep(3000, 220);
}

void startJob() {
  resetPwmVars();
  delay(20);

  digitalWrite(PIN_OUT_EN, LOW);
  delay(40);
  shuntZero = readShuntOversampled64();
  updateAllMeasurements();

  if (measVin < VIN_ABS_MIN_VOLTS) {
    abortJobWithError("INPUT VOLT LOW!");
    return;
  }

  // Адаптивный порог просадки: для 12В БП держим >= 10.6В, для 15-19В БП держим >= 85% от ХХ
  vinMinLimit = (measVin >= 14.5f) ? (measVin * 0.85f) : VIN_12V_SAG_LIMIT;
  if (vinMinLimit < VIN_12V_SAG_LIMIT) vinMinLimit = VIN_12V_SAG_LIMIT;

  useVoutAs1S = false;

  if (selMode == MODE_PSU) {
    if (!startLabPsu()) { abortJobWithError("EXT VOLT HIGH!"); return; }
    useVoutAs1S = true;
    capacityMah = 0.0f;
    jobStartMs = jobEndMs = lastCapMs = lastCtrlMs = millis();
    sysState = STATE_RUNNING;
    sendTelemetryNow(nullptr);
    softBeep(2800, 100);
    return;
  }

  if (cellV1 >= 0.50f) {
    for (uint8_t i = 0; i < selCells; i++) {
      float vc = getCellVoltage(i);
      if (vc < 2.20f || vc > 4.35f) { abortJobWithError("CHECK BAL CABLE"); return; }
    }
    if (measVout < (selCells * 2.0f)) { abortJobWithError("NO MAIN BATT"); return; }
  } else {
    if (selMode == MODE_BALANCE) { abortJobWithError("NEED BAL CABLE"); return; }
    if (measVout >= (selCells * 2.20f) && measVout <= (selCells * 4.35f)) {
      useVoutAs1S = true;
    } else {
      abortJobWithError("NO BATTERY"); return;
    }
  }

  if (selMode == MODE_IR) {
    sysState = STATE_RUNNING;
    sendTelemetryNow(nullptr);
    if (runInternalResistanceTest()) triggerDone();
    else abortJobWithError("IR TEST FAIL");
    return;
  }

  capacityMah = 0.0f;
  jobStartMs = jobEndMs = lastCapMs = lastCtrlMs = millis();
  cvDoneTicks = 0;
  sysState = STATE_RUNNING;
  sendTelemetryNow(nullptr);
  softBeep(2600, 120);
}

void stopJobManual() {
  resetPwmVars();
  measAmps = 0.0f;
  capacityMah = 0.0f;
  sysState = STATE_IDLE;
  sendTelemetryNow("STOP");
  softBeep(1800, 90);
}

void runControlStep() {
  if (sysState != STATE_RUNNING) return;

  uint16_t stepMs = (selMode == MODE_PSU) ? 50 : 150;
  if (millis() - lastCtrlMs < stepMs) return;
  lastCtrlMs = millis();

  updateAllMeasurements();

  if (measVin < VIN_ABS_MIN_VOLTS) {
    abortJobWithError("INPUT VOLT LOW!");
    return;
  }

  unsigned long now = millis();
  capacityMah += (measAmps * 1000.0f) * ((now - lastCapMs) / 3600000.0f);
  lastCapMs = now;

  if (selMode == MODE_PSU) { runLabPsuStep(); return; }

  float minV = 9.99f, maxV = 0.00f;
  for (uint8_t i = 0; i < selCells; i++) {
    float v = getCellVoltage(i);
    if (v < 1.80f) { abortJobWithError("CELL LOW / OPEN"); return; }
    if (v > getMaxSafeCellV()) { abortJobWithError("OVERVOLTAGE!"); return; }
    if (v < minV) minV = v;
    if (v > maxV) maxV = v;
  }

  WorkMode activeAction = selMode;
  if (selMode == MODE_STORAGE) {
    float vStore = getTargetStoreV();
    if (maxV > vStore + 0.04f) activeAction = MODE_DISCH;
    else if (minV < vStore - 0.04f) activeAction = MODE_CHARGE;
    else { triggerDone(); return; }
  }

  if (activeAction == MODE_CHARGE) {
    analogWrite(PIN_PWM_DISCH, 0); pwmDisch = 0;
    float vTarget = (selMode == MODE_STORAGE) ? getTargetStoreV() : getTargetChargeV();

    uint8_t newBal = 0;
    if (selCells > 1 && !useVoutAs1S && maxV > 3.85f) {
      for (uint8_t i = 0; i < selCells; i++) {
        if (getCellVoltage(i) - minV > 0.015f) newBal |= (1 << i);
      }
    }
    if (newBal != balMask) { balMask = newBal; applyBalanceMask(balMask); }

    uint16_t maxSafeBuck = (selCells == 1) ? PWM_LIMIT_BUCK_1S : ((selCells <= 3) ? PWM_LIMIT_BUCK_3S : PWM_LIMIT_BUCK_6S);

    float packV = getTotalPackVoltage();
    float effSetAmps = (packV > 1.0f && (setAmps * packV) > MAX_HARD_WATTS) ? (MAX_HARD_WATTS / packV) : setAmps;
    float cutOffAmps = max(0.05f, effSetAmps * 0.10f);

    bool vinSagging = (measVin < vinMinLimit);

    if (!vinSagging && maxV >= (vTarget - 0.01f) && measAmps <= cutOffAmps) {
      if (++cvDoneTicks >= 20) { cvDoneTicks = 0; triggerDone(); return; }
    } else cvDoneTicks = 0;

    // Если входной БП просел до порога — плавно снижаем накачку, выравнивая баланс мощности
    if (vinSagging) {
      uint16_t drop = (measVin < vinMinLimit - 0.30f) ? 6 : 2;
      if (pwmBoost > 0) pwmBoost = (pwmBoost >= drop) ? (pwmBoost - drop) : 0;
      else if (pwmBuck > 8) pwmBuck -= drop;
    } else if (maxV >= vTarget) {
      if (pwmBoost > 0) pwmBoost = (pwmBoost >= 4) ? (pwmBoost - 4) : 0;
      else if (pwmBuck > 8) pwmBuck -= 2;
    } else {
      // Набираем ток только если есть запас по входному напряжению (+0.15 В гистерезис)
      if (measAmps < effSetAmps - 0.03f && pwmBuck < maxSafeBuck && measVin > vinMinLimit + 0.15f) {
        pwmBuck += 2;
      } else if (measAmps > effSetAmps + 0.03f) {
        if (pwmBoost > 0) pwmBoost = (pwmBoost >= 2) ? (pwmBoost - 2) : 0;
        else if (pwmBuck > 4) pwmBuck -= 2;
      }
    }

    // Повышающий каскад Boost подключаем только при достаточном напряжении БП (и если вход < выхода)
    if (!vinSagging && selCells >= 4 && pwmBuck > 350 && maxV < vTarget && (packV + 1.5f > measVin)) {
      if (measAmps < effSetAmps - 0.03f && pwmBoost < PWM_LIMIT_BOOST_6S && measVin > vinMinLimit + 0.15f) {
        pwmBoost += 2;
      }
    } else if (selCells < 4 || pwmBuck <= 350 || (packV + 1.5f <= measVin)) {
      if (pwmBoost > 0) pwmBoost = (pwmBoost >= 4) ? (pwmBoost - 4) : 0;
    }

    analogWrite(PIN_PWM_BUCK, pwmBuck);
    analogWrite(PIN_PWM_BOOST, pwmBoost);
  }
  else if (activeAction == MODE_DISCH) {
    analogWrite(PIN_PWM_BUCK, 0); analogWrite(PIN_PWM_BOOST, 0);
    pwmBuck = pwmBoost = 0; balMask = 0; applyBalanceMask(0);

    float vCut = (selMode == MODE_STORAGE) ? getTargetStoreV() : getTargetDischV();
    if (minV <= vCut) { triggerDone(); return; }

    float packV = max(2.0f, getTotalPackVoltage());
    float safeAmps = ((setAmps * packV) > MAX_DISCH_WATTS) ? (MAX_DISCH_WATTS / packV) : setAmps;

    float errI = safeAmps - measAmps;
    if (pwmDisch < PWM_DISCH_OFFSET) {
      pwmDisch = PWM_DISCH_OFFSET;
    } else if (errI > 0.02f && pwmDisch < PWM_LIMIT_DISCH) {
      uint16_t stepUp = (errI > 0.25f) ? 6 : 2;
      pwmDisch = min((uint16_t)PWM_LIMIT_DISCH, (uint16_t)(pwmDisch + stepUp));
    } else if (errI < -0.02f && pwmDisch > PWM_DISCH_OFFSET) {
      uint16_t stepDn = (errI < -0.25f) ? 8 : 2;
      pwmDisch = (pwmDisch > PWM_DISCH_OFFSET + stepDn) ? (pwmDisch - stepDn) : PWM_DISCH_OFFSET;
    }

    analogWrite(PIN_PWM_DISCH, pwmDisch);
  }
  else if (activeAction == MODE_BALANCE) {
    analogWrite(PIN_PWM_BUCK, 0); analogWrite(PIN_PWM_BOOST, 0); analogWrite(PIN_PWM_DISCH, 0);
    if (selCells == 1 || useVoutAs1S) { abortJobWithError("NEED BAL CABLE"); return; }

    uint8_t newBal = 0;
    for (uint8_t i = 0; i < selCells; i++) {
      if (getCellVoltage(i) - minV > 0.012f) newBal |= (1 << i);
    }
    balMask = newBal; applyBalanceMask(balMask);
    if (balMask == 0) { triggerDone(); return; }
  }
}
