#include "http_server.h"
#include "utils.h"
#include "packet_builder.h"

// ----------------------------------------------------------------------------
// Sends a standard HTTP response with CORS headers and specified content type.
// ----------------------------------------------------------------------------
void sendResponse(int status, const String& message, const String& contentType) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(status, contentType, message);
}

// ----------------------------------------------------------------------------
// Initializes and registers all HTTP REST API endpoints and web server handlers.
// ----------------------------------------------------------------------------
void setupHttpServer() {
    // PIN code reception: http://glance-clock.local/pin?code=123456
    server.on("/pin", HTTP_GET, []() {
        if (server.hasArg("code")) {
            webPinCode = server.arg("code");
            hasWebPin = true;
            sendResponse(200, "OK: PIN received -> " + webPinCode);
            M5.Display.printf("PIN Recv: %s\n", webPinCode.c_str());
        } else {
            handleErrorResponse(400, "Error: Missing code parameter");
        }
    });

    // Time sync: http://glance-clock.local/sync
    server.on("/sync", HTTP_GET, []() {
        notifyCTSTime();
        sendResponse(200, "OK: Time synced");
        logMessage("Sync: Time Synced");
    });

    // Update device settings: http://glance-clock.local/settings?pointsEnabled=1
    server.on("/settings", HTTP_GET, []() {
        if (server.hasArg("nightMode"))     currentSettings.nightMode = (server.arg("nightMode") == "1");
        if (server.hasArg("permanentDND"))  currentSettings.permanentDND = (server.arg("permanentDND") == "1");
        if (server.hasArg("permanentMute")) currentSettings.permanentMute = (server.arg("permanentMute") == "1");
        if (server.hasArg("dateFormat"))    currentSettings.dateFormat = server.arg("dateFormat").toInt();
        if (server.hasArg("pointsEnabled")) currentSettings.pointsEnabled = (server.arg("pointsEnabled") == "1");
        if (server.hasArg("brightness"))    currentSettings.brightness = server.arg("brightness").toInt();
        if (server.hasArg("timeModeEnable")) currentSettings.timeModeEnable = (server.arg("timeModeEnable") == "1");
        if (server.hasArg("timeFormat12"))  currentSettings.timeFormat12 = (server.arg("timeFormat12") == "1");

        sendSettingsCommand();
        sendResponse(200, "OK: Settings applied");
        M5.Display.printf("Set: Bright=%d\n", currentSettings.brightness);
    });

    // Send notification: http://glance-clock.local/notice?text=Hello&icon=133
    server.on("/notice", HTTP_GET, []() {
        String msg  = server.hasArg("text") ? server.arg("text") : " ";
        int anim    = server.hasArg("anim") ? server.arg("anim").toInt() : 1;
        int sound   = server.hasArg("sound") ? server.arg("sound").toInt() : 5;
        int color   = server.hasArg("color") ? server.arg("color").toInt() : 23;
        int mod     = server.hasArg("mod") ? server.arg("mod").toInt() : 1;
        int icon    = server.hasArg("icon") ? server.arg("icon").toInt() : 133;

        sendCustomNoticeCommand(msg, anim, sound, color, mod, icon);
        sendResponse(200, "OK: Notice sent -> " + msg);
    });

    // Register calendar event: http://glance-clock.local/calendar?starth=21&startmin=0&inform=12&dur=60&title=test
    server.on("/calendar", HTTP_GET, []() {
        std::vector<uint8_t> cmd(Prefix::APPOINTMENTS_SCENE, Prefix::APPOINTMENTS_SCENE + sizeof(Prefix::APPOINTMENTS_SCENE));
        cmd.push_back(8);
        cmd.push_back(1); // Slot
        
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            handleErrorResponse(500, "Error: Time not synced", "Time Not Synced");
            return;
        }

        int startHour   = server.hasArg("starth") ? server.arg("starth").toInt() : 21;
        int startMin    = server.hasArg("startmin") ? server.arg("startmin").toInt() : 0;
        int informMin   = server.hasArg("inform") ? server.arg("inform").toInt() : 12;
        int durationMin = server.hasArg("dur") ? server.arg("dur").toInt() : 60;
        String title    = server.hasArg("title") ? server.arg("title") : "Meeting";

        time_t now;
        time(&now);

        cmd.reserve(128);
        
        cmd.insert(cmd.end(), { PB_TAG(1, PbWire::VARINT) });
        encodeVarintVector(cmd, (uint64_t)now);

        cmd.insert(cmd.end(), {PB_TAG(2, PbWire::BYTES), 0x08});

        // Safe bounds checking and time calculations
        if (startMin >= 60) startMin = 59;
        if (startMin < 0) startMin = 0;
        if (startHour < 2) startHour += 24;
        if (startHour > 25) startHour = 25;
        int targetMinutesToday = (startHour - 2) * 60 + startMin;
        
        uint16_t tmpVal = (((targetMinutesToday << 5) & 0xFFE0) | 0x001C); 
        uint16_t timeVal = (((tmpVal << 3) & 0xFFF8) | ((tmpVal >> 13) & 0x0007));
        
        cmd.insert(cmd.end(), { (uint8_t)(timeVal & 0xFF), (uint8_t)((timeVal >> 8) & 0xFF) });

        if (durationMin < 30) durationMin = 30;
        tmpVal = (((informMin << 6) & 0xFFC0) | ((durationMin/30 - 1) & 0x3F));
        uint16_t durVal = (((tmpVal << 8) & 0xFF00) | ((tmpVal >> 8) & 0x00FF));

        cmd.insert(cmd.end(), { (uint8_t)(durVal & 0xFF), (uint8_t)((durVal >> 8) & 0xFF) });
        cmd.insert(cmd.end(), { 255, 255, 0, 12 });

        std::vector<uint8_t> textData;
        textData.reserve(title.length() + 4);
        textData.insert(textData.end(), {PB_TAG(1, PbWire::VARINT), 0x01});
        textData.insert(textData.end(), {PB_TAG(2, PbWire::BYTES), (uint8_t)title.length()});

        const uint8_t* titleBytes = (const uint8_t*)title.c_str();
        textData.insert(textData.end(), titleBytes, titleBytes + title.length());

        cmd.insert(cmd.end(), { PB_TAG(3, PbWire::BYTES) });
        encodeVarintVector(cmd, textData.size());
        cmd.insert(cmd.end(), textData.begin(), textData.end());

        cmd.insert(cmd.end(), {PB_TAG(6, PbWire::VARINT), 0x01});
        cmd.insert(cmd.end(), {PB_TAG(7, PbWire::VARINT), (uint8_t)Sound::Radar});

        if (sendGlanceCommand(cmd.data(), cmd.size())) {
            uint8_t refreshCmd[] = {CMD_UPDATE_REFRESH, 0x00, 0x00, 0x00};
            sendGlanceCommand(refreshCmd, sizeof(refreshCmd));
            
            delay(50);
            uint8_t sceneCmd[] = {CMD_SCENE_START, 0x00, 0x00, 0x00};
            sendGlanceCommand(sceneCmd, sizeof(sceneCmd));

            sendResponse(200, "OK: Event created, refreshed & scenes started");
            M5.Display.printf("Cal: %s\n", title.c_str());
        } else {
            handleErrorResponse(500, "Error: Failed to send via BLE", "Calendar Send Failed");
        }
    });

    server.on("/calendar_clr", HTTP_GET, []() {
        std::vector<uint8_t> packet(Prefix::APPOINTMENTS_SCENE, Prefix::APPOINTMENTS_SCENE + sizeof(Prefix::APPOINTMENTS_SCENE));
        packet.push_back(0);
        packet.push_back(1); // Slot
        
        time_t now;
        time(&now);

        packet.reserve(32);

        packet.insert(packet.end(), { PB_TAG(1, PbWire::VARINT) });
        encodeVarintVector(packet, (uint32_t)now);
        
        const uint8_t clearPayload[] = {18, 0, 48, 0, 56, 5};
        packet.insert(packet.end(), clearPayload, clearPayload + sizeof(clearPayload));

        if (sendGlanceCommand(packet.data(), packet.size())) {
            sendResponse(200, "OK: Calendar cleared");
            logMessage("Calendar Clear: Success");
        } else {
            handleErrorResponse(500, "Error: Failed to send via BLE");
        }
    });
    
    // Ring LED display: http://glance-clock.local/ring?values=0,10,20,30,40,50,60,70,80,90,100,110,120,130,140,150,160,170,180,190,200,210,220,230&maxValue=230&minValue=0&message=Test
    server.on("/ring", HTTP_GET, []() {
        uint8_t slot      = server.hasArg("slot") ? server.arg("slot").toInt() : 1;
        uint32_t maxColor = server.hasArg("maxColor") ? strtoul(server.arg("maxColor").c_str(), NULL, 16) : 0xFF0000;
        uint32_t minColor = server.hasArg("minColor") ? strtoul(server.arg("minColor").c_str(), NULL, 16) : 0x0000FF;
        int16_t maxValue  = server.hasArg("maxValue") ? server.arg("maxValue").toInt() : 30;
        int16_t minValue  = server.hasArg("minValue") ? server.arg("minValue").toInt() : 0;
        String message    = server.hasArg("message") ? server.arg("message") : "\xC2\x8F\x08\xC2\xB0";

        // Define default values string if values parameter is omitted
        String vStr = server.hasArg("values") 
            ? server.arg("values") 
            : "25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25";

        std::vector<int16_t> ringValues;
        ringValues.reserve(24);

        int start = 0;
        int commaIdx = vStr.indexOf(',');
        while (commaIdx != -1) {
            ringValues.push_back(vStr.substring(start, commaIdx).toInt());
            start = commaIdx + 1;
            commaIdx = vStr.indexOf(',', start);
        }
        if (start < vStr.length()) {
            ringValues.push_back(vStr.substring(start).toInt());
        }

        std::vector<uint8_t> packet = buildRingPacket(ringValues, slot, maxColor, minColor, maxValue, minValue, message);
        
        if (sendGlanceCommand(packet.data(), packet.size())) {
            uint8_t refreshCmd[] = {CMD_UPDATE_REFRESH, 0x00, 0x00, 0x00};
            sendGlanceCommand(refreshCmd, sizeof(refreshCmd));
            
            delay(50);
            uint8_t sceneCmd[] = {CMD_SCENE_START, 0x00, 0x00, 0x00};
            sendGlanceCommand(sceneCmd, sizeof(sceneCmd));
            
            sendResponse(200, "OK: Ring command sent -> " + message);
            M5.Display.printf("Ring Sent (%ub)\n", packet.size());
        } else {
            handleErrorResponse(500, "Error: Failed to send via BLE");
        }
    });
    
    // Timer setup: http://glance-clock.local/timer?duration=60
    server.on("/timer", HTTP_GET, []() {
        int duration    = server.hasArg("duration") ? server.arg("duration").toInt() : 60;
        String label    = server.hasArg("label") ? server.arg("label" ) : "Timer";
        String message    = server.hasArg("message") ? server.arg("message") : "Time's up";
    
        std::vector<uint8_t> packet = buildTimerPacket(duration, label, message);

        if (sendGlanceCommand(packet.data(), packet.size())) {
            delay(50);

            sendResponse(200, "{\"status\":\"success\",\"duration\":"  + String(duration) + "}", "application/json");
            M5.Display.printf("Timer Set: %02d sec\n", duration);
        } else {
            handleErrorResponse(500, "Error: Failed to send via BLE", "Timer Send Failed");
        }
    });

    // Alarm setup: http://glance-clock.local/alarm?alarmHour=23&alarmMin=37&alarmDays=127&alarmSound=1&alarmText=ALARM
    server.on("/alarm", HTTP_GET, []() {
        int days    = server.hasArg("alarmDays") ? server.arg("alarmDays").toInt() : 127; 
        int hour    = server.hasArg("alarmHour") ? server.arg("alarmHour").toInt() : 2;
        int minutes = server.hasArg("alarmMin") ? server.arg("alarmMin").toInt() : 0;
        int sound   = server.hasArg("alarmSound") ? server.arg("alarmSound").toInt() : 1; 
        String message = server.hasArg("alarmText") ? server.arg("alarmText") : "ALARM";

        std::vector<uint8_t> packet = buildAlarmPacket(days, hour, minutes, sound, message);

        if (sendGlanceCommand(packet.data(), packet.size())) {
            uint8_t refreshCmd[] = {CMD_UPDATE_REFRESH, 0x00, 0x00, 0x00};
            sendGlanceCommand(refreshCmd, sizeof(refreshCmd));
            
            delay(50);
            uint8_t sceneCmd[] = {CMD_SCENE_START, 0x00, 0x00, 0x00};
            sendGlanceCommand(sceneCmd, sizeof(sceneCmd));

            sendResponse(200, "OK: Alarm set");
            M5.Display.printf("Alarm Set: %02d:%02d\n", hour, minutes);
        } else {
            handleErrorResponse(500, "Error: Failed to send via BLE", "Alarm Send Failed");
        }
    });

    // Raw byte array direct send endpoint: http://glance-clock.local/array?data=[]
    server.on("/array", HTTP_GET, []() {
        if (!server.hasArg("data")) {
            handleErrorResponse(400, "Error: Missing data argument");
            return;
        }

        String rawData = server.arg("data");
        String cleanHex = "";

        // Extract valid hexadecimal characters safely
        for (size_t i = 0; i < rawData.length(); i++) {
            char c = rawData.charAt(i);
            if (isxdigit(c)) {
                cleanHex += c;
            }
        }

        if (cleanHex.length() % 2 != 0) {
            cleanHex = cleanHex.substring(0, cleanHex.length() - 1);
        }

        std::vector<uint8_t> cmd;
        cmd.reserve(cleanHex.length() / 2);
        
        for (size_t i = 0; i < cleanHex.length(); i += 2) {
            String byteStr = cleanHex.substring(i, i + 2);
            uint8_t b = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
            cmd.push_back(b);
        }

        clearLogArea();
        M5.Display.printf("Array: %d bytes\n", cmd.size());

        if (sendGlanceCommand(cmd.data(), cmd.size())) {
            uint8_t refreshCmd[] = {CMD_UPDATE_REFRESH, 0x00, 0x00, 0x00};
            sendGlanceCommand(refreshCmd, sizeof(refreshCmd));
            
            delay(50);
            uint8_t sceneCmd[] = {CMD_SCENE_START, 0x00, 0x00, 0x00};
            sendGlanceCommand(sceneCmd, sizeof(sceneCmd));

            sendResponse(200, "OK: Custom array sent");
        } else {
            handleErrorResponse(500, "Error: BLE send failed");
        }
    });

    // Unified generic quick command endpoint: http://glance-clock.local/cmd?byte=31
    server.on("/cmd", HTTP_GET, []() {
        if (!server.hasArg("byte")) {
            handleErrorResponse(400, "Error: Missing byte parameter");
            return;
        }

        String byteStr = server.arg("byte");
        uint8_t cmdByte = (uint8_t)strtoul(byteStr.c_str(), NULL, 0);

        String label = "";

        // Argument-free single-byte command routing (Sorted by numeric value)
        switch (cmdByte) {
            case CMD_TIMER_STOP:             // 10 (0x0A)
                label = "Timer Stop";
                break;
            case CMD_ALARM_STOP:             // 20 (0x14)
                label = "Alarm Stop";
                break;
            case CMD_ALARM_CLEAR:            // 21 (0x15)
                label = "Alarm Clear";
                break;
            case CMD_SCENE_STOP:             // 30 (0x1E)
                label = "Scenes Stop";
                break;
            case CMD_SCENE_START:            // 31 (0x1F)
                label = "Scenes Start";
                break;
            case CMD_SCENE_CLEAR:            // 32 (0x20)
                label = "Scenes Clear";
                break;
            case CMD_UPDATE_REFRESH:         // 35 (0x23)
                label = "Update & Refresh";
                break;
            case CMD_AUTO_NIGHT_MODE_EN:     // 40 (0x28)
                label = "Enable Auto Night Mode";
                break;
            case CMD_AUTO_NIGHT_MODE_DIS:    // 41 (0x29)
                label = "Disable Auto Night Mode";
                break;
            case CMD_CLEAR_BONDS:            // 42 (0x2A)
                label = "Bonds Clear";
                break;
            case CMD_CALIBRATION_START:      // 43 (0x2B)
                label = "Start Calibration";
                break;
            case CMD_CALIBRATION_CONFIRM:    // 44 (0x2C)
                label = "Confirm Calibration";
                break;
            case CMD_ALARM_WITH_NOTES:       // 45 (0x2D)
                label = "Alarm with Notes";
                break;
            case CMD_CLEAR_USER_INFO:        // 50 (0x32)
                label = "Clear User Info (Factory Reset)";
                break;
            case CMD_BRIGHTNESS_SCENE_STOP:  // 60 (0x3C)
                label = "Brightness Scene Stop";
                break;
            case CMD_BRIGHTNESS_SCENE_START: // 61 (0x3D)
                label = "Brightness Scene Start";
                break;
            case CMD_DSP_STATE_SHOW:         // 70 (0x46)
                label = "DSP State Show (Serial)";
                break;
            default:
                // Fallback for unlisted custom or unknown commands
                label = "Unknown (0x" + String(cmdByte, HEX) + ")";
                break;
        }

        uint8_t cmd[] = {cmdByte, 0x00, 0x00, 0x00};
        
        if (sendGlanceCommand(cmd, sizeof(cmd))) {
            sendResponse(200, "OK: " + label);
            M5.Display.printf("Cmd: %s\n", label.c_str());
        } else {
            handleErrorResponse(500, "Error: BLE send failed", label + " Failed");
        }
    });

    server.begin();
    logMessage("HTTP server started.");
}
