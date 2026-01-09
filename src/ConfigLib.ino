/*
 * ESP32-S3 Multi-Protocol Sensor Channel Config Manager
 * Supports: I2C, SPI, Analog, Digital, One-Wire
 * .ino file for Arduino IDE
 */

#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <OneWire.h>
#include <vector>

// SD Card SPI pins for ESP32-S3
#define SD_CS 10
#define SD_MOSI 11
#define SD_MISO 13
#define SD_SCK 12

// I2C pins
#define I2C_SDA 21
#define I2C_SCL 22

// Channel configuration
#define MAX_FIXED_CHANNELS 30  // Change this to set how many fixed channels you have
// I2C channels will start after this number

const char* CONFIG_FILE = "/config.json";

struct FixedChannel {
  int channel;
  int pin;
  String mode;  // "DIGITAL", "ANALOG", "ONEWIRE", "SPI"
  bool active;
};

struct I2CChannel {
  int channel;
  int id;
  uint8_t address;
  bool active;
};

class ConfigManager {
private:
  std::vector<FixedChannel> fixedChannels;
  std::vector<I2CChannel> i2cChannels;
  
public:
  bool begin() {
    // Initialize I2C
    Wire.begin(I2C_SDA, I2C_SCL);
    Serial.println("I2C initialized");
    
    // Initialize SD card
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS)) {
      Serial.println("SD Card initialization failed!");
      return false;
    }
    Serial.println("SD Card initialized");
    return true;
  }

  bool loadConfig() {
    File file = SD.open(CONFIG_FILE, FILE_READ);
    if (!file) {
      Serial.println("Failed to open config file");
      return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return false;
    }

    // Load fixed channels
    fixedChannels.clear();
    JsonArray fixed = doc["fixed_channels"];
    for (JsonObject ch : fixed) {
      FixedChannel fc;
      fc.channel = ch["channel"];
      fc.pin = ch["pin"];
      fc.mode = ch["mode"].as<String>();
      fc.active = ch["active"];
      fixedChannels.push_back(fc);
      
      // Setup pin mode if active
      if (fc.active) {
        setupPin(fc.pin, fc.mode);
      }
    }

    // Load I2C channels
    i2cChannels.clear();
    JsonArray i2c = doc["i2c_channels"];
    for (JsonObject ch : i2c) {
      I2CChannel ic;
      ic.channel = ch["channel"];
      ic.id = ch["id"];
      ic.address = ch["address"];
      ic.active = ch["active"];
      i2cChannels.push_back(ic);
    }

    Serial.println("Config loaded successfully");
    return true;
  }

  bool saveConfig() {
    JsonDocument doc;

    // Save fixed channels
    JsonArray fixed = doc.createNestedArray("fixed_channels");
    for (const auto& ch : fixedChannels) {
      JsonObject obj = fixed.add<JsonObject>();
      obj["channel"] = ch.channel;
      obj["pin"] = ch.pin;
      obj["mode"] = ch.mode;
      obj["active"] = ch.active;
    }

    // Save I2C channels
    JsonArray i2c = doc.createNestedArray("i2c_channels");
    for (const auto& ch : i2cChannels) {
      JsonObject obj = i2c.add<JsonObject>();
      obj["channel"] = ch.channel;
      obj["id"] = ch.id;
      obj["address"] = ch.address;
      obj["active"] = ch.active;
    }

    File file = SD.open(CONFIG_FILE, FILE_WRITE);
    if (!file) {
      Serial.println("Failed to open config file for writing");
      return false;
    }

    serializeJsonPretty(doc, file);
    file.close();
    
    Serial.println("Config saved successfully");
    return true;
  }

  void setupPin(int pin, String mode) {
    if (mode == "DIGITAL") {
      pinMode(pin, INPUT);
    } else if (mode == "ANALOG") {
      pinMode(pin, INPUT);
    } else if (mode == "ONEWIRE") {
      pinMode(pin, INPUT);
    } else if (mode == "SPI") {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, HIGH);  // CS pin HIGH (inactive)
    }
  }

  // Get channel mode
  String getChannelMode(int channel) {
    // Check fixed channels
    for (const auto& ch : fixedChannels) {
      if (ch.channel == channel && ch.active) {
        return ch.mode;
      }
    }
    
    // Check I2C channels
    for (const auto& ch : i2cChannels) {
      if (ch.channel == channel && ch.active) {
        return "I2C";
      }
    }
    
    return "NONE";
  }

  // Get pin for fixed channel
  int getChannelPin(int channel) {
    for (const auto& ch : fixedChannels) {
      if (ch.channel == channel && ch.active) {
        return ch.pin;
      }
    }
    return -1;
  }

  // Get I2C address for I2C channel
  uint8_t getChannelI2CAddress(int channel) {
    for (const auto& ch : i2cChannels) {
      if (ch.channel == channel && ch.active) {
        return ch.address;
      }
    }
    return 0;
  }

  std::vector<FixedChannel>& getFixedChannels() {
    return fixedChannels;
  }

  std::vector<I2CChannel>& getI2CChannels() {
    return i2cChannels;
  }

  // Get count of active channels
  int getActiveChannelCount() {
    int count = 0;
    
    for (const auto& ch : fixedChannels) {
      if (ch.active) count++;
    }
    
    for (const auto& ch : i2cChannels) {
      if (ch.active) count++;
    }
    
    return count;
  }

  // Get list of all active channel numbers
  std::vector<int> getActiveChannelList() {
    std::vector<int> activeChannels;
    
    for (const auto& ch : fixedChannels) {
      if (ch.active) {
        activeChannels.push_back(ch.channel);
      }
    }
    
    for (const auto& ch : i2cChannels) {
      if (ch.active) {
        activeChannels.push_back(ch.channel);
      }
    }
    
    return activeChannels;
  }

  void printConfig() {
    Serial.println("\n=== Current Configuration ===");
    Serial.println("Fixed Channels:");
    for (const auto& ch : fixedChannels) {
      Serial.printf("  Channel %d: Pin %d - %s (%s)\n", ch.channel, ch.pin, 
                    ch.mode.c_str(), ch.active ? "ACTIVE" : "DISABLED");
    }
    
    Serial.println("\nI2C Channels:");
    for (const auto& ch : i2cChannels) {
      Serial.printf("  Channel %d: 0x%02X (%s)\n", 
        ch.channel, ch.address, ch.active ? "ACTIVE" : "DISABLED");
    }
    Serial.println("============================\n");
  }
};

ConfigManager config;

// ========== USER API FUNCTIONS ==========

// Change fixed channel mode (keeps current active state)
void updateChannel(int channel, String newMode) {
  bool found = false;
  
  for (auto& ch : config.getFixedChannels()) {
    if (ch.channel == channel) {
      ch.mode = newMode;
      found = true;
      
      // Update pin mode if channel is active
      if (ch.active) {
        config.setupPin(ch.pin, newMode);
      }
      
      Serial.printf("Channel %d updated: %s (%s)\n", channel, newMode.c_str(), 
                    ch.active ? "ACTIVE" : "DISABLED");
      break;
    }
  }
  
  if (!found) {
    Serial.printf("Channel %d not found!\n", channel);
  } else {
    config.saveConfig();
  }
}

// Add I2C channel dynamically
int addI2C(int channel, uint8_t address) {
  // Validate channel number
  if (channel <= MAX_FIXED_CHANNELS) {
    Serial.printf("Error: I2C channel %d must be > %d (reserved for fixed channels)\n", 
                  channel, MAX_FIXED_CHANNELS);
    return -1;
  }
  
  I2CChannel ic;
  ic.channel = channel;
  ic.id = config.getI2CChannels().size();
  ic.address = address;
  ic.active = true;
  
  config.getI2CChannels().push_back(ic);
  config.saveConfig();
  
  Serial.printf("I2C Channel %d added: 0x%02X\n", channel, address);
  return ic.id;
}

// Remove I2C channel
void removeI2C(int channel) {
  auto& channels = config.getI2CChannels();
  
  auto it = std::remove_if(channels.begin(), channels.end(),
    [channel](const I2CChannel& ch) { return ch.channel == channel; });
  
  if (it != channels.end()) {
    channels.erase(it, channels.end());
    config.saveConfig();
    Serial.printf("I2C Channel %d removed\n", channel);
  } else {
    Serial.printf("I2C Channel %d not found!\n", channel);
  }
}

// Disable any channel (fixed or I2C)
void disableChannel(int channel) {
  // Try fixed channels
  for (auto& ch : config.getFixedChannels()) {
    if (ch.channel == channel) {
      ch.active = false;
      config.saveConfig();
      Serial.printf("Channel %d disabled\n", channel);
      return;
    }
  }
  
  // Try I2C channels
  for (auto& ch : config.getI2CChannels()) {
    if (ch.channel == channel) {
      ch.active = false;
      config.saveConfig();
      Serial.printf("I2C channel %d disabled\n", channel);
      return;
    }
  }
  
  Serial.printf("Channel %d not found\n", channel);
}

// Enable any channel (fixed or I2C)
void enableChannel(int channel) {
  // Try fixed channels
  for (auto& ch : config.getFixedChannels()) {
    if (ch.channel == channel) {
      ch.active = true;
      config.setupPin(ch.pin, ch.mode);
      config.saveConfig();
      Serial.printf("Channel %d enabled\n", channel);
      return;
    }
  }
  
  // Try I2C channels
  for (auto& ch : config.getI2CChannels()) {
    if (ch.channel == channel) {
      ch.active = true;
      config.saveConfig();
      Serial.printf("I2C channel %d enabled\n", channel);
      return;
    }
  }
  
  Serial.printf("Channel %d not found\n", channel);
}

// Read sensor from any channel
float readChannel(int channel) {
  String mode = config.getChannelMode(channel);
  
  if (mode == "NONE") {
    Serial.printf("Channel %d is not active or doesn't exist\n", channel);
    return -1;
  }
  
  if (mode == "DIGITAL") {
    int pin = config.getChannelPin(channel);
    int value = digitalRead(pin);
    Serial.printf("Channel %d (Digital Pin %d): %d\n", channel, pin, value);
    return value;
    
  } else if (mode == "ANALOG") {
    int pin = config.getChannelPin(channel);
    int value = analogRead(pin);
    Serial.printf("Channel %d (Analog Pin %d): %d\n", channel, pin, value);
    return value;
    
  } else if (mode == "ONEWIRE") {
    int pin = config.getChannelPin(channel);
    OneWire ow(pin);
    
    byte addr[8];
    if (ow.search(addr)) {
      // Found a device - implement your specific One-Wire protocol here
      // Example: DS18B20 temperature sensor would need specific commands
      Serial.printf("Channel %d (One-Wire Pin %d): Device found at address: ", channel, pin);
      for(int i = 0; i < 8; i++) {
        Serial.printf("%02X ", addr[i]);
      }
      Serial.println();
      
      // Return device address first byte as example
      return addr[0];
    } else {
      Serial.printf("Channel %d (One-Wire Pin %d): No device found\n", channel, pin);
      return -1;
    }
    
  } else if (mode == "SPI") {
    int csPin = config.getChannelPin(channel);
    
    // Example SPI read - modify based on your sensor protocol
    digitalWrite(csPin, LOW);
    byte value = SPI.transfer(0x00); // Read command varies by sensor
    digitalWrite(csPin, HIGH);
    
    Serial.printf("Channel %d (SPI CS Pin %d): %d\n", channel, csPin, value);
    return value;
    
  } else if (mode == "I2C") {
    uint8_t address = config.getChannelI2CAddress(channel);
    
    // Check if device responds
    Wire.beginTransmission(address);
    int error = Wire.endTransmission();
    
    if (error == 0) {
      // Device found - implement your specific I2C protocol here
      // Example generic read
      Wire.requestFrom(address, (uint8_t)2);
      if (Wire.available() >= 2) {
        byte msb = Wire.read();
        byte lsb = Wire.read();
        int value = (msb << 8) | lsb;
        Serial.printf("Channel %d (I2C 0x%02X): %d\n", channel, address, value);
        return value;
      }
    } else {
      Serial.printf("I2C error on Channel %d (0x%02X)\n", channel, address);
      return -1;
    }
  }
  
  return -1;
}

void setup() {
  // Your setup code here
}

void loop() {
  // Your loop code here
}