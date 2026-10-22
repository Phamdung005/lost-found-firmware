#include "lcd.h"
#include "config.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

static LiquidCrystal_I2C* pLcd = nullptr;
static uint8_t actualAddr = 0;

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
  Serial.println("[I2C] Vui long kiem tra:");
  Serial.println("      1. Dây VCC phai cam vao 5V (khong cam 3.3V)");
  Serial.println("      2. Dây SDA cam vao GPIO 21");
  Serial.println("      3. Dây SCL cam vao GPIO 22");
  return 0;
}

void initLcd() {
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  delay(100);

  actualAddr = scanI2CAddress();
  if (actualAddr == 0) {
    actualAddr = LCD_I2C_ADDR; // Dùng cấu hình dự phòng
  }

  if (pLcd) delete pLcd;
  pLcd = new LiquidCrystal_I2C(actualAddr, LCD_COLS, LCD_ROWS);

  pLcd->init();
  pLcd->backlight();
  pLcd->clear();
  lcdShowBooting();
}

void lcdShowBooting() {
  if (!pLcd) return;
  pLcd->clear();
  pLcd->setCursor(0, 0);
  pLcd->print("  FINDBOX LOCKER");
  pLcd->setCursor(0, 1);
  pLcd->print("Khoi dong...");
}

void lcdShowWiFiConnecting(const char* ssid) {
  if (!pLcd) return;
  pLcd->clear();
  pLcd->setCursor(0, 0);
  pLcd->print("Ket noi Wi-Fi:");
  pLcd->setCursor(0, 1);
  String s = String(ssid);
  if (s.length() > 16) s = s.substring(0, 16);
  pLcd->print(s);
}

void lcdShowWiFiConnected(const String& ip) {
  if (!pLcd) return;
  pLcd->clear();
  pLcd->setCursor(0, 0);
  pLcd->print("Wi-Fi Da Ket Noi");
  pLcd->setCursor(0, 1);
  pLcd->print(ip.substring(0, 16));
}

void lcdShowIdle(const String& currentOtp) {
  if (!pLcd) return;
  pLcd->clear();
  pLcd->setCursor(0, 0);
  pLcd->print("FINDBOX - SAN SANG");
  pLcd->setCursor(0, 1);
  if (currentOtp.length() == 0) {
    pLcd->print("Nhap OTP: ______");
  } else {
    pLcd->print("OTP: ");
    pLcd->print(currentOtp);
    for (int i = currentOtp.length(); i < 6; i++) {
      pLcd->print("_");
    }
  }
}

void lcdShowVerifying() {
  if (!pLcd) return;
  pLcd->clear();
  pLcd->setCursor(0, 0);
  pLcd->print("DANG KIEM TRA...");
  pLcd->setCursor(0, 1);
  pLcd->print("Xin vui long cho");
}

void lcdShowSuccess(const String& slotCode) {
  if (!pLcd) return;
  pLcd->clear();
  pLcd->setCursor(0, 0);
  pLcd->print("XAC THUC DUNG!");
  pLcd->setCursor(0, 1);
  pLcd->print("Mo Ngan ");
  pLcd->print(slotCode);
  pLcd->print(" (5s)");
}

void lcdShowError(const String& errorMsg) {
  if (!pLcd) return;
  pLcd->clear();
  pLcd->setCursor(0, 0);
  pLcd->print("SAI MA / HET HAN");
  pLcd->setCursor(0, 1);
  String msg = errorMsg.length() > 16 ? errorMsg.substring(0, 16) : errorMsg;
  pLcd->print(msg);
}

void lcdShowDoorOpen(const String& slotCode) {
  if (!pLcd) return;
  pLcd->clear();
  pLcd->setCursor(0, 0);
  pLcd->print("CUA DANG MO:");
  pLcd->setCursor(0, 1);
  pLcd->print("Ngan ");
  pLcd->print(slotCode);
}
