/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>

#include "mm_qcamera_main_menu.h"
#include "mm_qcamera_app.h"
#include "mm_qcamera_dbg.h"
#include "mm_qcamera_socket.h"

/*===========================================================================
 * Macro
 *===========================================================================*/
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define VIDEO_BUFFER_SIZE       (PREVIEW_WIDTH * PREVIEW_HEIGHT * 3/2)
#define THUMBNAIL_BUFFER_SIZE   (THUMBNAIL_WIDTH * THUMBNAIL_HEIGHT * 3/2)
#define SNAPSHOT_BUFFER_SIZE    (PICTURE_WIDTH * PICTURE_HEIGHT * 3/2)
//TODO:check this Macros with current app.

/*===========================================================================
 * Defines
 *===========================================================================*/
//#define VIDEO_FRAMES_NUM      4
#define THUMBNAIL_FRAMES_NUM  1
#define SNAPSHOT_FRAMES_NUM   1
#define MAX_NUM_FORMAT        32
#define ZOOM_STEP             2
#define ZOOM_MIN_VALUE        0
#define EXPOSURE_COMPENSATION_MAXIMUM_NUMERATOR 12
#define EXPOSURE_COMPENSATION_MINIMUM_NUMERATOR -12
#define EXPOSURE_COMPENSATION_DEFAULT_NUMERATOR 0
#define EXPOSURE_COMPENSATION_DENOMINATOR 6

//TODO: find correct values of Contrast defines.
#define CAMERA_MIN_CONTRAST    0
#define CAMERA_DEF_CONTRAST    5
#define CAMERA_MAX_CONTRAST    10
#define CAMERA_CONTRAST_STEP   1

//TODO: find correct values of Brightness defines.
#define CAMERA_MIN_BRIGHTNESS  0
#define CAMERA_DEF_BRIGHTNESS  3
#define CAMERA_MAX_BRIGHTNESS  6
#define CAMERA_BRIGHTNESS_STEP 1

//TODO: find correct values of Saturation defines.
#define CAMERA_MIN_SATURATION  0
#define CAMERA_DEF_SATURATION  5
#define CAMERA_MAX_SATURATION  10
#define CAMERA_SATURATION_STEP 1

#define CAMERA_MIN_SHARPNESS 0
#define CAMERA_MAX_SHARPNESS 10
#define CAMERA_DEF_SHARPNESS 5
#define CAMERA_SHARPNESS_STEP 1

const CAMERA_MAIN_MENU_TBL_T camera_main_menu_tbl[] = {
  {START_PREVIEW,               "Start preview"},
  {STOP_PREVIEW,               "Stop preview/video"},
  {SET_WHITE_BALANCE,          "Set white balance mode"},
  {SET_TINTLESS_ENABLE,        "Set Tintless Enable"},
  {SET_TINTLESS_DISABLE,       "Set Tintless Disable"},
  {SET_EXP_METERING,           "Set exposure metering mode"},
  {GET_CTRL_VALUE,             "Get control value menu"},
  {TOGGLE_AFR,                 "Toggle auto frame rate. Default fixed frame rate"},
  {SET_ISO,                    "ISO changes."},
  {BRIGHTNESS_GOTO_SUBMENU,    "Brightness changes."},
  {CONTRAST_GOTO_SUBMENU,      "Contrast changes."},
  {EV_GOTO_SUBMENU,            "EV changes."},
  {SATURATION_GOTO_SUBMENU,    "Saturation changes."},
  {SET_ZOOM,                   "Set Digital Zoom."},
  {SET_SHARPNESS,              "Set Sharpness."},
  {TAKE_JPEG_SNAPSHOT,         "Take a snapshot"},
  {START_RECORDING,            "Start RECORDING"},
  {STOP_RECORDING,             "Stop RECORDING"},
  {BEST_SHOT,                  "Set best-shot mode"},
  {LIVE_SHOT,                  "Take a live snapshot"},
  {FLASH_MODES,                "Set Flash modes"},
  {TOGGLE_ZSL,                 "Toggle ZSL On/Off"},
  {TAKE_RAW_SNAPSHOT,          "Take RAW snapshot"},
  {SWITCH_SNAP_RESOLUTION,     "Select Jpeg resolution"},
  {TOGGLE_WNR,                 "Toggle Wavelet Denoise"},
  {EXIT,                       "Exit"}
};

CAMERA_SENSOR_MENU_TLB_T sensor_tbl[] = {
        {"Primary Camera",      0},
        {"Secondary Camera",    0},
        {"Camera Sensor 3",     0},
        {"Camera Sensor 4",     0}
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
  {   WB_AUTO,               "White Balance - Auto"},
  {   WB_INCANDESCENT,       "White Balance - Incandescent"},
  {   WB_FLUORESCENT,        "White Balance - Fluorescent"},
  {   WB_WARM_FLUORESCENT,   "White Balance - Warm Fluorescent"},
  {   WB_DAYLIGHT,           "White Balance - Daylight"},
  {   WB_CLOUDY_DAYLIGHT,    "White Balance - Cloudy Daylight"},
  {   WB_TWILIGHT,           "White Balance - Twilight"},
  {   WB_SHADE,              "White Balance - Shade"},
};

const GET_CTRL_TBL_T get_ctrl_tbl[] = {
  {     WHITE_BALANCE_STATE,            "Get white balance state (auto/off)"},
  {     WHITE_BALANCE_TEMPERATURE,      "Get white balance temperature"},
  {     BRIGHTNESS_CTRL,                "Get brightness value"},
  {     EV,                             "Get exposure value"},
  {     CONTRAST_CTRL,                  "Get contrast value"},
  {     SATURATION_CTRL,                "Get saturation value"},
  {     SHARPNESS_CTRL,                 "Get sharpness value"},
};

const EXP_METERING_TBL_T exp_metering_tbl[] = {
  {   AUTO_EXP_FRAME_AVG,          "Exposure Metering - Frame Average"},
  {   AUTO_EXP_CENTER_WEIGHTED,    "Exposure Metering - Center Weighted"},
  {   AUTO_EXP_SPOT_METERING,      "Exposure Metering - Spot Metering"},
  {   AUTO_EXP_SMART_METERING,     "Exposure Metering - Smart Metering"},
  {   AUTO_EXP_USER_METERING,      "Exposure Metering - User Metering"},
  {   AUTO_EXP_SPOT_METERING_ADV,  "Exposure Metering - Spot Metering Adv"},
  {   AUTO_EXP_CENTER_WEIGHTED_ADV,"Exposure Metering - Center Weighted Adv"},
};

const ISO_TBL_T iso_tbl[] = {
  {   ISO_AUTO,   "ISO: Auto"},
  {   ISO_DEBLUR, "ISO: Deblur"},
  {   ISO_100,    "ISO: 100"},
  {   ISO_200,    "ISO: 200"},
  {   ISO_400,    "ISO: 400"},
  {   ISO_800,    "ISO: 800"},
  {   ISO_1600,   "ISO: 1600"},
};

const ZOOM_TBL_T zoom_tbl[] = {
  {   ZOOM_IN,  "Zoom In one step"},
  {   ZOOM_OUT, "Zoom Out one step"},
};

const BESTSHOT_MODE_TBT_T bestshot_mode_tbl[] = {
  {BESTSHOT_AUTO,           "Bestshot Mode: Auto"},
  {BESTSHOT_ACTION,         "Bestshot Mode: Action"},
  {BESTSHOT_PORTRAIT,       "Bestshot Mode: Portrait"},
  {BESTSHOT_LANDSCAPE,      "Bestshot Mode: Landscape"},
  {BESTSHOT_NIGHT,          "Bestshot Mode: Night"},
  {BESTSHOT_NIGHT_PORTRAIT, "Bestshot Mode: Night Portrait"},
  {BESTSHOT_THEATRE,        "Bestshot Mode: Theatre"},
  {BESTSHOT_BEACH,          "Bestshot Mode: Beach"},
  {BESTSHOT_SNOW,           "Bestshot Mode: Snow"},
  {BESTSHOT_SUNSET,         "Bestshot Mode: Sunset"},
  {BESTSHOT_ANTISHAKE,      "Bestshot Mode: Antishake"},
  {BESTSHOT_FIREWORKS,      "Bestshot Mode: Fireworks"},
  {BESTSHOT_SPORTS,         "Bestshot Mode: Sports"},
  {BESTSHOT_PARTY,          "Bestshot Mode: Party"},
  {BESTSHOT_CANDLELIGHT,    "Bestshot Mode: Candlelight"},
  {BESTSHOT_ASD,            "Bestshot Mode: ASD"},
  {BESTSHOT_BACKLIGHT,      "Bestshot Mode: Backlight"},
  {BESTSHOT_FLOWERS,        "Bestshot Mode: Flowers"},
  {BESTSHOT_AR,             "Bestshot Mode: Augmented Reality"},
  {BESTSHOT_HDR,            "Bestshot Mode: HDR"},
};

const FLASH_MODE_TBL_T flashmodes_tbl[] = {
  {   FLASH_MODE_OFF,   "Flash Mode Off"},
  {   FLASH_MODE_AUTO,  "Flash Mode Auto"},
  {   FLASH_MODE_ON,    "Flash Mode On"},
  {   FLASH_MODE_TORCH, "Flash Mode Torch"},
};

DIMENSION_TBL_T dimension_tbl[] = {
{VGA_WIDTH,      VGA_HEIGHT,      "VGA",   "Size: VGA <640x480>"   , 0},
{MP1_WIDTH,      MP1_HEIGHT,      "1MP",   "Size: 1MP <1280x960>"  , 0},
{MP5_WIDTH,      MP5_HEIGHT,      "5MP",   "Size: 5MP <2592x1944>",  0},
{MP8_WIDTH,      MP8_HEIGHT,      "8MP",   "Size: 8MP <3264x2448>",  0},
{MP12_WIDTH,     MP12_HEIGHT,     "12MP",  "Size: 12MP <4000x3000>", 0},
};

/*===========================================================================
 * Forward declarations
 *===========================================================================*/
//static void system_dimension_set(mm_camera_test_obj_t *test_obj);
/*===========================================================================
 * Static global variables
 *===========================================================================*/
USER_INPUT_DISPLAY_T input_display;
int preview_video_resolution_flag = 0;

//TODO: default values.
#if 1
int brightness = CAMERA_DEF_BRIGHTNESS;
int contrast = CAMERA_DEF_CONTRAST;
int saturation = CAMERA_DEF_SATURATION;
int sharpness = CAMERA_DEF_SHARPNESS;
#else
int brightness = 0;
int contrast = 0;
int saturation = 0;
int sharpness = 0;
#endif
//TODO: find new method to calculate ev.
//int32_t ev_numerator = EXPOSURE_COMPENSATION_DEFAULT_NUMERATOR;

//TODO:
//fps_mode_t fps_mode = FPS_MODE_FIXED;
int zoom_level;
int zoom_max_value;
int cam_id;
int is_rec = 0;


static int submain();

/*===========================================================================
 * FUNCTION    - keypress_to_event -
 *
 * DESCRIPTION:
 *==========================================================================*/
int keypress_to_event(char keypress)
{
  int out_buf = INVALID_KEY_PRESS;
  if ((keypress >= 'A' && keypress <= 'Z') ||
    (keypress >= 'a' && keypress <= 'z')) {
    out_buf = tolower(keypress);
    out_buf = out_buf - 'a';
  } else if (keypress >= '0' && keypress <= '9') {
    out_buf = keypress - '0';
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
  printf("output_to_event=%d\n",output_to_event);
  switch(current_menu_id) {
    case MENU_ID_MAIN:
      switch(output_to_event) {
        case START_PREVIEW:
          * action_id_ptr = ACTION_START_PREVIEW;
          CDBG("START_PREVIEW\n");
          break;
        case STOP_PREVIEW:
          * action_id_ptr = ACTION_STOP_PREVIEW;
          CDBG("STOP_PREVIEW\n");
          break;

        case SET_WHITE_BALANCE:
          next_menu_id = MENU_ID_WHITEBALANCECHANGE;
          CDBG("next_menu_id = MENU_ID_WHITEBALANCECHANGE = %d\n", next_menu_id);
          break;

        case SET_TINTLESS_ENABLE:
          * action_id_ptr = ACTION_SET_TINTLESS_ENABLE;
          next_menu_id = MENU_ID_MAIN;
          CDBG("next_menu_id = MENU_ID_TINTLESSENABLE = %d\n", next_menu_id);
          break;

        case SET_TINTLESS_DISABLE:
          * action_id_ptr = ACTION_SET_TINTLESS_DISABLE;
          next_menu_id = MENU_ID_MAIN;
          CDBG("next_menu_id = MENU_ID_TINTLESSDISABLE = %d\n", next_menu_id);
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

        case BEST_SHOT:
          next_menu_id = MENU_ID_BESTSHOT;
          CDBG("next_menu_id = MENU_ID_BESTSHOT = %d\n", next_menu_id);
          break;

        case LIVE_SHOT:
          * action_id_ptr = ACTION_TAKE_LIVE_SNAPSHOT;
          CDBG("\nTaking Live snapshot\n");
          break;

        case FLASH_MODES:
          next_menu_id = MENU_ID_FLASHMODE;
          CDBG("next_menu_id = MENU_ID_FLASHMODE = %d\n", next_menu_id);
          break;

        case SET_SHARPNESS:
          next_menu_id = MENU_ID_SHARPNESSCHANGE;
          CDBG("next_menu_id = MENU_ID_SHARPNESSCHANGE = %d\n", next_menu_id);
          break;

        case SWITCH_SNAP_RESOLUTION:
          next_menu_id = MENU_ID_SWITCH_RES;
          CDBG("next_menu_id = MENU_ID_SWITCH_RES = %d\n", next_menu_id);
          break;

        case TAKE_JPEG_SNAPSHOT:
          * action_id_ptr = ACTION_TAKE_JPEG_SNAPSHOT;
          printf("\n Taking JPEG snapshot\n");
          break;

        case START_RECORDING:
          * action_id_ptr = ACTION_START_RECORDING;
          CDBG("Start recording\n");
          break;
        case STOP_RECORDING:
          * action_id_ptr = ACTION_STOP_RECORDING;
          CDBG("Stop recording\n");
          break;
        case TOGGLE_ZSL:
          * action_id_ptr = ACTION_TOGGLE_ZSL;
          CDBG("Toggle ZSL\n");
          break;
        case TAKE_RAW_SNAPSHOT:
            * action_id_ptr = ACTION_TAKE_RAW_SNAPSHOT;
            next_menu_id = MENU_ID_MAIN;
            CDBG("Capture RAW\n");
            break;
        case TOGGLE_WNR:
            * action_id_ptr = ACTION_TOGGLE_WNR;
            next_menu_id = MENU_ID_MAIN;
            CDBG("Toggle WNR");
            break;
        case EXIT:
          * action_id_ptr = ACTION_EXIT;
          CDBG("Exit \n");
          break;
        default:
          next_menu_id = MENU_ID_MAIN;
          CDBG("next_menu_id = MENU_ID_MAIN = %d\n", next_menu_id);
          break;
      }
      break;

    case MENU_ID_SWITCH_RES:
        printf("MENU_ID_SWITCH_RES\n");
        *action_id_ptr = ACTION_SWITCH_RESOLUTION;
        *action_param = output_to_event;
        int available_sizes = sizeof(dimension_tbl)/sizeof(dimension_tbl[0]);
        if ( ( *action_param >= 0 ) &&
             ( *action_param < available_sizes ) &&
             ( dimension_tbl[*action_param].supported )) {
            next_menu_id = MENU_ID_MAIN;
        }
        else {
          next_menu_id = current_menu_id;
        }
        break;

    case MENU_ID_SENSORS:
        next_menu_id = MENU_ID_MAIN;
        *action_id_ptr = ACTION_SWITCH_CAMERA;
        *action_param = output_to_event;
        break;

    case MENU_ID_WHITEBALANCECHANGE:
      printf("MENU_ID_WHITEBALANCECHANGE\n");
      if (output_to_event >= WB_MAX) {
        next_menu_id = current_menu_id;
        * action_id_ptr = ACTION_NO_ACTION;
      } else {
        next_menu_id = MENU_ID_MAIN;
        * action_id_ptr = ACTION_SET_WHITE_BALANCE;
        * action_param = output_to_event;
      }
      break;

    case MENU_ID_EXPMETERINGCHANGE:
      printf("MENU_ID_EXPMETERINGCHANGE\n");
      if (output_to_event >= AUTO_EXP_MAX) {
        next_menu_id = current_menu_id;
        * action_id_ptr = ACTION_NO_ACTION;
      } else {
        next_menu_id = MENU_ID_MAIN;
        * action_id_ptr = ACTION_SET_EXP_METERING;
        * action_param = output_to_event;
      }
      break;

    case MENU_ID_GET_CTRL_VALUE:
      printf("MENU_ID_GET_CTRL_VALUE\n");
      * action_id_ptr = ACTION_GET_CTRL_VALUE;
      if (output_to_event > 0 &&
        output_to_event <= (int)(sizeof(get_ctrl_tbl)/sizeof(get_ctrl_tbl[0]))) {
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
      if (output_to_event >= ISO_MAX) {
        next_menu_id = current_menu_id;
        * action_id_ptr = ACTION_NO_ACTION;
      } else {
        next_menu_id = MENU_ID_MAIN;
        * action_id_ptr = ACTION_SET_ISO;
        * action_param = output_to_event;
      }
      break;

    case MENU_ID_ZOOMCHANGE:
      * action_id_ptr = ACTION_SET_ZOOM;
      if (output_to_event > 0 &&
        output_to_event <= (int)(sizeof(zoom_tbl)/sizeof(zoom_tbl[0]))) {
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

    case MENU_ID_BESTSHOT:
      if (output_to_event >= BESTSHOT_MAX) {
        next_menu_id = current_menu_id;
        * action_id_ptr = ACTION_NO_ACTION;
      } else {
        next_menu_id = MENU_ID_MAIN;
        * action_id_ptr = ACTION_SET_BESTSHOT_MODE;
        * action_param = output_to_event;
      }
      break;

    case MENU_ID_FLASHMODE:
      if (output_to_event >= FLASH_MODE_MAX) {
        next_menu_id = current_menu_id;
        * action_id_ptr = ACTION_NO_ACTION;
      } else {
        next_menu_id = MENU_ID_MAIN;
        * action_id_ptr = ACTION_SET_FLASH_MODE;
        * action_param = output_to_event;
      }
      break;

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
  if (!is_rec) {
    printf("\n");
    printf("===========================================\n");
    printf("      Camera is in preview/video mode now        \n");
    printf("===========================================\n\n");
  } else {
    printf("\n");
    printf("===========================================\n");
    printf("      Camera is in RECORDING mode now       \n");
    printf("        Press 'Q' To Stop Recording          \n");
    printf("        Press 'S' To Take Live Snapshot       \n");
    printf("===========================================\n\n");
  }
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

static void camera_preview_video_wb_change_tbl(void) {
  unsigned int i;
  printf("\n");
  printf("==========================================================\n");
  printf("      Camera is in white balance change mode       \n");
  printf("==========================================================\n\n");

  char submenuNum = 'A';
  for (i = 0 ; i < sizeof(white_balance_tbl) /
                   sizeof(white_balance_tbl[0]); i++) {
        printf("%c.  %s\n", submenuNum, white_balance_tbl[i].wb_name);
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

static void camera_resolution_change_tbl(void) {
    unsigned int i;

    printf("\n");
    printf("==========================================================\n");
    printf("      Camera is in snapshot resolution mode               \n");
    printf("==========================================================\n\n");

    for (i = 0; i < sizeof(dimension_tbl) /
      sizeof(dimension_tbl[0]); i++) {
        if ( dimension_tbl[i].supported ) {
            printf("%d.  %s\n", i,
                    dimension_tbl[i].str_name);
        }
    }

    printf("\nPlease enter your choice for Resolution: ");
    return;
}

static void camera_preview_video_zoom_change_tbl(void) {
    unsigned int i;
    zoom_max_value = MAX_ZOOMS_CNT;
    printf("\nCurrent Zoom Value = %d ,Max Zoom Value = %d\n",zoom_level,zoom_max_value);
    char submenuNum = 'A';
    for (i = 0 ; i < sizeof(zoom_tbl) /
                   sizeof(zoom_tbl[0]); i++) {
        printf("%c.  %s\n", submenuNum, zoom_tbl[i].zoom_direction_name);
        submenuNum++;
    }
    printf("\nPlease enter your choice for zoom change direction: ");
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

static void camera_set_bestshot_tbl(void)
{
  unsigned int i;

  printf("\n");
  printf("===========================================\n");
  printf("      Camera is in set besthot mode now       \n");
  printf("===========================================\n\n");


  char bsmenuNum = 'A';
  for (i = 0; i < sizeof(bestshot_mode_tbl)/sizeof(bestshot_mode_tbl[0]); i++) {
    printf("%c.  %s\n", bsmenuNum,
      bestshot_mode_tbl[i].name);
    bsmenuNum++;
  }

  printf("\nPlease enter your choice of Bestshot Mode: ");
  return;
}

static void camera_set_flashmode_tbl(void)
{
  unsigned int i;

  printf("\n");
  printf("===========================================\n");
  printf("      Camera is in set flash mode now       \n");
  printf("===========================================\n\n");


  char bsmenuNum = 'A';
  for (i = 0; i < sizeof(flashmodes_tbl)/sizeof(flashmodes_tbl[0]); i++) {
    printf("%c.  %s\n", bsmenuNum,
      flashmodes_tbl[i].name);
    bsmenuNum++;
  }

  printf("\nPlease enter your choice of Bestshot Mode: ");
  return;
}

static void camera_sensors_tbl(void)
{
  unsigned int i;
  size_t available_sensors = sizeof(sensor_tbl)/sizeof(sensor_tbl[0]);

  printf("\n");
  printf("===========================================\n");
  printf("      Camera Sensor to be used:            \n");
  printf("===========================================\n\n");


  char bsmenuNum = 'A';
  for (i = 0; ( i < available_sensors ) && ( sensor_tbl[i].present ) ; i++) {
    printf("%c.  %s\n", bsmenuNum,
            sensor_tbl[i].menu_name);
    bsmenuNum++;
  }

  printf("\nPlease enter your choice for sensor: ");
  return;
}

/*===========================================================================
 * FUNCTION     - increase_contrast -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int increase_contrast (mm_camera_lib_handle *lib_handle) {
        contrast += CAMERA_CONTRAST_STEP;
        if (contrast > CAMERA_MAX_CONTRAST) {
                contrast = CAMERA_MAX_CONTRAST;
                printf("Reached max CONTRAST. \n");
        }
        printf("Increase Contrast to %d\n", contrast);
        return mm_camera_lib_send_command(lib_handle,
                                          MM_CAMERA_LIB_CONTRAST,
                                          &contrast,
                                          NULL);
}

/*===========================================================================
 * FUNCTION     - decrease_contrast -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int decrease_contrast (mm_camera_lib_handle *lib_handle) {
        contrast -= CAMERA_CONTRAST_STEP;
        if (contrast < CAMERA_MIN_CONTRAST) {
                contrast = CAMERA_MIN_CONTRAST;
                printf("Reached min CONTRAST. \n");
        }
        printf("Decrease Contrast to %d\n", contrast);
        return mm_camera_lib_send_command(lib_handle,
                                          MM_CAMERA_LIB_CONTRAST,
                                          &contrast,
                                          NULL);
}

/*===========================================================================
 * FUNCTION     - decrease_brightness -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int decrease_brightness (mm_camera_lib_handle *lib_handle) {
        brightness -= CAMERA_BRIGHTNESS_STEP;
        if (brightness < CAMERA_MIN_BRIGHTNESS) {
                brightness = CAMERA_MIN_BRIGHTNESS;
                printf("Reached min BRIGHTNESS. \n");
        }
        printf("Decrease Brightness to %d\n", brightness);
        return mm_camera_lib_send_command(lib_handle,
                                          MM_CAMERA_LIB_BRIGHTNESS,
                                          &brightness,
                                          NULL);
}

/*===========================================================================
 * FUNCTION     - increase_brightness -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int increase_brightness (mm_camera_lib_handle *lib_handle) {
        brightness += CAMERA_BRIGHTNESS_STEP;
        if (brightness > CAMERA_MAX_BRIGHTNESS) {
                brightness = CAMERA_MAX_BRIGHTNESS;
                printf("Reached max BRIGHTNESS. \n");
        }
        printf("Increase Brightness to %d\n", brightness);
        return mm_camera_lib_send_command(lib_handle,
                                          MM_CAMERA_LIB_BRIGHTNESS,
                                          &brightness,
                                          NULL);
}

/*===========================================================================
 * FUNCTION     - increase_EV -
 *
 * DESCRIPTION:
 * ===========================================================================*/

int increase_EV (void) {
#if 0
   int rc = 0;
   int32_t value = 0;
   rc = cam_config_is_parm_supported(cam_id, MM_CAMERA_PARM_EXPOSURE_COMPENSATION);
    if(!rc) {
       printf("MM_CAMERA_PARM_EXPOSURE_COMPENSATION mode is not supported for this sensor");
       return -1;
    }
    ev_numerator += 1;
    if(ev_numerator >= EXPOSURE_COMPENSATION_MINIMUM_NUMERATOR &&
            ev_numerator <= EXPOSURE_COMPENSATION_MAXIMUM_NUMERATOR){
        int16_t  numerator16 = (int16_t)(ev_numerator & 0x0000ffff);
        uint16_t denominator16 = EXPOSURE_COMPENSATION_DENOMINATOR;
        value = numerator16 << 16 | denominator16;
    } else {
       printf("Reached max EV.\n");
    }
    return mm_app_set_config_parm(cam_id, MM_CAMERA_PARM_EXPOSURE_COMPENSATION, value);
#endif
  return 0;
}

/*===========================================================================
 * FUNCTION     - decrease_EV -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int decrease_EV (void) {
#if 0
   int rc = 0;
   int32_t  value = 0;
   rc = cam_config_is_parm_supported(cam_id, MM_CAMERA_PARM_EXPOSURE_COMPENSATION);
    if(!rc) {
       printf("MM_CAMERA_PARM_EXPOSURE_COMPENSATION mode is not supported for this sensor");
       return -1;
    }
    ev_numerator -= 1;
    if(ev_numerator >= EXPOSURE_COMPENSATION_MINIMUM_NUMERATOR &&
            ev_numerator <= EXPOSURE_COMPENSATION_MAXIMUM_NUMERATOR){
        int16_t  numerator16 = (int16_t)(ev_numerator & 0x0000ffff);
        uint16_t denominator16 = EXPOSURE_COMPENSATION_DENOMINATOR;
        value = numerator16 << 16 | denominator16;
    } else {
       printf("Reached min EV.\n");
    }
    return mm_app_set_config_parm(cam_id, MM_CAMERA_PARM_EXPOSURE_COMPENSATION, value);
#endif
  return 0;
}

/*===========================================================================
 * FUNCTION     - increase_saturation -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int increase_saturation (void) {
#if 0
  saturation += CAMERA_SATURATION_STEP;
  if (saturation > CAMERA_MAX_SATURATION) {
    saturation = CAMERA_MAX_SATURATION;
    printf("Reached max saturation. \n");
  }
  printf("Increase Saturation to %d\n", saturation);
  return mm_app_set_config_parm(cam_id, MM_CAMERA_PARM_SATURATION, saturation);
#endif
  return 0;
}

/*===========================================================================
 * FUNCTION     - decrease_saturation -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int decrease_saturation (void) {
#if 0
  saturation -= CAMERA_SATURATION_STEP;
  if (saturation < CAMERA_MIN_SATURATION) {
    saturation = CAMERA_MIN_SATURATION;
    printf("Reached min saturation. \n");
  }
  printf("Dcrease Saturation to %d\n", saturation);
  return mm_app_set_config_parm(cam_id, MM_CAMERA_PARM_SATURATION, saturation);
#endif
  return 0;
}


int take_jpeg_snapshot(mm_camera_test_obj_t *test_obj, int is_burst_mode)
{
  CDBG_HIGH("\nEnter take_jpeg_snapshot!!\n");
  int rc = mm_app_take_picture (test_obj, (uint8_t)is_burst_mode);
  if (MM_CAMERA_OK != rc) {
    CDBG_ERROR("%s: mm_app_take_picture() err=%d\n", __func__, rc);
  }
  return rc;
}

/*===========================================================================
 * FUNCTION    - main -
 *
 * DESCRIPTION:
 *==========================================================================*/
int main()
{
    char tc_buf[3];
    int mode = 0;
    int rc = 0;

    printf("Please Select Execution Mode:\n");
    printf("0: Menu Based 1: Regression\n");
    fgets(tc_buf, 3, stdin);
    mode = tc_buf[0] - '0';
    if(mode == 0) {
      printf("\nStarting Menu based!!\n");
    } else if(mode == 1) {
      printf("Starting Regression testing!!\n");
      if(!mm_app_start_regression_test(1)) {
         printf("\nRegressiion test passed!!\n");
         return 0;
      } else {
        printf("\nRegression test failed!!\n");
        exit(-1);
      }
    } else {
       printf("\nPlease Enter 0 or 1\n");
       printf("\nExisting the App!!\n");
       exit(-1);
    }


    rc = submain();

    printf("Exiting application\n");

    return rc;
}

/*===========================================================================
 * FUNCTION     - set_whitebalance -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int set_whitebalance (mm_camera_lib_handle *lib_handle, int wb_action_param) {
        cam_wb_mode_type type = 0;
        switch (wb_action_param) {
                case WB_AUTO:
                        printf("\n WB_AUTO\n");
                        type = CAM_WB_MODE_AUTO;
                        break;
                case WB_INCANDESCENT:
                        printf("\n WB_INCANDESCENT\n");
                        type = CAM_WB_MODE_INCANDESCENT;
                        break;
                case WB_FLUORESCENT:
                        printf("\n WB_FLUORESCENT\n");
                        type = CAM_WB_MODE_FLUORESCENT;
                        break;
                case WB_WARM_FLUORESCENT:
                        printf("\n WB_WARM_FLUORESCENT\n");
                        type = CAM_WB_MODE_WARM_FLUORESCENT;
                        break;
                case WB_DAYLIGHT:
                        printf("\n WB_DAYLIGHT\n");
                        type = CAM_WB_MODE_DAYLIGHT;
                        break;
                case WB_CLOUDY_DAYLIGHT:
                        printf("\n WB_CLOUDY_DAYLIGHT\n");
                        type = CAM_WB_MODE_CLOUDY_DAYLIGHT;
                        break;
               case WB_TWILIGHT:
                        printf("\n WB_TWILIGHT\n");
                        type = CAM_WB_MODE_TWILIGHT;
                        break;
               case WB_SHADE:
                        printf("\n WB_SHADE\n");
                        type = CAM_WB_MODE_SHADE;
                        break;
                default:
                        break;
        }
        return mm_camera_lib_send_command(lib_handle,
                                          MM_CAMERA_LIB_WB,
                                          &type,
                                          NULL);
}


/*===========================================================================
 * FUNCTION     - set_exp_metering -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int set_exp_metering (mm_camera_lib_handle *lib_handle, int exp_metering_action_param) {
        cam_auto_exposure_mode_type type = 0;
        switch (exp_metering_action_param) {
		case AUTO_EXP_FRAME_AVG:
                        printf("\nAUTO_EXP_FRAME_AVG\n");
                        type = CAM_AEC_MODE_FRAME_AVERAGE;
                        break;
                case AUTO_EXP_CENTER_WEIGHTED:
                        printf("\n AUTO_EXP_CENTER_WEIGHTED\n");
                        type = CAM_AEC_MODE_CENTER_WEIGHTED;
                        break;
                case AUTO_EXP_SPOT_METERING:
                        printf("\n AUTO_EXP_SPOT_METERING\n");
                        type = CAM_AEC_MODE_SPOT_METERING;
                        break;
                case AUTO_EXP_SMART_METERING:
                        printf("\n AUTO_EXP_SMART_METERING\n");
                        type = CAM_AEC_MODE_SMART_METERING;
                        break;
                case AUTO_EXP_USER_METERING:
                        printf("\n AUTO_EXP_USER_METERING\n");
                        type = CAM_AEC_MODE_USER_METERING;
                        break;
                case AUTO_EXP_SPOT_METERING_ADV:
                        printf("\n AUTO_EXP_SPOT_METERING_ADV\n");
                        type = CAM_AEC_MODE_SPOT_METERING_ADV;
                        break;
                case AUTO_EXP_CENTER_WEIGHTED_ADV:
                        printf("\n AUTO_EXP_CENTER_WEIGHTED_ADV\n");
                        type = CAM_AEC_MODE_CENTER_WEIGHTED_ADV;
                        break;
                default:
                        break;
        }
        return mm_camera_lib_send_command(lib_handle,
                                          MM_CAMERA_LIB_EXPOSURE_METERING,
                                          &type,
                                          NULL);
}

int get_ctrl_value (int ctrl_value_mode_param){
#if 0
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
#endif
  return ctrl_value_mode_param;
}

/*===========================================================================
 * FUNCTION     - toggle_afr -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int toggle_afr () {
#if 0
    if (fps_mode == FPS_MODE_AUTO) {
        printf("\nSetting FPS_MODE_FIXED\n");
        fps_mode = FPS_MODE_FIXED;
    } else {
        printf("\nSetting FPS_MODE_AUTO\n");
        fps_mode = FPS_MODE_AUTO;
    }
    return mm_app_set_config_parm(cam_id, MM_CAMERA_PARM_FPS_MODE, fps_mode);
#endif
  return 0;
}

int set_zoom (mm_camera_lib_handle *lib_handle, int zoom_action_param) {

    if (zoom_action_param == ZOOM_IN) {
        zoom_level += ZOOM_STEP;
        if (zoom_level > zoom_max_value)
            zoom_level = zoom_max_value;
    } else if (zoom_action_param == ZOOM_OUT) {
        zoom_level -= ZOOM_STEP;
        if (zoom_level < ZOOM_MIN_VALUE)
            zoom_level = ZOOM_MIN_VALUE;
    } else {
        CDBG("%s: Invalid zoom_action_param value\n", __func__);
        return -EINVAL;
    }
    return mm_camera_lib_send_command(lib_handle,
                                      MM_CAMERA_LIB_ZOOM,
                                      &zoom_level,
                                      NULL);
}

/*===========================================================================
 * FUNCTION     - set_iso -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int set_iso (mm_camera_lib_handle *lib_handle, int iso_action_param) {
    cam_iso_mode_type type = 0;
    switch (iso_action_param) {
        case ISO_AUTO:
            printf("\n ISO_AUTO\n");
            type = CAM_ISO_MODE_AUTO;
            break;
        case ISO_DEBLUR:
            printf("\n ISO_DEBLUR\n");
            type = CAM_ISO_MODE_DEBLUR;
            break;
        case ISO_100:
            printf("\n ISO_100\n");
            type = CAM_ISO_MODE_100;
            break;
        case ISO_200:
            printf("\n ISO_200\n");
            type = CAM_ISO_MODE_200;
            break;
        case ISO_400:
            printf("\n ISO_400\n");
            type = CAM_ISO_MODE_400;
            break;
        case ISO_800:
            printf("\n ISO_800\n");
            type = CAM_ISO_MODE_800;
            break;
        case ISO_1600:
            printf("\n ISO_1600\n");
            type = CAM_ISO_MODE_1600;
            break;
        default:
            break;
    }
    return mm_camera_lib_send_command(lib_handle,
                                      MM_CAMERA_LIB_ISO,
                                      &type,
                                      NULL);
}

/*===========================================================================
 * FUNCTION     - increase_sharpness -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int increase_sharpness (mm_camera_lib_handle *lib_handle) {
    sharpness += CAMERA_SHARPNESS_STEP;
    if (sharpness > CAMERA_MAX_SHARPNESS) {
        sharpness = CAMERA_MAX_SHARPNESS;
        printf("Reached max SHARPNESS. \n");
    }
    printf("Increase Sharpness to %d\n", sharpness);
    return mm_camera_lib_send_command(lib_handle,
                                      MM_CAMERA_LIB_SHARPNESS,
                                      &sharpness,
                                      NULL);
}

/*===========================================================================
 * FUNCTION     - decrease_sharpness -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int decrease_sharpness (mm_camera_lib_handle *lib_handle) {
    sharpness -= CAMERA_SHARPNESS_STEP;
    if (sharpness < CAMERA_MIN_SHARPNESS) {
        sharpness = CAMERA_MIN_SHARPNESS;
        printf("Reached min SHARPNESS. \n");
    }
    printf("Decrease Sharpness to %d\n", sharpness);
    return mm_camera_lib_send_command(lib_handle,
                                      MM_CAMERA_LIB_SHARPNESS,
                                      &sharpness,
                                      NULL);
}

int set_flash_mode (mm_camera_lib_handle *lib_handle, int action_param) {
    cam_flash_mode_t type = 0;
    switch (action_param) {
        case FLASH_MODE_OFF:
            printf("\n FLASH_MODE_OFF\n");
            type = CAM_FLASH_MODE_OFF;
            break;
        case FLASH_MODE_AUTO:
            printf("\n FLASH_MODE_AUTO\n");
            type = CAM_FLASH_MODE_AUTO;
            break;
        case FLASH_MODE_ON:
            printf("\n FLASH_MODE_ON\n");
            type = CAM_FLASH_MODE_ON;
            break;
        case FLASH_MODE_TORCH:
            printf("\n FLASH_MODE_TORCH\n");
            type = CAM_ISO_MODE_100;
            break;
        default:
            break;
    }
    return mm_camera_lib_send_command(lib_handle,
                                      MM_CAMERA_LIB_FLASH,
                                      &type,
                                      NULL);
}

int set_bestshot_mode(mm_camera_lib_handle *lib_handle, int action_param) {
    cam_scene_mode_type type = 0;
    switch (action_param) {
       case BESTSHOT_AUTO:
            printf("\n BEST SHOT AUTO\n");
            type = CAM_SCENE_MODE_OFF;
            break;
        case BESTSHOT_ACTION:
            printf("\n BEST SHOT ACTION\n");
            type = CAM_SCENE_MODE_ACTION;
            break;
        case BESTSHOT_PORTRAIT:
            printf("\n BEST SHOT PORTRAIT\n");
            type = CAM_SCENE_MODE_PORTRAIT;
            break;
        case BESTSHOT_LANDSCAPE:
            printf("\n BEST SHOT LANDSCAPE\n");
            type = CAM_SCENE_MODE_LANDSCAPE;
            break;
        case BESTSHOT_NIGHT:
            printf("\n BEST SHOT NIGHT\n");
            type = CAM_SCENE_MODE_NIGHT;
            break;
        case BESTSHOT_NIGHT_PORTRAIT:
            printf("\n BEST SHOT NIGHT PORTRAIT\n");
            type = CAM_SCENE_MODE_NIGHT_PORTRAIT;
            break;
        case BESTSHOT_THEATRE:
            printf("\n BEST SHOT THREATRE\n");
            type = CAM_SCENE_MODE_THEATRE;
            break;
        case BESTSHOT_BEACH:
            printf("\n BEST SHOT BEACH\n");
            type = CAM_SCENE_MODE_BEACH;
            break;
        case BESTSHOT_SNOW:
            printf("\n BEST SHOT SNOW\n");
            type = CAM_SCENE_MODE_SNOW;
            break;
        case BESTSHOT_SUNSET:
            printf("\n BEST SHOT SUNSET\n");
            type = CAM_SCENE_MODE_SUNSET;
            break;
        case BESTSHOT_ANTISHAKE:
            printf("\n BEST SHOT ANTISHAKE\n");
            type = CAM_SCENE_MODE_ANTISHAKE;
            break;
        case BESTSHOT_FIREWORKS:
            printf("\n BEST SHOT FIREWORKS\n");
            type = CAM_SCENE_MODE_FIREWORKS;
            break;
        case BESTSHOT_SPORTS:
            printf("\n BEST SHOT SPORTS\n");
            type = CAM_SCENE_MODE_SPORTS;
            break;
        case BESTSHOT_PARTY:
            printf("\n BEST SHOT PARTY\n");
            type = CAM_SCENE_MODE_PARTY;
            break;
        case BESTSHOT_CANDLELIGHT:
            printf("\n BEST SHOT CANDLELIGHT\n");
            type = CAM_SCENE_MODE_CANDLELIGHT;
            break;
        case BESTSHOT_ASD:
            printf("\n BEST SHOT ASD\n");
            type = CAM_SCENE_MODE_AUTO;
            break;
        case BESTSHOT_BACKLIGHT:
            printf("\n BEST SHOT BACKLIGHT\n");
            type = CAM_SCENE_MODE_BACKLIGHT;
            break;
        case BESTSHOT_FLOWERS:
            printf("\n BEST SHOT FLOWERS\n");
            type = CAM_SCENE_MODE_FLOWERS;
            break;
        case BESTSHOT_AR:
            printf("\n BEST SHOT AR\n");
            type = CAM_SCENE_MODE_AR;
            break;
        case BESTSHOT_HDR:
            printf("\n BEST SHOT HDR\n");
            type = CAM_SCENE_MODE_OFF;
            break;
        default:
            break;
        }
        return mm_camera_lib_send_command(lib_handle,
                                          MM_CAMERA_LIB_BESTSHOT,
                                          &type,
                                          NULL);
}
/*===========================================================================
 * FUNCTION     - print_current_menu -
 *
 * DESCRIPTION:
 * ===========================================================================*/
int print_current_menu (menu_id_change_t current_menu_id) {
  if (current_menu_id == MENU_ID_MAIN) {
    print_menu_preview_video ();
  } else if (current_menu_id == MENU_ID_WHITEBALANCECHANGE) {
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
  } else if (current_menu_id == MENU_ID_BESTSHOT) {
    camera_set_bestshot_tbl();
  } else if (current_menu_id == MENU_ID_FLASHMODE) {
    camera_set_flashmode_tbl();
  } else if (current_menu_id == MENU_ID_SENSORS ) {
    camera_sensors_tbl();
  } else if (current_menu_id == MENU_ID_SWITCH_RES ) {
    camera_resolution_change_tbl();
  }

  return 0;
}

int filter_resolutions(mm_camera_lib_handle *lib_handle,
                       DIMENSION_TBL_T *tbl,
                       size_t tbl_size)
{
    size_t i, j;
    cam_capability_t camera_cap;
    int rc = 0;

    if ( ( NULL == lib_handle ) || ( NULL == tbl ) ) {
        return -1;
    }

    rc = mm_camera_lib_get_caps(lib_handle, &camera_cap);
    if ( MM_CAMERA_OK != rc ) {
        CDBG_ERROR("%s:mm_camera_lib_get_caps() err=%d\n", __func__, rc);
        return -1;
    }

    for( i = 0 ; i < tbl_size ; i++ ) {
        for( j = 0; j < camera_cap.picture_sizes_tbl_cnt; j++ ) {
            if ( ( tbl[i].width == camera_cap.picture_sizes_tbl[j].width ) &&
                 ( tbl[i].height == camera_cap.picture_sizes_tbl[j].height ) ) {
                tbl[i].supported = 1;
                rc = (int)i;
                break;
            }
        }
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : enableAFR
 *
 * DESCRIPTION: This function will go through the list
 *              of supported FPS ranges and select the
 *              one which has maximum range
 *
 * PARAMETERS :
 *   @lib_handle   : camera test library handle
 *
 * RETURN     : uint32_t type of stream handle
 *              MM_CAMERA_OK  -- Success
 *              !=MM_CAMERA_OK -- Error status
 *==========================================================================*/
int enableAFR(mm_camera_lib_handle *lib_handle)
{
    size_t i, j;
    float max_range = 0.0f;
    cam_capability_t cap;
    int rc = MM_CAMERA_OK;

    if ( NULL == lib_handle ) {
        return MM_CAMERA_E_INVALID_INPUT;
    }

    rc = mm_camera_lib_get_caps(lib_handle, &cap);
    if ( MM_CAMERA_OK != rc ) {
        CDBG_ERROR("%s:mm_camera_lib_get_caps() err=%d\n", __func__, rc);
        return rc;
    }

    for( i = 0, j = 0 ; i < cap.fps_ranges_tbl_cnt ; i++ ) {
        if ( max_range < (cap.fps_ranges_tbl[i].max_fps - cap.fps_ranges_tbl[i].min_fps) ) {
            j = i;
        }
    }

    rc = mm_camera_lib_send_command(lib_handle,
                                    MM_CAMERA_LIB_FPS_RANGE,
                                    &cap.fps_ranges_tbl[j],
                                    NULL);

    CDBG_ERROR("%s : FPS range [%5.2f:%5.2f] rc = %d",
              __func__,
              cap.fps_ranges_tbl[j].min_fps,
              cap.fps_ranges_tbl[j].max_fps,
              rc);

    return rc;
}

/*===========================================================================
 * FUNCTION     - submain -
 *
 * DESCRIPTION:
 * ===========================================================================*/
static int submain()
{
    int rc = 0;
    char tc_buf[3];
    menu_id_change_t current_menu_id = MENU_ID_MAIN, next_menu_id;
    camera_action_t action_id;
    int action_param;
    uint8_t previewing = 0;
    int isZSL = 0;
    uint8_t wnr_enabled = 0;
    mm_camera_lib_handle lib_handle;
    int num_cameras;
    int available_sensors =
        (int)(sizeof(sensor_tbl) / sizeof(sensor_tbl[0]));
    int available_snap_sizes =
        (int)(sizeof(dimension_tbl)/sizeof(dimension_tbl[0]));
    int i,c;
    mm_camera_lib_snapshot_params snap_dim;
    snap_dim.width = DEFAULT_SNAPSHOT_WIDTH;
    snap_dim.height = DEFAULT_SNAPSHOT_HEIGHT;
    cam_scene_mode_type default_scene= CAM_SCENE_MODE_OFF;
    int set_tintless= 0;

    mm_camera_test_obj_t test_obj;
    memset(&test_obj, 0, sizeof(mm_camera_test_obj_t));

    rc = mm_camera_lib_open(&lib_handle, 0);
    if (rc != MM_CAMERA_OK) {
        CDBG_ERROR("%s:mm_camera_lib_open() err=%d\n", __func__, rc);
        return -1;
    }

    num_cameras = mm_camera_lib_number_of_cameras(&lib_handle);
    if ( 0 >= num_cameras ) {
        CDBG_ERROR("%s: No camera sensors reported!", __func__);
        rc = -1;
        goto ERROR;
    } else if ( 1 <= num_cameras ) {
        c = MIN(num_cameras, available_sensors);
        for ( i = 0 ; i < c ; i++ ) {
            sensor_tbl[i].present = 1;
        }
        current_menu_id = MENU_ID_SENSORS;
    } else {
        i = filter_resolutions(&lib_handle,
                                dimension_tbl,
                                (size_t)available_snap_sizes);
        if ( ( i < 0 ) || ( i >= available_snap_sizes ) ) {
            CDBG_ERROR("%s:filter_resolutions()\n", __func__);
            goto ERROR;
        }
        snap_dim.width = dimension_tbl[i].width;
        snap_dim.height = dimension_tbl[i].height;

        rc = enableAFR(&lib_handle);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:enableAFR() err=%d\n", __func__, rc);
            goto ERROR;
        }

        rc =  mm_camera_lib_send_command(&lib_handle,
                                         MM_CAMERA_LIB_BESTSHOT,
                                         &default_scene,
                                         NULL);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_camera_lib_send_command() err=%d\n", __func__, rc);
            goto ERROR;
        }
    }
    /*start the eztune server*/
    CDBG_HIGH("Starting eztune Server \n");
    eztune_server_start(&lib_handle);

    do {
        print_current_menu (current_menu_id);
        fgets(tc_buf, 3, stdin);

        next_menu_id = next_menu(current_menu_id, tc_buf[0], & action_id, & action_param);

        if (next_menu_id != MENU_ID_INVALID) {
          current_menu_id = next_menu_id;
        }
        if (action_id == ACTION_NO_ACTION) {
          continue;
        }

        switch(action_id) {
            case ACTION_START_PREVIEW:
                CDBG_ERROR("ACTION_START_PREVIEW \n");
                rc = mm_camera_lib_start_stream(&lib_handle);
                if (rc != MM_CAMERA_OK) {
                    CDBG_ERROR("%s:mm_camera_lib_start_stream() err=%d\n", __func__, rc);
                    goto ERROR;
                }
                previewing = 1;
                break;

            case ACTION_STOP_PREVIEW:
                CDBG("ACTION_STOP_PREVIEW \n");
                rc = mm_camera_lib_stop_stream(&lib_handle);
                if (rc != MM_CAMERA_OK) {
                    CDBG_ERROR("%s:mm_camera_lib_stop_stream() err=%d\n", __func__, rc);
                    goto ERROR;
                }
                previewing = 0;
                break;

            case ACTION_SET_WHITE_BALANCE:
                CDBG("Selection for the White Balance changes\n");
                set_whitebalance(&lib_handle, action_param);
                break;

            case ACTION_SET_TINTLESS_ENABLE:
                CDBG("Selection for the Tintless enable changes\n");
                set_tintless = 1;
                rc =  mm_camera_lib_send_command(&lib_handle,
                                                 MM_CAMERA_LIB_SET_TINTLESS,
                                                 &set_tintless,
                                                 NULL);
                if (rc != MM_CAMERA_OK) {
                    CDBG_ERROR("%s:mm_camera_lib_send_command() err=%d\n", __func__, rc);
                    goto ERROR;
                }
                break;

            case ACTION_SET_TINTLESS_DISABLE:
                CDBG("Selection for the Tintless disable changes\n");
                set_tintless = 0;
                rc =  mm_camera_lib_send_command(&lib_handle,
                                                 MM_CAMERA_LIB_SET_TINTLESS,
                                                 &set_tintless,
                                                 NULL);
                if (rc != MM_CAMERA_OK) {
                    CDBG_ERROR("%s:mm_camera_lib_send_command() err=%d\n", __func__, rc);
                    goto ERROR;
                }
                break;

            case ACTION_SET_EXP_METERING:
                CDBG("Selection for the Exposure Metering changes\n");
                set_exp_metering(&lib_handle, action_param);
                break;

            case ACTION_GET_CTRL_VALUE:
                CDBG("Selection for getting control value\n");
                get_ctrl_value(action_param);
                break;

            case ACTION_BRIGHTNESS_INCREASE:
                printf("Increase brightness\n");
                increase_brightness(&lib_handle);
                break;

            case ACTION_BRIGHTNESS_DECREASE:
                printf("Decrease brightness\n");
                decrease_brightness(&lib_handle);
                break;

            case ACTION_CONTRAST_INCREASE:
                CDBG("Selection for the contrast increase\n");
                increase_contrast (&lib_handle);
                break;

            case ACTION_CONTRAST_DECREASE:
                CDBG("Selection for the contrast decrease\n");
                decrease_contrast (&lib_handle);
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
                set_iso(&lib_handle, action_param);
                break;

            case ACTION_SET_ZOOM:
                CDBG("Selection for the zoom direction changes\n");
                set_zoom(&lib_handle, action_param);
                break;

            case ACTION_SHARPNESS_INCREASE:
                CDBG("Selection for sharpness increase\n");
                increase_sharpness(&lib_handle);
                break;

            case ACTION_SHARPNESS_DECREASE:
                CDBG("Selection for sharpness decrease\n");
                decrease_sharpness(&lib_handle);
                break;

            case ACTION_SET_BESTSHOT_MODE:
                CDBG("Selection for bestshot\n");
                set_bestshot_mode(&lib_handle, action_param);
                break;

            case ACTION_SET_FLASH_MODE:
                printf("\n Selection for flashmode\n");
                set_flash_mode(&lib_handle, action_param);
                break;

            case ACTION_SWITCH_CAMERA:
                rc = mm_camera_lib_close(&lib_handle);
                if (rc != MM_CAMERA_OK) {
                    CDBG_ERROR("%s:mm_camera_lib_close() err=%d\n", __func__, rc);
                    goto ERROR;
                }

                rc = mm_camera_lib_open(&lib_handle, action_param);
                if (rc != MM_CAMERA_OK) {
                    CDBG_ERROR("%s:mm_camera_lib_open() err=%d\n", __func__, rc);
                    goto ERROR;
                }

                i = filter_resolutions(&lib_handle,
                                        dimension_tbl,
                                        sizeof(dimension_tbl)/sizeof(dimension_tbl[0]));
                if ( ( i < 0 ) || ( i >=  available_snap_sizes ) ) {
                    CDBG_ERROR("%s:filter_resolutions()\n", __func__);
                    goto ERROR;
                }
                snap_dim.width = dimension_tbl[i].width;
                snap_dim.height = dimension_tbl[i].height;

                rc = enableAFR(&lib_handle);
                if (rc != MM_CAMERA_OK) {
                    CDBG_ERROR("%s:enableAFR() err=%d\n", __func__, rc);
                    goto ERROR;
                }

                rc =  mm_camera_lib_send_command(&lib_handle,
                                                 MM_CAMERA_LIB_BESTSHOT,
                                                 &default_scene,
                                                 NULL);
                if (rc != MM_CAMERA_OK) {
                    CDBG_ERROR("%s:mm_camera_lib_send_command() err=%d\n", __func__, rc);
                    goto ERROR;
                }
                break;

            case ACTION_TOGGLE_ZSL:
                printf("ZSL Toggle !!!\n");
                isZSL = !isZSL;
                if ( isZSL ) {
                    printf("ZSL on !!!\n");
                } else {
                    printf("ZSL off !!!\n");
                }
                rc = mm_camera_lib_send_command(&lib_handle,
                                                MM_CAMERA_LIB_ZSL_ENABLE,
                                                &isZSL,
                                                NULL);
                if (rc != MM_CAMERA_OK) {
                    CDBG_ERROR("%s:mm_camera_lib_send_command() err=%d\n", __func__, rc);
                    goto ERROR;
                }
                break;

            case ACTION_TAKE_RAW_SNAPSHOT:
                CDBG_HIGH("\n Take RAW snapshot\n");

                rc = mm_camera_lib_send_command(&lib_handle,
                                                MM_CAMERA_LIB_DO_AF,
                                                NULL,
                                                NULL);

                if (rc != MM_CAMERA_OK) {
                    CDBG_ERROR("%s:mm_camera_lib_send_command() err=%d\n", __func__, rc);
                    goto ERROR;
                }

                rc = mm_camera_lib_send_command(&lib_handle,
                                                MM_CAMERA_LIB_RAW_CAPTURE,
                                                NULL,
                                                NULL);
                if (rc != MM_CAMERA_OK) {
                    CDBG_ERROR("%s:mm_camera_lib_send_command() err=%d\n", __func__, rc);
                    goto ERROR;
                }
                break;

            case ACTION_TAKE_JPEG_SNAPSHOT:
                CDBG_HIGH("\n Take JPEG snapshot\n");

                rc = mm_camera_lib_send_command(&lib_handle,
                                                MM_CAMERA_LIB_DO_AF,
                                                NULL,
                                                NULL);

                if (rc != MM_CAMERA_OK) {
                    CDBG_ERROR("%s:mm_camera_lib_send_command() err=%d\n", __func__, rc);
                    goto ERROR;
                }

                rc = mm_camera_lib_send_command(&lib_handle,
                                                MM_CAMERA_LIB_JPEG_CAPTURE,
                                                &snap_dim,
                                                NULL);
                if (rc != MM_CAMERA_OK) {
                    CDBG_ERROR("%s:mm_camera_lib_send_command() err=%d\n", __func__, rc);
                    goto ERROR;
                }
                break;
            case ACTION_SWITCH_RESOLUTION:
                printf("\n Switch snapshot resolution to %dx%d\n",
                       dimension_tbl[action_param].width,
                       dimension_tbl[action_param].height);
                snap_dim.width = dimension_tbl[action_param].width;
                snap_dim.height = dimension_tbl[action_param].height;
                break;

      case ACTION_START_RECORDING:
        CDBG("Start recording action\n");
#if 0
        if (mm_app_start_video(cam_id) < 0)
          goto ERROR;
        is_rec = 1;
#endif
        break;
      case ACTION_STOP_RECORDING:
        CDBG("Stop recording action\n");
#if 0
        if(is_rec) {
          if (mm_app_stop_video(cam_id) < 0)
            goto ERROR;
          is_rec = 0;
        }
#endif
        break;
      case ACTION_TAKE_LIVE_SNAPSHOT:
        printf("Selection for live shot\n");
#if 0
        if(is_rec)
           mm_app_take_live_snapshot(cam_id);
        else
           printf("\n !!! Use live snapshot option while recording only !!!\n");
#endif
        break;

        case ACTION_TOGGLE_WNR:
          wnr_enabled = !wnr_enabled;
          printf("WNR Enabled = %d\n", wnr_enabled);
          rc = mm_camera_lib_send_command(&lib_handle,
                                          MM_CAMERA_LIB_WNR_ENABLE,
                                          &wnr_enabled,
                                          NULL);
          if (rc != MM_CAMERA_OK) {
              CDBG_ERROR("%s:mm_camera_lib_send_command() err=%d\n", __func__, rc);
              goto ERROR;
          }
          break;

        case ACTION_EXIT:
            printf("Exiting....\n");
            break;
        case ACTION_NO_ACTION:
            printf("Go back to main menu");
            break;

        default:
            printf("\n\n!!!!!WRONG INPUT: %d!!!!\n", action_id);
            break;
    }

    usleep(1000 * 1000);
    CDBG("action_id = %d\n", action_id);

  } while (action_id != ACTION_EXIT);
  action_id = ACTION_NO_ACTION;

    mm_camera_lib_close(&lib_handle);
    return 0;

ERROR:

    mm_camera_lib_close(&lib_handle);

    return rc;
}

