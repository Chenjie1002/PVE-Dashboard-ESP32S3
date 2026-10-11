#ifndef __PVE_CLIENT_H__
#define __PVE_CLIENT_H__

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "config.h"

struct PVEStatus {
    bool online;
    double cpu_usage;       // 百分比 (0.0 ~ 100.0)
    uint64_t mem_used;      // 字节
    uint64_t mem_total;     // 字节
    double mem_usage;       // 内存占用百分比 (0.0 ~ 100.0)
    uint64_t rootfs_used;   // 根存储已用字节
    uint64_t rootfs_total;  // 根存储总字节
    double rootfs_usage;    // 存储占用百分比 (0.0 ~ 100.0)
    double loadavg;         // 1分钟平均负载
    long uptime_sec;        // 运行时长 (秒)
    char uptime_str[24];    // 格式化文本 (如 "15d 08h")
    char pve_version[32];   // PVE 版本号
    double rx_speed_kb;     // 下行网速 (KB/s)
    double tx_speed_kb;     // 上行网速 (KB/s)
    double cpu_temp;        // CPU 温度 (℃)
    double disk_temp;       // 硬盘/NVMe 温度 (℃)
};

class PVEClient {
public:
    PVEClient();
    bool begin();
    void setConfig(const String &host, uint16_t port, const String &node, 
                   const String &token_id, const String &token_secret, const String &iface,
                   const String &vmid = "100");
    bool updateStatus(PVEStatus &status);

    const String& getNodeName() const { return _node_name; }

private:
    WiFiClientSecure _client;
    String _host;
    uint16_t _port;
    String _node_name;
    String _token_id;
    String _token_secret;
    String _net_iface;
    String _vmid;

    uint64_t _last_net_rx;
    uint64_t _last_net_tx;
    unsigned long _last_net_time;
    bool _has_prev_net;

    bool queryNodeStatus(PVEStatus &status);
    bool queryNetstat(PVEStatus &status);
    bool queryQemuNetstat(PVEStatus &status);
    void formatUptime(long seconds, char *buffer, size_t max_len);
};

#endif // __PVE_CLIENT_H__
