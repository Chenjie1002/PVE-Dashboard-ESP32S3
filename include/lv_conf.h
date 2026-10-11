/**
 * @file lv_conf.h
 * Configuration file for v7.11.0
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   Graphical settings
 *====================*/

#define LV_HOR_RES_MAX          (320)
#define LV_VER_RES_MAX          (240)

#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        1   /* 1: ST7789 SPI 传输需高低字节调换 */
#define LV_COLOR_SCREEN_TRANSP  0
#define LV_ANTIALIAS            1

#define LV_DISP_DEF_REFR_PERIOD 30  /* [ms] 刷新周期 */
#define LV_DPI                  130 /* [px] */

typedef int32_t lv_coord_t;

/*=========================
   Memory manager settings
 *=========================*/

#define LV_MEM_CUSTOM           0
#if LV_MEM_CUSTOM == 0
#  define LV_MEM_SIZE           (48U * 1024U) /* 48KB 内存池，ESP32-C3 资源充裕 */
#  define LV_MEM_ATTR
#  define LV_MEM_ADR            0
#  define LV_MEM_AUTO_DEFRAG    1
#else
#  define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#  define LV_MEM_CUSTOM_ALLOC   malloc
#  define LV_MEM_CUSTOM_FREE    free
#endif

/*==================
   HAL settings
 *==================*/

#define LV_TICK_CUSTOM          1
#if LV_TICK_CUSTOM == 1
#  define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#  define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
#endif

typedef void * lv_disp_drv_user_data_t;
typedef void * lv_indev_drv_user_data_t;

/*================
   Log & Assert
 *===============*/

#define LV_USE_LOG              0
#if LV_USE_LOG
#  define LV_LOG_LEVEL          LV_LOG_LEVEL_WARN
#  define LV_LOG_PRINTF         1
#endif

#define LV_USE_DEBUG            0
#define LV_USE_ASSERT_NULL      0
#define LV_USE_ASSERT_MEM       0

/*=================
   Font settings
 *=================*/

#define LV_FONT_MONTSERRAT_10   1
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_18   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_22   1
#define LV_FONT_MONTSERRAT_24   1

#define LV_FONT_DEFAULT         &lv_font_montserrat_14

#define LV_FONT_FMT_TXT_LARGE   0
#define LV_FONT_SUBPX_BGR       0

/*=================
   Text settings
 *=================*/

#define LV_TXT_ENC              LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS      " ,.;:-_"
#define LV_TXT_LINE_BREAK_LONG_LEN 0
#define LV_TXT_COLOR_CMD        "#"

#define LV_SPRINTF_CUSTOM       1
#if LV_SPRINTF_CUSTOM
#  define LV_SPRINTF_INCLUDE    <stdio.h>
#  define lv_snprintf           snprintf
#  define lv_vsnprintf          vsnprintf
#endif

/*===================
   LV_OBJ settings
 *==================*/

#define LV_USE_USER_DATA        0
#define LV_USE_OBJ_REALIGN      1
#define LV_USE_EXT_CLICK_AREA   LV_EXT_CLICK_AREA_TINY

/*==================
   LV OBJ X Usage
 *=================*/

#define LV_USE_ARC              1
#define LV_USE_BAR              1
#define LV_USE_BTN              1
#define LV_USE_CHART            1
#if LV_USE_CHART
#  define LV_CHART_AXIS_TICK_LABEL_MAX_LEN 256
#endif
#define LV_USE_CONT             1
#define LV_USE_LABEL            1
#define LV_USE_LINE             1
#define LV_USE_SPINNER          1
#define LV_USE_SPINBOX          0

/* Other unused widgets */
#define LV_USE_BTNMATRIX        0
#define LV_USE_CALENDAR         0
#define LV_USE_CANVAS           0
#define LV_USE_CHECKBOX         0
#define LV_USE_CPICKER          0
#define LV_USE_DROPDOWN         0
#define LV_USE_GAUGE            0
#define LV_USE_IMG              0
#define LV_USE_IMGBTN           0
#define LV_USE_KEYBOARD         0
#define LV_USE_LED              0
#define LV_USE_LINEMETER        0
#define LV_USE_LIST             0
#define LV_USE_MSGBOX           0
#define LV_USE_OBJMASK          0
#define LV_USE_PAGE             0
#define LV_USE_ROLLER           0
#define LV_USE_SLIDER           0
#define LV_USE_SWITCH           0
#define LV_USE_TABLE            0
#define LV_USE_TABVIEW          0
#define LV_USE_TEXTAREA         0
#define LV_USE_TILEVIEW         0
#define LV_USE_WIN              0

/*==================
   Themes
 *=================*/

#define LV_USE_THEME_EMPTY      1
#define LV_USE_THEME_MATERIAL   1
#define LV_THEME_DEFAULT_INIT   lv_theme_material_init
#define LV_THEME_DEFAULT_COLOR_PRIMARY   LV_COLOR_RED
#define LV_THEME_DEFAULT_COLOR_SECONDARY LV_COLOR_BLUE
#define LV_THEME_DEFAULT_FLAG   LV_THEME_MATERIAL_FLAG_DARK
#define LV_THEME_DEFAULT_FONT_SMALL     &lv_font_montserrat_12
#define LV_THEME_DEFAULT_FONT_NORMAL    &lv_font_montserrat_14
#define LV_THEME_DEFAULT_FONT_SUBTITLE  &lv_font_montserrat_14
#define LV_THEME_DEFAULT_FONT_TITLE     &lv_font_montserrat_16

#endif /* LV_CONF_H */
