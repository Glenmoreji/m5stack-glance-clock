#include "config.h"
#include "utils.h"
#include "wifi_manager.h"
#include "ble_manager.h"
#include "packet_builder.h"
#include "http_server.h"

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
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);

    M5.Display.setTextFont(2);
    M5.Display.setTextScroll(true);

    // Initialize Wi-Fi connection or start Captive Portal
    initWiFi();

    // Set scroll area and draw initial screen header
    M5.Display.setScrollRect(0, 40, M5.Display.width(), M5.Display.height() - 40);
    clearLogArea();
    updateScreenStatus();

    // Synchronize system time with NTP server
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    M5.Display.print("\nSyncing time");
    struct tm timeinfo;
    int retry = 0;
    while (!getLocalTime(&timeinfo)) {
        delay(500);
        Serial.print(".");
        M5.Display.print(".");
        retry++;
        if (retry > 20) {
            Serial.println("\nNTP sync timeout.");
            break;
        }
    }
    Serial.println("\nTime synchronized.");
    
    // Register HTTP server endpoint handlers
    setupHttpServer();

    // Load saved BLE target address from NVS Preferences
    preferences.begin("ble-config", true);
    String savedAddr = preferences.getString("targetAddr", "");
    preferences.end();

    if (savedAddr.length() > 0) {
        targetServerAddress = BLEAddress(savedAddr.c_str());
        hasTargetAddress = true;
        Serial.printf("Loaded saved BLE address: %s\n", savedAddr.c_str());
    } else {
        hasTargetAddress = false;
        Serial.println("No saved BLE address found. Performing initial dynamic discovery.");
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
        clearLogArea();
        logMessage("[BtnA] Starting reset of Wi-Fi and BLE configurations...", "Resetting All Config...");

        if (bleState.connected && pClient) {
            pClient->disconnect();
        }
        clearBondInformation();

        preferences.begin("ble-config", false);
        preferences.clear();
        preferences.end();

        preferences.begin("wifi-config", false);
        preferences.clear();
        preferences.end();

        logMessage("[BtnA] Configurations cleared. Restarting...", "Cleared! Restarting...");
        delay(2000);
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
