#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <lvgl.h>
#include "config.h"
#include "DisplayUI.h"
#include "PVEClient.h"
#include "ConfigManager.h"
#include "WebPortal.h"
#include "IKuaiClient.h"

static DisplayUI ui;
static PVEClient pve;
static IKuaiClient ikuai;
static PVEStatus pve_status;
static AppConfig app_config;
static WebPortal portal;

static bool portal_mode = false;
static volatile bool wifi_connected = false;
static unsigned long last_poll_time = 0;
static unsigned long wifi_lost_time = 0;

static PVEStatus shared_pve_status;
static volatile bool new_data_available = false;
static SemaphoreHandle_t data_mutex = NULL;

void networkPollTask(void *pvParameters) {
    while (1) {
        if (wifi_connected && !portal_mode) {
            PVEStatus temp_status;
            bool success = pve.updateStatus(temp_status);
            if (success) {
                if (app_config.ikuai_enable && ikuai.isConfigured()) {
                    double ik_rx = 0, ik_tx = 0;
                    if (ikuai.updateSpeed(ik_rx, ik_tx)) {
                        temp_status.rx_speed_kb = ik_rx;
                        temp_status.tx_speed_kb = ik_tx;
                    }
                }

                Serial.printf("[PVE] CPU: %.1f%% (%0.1fC) | RAM: %.1f%% | DISK: %.1f%% (%0.1fC) | LOAD: %.2f\n",
                              temp_status.cpu_usage,
                              temp_status.cpu_temp,
                              temp_status.mem_usage,
                              temp_status.rootfs_usage,
                              temp_status.disk_temp,
                              temp_status.loadavg);

                if (data_mutex && xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    shared_pve_status = temp_status;
                    new_data_available = true;
                    xSemaphoreGive(data_mutex);
                }
            } else {
                Serial.println("[PVE] Poll node status failed or PVE host unreachable.");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(PVE_POLL_INTERVAL_MS));
    }
}

void startPortalMode() {
    Serial.println("[Main] Entering AP WebPortal mode...");
    portal_mode = true;
    wifi_connected = false;
    portal.begin();
    ui.showAPPortal("PVE-Monitor-AP", "192.168.4.1");
}

void tryConnectWiFi() {
    Serial.printf("[WiFi] Connecting to saved SSID: %s...\n", app_config.wifi_ssid.c_str());
    ui.updateLoginStatus("Connecting WiFi...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(app_config.wifi_ssid.c_str(), app_config.wifi_password.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        lv_task_handler(); // 保持界面动画渲染
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Connected successfully!");
        Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
        wifi_connected = true;
        portal_mode = false;

        // 同步 NTP 网络时间 (UTC+8: 8 * 3600 秒, 夏令时 0)
        configTime(8 * 3600, 0, "ntp.aliyun.com", "cn.pool.ntp.org", "pool.ntp.org");
        Serial.println("[NTP] Initialized SNTP (UTC+8)");

        ui.updateWiFiIP(WiFi.localIP().toString().c_str());
        pve.setConfig(app_config.pve_host, app_config.pve_port, app_config.pve_node,
                      app_config.pve_token_id, app_config.pve_token_secret, app_config.pve_net_iface,
                      app_config.pve_vmid);
        pve.begin();

        if (app_config.ikuai_enable && app_config.ikuai_host.length() > 0) {
            ikuai.setConfig(app_config.ikuai_host, app_config.ikuai_port, app_config.ikuai_user, app_config.ikuai_pass);
            Serial.printf("[iKuai] Configured: http://%s:%d (user: %s)\n",
                          app_config.ikuai_host.c_str(), app_config.ikuai_port, app_config.ikuai_user.c_str());
        }

        // 启动局域网后台 Web 控制台
        portal.beginStaServer();

        ui.showMonitor();
    } else {
        Serial.println("\n[WiFi] Connect failed! Fallback to AP Portal...");
        startPortalMode();
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=================================");
    Serial.println("  ESP32-S3 Super Mini PVE Monitor ");
    Serial.println("=================================");

    // 初始化屏幕与 LVGL 布局
    Serial.println("[Init] Initializing Display UI...");
    ui.begin();
    Serial.println("[Init] Display UI initialized successfully.");

    pinMode(0, INPUT_PULLUP); // BOOT 按键 (GPIO 0) 用于切换主题

    // 从 NVS 读取用户配置
    bool has_config = ConfigManager::load(app_config);

    data_mutex = xSemaphoreCreateMutex();
    // 启动 Core 0 异步后台网络轮询任务 (8KB 栈空间，优先级 1，独占 Core 0)
    xTaskCreatePinnedToCore(networkPollTask, "NetPollTask", 8192, NULL, 1, NULL, 0);

    if (!has_config) {
        Serial.println("[Config] No valid WiFi/PVE config in NVS. Starting AP WebPortal...");
        startPortalMode();
    } else {
        Serial.printf("[Config] Loaded NVS config for SSID: %s, Host: %s\n", 
                      app_config.wifi_ssid.c_str(), app_config.pve_host.c_str());
        ui.setTheme(app_config.theme);
        tryConnectWiFi();
    }
}

void loop() {
    // 驱动 LVGL 内部渲染与动画
    lv_task_handler();
    ui.loop(); // 驱动折线图高帧率丝滑流水推进与平滑插值

    // 检测 BOOT 按键 (GPIO 0) 单击快速切换主题
    static unsigned long btn_press_time = 0;
    static bool btn_was_down = false;
    if (digitalRead(0) == LOW) {
        if (!btn_was_down) {
            btn_was_down = true;
            btn_press_time = millis();
        }
    } else {
        if (btn_was_down) {
            if (millis() - btn_press_time > 50 && millis() - btn_press_time < 2000) {
                Serial.printf("[Button] BOOT pressed! Switching theme from %d...\n", ui.getTheme());
                ui.nextTheme();
                Serial.printf("[Button] Switched to theme: %d\n", ui.getTheme());
            }
            btn_was_down = false;
        }
    }

    if (portal_mode) {
        portal.handleClient();
        static unsigned long last_portal_log = 0;
        if (millis() - last_portal_log > 3000) {
            last_portal_log = millis();
            Serial.printf("[Portal] AP active: PVE-Monitor-AP (192.168.4.1), Clients connected: %d\n",
                          WiFi.softAPgetStationNum());
        }
        delay(5);
        return;
    }

    // 响应局域网 Web 控制台 HTTP 请求
    portal.handleClient();

    unsigned long now = millis();

    // 检查 WiFi 状态与重连
    if (WiFi.status() != WL_CONNECTED) {
        if (wifi_connected) {
            Serial.println("[WiFi] Lost connection, attempting reconnect...");
            wifi_connected = false;
            wifi_lost_time = now;
            ui.showLogin();
            ui.updateLoginStatus("WiFi Disconnected. Reconnecting...");
        }

        // 掉线超过 40 秒，自动退回热点配网模式
        if (now - wifi_lost_time > 40000) {
            Serial.println("[WiFi] Reconnect timeout. Fallback to AP Portal.");
            startPortalMode();
            return;
        }

        if (now - last_poll_time > 5000) {
            last_poll_time = now;
            WiFi.reconnect();
        }
        delay(5);
        return;
    } else if (!wifi_connected) {
        wifi_connected = true;
        configTime(8 * 3600, 0, "ntp.aliyun.com", "cn.pool.ntp.org", "pool.ntp.org");
        ui.updateWiFiIP(WiFi.localIP().toString().c_str());
        ui.showMonitor();
    }

    // 检查是否有 Core 0 异步获取回来的最新状态数据
    if (new_data_available) {
        PVEStatus status_copy;
        bool has_copy = false;
        if (data_mutex && xSemaphoreTake(data_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            status_copy = shared_pve_status;
            new_data_available = false;
            has_copy = true;
            xSemaphoreGive(data_mutex);
        }
        if (has_copy) {
            ui.updateData(status_copy);
        }
    }

    delay(2);
}
