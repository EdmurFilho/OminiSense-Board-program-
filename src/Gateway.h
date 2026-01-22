#ifndef GATEWAY_H
#define GATEWAY_H

#include <WebServer.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "ConfigManager.h"

extern ConfigManager config;

class Gateway
{
private:
  WebServer server;
  const char *apSSID = "OmniSense-Setup";
  const char *apPassword = "12345678";

  // HTML Dashboard em PROGMEM
  static const char DASHBOARD_HTML[] PROGMEM;
  static const char CONFIG_HTML[] PROGMEM;
  static const char STYLES_CSS[] PROGMEM;

public:
  Gateway(int port = 80) : server(port) {}

  void begin()
  {
    // Setup WiFi AP
    WiFi.softAP(apSSID, apPassword);
    Serial.print("✓ Access Point started: ");
    Serial.print(apSSID);
    Serial.print(" | IP: ");
    Serial.println(WiFi.softAPIP());

    // Register routes
    server.on("/", HTTP_GET, [this]() { handleDashboard(); });
    server.on("/data", HTTP_GET, [this]() { handleData(); });
    server.on("/config", HTTP_GET, [this]() { handleConfigPage(); });
    server.on("/save", HTTP_POST, [this]() { handleSave(); });
    server.on("/style.css", HTTP_GET, [this]() { handleCSS(); });

    server.begin();
    Serial.println("✓ Web server started on port 80");
  }

  void handleClient()
  {
    server.handleClient();
  }

private:
  void handleDashboard()
  {
    server.send(200, "text/html", (const char *)DASHBOARD_HTML);
  }

  void handleCSS()
  {
    server.send(200, "text/css", (const char *)STYLES_CSS);
  }

  // ========== REFACTORED /data endpoint with ConfigManager integration ==========
  void handleData()
  {
    JsonDocument doc;
    JsonArray channels = doc["channels"].to<JsonArray>();

    // CRITICAL SECTION: Protect channel iteration with mutex
    if (xSemaphoreTake(config.getMutex(), portMAX_DELAY) != pdTRUE)
    {
      server.send(500, "application/json", "{\"error\":\"Mutex acquisition failed\"}");
      return;
    }

    // Iterate all 48 channels and populate with active data
    for (int i = 1; i <= 48; i++)
    {
      ChannelInfo info = config.getChannelInfo(i);

      if (info.valid)
      {
        JsonObject ch = channels.add<JsonObject>();
        ch["channel"] = i;
        ch["active"] = info.active;

        if (info.active)
        {
          // Determine value based on channel type
          if (info.mode == ChannelMode::M_ANALOG)
          {
            // Analog: send voltage directly
            ch["value"] = info.voltage;
            ch["type"] = "ANALOG";
          }
          else if (info.mode == ChannelMode::DIGITAL)
          {
            // Digital: read pin state
            ch["value"] = digitalRead(info.pin);
            ch["type"] = "DIGITAL";
          }
          else if (info.mode == ChannelMode::ONEWIRE)
          {
            ch["value"] = 0; // OneWire requires external handler
            ch["type"] = "ONEWIRE";
          }
          else if (info.mode == ChannelMode::SPI)
          {
            ch["value"] = 0; // SPI state not directly readable
            ch["type"] = "SPI";
          }
          else if (info.mode == ChannelMode::I2C)
          {
            ch["value"] = info.i2cAddress;
            ch["type"] = "I2C";
          }
        }
        else
        {
          ch["value"] = 0;
          ch["type"] = "DISABLED";
        }
      }
    }

    // Release mutex BEFORE sending response
    xSemaphoreGive(config.getMutex());

    // Send JSON response
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  }

  void handleConfigPage()
  {
    server.send(200, "text/html", (const char *)CONFIG_HTML);
  }

  // ========== REFACTORED /save endpoint with validation ==========
  void handleSave()
  {
    if (!server.hasArg("plain"))
    {
      server.send(400, "application/json", "{\"error\":\"No data provided\"}");
      return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error)
    {
      server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }

    String response = "{\"status\":\"success\",\"message\":\"";

    // Handle WiFi credentials if provided
    if (doc.containsKey("wifi_ssid") && doc.containsKey("wifi_password"))
    {
      String ssid = doc["wifi_ssid"].as<String>();
      String password = doc["wifi_password"].as<String>();
      config.setWiFiCredentials(ssid, password);
      response += "WiFi updated. ";
    }

    // Handle channel configurations if provided
    if (doc.containsKey("channels"))
    {
      JsonArray channelsArray = doc["channels"].as<JsonArray>();

      for (JsonObject ch : channelsArray)
      {
        int channelNum = ch["channel"];
        String type = ch["type"];
        bool active = ch["active"];

        if (type == "DIGITAL")
        {
          int pin = ch["pin"];
          float voltage = ch["voltage"] | 5.0;
          config.addDigital(channelNum, pin, voltage);
        }
        else if (type == "ANALOG")
        {
          int pin = ch["pin"];
          float voltage = ch["voltage"] | 3.3;
          config.addAnalog(channelNum, pin, voltage);
        }
        else if (type == "ONEWIRE")
        {
          int pin = ch["pin"];
          config.addOneWire(channelNum, pin);
        }
        else if (type == "SPI")
        {
          int csPin = ch["cs_pin"];
          config.addSPI(channelNum, csPin);
        }
        else if (type == "I2C")
        {
          uint8_t address = ch["address"];
          config.addI2C(channelNum, address);
        }

        // Handle active/inactive state
        if (!active)
        {
          config.disableChannel(channelNum);
        }
        else
        {
          config.enableChannel(channelNum);
        }
      }

      response += "Channels updated. ";
    }

    response += "\"}";

    // Save config to SD card
    if (config.saveConfig())
    {
      server.send(200, "application/json", response);
    }
    else
    {
      server.send(500, "application/json", "{\"error\":\"Failed to save config\"}");
    }
  }
};

// ========== HTML AND CSS CONSTANTS IN PROGMEM ==========

const char Gateway::DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-PT">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>OmniSense Dashboard</title>
  <link rel="stylesheet" href="/style.css">
</head>
<body>
  <div class="container">
    <h1>🎛️ OmniSense Dashboard</h1>
    <div id="channels-grid" class="grid"></div>
  </div>

  <script>
    async function loadChannels() {
      try {
        const response = await fetch('/data');
        const data = await response.json();
        const grid = document.getElementById('channels-grid');
        grid.innerHTML = '';

        data.channels.forEach(ch => {
          const card = document.createElement('div');
          card.className = `channel-card ${ch.active ? 'active' : 'inactive'}`;
          card.innerHTML = `
            <h3>Channel ${ch.channel}</h3>
            <p>Type: ${ch.type}</p>
            <p>Value: <strong>${ch.value}</strong></p>
            <p>Status: ${ch.active ? '✓ Active' : '✗ Disabled'}</p>
          `;
          grid.appendChild(card);
        });
      } catch (error) {
        console.error('Error loading channels:', error);
      }
    }

    // Load channels on page load and refresh every 1 second
    loadChannels();
    setInterval(loadChannels, 1000);
  </script>
</body>
</html>
)rawliteral";

const char Gateway::CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-PT">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>OmniSense Configuration</title>
  <link rel="stylesheet" href="/style.css">
</head>
<body>
  <div class="container">
    <h1>⚙️ Configuration</h1>
    
    <section>
      <h2>WiFi Credentials</h2>
      <form id="wifi-form">
        <input type="text" id="wifi_ssid" placeholder="SSID" required>
        <input type="password" id="wifi_password" placeholder="Password" required>
        <button type="submit">Save WiFi</button>
      </form>
    </section>

    <section>
      <h2>Channels</h2>
      <div id="channels-config"></div>
      <button onclick="addChannel()">+ Add Channel</button>
    </section>

    <button class="btn-save" onclick="saveAll()">💾 Save All</button>
  </div>

  <script>
    document.getElementById('wifi-form').addEventListener('submit', async (e) => {
      e.preventDefault();
      const ssid = document.getElementById('wifi_ssid').value;
      const password = document.getElementById('wifi_password').value;
      
      await fetch('/save', {
        method: 'POST',
        body: JSON.stringify({
          wifi_ssid: ssid,
          wifi_password: password
        })
      });
      alert('WiFi saved!');
    });

    function addChannel() {
      const config = document.getElementById('channels-config');
      const form = document.createElement('div');
      form.className = 'channel-form';
      form.innerHTML = `
        <input type="number" placeholder="Channel" min="1" max="48">
        <select>
          <option value="DIGITAL">Digital</option>
          <option value="ANALOG">Analog</option>
          <option value="ONEWIRE">One-Wire</option>
          <option value="SPI">SPI</option>
          <option value="I2C">I2C</option>
        </select>
        <input type="number" placeholder="Pin" min="0">
        <input type="number" placeholder="Voltage" step="0.1">
        <button onclick="this.parentElement.remove()">Remove</button>
      `;
      config.appendChild(form);
    }

    async function saveAll() {
      const channels = [];
      document.querySelectorAll('.channel-form').forEach(form => {
        const inputs = form.querySelectorAll('input, select');
        channels.push({
          channel: parseInt(inputs[0].value),
          type: inputs[1].value,
          pin: parseInt(inputs[2].value),
          voltage: parseFloat(inputs[3].value),
          active: true
        });
      });

      await fetch('/save', {
        method: 'POST',
        body: JSON.stringify({ channels })
      });
      alert('Configuration saved!');
    }
  </script>
</body>
</html>
)rawliteral";

const char Gateway::STYLES_CSS[] PROGMEM = R"rawliteral(
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

body {
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  min-height: 100vh;
  padding: 20px;
}

.container {
  max-width: 1000px;
  margin: 0 auto;
  background: white;
  border-radius: 10px;
  box-shadow: 0 10px 40px rgba(0,0,0,0.2);
  padding: 30px;
}

h1 {
  color: #333;
  margin-bottom: 30px;
  text-align: center;
}

h2 {
  color: #667eea;
  margin-top: 25px;
  margin-bottom: 15px;
}

.grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
  gap: 15px;
  margin-bottom: 30px;
}

.channel-card {
  border: 2px solid #ddd;
  border-radius: 8px;
  padding: 15px;
  text-align: center;
  transition: all 0.3s;
}

.channel-card.active {
  border-color: #667eea;
  background: rgba(102, 126, 234, 0.1);
}

.channel-card.inactive {
  background: rgba(200, 200, 200, 0.1);
  opacity: 0.6;
}

.channel-card h3 {
  color: #667eea;
  margin-bottom: 10px;
}

.channel-card p {
  color: #666;
  font-size: 14px;
  margin: 5px 0;
}

section {
  margin-bottom: 25px;
}

form, .channel-form {
  display: flex;
  flex-direction: column;
  gap: 10px;
}

input, select {
  padding: 10px;
  border: 1px solid #ddd;
  border-radius: 5px;
  font-size: 14px;
}

button {
  padding: 10px 20px;
  background: #667eea;
  color: white;
  border: none;
  border-radius: 5px;
  cursor: pointer;
  font-weight: bold;
  transition: background 0.3s;
}

button:hover {
  background: #764ba2;
}

.btn-save {
  width: 100%;
  padding: 15px;
  font-size: 16px;
}
)rawliteral";

#endif // GATEWAY_H
