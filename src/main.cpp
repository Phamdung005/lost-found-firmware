#include <Arduino.h>
#include "config.h"
#include "network.h"
#include "locker.h"
#include "door_sensor.h"
#include "command_handler.h"
#include "keypad.h"

static unsigned long lastHeartbeatTime = 0;
static unsigned long lastPollTime = 0;
static unsigned long lastWiFiCheckTime = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println("  FINDBOX — SMART LOST & FOUND LOCKER");
  Serial.println("  ESP32 Firmware Controller v1.2.0");
  Serial.println("==========================================");

  // 1. Khởi tạo phần cứng
  initLockers();      // Servo SG90 GPIO 18 (A01), GPIO 19 (A02)
  initDoorSensors();  // Cảm biến cửa từ GPIO 22 (A01), GPIO 23 (A02)
  initKeypad();       // Bàn phím ma trận 4x4

  // 2. Kết nối Wi-Fi
  connectWiFi();

  // 3. Gửi heartbeat đầu tiên báo online
  if (isWiFiConnected()) {
    sendHeartbeat();
  }

  Serial.println("[SYSTEM] Ready and listening for locker commands...");
}

void loop() {
  unsigned long now = millis();

  // 1. Cập nhật trạng thái servo (tự khóa lại sau khi mở)
  updateLockerServos();

  // 2. Cập nhật keypad
  updateKeypad();

  // 3. Kiểm tra kết nối Wi-Fi định kỳ mỗi 15 giây
  if (now - lastWiFiCheckTime >= 15000) {
    lastWiFiCheckTime = now;
    if (!isWiFiConnected()) {
      Serial.println("[WIFI] Lost connection. Reconnecting...");
      connectWiFi();
    }
  }

  // 4. Polling lấy lệnh mở ngăn từ Backend (Mỗi 1.5 giây)
  if (now - lastPollTime >= COMMAND_POLL_INTERVAL_MS) {
    lastPollTime = now;
    if (isWiFiConnected()) {
      pollNextCommand();
    }
  }

  // 5. Gửi Heartbeat và trạng thái cảm biến cửa từ lên Backend (Mỗi 10 giây)
  if (now - lastHeartbeatTime >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatTime = now;
    if (isWiFiConnected()) {
      sendHeartbeat();
    }
  }

  delay(10); // Nhường chu kỳ CPU ngắn
}
