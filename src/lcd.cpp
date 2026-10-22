#include "lcd.h"
#include "config.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

static LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);

void initLcd() {
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcdShowBooting();
}

void lcdShowBooting() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  FINDBOX LOCKER");
  lcd.setCursor(0, 1);
  lcd.print("Khoi dong...");
}

void lcdShowWiFiConnecting(const char* ssid) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ket noi Wi-Fi:");
  lcd.setCursor(0, 1);
  String s = String(ssid);
  if (s.length() > 16) s = s.substring(0, 16);
  lcd.print(s);
}

void lcdShowWiFiConnected(const String& ip) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Wi-Fi Da Ket Noi");
  lcd.setCursor(0, 1);
  lcd.print(ip.substring(0, 16));
}

void lcdShowIdle(const String& currentOtp) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("FINDBOX - SAN SANG");
  lcd.setCursor(0, 1);
  if (currentOtp.length() == 0) {
    lcd.print("Nhap OTP: ______");
  } else {
    lcd.print("OTP: ");
    lcd.print(currentOtp);
    // Điền dấu gạch dưới còn lại
    for (int i = currentOtp.length(); i < 6; i++) {
      lcd.print("_");
    }
  }
}

void lcdShowVerifying() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DANG KIEM TRA...");
  lcd.setCursor(0, 1);
  lcd.print("Xin vui long cho");
}

void lcdShowSuccess(const String& slotCode) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("XAC THUC DUNG!");
  lcd.setCursor(0, 1);
  lcd.print("Mo Ngan ");
  lcd.print(slotCode);
  lcd.print(" (5s)");
}

void lcdShowError(const String& errorMsg) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SAI MA / HET HAN");
  lcd.setCursor(0, 1);
  String msg = errorMsg.length() > 16 ? errorMsg.substring(0, 16) : errorMsg;
  lcd.print(msg);
}

void lcdShowDoorOpen(const String& slotCode) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CUA DANG MO:");
  lcd.setCursor(0, 1);
  lcd.print("Ngan ");
  lcd.print(slotCode);
}
