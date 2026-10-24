#include "lcd.h"
#include "config.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

static LiquidCrystal_I2C* pLcd = nullptr;
static uint8_t actualAddr = 0;

// Hàm in 1 dòng chuẩn 16 ký tự, tự động chèn khoảng trắng để KHÔNG CẦN gọi clear()
// (Tránh hoàn toàn lỗi bị lệch pha sinh ra ký tự rác o_0 hoặc ba gạch)
static void printLine16(int line, const String& text) {
  if (!pLcd) return;
  pLcd->setCursor(0, line);
  String padded = text;
  while (padded.length() < 16) {
    padded += ' ';
  }
  if (padded.length() > 16) {
    padded = padded.substring(0, 16);
  }
  pLcd->print(padded);
}

static uint8_t scanI2CAddress() {
  Serial.println("[I2C] Bat dau quet dia chi I2C...");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    byte error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf("[I2C] >>> TIM THAY MAN HINH TAI DIA CHI: 0x%02X <<<\n", addr);
      return addr;
    }
  }
  Serial.println("[I2C] CANH BAO: Khong tim thay thiet bi I2C nao!");
  return 0;
}

void initLcd() {
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  Wire.setClock(50000); // Hạ xung I2C xuống 50kHz để chống nhiễu đường truyền và cáp dài
  delay(200);

  actualAddr = scanI2CAddress();
  if (actualAddr == 0) {
    actualAddr = LCD_I2C_ADDR;
  }

  if (pLcd) delete pLcd;
  pLcd = new LiquidCrystal_I2C(actualAddr, LCD_COLS, LCD_ROWS);

  delay(50);
  pLcd->init();
  delay(50);
  pLcd->backlight();
  delay(50);
  pLcd->clear();
  delay(50);

  printLine16(0, " FINDBOX LOCKER ");
  printLine16(1, "  Khoi dong...  ");
}

void lcdShowBooting() {
  printLine16(0, " FINDBOX LOCKER ");
  printLine16(1, "  Khoi dong...  ");
}

void lcdShowWiFiConnecting(const char* ssid) {
  printLine16(0, "Ket noi Wi-Fi:  ");
  printLine16(1, String(ssid));
}

void lcdShowWiFiConnected(const String& ip) {
  printLine16(0, "Wi-Fi Connected ");
  printLine16(1, ip);
}

void lcdShowSelectSlot() {
  printLine16(0, " CHON NGAN TU:  ");
  printLine16(1, "1:A01      2:A02");
}

void lcdShowOtpInput(const String& slotCode, const String& currentOtp) {
  printLine16(0, "NGAN " + slotCode + ": NHAP ");
  if (currentOtp.length() == 0) {
    printLine16(1, "OTP: ______  *<-");
  } else {
    String line = "OTP: " + currentOtp;
    while (line.length() < 12) {
      line += '_';
    }
    line += " *<-";
    printLine16(1, line);
  }
}

void lcdShowIdle(const String& currentOtp) {
  if (currentOtp.length() == 0) {
    lcdShowSelectSlot();
  } else {
    lcdShowOtpInput("A01", currentOtp);
  }
}

void lcdShowVerifying() {
  printLine16(0, "DANG KIEM TRA...");
  printLine16(1, "Xin vui long cho");
}

void lcdShowSuccess(const String& slotCode) {
  printLine16(0, " XAC THUC DUNG! ");
  printLine16(1, "Mo Ngan: " + slotCode);
}

void lcdShowError(const String& errorMsg) {
  printLine16(0, "SAI MA / HET HAN");
  printLine16(1, errorMsg);
}

void lcdShowDoorOpen(const String& slotCode) {
  printLine16(0, "CUA DANG MO:    ");
  printLine16(1, "Ngan " + slotCode);
}

void lcdShowWaitingDoorClose(const String& slotCode) {
  printLine16(0, "MO NGAN " + slotCode + " XONG");
  printLine16(1, "VUI LONG DONG CUA");
}

void lcdShowDoorLocked(const String& slotCode) {
  printLine16(0, "DA DONG CUA: " + slotCode);
  printLine16(1, "DA KHOA CHOT OK ");
}

void lcdShowLockout(int secondsLeft) {
  printLine16(0, "KHOA BAN PHIM!  ");
  printLine16(1, "Thu lai sau: " + String(secondsLeft) + "s");
}