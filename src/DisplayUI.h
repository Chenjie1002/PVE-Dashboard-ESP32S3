#ifndef __DISPLAY_UI_H__
#define __DISPLAY_UI_H__

#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <Preferences.h>
#include "PVEClient.h"
#include "fonts/custom_fonts.h"
#include "custom_symbols.h"
#include "config.h"

class DisplayUI {
public:
    DisplayUI();
    void begin();
    void showLogin();
    void showMonitor();
    void updateWiFiIP(const char *ip_str);
    void updateData(const PVEStatus &status);
    void setBrightness(uint8_t value);
    void showAPPortal(const char *ssid, const char *ip);
    void updateLoginStatus(const char *msg);
    void setTheme(int theme_id);
    void nextTheme();
    int getTheme() const { return _current_theme; }
    void loop();

private:
    TFT_eSPI _tft;
    static lv_disp_buf_t _disp_buf;
    static lv_color_t _buf[LV_HOR_RES_MAX * 20]; // 20 行缓冲区

    int _current_theme; // 0: Bento, 1: Cyberpunk, 2: Industrial
    PVEStatus _last_status;
    bool _has_last_status;
    char _last_ip[32];

    static const int CHART_POINTS = 28;
    double _target_rx_kb;
    double _target_tx_kb;
    double _current_rx_kb;
    double _current_tx_kb;
    double _chart_y_range;
    unsigned long _last_chart_flow_time;

    // 页面容器
    lv_obj_t *_login_page;
    lv_obj_t *_monitor_page;
    lv_obj_t *_login_status_label;

    // 监控页面组件
    lv_obj_t *_node_label;
    lv_obj_t *_uptime_label;

    lv_obj_t *_up_speed_label;
    lv_obj_t *_up_speed_unit;
    lv_obj_t *_down_speed_label;
    lv_obj_t *_down_speed_unit;
    lv_obj_t *_chart;
    lv_chart_series_t *_ser_up;
    lv_chart_series_t *_ser_down;
    lv_coord_t _upload_series[CHART_POINTS];
    lv_coord_t _download_series[CHART_POINTS];
    lv_coord_t _up_speed_max;
    lv_coord_t _down_speed_max;

    lv_obj_t *_cpu_bar;
    lv_obj_t *_cpu_val_label;
    lv_obj_t *_cpu_temp_label;
    lv_obj_t *_mem_bar;
    lv_obj_t *_mem_val_label;
    lv_obj_t *_disk_bar;
    lv_obj_t *_disk_val_label;
    lv_obj_t *_disk_temp_label;
    lv_obj_t *_ip_label;
    lv_obj_t *_footer_date_label;
    lv_obj_t *_footer_time_label;
    lv_obj_t *_footer_week_label;
    lv_obj_t *_footer_info_label; // 保持向后兼容

    void updateClock();
    void initStyles();
    void createLoginPage();
    void rebuildMonitorPage();
    void createMonitorPageBento();
    void createMonitorPageCyberpunk();
    void createMonitorPageIndustrial();
    void createMonitorPageMono();
    void updateSpeedLabels(double up_kb, double down_kb);
    lv_coord_t updateSeries(lv_coord_t *series, double speed);
    void flowChart();
};

#endif // __DISPLAY_UI_H__
