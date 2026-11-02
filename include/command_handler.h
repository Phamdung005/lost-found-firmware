#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <Arduino.h>

void sendHeartbeat();
void pollNextCommand();
bool loadDeviceConfig();

#endif
