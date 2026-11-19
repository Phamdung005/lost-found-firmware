#include <Arduino.h>
#include "config.h"
#include "network.h"
#include "locker.h"
#include "door_sensor.h"
#include "command_handler.h"
#include "keypad.h"
#include "lcd.h"
#include "device_config.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

static unsigned long lastHeartbeatTime = 0;
static unsigned long lastPollTime = 0;
static unsigned long lastWiFiCheckTime = 0;
static unsigned long lastDeviceConfigSyncTime = 0;

void setup() {
  // Tắt bộ phát hiện sụt nguồn (Brownout Detector) của ESP32 để ngăn CPU tự reset khi servo khởi động
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  delay(500);
  Serial.println("\n==========================================");
  Serial.println("  FINDBOX — SMART LOST & FOUND LOCKER");
  Serial.println("  ESP32 Firmware Controller v1.3.0");
  Serial.println("==========================================");

  // 1. Khởi tạo Màn hình LCD 1602 I2C
  initLcd();

  // 2. Khởi tạo phần cứng (Servo, Cảm biến, Keypad)
  initLockers();      // Servo SG90 GPIO 18 (A01), GPIO 19 (A02)
  initDoorSensors();  // Cảm biến cửa từ GPIO 4 (A01), GPIO 5 (A02)
  initDeviceConfig(); // Mặc định dự phòng; sau đó tải cấu hình từ Admin
  initKeypad();       // Bàn phím ma trận 4x4

  // 3. Kết nối Wi-Fi
  lcdShowWiFiConnecting(WIFI_SSID);
  connectWiFi();

  // 4. Cập nhật trạng thái sau khi có Wi-Fi
  if (isWiFiConnected()) {
    lcdShowWiFiConnected(getWiFiIP());
    delay(1500);
    syncDeviceConfig();
    lastDeviceConfigSyncTime = millis();
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
  if (now - lastWiFiCheckTime >= 15000) {
    lastWiFiCheckTime = now;
    if (!isWiFiConnected()) {
      Serial.println("[WIFI] Lost connection. Reconnecting...");
      connectWiFi();
    }
  }

  // Admin thêm/sửa ngăn và GPIO trên app -> ESP32 tự nhận cấu hình mới.
  if (now - lastDeviceConfigSyncTime >= 60000) {
    lastDeviceConfigSyncTime = now;
    if (isWiFiConnected()) syncDeviceConfig();
  }

  // 4. Polling lấy lệnh mở ngăn từ Backend (Mỗi 1.5 giây)
  if (now - lastPollTime >= COMMAND_POLL_INTERVAL_MS) {
    lastPollTime = now;
    if (isWiFiConnected()) {
      pollNextCommand();
    }
  }

  // 5. Kiểm tra cảm biến cửa thay đổi trạng thái -> GỬI CẬP NHẬT NGAY LẬP TỨC (Realtime)
  static String lastSentDoorStatus[MAX_DEVICE_SLOTS];
  static unsigned long lastDoorCheckTime = 0;

  bool doorChanged = false;
  for (int i = 0; i < getConfiguredSlotCount(); i++) {
    const DeviceSlotConfig* config = getDeviceSlotConfig(i);
    if (!config) continue;
    const String currentStatus = getDoorStatus(config->slotCode);
    if (currentStatus != lastSentDoorStatus[i]) {
      lastSentDoorStatus[i] = currentStatus;
      doorChanged = true;
    }
  }

  if (doorChanged && (now - lastDoorCheckTime >= 600)) {
    lastDoorCheckTime = now;
    lastHeartbeatTime = now; // Reset lại timer heartbeat
    if (isWiFiConnected()) {
      Serial.println("[DOOR REALTIME] Door state changed -> sending heartbeat immediately.");
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
