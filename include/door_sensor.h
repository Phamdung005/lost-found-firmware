#ifndef DOOR_SENSOR_H
#define DOOR_SENSOR_H

#include <Arduino.h>

void initDoorSensors();
String getDoorStatus(const String& slotCode);
String getDoorStatusA01();
String getDoorStatusA02();

#endif
