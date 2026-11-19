#ifndef LOCKER_H
#define LOCKER_H

#include <Arduino.h>

void initLockers();
void configureLockerSlot(const String& slotCode, int servoPin);
bool isAnySlotBusy();
bool openSlot(const String& slotCode, int durationMs = 5000);
void updateLockerServos();
bool takeSlotCycleResult(const String& slotCode, bool& success);

#endif
