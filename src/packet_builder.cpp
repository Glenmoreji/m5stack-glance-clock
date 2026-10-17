#include "packet_builder.h"
#include "utils.h"
#include "SharedData.h"

// ----------------------------------------------------------------------------
// Encodes a 64-bit integer into Protobuf Varint format byte array.
// ----------------------------------------------------------------------------
size_t encodeVarint(uint8_t* buf, uint64_t value) {
    size_t i = 0;
    // Loop through value bytes using 7-bit shifts and set continuation bit (0x80) for subsequent bytes
    while (value >= 0x80) {
        buf[i++] = (uint8_t)((value & 0x7F) | 0x80);
        value >>= 7;
    }
    buf[i++] = (uint8_t)(value & 0x7F);
    return i;
}

// ----------------------------------------------------------------------------
// Encodes a 64-bit integer into Protobuf Varint format vector.
// ----------------------------------------------------------------------------
void encodeVarintVector(std::vector<uint8_t>& buffer, uint64_t value) {
    // Dynamically push encoded varint bytes into the target std::vector buffer
    while (value >= 0x80) {
        buffer.push_back(static_cast<uint8_t>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    buffer.push_back(static_cast<uint8_t>(value & 0x7F));
}

// ----------------------------------------------------------------------------
// Sends a raw command packet to Glance Clock via BLE with safe mutex lock and fallback.
// ----------------------------------------------------------------------------
bool sendGlanceCommand(const uint8_t* cmdData, size_t length) {
    // Safely retrieve BLE connection and security status flags under mutex protection
    xSemaphoreTake(systemMutex, portMAX_DELAY);

    // Validate that device is connected, authenticated, and remote characteristic is available
    if (!bleState.connected || !bleState.isAuthReady || pRemoteChar == nullptr || pClient == nullptr || !pClient->isConnected()) {
        xSemaphoreGive(systemMutex);
        logMessage(" -> Send failed: Not connected or Auth/Encryption not ready", " -> Failed: Security not ready");
        return false;
    }
    
    // Verify that the target characteristic supports write operations
    if (!pRemoteChar->canWrite() && !pRemoteChar->canWriteNoResponse()) {
        xSemaphoreGive(systemMutex);
        logMessage(" -> Send failed: Characteristic is not writable", " -> Failed: Cannot write");
        return false;
    }

    bool success = false;
    try {
        // Attempt writing command byte array with retries to ensure reliable transmission
        for (int attempt = 1; attempt <= 3; attempt++) {
            if (pRemoteChar->canWrite() && pRemoteChar->writeValue((uint8_t*)cmdData, length, true)) {
                success = true;
                break;
            }
            if (pRemoteChar->canWriteNoResponse() && pRemoteChar->writeValue((uint8_t*)cmdData, length, false)) {
                success = true;
                break;
            }
            xSemaphoreGive(systemMutex);
            delay(100);
            xSemaphoreTake(systemMutex, portMAX_DELAY);
            if (!bleState.connected || pRemoteChar == nullptr) {
                break;
            }
        }
    } catch (...) {
        xSemaphoreGive(systemMutex);
        logMessage(" -> Exception caught during writeValue.", " -> Write Exception");
        return false;
    }

    xSemaphoreGive(systemMutex);

    if (!success) {
        logMessage(" -> Write failed after attempts.", " -> Write failed");
    }
    return success;
}

// ----------------------------------------------------------------------------
// Constructs current local time data formatted for Current Time Service (CTS).
// ----------------------------------------------------------------------------
bool createCtsTimeData(uint8_t* currentTime) {
    // Ensure NTP time synchronization has been completed prior to generating CTS data
    if (!sharedData.isNtpSynced()) {
        logMessage(" -> CTS Data Failed: NTP not synced.");
        return false;
    }

    struct tm timeinfo;
    // Retrieve local time structure from system time (ensuring correct TZ environment is set)
    if (!getLocalTime(&timeinfo)) {
        logMessage(" -> CTS Data Failed: getLocalTime failed.");
        return false;
    }

    // Map time structure fields into standard 10-byte Current Time Service (CTS) format
    int year = timeinfo.tm_year + 1900;
    currentTime[0] = year & 0xFF;
    currentTime[1] = (year >> 8) & 0xFF;
    currentTime[2] = timeinfo.tm_mon + 1;
    currentTime[3] = timeinfo.tm_mday;
    currentTime[4] = timeinfo.tm_hour;
    currentTime[5] = timeinfo.tm_min;
    currentTime[6] = timeinfo.tm_sec;
    currentTime[7] = (timeinfo.tm_wday == 0) ? 7 : timeinfo.tm_wday;
    currentTime[8] = 0;
    currentTime[9] = 1;

    // Output the generated 10-byte CTS payload in HEX format for verification
    char hexLog[64];
    snprintf(hexLog, sizeof(hexLog), " -> CTS 10-byte HEX: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
             currentTime[0], currentTime[1], currentTime[2], currentTime[3], currentTime[4],
             currentTime[5], currentTime[6], currentTime[7], currentTime[8], currentTime[9]);
    logMessage(String(hexLog));

    // Log the generated time formatted for CTS transmission
    char timeStr[64];
    snprintf(timeStr, sizeof(timeStr), " -> CTS Time generated: %04d-%02d-%02d %02d:%02d:%02d",
             year, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    logMessage(String(timeStr));

    return true;
}

// ----------------------------------------------------------------------------
// Generates and transmits device configuration settings packet.
// ----------------------------------------------------------------------------
void sendSettingsCommand() {
    // Retrieve current device settings from shared data instance
    DeviceSettings currentSettings = sharedData.getSettings();
    int brightPercent = (currentSettings.brightness * 100) / 255;
    if (brightPercent > 100) brightPercent = 100;

    // Initialize protobuf packet vector with settings prefix bytes
    std::vector<uint8_t> packet(Prefix::SETTINGS, Prefix::SETTINGS + sizeof(Prefix::SETTINGS));
    packet.reserve(32); 

    // Append all configuration flags and values mapped with respective Protobuf field tags
    packet.insert(packet.end(), {
        PB_TAG(2, PbWire::VARINT), (uint8_t)(currentSettings.nightMode ? 0x01 : 0x00),
        PB_TAG(3, PbWire::VARINT), (uint8_t)(currentSettings.permanentDND ? 0x01 : 0x00),
        PB_TAG(4, PbWire::VARINT), (uint8_t)(currentSettings.permanentMute ? 0x01 : 0x00),
        PB_TAG(5, PbWire::VARINT), (uint8_t)currentSettings.dateFormat,
        PB_TAG(9, PbWire::VARINT), (uint8_t)(currentSettings.pointsEnabled ? 0x01 : 0x00),
        PB_TAG(10, PbWire::VARINT), (uint8_t)brightPercent,
        PB_TAG(11, PbWire::VARINT), (uint8_t)(currentSettings.timeModeEnable ? 0x01 : 0x00),
        PB_TAG(12, PbWire::VARINT), (uint8_t)(currentSettings.timeFormat12 ? 0x01 : 0x00)
    });

    // Send the settings packet via BLE
    if (sendGlanceCommand(packet.data(), packet.size())) {
        logMessage(" -> Settings command sent.", " -> Settings Applied");
    }
}

// ----------------------------------------------------------------------------
// Synchronizes current time and settings directly via Data Characteristic.
// ----------------------------------------------------------------------------
void notifyCTSTime() {
    logMessage("CTS sync start");

    // Check NTP synchronization status before sending time sync
    if (!sharedData.isNtpSynced()) {
        logMessage(" -> Failed: NTP synchronization not completed yet.", " -> NTP Sync Required");
        return;
    }

    // Check first pairing status flag under mutex protection
    xSemaphoreTake(systemMutex, portMAX_DELAY);
    bool firstPairing = bleState.isFirstPairing;
    xSemaphoreGive(systemMutex);

    // If it is initial pairing, apply default settings and reset the pairing flag
    if (firstPairing) {
        logMessage("Initial pairing detected: Applying default settings.", "Initial Pairing");
        
        // Execute Alarm Clear once on initial pairing / post-update
        uint8_t clearAlarmCmd[] = {CMD_ALARM_CLEAR, 0x00, 0x00, 0x00};
        if (sendGlanceCommand(clearAlarmCmd, sizeof(clearAlarmCmd))) {
            logMessage(" -> Initial Alarm Clear sent successfully.");
        } else {
            logMessage(" -> Failed to send Initial Alarm Clear.");
        }
        vTaskDelay(pdMS_TO_TICKS(50));
        
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        bleState.isFirstPairing = false;
        xSemaphoreGive(systemMutex);
    }
    
    sendSettingsCommand();
    vTaskDelay(pdMS_TO_TICKS(50));

    // Update local CTS characteristic value and notify connected clock via NimBLE
    if (pCtsCharacteristic != nullptr) {
        uint8_t currentTime[10];
        if (!createCtsTimeData(currentTime)) {
            logMessage(" -> Failed: Unable to obtain NTP time.", " -> Failed to obtain time");
            return;
        }

        logMessage("CTS setValue()");
        pCtsCharacteristic->setValue(currentTime, 10);
        logMessage("setValue done");

        logMessage("CTS notify() executing...");
        bool notified = pCtsCharacteristic->notify();
        logMessage(String("CTS notify result=") + (notified ? "true" : "false"));
        logMessage("notify done");

        logMessage(" -> Display turned on & NTP time sync sent.", " -> Display On & Time Synced");
    } else {
        logMessage(" -> Failed: pCtsCharacteristic is nullptr.");
    }
}

// ----------------------------------------------------------------------------
// Constructs text payload buffer with optional leading and trailing icons.
// ----------------------------------------------------------------------------
std::vector<uint8_t> buildTextPayloadWithIcon(int mod, const String& message, int leadingIcon, int trailingIcon) {
    std::vector<uint8_t> contentBuffer;
    size_t msgLen = message.length();
    contentBuffer.reserve(msgLen + 8);
    
    // Append leading icon byte with tag if specified
    if (leadingIcon > 0) {
        contentBuffer.push_back(PB_TAG(24, PbWire::BYTES));
        contentBuffer.push_back((uint8_t)leadingIcon);
    }
    
    // Append text message bytes if present
    if (msgLen > 0) {
        const uint8_t* msgBytes = (const uint8_t*)message.c_str();
        contentBuffer.insert(contentBuffer.end(), msgBytes, msgBytes + msgLen);
    }

    // Append trailing icon byte with tag if specified
    if (trailingIcon > 0) {
        contentBuffer.push_back(PB_TAG(24, PbWire::BYTES));
        contentBuffer.push_back((uint8_t)trailingIcon);
    }

    // Wrap content buffer into a text payload structure with modifier and length prefix
    std::vector<uint8_t> textPayload;
    textPayload.reserve(contentBuffer.size() + 8);
    textPayload.push_back(PB_TAG(1, PbWire::VARINT));
    textPayload.push_back((uint8_t)mod);
    
    textPayload.push_back(PB_TAG(2, PbWire::BYTES));
    encodeVarintVector(textPayload, contentBuffer.size());
    textPayload.insert(textPayload.end(), contentBuffer.begin(), contentBuffer.end());

    return textPayload;
}

// ----------------------------------------------------------------------------
// Builds and transmits a custom notification command packet.
// ----------------------------------------------------------------------------
void sendCustomNoticeCommand(const String& message, int anim, int sound, int color, int mod, int icon) {
    // Initialize packet vector with notice prefix bytes
    std::vector<uint8_t> packet(Prefix::NOTICE, Prefix::NOTICE + sizeof(Prefix::NOTICE));
    packet.reserve(32 + message.length());
    
    // Append animation, sound, and color properties
    packet.insert(packet.end(), {
        PB_TAG(1, PbWire::VARINT), (uint8_t)anim,
        PB_TAG(2, PbWire::VARINT), (uint8_t)sound,
        PB_TAG(3, PbWire::VARINT), (uint8_t)color,
    });

    // Build nested text payload containing the notice message and icon
    std::vector<uint8_t> textPayload = buildTextPayloadWithIcon(mod, message, icon);

    packet.push_back(PB_TAG(4, PbWire::BYTES));
    encodeVarintVector(packet, textPayload.size());
    packet.insert(packet.end(), textPayload.begin(), textPayload.end());
    
    // Transmit completed notice packet via BLE
    if (sendGlanceCommand(packet.data(), packet.size())) {
        logMessage(" -> Message sent successfully: '" + message + "' (Icon: " + String(icon) + ")", 
                   " -> Sent: " + message + " (Icon:" + String(icon) + ")");
    }
}

// ----------------------------------------------------------------------------
// Builds a ring LED chart display packet for the forecast scene.
// ----------------------------------------------------------------------------
std::vector<uint8_t> buildRingPacket(const std::vector<int16_t>& ringValues, uint8_t slot, uint32_t maxColor, uint32_t minColor, int16_t maxValue, int16_t minValue, const String& message) {
    // Initialize packet vector with forecast scene prefix and slot identifier
    std::vector<uint8_t> packet(Prefix::FORECAST_SCENE, Prefix::FORECAST_SCENE + sizeof(Prefix::FORECAST_SCENE));
    packet.push_back(slot);
    size_t msgLen = message.length();
    packet.reserve(64 + msgLen);

    time_t now;
    time(&now);
    uint64_t timestamp = (uint64_t)now;
    
    // Append current timestamp, colors, and value range limits
    packet.insert(packet.end(), { PB_TAG(1, PbWire::VARINT) });
    encodeVarintVector(packet, timestamp);
    
    packet.insert(packet.end(), { PB_TAG(4, PbWire::VARINT) });
    encodeVarintVector(packet, maxColor);
    
    packet.insert(packet.end(), { PB_TAG(5, PbWire::VARINT) });
    encodeVarintVector(packet, minColor);

    packet.insert(packet.end(), { PB_TAG(6, PbWire::VARINT) });
    encodeVarintVector(packet, static_cast<uint64_t>(static_cast<int32_t>(maxValue)));
    
    packet.insert(packet.end(), { PB_TAG(7, PbWire::VARINT) });
    encodeVarintVector(packet, static_cast<uint64_t>(static_cast<int32_t>(minValue)));

    // Build 24-element ring chart values byte buffer (2 bytes per value) without vector copy
    std::vector<uint8_t> ringDataBytes;
    ringDataBytes.reserve(48);
    
    for (size_t i = 0; i < 24; i++) {
        int16_t val = (i < ringValues.size()) ? ringValues[i] : (ringValues.empty() ? 0 : ringValues.back());
        ringDataBytes.push_back(val & 0xFF);
        ringDataBytes.push_back((val >> 8) & 0xFF);
    }
    
    packet.insert(packet.end(), { PB_TAG(8, PbWire::BYTES) });
    encodeVarintVector(packet, ringDataBytes.size());
    packet.insert(packet.end(), ringDataBytes.begin(), ringDataBytes.end());

    // Append display label message string
    packet.insert(packet.end(), { PB_TAG(9, PbWire::BYTES) });
    encodeVarintVector(packet, static_cast<uint64_t>(msgLen));
    const uint8_t* msgBytes = (const uint8_t*)message.c_str();
    packet.insert(packet.end(), msgBytes, msgBytes + msgLen);
    
    return packet;
}

// ----------------------------------------------------------------------------
// Builds a countdown timer configuration packet.
// ----------------------------------------------------------------------------
std::vector<uint8_t> buildTimerPacket(int duration, const String& label, const String& message) {
    // Initialize packet vector with timer prefix bytes
    std::vector<uint8_t> packet(Prefix::TIMER, Prefix::TIMER + sizeof(Prefix::TIMER));
    size_t labelLen = label.length();
    size_t msgLen = message.length();
    packet.reserve(64 + labelLen + msgLen);

    std::vector<uint8_t> intervalParts;
    intervalParts.reserve(32 + labelLen);

    // If a label is provided, build and insert text payload into interval parts
    if (labelLen > 0) {
        int mod = 0x01;
        int icon = 0x00;
        std::vector<uint8_t> textData = buildTextPayloadWithIcon(mod, label, icon);

        intervalParts.push_back(PB_TAG(1, PbWire::BYTES));
        encodeVarintVector(intervalParts, textData.size());
        intervalParts.insert(intervalParts.end(), textData.begin(), textData.end());
    }

    // Append countdown duration value
    intervalParts.push_back(PB_TAG(2, PbWire::VARINT));
    encodeVarintVector(intervalParts, duration);

    packet.push_back(PB_TAG(2, PbWire::BYTES));
    encodeVarintVector(packet, intervalParts.size());
    packet.insert(packet.end(), intervalParts.begin(), intervalParts.end());

    // If an expiration message is provided, build and insert final text payload
    if (msgLen > 0) {
        int mod = 0x01;
        int icon = 0x00;
        std::vector<uint8_t> finalTextData = buildTextPayloadWithIcon(mod, message, icon);

        packet.push_back(PB_TAG(3, PbWire::BYTES));
        encodeVarintVector(packet, finalTextData.size());
        packet.insert(packet.end(), finalTextData.begin(), finalTextData.end());
    }

    return packet;
}

// ----------------------------------------------------------------------------
// Builds an alarm configuration packet.
// ----------------------------------------------------------------------------
std::vector<uint8_t> buildAlarmPacket(uint8_t days, uint8_t hour, uint8_t minutes, uint8_t sound, const String& message) {
    // Initialize packet vector with alarm prefix bytes
    std::vector<uint8_t> packet(Prefix::ALARM, Prefix::ALARM + sizeof(Prefix::ALARM));
    size_t msgLen = message.length();
    packet.reserve(64 + msgLen);

    std::vector<uint8_t> alarmData;
    alarmData.reserve(32 + msgLen);

    // Append alarm activation state and active days bitmask
    alarmData.insert(alarmData.end(), {PB_TAG(2, PbWire::VARINT), 0x01});
    alarmData.insert(alarmData.end(), {PB_TAG(3, PbWire::VARINT), days});

    // Build nested time payload containing hour and minute values
    std::vector<uint8_t> timePayload;
    timePayload.reserve(8);
    timePayload.insert(timePayload.end(), {PB_TAG(1, PbWire::VARINT), hour});
    timePayload.insert(timePayload.end(), {PB_TAG(2, PbWire::VARINT), minutes});

    alarmData.push_back(PB_TAG(4, PbWire::BYTES));
    encodeVarintVector(alarmData, timePayload.size());
    alarmData.insert(alarmData.end(), timePayload.begin(), timePayload.end());

    // Append alarm sound identifier
    alarmData.insert(alarmData.end(), {PB_TAG(5, PbWire::VARINT), sound});

    // If an alarm message is provided, append text payload
    if (msgLen > 0) {
        int mod = 0x01;
        int icon = 0x82;
        std::vector<uint8_t> textPayload = buildTextPayloadWithIcon(mod, message, icon);

        alarmData.push_back(PB_TAG(6, PbWire::BYTES));
        encodeVarintVector(alarmData, textPayload.size());
        alarmData.insert(alarmData.end(), textPayload.begin(), textPayload.end());
    }

    packet.push_back(PB_TAG(1, PbWire::BYTES));
    encodeVarintVector(packet, alarmData.size());
    packet.insert(packet.end(), alarmData.begin(), alarmData.end());

    return packet;
}
