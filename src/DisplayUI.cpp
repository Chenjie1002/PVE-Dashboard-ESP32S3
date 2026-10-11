#include "DisplayUI.h"

lv_disp_buf_t DisplayUI::_disp_buf;
lv_color_t DisplayUI::_buf[LV_HOR_RES_MAX * 20];

static TFT_eSPI *tft_ptr = nullptr;

static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    if (tft_ptr) {
        uint32_t w = (area->x2 - area->x1 + 1);
        uint32_t h = (area->y2 - area->y1 + 1);
        tft_ptr->startWrite();
        tft_ptr->setAddrWindow(area->x1, area->y1, w, h);
        tft_ptr->pushColors((uint16_t *)&color_p->full, w * h, false);
        tft_ptr->endWrite();
    }
    lv_disp_flush_ready(disp);
}

static lv_style_t style_kh_22;
static lv_style_t style_kh_16;
static lv_style_t style_f1_bold;
static lv_style_t style_f1_reg;
static lv_style_t style_icon;
static lv_style_t style_sub_10;
static lv_style_t style_sub_12;
static bool styles_inited = false;

DisplayUI::DisplayUI() :
    _tft(), _current_theme(0), _has_last_status(false),
    _target_rx_kb(0.0), _target_tx_kb(0.0), _current_rx_kb(0.0), _current_tx_kb(0.0),
    _chart_y_range(64.0), _last_chart_flow_time(0),
    _login_page(nullptr), _monitor_page(nullptr), _login_status_label(nullptr),
    _up_speed_max(16), _down_speed_max(16) {
    tft_ptr = &_tft;
    memset(_upload_series, 0, sizeof(_upload_series));
    memset(_download_series, 0, sizeof(_download_series));
    memset(&_last_status, 0, sizeof(_last_status));
    _last_ip[0] = '\0';
}

void DisplayUI::setBrightness(uint8_t value) {
    pinMode(PIN_TFT_BL, OUTPUT);
    analogWrite(PIN_TFT_BL, value);
}

void DisplayUI::initStyles() {
    if (styles_inited) return;
    lv_style_init(&style_kh_22);
    lv_style_set_text_font(&style_kh_22, LV_STATE_DEFAULT, &kh_interference_bold_22);

    lv_style_init(&style_kh_16);
    lv_style_set_text_font(&style_kh_16, LV_STATE_DEFAULT, &kh_interference_bold_16);

    lv_style_init(&style_f1_bold);
    lv_style_set_text_font(&style_f1_bold, LV_STATE_DEFAULT, &formula1_bold_14);

    lv_style_init(&style_f1_reg);
    lv_style_set_text_font(&style_f1_reg, LV_STATE_DEFAULT, &formula1_reg_14);

    lv_style_init(&style_icon);
    lv_style_set_text_font(&style_icon, LV_STATE_DEFAULT, &iconfont_symbol);

    lv_style_init(&style_sub_10);
    lv_style_set_text_font(&style_sub_10, LV_STATE_DEFAULT, &lv_font_montserrat_10);

    lv_style_init(&style_sub_12);
    lv_style_set_text_font(&style_sub_12, LV_STATE_DEFAULT, &lv_font_montserrat_12);

    styles_inited = true;
}

void DisplayUI::begin() {
    setBrightness(SCREEN_BRIGHTNESS);

    _tft.begin();
    _tft.setRotation(1); // 横屏 320x240 (ST7789 rotation 1)
    _tft.invertDisplay(true);

    lv_init();
    lv_disp_buf_init(&_disp_buf, _buf, NULL, LV_HOR_RES_MAX * 20);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 320;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.buffer = &_disp_buf;
    lv_disp_drv_register(&disp_drv);

    initStyles();

    // 从 NVS 读取用户保存的主题 (默认 0: Bento Grid)
    Preferences prefs;
    if (prefs.begin("pve_dash", true)) {
        _current_theme = prefs.getInt("theme", 0);
        prefs.end();
    }

    createLoginPage();

    _monitor_page = lv_cont_create(lv_scr_act(), NULL);
    lv_obj_set_size(_monitor_page, 320, 240);
    lv_obj_set_style_local_border_width(_monitor_page, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_radius(_monitor_page, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);

    rebuildMonitorPage();
    showLogin();
}

void DisplayUI::setTheme(int theme_id) {
    if (theme_id < 0 || theme_id > 3) theme_id = 0;
    _current_theme = theme_id;
    rebuildMonitorPage();

    Preferences prefs;
    if (prefs.begin("pve_dash", false)) {
        prefs.putInt("theme", _current_theme);
        prefs.end();
    }
}

void DisplayUI::nextTheme() {
    setTheme((_current_theme + 1) % 4);
}

void DisplayUI::rebuildMonitorPage() {
    if (!_monitor_page) return;
    lv_obj_clean(_monitor_page);

    switch (_current_theme) {
        case 1:
            createMonitorPageCyberpunk();
            break;
        case 2:
            createMonitorPageIndustrial();
            break;
        case 3:
            createMonitorPageMono();
            break;
        case 0:
        default:
            createMonitorPageBento();
            break;
    }

    if (strlen(_last_ip) > 0) {
        updateWiFiIP(_last_ip);
    }
    if (_has_last_status) {
        updateData(_last_status);
    }
}

void DisplayUI::createLoginPage() {
    _login_page = lv_cont_create(lv_scr_act(), NULL);
    lv_obj_set_size(_login_page, 320, 240);
    lv_obj_set_style_local_bg_color(_login_page, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x000000));
    lv_obj_set_style_local_border_width(_login_page, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_radius(_login_page, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);

    // 加载动画圈
    static lv_style_t spinner_style;
    lv_style_init(&spinner_style);
    lv_style_set_line_width(&spinner_style, LV_STATE_DEFAULT, 6);
    lv_style_set_line_color(&spinner_style, LV_STATE_DEFAULT, lv_color_hex(0x00d2ff));

    lv_obj_t *preload = lv_spinner_create(_login_page, NULL);
    lv_obj_set_size(preload, 70, 70);
    lv_obj_align(preload, NULL, LV_ALIGN_CENTER, 0, -25);
    lv_obj_add_style(preload, LV_SPINNER_PART_INDIC, &spinner_style);

    _login_status_label = lv_label_create(_login_page, NULL);
    lv_label_set_text(_login_status_label, "Connecting WiFi & PVE...");
    lv_obj_set_style_local_text_color(_login_status_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x8da0b8));
    lv_obj_align(_login_status_label, preload, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
}

// ========================================================
// 风格二：Apple Bento Grid (现代极简卡片风 · 黑色背景 · 粗条)
// ========================================================
void DisplayUI::createMonitorPageBento() {
    lv_obj_set_style_local_bg_color(_monitor_page, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x000000));

    // 1. 顶部状态栏 (Y: 2 ~ 20)
    lv_obj_t *dot = lv_obj_create(_monitor_page, NULL);
    lv_obj_set_size(dot, 6, 6);
    lv_obj_set_pos(dot, 8, 7);
    lv_obj_set_style_local_radius(dot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 3);
    lv_obj_set_style_local_bg_color(dot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x3fb950));
    lv_obj_set_style_local_border_width(dot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);

    _node_label = lv_label_create(_monitor_page, NULL);
    lv_obj_add_style(_node_label, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(_node_label, "PROXMOX VE");
    lv_obj_set_style_local_text_color(_node_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x58a6ff));
    lv_obj_set_pos(_node_label, 18, 2);

    _uptime_label = lv_label_create(_monitor_page, NULL);
    lv_obj_add_style(_uptime_label, LV_LABEL_PART_MAIN, &style_f1_reg);
    lv_label_set_text(_uptime_label, "UP: --");
    lv_obj_set_style_local_text_color(_uptime_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x8b949e));
    lv_obj_set_pos(_uptime_label, 205, 2);

    // 2. 左侧 Bento 网络卡片 (8, 22, 148, 168)
    lv_obj_t *card_net = lv_obj_create(_monitor_page, NULL);
    lv_obj_set_size(card_net, 148, 168);
    lv_obj_set_pos(card_net, 8, 22);
    lv_obj_set_style_local_bg_color(card_net, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x0c1017));
    lv_obj_set_style_local_border_color(card_net, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x21262d));
    lv_obj_set_style_local_border_width(card_net, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_radius(card_net, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 8);

    lv_obj_t *lbl_net_title = lv_label_create(card_net, NULL);
    lv_obj_add_style(lbl_net_title, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(lbl_net_title, "NET");
    lv_obj_set_style_local_text_color(lbl_net_title, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x8b949e));
    lv_obj_set_pos(lbl_net_title, 8, 6);

    lv_obj_t *badge = lv_obj_create(card_net, NULL);
    lv_obj_set_size(badge, 44, 16);
    lv_obj_set_pos(badge, 94, 5);
    lv_obj_set_style_local_bg_color(badge, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x131f33));
    lv_obj_set_style_local_border_color(badge, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x1f6feb));
    lv_obj_set_style_local_border_width(badge, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_radius(badge, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 4);

    lv_obj_t *badge_txt = lv_label_create(badge, NULL);
    lv_obj_add_style(badge_txt, LV_LABEL_PART_MAIN, &style_sub_10);
    lv_label_set_text(badge_txt, "iKuai");
    lv_obj_set_style_local_text_color(badge_txt, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x58a6ff));
    lv_obj_align(badge_txt, NULL, LV_ALIGN_CENTER, 0, 0);

    // 下行 (左列: 顶部标签 DOWN + 单位 K/s，下方 16px 主数值)
    lv_obj_t *down_tag = lv_label_create(card_net, NULL);
    lv_obj_add_style(down_tag, LV_LABEL_PART_MAIN, &style_sub_10);
    lv_label_set_text(down_tag, "DOWN");
    lv_obj_set_style_local_text_color(down_tag, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x3fb950));
    lv_obj_set_pos(down_tag, 8, 24);

    _down_speed_unit = lv_label_create(card_net, NULL);
    lv_obj_add_style(_down_speed_unit, LV_LABEL_PART_MAIN, &style_sub_10);
    lv_label_set_text(_down_speed_unit, "K/s");
    lv_obj_set_style_local_text_color(_down_speed_unit, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x8b949e));
    lv_obj_set_pos(_down_speed_unit, 46, 24);

    _down_speed_label = lv_label_create(card_net, NULL);
    lv_obj_add_style(_down_speed_label, LV_LABEL_PART_MAIN, &style_kh_16);
    lv_label_set_text(_down_speed_label, "0.0");
    lv_obj_set_style_local_text_color(_down_speed_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x3fb950));
    lv_obj_set_pos(_down_speed_label, 8, 38);

    // 上行 (右列: 顶部标签 UP + 单位 K/s，下方 16px 主数值)
    lv_obj_t *up_tag = lv_label_create(card_net, NULL);
    lv_obj_add_style(up_tag, LV_LABEL_PART_MAIN, &style_sub_10);
    lv_label_set_text(up_tag, "UP");
    lv_obj_set_style_local_text_color(up_tag, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x58a6ff));
    lv_obj_set_pos(up_tag, 78, 24);

    _up_speed_unit = lv_label_create(card_net, NULL);
    lv_obj_add_style(_up_speed_unit, LV_LABEL_PART_MAIN, &style_sub_10);
    lv_label_set_text(_up_speed_unit, "K/s");
    lv_obj_set_style_local_text_color(_up_speed_unit, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x8b949e));
    lv_obj_set_pos(_up_speed_unit, 116, 24);

    _up_speed_label = lv_label_create(card_net, NULL);
    lv_obj_add_style(_up_speed_label, LV_LABEL_PART_MAIN, &style_kh_16);
    lv_label_set_text(_up_speed_label, "0.0");
    lv_obj_set_style_local_text_color(_up_speed_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x58a6ff));
    lv_obj_set_pos(_up_speed_label, 78, 38);

    // 面积折线图
    _chart = lv_chart_create(card_net, NULL);
    lv_obj_set_size(_chart, 140, 98);
    lv_obj_set_pos(_chart, 4, 64);
    lv_chart_set_type(_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(_chart, 0, 1024);
    lv_chart_set_point_count(_chart, CHART_POINTS);
    lv_chart_set_update_mode(_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(_chart, 0, 0); // 去除网格线

    lv_obj_set_style_local_bg_color(_chart, LV_CHART_PART_BG, LV_STATE_DEFAULT, lv_color_hex(0x07090e));
    lv_obj_set_style_local_border_width(_chart, LV_CHART_PART_BG, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_line_width(_chart, LV_CHART_PART_BG, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_line_opa(_chart, LV_CHART_PART_BG, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_local_radius(_chart, LV_CHART_PART_BG, LV_STATE_DEFAULT, 4);
    lv_obj_set_style_local_size(_chart, LV_CHART_PART_SERIES, LV_STATE_DEFAULT, 0); // 消除顶点生硬方块
    lv_obj_set_style_local_line_width(_chart, LV_CHART_PART_SERIES, LV_STATE_DEFAULT, 2);
    lv_obj_set_style_local_bg_opa(_chart, LV_CHART_PART_SERIES, LV_STATE_DEFAULT, LV_OPA_30);
    lv_obj_set_style_local_bg_grad_dir(_chart, LV_CHART_PART_SERIES, LV_STATE_DEFAULT, LV_GRAD_DIR_VER);

    _ser_up = lv_chart_add_series(_chart, lv_color_hex(0xf85149));
    _ser_down = lv_chart_add_series(_chart, lv_color_hex(0x3fb950));
    lv_chart_set_points(_chart, _ser_up, _upload_series);
    lv_chart_set_points(_chart, _ser_down, _download_series);

    // 3. 右侧 Bento 硬件资源卡片 (164, 22, 148, 168)
    lv_obj_t *card_res = lv_obj_create(_monitor_page, NULL);
    lv_obj_set_size(card_res, 148, 168);
    lv_obj_set_pos(card_res, 164, 22);
    lv_obj_set_style_local_bg_color(card_res, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x0c1017));
    lv_obj_set_style_local_border_color(card_res, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x21262d));
    lv_obj_set_style_local_border_width(card_res, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_radius(card_res, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 8);

    // Row 1: CPU (加粗至 12px)
    lv_obj_t *lbl_cpu = lv_label_create(card_res, NULL);
    lv_obj_add_style(lbl_cpu, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(lbl_cpu, "CPU");
    lv_obj_set_style_local_text_color(lbl_cpu, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x79c0ff));
    lv_obj_set_pos(lbl_cpu, 8, 7);

    _cpu_temp_label = lv_label_create(card_res, NULL);
    lv_obj_add_style(_cpu_temp_label, LV_LABEL_PART_MAIN, &style_sub_12);
    lv_label_set_text(_cpu_temp_label, "41°C");
    lv_obj_set_style_local_text_color(_cpu_temp_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x3fb950));
    lv_obj_set_pos(_cpu_temp_label, 44, 9);

    _cpu_val_label = lv_label_create(card_res, NULL);
    lv_obj_add_style(_cpu_val_label, LV_LABEL_PART_MAIN, &style_kh_16);
    lv_label_set_text(_cpu_val_label, "0.0%");
    lv_obj_set_style_local_text_color(_cpu_val_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(_cpu_val_label, 94, 7);

    _cpu_bar = lv_bar_create(card_res, NULL);
    lv_obj_set_size(_cpu_bar, 132, 12); // 加粗至 12px
    lv_obj_set_pos(_cpu_bar, 8, 28);
    lv_bar_set_anim_time(_cpu_bar, 400); // 400ms 丝滑平滑过渡
    lv_obj_set_style_local_bg_color(_cpu_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, lv_color_hex(0x192233));
    lv_obj_set_style_local_bg_color(_cpu_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, lv_color_hex(0x58a6ff));
    lv_obj_set_style_local_radius(_cpu_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, 6);
    lv_obj_set_style_local_radius(_cpu_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, 6);

    // Row 2: RAM (加粗至 12px)
    lv_obj_t *lbl_mem = lv_label_create(card_res, NULL);
    lv_obj_add_style(lbl_mem, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(lbl_mem, "RAM");
    lv_obj_set_style_local_text_color(lbl_mem, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xd2a8ff));
    lv_obj_set_pos(lbl_mem, 8, 59);

    _mem_val_label = lv_label_create(card_res, NULL);
    lv_obj_add_style(_mem_val_label, LV_LABEL_PART_MAIN, &style_kh_16);
    lv_label_set_text(_mem_val_label, "0.0%");
    lv_obj_set_style_local_text_color(_mem_val_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(_mem_val_label, 94, 59);

    _mem_bar = lv_bar_create(card_res, NULL);
    lv_obj_set_size(_mem_bar, 132, 12); // 加粗至 12px
    lv_obj_set_pos(_mem_bar, 8, 80);
    lv_bar_set_anim_time(_mem_bar, 400);
    lv_obj_set_style_local_bg_color(_mem_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, lv_color_hex(0x241a33));
    lv_obj_set_style_local_bg_color(_mem_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, lv_color_hex(0xbc8cff));
    lv_obj_set_style_local_radius(_mem_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, 6);
    lv_obj_set_style_local_radius(_mem_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, 6);

    // Row 3: SSD (加粗至 12px)
    lv_obj_t *lbl_disk = lv_label_create(card_res, NULL);
    lv_obj_add_style(lbl_disk, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(lbl_disk, "SSD");
    lv_obj_set_style_local_text_color(lbl_disk, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xffa657));
    lv_obj_set_pos(lbl_disk, 8, 111);

    _disk_temp_label = lv_label_create(card_res, NULL);
    lv_obj_add_style(_disk_temp_label, LV_LABEL_PART_MAIN, &style_sub_12);
    lv_label_set_text(_disk_temp_label, "38°C");
    lv_obj_set_style_local_text_color(_disk_temp_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x3fb950));
    lv_obj_set_pos(_disk_temp_label, 44, 113);

    _disk_val_label = lv_label_create(card_res, NULL);
    lv_obj_add_style(_disk_val_label, LV_LABEL_PART_MAIN, &style_kh_16);
    lv_label_set_text(_disk_val_label, "0.0%");
    lv_obj_set_style_local_text_color(_disk_val_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(_disk_val_label, 94, 111);

    _disk_bar = lv_bar_create(card_res, NULL);
    lv_obj_set_size(_disk_bar, 132, 12); // 加粗至 12px
    lv_obj_set_pos(_disk_bar, 8, 132);
    lv_bar_set_anim_time(_disk_bar, 400);
    lv_obj_set_style_local_bg_color(_disk_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, lv_color_hex(0x2c1f18));
    lv_obj_set_style_local_bg_color(_disk_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, lv_color_hex(0xf0883e));
    lv_obj_set_style_local_radius(_disk_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, 6);
    lv_obj_set_style_local_radius(_disk_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, 6);

    // 4. 底部系统信息横卡 (8, 196, 304, 38)
    lv_obj_t *card_foot = lv_obj_create(_monitor_page, NULL);
    lv_obj_set_size(card_foot, 304, 38);
    lv_obj_set_pos(card_foot, 8, 196);
    lv_obj_set_style_local_bg_color(card_foot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x0c1017));
    lv_obj_set_style_local_border_color(card_foot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x21262d));
    lv_obj_set_style_local_border_width(card_foot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_radius(card_foot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 6);

    _ip_label = lv_label_create(card_foot, NULL);
    lv_obj_add_style(_ip_label, LV_LABEL_PART_MAIN, &style_sub_12);
    lv_label_set_text(_ip_label, "IP: Connecting...");
    lv_obj_set_style_local_text_color(_ip_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x8b949e));
    lv_obj_align(_ip_label, NULL, LV_ALIGN_IN_LEFT_MID, 10, 0);

    _footer_info_label = lv_label_create(card_foot, NULL);
    lv_obj_add_style(_footer_info_label, LV_LABEL_PART_MAIN, &style_sub_12);
    lv_label_set_text(_footer_info_label, "LOAD: 0.00");
    lv_obj_set_style_local_text_color(_footer_info_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x8b949e));
    lv_obj_align(_footer_info_label, NULL, LV_ALIGN_IN_RIGHT_MID, -10, 0);
}

// ========================================================
// 风格一：Cyberpunk HUD (赛博科幻机甲风)
// ========================================================
void DisplayUI::createMonitorPageCyberpunk() {
    lv_obj_set_style_local_bg_color(_monitor_page, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x000000));

    // 顶部 HUD 栏
    _node_label = lv_label_create(_monitor_page, NULL);
    lv_obj_add_style(_node_label, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(_node_label, "// PVE 8.1 HUD");
    lv_obj_set_style_local_text_color(_node_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x00f2fe));
    lv_obj_set_pos(_node_label, 8, 2);

    _uptime_label = lv_label_create(_monitor_page, NULL);
    lv_obj_add_style(_uptime_label, LV_LABEL_PART_MAIN, &style_f1_reg);
    lv_label_set_text(_uptime_label, "UP: --");
    lv_obj_set_style_local_text_color(_uptime_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x00ff88));
    lv_obj_set_pos(_uptime_label, 205, 2);

    // 左侧网速 HUD 卡
    lv_obj_t *card_net = lv_obj_create(_monitor_page, NULL);
    lv_obj_set_size(card_net, 148, 168);
    lv_obj_set_pos(card_net, 8, 22);
    lv_obj_set_style_local_bg_color(card_net, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x06090e));
    lv_obj_set_style_local_border_color(card_net, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x00f2fe));
    lv_obj_set_style_local_border_width(card_net, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_radius(card_net, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 4);

    lv_obj_t *lbl_net = lv_label_create(card_net, NULL);
    lv_obj_add_style(lbl_net, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(lbl_net, "NET TRAFFIC");
    lv_obj_set_style_local_text_color(lbl_net, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xff007f));
    lv_obj_set_pos(lbl_net, 8, 6);

    // 下行
    lv_obj_t *down_icon = lv_label_create(card_net, NULL);
    lv_obj_add_style(down_icon, LV_LABEL_PART_MAIN, &style_icon);
    lv_label_set_text(down_icon, CUSTOM_SYMBOL_DOWNLOAD);
    lv_obj_set_style_local_text_color(down_icon, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x00ff88));
    lv_obj_set_pos(down_icon, 8, 26);

    _down_speed_label = lv_label_create(card_net, NULL);
    lv_obj_add_style(_down_speed_label, LV_LABEL_PART_MAIN, &style_kh_16);
    lv_label_set_text(_down_speed_label, "0.0");
    lv_obj_set_style_local_text_color(_down_speed_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x00ff88));
    lv_obj_set_pos(_down_speed_label, 24, 25);

    _down_speed_unit = lv_label_create(card_net, NULL);
    lv_obj_add_style(_down_speed_unit, LV_LABEL_PART_MAIN, &style_f1_reg);
    lv_label_set_text(_down_speed_unit, "K/s");
    lv_obj_set_style_local_text_color(_down_speed_unit, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x8da0b8));
    lv_obj_set_pos(_down_speed_unit, 66, 26);

    // 上行
    lv_obj_t *up_icon = lv_label_create(card_net, NULL);
    lv_obj_add_style(up_icon, LV_LABEL_PART_MAIN, &style_icon);
    lv_label_set_text(up_icon, CUSTOM_SYMBOL_UPLOAD);
    lv_obj_set_style_local_text_color(up_icon, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xff007f));
    lv_obj_set_pos(up_icon, 8, 44);

    _up_speed_label = lv_label_create(card_net, NULL);
    lv_obj_add_style(_up_speed_label, LV_LABEL_PART_MAIN, &style_kh_16);
    lv_label_set_text(_up_speed_label, "0.0");
    lv_obj_set_style_local_text_color(_up_speed_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xff007f));
    lv_obj_set_pos(_up_speed_label, 24, 43);

    _up_speed_unit = lv_label_create(card_net, NULL);
    lv_obj_add_style(_up_speed_unit, LV_LABEL_PART_MAIN, &style_f1_reg);
    lv_label_set_text(_up_speed_unit, "K/s");
    lv_obj_set_style_local_text_color(_up_speed_unit, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x8da0b8));
    lv_obj_set_pos(_up_speed_unit, 66, 44);

    _chart = lv_chart_create(card_net, NULL);
    lv_obj_set_size(_chart, 140, 98);
    lv_obj_set_pos(_chart, 4, 64);
    lv_chart_set_type(_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(_chart, 0, 1024);
    lv_chart_set_point_count(_chart, CHART_POINTS);
    lv_chart_set_update_mode(_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(_chart, 0, 0); // 去除网格线
    lv_obj_set_style_local_bg_color(_chart, LV_CHART_PART_BG, LV_STATE_DEFAULT, lv_color_hex(0x06090e));
    lv_obj_set_style_local_border_width(_chart, LV_CHART_PART_BG, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_line_width(_chart, LV_CHART_PART_BG, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_line_opa(_chart, LV_CHART_PART_BG, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_local_size(_chart, LV_CHART_PART_SERIES, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_line_width(_chart, LV_CHART_PART_SERIES, LV_STATE_DEFAULT, 2);
    lv_obj_set_style_local_bg_opa(_chart, LV_CHART_PART_SERIES, LV_STATE_DEFAULT, LV_OPA_30);
    lv_obj_set_style_local_bg_grad_dir(_chart, LV_CHART_PART_SERIES, LV_STATE_DEFAULT, LV_GRAD_DIR_VER);

    _ser_up = lv_chart_add_series(_chart, lv_color_hex(0xff007f));
    _ser_down = lv_chart_add_series(_chart, lv_color_hex(0x00ff88));
    lv_chart_set_points(_chart, _ser_up, _upload_series);
    lv_chart_set_points(_chart, _ser_down, _download_series);

    // 右侧卡片
    lv_obj_t *card_res = lv_obj_create(_monitor_page, NULL);
    lv_obj_set_size(card_res, 148, 168);
    lv_obj_set_pos(card_res, 164, 22);
    lv_obj_set_style_local_bg_color(card_res, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x06090e));
    lv_obj_set_style_local_border_color(card_res, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x00f2fe));
    lv_obj_set_style_local_border_width(card_res, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_radius(card_res, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 4);

    // CPU
    lv_obj_t *lbl_cpu = lv_label_create(card_res, NULL);
    lv_obj_add_style(lbl_cpu, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(lbl_cpu, "CPU");
    lv_obj_set_style_local_text_color(lbl_cpu, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x00f2fe));
    lv_obj_set_pos(lbl_cpu, 8, 7);

    _cpu_temp_label = lv_label_create(card_res, NULL);
    lv_obj_add_style(_cpu_temp_label, LV_LABEL_PART_MAIN, &style_kh_16);
    lv_label_set_text(_cpu_temp_label, "41°C");
    lv_obj_set_style_local_text_color(_cpu_temp_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x00ff88));
    lv_obj_set_pos(_cpu_temp_label, 44, 7);

    _cpu_val_label = lv_label_create(card_res, NULL);
    lv_obj_add_style(_cpu_val_label, LV_LABEL_PART_MAIN, &style_kh_16);
    lv_label_set_text(_cpu_val_label, "0.0%");
    lv_obj_set_style_local_text_color(_cpu_val_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(_cpu_val_label, 94, 7);

    _cpu_bar = lv_bar_create(card_res, NULL);
    lv_obj_set_size(_cpu_bar, 132, 12);
    lv_obj_set_pos(_cpu_bar, 8, 28);
    lv_bar_set_anim_time(_cpu_bar, 400);
    lv_obj_set_style_local_bg_color(_cpu_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, lv_color_hex(0x131c28));
    lv_obj_set_style_local_bg_color(_cpu_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, lv_color_hex(0x00f2fe));
    lv_obj_set_style_local_radius(_cpu_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, 2);

    // RAM
    lv_obj_t *lbl_mem = lv_label_create(card_res, NULL);
    lv_obj_add_style(lbl_mem, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(lbl_mem, "RAM");
    lv_obj_set_style_local_text_color(lbl_mem, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xff007f));
    lv_obj_set_pos(lbl_mem, 8, 59);

    _mem_val_label = lv_label_create(card_res, NULL);
    lv_obj_add_style(_mem_val_label, LV_LABEL_PART_MAIN, &style_kh_16);
    lv_label_set_text(_mem_val_label, "0.0%");
    lv_obj_set_style_local_text_color(_mem_val_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(_mem_val_label, 94, 59);

    _mem_bar = lv_bar_create(card_res, NULL);
    lv_obj_set_size(_mem_bar, 132, 12);
    lv_obj_set_pos(_mem_bar, 8, 80);
    lv_bar_set_anim_time(_mem_bar, 400);
    lv_obj_set_style_local_bg_color(_mem_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, lv_color_hex(0x28131e));
    lv_obj_set_style_local_bg_color(_mem_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, lv_color_hex(0xff007f));
    lv_obj_set_style_local_radius(_mem_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, 2);

    // SSD
    lv_obj_t *lbl_disk = lv_label_create(card_res, NULL);
    lv_obj_add_style(lbl_disk, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(lbl_disk, "SSD");
    lv_obj_set_style_local_text_color(lbl_disk, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xffb000));
    lv_obj_set_pos(lbl_disk, 8, 111);

    _disk_temp_label = lv_label_create(card_res, NULL);
    lv_obj_add_style(_disk_temp_label, LV_LABEL_PART_MAIN, &style_kh_16);
    lv_label_set_text(_disk_temp_label, "38°C");
    lv_obj_set_style_local_text_color(_disk_temp_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x00ff88));
    lv_obj_set_pos(_disk_temp_label, 44, 111);

    _disk_val_label = lv_label_create(card_res, NULL);
    lv_obj_add_style(_disk_val_label, LV_LABEL_PART_MAIN, &style_kh_16);
    lv_label_set_text(_disk_val_label, "0.0%");
    lv_obj_set_style_local_text_color(_disk_val_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(_disk_val_label, 94, 111);

    _disk_bar = lv_bar_create(card_res, NULL);
    lv_obj_set_size(_disk_bar, 132, 12);
    lv_obj_set_pos(_disk_bar, 8, 132);
    lv_bar_set_anim_time(_disk_bar, 400);
    lv_obj_set_style_local_bg_color(_disk_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, lv_color_hex(0x282013));
    lv_obj_set_style_local_bg_color(_disk_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, lv_color_hex(0xffb000));
    lv_obj_set_style_local_radius(_disk_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, 2);

    // 底栏
    lv_obj_t *card_foot = lv_obj_create(_monitor_page, NULL);
    lv_obj_set_size(card_foot, 304, 38);
    lv_obj_set_pos(card_foot, 8, 196);
    lv_obj_set_style_local_bg_color(card_foot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x06090e));
    lv_obj_set_style_local_border_color(card_foot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x00f2fe));
    lv_obj_set_style_local_border_width(card_foot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_radius(card_foot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 4);

    _ip_label = lv_label_create(card_foot, NULL);
    lv_obj_add_style(_ip_label, LV_LABEL_PART_MAIN, &style_f1_reg);
    lv_label_set_text(_ip_label, "IP: 192.168.1.100");
    lv_obj_set_style_local_text_color(_ip_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x00f2fe));
    lv_obj_align(_ip_label, NULL, LV_ALIGN_IN_LEFT_MID, 10, 0);

    _footer_info_label = lv_label_create(card_foot, NULL);
    lv_obj_add_style(_footer_info_label, LV_LABEL_PART_MAIN, &style_f1_reg);
    lv_label_set_text(_footer_info_label, "LOAD: 0.00");
    lv_obj_set_style_local_text_color(_footer_info_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x8da0b8));
    lv_obj_align(_footer_info_label, NULL, LV_ALIGN_IN_RIGHT_MID, -10, 0);
}

// ========================================================
// 风格三：Industrial Telemetry (工业遥测极客风)
// ========================================================
void DisplayUI::createMonitorPageIndustrial() {
    lv_obj_set_style_local_bg_color(_monitor_page, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x000000));

    // 顶部反色横条
    lv_obj_t *top_box = lv_obj_create(_monitor_page, NULL);
    lv_obj_set_size(top_box, 304, 18);
    lv_obj_set_pos(top_box, 8, 3);
    lv_obj_set_style_local_bg_color(top_box, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xffb000));
    lv_obj_set_style_local_border_width(top_box, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_radius(top_box, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);

    _node_label = lv_label_create(top_box, NULL);
    lv_obj_add_style(_node_label, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(_node_label, "[ TELEMETRY // PVE ]");
    lv_obj_set_style_local_text_color(_node_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x000000));
    lv_obj_set_pos(_node_label, 6, 1);

    _uptime_label = lv_label_create(top_box, NULL);
    lv_obj_add_style(_uptime_label, LV_LABEL_PART_MAIN, &style_f1_reg);
    lv_label_set_text(_uptime_label, "UP: --");
    lv_obj_set_style_local_text_color(_uptime_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x000000));
    lv_obj_set_pos(_uptime_label, 195, 1);

    // 左侧面板
    lv_obj_t *card_net = lv_obj_create(_monitor_page, NULL);
    lv_obj_set_size(card_net, 148, 168);
    lv_obj_set_pos(card_net, 8, 24);
    lv_obj_set_style_local_bg_color(card_net, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x000000));
    lv_obj_set_style_local_border_color(card_net, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xffb000));
    lv_obj_set_style_local_border_width(card_net, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_radius(card_net, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);

    lv_obj_t *lbl_net = lv_label_create(card_net, NULL);
    lv_obj_add_style(lbl_net, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(lbl_net, "NET::QEMU_100");
    lv_obj_set_style_local_text_color(lbl_net, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xffb000));
    lv_obj_set_pos(lbl_net, 6, 6);

    // 下行
    lv_obj_t *down_icon = lv_label_create(card_net, NULL);
    lv_obj_add_style(down_icon, LV_LABEL_PART_MAIN, &style_icon);
    lv_label_set_text(down_icon, CUSTOM_SYMBOL_DOWNLOAD);
    lv_obj_set_style_local_text_color(down_icon, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x00ff66));
    lv_obj_set_pos(down_icon, 6, 26);

    _down_speed_label = lv_label_create(card_net, NULL);
    lv_obj_add_style(_down_speed_label, LV_LABEL_PART_MAIN, &style_kh_16);
    lv_label_set_text(_down_speed_label, "0.0");
    lv_obj_set_style_local_text_color(_down_speed_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x00ff66));
    lv_obj_set_pos(_down_speed_label, 22, 25);

    _down_speed_unit = lv_label_create(card_net, NULL);
    lv_obj_add_style(_down_speed_unit, LV_LABEL_PART_MAIN, &style_f1_reg);
    lv_label_set_text(_down_speed_unit, "K/s");
    lv_obj_set_style_local_text_color(_down_speed_unit, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xffb000));
    lv_obj_set_pos(_down_speed_unit, 66, 26);

    // 上行
    lv_obj_t *up_icon = lv_label_create(card_net, NULL);
    lv_obj_add_style(up_icon, LV_LABEL_PART_MAIN, &style_icon);
    lv_label_set_text(up_icon, CUSTOM_SYMBOL_UPLOAD);
    lv_obj_set_style_local_text_color(up_icon, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xffb000));
    lv_obj_set_pos(up_icon, 6, 44);

    _up_speed_label = lv_label_create(card_net, NULL);
    lv_obj_add_style(_up_speed_label, LV_LABEL_PART_MAIN, &style_kh_16);
    lv_label_set_text(_up_speed_label, "0.0");
    lv_obj_set_style_local_text_color(_up_speed_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xffb000));
    lv_obj_set_pos(_up_speed_label, 22, 43);

    _up_speed_unit = lv_label_create(card_net, NULL);
    lv_obj_add_style(_up_speed_unit, LV_LABEL_PART_MAIN, &style_f1_reg);
    lv_label_set_text(_up_speed_unit, "K/s");
    lv_obj_set_style_local_text_color(_up_speed_unit, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xffb000));
    lv_obj_set_pos(_up_speed_unit, 66, 44);

    _chart = lv_chart_create(card_net, NULL);
    lv_obj_set_size(_chart, 140, 98);
    lv_obj_set_pos(_chart, 4, 64);
    lv_chart_set_type(_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(_chart, 0, 1024);
    lv_chart_set_point_count(_chart, CHART_POINTS);
    lv_chart_set_update_mode(_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(_chart, 0, 0); // 去除网格线
    lv_obj_set_style_local_bg_color(_chart, LV_CHART_PART_BG, LV_STATE_DEFAULT, lv_color_hex(0x000000));
    lv_obj_set_style_local_border_width(_chart, LV_CHART_PART_BG, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_line_width(_chart, LV_CHART_PART_BG, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_line_opa(_chart, LV_CHART_PART_BG, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_local_size(_chart, LV_CHART_PART_SERIES, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_line_width(_chart, LV_CHART_PART_SERIES, LV_STATE_DEFAULT, 2);
    lv_obj_set_style_local_bg_opa(_chart, LV_CHART_PART_SERIES, LV_STATE_DEFAULT, LV_OPA_20);
    lv_obj_set_style_local_bg_grad_dir(_chart, LV_CHART_PART_SERIES, LV_STATE_DEFAULT, LV_GRAD_DIR_VER);

    _ser_up = lv_chart_add_series(_chart, lv_color_hex(0xffb000));
    _ser_down = lv_chart_add_series(_chart, lv_color_hex(0x00ff66));
    lv_chart_set_points(_chart, _ser_up, _upload_series);
    lv_chart_set_points(_chart, _ser_down, _download_series);

    // 右侧卡片
    lv_obj_t *card_res = lv_obj_create(_monitor_page, NULL);
    lv_obj_set_size(card_res, 148, 168);
    lv_obj_set_pos(card_res, 164, 24);
    lv_obj_set_style_local_bg_color(card_res, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x000000));
    lv_obj_set_style_local_border_color(card_res, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xffb000));
    lv_obj_set_style_local_border_width(card_res, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_radius(card_res, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);

    // CPU
    lv_obj_t *lbl_cpu = lv_label_create(card_res, NULL);
    lv_obj_add_style(lbl_cpu, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(lbl_cpu, "CPU");
    lv_obj_set_style_local_text_color(lbl_cpu, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xffb000));
    lv_obj_set_pos(lbl_cpu, 6, 7);

    _cpu_temp_label = lv_label_create(card_res, NULL);
    lv_obj_add_style(_cpu_temp_label, LV_LABEL_PART_MAIN, &style_kh_16);
    lv_label_set_text(_cpu_temp_label, "41°C");
    lv_obj_set_style_local_text_color(_cpu_temp_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x00ff66));
    lv_obj_set_pos(_cpu_temp_label, 42, 7);

    _cpu_val_label = lv_label_create(card_res, NULL);
    lv_obj_add_style(_cpu_val_label, LV_LABEL_PART_MAIN, &style_kh_16);
    lv_label_set_text(_cpu_val_label, "0.0%");
    lv_obj_set_style_local_text_color(_cpu_val_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(_cpu_val_label, 92, 7);

    _cpu_bar = lv_bar_create(card_res, NULL);
    lv_obj_set_size(_cpu_bar, 134, 12);
    lv_obj_set_pos(_cpu_bar, 6, 28);
    lv_bar_set_anim_time(_cpu_bar, 400);
    lv_obj_set_style_local_bg_color(_cpu_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, lv_color_hex(0x282000));
    lv_obj_set_style_local_bg_color(_cpu_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, lv_color_hex(0xffb000));
    lv_obj_set_style_local_radius(_cpu_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, 0);

    // RAM
    lv_obj_t *lbl_mem = lv_label_create(card_res, NULL);
    lv_obj_add_style(lbl_mem, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(lbl_mem, "RAM");
    lv_obj_set_style_local_text_color(lbl_mem, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xffb000));
    lv_obj_set_pos(lbl_mem, 6, 59);

    _mem_val_label = lv_label_create(card_res, NULL);
    lv_obj_add_style(_mem_val_label, LV_LABEL_PART_MAIN, &style_kh_16);
    lv_label_set_text(_mem_val_label, "0.0%");
    lv_obj_set_style_local_text_color(_mem_val_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(_mem_val_label, 92, 59);

    _mem_bar = lv_bar_create(card_res, NULL);
    lv_obj_set_size(_mem_bar, 134, 12);
    lv_obj_set_pos(_mem_bar, 6, 80);
    lv_bar_set_anim_time(_mem_bar, 400);
    lv_obj_set_style_local_bg_color(_mem_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, lv_color_hex(0x282000));
    lv_obj_set_style_local_bg_color(_mem_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, lv_color_hex(0xffb000));
    lv_obj_set_style_local_radius(_mem_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, 0);

    // SSD
    lv_obj_t *lbl_disk = lv_label_create(card_res, NULL);
    lv_obj_add_style(lbl_disk, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(lbl_disk, "SSD");
    lv_obj_set_style_local_text_color(lbl_disk, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xffb000));
    lv_obj_set_pos(lbl_disk, 6, 111);

    _disk_temp_label = lv_label_create(card_res, NULL);
    lv_obj_add_style(_disk_temp_label, LV_LABEL_PART_MAIN, &style_kh_16);
    lv_label_set_text(_disk_temp_label, "38°C");
    lv_obj_set_style_local_text_color(_disk_temp_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x00ff66));
    lv_obj_set_pos(_disk_temp_label, 42, 111);

    _disk_val_label = lv_label_create(card_res, NULL);
    lv_obj_add_style(_disk_val_label, LV_LABEL_PART_MAIN, &style_kh_16);
    lv_label_set_text(_disk_val_label, "0.0%");
    lv_obj_set_style_local_text_color(_disk_val_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(_disk_val_label, 92, 111);

    _disk_bar = lv_bar_create(card_res, NULL);
    lv_obj_set_size(_disk_bar, 134, 12);
    lv_obj_set_pos(_disk_bar, 6, 132);
    lv_bar_set_anim_time(_disk_bar, 400);
    lv_obj_set_style_local_bg_color(_disk_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, lv_color_hex(0x282000));
    lv_obj_set_style_local_bg_color(_disk_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, lv_color_hex(0xffb000));
    lv_obj_set_style_local_radius(_disk_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, 0);

    // 底栏
    lv_obj_t *card_foot = lv_obj_create(_monitor_page, NULL);
    lv_obj_set_size(card_foot, 304, 38);
    lv_obj_set_pos(card_foot, 8, 196);
    lv_obj_set_style_local_bg_color(card_foot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x000000));
    lv_obj_set_style_local_border_color(card_foot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xffb000));
    lv_obj_set_style_local_border_width(card_foot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_radius(card_foot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);

    _ip_label = lv_label_create(card_foot, NULL);
    lv_obj_add_style(_ip_label, LV_LABEL_PART_MAIN, &style_f1_reg);
    lv_label_set_text(_ip_label, "IP: 192.168.1.100");
    lv_obj_set_style_local_text_color(_ip_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xffb000));
    lv_obj_align(_ip_label, NULL, LV_ALIGN_IN_LEFT_MID, 10, 0);

    _footer_info_label = lv_label_create(card_foot, NULL);
    lv_obj_add_style(_footer_info_label, LV_LABEL_PART_MAIN, &style_f1_reg);
    lv_label_set_text(_footer_info_label, "LOAD: 0.00");
    lv_obj_set_style_local_text_color(_footer_info_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xffb000));
    lv_obj_align(_footer_info_label, NULL, LV_ALIGN_IN_RIGHT_MID, -10, 0);
}

// ========================================================
// 风格四：纯黑白极简仪器仪表风 (Monochrome Telemetry)
// ========================================================
void DisplayUI::createMonitorPageMono() {
    lv_obj_set_style_local_bg_color(_monitor_page, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x000000));

    // 1. 顶部状态栏细线风格 (8, 2, 304, 20)
    _node_label = lv_label_create(_monitor_page, NULL);
    lv_obj_add_style(_node_label, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(_node_label, "PVE HOST");
    lv_obj_set_style_local_text_color(_node_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(_node_label, 8, 2);

    lv_obj_t *badge = lv_obj_create(_monitor_page, NULL);
    lv_obj_set_size(badge, 36, 15);
    lv_obj_set_pos(badge, 88, 2);
    lv_obj_set_style_local_bg_color(badge, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_style_local_border_width(badge, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_radius(badge, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 2);

    lv_obj_t *badge_txt = lv_label_create(badge, NULL);
    lv_obj_add_style(badge_txt, LV_LABEL_PART_MAIN, &style_sub_10);
    lv_label_set_text(badge_txt, "LIVE");
    lv_obj_set_style_local_text_color(badge_txt, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_obj_align(badge_txt, NULL, LV_ALIGN_CENTER, 0, 0);

    _uptime_label = lv_label_create(_monitor_page, NULL);
    lv_obj_add_style(_uptime_label, LV_LABEL_PART_MAIN, &style_f1_reg);
    lv_label_set_text(_uptime_label, "UP: --");
    lv_obj_set_style_local_text_color(_uptime_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xaaaaaa));
    lv_obj_set_pos(_uptime_label, 195, 2);

    // 2. 左侧高对比网络卡 (8, 22, 148, 168)
    lv_obj_t *card_net = lv_obj_create(_monitor_page, NULL);
    lv_obj_set_size(card_net, 148, 168);
    lv_obj_set_pos(card_net, 8, 22);
    lv_obj_set_style_local_bg_color(card_net, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x000000));
    lv_obj_set_style_local_border_color(card_net, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x444444));
    lv_obj_set_style_local_border_width(card_net, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_radius(card_net, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 2);

    lv_obj_t *lbl_net_title = lv_label_create(card_net, NULL);
    lv_obj_add_style(lbl_net_title, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(lbl_net_title, "TRAFFIC");
    lv_obj_set_style_local_text_color(lbl_net_title, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x888888));
    lv_obj_set_pos(lbl_net_title, 8, 4);

    lv_obj_t *lbl_ik = lv_label_create(card_net, NULL);
    lv_obj_add_style(lbl_ik, LV_LABEL_PART_MAIN, &style_sub_10);
    lv_label_set_text(lbl_ik, "IKUAI");
    lv_obj_set_style_local_text_color(lbl_ik, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(lbl_ik, 104, 5);

    // 第 1 行: DOWN (左侧标签 DOWN + KB/s，右侧 22px 加粗大数值)
    lv_obj_t *down_tag = lv_label_create(card_net, NULL);
    lv_obj_add_style(down_tag, LV_LABEL_PART_MAIN, &style_sub_10);
    lv_label_set_text(down_tag, "DOWN");
    lv_obj_set_style_local_text_color(down_tag, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(down_tag, 8, 23);

    _down_speed_unit = lv_label_create(card_net, NULL);
    lv_obj_add_style(_down_speed_unit, LV_LABEL_PART_MAIN, &style_sub_10);
    lv_label_set_text(_down_speed_unit, "K/s");
    lv_obj_set_style_local_text_color(_down_speed_unit, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x777777));
    lv_obj_set_pos(_down_speed_unit, 42, 23);

    _down_speed_label = lv_label_create(card_net, NULL);
    lv_obj_add_style(_down_speed_label, LV_LABEL_PART_MAIN, &style_kh_22);
    lv_label_set_text(_down_speed_label, "0.0");
    lv_obj_set_style_local_text_color(_down_speed_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(_down_speed_label, 72, 17);

    // 第 2 行: UP (左侧标签 UP + KB/s，右侧 22px 加粗大数值)
    lv_obj_t *up_tag = lv_label_create(card_net, NULL);
    lv_obj_add_style(up_tag, LV_LABEL_PART_MAIN, &style_sub_10);
    lv_label_set_text(up_tag, "UP");
    lv_obj_set_style_local_text_color(up_tag, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(up_tag, 8, 45);

    _up_speed_unit = lv_label_create(card_net, NULL);
    lv_obj_add_style(_up_speed_unit, LV_LABEL_PART_MAIN, &style_sub_10);
    lv_label_set_text(_up_speed_unit, "K/s");
    lv_obj_set_style_local_text_color(_up_speed_unit, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x777777));
    lv_obj_set_pos(_up_speed_unit, 28, 45);

    _up_speed_label = lv_label_create(card_net, NULL);
    lv_obj_add_style(_up_speed_label, LV_LABEL_PART_MAIN, &style_kh_22);
    lv_label_set_text(_up_speed_label, "0.0");
    lv_obj_set_style_local_text_color(_up_speed_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(_up_speed_label, 72, 39);

    // 紧凑 60px 精致刻度示波窗
    _chart = lv_chart_create(card_net, NULL);
    lv_obj_set_size(_chart, 138, 62);
    lv_obj_set_pos(_chart, 4, 70);
    lv_chart_set_type(_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(_chart, 0, 1024);
    lv_chart_set_point_count(_chart, CHART_POINTS);
    lv_chart_set_update_mode(_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(_chart, 0, 0);

    lv_obj_set_style_local_bg_color(_chart, LV_CHART_PART_BG, LV_STATE_DEFAULT, lv_color_hex(0x050505));
    lv_obj_set_style_local_border_color(_chart, LV_CHART_PART_BG, LV_STATE_DEFAULT, lv_color_hex(0x222222));
    lv_obj_set_style_local_border_width(_chart, LV_CHART_PART_BG, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_radius(_chart, LV_CHART_PART_BG, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_size(_chart, LV_CHART_PART_SERIES, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_line_width(_chart, LV_CHART_PART_SERIES, LV_STATE_DEFAULT, 2);

    _ser_down = lv_chart_add_series(_chart, LV_COLOR_WHITE);         // 纯白下行线
    _ser_up   = lv_chart_add_series(_chart, lv_color_hex(0x777777)); // 中灰上行线

    // 3. 右侧系统指标卡 (162, 22, 150, 168)
    lv_obj_t *card_res = lv_obj_create(_monitor_page, NULL);
    lv_obj_set_size(card_res, 150, 168);
    lv_obj_set_pos(card_res, 162, 22);
    lv_obj_set_style_local_bg_color(card_res, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x000000));
    lv_obj_set_style_local_border_color(card_res, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x444444));
    lv_obj_set_style_local_border_width(card_res, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_radius(card_res, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 2);

    lv_obj_t *lbl_sys_title = lv_label_create(card_res, NULL);
    lv_obj_add_style(lbl_sys_title, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(lbl_sys_title, "SYSTEM");
    lv_obj_set_style_local_text_color(lbl_sys_title, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x888888));
    lv_obj_set_pos(lbl_sys_title, 8, 4);

    lv_obj_t *lbl_gauge = lv_label_create(card_res, NULL);
    lv_obj_add_style(lbl_gauge, LV_LABEL_PART_MAIN, &style_sub_10);
    lv_label_set_text(lbl_gauge, "GAUGE");
    lv_obj_set_style_local_text_color(lbl_gauge, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(lbl_gauge, 102, 5);

    // CPU 仪表
    lv_obj_t *lbl_cpu = lv_label_create(card_res, NULL);
    lv_obj_add_style(lbl_cpu, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(lbl_cpu, "CPU");
    lv_obj_set_style_local_text_color(lbl_cpu, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(lbl_cpu, 8, 23);

    _cpu_temp_label = lv_label_create(card_res, NULL);
    lv_obj_add_style(_cpu_temp_label, LV_LABEL_PART_MAIN, &style_sub_10);
    lv_label_set_text(_cpu_temp_label, "0°C");
    lv_obj_set_style_local_text_color(_cpu_temp_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x888888));
    lv_obj_set_pos(_cpu_temp_label, 44, 25);

    _cpu_val_label = lv_label_create(card_res, NULL);
    lv_obj_add_style(_cpu_val_label, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(_cpu_val_label, "0.0%");
    lv_obj_set_style_local_text_color(_cpu_val_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(_cpu_val_label, 94, 23);

    _cpu_bar = lv_bar_create(card_res, NULL);
    lv_obj_set_size(_cpu_bar, 134, 10);
    lv_obj_set_pos(_cpu_bar, 8, 41);
    lv_bar_set_anim_time(_cpu_bar, 400);
    lv_obj_set_style_local_bg_color(_cpu_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, lv_color_hex(0x181818));
    lv_obj_set_style_local_border_color(_cpu_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, lv_color_hex(0x333333));
    lv_obj_set_style_local_border_width(_cpu_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_bg_color(_cpu_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_style_local_radius(_cpu_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, 0);

    // RAM 仪表
    lv_obj_t *lbl_mem = lv_label_create(card_res, NULL);
    lv_obj_add_style(lbl_mem, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(lbl_mem, "RAM");
    lv_obj_set_style_local_text_color(lbl_mem, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(lbl_mem, 8, 67);

    _mem_val_label = lv_label_create(card_res, NULL);
    lv_obj_add_style(_mem_val_label, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(_mem_val_label, "0.0%");
    lv_obj_set_style_local_text_color(_mem_val_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(_mem_val_label, 94, 67);

    _mem_bar = lv_bar_create(card_res, NULL);
    lv_obj_set_size(_mem_bar, 134, 10);
    lv_obj_set_pos(_mem_bar, 8, 85);
    lv_bar_set_anim_time(_mem_bar, 400);
    lv_obj_set_style_local_bg_color(_mem_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, lv_color_hex(0x181818));
    lv_obj_set_style_local_border_color(_mem_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, lv_color_hex(0x333333));
    lv_obj_set_style_local_border_width(_mem_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_bg_color(_mem_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_style_local_radius(_mem_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, 0);

    // SSD 仪表
    lv_obj_t *lbl_disk = lv_label_create(card_res, NULL);
    lv_obj_add_style(lbl_disk, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(lbl_disk, "SSD");
    lv_obj_set_style_local_text_color(lbl_disk, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(lbl_disk, 8, 111);

    _disk_temp_label = lv_label_create(card_res, NULL);
    lv_obj_add_style(_disk_temp_label, LV_LABEL_PART_MAIN, &style_sub_10);
    lv_label_set_text(_disk_temp_label, "0°C");
    lv_obj_set_style_local_text_color(_disk_temp_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x888888));
    lv_obj_set_pos(_disk_temp_label, 44, 113);

    _disk_val_label = lv_label_create(card_res, NULL);
    lv_obj_add_style(_disk_val_label, LV_LABEL_PART_MAIN, &style_f1_bold);
    lv_label_set_text(_disk_val_label, "0.0%");
    lv_obj_set_style_local_text_color(_disk_val_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_pos(_disk_val_label, 94, 111);

    _disk_bar = lv_bar_create(card_res, NULL);
    lv_obj_set_size(_disk_bar, 134, 10);
    lv_obj_set_pos(_disk_bar, 8, 129);
    lv_bar_set_anim_time(_disk_bar, 400);
    lv_obj_set_style_local_bg_color(_disk_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, lv_color_hex(0x181818));
    lv_obj_set_style_local_border_color(_disk_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, lv_color_hex(0x333333));
    lv_obj_set_style_local_border_width(_disk_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_bg_color(_disk_bar, LV_BAR_PART_INDIC, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_style_local_radius(_disk_bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, 0);

    // 4. 底栏 (8, 196, 304, 38)
    lv_obj_t *card_foot = lv_obj_create(_monitor_page, NULL);
    lv_obj_set_size(card_foot, 304, 38);
    lv_obj_set_pos(card_foot, 8, 196);
    lv_obj_set_style_local_bg_color(card_foot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x000000));
    lv_obj_set_style_local_border_color(card_foot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x444444));
    lv_obj_set_style_local_border_width(card_foot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_radius(card_foot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 2);

    _ip_label = lv_label_create(card_foot, NULL);
    lv_obj_add_style(_ip_label, LV_LABEL_PART_MAIN, &style_f1_reg);
    lv_label_set_text(_ip_label, "IP: 192.168.1.100");
    lv_obj_set_style_local_text_color(_ip_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_align(_ip_label, NULL, LV_ALIGN_IN_LEFT_MID, 10, 0);

    _footer_info_label = lv_label_create(card_foot, NULL);
    lv_obj_add_style(_footer_info_label, LV_LABEL_PART_MAIN, &style_f1_reg);
    lv_label_set_text(_footer_info_label, "LOAD: 0.00");
    lv_obj_set_style_local_text_color(_footer_info_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x888888));
    lv_obj_align(_footer_info_label, NULL, LV_ALIGN_IN_RIGHT_MID, -10, 0);
}

void DisplayUI::showLogin() {
    lv_obj_set_hidden(_login_page, false);
    lv_obj_set_hidden(_monitor_page, true);
}

void DisplayUI::showMonitor() {
    lv_obj_set_hidden(_login_page, true);
    lv_obj_set_hidden(_monitor_page, false);
}

void DisplayUI::showAPPortal(const char *ssid, const char *ip) {
    showLogin();
    if (_login_status_label) {
        char buf[128];
        snprintf(buf, sizeof(buf), "AP: %s\nIP: %s\nConnect phone to configure", ssid, ip);
        lv_label_set_text(_login_status_label, buf);
        lv_obj_align(_login_status_label, NULL, LV_ALIGN_CENTER, 0, 45);
    }
}

void DisplayUI::updateLoginStatus(const char *msg) {
    if (_login_status_label) {
        lv_label_set_text(_login_status_label, msg);
        lv_obj_align(_login_status_label, NULL, LV_ALIGN_CENTER, 0, 45);
    }
}

void DisplayUI::updateWiFiIP(const char *ip_str) {
    if (ip_str) {
        strncpy(_last_ip, ip_str, sizeof(_last_ip) - 1);
        _last_ip[sizeof(_last_ip) - 1] = '\0';
    }
    if (_ip_label) {
        lv_label_set_text_fmt(_ip_label, "IP: %s", _last_ip);
        lv_obj_align(_ip_label, NULL, LV_ALIGN_IN_LEFT_MID, 10, 0);
    }
}

lv_coord_t DisplayUI::updateSeries(lv_coord_t *series, double speed) {
    lv_coord_t local_max = series[0];
    for (int i = 0; i < 9; i++) {
        series[i] = series[i + 1];
        if (local_max < series[i]) local_max = series[i];
    }
    series[9] = (lv_coord_t)speed;
    if (local_max < series[9]) local_max = series[9];
    return local_max;
}

void DisplayUI::updateSpeedLabels(double up_kb, double down_kb) {
    // 上行
    if (up_kb < 1000.0) {
        lv_label_set_text_fmt(_up_speed_label, "%.1f", up_kb);
        lv_label_set_text(_up_speed_unit, "K/s");
    } else {
        lv_label_set_text_fmt(_up_speed_label, "%.1f", up_kb / 1024.0);
        lv_label_set_text(_up_speed_unit, "M/s");
    }

    // 下行
    if (down_kb < 1000.0) {
        lv_label_set_text_fmt(_down_speed_label, "%.1f", down_kb);
        lv_label_set_text(_down_speed_unit, "K/s");
    } else {
        lv_label_set_text_fmt(_down_speed_label, "%.1f", down_kb / 1024.0);
        lv_label_set_text(_down_speed_unit, "M/s");
    }
}

void DisplayUI::updateData(const PVEStatus &status) {
    _last_status = status;
    _has_last_status = true;

    if (!status.online) {
        lv_label_set_text(_uptime_label, "OFFLINE");
        lv_obj_set_style_local_text_color(_uptime_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0xf85149));
        return;
    }

    // 1. 运行时间
    lv_label_set_text_fmt(_uptime_label, "UP: %s", status.uptime_str);
    lv_obj_set_style_local_text_color(_uptime_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(0x8b949e));

    // 2. 网速目标值 (供 flowChart 高帧率平滑插值流动)
    updateSpeedLabels(status.tx_speed_kb, status.rx_speed_kb);
    _target_tx_kb = status.tx_speed_kb;
    _target_rx_kb = status.rx_speed_kb;

    // 3. 资源条平滑动画过渡 (LV_ANIM_ON 驱动 400ms 阻尼缓动)
    lv_bar_set_value(_cpu_bar, (int16_t)status.cpu_usage, LV_ANIM_ON);
    lv_label_set_text_fmt(_cpu_val_label, "%2.1f%%", status.cpu_usage);

    lv_bar_set_value(_mem_bar, (int16_t)status.mem_usage, LV_ANIM_ON);
    lv_label_set_text_fmt(_mem_val_label, "%2.0f%%", status.mem_usage);

    lv_bar_set_value(_disk_bar, (int16_t)status.rootfs_usage, LV_ANIM_ON);
    lv_label_set_text_fmt(_disk_val_label, "%2.0f%%", status.rootfs_usage);

    // 4. 温度与右侧表盘 (CPU 温度仪表)
    if (_cpu_temp_label) {
        lv_label_set_text_fmt(_cpu_temp_label, "%.0f°C", status.cpu_temp);
        lv_color_t cpu_t_col = status.cpu_temp > 75.0 ? lv_color_hex(0xf85149) :
                               status.cpu_temp > 60.0 ? lv_color_hex(0xd29922) : lv_color_hex(0x3fb950);
        lv_obj_set_style_local_text_color(_cpu_temp_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, cpu_t_col);
    }
    if (_disk_temp_label) {
        lv_label_set_text_fmt(_disk_temp_label, "%.0f°C", status.disk_temp);
        lv_color_t disk_t_col = status.disk_temp > 65.0 ? lv_color_hex(0xf85149) :
                                status.disk_temp > 50.0 ? lv_color_hex(0xd29922) : lv_color_hex(0x3fb950);
        lv_obj_set_style_local_text_color(_disk_temp_label, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, disk_t_col);
    }

    // 5. 底部栏 (仅展示系统平均负载，移除 PVE 版本号)
    if (_footer_info_label) {
        lv_label_set_text_fmt(_footer_info_label, "LOAD: %.2f", status.loadavg);
        lv_obj_align(_footer_info_label, NULL, LV_ALIGN_IN_RIGHT_MID, -10, 0);
    }
}

void DisplayUI::flowChart() {
    if (!_chart || !_ser_up || !_ser_down) return;

    // 1. 指数渐进逼近目标网速
    _current_rx_kb = _current_rx_kb * 0.70 + _target_rx_kb * 0.30;
    _current_tx_kb = _current_tx_kb * 0.70 + _target_tx_kb * 0.30;

    // 2. 队列左移 1 步 (28 点密集平滑推进)
    for (int i = 0; i < CHART_POINTS - 1; i++) {
        _download_series[i] = _download_series[i + 1];
        _upload_series[i] = _upload_series[i + 1];
    }

    // 3. 填入最新平滑点
    _download_series[CHART_POINTS - 1] = (lv_coord_t)_current_rx_kb;
    _upload_series[CHART_POINTS - 1] = (lv_coord_t)_current_tx_kb;

    // 4. 动态平滑 Y 轴量程 (消除量程突变震荡)
    lv_coord_t peak = 32;
    for (int i = 0; i < CHART_POINTS; i++) {
        if (_download_series[i] > peak) peak = _download_series[i];
        if (_upload_series[i] > peak) peak = _upload_series[i];
    }
    double target_y = peak * 1.25;
    if (target_y < 32.0) target_y = 32.0;
    _chart_y_range = _chart_y_range * 0.85 + target_y * 0.15;

    lv_chart_set_range(_chart, 0, (lv_coord_t)_chart_y_range);
    lv_chart_set_points(_chart, _ser_up, _upload_series);
    lv_chart_set_points(_chart, _ser_down, _download_series);
    lv_chart_refresh(_chart);
}

void DisplayUI::loop() {
    unsigned long now = millis();
    if (now - _last_chart_flow_time >= 150) { // 每 150ms 连续平滑流水推进一帧 (约 7 FPS)
        _last_chart_flow_time = now;
        flowChart();
    }
}
