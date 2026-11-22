#include "ir_tester.h"
#include "config.h"
#include "hal_drivers.h"
#include "sensors.h"
#include "charger_fsm.h"

uint16_t cellIrMohm[6]   = {0, 0, 0, 0, 0, 0};
uint16_t totalPackIrMohm = 0;

// Усиленный оверсэмплинг x128 для высокой точности малых перепадов напряжения
static uint16_t readAdcAvg128(uint8_t ch) {
  ADC1->CHSELR = (1UL << ch);
  ADC1->CR |= ADC_CR_ADSTART;
  while ((ADC1->ISR & ADC_ISR_EOC) == 0);
  (void)ADC1->DR; // Dummy read

  uint32_t sum = 0;
  for (uint8_t i = 0; i < 128; i++) {
    ADC1->CR |= ADC_CR_ADSTART;
    while ((ADC1->ISR & ADC_ISR_EOC) == 0);
    sum += ADC1->DR;
  }
  return (uint16_t)(sum / 128);
}

// Снятие напряжений всех активных ячеек с усреднением x128
static void sampleAllCellsHighRes(float* outV) {
  if (useVoutAs1S) {
    uint16_t rV = readAdcAvg128(7);
    float vTotal = (rV > 12) ? (rV * 0.00888f) : 0.0f;
    float vPerCell = (selCells > 0) ? (vTotal / (float)selCells) : vTotal;
    for (uint8_t i = 0; i < 6; i++) {
      outV[i] = (i < selCells) ? vPerCell : 0.0f;
    }
    return;
  }

  float s[6];
  s[0] = readAdcAvg128(1) * K_B1;
  s[1] = readAdcAvg128(2) * K_B2;
  s[2] = readAdcAvg128(3) * K_B3;
  s[3] = readAdcAvg128(4) * K_B4;
  s[4] = readAdcAvg128(5) * K_B5;
  s[5] = readAdcAvg128(6) * K_B6;

  outV[0] = s[0];
  for (uint8_t i = 1; i < 6; i++) {
    outV[i] = (s[i] > s[i - 1] + 0.1f) ? (s[i] - s[i - 1]) : 0.0f;
  }
}

bool runInternalResistanceTest() {
  for (uint8_t i = 0; i < 6; i++) cellIrMohm[i] = 0;
  totalPackIrMohm = 0;

  // Отключаем балансир и все силовые ключи перед замером
  analogWrite(PIN_PWM_BUCK, 0);
  analogWrite(PIN_PWM_BOOST, 0);
  analogWrite(PIN_PWM_DISCH, 0);
  applyBalanceMask(0);
  delay(50);

  float vPack = getTotalPackVoltage();
  if (vPack < 2.2f) return false;

  // Безопасный импульс нагрузки (~0.65 А на 1S, не более 3.2 Вт на IRFZ44N / D880-Y)
  float targetPulseAmps = 3.2f / vPack;
  if (targetPulseAmps > 0.75f) targetPulseAmps = 0.75f;
  if (targetPulseAmps < 0.20f) targetPulseAmps = 0.20f;

  // Пересчитываем скважность с учётом реального напряжения сборки (база 3.80 В)
  float normAmps = targetPulseAmps * (3.80f / vPack);
  uint16_t pulsePwm = PWM_DISCH_OFFSET + (uint16_t)((normAmps / K_DISCH_CALIB) * 100.0f);
  if (pulsePwm > 360) pulsePwm = 360;

  // Точный ток импульса для расчёта по закону Ома
  float effPwm = (pulsePwm > PWM_DISCH_OFFSET) ? (pulsePwm - PWM_DISCH_OFFSET) : 0.0f;
  float actualPulseAmps = (effPwm / 100.0f) * K_DISCH_CALIB * (vPack / 3.80f);
  if (actualPulseAmps < 0.05f) actualPulseAmps = 0.05f;

  float irAccum[6] = {0, 0, 0, 0, 0, 0};

  // Проводим 2 измерительных цикла для исключения случайных помех
  for (uint8_t pass = 0; pass < 2; pass++) {
    float vIdle[6];
    float vLoad[6];

    // 1. Замер напряжения покоя (ХХ)
    analogWrite(PIN_PWM_DISCH, 0);
    delay(250);
    sampleAllCellsHighRes(vIdle);

    // 2. Подаём калиброванный импульс нагрузки через PA_8 на 450 мс
    analogWrite(PIN_PWM_DISCH, pulsePwm);
    delay(450);
    sampleAllCellsHighRes(vLoad);

    // 3. Мгновенно снимаем нагрузку
    analogWrite(PIN_PWM_DISCH, 0);

    // 4. Вычисляем сопротивление каждой ячейки: R (мОм) = (dV / I) * 1000
    for (uint8_t i = 0; i < selCells; i++) {
      float dv = vIdle[i] - vLoad[i];
      if (dv > 0.001f) {
        irAccum[i] += (dv / actualPulseAmps) * 1000.0f;
      }
    }
  }

  // Усредняем по 2 проходам и считаем суммарное IR сборки
  uint32_t sumPack = 0;
  for (uint8_t i = 0; i < selCells; i++) {
    uint16_t avgMohm = (uint16_t)((irAccum[i] / 2.0f) + 0.5f);
    cellIrMohm[i] = avgMohm;
    sumPack += avgMohm;
  }
  totalPackIrMohm = (uint16_t)sumPack;

  return true;
}
