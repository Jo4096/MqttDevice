#include "MqttDevice.hpp"
#include <ArduinoJson.h>

const char* SSID = "SSID";
const char* PASSWORD = "PASS";
const char* MQTT_BROKER = "test.mosquitto.org";
const int PORT = 1883;
const char* MQTT_USER = nullptr;
const char* MQTT_PASSWORD = nullptr;


MqttDevice device("himbeere");

//------------------------------------- Handlers -------------------------------------//

// A função de callback agora aceita um JsonDocument
void sensor_json_handler(const char* topic, const JsonDocument& doc)
{
  int sensor_value = doc["value"] | -999;
  if(sensor_value == -999)
  {
    Serial.println("Algo conteceu com o sensor...");
    return;
  }
  
  const int threshold = 500;

  Serial.printf("Valor JSON lido: %d\n", sensor_value);

  JsonDocument cmd_doc; // Documento para o comando LED
  JsonDocument msg_doc; // Documento para a mensagem

  if (sensor_value > threshold)
  {
    cmd_doc["command"] = "ON";
    msg_doc["message"] = "Valor do sensor superior a threshold";
  }
  else
  {
    cmd_doc["command"] = "OFF";
    msg_doc["message"] = "Valor do sensor inferior a threshold";
  }

  device.publishJson("devices/schwarz/led/cmd", cmd_doc);
  device.publishJson("devices/schwarz/msg", msg_doc); 
}

//------------------------------------------------------------------------------------//

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("A começar");

  device.onJson("devices/schwarz/sensor/data", sensor_json_handler); 
  device.begin(SSID, PASSWORD, MQTT_BROKER, PORT, MQTT_USER, MQTT_PASSWORD);
}

void loop()
{
  device.loop();
}
