#include "locker.h"
#include "config.h"
#include "lcd.h"
#include "door_sensor.h"
#include <ESP32Servo.h>

enum SlotState {
  SLOT_IDLE,
  SLOT_UNLOCKING,
  SLOT_WAITING_DOOR_CLOSE,
  SLOT_LOCKING
};

enum SlotCycleResult {
  SLOT_CYCLE_NONE,
  SLOT_CYCLE_SUCCESS,
  SLOT_CYCLE_FAILED,
};

struct SlotController {
  SlotConfig config;
  Servo servo;
  SlotState state;
  unsigned long timer;
  bool doorEverOpened;
  SlotCycleResult cycleResult;
  unsigned long closeDeadline;
};

static const int MAX_CONFIGURED_SLOTS = 12;
static SlotController controllers[MAX_CONFIGURED_SLOTS];
static int configuredSlotCount = 0;

int getConfiguredSlotCount() {
  return configuredSlotCount;
}

bool isAnySlotBusy() {
  for (int i = 0; i < configuredSlotCount; i++) {
    if (controllers[i].state != SLOT_IDLE) return true;
  }
  return false;
}

const SlotConfig* getSlotConfig(int index) {
  if (index < 0 || index >= configuredSlotCount) return nullptr;
  return &controllers[index].config;
}

const SlotConfig* findSlotConfig(const String& slotCode) {
  for (int i = 0; i < configuredSlotCount; i++) {
    if (controllers[i].config.slotCode == slotCode) return &controllers[i].config;
  }
  return nullptr;
}

void clearSlotConfigs() {
  for (int i = 0; i < configuredSlotCount; i++) {
    controllers[i].servo.detach();
  }
  configuredSlotCount = 0;
}

bool configureSlot(const String& slotCode, int servoPin, int sensorPin) {
  if (slotCode.length() == 0 || servoPin < 0 || sensorPin < 0 || servoPin == sensorPin) {
    return false;
  }
  if (configuredSlotCount >= MAX_CONFIGURED_SLOTS) {
    Serial.println("[LOCKER] Config rejected: maximum slot count reached.");
    return false;
  }
  if (findSlotConfig(slotCode) != nullptr) {
    Serial.println("[LOCKER] Config rejected: duplicate slot code " + slotCode);
    return false;
  }
  for (int i = 0; i < configuredSlotCount; i++) {
    if (controllers[i].config.servoPin == servoPin ||
        controllers[i].config.sensorPin == sensorPin ||
        controllers[i].config.servoPin == sensorPin ||
        controllers[i].config.sensorPin == servoPin) {
      Serial.println("[LOCKER] Config rejected: duplicate GPIO for " + slotCode);
      return false;
    }
  }

  SlotController& controller = controllers[configuredSlotCount++];
  controller.config = {slotCode, servoPin, sensorPin};
  controller.state = SLOT_IDLE;
  controller.timer = 0;
  controller.doorEverOpened = false;
  controller.cycleResult = SLOT_CYCLE_NONE;
  controller.closeDeadline = 0;
  controller.servo.setPeriodHertz(50);
  return true;
}

void initLockers() {
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  Serial.printf("[LOCKER] Ready for %d dynamic slot configurations.\n", MAX_CONFIGURED_SLOTS);
}

bool openSlot(const String& slotCode, int durationMs) {
  const SlotConfig* config = findSlotConfig(slotCode);
  if (config == nullptr) {
    Serial.println("[LOCKER] Error: slot is not configured: " + slotCode);
    return false;
  }
  for (int i = 0; i < configuredSlotCount; i++) {
    SlotController& controller = controllers[i];
    if (controller.config.slotCode != slotCode) continue;

    unsigned long now = millis();
    controller.servo.attach(config->servoPin, 544, 2400);
    controller.servo.write(SERVO_UNLOCK_ANGLE);
    controller.state = SLOT_UNLOCKING;
    controller.timer = now + 500;
    controller.doorEverOpened = false;
    controller.cycleResult = SLOT_CYCLE_NONE;
    controller.closeDeadline = now + (durationMs < 120000 ? 120000UL : (unsigned long)durationMs);
    Serial.printf("[LOCKER] Slot %s -> unlock on GPIO %d.\n", slotCode.c_str(), config->servoPin);
    (void)durationMs;
    return true;
  }
  return false;
}

void updateLockerServos() {
  unsigned long now = millis();

  for (int i = 0; i < configuredSlotCount; i++) {
    SlotController& controller = controllers[i];
    const String& slotCode = controller.config.slotCode;
    const String doorStatus = getDoorStatus(slotCode);

    if (controller.state == SLOT_UNLOCKING && now >= controller.timer) {
      controller.servo.detach();
      controller.state = SLOT_WAITING_DOOR_CLOSE;
      // Do not use a countdown to lock. The door must first be observed open
      // and then closed by the reed sensor.
      controller.timer = 0;
      lcdShowWaitingDoorClose(slotCode);
    } else if (controller.state == SLOT_WAITING_DOOR_CLOSE) {
      if (doorStatus == "open") controller.doorEverOpened = true;
      if (doorStatus == "closed" && controller.doorEverOpened) {
        controller.servo.attach(controller.config.servoPin, 544, 2400);
        controller.servo.write(SERVO_LOCK_ANGLE);
        controller.state = SLOT_LOCKING;
        controller.timer = now + 500;
        lcdShowDoorLocked(slotCode);
      } else if (controller.closeDeadline != 0 && (long)(now - controller.closeDeadline) >= 0) {
        controller.state = SLOT_IDLE;
        controller.cycleResult = SLOT_CYCLE_FAILED;
        Serial.printf("[LOCKER] Slot %s sensor cycle timed out; leaving latch unlocked.\n", slotCode.c_str());
      }
    } else if (controller.state == SLOT_LOCKING && now >= controller.timer) {
      controller.servo.detach();
      controller.state = SLOT_IDLE;
      controller.cycleResult = SLOT_CYCLE_SUCCESS;
      delay(1200);
      lcdShowSelectSlot();
    }
  }
}

bool takeSlotCycleResult(const String& slotCode, bool& success) {
  for (int i = 0; i < configuredSlotCount; i++) {
    SlotController& controller = controllers[i];
    if (controller.config.slotCode != slotCode || controller.cycleResult == SLOT_CYCLE_NONE) continue;
    success = controller.cycleResult == SLOT_CYCLE_SUCCESS;
    controller.cycleResult = SLOT_CYCLE_NONE;
    return true;
  }
  return false;
}
