#include "wifi_manager.h"
#include "utils.h"
#include "ble_manager.h"
#include "packet_builder.h"
#include "SharedData.h"
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ----------------------------------------------------------------------------
// Generates HTML page for Wi-Fi setup captive portal.
// ----------------------------------------------------------------------------
static String generateSetupHTML(int networkCount) {
    String html;
    html.reserve(1024);
    // Build basic HTML head and form structure for network configuration
    html += F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'></head><body><h1>Wi-Fi Setup</h1>");
    html += F("<form action='/save' method='POST'>");
    html += F("SSID: <select name='ssid' style='font-size:16px; padding:5px;'>");

    // Populate dropdown options based on scanned Wi-Fi networks count
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
    // Disconnect and turn off Wi-Fi to clear previous states
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Switch to station mode temporarily to scan nearby networks
    WiFi.mode(WIFI_STA);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    int n = WiFi.scanNetworks(false, true); // Scan available Wi-Fi networks asynchronously/synchronously

    WiFi.mode(WIFI_AP); // Switch to Access Point mode
    
    // Configure AP static IP parameters
    IPAddress local_ip(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    
    WiFi.softAPConfig(local_ip, gateway, subnet);
    WiFi.softAP(apSSID, nullptr, 1); // Start SoftAP with defined SSID

    vTaskDelay(pdMS_TO_TICKS(500));

    dnsServer.start(53, "*", WiFi.softAPIP()); // Start DNS server to redirect all requests (captive portal)

    // Handle root HTTP request by displaying the network setup form
    server.on("/", HTTP_GET, [n]() {
        server.send(200, "text/html", generateSetupHTML(n));
    });

    // Handle form submission to save credentials to NVS and restart device
    server.on("/save", HTTP_POST, []() {
        if (server.hasArg("ssid") && server.hasArg("pass")) {
            ssid = server.arg("ssid");
            password = server.arg("pass");

            // Save entered SSID and password to NVS storage
            preferences.begin("wifi-config", false);
            preferences.putString("ssid", ssid);
            preferences.putString("pass", password);
            preferences.end();

            server.send(200, "text/html", "Saved! Restarting...");
            
            // Stop servers and Wi-Fi before rebooting
            dnsServer.stop();
            server.stop();
            WiFi.disconnect(true, true);
            WiFi.mode(WIFI_OFF);
            vTaskDelay(pdMS_TO_TICKS(500));

            ESP.restart(); // Restart ESP32 to connect with new credentials
        } else {
            server.send(400, "text/plain", "Bad Request");
        }
    });

    // Catch-all route to redirect unmatched URLs to setup page (Captive portal behavior)
    server.onNotFound([n]() {
        server.send(200, "text/html", generateSetupHTML(n));
    });

    server.begin(); // Start web server

    // Main loop handling captive portal tasks and hard reset trigger
    while (true) {
        M5.update();
        
        // Check if button A is held down for 1 second to clear all configurations and credentials
        if (M5.BtnA.pressedFor(1000)) {
            logMessage("[Portal] Erasing all NVS and Wi-Fi/BLE credentials...");
            
            // Send BLE command to clear bonding info
            uint8_t clearBondsCmd[] = {CMD_CLEAR_BONDS, 0x00, 0x00, 0x00};
            sendGlanceCommand(clearBondsCmd, sizeof(clearBondsCmd));
            vTaskDelay(pdMS_TO_TICKS(100));

            // Erase entire NVS flash memory
            nvs_flash_erase();
            nvs_flash_init();

            WiFi.disconnect(true, true);
            WiFi.mode(WIFI_OFF);

            clearBondInformation(); // Clear internal bond state tracker

            vTaskDelay(pdMS_TO_TICKS(1000));
            ESP.restart(); // Restart device after total reset
        }

        dnsServer.processNextRequest(); // Process DNS queries
        server.handleClient(); // Handle incoming client HTTP requests
        
        // Yield control to prevent triggering the Task Watchdog Timer (WDT)
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ----------------------------------------------------------------------------
// Loads stored credentials and connects to Wi-Fi, falling back to Captive Portal.
// ----------------------------------------------------------------------------
void initWiFi() {
    // Load saved Wi-Fi credentials from NVS preferences
    preferences.begin("wifi-config", true);
    ssid = preferences.getString("ssid", "");
    password = preferences.getString("pass", "");
    preferences.end();

    // If no SSID is configured, launch captive portal mode
    if (ssid == "") {
        logMessage("Wi-Fi configuration not found. Starting Captive Portal.");
        startCaptivePortal();
    }

    logMessage("Connecting to WiFi: " + ssid);
    WiFi.begin(ssid.c_str(), password.c_str());
    int retries = 0;
    // Wait for Wi-Fi connection with retry limit
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
        logMessage(".", "."); 
        retries++;
        if (retries > 20) {
            logMessage("\nWi-Fi connection failed. Switching to Captive Portal.");
            startCaptivePortal(); // Fallback to portal if connection times out
        }
    }
    logMessage("\nWiFi connected.");
    
    // Initialize mDNS service for local hostname resolution
    if (MDNS.begin("glance-clock")) {
        logMessage("mDNS responder started!");
        logMessage("You can access via: http://glance-clock.local");
    } else {
        logMessage("Error setting up MDNS responder!");
    }
}

// ----------------------------------------------------------------------------
// Converts UTC offset in seconds to POSIX TZ format string.
// ----------------------------------------------------------------------------
String getPosixTzFromOffset(long offsetSeconds) {
    long totalMinutes = offsetSeconds / 60;
    long hours = totalMinutes / 60;
    long minutes = abs(totalMinutes) % 60;
    
    // POSIX TZ offset has reversed sign (East is negative)
    long posixHours = -hours;
    
    String tz = "GMT";
    if (posixHours > 0) {
        tz += "+" + String(posixHours);
    } else {
        tz += String(posixHours);
    }
    
    if (minutes > 0) {
        tz += (minutes < 10 ? ":0" : ":");
        tz += String(minutes);
    }
    
    return tz;
}

// ----------------------------------------------------------------------------
// Fetches timezone and offset via IP geolocation API, falling back to NVS.
// ----------------------------------------------------------------------------
void fetchTimeZoneFromIP() {
    bool apiSuccess = false;
    String iana = "";
    long offset = 0;

    // Check if Wi-Fi is connected before making HTTP requests
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin("http://ip-api.com/json/?fields=status,country,regionName,city,timezone,offset,query");
        int httpCode = http.GET();
        if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, payload);
            if (!error) {
                iana = doc["timezone"].as<String>();
                offset = doc["offset"].as<long>();
                if (iana.length() > 0) {
                    apiSuccess = true;
                    logMessage("Timezone fetched: " + iana + " (Offset: " + String(offset) + "s)");
                }
            }
        }
        http.end();
    }

    // If API fetch succeeded, apply API values directly and update NVS backup
    if (apiSuccess) {
        String posixNew = getPosixTzFromOffset(offset);
        sharedData.setIanaTimezone(iana);
        sharedData.setPosixTz(posixNew);
        setenv("TZ", posixNew.c_str(), 1);
        tzset();

        // Overwrite NVS memory with newly fetched IP geolocation settings
        preferences.begin("time-config", false);
        preferences.putString("iana", iana);
        preferences.putLong("offset", offset);
        preferences.putString("posix", posixNew);
        preferences.putBool("valid", true);
        preferences.end();
    } else {
        // Fallback to NVS (or default America/Los_Angeles if NVS is empty) when API fails
        logMessage("API fetch failed. Falling back to NVS timezone configuration.");

        preferences.begin("time-config", true);
        bool isValid = preferences.getBool("valid", false);
        long savedOffset = preferences.getLong("offset", -28800);
        String savedIana = preferences.getString("iana", "America/Los_Angeles");
        preferences.end();

        // If NVS is empty or invalid, write America/Los_Angeles as default fallback baseline
        if (!isValid) {
            savedOffset = -28800; // America/Los_Angeles offset in seconds (-8 hours)
            savedIana = "America/Los_Angeles";
            String defaultPosix = getPosixTzFromOffset(savedOffset);

            preferences.begin("time-config", false);
            preferences.putString("iana", savedIana);
            preferences.putLong("offset", savedOffset);
            preferences.putString("posix", defaultPosix);
            preferences.putBool("valid", true);
            preferences.end();
        }

        String posix = getPosixTzFromOffset(savedOffset);
        sharedData.setIanaTimezone(savedIana);
        sharedData.setPosixTz(posix);
        setenv("TZ", posix.c_str(), 1);
        tzset();
    }
}

// ----------------------------------------------------------------------------
// Refreshes timezone offset from API once per day at UTC 0:00.
// ----------------------------------------------------------------------------
void checkDailyTimezoneSync() {
    if (WiFi.status() != WL_CONNECTED) return;
    
    time_t now;
    struct tm timeinfo;
    time(&now);
    gmtime_r(&now, &timeinfo); // Get UTC time
    
    static int lastSyncedDay = -1;
    
    // Check if UTC day has changed
    if (timeinfo.tm_year > 120) { // Valid time after year 2020
        if (lastSyncedDay == -1) {
            lastSyncedDay = timeinfo.tm_yday;
        } else if (timeinfo.tm_yday != lastSyncedDay) {
            fetchTimeZoneFromIP();
            lastSyncedDay = timeinfo.tm_yday;
            logMessage("Daily timezone refreshed via IP geolocation.");
        }
    }
}

// ----------------------------------------------------------------------------
// Initializes timezone from NVS or dynamic IP lookup and synchronizes time via NTP.
// ----------------------------------------------------------------------------
void initTimeSync() {
    fetchTimeZoneFromIP();

    // Configure and perform NTP time synchronization if connected to the internet
    if (WiFi.status() == WL_CONNECTED) {
        configTime(0, 0, ntpServer);

        // Re-apply environment TZ setting after configTime resets it
        String currentPosixTz = sharedData.getPosixTz();
        setenv("TZ", currentPosixTz.c_str(), 1);
        tzset();

        logMessage("\nSyncing time");
        struct tm timeinfo;
        int retry = 0;
        
        // Loop until local time is successfully retrieved via NTP
        while (!getLocalTime(&timeinfo)) {
            vTaskDelay(pdMS_TO_TICKS(500));
            logMessage(".", ".");
            retry++;
            if (retry > 20) {
                logMessage("\nNTP sync timeout.");
                break;
            }
        }
        
        // Mark NTP as successfully synchronized if retry limit was not exceeded
        if (retry <= 20) {
            sharedData.setNtpSynced(true);
            logMessage("\nTime synchronized.");
        }
    } else {
        logMessage("\nSkipping NTP sync (No internet)");
    }
}
