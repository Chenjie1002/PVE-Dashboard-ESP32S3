#include "ConfigManager.h"

static const char *NVS_NAMESPACE = "pve_dash";

bool ConfigManager::load(AppConfig &config) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) { // 只读打开
        return false;
    }

    config.wifi_ssid = prefs.getString("ssid", "");
    config.wifi_password = prefs.getString("pass", "");
    config.pve_host = prefs.getString("host", "192.168.1.100");
    config.pve_port = prefs.getUShort("port", 8006);
    config.pve_node = prefs.getString("node", "pve");
    config.pve_token_id = prefs.getString("tok_id", "root@pam!dashboard");
    config.pve_token_secret = prefs.getString("tok_sec", "");
    config.pve_net_iface = prefs.getString("iface", "vmbr0");
    config.pve_vmid = prefs.getString("vmid", "100");
    config.theme = prefs.getInt("theme", 0);
    config.ikuai_enable = prefs.getBool("ik_en", true);
    config.ikuai_host = prefs.getString("ik_host", "10.0.0.1");
    config.ikuai_port = prefs.getUShort("ik_port", 80);
    config.ikuai_user = prefs.getString("ik_user", "admin");
    config.ikuai_pass = prefs.getString("ik_pass", "");

    prefs.end();

    // 如果未配置有效 WiFi 或 Token Secret，视为未初始化
    return (config.wifi_ssid.length() > 0 && config.pve_token_secret.length() > 0);
}

bool ConfigManager::save(const AppConfig &config) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) { // 读写打开
        return false;
    }

    prefs.putString("ssid", config.wifi_ssid);
    prefs.putString("pass", config.wifi_password);
    prefs.putString("host", config.pve_host);
    prefs.putUShort("port", config.pve_port);
    prefs.putString("node", config.pve_node);
    prefs.putString("tok_id", config.pve_token_id);
    prefs.putString("tok_sec", config.pve_token_secret);
    prefs.putString("iface", config.pve_net_iface);
    prefs.putString("vmid", config.pve_vmid);
    prefs.putInt("theme", config.theme);
    prefs.putBool("ik_en", config.ikuai_enable);
    prefs.putString("ik_host", config.ikuai_host);
    prefs.putUShort("ik_port", config.ikuai_port);
    prefs.putString("ik_user", config.ikuai_user);
    prefs.putString("ik_pass", config.ikuai_pass);

    prefs.end();
    return true;
}

bool ConfigManager::isConfigured() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {
        return false;
    }
    String ssid = prefs.getString("ssid", "");
    String sec = prefs.getString("tok_sec", "");
    prefs.end();
    return (ssid.length() > 0 && sec.length() > 0);
}

void ConfigManager::clear() {
    Preferences prefs;
    if (prefs.begin(NVS_NAMESPACE, false)) {
        prefs.clear();
        prefs.end();
    }
}
