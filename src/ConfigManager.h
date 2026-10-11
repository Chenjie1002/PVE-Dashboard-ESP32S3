#ifndef __CONFIG_MANAGER_H__
#define __CONFIG_MANAGER_H__

#include <Arduino.h>
#include <Preferences.h>

struct AppConfig {
    String wifi_ssid;
    String wifi_password;
    String pve_host;
    uint16_t pve_port;
    String pve_node;
    String pve_token_id;
    String pve_token_secret;
    String pve_net_iface;
    String pve_vmid; // iKuai 等直通网卡虚拟机的 VMID (如 "100")
    int theme; // 0: Bento, 1: Cyberpunk, 2: Industrial
    bool ikuai_enable;      // 是否启用 iKuai 原生外网网速接口
    String ikuai_host;      // iKuai 管理地址 (如 "10.0.0.1")
    uint16_t ikuai_port;    // iKuai HTTP 端口 (默认 80)
    String ikuai_user;      // iKuai 登录用户名 (默认 "admin")
    String ikuai_pass;      // iKuai 登录密码
};

class ConfigManager {
public:
    static bool load(AppConfig &config);
    static bool save(const AppConfig &config);
    static bool isConfigured();
    static void clear();
};

#endif // __CONFIG_MANAGER_H__
