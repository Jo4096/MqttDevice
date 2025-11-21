#include "MqttDevice.hpp"
#include <ArduinoJson.h>

const char* SSID = "SSID";
const char* PASSWORD = "PASS";
const char* MQTT_BROKER = "test.mosquitto.org";
const int PORT = 1883;
const char* MQTT_USER = nullptr;
const char* MQTT_PASSWORD = nullptr;


MqttDevice device("schwarz");

// ---------------------- Callback de Conexão ----------------------
void on_connection_change(bool connected)
{
    if (connected)
    {
        Serial.println("Conexão MQTT estabelecida. O MqttDevice já publica 'online' com Retain.");
    }
    else
    {
        Serial.println("Conexão MQTT perdida ou desconectada.");
    }
}

// ---------------------- Setup ----------------------

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("A começar e a configurar Last Will.");

    // Configura a função de callback de conexão
    device.onConnectionChange(on_connection_change);

    // --- Configuração da Last Will ---
    const char* WILL_TOPIC = "devices/schwarz/status";
    const char* WILL_PAYLOAD = "offline"; // Quando o 'schwarz' desconecta, o broker envia "offline" 
    const bool WILL_RETAIN = true;        // A mensagem é permanente 
    const int WILL_QOS = 1;               // Garante a entrega ao broker

    // Inicializa a MqttDevice, passando os parâmetros da Last Will
    device.begin(SSID, PASSWORD, MQTT_BROKER, PORT, 
                 MQTT_USER, MQTT_PASSWORD, 
                 WILL_TOPIC, WILL_PAYLOAD, WILL_RETAIN, WILL_QOS);
}


unsigned long lastTime = 0;
void loop()
{
    device.loop();

    unsigned long now = millis();
    if (now - lastTime >= 10000)
    {
        Serial.println("A manter a ligação...\nDesconecta-me e vê o que acontece com a Himbeere");
        lastTime = now;
    }
}