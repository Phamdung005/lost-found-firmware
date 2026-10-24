#include "keypad.h"
#include "config.h"
#include "lcd.h"
#include "locker.h"
#include "network.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Định nghĩa ma trận phím 4x4 (Tự quét phần cứng, KHÔNG CẦN thư viện bên ngoài)
static const char keymap[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

static const uint8_t rowPins[4] = {KEYPAD_ROW_1, KEYPAD_ROW_2, KEYPAD_ROW_3, KEYPAD_ROW_4};
static const uint8_t colPins[4] = {KEYPAD_COL_1, KEYPAD_COL_2, KEYPAD_COL_3, KEYPAD_COL_4};

enum KeypadState {
  STATE_CHOOSE_SLOT,
  STATE_ENTER_OTP
};

static KeypadState keypadState = STATE_CHOOSE_SLOT;
static String selectedSlot = "";
static String otpBuffer = "";
static unsigned long messageResetTime = 0;
static bool showingFeedback = false;
static bool lastVerifySuccess = false;
static int failedAttempts = 0;
static bool isLockedOut = false;
static unsigned long lockoutUntil = 0;
static unsigned long lastCountdownUpdate = 0;

// Hàm quét ma trận phím trực tiếp
static char scanKeypadMatrix() {
  for (int r = 0; r < 4; r++) {
    pinMode(rowPins[r], INPUT_PULLUP);
  }
  for (int c = 0; c < 4; c++) {
    pinMode(colPins[c], INPUT_PULLUP);
  }

  char detected = 0;

  for (int c = 0; c < 4; c++) {
    pinMode(colPins[c], OUTPUT);
    digitalWrite(colPins[c], LOW);
    delayMicroseconds(20);

    for (int r = 0; r < 4; r++) {
      if (digitalRead(rowPins[r]) == LOW) {
        detected = keymap[r][c];
        break;
      }
    }

    digitalWrite(colPins[c], HIGH);
    pinMode(colPins[c], INPUT_PULLUP);

    if (detected != 0) break;
  }

  return detected;
}

static unsigned long lastDebounceTime = 0;
static bool keyReleased = true;

// Hàm bắt phím có chống rung (Debounce)
static char getDebouncedKey() {
  char current = scanKeypadMatrix();
  unsigned long now = millis();

  if (current != 0) {
    if (keyReleased && (now - lastDebounceTime > 150)) {
      lastDebounceTime = now;
      keyReleased = false;
      return current;
    }
  } else {
    if (now - lastDebounceTime > 100) {
      keyReleased = true;
    }
  }
  return 0;
}

static void verifyOtpWithServer(const String& slot, const String& otp) {
  if (!isWiFiConnected()) {
    lcdShowError("Khong co Wi-Fi");
    showingFeedback = true;
    lastVerifySuccess = false;
    messageResetTime = millis() + 2500;
    return;
  }

  lcdShowVerifying();
  Serial.printf("[KEYPAD] Dang gui ma OTP '%s' cua ngan '%s' len Backend kiem tra...\n", otp.c_str(), slot.c_str());

  HTTPClient http;
  String url = String(BACKEND_BASE_URL) + "/verify-otp";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Token", DEVICE_TOKEN);

  StaticJsonDocument<256> reqDoc;
  reqDoc["otp"] = otp;
  reqDoc["slot_code"] = slot;
  reqDoc["failed_attempts"] = failedAttempts + 1;

  String reqBody;
  serializeJson(reqDoc, reqBody);

  int httpCode = http.POST(reqBody);
  String response = http.getString();
  http.end();

  Serial.printf("[KEYPAD] Phan hoi HTTP %d: %s\n", httpCode, response.c_str());

  StaticJsonDocument<512> resDoc;
  deserializeJson(resDoc, response);

  if (httpCode == 200 || httpCode == 201) {
    if (resDoc["success"]) {
      String openSlotCode = resDoc["data"]["slot_code"].as<String>();
      if (openSlotCode.length() == 0) openSlotCode = slot;
      failedAttempts = 0;

      Serial.printf("[KEYPAD] OTP hop le! Mo ngan %s\n", openSlotCode.c_str());
      lcdShowSuccess(openSlotCode);
      openSlot(openSlotCode, SERVO_AUTO_LOCK_DELAY_MS);
      showingFeedback = true;
      lastVerifySuccess = true;
      messageResetTime = millis() + 4500;
      return;
    }
  }

  // Trường hợp sai mã hoặc lỗi
  failedAttempts++;
  Serial.printf("[KEYPAD] OTP khong dung! nhap sai lan %d/3\n", failedAttempts);
  if(failedAttempts >= 3) {
    isLockedOut = true;
    lockoutUntil = millis() + 60 * 1000;
    lcdShowLockout(60);
    showingFeedback = false;
    otpBuffer = "";
    return;
  }

  String errMsg = "Sai OTP (" + String(failedAttempts) + "/3)";
  lcdShowError(errMsg);
  showingFeedback = true;
  lastVerifySuccess = false;
  messageResetTime = millis() + 2500;
}

void initKeypad() {
  keypadState = STATE_CHOOSE_SLOT;
  selectedSlot = "";
  otpBuffer = "";
  showingFeedback = false;
  lastVerifySuccess = false;
  for (int r = 0; r < 4; r++) pinMode(rowPins[r], INPUT_PULLUP);
  for (int c = 0; c < 4; c++) pinMode(colPins[c], INPUT_PULLUP);
  Serial.println("[KEYPAD] Pure 4x4 matrix scanner initialized with 2-step slot selection.");
}

void updateKeypad() {
  unsigned long now = millis();
  if (isLockedOut) {
    if(now < lockoutUntil) {
      if(now - lastCountdownUpdate >= 1000) {
        lastCountdownUpdate = now;
        int secondsLeft = (lockoutUntil - now) / 1000;
        lcdShowLockout(secondsLeft);
      }
      return;
    } else {
      isLockedOut = false;
      failedAttempts = 0;
      keypadState = STATE_CHOOSE_SLOT;
      selectedSlot = "";
      otpBuffer = "";
      lcdShowSelectSlot();
      Serial.printf("[KEYPAD] Da het thoi gian khoa. Mo lai ban phim!");
    } 
  }
  
  // Đặt lại màn hình sau khi hiển thị kết quả thành công/thất bại
  if (showingFeedback && now >= messageResetTime) {
    showingFeedback = false;
    otpBuffer = "";
    if (lastVerifySuccess) {
      keypadState = STATE_CHOOSE_SLOT;
      selectedSlot = "";
      // Nhường LCD cho locker.cpp hiển thị "VUI LONG DONG CUA" cho đến khi đóng cửa thật
    } else {
      // Nếu sai OTP, cho phép nhập lại ngay trên ngăn đã chọn
      lcdShowOtpInput(selectedSlot, otpBuffer);
    }
  }

  char key = getDebouncedKey();
  if (!key) return;

  Serial.printf("[KEYPAD] Key pressed: %c (State: %d)\n", key, keypadState);

  // Nếu đang hiển thị feedback lỗi/thành công, bấm phím bất kỳ sẽ thoát ngay
  if (showingFeedback) {
    showingFeedback = false;
    otpBuffer = "";
    if (lastVerifySuccess) {
      keypadState = STATE_CHOOSE_SLOT;
      selectedSlot = "";
      lcdShowSelectSlot();
    } else {
      lcdShowOtpInput(selectedSlot, otpBuffer);
    }
    return;
  }

  // BƯỚC 1: ĐANG Ở MÀN HÌNH CHỌN NGĂN TỦ
  if (keypadState == STATE_CHOOSE_SLOT) {
    if (key == '1' || key == 'A') {
      selectedSlot = "A01";
      otpBuffer = "";
      keypadState = STATE_ENTER_OTP;
      lcdShowOtpInput(selectedSlot, otpBuffer);
      Serial.println("[KEYPAD] Da chon ngan A01 -> Chuyen sang nhap OTP");
    } else if (key == '2' || key == 'B') {
      selectedSlot = "A02";
      otpBuffer = "";
      keypadState = STATE_ENTER_OTP;
      lcdShowOtpInput(selectedSlot, otpBuffer);
      Serial.println("[KEYPAD] Da chon ngan A02 -> Chuyen sang nhap OTP");
    } else {
      // Phím khác -> nhắc lại màn hình chọn ngăn
      lcdShowSelectSlot();
    }
    return;
  }

  // BƯỚC 2: ĐANG Ở MÀN HÌNH NHẬP MÃ OTP CHO NGĂN ĐÃ CHỌN
  if (keypadState == STATE_ENTER_OTP) {
    if (key >= '0' && key <= '9') {
      if (otpBuffer.length() < 6) {
        otpBuffer += key;
        lcdShowOtpInput(selectedSlot, otpBuffer);
      }
    } else if (key == '*') {
      // Phím '*' làm phím Backspace / Quay lại
      if (otpBuffer.length() > 0) {
        otpBuffer.remove(otpBuffer.length() - 1);
        lcdShowOtpInput(selectedSlot, otpBuffer);
      } else {
        // Nếu ô OTP đang trống mà bấm '*' -> Quay lại bước chọn ngăn tủ!
        keypadState = STATE_CHOOSE_SLOT;
        selectedSlot = "";
        lcdShowSelectSlot();
        Serial.println("[KEYPAD] Quay lai man hinh chon ngan tu.");
      }
    } else if (key == 'C') {
      // Phím 'C' làm sạch ô OTP
      otpBuffer = "";
      lcdShowOtpInput(selectedSlot, otpBuffer);
    } else if (key == '#') {
      // Phím '#' làm phím Xác nhận gửi mã
      if (otpBuffer.length() == 6) {
        verifyOtpWithServer(selectedSlot, otpBuffer);
      } else if (otpBuffer.length() == 0) {
        // Chưa nhập gì mà bấm Enter -> quay về chọn ngăn
        keypadState = STATE_CHOOSE_SLOT;
        selectedSlot = "";
        lcdShowSelectSlot();
      } else {
        lcdShowError("OTP can 6 so");
        showingFeedback = true;
        lastVerifySuccess = false;
        messageResetTime = now + 1800;
      }
    }
  }
}
