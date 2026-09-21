#include "ble_manager.h"
#include "utils.h"
#include "packet_builder.h"

// Global definitions for async PIN entry tracking
bool isWaitingForPin = false;
unsigned long pinRequestStartTime = 0;
esp_bd_addr_t pinRequestBda;

// ----------------------------------------------------------------------------
// Checks if the target device is already bonded in NVS storage.
// ----------------------------------------------------------------------------
bool isAlreadyBonded() {
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num <= 0) return false;

    std::vector<esp_ble_bond_dev_t> dev_list(dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list.data());

    uint8_t* targetNative = (uint8_t*)targetServerAddress.getNative();
    for (int i = 0; i < dev_num; i++) {
        if (memcmp(dev_list[i].bd_addr, targetNative, 6) == 0) {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// Clears all stored BLE bonding information from the device.
// ----------------------------------------------------------------------------
void clearBondInformation() {
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num <= 0) {
        logMessage("[BtnA] No bonding info to clear.", "[BtnA] No bonding info to clear.");
        return;
    }

    std::vector<esp_ble_bond_dev_t> dev_list(dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list.data());

    for (int i = 0; i < dev_num; i++) {
        esp_ble_remove_bond_device(dev_list[i].bd_addr);
    }
    
    logMessage("[BtnA] Cleared bonding info.", "[BtnA] Cleared bonding info.");
}

// ----------------------------------------------------------------------------
// Processes raw notification data received from characteristic FC78.
// ----------------------------------------------------------------------------
void notifyCallbackFC78(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    Serial.print("[fc78] Rx: ");
    for (size_t i = 0; i < length; i++) {
        Serial.printf("%02X ", pData[i]);
    }
    Serial.println();
}

// ----------------------------------------------------------------------------
// Security callbacks for handling PassKey entry and authentication events.
// ----------------------------------------------------------------------------
class MySecurityCallbacks : public BLESecurityCallbacks {
    uint32_t onPassKeyRequest() override {
        Serial.println("\n[SECURITY] Enter 6-digit PIN code (via Web/Serial):");
        Serial.println("  Web: http://<IP>/pin?code=XXXXXX or Serial input");
        M5.Display.println("\n Enter PIN via Web/Serial (20s):");

        hasWebPin = false;
        webPinCode = "";
        isWaitingForPin = true;
        
        uint32_t passKey = 0;
        unsigned long startTime = millis();

        // Wait up to 20 seconds for PIN input without blocking main loop WebServer execution
        while (millis() - startTime < 20000) {
            if (hasWebPin) {
                passKey = webPinCode.toInt();
                hasWebPin = false;
                isWaitingForPin = false;
                Serial.printf(" -> PIN code received via Web: %06d\n", passKey);
                return passKey;
            }

            if (Serial.available() > 0) {
                String inputStr = Serial.readStringUntil('\n');
                inputStr.trim();
                if (inputStr.length() > 0) {
                    passKey = inputStr.toInt();
                    isWaitingForPin = false;
                    Serial.printf(" -> PIN code received via Serial: %06d\n", passKey);
                    return passKey;
                }
            }

            delay(50); // Yield control to prevent Watchdog timeout and allow main loop execution
        }

        Serial.println(" -> PIN entry timed out.");
        M5.Display.println(" -> PIN timeout.");
        isWaitingForPin = false;
        return 0;
    }

    void onPassKeyNotify(uint32_t pass_key) override {}
    
    bool onConfirmPIN(uint32_t pin) override { 
        return true; 
    }
    
    bool onSecurityRequest() override { 
        return true; 
    }

    void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
        isWaitingForPin = false;
        if (cmpl.success) {
            logMessage(" -> Pairing/Encryption success!", " -> Pairing/Encryption success!");
            bleState.isAuthReady = true;
            bleState.needCtsSync = true;
        } else {
            logMessage(" -> Pairing failed (Reason: 0x" + String(cmpl.fail_reason, HEX) + ")", 
                       " -> Pairing failed (0x" + String(cmpl.fail_reason, HEX) + ")");
            bleState.isAuthReady = false;
        }
    }
};
static MySecurityCallbacks securityCallbacks;

// ----------------------------------------------------------------------------
// Handles read requests for the local Current Time Service (CTS).
// ----------------------------------------------------------------------------
class CTSCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic* pCharacteristic) override {
        uint8_t currentTime[10];
        if (!createCtsTimeData(currentTime)) {
            Serial.println(" -> Failed to get NTP time on Read.");
            return;
        }
        
        pCharacteristic->setValue(currentTime, 10);
        Serial.println(" -> Returned CTS time data to clock (Read).");
    }
};

// ----------------------------------------------------------------------------
// Manages BLE client connection status and security encryption requests.
// ----------------------------------------------------------------------------
class MyClientCallbacks : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) override {
        bleState.connected = true;
        bleState.isAuthReady = false;
        bleState.needCtsSync = false;
        logMessage("Connected to Glance Clock.", "Connected to Glance Clock.");
        updateScreenStatus();
        
        uint8_t* peerAddr = (uint8_t*)pclient->getPeerAddress().getNative();
        if (isAlreadyBonded()) {
            Serial.println("Already bonded. Requesting encryption...");
            bleState.isFirstPairing = false;
            esp_ble_set_encryption(peerAddr, ESP_BLE_SEC_ENCRYPT);
        } else {
            Serial.println("New pairing required. Requesting MITM encryption...");
            bleState.isFirstPairing = true;
            esp_ble_set_encryption(peerAddr, ESP_BLE_SEC_ENCRYPT_MITM);
        }
    }

    void onDisconnect(BLEClient* pclient) override {
        bleState.connected = false;
        bleState.isAuthReady = false;
        bleState.needCtsSync = false;
        pRemoteChar = nullptr;
        pNotifyChar = nullptr;
        isWaitingForPin = false;
        logMessage("Disconnected. Rescan in 5s.", "Disconnected. Rescan in 5s.");
        updateScreenStatus();
        
        bleState.doScan = true;
        bleState.disconnectTime = millis();
    }
};

// ----------------------------------------------------------------------------
// Processes BLE advertising devices during scanning to locate target device.
// ----------------------------------------------------------------------------
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        if (hasTargetAddress) {
            if (advertisedDevice.getAddress().equals(targetServerAddress)) {
                BLEDevice::getScan()->stop();
                bleState.doConnect = true;
                logMessage("Found saved target Glance Clock.", "Found Saved Glance Clock.");
            }
            return;
        }

        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(GLANCE_SERVICE_UUID))) {
            BLEDevice::getScan()->stop();
            targetServerAddress = advertisedDevice.getAddress();
            hasTargetAddress = true;
            bleState.doConnect = true;

            preferences.begin("ble-config", false);
            preferences.putString("targetAddr", targetServerAddress.toString().c_str());
            preferences.end();

            logMessage("Initial scan success: Found target and saved address [" + String(targetServerAddress.toString().c_str()) + "]", 
                       "Found & Saved: " + String(targetServerAddress.toString().c_str()));
        }
    }
};

// ----------------------------------------------------------------------------
// Initializes BLE stack, security settings, local CTS server, and client scan.
// ----------------------------------------------------------------------------
void setupBLE() {
    BLEDevice::init("M5Stack-Bridge");
    BLEDevice::setMTU(512);

    BLESecurity security;
    security.setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
    security.setCapability(ESP_IO_CAP_KBDISP);
    security.setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    security.setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    BLEDevice::setSecurityCallbacks(&securityCallbacks);

    pServer = BLEDevice::createServer();
    BLEService* pCtsService = pServer->createService("1805");
    pCtsCharacteristic = pCtsService->createCharacteristic(
        "2a2b",
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pCtsCharacteristic->setCallbacks(new CTSCallbacks());
    pCtsCharacteristic->addDescriptor(new BLE2902());
    pCtsService->start();

    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallbacks());

    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);

    if (hasTargetAddress) {
        bleState.doConnect = true;
    } else {
        pBLEScan->start(10, false);
    }
}

// ----------------------------------------------------------------------------
// Manages BLE state loop including service discovery, connection, and rescanning.
// ----------------------------------------------------------------------------
void loopBLE() {
    if (bleState.connected && bleState.isAuthReady && bleState.needCtsSync) {
        bleState.needCtsSync = false;
        delay(500);

        BLERemoteService* pRemoteService = pClient->getService(GLANCE_SERVICE_UUID);
        if (pRemoteService != nullptr) {
            pNotifyChar = pRemoteService->getCharacteristic(GLANCE_CHAR_NOTIFY_UUID);
            if (pNotifyChar != nullptr && pNotifyChar->canNotify()) {
                pNotifyChar->registerForNotify(notifyCallbackFC78);
                Serial.println("Registered for Notify (fc78) successfully.");
            }

            pRemoteChar = pRemoteService->getCharacteristic(GLANCE_CHAR_UUID);
            if (pRemoteChar != nullptr) {
                Serial.println("Successfully obtained communication characteristic.");
            }
        } else {
            Serial.println("Error: Glance service not found.");
        }

        if (pRemoteChar != nullptr) {
            notifyCTSTime();
        }
    }

    if (bleState.doConnect) {
        bleState.doConnect = false;

        if (!pClient->connect(targetServerAddress, BLE_ADDR_TYPE_RANDOM)) {
            Serial.println("Connection failed. Rescanning in 5 seconds.");
            bleState.doScan = true;
            bleState.disconnectTime = millis();
        }
        updateScreenStatus();
    }

    if (bleState.doScan && !bleState.connected && (millis() - bleState.disconnectTime >= 5000)) {
        bleState.doScan = false;
        Serial.println("Rescanning...");
        BLEDevice::getScan()->start(5, false);
        updateScreenStatus();
    }
}
