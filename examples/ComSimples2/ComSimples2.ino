#include "MqttDevice.hpp"

const char* SSID = "SSID";
const char* PASSWORD = "PASS";
const char* MQTT_BROKER = "test.mosquitto.org";
const int PORT = 1883;
const char* MQTT_USER = nullptr;
const char* MQTT_PASSWORD = nullptr;


MqttDevice device("himbeere");

//------------------------------------- Handlers -------------------------------------//
void sensor_handler(const char* topic, const char* payload)
{
  if(!payload)
  {
    Serial.println("Algo de errado aconteceu com o sensor...");
    return;
  }

  //converter de string para int
  int sensor_value = atoi(payload);
  const int threshold = 500;

  Serial.printf("Valor lido: %d\n", sensor_value);

  // Se o valor lido for superior a um threshold definido envia "ON" para o led/cmd e envia uma mensagem para ser printada no ecrã
  if(sensor_value > threshold)
  {
    device.publish("devices/schwarz/led/cmd", "ON");
    device.publish("devices/schwarz/msg", "Valor do sensor superior a threshold");
  }
  else
  {
    device.publish("devices/schwarz/led/cmd", "OFF");
    device.publish("devices/schwarz/msg", "Valor do sensor inferior a threshold");
  }
}

//------------------------------------------------------------------------------------//

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("A começar");

  //esta função vai ser corrida quando o schwarz enviar os valores do sensor no sensor/data
  device.on("devices/schwarz/sensor/data", sensor_handler);
  device.begin(SSID, PASSWORD, MQTT_BROKER, PORT, MQTT_USER, MQTT_PASSWORD);
}


void loop()
{
  device.loop();
}
