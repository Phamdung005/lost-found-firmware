#ifndef LOCKER_H
#define LOCKER_H

#include <Arduino.h>

struct SlotConfig {
  String slotCode;
  int servoPin;
  int sensorPin;
};

void initLockers();
void clearSlotConfigs();
bool configureSlot(const String& slotCode, int servoPin, int sensorPin);
int getConfiguredSlotCount();
const SlotConfig* getSlotConfig(int index);
const SlotConfig* findSlotConfig(const String& slotCode);
bool openSlot(const String& slotCode, int durationMs = 5000);
void updateLockerServos();

#endif
