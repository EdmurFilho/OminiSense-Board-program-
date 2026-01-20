#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

// ============================================
// CONFIGURAÇÕES DE DEBUG
// ============================================
#define DEBUG_ENABLED 1
#if DEBUG_ENABLED
    #define DEBUG_PRINT(x) Serial.println(String("[DEBUG] ") + x)
#else
    #define DEBUG_PRINT(x)
#endif

// ============================================
// UTILITÁRIOS
// ============================================
namespace PortalUtils {
    // Escapa caracteres HTML perigosos em strings
    String escapeHTML(const String& input) {
        String output = "";
        output.reserve(input.length() * 1.2);
        
        for (unsigned int i = 0; i < input.length(); i++) {
            char c = input.charAt(i);
            switch(c) {
                case '<': output += "&lt;"; break;
                case '>': output += "&gt;"; break;
                case '&': output += "&amp;"; break;
                case '"': output += "&quot;"; break;
                case '\'': output += "&#x27;"; break;
                default: output += c;
            }
        }
        return output;
    }
}

// ============================================
// GATEWAY - PORTAL DE CONFIGURAÇÃO WIFI
// ============================================
class Gateway {
private:
    const char* ap_ssid;
    WebServer* server;
    DNSServer* dnsServer;
    IPAddress apIP;
    IPAddress netMsk;
    bool isActive;
    bool scanInProgress;
    String networkOptions; 
    void (*onCredentialsCallback)(String ssid, String password);

    const char* PORTAL_HTML = R"rawliteral(
<!DOCTYPE html>
<html lang='pt-BR'>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>
<title>OMINISENSE GATEWAY</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Courier New',monospace;background:#000;color:#00ffff;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px;background-image:repeating-linear-gradient(0deg,transparent,transparent 2px,rgba(0,255,255,.03) 2px,rgba(0,255,255,.03) 4px),repeating-linear-gradient(90deg,transparent,transparent 2px,rgba(255,0,255,.03) 2px,rgba(255,0,255,.03) 4px);}
.container{max-width:500px;width:100%;background:rgba(0,0,0,.9);border:2px solid #00ffff;clip-path:polygon(0 0,calc(100% - 20px) 0,100% 20px,100% 100%,20px 100%,0 calc(100% - 20px));padding:30px;box-shadow:0 0 40px rgba(0,255,255,.4);animation:flicker 3s infinite alternate}
@keyframes flicker{0%,100%{box-shadow:0 0 40px rgba(0,255,255,.4)}50%{box-shadow:0 0 60px rgba(255,0,255,.4)}}
.title{font-size:24px;text-align:center;text-transform:uppercase;letter-spacing:4px;text-shadow:0 0 10px #00ffff;margin-bottom:30px}
.form-group{margin-bottom:25px}
label{display:block;margin-bottom:8px;font-size:12px;color:#00ffff}
select,input{width:100%;padding:12px;background:rgba(0,20,40,.8);border:2px solid #00ffff;color:#00ffff;outline:none;font-family:inherit}
.btn{width:100%;padding:15px;background:linear-gradient(135deg,#00ffff,#ff00ff);border:none;color:#000;font-weight:700;text-transform:uppercase;cursor:pointer;clip-path:polygon(0 0,calc(100% - 15px) 0,100% 15px,100% 100%,15px 100%,0 calc(100% - 20px));font-family:inherit}
.info{font-size:10px;color:#0ff;text-align:center;margin-top:20px;opacity:0.7}
</style>
</head>
<body>
<div class='container'>
  <div class='title'>◢ OMINISENSE ◣</div>
  <form action='/save' method='POST'>
    <div class='form-group'>
      <label>◢ NETWORK SSID</label>
      <select name='ssid' required>%NETWORKS%</select>
    </div>
    <div class='form-group'>
      <label>◢ ACCESS KEY</label>
      <input type='password' name='password' placeholder='Senha do WiFi' required minlength='8' maxlength='63'>
    </div>
    <button type='submit' class='btn'>◢ ESTABLISH LINK ◣</button>
  </form>
  <div class='info'>⚡ GATEWAY v2.0 | ESP32-S3 ⚡</div>
</div>
</body>
</html>
)rawliteral";

    void startScanAsync() {
        DEBUG_PRINT("Iniciando scan WiFi assíncrono...");
        networkOptions = "<option value=''>🔍 ESCANEANDO REDES...</option>";
        WiFi.scanNetworks(true, false, false, 300);
        scanInProgress = true;
    }

    void checkScanComplete() {
        if (!scanInProgress) return;
        
        int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_RUNNING) return;
        
        if (n >= 0) {
            DEBUG_PRINT(String("Scan completo: ") + n + " redes encontradas");
            networkOptions = "<option value=''>⚡ SELECIONE A REDE...</option>";
            
            for (int i = 0; i < n; ++i) {
                String ssid = WiFi.SSID(i);
                String ssidEscaped = PortalUtils::escapeHTML(ssid);
                int rssi = WiFi.RSSI(i);
                String security = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "🔓" : "🔒";
                
                networkOptions += "<option value='" + ssidEscaped + "'>" + 
                                 security + " " + ssidEscaped + " (" + String(rssi) + " dBm)</option>";
            }
            WiFi.scanDelete();
            scanInProgress = false;
        } else {
            DEBUG_PRINT("Erro no scan WiFi");
            networkOptions = "<option value=''>❌ ERRO AO ESCANEAR</option>";
            scanInProgress = false;
        }
    }

    void handleRoot() {
        checkScanComplete();
        String html = String(PORTAL_HTML);
        html.replace("%NETWORKS%", networkOptions);
        server->send(200, "text/html", html);
        DEBUG_PRINT("Página root servida");
    }

    void handleSave() {
        if (!server->hasArg("ssid") || !server->hasArg("password")) {
            server->send(400, "text/plain", "Parâmetros inválidos");
            DEBUG_PRINT("Requisição inválida - faltam parâmetros");
            return;
        }

        String s = server->arg("ssid");
        String p = server->arg("password");
        
        if (s.length() == 0 || s.length() > 32) {
            server->send(400, "text/plain", "SSID inválido (0-32 caracteres)");
            DEBUG_PRINT("SSID inválido: " + s);
            return;
        }
        
        if (p.length() < 8 || p.length() > 63) {
            server->send(400, "text/plain", "Senha inválida (8-63 caracteres)");
            DEBUG_PRINT("Senha inválida (tamanho incorreto)");
            return;
        }
        
        DEBUG_PRINT("Credenciais válidas recebidas - SSID: " + s);
        server->send(200, "text/html", 
            "<html><body style='background:#000;color:#0f0;font-family:monospace;text-align:center;padding-top:50px;'>"
            "<h1>✓ LINK ESTABLISHED</h1><p>Reconectando ao WiFi...</p></body></html>");
        
        if (onCredentialsCallback) {
            onCredentialsCallback(s, p);
        }
    }

public:
    Gateway(const char* ssid = "OMINISENSE_GATEWAY") 
        : ap_ssid(ssid), apIP(192,168,4,1), netMsk(255,255,255,0), 
          isActive(false), scanInProgress(false), onCredentialsCallback(nullptr) {
        server = new WebServer(80);
        dnsServer = new DNSServer();
        DEBUG_PRINT("Gateway inicializado");
    }

    ~Gateway() {
        end();
        if (server) {
            delete server;
            server = nullptr;
        }
        if (dnsServer) {
            delete dnsServer;
            dnsServer = nullptr;
        }
        DEBUG_PRINT("Gateway destruído");
    }

    void begin() {
        if (isActive) {
            DEBUG_PRINT("Gateway já está ativo");
            return;
        }
        
        DEBUG_PRINT("Iniciando Gateway AP...");
        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(apIP, apIP, netMsk);
        WiFi.softAP(ap_ssid);
        
        startScanAsync();
        
        dnsServer->start(53, "*", apIP);
        MDNS.begin("ominisense");
        
        server->on("/", [this](){ this->handleRoot(); });
        server->on("/save", HTTP_POST, [this](){ this->handleSave(); });
        server->onNotFound([this](){ 
            server->sendHeader("Location", "/", true); 
            server->send(302, "text/plain", ""); 
        });
        
        server->begin();
        isActive = true;
        DEBUG_PRINT("Gateway ativo em: " + WiFi.softAPIP().toString());
    }

    void end() {
        if (!isActive) return;
        
        DEBUG_PRINT("Desligando Gateway...");
        server->stop();
        dnsServer->stop();
        
        if (scanInProgress) {
            WiFi.scanDelete();
            scanInProgress = false;
        }
        
        WiFi.softAPdisconnect(true);
        isActive = false;
        DEBUG_PRINT("Gateway desligado");
    }

    void handle() { 
        if (isActive) { 
            dnsServer->processNextRequest(); 
            server->handleClient(); 
            checkScanComplete();
        } 
    }

    void onCredentials(void (*cb)(String, String)) { 
        onCredentialsCallback = cb; 
    }

    bool active() { return isActive; }
};

// ============================================
// RAWVALUES - MONITOR DE LEITURAS BRUTAS
// ============================================
class RawValues {
private:
    WebServer* server;
    bool isActive;
    String (*getPayloadCallback)();
    IPAddress apIP;
    unsigned long lastRequestTime;
    const unsigned long REQUEST_TIMEOUT = 10000;

    const char* MONITOR_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>RAW VALUES MONITOR</title>
    <style>
        * {margin: 0;padding: 0;box-sizing: border-box}
        body {font-family: 'Courier New', monospace;background: #0a0a0a;color: #0ff;overflow-x: hidden;}
        header {background: #111;padding: 20px;border-bottom: 2px solid #f0f;text-align: center;}
        .container {padding: 20px;display: grid;grid-template-columns: repeat(auto-fill, minmax(250px, 1fr));gap: 15px;}
        .channel-card {background: #1a1a2e;border: 1px solid #0ff;padding: 15px;border-radius: 8px;transition: all 0.3s;}
        .channel-card:hover {box-shadow: 0 0 15px rgba(0,255,255,0.5);}
        .progress-bar {width: 100%;height: 15px;background: #000;border: 1px solid #0ff;margin-top: 10px;overflow: hidden;}
        .progress-fill {height: 100%;background: linear-gradient(90deg, #0ff, #f0f);transition: width 0.3s;}
        .screen {display: none} .active {display: block}
        .btn {padding: 10px 20px; background: #0ff; border: none; cursor: pointer; font-weight: bold; color: #000; margin: 5px; border-radius: 4px;}
        .btn:hover {background: #f0f;}
        .error {color: #f00;}
        .success {color: #0f0;}
    </style>
</head>
<body>
    <header>
        <h1 id="title">⚡ RAW VALUES MONITOR ⚡</h1>
        <div id="status" style="font-size: 0.8em; color: #ff0;">⏳ CONNECTING...</div>
    </header>

    <div id="monitoringScreen" class="screen active">
        <div style="text-align:center; padding: 20px;">
            <button class="btn" onclick="goToSelector()">➕ GERENCIAR CANAIS</button>
            <button class="btn" onclick="clearSelection()">🗑️ LIMPAR SELEÇÃO</button>
        </div>
        <div class="container" id="channelsContainer"></div>
    </div>

    <div id="selectorScreen" class="screen">
        <div style="text-align:center; padding: 20px;">
            <button class="btn" onclick="backToMonitoring()">✓ VOLTAR AO MONITOR</button>
        </div>
        <div class="container" id="channelGrid"></div>
    </div>

    <script>
        let selectedChannels = new Set();
        let allChannels = new Set();
        let errorCount = 0;
        const MAX_ERRORS = 5;
        const container = document.getElementById('channelsContainer');
        const grid = document.getElementById('channelGrid');
        const statusDiv = document.getElementById('status');

        function goToSelector() {
            document.getElementById('monitoringScreen').classList.remove('active');
            document.getElementById('selectorScreen').classList.add('active');
        }

        function backToMonitoring() {
            document.getElementById('selectorScreen').classList.remove('active');
            document.getElementById('monitoringScreen').classList.add('active');
        }

        function clearSelection() {
            if (confirm('Limpar todos os canais selecionados?')) {
                selectedChannels.clear();
                container.innerHTML = '';
                updateSelectorCheckboxes();
            }
        }

        function updateSelectorCheckboxes() {
            document.querySelectorAll('#channelGrid input[type="checkbox"]').forEach(cb => {
                const id = parseInt(cb.dataset.channelId);
                cb.checked = selectedChannels.has(id);
            });
        }

        async function updateLoop() {
            try {
                const res = await fetch('/data', {
                    signal: AbortSignal.timeout(5000)
                });
                
                if (!res.ok) throw new Error('HTTP ' + res.status);
                
                const data = await res.json();
                errorCount = 0;
                statusDiv.innerHTML = "<span class='success'>● LINK ACTIVE</span>";
                
                data.channels.forEach(ch => {
                    if (!allChannels.has(ch.id)) {
                        allChannels.add(ch.id);
                        const opt = document.createElement('div');
                        opt.className = 'channel-card';
                        opt.innerHTML = `<b>CH ${ch.id}</b> 
                            <input type="checkbox" data-channel-id="${ch.id}" onchange="toggleCH(${ch.id}, this.checked)" 
                            ${selectedChannels.has(ch.id) ? 'checked' : ''}>`;
                        grid.appendChild(opt);
                    }
                    
                    if (selectedChannels.has(ch.id)) {
                        let card = document.getElementById(`card-${ch.id}`);
                        if (!card) {
                            card = document.createElement('div');
                            card.id = `card-${ch.id}`;
                            card.className = 'channel-card';
                            container.appendChild(card);
                        }
                        const p = Math.min(100, (ch.val / 4095 * 100)).toFixed(1);
                        card.innerHTML = `<b>CH ${ch.id}</b> <span style="float:right">${ch.val}</span>
                                          <div class="progress-bar"><div class="progress-fill" style="width:${p}%"></div></div>`;
                    }
                });
            } catch(e) {
                errorCount++;
                console.error('Erro ao buscar dados:', e);
                statusDiv.innerHTML = `<span class='error'>● ERRO (${errorCount}/${MAX_ERRORS})</span>`;
                
                if (errorCount >= MAX_ERRORS) {
                    statusDiv.innerHTML = "<span class='error'>● CONEXÃO PERDIDA</span>";
                    return;
                }
            }
            setTimeout(updateLoop, 250);
        }

        function toggleCH(id, checked) {
            if(checked) {
                selectedChannels.add(id);
            } else {
                selectedChannels.delete(id);
                const card = document.getElementById(`card-${id}`);
                if(card) card.remove();
            }
        }

        updateLoop();
    </script>
</body>
</html>
)rawliteral";

public:
    RawValues() : isActive(false), getPayloadCallback(nullptr), 
                  apIP(192,168,5,1), lastRequestTime(0) {
        server = new WebServer(80);
        DEBUG_PRINT("RawValues inicializado");
    }

    ~RawValues() {
        end();
        if (server) {
            delete server;
            server = nullptr;
        }
        DEBUG_PRINT("RawValues destruído");
    }

    void begin(bool useAP = false) {
        if (isActive) {
            DEBUG_PRINT("RawValues já está ativo");
            return;
        }
        
        DEBUG_PRINT(useAP ? "Iniciando RawValues em modo AP..." : "Iniciando RawValues em modo STA...");
        
        if (useAP) {
            WiFi.mode(WIFI_AP);
            WiFi.softAPConfig(apIP, apIP, IPAddress(255,255,255,0));
            WiFi.softAP("OminiSense_Monitor");
            DEBUG_PRINT("AP iniciado: " + WiFi.softAPIP().toString());
        }
        
        MDNS.begin("ominisense");
        
        server->on("/", [this](){ 
            server->send(200, "text/html", MONITOR_HTML); 
            DEBUG_PRINT("Dashboard servido");
        });
        
        server->on("/data", [this](){
            lastRequestTime = millis();
            if (getPayloadCallback) {
                String payload = getPayloadCallback();
                server->send(200, "application/json", payload);
            } else {
                server->send(500, "application/json", "{\"error\":\"No data callback\"}");
                DEBUG_PRINT("ERRO: Callback de dados não configurado");
            }
        });
        
        server->begin();
        isActive = true;
        DEBUG_PRINT("RawValues ativo");
    }

    void end() {
        if (!isActive) return;
        
        DEBUG_PRINT("Desligando RawValues...");
        server->stop();
        if (WiFi.getMode() & WIFI_AP) {
            WiFi.softAPdisconnect(true);
        }
        isActive = false;
        DEBUG_PRINT("RawValues desligado");
    }

    void handle() { 
        if (isActive) {
            server->handleClient(); 
        }
    }

    void setDataCallback(String (*cb)()) { 
        getPayloadCallback = cb; 
        DEBUG_PRINT("Callback de dados configurado");
    }

    bool active() { return isActive; }
    
    unsigned long timeSinceLastRequest() {
        return isActive ? (millis() - lastRequestTime) : 0;
    }
};

#endif