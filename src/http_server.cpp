#include "http_server.h"
#include "utils.h"
#include "packet_builder.h"
#include "SharedData.h"
#include "mqtt_manager.h"

// ----------------------------------------------------------------------------
// Embedded HTML Control Panel Page (Stored in Flash memory / PROGMEM)
// ----------------------------------------------------------------------------
const char HTML_INDEX[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Glance Clock Master Control Panel</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            max-width: 900px;
            margin: 20px auto;
            padding: 0 20px;
            background-color: #f0f2f5;
            color: #333;
        }
        h1 { margin-bottom: 20px; }
        h2 { border-bottom: 2px solid #007bff; padding-bottom: 5px; color: #007bff; font-size: 1.2rem; }
        .config-box, .section {
            background: #fff;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.05);
            margin-bottom: 20px;
        }
        .form-group {
            margin-bottom: 15px;
            display: flex;
            flex-direction: column;
        }
        .form-row {
            display: flex;
            gap: 15px;
            flex-wrap: wrap;
        }
        .form-row .form-group {
            flex: 1;
            min-width: 200px;
        }
        label {
            font-weight: bold;
            margin-bottom: 5px;
            font-size: 0.9rem;
        }
        input[type="text"], input[type="number"], select {
            padding: 10px;
            border: 1px solid #ccc;
            border-radius: 4px;
            font-size: 14px;
        }
        .checkbox-group {
            display: flex;
            align-items: center;
            gap: 10px;
            margin-top: 10px;
        }
        .checkbox-group input { width: 18px; height: 18px; }
        button {
            padding: 12px 20px;
            background-color: #007bff;
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 14px;
            font-weight: bold;
            transition: background-color 0.2s;
        }
        button:hover { background-color: #0056b3; }
        button.secondary { background-color: #6c757d; }
        button.secondary:hover { background-color: #5a6268; }
        button.danger { background-color: #dc3545; }
        button.danger:hover { background-color: #bd2130; }
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(180px, 1fr));
            gap: 10px;
        }
        #log {
            background: #1e1e1e;
            color: #00ff66;
            padding: 12px;
            border-radius: 4px;
            font-family: 'Courier New', Courier, monospace;
            height: 120px;
            overflow-y: auto;
            font-size: 13px;
        }
    </style>
</head>
<body>

    <h1>Glance Clock Control Panel</h1>
    
    <!-- Connection Settings -->
    <div class="config-box">
        <div class="form-group" style="margin-bottom: 0;">
            <label for="m5Ip">M5Stack IP Address</label>
            <input type="text" id="m5Ip" value="%IP_ADDRESS%" placeholder="glance-clock.local or IP" readonly>
        </div>
    </div>
    
    <!-- 1. Custom Notice Command -->
    <div class="section">
        <h2>1. Custom Notice Command</h2>
        <div class="form-group">
            <label for="noticeText">Message Text</label>
            <input type="text" id="noticeText" value="Hello World!" placeholder="Enter message to display">
        </div>

        <div class="form-row">
            <div class="form-group">
                <label for="noticeAnim">Animation</label>
                <select id="noticeAnim">
                    <option value="0">NoneAnimation (0)</option>
                    <option value="1" selected>Pulse (1)</option>
                    <option value="2">Wave (2)</option>
                    <option value="10">Fire (10)</option>
                    <option value="11">Wheel (11)</option>
                    <option value="12">Flower (12)</option>
                    <option value="13">Flower2 (13)</option>
                    <option value="14">Fan (14)</option>
                    <option value="15">Sun (15)</option>
                    <option value="16">Thunderstorm (16)</option>
                    <option value="17">Cloud (17)</option>
                    <option value="101">WeatherClear (101)</option>
                    <option value="102">WeatherCloudy (102)</option>
                    <option value="103">WeatherFog (103)</option>
                    <option value="104">WeatherLightRain (104)</option>
                    <option value="105">WeatherRain (105)</option>
                    <option value="106">WeatherThunderstorm (106)</option>
                    <option value="107">WeatherSnow (107)</option>
                    <option value="108">WeatherHail (108)</option>
                    <option value="109">WeatherWind (109)</option>
                    <option value="110">WeatherTornado (110)</option>
                    <option value="111">WeatherHurricane (111)</option>
                    <option value="112">WeatherSnowThunderstorm (112)</option>
                </select>
            </div>

            <div class="form-group">
                <label for="noticeSound">Sound</label>
                <select id="noticeSound">
                    <option value="0">NoneSound (0)</option>
                    <option value="1">Waves / Alarm (1)</option>
                    <option value="2">Rise / Calendar (2)</option>
                    <option value="3">Charging (3)</option>
                    <option value="4">Steps / Fitness (4)</option>
                    <option value="5" selected>Radar / General 1 (5)</option>
                    <option value="6">Bells / General 2 (6)</option>
                    <option value="7">Bye (7)</option>
                    <option value="8">Hello (8)</option>
                    <option value="9">Flowers / Ringtone (9)</option>
                    <option value="10">Circles / Taxi (10)</option>
                    <option value="11">Complete / Timer End (11)</option>
                    <option value="12">Popcorn / Weather (12)</option>
                    <option value="13">Break (13)</option>
                    <option value="14">Opening (14)</option>
                    <option value="15">High (15)</option>
                    <option value="16">Shine (16)</option>
                    <option value="17">Extension (17)</option>
                </select>
            </div>
        </div>

        <div class="form-row">
            <div class="form-group">
                <label for="noticeColor">Color</label>
                <select id="noticeColor">
                    <option value="0">Black (0)</option>
                    <option value="1">DarkGoldenRod (1)</option>
                    <option value="2">Darkorange (2)</option>
                    <option value="3">Olive (3)</option>
                    <option value="4">OrangeRed (4)</option>
                    <option value="5">Red (5)</option>
                    <option value="12">White (12)</option>
                    <option value="15">Blue (15)</option>
                    <option value="23" selected>Lime (23)</option>
                    <option value="25">Google1 (25)</option>
                    <option value="26">Google2 (26)</option>
                    <option value="27">Google3 (27)</option>
                </select>
            </div>

            <div class="form-group">
                <label for="noticeMod">Text Modificator</label>
                <select id="noticeMod">
                    <option value="0">None (0)</option>
                    <option value="1" selected>Repeat (1)</option>
                    <option value="2">Rapid (2)</option>
                    <option value="3">Delay (3)</option>
                </select>
            </div>
        </div>

        <div class="form-row" style="margin-top: 15px;">
            <div class="form-group">
                <label for="noticeIcon">Icon</label>
                <select id="noticeIcon">
                    <option value="0">None (0)</option>
                    <option value="128">House (128)</option>
                    <option value="129">Phone (129)</option>
                    <option value="130">Clock (130)</option>
                    <option value="131">Plug (131)</option>
                    <option value="132">Smartphone (132)</option>
                    <option value="133">Star/Sun/Bright (133)</option>
                    <option value="134" selected>Smiley (134)</option>
                    <option value="135">White Block (135)</option>
                    <option value="136">Nothing (136)</option>
                    <option value="137">0/3 Battery (137)</option>
                    <option value="138">1/3 Battery (138)</option>
                    <option value="139">2/3 Battery (139)</option>
                    <option value="140">3/3 Battery (140)</option>
                    <option value="141">Bell (diagonal line) (141)</option>
                    <option value="142">Speaker off (142)</option>
                    <option value="143">Thermometer (143)</option>
                    <option value="144">Droplet (144)</option>
                    <option value="145">Heart (145)</option>
                    <option value="146">Barometer (146)</option>
                    <option value="147">Pipe arrow left (147)</option>
                    <option value="148">Arrow right pipe (148)</option>
                    <option value="149">Umbrella (149)</option>
                    <option value="150">Bell (150)</option>
                    <option value="151">Speaker (151)</option>
                    <option value="152">Wind speed (152)</option>
                    <option value="153">Cloud (153)</option>
                    <option value="154">Missing Character (154)</option>
                    <option value="176">Åã Degree (176)</option>
                </select>
            </div>
        </div>

        <button style="margin-top: 15px;" onclick="sendNotice()">Send Custom Notice</button>
    </div>

    <!-- 2. Device Settings -->
    <div class="section">
        <h2>2. Device Settings</h2>
        <div class="form-row">
            <div class="form-group">
                <label for="dateFormat">Date Format</label>
                <select id="dateFormat">
                    <option value="0">DateDisabled (0)</option>
                    <option value="1" selected>Date24Jan (1)</option>
                    <option value="2">Date24Tue (2)</option>
                    <option value="3">DateJan24 (3)</option>
                    <option value="4">DateTue24 (4)</option>
                </select>
            </div>

            <div class="form-group">
                <label for="brightness">Display Brightness (0-255)</label>
                <input type="number" id="brightness" value="250" min="0" max="255">
            </div>
        </div>

        <div class="form-row">
            <div class="form-group checkbox-group">
                <input type="checkbox" id="pointsEnabled" checked>
                <label for="pointsEnabled" style="margin-bottom:0;">Points Always Enabled</label>
            </div>

            <div class="form-group checkbox-group">
                <input type="checkbox" id="nightMode">
                <label for="nightMode" style="margin-bottom:0;">Night Mode</label>
            </div>

            <div class="form-group checkbox-group">
                <input type="checkbox" id="timeFormat12">
                <label for="timeFormat12" style="margin-bottom:0;">12-Hour Format</label>
            </div>

            <div class="form-group checkbox-group">
                <input type="checkbox" id="permanentMute">
                <label for="permanentMute" style="margin-bottom:0;">Permanent Mute</label>
            </div>
        </div>

        <button style="margin-top: 15px;" onclick="sendSettings()">Apply Settings</button>
    </div>

    <!-- 3. Alarm & Timer Settings -->
    <div class="section">
        <h2>3. Alarm & Timer Settings</h2>
        <div class="form-row">
            <div class="form-group">
                <label>Alarm Time</label>
                <div style="display: flex; gap: 8px; align-items: center; margin-bottom: 8px;">
                    <input type="number" id="Hour" value="7" min="0" max="23" style="width: 70px;"> :
                    <input type="number" id="Min" value="0" min="0" max="59" style="width: 70px;"> 
                </div>
                <div style="display: flex; gap: 8px; align-items: center; flex-wrap: wrap;">
                    <button onclick="setAlarm()">Set Alarm</button>
                    <!-- Stop Alarm: CMD_ALARM_STOP (byte=20) -->
                    <button class="secondary" onclick="sendQuick('quick?byte=20')">Stop Alarm</button>
                    <!-- Alarm Clear: CMD_ALARM_CLEAR (byte=21) -->
                    <button class="secondary" onclick="sendQuick('quick?byte=21')">Alarm Clear</button>
                </div>
            </div>

            <div class="form-group">
                <label>Timer Duration</label>
                <div style="display: flex; gap: 8px; align-items: center; margin-bottom: 8px;">
                    <input type="number" id="timerSec" value="180" min="1" style="width: 90px;"> sec
                </div>
                <div style="display: flex; gap: 8px; align-items: center;">
                    <button onclick="setTimer()">Start Timer</button>
                    <!-- Stop Timer: CMD_TIMER_STOP (byte=10) -->
                    <button class="secondary" onclick="sendQuick('quick?byte=10')">Stop Timer</button>
                </div>
            </div>
        </div>
    </div>

    <!-- 4. Quick Control Commands -->
    <div class="section">
        <h2>4. Quick Control Commands</h2>
        <div class="grid">
            <!-- Scenes Stop: byte=30 -->
            <button class="secondary" onclick="sendQuick('quick?byte=30')">Scenes Stop</button>
            <!-- Scenes Start: byte=31 -->
            <button class="secondary" onclick="sendQuick('quick?byte=31')">Scenes Start</button>
            <!-- Scenes Clear: byte=32 -->
            <button class="secondary" onclick="sendQuick('quick?byte=32')">Scenes Clear</button>
            <!-- Update & Refresh: byte=35 -->
            <button class="secondary" onclick="sendQuick('quick?byte=35')">Update & Refresh</button>
        </div>
    </div>

    <!-- 5. PIN Code Authentication -->
    <div class="section">
        <h2>5. PIN Code Authentication</h2>
        <div class="form-row">
            <div class="form-group">
                <label for="pinCode">PIN Code</label>
                <div style="display: flex; gap: 8px; align-items: center;">
                    <input type="text" id="pinCode" value="123456" placeholder="Enter PIN code" style="width: 150px;">
                    <button onclick="sendPinCode()">Send PIN</button>
                </div>
            </div>
        </div>
    </div>

    <!-- 6. MQTT Server Settings -->
    <div class="section">
        <h2>6. MQTT Server Settings</h2>
        <div class="form-row">
            <div class="form-group">
                <label for="mqttServerIp">MQTT Broker IP / Host</label>
                <input type="text" id="mqttServerIp" value="" placeholder="192.168.x.x">
            </div>
            <div class="form-group" style="max-width: 120px;">
                <label for="mqttPortNum">Port</label>
                <input type="number" id="mqttPortNum" value="1883">
            </div>
        </div>
        <button style="margin-top: 15px;" onclick="sendMqttConfig()">Save & Connect MQTT</button>
    </div>

    <!-- Console Output -->
    <div class="section">
        <h2>Console Output</h2>
        <div id="log">Ready...</div>
    </div>

    <script>
        function log(message) {
            const logBox = document.getElementById('log');
            logBox.innerHTML += `[${new Date().toLocaleTimeString()}] ${message}<br>`;
            logBox.scrollTop = logBox.scrollHeight;
        }
        
        // Send PIN code function (/pin endpoint)
        function sendPinCode() {
            const pin = document.getElementById('pinCode').value;

            const url = `/pin?code=${pin}`;
            log(`Sending PIN Request: ${url}`);

            fetch(url, { method: 'GET' })
                .then(res => res.text())
                .then(data => log(`Success: ${data || 'OK'}`))
                .catch(err => log(`Error: ${err.message}`));
        }

        // Save MQTT server configuration function (/mqtt endpoint)
        function sendMqttConfig() {
            const server = document.getElementById('mqttServerIp').value;
            const port = document.getElementById('mqttPortNum').value;

            const url = `/mqtt?server=${server}&port=${port}`;
            log(`Sending MQTT Config Request: ${url}`);

            fetch(url, { method: 'GET' })
                .then(res => res.text())
                .then(data => log(`Success: ${data || 'OK'}`))
                .catch(err => log(`Error: ${err.message}`));
        }

        // Apply settings function (/settings endpoint)
        function sendSettings() {
            const dateFormat = document.getElementById('dateFormat').value;
            const brightness = document.getElementById('brightness').value;
            const nightMode = document.getElementById('nightMode').checked ? 1 : 0;
            const pointsEnabled = document.getElementById('pointsEnabled').checked ? 1 : 0;
            const timeFormat12 = document.getElementById('timeFormat12').checked ? 1 : 0;
            const permanentMute = document.getElementById('permanentMute').checked ? 1 : 0;

            const url = `/settings?dateFormat=${dateFormat}&brightness=${brightness}&nightMode=${nightMode}&pointsEnabled=${pointsEnabled}&timeFormat12=${timeFormat12}&permanentMute=${permanentMute}`;
            log(`Sending Settings Request: ${url}`);

            fetch(url, { method: 'GET' })
                .then(res => res.text())
                .then(data => log(`Success: ${data || 'OK'}`))
                .catch(err => log(`Error: ${err.message}`));
        }
        
        // Send custom notice function (/notice endpoint)
        function sendNotice() {
            const text = encodeURIComponent(document.getElementById('noticeText').value);
            const anim = document.getElementById('noticeAnim').value;
            const sound = document.getElementById('noticeSound').value;
            const color = document.getElementById('noticeColor').value;
            const mod = document.getElementById('noticeMod').value;
            const icon = document.getElementById('noticeIcon').value;

            const url = `/notice?text=${text}&anim=${anim}&sound=${sound}&color=${color}&mod=${mod}&icon=${icon}`;
            log(`Sending Notice Request: ${url}`);
            
            fetch(url, { method: 'GET' })
                .then(res => res.text())
                .then(data => log(`Success: ${data || 'OK'}`))
                .catch(err => log(`Error: ${err.message}`));
        }

        // Set timer function (/timer endpoint)
        function setTimer() {
            const sec = document.getElementById('timerSec').value;

            const url = `/timer?duration=${sec}`;
            log(`Sending Timer Set Request: ${url}`);

            fetch(url, { method: 'GET' })
                .then(res => res.text())
                .then(data => log(`Success: ${data || 'OK'}`))
                .catch(err => log(`Error: ${err.message}`));
        }

        // Set alarm function (/alarm endpoint)
        function setAlarm() {
            const alarmHour = document.getElementById('Hour').value;
            const alarmMin = document.getElementById('Min').value;

            const url = `/alarm?alarmHour=${alarmHour}&alarmMin=${alarmMin}`;
            log(`Sending Alarm Set Request: ${url}`);

            fetch(url, { method: 'GET' })
                .then(res => res.text())
                .then(data => log(`Success: ${data || 'OK'}`))
                .catch(err => log(`Error: ${err.message}`));
        }

        // Send quick command function
        function sendQuick(endpoint) {
            const url = `/${endpoint}`;
            log(`Sending Quick Command: ${url}`);

            fetch(url, { method: 'GET' })
                .then(res => res.text())
                .then(data => log(`Success: ${data || 'OK'}`))
                .catch(err => log(`Error: ${err.message}`));
        }

    </script>
</body>
</html>
)rawliteral";

// ----------------------------------------------------------------------------
// Sends a standard HTTP response with CORS headers and specified content type.
// ----------------------------------------------------------------------------
void sendResponse(int status, const String& message, const String& contentType) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(status, contentType, message);
}

// ----------------------------------------------------------------------------
// Handle Root URL HTTP GET Requests
// ----------------------------------------------------------------------------
void handleRoot() {
    String html = FPSTR(HTML_INDEX);
    String currentIp = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "glance-clock.local";
    html.replace("%IP_ADDRESS%", currentIp);
    server.send(200, "text/html", html);
}

// ----------------------------------------------------------------------------
// Initializes and registers all HTTP REST API endpoints and web server handlers.
// ----------------------------------------------------------------------------
void setupHttpServer() {
    server.on("/", HTTP_GET, handleRoot);
    
    // MQTT configuration reception: http://glance-clock.local/mqtt?server=192.168.1.100&port=1883
    server.on("/mqtt", HTTP_GET, []() {
        if (server.hasArg("server")) {
            String mqttServerAddr = server.arg("server");
            int mqttPortNum = server.hasArg("port") ? server.arg("port").toInt() : 1883;

            // Save MQTT server configuration to NVS preferences
            preferences.begin("mqtt-config", false);
            preferences.putString("server", mqttServerAddr);
            preferences.putInt("port", mqttPortNum);
            preferences.end();

            // Re-initialize MQTT configuration immediately
            setupMQTT();

            sendResponse(200, "OK: MQTT server saved -> " + mqttServerAddr);
            logMessage("MQTT Saved: " + mqttServerAddr + ":" + String(mqttPortNum), "MQTT Saved");
        } else {
            handleErrorResponse(400, "Error: Missing server parameter");
        }
    });

    // PIN code reception: http://glance-clock.local/pin?code=123456
    server.on("/pin", HTTP_GET, []() {
        if (server.hasArg("code")) {
            String pinCode = server.arg("code");
            
            xSemaphoreTake(systemMutex, portMAX_DELAY);
            webPinCode = pinCode;
            hasWebPin = true;
            xSemaphoreGive(systemMutex);
            
            sendResponse(200, "OK: PIN received -> " + pinCode);
            logMessage("PIN Recv: " + pinCode, "PIN Recv: " + pinCode);
        } else {
            handleErrorResponse(400, "Error: Missing code parameter");
        }
    });

    // Time sync: http://glance-clock.local/sync
    server.on("/sync", HTTP_GET, []() {
        if (!sharedData.isNtpSynced()) {
            handleErrorResponse(500, "Error: NTP sync not completed yet");
            return;
        }
        notifyCTSTime();
        
        uint8_t refreshCmd[] = {CMD_UPDATE_REFRESH, 0x00, 0x00, 0x00};
        sendGlanceCommand(refreshCmd, sizeof(refreshCmd));
        
        sendResponse(200, "OK: Time synced");
        logMessage("Sync: Time Synced");
    });

    // Update device settings: http://glance-clock.local/settings?pointsEnabled=1
    server.on("/settings", HTTP_GET, []() {
        DeviceSettings currentSettings = sharedData.getSettings();
        if (server.hasArg("nightMode")) currentSettings.nightMode = (server.arg("nightMode") == "1");
        if (server.hasArg("permanentDND")) currentSettings.permanentDND = (server.arg("permanentDND") == "1");
        if (server.hasArg("permanentMute")) currentSettings.permanentMute = (server.arg("permanentMute") == "1");
        if (server.hasArg("dateFormat")) currentSettings.dateFormat = server.arg("dateFormat").toInt();
        if (server.hasArg("pointsEnabled")) currentSettings.pointsEnabled = (server.arg("pointsEnabled") == "1");
        if (server.hasArg("brightness")) currentSettings.brightness = server.arg("brightness").toInt();
        if (server.hasArg("timeModeEnable")) currentSettings.timeModeEnable = (server.arg("timeModeEnable") == "1");
        if (server.hasArg("timeFormat12")) currentSettings.timeFormat12 = (server.arg("timeFormat12") == "1");

        sharedData.setSettings(currentSettings);
        sendSettingsCommand();
        sendResponse(200, "OK: Settings applied");
        
        String msg = "Set: Bright=" + String(currentSettings.brightness);
        logMessage(msg, msg);
    });

    // Send notification: http://glance-clock.local/notice?text=Hello&icon=133
    server.on("/notice", HTTP_GET, []() {
        if (!sharedData.isNtpSynced()) {
            handleErrorResponse(500, "Error: NTP sync not completed yet");
            return;
        }
        String msg = server.hasArg("text") ? server.arg("text") : " ";
        int anim = server.hasArg("anim") ? server.arg("anim").toInt() : 1;
        int sound = server.hasArg("sound") ? server.arg("sound").toInt() : 5;
        int color = server.hasArg("color") ? server.arg("color").toInt() : 23;
        int mod = server.hasArg("mod") ? server.arg("mod").toInt() : 1;
        int icon = server.hasArg("icon") ? server.arg("icon").toInt() : 133;

        sendCustomNoticeCommand(msg, anim, sound, color, mod, icon);
        sendResponse(200, "OK: Notice sent -> " + msg);
    });

    // Register calendar event: http://glance-clock.local/calendar?starth=21&startmin=0&inform=12&dur=60&title=test
    server.on("/calendar", HTTP_GET, []() {
        if (!sharedData.isNtpSynced()) {
            handleErrorResponse(500, "Error: NTP sync not completed yet", "Calendar Error: NTP Not Synced");
            return;
        }
        std::vector<uint8_t> cmd(Prefix::APPOINTMENTS_SCENE, Prefix::APPOINTMENTS_SCENE + sizeof(Prefix::APPOINTMENTS_SCENE));
        cmd.push_back(8);
        cmd.push_back(1);
        
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            handleErrorResponse(500, "Error: Time not synced", "Time Not Synced");
            return;
        }

        int startHour = server.hasArg("starth") ? server.arg("starth").toInt() : 21;
        int startMin = server.hasArg("startmin") ? server.arg("startmin").toInt() : 0;
        int informMin = server.hasArg("inform") ? server.arg("inform").toInt() : 12;
        int durationMin = server.hasArg("dur") ? server.arg("dur").toInt() : 60;
        String title = server.hasArg("title") ? server.arg("title") : "Meeting";

        time_t now;
        time(&now);

        cmd.reserve(128);
        
        cmd.insert(cmd.end(), { PB_TAG(1, PbWire::VARINT) });
        encodeVarintVector(cmd, (uint64_t)now);

        cmd.insert(cmd.end(), {PB_TAG(2, PbWire::BYTES), 0x08});

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
            
            vTaskDelay(pdMS_TO_TICKS(50));
            uint8_t sceneCmd[] = {CMD_SCENE_START, 0x00, 0x00, 0x00};
            sendGlanceCommand(sceneCmd, sizeof(sceneCmd));

            sendResponse(200, "OK: Event created, refreshed & scenes started");
            String logStr = "Cal: " + title;
            logMessage(logStr, logStr);
        } else {
            handleErrorResponse(500, "Error: Failed to send via BLE", "Calendar Send Failed");
        }
    });

    server.on("/calendar_clr", HTTP_GET, []() {
        if (!sharedData.isNtpSynced()) {
            handleErrorResponse(500, "Error: NTP sync not completed yet");
            return;
        }
        std::vector<uint8_t> packet(Prefix::APPOINTMENTS_SCENE, Prefix::APPOINTMENTS_SCENE + sizeof(Prefix::APPOINTMENTS_SCENE));
        packet.push_back(0);
        packet.push_back(1);
        
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
        int16_t minValue  = server.hasArg("minValue") ? server.arg("minValue").toInt() : -30;
        String message    = server.hasArg("message") ? server.arg("message") : "\xC2\x8F\x08\xC2\xB0";

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
            
            vTaskDelay(pdMS_TO_TICKS(50));
            uint8_t sceneCmd[] = {CMD_SCENE_START, 0x00, 0x00, 0x00};
            sendGlanceCommand(sceneCmd, sizeof(sceneCmd));
            
            sendResponse(200, "OK: Ring command sent -> " + message);
            M5.Display.printf("Ring Sent (%u bytes)\n", (uint32_t)packet.size());
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
            vTaskDelay(pdMS_TO_TICKS(50));

            sendResponse(200, "{\"status\":\"success\",\"duration\":"  + String(duration) + "}", "application/json");
            M5.Display.printf("Timer Set: %02d sec\n", duration);
        } else {
            handleErrorResponse(500, "Error: Failed to send via BLE", "Timer Send Failed");
        }
    });

    // Alarm setup: http://glance-clock.local/alarm?alarmHour=22&alarmMin=37&alarmDays=127&alarmSound=1&alarmText=ALARM
    server.on("/alarm", HTTP_GET, []() {
        int days    = server.hasArg("alarmDays") ? server.arg("alarmDays").toInt() : 127; 
        int hour    = server.hasArg("alarmHour") ? server.arg("alarmHour").toInt() : 22;
        int minutes = server.hasArg("alarmMin") ? server.arg("alarmMin").toInt() : 0;
        int sound   = server.hasArg("alarmSound") ? server.arg("alarmSound").toInt() : 1; 
        String message = server.hasArg("alarmText") ? server.arg("alarmText") : "ALARM";

        std::vector<uint8_t> packet = buildAlarmPacket(days, hour, minutes, sound, message);

        if (sendGlanceCommand(packet.data(), packet.size())) {
            uint8_t refreshCmd[] = {CMD_UPDATE_REFRESH, 0x00, 0x00, 0x00};
            sendGlanceCommand(refreshCmd, sizeof(refreshCmd));
            
            vTaskDelay(pdMS_TO_TICKS(50));
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
            
            vTaskDelay(pdMS_TO_TICKS(50));
            uint8_t sceneCmd[] = {CMD_SCENE_START, 0x00, 0x00, 0x00};
            sendGlanceCommand(sceneCmd, sizeof(sceneCmd));

            sendResponse(200, "OK: Custom array sent");
        } else {
            handleErrorResponse(500, "Error: BLE send failed");
        }
    });

    // Unified generic quick command endpoint: http://glance-clock.local/quick?byte=31
    server.on("/quick", HTTP_GET, []() {
        if (!server.hasArg("byte")) {
            handleErrorResponse(400, "Error: Missing byte parameter");
            return;
        }

        String byteStr = server.arg("byte");
        uint8_t cmdByte = (uint8_t)strtoul(byteStr.c_str(), NULL, 0);

        String label = "";

        switch (cmdByte) {
            case CMD_TIMER_STOP:             label = "Timer Stop"; break;
            case CMD_ALARM_STOP:             label = "Alarm Stop"; break;
            case CMD_ALARM_CLEAR:            label = "Alarm Clear"; break;
            case CMD_SCENE_STOP:             label = "Scenes Stop"; break;
            case CMD_SCENE_START:            label = "Scenes Start"; break;
            case CMD_SCENE_CLEAR:            label = "Scenes Clear"; break;
            case CMD_UPDATE_REFRESH:         label = "Update & Refresh"; break;
            case CMD_AUTO_NIGHT_MODE_EN:     label = "Enable Auto Night Mode"; break;
            case CMD_AUTO_NIGHT_MODE_DIS:    label = "Disable Auto Night Mode"; break;
            case CMD_CLEAR_BONDS:            label = "Bonds Clear"; break;
            case CMD_CALIBRATION_START:      label = "Start Calibration"; break;
            case CMD_CALIBRATION_CONFIRM:    label = "Confirm Calibration"; break;
            case CMD_ALARM_WITH_NOTES:       label = "Alarm with Notes"; break;
            case CMD_CLEAR_USER_INFO:        label = "Clear User Info (Factory Reset)"; break;
            case CMD_BRIGHTNESS_SCENE_STOP:  label = "Brightness Scene Stop"; break;
            case CMD_BRIGHTNESS_SCENE_START: label = "Brightness Scene Start"; break;
            case CMD_DSP_STATE_SHOW:         label = "DSP State Show (Serial)"; break;
            default:                         label = "Unknown (0x" + String(cmdByte, HEX) + ")"; break;
        }

        uint8_t cmd[] = {cmdByte, 0x00, 0x00, 0x00};
        
        if (sendGlanceCommand(cmd, sizeof(cmd))) {
            sendResponse(200, "OK: " + label);
            M5.Display.printf("Quick: %s\n", label.c_str());
        } else {
            handleErrorResponse(500, "Error: BLE send failed", label + " Failed");
        }
    });

    server.begin();
    logMessage("HTTP server started.");
}
