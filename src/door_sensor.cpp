#include "door_sensor.h"
#include "locker.h"

void initDoorSensors() {
  for (int i = 0; i < getConfiguredSlotCount(); i++) {
    const SlotConfig* config = getSlotConfig(i);
    if (config != nullptr) pinMode(config->sensorPin, INPUT_PULLUP);
  }
}

String getDoorStatus(const String& slotCode) {
  const SlotConfig* config = findSlotConfig(slotCode);
  if (config == nullptr) return "unknown";
  return digitalRead(config->sensorPin) == LOW ? "closed" : "open";
}

String getDoorStatusA01() {
  return getDoorStatus("A01");
}

String getDoorStatusA02() {
  return getDoorStatus("A02");
}
