#pragma once
#include <Arduino.h>

// ----------------------------------------------------------------------------
// Timezone and POSIX TZ Conversion Functions
// ----------------------------------------------------------------------------
// Converts UTC offset in seconds to POSIX TZ format string
String getPosixTzFromOffset(long offsetSeconds);

// ----------------------------------------------------------------------------
// Wi-Fi Connection, Captive Portal, and Time Synchronization Functions
// ----------------------------------------------------------------------------
// Fetches the IANA timezone from an IP geolocation API and stores it in NVS
void fetchTimeZoneFromIP();

// Starts the Wi-Fi Access Point and captive portal web server for initial configuration
void startCaptivePortal();

// Loads saved Wi-Fi credentials from NVS and attempts to connect, falling back to AP mode if it fails
void initWiFi();

// Refreshes timezone offset from IP geolocation API daily at UTC 0:00
void checkDailyTimezoneSync();

// Initializes timezone settings from NVS or IP and synchronizes system time via NTP
void initTimeSync();
