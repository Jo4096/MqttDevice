#ifndef MQTT_DEVICE_HPP
#define MQTT_DEVICE_HPP

#include <Arduino.h>
#include <vector>
#include <functional>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#if defined(ESP32) || defined(ARDUINO_ARCH_RP2040)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif

// Novo Tipo de Função de Callback para Eventos de Conexão (Estado: true/false)
using MqttConnectionCallback = std::function<void(bool connected)>;

class MqttDevice
{
public:
    // Construtores
    MqttDevice();
    MqttDevice(String id);

    // Configuração inicial
    void begin(const char* wifi_ssid, const char* wifi_pass,
               const char* mqtt_host, int mqtt_port,
               const char* mqtt_user = nullptr, const char* mqtt_pass = nullptr,
               const char* willTopic = nullptr, const char* willPayload = nullptr, bool willRetain = true, int willQos = 0, int mqttKeepAliveSeconds = 15);

    String getDeviceId();

    // Métodos de Envio
    void publish(const char* fullTopic, const char* data, bool retain = false);
    void publishJson(const char* fullTopic, const JsonDocument& doc, bool retain = false);
    void publishBinary(const char* fullTopic, const uint8_t* data, unsigned int length, bool retain = false);
    
    void publishSubTopic(const char* subTopic, const char* data, bool retain = false);
    void publishJsonSubTopic(const char* subTopic, const JsonDocument& doc, bool retain = false);
    void publishBinarySubTopic(const char* subTopic, const uint8_t* data, unsigned int length, bool retain = false);
    
    // Request/Response - Com Timeout
    void request(const char* targetSubTopic, const JsonDocument& requestDoc, 
             std::function<void(const char* topic, const JsonDocument& doc)> response_callback,
             unsigned long timeoutMs = 10000); 

    // Métodos de Receção (Subscrição)
    void on(const char* subTopic, std::function<void(const char* topic, const char* payload)> call_back);
    void onJson(const char* subTopic, std::function<void(const char* topic, const JsonDocument& doc)> call_back);
    void onBinary(const char* subTopic, std::function<void(const char* topic, const uint8_t* payload, unsigned int length)> call_back);

    // Novo: Callback para Eventos de Conexão
    void onConnectionChange(MqttConnectionCallback cb);

    // Loop principal
    void loop();

    // Debug
    void setDebug(bool enable);

private:
    // Device identity
    String device_id;

    // Network credentials
    String _ssid, _pass, _mqttHost;
    int _mqttPort;
    String _mqttUser, _mqttPass;

    // WiFi & MQTT clients
    WiFiClient wifi;
    PubSubClient client;

    // Last will
    String lastWillTopic;
    String lastWillPayload;
    bool lastWillRetain = true;
    int lastWillQos = 0;

    // Timers & backoff
    unsigned long lastReconnectAttempt;
    unsigned long lastWiFiAttempt;
    unsigned long lastMQTTActivity;
    unsigned long wifiRetryDelay;
    unsigned long mqttRetryDelay;
    const unsigned long wifiRetryBase = 2000;
    const unsigned long mqttRetryBase = 2000;
    const unsigned long maxBackoff = 30000;
    const unsigned long mqttWatchdog = 60000; // Aumentado para 60s

    // KeepAlive
    uint16_t mqttKeepAliveSeconds = 15;

    // Callbacks storage
    struct TextCallback {
      String topic;
      std::function<void(const char*, const char*)> fn;
    };

    struct JsonCallback {
      String topic;
      std::function<void(const char* topic, const JsonDocument& doc)> fn;
    };

    struct RequestState {
        String requestId;
        String responseTopic;
        unsigned long timestamp; 
        unsigned long timeoutMs; 
        bool completed = false; // Se a resposta foi recebida
    };

    struct BinaryCallback {
      String topic;
      std::function<void(const char* topic, const uint8_t* payload, unsigned int length)> fn;
    };

    std::vector<TextCallback> textCallbacks;
    std::vector<JsonCallback> jsonCallbacks;
    std::vector<BinaryCallback> binaryCallbacks;
    std::vector<RequestState> pendingRequests;
    MqttConnectionCallback connectionCallback = nullptr; // Callback de conexão

    // Internals
    static MqttDevice* instance;
    static void globalMqttCallback(char* topic, byte* payload, unsigned int length);
    void handleMessage(const char* topic, byte* payload, unsigned int length);

    void checkPendingRequests(); // Novo: Para gestão de timeouts e limpeza

    bool wifiConnectOnce();
    bool ensureWiFi();
    bool reconnect();
    void forceReconnect();

    String buildFullTopic(const char* subTopic);
    String generateUniqueId();

    // debug
    bool debug = true;
    void log(const char* fmt, ...);
};

#endif