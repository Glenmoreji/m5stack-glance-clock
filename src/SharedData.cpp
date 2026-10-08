#include "SharedData.h"

SharedData sharedData;

// ----------------------------------------------------------------------------
// Constructor: Initializes the FreeRTOS mutex and default values
// ----------------------------------------------------------------------------
SharedData::SharedData() {
    // Create a FreeRTOS mutex for managing concurrent access to shared variables
    mutex = xSemaphoreCreateMutex();
    // Initialize NTP sync status to false
    ntpSynced = false;
    // Set default IANA timezone to Tokyo
    ianaTimezone = "Asia/Tokyo";
    // Set default POSIX timezone format for Japan
    posixTz = "JST-9";
}

// ----------------------------------------------------------------------------
// Returns a copy of current device settings safely
// ----------------------------------------------------------------------------
DeviceSettings SharedData::getSettings() {
    DeviceSettings temp;
    // Acquire mutex lock with blocking wait to ensure thread safety
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        temp = settings; // Copy current settings
        xSemaphoreGive(mutex); // Release mutex lock
    }
    return temp;
}

// ----------------------------------------------------------------------------
// Safely updates device settings
// ----------------------------------------------------------------------------
void SharedData::setSettings(const DeviceSettings& newSettings) {
    // Acquire mutex lock with blocking wait
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        settings = newSettings; // Update stored settings with new values
        xSemaphoreGive(mutex); // Release mutex lock
    }
}

// ----------------------------------------------------------------------------
// Returns NTP synchronization status safely
// ----------------------------------------------------------------------------
bool SharedData::isNtpSynced() {
    bool synced = false;
    // Acquire mutex lock to read NTP synchronization state
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        synced = ntpSynced;
        xSemaphoreGive(mutex); // Release mutex lock
    }
    return synced;
}

// ----------------------------------------------------------------------------
// Safely updates NTP synchronization status
// ----------------------------------------------------------------------------
void SharedData::setNtpSynced(bool synced) {
    // Acquire mutex lock to write NTP synchronization state
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        ntpSynced = synced;
        xSemaphoreGive(mutex); // Release mutex lock
    }
}

// ----------------------------------------------------------------------------
// Returns IANA timezone string safely
// ----------------------------------------------------------------------------
String SharedData::getIanaTimezone() {
    String tz = "";
    // Acquire mutex lock to safely read the IANA timezone string
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        tz = ianaTimezone;
        xSemaphoreGive(mutex); // Release mutex lock
    }
    return tz;
}

// ----------------------------------------------------------------------------
// Safely updates IANA timezone string
// ----------------------------------------------------------------------------
void SharedData::setIanaTimezone(const String& tz) {
    // Acquire mutex lock to safely update the IANA timezone string
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        ianaTimezone = tz;
        xSemaphoreGive(mutex); // Release mutex lock
    }
}

// ----------------------------------------------------------------------------
// Returns POSIX TZ string safely
// ----------------------------------------------------------------------------
String SharedData::getPosixTz() {
    String tz = "";
    // Acquire mutex lock to safely read the POSIX timezone string
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        tz = posixTz;
        xSemaphoreGive(mutex); // Release mutex lock
    }
    return tz;
}

// ----------------------------------------------------------------------------
// Safely updates POSIX TZ string
// ----------------------------------------------------------------------------
void SharedData::setPosixTz(const String& tz) {
    // Acquire mutex lock to safely update the POSIX timezone string
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        posixTz = tz;
        xSemaphoreGive(mutex); // Release mutex lock
    }
}
