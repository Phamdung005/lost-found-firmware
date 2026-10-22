#ifndef LOCKER_H
#define LOCKER_H

#include <Arduino.h>

void initLockers();
bool openSlot(const String& slotCode, int durationMs = 5000);
void updateLockerServos();

#endif
