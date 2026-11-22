#ifndef LAB_PSU_H
#define LAB_PSU_H

#include <Arduino.h>

// Настройки и состояние лабораторного БП
extern float setVolts;     // Целевое напряжение (1.0 .. 24.0 В)
extern bool  psuCcActive;  // true = активен режим ограничения тока (CC), false = CV

// Функции модуля
bool startLabPsu();        // Проверка выхода и запуск БП
void runLabPsuStep();      // Быстрый цикл стабилизации CC/CV (50 мс) + защита OVP
void resetLabPsuState();   // Сброс флагов при остановке

#endif
