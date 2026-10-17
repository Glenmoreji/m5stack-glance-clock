#pragma once
#include "config.h"

// ----------------------------------------------------------------------------
// Function Declarations for MQTT Client Management
// ----------------------------------------------------------------------------
void setupMQTT();
void loopMQTT();
void publishMQTTStatus(const String& topic, const String& payload, bool retain = false);
