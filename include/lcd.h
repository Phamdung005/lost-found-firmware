#ifndef LCD_H
#define LCD_H

#include <Arduino.h>

void initLcd();
void lcdShowBooting();
void lcdShowWiFiConnecting(const char* ssid);
void lcdShowWiFiConnected(const String& ip);
void lcdShowIdle(const String& currentOtp = "");
void lcdShowSelectSlot();
void lcdShowOtpInput(const String& slotCode, const String& currentOtp = "");
void lcdShowVerifying();
void lcdShowSuccess(const String& slotCode);
void lcdShowError(const String& errorMsg);
void lcdShowDoorOpen(const String& slotCode);
void lcdShowWaitingDoorClose(const String& slotCode);
void lcdShowDoorLocked(const String& slotCode);

#endif
