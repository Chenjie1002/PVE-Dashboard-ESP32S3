#include "WebPortal.h"

static const byte DNS_PORT = 53;
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_NETMASK(255, 255, 255, 0);

WebPortal::WebPortal() : _server(80), _is_ap_mode(false), _reboot_pending(false), _reboot_time(0) {
}

void WebPortal::scanNetworks() {
    Serial.println("[WebPortal] Scanning WiFi networks...");
    int n = WiFi.scanNetworks();
    _scanned_ssids = "";
    if (n > 0) {
        for (int i = 0; i < n; ++i) {
            String ssid = WiFi.SSID(i);
            if (ssid.length() > 0 && _scanned_ssids.indexOf(ssid) == -1) {
                _scanned_ssids += "<option value=\"" + ssid + "\">" + ssid + " (" + String(WiFi.RSSI(i)) + "dBm)</option>";
            }
        }
    }
}

void WebPortal::begin() {
    Serial.println("[WebPortal] Starting SoftAP mode...");
    _is_ap_mode = true;
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(AP_IP, AP_IP, AP_NETMASK);
    WiFi.softAP("PVE-Monitor-AP");

    Serial.printf("[WebPortal] AP IP: %s\n", WiFi.softAPIP().toString().c_str());

    // 扫描网络供下拉选择
    scanNetworks();

    // 启动 DNS 劫持服务器
    _dns_server.start(DNS_PORT, "*", AP_IP);

    // 绑定 HTTP 路由
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/save", HTTP_POST, [this]() { handleSave(); });

    // Captive Portal 常见探测 URL 重定向
    _server.on("/generate_204", HTTP_GET, [this]() { handleCaptivePortal(); });
    _server.on("/hotspot-detect.html", HTTP_GET, [this]() { handleCaptivePortal(); });
    _server.on("/canonical.html", HTTP_GET, [this]() { handleCaptivePortal(); });
    _server.on("/connecttest.txt", HTTP_GET, [this]() { handleCaptivePortal(); });
    _server.on("/ncsi.txt", HTTP_GET, [this]() { handleCaptivePortal(); });
    _server.onNotFound([this]() { handleCaptivePortal(); });

    _server.begin();
    Serial.println("[WebPortal] WebServer & DNSServer started on port 80 & 53.");
}

void WebPortal::beginStaServer() {
    _is_ap_mode = false;
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/save", HTTP_POST, [this]() { handleSave(); });
    _server.begin();
    Serial.printf("[WebPortal] Background Web Dashboard started on http://%s:80\n",
                  WiFi.localIP().toString().c_str());
}

void WebPortal::handleCaptivePortal() {
    _server.sendHeader("Location", String("http://192.168.4.1/"), true);
    _server.send(302, "text/plain", "");
}

void WebPortal::handleRoot() {
    AppConfig cfg;
    ConfigManager::load(cfg);

    String html = R"rawliteral(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>PVE Monitor 配网控制台</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: #0b0e14;
    color: #e6edf3;
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
    padding: 16px;
    display: flex;
    justify-content: center;
  }
  .card {
    background: #151b23;
    border: 1px solid #30363d;
    border-radius: 12px;
    padding: 22px;
    width: 100%;
    max-width: 440px;
    box-shadow: 0 10px 30px rgba(0,0,0,0.5);
  }
  h2 {
    font-size: 19px;
    color: #58a6ff;
    margin-bottom: 6px;
    display: flex;
    align-items: center;
    gap: 8px;
  }
  .subtitle {
    font-size: 12px;
    color: #8b949e;
    margin-bottom: 20px;
  }
  .section-title {
    font-size: 13px;
    font-weight: 600;
    color: #3fb950;
    margin-top: 14px;
    margin-bottom: 8px;
    text-transform: uppercase;
    letter-spacing: 0.5px;
    border-bottom: 1px solid #21262d;
    padding-bottom: 4px;
  }
  .field {
    margin-bottom: 12px;
  }
  label {
    display: block;
    font-size: 12px;
    color: #c9d1d9;
    margin-bottom: 4px;
    font-weight: 500;
  }
  input, select {
    width: 100%;
    background: #0d1117;
    border: 1px solid #30363d;
    border-radius: 6px;
    padding: 9px 12px;
    color: #f0f6fc;
    font-size: 14px;
    outline: none;
    transition: border-color 0.2s;
  }
  input:focus, select:focus {
    border-color: #58a6ff;
    box-shadow: 0 0 0 2px rgba(88,166,255,0.2);
  }
  .hint {
    font-size: 11px;
    color: #8b949e;
    margin-top: 3px;
  }
  .btn {
    width: 100%;
    background: #238636;
    color: #ffffff;
    border: none;
    border-radius: 6px;
    padding: 12px;
    font-size: 15px;
    font-weight: 600;
    cursor: pointer;
    margin-top: 18px;
    transition: background 0.15s;
  }
  .btn:hover { background: #2ea043; }
</style>
</head>
<body>
<div class="card">
  <h2>● PVE Dashboard 配网</h2>
  <div class="subtitle">ESP32-S3 Super Mini + 320x240 横屏监控</div>

  <form method="POST" action="/save">
    <div class="section-title">1. WiFi 无线网络设置</div>
    <div class="field">
      <label>周边扫描 WiFi (下拉选择):</label>
      <select onchange="document.getElementById('ssid').value=this.value">
        <option value="">-- 选择扫描到的 WiFi --</option>
)rawliteral";

    html += _scanned_ssids;

    html += R"rawliteral(
      </select>
    </div>
    <div class="field">
      <label>WiFi 名称 (SSID):</label>
      <input type="text" id="ssid" name="ssid" value=")rawliteral";
    html += cfg.wifi_ssid;
    html += R"rawliteral(" required placeholder="例如: MyHome_2.4G">
    </div>
    <div class="field">
      <label>WiFi 密码:</label>
      <input type="password" name="pass" value=")rawliteral";
    html += cfg.wifi_password;
    html += R"rawliteral(" placeholder="请输入 WiFi 密码">
    </div>

    <div class="section-title">2. PVE 宿主机连接</div>
    <div class="field">
      <label>PVE IP 地址:</label>
      <input type="text" name="host" value=")rawliteral";
    html += cfg.pve_host;
    html += R"rawliteral(" required placeholder="例如: 192.168.1.100">
    </div>
    <div class="field">
      <label>PVE 端口 (默认 8006):</label>
      <input type="number" name="port" value=")rawliteral";
    html += String(cfg.pve_port);
    html += R"rawliteral(" required>
    </div>
    <div class="field">
      <label>节点名称 (Node Name):</label>
      <input type="text" name="node" value=")rawliteral";
    html += cfg.pve_node;
    html += R"rawliteral(" required placeholder="例如: pve">
    </div>
    <div class="field">
      <label>监控网卡 (默认 vmbr0):</label>
      <input type="text" name="iface" value=")rawliteral";
    html += cfg.pve_net_iface;
    html += R"rawliteral(" required placeholder="例如: vmbr0">
    </div>
    <div class="field">
      <label>iKuai / 路由虚拟机 VMID (直通网卡必填):</label>
      <input type="text" name="vmid" value=")rawliteral";
    html += (cfg.pve_vmid.length() > 0 ? cfg.pve_vmid : "100");
    html += R"rawliteral(" placeholder="例如: 100 (填 0 则使用上方宿主机网卡)">
      <div class="hint">物理网卡直通给 iKuai 时填写其 VMID (如 100)，自动获取该虚拟机的网络吞吐流量</div>
    </div>

    <div class="section-title">3. PVE API Token 鉴权</div>
    <div class="field">
      <label>Token ID (默认 root@pam!dashboard):</label>
      <input type="text" name="tok_id" value=")rawliteral";
    html += cfg.pve_token_id;
    html += R"rawliteral(" required placeholder="例如: root@pam!dashboard">
    </div>
    <div class="field">
      <label>Token 密钥 Secret (UUID):</label>
      <input type="text" name="tok_sec" value=")rawliteral";
    html += cfg.pve_token_secret;
    html += R"rawliteral(" required placeholder="例如: c3829023-xxxx-xxxx-xxxx-xxxxxxxxxxxx">
      <div class="hint">在 PVE 终端执行: pveum user token add root@pam dashboard --privsep 0</div>
    </div>

    <div class="section-title">4. iKuai 原生外网网速直连 (推荐)</div>
    <div class="field">
      <label>启用 iKuai 真实 WAN 流量直连:</label>
      <select name="ik_en">
        <option value="1")rawliteral";
    if (cfg.ikuai_enable) html += " selected";
    html += R"rawliteral(>启用 (推荐: 直接获取 iKuai 真实外网上下行速度)</option>
        <option value="0")rawliteral";
    if (!cfg.ikuai_enable) html += " selected";
    html += R"rawliteral(>禁用 (使用 PVE VM 估算流量)</option>
      </select>
    </div>
    <div class="field">
      <label>iKuai 路由管理地址 (IP):</label>
      <input type="text" name="ik_host" value=")rawliteral";
    html += (cfg.ikuai_host.length() > 0 ? cfg.ikuai_host : "10.0.0.1");
    html += R"rawliteral(" placeholder="例如: 10.0.0.1">
    </div>
    <div class="field">
      <label>iKuai HTTP 端口 (默认 80):</label>
      <input type="number" name="ik_port" value=")rawliteral";
    html += String(cfg.ikuai_port > 0 ? cfg.ikuai_port : 80);
    html += R"rawliteral(">
    </div>
    <div class="field">
      <label>iKuai 管理员用户名:</label>
      <input type="text" name="ik_user" value=")rawliteral";
    html += (cfg.ikuai_user.length() > 0 ? cfg.ikuai_user : "admin");
    html += R"rawliteral(">
    </div>
    <div class="field">
      <label>iKuai 登录密码:</label>
      <input type="password" name="ik_pass" value=")rawliteral";
    html += cfg.ikuai_pass;
    html += R"rawliteral(" placeholder="请输入 iKuai Web 管理密码">
      <div class="hint">填写后将直连 iKuai API，屏幕网速与 iKuai 首页仪表盘 100% 同步！</div>
    </div>

    <div class="section-title">5. 界面主题风格 (可随时短按板载 BOOT 键轮换)</div>
    <div class="field">
      <label>默认启动主题:</label>
      <select name="theme">
        <option value="0")rawliteral";
    if (cfg.theme == 0) html += " selected";
    html += R"rawliteral(>风格二：Apple Bento Grid (现代极简卡片 · 纯黑背景 · 加粗条)</option>
        <option value="1")rawliteral";
    if (cfg.theme == 1) html += " selected";
    html += R"rawliteral(>风格一：Cyberpunk HUD (赛博科幻机甲 · 荧光霓虹)</option>
        <option value="2")rawliteral";
    if (cfg.theme == 2) html += " selected";
    html += R"rawliteral(>风格三：Industrial Telemetry (工业遥测终端 · 硬核黑金)</option>
        <option value="3")rawliteral";
    if (cfg.theme == 3) html += " selected";
    html += R"rawliteral(>风格四：Monochrome Telemetry (纯黑白极简仪器仪表风)</option>
      </select>
      <div class="hint">硬件运行中，随时按一下 ESP32-S3 上的 BOOT 按键 (GPIO 0) 即可无感即时切换！</div>
    </div>

    <button type="submit" class="btn">保存并启动监控</button>
  </form>
</div>
</body>
</html>)rawliteral";

    _server.send(200, "text/html", html);
}

void WebPortal::handleSave() {
    AppConfig cfg;
    cfg.wifi_ssid = _server.arg("ssid");
    cfg.wifi_password = _server.arg("pass");
    cfg.pve_host = _server.arg("host");
    cfg.pve_port = _server.arg("port").toInt();
    if (cfg.pve_port == 0) cfg.pve_port = 8006;
    cfg.pve_node = _server.arg("node");
    cfg.pve_token_id = _server.arg("tok_id");
    cfg.pve_token_secret = _server.arg("tok_sec");
    cfg.pve_net_iface = _server.arg("iface");
    cfg.pve_vmid = _server.arg("vmid");
    if (cfg.pve_vmid.length() == 0) cfg.pve_vmid = "100";
    cfg.theme = _server.arg("theme").toInt();
    if (cfg.theme < 0 || cfg.theme > 3) cfg.theme = 0;

    cfg.ikuai_enable = (_server.arg("ik_en") == "1");
    cfg.ikuai_host = _server.arg("ik_host");
    if (cfg.ikuai_host.length() == 0) cfg.ikuai_host = "10.0.0.1";
    cfg.ikuai_port = _server.arg("ik_port").toInt();
    if (cfg.ikuai_port == 0) cfg.ikuai_port = 80;
    cfg.ikuai_user = _server.arg("ik_user");
    if (cfg.ikuai_user.length() == 0) cfg.ikuai_user = "admin";
    cfg.ikuai_pass = _server.arg("ik_pass");

    ConfigManager::save(cfg);
    Serial.println("[WebPortal] Configuration saved successfully to NVS!");

    String resp = R"rawliteral(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>保存成功</title>
<style>
  body {
    background: #0b0e14;
    color: #e6edf3;
    font-family: -apple-system, sans-serif;
    display: flex;
    align-items: center;
    justify-content: center;
    height: 100vh;
    margin: 0;
    text-align: center;
  }
  .box {
    background: #151b23;
    border: 1px solid #30363d;
    border-radius: 12px;
    padding: 30px;
    max-width: 360px;
  }
  h2 { color: #3fb950; margin-bottom: 12px; }
  p { font-size: 14px; color: #8b949e; line-height: 1.6; }
</style>
</head>
<body>
<div class="box">
  <h2>✓ 配置已保存！</h2>
  <p>ESP32 正在自动重启并连接路由器与 PVE 宿主机...<br>请留意屏幕监控界面。</p>
</div>
</body>
</html>)rawliteral";

    _server.send(200, "text/html", resp);

    _reboot_pending = true;
    _reboot_time = millis() + 1500;
}

void WebPortal::handleClient() {
    if (_is_ap_mode) {
        _dns_server.processNextRequest();
    }
    _server.handleClient();

    if (_reboot_pending && millis() > _reboot_time) {
        Serial.println("[WebPortal] Rebooting to apply new configuration...");
        delay(200);
        ESP.restart();
    }
}
