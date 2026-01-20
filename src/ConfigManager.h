#include <SD_MMC.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <OneWire.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// SDMMC 4-bit pins for ESP32-S3 (native interface - high speed)
// CLK:  GPIO14
// CMD:  GPIO15
// D0:   GPIO2
// D1:   GPIO4
// D2:   GPIO12
// D3:   GPIO13
// Note: These are default SDMMC pins for ESP32-S3, no configuration needed

// I2C pins
#define I2C_SDA 21
#define I2C_SCL 22

// Channel ranges by type
#define DIGITAL_CHANNEL_START 1
#define DIGITAL_CHANNEL_END 10

#define ANALOG_CHANNEL_START 11
#define ANALOG_CHANNEL_END 20

#define ONEWIRE_CHANNEL_START 21
#define ONEWIRE_CHANNEL_END 28

#define SPI_CHANNEL_START 29
#define SPI_CHANNEL_END 38

#define I2C_CHANNEL_START 39
#define I2C_CHANNEL_END 48

#define MAX_CHANNELS 48

const char *CONFIG_FILE = "/config.json";

// Enum para substituir String mode - economia de memória e performance
enum class ChannelMode : uint8_t
{
  NONE = 0,
  DIGITAL,
  M_ANALOG,
  ONEWIRE,
  SPI,
  I2C
};

struct DigitalChannel
{
  int channel;
  int pin;
  float voltage;
  bool active;
};

struct AnalogChannel
{
  int channel;
  int pin;
  float voltage;
  bool active;
};

struct OneWireChannel
{
  int channel;
  int pin;
  bool active;
};

struct SPIChannel
{
  int channel;
  int pin;
  bool active;
};

struct I2CChannel
{
  int channel;
  int id;
  uint8_t address;
  bool active;
};

struct ChannelInfo
{
  ChannelMode mode; 
  int pin;
  uint8_t i2cAddress;
  float voltage;
  bool active;
  bool valid; 
};

class ConfigManager
{
private:
  std::vector<DigitalChannel> digitalChannels;
  std::vector<AnalogChannel> analogChannels;
  std::vector<OneWireChannel> oneWireChannels;
  std::vector<SPIChannel> spiChannels;
  std::vector<I2CChannel> i2cChannels;
  String wifiSSID;
  String wifiPassword;

  // Array de tamanho fixo para O(1) lookup sem fragmentação
  ChannelInfo channelCache[MAX_CHANNELS + 1]; // Índice 0 não usado, 1-48 são os canais

  // Mutex para proteção de thread-safety (RTOS)
  SemaphoreHandle_t configMutex;

  // Helper: Converte enum para string (apenas para debug/print)
  const char *modeToString(ChannelMode mode)
  {
    switch (mode)
    {
    case ChannelMode::DIGITAL:
      return "DIGITAL";
    case ChannelMode::M_ANALOG:
      return "ANALOG";
    case ChannelMode::ONEWIRE:
      return "ONEWIRE";
    case ChannelMode::SPI:
      return "SPI";
    case ChannelMode::I2C:
      return "I2C";
    default:
      return "NONE";
    }
  }

  // INTERNAL: Check if pin is already in use by any channel (MUST be called with mutex held)
  bool isPinInUseInternal(int pin)
  {
    for (const auto &ch : digitalChannels)
    {
      if (ch.pin == pin)
        return true;
    }
    for (const auto &ch : analogChannels)
    {
      if (ch.pin == pin)
        return true;
    }
    for (const auto &ch : oneWireChannels)
    {
      if (ch.pin == pin)
        return true;
    }
    for (const auto &ch : spiChannels)
    {
      if (ch.pin == pin)
        return true;
    }
    return false;
  }

  // INTERNAL: Check if I2C address is already in use (MUST be called with mutex held)
  bool isI2CAddressInUseInternal(uint8_t address)
  {
    for (const auto &ch : i2cChannels)
    {
      if (ch.address == address)
        return true;
    }
    return false;
  }

  // INTERNAL: Build cache WITHOUT acquiring mutex (called by functions that already hold it)
  void buildChannelCacheInternal()
  {
    // Limpa cache - marca todas posições como inválidas
    for (int i = 0; i <= MAX_CHANNELS; i++)
    {
      channelCache[i].valid = false;
      channelCache[i].mode = ChannelMode::NONE;
    }

    int cachedCount = 0;

    // Add digital channels to cache
    for (const auto &ch : digitalChannels)
    {
      if (ch.channel >= 1 && ch.channel <= MAX_CHANNELS)
      {
        channelCache[ch.channel].mode = ChannelMode::DIGITAL;
        channelCache[ch.channel].pin = ch.pin;
        channelCache[ch.channel].i2cAddress = 0;
        channelCache[ch.channel].voltage = ch.voltage;
        channelCache[ch.channel].active = ch.active;
        channelCache[ch.channel].valid = true;
        cachedCount++;
      }
    }

    // Add analog channels to cache
    for (const auto &ch : analogChannels)
    {
      if (ch.channel >= 1 && ch.channel <= MAX_CHANNELS)
      {
        channelCache[ch.channel].mode = ChannelMode::M_ANALOG;
        channelCache[ch.channel].pin = ch.pin;
        channelCache[ch.channel].i2cAddress = 0;
        channelCache[ch.channel].voltage = ch.voltage;
        channelCache[ch.channel].active = ch.active;
        channelCache[ch.channel].valid = true;
        cachedCount++;
      }
    }

    // Add onewire channels to cache
    for (const auto &ch : oneWireChannels)
    {
      if (ch.channel >= 1 && ch.channel <= MAX_CHANNELS)
      {
        channelCache[ch.channel].mode = ChannelMode::ONEWIRE;
        channelCache[ch.channel].pin = ch.pin;
        channelCache[ch.channel].i2cAddress = 0;
        channelCache[ch.channel].voltage = 0.0;
        channelCache[ch.channel].active = ch.active;
        channelCache[ch.channel].valid = true;
        cachedCount++;
      }
    }

    // Add SPI channels to cache
    for (const auto &ch : spiChannels)
    {
      if (ch.channel >= 1 && ch.channel <= MAX_CHANNELS)
      {
        channelCache[ch.channel].mode = ChannelMode::SPI;
        channelCache[ch.channel].pin = ch.pin;
        channelCache[ch.channel].i2cAddress = 0;
        channelCache[ch.channel].voltage = 0.0;
        channelCache[ch.channel].active = ch.active;
        channelCache[ch.channel].valid = true;
        cachedCount++;
      }
    }

    // Add I2C channels to cache
    for (const auto &ch : i2cChannels)
    {
      if (ch.channel >= 1 && ch.channel <= MAX_CHANNELS)
      {
        channelCache[ch.channel].mode = ChannelMode::I2C;
        channelCache[ch.channel].pin = 0;
        channelCache[ch.channel].i2cAddress = ch.address;
        channelCache[ch.channel].voltage = 0.0;
        channelCache[ch.channel].active = ch.active;
        channelCache[ch.channel].valid = true;
        cachedCount++;
      }
    }

    Serial.printf("✓ Channel cache rebuilt: %d channels indexed\n", cachedCount);
  }

public:
  ConfigManager() : configMutex(NULL)
  {
    // Inicializa array de cache como inválido
    for (int i = 0; i <= MAX_CHANNELS; i++)
    {
      channelCache[i].valid = false;
      channelCache[i].mode = ChannelMode::NONE;
    }
  }

  bool begin()
  {
    // Cria mutex para proteção thread-safe
    configMutex = xSemaphoreCreateMutex();
    if (configMutex == NULL)
    {
      Serial.println("✗ ERROR: Failed to create config mutex!");
      return false;
    }
    Serial.println("✓ Config mutex created successfully");

    // Initialize I2C
    Wire.begin(I2C_SDA, I2C_SCL);
    Serial.println("✓ I2C initialized");

    // Initialize SD Card using SDMMC 4-bit mode (high speed - up to 40MHz)
    // Mode: 4-bit bus width for maximum performance
    // Format if mount fails: false (don't auto-format)
    // Max files: 5 (adjust based on your needs)
    if (!SD_MMC.begin("/sdcard", false, true, SDMMC_FREQ_DEFAULT))
    {
      Serial.println("✗ SD Card (SDMMC 4-bit) initialization failed!");
      Serial.println("   Check connections:");
      Serial.println("   CLK:  GPIO14");
      Serial.println("   CMD:  GPIO15");
      Serial.println("   D0:   GPIO2");
      Serial.println("   D1:   GPIO4");
      Serial.println("   D2:   GPIO12");
      Serial.println("   D3:   GPIO13");
      return false;
    }

    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE)
    {
      Serial.println("✗ No SD card attached!");
      return false;
    }

    // Print card information
    Serial.print("✓ SD Card initialized (SDMMC 4-bit) - Type: ");
    if (cardType == CARD_MMC)
    {
      Serial.println("MMC");
    }
    else if (cardType == CARD_SD)
    {
      Serial.println("SDSC");
    }
    else if (cardType == CARD_SDHC)
    {
      Serial.println("SDHC");
    }
    else
    {
      Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("   Card Size: %lluMB\n", cardSize);
    Serial.printf("   Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
    Serial.printf("   Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));

    return true;
  }

  bool loadConfig()
  {
    File file = SD_MMC.open(CONFIG_FILE, FILE_READ);
    if (!file)
    {
      Serial.println("✗ Failed to open config file");
      return false;
    }

    JsonDocument doc; // ArduinoJson V7: No fixed size needed
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error)
    {
      Serial.print("✗ deserializeJson() failed: ");
      Serial.println(error.c_str());
      return false;
    }

    // CRITICAL SECTION - Protege modificação dos vetores
    if (xSemaphoreTake(configMutex, portMAX_DELAY) != pdTRUE)
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for loadConfig");
      return false;
    }

    // Load WiFi config
    if (doc.containsKey("wifi"))
    {
      wifiSSID = doc["wifi"]["ssid"].as<String>();
      wifiPassword = doc["wifi"]["password"].as<String>();
      Serial.println("✓ WiFi config loaded");
    }

    // Load digital channels com reserve para evitar realocações
    digitalChannels.clear();
    JsonArray digital = doc["digital_channels"].as<JsonArray>();
    digitalChannels.reserve(digital.size()); // Otimização: pré-aloca memória
    for (JsonObject ch : digital)
    {
      DigitalChannel dc;
      dc.channel = ch["channel"];
      dc.pin = ch["pin"];
      dc.voltage = ch["voltage"];
      dc.active = ch["active"];
      digitalChannels.push_back(dc);

      if (dc.active)
      {
        pinMode(dc.pin, INPUT_PULLUP); // Use INPUT_PULLUP to avoid noise
      }
    }

    // Load analog channels com reserve
    analogChannels.clear();
    JsonArray analog = doc["analog_channels"].as<JsonArray>();
    analogChannels.reserve(analog.size()); // Otimização: pré-aloca memória
    for (JsonObject ch : analog)
    {
      AnalogChannel ac;
      ac.channel = ch["channel"];
      ac.pin = ch["pin"];
      ac.voltage = ch["voltage"];
      ac.active = ch["active"];
      analogChannels.push_back(ac);

      if (ac.active)
      {
        pinMode(ac.pin, INPUT);
      }
    }

    // Load onewire channels com reserve
    oneWireChannels.clear();
    JsonArray onewire = doc["onewire_channels"].as<JsonArray>();
    oneWireChannels.reserve(onewire.size()); // Otimização: pré-aloca memória
    for (JsonObject ch : onewire)
    {
      OneWireChannel ow;
      ow.channel = ch["channel"];
      ow.pin = ch["pin"];
      ow.active = ch["active"];
      oneWireChannels.push_back(ow);

      if (ow.active)
      {
        pinMode(ow.pin, INPUT_PULLUP); // Use INPUT_PULLUP to avoid noise
      }
    }

    // Load SPI channels com reserve
    spiChannels.clear();
    JsonArray spi = doc["spi_channels"].as<JsonArray>();
    spiChannels.reserve(spi.size()); // Otimização: pré-aloca memória
    for (JsonObject ch : spi)
    {
      SPIChannel sc;
      sc.channel = ch["channel"];
      sc.pin = ch["pin"];
      sc.active = ch["active"];
      spiChannels.push_back(sc);

      if (sc.active)
      {
        pinMode(sc.pin, OUTPUT);
        digitalWrite(sc.pin, HIGH);
      }
    }

    // Load I2C channels com reserve
    i2cChannels.clear();
    JsonArray i2c = doc["i2c_channels"].as<JsonArray>();
    i2cChannels.reserve(i2c.size()); // Otimização: pré-aloca memória
    for (JsonObject ch : i2c)
    {
      I2CChannel ic;
      ic.channel = ch["channel"];
      ic.id = ch["id"];
      ic.address = ch["address"];
      ic.active = ch["active"];
      i2cChannels.push_back(ic);
    }

    Serial.println("✓ Config loaded successfully");

    // Rebuild cache BEFORE releasing mutex
    buildChannelCacheInternal();

    xSemaphoreGive(configMutex);
    return true;
  }

  bool saveConfig()
  {
    JsonDocument doc; // ArduinoJson V7: No fixed size needed

    // CRITICAL SECTION - Protege leitura dos vetores
    if (xSemaphoreTake(configMutex, portMAX_DELAY) != pdTRUE)
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for saveConfig");
      return false;
    }

    // ArduinoJson V7 syntax: doc["key"].to<JsonObject>()
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"] = wifiSSID;
    wifi["password"] = wifiPassword;

    // ArduinoJson V7 syntax: doc["key"].to<JsonArray>()
    JsonArray digital = doc["digital_channels"].to<JsonArray>();
    for (const auto &ch : digitalChannels)
    {
      JsonObject obj = digital.add<JsonObject>();
      obj["channel"] = ch.channel;
      obj["pin"] = ch.pin;
      obj["voltage"] = ch.voltage;
      obj["active"] = ch.active;
    }

    // Save analog channels
    JsonArray analog = doc["analog_channels"].to<JsonArray>();
    for (const auto &ch : analogChannels)
    {
      JsonObject obj = analog.add<JsonObject>();
      obj["channel"] = ch.channel;
      obj["pin"] = ch.pin;
      obj["voltage"] = ch.voltage;
      obj["active"] = ch.active;
    }

    // Save onewire channels
    JsonArray onewire = doc["onewire_channels"].to<JsonArray>();
    for (const auto &ch : oneWireChannels)
    {
      JsonObject obj = onewire.add<JsonObject>();
      obj["channel"] = ch.channel;
      obj["pin"] = ch.pin;
      obj["active"] = ch.active;
    }

    // Save SPI channels
    JsonArray spi = doc["spi_channels"].to<JsonArray>();
    for (const auto &ch : spiChannels)
    {
      JsonObject obj = spi.add<JsonObject>();
      obj["channel"] = ch.channel;
      obj["pin"] = ch.pin;
      obj["active"] = ch.active;
    }

    // Save I2C channels
    JsonArray i2c = doc["i2c_channels"].to<JsonArray>();
    for (const auto &ch : i2cChannels)
    {
      JsonObject obj = i2c.add<JsonObject>();
      obj["channel"] = ch.channel;
      obj["id"] = ch.id;
      obj["address"] = ch.address;
      obj["active"] = ch.active;
    }

    xSemaphoreGive(configMutex);

    File file = SD_MMC.open(CONFIG_FILE, FILE_WRITE);
    if (!file)
    {
      Serial.println("✗ Failed to open config file for writing");
      return false;
    }

    serializeJsonPretty(doc, file);
    file.close();

    Serial.println("✓ Config saved to SD card (SDMMC)");
    return true;
  }

  // PUBLIC: Build cache WITH mutex protection (safe for external calls)
  void buildChannelCache()
  {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE)
    {
      buildChannelCacheInternal();
      xSemaphoreGive(configMutex);
    }
    else
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for buildChannelCache");
    }
  }

  // ========== CHANNEL MANIPULATION API (All Thread-Safe with Validation) ==========

  // Add Digital channel - THREAD SAFE with conflict validation
  int addDigital(int channel, int pin, float voltage)
  {
    if (channel < DIGITAL_CHANNEL_START || channel > DIGITAL_CHANNEL_END)
    {
      Serial.printf("✗ Error: Digital channel %d must be between %d and %d\n",
                    channel, DIGITAL_CHANNEL_START, DIGITAL_CHANNEL_END);
      return -1;
    }

    // CRITICAL SECTION
    if (xSemaphoreTake(configMutex, portMAX_DELAY) != pdTRUE)
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for addDigital");
      return -1;
    }

    // Validation: Check if channel already exists
    if (channelCache[channel].valid)
    {
      Serial.printf("✗ Error: Channel %d already exists\n", channel);
      xSemaphoreGive(configMutex);
      return -1;
    }

    // Validation: Check if pin is already in use
    if (isPinInUseInternal(pin))
    {
      Serial.printf("✗ Error: Pin %d is already in use by another channel\n", pin);
      xSemaphoreGive(configMutex);
      return -1;
    }

    DigitalChannel dc;
    dc.channel = channel;
    dc.pin = pin;
    dc.voltage = voltage;
    dc.active = true;

    pinMode(pin, INPUT_PULLUP); // Hardware setup with PULLUP to avoid noise
    digitalChannels.push_back(dc);
    buildChannelCacheInternal(); // Update cache while holding mutex

    Serial.printf("✓ Digital Channel %d added: Pin %d, %.1fV\n", channel, pin, voltage);

    xSemaphoreGive(configMutex);
    return channel;
  }

  // Add Analog channel - THREAD SAFE with conflict validation
  int addAnalog(int channel, int pin, float voltage)
  {
    if (channel < ANALOG_CHANNEL_START || channel > ANALOG_CHANNEL_END)
    {
      Serial.printf("✗ Error: Analog channel %d must be between %d and %d\n",
                    channel, ANALOG_CHANNEL_START, ANALOG_CHANNEL_END);
      return -1;
    }

    // CRITICAL SECTION
    if (xSemaphoreTake(configMutex, portMAX_DELAY) != pdTRUE)
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for addAnalog");
      return -1;
    }

    // Validation: Check if channel already exists
    if (channelCache[channel].valid)
    {
      Serial.printf("✗ Error: Channel %d already exists\n", channel);
      xSemaphoreGive(configMutex);
      return -1;
    }

    // Validation: Check if pin is already in use
    if (isPinInUseInternal(pin))
    {
      Serial.printf("✗ Error: Pin %d is already in use by another channel\n", pin);
      xSemaphoreGive(configMutex);
      return -1;
    }

    AnalogChannel ac;
    ac.channel = channel;
    ac.pin = pin;
    ac.voltage = voltage;
    ac.active = true;

    pinMode(pin, INPUT); // Hardware setup
    analogChannels.push_back(ac);
    buildChannelCacheInternal(); // Update cache while holding mutex

    Serial.printf("✓ Analog Channel %d added: Pin %d, %.1fV\n", channel, pin, voltage);

    xSemaphoreGive(configMutex);
    return channel;
  }

  // Add One-Wire channel - THREAD SAFE with conflict validation
  int addOneWire(int channel, int pin)
  {
    if (channel < ONEWIRE_CHANNEL_START || channel > ONEWIRE_CHANNEL_END)
    {
      Serial.printf("✗ Error: OneWire channel %d must be between %d and %d\n",
                    channel, ONEWIRE_CHANNEL_START, ONEWIRE_CHANNEL_END);
      return -1;
    }

    // CRITICAL SECTION
    if (xSemaphoreTake(configMutex, portMAX_DELAY) != pdTRUE)
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for addOneWire");
      return -1;
    }

    // Validation: Check if channel already exists
    if (channelCache[channel].valid)
    {
      Serial.printf("✗ Error: Channel %d already exists\n", channel);
      xSemaphoreGive(configMutex);
      return -1;
    }

    // Validation: Check if pin is already in use
    if (isPinInUseInternal(pin))
    {
      Serial.printf("✗ Error: Pin %d is already in use by another channel\n", pin);
      xSemaphoreGive(configMutex);
      return -1;
    }

    OneWireChannel ow;
    ow.channel = channel;
    ow.pin = pin;
    ow.active = true;

    pinMode(pin, INPUT_PULLUP); // Hardware setup with PULLUP to avoid noise
    oneWireChannels.push_back(ow);
    buildChannelCacheInternal(); // Update cache while holding mutex

    Serial.printf("✓ OneWire Channel %d added: Pin %d\n", channel, pin);

    xSemaphoreGive(configMutex);
    return channel;
  }

  // Add SPI channel - THREAD SAFE with conflict validation
  int addSPI(int channel, int csPin)
  {
    if (channel < SPI_CHANNEL_START || channel > SPI_CHANNEL_END)
    {
      Serial.printf("✗ Error: SPI channel %d must be between %d and %d\n",
                    channel, SPI_CHANNEL_START, SPI_CHANNEL_END);
      return -1;
    }

    // CRITICAL SECTION
    if (xSemaphoreTake(configMutex, portMAX_DELAY) != pdTRUE)
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for addSPI");
      return -1;
    }

    // Validation: Check if channel already exists
    if (channelCache[channel].valid)
    {
      Serial.printf("✗ Error: Channel %d already exists\n", channel);
      xSemaphoreGive(configMutex);
      return -1;
    }

    // Validation: Check if pin is already in use
    if (isPinInUseInternal(csPin))
    {
      Serial.printf("✗ Error: Pin %d is already in use by another channel\n", csPin);
      xSemaphoreGive(configMutex);
      return -1;
    }

    SPIChannel sc;
    sc.channel = channel;
    sc.pin = csPin;
    sc.active = true;

    pinMode(csPin, OUTPUT);    // Hardware setup
    digitalWrite(csPin, HIGH); // CS inactive
    spiChannels.push_back(sc);
    buildChannelCacheInternal(); // Update cache while holding mutex

    Serial.printf("✓ SPI Channel %d added: CS Pin %d\n", channel, csPin);

    xSemaphoreGive(configMutex);
    return channel;
  }

  // Add I2C channel - THREAD SAFE with conflict validation
  // Returns channel number (not vector index) as per requirements
  int addI2C(int channel, uint8_t address)
  {
    if (channel < I2C_CHANNEL_START || channel > I2C_CHANNEL_END)
    {
      Serial.printf("✗ Error: I2C channel %d must be between %d and %d\n",
                    channel, I2C_CHANNEL_START, I2C_CHANNEL_END);
      return -1;
    }

    // CRITICAL SECTION
    if (xSemaphoreTake(configMutex, portMAX_DELAY) != pdTRUE)
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for addI2C");
      return -1;
    }

    // Validation: Check if channel already exists
    if (channelCache[channel].valid)
    {
      Serial.printf("✗ Error: Channel %d already exists\n", channel);
      xSemaphoreGive(configMutex);
      return -1;
    }

    // Validation: Check if I2C address is already in use
    if (isI2CAddressInUseInternal(address))
    {
      Serial.printf("✗ Error: I2C address 0x%02X is already in use\n", address);
      xSemaphoreGive(configMutex);
      return -1;
    }

    I2CChannel ic;
    ic.channel = channel;
    ic.id = i2cChannels.size();
    ic.address = address;
    ic.active = true;

    i2cChannels.push_back(ic);
    buildChannelCacheInternal(); // Update cache while holding mutex

    Serial.printf("✓ I2C Channel %d added: 0x%02X\n", channel, address);

    xSemaphoreGive(configMutex);
    return channel; // Return channel number, not index
  }

  // Remove any channel - THREAD SAFE
  void removeChannel(int channel)
  {
    // CRITICAL SECTION
    if (xSemaphoreTake(configMutex, portMAX_DELAY) != pdTRUE)
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for removeChannel");
      return;
    }

    bool found = false;

    // Determine mode from channel number
    if (channel >= DIGITAL_CHANNEL_START && channel <= DIGITAL_CHANNEL_END)
    {
      digitalChannels.erase(std::remove_if(digitalChannels.begin(), digitalChannels.end(),
                                           [channel](const DigitalChannel &ch)
                                           { return ch.channel == channel; }),
                            digitalChannels.end());
      Serial.printf("✓ Digital channel %d removed\n", channel);
      found = true;
    }
    else if (channel >= ANALOG_CHANNEL_START && channel <= ANALOG_CHANNEL_END)
    {
      analogChannels.erase(std::remove_if(analogChannels.begin(), analogChannels.end(),
                                          [channel](const AnalogChannel &ch)
                                          { return ch.channel == channel; }),
                           analogChannels.end());
      Serial.printf("✓ Analog channel %d removed\n", channel);
      found = true;
    }
    else if (channel >= ONEWIRE_CHANNEL_START && channel <= ONEWIRE_CHANNEL_END)
    {
      oneWireChannels.erase(std::remove_if(oneWireChannels.begin(), oneWireChannels.end(),
                                           [channel](const OneWireChannel &ch)
                                           { return ch.channel == channel; }),
                            oneWireChannels.end());
      Serial.printf("✓ OneWire channel %d removed\n", channel);
      found = true;
    }
    else if (channel >= SPI_CHANNEL_START && channel <= SPI_CHANNEL_END)
    {
      spiChannels.erase(std::remove_if(spiChannels.begin(), spiChannels.end(),
                                       [channel](const SPIChannel &ch)
                                       { return ch.channel == channel; }),
                        spiChannels.end());
      Serial.printf("✓ SPI channel %d removed\n", channel);
      found = true;
    }
    else if (channel >= I2C_CHANNEL_START && channel <= I2C_CHANNEL_END)
    {
      i2cChannels.erase(std::remove_if(i2cChannels.begin(), i2cChannels.end(),
                                       [channel](const I2CChannel &ch)
                                       { return ch.channel == channel; }),
                        i2cChannels.end());
      Serial.printf("✓ I2C channel %d removed\n", channel);
      found = true;
    }

    if (found)
    {
      buildChannelCacheInternal(); // Update cache while holding mutex
    }
    else
    {
      Serial.printf("✗ Channel %d not found\n", channel);
    }

    xSemaphoreGive(configMutex);
  }

  // Disable any channel - THREAD SAFE
  void disableChannel(int channel)
  {
    // CRITICAL SECTION
    if (xSemaphoreTake(configMutex, portMAX_DELAY) != pdTRUE)
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for disableChannel");
      return;
    }

    bool found = false;

    // Determine mode from channel number
    if (channel >= DIGITAL_CHANNEL_START && channel <= DIGITAL_CHANNEL_END)
    {
      for (auto &ch : digitalChannels)
      {
        if (ch.channel == channel)
        {
          ch.active = false;
          Serial.printf("✓ Digital channel %d disabled\n", channel);
          found = true;
          break;
        }
      }
    }
    else if (channel >= ANALOG_CHANNEL_START && channel <= ANALOG_CHANNEL_END)
    {
      for (auto &ch : analogChannels)
      {
        if (ch.channel == channel)
        {
          ch.active = false;
          Serial.printf("✓ Analog channel %d disabled\n", channel);
          found = true;
          break;
        }
      }
    }
    else if (channel >= ONEWIRE_CHANNEL_START && channel <= ONEWIRE_CHANNEL_END)
    {
      for (auto &ch : oneWireChannels)
      {
        if (ch.channel == channel)
        {
          ch.active = false;
          Serial.printf("✓ OneWire channel %d disabled\n", channel);
          found = true;
          break;
        }
      }
    }
    else if (channel >= SPI_CHANNEL_START && channel <= SPI_CHANNEL_END)
    {
      for (auto &ch : spiChannels)
      {
        if (ch.channel == channel)
        {
          ch.active = false;
          Serial.printf("✓ SPI channel %d disabled\n", channel);
          found = true;
          break;
        }
      }
    }
    else if (channel >= I2C_CHANNEL_START && channel <= I2C_CHANNEL_END)
    {
      for (auto &ch : i2cChannels)
      {
        if (ch.channel == channel)
        {
          ch.active = false;
          Serial.printf("✓ I2C channel %d disabled\n", channel);
          found = true;
          break;
        }
      }
    }

    if (found)
    {
      buildChannelCacheInternal(); // Update cache while holding mutex
    }
    else
    {
      Serial.printf("✗ Channel %d not found\n", channel);
    }

    xSemaphoreGive(configMutex);
  }

  // Enable any channel - THREAD SAFE
  void enableChannel(int channel)
  {
    // CRITICAL SECTION
    if (xSemaphoreTake(configMutex, portMAX_DELAY) != pdTRUE)
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for enableChannel");
      return;
    }

    bool found = false;

    // Determine mode from channel number
    if (channel >= DIGITAL_CHANNEL_START && channel <= DIGITAL_CHANNEL_END)
    {
      for (auto &ch : digitalChannels)
      {
        if (ch.channel == channel)
        {
          ch.active = true;
          pinMode(ch.pin, INPUT); // Re-setup hardware with PULLUP
          Serial.printf("✓ Digital channel %d enabled\n", channel);
          found = true;
          break;
        }
      }
    }
    else if (channel >= ANALOG_CHANNEL_START && channel <= ANALOG_CHANNEL_END)
    {
      for (auto &ch : analogChannels)
      {
        if (ch.channel == channel)
        {
          ch.active = true;
          pinMode(ch.pin, INPUT); // Re-setup hardware
          Serial.printf("✓ Analog channel %d enabled\n", channel);
          found = true;
          break;
        }
      }
    }
    else if (channel >= ONEWIRE_CHANNEL_START && channel <= ONEWIRE_CHANNEL_END)
    {
      for (auto &ch : oneWireChannels)
      {
        if (ch.channel == channel)
        {
          ch.active = true;
          pinMode(ch.pin, INPUT_PULLUP); // Re-setup hardware with PULLUP
          Serial.printf("✓ OneWire channel %d enabled\n", channel);
          found = true;
          break;
        }
      }
    }
    else if (channel >= SPI_CHANNEL_START && channel <= SPI_CHANNEL_END)
    {
      for (auto &ch : spiChannels)
      {
        if (ch.channel == channel)
        {
          ch.active = true;
          pinMode(ch.pin, OUTPUT);    // Re-setup hardware
          digitalWrite(ch.pin, HIGH); // CS inactive
          Serial.printf("✓ SPI channel %d enabled\n", channel);
          found = true;
          break;
        }
      }
    }
    else if (channel >= I2C_CHANNEL_START && channel <= I2C_CHANNEL_END)
    {
      for (auto &ch : i2cChannels)
      {
        if (ch.channel == channel)
        {
          ch.active = true;
          Serial.printf("✓ I2C channel %d enabled\n", channel);
          found = true;
          break;
        }
      }
    }

    if (found)
    {
      buildChannelCacheInternal(); // Update cache while holding mutex
    }
    else
    {
      Serial.printf("✗ Channel %d not found\n", channel);
    }

    xSemaphoreGive(configMutex);
  }

  // ========== QUERY FUNCTIONS (All Thread-Safe, Cache-First) ==========

  // PUBLIC: Get channel info - returns COPY of ChannelInfo struct (thread-safe)
  ChannelInfo getChannelInfo(int channel)
  {
    ChannelInfo info;
    info.valid = false; // Default: invalid

    if (channel < 1 || channel > MAX_CHANNELS)
    {
      return info;
    }

    // CRITICAL SECTION - Read from cache
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE)
    {
      if (channelCache[channel].valid)
      {
        info = channelCache[channel]; // Return COPY
      }
      xSemaphoreGive(configMutex);
    }
    else
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for getChannelInfo");
    }

    return info;
  }

  // Get pin for any channel - THREAD SAFE, cache-first
  int getChannelPin(int channel)
  {
    if (channel < 1 || channel > MAX_CHANNELS)
    {
      return -1;
    }

    // CRITICAL SECTION - Try cache first
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE)
    {
      if (channelCache[channel].valid)
      {
        int pin = channelCache[channel].pin;
        xSemaphoreGive(configMutex);
        return pin;
      }
      xSemaphoreGive(configMutex);
    }
    else
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for getChannelPin");
    }

    return -1;
  }

  // Get I2C address for I2C channel - THREAD SAFE, cache-first
  uint8_t getChannelI2CAddress(int channel)
  {
    if (channel < 1 || channel > MAX_CHANNELS)
    {
      return 0;
    }

    // CRITICAL SECTION - Try cache first
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE)
    {
      if (channelCache[channel].valid && channelCache[channel].mode == ChannelMode::I2C)
      {
        uint8_t address = channelCache[channel].i2cAddress;
        xSemaphoreGive(configMutex);
        return address;
      }
      xSemaphoreGive(configMutex);
    }
    else
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for getChannelI2CAddress");
    }

    return 0;
  }

  // Get voltage for digital/analog channels - THREAD SAFE, cache-first
  float getChannelVoltage(int channel)
  {
    if (channel < 1 || channel > MAX_CHANNELS)
    {
      return 0.0;
    }

    // CRITICAL SECTION - Try cache first
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE)
    {
      if (channelCache[channel].valid)
      {
        float voltage = channelCache[channel].voltage;
        xSemaphoreGive(configMutex);
        return voltage;
      }
      xSemaphoreGive(configMutex);
    }
    else
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for getChannelVoltage");
    }

    return 0.0;
  }

  // Get count of active channels - THREAD SAFE
  int getActiveChannelCount()
  {
    int count = 0;

    // CRITICAL SECTION
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE)
    {
      for (const auto &ch : digitalChannels)
      {
        if (ch.active)
          count++;
      }
      for (const auto &ch : analogChannels)
      {
        if (ch.active)
          count++;
      }
      for (const auto &ch : oneWireChannels)
      {
        if (ch.active)
          count++;
      }
      for (const auto &ch : spiChannels)
      {
        if (ch.active)
          count++;
      }
      for (const auto &ch : i2cChannels)
      {
        if (ch.active)
          count++;
      }
      xSemaphoreGive(configMutex);
    }
    else
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for getActiveChannelCount");
    }

    return count;
  }

  // Get list of all active channel numbers - THREAD SAFE
  std::vector<int> getActiveChannelList()
  {
    std::vector<int> activeChannels;

    // CRITICAL SECTION
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE)
    {
      for (const auto &ch : digitalChannels)
      {
        if (ch.active)
          activeChannels.push_back(ch.channel);
      }
      for (const auto &ch : analogChannels)
      {
        if (ch.active)
          activeChannels.push_back(ch.channel);
      }
      for (const auto &ch : oneWireChannels)
      {
        if (ch.active)
          activeChannels.push_back(ch.channel);
      }
      for (const auto &ch : spiChannels)
      {
        if (ch.active)
          activeChannels.push_back(ch.channel);
      }
      for (const auto &ch : i2cChannels)
      {
        if (ch.active)
          activeChannels.push_back(ch.channel);
      }
      xSemaphoreGive(configMutex);
    }
    else
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for getActiveChannelList");
    }

    return activeChannels;
  }

  // WiFi getters and setters - THREAD SAFE
  String getWiFiSSID()
  {
    String ssid;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE)
    {
      ssid = wifiSSID;
      xSemaphoreGive(configMutex);
    }
    else
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for getWiFiSSID");
    }
    return ssid;
  }

  String getWiFiPassword()
  {
    String password;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE)
    {
      password = wifiPassword;
      xSemaphoreGive(configMutex);
    }
    else
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for getWiFiPassword");
    }
    return password;
  }

  void setWiFiCredentials(String ssid, String password)
  {
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE)
    {
      wifiSSID = ssid;
      wifiPassword = password;
      Serial.println("✓ WiFi credentials updated");
      xSemaphoreGive(configMutex);
    }
    else
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for setWiFiCredentials");
    }
  }

  // Fast lookup: Check if channel exists in cache - THREAD SAFE
  bool channelExists(int channel)
  {
    if (channel < 1 || channel > MAX_CHANNELS)
    {
      return false;
    }

    bool exists = false;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE)
    {
      exists = channelCache[channel].valid;
      xSemaphoreGive(configMutex);
    }
    else
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for channelExists");
    }

    return exists;
  }

  // Fast lookup: Check if channel is active from cache - THREAD SAFE
  bool isChannelActive(int channel)
  {
    if (channel < 1 || channel > MAX_CHANNELS)
    {
      return false;
    }

    bool active = false;
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE)
    {
      if (channelCache[channel].valid)
      {
        active = channelCache[channel].active;
      }
      xSemaphoreGive(configMutex);
    }
    else
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for isChannelActive");
    }

    return active;
  }

  // Search: Find channel by pin - THREAD SAFE
  int findChannelByPin(int pin)
  {
    int foundChannel = -1;

    // CRITICAL SECTION
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE)
    {
      for (const auto &ch : digitalChannels)
      {
        if (ch.pin == pin)
        {
          foundChannel = ch.channel;
          break;
        }
      }
      if (foundChannel == -1)
      {
        for (const auto &ch : analogChannels)
        {
          if (ch.pin == pin)
          {
            foundChannel = ch.channel;
            break;
          }
        }
      }
      if (foundChannel == -1)
      {
        for (const auto &ch : oneWireChannels)
        {
          if (ch.pin == pin)
          {
            foundChannel = ch.channel;
            break;
          }
        }
      }
      if (foundChannel == -1)
      {
        for (const auto &ch : spiChannels)
        {
          if (ch.pin == pin)
          {
            foundChannel = ch.channel;
            break;
          }
        }
      }
      xSemaphoreGive(configMutex);
    }
    else
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for findChannelByPin");
    }

    return foundChannel;
  }

  // Search: Find I2C channel by address - THREAD SAFE
  int findChannelByI2CAddress(uint8_t address)
  {
    int foundChannel = -1;

    // CRITICAL SECTION
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE)
    {
      for (const auto &ch : i2cChannels)
      {
        if (ch.address == address)
        {
          foundChannel = ch.channel;
          break;
        }
      }
      xSemaphoreGive(configMutex);
    }
    else
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for findChannelByI2CAddress");
    }

    return foundChannel;
  }

  // Search: Get all channels by type - THREAD SAFE
  std::vector<int> getChannelsByType(ChannelMode type)
  {
    std::vector<int> channels;

    // CRITICAL SECTION
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE)
    {
      if (type == ChannelMode::DIGITAL)
      {
        for (const auto &ch : digitalChannels)
        {
          channels.push_back(ch.channel);
        }
      }
      else if (type == ChannelMode::M_ANALOG)
      {
        for (const auto &ch : analogChannels)
        {
          channels.push_back(ch.channel);
        }
      }
      else if (type == ChannelMode::ONEWIRE)
      {
        for (const auto &ch : oneWireChannels)
        {
          channels.push_back(ch.channel);
        }
      }
      else if (type == ChannelMode::SPI)
      {
        for (const auto &ch : spiChannels)
        {
          channels.push_back(ch.channel);
        }
      }
      else if (type == ChannelMode::I2C)
      {
        for (const auto &ch : i2cChannels)
        {
          channels.push_back(ch.channel);
        }
      }
      xSemaphoreGive(configMutex);
    }
    else
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for getChannelsByType");
    }

    return channels;
  }

  // Search: Get all active channels by type - THREAD SAFE
  std::vector<int> getActiveChannelsByType(ChannelMode type)
  {
    std::vector<int> channels;

    // CRITICAL SECTION
    if (xSemaphoreTake(configMutex, portMAX_DELAY) == pdTRUE)
    {
      if (type == ChannelMode::DIGITAL)
      {
        for (const auto &ch : digitalChannels)
        {
          if (ch.active)
            channels.push_back(ch.channel);
        }
      }
      else if (type == ChannelMode::M_ANALOG)
      {
        for (const auto &ch : analogChannels)
        {
          if (ch.active)
            channels.push_back(ch.channel);
        }
      }
      else if (type == ChannelMode::ONEWIRE)
      {
        for (const auto &ch : oneWireChannels)
        {
          if (ch.active)
            channels.push_back(ch.channel);
        }
      }
      else if (type == ChannelMode::SPI)
      {
        for (const auto &ch : spiChannels)
        {
          if (ch.active)
            channels.push_back(ch.channel);
        }
      }
      else if (type == ChannelMode::I2C)
      {
        for (const auto &ch : i2cChannels)
        {
          if (ch.active)
            channels.push_back(ch.channel);
        }
      }
      xSemaphoreGive(configMutex);
    }
    else
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for getActiveChannelsByType");
    }

    return channels;
  }

  // Print detailed info for a channel - THREAD SAFE
  void printChannelDetails(int channel)
  {
    ChannelInfo info = getChannelInfo(channel); // Uses thread-safe method

    if (info.valid)
    {
      Serial.printf("\n╔═══════════════════════════════╗\n");
      Serial.printf("║   Channel %d Details          ║\n", channel);
      Serial.printf("╠═══════════════════════════════╣\n");
      Serial.printf("║ Type: %-23s ║\n", modeToString(info.mode));

      if (info.mode == ChannelMode::I2C)
      {
        Serial.printf("║ Address: 0x%02X                  ║\n", info.i2cAddress);
      }
      else
      {
        Serial.printf("║ Pin: %-24d ║\n", info.pin);
      }

      if (info.mode == ChannelMode::DIGITAL || info.mode == ChannelMode::M_ANALOG)
      {
        Serial.printf("║ Voltage: %.1fV                 ║\n", info.voltage);
      }

      Serial.printf("║ Status: %-21s ║\n", info.active ? "ACTIVE" : "DISABLED");
      Serial.printf("╚═══════════════════════════════╝\n\n");
    }
    else
    {
      Serial.printf("✗ Channel %d not found\n", channel);
    }
  }

  void printConfig()
  {
    // CRITICAL SECTION
    if (xSemaphoreTake(configMutex, portMAX_DELAY) != pdTRUE)
    {
      Serial.println("✗ ERROR: Failed to acquire mutex for printConfig");
      return;
    }

    Serial.println("\n╔═══════════════════════════════════════════════════════════╗");
    Serial.println("║              Current Configuration                        ║");
    Serial.println("╚═══════════════════════════════════════════════════════════╝");

    Serial.println("\n📶 WiFi:");
    Serial.printf("  SSID: %s\n", wifiSSID.c_str());
    Serial.printf("  Password: %s\n", wifiPassword.length() > 0 ? "********" : "(not set)");

    Serial.println("\n🔌 Digital Channels:");
    for (const auto &ch : digitalChannels)
    {
      Serial.printf("  Channel %d: Pin %d, %.1fV (%s)\n", ch.channel, ch.pin,
                    ch.voltage, ch.active ? "ACTIVE" : "DISABLED");
    }

    Serial.println("\n📊 Analog Channels:");
    for (const auto &ch : analogChannels)
    {
      Serial.printf("  Channel %d: Pin %d, %.1fV (%s)\n", ch.channel, ch.pin,
                    ch.voltage, ch.active ? "ACTIVE" : "DISABLED");
    }

    Serial.println("\n🌡️  One-Wire Channels:");
    for (const auto &ch : oneWireChannels)
    {
      Serial.printf("  Channel %d: Pin %d (%s)\n", ch.channel, ch.pin,
                    ch.active ? "ACTIVE" : "DISABLED");
    }

    Serial.println("\n⚡ SPI Channels:");
    for (const auto &ch : spiChannels)
    {
      Serial.printf("  Channel %d: CS Pin %d (%s)\n", ch.channel, ch.pin,
                    ch.active ? "ACTIVE" : "DISABLED");
    }

    Serial.println("\n🔗 I2C Channels:");
    for (const auto &ch : i2cChannels)
    {
      Serial.printf("  Channel %d: 0x%02X (%s)\n",
                    ch.channel, ch.address, ch.active ? "ACTIVE" : "DISABLED");
    }
    Serial.println("\n════════════════════════════════════════════════════════════\n");

    xSemaphoreGive(configMutex);
  }
};

// ========== GLOBAL INSTANCE ==========
ConfigManager config;

// ========== SETUP AND LOOP ==========

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n╔═════════════════════════════════════════════════════════╗");
  Serial.println("║  ESP32-S3 Production-Grade ConfigManager v3.0           ║");
  Serial.println("║  ArduinoJson V7 | SDMMC 4-bit | Full Thread-Safety      ║");
  Serial.println("╚═════════════════════════════════════════════════════════╝");
  Serial.println("\nInitializing...");

  if (!config.begin())
  {
    Serial.println("❌ FATAL: Config initialization failed!");
    while (1)
    {
      delay(1000);
    }
  }

  if (config.loadConfig())
  {
    Serial.println("\n✓ Configuration loaded from SD card (SDMMC)");
    config.printConfig();
  }
  else
  {
    Serial.println("\n⚠️  No config file found or error loading. Starting fresh.");
  }

  Serial.println("\n╔═════════════════════════════════════════╗");
  Serial.println("║  Setup complete. Entering main loop...  ║");
  Serial.println("╚═════════════════════════════════════════╝");

}

void loop()
{
}