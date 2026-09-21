#pragma once
#include "config.h"

// ----------------------------------------------------------------------------
// Captive Portal and Wi-Fi Provisioning Functions
// ----------------------------------------------------------------------------

// Starts Access Point mode and HTTP captive portal for Wi-Fi setup.
void startCaptivePortal();

// Loads stored credentials and connects to Wi-Fi, falling back to Captive Portal.
void initWiFi();
