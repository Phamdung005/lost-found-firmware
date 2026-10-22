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

void sendHeartbeat() {
  if (!isWiFiConnected()) return;

  HTTPClient http;
  String url = String(BACKEND_BASE_URL) + "/heartbeat";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Token", DEVICE_TOKEN);

  StaticJsonDocument<512> doc;
  doc["wifi_rssi"] = getWiFiRSSI();
  doc["firmware_version"] = "v1.2.0";

  JsonArray slots = doc.createNestedArray("slots");

  JsonObject slot1 = slots.createNestedObject();
  slot1["slot_code"] = "A01";
  slot1["door_status"] = getDoorStatusA01();

  JsonObject slot2 = slots.createNestedObject();
  slot2["slot_code"] = "A02";
  slot2["door_status"] = getDoorStatusA02();

  String requestBody;
  serializeJson(doc, requestBody);

  int httpCode = http.POST(requestBody);
  if (httpCode == HTTP_CODE_OK) {
    Serial.printf("[HEARTBEAT] Sent OK (RSSI: %d, A01: %s, A02: %s)\n",
                  getWiFiRSSI(),
                  getDoorStatusA01().c_str(),
                  getDoorStatusA02().c_str());
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

    StaticJsonDocument<512> doc;
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

      String currentDoorStatus = (slotCode == "A01") ? getDoorStatusA01() : getDoorStatusA02();
      sendAck(cmdId, ok, resultMsg, currentDoorStatus);
    }
  }

  http.end();
}
