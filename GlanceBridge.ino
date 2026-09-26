#include "./src/config.h"
#include "./src/utils.h"
#include "./src/wifi_manager.h"
#include "./src/ble_manager.h"
#include "./src/packet_builder.h"
#include "./src/http_server.h"

// ----------------------------------------------------------------------------
// Global Variable Definitions
// ----------------------------------------------------------------------------
Preferences preferences;
String ssid = "";
String password = "";
const char* apSSID = "M5Stack-Setup";
DNSServer dnsServer;
WebServer server(80);
const char* ntpServer         = "ntp.nict.jp";
const long  gmtOffset_sec     = 9 * 3600; // Japan Standard Time (JST+9)
const int   daylightOffset_sec = 0;

BLEClient*               pClient            = nullptr;
BLERemoteCharacteristic* pRemoteChar        = nullptr;
BLERemoteCharacteristic* pNotifyChar        = nullptr;
BLEServer*               pServer            = nullptr;
BLECharacteristic*       pCtsCharacteristic = nullptr;

BLEAddress targetServerAddress("00:00:00:00:00:00");
bool hasTargetAddress = false;
String webPinCode = "";
bool hasWebPin = false;

DeviceSettings currentSettings;
BleConnectionState bleState;

// ----------------------------------------------------------------------------
// System Setup Initialization
// ----------------------------------------------------------------------------
void setup() {
    delay(1000);

    auto cfg = M5.config();
    cfg.serial_baudrate = 0;
    M5.begin(cfg);
    Serial.begin(115200);

    unsigned long start = millis();
    while (!Serial && millis() - start < 3000) {
        delay(10);
    }

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

    // Synchronize system time with NTP server
    if (WiFi.status() == WL_CONNECTED) {
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        M5.Display.print("\nSyncing time");
        struct tm timeinfo;
        int retry = 0;
        
        // Wait for response from NTP server (max 10 seconds)
        while (!getLocalTime(&timeinfo)) {
            delay(500);
            if (Serial && Serial.availableForWrite() > 0) {
                Serial.print(".");
            }
            M5.Display.print(".");
            retry++;
            if (retry > 20) {
                logMessage("\nNTP sync timeout.");
                break;
            }
        }
        
        if (retry <= 20) {
            logMessage("\nTime synchronized.");
        }
    } else {
        logMessage("\nSkipping NTP sync (No internet)");
    }
    
    // Register HTTP server endpoint handlers
    setupHttpServer();

    // Load saved BLE target address from NVS Preferences
    preferences.begin("ble-config", true);
    String savedAddr = preferences.getString("targetAddr", "");
    preferences.end();

    if (savedAddr.length() > 0) {
        targetServerAddress = BLEAddress(savedAddr.c_str());
        hasTargetAddress = true;
        logMessage("Loaded saved BLE address: " + savedAddr);
    } else {
        hasTargetAddress = false;
        logMessage("No saved BLE address found. Performing initial dynamic discovery.");
    }

    // Initialize BLE client and server stack
    setupBLE();
    updateScreenStatus();
}

// ----------------------------------------------------------------------------
// Main Event Loop Execution
// ----------------------------------------------------------------------------
void loop() {
    M5.update();
    
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
        server.handleClient();
    }

    // Button A: Reset Wi-Fi and BLE credentials and restart device
    if (M5.BtnA.wasPressed()) {
            clearBondInformation();

            preferences.begin("ble-config", false);
            preferences.clear();
            preferences.end();

            preferences.begin("wifi-config", false);
            preferences.clear();
            preferences.end();

            dnsServer.stop();
            server.stop();
            WiFi.disconnect(true, true);
            WiFi.mode(WIFI_OFF);
            delay(500);

            ESP.restart();
        }

    // Button B: Send test custom notice packet
    if (M5.BtnB.wasPressed()) {
        sendCustomNoticeCommand("I'm back!", 1, 5, 23, 1, 130); // message, anim, sound, color, mod, icon
    }

    // Button C: Force CTS time synchronization notification
    if (M5.BtnC.wasPressed()) {
        notifyCTSTime();
    }

    // Execute BLE connection, scanning, and state machine loop
    loopBLE();

    delay(10);
}
