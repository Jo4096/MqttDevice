#include "MqttDevice.hpp"

// ================= CONFIGURAÇÃO 1 (Rede A + Broker A) =================
#define WIFI_SSID_1     "SSID1"
#define WIFI_PASS_1     "PASS1"
#define BROKER_1        "broker.hivemq.com"

// ================= CONFIGURAÇÃO 2 (Rede B + Broker B) =================
#define WIFI_SSID_2     "SSID2"
#define WIFI_PASS_2     "PASS2"
#define BROKER_2        "test.mosquitto.org"

// ================= CONFIGURAÇÃO COMUM =================
#define MQTT_PORT       1883

MqttDevice device("test-switcher");

unsigned long lastSwitchTime = 0;
const unsigned long switchInterval = 10000;
bool usingConfig1 = true;
unsigned long lastPublish = 0;

void setup() 
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== INICIO DO TESTE DE TROCA DE REDE & BROKER ===");
  device.setDebug(true);
  
  device.on("cmd/switch", [](const char* topic, const char* payload)
  {
    Serial.printf("[CALLBACK] Comando recebido: %s\n", payload);
  });

  device.on("status/heartbeat", [](const char* topic, const char* msg)
  {
    Serial.printf("[CALLBACK] Ouvi o meu próprio heartbeat: %s\n", msg);
  });

  device.onConnectionChange([](bool connected) 
  {
    if (connected) 
    {
      Serial.println(">>> EVENTO: MQTT Conectado! <<<");
    } 
    else 
    {
      Serial.println(">>> EVENTO: MQTT Desconectado! <<<");
    }
  });

  Serial.printf("A iniciar na CONFIG 1 (Wifi: %s | Broker: %s)\n", WIFI_SSID_1, BROKER_1);
  device.begin(WIFI_SSID_1, WIFI_PASS_1, BROKER_1, MQTT_PORT);
  
  lastSwitchTime = millis();
}

void loop() 
{
  device.loop();

  if (millis() - lastPublish > 2000) 
  {
    lastPublish = millis();
    if (device.connected()) 
    {
      String brokerName = usingConfig1 ? "HiveMQ" : "Mosquitto";
      String ssidName = usingConfig1 ? WIFI_SSID_1 : WIFI_SSID_2;
      String msg = "SSID: " + ssidName + " | Broker: " + brokerName + " | Time: " + String(millis()/1000);
      device.publishSubTopic("status/heartbeat", msg.c_str());
      Serial.println("Heartbeat enviado.");
    }
  }

  if (millis() - lastSwitchTime > switchInterval) 
  {
    lastSwitchTime = millis();
    usingConfig1 = !usingConfig1;

    Serial.println("\n------------------------------------------------");
    Serial.println("!!! HORA DE TROCAR TUDO (WIFI & BROKER) !!!");
    
    if (usingConfig1) 
    {
      Serial.printf("A chamar device.begin() para CONFIG 1\n");
      Serial.printf(" > Wifi: %s\n > Broker: %s\n", WIFI_SSID_1, BROKER_1);
      
      device.begin(WIFI_SSID_1, WIFI_PASS_1, BROKER_1, MQTT_PORT);
      
    }
    else
    {
      Serial.printf("A chamar device.begin() para CONFIG 2\n");
      Serial.printf(" > Wifi: %s\n > Broker: %s\n", WIFI_SSID_2, BROKER_2);
      
      device.begin(WIFI_SSID_2, WIFI_PASS_2, BROKER_2, MQTT_PORT);
    }
    Serial.println("------------------------------------------------\n");
  }
}
