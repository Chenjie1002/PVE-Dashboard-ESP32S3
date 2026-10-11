#include "PVEClient.h"

PVEClient::PVEClient() : 
    _host(PVE_HOST), _port(PVE_PORT), _node_name(PVE_NODE_NAME),
    _token_id(PVE_TOKEN_ID), _token_secret(PVE_TOKEN_SECRET), _net_iface(PVE_NET_IFACE),
    _vmid("100"),
    _last_net_rx(0), _last_net_tx(0), _last_net_time(0), _has_prev_net(false) {
}

bool PVEClient::begin() {
    _client.setInsecure(); // 允许自签名证书 (PVE 默认自签名证书)
    _client.setTimeout(6000); // 6000 毫秒超时
    return true;
}

void PVEClient::setConfig(const String &host, uint16_t port, const String &node, 
                          const String &token_id, const String &token_secret, const String &iface,
                          const String &vmid) {
    _host = host;
    _port = port;
    _node_name = node;
    _token_id = token_id;
    _token_secret = token_secret;
    _net_iface = iface;
    _vmid = vmid;
}

void PVEClient::formatUptime(long seconds, char *buffer, size_t max_len) {
    long days = seconds / 86400;
    long hours = (seconds % 86400) / 3600;
    long mins = (seconds % 3600) / 60;
    if (days > 0) {
        snprintf(buffer, max_len, "%ldd %02ldh", days, hours);
    } else {
        snprintf(buffer, max_len, "%02ldh %02ldm", hours, mins);
    }
}

bool PVEClient::queryNodeStatus(PVEStatus &status) {
    if (_host.length() == 0 || _token_secret.length() == 0) {
        return false;
    }

    if (!_client.connect(_host.c_str(), _port)) {
        Serial.printf("[PVE] Connect to %s:%d failed!\n", _host.c_str(), _port);
        return false;
    }

    // 构造原生 PVE REST API 请求
    String url = "/api2/json/nodes/" + _node_name + "/status";
    String authHeader = "PVEAPIToken=" + _token_id + "=" + _token_secret;

    String request = String("GET ") + url + " HTTP/1.1\r\n" +
                     "Host: " + _host + ":" + String(_port) + "\r\n" +
                     "Authorization: " + authHeader + "\r\n" +
                     "User-Agent: ESP32-S3-PVE-Monitor\r\n" +
                     "Connection: close\r\n\r\n";

    _client.print(request);

    // 读取首行状态
    String status_line = _client.readStringUntil('\n');
    Serial.printf("[PVE Status] %s\n", status_line.c_str());

    if (status_line.indexOf("200") == -1) {
        Serial.printf("[PVE] Status request returned error: %s\n", status_line.c_str());
        _client.stop();
        return false;
    }

    // 跳过其余响应头
    while (_client.connected()) {
        String line = _client.readStringUntil('\n');
        if (line == "\r" || line.length() == 0) {
            break;
        }
    }

    // 解析 JSON (PVE 自带大量状态文本，分配 16KB 确保大 JSON 不溢出)
    DynamicJsonDocument doc(16384);
    DeserializationError error = deserializeJson(doc, _client);
    _client.stop();

    if (error) {
        Serial.print(F("[PVE] deserializeJson failed: "));
        Serial.println(error.f_str());
        return false;
    }

    JsonObject data = doc["data"];
    if (data.isNull()) {
        Serial.println("[PVE] 'data' object is null in response!");
        return false;
    }

    // 1. CPU
    double cpu_val = data["cpu"] | 0.0;
    status.cpu_usage = cpu_val * 100.0;
    if (status.cpu_usage > 100.0) status.cpu_usage = 100.0;

    // 2. 内存
    status.mem_used = data["memory"]["used"] | (uint64_t)0;
    status.mem_total = data["memory"]["total"] | (uint64_t)1;
    status.mem_usage = ((double)status.mem_used / (double)status.mem_total) * 100.0;

    // 3. 根目录存储
    status.rootfs_used = data["rootfs"]["used"] | (uint64_t)0;
    status.rootfs_total = data["rootfs"]["total"] | (uint64_t)1;
    status.rootfs_usage = ((double)status.rootfs_used / (double)status.rootfs_total) * 100.0;

    // 4. 负载与运行时间
    JsonArray load = data["loadavg"];
    if (!load.isNull() && load.size() > 0) {
        status.loadavg = load[0].as<double>();
    } else {
        status.loadavg = 0.0;
    }

    status.uptime_sec = data["uptime"] | 0L;
    formatUptime(status.uptime_sec, status.uptime_str, sizeof(status.uptime_str));

    // 5. 版本
    const char *ver = data["pveversion"] | "PVE 8.1";
    const char *slash = strchr(ver, '/');
    if (slash) {
        snprintf(status.pve_version, sizeof(status.pve_version), "PVE %s", slash + 1);
        char *space = strchr(status.pve_version, '/');
        if (space) *space = '\0';
    } else {
        strncpy(status.pve_version, ver, sizeof(status.pve_version) - 1);
    }

    // 6. CPU 温度与硬盘温度解析
    status.cpu_temp = 0.0;
    status.disk_temp = 0.0;

    if (data.containsKey("cpu_temp")) {
        status.cpu_temp = data["cpu_temp"].as<double>();
    }
    if (data.containsKey("disk_temp")) {
        status.disk_temp = data["disk_temp"].as<double>();
    }

    // 检查是否存在传感器文本 (sensors_info / thermalstate / sensors)
    const char *tstate = nullptr;
    if (data.containsKey("sensors_info")) {
        tstate = data["sensors_info"];
    } else if (data.containsKey("thermalstate")) {
        tstate = data["thermalstate"];
    } else if (data.containsKey("sensors")) {
        tstate = data["sensors"];
    }

    if (tstate && strlen(tstate) > 0) {
        // 查找 CPU / Package 温度
        // 优先级: Package id 0 > Tdie > Tctl > Core 0 > cpu:
        const char *p_cpu = strstr(tstate, "Package id 0");
        if (!p_cpu) p_cpu = strstr(tstate, "Tdie");
        if (!p_cpu) p_cpu = strstr(tstate, "Tctl");
        if (!p_cpu) p_cpu = strstr(tstate, "Core 0");
        if (!p_cpu) p_cpu = strstr(tstate, "cpu:");

        if (p_cpu) {
            const char *p_colon = strchr(p_cpu, ':');
            if (p_colon) {
                const char *p_plus = strchr(p_colon, '+');
                const char *num_start = strpbrk(p_colon, "0123456789");
                if (p_plus && p_plus < p_colon + 10) {
                    num_start = p_plus + 1;
                }
                if (num_start) {
                    status.cpu_temp = atof(num_start);
                    Serial.printf("[PVE] CPU Temp parsed: %.1f C\n", status.cpu_temp);
                }
            }
        }

        // 若上述未匹配，退回备选 temp1
        if (status.cpu_temp <= 1.0) {
            const char *p_temp1 = strstr(tstate, "temp1:");
            if (p_temp1) {
                const char *p_plus = strchr(p_temp1, '+');
                if (p_plus) {
                    status.cpu_temp = atof(p_plus + 1);
                    Serial.printf("[PVE] CPU Temp fallback temp1: %.1f C\n", status.cpu_temp);
                }
            }
        }

        // 从传感器文本查找 NVMe / 硬盘传感器
        const char *p_disk = strstr(tstate, "Composite:");
        if (!p_disk) p_disk = strstr(tstate, "Sensor 1:");
        if (!p_disk) p_disk = strstr(tstate, "nvme:");
        if (!p_disk) p_disk = strstr(tstate, "drivetemp");
        if (p_disk) {
            const char *p_colon = strchr(p_disk, ':');
            if (p_colon) {
                const char *p_plus = strchr(p_colon, '+');
                const char *num_start = strpbrk(p_colon, "0123456789");
                if (p_plus && p_plus < p_colon + 10) num_start = p_plus + 1;
                if (num_start) {
                    status.disk_temp = atof(num_start);
                    Serial.printf("[PVE] Disk Temp parsed from sensors: %.1f C\n", status.disk_temp);
                }
            }
        }
    }

    // 检查硬盘 SMART 信息 (遍历键名包含 status 或 sda / nvme / disk)
    if (status.disk_temp <= 1.0) {
        for (JsonPair kv : data) {
            const char *k = kv.key().c_str();
            if (strstr(k, "_status") || strstr(k, "disk") || strstr(k, "sda") || strstr(k, "sdb") || strstr(k, "sdc") || strstr(k, "nvme")) {
                const char *v = kv.value().as<const char*>();
                if (v && strlen(v) > 0) {
                    // SATA SMART 属性 194: "Temperature_Celsius ... - <temp>"
                    // 或 属性 190: "Airflow_Temperature"
                    const char *p_tc = strstr(v, "Temperature_Celsius");
                    if (!p_tc) p_tc = strstr(v, "Airflow_Temperature");
                    if (p_tc) {
                        const char *p_dash = strstr(p_tc, " - ");
                        if (p_dash) {
                            double t = atof(p_dash + 3);
                            if (t > 0 && t < 120) {
                                if (t > status.disk_temp) status.disk_temp = t;
                                Serial.printf("[PVE] Disk Temp parsed from SMART %s: %.1f C\n", k, t);
                            }
                        }
                    }
                    // NVMe SMART: "Temperature: <temp> Celsius"
                    const char *p_nvme = strstr(v, "Temperature:");
                    if (p_nvme) {
                        const char *num_start = strpbrk(p_nvme, "0123456789");
                        if (num_start) {
                            double t = atof(num_start);
                            if (t > 0 && t < 120) {
                                if (t > status.disk_temp) status.disk_temp = t;
                                Serial.printf("[PVE] Disk Temp parsed from NVMe SMART %s: %.1f C\n", k, t);
                            }
                        }
                    }
                }
            }
        }
    }

    // 若无传感器数据，根据负载给平滑估算值作为保底显示
    if (status.cpu_temp <= 1.0) {
        status.cpu_temp = 38.0 + (status.cpu_usage * 0.3);
    }
    if (status.disk_temp <= 1.0) {
        status.disk_temp = 36.0 + (status.rootfs_usage * 0.08);
    }

    return true;
}

bool PVEClient::queryNetstat(PVEStatus &status) {
    if (_host.length() == 0 || _token_secret.length() == 0) {
        return false;
    }

    if (!_client.connect(_host.c_str(), _port)) {
        return false;
    }

    String url = "/api2/json/nodes/" + _node_name + "/netstat";
    String authHeader = "PVEAPIToken=" + _token_id + "=" + _token_secret;

    String request = String("GET ") + url + " HTTP/1.1\r\n" +
                     "Host: " + _host + ":" + String(_port) + "\r\n" +
                     "Authorization: " + authHeader + "\r\n" +
                     "User-Agent: ESP32-S3-PVE-Monitor\r\n" +
                     "Connection: close\r\n\r\n";

    _client.print(request);

    String net_status = _client.readStringUntil('\n');
    if (net_status.indexOf("200") == -1) {
        _client.stop();
        return false;
    }

    while (_client.connected()) {
        String line = _client.readStringUntil('\n');
        if (line == "\r" || line.length() == 0) {
            break;
        }
    }

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, _client);
    _client.stop();

    if (error) {
        return false;
    }

    JsonArray list = doc["data"];
    if (list.isNull()) return false;

    uint64_t curr_rx = 0;
    uint64_t curr_tx = 0;
    bool found = false;

    for (JsonObject item : list) {
        const char *dev = item["dev"] | "";
        if (strcmp(dev, _net_iface.c_str()) == 0) {
            curr_rx = item["in"] | (uint64_t)0;
            curr_tx = item["out"] | (uint64_t)0;
            found = true;
            break;
        }
    }

    unsigned long now = millis();
    if (found && _has_prev_net && (now > _last_net_time)) {
        double dt = (now - _last_net_time) / 1000.0;
        if (dt > 0.1 && curr_rx >= _last_net_rx && curr_tx >= _last_net_tx) {
            status.rx_speed_kb = ((curr_rx - _last_net_rx) / 1024.0) / dt;
            status.tx_speed_kb = ((curr_tx - _last_net_tx) / 1024.0) / dt;
        }
    }

    if (found) {
        _last_net_rx = curr_rx;
        _last_net_tx = curr_tx;
        _last_net_time = now;
        _has_prev_net = true;
    }

    return true;
}

bool PVEClient::queryQemuNetstat(PVEStatus &status) {
    if (_host.length() == 0 || _token_secret.length() == 0 || _vmid.length() == 0) {
        return false;
    }

    if (!_client.connect(_host.c_str(), _port)) {
        return false;
    }

    String url = "/api2/json/nodes/" + _node_name + "/qemu/" + _vmid + "/status/current";
    String authHeader = "PVEAPIToken=" + _token_id + "=" + _token_secret;

    String request = String("GET ") + url + " HTTP/1.1\r\n" +
                     "Host: " + _host + ":" + String(_port) + "\r\n" +
                     "Authorization: " + authHeader + "\r\n" +
                     "User-Agent: ESP32-S3-PVE-Monitor\r\n" +
                     "Connection: close\r\n\r\n";

    _client.print(request);

    String net_status = _client.readStringUntil('\n');
    if (net_status.indexOf("200") == -1) {
        Serial.printf("[PVE QEMU %s] request failed: %s\n", _vmid.c_str(), net_status.c_str());
        _client.stop();
        return false;
    }

    while (_client.connected()) {
        String line = _client.readStringUntil('\n');
        if (line == "\r" || line.length() == 0) {
            break;
        }
    }

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, _client);
    _client.stop();

    if (error) {
        Serial.printf("[PVE QEMU %s] JSON parse failed\n", _vmid.c_str());
        return false;
    }

    JsonObject data = doc["data"];
    if (data.isNull()) return false;

    uint64_t curr_in = data["netin"] | (uint64_t)0;
    uint64_t curr_out = data["netout"] | (uint64_t)0;

    unsigned long now = millis();
    if (_has_prev_net && (now > _last_net_time)) {
        double dt = (now - _last_net_time) / 1000.0;
        if (dt > 0.1 && curr_in >= _last_net_rx && curr_out >= _last_net_tx) {
            // 对直通网卡的软路由虚拟机(iKuai)，虚拟网卡连接内网/PVE host：
            // netout: 由虚拟机发往内网/宿主机的流量，即内网下行/下载 (rx_speed)
            // netin: 由内网发往虚拟机的流量，即内网上行/上传 (tx_speed)
            status.rx_speed_kb = ((curr_out - _last_net_tx) / 1024.0) / dt;
            status.tx_speed_kb = ((curr_in - _last_net_rx) / 1024.0) / dt;
            Serial.printf("[iKuai VM %s] Down: %.1f KB/s | Up: %.1f KB/s\n",
                          _vmid.c_str(), status.rx_speed_kb, status.tx_speed_kb);
        }
    }

    _last_net_rx = curr_in;
    _last_net_tx = curr_out;
    _last_net_time = now;
    _has_prev_net = true;

    return true;
}

bool PVEClient::updateStatus(PVEStatus &status) {
    bool ok = queryNodeStatus(status);
    if (ok) {
        status.online = true;
        if (_vmid.length() > 0 && _vmid != "0") {
            if (!queryQemuNetstat(status)) {
                queryNetstat(status);
            }
        } else {
            queryNetstat(status);
        }
    } else {
        status.online = false;
    }
    return ok;
}
