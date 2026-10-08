#pragma once
#include "config.h"

// ----------------------------------------------------------------------------
// Utility Functions for Logging, HTTP Error Handling, and LCD UI
// ----------------------------------------------------------------------------

// Logs a message to the console and/or display
void logMessage(const String& consoleMsg, const String& displayMsg = "");
// Handles HTTP error responses by setting CORS headers, sending status/message, and logging
void handleErrorResponse(int status, const String& errMsg, const String& logMsg = "");
// Clears the text log area on the M5Stack LCD display
void clearLogArea();
// Updates the screen status bar with current Wi-Fi and BLE states
void updateScreenStatus();
