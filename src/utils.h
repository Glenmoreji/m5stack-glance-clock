#pragma once
#include "config.h"

// ----------------------------------------------------------------------------
// Utility Functions for Logging, HTTP Error Handling, and LCD UI
// ----------------------------------------------------------------------------

// Logs a message to the serial console and M5Stack LCD display.
void logMessage(const String& consoleMsg, const String& displayMsg = "");

// Sends an HTTP error response with CORS headers and logs error details.
void handleErrorResponse(int status, const String& errMsg, const String& logMsg = "");

// Clears the log output region on the M5Stack LCD display.
void clearLogArea();

// Redraws the top header status bar with current WiFi and BLE state.
void updateScreenStatus();
