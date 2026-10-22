#include "keypad.h"
#include "config.h"
#include "lcd.h"
#include "locker.h"
#include "network.h"
#include <Keypad.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Định nghĩa ma trận 4x4
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {KEYPAD_ROW_1, KEYPAD_ROW_2, KEYPAD_ROW_3, KEYPAD_ROW_4};
byte colPins[COLS] = {KEYPAD_COL_1, KEYPAD_COL_2, KEYPAD_COL_3, KEYPAD_COL_4};

static Keypad customKeypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

static String otpBuffer = "";
static unsigned long messageResetTime = 0;
static bool showingFeedback = false;

static void verifyOtpWithServer(const String& otp) {
  if (!isWiFiConnected()) {
    lcdShowError("Khong co Wi-Fi");
    showingFeedback = true;
    messageResetTime = millis() + 2500;
    return;
  }

  lcdShowVerifying();
  Serial.printf("[KEYPAD] Dang gui ma OTP '%s' len Backend kiem tra...\n", otp.c_str());

  HTTPClient http;
  String url = String(BACKEND_BASE_URL) + "/verify-otp";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Token", DEVICE_TOKEN);

  StaticJsonDocument<256> reqDoc;
  reqDoc["otp"] = otp;

  String reqBody;
  serializeJson(reqDoc, reqBody);

  int httpCode = http.POST(reqBody);
  String response = http.getString();
  http.end();

  Serial.printf("[KEYPAD] Phan hoi HTTP %d: %s\n", httpCode, response.c_str());

  if (httpCode == 200 || httpCode == 201) {
    StaticJsonDocument<512> resDoc;
    DeserializationError err = deserializeJson(resDoc, response);
    if (!err && resDoc["success"]) {
      String slotCode = resDoc["data"]["slot_code"].as<String>();
      if (slotCode.length() == 0) slotCode = "A01";

      Serial.printf("[KEYPAD] OTP hop le! Mo ngan %s\n", slotCode.c_str());
      lcdShowSuccess(slotCode);
      openSlot(slotCode, SERVO_AUTO_LOCK_DELAY_MS);
      showingFeedback = true;
      messageResetTime = millis() + 4500;
      return;
    }
  }

  // Trường hợp sai mã hoặc lỗi
  Serial.println("[KEYPAD] OTP khong hop le hoac da het han!");
  lcdShowError("Thu lai sau 2s");
  showingFeedback = true;
  messageResetTime = millis() + 2500;
}

void initKeypad() {
  otpBuffer = "";
  showingFeedback = false;
  Serial.println("[KEYPAD] Keypad 4x4 matrix initialized successfully.");
}

void updateKeypad() {
  unsigned long now = millis();

  // Đặt lại màn hình chờ sau khi hiển thị kết quả thành công/thất bại
  if (showingFeedback && now >= messageResetTime) {
    showingFeedback = false;
    otpBuffer = "";
    lcdShowIdle(otpBuffer);
  }

  char key = customKeypad.getKey();
  if (key) {
    Serial.printf("[KEYPAD] Key pressed: %c\n", key);

    // Nếu đang hiện thông báo, bấm bất kỳ phím nào cũng thoát về màn hình nhập
    if (showingFeedback) {
      showingFeedback = false;
      otpBuffer = "";
      lcdShowIdle(otpBuffer);
    }

    if (key >= '0' && key <= '9') {
      if (otpBuffer.length() < 6) {
        otpBuffer += key;
        lcdShowIdle(otpBuffer);
      }
    } else if (key == '*') {
      // Phím '*' làm phím Xóa (Backspace)
      if (otpBuffer.length() > 0) {
        otpBuffer.remove(otpBuffer.length() - 1);
        lcdShowIdle(otpBuffer);
      }
    } else if (key == '#') {
      // Phím '#' làm phím Xác nhận (Enter)
      if (otpBuffer.length() == 6) {
        verifyOtpWithServer(otpBuffer);
      } else if (otpBuffer.length() == 0) {
        lcdShowIdle("");
      } else {
        lcdShowError("OTP can 6 so");
        showingFeedback = true;
        messageResetTime = now + 1800;
      }
    } else if (key == 'C') {
      // Phím 'C' làm phím Clear (Xóa sạch)
      otpBuffer = "";
      lcdShowIdle(otpBuffer);
    }
  }
}
