#include "MqttDevice.hpp"
#include <ArduinoJson.h>

const char* SSID = "SSID";
const char* PASSWORD = "PASSWORD";
const char* MQTT_BROKER = "test.mosquitto.org";
const int PORT = 1883;
const char* MQTT_USER = nullptr;
const char* MQTT_PASSWORD = nullptr;

#define LED_PIN 2
MqttDevice device("schwarz");

// Função que vai correr periodicamente para escrever valores no subtópico "sensor/data"
void writeRandomValue()
{
  int value = random(1000);
  JsonDocument doc;
  doc["value"] = value;

  Serial.print("A escrever dados do sensor JSON em sensor/data : ");

  serializeJson(doc, Serial);
  Serial.println();
  device.publishJsonSubTopic("sensor/data", doc);
}

//------------------------------------- Handlers -------------------------------------//


void msg_json_handler(const char* topic, const JsonDocument& doc)
{
  String message = doc["text"] | "";

  if (message.length() == 0)
  {
    message = doc["message"] | "";
  }

  if (message.length() > 0)
  {
    Serial.print("MSG JSON recebida: ");
    Serial.println(message);
  }
  else
  {
    Serial.println("MSG JSON recebida, mas o campo 'text' ou 'message' não foi encontrado/inválido.");
  }
}




// Handler para o subtópico "msg", esperando o campo JSON "text"
void led_json_handler(const char* topic, const JsonDocument& doc)
{
  bool ledState = false;
  String command = doc["command"] | ""; 
    
  if (command.length() > 0)
  {
    if (command.equalsIgnoreCase("ON"))
    {
      ledState = true;
      Serial.println("Comando LED: ON");
    }
    else if (command.equalsIgnoreCase("OFF"))
    {
      ledState = false;
      Serial.println("Comando LED: OFF");
    }
    else
    {
      Serial.printf("Comando LED desconhecido: %s\n", command.c_str());
    }
  }
  else
  {
    Serial.println("Comando LED JSON inválido: campo 'command' em falta ou formato errado.");
  }

  digitalWrite(LED_PIN, ledState);
}

//------------------------------------------------------------------------------------//

void setup()
{
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    delay(1000);
    Serial.println("A começar");

    device.onJson("msg", msg_json_handler);
    device.onJson("led/cmd", led_json_handler);

    device.begin(SSID, PASSWORD, MQTT_BROKER, PORT, MQTT_USER, MQTT_PASSWORD);
}


unsigned long lastTime = 0;
void loop()
{
  device.loop();

  unsigned long now = millis();
  
  if(now - lastTime >= 5000)
  {
    writeRandomValue();
    lastTime = now;
  }
}