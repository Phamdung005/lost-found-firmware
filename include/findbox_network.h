#ifndef FINDBOX_NETWORK_H
#define FINDBOX_NETWORK_H

#include <Arduino.h>

void connectWiFi();
bool isWiFiConnected();
int getWiFiRSSI();
String getWiFiIP();
void recordBackendRequest(bool responded);
bool isBackendAvailable();

#endif
