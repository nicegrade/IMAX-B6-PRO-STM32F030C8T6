#ifndef CHARGER_FSM_H
#define CHARGER_FSM_H

#include <Arduino.h>

enum ChemType : uint8_t { CHEM_LION = 0, CHEM_LIPO = 1, CHEM_LIFE = 2, CHEM_LIHV = 3 };
enum WorkMode : uint8_t { MODE_CHARGE = 0, MODE_STORAGE = 1, MODE_DISCH = 2, MODE_BALANCE = 3, MODE_PSU = 4, MODE_IR = 5 };
enum RunState : uint8_t { STATE_IDLE = 0, STATE_RUNNING = 1, STATE_DONE = 2, STATE_ERROR = 3, STATE_CONFIRM_NOBAL = 4 };

extern ChemType selChem;
extern WorkMode selMode;
extern uint8_t  selCells;
extern float    setAmps;
extern float    vinMinLimit;

extern RunState    sysState;
extern const char* errText;
extern bool        useVoutAs1S;

extern uint16_t pwmBuck;
extern uint16_t pwmBoost;
extern uint16_t pwmDisch;
extern uint8_t  balMask;

extern float         capacityMah;
extern unsigned long jobStartMs;
extern unsigned long jobEndMs;

void startJob(bool skipNoBalConfirm = false);
void stopJobManual();
void abortJobWithError(const char* msg);
void runControlStep();

#endif
