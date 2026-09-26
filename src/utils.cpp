#include "utils.h"

// ----------------------------------------------------------------------------
// Logs a message to the serial console and M5Stack LCD display.
// ----------------------------------------------------------------------------
// utils.cpp ‚Ì logMessage
void logMessage(const String& consoleMsg, const String& displayMsg) {
    if (Serial && Serial.availableForWrite() > 0) {
        Serial.println(consoleMsg);
    }
    
    if (M5.Display.height() > 0) {
        if (displayMsg.length() > 0) {
            M5.Display.println(displayMsg);
        } else {
            M5.Display.println(consoleMsg);
        }
    }
}

// ----------------------------------------------------------------------------
// Sends an HTTP error response with CORS headers and logs error details.
// ----------------------------------------------------------------------------
void handleErrorResponse(int status, const String& errMsg, const String& logMsg) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(status, "text/plain", errMsg);
    logMessage(logMsg.length() > 0 ? logMsg : errMsg);
}

// ----------------------------------------------------------------------------
// Clears the log output region on the M5Stack LCD display.
// ----------------------------------------------------------------------------
void clearLogArea() {
    if (M5.Display.height() > 40) {
        M5.Display.fillRect(0, 40, M5.Display.width(), M5.Display.height() - 40, BLACK);
        M5.Display.setCursor(0, 40);
    }
}

// ----------------------------------------------------------------------------
// Redraws the top header status bar with current WiFi and BLE state.
// ----------------------------------------------------------------------------
void updateScreenStatus() {
    M5.Display.startWrite();
    M5.Display.fillRect(0, 0, M5.Display.width(), 40, BLACK); 
    M5.Display.setCursor(0, 0);
    M5.Display.printf("WiFi: %s\n", WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "Disconnected");
    M5.Display.printf("BLE : %s\n", bleState.connected ? "Connected" : (bleState.doScan ? "Scanning" : "Idle"));
    M5.Display.drawFastHLine(0, 38, M5.Display.width(), WHITE);
    M5.Display.endWrite();
}
