#include "ble_manager.h"
#include "utils.h"
#include "packet_builder.h"

// ----------------------------------------------------------------------------
// Checks if the target device is already bonded in NVS storage.
// ----------------------------------------------------------------------------
bool isAlreadyBonded() {
    // Retrieve total number of bonded BLE devices stored in NVS
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num <= 0) return false;

    // Fetch list of bonded devices from ESP BLE stack
    std::vector<esp_ble_bond_dev_t> dev_list(dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list.data());

    // Obtain target device BLE address under mutex protection
    xSemaphoreTake(systemMutex, portMAX_DELAY);
    uint8_t* targetNative = (uint8_t*)targetServerAddress.getNative();
    esp_bd_addr_t targetBda;
    memcpy(targetBda, targetNative, 6);
    xSemaphoreGive(systemMutex);

    // Compare bonded device addresses against target address
    for (int i = 0; i < dev_num; i++) {
        if (memcmp(dev_list[i].bd_addr, targetBda, 6) == 0) {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// Clears all stored BLE bonding information from the device.
// ----------------------------------------------------------------------------
void clearBondInformation() {
    // Retrieve number of currently bonded devices
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num <= 0) {
        logMessage("[BtnA] No bonding info to clear.", "[BtnA] No bonding info to clear.");
        return;
    }

    // Populate device list and iterate through each to remove bonding records
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
    // If Serial monitor is active, format and print incoming notification payload bytes as a hex string
    if (Serial) {
        String hexStr = "[fc78] Rx: ";
        for (size_t i = 0; i < length; i++) {
            char buf[4];
            snprintf(buf, sizeof(buf), "%02X ", pData[i]);
            hexStr += buf;
        }
        Serial.println(hexStr);
    }

    // Retrieve authentication status, remote characteristic, and notification state under mutex protection
    xSemaphoreTake(systemMutex, portMAX_DELAY);
    bool authReady = bleState.isAuthReady;
    BLERemoteCharacteristic* remoteChar = pRemoteChar;
    bool isFirst = bleState.isFirstPairing || !bleState.hasReceivedNotification;
    bleState.hasReceivedNotification = true;
    xSemaphoreGive(systemMutex);

    if (length > 0 && authReady && remoteChar != nullptr) {
#ifdef IGNORE_PERIODIC_FC78
        // Only trigger CTS sync on initial pairing / first notification after connection
        if (isFirst) {
            xSemaphoreTake(systemMutex, portMAX_DELAY);
            bleState.needCtsSync = true;
            xSemaphoreGive(systemMutex);
        }
#else
        // Trigger CTS sync on every incoming fc78 notification
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        bleState.needCtsSync = true;
        xSemaphoreGive(systemMutex);
#endif
    }
}

// ----------------------------------------------------------------------------
// Security callbacks for handling PassKey entry and authentication events.
// ----------------------------------------------------------------------------
class MySecurityCallbacks : public BLESecurityCallbacks {
    uint32_t onPassKeyRequest() override {
        logMessage("\n[SECURITY] Enter 6-digit PIN code (via Web/Serial):", "Enter PIN (20s)");
        logMessage(" Web: http://<IP>/pin?code=XXXXXX or Serial input");

        // Set state variables indicating waiting for PIN input under mutex protection
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        hasWebPin = false;
        webPinCode = "";
        isWaitingForPin = true;
        xSemaphoreGive(systemMutex);
        
        uint32_t passKey = 0;
        unsigned long startTime = millis();

        // Wait up to 20 seconds for PIN input without blocking main loop WebServer execution
        while (millis() - startTime < 20000) {
            bool pinReady = false;
            String pinStr = "";
            
            // Check if web interface has supplied a PIN code
            xSemaphoreTake(systemMutex, portMAX_DELAY);
            if (hasWebPin) {
                pinReady = true;
                pinStr = webPinCode;
                hasWebPin = false;
                isWaitingForPin = false;
            }
            xSemaphoreGive(systemMutex);

            if (pinReady) {
                passKey = pinStr.toInt();
                logMessage(" -> PIN code received via Web: " + String(passKey));
                return passKey;
            }

            // Check if a PIN code has been entered via Serial input
            if (Serial && Serial.available() > 0) {
                String inputStr = Serial.readStringUntil('\n');
                inputStr.trim();
                if (inputStr.length() > 0) {
                    passKey = inputStr.toInt();
                    xSemaphoreTake(systemMutex, portMAX_DELAY);
                    isWaitingForPin = false;
                    xSemaphoreGive(systemMutex);
                    logMessage(" -> PIN code received via Serial: " + String(passKey));
                    return passKey;
                }
            }

            vTaskDelay(pdMS_TO_TICKS(50)); // Yield control safely
        }

        // Handle PIN entry timeout
        logMessage(" -> PIN entry timed out.", " -> PIN timeout.");
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        isWaitingForPin = false;
        xSemaphoreGive(systemMutex);
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
        // Update authentication state flags upon completion event
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        isWaitingForPin = false;
        if (cmpl.success) {
            bleState.isAuthReady = true;
            xSemaphoreGive(systemMutex);
            logMessage(" -> Pairing/Encryption success!", " -> Pairing/Encryption success!");
        } else {
            bleState.isAuthReady = false;
            xSemaphoreGive(systemMutex);
            logMessage(" -> Pairing failed (Reason: 0x" + String(cmpl.fail_reason, HEX) + ")", 
                       " -> Pairing failed (0x" + String(cmpl.fail_reason, HEX) + ")");
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
        // Create current NTP time data and return it to the requesting peer on read
        if (!createCtsTimeData(currentTime)) {
            logMessage(" -> Failed to get NTP time on Read.");
            return;
        }
        
        pCharacteristic->setValue(currentTime, 10);
        logMessage(" -> Returned CTS time data to clock (Read).");
    }
};

// ----------------------------------------------------------------------------
// Manages BLE client connection status and security encryption requests.
// ----------------------------------------------------------------------------
class MyClientCallbacks : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) override {
        // Check if device is already bonded
        bool bonded = isAlreadyBonded();

        // Update connection state flags upon successful connection
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        bleState.connected = true;
        bleState.isAuthReady = bonded; // Set auth ready directly if already bonded
        bleState.needCtsSync = false;
        bleState.hasReceivedNotification = false;
        xSemaphoreGive(systemMutex);
        
        logMessage("Connected to Glance Clock.", "Connected to Glance Clock.");
        updateScreenStatus();
        
        uint8_t* peerAddr = (uint8_t*)pclient->getPeerAddress().getNative();
        // If already bonded, request standard encryption; otherwise request MITM encrypted pairing
        if (bonded) {
            logMessage("Already bonded. Requesting encryption...");
            xSemaphoreTake(systemMutex, portMAX_DELAY);
            bleState.isFirstPairing = false;
            xSemaphoreGive(systemMutex);
            esp_ble_set_encryption(peerAddr, ESP_BLE_SEC_ENCRYPT);
        } else {
            logMessage("New pairing required. Requesting MITM encryption...");
            xSemaphoreTake(systemMutex, portMAX_DELAY);
            bleState.isFirstPairing = true;
            xSemaphoreGive(systemMutex);
            esp_ble_set_encryption(peerAddr, ESP_BLE_SEC_ENCRYPT_MITM);
        }
    }

    void onDisconnect(BLEClient* pclient) override {
        // Reset connection and characteristic pointers and schedule reconnection scan upon disconnection
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        bleState.connected = false;
        bleState.isAuthReady = false;
        bleState.needCtsSync = false;
        bleState.hasReceivedNotification = false;
        isWaitingForPin = false;
        bleState.doScan = true;
        bleState.disconnectTime = millis();
        pRemoteChar = nullptr;
        pNotifyChar = nullptr;
        xSemaphoreGive(systemMutex);
        
        logMessage("Disconnected. Rescan in 5s.", "Disconnected. Rescan in 5s.");
        updateScreenStatus();
    }
};

// ----------------------------------------------------------------------------
// Processes BLE advertising devices during scanning to locate target device.
// ----------------------------------------------------------------------------
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        bool hasAddr = hasTargetAddress;
        BLEAddress targetAddr = targetServerAddress;
        xSemaphoreGive(systemMutex);

        // If a target address is already saved, check if the advertised device matches it
        if (hasAddr) {
            if (advertisedDevice.getAddress().equals(targetAddr)) {
                BLEDevice::getScan()->stop();
                xSemaphoreTake(systemMutex, portMAX_DELAY);
                bleState.doConnect = true;
                xSemaphoreGive(systemMutex);
                logMessage("Found saved target Glance Clock.", "Found Saved Glance Clock.");
            }
            return;
        }

        // If no address is saved, scan for the Glance service UUID to identify the clock
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(GLANCE_SERVICE_UUID))) {
            BLEDevice::getScan()->stop();
            
            xSemaphoreTake(systemMutex, portMAX_DELAY);
            targetServerAddress = advertisedDevice.getAddress();
            hasTargetAddress = true;
            bleState.doConnect = true;
            BLEAddress newAddr = targetServerAddress;
            xSemaphoreGive(systemMutex);

            // Save the discovered device address to Preferences NVS storage
            preferences.begin("ble-config", false);
            preferences.putString("targetAddr", newAddr.toString().c_str());
            preferences.end();

            logMessage("Initial scan success: Found target and saved address [" + String(newAddr.toString().c_str()) + "]", 
                       "Found & Saved: " + String(newAddr.toString().c_str()));
        }
    }
};

// ----------------------------------------------------------------------------
// Scan complete callback handler for non-blocking asynchronous scans.
// ----------------------------------------------------------------------------
void scanCompleteCB(BLEScanResults foundDevices) {
    xSemaphoreTake(systemMutex, portMAX_DELAY);
    bool hasAddr = hasTargetAddress;
    bool isConnected = bleState.connected;
    xSemaphoreGive(systemMutex);

    // If target is not found and not connected after scan completion, schedule a rescan retry
    if (!hasAddr && !isConnected) {
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        bleState.doScan = true;
        bleState.disconnectTime = millis();
        xSemaphoreGive(systemMutex);
        logMessage("Scan completed. Target not found, retrying...");
    }
}

// ----------------------------------------------------------------------------
// Initializes BLE stack, security settings, local CTS server, and client scan.
// ----------------------------------------------------------------------------
void setupBLE() {
    BLEDevice::init("M5Stack-Bridge");
    BLEDevice::setMTU(512);

    // Configure BLE security parameters and authentication requirements
    BLESecurity security;
    security.setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
    // CAUTION: Never change the BLE security IO Capability from ESP_IO_CAP_KBDISP.
    security.setCapability(ESP_IO_CAP_KBDISP);
    security.setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    security.setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    BLEDevice::setSecurityCallbacks(&securityCallbacks);

    // Create local BLE server and Current Time Service (CTS) characteristic
    pServer = BLEDevice::createServer();
    BLEService* pCtsService = pServer->createService("1805");
    pCtsCharacteristic = pCtsService->createCharacteristic(
        "2a2b",
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pCtsCharacteristic->setCallbacks(new CTSCallbacks());
    pCtsCharacteristic->addDescriptor(new BLE2902());
    pCtsService->start();

    // Create BLE client instance and register client callbacks
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallbacks());

    // Configure BLE scanner and advertised device callbacks
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);

    xSemaphoreTake(systemMutex, portMAX_DELAY);
    bool hasAddr = hasTargetAddress;
    xSemaphoreGive(systemMutex);

    // If target address is already known, trigger connection; otherwise start asynchronous scan
    if (hasAddr) {
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        bleState.doConnect = true;
        xSemaphoreGive(systemMutex);
    } else {
        logMessage("Starting initial asynchronous BLE scan...");
        pBLEScan->start(5, scanCompleteCB, false);
    }
}

// ----------------------------------------------------------------------------
// Manages BLE state loop including service discovery, connection, and rescanning.
// ----------------------------------------------------------------------------
void loopBLE() {
    xSemaphoreTake(systemMutex, portMAX_DELAY);
    bool connected = bleState.connected;
    bool authReady = bleState.isAuthReady;
    bool doConnect = bleState.doConnect;
    bool doScan = bleState.doScan;
    unsigned long discTime = bleState.disconnectTime;
    BLEAddress targetAddr = targetServerAddress;
    BLERemoteCharacteristic* currentRemoteCharCheck = pRemoteChar;
    xSemaphoreGive(systemMutex);

    // When connected and authenticated, discover remote services and characteristics if not yet discovered
    if (connected && authReady && currentRemoteCharCheck == nullptr) {
        vTaskDelay(pdMS_TO_TICKS(500));

        BLERemoteService* pRemoteService = pClient->getService(GLANCE_SERVICE_UUID);
        if (pRemoteService != nullptr) {
            // Obtain main communication remote characteristic first
            BLERemoteCharacteristic* tempRemoteChar = pRemoteService->getCharacteristic(GLANCE_CHAR_UUID);
            if (tempRemoteChar != nullptr) {
                xSemaphoreTake(systemMutex, portMAX_DELAY);
                pRemoteChar = tempRemoteChar;
                xSemaphoreGive(systemMutex);
                
                logMessage("Successfully obtained communication characteristic.");
            }

            // Register for notifications on the notification characteristic (fc78) after pRemoteChar is set
            BLERemoteCharacteristic* tempNotifyChar = pRemoteService->getCharacteristic(GLANCE_CHAR_NOTIFY_UUID);
            if (tempNotifyChar != nullptr && tempNotifyChar->canNotify()) {
                tempNotifyChar->registerForNotify(notifyCallbackFC78);
                
                xSemaphoreTake(systemMutex, portMAX_DELAY);
                pNotifyChar = tempNotifyChar;
                xSemaphoreGive(systemMutex);
                
                logMessage("Registered for Notify (fc78) successfully.");
            }
        } else {
            logMessage("Error: Glance service not found.");
        }
    }

    // Trigger time sync and settings automatically once notification response is received from clock
    xSemaphoreTake(systemMutex, portMAX_DELAY);
    bool currentNeedSync = bleState.needCtsSync;
    BLERemoteCharacteristic* currentRemoteChar = pRemoteChar;
    bool currentAuthReady = bleState.isAuthReady;
    xSemaphoreGive(systemMutex);

    if (connected && currentNeedSync && currentRemoteChar != nullptr && currentAuthReady) {
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        bleState.needCtsSync = false;
        xSemaphoreGive(systemMutex);
        
        vTaskDelay(pdMS_TO_TICKS(100));

        // CAUTION: It is necessary to send the time after notification response is received.
        notifyCTSTime();
    }

    // Handle connection request execution
    if (doConnect) {
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        bleState.doConnect = false;
        xSemaphoreGive(systemMutex);

        vTaskDelay(pdMS_TO_TICKS(50));

        // Attempt connection using random address type; if failed, schedule rescan retry
        // CAUTION: Never change the BLE address type from BLE_ADDR_TYPE_RANDOM.
        if (!pClient->connect(targetAddr, BLE_ADDR_TYPE_RANDOM)) {
            logMessage("Connection failed. Rescanning in 5 seconds.");
            xSemaphoreTake(systemMutex, portMAX_DELAY);
            bleState.doScan = true;
            bleState.disconnectTime = millis();
            xSemaphoreGive(systemMutex);
        }
        updateScreenStatus();
    }

    // Handle periodic rescanning after disconnect timeout delay
    if (doScan && !connected && (millis() - discTime >= 5000)) {
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        bleState.doScan = false;
        xSemaphoreGive(systemMutex);
        
        logMessage("Rescanning...");
        BLEDevice::getScan()->start(5, scanCompleteCB, false);
        updateScreenStatus();
    }
}
