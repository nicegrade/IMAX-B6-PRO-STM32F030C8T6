#include "hal_drivers.h"
#include "config.h"

bool buzzerEnabled = true;

void initHardwarePinsSafe() {
  pinMode(PIN_OUT_EN, OUTPUT);    digitalWrite(PIN_OUT_EN, HIGH); // V21 ВЫКЛ
  pinMode(PIN_PWM_BUCK, OUTPUT);  digitalWrite(PIN_PWM_BUCK, LOW);
  pinMode(PIN_PWM_BOOST, OUTPUT); digitalWrite(PIN_PWM_BOOST, LOW);
  pinMode(PIN_PWM_DISCH, OUTPUT); digitalWrite(PIN_PWM_DISCH, LOW);

  // Полная разблокировка домена RTC/Backup для PC_13 (ножка 2) и PC_14 (ножка 3)
  RCC->AHBENR  |= RCC_AHBENR_GPIOCEN;
  RCC->APB1ENR |= RCC_APB1ENR_PWREN;
  PWR->CR      |= PWR_CR_DBP;
  RCC->BDCR    &= ~(RCC_BDCR_LSEON | RCC_BDCR_LSEBYP);
  RTC->WPR      = 0xCA;
  RTC->WPR      = 0x53;
  RTC->TAFCR    = 0;

  // Настройка PC_13 и PC_14 напрямую как Push-Pull Output (01)
  GPIOC->MODER  &= ~((3UL << (13 * 2)) | (3UL << (14 * 2)));
  GPIOC->MODER  |=  ((1UL << (13 * 2)) | (1UL << (14 * 2)));
  GPIOC->OTYPER &= ~((1UL << 13) | (1UL << 14));
  GPIOC->BSRR    =  (1UL << (13 + 16)) | (1UL << (14 + 16)); // LOW

  pinMode(PIN_BAL_1, OUTPUT);
  pinMode(PIN_BAL_2, OUTPUT);
  pinMode(PIN_BAL_3, OUTPUT);
  pinMode(PIN_BAL_4, OUTPUT);
  pinMode(PIN_BAL_5, OUTPUT);
  pinMode(PIN_BAL_6, OUTPUT);
  applyBalanceMask(0);
}

void initHardwareADC() {
  RCC->AHBENR  |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;
  RCC->APB2ENR |= RCC_APB2ENR_ADCEN;
  RCC->CR2     |= RCC_CR2_HSI14ON;
  while ((RCC->CR2 & RCC_CR2_HSI14RDY) == 0);

  GPIOA->MODER |= 0x0000FFFF;       // PA0..PA7 -> Analog
  GPIOB->MODER |= (3UL << (1 * 2)); // PB1 -> Analog (ADC1_CH9)

  if ((ADC1->CR & ADC_CR_ADEN) != 0) {
    ADC1->CR |= ADC_CR_ADDIS;
    while ((ADC1->CR & ADC_CR_ADEN) != 0);
  }
  ADC1->CR |= ADC_CR_ADCAL;
  while ((ADC1->CR & ADC_CR_ADCAL) != 0);

  ADC1->SMPR = ADC_SMPR_SMP_0 | ADC_SMPR_SMP_1 | ADC_SMPR_SMP_2;
  ADC1->ISR |= ADC_ISR_ADRDY;
  ADC1->CR  |= ADC_CR_ADEN;
  while ((ADC1->ISR & ADC_ISR_ADRDY) == 0);
}

void initUltrasonicPWM() {
  analogWriteFrequency(20000); // 20 кГц (устраняет писк дросселя)
  analogWriteResolution(10);   // 10 бит (0..1023)
}

uint16_t readAdcAvg16(uint8_t ch) {
  ADC1->CHSELR = (1UL << ch);
  ADC1->CR |= ADC_CR_ADSTART;
  while ((ADC1->ISR & ADC_ISR_EOC) == 0);
  (void)ADC1->DR; // Dummy read

  uint32_t sum = 0;
  for (uint8_t i = 0; i < 16; i++) {
    ADC1->CR |= ADC_CR_ADSTART;
    while ((ADC1->ISR & ADC_ISR_EOC) == 0);
    sum += ADC1->DR;
  }
  return (uint16_t)(sum / 16);
}

uint16_t readMuxAvg16(uint8_t muxCh) {
  // PC_13 = A (вывод 11 U4), PC_14 = B (вывод 10 U4)
  if (muxCh & 0x01) GPIOC->BSRR = (1UL << 13);
  else              GPIOC->BSRR = (1UL << (13 + 16));

  if (muxCh & 0x02) GPIOC->BSRR = (1UL << 14);
  else              GPIOC->BSRR = (1UL << (14 + 16));

  delay(2);
  return readAdcAvg16(ADC_CH_MUX);
}

float readShuntOversampled64() {
  ADC1->CHSELR = 1UL; // CH0 (PA_0)
  ADC1->CR |= ADC_CR_ADSTART;
  while ((ADC1->ISR & ADC_ISR_EOC) == 0);
  (void)ADC1->DR; // Dummy read

  uint32_t sum = 0;
  for (uint8_t i = 0; i < 64; i++) {
    ADC1->CR |= ADC_CR_ADSTART;
    while ((ADC1->ISR & ADC_ISR_EOC) == 0);
    sum += ADC1->DR;
    delayMicroseconds(15); // Усреднение по периодам ШИМ 20 кГц
  }
  return (float)sum / 64.0f;
}

void softBeep(uint16_t freqHz, uint16_t durationMs, bool forceAlarm) {
  if (!buzzerEnabled && !forceAlarm) return;
  uint32_t hp = 500000UL / freqHz;
  uint32_t cycles = ((uint32_t)durationMs * 1000UL) / (hp * 2);
  for (uint32_t i = 0; i < cycles; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delayMicroseconds(hp);
    digitalWrite(PIN_BUZZER, LOW);
    delayMicroseconds(hp);
  }
}

void applyBalanceMask(uint8_t mask) {
  digitalWrite(PIN_BAL_1, (mask & 0x01) ? HIGH : LOW);
  digitalWrite(PIN_BAL_2, (mask & 0x02) ? HIGH : LOW);
  digitalWrite(PIN_BAL_3, (mask & 0x04) ? HIGH : LOW);
  digitalWrite(PIN_BAL_4, (mask & 0x08) ? HIGH : LOW);
  digitalWrite(PIN_BAL_5, (mask & 0x10) ? HIGH : LOW);
  digitalWrite(PIN_BAL_6, (mask & 0x20) ? HIGH : LOW);
}

void hardwareAllOff() {
  analogWrite(PIN_PWM_BUCK, 0);
  analogWrite(PIN_PWM_BOOST, 0);
  analogWrite(PIN_PWM_DISCH, 0);
  digitalWrite(PIN_OUT_EN, HIGH); // V21 ВЫКЛ
  applyBalanceMask(0);
}
