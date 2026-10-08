#pragma once
#include "config.h"

// ----------------------------------------------------------------------------
// BLE Manager Functions and Callbacks Declarations
// ----------------------------------------------------------------------------

// Function prototype declarations for bond checks, clearing bonds, BLE setup, and loop execution
bool isAlreadyBonded();
void clearBondInformation();
void setupBLE();
void loopBLE();
