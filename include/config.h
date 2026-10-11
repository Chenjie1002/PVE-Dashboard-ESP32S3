#ifndef __CONFIG_H__
#define __CONFIG_H__

// ==========================================
// 1. WiFi 配置
// ==========================================
#define WIFI_SSID       "YOUR_WIFI_SSID"        // 请替换为你的 WiFi 名称
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"    // 请替换为你的 WiFi 密码

// ==========================================
// 2. PVE 8.1.4 连接与 API Token 配置
// ==========================================
// PVE 宿主机 IP 与端口 (默认 HTTPS 8006)
#define PVE_HOST        "192.168.1.100"         // 请替换为你的 PVE IP 地址
#define PVE_PORT        8006                    // PVE 默认 Web API 端口 8006

// 监控的目标节点名称 (例如 "pve")
#define PVE_NODE_NAME   "pve"

// PVE API Token 鉴权格式:
// Authorization: PVEAPIToken=USER@REALM!TOKENID=UUID
// 生成方法 (在 PVE 终端执行):
//   pveum user token add root@pam dashboard --privsep 0
// 生成后会显示:
//   ┌──────────────┬──────────────────────────────────────┐
//   │ key          │ value                                │
//   ├──────────────┼──────────────────────────────────────┤
//   │ full-tokenid │ root@pam!dashboard                   │
//   │ value        │ xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx │
//   └──────────────┴──────────────────────────────────────┘
#define PVE_TOKEN_ID     "root@pam!dashboard"
#define PVE_TOKEN_SECRET "YOUR-PVE-API-TOKEN-UUID"

// 监控网卡接口名称 (PVE 默认为 vmbr0 桥接网卡，也可以是 eno1, eth0 等)
#define PVE_NET_IFACE   "vmbr0"

// ==========================================
// 3. 运行与刷新参数
// ==========================================
#define PVE_POLL_INTERVAL_MS   2000    // PVE API 状态轮询间隔 (毫秒)
#define SCREEN_BRIGHTNESS      200     // 默认背光亮度 (0~255)

// ==========================================
// 4. ESP32-S3 Super Mini 硬件引脚参考 (定义在 platformio.ini 中)
// ==========================================
// TFT_MOSI : GPIO 5  (侧边排针 IO5)
// TFT_SCLK : GPIO 7  (侧边排针 IO7)
// TFT_CS   : GPIO 4  (侧边排针 IO4, 若屏幕无 CS 引脚接地即可)
// TFT_DC   : GPIO 6  (侧边排针 IO6)
// TFT_RST  : GPIO 1  (侧边排针 IO1)
// TFT_BL   : GPIO 2  (侧边排针 IO2, 背光 PWM 调光)
#define PIN_TFT_BL      2

#endif // __CONFIG_H__
