#include "MqttDevice.hpp"

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
  int value = random() % 1000;
  String str = String(value);
  Serial.println("A escrever dados do sensor em sensor/data : "+str);
  device.publishSubTopic("sensor/data", str.c_str());
}

//------------------------------------- Handlers -------------------------------------//

// Os handlers/callbacks são funções que vão ser corridas quando um evento for detectado.
// No nosso caso o evento é a receção de uma mensagem num determinado tópico

void msg_handler(const char* topic, const char* payload)
{
  if(payload)
  {
    Serial.println(payload);
  }
  else
  {
    Serial.println("nullptr");
  }
}

void led_handler(const char* topic, const char* payload)
{
  bool ledState = false;
  if(payload != nullptr && strcmp(payload, "ON") == 0)
  {
    ledState = true;
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

  device.on("msg", msg_handler);
  device.on("led/cmd", led_handler);

  // como o tópico fornecido não começa por "devices/", a biblioteca assume que é um subtópico de "devices/schwarz"
  // esta lambda vai correr sempre quando o writeRandomValue() é chamada, pois writeRandomValue() escreve para este tópico
  device.on("sensor/data", [](const char* topic, const char* payload)
  {
    Serial.println("Mensagem recebida no sensor data: " + String(payload));
  });

  // se um dispositivo quer ouvir a mensagem enviada por este dispositivo ele teria de fazer: .on("devices/schwarz/sensor/data", ...);
  // se um dispositivo quer ouvir a mensagem enviada para "sensor/data" independentemente de que dispositivo for ele faria .on("devices/+/sensor/data", ...);

  device.begin(SSID, PASSWORD, MQTT_BROKER, PORT, MQTT_USER, MQTT_PASSWORD);
}



unsigned long lastTime = 0;
void loop()
{
  device.loop();

  unsigned long now = millis();
  
  //corre de 5000ms em 5000ms (5 segundos)
  if(now - lastTime >= 5000)
  {
    writeRandomValue();
    lastTime = now;
  }
}