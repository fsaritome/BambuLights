#ifndef MQTT_BROKER_H
#define MQTT_BROKER_H
#include <ConfigItem.h>
#include <espMqttClient.h>
#include <ArduinoJson.h>
#include <map>
#include <set>

// Optional, gitignored. Lets you bake your own printer details into a build
// without them ever reaching the repository. See include/local_secrets.h.example.
#if __has_include("local_secrets.h")
#include "local_secrets.h"
#endif

#ifndef MQTT_HOST_DEFAULT
#define MQTT_HOST_DEFAULT ""
#endif
#ifndef MQTT_PASSWORD_DEFAULT
#define MQTT_PASSWORD_DEFAULT ""
#endif
#ifndef MQTT_SERIAL_DEFAULT
#define MQTT_SERIAL_DEFAULT ""
#endif

class MQTTBroker
{
public:
    MQTTBroker();

    enum State { disconnected, idle, printing, no_lights, error, warning };

    // Left blank on purpose: never commit printer credentials. Set these once
    // in the web UI and they persist in NVS across reflashes (only a full
    // `pio run -t erase` clears them).
    //
    // If you want a freshly erased board to come up pre-configured, put your
    // values in include/local_secrets.h, which is gitignored:
    //
    //     #define MQTT_HOST_DEFAULT   "192.168.1.50"
    //     #define MQTT_PASSWORD_DEFAULT "your-access-code"
    //     #define MQTT_SERIAL_DEFAULT "01P00A1234567890"
    static StringConfigItem& getHost() { static StringConfigItem mqtt_host("mqtt_host", 25, MQTT_HOST_DEFAULT); return mqtt_host; }
    static IntConfigItem& getPort() { static IntConfigItem mqtt_port("mqtt_port", 8883); return mqtt_port; }
    static StringConfigItem& getUser() { static StringConfigItem mqtt_user("mqtt_user", 25, "bblp"); return mqtt_user; }
    static StringConfigItem& getPassword() { static StringConfigItem mqtt_password("mqtt_password", 25, MQTT_PASSWORD_DEFAULT); return mqtt_password; }
    static StringConfigItem& getSerialNumber() { static StringConfigItem mqtt_serialnumber("mqtt_serialnumber", 25, MQTT_SERIAL_DEFAULT); return mqtt_serialnumber; }

    void setStateChangedCallback(std::function<void(MQTTBroker *)> callback);
    bool init(const String& id);
    void connect();
    void checkConnection();
    bool isConnected() { return connected; }
    bool isDoorOpen() { return doorOpen; }
    bool isLightOn() { return lightOn; }
    State getState() { return state; }
    const char* getStateName() const;
    // Text of the most severe active HMS fault, or "" when nothing is raised.
    const String& getHmsMessage() const { return hmsMessage; }
    void setChamberLight(bool on);

private:
    void onConnect(bool sessionPresent);
    void onDisconnect(espMqttClientTypes::DisconnectReason reason);
    void onMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t*  payload, size_t length, size_t index, size_t total_length);
    void onCompleteMessage(const espMqttClientTypes::MessageProperties& properties, const char* topic, const uint8_t* payload, size_t length);
    void handleMQTTMessage(JsonDocument &jsonMsg);
    static String describeHMS(uint64_t value);

    String id;
    JsonDocument filter;
    char deviceTopic[64];
    char reportTopic[64];
    char requestTopic[64];

    bool reconnect = false;
    bool connected = false;
    State state = disconnected;
    bool doorOpen;
    bool lightOn = true;
    String hmsMessage;

    uint32_t lastReconnect = 0;

    espMqttClientSecure client;

    std::function<void(MQTTBroker*)> stateChangedCallback = [](MQTTBroker*) {};

    static std::map<int, std::string> CURRENT_STAGE_IDS;
    static std::map<uint64_t, std::string> HMS_ERRORS;
    static std::map<int, std::string> HMS_SEVERITY_LEVELS;
    
    static std::set<int> ERROR_STAGES;
    static std::set<int> CAMERA_OFF_STAGES;
    static std::set<int> IDLE_STAGES;

    static std::set<int> PRINT_WARNINGS;

    // HMS faults that stay raised indefinitely and say nothing about the print.
    // Without this the lights would sit on the warning colour forever.
    static std::set<uint64_t> HMS_IGNORED;
};
#endif