#include <Arduino.h>
#include "config.h"
#include "findbox_network.h"
#include "locker.h"
#include "door_sensor.h"
#include "command_handler.h"
#include "keypad.h"
#include "lcd.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

static unsigned long lastHeartbeatTime = 0;
static unsigned long lastPollTime = 0;
static unsigned long lastWiFiCheckTime = 0;
static unsigned long lastConfigSyncTime = 0;
static unsigned long lastAckRetryTime = 0;

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Vô hiệu hóa ngắt sụt áp do Servo khởi động

  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println("  FINDBOX — SMART LOST & FOUND LOCKER");
  Serial.println("  ESP32 Firmware Controller v1.3.0");
  Serial.println("==========================================");

  // 1. Khởi tạo Màn hình LCD 1602 I2C
  initLcd();

  // 2. Khởi tạo phần cứng (Servo, Cảm biến, Keypad)
  initLockers();      // Cấu hình servo động từ Backend
  if (!loadPersistedDeviceConfig()) {
    configureSlot("A01", SERVO_A01_PIN, SENSOR_A01_PIN);
    configureSlot("A02", SERVO_A02_PIN, SENSOR_A02_PIN);
  }
  initDoorSensors();
  initKeypad();       // Bàn phím ma trận 4x4

  // 3. Kết nối Wi-Fi
  lcdShowWiFiConnecting(WIFI_SSID);
  connectWiFi();

  // 4. Cập nhật trạng thái sau khi có Wi-Fi
  if (isWiFiConnected()) {
    lcdShowWiFiConnected(getWiFiIP());
    delay(1500);
    if (!loadDeviceConfig()) {
      Serial.println("[CONFIG] Keeping persisted/built-in configuration.");
    }
    sendHeartbeat();
  }

  // 5. Hiển thị màn hình chờ nhập mã OTP
  lcdShowIdle("");
  Serial.println("[SYSTEM] Ready and listening for locker commands & keypad OTP...");
}

void loop() {
  unsigned long now = millis();

  // 1. Cập nhật trạng thái servo (tự khóa lại sau khi mở)
  updateLockerServos();
  processPendingCommandAcks();

  // 2. Cập nhật keypad
  updateKeypad();

  // 3. Kiểm tra kết nối Wi-Fi định kỳ mỗi 15 giây
  if (now - lastWiFiCheckTime >= WIFI_RECONNECT_INTERVAL_MS) {
    lastWiFiCheckTime = now;
    if (!isWiFiConnected()) {
      Serial.println("[WIFI] Lost connection. Reconnecting...");
      connectWiFi();
    }

    if (now - lastConfigSyncTime >= CONFIG_SYNC_INTERVAL_MS &&
        isWiFiConnected() && !isAnySlotBusy()) {
      lastConfigSyncTime = now;
      loadDeviceConfig();
    }
  }

  // 4. Polling lấy lệnh mở ngăn từ Backend (Mỗi 1.5 giây)
  if (now - lastPollTime >= COMMAND_POLL_INTERVAL_MS) {
    lastPollTime = now;
    if (isWiFiConnected()) {
      pollNextCommand();
    }
  }

  if (now - lastAckRetryTime >= ACK_RETRY_INTERVAL_MS) {
    lastAckRetryTime = now;
    flushPendingAcks();
  }

  // 5. Kiểm tra cảm biến cửa thay đổi trạng thái -> GỬI CẬP NHẬT NGAY LẬP TỨC (Realtime)
  static String lastSentDoorState = "";
  static unsigned long lastDoorCheckTime = 0;

  String currentDoorState = "";
  for (int i = 0; i < getConfiguredSlotCount(); i++) {
    const SlotConfig* config = getSlotConfig(i);
    if (config == nullptr) continue;
    currentDoorState += config->slotCode + ":" + getDoorStatus(config->slotCode) + ";";
  }

  if (currentDoorState != lastSentDoorState && (now - lastDoorCheckTime >= 600)) {
    lastDoorCheckTime = now;
    lastSentDoorState = currentDoorState;
    lastHeartbeatTime = now;
    if (isWiFiConnected()) {
      Serial.printf("[DOOR REALTIME] Door state changed (%s) -> heartbeat.\n", currentDoorState.c_str());
      sendHeartbeat();
    }
  }

  // 6. Gửi Heartbeat định kỳ lên Backend (Mỗi 10 giây nếu cửa không thay đổi)
  if (now - lastHeartbeatTime >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatTime = now;
    if (isWiFiConnected()) {
      sendHeartbeat();
    }
  }

  delay(10); // Nhường chu kỳ CPU ngắn
}
