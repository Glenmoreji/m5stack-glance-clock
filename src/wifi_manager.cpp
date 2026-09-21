#include "wifi_manager.h"
#include "utils.h"
#include "ble_manager.h" // For clearBondInformation
#include <ESPmDNS.h>

// ----------------------------------------------------------------------------
// Starts Access Point mode and HTTP captive portal for Wi-Fi setup.
// ----------------------------------------------------------------------------
void startCaptivePortal() {
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSSID);
    dnsServer.start(53, "*", WiFi.softAPIP());

    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Scanning Wi-Fi networks...");

    int n = WiFi.scanNetworks();

    // Serve HTML configuration page with scanned Wi-Fi SSIDs
    server.on("/", HTTP_GET, [n]() {
        String html;
        html.reserve(1024); // Pre-allocate memory to prevent fragmentation
        html += F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'></head><body><h1>Wi-Fi Setup</h1>");
        html += F("<form action='/save' method='POST'>");
        html += F("SSID: <select name='ssid' style='font-size:16px; padding:5px;'>");

        if (n == 0) {
            html += F("<option value=''>No networks found</option>");
        } else {
            for (int i = 0; i < n; ++i) {
                String scannedSSID = WiFi.SSID(i);
                if (scannedSSID.length() > 0) {
                    html += "<option value='" + scannedSSID + "'>" + scannedSSID + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
                }
            }
        }

        html += F("</select><br><br>");
        html += F("Password: <input type='password' name='pass' style='font-size:16px; padding:5px;'><br><br>");
        html += F("<input type='submit' value='Save' style='font-size:16px; padding:5px 15px;'>");
        html += F("</form></body></html>");
        
        server.send(200, "text/html", html);
    });

    // Handle credential submission and store settings in Preferences
    server.on("/save", HTTP_POST, []() {
        if (server.hasArg("ssid") && server.hasArg("pass")) {
            ssid = server.arg("ssid");
            password = server.arg("pass");

            preferences.begin("wifi-config", false);
            preferences.putString("ssid", ssid);
            preferences.putString("pass", password);
            preferences.end();

            server.send(200, "text/html", "Saved! Restarting...");
            delay(2000);
            ESP.restart();
        } else {
            server.send(400, "text/plain", "Bad Request");
        }
    });

    // Redirect unhandled requests to captive portal root page
    server.onNotFound([]() {
        server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
        server.send(302, "text/plain", "");
    });

    server.begin();
    
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);
    logMessage("--- Captive Portal ---");
    M5.Display.printf("Connect to AP: %s\n", apSSID);
    M5.Display.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());

    // Event processing loop for DNS/HTTP requests and hard reset via BtnA
    while (true) {
        M5.update();
        if (M5.BtnA.wasPressed()) {
            M5.Display.fillScreen(BLACK);
            M5.Display.setCursor(0, 0);
            logMessage("[BtnA] Reset requested during Captive Portal...", "Resetting All Config...");

            clearBondInformation();

            preferences.begin("ble-config", false);
            preferences.clear();
            preferences.end();

            preferences.begin("wifi-config", false);
            preferences.clear();
            preferences.end();

            delay(1000);
            ESP.restart();
        }

        dnsServer.processNextRequest();
        server.handleClient();
        // Prevent Watchdog Timer reset during loop
        delay(10);
    }
}

// ----------------------------------------------------------------------------
// Loads stored credentials and connects to Wi-Fi, falling back to Captive Portal.
// ----------------------------------------------------------------------------
void initWiFi() {
    preferences.begin("wifi-config", true);
    ssid = preferences.getString("ssid", "");
    password = preferences.getString("pass", "");
    preferences.end();

    if (ssid == "") {
        Serial.println("Wi-Fi configuration not found. Starting Captive Portal.");
        startCaptivePortal();
    }

    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.print("Connecting to WiFi");
    WiFi.begin(ssid.c_str(), password.c_str());
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        M5.Display.print(".");
        retries++;
        if (retries > 20) {
            Serial.println("\nWi-Fi connection failed. Switching to Captive Portal.");
            startCaptivePortal();
        }
    }
    Serial.println("\nWiFi connected.");
        
    if (MDNS.begin("glance-clock")) {
        Serial.println("mDNS responder started!");
        Serial.println("You can access via: http://glance-clock.local");
    } else {
        Serial.println("Error setting up MDNS responder!");
    }
}
