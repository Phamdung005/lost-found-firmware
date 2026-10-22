#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>

void connectWiFi();
bool isWiFiConnected();
int getWiFiRSSI();

#endif
