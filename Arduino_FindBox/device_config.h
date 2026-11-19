#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <Arduino.h>

struct DeviceSlotConfig {
  String slotCode;
  int servoPin;
  int sensorPin;
};

const uint8_t MAX_DEVICE_SLOTS = 16;

void initDeviceConfig();
bool syncDeviceConfig();
int getConfiguredSlotCount();
const DeviceSlotConfig* getDeviceSlotConfig(int index);

#endif
