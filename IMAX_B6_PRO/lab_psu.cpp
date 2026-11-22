#include "lab_psu.h"
#include "config.h"
#include "hal_drivers.h"
#include "charger_fsm.h"
#include "sensors.h"

float setVolts    = 5.00f;
bool  psuCcActive = false;

void resetLabPsuState() {
  psuCcActive = false;
}

bool startLabPsu() {
  if (measVout > setVolts + PSU_EXT_VOLT_MARGIN) {
    return false;
  }
  psuCcActive = false;
  return true;
}

void runLabPsuStep() {
  if (pwmDisch != 0) {
    analogWrite(PIN_PWM_DISCH, 0);
    pwmDisch = 0;
  }
  balMask = 0;

  if (measVout > setVolts + PSU_OVP_MARGIN && pwmBuck > 10) {
    abortJobWithError("PSU OVERVOLT!");
    return;
  }

  float errV = setVolts - measVout;
  float errI = setAmps  - measAmps;
  bool vinSagging = (measVin < vinMinLimit);

  if (vinSagging || (measAmps >= setAmps - 0.02f && errV > 0.05f)) {
    psuCcActive = true;
  } else if (measVout >= setVolts - 0.03f) {
    psuCcActive = false;
  }

  // Просадка входного БП, превышен ток (CC) или напряжение (CV) -> снижаем накачку
  if (vinSagging || measAmps > setAmps + 0.02f || measVout > setVolts + 0.02f) {
    uint16_t dropStep = (measVout > setVolts + 0.25f || measAmps > setAmps + 0.30f || measVin < vinMinLimit - 0.30f) ? 8 : 2;

    if (pwmBoost > 0) {
      pwmBoost = (pwmBoost >= dropStep) ? (pwmBoost - dropStep) : 0;
    } else if (pwmBuck > 0) {
      pwmBuck = (pwmBuck >= dropStep) ? (pwmBuck - dropStep) : 0;
    }
  }
  // Ниже уставки и входной БП держит нагрузку -> плавно добираем
  else if (errV > 0.02f && errI > 0.02f && measVin > vinMinLimit + 0.15f) {
    uint16_t upStep = (errV > 1.0f && errI > 0.20f) ? 3 : 1;

    if (pwmBuck < PWM_LIMIT_BUCK_6S) {
      pwmBuck += upStep;
      if (pwmBuck > PWM_LIMIT_BUCK_6S) pwmBuck = PWM_LIMIT_BUCK_6S;
    }

    // Boost включаем только если требуемое напряжение выше текущего входного минус запас 1.5 В
    bool needBoost = (setVolts > PSU_BOOST_THRESH_V) && (setVolts + 1.5f > measVin);
    if (needBoost && pwmBuck > 400 && pwmBoost < PWM_LIMIT_BOOST_6S) {
      pwmBoost += 1;
    }
  }

  bool needBoost = (setVolts > PSU_BOOST_THRESH_V) && (setVolts + 1.5f > measVin);
  if (!needBoost && pwmBoost > 0) {
    pwmBoost = (pwmBoost >= 8) ? (pwmBoost - 8) : 0;
  }

  analogWrite(PIN_PWM_BUCK, pwmBuck);
  analogWrite(PIN_PWM_BOOST, pwmBoost);
}
