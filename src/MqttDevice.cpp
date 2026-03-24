#include "MqttDevice.hpp"
#include <stdarg.h>

MqttDevice *MqttDevice::instance = nullptr;

MqttDevice::MqttDevice() : MqttDevice("") {}

MqttDevice::MqttDevice(String id)
{
  if (id == "")
    this->device_id = generateUniqueId();
  else
    this->device_id = id;

  this->lastReconnectAttempt = 0;
  this->lastWiFiAttempt = 0;
  this->lastMQTTActivity = 0;
  this->wifiRetryDelay = wifiRetryBase;
  this->mqttRetryDelay = mqttRetryBase;
  this->_initialized = false;
  instance = this;
}

// ---------------------- Utilities ----------------------
void MqttDevice::log(const char *fmt, ...)
{
  if (!debug)
    return;
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print("[MqttDevice] ");
  Serial.println(buf);
}

String MqttDevice::generateUniqueId()
{
  String uid = "dev_";
#if defined(ESP32)
  uint64_t mac = ESP.getEfuseMac();
  // Usar os 4 bytes mais baixos (8 caracteres HEX) para um ID conciso
  uid += String((uint32_t)mac, HEX);
#elif defined(ESP8266)
  uid += String(ESP.getChipId(), HEX);
#elif defined(ARDUINO_ARCH_RP2040)
  uint8_t macArr[6];
  WiFi.macAddress(macArr);
  for (int i = 3; i < 6; ++i)
  {
    if (macArr[i] < 16)
      uid += "0";
    uid += String(macArr[i], HEX);
  }
  if (uid == "dev_000000")
  {
    uid = "dev_" + String(random(0xffff), HEX);
  }
#else
  uid += "unknown";
#endif
  return uid;
}

// ---------------------- Begin & WiFi ----------------------
void MqttDevice::setDebug(bool enable) { debug = enable; }

void MqttDevice::begin(const char *wifi_ssid, const char *wifi_pass,
                       const char *mqtt_host, int mqtt_port,
                       const char *mqtt_user, const char *mqtt_pass,
                       const char *willTopic, const char *willPayload, bool willRetain, int willQos, int mqttKeepAliveSeconds)
{
  if (_initialized) 
  {
    log("Reconfiguring MqttDevice... Scheduling reset.");
    _needsReset = true;
  } 
  else 
  {
    _needsReset = true;
  }

  _ssid = wifi_ssid;
  _pass = wifi_pass;
  _mqttHost = mqtt_host;
  _mqttPort = mqtt_port;
  this->mqttKeepAliveSeconds = mqttKeepAliveSeconds;

  if (mqtt_user) _mqttUser = mqtt_user;
  if (mqtt_pass) _mqttPass = mqtt_pass;

  if (willTopic)
  {
    lastWillTopic = String(willTopic);
    lastWillPayload = willPayload ? String(willPayload) : String("");
    lastWillRetain = willRetain;
    lastWillQos = willQos;
  }

  lastReconnectAttempt = 0;
  lastWiFiAttempt = 0;
  lastMQTTActivity = 0;
  wifiRetryDelay = 0;
  mqttRetryDelay = mqttRetryBase;

  WiFi.mode(WIFI_STA);
#if defined(ESP32)
  WiFi.setAutoReconnect(true);
#endif

  client.setClient(wifi);
  client.setServer(_mqttHost.c_str(), _mqttPort);
  client.setCallback(MqttDevice::globalMqttCallback);
  client.setBufferSize(4096);
  client.setKeepAlive(mqttKeepAliveSeconds);

#if defined(ESP8266)
  wifi.setTimeout(2000);
#endif

  _initialized = true;
  log("Begin (re)configured: wifi SSID='%s', mqtt='%s:%d'", _ssid.c_str(), _mqttHost.c_str(), _mqttPort);
}

String MqttDevice::getDeviceId()
{
  return device_id;
}

// ---------------------- WiFi ----------------------
/*
bool MqttDevice::wifiConnectOnce()
{
  log("Starting WiFi connect to '%s' ...", _ssid.c_str());

#if defined(ESP8266) || defined(ESP32)
  WiFi.disconnect(true, true);
#else
  WiFi.disconnect(true);
#endif

  delay(200);
  WiFi.begin(_ssid.c_str(), _pass.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000)
  {
    delay(200);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    log("WiFi connected: %s", WiFi.localIP().toString().c_str());
    return true;
  }
  else
  {
    log("WiFi not connected after timeout");
    return false;
  }
}
*/


bool MqttDevice::ensureWiFi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return true;
  }

  unsigned long now = millis();
  if (now - lastWiFiAttempt < wifiRetryDelay)
  {
    return false;
  }

  lastWiFiAttempt = now;
  log("ensureWiFi: trying connect/reconnect (delay %lu ms)", wifiRetryDelay);

#if defined(ESP32) || defined(ESP8266)
  if (WiFi.status() == WL_DISCONNECTED)
  {
    WiFi.reconnect();
  }
  else
  {
    WiFi.disconnect();
    WiFi.begin(_ssid.c_str(), _pass.c_str());
  }
#else
  WiFi.disconnect(true);
  WiFi.begin(_ssid.c_str(), _pass.c_str());
#endif

  if (wifiRetryDelay == 0) wifiRetryDelay = wifiRetryBase;
  else wifiRetryDelay = min(maxBackoff, wifiRetryDelay * 2);

  return false;
}

// ---------------------- MQTT helpers ----------------------
void MqttDevice::globalMqttCallback(char *topic, byte *payload, unsigned int length)
{
  if (instance)
  {
    instance->handleMessage(topic, payload, length);
  }
}

void MqttDevice::handleMessage(const char *topic, byte *payload, unsigned int length)
{
  lastMQTTActivity = millis();
  String strTopic = String(topic);
  bool messageHandled = false;

  // 1. Callbacks Binárias (Prioridade Máxima)
  for (auto &cb : binaryCallbacks)
  {
    if (strTopic == cb.topic)
    {
      cb.fn(topic, payload, length);
      messageHandled = true;
    }
  }

  // Se a mensagem foi tratada como binária, não continua para texto ou JSON.
  if (messageHandled)
  {
    return;
  }

  // O buffer para Callbacks de Texto permanece para compatibilidade
  char buf[length + 1];
  memcpy(buf, payload, length);
  buf[length] = '\0';

  // 2. Callbacks de Texto
  for (auto &cb : textCallbacks)
  {
    if (strTopic == cb.topic)
    {
      cb.fn(topic, buf);
      // Não definir messageHandled = true aqui, pois JSON e Texto podem ser subscritos no mesmo tópico
      // para suportar fallback (embora idealmente o utilizador não o faça).
    }
  }

  // 3. Callbacks JSON (incluindo Request/Response)
  bool anyJson = false;
  for (auto &cb : jsonCallbacks)
  {
    if (strTopic == cb.topic)
    {
      anyJson = true;
      break;
    }
  }

  if (anyJson)
  {
    JsonDocument doc;
    DeserializationError err = deserializeMsgPack(doc, payload, length);

    if (err)
    {
      log("MessagePack parse error on topic %s (%s). Trying JSON (text) fallback.", topic, err.c_str());
      err = deserializeJson(doc, buf);
    }

    if (!err)
    {
      for (auto it = jsonCallbacks.begin(); it != jsonCallbacks.end();)
      {
        if (strTopic == it->topic)
        {
          it->fn(topic, doc);

          bool isPendingResponse = false;
          for (auto &req : pendingRequests)
          {
            if (strTopic == req.responseTopic)
            {
              req.completed = true;
              isPendingResponse = true;
              break;
            }
          }

          if (isPendingResponse)
          {
            log("Response received on %s. Callback removed.", strTopic.c_str());
            it = jsonCallbacks.erase(it); // Remove a callback DINÂMICA
          }
          else
          {
            ++it; // Callback estática (onJson) não é removida
          }
        }
        else
        {
          ++it;
        }
      }
    }
    else
    {
      log("Final JSON parse error on topic %s (%s) after trying MessagePack and JSON text.", topic, err.c_str());
    }
  }
}

// ---------------------- Gestão de Conexão ----------------------

bool MqttDevice::reconnect()
{
  if (!ensureWiFi())
  {
    log("reconnect: no WiFi");
    return false;
  }

  unsigned long now = millis();
  if (now - lastReconnectAttempt < mqttRetryDelay)
  {
    return false;
  }

  lastReconnectAttempt = now;

  String clientId = "MQTT_" + device_id;
  String statusTopic = "devices/" + device_id + "/status";

  bool ok = false;
  log("Trying MQTT connect as '%s' ...", clientId.c_str());

  // Lógica de conexão (mantida)
  if (_mqttUser.length() || _mqttPass.length())
  {
    if (lastWillTopic.length())
    {
      ok = client.connect(clientId.c_str(), _mqttUser.c_str(), _mqttPass.c_str(),
                          lastWillTopic.c_str(), lastWillQos, lastWillRetain,
                          lastWillPayload.c_str());
    }
    else
    {
      ok = client.connect(clientId.c_str(), _mqttUser.c_str(), _mqttPass.c_str(),
                          statusTopic.c_str(), 0, true, "offline");
    }
  }
  else
  {
    if (lastWillTopic.length())
    {
      ok = client.connect(clientId.c_str(), nullptr, nullptr,
                          lastWillTopic.c_str(), lastWillQos, lastWillRetain,
                          lastWillPayload.c_str());
    }
    else
    {
      ok = client.connect(clientId.c_str(), nullptr, nullptr, statusTopic.c_str(), 0, true, "offline");
    }
  }

  if (ok)
  {
    log("MQTT connected!");
    mqttRetryDelay = mqttRetryBase;
    client.publish(statusTopic.c_str(), "online", true);
    for (auto &cb : textCallbacks)
      client.subscribe(cb.topic.c_str());
    for (auto &cb : jsonCallbacks)
      client.subscribe(cb.topic.c_str());
    for (auto &cb : binaryCallbacks)
      client.subscribe(cb.topic.c_str());
    lastMQTTActivity = millis();

    if (connectionCallback)
    {
      connectionCallback(true);
    }

    return true;
  }
  else
  {
    int st = client.state();
    log("MQTT connect failed, state=%d. Backoff=%lu", st, mqttRetryDelay);
    mqttRetryDelay = min(maxBackoff, mqttRetryDelay * 2);
    return false;
  }
}

void MqttDevice::forceReconnect()
{
  log("forceReconnect: disconnecting client and forcing immediate reconnect");
  client.disconnect();

  if (connectionCallback)
  {
    connectionCallback(false);
  }

  mqttRetryDelay = mqttRetryBase;
  lastReconnectAttempt = 0;
}

void MqttDevice::onConnectionChange(MqttConnectionCallback cb)
{
  connectionCallback = cb;
}

// ---------------------- Publish / Subscribe----------------------

void MqttDevice::publish(const char *fullTopic, const char *data, bool retain)
{
  if (!client.connected())
  {
    log("publish failed: MQTT not connected");
    return;
  }

  client.publish(fullTopic, (const uint8_t *)data, strlen(data), retain);
  lastMQTTActivity = millis();
}

void MqttDevice::publishJson(const char *fullTopic, const JsonDocument &doc, bool retain)
{
  if (!client.connected())
  {
    log("publishJson failed: MQTT not connected");
    return;
  }

  const size_t MAX_PAYLOAD_SIZE = 1024;
  uint8_t buffer[MAX_PAYLOAD_SIZE];

  size_t len = serializeMsgPack(doc, buffer, MAX_PAYLOAD_SIZE);

  if (len > 0 && len <= MAX_PAYLOAD_SIZE)
  {
    client.publish(fullTopic, buffer, len, retain);
    lastMQTTActivity = millis();
  }
  else
  {
    log("Error serializing MessagePack (len=%u) in publishJson", len);
  }
}

void MqttDevice::publishBinary(const char *fullTopic, const uint8_t *data, unsigned int length, bool retain)
{
  if (!client.connected())
  {
    log("publishBinary failed: MQTT not connected");
    return;
  }

  // Usar a sobrecarga de publish do PubSubClient para arrays de bytes
  client.publish(fullTopic, data, length, retain);
  lastMQTTActivity = millis();
}

// ---------------------- 2. publishSubTopic/publishJsonSubTopic (NOVO NOME PARA O PADRÃO) ----------------------

String MqttDevice::buildFullTopic(const char *subTopic)
{
  if (strncmp(subTopic, "devices/", 8) == 0)
  {
    return String(subTopic);
  }
  return "devices/" + device_id + "/" + String(subTopic);
}

void MqttDevice::publishSubTopic(const char *subTopic, const char *data, bool retain)
{
  if (!client.connected())
  {
    log("publishSubTopic failed: MQTT not connected");
    return;
  }
  String topic = buildFullTopic(subTopic);

  client.publish(topic.c_str(), (const uint8_t *)data, strlen(data), retain);
  lastMQTTActivity = millis();
}

void MqttDevice::publishJsonSubTopic(const char *subTopic, const JsonDocument &doc, bool retain)
{
  if (!client.connected())
  {
    log("publishJsonSubTopic failed: MQTT not connected");
    return;
  }
  String topic = buildFullTopic(subTopic);

  const size_t MAX_PAYLOAD_SIZE = 1024;
  uint8_t buffer[MAX_PAYLOAD_SIZE];

  size_t len = serializeMsgPack(doc, buffer, MAX_PAYLOAD_SIZE);

  if (len > 0 && len <= MAX_PAYLOAD_SIZE)
  {
    client.publish(topic.c_str(), buffer, len, retain);
    lastMQTTActivity = millis();
  }
  else
  {
    log("Error serializing MessagePack (len=%u) in publishJsonSubTopic", len);
  }
}

void MqttDevice::publishBinarySubTopic(const char *subTopic, const uint8_t *data, unsigned int length, bool retain)
{
  if (!client.connected())
  {
    log("publishBinarySubTopic failed: MQTT not connected");
    return;
  }
  String topic = buildFullTopic(subTopic);

  // Usar a sobrecarga de publish do PubSubClient para arrays de bytes
  client.publish(topic.c_str(), data, length, retain);
  lastMQTTActivity = millis();
}

void MqttDevice::on(const char *Topic, std::function<void(const char *topic, const char *payload)> call_back, bool useDeviceIdPrefix)
{
  String topic = useDeviceIdPrefix ? buildFullTopic(Topic) : Topic;
  textCallbacks.push_back({topic, call_back});
  if (client.connected())
  {
    client.subscribe(topic.c_str());
  }
}

void MqttDevice::onJson(const char *Topic, std::function<void(const char *topic, const JsonDocument &doc)> call_back, bool useDeviceIdPrefix)
{
  String topic = useDeviceIdPrefix ? buildFullTopic(Topic) : Topic;
  jsonCallbacks.push_back({topic, call_back});
  if (client.connected())
  {
    client.subscribe(topic.c_str());
  }
}

void MqttDevice::onBinary(const char *Topic, std::function<void(const char *topic, const uint8_t *payload, unsigned int length)> call_back, bool useDeviceIdPrefix)
{
  String topic = useDeviceIdPrefix ? buildFullTopic(Topic) : Topic;
  binaryCallbacks.push_back({topic, call_back});
  if (client.connected())
  {
    client.subscribe(topic.c_str());
  }
}

// ---------------------- Request / Response (Com Timeout) ----------------------

void MqttDevice::request(const char *targetSubTopic, const JsonDocument &requestDoc,
                         std::function<void(const char *topic, const JsonDocument &doc)> response_callback,
                         unsigned long timeoutMs)
{
  if (!client.connected())
  {
    log("request failed: MQTT not connected");
    return;
  }

  // 1. Gera um ID de Pedido único
  String requestId = String(millis(), HEX);
  requestId += String(random(0xFFFF), HEX);

  // 2. Cria o subtópico de resposta
  String responseSubTopic = "response/" + requestId;
  String fullResponseTopic = buildFullTopic(responseSubTopic.c_str());

  // 3. Prepara a callback e armazena o estado do pedido
  // A callback é adicionada à lista JSON e será removida em handleMessage ou checkPendingRequests
  jsonCallbacks.push_back({fullResponseTopic, response_callback});

  // Armazenar o estado para a gestão de timeouts
  pendingRequests.push_back({requestId, fullResponseTopic, millis(), timeoutMs, false});

  if (client.connected())
    client.subscribe(fullResponseTopic.c_str());

  // 4. Clona e modifica o documento de pedido
  JsonDocument payloadDoc;
  payloadDoc.set(requestDoc);
  payloadDoc["response_topic"] = fullResponseTopic;

  // 5. Publica o pedido
  String targetTopic = buildFullTopic(targetSubTopic);

  // **********************************************
  // CORRIGIDO: Serializar para MessagePack (binário)
  // **********************************************
  const size_t MAX_REQ_PAYLOAD_SIZE = 512;
  uint8_t buffer[MAX_REQ_PAYLOAD_SIZE];

  size_t len = serializeMsgPack(payloadDoc, buffer, MAX_REQ_PAYLOAD_SIZE);

  if (len > 0 && len <= MAX_REQ_PAYLOAD_SIZE)
  {
    // Publicação Request é sempre QoS 0
    client.publish(targetTopic.c_str(), buffer, len, false);
  }
  else
  {
    log("Error serializing MessagePack (len=%u) in request", len);
  }

  log("Request sent to '%s'. Expecting response on '%s' (Timeout: %lu ms)",
      targetTopic.c_str(), fullResponseTopic.c_str(), timeoutMs);

  lastMQTTActivity = millis();
}

// ---------------------- Gestão de Pedidos Pendentes (NOVO) ----------------------
void MqttDevice::checkPendingRequests()
{
  unsigned long now = millis();
  for (auto it = pendingRequests.begin(); it != pendingRequests.end();)
  {
    bool timedOut = (now - it->timestamp > it->timeoutMs);
    // Se o pedido foi concluído (pela resposta) OU se excedeu o timeout
    if (it->completed || timedOut)
    {
      // 1. Log de timeout e limpeza de callback, se necessário
      if (timedOut && !it->completed)
      {
        log("Request timeout on topic %s. Callback removed.", it->responseTopic.c_str());

        // Se houve timeout, a callback DINÂMICA ainda está em 'jsonCallbacks' e deve ser removida.
        for (auto json_it = jsonCallbacks.begin(); json_it != jsonCallbacks.end(); ++json_it)
        {
          if (json_it->topic == it->responseTopic)
          {
            jsonCallbacks.erase(json_it);
            break;
          }
        }
      }

      if (client.connected())
      {
        client.unsubscribe(it->responseTopic.c_str());
      }
      // 3. Remover o estado do pedido (RequestState)
      it = pendingRequests.erase(it);
    }
    else
    {
      ++it;
    }
  }
}

// ---------------------- Loop ----------------------
void MqttDevice::loop()
{
  if (!_initialized) return;
  unsigned long now = millis();
  if (_needsReset) 
  {
    log("Executing scheduled disconnect/reset...");
    client.disconnect();
    #if defined(ESP8266) || defined(ESP32)
      WiFi.disconnect(true, true); 
    #else
      WiFi.disconnect(true);
    #endif
    _needsReset = false;
    
    lastWiFiAttempt = now; 
    wifiRetryDelay = 500; 
    return;
  }

  if (!ensureWiFi()) 
  {
    return;
  }

  if (!client.connected())
  {
    reconnect();
    return;
  }

  if (!client.loop())
  {
    log("MQTT client.loop() failed. Forcing reconnect.");
    if (connectionCallback)
    {
      connectionCallback(false);
    }
    client.disconnect(); 
    return;              
  }

  checkPendingRequests();

  if (millis() - lastMQTTActivity > mqttWatchdog)
  {
    log("MQTT watchdog triggered. Forcing reconnect.");
    if (connectionCallback)
    {
      connectionCallback(false);
    }
    client.disconnect();
  }
}

bool MqttDevice::unsubscribe(const char *subTopic)
{
  String fullTopic = buildFullTopic(subTopic);
  bool unsubscribed = false;

  if (client.connected())
  {
    client.unsubscribe(fullTopic.c_str());
    log("Unsubscribed from topic: %s", fullTopic.c_str());
    unsubscribed = true;
  }
  else
  {
    log("MQTT client not connected, skipping broker unsubscribe for %s.", fullTopic.c_str());
    unsubscribed = true;
  }

  for (auto it = textCallbacks.begin(); it != textCallbacks.end();)
  {
    if (it->topic == fullTopic)
    {
      it = textCallbacks.erase(it);
      log("Removed Text callback for topic: %s", fullTopic.c_str());
    }
    else
    {
      ++it;
    }
  }

  for (auto it = jsonCallbacks.begin(); it != jsonCallbacks.end();)
  {
    if (it->topic == fullTopic)
    {
      it = jsonCallbacks.erase(it);
      log("Removed JSON callback for topic: %s", fullTopic.c_str());
    }
    else
    {
      ++it;
    }
  }

  for (auto it = binaryCallbacks.begin(); it != binaryCallbacks.end();)
  {
    if (it->topic == fullTopic)
    {
      it = binaryCallbacks.erase(it);
      log("Removed Binary callback for topic: %s", fullTopic.c_str());
    }
    else
    {
      ++it;
    }
  }

  return unsubscribed;
}

bool MqttDevice::connected()
{
  return client.connected();
}


void MqttDevice::setDeviceId(String newId)
{
  if (newId == "" || newId == this->device_id) return;

  log("Changing Device ID from '%s' to '%s'", this->device_id.c_str(), newId.c_str());

  String oldPrefix = "devices/" + this->device_id + "/";
  String newPrefix = "devices/" + newId + "/";

  if (client.connected()) 
  {
    for (auto &cb : textCallbacks) client.unsubscribe(cb.topic.c_str());
    for (auto &cb : jsonCallbacks) client.unsubscribe(cb.topic.c_str());
    for (auto &cb : binaryCallbacks) client.unsubscribe(cb.topic.c_str());
    
    String oldStatusTopic = "devices/" + this->device_id + "/status";
    client.publish(oldStatusTopic.c_str(), "offline", true);
  }

  auto updateTopics = [&](String &topic) 
  {
    if (topic.startsWith(oldPrefix)) 
    {
      topic.replace(oldPrefix, newPrefix);
    }
  };

  for (auto &cb : textCallbacks)   updateTopics(cb.topic);
  for (auto &cb : jsonCallbacks)   updateTopics(cb.topic);
  for (auto &cb : binaryCallbacks) updateTopics(cb.topic);

  this->device_id = newId;

  _needsReset = true; 
}
