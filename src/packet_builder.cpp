#include "packet_builder.h"
#include "utils.h"

// ----------------------------------------------------------------------------
// Encodes a 64-bit integer into Protobuf Varint format byte array.
// ----------------------------------------------------------------------------
size_t encodeVarint(uint8_t* buf, uint64_t value) {
    size_t i = 0;
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
    while (value >= 0x80) {
        buffer.push_back(static_cast<uint8_t>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    buffer.push_back(static_cast<uint8_t>(value & 0x7F));
}

// ----------------------------------------------------------------------------
// Sends a raw command packet to Glance Clock via BLE.
// ----------------------------------------------------------------------------
bool sendGlanceCommand(const uint8_t* cmdData, size_t length) {
    if (!bleState.connected || pRemoteChar == nullptr) {
        logMessage(" -> Send failed: Not connected or characteristic unavailable", " -> Failed: Not connected");
        return false;
    }
    
    if (!pRemoteChar->canWrite()) {
        logMessage(" -> Send failed: Characteristic is not writable", " -> Failed: Cannot write");
        return false;
    }

    pRemoteChar->writeValue((uint8_t*)cmdData, length, true);
    return true;
}

// ----------------------------------------------------------------------------
// Constructs current local time data formatted for Current Time Service (CTS).
// ----------------------------------------------------------------------------
bool createCtsTimeData(uint8_t* currentTime) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return false;
    }

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

    return true;
}

// ----------------------------------------------------------------------------
// Generates and transmits device configuration settings packet.
// ----------------------------------------------------------------------------
void sendSettingsCommand() {
    int brightPercent = (currentSettings.brightness * 100) / 255;
    if (brightPercent > 100) brightPercent = 100;

    std::vector<uint8_t> packet(Prefix::SETTINGS, Prefix::SETTINGS + sizeof(Prefix::SETTINGS));
    packet.reserve(32); 
    
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

    if (sendGlanceCommand(packet.data(), packet.size())) {
        logMessage(" -> Settings command sent.", " -> Settings Applied");
    }
}

// ----------------------------------------------------------------------------
// Synchronizes current time via CTS and updates display wake state.
// ----------------------------------------------------------------------------
void notifyCTSTime() {
    if (bleState.isFirstPairing) {
        logMessage("Initial pairing detected: Applying default settings.", "Initial Pairing");
        bleState.isFirstPairing = false;
    }
    sendSettingsCommand();

    delay(50);

    if (pCtsCharacteristic != nullptr) {
        uint8_t currentTime[10];
        if (!createCtsTimeData(currentTime)) {
            logMessage(" -> Failed: Unable to obtain NTP time.", " -> Failed to obtain time");
            return;
        }

        pCtsCharacteristic->setValue(currentTime, 10);
        pCtsCharacteristic->notify();
        logMessage(" -> Display turned on & NTP time sync sent.", " -> Display On & Time Synced");
    }
}

// ----------------------------------------------------------------------------
// Constructs text payload buffer with optional leading and trailing icons.
// ----------------------------------------------------------------------------
std::vector<uint8_t> buildTextPayloadWithIcon(int mod, const String& message, int leadingIcon, int trailingIcon) {
    std::vector<uint8_t> contentBuffer;
    contentBuffer.reserve(message.length() + 8);
    
    // Append leading icon (Tag 24)
    if (leadingIcon > 0) {
        contentBuffer.push_back(PB_TAG(24, PbWire::BYTES));
        contentBuffer.push_back((uint8_t)leadingIcon);
    }
    
    // Append text message body
    if (message.length() > 0) {
        const uint8_t* msgBytes = (const uint8_t*)message.c_str();
        contentBuffer.insert(contentBuffer.end(), msgBytes, msgBytes + message.length());
    }

    // Append trailing icon (Tag 24)
    if (trailingIcon > 0) {
        contentBuffer.push_back(PB_TAG(24, PbWire::BYTES));
        contentBuffer.push_back((uint8_t)trailingIcon);
    }

    // Wrap into text payload structure (Tag 1: Display Mode, Tag 2: Content Buffer)
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
    std::vector<uint8_t> packet(Prefix::NOTICE, Prefix::NOTICE + sizeof(Prefix::NOTICE));
    packet.reserve(32 + message.length());
    
    // Append header fields (Tag 1: Animation, Tag 2: Sound, Tag 3: Color)
    packet.insert(packet.end(), {
        PB_TAG(1, PbWire::VARINT), (uint8_t)anim,
        PB_TAG(2, PbWire::VARINT), (uint8_t)sound,
        PB_TAG(3, PbWire::VARINT), (uint8_t)color,
    });

    // Build text payload with leading icon
    std::vector<uint8_t> textPayload = buildTextPayloadWithIcon(mod, message, icon);

    // Append payload to outer notification packet (Tag 4)
    packet.push_back(PB_TAG(4, PbWire::BYTES));
    encodeVarintVector(packet, textPayload.size());
    packet.insert(packet.end(), textPayload.begin(), textPayload.end());
    
    if (sendGlanceCommand(packet.data(), packet.size())) {
        logMessage(" -> Message sent successfully: '" + message + "' (Icon: " + String(icon) + ")", 
                   " -> Sent: " + message + " (Icon:" + String(icon) + ")");
    }
}

// ----------------------------------------------------------------------------
// Builds a ring LED chart display packet for the forecast scene.
// ----------------------------------------------------------------------------
std::vector<uint8_t> buildRingPacket(std::vector<int16_t> ringValues, uint8_t slot, uint32_t maxColor, uint32_t minColor, int16_t maxValue, int16_t minValue, const String& message) {
    std::vector<uint8_t> packet(Prefix::FORECAST_SCENE, Prefix::FORECAST_SCENE + sizeof(Prefix::FORECAST_SCENE));
    packet.push_back(slot);
    packet.reserve(64 + message.length());

    time_t now;
    time(&now);
    uint64_t timestamp = (uint64_t)now;
    
    // Append metadata (Tag 1: Timestamp, Tag 4-7: Max/Min Colors & Values)
    packet.insert(packet.end(), { PB_TAG(1, PbWire::VARINT) });
    encodeVarintVector(packet, timestamp);
    
    packet.insert(packet.end(), { PB_TAG(4, PbWire::VARINT) });
    encodeVarintVector(packet, maxColor);
    
    packet.insert(packet.end(), { PB_TAG(5, PbWire::VARINT) });
    encodeVarintVector(packet, minColor);

    packet.insert(packet.end(), { PB_TAG(6, PbWire::VARINT) });
    encodeVarintVector(packet, maxValue);
    
    packet.insert(packet.end(), { PB_TAG(7, PbWire::VARINT) });
    encodeVarintVector(packet, minValue);

    // Build 24-slot ring LED value array (16-bit little-endian per slot)
    std::vector<uint8_t> ringDataBytes;
    ringDataBytes.reserve(48);
    
    for (int i = 0; i < 24; i++) {
        int16_t val = (i < ringValues.size()) ? ringValues[i] : (ringValues.empty() ? 0 : ringValues.back());
        ringDataBytes.push_back(val & 0xFF);
        ringDataBytes.push_back((val >> 8) & 0xFF);
    }
    
    // Append ring LED data bytes (Tag 8)
    packet.insert(packet.end(), { PB_TAG(8, PbWire::BYTES) });
    encodeVarintVector(packet, ringDataBytes.size());
    packet.insert(packet.end(), ringDataBytes.begin(), ringDataBytes.end());

    // Append raw caption message bytes (Tag 9)
    packet.insert(packet.end(), { PB_TAG(9, PbWire::BYTES) });
    encodeVarintVector(packet, message.length());
    const uint8_t* msgBytes = (const uint8_t*)message.c_str();
    packet.insert(packet.end(), msgBytes, msgBytes + message.length());
    
    return packet;
}

// ----------------------------------------------------------------------------
// Builds a countdown timer configuration packet.
// ----------------------------------------------------------------------------
std::vector<uint8_t> buildTimerPacket(int duration, const String& label, const String& message) {
    std::vector<uint8_t> packet(Prefix::TIMER, Prefix::TIMER + sizeof(Prefix::TIMER));
    packet.reserve(64 + label.length() + message.length());

    std::vector<uint8_t> intervalParts;
    intervalParts.reserve(32 + label.length());

    // Append timer label payload (Tag 1)
    if (label.length() > 0) {
        int mod = 0x01;
        int icon = 0x00;
        std::vector<uint8_t> textData = buildTextPayloadWithIcon(mod, label, icon);

        intervalParts.push_back(PB_TAG(1, PbWire::BYTES));
        encodeVarintVector(intervalParts, textData.size());
        intervalParts.insert(intervalParts.end(), textData.begin(), textData.end());
    }

    // Append timer duration (Tag 2)
    intervalParts.push_back(PB_TAG(2, PbWire::VARINT));
    encodeVarintVector(intervalParts, duration);

    // Append interval configuration payload (Tag 2)
    packet.push_back(PB_TAG(2, PbWire::BYTES));
    encodeVarintVector(packet, intervalParts.size());
    packet.insert(packet.end(), intervalParts.begin(), intervalParts.end());

    // Append completion text payload (Tag 3)
    if (message.length() > 0) {
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
    std::vector<uint8_t> packet(Prefix::ALARM, Prefix::ALARM + sizeof(Prefix::ALARM));
    packet.reserve(64 + message.length());

    std::vector<uint8_t> alarmData;
    alarmData.reserve(32 + message.length());

    // Append alarm state and recurring days bitmask (Tag 2: Enabled, Tag 3: Days)
    alarmData.insert(alarmData.end(), {PB_TAG(2, PbWire::VARINT), 0x01});
    alarmData.insert(alarmData.end(), {PB_TAG(3, PbWire::VARINT), days});

    // Build alarm time payload (Tag 1: Hour, Tag 2: Minutes)
    std::vector<uint8_t> timePayload;
    timePayload.reserve(8);
    timePayload.insert(timePayload.end(), {PB_TAG(1, PbWire::VARINT), hour});
    timePayload.insert(timePayload.end(), {PB_TAG(2, PbWire::VARINT), minutes});

    // Append time payload to alarm data (Tag 4)
    alarmData.push_back(PB_TAG(4, PbWire::BYTES));
    encodeVarintVector(alarmData, timePayload.size());
    alarmData.insert(alarmData.end(), timePayload.begin(), timePayload.end());

    // Append alarm sound ID (Tag 5)
    alarmData.insert(alarmData.end(), {PB_TAG(5, PbWire::VARINT), sound});

    // Append alarm custom text message (Tag 6)
    if (message.length() > 0) {
        int mod = 0x01;
        int icon = 0x00;
        std::vector<uint8_t> textPayload = buildTextPayloadWithIcon(mod, message, icon);

        alarmData.push_back(PB_TAG(6, PbWire::BYTES));
        encodeVarintVector(alarmData, textPayload.size());
        alarmData.insert(alarmData.end(), textPayload.begin(), textPayload.end());
    }

    // Append complete alarm configuration payload to outer packet (Tag 1)
    packet.push_back(PB_TAG(1, PbWire::BYTES));
    encodeVarintVector(packet, alarmData.size());
    packet.insert(packet.end(), alarmData.begin(), alarmData.end());

    return packet;
}
