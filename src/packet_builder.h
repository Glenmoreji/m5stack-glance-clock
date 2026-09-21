#pragma once
#include "config.h"

// ----------------------------------------------------------------------------
// Protobuf and Packet Encoding / Transmission Functions
// ----------------------------------------------------------------------------

// Encodes a 64-bit integer into Protobuf Varint format byte array.
size_t encodeVarint(uint8_t* buf, uint64_t value);

// Encodes a 64-bit integer into Protobuf Varint format vector.
void encodeVarintVector(std::vector<uint8_t>& buffer, uint64_t value);

// Sends a raw command packet to Glance Clock via BLE.
bool sendGlanceCommand(const uint8_t* cmdData, size_t length);

// Constructs current local time data formatted for Current Time Service (CTS).
bool createCtsTimeData(uint8_t* currentTime);

// Generates and transmits device configuration settings packet.
void sendSettingsCommand();

// Synchronizes current time via CTS and updates display wake state.
void notifyCTSTime();

// Sends a custom notification message command.
void sendCustomNoticeCommand(const String& message, int anim, int sound, int color, int mod, int icon);

// Constructs text payload buffer with optional leading and trailing icons.
std::vector<uint8_t> buildTextPayloadWithIcon(int mod, const String& message, int leadingIcon = 0, int trailingIcon = 0);

// Builds a ring LED chart display packet for the forecast scene.
std::vector<uint8_t> buildRingPacket(std::vector<int16_t> ringValues, uint8_t slot, uint32_t maxColor, uint32_t minColor, int16_t maxValue, int16_t minValue, const String& message);

// Builds a countdown timer configuration packet.
std::vector<uint8_t> buildTimerPacket(int duration, const String& label, const String& message);

// Builds an alarm configuration packet.
std::vector<uint8_t> buildAlarmPacket(uint8_t days, uint8_t hour, uint8_t minutes, uint8_t sound, const String& message);
