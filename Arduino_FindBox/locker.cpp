#include "locker.h"
#include "config.h"
#include "lcd.h"
#include "door_sensor.h"
#include <ESP32Servo.h>

enum SlotState {
  SLOT_IDLE,                 // Cửa đang khóa, servo ngắt điện
  SLOT_UNLOCKING,            // Đang cấp điện để servo quay sang 90 độ (500ms)
  SLOT_WAITING_DOOR_CLOSE,   // Đã mở xong (servo ngắt điện). Chờ cảm biến báo cửa đóng (LOW)
  SLOT_LOCKING               // Cửa đã đóng -> Cấp điện quay servo về 0 độ (500ms) -> ngắt điện
};

enum SlotCycleResult {
  SLOT_CYCLE_NONE,
  SLOT_CYCLE_SUCCESS,
  SLOT_CYCLE_FAILED,
};

static Servo servoA01;
static Servo servoA02;

static SlotState stateA01 = SLOT_IDLE;
static unsigned long timerA01 = 0;
static bool doorEverOpenedA01 = false;
static SlotCycleResult cycleResultA01 = SLOT_CYCLE_NONE;
static unsigned long closeDeadlineA01 = 0;

static SlotState stateA02 = SLOT_IDLE;
static unsigned long timerA02 = 0;
static bool doorEverOpenedA02 = false;
static SlotCycleResult cycleResultA02 = SLOT_CYCLE_NONE;
static unsigned long closeDeadlineA02 = 0;

void initLockers() {
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  servoA01.setPeriodHertz(50);
  servoA02.setPeriodHertz(50);

  stateA01 = SLOT_IDLE;
  stateA02 = SLOT_IDLE;
  cycleResultA01 = SLOT_CYCLE_NONE;
  cycleResultA02 = SLOT_CYCLE_NONE;
  closeDeadlineA01 = 0;
  closeDeadlineA02 = 0;

  Serial.println("[LOCKER] Initialized servos for A01 (GPIO 18) and A02 (GPIO 19).");
}

bool isAnySlotBusy() {
  return stateA01 != SLOT_IDLE || stateA02 != SLOT_IDLE;
}

bool openSlot(const String& slotCode, int durationMs) {
  unsigned long now = millis();

  if (slotCode == "A01") {
    // 1. Cấp xung điều khiển servo quay sang 90 độ để mở chốt
    servoA01.attach(SERVO_A01_PIN, 544, 2400);
    servoA01.write(SERVO_UNLOCK_ANGLE);

    stateA01 = SLOT_UNLOCKING;
    timerA01 = now + 500; // Cấp điện 500ms để servo quay trọn vẹn đến 90 độ
    doorEverOpenedA01 = false;
    cycleResultA01 = SLOT_CYCLE_NONE;
    closeDeadlineA01 = now + (durationMs < 120000 ? 120000UL : (unsigned long)durationMs);

    Serial.printf("[LOCKER] Slot A01 -> Xoay mo (%d deg)...\n", SERVO_UNLOCK_ANGLE);
    return true;
  } else if (slotCode == "A02") {
    servoA02.attach(SERVO_A02_PIN, 544, 2400);
    servoA02.write(SERVO_UNLOCK_ANGLE);

    stateA02 = SLOT_UNLOCKING;
    timerA02 = now + 500;
    doorEverOpenedA02 = false;
    cycleResultA02 = SLOT_CYCLE_NONE;
    closeDeadlineA02 = now + (durationMs < 120000 ? 120000UL : (unsigned long)durationMs);

    Serial.printf("[LOCKER] Slot A02 -> Xoay mo (%d deg)...\n", SERVO_UNLOCK_ANGLE);
    return true;
  }

  Serial.println("[LOCKER] Error: Unknown slot code: " + slotCode);
  return false;
}

void updateLockerServos() {
  unsigned long now = millis();

  // ==========================================
  // XỬ LÝ NGĂN A01
  // ==========================================
  if (stateA01 == SLOT_UNLOCKING) {
    if (now >= timerA01) {
      servoA01.detach(); // Ngắt điện hoàn toàn (0mA, không nóng motor, chống sụt áp)
      stateA01 = SLOT_WAITING_DOOR_CLOSE;
      // Không dùng countdown để khóa; phải ghi nhận open rồi closed.
      timerA01 = 0;
      
      lcdShowWaitingDoorClose("A01");
      Serial.println("[LOCKER] Slot A01 da mo (90 deg). Detached servo. Dang cho dong cua...");
    }
  } else if (stateA01 == SLOT_WAITING_DOOR_CLOSE) {
    String doorStatus = getDoorStatusA01();

    // Ghi nhận cửa đã được kéo mở ra
    if (doorStatus == "open") {
      doorEverOpenedA01 = true;
    }

    // Khi người dùng khép cửa lại (cảm biến báo closed) và đã qua ít nhất 1.5s
    // Hoặc nếu đang test chưa mở mà khép nam châm vào sau 1.5s
    if (doorStatus == "closed" && doorEverOpenedA01) {
      Serial.println("[LOCKER] Cua A01 da DONG! Bat dau quay khoa ve 0 deg...");
      
      servoA01.attach(SERVO_A01_PIN, 544, 2400);
      servoA01.write(SERVO_LOCK_ANGLE);

      stateA01 = SLOT_LOCKING;
      timerA01 = now + 500; // Cấp điện 500ms để servo quay về vị trí khóa
      lcdShowDoorLocked("A01");
    } else if (closeDeadlineA01 != 0 && (long)(now - closeDeadlineA01) >= 0) {
      stateA01 = SLOT_IDLE;
      cycleResultA01 = SLOT_CYCLE_FAILED;
      Serial.println("[LOCKER] Slot A01 sensor cycle timed out; latch remains unlocked.");
    }
  } else if (stateA01 == SLOT_LOCKING) {
    if (now >= timerA01) {
      servoA01.detach(); // Đã khóa xong -> ngắt điện hoàn toàn!
      stateA01 = SLOT_IDLE;
      cycleResultA01 = SLOT_CYCLE_SUCCESS;
      Serial.println("[LOCKER] Slot A01 DA KHOA XONG. Ngat dien hoan toan.");
      delay(1200);
      lcdShowSelectSlot(); // Làm mới lại LCD về màn hình chọn ngăn
    }
  }

  // ==========================================
  // XỬ LÝ NGĂN A02
  // ==========================================
  if (stateA02 == SLOT_UNLOCKING) {
    if (now >= timerA02) {
      servoA02.detach();
      stateA02 = SLOT_WAITING_DOOR_CLOSE;
      timerA02 = 0;
      
      lcdShowWaitingDoorClose("A02");
      Serial.println("[LOCKER] Slot A02 da mo (90 deg). Detached servo. Dang cho dong cua...");
    }
  } else if (stateA02 == SLOT_WAITING_DOOR_CLOSE) {
    String doorStatus = getDoorStatusA02();

    if (doorStatus == "open") {
      doorEverOpenedA02 = true;
    }

    if (doorStatus == "closed" && doorEverOpenedA02) {
      Serial.println("[LOCKER] Cua A02 da DONG! Bat dau quay khoa ve 0 deg...");
      
      servoA02.attach(SERVO_A02_PIN, 544, 2400);
      servoA02.write(SERVO_LOCK_ANGLE);

      stateA02 = SLOT_LOCKING;
      timerA02 = now + 500;
      lcdShowDoorLocked("A02");
    } else if (closeDeadlineA02 != 0 && (long)(now - closeDeadlineA02) >= 0) {
      stateA02 = SLOT_IDLE;
      cycleResultA02 = SLOT_CYCLE_FAILED;
      Serial.println("[LOCKER] Slot A02 sensor cycle timed out; latch remains unlocked.");
    }
  } else if (stateA02 == SLOT_LOCKING) {
    if (now >= timerA02) {
      servoA02.detach();
      stateA02 = SLOT_IDLE;
      cycleResultA02 = SLOT_CYCLE_SUCCESS;
      Serial.println("[LOCKER] Slot A02 DA KHOA XONG. Ngat dien hoan toan.");
      delay(1200);
      lcdShowSelectSlot();
    }
  }
}

bool takeSlotCycleResult(const String& slotCode, bool& success) {
  if (slotCode == "A01" && cycleResultA01 != SLOT_CYCLE_NONE) {
    success = cycleResultA01 == SLOT_CYCLE_SUCCESS;
    cycleResultA01 = SLOT_CYCLE_NONE;
    return true;
  }
  if (slotCode == "A02" && cycleResultA02 != SLOT_CYCLE_NONE) {
    success = cycleResultA02 == SLOT_CYCLE_SUCCESS;
    cycleResultA02 = SLOT_CYCLE_NONE;
    return true;
  }
  return false;
}
