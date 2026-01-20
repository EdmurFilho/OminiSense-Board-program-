#include <Arduino.h>
#include "ConfigManager.h"
#include "CaptivePortals.h"

ConfigManager config;

void setup() {
  Serial.begin(115200);
  
  config.begin();
  config.loadConfig();
  if (!config.begin()) {
    Serial.println("ConfigManager initialization failed!");
    while (1);
  }
  
  if (!config.loadConfig()) {
    Serial.println("Failed to load configuration!");
    while (1);
  }
  
  config.buildChannelCache();
  config.printConfig();
}
  // Your main loop code here
  // All channel operations are thread-safe and can be safely
  // called from multiple FreeRTOS tasks running on different cores
  
  // Example: Query channel status safely from any task
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) {  // Every 10 seconds
    lastCheck = millis();
    
    int activeCount = config.getActiveChannelCount();
    std::vector<int> activeChannels = config.getActiveChannelList();
    Serial.printf("📊 Active channels: %d\n", activeCount);
    Serial.printf("📊 Active list: %d\n", activeChannels);
  
    if (config.channelExists(1)) {
      ChannelInfo info = config.getChannelInfo(1);
      if (info.valid) {
        Serial.printf("   Channel 1: %s, Pin %d,", 
          info.active ? "ACTIVE" : "DISABLED",
          info.pin);
      }
    }
  }
  
  delay(100);