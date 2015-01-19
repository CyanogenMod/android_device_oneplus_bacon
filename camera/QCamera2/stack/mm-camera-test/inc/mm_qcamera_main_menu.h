/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __MM_QCAMERA_MAIN_MENU_H__
#define __MM_QCAMERA_MAIN_MENU_H__

#include "mm_camera_interface.h"
#include "mm_jpeg_interface.h"

#define VIDEO_BUFFER_SIZE       (PREVIEW_WIDTH * PREVIEW_HEIGHT * 3/2)
#define THUMBNAIL_BUFFER_SIZE   (THUMBNAIL_WIDTH * THUMBNAIL_HEIGHT * 3/2)
#define SNAPSHOT_BUFFER_SIZE    (PICTURE_WIDTH * PICTURE_HEIGHT * 3/2)

/*===========================================================================
 * Macro
 *===========================================================================*/
#define PREVIEW_FRAMES_NUM    5
#define VIDEO_FRAMES_NUM      5
#define THUMBNAIL_FRAMES_NUM  1
#define SNAPSHOT_FRAMES_NUM   1
#define MAX_NUM_FORMAT        32

typedef enum
{
  START_PREVIEW,
  STOP_PREVIEW,
  SET_WHITE_BALANCE,
  SET_TINTLESS_ENABLE,
  SET_TINTLESS_DISABLE,
  SET_EXP_METERING,
  GET_CTRL_VALUE,
  TOGGLE_AFR,
  SET_ISO,
  BRIGHTNESS_GOTO_SUBMENU,
  CONTRAST_GOTO_SUBMENU,
  EV_GOTO_SUBMENU,
  SATURATION_GOTO_SUBMENU,
  SET_ZOOM,
  SET_SHARPNESS,
  TAKE_JPEG_SNAPSHOT,
  START_RECORDING,
  STOP_RECORDING,
  BEST_SHOT,
  LIVE_SHOT,
  FLASH_MODES,
  TOGGLE_ZSL,
  TAKE_RAW_SNAPSHOT,
  SWITCH_SNAP_RESOLUTION,
  TOGGLE_WNR,
  EXIT
} Camera_main_menu_t;

typedef enum
{
  ACTION_NO_ACTION,
  ACTION_START_PREVIEW,
  ACTION_STOP_PREVIEW,
  ACTION_SET_WHITE_BALANCE,
  ACTION_SET_TINTLESS_ENABLE,
  ACTION_SET_TINTLESS_DISABLE,
  ACTION_SET_EXP_METERING,
  ACTION_GET_CTRL_VALUE,
  ACTION_TOGGLE_AFR,
  ACTION_SET_ISO,
  ACTION_BRIGHTNESS_INCREASE,
  ACTION_BRIGHTNESS_DECREASE,
  ACTION_CONTRAST_INCREASE,
  ACTION_CONTRAST_DECREASE,
  ACTION_EV_INCREASE,
  ACTION_EV_DECREASE,
  ACTION_SATURATION_INCREASE,
  ACTION_SATURATION_DECREASE,
  ACTION_SET_ZOOM,
  ACTION_SHARPNESS_INCREASE,
  ACTION_SHARPNESS_DECREASE,
  ACTION_TAKE_JPEG_SNAPSHOT,
  ACTION_START_RECORDING,
  ACTION_STOP_RECORDING,
  ACTION_SET_BESTSHOT_MODE,
  ACTION_TAKE_LIVE_SNAPSHOT,
  ACTION_SET_FLASH_MODE,
  ACTION_SWITCH_CAMERA,
  ACTION_TOGGLE_ZSL,
  ACTION_TAKE_RAW_SNAPSHOT,
  ACTION_SWITCH_RESOLUTION,
  ACTION_TOGGLE_WNR,
  ACTION_EXIT
} camera_action_t;

#define INVALID_KEY_PRESS 0
#define BASE_OFFSET  ('Z' - 'A' + 1)
#define BASE_OFFSET_NUM  ('Z' - 'A' + 2)
#define PAD_TO_WORD(a)  (((a)+3)&~3)


#define SQCIF_WIDTH     128
#define SQCIF_HEIGHT     96
#define QCIF_WIDTH      176
#define QCIF_HEIGHT     144
#define QVGA_WIDTH      320
#define QVGA_HEIGHT     240
#define HD_THUMBNAIL_WIDTH      256
#define HD_THUMBNAIL_HEIGHT     144
#define CIF_WIDTH       352
#define CIF_HEIGHT      288
#define VGA_WIDTH       640
#define VGA_HEIGHT      480
#define WVGA_WIDTH      800
#define WVGA_HEIGHT     480
#define WVGA_PLUS_WIDTH      960
#define WVGA_PLUS_HEIGHT     720

#define MP1_WIDTH      1280
#define MP1_HEIGHT      960
#define MP2_WIDTH      1600
#define MP2_HEIGHT     1200
#define MP3_WIDTH      2048
#define MP3_HEIGHT     1536
#define MP5_WIDTH      2592
#define MP5_HEIGHT     1944
#define MP8_WIDTH      3264
#define MP8_HEIGHT     2448
#define MP12_WIDTH     4000
#define MP12_HEIGHT    3000

#define SVGA_WIDTH      800
#define SVGA_HEIGHT     600
#define XGA_WIDTH      1024
#define XGA_HEIGHT      768
#define HD720_WIDTH    1280
#define HD720_HEIGHT    720
#define HD720_PLUS_WIDTH    1440
#define HD720_PLUS_HEIGHT   1080
#define WXGA_WIDTH     1280
#define WXGA_HEIGHT     768
#define HD1080_WIDTH   1920
#define HD1080_HEIGHT  1080


#define ONEMP_WIDTH    1280
#define SXGA_WIDTH     1280
#define UXGA_WIDTH     1600
#define QXGA_WIDTH     2048
#define FIVEMP_WIDTH   2560


#define ONEMP_HEIGHT    960
#define SXGA_HEIGHT     1024
#define UXGA_HEIGHT     1200
#define QXGA_HEIGHT     1536
#define FIVEMP_HEIGHT   1920


typedef enum
{
  RESOLUTION_MIN,
  QCIF                  = RESOLUTION_MIN,
  QVGA,
  VGA,
  WVGA,
  WVGA_PLUS ,
  HD720,
  HD720_PLUS,
  HD1080,
  RESOLUTION_PREVIEW_VIDEO_MAX = HD1080,
  WXGA,
  MP1,
  MP2,
  MP3,
  MP5,
  MP8,
  MP12,
  RESOLUTION_MAX         = MP12,
} Camera_Resolution;

typedef struct{
    uint16_t width;
    uint16_t  height;
    char * name;
    char * str_name;
    int supported;
} DIMENSION_TBL_T;

typedef enum {
    WHITE_BALANCE_STATE,
    WHITE_BALANCE_TEMPERATURE,
    BRIGHTNESS_CTRL,
    EV,
    CONTRAST_CTRL,
    SATURATION_CTRL,
    SHARPNESS_CTRL
} Get_Ctrl_modes;

typedef enum {
    AUTO_EXP_FRAME_AVG,
    AUTO_EXP_CENTER_WEIGHTED,
    AUTO_EXP_SPOT_METERING,
    AUTO_EXP_SMART_METERING,
    AUTO_EXP_USER_METERING,
    AUTO_EXP_SPOT_METERING_ADV,
    AUTO_EXP_CENTER_WEIGHTED_ADV,
    AUTO_EXP_MAX
} Exp_Metering_modes;

typedef enum {
  ISO_AUTO,
  ISO_DEBLUR,
  ISO_100,
  ISO_200,
  ISO_400,
  ISO_800,
  ISO_1600,
  ISO_MAX
} ISO_modes;

typedef enum {
  BESTSHOT_AUTO,
  BESTSHOT_ACTION,
  BESTSHOT_PORTRAIT,
  BESTSHOT_LANDSCAPE,
  BESTSHOT_NIGHT,
  BESTSHOT_NIGHT_PORTRAIT,
  BESTSHOT_THEATRE,
  BESTSHOT_BEACH,
  BESTSHOT_SNOW,
  BESTSHOT_SUNSET,
  BESTSHOT_ANTISHAKE,
  BESTSHOT_FIREWORKS,
  BESTSHOT_SPORTS,
  BESTSHOT_PARTY,
  BESTSHOT_CANDLELIGHT,
  BESTSHOT_ASD,
  BESTSHOT_BACKLIGHT,
  BESTSHOT_FLOWERS,
  BESTSHOT_AR,
  BESTSHOT_HDR,
  BESTSHOT_MAX
}Bestshot_modes;

typedef enum {
    FLASH_MODE_OFF,
    FLASH_MODE_AUTO,
    FLASH_MODE_ON,
    FLASH_MODE_TORCH,
    FLASH_MODE_MAX,
}Flash_modes;

typedef enum {
  WB_AUTO,
  WB_INCANDESCENT,
  WB_FLUORESCENT,
  WB_WARM_FLUORESCENT,
  WB_DAYLIGHT,
  WB_CLOUDY_DAYLIGHT,
  WB_TWILIGHT,
  WB_SHADE,
  WB_MAX
} White_Balance_modes;

typedef enum
{
  MENU_ID_MAIN,
  MENU_ID_WHITEBALANCECHANGE,
  MENU_ID_EXPMETERINGCHANGE,
  MENU_ID_GET_CTRL_VALUE,
  MENU_ID_TOGGLEAFR,
  MENU_ID_ISOCHANGE,
  MENU_ID_BRIGHTNESSCHANGE,
  MENU_ID_CONTRASTCHANGE,
  MENU_ID_EVCHANGE,
  MENU_ID_SATURATIONCHANGE,
  MENU_ID_ZOOMCHANGE,
  MENU_ID_SHARPNESSCHANGE,
  MENU_ID_BESTSHOT,
  MENU_ID_FLASHMODE,
  MENU_ID_SENSORS,
  MENU_ID_SWITCH_RES,
  MENU_ID_INVALID,
} menu_id_change_t;

typedef enum
{
  DECREASE_ZOOM,
  INCREASE_ZOOM,
  INCREASE_STEP_ZOOM,
  DECREASE_STEP_ZOOM,
} Camera_Zoom;

typedef enum
{
  INC_CONTRAST,
  DEC_CONTRAST,
} Camera_Contrast_changes;

typedef enum
{
  INC_BRIGHTNESS,
  DEC_BRIGHTNESS,
} Camera_Brightness_changes;

typedef enum
{
  INCREASE_EV,
  DECREASE_EV,
} Camera_EV_changes;

typedef enum {
  INC_SATURATION,
  DEC_SATURATION,
} Camera_Saturation_changes;

typedef enum
{
  INC_ISO,
  DEC_ISO,
} Camera_ISO_changes;

typedef enum
{
  INC_SHARPNESS,
  DEC_SHARPNESS,
} Camera_Sharpness_changes;

typedef enum {
  ZOOM_IN,
  ZOOM_OUT,
} Zoom_direction;

typedef struct{
    Camera_main_menu_t main_menu;
    char * menu_name;
} CAMERA_MAIN_MENU_TBL_T;

typedef struct{
    char * menu_name;
    int present;
} CAMERA_SENSOR_MENU_TLB_T;

typedef struct{
    Camera_Resolution cs_id;
    uint16_t width;
    uint16_t  height;
    char * name;
    char * str_name;
} PREVIEW_DIMENSION_TBL_T;

typedef struct {
  White_Balance_modes wb_id;
  char * wb_name;
} WHITE_BALANCE_TBL_T;

typedef struct {
  Get_Ctrl_modes get_ctrl_id;
  char * get_ctrl_name;
} GET_CTRL_TBL_T;

typedef struct{
  Exp_Metering_modes exp_metering_id;
  char * exp_metering_name;
} EXP_METERING_TBL_T;

typedef struct {
  Bestshot_modes bs_id;
  char *name;
} BESTSHOT_MODE_TBT_T;

typedef struct {
  Flash_modes bs_id;
  char *name;
} FLASH_MODE_TBL_T;

typedef struct {
  ISO_modes iso_modes;
  char *iso_modes_name;
} ISO_TBL_T;

typedef struct {
  Zoom_direction zoom_direction;
  char * zoom_direction_name;
} ZOOM_TBL_T;

typedef struct {
  Camera_Sharpness_changes sharpness_change;
  char *sharpness_change_name;
} SHARPNESS_TBL_T;

typedef struct {
  Camera_Brightness_changes bc_id;
  char * brightness_name;
} CAMERA_BRIGHTNESS_TBL_T;

typedef struct {
  Camera_Contrast_changes cc_id;
  char * contrast_name;
} CAMERA_CONTRST_TBL_T;

typedef struct {
  Camera_EV_changes ec_id;
  char * EV_name;
} CAMERA_EV_TBL_T;

typedef struct {
  Camera_Saturation_changes sc_id;
  char * saturation_name;
} CAMERA_SATURATION_TBL_T;

typedef struct {
  Camera_Sharpness_changes bc_id;
  char * sharpness_name;
} CAMERA_SHARPNESS_TBL_T;

#endif /* __MM_QCAMERA_MAIN_MENU_H__ */
