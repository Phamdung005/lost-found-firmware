#include "locker.h"
#include "config.h"
#include <ESP32Servo.h>

static Servo servoA01;
static Servo servoA02;

static unsigned long unlockTimerA01 = 0;
static unsigned long unlockTimerA02 = 0;
static unsigned long detachTimerA01 = 0;
static unsigned long detachTimerA02 = 0;

static bool isUnlockedA01 = false;
static bool isUnlockedA02 = false;
static bool isAttachedA01 = false;
static bool isAttachedA02 = false;

void initLockers() {
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);

  servoA01.setPeriodHertz(50);
  servoA02.setPeriodHertz(50);

  // Khởi động tuần tự (tránh cả 2 cùng hút dòng cùng 1 lúc)
  servoA01.attach(SERVO_A01_PIN, 500, 2400);
  servoA01.write(SERVO_LOCK_ANGLE);
  delay(150);
  servoA01.detach(); // Ngắt điện cuộn dây khi đứng yên để không ăn dòng

  servoA02.attach(SERVO_A02_PIN, 500, 2400);
  servoA02.write(SERVO_LOCK_ANGLE);
  delay(150);
  servoA02.detach();

  isAttachedA01 = false;
  isAttachedA02 = false;

  Serial.println("[LOCKER] Initialized servos with smart detach power saving.");
}

bool openSlot(const String& slotCode, int durationMs) {
  if (durationMs <= 0) durationMs = SERVO_AUTO_LOCK_DELAY_MS;

  if (slotCode == "A01") {
    if (!isAttachedA01) {
      servoA01.attach(SERVO_A01_PIN, 500, 2400);
      isAttachedA01 = true;
    }
    servoA01.write(SERVO_UNLOCK_ANGLE);
    isUnlockedA01 = true;
    unlockTimerA01 = millis() + durationMs;
    Serial.println("[LOCKER] Slot A01 UNLOCKED (Servo GPIO 18 -> 90 deg)");
    return true;
  } else if (slotCode == "A02") {
    if (!isAttachedA02) {
      servoA02.attach(SERVO_A02_PIN, 500, 2400);
      isAttachedA02 = true;
    }
    servoA02.write(SERVO_UNLOCK_ANGLE);
    isUnlockedA02 = true;
    unlockTimerA02 = millis() + durationMs;
    Serial.println("[LOCKER] Slot A02 UNLOCKED (Servo GPIO 19 -> 90 deg)");
    return true;
  }

  Serial.println("[LOCKER] Error: Unknown slot code: " + slotCode);
  return false;
}

// Cập nhật non-blocking: Đóng khóa và ngắt điện servo
void updateLockerServos() {
  unsigned long now = millis();

  // Khóa lại ngăn A01 sau khi hết thời gian mở
  if (isUnlockedA01 && now >= unlockTimerA01) {
    servoA01.write(SERVO_LOCK_ANGLE);
    isUnlockedA01 = false;
    detachTimerA01 = now + 600; // Cho 600ms để servo kịp quay về 0 độ rồi mới ngắt điện
    Serial.println("[LOCKER] Slot A01 AUTO-LOCKED (Servo GPIO 18 -> 0 deg)");
  }

  // Tự ngắt điện servo A01 để bảo vệ nguồn, chống sụt áp
  if (!isUnlockedA01 && isAttachedA01 && now >= detachTimerA01) {
    servoA01.detach();
    isAttachedA01 = false;
    Serial.println("[LOCKER] Slot A01 Servo detached (Power saved)");
  }

  // Khóa lại ngăn A02 sau khi hết thời gian mở
  if (isUnlockedA02 && now >= unlockTimerA02) {
    servoA02.write(SERVO_LOCK_ANGLE);
    isUnlockedA02 = false;
    detachTimerA02 = now + 600;
    Serial.println("[LOCKER] Slot A02 AUTO-LOCKED (Servo GPIO 19 -> 0 deg)");
  }

  // Tự ngắt điện servo A02
  if (!isUnlockedA02 && isAttachedA02 && now >= detachTimerA02) {
    servoA02.detach();
    isAttachedA02 = false;
    Serial.println("[LOCKER] Slot A02 Servo detached (Power saved)");
  }
}
