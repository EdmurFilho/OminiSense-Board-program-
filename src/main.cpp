#include <Arduino.h>
#include "ConfigManager.h"
#include "CaptivePortals.h"
#include "WiFi.h"

ConfigManager config;
Gateway gateway("OMINISENSE_GATEWAY");
RawValues monitor;

String BuildPayload();
void onWiFiCredentials(String ssid, String password);
float readChannel(int channelId);
void SetupWifi(void *parameter);
void GetPayload(void *parameter);
String CheckMonitorQueue();

TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t payloadTaskHandle = NULL;
 
JsonDocument doc1;

void setup()
{
  xTaskCreatePinnedToCore(SetupWifi, "WiFiSetupTask", 8192, NULL, 1, &wifiTaskHandle, 0);
     
  xTaskCreatePinnedToCore(BuildPayload, "PayloadTask", 8192, NULL, 1, &payloadTaskHandle, 0);
  
  Serial.begin(115200);
  
  Serial.println("\n Iniciando ConfigManager...");
  config.begin();
  Serial.println("\n Carregando Configuração...");
  config.loadConfig();
  Serial.println("\n Configuração atual:");
  config.printConfig();
  
  Serial.println("\n Iniciando WiFi...");
  SetupWifi(NULL);
  gateway.onCredentials(onWiFiCredentials);
  monitor.setDataCallback(GetPayload);

}
void loop()
{

  
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

void BuildPayload(void *parameter)
{
  std::vector<int> activeChannels = config.getActiveChannelList();
  
  doc1.clear();

  JsonArray channels = doc1["channels"].to<JsonArray>();

  for (int channelId : activeChannels)
  {
    JsonObject channelObj = channels.add<JsonObject>();
    channelObj["id"] = channelId;
    channelObj["val"] = readChannel(channelId);
  }

  doc1["timestamp"] = millis();
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

float readChannel(int channelId)
{
  // Simula a leitura do canal (substitua pela lógica real de leitura)
  return random(0, 1024) / 10.24; // Retorna um valor entre 0.0 e 100.0
}

void SetupWifi(void *parameter)
{
  String ssid = config.getWiFiSSID();
  String password = config.getWiFiPassword();
  Serial.printf("WiFi SSID: %s\n", ssid.c_str());
  Serial.printf("WiFi Password: %s\n", password.length() > 0 ? "********" : "(not set)");

  WiFi.begin(ssid.c_str(), password.c_str());
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++)
  {
    vTaskDelay(500 / portTICK_PERIOD_MS);
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

String GetPayload()
{
  String payload;
  serializeJson(doc1, payload);
  return payload;
}