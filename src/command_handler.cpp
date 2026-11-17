#include "command_handler.h"
#include "config.h"
#include "locker.h"
#include "door_sensor.h"
#include "findbox_network.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

struct PendingAck {
  bool used;
  int commandId;
  bool success;
  String resultMsg;
  String doorStatus;
};

static PendingAck pendingAcks[MAX_PENDING_ACKS];
static Preferences preferences;
static bool preferencesReady = false;

static void configureHttp(HTTPClient& http) {
  http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(HTTP_RESPONSE_TIMEOUT_MS);
}

static bool sendAckNow(const PendingAck& ack) {
  if (!isWiFiConnected()) return false;

  HTTPClient http;
  String url = String(BACKEND_BASE_URL) + "/commands/" + String(ack.commandId) + "/ack";
  http.begin(url);
  configureHttp(http);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Token", DEVICE_TOKEN);

  StaticJsonDocument<256> doc;
  doc["status"] = ack.success ? "success" : "failed";
  doc["result_message"] = ack.resultMsg;
  doc["door_status"] = ack.doorStatus;

  String requestBody;
  serializeJson(doc, requestBody);

  int httpCode = http.POST(requestBody);
  const bool responded = httpCode > 0;
  recordBackendRequest(responded);
  if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
    Serial.printf("[HTTP] ACK sent for command #%d: %s\n", ack.commandId, ack.resultMsg.c_str());
  } else {
    Serial.printf("[HTTP] ACK failed for command #%d, HTTP code: %d\n", ack.commandId, httpCode);
  }

  http.end();
  return httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED;
}

static void queueAck(const PendingAck& ack) {
  for (int i = 0; i < MAX_PENDING_ACKS; i++) {
    if (pendingAcks[i].used && pendingAcks[i].commandId == ack.commandId) return;
  }
  for (int i = 0; i < MAX_PENDING_ACKS; i++) {
    if (!pendingAcks[i].used) {
      pendingAcks[i] = ack;
      pendingAcks[i].used = true;
      Serial.printf("[ACK] Queued command #%d for retry.\n", ack.commandId);
      return;
    }
  }
  Serial.println("[ACK] Queue full; dropping oldest ACK.");
  pendingAcks[0] = ack;
  pendingAcks[0].used = true;
}

static void sendAck(int commandId, bool success, const String& resultMsg, const String& doorStatus) {
  PendingAck ack{true, commandId, success, resultMsg, doorStatus};
  if (!sendAckNow(ack)) queueAck(ack);
}

void flushPendingAcks() {
  if (!isWiFiConnected()) return;
  for (int i = 0; i < MAX_PENDING_ACKS; i++) {
    if (!pendingAcks[i].used) continue;
    if (sendAckNow(pendingAcks[i])) {
      pendingAcks[i].used = false;
    } else {
      break;
    }
  }
}

static bool applyConfig(JsonArray slots) {
  clearSlotConfigs();
  for (JsonObject slot : slots) {
    String slotCode = slot["slot_code"].as<String>();
    int servoPin = slot["servo_pin"] | -1;
    int sensorPin = slot["sensor_pin"] | -1;
    if (!configureSlot(slotCode, servoPin, sensorPin)) {
      Serial.println("[CONFIG] Rejected slot config: " + slotCode);
    }
  }
  if (getConfiguredSlotCount() == 0) return false;
  initDoorSensors();
  Serial.printf("[CONFIG] Applied %d hardware slot(s).\n", getConfiguredSlotCount());
  return true;
}

bool loadPersistedDeviceConfig() {
  if (!preferencesReady) {
    preferencesReady = preferences.begin("findbox", false);
  }
  if (!preferencesReady) return false;
  String stored = preferences.getString("slots", "");
  if (stored.length() == 0) return false;

  StaticJsonDocument<1536> doc;
  if (deserializeJson(doc, stored) != DeserializationError::Ok || !doc.is<JsonArray>()) {
    Serial.println("[CONFIG] Persisted config is invalid.");
    return false;
  }
  return applyConfig(doc.as<JsonArray>());
}

bool loadDeviceConfig() {
  if (!isWiFiConnected()) return false;

  HTTPClient http;
  String url = String(BACKEND_BASE_URL) + "/config";
  http.begin(url);
  configureHttp(http);
  http.addHeader("X-Device-Token", DEVICE_TOKEN);

  int httpCode = http.GET();
  recordBackendRequest(httpCode > 0);
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

  JsonArray slots = doc["slots"].as<JsonArray>();
  if (!applyConfig(slots)) {
    Serial.println("[CONFIG] No usable hardware slots returned.");
    return false;
  }
  if (!preferencesReady) preferencesReady = preferences.begin("findbox", false);
  if (preferencesReady) {
    String persisted;
    serializeJson(slots, persisted);
    if (preferences.getString("slots", "") != persisted) {
      preferences.putString("slots", persisted);
      Serial.println("[CONFIG] Persisted updated device config.");
    }
  }
  return true;
}

void sendHeartbeat() {
  if (!isWiFiConnected()) return;

  HTTPClient http;
  String url = String(BACKEND_BASE_URL) + "/heartbeat";
  http.begin(url);
  configureHttp(http);
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
  recordBackendRequest(httpCode > 0);
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
  configureHttp(http);
  http.addHeader("X-Device-Token", DEVICE_TOKEN);

  int httpCode = http.GET();
  recordBackendRequest(httpCode > 0);
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
