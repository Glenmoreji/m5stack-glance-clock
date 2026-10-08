#include "./src/config.h"
#include "./src/utils.h"
#include "./src/wifi_manager.h"
#include "./src/ble_manager.h"
#include "./src/packet_builder.h"
#include "./src/http_server.h"
#include "./src/SharedData.h"

// ----------------------------------------------------------------------------
// Global Variable Definitions
// ----------------------------------------------------------------------------
Preferences preferences; // NVS preference storage instance
String ssid = "";        // Wi-Fi SSID storage
String password = "";    // Wi-Fi password storage
const char* apSSID = "M5Stack-Setup"; // Access point name for captive portal
DNSServer dnsServer;     // DNS server for captive portal redirection
WebServer server(80);    // HTTP web server instance on port 80
const char* ntpServer = "ntp.nict.jp"; // NTP time synchronization server address

BLEClient* pClient = nullptr;                     // BLE client instance for connecting to peripherals
BLERemoteCharacteristic* pRemoteChar = nullptr;   // BLE remote characteristic for writing commands
BLERemoteCharacteristic* pNotifyChar = nullptr;   // BLE remote characteristic for notifications
BLEServer* pServer = nullptr;                     // BLE server instance for local hosting
BLECharacteristic* pCtsCharacteristic = nullptr;  // Current Time Service characteristic

BLEAddress targetServerAddress("00:00:00:00:00:00"); // Target BLE device MAC address holder
bool hasTargetAddress = false; // Flag indicating if a target BLE address is loaded
String webPinCode = "";        // PIN code entered via web interface
bool hasWebPin = false;        // Flag indicating if web PIN has been provided

SemaphoreHandle_t systemMutex = NULL; // FreeRTOS mutex for protecting shared system state
BleConnectionState bleState;          // Current BLE connection and scanning state tracker
bool isWaitingForPin = false;         // Flag indicating if the system is waiting for a PIN entry

// ----------------------------------------------------------------------------
// System Setup Initialization
// ----------------------------------------------------------------------------
void setup() {
    // Create FreeRTOS mutex for safe concurrent access
    systemMutex = xSemaphoreCreateMutex();
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Configure M5Stack hardware settings
    auto cfg = M5.config();
    cfg.serial_baudrate = 0;
    M5.begin(cfg);
    Serial.begin(115200);

    // Wait for Serial monitor connection with a 3-second timeout
    unsigned long start = millis();
    while (!Serial && millis() - start < 3000) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Initialize display properties, font size, and text scrolling area if display is active
    if (M5.Display.height() > 0) {
        M5.Display.setTextFont(2);
        M5.Display.setTextScroll(true);
        if (M5.Display.height() > 40) {
            M5.Display.setScrollRect(0, 40, M5.Display.width(), M5.Display.height() - 40);
        }
    }

    // 1. Initialize Wi-Fi connection or start Captive Portal
    initWiFi();

    // Set scroll area and draw initial screen header
    if (M5.Display.height() > 40) {
        M5.Display.setScrollRect(0, 40, M5.Display.width(), M5.Display.height() - 40);
    }
    clearLogArea();
    updateScreenStatus();

    // 2. Initialize Timezone (NVS load -> IP fetch update -> NTP sync)
    initTimeSync();

    // 3. Register HTTP server endpoint handlers
    setupHttpServer();

    // 4. Load saved BLE target address from NVS Preferences
    preferences.begin("ble-config", true);
    String savedAddr = preferences.getString("targetAddr", "");
    preferences.end();

    // Check and apply saved BLE target address if present
    if (savedAddr.length() > 0) {
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        targetServerAddress = BLEAddress(savedAddr.c_str());
        hasTargetAddress = true;
        xSemaphoreGive(systemMutex);
        logMessage("Loaded saved BLE address: " + savedAddr);
    } else {
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        hasTargetAddress = false;
        xSemaphoreGive(systemMutex);
        logMessage("No saved BLE address found. Performing initial dynamic discovery.");
    }

    // 5. Initialize BLE client and server stack
    setupBLE();
    updateScreenStatus();
}

// ----------------------------------------------------------------------------
// Main Event Loop Execution
// ----------------------------------------------------------------------------
void loop() {
    // Update button states and device input handlers
    M5.update();

    // Button A (1-sec pressed): Send CMD_CLEAR_BONDS, erase NVS, and restart device
    if (M5.BtnA.pressedFor(1000)) {
        logMessage("[BtnA] Erasing all settings and NVS storage...");

        // Prepare and send command packet to clear BLE bonding information
        uint8_t clearBondsCmd[] = {CMD_CLEAR_BONDS, 0x00, 0x00, 0x00};
        if (sendGlanceCommand(clearBondsCmd, sizeof(clearBondsCmd))) {
            logMessage("Sent CMD_CLEAR_BONDS successfully.");
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            logMessage("Failed to send CMD_CLEAR_BONDS.");
        }
        
        // Erase entire NVS flash memory and re-initialize
        nvs_flash_erase();
        nvs_flash_init();

        // Stop servers and disconnect Wi-Fi before rebooting
        dnsServer.stop();
        server.stop();
        WiFi.disconnect(true, true);
        WiFi.mode(WIFI_OFF);
        vTaskDelay(pdMS_TO_TICKS(500));

        ESP.restart();
    }

    // Button B: Send test custom notice packet (Guard with NTP check)
    if (M5.BtnB.wasPressed()) {
        if (!sharedData.isNtpSynced()) {
            logMessage("Notice ignored: NTP not synced yet.");
        } else {
            sendCustomNoticeCommand("I'm back!", 1, 5, 23, 1, 130);
        }
    }

    // Button C: Force CTS time synchronization notification
    if (M5.BtnC.wasPressed()) {
        notifyCTSTime();
    }

    // Monitor Wi-Fi connection and attempt reconnect if dropped
    static unsigned long lastWifiCheck = 0;
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - lastWifiCheck >= 5000) {
            lastWifiCheck = millis();
            logMessage("Wi-Fi disconnected. Reconnecting...", "WiFi disconnected. Reconnecting...");
            updateScreenStatus();
            WiFi.reconnect();
        }
    } else {
        // Handle incoming HTTP client requests when connected
        server.handleClient();
        
        // Check daily timezone synchronization via IP geolocation
        checkDailyTimezoneSync();
    }

    // Execute BLE connection, scanning, and state machine loop
    loopBLE();

    // Yield CPU control briefly using FreeRTOS task delay
    vTaskDelay(pdMS_TO_TICKS(10));
}
