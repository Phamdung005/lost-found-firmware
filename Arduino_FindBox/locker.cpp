#include "locker.h"
#include "config.h"
#include "device_config.h"
#include "lcd.h"
#include "door_sensor.h"
#include <ESP32Servo.h>

enum SlotState { SLOT_IDLE, SLOT_UNLOCKING, SLOT_WAITING_DOOR_CLOSE, SLOT_LOCKING };
enum SlotCycleResult { SLOT_CYCLE_NONE, SLOT_CYCLE_SUCCESS, SLOT_CYCLE_FAILED };

struct RuntimeSlot {
  String slotCode;
  int servoPin;
  Servo servo;
  SlotState state;
  unsigned long timer;
  unsigned long closeDeadline;
  bool doorEverOpened;
  SlotCycleResult cycleResult;
  String lastDoorStatus;
};

static RuntimeSlot slots[MAX_DEVICE_SLOTS];
static int slotCount = 0;

static int servoPulseFor(const String& slotCode, bool unlocking) {
  // A01 is continuous rotation and must be controlled by pulse width.
  if (slotCode == "A01") return unlocking ? SERVO_A01_OPEN_US : SERVO_A01_CLOSE_US;
  return unlocking ? 10 : 80;
}

static void stopServoIfContinuous(const String& slotCode, Servo& servo) {
  if (slotCode == "A01") servo.writeMicroseconds(SERVO_A01_STOP_US);
}

static void releaseServoIfPositional(const String& slotCode, Servo& servo) {
  if (slotCode != "A01") servo.detach();
}

static RuntimeSlot* findSlot(const String& slotCode) {
  for (int i = 0; i < slotCount; i++) {
    if (slots[i].slotCode == slotCode) return &slots[i];
  }
  return nullptr;
}

void configureLockerSlot(const String& slotCode, int servoPin) {
  if (slotCode.length() == 0 || servoPin < 0) return;
  RuntimeSlot* existing = findSlot(slotCode);
  if (existing) {
    existing->servoPin = servoPin;
    return;
  }
  if (slotCount >= MAX_DEVICE_SLOTS) return;
  RuntimeSlot& slot = slots[slotCount++];
  slot.slotCode = slotCode;
  slot.servoPin = servoPin;
  slot.state = SLOT_IDLE;
  slot.timer = 0;
  slot.closeDeadline = 0;
  slot.doorEverOpened = false;
  slot.cycleResult = SLOT_CYCLE_NONE;
  slot.lastDoorStatus = "unknown";
}

void initLockers() {
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  slotCount = 0;
  configureLockerSlot("A01", SERVO_A01_PIN);
  configureLockerSlot("A02", SERVO_A02_PIN);
  Serial.println("[LOCKER] Initialized runtime locker configuration.");
}

bool isAnySlotBusy() {
  for (int i = 0; i < slotCount; i++) {
    if (slots[i].state != SLOT_IDLE) return true;
  }
  return false;
}

bool openSlot(const String& slotCode, int durationMs) {
  RuntimeSlot* slot = findSlot(slotCode);
  if (!slot || slot->state != SLOT_IDLE) {
    Serial.println("[LOCKER] Error: Unknown or busy slot: " + slotCode);
    return false;
  }

  const unsigned long now = millis();
  slot->servo.attach(slot->servoPin, 544, 2400);
  const int unlockValue = servoPulseFor(slotCode, true);
  if (slotCode == "A01") slot->servo.writeMicroseconds(unlockValue);
  else slot->servo.write(unlockValue);
  slot->state = SLOT_UNLOCKING;
  slot->timer = now + (slotCode == "A01" ? SERVO_A01_OPEN_TIME_MS : 500);
  slot->closeDeadline = now + (durationMs < 120000 ? 120000UL : (unsigned long)durationMs);
  slot->doorEverOpened = false;
  slot->cycleResult = SLOT_CYCLE_NONE;
  Serial.printf("[LOCKER] Slot %s -> opening on GPIO %d at %d %s...\n", slotCode.c_str(), slot->servoPin, unlockValue, slotCode == "A01" ? "us" : "degrees");
  return true;
}

void updateLockerServos() {
  const unsigned long now = millis();
  for (int i = 0; i < slotCount; i++) {
    RuntimeSlot& slot = slots[i];
    if (slot.state == SLOT_UNLOCKING && now >= slot.timer) {
      stopServoIfContinuous(slot.slotCode, slot.servo);
      releaseServoIfPositional(slot.slotCode, slot.servo);
      slot.state = SLOT_WAITING_DOOR_CLOSE;
      slot.timer = 0;
      lcdShowWaitingDoorClose(slot.slotCode);
      Serial.println("[LOCKER] Slot " + slot.slotCode + " opened; waiting for open/close sensor cycle.");
    } else if (slot.state == SLOT_WAITING_DOOR_CLOSE) {
      const String doorStatus = getDoorStatus(slot.slotCode);
      if (doorStatus != slot.lastDoorStatus) {
        Serial.printf("[DOOR] Slot %s sensor GPIO status: %s\n", slot.slotCode.c_str(), doorStatus.c_str());
        slot.lastDoorStatus = doorStatus;
      }
      if (doorStatus == "open") slot.doorEverOpened = true;

      if (doorStatus == "closed" && slot.doorEverOpened) {
        slot.servo.attach(slot.servoPin, 544, 2400);
        const int lockValue = servoPulseFor(slot.slotCode, false);
        if (slot.slotCode == "A01") slot.servo.writeMicroseconds(lockValue);
        else slot.servo.write(lockValue);
        Serial.printf("[LOCKER] Slot %s -> locking at %d %s.\n", slot.slotCode.c_str(), lockValue, slot.slotCode == "A01" ? "us" : "degrees");
        slot.state = SLOT_LOCKING;
        slot.timer = now + (slot.slotCode == "A01" ? SERVO_A01_CLOSE_TIME_MS : 500);
        lcdShowDoorLocked(slot.slotCode);
      } else if (slot.closeDeadline != 0 && (long)(now - slot.closeDeadline) >= 0) {
        slot.state = SLOT_IDLE;
        slot.cycleResult = SLOT_CYCLE_FAILED;
        Serial.println("[LOCKER] Slot " + slot.slotCode + " sensor cycle timed out; latch remains unlocked.");
      }
    } else if (slot.state == SLOT_LOCKING && now >= slot.timer) {
      stopServoIfContinuous(slot.slotCode, slot.servo);
      releaseServoIfPositional(slot.slotCode, slot.servo);
      slot.state = SLOT_IDLE;
      slot.cycleResult = SLOT_CYCLE_SUCCESS;
      Serial.println("[LOCKER] Slot " + slot.slotCode + " locked successfully.");
      delay(300);
      lcdShowSelectSlot();
    }
  }
}

bool takeSlotCycleResult(const String& slotCode, bool& success) {
  RuntimeSlot* slot = findSlot(slotCode);
  if (!slot || slot->cycleResult == SLOT_CYCLE_NONE) return false;
  success = slot->cycleResult == SLOT_CYCLE_SUCCESS;
  slot->cycleResult = SLOT_CYCLE_NONE;
  return true;
}
