#include "wifi_manager.h"
#include "utils.h"
#include "ble_manager.h" // For clearBondInformation
#include <ESPmDNS.h>

// ----------------------------------------------------------------------------
// Generates HTML page for Wi-Fi setup captive portal.
// ----------------------------------------------------------------------------
static String generateSetupHTML(int networkCount) {
    String html;
    html.reserve(1024); // Pre-allocate memory to prevent fragmentation
    html += F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'></head><body><h1>Wi-Fi Setup</h1>");
    html += F("<form action='/save' method='POST'>");
    html += F("SSID: <select name='ssid' style='font-size:16px; padding:5px;'>");

    if (networkCount <= 0) {
        html += F("<option value=''>No networks found</option>");
    } else {
        for (int i = 0; i < networkCount; ++i) {
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
    return html;
}

// ----------------------------------------------------------------------------
// Starts Access Point mode and HTTP captive portal for Wi-Fi setup.
// ----------------------------------------------------------------------------
void startCaptivePortal() {
    // 1. Reset Wi-Fi state completely
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(100);

    // 2. Set to STA mode and perform a quick scan
    WiFi.mode(WIFI_STA);
    delay(100);
    
    // Scan networks asynchronously or with short timeout
    int n = WiFi.scanNetworks(false, true); // async=false, show_hidden=true

    // 3. Switch to AP mode
    WiFi.mode(WIFI_AP);
    
    IPAddress local_ip(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    
    WiFi.softAPConfig(local_ip, gateway, subnet);
    // Force WiFi channel to 1 for better compatibility
    bool apResult = WiFi.softAP(apSSID, nullptr, 1); 
    
    delay(500); // Wait for AP stack to initialize

    // Redirect all DNS requests to local AP IP address
    dnsServer.start(53, "*", WiFi.softAPIP());

    // Serve HTML configuration page on root path
    server.on("/", HTTP_GET, [n]() {
        server.send(200, "text/html", generateSetupHTML(n));
    });

    // Handle credential submission
    server.on("/save", HTTP_POST, []() {
        if (server.hasArg("ssid") && server.hasArg("pass")) {
            ssid = server.arg("ssid");
            password = server.arg("pass");

            preferences.begin("wifi-config", false);
            preferences.putString("ssid", ssid);
            preferences.putString("pass", password);
            preferences.end();

            server.send(200, "text/html", "Saved! Restarting...");
            
            dnsServer.stop();
            server.stop();
            WiFi.disconnect(true, true);
            WiFi.mode(WIFI_OFF);
            delay(500);

            ESP.restart();
        } else {
            server.send(400, "text/plain", "Bad Request");
        }
    });

    server.onNotFound([n]() {
        server.send(200, "text/html", generateSetupHTML(n));
    });

    server.begin();

    // Loop processing
    while (true) {
        M5.update();
        
        if (M5.BtnA.wasPressed()) {
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
        delay(10); // Yield to prevent Watchdog reset
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
        logMessage("Wi-Fi configuration not found. Starting Captive Portal.");
        startCaptivePortal();
    }

    logMessage("Connecting to WiFi: " + ssid);
    WiFi.begin(ssid.c_str(), password.c_str());
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);

        logMessage(".", "."); 
        retries++;
        if (retries > 20) {
            logMessage("\nWi-Fi connection failed. Switching to Captive Portal.");
            startCaptivePortal();
        }
    }
    logMessage("\nWiFi connected.");
        
    if (MDNS.begin("glance-clock")) {
        logMessage("mDNS responder started!");
        logMessage("You can access via: http://glance-clock.local");
    } else {
        logMessage("Error setting up MDNS responder!");
    }
}
