#include "locker.h"
#include "config.h"
#include <ESP32Servo.h>

static Servo servoA01;
static Servo servoA02;

static unsigned long unlockTimerA01 = 0;
static unsigned long unlockTimerA02 = 0;
static bool isUnlockedA01 = false;
static bool isUnlockedA02 = false;

void initLockers() {
  // Cấu hình timer PWM cho ESP32 Servo
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);

  servoA01.setPeriodHertz(50); // 50Hz cho servo tiêu chuẩn
  servoA02.setPeriodHertz(50);

  servoA01.attach(SERVO_A01_PIN, 500, 2400);
  servoA02.attach(SERVO_A02_PIN, 500, 2400);

  // Khóa ban đầu ở góc 0 độ
  servoA01.write(SERVO_LOCK_ANGLE);
  servoA02.write(SERVO_LOCK_ANGLE);

  Serial.println("[LOCKER] Initialized servos for A01 (GPIO 18) and A02 (GPIO 19)");
}

bool openSlot(const String& slotCode, int durationMs) {
  if (durationMs <= 0) durationMs = SERVO_AUTO_LOCK_DELAY_MS;

  if (slotCode == "A01") {
    servoA01.write(SERVO_UNLOCK_ANGLE);
    isUnlockedA01 = true;
    unlockTimerA01 = millis() + durationMs;
    Serial.println("[LOCKER] Slot A01 UNLOCKED (Servo GPIO 18 -> 90 deg)");
    return true;
  } else if (slotCode == "A02") {
    servoA02.write(SERVO_UNLOCK_ANGLE);
    isUnlockedA02 = true;
    unlockTimerA02 = millis() + durationMs;
    Serial.println("[LOCKER] Slot A02 UNLOCKED (Servo GPIO 19 -> 90 deg)");
    return true;
  }

  Serial.println("[LOCKER] Error: Unknown slot code: " + slotCode);
  return false;
}

// Hàm cập nhật non-blocking tự động khóa lại sau khi hết thời gian mở
void updateLockerServos() {
  unsigned long now = millis();

  if (isUnlockedA01 && now >= unlockTimerA01) {
    servoA01.write(SERVO_LOCK_ANGLE);
    isUnlockedA01 = false;
    Serial.println("[LOCKER] Slot A01 AUTO-LOCKED (Servo GPIO 18 -> 0 deg)");
  }

  if (isUnlockedA02 && now >= unlockTimerA02) {
    servoA02.write(SERVO_LOCK_ANGLE);
    isUnlockedA02 = false;
    Serial.println("[LOCKER] Slot A02 AUTO-LOCKED (Servo GPIO 19 -> 0 deg)");
  }
}
