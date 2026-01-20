#include <Arduino.h>
#include "ConfigManager.h"
#include "CaptivePortals.h"

ConfigManager config;

void setup()
{
  Serial.begin(115200);

  config.begin();
  config.loadConfig();
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

  config.buildChannelCache();
  config.printConfig();
}
// Your main loop code here
// All channel operations are thread-safe and can be safely
// called from multiple FreeRTOS tasks running on different cores

// Example: Query channel status safely from any task
static unsigned long lastCheck = 0;
if (millis() - lastCheck > 10000)
{ // Every 10 seconds
  lastCheck = millis();

  int activeCount = config.getActiveChannelCount();
  std::vector<int> activeChannels = config.getActiveChannelList();
  Serial.printf("📊 Active channels: %d\n", activeCount);
  Serial.printf("📊 Active list: %d\n", activeChannels);

  if (config.channelExists(1))
  {
    ChannelInfo info = config.getChannelInfo(1);
    if (info.valid)
    {
      Serial.printf("   Channel 1: %s, Pin %d,",
                    info.active ? "ACTIVE" : "DISABLED",
                    info.pin);
    }
  }
}

delay(100);


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
  gateway.onCredentials(onWiFiCredentials);

Gateway gateway("OMINISENSE_GATEWAY");

gateway.begin();                    // Inicia o Gateway (porta 80, IP 192.168.4.1)
gateway.end();                      // Para o Gateway
gateway.handle();                   // Processa requisições (chame no loop)
gateway.active();                   // Retorna true se ativo
gateway.onCredentials(funcao);      // Registra callback para credenciais

RawValues monitor;

monitor.begin(false);               // Inicia em modo STA (conectado ao WiFi)
monitor.begin(true);                // Inicia em modo AP (rede própria, IP 192.168.5.1)
monitor.end();                      // Para o monitor
monitor.handle();                   // Processa requisições (chame no loop)
monitor.active();                   // Retorna true se ativo
monitor.setDataCallback(funcao);    // Registra função que gera o JSON
monitor.timeSinceLastRequest();     // Retorna tempo (ms) desde última requisição