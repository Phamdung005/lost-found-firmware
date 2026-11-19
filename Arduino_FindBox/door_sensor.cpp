#include "door_sensor.h"
#include "config.h"
#include "device_config.h"

struct DoorSensorConfig {
  String slotCode;
  int sensorPin;
};

static DoorSensorConfig sensors[MAX_DEVICE_SLOTS];
static int sensorCount = 0;

void configureDoorSensor(const String& slotCode, int sensorPin) {
  if (slotCode.length() == 0 || sensorPin < 0) return;
  for (int i = 0; i < sensorCount; i++) {
    if (sensors[i].slotCode == slotCode) {
      sensors[i].sensorPin = sensorPin;
      pinMode(sensorPin, INPUT_PULLUP);
      return;
    }
  }
  if (sensorCount >= MAX_DEVICE_SLOTS) return;
  sensors[sensorCount++] = {slotCode, sensorPin};
  pinMode(sensorPin, INPUT_PULLUP);
}

void initDoorSensors() {
  sensorCount = 0;
  configureDoorSensor("A01", SENSOR_A01_PIN);
  configureDoorSensor("A02", SENSOR_A02_PIN);
}

String getDoorStatus(const String& slotCode) {
  for (int i = 0; i < sensorCount; i++) {
    if (sensors[i].slotCode == slotCode) {
      return digitalRead(sensors[i].sensorPin) == LOW ? "closed" : "open";
    }
  }
  return "unknown";
}

String getDoorStatusA01() { return getDoorStatus("A01"); }
String getDoorStatusA02() { return getDoorStatus("A02"); }
