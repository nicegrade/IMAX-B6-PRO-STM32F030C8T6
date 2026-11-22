#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =========================================================================
// РАСПИНОВКА ПО ПАСПОРТУ (STM32F030C8T6, РЕВИЗИЯ БЕЗ LM358)
// =========================================================================
// Дисплей 1602: RS=PB_9, E=PB_8, D4=PB_7, D5=PB_6, D6=PB_5, D7=PB_3
const uint8_t LCD_RS = PB_9;
const uint8_t LCD_E  = PB_8;
const uint8_t LCD_D4 = PB_7;
const uint8_t LCD_D5 = PB_6;
const uint8_t LCD_D6 = PB_5;
const uint8_t LCD_D7 = PB_3;

// Отладка UART TX (пин 4 разъёма J8)
const uint8_t PIN_UART_RX = PA_15;
const uint8_t PIN_UART_TX = PA_14;

// Кнопки управления
const uint8_t BTN_STOP   = PB_2;
const uint8_t BTN_DEC    = PB_10;
const uint8_t BTN_INC    = PB_11;
const uint8_t BTN_START  = PB_12;
const uint8_t PIN_BUZZER = PB_4;

// Силовые выходы (TIM1: CH1=PA_8, CH2=PA_9, CH3=PA_10)
const uint8_t PIN_OUT_EN    = PB_13; // ИНВЕРСНАЯ: LOW = ВКЛ, HIGH = ВЫКЛ
const uint8_t PIN_PWM_BUCK  = PA_10; // TIM1_CH3 (Buck - Заряд)
const uint8_t PIN_PWM_BOOST = PA_9;  // TIM1_CH2 (Boost FQP50N06)
const uint8_t PIN_PWM_DISCH = PA_8;  // TIM1_CH1 (Разряд D880-Y)

// Пины балансировки (HIGH = ВКЛ, LOW = ВЫКЛ)
const uint8_t PIN_BAL_1 = PF_7;
const uint8_t PIN_BAL_2 = PF_6;
const uint8_t PIN_BAL_3 = PA_12;
const uint8_t PIN_BAL_4 = PA_11;
const uint8_t PIN_BAL_5 = PB_15;
const uint8_t PIN_BAL_6 = PB_14;

// =========================================================================
// КАЛИБРОВОЧНЫЕ КОЭФФИЦИЕНТЫ АЦП (12-БИТ: 0..4095, VREF = 3.30 В)
// =========================================================================
const float K_B1 = (3.30f / 4095.0f) * 1.383f;
const float K_B2 = (3.30f / 4095.0f) * 2.752f;
const float K_B3 = (3.30f / 4095.0f) * 4.191f;
const float K_B4 = (3.30f / 4095.0f) * 5.688f;
const float K_B5 = (3.30f / 4095.0f) * 6.802f;
const float K_B6 = (3.30f / 4095.0f) * 8.045f;

const float K_VOUT        = 0.00888f; // Выходное напряжение V_OUT (PA_7, двухточечная калибровка)
const float K_SHUNT_A     = 0.075f;   // Ток заряда/БП (А на единицу АЦП PA_0)
const float K_DISCH_CALIB = 0.224f;    // Ток разряда (А на 100 ед. 10-бит ШИМ при 3.8 В)

// =========================================================================
// БЕЗОПАСНЫЕ ЛИМИТЫ 10-БИТНОГО ШИМ (0..1023, 20 кГц)
// =========================================================================
const uint16_t PWM_LIMIT_BUCK_1S  = 550; // До ~3.5 - 5.0 А на 1S (следить за мощностью БП!)
const uint16_t PWM_LIMIT_BUCK_3S  = 750; // До ~5.0 А на 2S-3S
const uint16_t PWM_LIMIT_BUCK_6S  = 900; // Почти полное открытие Buck в режиме Boost
const uint16_t PWM_LIMIT_BOOST_6S = 420; // Достаточно для 2.0-2.5 А на 4S-6S
const uint16_t PWM_LIMIT_DISCH    = 520; // Макс. разряд (до ~1.0-1.2 А на 1S)
const uint16_t PWM_DISCH_OFFSET   = 2;   // Порог открытия транзистора D880-Y (ед. ШИМ)

// Защита входного БП (12 В / 2 А = 24 Вт -> потолок 19.5 Вт) и ключей при разряде (5.5 Вт)
const float MAX_OUTPUT_WATTS = 19.5f;
const float MAX_DISCH_WATTS  = 5.5f;

// =========================================================================
// ПОРОГИ ХИМИИ И РЕЖИМА ЛАБОРАТОРНОГО БП (PSU)
// =========================================================================
const float LION_CHG_V   = 4.20f;
const float LION_STO_V   = 3.80f;
const float LION_DSC_V   = 3.10f;
const float LION_MAX_V   = 4.26f;

const float LIFE_CHG_V   = 3.65f;
const float LIFE_STO_V   = 3.30f;
const float LIFE_DSC_V   = 2.60f;
const float LIFE_MAX_V   = 3.72f;

const float PSU_MIN_VOLTS       = 1.0f;
const float PSU_MAX_VOLTS       = 24.0f;
const float PSU_BOOST_THRESH_V  = 10.5f; // Порог подключения повышающего каскада Boost
const float PSU_EXT_VOLT_MARGIN = 0.50f; // Допуск внешнего напряжения на клеммах при старте
const float PSU_OVP_MARGIN      = 0.80f; // Порог аварийной отсечки перенапряжения (OVP)

#endif
