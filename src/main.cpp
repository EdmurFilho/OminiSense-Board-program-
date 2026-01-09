/*
 * COMPLETE API REFERENCE - ESP32-S3 Sensor Manager
 * Each section demonstrates one feature and can be copy-pasted independently
 */

#include "ConfigLib.ino"  // Include your main .ino file

// ============================================================================
// SECTION 1: INITIALIZATION
// ============================================================================
void example_initialization() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize the config system
  if (!config.begin()) {
    Serial.println("Failed to initialize!");
    return;
  }
  
  Serial.println("Config system initialized successfully");
}

// ============================================================================
// SECTION 2: LOAD EXISTING CONFIG
// ============================================================================
void example_loadConfig() {
  if (config.loadConfig()) {
    Serial.println("Config loaded from SD card");
  } else {
    Serial.println("No config found on SD card");
  }
}

// ============================================================================
// SECTION 3: CREATE DEFAULT FIXED CHANNELS
// ============================================================================
void example_createDefaultFixedChannels() {
  // Add fixed channels manually to the vector
  // Format: {channel, pin, mode, active}
  // Modes: "DIGITAL", "ANALOG", "ONEWIRE", "SPI"
  
  config.getFixedChannels().push_back({1, 2, "DIGITAL", true});   // Digital sensor on pin 2
  config.getFixedChannels().push_back({2, 4, "ANALOG", true});    // Analog sensor on pin 4
  config.getFixedChannels().push_back({3, 5, "ONEWIRE", true});   // DS18B20 on pin 5
  config.getFixedChannels().push_back({4, 15, "DIGITAL", true});  // Digital sensor on pin 15
  config.getFixedChannels().push_back({5, 14, "SPI", true});      // SPI sensor CS on pin 14
  config.getFixedChannels().push_back({6, 27, "SPI", true});      // SPI sensor CS on pin 27
  config.getFixedChannels().push_back({7, 34, "ANALOG", false});  // Analog (disabled)
  
  config.saveConfig();
  Serial.println("Default fixed channels created");
}

// ============================================================================
// SECTION 4: ADD I2C CHANNEL
// ============================================================================
void example_addI2C() {
  // Add I2C sensor dynamically
  // Format: addI2C(channel_number, i2c_address)
  // Channel must be > MAX_FIXED_CHANNELS (default: > 30)
  
  int id1 = addI2C(31, 0x3C);  // OLED at address 0x3C
  Serial.printf("I2C channel 31 added with ID: %d\n", id1);
  
  int id2 = addI2C(32, 0x68);  // IMU at address 0x68
  Serial.printf("I2C channel 32 added with ID: %d\n", id2);
  
  addI2C(33, 0x76);  // Pressure sensor at address 0x76
  addI2C(34, 0x40);  // Another I2C sensor
}

// ============================================================================
// SECTION 5: UPDATE FIXED CHANNEL MODE
// ============================================================================
void example_updateChannelMode() {
  // Change mode of fixed channel
  // Format: updateChannel(channel, "NEW_MODE", active_status)
  // Modes: "DIGITAL", "ANALOG", "ONEWIRE", "SPI"
  
  updateChannel(2, "DIGITAL" );   // Change channel 2 from ANALOG to DIGITAL
  updateChannel(3, "ANALOG");    // Change channel 3 from ONEWIRE to ANALOG
  updateChannel(5, "SPI");       // Ensure channel 5 is SPI
}

void example_disableChannel() {
  disableChannel(4);   // Disable fixed channel 4
  disableChannel(33);  // Disable I2C channel 33
  disableChannel(6);   // Disable SPI channel 6
}

void example_enableChannel() {
  enableChannel(4);   // Enable fixed channel 4
  enableChannel(33);  // Enable I2C channel 33
  enableChannel(6);   // Enable SPI channel 6
}


// ============================================================================
// SECTION 8: REMOVE I2C CHANNEL
// ============================================================================
void example_removeI2C() {
  // Remove I2C channel completely from config
  // Format: removeI2C(channel_number)
  
  removeI2C(34);  // Remove I2C channel 34
  Serial.println("I2C channel 34 removed");
}

// ============================================================================
// SECTION 9: READ SINGLE CHANNEL
// ============================================================================
void example_readSingleChannel() {
  // Read any channel (auto-detects type)
  // Format: readChannel(channel_number)
  // Returns float value
  
  float value1 = readChannel(1);   // Read digital channel
  float value2 = readChannel(2);   // Read analog channel
  float value3 = readChannel(3);   // Read one-wire channel
  float value4 = readChannel(5);   // Read SPI channel
  float value5 = readChannel(31);  // Read I2C channel
  
  Serial.printf("Channel 1 (Digital): %.0f\n", value1);
  Serial.printf("Channel 2 (Analog): %.0f\n", value2);
  Serial.printf("Channel 3 (One-Wire): %.2f\n", value3);
  Serial.printf("Channel 5 (SPI): %.0f\n", value4);
  Serial.printf("Channel 31 (I2C): %.0f\n", value5);
}

// ============================================================================
// SECTION 10: GET ACTIVE CHANNEL COUNT
// ============================================================================
void example_getActiveCount() {
  // Get total number of active channels
  // Format: config.getActiveChannelCount()
  
  int count = config.getActiveChannelCount();
  Serial.printf("Total active channels: %d\n", count);
  
  // You can use this to check if any sensors are active
  if (count == 0) {
    Serial.println("Warning: No active channels!");
  }
}

// ============================================================================
// SECTION 11: GET ACTIVE CHANNEL LIST
// ============================================================================
void example_getActiveList() {
  // Get vector of all active channel numbers
  // Format: config.getActiveChannelList()
  
  std::vector<int> activeChannels = config.getActiveChannelList();
  
  Serial.printf("Found %d active channels: ", activeChannels.size());
  for (int ch : activeChannels) {
    Serial.printf("%d ", ch);
  }
  Serial.println();
}

// ============================================================================
// SECTION 12: READ ALL ACTIVE CHANNELS
// ============================================================================
void example_readAllActive() {
  // Read all active channels in one loop
  
  std::vector<int> activeChannels = config.getActiveChannelList();
  
  Serial.printf("Reading %d active channels:\n", activeChannels.size());
  for (int channel : activeChannels) {
    float value = readChannel(channel);
    // Process value here
  }
}

// ============================================================================
// SECTION 13: PRINT CONFIGURATION
// ============================================================================
void example_printConfig() {
  // Display entire configuration to Serial
  
  config.printConfig();
}

// ============================================================================
// SECTION 14: SAVE CONFIGURATION
// ============================================================================
void example_saveConfig() {
  // Save current config to SD card
  
  if (config.saveConfig()) {
    Serial.println("Configuration saved to SD card");
  } else {
    Serial.println("Failed to save configuration");
  }
}

// ============================================================================
// SECTION 15: AUTO-SCAN I2C BUS
// ============================================================================
void example_scanI2C() {
  // Automatically scan I2C bus and add all found devices
  
  Serial.println("Scanning I2C bus...");
  
  int channelNum = MAX_FIXED_CHANNELS + 1;
  int found = 0;
  
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    int error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.printf("Found I2C device at 0x%02X, adding as channel %d\n", 
                    address, channelNum);
      addI2C(channelNum, address);
      channelNum++;
      found++;
    }
  }
  
  Serial.printf("I2C scan complete: %d devices found\n", found);
}

// ============================================================================
// SECTION 16: ACCESS FIXED CHANNEL DETAILS
// ============================================================================
void example_accessFixedChannelDetails() {
  // Access individual fixed channel properties
  
  Serial.println("Fixed Channel Details:");
  for (const auto& ch : config.getFixedChannels()) {
    Serial.printf("  Channel %d: Pin %d, Mode %s, %s\n", 
                  ch.channel, ch.pin, ch.mode.c_str(), 
                  ch.active ? "ACTIVE" : "DISABLED");
  }
}

// ============================================================================
// SECTION 17: ACCESS I2C CHANNEL DETAILS
// ============================================================================
void example_accessI2CChannelDetails() {
  // Access individual I2C channel properties
  
  Serial.println("I2C Channel Details:");
  for (const auto& ch : config.getI2CChannels()) {
    Serial.printf("  Channel %d: ID %d, I2C Address 0x%02X, %s\n", 
                  ch.channel, ch.id, ch.address, 
                  ch.active ? "ACTIVE" : "DISABLED");
  }
}

// ============================================================================
// SECTION 18: READ ONLY FIXED CHANNELS
// ============================================================================
void example_readFixedChannels() {
  // Read only fixed channels (Digital, Analog, One-Wire, SPI)
  
  Serial.println("Reading fixed channels:");
  for (const auto& ch : config.getFixedChannels()) {
    if (ch.active) {
      float value = readChannel(ch.channel);
      Serial.printf("  Channel %d (Pin %d, %s): %.2f\n", 
                    ch.channel, ch.pin, ch.mode.c_str(), value);
    }
  }
}

// ============================================================================
// SECTION 19: READ ONLY I2C CHANNELS
// ============================================================================
void example_readI2CChannels() {
  // Read only I2C channels
  
  Serial.println("Reading I2C channels:");
  for (const auto& ch : config.getI2CChannels()) {
    if (ch.active) {
      float value = readChannel(ch.channel);
      Serial.printf("  Channel %d (0x%02X): %.2f\n", 
                    ch.channel, ch.address, value);
    }
  }
}

// ============================================================================
// SECTION 20: READ BY MODE - DIGITAL ONLY
// ============================================================================
void example_readDigitalOnly() {
  // Read only DIGITAL channels
  
  Serial.println("Reading only DIGITAL channels:");
  for (const auto& ch : config.getFixedChannels()) {
    if (ch.active && ch.mode == "DIGITAL") {
      float value = readChannel(ch.channel);
      Serial.printf("  Channel %d (Pin %d): %.0f\n", ch.channel, ch.pin, value);
    }
  }
}

// ============================================================================
// SECTION 21: READ BY MODE - ANALOG ONLY
// ============================================================================
void example_readAnalogOnly() {
  // Read only ANALOG channels
  
  Serial.println("Reading only ANALOG channels:");
  for (const auto& ch : config.getFixedChannels()) {
    if (ch.active && ch.mode == "ANALOG") {
      float value = readChannel(ch.channel);
      Serial.printf("  Channel %d (Pin %d): %.2f\n", ch.channel, ch.pin, value);
    }
  }
}

// ============================================================================
// SECTION 22: READ BY MODE - ONE-WIRE ONLY
// ============================================================================
void example_readOneWireOnly() {
  // Read only ONE-WIRE channels
  
  Serial.println("Reading only ONE-WIRE channels:");
  for (const auto& ch : config.getFixedChannels()) {
    if (ch.active && ch.mode == "ONEWIRE") {
      float value = readChannel(ch.channel);
      Serial.printf("  Channel %d (Pin %d): %.2f\n", ch.channel, ch.pin, value);
    }
  }
}

// ============================================================================
// SECTION 23: READ BY MODE - SPI ONLY
// ============================================================================
void example_readSPIOnly() {
  // Read only SPI channels
  
  Serial.println("Reading only SPI channels:");
  for (const auto& ch : config.getFixedChannels()) {
    if (ch.active && ch.mode == "SPI") {
      float value = readChannel(ch.channel);
      Serial.printf("  Channel %d (CS Pin %d): %.2f\n", ch.channel, ch.pin, value);
    }
  }
}

// ============================================================================
// SECTION 24: GET CHANNEL MODE
// ============================================================================
void example_getChannelMode() {
  // Get the mode/type of any channel
  // Returns: "DIGITAL", "ANALOG", "ONEWIRE", "SPI", "I2C", or "NONE"
  
  String mode1 = config.getChannelMode(1);
  String mode2 = config.getChannelMode(5);
  String mode3 = config.getChannelMode(31);
  String mode4 = config.getChannelMode(99);  // Non-existent
  
  Serial.printf("Channel 1 mode: %s\n", mode1.c_str());
  Serial.printf("Channel 5 mode: %s\n", mode2.c_str());
  Serial.printf("Channel 31 mode: %s\n", mode3.c_str());
  Serial.printf("Channel 99 mode: %s\n", mode4.c_str());
}

// ============================================================================
// SECTION 25: CHECK IF CHANNEL EXISTS AND IS ACTIVE
// ============================================================================
void example_checkChannelActive() {
  // Check if a specific channel exists and is active
  
  int channelToCheck = 5;
  String mode = config.getChannelMode(channelToCheck);
  
  if (mode != "NONE") {
    Serial.printf("Channel %d exists (Mode: %s)\n", channelToCheck, mode.c_str());
    
    // Check if it's in the active list
    std::vector<int> activeChannels = config.getActiveChannelList();
    bool isActive = std::find(activeChannels.begin(), activeChannels.end(), 
                               channelToCheck) != activeChannels.end();
    
    Serial.printf("Channel %d is %s\n", channelToCheck, 
                  isActive ? "ACTIVE" : "DISABLED");
  } else {
    Serial.printf("Channel %d does not exist\n", channelToCheck);
  }
}

// ============================================================================
// SECTION 26: GET CHANNEL PIN
// ============================================================================
void example_getChannelPin() {
  // Get the pin number for a fixed channel
  
  int pin1 = config.getChannelPin(1);
  int pin5 = config.getChannelPin(5);
  
  if (pin1 != -1) {
    Serial.printf("Channel 1 uses pin: %d\n", pin1);
  }
  
  if (pin5 != -1) {
    Serial.printf("Channel 5 (SPI CS) uses pin: %d\n", pin5);
  }
}

// ============================================================================
// SECTION 27: GET I2C ADDRESS
// ============================================================================
void example_getI2CAddress() {
  // Get the I2C address for an I2C channel
  
  uint8_t addr31 = config.getChannelI2CAddress(31);
  uint8_t addr32 = config.getChannelI2CAddress(32);
  
  if (addr31 != 0) {
    Serial.printf("Channel 31 I2C address: 0x%02X\n", addr31);
  }
  
  if (addr32 != 0) {
    Serial.printf("Channel 32 I2C address: 0x%02X\n", addr32);
  }
}

// ============================================================================
// SECTION 28: FIND CHANNELS BY PIN
// ============================================================================
void example_findChannelsByPin() {
  // Find all channels using a specific pin
  
  int searchPin = 14;
  Serial.printf("Channels using pin %d:\n", searchPin);
  
  for (const auto& ch : config.getFixedChannels()) {
    if (ch.pin == searchPin) {
      Serial.printf("  Channel %d (%s, %s)\n", ch.channel, ch.mode.c_str(),
                    ch.active ? "ACTIVE" : "DISABLED");
    }
  }
}

// ============================================================================
// SECTION 29: COUNT CHANNELS BY MODE
// ============================================================================
void example_countChannelsByMode() {
  // Count how many channels of each mode exist
  
  int digitalCount = 0, analogCount = 0, oneWireCount = 0, spiCount = 0, i2cCount = 0;
  
  for (const auto& ch : config.getFixedChannels()) {
    if (!ch.active) continue;
    
    if (ch.mode == "DIGITAL") digitalCount++;
    else if (ch.mode == "ANALOG") analogCount++;
    else if (ch.mode == "ONEWIRE") oneWireCount++;
    else if (ch.mode == "SPI") spiCount++;
  }
  
  for (const auto& ch : config.getI2CChannels()) {
    if (ch.active) i2cCount++;
  }
  
  Serial.println("Active channels by type:");
  Serial.printf("  DIGITAL: %d\n", digitalCount);
  Serial.printf("  ANALOG: %d\n", analogCount);
  Serial.printf("  ONEWIRE: %d\n", oneWireCount);
  Serial.printf("  SPI: %d\n", spiCount);
  Serial.printf("  I2C: %d\n", i2cCount);
}

// ============================================================================
// SECTION 30: BATCH READ WITH ERROR HANDLING
// ============================================================================
void example_batchReadWithErrorHandling() {
  // Read all active channels with error handling
  
  std::vector<int> activeChannels = config.getActiveChannelList();
  int successCount = 0;
  int errorCount = 0;
  
  Serial.println("Batch reading all active channels:");
  for (int channel : activeChannels) {
    float value = readChannel(channel);
    if (value != -1) {
      successCount++;
    } else {
      errorCount++;
      Serial.printf("  ERROR reading channel %d\n", channel);
    }
  }
  
  Serial.printf("Results: %d successful, %d errors\n", successCount, errorCount);
}

// ============================================================================
// MAIN SETUP - DEMONSTRATES TYPICAL WORKFLOW
// ============================================================================
void setup() {
  // Step 1: Initialize
  example_initialization();
  
  // Step 2: Try to load config, create default if not found
  if (!config.loadConfig()) {
    Serial.println("No config found, creating default...");
    example_createDefaultFixedChannels();
  }
  
  // Step 3: Show initial configuration
  Serial.println("\n>>> Initial Configuration:");
  example_printConfig();
  
  // Step 4: Add dynamic I2C sensors
  Serial.println("\n>>> Adding I2C sensors...");
  example_addI2C();
  
  // Step 5: Update some channels
  Serial.println("\n>>> Updating channels...");
  example_updateChannelMode();
  example_disableChannel();
  
  // Step 6: Save changes
  example_saveConfig();
  
  // Step 7: Show final config with statistics
  Serial.println("\n>>> Final Configuration:");
  example_printConfig();
  example_getActiveCount();
  example_getActiveList();
  example_countChannelsByMode();
  
  Serial.println("\n=== Setup Complete ===\n");
}

// ============================================================================
// MAIN LOOP - DEMONSTRATES CONTINUOUS READING
// ============================================================================
void loop() {
  Serial.println("\n========================================");
  Serial.println("Reading all active sensors...");
  Serial.println("========================================");
  
  // Option 1: Simple read all active
  example_readAllActive();
  
  // Option 2: Read by type (uncomment to use)
  // example_readFixedChannels();
  // example_readI2CChannels();
  
  // Option 3: Read by mode (uncomment to use)
  // example_readDigitalOnly();
  // example_readAnalogOnly();
  // example_readSPIOnly();
  
  // Option 4: Batch read with error handling (uncomment to use)
  // example_batchReadWithErrorHandling();
  
  Serial.println("========================================\n");
  
  delay(5000);  // Read every 5 seconds
}