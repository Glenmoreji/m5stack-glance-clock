#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "config.h"

// ----------------------------------------------------------------------------
// Thread-Safe Shared Data Access Wrapper Class
// ----------------------------------------------------------------------------
class SharedData {
private:
    SemaphoreHandle_t mutex; // FreeRTOS mutex for thread-safe variable access
    DeviceSettings settings; // Current device configuration settings
    bool ntpSynced;          // Flag indicating if NTP time synchronization is complete
    String ianaTimezone;     // Current IANA timezone string (e.g., "Asia/Tokyo")
    String posixTz;          // Current POSIX timezone string (e.g., "JST-9")

public:
    SharedData();
    
    // Settings getters and setters with Mutex protection
    DeviceSettings getSettings();
    void setSettings(const DeviceSettings& newSettings);
    
    // NTP & Timezone state accessors
    bool isNtpSynced();
    void setNtpSynced(bool synced);
    
    String getIanaTimezone();
    void setIanaTimezone(const String& tz);
    
    String getPosixTz();
    void setPosixTz(const String& tz);
};

extern SharedData sharedData;
