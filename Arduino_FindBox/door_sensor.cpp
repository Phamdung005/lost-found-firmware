#include "door_sensor.h"
#include "config.h"

void initDoorSensors() {
  pinMode(SENSOR_A01_PIN, INPUT_PULLUP);
  pinMode(SENSOR_A02_PIN, INPUT_PULLUP);
}

// Cảm biến từ: khi cửa đóng nam châm chạm vào switch đóng mạch -> LOW
// Khi cửa mở tách nam châm -> HIGH
String getDoorStatusA01() {
  int state = digitalRead(SENSOR_A01_PIN);
  return (state == LOW) ? "closed" : "open";
}

String getDoorStatusA02() {
  int state = digitalRead(SENSOR_A02_PIN);
  return (state == LOW) ? "closed" : "open";
}
