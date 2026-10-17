#include "mqtt_manager.h"
#include "utils.h"
#include "packet_builder.h"
#include "SharedData.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);
String mqttServer = "";
int mqttPort = 1883;

// ----------------------------------------------------------------------------
// Incoming MQTT Message Callback Handler
// ----------------------------------------------------------------------------
static void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    logMessage("[MQTT] Rx [" + String(topic) + "]: " + message);
    String topicStr = String(topic);

    // Handle settings update command: glance/cmd/settings (JSON format expected)
    if (topicStr == "glance/cmd/settings") {
        DeviceSettings currentSettings = sharedData.getSettings();
        
        if (message.indexOf("nightMode:1") >= 0) currentSettings.nightMode = true;
        if (message.indexOf("nightMode:0") >= 0) currentSettings.nightMode = false;
        if (message.indexOf("permanentDND:1") >= 0) currentSettings.permanentDND = true;
        if (message.indexOf("permanentDND:0") >= 0) currentSettings.permanentDND = false;
        if (message.indexOf("permanentMute:1") >= 0) currentSettings.permanentMute = true;
        if (message.indexOf("permanentMute:0") >= 0) currentSettings.permanentMute = false;
        if (message.indexOf("pointsEnabled:1") >= 0) currentSettings.pointsEnabled = true;
        if (message.indexOf("pointsEnabled:0") >= 0) currentSettings.pointsEnabled = false;
        if (message.indexOf("timeFormat12:1") >= 0) currentSettings.timeFormat12 = true;
        if (message.indexOf("timeFormat12:0") >= 0) currentSettings.timeFormat12 = false;

        sharedData.setSettings(currentSettings);
        sendSettingsCommand();
        logMessage("[MQTT] Settings applied via MQTT command.");
    }
    // Handle custom notice command: glance/cmd/notice
    else if (topicStr == "glance/cmd/notice") {
        if (!sharedData.isNtpSynced()) {
            logMessage("[MQTT] Notice ignored: NTP not synced yet.");
            return;
        }
        sendCustomNoticeCommand(message, 1, 5, 23, 1, 134);
    }
    // Handle timer configuration command: glance/cmd/timer
    else if (topicStr == "glance/cmd/timer") {
        int duration = message.toInt();
        if (duration <= 0) duration = 60;
        
        std::vector<uint8_t> packet = buildTimerPacket(duration, "Timer", "Time's up");
        if (sendGlanceCommand(packet.data(), packet.size())) {
            logMessage("[MQTT] Timer set for " + String(duration) + " seconds.");
        } else {
            logMessage("[MQTT] Failed to send timer command.");
        }
    }
    // Handle alarm configuration command: glance/cmd/alarm
    else if (topicStr == "glance/cmd/alarm") {
        int hour = 7;
        int minutes = 0;
        int colonIdx = message.indexOf(':');
        if (colonIdx != -1) {
            hour = message.substring(0, colonIdx).toInt();
            minutes = message.substring(colonIdx + 1).toInt();
        }

        std::vector<uint8_t> packet = buildAlarmPacket(127, hour, minutes, 1, "ALARM");
        if (sendGlanceCommand(packet.data(), packet.size())) {
            uint8_t refreshCmd[] = {CMD_UPDATE_REFRESH, 0x00, 0x00, 0x00};
            sendGlanceCommand(refreshCmd, sizeof(refreshCmd));
            vTaskDelay(pdMS_TO_TICKS(50));
            uint8_t sceneCmd[] = {CMD_SCENE_START, 0x00, 0x00, 0x00};
            sendGlanceCommand(sceneCmd, sizeof(sceneCmd));

            logMessage("[MQTT] Alarm set to " + String(hour) + ":" + String(minutes));
        } else {
            logMessage("[MQTT] Failed to send alarm command.");
        }
    }
    // Handle generic single-byte quick control commands: glance/cmd/quick
    else if (topicStr == "glance/cmd/quick") {
        uint8_t cmdByte = (uint8_t)strtoul(message.c_str(), NULL, 0);
        uint8_t cmd[] = {cmdByte, 0x00, 0x00, 0x00};
        
        if (sendGlanceCommand(cmd, sizeof(cmd))) {
            logMessage("[MQTT] Sent quick command byte: 0x" + String(cmdByte, HEX));
        } else {
            logMessage("[MQTT] Failed to send quick command byte.");
        }
    }
    // Handle ring LED display command: glance/cmd/ring
    else if (topicStr == "glance/cmd/ring") {
        String vStr = message.length() > 0 ? message : "0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0";
        
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

        std::vector<uint8_t> packet = buildRingPacket(ringValues, 1, 0xFF0000, 0x0000FF, 30, -30, "\xC2\x8F\x08\xC2\xB0");
        if (sendGlanceCommand(packet.data(), packet.size())) {
            uint8_t refreshCmd[] = {CMD_UPDATE_REFRESH, 0x00, 0x00, 0x00};
            sendGlanceCommand(refreshCmd, sizeof(refreshCmd));
            vTaskDelay(pdMS_TO_TICKS(50));
            uint8_t sceneCmd[] = {CMD_SCENE_START, 0x00, 0x00, 0x00};
            sendGlanceCommand(sceneCmd, sizeof(sceneCmd));

            logMessage("[MQTT] Ring command sent successfully.");
        } else {
            logMessage("[MQTT] Failed to send ring command.");
        }
    }
}

// ----------------------------------------------------------------------------
// Initializes MQTT Client Settings and Server Configuration
// ----------------------------------------------------------------------------
void setupMQTT() {
    preferences.begin("mqtt-config", true);
    String srv = preferences.getString("server", "");
    int prt = preferences.getInt("port", 1883);
    preferences.end();

    xSemaphoreTake(systemMutex, portMAX_DELAY);
    mqttServer = srv;
    mqttPort = prt;
    xSemaphoreGive(systemMutex);

    if (mqttServer.length() > 0) {
        mqttClient.setServer(mqttServer.c_str(), mqttPort);
        mqttClient.setCallback(mqttCallback);
        logMessage("[MQTT] Server configured: " + mqttServer + ":" + String(mqttPort));
    } else {
        logMessage("[MQTT] Server address not configured in NVS.");
    }
}

// ----------------------------------------------------------------------------
// Publishes Payload Data to Specified MQTT Topic
// ----------------------------------------------------------------------------
void publishMQTTStatus(const String& topic, const String& payload, bool retain) {
    if (mqttClient.connected()) {
        mqttClient.publish(topic.c_str(), payload.c_str(), retain);
    }
}

// ----------------------------------------------------------------------------
// Manages MQTT Connection State, Subscriptions, and Client Loop
// ----------------------------------------------------------------------------
void loopMQTT() {
    xSemaphoreTake(systemMutex, portMAX_DELAY);
    String srv = mqttServer;
    int prt = mqttPort;
    xSemaphoreGive(systemMutex);

    if (srv.length() == 0 || WiFi.status() != WL_CONNECTED) {
        return;
    }

    static unsigned long lastReconnectAttempt = 0;
    unsigned long now = millis();

    if (!mqttClient.connected()) {
        if (now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;
            logMessage("[MQTT] Attempting connection...");
            
            if (mqttClient.connect("GlanceBridgeClient", "glance/status", 0, true, "offline")) {
                logMessage("[MQTT] Connected successfully.");
                mqttClient.publish("glance/status", "online", true);
                mqttClient.subscribe("glance/cmd/settings");
                mqttClient.subscribe("glance/cmd/notice");
                mqttClient.subscribe("glance/cmd/timer");
                mqttClient.subscribe("glance/cmd/alarm");
                mqttClient.subscribe("glance/cmd/quick");
                mqttClient.subscribe("glance/cmd/ring");
            } else {
                logMessage("[MQTT] Connection failed, rc=" + String(mqttClient.state()));
            }
        }
    } else {
        mqttClient.loop();
    }
}
