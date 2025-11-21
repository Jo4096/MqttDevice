# MqttDevice
Uma biblioteca wrapper robusta e de alto nível para ESP32, ESP8266 e RP2040, construída sobre a **`PubSubClient`** e **`ArduinoJson`**.

A **`MqttDevice`** abstrai a gestão complexa de conexões WiFi/MQTT e implementa padrões avançados de comunicação IoT, como **Request/Response (RPC)**, serialização automática de JSON/MessagePack e gestão de timeouts.


# Funcionalidades Principais
- Gestão Automática de Conexão: Lida com reconexões WiFi e MQTT transparentemente.
- Padrão Request/Response (RPC): Implementação completa de chamadas síncronas/assíncronas. Permite que um dispositivo "peça" algo a outro e aguarde a resposta com timeout (sem bloquear o código).
- Topologia Organizada: Gere automaticamente tópicos com o prefixo devices/{device_id}/.
- Callbacks de Estado: Notificações de eventos de conexão/desconexão para feedback visual (LEDs, UI).
- Last Will & Testament: Suporte integrado para detetar dispositivos offline.

# Dependências
Esta biblioteca depende de duas bibliotecas padrão do ecossistema Arduino:
- PubSubClient (Nick O'Leary)
- ArduinoJson (Benoit Blanchon) - Recomendado v7.x

# Instalação
1) Inicie o download do código no formato **.ZIP** clicando no botão verde que diz "code"
2) No Arduino IDE vá a ***`Sketch` `>` `Include Library` `>` `Add .ZIP Library`***
3) Selecione o ficheiro **.ZIP** que instalou
4) Reiniciar o IDE *(just to make sure)*
