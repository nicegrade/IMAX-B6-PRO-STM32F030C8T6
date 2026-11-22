#include "lab_psu.h"
#include "config.h"
#include "hal_drivers.h"
#include "charger_fsm.h"
#include "sensors.h"

float setVolts    = 5.00f; // Напряжение БП по умолчанию: 5.0 В
bool  psuCcActive = false;

void resetLabPsuState() {
  psuCcActive = false;
}

bool startLabPsu() {
  // Защита от подключения заряженной батареи выше уставки БП
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

  // Аварийная защита от перенапряжения (OVP)
  if (measVout > setVolts + PSU_OVP_MARGIN && pwmBuck > 10) {
    abortJobWithError("PSU OVERVOLT!");
    return;
  }

  float errV = setVolts - measVout;
  float errI = setAmps  - measAmps;

  // Индикация активного режима стабилизации (CC или CV)
  if (measAmps >= setAmps - 0.02f && errV > 0.05f) {
    psuCcActive = true;
  } else if (measVout >= setVolts - 0.03f) {
    psuCcActive = false;
  }

  // Превышен ток (CC) или напряжение (CV) -> плавно или быстро снижаем накачку
  if (measAmps > setAmps + 0.02f || measVout > setVolts + 0.02f) {
    uint16_t dropStep = (measVout > setVolts + 0.25f || measAmps > setAmps + 0.30f) ? 8 : 2;

    if (pwmBoost > 0) {
      pwmBoost = (pwmBoost >= dropStep) ? (pwmBoost - dropStep) : 0;
    } else if (pwmBuck > 0) {
      pwmBuck = (pwmBuck >= dropStep) ? (pwmBuck - dropStep) : 0;
    }
  }
  // Ниже уставки и по току, и по напряжению -> плавно добираем
  else if (errV > 0.02f && errI > 0.02f) {
    uint16_t upStep = (errV > 1.0f && errI > 0.20f) ? 3 : 1;

    if (pwmBuck < PWM_LIMIT_BUCK_6S) {
      pwmBuck += upStep;
      if (pwmBuck > PWM_LIMIT_BUCK_6S) pwmBuck = PWM_LIMIT_BUCK_6S;
    }

    // Подключаем повышающий каскад (Boost) при напряжении выше порога
    if (setVolts > PSU_BOOST_THRESH_V && pwmBuck > 400 && pwmBoost < PWM_LIMIT_BOOST_6S) {
      pwmBoost += 1;
    }
  }

  if (setVolts <= PSU_BOOST_THRESH_V && pwmBoost > 0) {
    pwmBoost = (pwmBoost >= 8) ? (pwmBoost - 8) : 0;
  }

  analogWrite(PIN_PWM_BUCK, pwmBuck);
  analogWrite(PIN_PWM_BOOST, pwmBoost);
}
