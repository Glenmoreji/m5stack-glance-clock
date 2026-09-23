#pragma once
#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <time.h>
#include <vector>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <BLE2902.h>
#include "esp_gap_ble_api.h"
#include <stdarg.h>

// ----------------------------------------------------------------------------
// Global Variable Extern Declarations
// ----------------------------------------------------------------------------
extern Preferences preferences;
extern String ssid;
extern String password;
extern const char* apSSID;
extern DNSServer dnsServer;
extern WebServer server;
extern const char* ntpServer;
extern const long gmtOffset_sec;
extern const int daylightOffset_sec;

extern BLEClient* pClient;
extern BLERemoteCharacteristic* pRemoteChar;
extern BLERemoteCharacteristic* pNotifyChar;
extern BLEServer* pServer;
extern BLECharacteristic* pCtsCharacteristic;

extern BLEAddress targetServerAddress;
extern bool hasTargetAddress;
extern String webPinCode;
extern bool hasWebPin;

// Async PIN entry state tracking variables
extern bool isWaitingForPin;
extern unsigned long pinRequestStartTime;
extern esp_bd_addr_t pinRequestBda;

// ----------------------------------------------------------------------------
// BLE Service and Characteristic UUID Constants
// ----------------------------------------------------------------------------
constexpr const char* GLANCE_SERVICE_UUID = "5075f606-1e0e-11e7-93ae-92361f002671";
constexpr const char* GLANCE_CHAR_UUID = "5075fb2e-1e0e-11e7-93ae-92361f002671";
constexpr const char* GLANCE_CHAR_NOTIFY_UUID = "5075fc78-1e0e-11e7-93ae-92361f002671";

// ----------------------------------------------------------------------------
// Glance Protocol Command and Scene Prefixes
// ----------------------------------------------------------------------------
namespace Prefix {
 constexpr uint8_t CUSTOM_SCENE[] = { 0x00, 0x00 };
 constexpr uint8_t NOTICE[] = { 0x02, 0x30, 0x00, 0x00 };
 constexpr uint8_t TIMER[] = { 0x03, 0x00, 0x00, 0x00 };
 constexpr uint8_t ALARM[] = { 0x04, 0x2F, 0x00, 0x00 };
 constexpr uint8_t SETTINGS[] = { 0x05, 0x00, 0x00, 0x00 };
 constexpr uint8_t CALL_SCENE[] = { 0x06, 0x53, 0x00, 0x67 };
 constexpr uint8_t FORECAST_SCENE[] = { 0x07, 0x10, 0x18 };
 constexpr uint8_t APPOINTMENTS_SCENE[] = { 0x08, 0x00, 0x08 };
}

// ----------------------------------------------------------------------------
// Glance Direct Single-Byte Command Constants (Sorted by Numeric Value)
// ----------------------------------------------------------------------------
constexpr uint8_t CMD_TIMER_STOP             = 0x0A; // 10
constexpr uint8_t CMD_ALARM_STOP             = 0x14; // 20
constexpr uint8_t CMD_ALARM_CLEAR            = 0x15; // 21
constexpr uint8_t CMD_SCENE_STOP             = 0x1E; // 30
constexpr uint8_t CMD_SCENE_START            = 0x1F; // 31
constexpr uint8_t CMD_SCENE_CLEAR            = 0x20; // 32
constexpr uint8_t CMD_UPDATE_REFRESH         = 0x23; // 35
constexpr uint8_t CMD_AUTO_NIGHT_MODE_EN     = 0x28; // 40
constexpr uint8_t CMD_AUTO_NIGHT_MODE_DIS    = 0x29; // 41
constexpr uint8_t CMD_CLEAR_BONDS            = 0x2A; // 42
constexpr uint8_t CMD_CALIBRATION_START      = 0x2B; // 43
constexpr uint8_t CMD_CALIBRATION_CONFIRM    = 0x2C; // 44
constexpr uint8_t CMD_ALARM_WITH_NOTES       = 0x2D; // 45
constexpr uint8_t CMD_CLEAR_USER_INFO        = 0x32; // 50
constexpr uint8_t CMD_BRIGHTNESS_SCENE_STOP  = 0x3C; // 60
constexpr uint8_t CMD_BRIGHTNESS_SCENE_START = 0x3D; // 61
constexpr uint8_t CMD_DSP_STATE_SHOW         = 0x46; // 70

// ----------------------------------------------------------------------------
// Protobuf Wire Encoding Helpers
// ----------------------------------------------------------------------------
namespace PbWire {
 constexpr uint8_t VARINT = 0;
 constexpr uint8_t BYTES = 2;
}

// Encodes field number and wire type into a single Protobuf tag byte.
constexpr uint8_t PB_TAG(uint8_t field, uint8_t wire) {
 return (field << 3) | wire;
}

// ----------------------------------------------------------------------------
// Enums and Configuration Data Structures
// ----------------------------------------------------------------------------

// Sound notification identifiers supported by Glance Clock.
enum class Sound : uint8_t {
 NoneSound = 0,
 Waves = 1,
 Rise = 2,
 Radar = 5
};

// Device settings configuration state structure.
struct DeviceSettings {
 bool nightMode = false;
 bool permanentDND = false;
 bool permanentMute = false;
 int dateFormat = 1;
 bool pointsEnabled = true;
 int brightness = 250;
 bool timeModeEnable = true;
 bool timeFormat12 = false;
};
extern DeviceSettings currentSettings;

// BLE runtime connection and handshake state tracking structure.
struct BleConnectionState {
 bool doConnect = false;
 bool connected = false;
 bool doScan = false;
 bool isAuthReady = false;
 bool needCtsSync = false;
 bool isFirstPairing = false;
 unsigned long disconnectTime = 0;
};
extern BleConnectionState bleState;
