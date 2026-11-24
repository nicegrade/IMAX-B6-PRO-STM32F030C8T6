#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =========================================================================
// РАСПИНОВКА ПО ПАСПОРТУ (STM32F030C8T6, РЕВИЗИЯ БЕЗ LM358)
// =========================================================================
const uint8_t LCD_RS = PB_9;
const uint8_t LCD_E  = PB_8;
const uint8_t LCD_D4 = PB_7;
const uint8_t LCD_D5 = PB_6;
const uint8_t LCD_D6 = PB_5;
const uint8_t LCD_D7 = PB_3;

const uint8_t PIN_UART_RX = PA_15;
const uint8_t PIN_UART_TX = PA_14;

const uint8_t BTN_STOP   = PB_2;
const uint8_t BTN_DEC    = PB_10;
const uint8_t BTN_INC    = PB_11;
const uint8_t BTN_START  = PB_12;
const uint8_t PIN_BUZZER = PB_4;

// Аналоговый мультиплексор U4 (74HC4051, выход Z -> PB_1 / ADC1_CH9)
const uint8_t PIN_MUX_A  = PC_13; // Ножка 2 U2 -> вывод 11 U4 (бит 0)
const uint8_t PIN_MUX_B  = PC_14; // Ножка 3 U2 -> вывод 10 U4 (бит 1)
const uint8_t ADC_CH_MUX = 9;     // PB_1 -> ADC1_CH9

// Силовые выходы (TIM1: CH1=PA_8, CH2=PA_9, CH3=PA_10)
const uint8_t PIN_OUT_EN    = PB_13; // ИНВЕРСНАЯ: LOW = ВКЛ, HIGH = ВЫКЛ
const uint8_t PIN_PWM_BUCK  = PA_10; // TIM1_CH3 (Buck - Заряд / БП)
const uint8_t PIN_PWM_BOOST = PA_9;  // TIM1_CH2 (Boost FQP50N06)
const uint8_t PIN_PWM_DISCH = PA_8;  // TIM1_CH1 (Разряд IRFZ44N)

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

const float K_VOUT      = 0.00888f;  // Выходное напряжение V_OUT (PA_7)
const float K_SHUNT_A   = 0.075f;    // Ток заряда/БП (PA_0)
const float K_DISCH_ADC = 0.001689f; // Реальный ток разряда по каналу Y2 мультиплексора
const float K_VIN       = 0.008898f; // Входное напряжение V_IN (калибровка 12.11 В / 1361 ед.)

// =========================================================================
// БЕЗОПАСНЫЕ ЛИМИТЫ 10-БИТНОГО ШИМ (0..1023, 20 кГц)
// =========================================================================
const uint16_t PWM_LIMIT_BUCK_1S  = 550;
const uint16_t PWM_LIMIT_BUCK_3S  = 750;
const uint16_t PWM_LIMIT_BUCK_6S  = 900;
const uint16_t PWM_LIMIT_BOOST_6S = 420;
const uint16_t PWM_LIMIT_DISCH    = 840; // Позволяет замкнутому контуру выйти на полные 2.00 А на 1S
const uint16_t PWM_DISCH_OFFSET   = 2;

// Защита входного БП по напряжению V_IN и тепловой лимит разряда
const float VIN_ABS_MIN_VOLTS = 9.5f;  // Аварийное отключение при полном провале входа
const float VIN_12V_SAG_LIMIT = 10.6f; // Порог стабилизации просадки для 12В адаптера
const float MAX_HARD_WATTS    = 55.0f; // Безопасный потолок мощности дросселя и диода (50-55 Вт)
const float MAX_DISCH_WATTS   = 7.5f;  // Тепловой потолок линейного разряда (с учётом теплоотвода на корпус)
const float MAX_DISCH_AMPS    = 2.0f;  // Аппаратный потолок тока разряда


// =========================================================================
// ПОРОГИ ВСЕХ ЛИТИЕВЫХ ХИМИЙ ОРИГИНАЛЬНОГО IMAX B6 (НА 1 ЯЧЕЙКУ)
// =========================================================================
// 1. Li-Ion (номинал 3.6 В): Заряд 4.10 В, Хранение 3.75 В, Разряд 3.00 В
const float LION_CHG_V       = 4.10f;
const float LION_STO_V       = 3.75f;
const float LION_DSC_V       = 3.00f;
const float LION_MAX_V       = 4.18f;
const float LION_START_MIN_V = 2.75f;

// 2. Li-Po (номинал 3.7 В): Заряд 4.20 В, Хранение 3.85 В, Разряд 3.10 В
const float LIPO_CHG_V       = 4.20f;
const float LIPO_STO_V       = 3.85f;
const float LIPO_DSC_V       = 3.10f;
const float LIPO_MAX_V       = 4.26f;
const float LIPO_START_MIN_V = 2.75f;

// 3. Li-Fe / LiFePO4 (номинал 3.3 В): Заряд 3.65 В, Хранение 3.30 В, Разряд 2.60 В
const float LIFE_CHG_V       = 3.65f;
const float LIFE_STO_V       = 3.30f;
const float LIFE_DSC_V       = 2.60f;
const float LIFE_MAX_V       = 3.72f;
const float LIFE_START_MIN_V = 2.30f;

// 4. Li-HV (номинал 3.8 В): Заряд 4.35 В, Хранение 3.90 В, Разряд 3.20 В
const float LIHV_CHG_V       = 4.35f;
const float LIHV_STO_V       = 3.90f;
const float LIHV_DSC_V       = 3.20f;
const float LIHV_MAX_V       = 4.42f;
const float LIHV_START_MIN_V = 2.80f;

// Параметры лабораторного БП (PSU)
const float PSU_MIN_VOLTS       = 1.0f;
const float PSU_MAX_VOLTS       = 24.0f;
const float PSU_BOOST_THRESH_V  = 10.5f;
const float PSU_EXT_VOLT_MARGIN = 0.50f;
const float PSU_OVP_MARGIN      = 0.80f;

#endif
