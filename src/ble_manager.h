#pragma once
#include "config.h"

// ----------------------------------------------------------------------------
// BLE Bonding and Connection Management
// ----------------------------------------------------------------------------

// Checks if the target device is already bonded in NVS storage.
bool isAlreadyBonded();

// Clears all stored BLE bonding information from the device.
void clearBondInformation();

// Handles deferred PIN response processing via ESP-IDF native stack.
void handlePinResponse(uint32_t passKey);

// Initializes BLE stack, security settings, local CTS server, and client scanning.
void setupBLE();

// Manages BLE state loop including connection handling, discovery, and rescanning.
void loopBLE();
