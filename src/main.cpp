#include <Arduino.h>
#include "ConfigManager.h"
#include "CaptivePortals.h"
#include "WiFi.h"

ConfigManager config;
Gateway gateway("OMINISENSE_GATEWAY");
RawValues monitor;

void buildPayload();
void onWiFiCredentials(String ssid, String password);
float readChannel(int channelId);

void setup()
{
  Serial.begin(115200);

  if (!config.begin())
  {
    Serial.println("ConfigManager initialization failed!");
    while (1)
      ;
  }

  if (!config.loadConfig())
  {
    Serial.println("Failed to load configuration!");
    while (1)
      ;
  }
  config.printConfig();
  String ssid = config.getWiFiSSID();
  String password = config.getWiFiPassword();
  Serial.printf("WiFi SSID: %s\n", ssid.c_str());
  Serial.printf("WiFi Password: %s\n", password.length() > 0 ? "********" : "(not set)");

  WiFi.begin(ssid.c_str(), password.c_str());
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++)
  {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWiFi connected!");
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
  }
  else
  {
    Serial.println("\nFailed to connect to WiFi. Starting Gateway...");
    gateway.begin();
  }
}
void loop()
{
  gateway.handle();
  gateway.onCredentials(onWiFiCredentials);
  monitor.handle();
  monitor.setDataCallback(BuildPayload);
}

/*
monitor.begin(false);               // Inicia em modo STA (conectado ao WiFi)
monitor.begin(true);                // Inicia em modo AP (rede própria, IP 192.168.5.1)
monitor.end();                      // Para o monitor
monitor.handle();                   // Processa requisições (chame no loop)
monitor.active();                   // Retorna true se ativo
monitor.setDataCallback(funcao);    // Registra função que gera o JSON
monitor.timeSinceLastRequest();     // Retorna tempo (ms) desde última requisição
*/
void onWiFiCredentials(String ssid, String password)
{
  Serial.println("\n📡 Credenciais recebidas do Gateway");
  Serial.println("   SSID: " + ssid);
  Serial.println("   Senha: " + String(password.length()) + " caracteres");

  // Salva as credenciais usando sua função existente
  config.setWiFiCredentials(ssid, password);
  Serial.println("✅ Credenciais salvas na configuração");

  // Desliga o Gateway
  gateway.end();
  delay(500);

  // Tenta conectar ao WiFi
  Serial.println("⏳ Conectando ao WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  // Aguarda conexão com timeout de 20 segundos
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 20000)
  {
    delay(500);
    Serial.print(".");
  }
}

String BuildPayload()
{
  std::vector<int> activeChannels = config.getActiveChannelList();
  
  JsonDocument doc;
  
  JsonArray channels = doc["channels"].to<JsonArray>();
  
  for (int channelId : activeChannels) {
    JsonObject channelObj = channels.add<JsonObject>();
    channelObj["id"] = channelId;
    channelObj["val"] = readChannel(channelId);
  }
  
  doc["timestamp"] = millis();
  
  String payload;
  serializeJson(doc, payload);
  
  return payload;
}

float readChannel(int channelId)
{
  // Simula a leitura do canal (substitua pela lógica real de leitura)
  return random(0, 1024) / 10.24; // Retorna um valor entre 0.0 e 100.0
}