#include "command_handler.h"
#include "config.h"
#include "locker.h"
#include "door_sensor.h"
#include "network.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

static void sendAck(int commandId, bool success, const String& resultMsg, const String& doorStatus) {
  if (!isWiFiConnected()) return;

  HTTPClient http;
  String url = String(BACKEND_BASE_URL) + "/commands/" + String(commandId) + "/ack";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Token", DEVICE_TOKEN);

  StaticJsonDocument<256> doc;
  doc["status"] = success ? "success" : "failed";
  doc["result_message"] = resultMsg;
  doc["door_status"] = doorStatus;

  String requestBody;
  serializeJson(doc, requestBody);

  int httpCode = http.POST(requestBody);
  if (httpCode == HTTP_CODE_OK || httpCode == 201) {
    Serial.printf("[HTTP] ACK sent for command #%d: %s\n", commandId, resultMsg.c_str());
  } else {
    Serial.printf("[HTTP] ACK failed for command #%d, HTTP code: %d\n", commandId, httpCode);
  }

  http.end();
}

bool loadDeviceConfig() {
  if (!isWiFiConnected()) return false;

  HTTPClient http;
  String url = String(BACKEND_BASE_URL) + "/config";
  http.begin(url);
  http.addHeader("X-Device-Token", DEVICE_TOKEN);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[CONFIG] Failed to load device config, HTTP code: %d\n", httpCode);
    http.end();
    return false;
  }

  StaticJsonDocument<1536> doc;
  DeserializationError error = deserializeJson(doc, http.getString());
  http.end();
  if (error || doc["success"] != true || !doc["slots"].is<JsonArray>()) {
    Serial.println("[CONFIG] Invalid device config response.");
    return false;
  }

  clearSlotConfigs();
  JsonArray slots = doc["slots"].as<JsonArray>();
  for (JsonObject slot : slots) {
    String slotCode = slot["slot_code"].as<String>();
    int servoPin = slot["servo_pin"] | -1;
    int sensorPin = slot["sensor_pin"] | -1;
    if (!configureSlot(slotCode, servoPin, sensorPin)) {
      Serial.println("[CONFIG] Rejected slot config: " + slotCode);
    }
  }

  if (getConfiguredSlotCount() == 0) {
    Serial.println("[CONFIG] No usable hardware slots returned.");
    configureSlot("A01", SERVO_A01_PIN, SENSOR_A01_PIN);
    configureSlot("A02", SERVO_A02_PIN, SENSOR_A02_PIN);
    initDoorSensors();
    return false;
  }
  initDoorSensors();
  Serial.printf("[CONFIG] Loaded %d hardware slot(s).\n", getConfiguredSlotCount());
  return true;
}

void sendHeartbeat() {
  if (!isWiFiConnected()) return;

  HTTPClient http;
  String url = String(BACKEND_BASE_URL) + "/heartbeat";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Token", DEVICE_TOKEN);

  StaticJsonDocument<1536> doc;
  doc["wifi_rssi"] = getWiFiRSSI();
  doc["firmware_version"] = "v1.2.0";

  JsonArray slots = doc.createNestedArray("slots");

  for (int i = 0; i < getConfiguredSlotCount(); i++) {
    const SlotConfig* config = getSlotConfig(i);
    if (config == nullptr) continue;
    JsonObject slot = slots.createNestedObject();
    slot["slot_code"] = config->slotCode;
    slot["door_status"] = getDoorStatus(config->slotCode);
  }

  String requestBody;
  serializeJson(doc, requestBody);

  int httpCode = http.POST(requestBody);
  if (httpCode == HTTP_CODE_OK) {
    Serial.printf("[HEARTBEAT] Sent OK (RSSI: %d, slots: %d)\n",
                  getWiFiRSSI(), getConfiguredSlotCount());
  } else {
    Serial.printf("[HEARTBEAT] Failed with HTTP code: %d\n", httpCode);
  }

  http.end();
}

void pollNextCommand() {
  if (!isWiFiConnected()) return;

  HTTPClient http;
  String url = String(BACKEND_BASE_URL) + "/commands/next";
  http.begin(url);
  http.addHeader("X-Device-Token", DEVICE_TOKEN);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();

    StaticJsonDocument<768> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error && doc["success"] == true && doc["has_command"] == true) {
      JsonObject cmd = doc["command"];
      int cmdId = cmd["id"];
      String cmdType = cmd["command_type"].as<String>();
      String slotCode = cmd["slot_code"].as<String>();
      int durationMs = cmd["parameters"]["duration_ms"] | SERVO_AUTO_LOCK_DELAY_MS;

      Serial.printf("[COMMAND] Received #%d: %s on %s\n", cmdId, cmdType.c_str(), slotCode.c_str());

      bool ok = false;
      String resultMsg;

      if (cmdType == "OPEN_SLOT") {
        ok = openSlot(slotCode, durationMs);
        resultMsg = ok ? ("Opened slot " + slotCode + " successfully") : ("Failed to open slot " + slotCode);
      } else {
        resultMsg = "Unsupported command type: " + cmdType;
      }

      String currentDoorStatus = getDoorStatus(slotCode);
      sendAck(cmdId, ok, resultMsg, currentDoorStatus);
    }
  }

  http.end();
}
