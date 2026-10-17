#include "./src/config.h"
#include "./src/utils.h"
#include "./src/wifi_manager.h"
#include "./src/ble_manager.h"
#include "./src/packet_builder.h"
#include "./src/http_server.h"
#include "./src/mqtt_manager.h"
#include "./src/SharedData.h"

// ----------------------------------------------------------------------------
// Global Variable Definitions
// ----------------------------------------------------------------------------
Preferences preferences;                            // NVS preference storage instance
String ssid = "";                                   // Wi-Fi SSID storage
String password = "";                               // Wi-Fi password storage
const char* apSSID = "M5Stack-Setup";               // Access point name for captive portal
DNSServer dnsServer;                                // DNS server for captive portal redirection
WebServer server(80);                               // HTTP web server instance on port 80
const char* ntpServer = "ntp.nict.jp";              // NTP time synchronization server address

NimBLEClient* pClient = nullptr;                    // BLE client instance for connecting to peripherals
NimBLERemoteCharacteristic* pRemoteChar = nullptr;  // BLE remote characteristic for writing commands
NimBLERemoteCharacteristic* pNotifyChar = nullptr;  // BLE remote characteristic for notifications
NimBLEServer* pServer = nullptr;                    // BLE server instance for local hosting
NimBLECharacteristic* pCtsCharacteristic = nullptr; // Current Time Service characteristic

NimBLEAddress targetServerAddress("00:00:00:00:00:00", BLE_ADDR_RANDOM); // Target BLE device MAC address holder
bool hasTargetAddress = false;                      // Flag indicating if a target BLE address is loaded
String webPinCode = "";                             // PIN code entered via web interface
bool hasWebPin = false;                             // Flag indicating if web PIN has been provided

SemaphoreHandle_t systemMutex = NULL;               // FreeRTOS mutex for protecting shared system state
BleConnectionState bleState;                        // Current BLE connection and scanning state tracker
bool isWaitingForPin = false;                       // Flag indicating if the system is waiting for a PIN entry

// ----------------------------------------------------------------------------
// Network Background Task (Pinned to Core 0)
// ----------------------------------------------------------------------------
void networkTask(void *pvParameters) {
    while (true) {
        // Monitor Wi-Fi connection and attempt reconnect if dropped
        if (WiFi.status() != WL_CONNECTED) {
            logMessage("Wi-Fi disconnected. Reconnecting...", "WiFi disconnected. Reconnecting...");
            updateScreenStatus();
            WiFi.reconnect();
            vTaskDelay(pdMS_TO_TICKS(5000));
        } else {
            // Handle incoming HTTP client requests, MQTT communication, and check daily timezone synchronization
            server.handleClient();
            loopMQTT();
            checkDailyTimezoneSync();
        }
        
        // Yield CPU control briefly on Core 0
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

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

    // Initialize Wi-Fi connection or start Captive Portal
    initWiFi();

    // Set scroll area and draw initial screen header
    if (M5.Display.height() > 40) {
        M5.Display.setScrollRect(0, 40, M5.Display.width(), M5.Display.height() - 40);
    }
    clearLogArea();
    updateScreenStatus();

    // Initialize Timezone (NVS load -> IP fetch update -> NTP sync)
    initTimeSync();

    // Register HTTP server endpoint handlers and MQTT configuration
    setupHttpServer();
    setupMQTT();

    // Load saved BLE target address from NVS Preferences
    preferences.begin("ble-config", true);
    String savedAddr = preferences.getString("targetAddr", "");
    preferences.end();

    // Check and apply saved BLE target address if present
    if (savedAddr.length() > 0) {
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        targetServerAddress = NimBLEAddress(savedAddr.c_str(), BLE_ADDR_RANDOM);
        hasTargetAddress = true;
        xSemaphoreGive(systemMutex);
        logMessage("Loaded saved BLE address: " + savedAddr);
    } else {
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        hasTargetAddress = false;
        xSemaphoreGive(systemMutex);
        logMessage("No saved BLE address found. Performing initial dynamic discovery.");
    }

    // Initialize BLE client and server stack
    setupBLE();
    updateScreenStatus();

    // Create and pin network background processing task to Core 0
    xTaskCreatePinnedToCore(
        networkTask,   // Task function
        "NetworkTask", // Task name
        4096,          // Stack size
        NULL,          // Parameters
        1,             // Priority
        NULL,          // Task handle
        0              // Core ID (Core 0)
    );
}

// ----------------------------------------------------------------------------
// Main Event Loop Execution (Pinned to Core 1)
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

    // Execute BLE connection, scanning, and state machine loop on Core 1
    loopBLE();

    // Yield CPU control briefly using FreeRTOS task delay on Core 1
    vTaskDelay(pdMS_TO_TICKS(10));
}
