#pragma once
#include "config.h"

// ----------------------------------------------------------------------------
// Protobuf and Packet Encoding / Transmission Functions
// ----------------------------------------------------------------------------

size_t encodeVarint(uint8_t* buf, uint64_t value);
void encodeVarintVector(std::vector<uint8_t>& buffer, uint64_t value);
bool sendGlanceCommand(const uint8_t* cmdData, size_t length);
bool createCtsTimeData(uint8_t* currentTime);
void sendSettingsCommand();
void notifyCTSTime();
void sendCustomNoticeCommand(const String& message, int anim, int sound, int color, int mod, int icon);
std::vector<uint8_t> buildTextPayloadWithIcon(int mod, const String& message, int leadingIcon = 0, int trailingIcon = 0);
std::vector<uint8_t> buildRingPacket(const std::vector<int16_t>& ringValues, uint8_t slot, uint32_t maxColor, uint32_t minColor, int16_t maxValue, int16_t minValue, const String& message);
std::vector<uint8_t> buildTimerPacket(int duration, const String& label, const String& message);
std::vector<uint8_t> buildAlarmPacket(uint8_t days, uint8_t hour, uint8_t minutes, uint8_t sound, const String& message);
