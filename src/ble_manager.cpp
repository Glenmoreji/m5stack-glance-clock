#include "ble_manager.h"
#include "utils.h"
#include "packet_builder.h"

// ----------------------------------------------------------------------------
// Handles read requests and subscription events for the local Current Time Service (CTS).
// ----------------------------------------------------------------------------
class CTSCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        logMessage("CTS READ OCCURRED");

        uint8_t currentTime[10];
        // Create current NTP time data and return it to the requesting peer on read
        if (!createCtsTimeData(currentTime)) {
            logMessage(" -> Failed to get NTP time on Read.");
            return;
        }
        
        pCharacteristic->setValue(currentTime, 10);
        logMessage(" -> Returned CTS time data to clock (Read).");
    }

    void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override {
        char subLog[64];
        snprintf(subLog, sizeof(subLog), "CTS SUBSCRIBED: subValue=0x%04X", subValue);
        logMessage(String(subLog));
    }
};

// ----------------------------------------------------------------------------
// Server connection callbacks to verify if the clock connects to the local CTS server.
// ----------------------------------------------------------------------------
class MyServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        logMessage("[CTS] Client connected");
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        logMessage("[CTS] Client disconnected");
    }
};
static MyServerCallbacks serverCallbacks;

// ----------------------------------------------------------------------------
// Checks if the target device is already bonded in NVS storage.
// ----------------------------------------------------------------------------
bool isAlreadyBonded() {
    int dev_num = NimBLEDevice::getNumBonds();
    if (dev_num <= 0) return false;

    // Obtain target device BLE address under mutex protection
    xSemaphoreTake(systemMutex, portMAX_DELAY);
    NimBLEAddress targetAddr = targetServerAddress;
    xSemaphoreGive(systemMutex);

    // Compare bonded device addresses against target address
    for (int i = 0; i < dev_num; i++) {
        NimBLEAddress bondedAddr = NimBLEDevice::getBondedAddress(i);
        if (bondedAddr == targetAddr) {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// Clears all stored BLE bonding information from the device.
// ----------------------------------------------------------------------------
void clearBondInformation() {
    int dev_num = NimBLEDevice::getNumBonds();
    if (dev_num <= 0) {
        logMessage("[BtnA] No bonding info to clear.", "[BtnA] No bonding info to clear.");
        return;
    }

    // Delete existing bond record from storage
    NimBLEDevice::deleteBond(NimBLEDevice::getBondedAddress(0));
    logMessage("[BtnA] Cleared bonding info.", "Bonds cleared.");
}

// ----------------------------------------------------------------------------
// Processes raw notification data received from characteristic FC78.
// ----------------------------------------------------------------------------
void notifyCallbackFC78(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
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
    NimBLERemoteCharacteristic* remoteChar = pRemoteChar;
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
// Manages BLE client connection status, security encryption, and PassKey entry.
// ----------------------------------------------------------------------------
class MyClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pclient) override {
        // Check if device is already bonded
        bool bonded = isAlreadyBonded();

        // Update connection state flags upon successful connection
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        bleState.connected = true;
        bleState.isAuthReady = false;
        bleState.needCtsSync = false;
        bleState.hasReceivedNotification = false;
        bleState.isFirstPairing = !bonded;
        bleState.doScan = false;
        xSemaphoreGive(systemMutex);
        
        logMessage("Connected to Glance Clock.", "Connected");
        updateScreenStatus();
    }

    void onDisconnect(NimBLEClient* pclient, int reason) override {
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
        
        logMessage("Disconnected. Rescan in 5s.", "Disconnected");
        updateScreenStatus();
    }

    void onPassKeyEntry(NimBLEConnInfo& connInfo) override {
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
                NimBLEDevice::injectPassKey(connInfo, passKey);
                return;
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
                    NimBLEDevice::injectPassKey(connInfo, passKey);
                    return;
                }
            }

            vTaskDelay(pdMS_TO_TICKS(50)); // Yield control safely
        }

        // Handle PIN entry timeout
        logMessage(" -> PIN entry timed out.", " -> PIN timeout.");
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        isWaitingForPin = false;
        xSemaphoreGive(systemMutex);
        NimBLEDevice::injectPassKey(connInfo, 0);
    }

    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
        // Update authentication state flags upon completion event
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        isWaitingForPin = false;
        if (connInfo.isEncrypted()) {
            bleState.isAuthReady = true;
            xSemaphoreGive(systemMutex);
            logMessage(" -> Pairing/Encryption success!", " -> Pairing/Encryption success!");
        } else {
            bleState.isAuthReady = false;
            xSemaphoreGive(systemMutex);
            logMessage(" -> Pairing failed. Clearing bond and disconnecting.", " -> Pairing failed");
            NimBLEDevice::deleteBond(connInfo.getAddress());
            if (pClient != nullptr) {
                pClient->disconnect();
            }
        }
    }
};
static MyClientCallbacks clientCallbacks;

// ----------------------------------------------------------------------------
// Processes BLE advertising devices during scanning to locate target device.
// ----------------------------------------------------------------------------
class MyAdvertisedDeviceCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        if (Serial) {
            String devName = advertisedDevice->haveName() ? advertisedDevice->getName().c_str() : "Unknown";
            Serial.printf("Found BLE -> Addr: %s, Name: %s\n", 
                advertisedDevice->getAddress().toString().c_str(), devName.c_str());
        }

        xSemaphoreTake(systemMutex, portMAX_DELAY);
        bool hasAddr = hasTargetAddress;
        NimBLEAddress targetAddr = targetServerAddress;
        xSemaphoreGive(systemMutex);

        bool match = false;
        // If a target address is already saved, check if the advertised device matches it
        if (hasAddr && advertisedDevice->getAddress() == targetAddr) {
            match = true;
        } else if (advertisedDevice->haveName() && advertisedDevice->getName() == "GlanceClock") {
            match = true;
        }

        // If target is matched, stop scan and save configuration
        if (match) {
            NimBLEDevice::getScan()->stop();
            
            xSemaphoreTake(systemMutex, portMAX_DELAY);
            targetServerAddress = advertisedDevice->getAddress();
            hasTargetAddress = true;
            bleState.doConnect = true;
            NimBLEAddress newAddr = targetServerAddress;
            xSemaphoreGive(systemMutex);

            // Save the discovered device address to Preferences NVS storage
            preferences.begin("ble-config", false);
            preferences.putString("targetAddr", newAddr.toString().c_str());
            preferences.end();

            logMessage("Glance Clock found & saved [" + String(newAddr.toString().c_str()) + "]", 
                       "Found & Saved: " + String(newAddr.toString().c_str()));
        }
    }
};

// ----------------------------------------------------------------------------
// Initializes BLE stack, security settings, local CTS server, and client scan.
// ----------------------------------------------------------------------------
void setupBLE() {
    NimBLEDevice::init("M5Stack-Bridge");
    NimBLEDevice::setMTU(512);

    // Configure BLE security parameters and authentication requirements
    NimBLEDevice::setSecurityAuth(true, true, true);
    // CAUTION: Never change the BLE security IO Capability from BLE_HS_IO_KEYBOARD_DISPLAY.
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_DISPLAY);

    // Initialize local CTS server to allow the clock to read/subscribe to current time (UUID 1805 / 2a2b)
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);

    NimBLEService* pCtsService = pServer->createService("1805");
    pCtsCharacteristic = pCtsService->createCharacteristic(
        "2a2b",
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    pCtsCharacteristic->setCallbacks(new CTSCallbacks());
    pCtsCharacteristic->createDescriptor("2902");
    pCtsService->start();

    // Start advertising local services including CTS (1805)
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(pCtsService->getUUID());
    pAdvertising->addServiceUUID(GLANCE_SERVICE_UUID);
    pAdvertising->start();

    // Create BLE client instance and register client callbacks
    pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(&clientCallbacks);

    // Configure BLE scanner and advertised device callbacks
    NimBLEScan* pBLEScan = NimBLEDevice::getScan();
    pBLEScan->setScanCallbacks(new MyAdvertisedDeviceCallbacks(), false);
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
        logMessage("Starting initial BLE scan (10s)...");
        pBLEScan->start(10000, false);
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        if (!hasTargetAddress && !bleState.connected) {
            bleState.doScan = true;
            bleState.disconnectTime = millis();
        }
        xSemaphoreGive(systemMutex);
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
    NimBLEAddress targetAddr = targetServerAddress;
    NimBLERemoteCharacteristic* currentRemoteCharCheck = pRemoteChar;
    NimBLEClient* localClient = pClient;
    xSemaphoreGive(systemMutex);

    // Verify encryption status from connection info if not yet marked ready
    if (connected && !authReady && localClient != nullptr) {
        auto connInfo = localClient->getConnInfo();
        if (connInfo.isEncrypted()) {
            xSemaphoreTake(systemMutex, portMAX_DELAY);
            bleState.isAuthReady = true;
            xSemaphoreGive(systemMutex);
        }
    }

    // When connected and authenticated, discover remote services and characteristics if not yet discovered
    if (connected && authReady && currentRemoteCharCheck == nullptr && localClient != nullptr) {
        vTaskDelay(pdMS_TO_TICKS(500));

        NimBLERemoteService* pRemoteService = localClient->getService(GLANCE_SERVICE_UUID);
        if (pRemoteService != nullptr) {
            // Obtain main communication remote characteristic first
            NimBLERemoteCharacteristic* tempRemoteChar = pRemoteService->getCharacteristic(GLANCE_CHAR_UUID);
            if (tempRemoteChar != nullptr) {
                xSemaphoreTake(systemMutex, portMAX_DELAY);
                pRemoteChar = tempRemoteChar;
                xSemaphoreGive(systemMutex);
            }

            // Register for notifications on the notification characteristic (fc78) after pRemoteChar is set
            NimBLERemoteCharacteristic* tempNotifyChar = pRemoteService->getCharacteristic(GLANCE_CHAR_NOTIFY_UUID);
            if (tempNotifyChar != nullptr && tempNotifyChar->canNotify()) {
                tempNotifyChar->subscribe(true, notifyCallbackFC78);
                xSemaphoreTake(systemMutex, portMAX_DELAY);
                pNotifyChar = tempNotifyChar;
                xSemaphoreGive(systemMutex);
            }
        }
    }

    // Trigger time sync automatically once notification response is received from clock
    xSemaphoreTake(systemMutex, portMAX_DELAY);
    bool currentNeedSync = bleState.needCtsSync;
    NimBLERemoteCharacteristic* currentRemoteChar = pRemoteChar;
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
    if (doConnect && localClient != nullptr) {
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        bleState.doConnect = false;
        xSemaphoreGive(systemMutex);

        vTaskDelay(pdMS_TO_TICKS(50));

        // Attempt connection using random address type; if failed, schedule rescan retry
        // CAUTION: Never change the BLE address type from BLE_ADDR_RANDOM.
        if (localClient->connect(targetAddr, BLE_ADDR_RANDOM)) {
            vTaskDelay(pdMS_TO_TICKS(500));

            logMessage("Requesting secure connection / encryption...");
            localClient->secureConnection();
        } else {
            logMessage("Connection failed. Rescanning in 5 seconds.");
            xSemaphoreTake(systemMutex, portMAX_DELAY);
            bleState.doScan = true;
            bleState.disconnectTime = millis();
            xSemaphoreGive(systemMutex);
        }
        updateScreenStatus();
    }

    // Re-verify scan and connection flags safely via mutex before starting periodic rescan
    xSemaphoreTake(systemMutex, portMAX_DELAY);
    bool stillScan = bleState.doScan;
    bool stillConn = bleState.connected;
    xSemaphoreGive(systemMutex);

    // Handle periodic rescanning after disconnect timeout delay
    if (stillScan && !stillConn && (millis() - discTime >= 5000)) {
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        bleState.doScan = false;
        xSemaphoreGive(systemMutex);
        
        logMessage("Rescanning (10s)...");
        NimBLEScan* pBLEScan = NimBLEDevice::getScan();
        pBLEScan->start(10000, false);
        
        xSemaphoreTake(systemMutex, portMAX_DELAY);
        bool hasAddr = hasTargetAddress;
        bool isConn = bleState.connected;
        if (!hasAddr && !isConn) {
            bleState.doScan = true;
            bleState.disconnectTime = millis();
        }
        xSemaphoreGive(systemMutex);

        updateScreenStatus();
    }
}
