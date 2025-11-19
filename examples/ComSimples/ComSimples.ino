#include "MqttDevice.hpp"



const char* SSID = "SSID";

const char* PASSWORD = "PASSWORD";



const int PORT = 1883;



const char* MQTT_BROKER = "test.mosquitto.org";



const char* MQTT_USER = nullptr;

const char* MQTT_PASSWORD = nullptr;



#define LED_PIN 2



MqttDevice device("schwarz");





void writeRandomValue()

{

  int value = random() % 1000;

  String str = String(value);

  Serial.println("A escrever dados do sensor em sensor/data : "+str);

  device.publishSubTopic("sensor/data", str.c_str());

}



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



void setup()

{

  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);

  delay(1000);

  Serial.println("A começar");



  device.on("msg", msg_handler);

  device.on("led/cmd", led_handler);

  device.on("sensor/data", [](const char* topic, const char* payload)

  {

    Serial.println("Mensagem recebida no sensor data: " + String(payload));

  });

//para confirmar que a mensagem foi escrita. Como o topico fornecido nao comeca por devices/ signfica que ele vai ouvir os seus topicos

// se um dispositivo quer ouvir a mensagem enviada por este dispositivo ele tem de fazer: .on("devices/schwarz/sensor/data", ...)

// ou se sabes que os dispositivos tem sensor e queres ouvir todos os topicos sensor/data fazes .on("devices/+/sensor/data", ...) para ser generico



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