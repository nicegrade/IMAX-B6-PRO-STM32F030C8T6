#ifndef IR_TESTER_H
#define IR_TESTER_H

#include <Arduino.h>

// Результаты замера внутреннего сопротивления (в мОм)
extern uint16_t cellIrMohm[6];
extern uint16_t totalPackIrMohm;

// Запуск и выполнение замера внутреннего сопротивления
bool runInternalResistanceTest();

#endif
