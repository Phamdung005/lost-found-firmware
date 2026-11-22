#include "device_config.h"
#include "config.h"
#include "network.h"
#include "locker.h"
#include "door_sensor.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

static DeviceSlotConfig configs[MAX_DEVICE_SLOTS];
static int configCount = 0;

static void addOrUpdateConfig(const String& slotCode, int servoPin, int sensorPin) {
  if (slotCode.length() == 0 || servoPin < 0 || sensorPin < 0) return;
  Serial.printf("[CONFIG] %s servo_gpio=%d sensor_gpio=%d\n", slotCode.c_str(), servoPin, sensorPin);
  for (int i = 0; i < configCount; i++) {
    if (configs[i].slotCode == slotCode) {
      configs[i].servoPin = servoPin;
      configs[i].sensorPin = sensorPin;
      configureLockerSlot(slotCode, servoPin);
      configureDoorSensor(slotCode, sensorPin);
      return;
    }
  }
  if (configCount >= MAX_DEVICE_SLOTS) return;
  configs[configCount] = {slotCode, servoPin, sensorPin};
  configureLockerSlot(slotCode, servoPin);
  configureDoorSensor(slotCode, sensorPin);
  configCount++;
}

void initDeviceConfig() {
  configCount = 0;
  addOrUpdateConfig("A01", SERVO_A01_PIN, SENSOR_A01_PIN);
  addOrUpdateConfig("A02", SERVO_A02_PIN, SENSOR_A02_PIN);
}

bool syncDeviceConfig() {
  if (!isWiFiConnected()) return false;
  HTTPClient http;
  http.begin(String(BACKEND_BASE_URL) + "/config");
  http.addHeader("X-Device-Token", DEVICE_TOKEN);
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[CONFIG] Failed to load device config, HTTP %d\n", code);
    http.end();
    return false;
  }
  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(4096);
  const DeserializationError error = deserializeJson(doc, payload);
  if (error || doc["success"] != true || !doc["slots"].is<JsonArray>()) {
    Serial.println("[CONFIG] Invalid device config response");
    return false;
  }
  for (JsonObject slot : doc["slots"].as<JsonArray>()) {
    addOrUpdateConfig(slot["slot_code"].as<String>(), slot["servo_pin"] | -1, slot["sensor_pin"] | -1);
  }
  Serial.printf("[CONFIG] Loaded %d hardware slot(s) from admin\n", configCount);
  return true;
}

int getConfiguredSlotCount() { return configCount; }

const DeviceSlotConfig* getDeviceSlotConfig(int index) {
  if (index < 0 || index >= configCount) return nullptr;
  return &configs[index];
}
