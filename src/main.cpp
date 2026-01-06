#include <Arduino.h>
#include "ConfigLib.ino"

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== Sensor Manager Example ===\n");

  // Initialize the config system
  if (!config.begin()) {
    Serial.println("Failed to initialize!");
    return;
  }

  // Load existing config or create new one
  if (!config.loadConfig()) {
    Serial.println("Creating new configuration...");
    
    
    config.getFixedChannels().push_back({1, 2, "DIGITAL", true});   // Door sensor on pin 2
    config.getFixedChannels().push_back({2, 4, "ANALOG", true});    // Light sensor on pin 4
    config.getFixedChannels().push_back({3, 5, "ONEWIRE", true});   // Temperature DS18B20 on pin 5
    config.getFixedChannels().push_back({4, 15, "DIGITAL", true});  // Motion sensor on pin 15
    config.getFixedChannels().push_back({5, 34, "ANALOG", true});   // Moisture sensor on pin 34
    
    config.saveConfig();
  }

  config.printConfig();

  // ========== DYNAMICALLY ADD I2C SENSORS ==========
  // These can be discovered or added at runtime
  
  Serial.println("\n>>> Scanning and adding I2C devices...");
  addI2C(10, 0x3C);  // OLED display
  addI2C(11, 0x68);  // MPU6050 gyro/accelerometer
  addI2C(12, 0x76);  // BME280 environment sensor
  
  config.printConfig();

  // ========== DYNAMICALLY ADD SPI SENSORS ==========
  
  Serial.println("\n>>> Adding SPI devices...");
  addSPI(20, 14);  // Thermocouple on CS pin 14
  addSPI(21, 27);  // SD card reader on CS pin 27
  
  config.printConfig();

  // ========== MODIFY EXISTING CHANNEL ==========
  
  Serial.println("\n>>> Changing channel 2 from ANALOG to DIGITAL...");
  updateChannel(2, "DIGITAL", true);
  
  config.printConfig();

  // ========== DISABLE A CHANNEL ==========
  
  Serial.println("\n>>> Disabling channel 4 (motion sensor)...");
  updateChannel(4, "DIGITAL", false);
  
  config.printConfig();

  // ========== REMOVE AN I2C DEVICE ==========
  
  Serial.println("\n>>> Removing I2C channel 12...");
  removeI2C(12);
  
  config.printConfig();

  Serial.println("\n=== Setup Complete ===\n");
}

void loop() {
  Serial.println("\n========== Reading All Sensors ==========");
  
  // Read all fixed channels
  Serial.println("\n--- Fixed Channels ---");
  readChannel(1);  // Door sensor
  readChannel(2);  // Light sensor (now digital)
  readChannel(3);  // Temperature
  readChannel(5);  // Moisture
  // Channel 4 is disabled, so it won't read
  
  // Read all I2C channels
  Serial.println("\n--- I2C Channels ---");
  readChannel(10); // OLED
  readChannel(11); // MPU6050
  // Channel 12 was removed
  
  // Read all SPI channels
  Serial.println("\n--- SPI Channels ---");
  readChannel(20); // Thermocouple
  readChannel(21); // SD card
  
  Serial.println("\n==========================================");
  
  delay(5000);  // Read every 5 seconds
}

// ========== ADVANCED EXAMPLE ==========
// You can also create custom functions that use the API

void scanAndAddI2CDevices() {
  Serial.println("\n>>> Auto-scanning I2C bus...");
  
  int channelNum = 50;  // Start at channel 50 for auto-discovered devices
  
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    int error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.printf("Found I2C device at 0x%02X, adding as channel %d\n", address, channelNum);
      addI2C(channelNum, address);
      channelNum++;
    }
  }
}

void disableAllAnalogChannels() {
  Serial.println("\n>>> Disabling all analog channels...");
  
  for (auto& ch : config.getFixedChannels()) {
    if (ch.mode == "ANALOG" && ch.active) {
      updateChannel(ch.channel, "ANALOG", false);
      Serial.printf("Disabled channel %d\n", ch.channel);
    }
  }
}

void readOnlyActiveChannels() {
  Serial.println("\n>>> Reading only active channels...");
  
  // Fixed channels
  for (const auto& ch : config.getFixedChannels()) {
    if (ch.active) {
      float value = readChannel(ch.channel);
      // Do something with value
    }
  }
  
  // I2C channels
  for (const auto& ch : config.getI2CChannels()) {
    if (ch.active) {
      float value = readChannel(ch.channel);
      // Do something with value
    }
  }
  
  // SPI channels
  for (const auto& ch : config.getSPIChannels()) {
    if (ch.active) {
      float value = readChannel(ch.channel);
      // Do something with value
    }
  }