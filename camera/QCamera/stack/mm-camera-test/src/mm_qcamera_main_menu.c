/*
Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <pthread.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#ifdef _ANDROID_
#include <cutils/log.h>
#endif
#include <dlfcn.h>

#include "camera.h"
#include "mm_camera_dbg.h"
#include "mm_qcamera_main_menu.h"
#include "mm_qcamera_display_dimensions.h"
#include "camera_defs_i.h"
#include "mm_qcamera_app.h"

#define CAMERA_OPENED 0

#define VIDEO_BUFFER_SIZE       (PREVIEW_WIDTH * PREVIEW_HEIGHT * 3/2)
#define THUMBNAIL_BUFFER_SIZE   (THUMBNAIL_WIDTH * THUMBNAIL_HEIGHT * 3/2)
#define SNAPSHOT_BUFFER_SIZE    (PICTURE_WIDTH * PICTURE_HEIGHT * 3/2)

extern int mm_app_take_zsl(int cam_id);
extern int mm_app_take_live_snapshot(int cam_id);
extern int stopPreview(int cam_id);
extern void switchRes(int cam_id);
extern void switchCamera(int cam_id);
extern int startRdi(int cam_id);
extern int stopRdi(int cam_id);

/*===========================================================================
 * Macro
 *===========================================================================*/
#define PREVIEW_FRAMES_NUM    4
#define VIDEO_FRAMES_NUM      4
#define THUMBNAIL_FRAMES_NUM  1
#define SNAPSHOT_FRAMES_NUM   1
#define MAX_NUM_FORMAT        32
/*===========================================================================
 * Defines
 *===========================================================================*/

const CAMERA_MAIN_MENU_TBL_T camera_main_menu_tbl[] = {
  {STOP_CAMERA,                   "Stop preview/video and exit camera."},
  {PREVIEW_VIDEO_RESOLUTION,      "Preview/Video Resolution: SQCIF/QCIF/"
                              "QVGA/CIF/VGA/WVGA... Default WVGA."},
  {TAKE_YUV_SNAPSHOT,       "Take a snapshot"},
  {TAKE_RAW_SNAPSHOT,       "Take a raw snapshot"},
  {TAKE_ZSL_SNAPSHOT,       "Take a ZSL snapshot"},
  {START_RECORDING,       "Start RECORDING"},
  {START_RDI, "Start RDI stream"},
  {STOP_RDI, "Stop RDI stream"},
  {SWITCH_CAMERA,       "Switch Camera"},
#if 0
  {SET_WHITE_BALANCE,          "Set white balance mode: Auto/Off/Daylight/Incandescent/Fluorescent. Default Auto."},
  {SET_EXP_METERING,          "Set exposure metering mode: FrameAverage/CenterWeighted/SpotMetering. Default CenterWeighted"},
  {GET_CTRL_VALUE,              "Get control value menu"},
  {TOGGLE_AFR,                 "Toggle auto frame rate. Default fixed frame rate"},
  {SET_ISO,                 "ISO changes."},
  {BRIGHTNESS_GOTO_SUBMENU,                               "Brightness changes."},
  {CONTRAST_GOTO_SUBMENU,                                 "Contrast changes."},
  {EV_GOTO_SUBMENU,                                       "EV changes."},
  {SATURATION_GOTO_SUBMENU,                               "Saturation changes."},
  {SET_ZOOM,          "Set Digital Zoom."},
  {SET_SHARPNESS,          "Set Sharpness."},
#endif
};

const PREVIEW_DIMENSION_TBL_T preview_video_dimension_tbl[] = {
   { SQCIF, SQCIF_WIDTH, SQCIF_HEIGHT, "SQCIF",  "Preview/Video Resolution: SQCIF <128x96>"},
   {  QCIF,  QCIF_WIDTH,  QCIF_HEIGHT,  "QCIF",  "Preview/Video Resolution: QCIF <176x144>"},
   {  QVGA,  QVGA_WIDTH,  QVGA_HEIGHT,  "QVGA",  "Preview/Video Resolution: QVGA <320x240>"},
   {   CIF,   CIF_WIDTH,   CIF_HEIGHT,   "CIF",  "Preview/Video Resolution: CIF <352x288>"},
   {   VGA,   VGA_WIDTH,   VGA_HEIGHT,   "VGA",  "Preview/Video Resolution: VGA <640x480>"},
   {  WVGA,  WVGA_WIDTH,  WVGA_HEIGHT,  "WVGA",  "Preview/Video Resolution: WVGA <800x480>"},
   {  SVGA,  SVGA_WIDTH,  SVGA_HEIGHT,  "SVGA",  "Preview/Video Resolution: SVGA <800x600>"},
   {   XGA,   XGA_WIDTH,   XGA_HEIGHT,    "XGA", "Preview/Video Resolution: XGA <1024x768>"},
   { HD720, HD720_WIDTH, HD720_HEIGHT,  "HD720", "Preview/Video Resolution: HD720 <1280x720>"},
};

const CAMERA_BRIGHTNESS_TBL_T brightness_change_tbl[] = {
  {INC_BRIGHTNESS, "Increase Brightness by one step."},
  {DEC_BRIGHTNESS, "Decrease Brightness by one step."},
};

const CAMERA_CONTRST_TBL_T contrast_change_tbl[] = {
  {INC_CONTRAST, "Increase Contrast by one step."},
  {DEC_CONTRAST, "Decrease Contrast by one step."},
};

const CAMERA_EV_TBL_T camera_EV_tbl[] = {
  {INCREASE_EV, "Increase EV by one step."},
  {DECREASE_EV, "Decrease EV by one step."},
};

const CAMERA_SATURATION_TBL_T camera_saturation_tbl[] = {
  {INC_SATURATION, "Increase Satuation by one step."},
  {DEC_SATURATION, "Decrease Satuation by one step."},
};

const CAMERA_SHARPNESS_TBL_T camera_sharpness_tbl[] = {
  {INC_SHARPNESS, "Increase Sharpness."},
  {DEC_SHARPNESS, "Decrease Sharpness."},
};

const WHITE_BALANCE_TBL_T white_balance_tbl[] = {
  { CAMERA_WB_AUTO,         "White Balance - Auto"},
  { CAMERA_WB_DAYLIGHT,     "White Balance - Daylight"},
  { CAMERA_WB_INCANDESCENT, "White Balance - Incandescent"},
  { CAMERA_WB_FLUORESCENT,  "White Balance - Fluorescent"},
  { CAMERA_WB_CLOUDY_DAYLIGHT,  "White Balance - Cloudy"},
};

const CAMERA_TBL_T cam_tbl[] = {
  { 	1,          "Back Camera"},
  { 	2,         "Front Camera"},
};

const RECORD_TBL_T record_tbl[] = {
  { 	LIVE_SNAPSHOT_MENU,          "Take Live snapshot"},
  { 	STOP_RECORDING_MENU,         "Stop Recording"},
};

const GET_CTRL_TBL_T get_ctrl_tbl[] = {
  {     WHITE_BALANCE_STATE,           "Get white balance state (auto/off)"},
  {     WHITE_BALANCE_TEMPERATURE,      "Get white balance temperature"},
  {     BRIGHTNESS_CTRL,      "Get brightness value"},
  {     EV,      "Get exposure value"},
  {     CONTRAST_CTRL,      "Get contrast value"},
  {     SATURATION_CTRL,      "Get saturation value"},
  {     SHARPNESS_CTRL,      "Get sharpness value"},
};

const EXP_METERING_TBL_T exp_metering_tbl[] = {
  {   EXP_METERING_FRAME_AVERAGE,      "Exposure Metering - Frame Average"},
  {   EXP_METERING_CENTER_WEIGHTED,    "Exposure Metering - Center Weighted"},
  {   EXP_METERING_SPOT_METERING,      "Exposure Metering - Spot Metering"},
};

const ISO_TBL_T iso_tbl[] = {
  {   ISO_AUTO, "ISO: Auto"},
  {   ISO_DEBLUR, "ISO: Deblur"},
  {   ISO_100, "ISO: 100"},
  {   ISO_200, "ISO: 200"},
  {   ISO_400, "ISO: 400"},
  {   ISO_800, "ISO: 800"},
  {   ISO_1600, "ISO: 1600"},
};

const ZOOM_TBL_T zoom_tbl[] = {
  {   ZOOM_IN, "Zoom In one step"},
  {   ZOOM_OUT, "Zoom Out one step"},
};


struct v4l2_fmtdesc enumfmtdesc[MAX_NUM_FORMAT];
struct v4l2_format current_fmt;

/*===========================================================================
 * Forward declarations
 *===========================================================================*/
static int set_fps(int fps);
static int start_snapshot (void);
static int stop_snapshot (void);
/*===========================================================================
 * Static global variables
 *===========================================================================*/
USER_INPUT_DISPLAY_T input_display;
static int camframe_status = 0;

#ifdef _ANDROID_
char *sdcard_path = "/data";
#else
char *sdcard_path = ".";
#endif

//void *libqcamera = NULL;
//void (**LINK_jpegfragment_callback)(uint8_t * buff_ptr , uint32_t buff_size);
//void (**LINK_jpeg_callback)(void);

int num_supported_fmts = 0;
int memoryType = V4L2_MEMORY_MMAP; /* default */
int preview_video_resolution_flag = 0;
int effect = CAMERA_EFFECT_OFF;
int brightness = CAMERA_DEF_BRIGHTNESS;
int contrast = CAMERA_DEF_CONTRAST;
int saturation = CAMERA_DEF_SATURATION;
int sharpness = CAMERA_DEF_SHARPNESS;
int32_t ev_num = 0;
uint8_t ezTune = FALSE;
int pmemThumbnailfd = 0;
int pmemSnapshotfd = 0;
int pmemRawSnapshotfd = 0;
int fdSnapshot = 0;
int fdThumbnail = 0;
char snapshotBuf[256] = { 0};
char thumbnailBuf[256] = { 0};
uint32_t snapshot_buff_size = 0;
uint32_t raw_snapshot_buffer_size = 0;
static int thumbnailCntr = 0, snapshotCntr = 0;
unsigned char *thumbnail_buf = NULL, *main_img_buf = NULL, *raw_img_buf = NULL;
int32_t *sharpness_AF = NULL;
struct crop_info cropInfo;
common_crop_t cropInfo_s;

interface_ctrl_t intrfcCtrl;
config3a_wb_t autoWB = CAMERA_WB_AUTO;
isp3a_af_mode_t af_mode = AF_MODE_AUTO;
cam_af_focusrect_t afFocusRect = AUTO;

cam_af_ctrl_t af_ctrl;
camera_iso_mode_type iso = CAMERA_ISO_AUTO;
camera_antibanding_type antibanding = CAMERA_ANTIBANDING_OFF;
camera_auto_exposure_mode_type aec_mode = CAMERA_AEC_CENTER_WEIGHTED;
led_mode_t led_mode = LED_MODE_OFF;
motion_iso_t motion_iso = MOTION_ISO_OFF;
int32_t hue = CAMERA_DEF_HUE;
fps_mode_t fps_mode = FPS_MODE_AUTO;

struct v4l2_cropcap cropcap;
struct v4l2_queryctrl zoom_queryctrl;
struct v4l2_queryctrl sharpness_queryctrl;
int zoom_level;

Camera_Resolution Resolution;
//int32_t g_camParmInfo_current_value = 0;
//extern unsigned long preview_frames_buf;
//extern void test_app_mmcamera_videoframe_callback(struct msm_frame *frame); // video_cam.c

/* To flush free video buffers queue */
//void (*LINK_cam_frame_flush_free_video)(void);
static int submain();

//struct v4l2_frame_buffer frames[PREVIEW_FRAMES_NUM];
//struct v4l2_frame_buffer video_frames[VIDEO_FRAMES_NUM];

//pthread_t frame_thread;

void test_app_camframe_timeout_callback(void)
{
  camframe_status = -1;
}

/*===========================================================================
 * FUNCTION    - keypress_to_event -
 *
 * DESCRIPTION:
 *==========================================================================*/
int keypress_to_event(char keypress)
{
  char out_buf = INVALID_KEY_PRESS;
  if ((keypress >= 'A' && keypress <= 'Z') ||
    (keypress >= 'a' && keypress <= 'z')) {
    out_buf = tolower(keypress);
    out_buf = out_buf - 'a' + 1;
  } else if (keypress >= '1' && keypress <= '9') {
    out_buf = keypress;
    out_buf = keypress - '1' + BASE_OFFSET_NUM;
  }
  return out_buf;
}

int next_menu(menu_id_change_t current_menu_id, char keypress, camera_action_t * action_id_ptr, int * action_param)
{
  int output_to_event;
  menu_id_change_t next_menu_id = MENU_ID_INVALID;
  * action_id_ptr = ACTION_NO_ACTION;

  output_to_event = keypress_to_event(keypress);
  CDBG("current_menu_id=%d\n",current_menu_id);
  CDBG("output_to_event=%d\n",output_to_event);
  switch(current_menu_id) {
    case MENU_ID_MAIN:
      switch(output_to_event) {
        case STOP_CAMERA:
          * action_id_ptr = ACTION_STOP_CAMERA;
          CDBG("STOP_CAMERA\n");
          break;

        case PREVIEW_VIDEO_RESOLUTION:
          next_menu_id = MENU_ID_PREVIEWVIDEORESOLUTIONCHANGE;
          CDBG("next_menu_id = MENU_ID_PREVIEWVIDEORESOLUTIONCHANGE = %d\n", next_menu_id);
          break;
#if 0
        case SET_WHITE_BALANCE:
          next_menu_id = MENU_ID_WHITEBALANCECHANGE;
          CDBG("next_menu_id = MENU_ID_WHITEBALANCECHANGE = %d\n", next_menu_id);
          break;

        case SET_EXP_METERING:
          next_menu_id = MENU_ID_EXPMETERINGCHANGE;
          CDBG("next_menu_id = MENU_ID_EXPMETERINGCHANGE = %d\n", next_menu_id);
          break;

        case GET_CTRL_VALUE:
          next_menu_id = MENU_ID_GET_CTRL_VALUE;
          CDBG("next_menu_id = MENU_ID_GET_CTRL_VALUE = %d\n", next_menu_id);
          break;

        case BRIGHTNESS_GOTO_SUBMENU:
          next_menu_id = MENU_ID_BRIGHTNESSCHANGE;
          CDBG("next_menu_id = MENU_ID_BRIGHTNESSCHANGE = %d\n", next_menu_id);
          break;

        case CONTRAST_GOTO_SUBMENU:
          next_menu_id = MENU_ID_CONTRASTCHANGE;
          break;

        case EV_GOTO_SUBMENU:
          next_menu_id = MENU_ID_EVCHANGE;
          break;

        case SATURATION_GOTO_SUBMENU:
          next_menu_id = MENU_ID_SATURATIONCHANGE;
          break;

        case TOGGLE_AFR:
          * action_id_ptr = ACTION_TOGGLE_AFR;
          CDBG("next_menu_id = MENU_ID_TOGGLEAFR = %d\n", next_menu_id);
          break;

        case SET_ISO:
          next_menu_id = MENU_ID_ISOCHANGE;
          CDBG("next_menu_id = MENU_ID_ISOCHANGE = %d\n", next_menu_id);
          break;

        case SET_ZOOM:
          next_menu_id = MENU_ID_ZOOMCHANGE;
          CDBG("next_menu_id = MENU_ID_ZOOMCHANGE = %d\n", next_menu_id);
          break;

        case SET_SHARPNESS:
          next_menu_id = MENU_ID_SHARPNESSCHANGE;
          CDBG("next_menu_id = MENU_ID_SHARPNESSCHANGE = %d\n", next_menu_id);
          break;
#endif
        case TAKE_YUV_SNAPSHOT:
          * action_id_ptr = ACTION_TAKE_YUV_SNAPSHOT;
          CDBG("Taking YUV snapshot\n");
          break;

        case TAKE_RAW_SNAPSHOT:
          * action_id_ptr = ACTION_TAKE_RAW_SNAPSHOT;
          CDBG("Taking RAW snapshot\n");
          break;
        case START_RECORDING:
          *action_id_ptr = ACTION_START_RECORDING;
          next_menu_id = MENU_ID_RECORD;
          CDBG("Start recording\n");
          break;
        /*case STOP_RECORDING:
          * action_id_ptr = ACTION_STOP_RECORDING;
          CDBG("Stop recording\n");
          break;*/
        case SWITCH_CAMERA:
          next_menu_id = MENU_ID_SWITCHCAMERA;
          CDBG("SWitch Camera\n");
          break;
        case TAKE_ZSL_SNAPSHOT:
          * action_id_ptr = ACTION_TAKE_ZSL_SNAPSHOT;
          CDBG("Taking ZSL snapshot\n");
          break;
        case START_RDI:
          * action_id_ptr = ACTION_START_RDI;
          break;
        case STOP_RDI:
          * action_id_ptr = ACTION_STOP_RDI;
          break;
        default:
          next_menu_id = MENU_ID_MAIN;
          CDBG("next_menu_id = MENU_ID_MAIN = %d\n", next_menu_id);
          break;
      }
      break;

    case MENU_ID_PREVIEWVIDEORESOLUTIONCHANGE:
      printf("MENU_ID_PREVIEWVIDEORESOLUTIONCHANGE\n");
      * action_id_ptr = ACTION_PREVIEW_VIDEO_RESOLUTION;
      if (output_to_event > RESOLUTION_PREVIEW_VIDEO_MAX ||
        output_to_event < RESOLUTION_MIN) {
          next_menu_id = current_menu_id;
      }
      else {
        next_menu_id = MENU_ID_MAIN;
        * action_param = output_to_event;
      }
      break;

    case MENU_ID_WHITEBALANCECHANGE:
      printf("MENU_ID_WHITEBALANCECHANGE\n");
      * action_id_ptr = ACTION_SET_WHITE_BALANCE;
      if (output_to_event > 0 &&
        output_to_event <= sizeof(white_balance_tbl)/sizeof(white_balance_tbl[0])) {
          next_menu_id = MENU_ID_MAIN;
          * action_param = output_to_event;
      }
      else {
        next_menu_id = current_menu_id;
      }
      break;

    case MENU_ID_EXPMETERINGCHANGE:
      printf("MENU_ID_EXPMETERINGCHANGE\n");
      * action_id_ptr = ACTION_SET_EXP_METERING;
      if (output_to_event > 0 &&
        output_to_event <= sizeof(exp_metering_tbl)/sizeof(exp_metering_tbl[0])) {
          next_menu_id = MENU_ID_MAIN;
          * action_param = output_to_event;
      }
      else {
        next_menu_id = current_menu_id;
      }
      break;

    case MENU_ID_GET_CTRL_VALUE:
      printf("MENU_ID_GET_CTRL_VALUE\n");
      * action_id_ptr = ACTION_GET_CTRL_VALUE;
      if (output_to_event > 0 &&
        output_to_event <= sizeof(get_ctrl_tbl)/sizeof(get_ctrl_tbl[0])) {
          next_menu_id = MENU_ID_MAIN;
          * action_param = output_to_event;
      }
      else {
        next_menu_id = current_menu_id;
      }
      break;

    case MENU_ID_BRIGHTNESSCHANGE:
      switch (output_to_event) {
        case INC_BRIGHTNESS:
          * action_id_ptr = ACTION_BRIGHTNESS_INCREASE;
          next_menu_id = MENU_ID_MAIN;
          break;

        case DEC_BRIGHTNESS:
          * action_id_ptr = ACTION_BRIGHTNESS_DECREASE;
          next_menu_id = MENU_ID_MAIN;
          break;

        default:
          next_menu_id = MENU_ID_BRIGHTNESSCHANGE;
          break;
      }
      break;

    case MENU_ID_CONTRASTCHANGE:
      switch (output_to_event) {
        case INC_CONTRAST:
          * action_id_ptr = ACTION_CONTRAST_INCREASE;
          next_menu_id = MENU_ID_MAIN;
          break;

        case DEC_CONTRAST:
          * action_id_ptr = ACTION_CONTRAST_DECREASE;
          next_menu_id = MENU_ID_MAIN;
          break;

        default:
          next_menu_id = MENU_ID_CONTRASTCHANGE;
          break;
      }
      break;

    case MENU_ID_EVCHANGE:
      switch (output_to_event) {
        case INCREASE_EV:
          * action_id_ptr = ACTION_EV_INCREASE;
          next_menu_id = MENU_ID_MAIN;
          break;

        case DECREASE_EV:
          * action_id_ptr = ACTION_EV_DECREASE;
          next_menu_id = MENU_ID_MAIN;
          break;

        default:
          next_menu_id = MENU_ID_EVCHANGE;
          break;
      }
      break;

    case MENU_ID_SATURATIONCHANGE:
      switch (output_to_event) {
        case INC_SATURATION:
          * action_id_ptr = ACTION_SATURATION_INCREASE;
          next_menu_id = MENU_ID_MAIN;
          break;

        case DEC_SATURATION:
          * action_id_ptr = ACTION_SATURATION_DECREASE;
          next_menu_id = MENU_ID_MAIN;
          break;

        default:
          next_menu_id = MENU_ID_EVCHANGE;
          break;
      }
      break;

    case MENU_ID_ISOCHANGE:
      printf("MENU_ID_ISOCHANGE\n");
      * action_id_ptr = ACTION_SET_ISO;
      if (output_to_event > 0 &&
        output_to_event <= sizeof(iso_tbl)/sizeof(iso_tbl[0])) {
          next_menu_id = MENU_ID_MAIN;
          * action_param = output_to_event;
      } else {
        next_menu_id = current_menu_id;
      }
      break;

    case MENU_ID_ZOOMCHANGE:
      * action_id_ptr = ACTION_SET_ZOOM;
      if (output_to_event > 0 &&
        output_to_event <= sizeof(zoom_tbl)/sizeof(zoom_tbl[0])) {
          next_menu_id = MENU_ID_MAIN;
          * action_param = output_to_event;
      } else {
        next_menu_id = current_menu_id;
      }
      break;

    case MENU_ID_SHARPNESSCHANGE:
      switch (output_to_event) {
        case INC_SHARPNESS:
          * action_id_ptr = ACTION_SHARPNESS_INCREASE;
          next_menu_id = MENU_ID_MAIN;
          break;
        case DEC_SHARPNESS:
          * action_id_ptr = ACTION_SHARPNESS_DECREASE;
          next_menu_id = MENU_ID_MAIN;
          break;
        default:
          next_menu_id = MENU_ID_SHARPNESSCHANGE;
          break;
      }
      break;
    case MENU_ID_SWITCHCAMERA:
      * action_id_ptr = ACTION_SWITCH_CAMERA;
      if (output_to_event >= 0 &&
        output_to_event <= sizeof(cam_tbl)/sizeof(cam_tbl[0])) {
          next_menu_id = MENU_ID_MAIN;
          * action_param = output_to_event;
      } else {
        next_menu_id = current_menu_id;
      }
      break;
   case MENU_ID_RECORD:
      switch (output_to_event) {
        case LIVE_SNAPSHOT_MENU:
          * action_id_ptr = ACTION_TAKE_LIVE_SNAPSHOT;
          next_menu_id = MENU_ID_RECORD;
          break;

        default:
        case STOP_RECORDING_MENU:
          * action_id_ptr = ACTION_STOP_RECORDING;
          next_menu_id = MENU_ID_MAIN;
          break;
      }
   default:
      CDBG("menu id is wrong: %d\n", current_menu_id);
      break;
  }

  return next_menu_id;
}

/*===========================================================================
 * FUNCTION    - print_menu_preview_video -
 *
 * DESCRIPTION:
 * ===========================================================================*/
static void print_menu_preview_video(void) {
  unsigned int i;

  printf("\n");
  printf("===========================================\n");
  printf("      Camera is in preview/video mode now        \n");
  printf("===========================================\n\n");

  char menuNum = 'A';
  for (i = 0; i < sizeof(camera_main_menu_tbl)/sizeof(camera_main_menu_tbl[0]); i++) {
    if (i == BASE_OFFSET) {
      menuNum = '1';
    }

    printf("%c.  %s\n", menuNum, camera_main_menu_tbl[i].menu_name);
    menuNum++;
  }

  printf("\nPlease enter your choice: ");

  return;
}

static void camera_preview_video_resolution_change_tbl(void) {
    unsigned int i;

    printf("\n");
    printf("==========================================================\n");
    printf("      Camera is in preview/video resolution mode       \n");
    printf("==========================================================\n\n");

    char previewVideomenuNum = 'A';
    for (i = 0; i < sizeof(preview_video_dimension_tbl) /
      sizeof(preview_video_dimension_tbl[0]); i++) {
        printf("%c.  %s\n", previewVideomenuNum,
          preview_video_dimension_tbl[i].str_name);
        previewVideomenuNum++;
    }

    printf("\nPlease enter your choice for Preview/Video Resolution: ");
    return;
}

static void camera_preview_video_wb_change_tbl(void) {
  unsigned int i;
  printf("\n");
  printf("==========================================================\n");
  printf("      Camera is in white balance change mode       \n");
  printf("==========================================================\n\n");

  char submenuNum = 'A';
  for (i = 0 ; i < sizeof(white_balance_tbl) /
                   sizeof(white_balance_tbl[0]); i++) {
        //printf("%c.  %s\n", submenuNum, white_balance_tbl[i].wb_name);
        submenuNum++;
  }
  printf("\nPlease enter your choice for White Balance modes: ");
  return;
}

static void camera_preview_video_get_ctrl_value_tbl(void) {
  unsigned int i;
  printf("\n");
  printf("==========================================================\n");
  printf("      Camera is in get control value mode       \n");
  printf("==========================================================\n\n");

  char submenuNum = 'A';
  for (i = 0 ; i < sizeof(get_ctrl_tbl) /
                   sizeof(get_ctrl_tbl[0]); i++) {
        printf("%c.  %s\n", submenuNum, get_ctrl_tbl[i].get_ctrl_name);
        submenuNum++;
  }
  printf("\nPlease enter your choice for control value you want to get: ");
  return;
}

static void camera_preview_video_exp_metering_change_tbl(void) {
  unsigned int i;
  printf("\n");
  printf("==========================================================\n");
  printf("      Camera is in exposure metering change mode       \n");
  printf("==========================================================\n\n");

  char submenuNum = 'A';
  for (i = 0 ; i < sizeof(exp_metering_tbl) /
                   sizeof(exp_metering_tbl[0]); i++) {
        printf("%c.  %s\n", submenuNum, exp_metering_tbl[i].exp_metering_name);
        submenuNum++;
  }
  printf("\nPlease enter your choice for exposure metering modes: ");
  return;
}

static void camera_contrast_change_tbl(void) {
    unsigned int i;

    printf("\n");
    printf("==========================================================\n");
    printf("      Camera is in change contrast resolution mode       \n");
    printf("==========================================================\n\n");

    char contrastmenuNum = 'A';
    for (i = 0; i < sizeof(contrast_change_tbl) /
                    sizeof(contrast_change_tbl[0]); i++) {
        printf("%c.  %s\n", contrastmenuNum,
                            contrast_change_tbl[i].contrast_name);
        contrastmenuNum++;
    }

    printf("\nPlease enter your choice for contrast Change: ");
    return;
}

static void camera_EV_change_tbl(void) {
  unsigned int i;

  printf("\n");
  printf("===========================================\n");
  printf("      Camera is in EV change mode now       \n");
  printf("===========================================\n\n");

  char submenuNum = 'A';
  for (i = 0; i < sizeof(camera_EV_tbl)/sizeof(camera_EV_tbl[0]); i++) {
    printf("%c.  %s\n", submenuNum, camera_EV_tbl[i].EV_name);
    submenuNum++;
  }

  printf("\nPlease enter your choice for EV changes: ");
  return;
}

static void camera_preview_video_zoom_change_tbl(void) {
  unsigned int i;
  struct v4l2_control ctrl;

  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_ZOOM_ABSOLUTE;
#if 0 /* TBD */
  if (ioctl(camfd, VIDIOC_G_CTRL, &ctrl) >= 0) {
    zoom_level = ctrl.value;
    printf("\n");
    printf("==========================================================\n");
    printf("      Camera is in zoom change mode: %d,  [%d..%d]        \n",
        ctrl.value, zoom_queryctrl.minimum, zoom_queryctrl.maximum);
    printf("==========================================================\n\n");

    char submenuNum = 'A';
    for (i = 0 ; i < sizeof(zoom_tbl) /
                   sizeof(zoom_tbl[0]); i++) {
        printf("%c.  %s\n", submenuNum, zoom_tbl[i].zoom_direction_name);
        submenuNum++;
    }
    printf("\nPlease enter your choice for zoom change direction: ");
  } else {
    printf("\nVIDIOC_G_CTRL error: %d\n", errno);
  }
#endif /* TBD */
  return;
}

static void camera_brightness_change_tbl(void) {
    unsigned int i;

    printf("\n");
    printf("==========================================================\n");
    printf("      Camera is in change brightness mode       \n");
    printf("==========================================================\n\n");

    char brightnessmenuNum = 'A';
    for (i = 0; i < sizeof(brightness_change_tbl) /
                    sizeof(brightness_change_tbl[0]); i++) {
        printf("%c.  %s\n", brightnessmenuNum,
                            brightness_change_tbl[i].brightness_name);
        brightnessmenuNum++;
    }

    printf("\nPlease enter your choice for Brightness Change: ");
    return;
}

static void camera_saturation_change_tbl(void) {
    unsigned int i;

    printf("\n");
    printf("==========================================================\n");
    printf("      Camera is in change saturation mode       \n");
    printf("==========================================================\n\n");

    char saturationmenuNum = 'A';
    for (i = 0; i < sizeof(camera_saturation_tbl) /
                    sizeof(camera_saturation_tbl[0]); i++) {
        printf("%c.  %s\n", saturationmenuNum,
                            camera_saturation_tbl[i].saturation_name);
        saturationmenuNum++;
    }

    printf("\nPlease enter your choice for Saturation Change: ");
    return;
}

char * set_preview_video_dimension_tbl(Camera_Resolution cs_id, uint16_t * width, uint16_t * height)
{
  unsigned int i;
  char * ptr = NULL;
  for (i = 0; i < sizeof(preview_video_dimension_tbl) /
    sizeof(preview_video_dimension_tbl[0]); i++) {
      if (cs_id == preview_video_dimension_tbl[i].cs_id) {
        *width = preview_video_dimension_tbl[i].width;
        *height = preview_video_dimension_tbl[i].height;
        ptr = preview_video_dimension_tbl[i].name;
        break;
      }
  }
  return ptr;
}

static void camera_preview_video_iso_change_tbl(void) {
  unsigned int i;
  printf("\n");
  printf("==========================================================\n");
  printf("      Camera is in ISO change mode       \n");
  printf("==========================================================\n\n");

  char submenuNum = 'A';
  for (i = 0 ; i < sizeof(iso_tbl) /
                   sizeof(iso_tbl[0]); i++) {
        printf("%c.  %s\n", submenuNum, iso_tbl[i].iso_modes_name);
        submenuNum++;
  }
  printf("\nPlease enter your choice for iso modes: ");
  return;
}

static void camera_preview_video_sharpness_change_tbl(void) {
  unsigned int i;
  printf("\n");
  printf("==========================================================\n");
  printf("      Camera is in sharpness change mode       \n");
  printf("==========================================================\n\n");

  char submenuNum = 'A';
  for (i = 0 ; i < sizeof(camera_sharpness_tbl) /
                   sizeof(camera_sharpness_tbl[0]); i++) {
        printf("%c.  %s\n", submenuNum, camera_sharpness_tbl[i].sharpness_name);
        submenuNum++;
  }
  printf("\nPlease enter your choice for sharpness modes: ");
  return;
}

static void camera_record_tbl(void) {
  unsigned int i;
  printf("\n");
  printf("==========================================================\n");
  printf("      Camera is in record mode       \n");
  printf("==========================================================\n\n");

  char submenuNum = 'A';
  for (i = 0 ; i < sizeof(record_tbl) /
                   sizeof(record_tbl[0]); i++) {
        printf("%c.  %s\n", submenuNum, record_tbl[i].act_name);
        submenuNum++;
  }
  printf("\nPlease enter your choice:  ");
  return;
}

static void camera_switch_tbl(void) {
  unsigned int i;
  printf("\n");
  printf("==========================================================\n");
  printf("      Camera is in switch camera mode       \n");
  printf("==========================================================\n\n");

  char submenuNum = 'A';
  for (i = 0 ; i < sizeof(cam_tbl) /
                   sizeof(cam_tbl[0]); i++) {
        printf("%c.  %s\n", submenuNum, cam_tbl[i].cam_name);
        submenuNum++;
  }
  printf("\nPlease enter your choice for camera modes: ");
  return;
}
/*===========================================================================
 * FUNCTION     - increase_contrast -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int increase_contrast (void) {
  ++contrast;
  if (contrast > CAMERA_MAX_CONTRAST) {
    contrast = CAMERA_MAX_CONTRAST;
    printf("Reached max CONTRAST. \n");
  } else
    printf("Increase CONTRAST to %d\n", contrast);

  /*intrfcCtrl.setContrast(camfd, contrast);*/

  struct v4l2_queryctrl queryctrl;
  struct v4l2_control control;

  memset (&queryctrl, 0, sizeof (queryctrl));
  queryctrl.id = V4L2_CID_CONTRAST;
#if 0 /* TBD */
  if (-1 == ioctl (camfd, VIDIOC_QUERYCTRL, &queryctrl)) {
    if (errno != EINVAL) {
      perror ("VIDIOC_QUERYCTRL");
      exit (EXIT_FAILURE);
    } else {
      printf ("V4L2_CID_contrast is not supported\n");
    }
  } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    printf ("V4L2_CID_contrast is not supported\n");
  } else {
    memset (&control, 0, sizeof (control));
    control.id = V4L2_CID_CONTRAST;
    /* Decreasing the contrast */
    control.value = contrast;

 //   if (-1 == ioctl (camfd, VIDIOC_S_CTRL, &control)) {
 //     perror ("VIDIOC_S_CTRL");
 //     return -1;
 //   }
  }
#endif /* TBD */
  return 0;
}

/*===========================================================================
 * FUNCTION     - decrease_contrast -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int decrease_contrast (void) {
  --contrast;
  if (contrast < CAMERA_MIN_CONTRAST) {
    contrast = CAMERA_MIN_CONTRAST;
    printf("Reached min CONTRAST. \n");
  } else
    printf("Decrease CONTRAST to %d\n", contrast);

  /*intrfcCtrl.setContrast(camfd, contrast);*/
  struct v4l2_queryctrl queryctrl;
  struct v4l2_control control;

  memset (&queryctrl, 0, sizeof (queryctrl));
  queryctrl.id = V4L2_CID_CONTRAST;
#if 0 /* TBD */
  if (-1 == ioctl (camfd, VIDIOC_QUERYCTRL, &queryctrl)) {
    if (errno != EINVAL) {
      perror ("VIDIOC_QUERYCTRL");
      exit (EXIT_FAILURE);
    } else {
      printf ("V4L2_CID_contrast is not supported\n");
    }
  } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    printf ("V4L2_CID_contrast is not supported\n");
  } else {
    memset (&control, 0, sizeof (control));
    control.id = V4L2_CID_CONTRAST;
    /* Decreasing the contrast */
    control.value = contrast;

  //  if (-1 == ioctl (camfd, VIDIOC_S_CTRL, &control)) {
  //    perror ("VIDIOC_S_CTRL");
  //    return -1;
  //  }
  }
#endif /* TBD */
  return 0;
}

/*===========================================================================
 * FUNCTION     - decrease_brightness -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int decrease_brightness (void) {
  brightness -= CAMERA_BRIGHTNESS_STEP;
  if (brightness < CAMERA_MIN_BRIGHTNESS) {
    brightness = CAMERA_MIN_BRIGHTNESS;
    printf("Reached min BRIGHTNESS. \n");
  } else
    printf("Decrease BRIGHTNESS to %d\n", brightness);

  struct v4l2_queryctrl queryctrl;
  struct v4l2_control control;

  memset (&queryctrl, 0, sizeof (queryctrl));
  queryctrl.id = V4L2_CID_BRIGHTNESS;
#if 0 /* TBD */
  if (-1 == ioctl (camfd, VIDIOC_QUERYCTRL, &queryctrl)) {
    if (errno != EINVAL) {
      perror ("VIDIOC_QUERYCTRL");
      exit (EXIT_FAILURE);
    } else {
      printf ("V4L2_CID_BRIGHTNESS is not supported\n");
    }
  } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    printf ("V4L2_CID_BRIGHTNESS is not supported\n");
  } else {
    memset (&control, 0, sizeof (control));
    control.id = V4L2_CID_BRIGHTNESS;
    /* Decreasing the Brightness */
    control.value = brightness;

    if (-1 == ioctl (camfd, VIDIOC_S_CTRL, &control)) {
      perror ("VIDIOC_S_CTRL");
      return -1;
    }
  }
#endif /* TBD */
  return 0;
}

/*===========================================================================
 * FUNCTION     - increase_brightness -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int increase_brightness (void) {
  brightness += CAMERA_BRIGHTNESS_STEP;
  if (brightness > CAMERA_MAX_BRIGHTNESS) {
    brightness = CAMERA_MAX_BRIGHTNESS;
    printf("Reached max BRIGHTNESS. \n");
  } else
    printf("Increase BRIGHTNESS to %d\n", brightness);

  struct v4l2_queryctrl queryctrl;
  struct v4l2_control control;

  memset (&queryctrl, 0, sizeof (queryctrl));
  queryctrl.id = V4L2_CID_BRIGHTNESS;
#if 0 /* TBD */
  if (-1 == ioctl (camfd, VIDIOC_QUERYCTRL, &queryctrl)) {
    if (errno != EINVAL) {
      perror ("VIDIOC_QUERYCTRL");
      exit (EXIT_FAILURE);
    } else {
      printf ("V4L2_CID_BRIGHTNESS is not supported\n");
    }
  } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    printf ("V4L2_CID_BRIGHTNESS is not supported\n");
  } else {
    memset (&control, 0, sizeof (control));
    control.id = V4L2_CID_BRIGHTNESS;
    /* Increasing the Brightness */
    control.value = brightness;

    if (-1 == ioctl (camfd, VIDIOC_S_CTRL, &control)) {
      perror ("VIDIOC_S_CTRL");
      return -1;
    }
  }
#endif /* TBD */
  return 0;
}

/*===========================================================================
 * FUNCTION     - increase_EV -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int increase_EV (void) {
  int32_t ev = 0;
  if (++ev_num <= 12) {
     ev = (ev_num << 16) | 6;
    printf("Increase EV to %d\n", ev_num);
  } else {
    printf("Reached max EV. \n");
    ev = ev_num;
  }

  struct v4l2_queryctrl queryctrl;
  struct v4l2_control control;

  memset (&queryctrl, 0, sizeof (queryctrl));
  queryctrl.id = V4L2_CID_EXPOSURE;
#if 0 /* TBD */
  if (-1 == ioctl (camfd, VIDIOC_QUERYCTRL, &queryctrl)) {
    if (errno != EINVAL) {
      perror ("VIDIOC_QUERYCTRL");
      exit (EXIT_FAILURE);
    } else {
      printf ("V4L2_CID_EXPOSURE is not supported\n");
    }
  } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    printf ("V4L2_CID_EXPOSURE is not supported\n");
  } else {
    memset (&control, 0, sizeof (control));
    control.id = V4L2_CID_EXPOSURE;
    /* Increasing the EV*/
    control.value = ev;

    if (-1 == ioctl (camfd, VIDIOC_S_CTRL, &control)) {
      perror ("VIDIOC_S_CTRL");
      return -1;
    }
  }
#endif /* TBD */
  return 0;
}

/*===========================================================================
 * FUNCTION     - decrease_EV -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int decrease_EV (void) {
  int32_t ev = 0;
  if (--ev_num > -12) {
    ev = (ev_num << 16) | 6;
    printf("Decrease EV to %d\n", ev_num);
  } else {
    printf("Reached min EV. \n");
    ev = ev_num;
  }

  struct v4l2_queryctrl queryctrl;
  struct v4l2_control control;

  memset (&queryctrl, 0, sizeof (queryctrl));
  queryctrl.id = V4L2_CID_EXPOSURE;
#if 0 /* TBD */
  if (-1 == ioctl (camfd, VIDIOC_QUERYCTRL, &queryctrl)) {
    if (errno != EINVAL) {
      perror ("VIDIOC_QUERYCTRL");
      exit (EXIT_FAILURE);
    } else {
      printf ("V4L2_CID_EXPOSURE is not supported\n");
    }
  } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    printf ("V4L2_CID_EXPOSURE is not supported\n");
  } else {
    memset (&control, 0, sizeof (control));
    control.id = V4L2_CID_EXPOSURE;
    /* Increasing the EV*/
    control.value = ev;

    if (-1 == ioctl (camfd, VIDIOC_S_CTRL, &control)) {
      perror ("VIDIOC_S_CTRL");
      return -1;
    }
  }
#endif /* TBD */
  return 0;
}

/*===========================================================================
 * FUNCTION     - increase_contrast -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int increase_saturation (void) {
  ++saturation;
  if (saturation > CAMERA_MAX_SATURATION) {
    saturation = CAMERA_MAX_SATURATION;
    printf("Reached max saturation. \n");
  } else
    printf("Increase saturation to %d\n", saturation);

  /*intrfcCtrl.setContrast(camfd, contrast);*/

  struct v4l2_queryctrl queryctrl;
  struct v4l2_control control;

  memset (&queryctrl, 0, sizeof (queryctrl));
  queryctrl.id = V4L2_CID_SATURATION;
#if 0 /* TBD */
  if (-1 == ioctl (camfd, VIDIOC_QUERYCTRL, &queryctrl)) {
    if (errno != EINVAL) {
      perror ("VIDIOC_QUERYCTRL");
      exit (EXIT_FAILURE);
    } else {
      printf ("V4L2_CID_saturation is not supported\n");
    }
  } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    printf ("V4L2_CID_saturation is not supported\n");
  } else {
    memset (&control, 0, sizeof (control));
    control.id = V4L2_CID_SATURATION;
    /* Decreasing the contrast */
    control.value = saturation;

    if (-1 == ioctl (camfd, VIDIOC_S_CTRL, &control)) {
      perror ("VIDIOC_S_CTRL");
      return -1;
    }
  }
#endif /* TBD */
  return 0;
}

/*===========================================================================
 * FUNCTION     - decrease_saturation -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int decrease_saturation (void) {
  --saturation;
  if (saturation < CAMERA_MIN_SATURATION) {
    saturation = CAMERA_MIN_SATURATION;
    printf("Reached min saturation. \n");
  } else
    printf("Decrease saturation to %d\n", saturation);

  /*intrfcCtrl.setContrast(camfd, contrast);*/
  struct v4l2_queryctrl queryctrl;
  struct v4l2_control control;

  memset (&queryctrl, 0, sizeof (queryctrl));
  queryctrl.id = V4L2_CID_SATURATION;
#if 0 /* TBD */
  if (-1 == ioctl (camfd, VIDIOC_QUERYCTRL, &queryctrl)) {
    if (errno != EINVAL) {
      perror ("VIDIOC_QUERYCTRL");
      exit (EXIT_FAILURE);
    } else {
      printf ("V4L2_CID_saturation is not supported\n");
    }
  } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
    printf ("V4L2_CID_saturation is not supported\n");
  } else {
    memset (&control, 0, sizeof (control));
    control.id = V4L2_CID_SATURATION;
    /* Decreasing the contrast */
    control.value = saturation;

    if (-1 == ioctl (camfd, VIDIOC_S_CTRL, &control)) {
      perror ("VIDIOC_S_CTRL");
      return -1;
    }
  }
#endif /* TBD */
  return 0;
}

int takePicture_yuv(int cam_id)
{
  int rc = 0;
  CDBG("%s:BEGIN\n", __func__);
  if(0 != (rc = mm_app_take_picture(cam_id))) {
    CDBG_ERROR("%s: mm_app_take_picture() err=%d\n", __func__, rc);
  }
  return rc;
}

int takePicture_zsl(int cam_id)
{
  int rc = 0;
  CDBG("%s:BEGIN\n", __func__);
  if(0 != (rc = mm_app_take_zsl(cam_id))) {
    CDBG_ERROR("%s: mm_app_take_picture() err=%d\n", __func__, rc);
  }
  return rc;
}

int takePicture_live(int cam_id)
{
  int rc = 0;
  CDBG("%s:BEGIN\n", __func__);
  if(0 != (rc = mm_app_take_live_snapshot(cam_id))) {
    CDBG_ERROR("%s: mm_app_take_picture() err=%d\n", __func__, rc);
  }
  return rc;
}

int takePicture_raw(int cam_id)
{
  int rc = 0;
  CDBG("%s:BEGIN\n", __func__);
  if(0 != (rc = mm_app_take_raw(cam_id))) {
    CDBG("%s: mm_app_take_raw_picture() err=%d\n", __func__, rc);
  }
  return rc;
}
int system_dimension_set(int cam_id)
{
  static cam_ctrl_dimension_t dim;
  int rc = 0;

  if (preview_video_resolution_flag == 0) {
    mm_app_set_dim_def(&dim);
  } else {
    dim.video_width = input_display.user_input_display_width;
    dim.video_width = CEILING32(dim.video_width);
    dim.video_height = input_display.user_input_display_height;
    dim.orig_video_width = dim.video_width;
    dim.orig_video_height = dim.video_height;
    dim.display_width = dim.video_width;
    dim.display_height = dim.video_height;
  }
  rc = mm_app_set_dim(cam_id, &dim);
  return rc;
}

/*int run_test_harness()
{
    int good_test_cnt = 0;

    rc = run_test_1();
    if(rc < 0) 
        CDBG_EROR("%s: run_test_1 err = %d", __func__, rc);
    rc = run_test_2();
}*/
/*===========================================================================
 * FUNCTION    - main -
 *
 * DESCRIPTION:
 *==========================================================================*/
int main(int argc, char **argv)
{
  int keep_on_going = 1;
  int c, rc = 0, tmp_fd;
  int run_tc = 0;
  int run_dual_tc = 0;
  struct v4l2_capability v4l2_cap;

  /* get v4l2 params - memory type etc */
  while ((c = getopt(argc, argv, "tdh")) != -1) {
    //printf("usage: %s [-m] [-u] [-o]\n", argv[1]);
    switch (c) {
#if 0
      case 'm':
        memoryType = V4L2_MEMORY_MMAP;
        break;

      case 'o':
        /*use_overlay_fb_display_driver();*/
        break;
      case 'u':
        memoryType = V4L2_MEMORY_USERPTR;
        break;
#endif
      case 't':
        run_tc = 1;
        break;
      case 'd':
        run_dual_tc = 1;
        break;
      case 'h':
      default:
        printf("usage: %s [-m] [-u] [-o]\n", argv[0]);
        printf("-m:   V4L2_MEMORY_MMAP.      \n");
        printf("-o:   use overlay fb display driver\n");
        printf("-u:   V4L2_MEMORY_USERPTR\n");
        exit(0);
    }
  }

  CDBG("\nCamera Test Application\n");

  struct timeval tdBeforePreviewVideo, tdStopCamera;
  struct timezone tz;

  //return run_test_harness();
  if((rc = mm_app_load_hal())) {
    CDBG_ERROR("%s:mm_app_init err=%d\n", __func__, rc);
    exit(-1);
  }
    /* we must init mm_app first */
  if(mm_app_init() != MM_CAMERA_OK) {
    CDBG_ERROR("%s:mm_app_init err=%d\n", __func__, rc);
    exit(-1);
  }

  if(run_tc) {
    printf("\tRunning unit test engine only\n");
    rc = mm_app_unit_test();
    printf("\tUnit test engine. EXIT(%d)!!!\n", rc);
    exit(rc);
  }

  if(run_dual_tc) {
    printf("\tRunning Dual camera test engine only\n");
    rc = mm_app_dual_test();
    printf("\t Dual camera engine. EXIT(%d)!!!\n", rc);
    exit(rc);
  }

  gettimeofday(&tdBeforePreviewVideo, &tz);

  CDBG("Profiling: Start Camera timestamp = %ld ms\n",
    (tdBeforePreviewVideo.tv_sec * 1000) + (tdBeforePreviewVideo.tv_usec/1000));

  /* launch the primary camera in default mode */
  if( mm_app_open(CAMERA_OPENED)) {
    CDBG_ERROR("%s:mm_app_open() err=%d\n",__func__, rc);
    exit(-2);
  }

  /* main loop doing the work*/
  do {
    keep_on_going = submain();
  } while ( keep_on_going );

  /* Clean up and exit. */
  CDBG("Exiting the app\n");

error_ionfd_open:
  mm_app_close(CAMERA_OPENED);

  gettimeofday(&tdStopCamera, &tz);
  CDBG("Exiting application\n");
  CDBG("Profiling: Stop camera end timestamp = %ld ms\n",
      (tdStopCamera.tv_sec * 1000) + (tdStopCamera.tv_usec/1000));

  return 0;
}


/*===========================================================================
 * FUNCTION     - submain -
 *
 * DESCRIPTION:
 * ===========================================================================*/
static int submain()
{
  int rc = 0;
  int back_mainflag = 0;
  char tc_buf[3];
  int stop_preview = 1;
  menu_id_change_t current_menu_id = MENU_ID_MAIN, next_menu_id;
  camera_action_t action_id;
  int action_param;
  int i;
  int cam_id;

  CDBG("%s:E", __func__);
  struct timeval tdStopCamera;
  struct timezone tz;

  cam_id = my_cam_app.cam_open;

  rc = system_dimension_set(cam_id);
  CDBG("Start Preview");
  if( 0 != (rc = startPreview(cam_id))) {
    CDBG("%s: startPreview() err=%d\n", __func__, rc);
    return 0;
  }

  do {
    print_current_menu (current_menu_id);
    fgets(tc_buf, 3, stdin);

    next_menu_id = next_menu(current_menu_id, tc_buf[0], &action_id, &action_param);

    if (next_menu_id != MENU_ID_INVALID) {
      current_menu_id = next_menu_id;
    }
    if (action_id == ACTION_NO_ACTION) {
      continue;
    }
    if(camframe_status == -1) {
      printf("Preview/Video ERROR condition reported Closing Camera APP\n");
      break;
    }

    switch(action_id) {
      case ACTION_STOP_CAMERA:
        CDBG("ACTION_STOP_CAMERA \n");
        stopPreview(cam_id);
        break;

      case ACTION_PREVIEW_VIDEO_RESOLUTION:
        back_mainflag = 1;
        CDBG("Selection for the preview/video resolution change\n");
        switchRes(cam_id);
        preview_video_resolution (action_param);
        break;
      case ACTION_SET_WHITE_BALANCE:
        CDBG("Selection for the White Balance changes\n");
        set_whitebalance(action_param);
        break;

      case ACTION_SET_EXP_METERING:
        CDBG("Selection for the Exposure Metering changes\n");
        set_exp_metering(action_param);
        break;

      case ACTION_GET_CTRL_VALUE:
        CDBG("Selection for getting control value\n");
        get_ctrl_value(action_param);
        break;

      case ACTION_BRIGHTNESS_INCREASE:
        printf("Increase brightness\n");
        increase_brightness();
        break;

      case ACTION_BRIGHTNESS_DECREASE:
        printf("Decrease brightness\n");
        decrease_brightness();
        break;

      case ACTION_CONTRAST_INCREASE:
        CDBG("Selection for the contrast increase\n");
        increase_contrast ();
        break;

      case ACTION_CONTRAST_DECREASE:
        CDBG("Selection for the contrast decrease\n");
        decrease_contrast ();
        break;

      case ACTION_EV_INCREASE:
        CDBG("Selection for the EV increase\n");
        increase_EV ();
        break;

      case ACTION_EV_DECREASE:
        CDBG("Selection for the EV decrease\n");
        decrease_EV ();
        break;

      case ACTION_SATURATION_INCREASE:
        CDBG("Selection for the EV increase\n");
        increase_saturation ();
        break;

      case ACTION_SATURATION_DECREASE:
        CDBG("Selection for the EV decrease\n");
        decrease_saturation ();
        break;

      case ACTION_TOGGLE_AFR:
        CDBG("Select for auto frame rate toggling\n");
        toggle_afr();
        break;

      case ACTION_SET_ISO:
        CDBG("Select for ISO changes\n");
        set_iso(action_param);
        break;

      case ACTION_SET_ZOOM:
        CDBG("Selection for the zoom direction changes\n");
        set_zoom(action_param);
        break;

      case ACTION_SHARPNESS_INCREASE:
        CDBG("Selection for sharpness increase\n");
        increase_sharpness();
        break;

      case ACTION_SHARPNESS_DECREASE:
        CDBG("Selection for sharpness decrease\n");
        decrease_sharpness();
        break;

      case ACTION_TAKE_YUV_SNAPSHOT:
        CDBG("Take YUV snapshot\n");
        if (takePicture_yuv(cam_id) < 0)
          goto ERROR;
        break;
      case ACTION_TAKE_RAW_SNAPSHOT:
        CDBG("Take YUV snapshot\n");
        if (takePicture_raw(cam_id) < 0)
          goto ERROR;
        break;
      case ACTION_START_RECORDING:
        CDBG("Start recording action\n");
        if (startRecording(cam_id) < 0)
          goto ERROR;
        break;
      case ACTION_STOP_RECORDING:
        CDBG("Stop recording action\n");
        if (stopRecording(cam_id) < 0)
          goto ERROR;
        break;
      case ACTION_NO_ACTION:
        printf("Go back to main menu");
        break;
      case ACTION_SWITCH_CAMERA:
        CDBG("Toggle Camera action\n");
        back_mainflag = 1;
        switchCamera(action_param - 1);
        break;

      case ACTION_TAKE_ZSL_SNAPSHOT:
        CDBG("Take ZSL snapshot\n");
        if (takePicture_zsl(cam_id) < 0)
        {
          CDBG("Error");    
          goto ERROR;
        }
        break;
      case ACTION_START_RDI:
		CDBG("Start RDI Stream\n");
		startRdi(cam_id);
		break;
	  case ACTION_STOP_RDI:
		CDBG("Stop RDI Stream\n");
		stopRdi(cam_id);
		break;
      case ACTION_TAKE_LIVE_SNAPSHOT:
        CDBG("Take Live snapshot\n");
        if (takePicture_live(cam_id) < 0)
        {
          CDBG("Error");    
          goto ERROR;
        }
        break;

      default:
        printf("\n\n!!!!!WRONG INPUT: %d!!!!\n", action_id);
        break;
    }

    usleep(1000 * 1000);
    CDBG("action_id = %d\n", action_id);
    camframe_status = 0;

  } while ((action_id != ACTION_STOP_CAMERA) &&
      (action_id != ACTION_PREVIEW_VIDEO_RESOLUTION) && (action_id !=ACTION_SWITCH_CAMERA));
  action_id = ACTION_NO_ACTION;

  //system_destroy();


  return back_mainflag;

ERROR:
  back_mainflag = 0;
  return back_mainflag;
}


/*===========================================================================
 * FUNCTION     - preview_resolution -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int preview_video_resolution (int preview_video_action_param) {
  char * resolution_name;
  CDBG("Selecting the action for preview/video resolution = %d \n", preview_video_action_param);
  resolution_name = set_preview_video_dimension_tbl(preview_video_action_param,
                      & input_display.user_input_display_width,
                      & input_display.user_input_display_height);

  CDBG("Selected preview/video resolution is %s\n", resolution_name);

  if (resolution_name == NULL) {
    CDBG("main:%d set_preview_dimension failed!\n", __LINE__);
    goto ERROR;
  }

  CDBG("Selected Preview Resolution: display_width = %d, display_height = %d\n",
    input_display.user_input_display_width, input_display.user_input_display_height);

  preview_video_resolution_flag = 1;
  return 0;

ERROR:
  return -1;
}

/*===========================================================================
 * FUNCTION     - set_whitebalance -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int set_whitebalance (int wb_action_param) {

    int rc = 0;
    struct v4l2_control ctrl_awb, ctrl_temperature;

    ctrl_awb.id = V4L2_CID_AUTO_WHITE_BALANCE;
    ctrl_awb.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE;

    switch (wb_action_param) {
    case CAMERA_WB_INCANDESCENT:
        ctrl_awb.value = FALSE;
        ctrl_temperature.value = 2800;
        break;
    case CAMERA_WB_DAYLIGHT:
        ctrl_awb.value = FALSE;
        ctrl_temperature.value = 6500;
	break;
    case CAMERA_WB_FLUORESCENT:
        ctrl_awb.value = FALSE;
	ctrl_temperature.value = 4200;
	break;
    case CAMERA_WB_CLOUDY_DAYLIGHT:
        ctrl_awb.value = FALSE;
	ctrl_temperature.value = 7500;
	break;
    case CAMERA_WB_AUTO:
    default:
        ctrl_awb.value = TRUE;
        break;
    }

DONE:
    return rc;
}


/*===========================================================================
 * FUNCTION     - set_exp_metering -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int set_exp_metering (int exp_metering_action_param) {

	int rc = 0;
	struct v4l2_control ctrl;

  ctrl.id = MSM_V4L2_PID_EXP_METERING;
  ctrl.value = exp_metering_action_param;
 // rc = ioctl(camfd, VIDIOC_S_CTRL, &ctrl);

  return rc;
}

int get_ctrl_value (int ctrl_value_mode_param){

    int rc = 0;
    struct v4l2_control ctrl;

    if (ctrl_value_mode_param == WHITE_BALANCE_STATE) {
        printf("You chose WHITE_BALANCE_STATE\n");
        ctrl.id = V4L2_CID_AUTO_WHITE_BALANCE;
    }
    else if (ctrl_value_mode_param == WHITE_BALANCE_TEMPERATURE) {
        printf("You chose WHITE_BALANCE_TEMPERATURE\n");
        ctrl.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE;
    }
    else if (ctrl_value_mode_param == BRIGHTNESS_CTRL) {
        printf("You chose brightness value\n");
        ctrl.id = V4L2_CID_BRIGHTNESS;
    }
    else if (ctrl_value_mode_param == EV) {
        printf("You chose exposure value\n");
        ctrl.id = V4L2_CID_EXPOSURE;
    }
    else if (ctrl_value_mode_param == CONTRAST_CTRL) {
        printf("You chose contrast value\n");
        ctrl.id = V4L2_CID_CONTRAST;
    }
    else if (ctrl_value_mode_param == SATURATION_CTRL) {
        printf("You chose saturation value\n");
        ctrl.id = V4L2_CID_SATURATION;
    } else if (ctrl_value_mode_param == SHARPNESS_CTRL) {
        printf("You chose sharpness value\n");
        ctrl.id = V4L2_CID_SHARPNESS;
    }

  //  rc = ioctl(camfd, VIDIOC_G_CTRL, &ctrl);

    return rc;
}

/*===========================================================================
 * FUNCTION     - toggle_afr -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int toggle_afr () {
  int rc = 0;
  struct v4l2_control ctrl;

  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_EXPOSURE_AUTO;
//  rc = ioctl(camfd, VIDIOC_G_CTRL, &ctrl);
  if (rc == -1) {
    CDBG("%s: VIDIOC_G_CTRL V4L2_CID_EXPOSURE_AUTO failed: %s\n",
        __func__, strerror(errno));
    return rc;
  }

  /* V4L2_CID_EXPOSURE_AUTO needs to be AUTO or SHUTTER_PRIORITY */
  if (ctrl.value != V4L2_EXPOSURE_AUTO &&
    ctrl.value != V4L2_EXPOSURE_SHUTTER_PRIORITY) {
    CDBG("%s: V4L2_CID_EXPOSURE_AUTO needs to be AUTO/SHUTTER_PRIORITY\n",
        __func__);
    return -1;
  }

  /* Get V4L2_CID_EXPOSURE_AUTO_PRIORITY */
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_EXPOSURE_AUTO_PRIORITY;
 // rc = ioctl(camfd, VIDIOC_G_CTRL, &ctrl);
  if (rc == -1) {
    CDBG("%s: VIDIOC_G_CTRL V4L2_CID_EXPOSURE_AUTO_PRIORITY failed: %s\n",
        __func__, strerror(errno));
    return rc;
  }

  ctrl.value = !ctrl.value;
  printf("V4L2_CID_EXPOSURE_AUTO_PRIORITY changed to %d\n", ctrl.value);
 // rc = ioctl(camfd, VIDIOC_S_CTRL, &ctrl);
  if (rc == -1) {
    CDBG("%s: VIDIOC_S_CTRL V4L2_CID_EXPOSURE_AUTO_PRIORITY failed: %s\n",
      __func__, strerror(errno));
  }
  return rc;
}

int set_zoom (int zoom_action_param) {
    int rc = 0;
    struct v4l2_control ctrl;

    if (zoom_action_param == ZOOM_IN) {
        zoom_level += zoom_queryctrl.step;
        if (zoom_level > zoom_queryctrl.maximum)
            zoom_level = zoom_queryctrl.maximum;
    } else if (zoom_action_param == ZOOM_OUT) {
        zoom_level -= zoom_queryctrl.step;
        if (zoom_level < zoom_queryctrl.minimum)
            zoom_level = zoom_queryctrl.minimum;
    } else {
        CDBG("%s: Invalid zoom_action_param value\n", __func__);
        return -EINVAL;
    }
    ctrl.id = V4L2_CID_ZOOM_ABSOLUTE;
    ctrl.value = zoom_level;
  //  rc = ioctl(camfd, VIDIOC_S_CTRL, &ctrl);

    return rc;
}

/*===========================================================================
 * FUNCTION     - set_iso -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int set_iso (int iso_action_param) {
    int rc = 0;
    struct v4l2_control ctrl;

    ctrl.id = MSM_V4L2_PID_ISO;
    ctrl.value = iso_action_param - 1;
  //  rc = ioctl(camfd, VIDIOC_S_CTRL, &ctrl);

    return rc;
}

/*===========================================================================
 * FUNCTION     - increase_sharpness -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int increase_sharpness () {
    int rc = 0;
    struct v4l2_control ctrl;

    sharpness += sharpness_queryctrl.step;
    if (sharpness > sharpness_queryctrl.maximum)
        sharpness = sharpness_queryctrl.maximum;

    ctrl.id = V4L2_CID_SHARPNESS;
    ctrl.value = sharpness;
 //   rc = ioctl(camfd, VIDIOC_S_CTRL, &ctrl);

    return rc;
}

/*===========================================================================
 * FUNCTION     - decrease_sharpness -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int decrease_sharpness () {
    int rc = 0;
    struct v4l2_control ctrl;

    sharpness -= sharpness_queryctrl.step;
    if (sharpness < sharpness_queryctrl.minimum)
        sharpness = sharpness_queryctrl.minimum;

    ctrl.id = V4L2_CID_SHARPNESS;
    ctrl.value = sharpness;
  //  rc = ioctl(camfd, VIDIOC_S_CTRL, &ctrl);

    return rc;
}

/*===========================================================================
 * FUNCTION     - print_current_menu -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int print_current_menu (menu_id_change_t current_menu_id) {
  if (current_menu_id == MENU_ID_MAIN) {
    print_menu_preview_video ();
  } else if (current_menu_id == MENU_ID_PREVIEWVIDEORESOLUTIONCHANGE) {
    camera_preview_video_resolution_change_tbl ();
  }else if (current_menu_id == MENU_ID_WHITEBALANCECHANGE) {
    camera_preview_video_wb_change_tbl();
  } else if (current_menu_id == MENU_ID_EXPMETERINGCHANGE) {
    camera_preview_video_exp_metering_change_tbl();
  } else if (current_menu_id == MENU_ID_GET_CTRL_VALUE) {
    camera_preview_video_get_ctrl_value_tbl();
  } else if (current_menu_id == MENU_ID_ISOCHANGE) {
    camera_preview_video_iso_change_tbl();
  } else if (current_menu_id == MENU_ID_BRIGHTNESSCHANGE) {
    camera_brightness_change_tbl ();
  } else if (current_menu_id == MENU_ID_CONTRASTCHANGE) {
    camera_contrast_change_tbl ();
  } else if (current_menu_id == MENU_ID_EVCHANGE) {
    camera_EV_change_tbl ();
  } else if (current_menu_id == MENU_ID_SATURATIONCHANGE) {
    camera_saturation_change_tbl ();
  } else if (current_menu_id == MENU_ID_ZOOMCHANGE) {
    camera_preview_video_zoom_change_tbl();
  } else if (current_menu_id == MENU_ID_SHARPNESSCHANGE) {
    camera_preview_video_sharpness_change_tbl();
  }else if (current_menu_id == MENU_ID_SWITCHCAMERA) {
    camera_switch_tbl();
  }else if (current_menu_id == MENU_ID_RECORD) {
    camera_record_tbl();
  }

  return 0;
}

