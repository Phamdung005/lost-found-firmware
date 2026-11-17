#include "findbox_network.h"
#include "config.h"
#include <WiFi.h>

static bool backendAvailable = false;

void connectWiFi() {
  Serial.printf("[WIFI] Connecting to %s...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Connected successfully!");
    Serial.printf("[WIFI] IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WIFI] Signal Strength (RSSI): %d dBm\n", WiFi.RSSI());
  } else {
    backendAvailable = false;
    Serial.println("\n[WIFI] Connection failed. Will retry periodically in background.");
  }
}

bool isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

int getWiFiRSSI() {
  if (isWiFiConnected()) {
    return WiFi.RSSI();
  }
  return 0;
}

String getWiFiIP() {
  if (isWiFiConnected()) {
    return WiFi.localIP().toString();
  }
  return "0.0.0.0";
}

void recordBackendRequest(bool responded) {
  const bool previous = backendAvailable;
  backendAvailable = responded;
  if (previous != backendAvailable) {
    Serial.printf("[BACKEND] Status: %s\n", backendAvailable ? "ONLINE" : "OFFLINE");
  }
}

bool isBackendAvailable() {
  return backendAvailable;
}
