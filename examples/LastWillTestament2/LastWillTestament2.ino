#include "MqttDevice.hpp"

const char* SSID = "SSID";
const char* PASSWORD = "PASSWORD";
const char* MQTT_BROKER = "test.mosquitto.org";
const int PORT = 1883;
const char* MQTT_USER = nullptr;
const char* MQTT_PASSWORD = nullptr;


MqttDevice device("himbeere");

// ---------------------- Handlers ----------------------
void on_LastWill(const char* topic, const char* payload)
{
  Serial.println("Mudança de estado detectada...");
  Serial.print("[schwarz]: "); Serial.println(payload);
}

// ---------------------- Setup ----------------------

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("A ouvir se alguem se desconectou...");

  // Nota o keep alive default é 15 segundos portanto esta func so vai ser corrida cerca de +/- 15s depois do schwarz se desconectar
  // o keep alive é uma mensagem que mandamos automaticamente de x em x tempo para dizermos ao broker que estamos vivos
  // se nao for enviado então é porque perdemos a conexão com o broker
  device.on("devices/schwarz/status", on_LastWill);

  // Inicializa a MqttDevice, passando os parâmetros da Last Will no final
  device.begin(SSID, PASSWORD, MQTT_BROKER, PORT, MQTT_USER, MQTT_PASSWORD);
}

void loop()
{
  device.loop();
}
