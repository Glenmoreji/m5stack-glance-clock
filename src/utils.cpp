#include "utils.h"

// ----------------------------------------------------------------------------
// Logs a message to the serial console and M5Stack LCD display.
// ----------------------------------------------------------------------------
void logMessage(const String& consoleMsg, const String& displayMsg) {
    // Output message to serial monitor if available
    if (Serial) {
        Serial.println(consoleMsg);
    }
    
    // Output message to LCD display if height is valid
    if (M5.Display.height() > 0) {
        if (displayMsg.length() > 0) {
            M5.Display.println(displayMsg); // Print specific display message if provided
        } else {
            M5.Display.println(consoleMsg); // Fallback to console message
        }
    }
}

// ----------------------------------------------------------------------------
// Sends an HTTP error response with CORS headers and logs error details.
// ----------------------------------------------------------------------------
void handleErrorResponse(int status, const String& errMsg, const String& logMsg) {
    // Add CORS header to allow cross-origin requests
    server.sendHeader("Access-Control-Allow-Origin", "*");
    // Send HTTP status code and plain text error message
    server.send(status, "text/plain", errMsg);
    // Log the error message (prioritize logMsg if specified)
    logMessage(logMsg.length() > 0 ? logMsg : errMsg);
}

// ----------------------------------------------------------------------------
// Clears the log output region on the M5Stack LCD display.
// ----------------------------------------------------------------------------
void clearLogArea() {
    // Check if display height is large enough to have a log area below the header
    if (M5.Display.height() > 40) {
        // Clear the screen rectangle below the status header with black color
        M5.Display.fillRect(0, 40, M5.Display.width(), M5.Display.height() - 40, BLACK);
        // Reset text cursor position to the top of the log area
        M5.Display.setCursor(0, 40);
    }
}

// ----------------------------------------------------------------------------
// Redraws the top header status bar with current WiFi and BLE state.
// ----------------------------------------------------------------------------
void updateScreenStatus() {
    // Exit early if display is inactive
    if (M5.Display.height() == 0) return;
    
    // Safely retrieve current BLE connection and scanning states using system mutex
    xSemaphoreTake(systemMutex, portMAX_DELAY);
    bool connected = bleState.connected;
    bool doScan = bleState.doScan;
    xSemaphoreGive(systemMutex);

    // Begin batch drawing operation to redraw the header status area
    M5.Display.startWrite();
    M5.Display.fillRect(0, 0, M5.Display.width(), 40, BLACK); // Clear header background
    M5.Display.setCursor(0, 0);
    // Display current IP address if connected to Wi-Fi, otherwise show "Disconnected"
    M5.Display.printf("WiFi: %s\n", WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "Disconnected");
    // Display BLE connection status (Connected, Scanning, or Idle)
    M5.Display.printf("BLE : %s\n", connected ? "Connected" : (doScan ? "Scanning" : "Idle"));
    // Draw a separating horizontal line below the header status
    M5.Display.drawFastHLine(0, 38, M5.Display.width(), WHITE);
    M5.Display.endWrite(); // End batch drawing
}
