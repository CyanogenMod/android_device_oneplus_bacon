/*
** Copyright (c) 2012 The Linux Foundation. All rights reserved.
**
** Not a Contribution, Apache license notifications and license are retained
** for attribution purposes only.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

//#define ALOG_NDEBUG 0
#define ALOG_NIDEBUG 0
#define LOG_TAG "QCameraHWI_Parm"
#include <utils/Log.h>

#include <utils/Errors.h>
#include <utils/threads.h>
#include <utils/String16.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cutils/properties.h>
#include <math.h>
#if HAVE_ANDROID_OS
#include <linux/android_pmem.h>
#endif
#include <linux/ioctl.h>
#include <camera/QCameraParameters.h>
#include <media/mediarecorder.h>
#include <gralloc_priv.h>

#include "linux/msm_mdp.h"
#include <linux/fb.h>
#include <limits.h>


extern "C" {
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
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
#include <sys/system_properties.h>
#include <sys/time.h>
#include <stdlib.h>
#include <linux/msm_ion.h>
#include <camera.h>
#include <cam_fifo.h>
#include <jpege.h>

} // extern "C"

#include "QCameraHWI.h"

/* QCameraHardwareInterface class implementation goes here*/
/* following code implements the parameter logic of this class*/
#define EXPOSURE_COMPENSATION_MAXIMUM_NUMERATOR 12
#define EXPOSURE_COMPENSATION_MINIMUM_NUMERATOR -12
#define EXPOSURE_COMPENSATION_DEFAULT_NUMERATOR 0
#define EXPOSURE_COMPENSATION_DENOMINATOR 6
#define EXPOSURE_COMPENSATION_STEP ((float (1))/EXPOSURE_COMPENSATION_DENOMINATOR)
#define DEFAULT_CAMERA_AREA "(0, 0, 0, 0, 0)"

#define HDR_HAL_FRAME 2

#define BURST_INTREVAL_MIN 1
#define BURST_INTREVAL_MAX 10
#define BURST_INTREVAL_DEFAULT 1

//Default FPS
#define MINIMUM_FPS 5
#define MAXIMUM_FPS 120
#define DEFAULT_FIXED_FPS 30
#define DEFAULT_FPS MAXIMUM_FPS

//Default Picture Width
#define DEFAULT_PICTURE_WIDTH  640
#define DEFAULT_PICTURE_HEIGHT 480

//Default Video Width
#define DEFAULT_VIDEO_WIDTH 1920
#define DEFAULT_VIDEO_HEIGHT 1080

#define THUMBNAIL_SIZE_COUNT (sizeof(thumbnail_sizes)/sizeof(thumbnail_size_type))
#define DEFAULT_THUMBNAIL_SETTING 4
#define THUMBNAIL_WIDTH_STR "512"
#define THUMBNAIL_HEIGHT_STR "384"
#define THUMBNAIL_SMALL_HEIGHT 144

#define DONT_CARE_COORDINATE -1

// All fps ranges which can be supported. This list will be filtered according
// to the min and max fps supported by hardware
// this list must be sorted first by max_fps and then min_fps
// fps values are multiplied by 1000
static android::FPSRange allFpsRanges[] = {
            android::FPSRange(7500, 7500),
            android::FPSRange(10000, 10000),
            android::FPSRange(7500, 15000),
            android::FPSRange(15000, 15000),
            android::FPSRange(7500, 20000),
            android::FPSRange(20000, 20000),
            android::FPSRange(7500, 30000),
            android::FPSRange(10000, 30000),
            android::FPSRange(15000, 30000),
            android::FPSRange(30000, 30000)
};
#define ALL_FPS_RANGES_COUNT (sizeof(allFpsRanges)/sizeof(android::FPSRange))

typedef struct {
    uint32_t aspect_ratio;
    uint32_t width;
    uint32_t height;
} thumbnail_size_type;

static thumbnail_size_type thumbnail_sizes[] = {
{ 7281, 512, 288 }, //1.777778
{ 6826, 480, 288 }, //1.666667
{ 6808, 256, 154 }, //1.66233
{ 6144, 432, 288 }, //1.5
{ 5461, 512, 384 }, //1.333333
{ 5006, 352, 288 }, //1.222222
{ 5461, 320, 240 }, //1.33333
{ 5006, 176, 144 }, //1.222222

};

static struct camera_size_type zsl_picture_sizes[] = {
  { 1024, 768}, // 1MP XGA
  { 800, 600}, //SVGA
  { 800, 480}, // WVGA
  { 640, 480}, // VGA
  { 352, 288}, //CIF
  { 320, 240}, // QVGA
  { 176, 144} // QCIF
};

static camera_size_type default_picture_sizes[] = {
  { 4000, 3000}, // 12MP
  { 3200, 2400}, // 8MP
  { 2592, 1944}, // 5MP
  { 2048, 1536}, // 3MP QXGA
  { 1920, 1080}, //HD1080
  { 1600, 1200}, // 2MP UXGA
  { 1280, 768}, //WXGA
  { 1280, 720}, //HD720
  { 1024, 768}, // 1MP XGA
  { 800, 600}, //SVGA
  { 800, 480}, // WVGA
  { 720, 480},
  { 640, 480}, // VGA
  { 352, 288}, //CIF
  { 320, 240}, // QVGA
  { 176, 144} // QCIF
};

static int iso_speed_values[] = {
    0, 1, 100, 200, 400, 800, 1600
};

extern int HAL_numOfCameras;
extern camera_info_t HAL_cameraInfo[MSM_MAX_CAMERA_SENSORS];
extern mm_camera_info_t * HAL_camerahandle[MSM_MAX_CAMERA_SENSORS];

namespace android {

static uint32_t  HFR_SIZE_COUNT=2;
static const int PICTURE_FORMAT_JPEG = 1;
static const int PICTURE_FORMAT_RAW = 2;

/********************************************************************/
static const str_map effects[] = {
    { QCameraParameters::EFFECT_NONE,       CAMERA_EFFECT_OFF },
    { QCameraParameters::EFFECT_MONO,       CAMERA_EFFECT_MONO },
    { QCameraParameters::EFFECT_NEGATIVE,   CAMERA_EFFECT_NEGATIVE },
    { QCameraParameters::EFFECT_SOLARIZE,   CAMERA_EFFECT_SOLARIZE },
    { QCameraParameters::EFFECT_SEPIA,      CAMERA_EFFECT_SEPIA },
    { QCameraParameters::EFFECT_POSTERIZE,  CAMERA_EFFECT_POSTERIZE },
    { QCameraParameters::EFFECT_WHITEBOARD, CAMERA_EFFECT_WHITEBOARD },
    { QCameraParameters::EFFECT_BLACKBOARD, CAMERA_EFFECT_BLACKBOARD },
    { QCameraParameters::EFFECT_AQUA,       CAMERA_EFFECT_AQUA },
    { QCameraParameters::EFFECT_EMBOSS,     CAMERA_EFFECT_EMBOSS },
    { QCameraParameters::EFFECT_SKETCH,     CAMERA_EFFECT_SKETCH },
    { QCameraParameters::EFFECT_NEON,       CAMERA_EFFECT_NEON }
};

static const str_map iso[] = {
    { QCameraParameters::ISO_AUTO,  CAMERA_ISO_AUTO},
    { QCameraParameters::ISO_HJR,   CAMERA_ISO_DEBLUR},
    { QCameraParameters::ISO_100,   CAMERA_ISO_100},
    { QCameraParameters::ISO_200,   CAMERA_ISO_200},
    { QCameraParameters::ISO_400,   CAMERA_ISO_400},
    { QCameraParameters::ISO_800,   CAMERA_ISO_800 },
    { QCameraParameters::ISO_1600,  CAMERA_ISO_1600 }
};

static const str_map scenemode[] = {
    { QCameraParameters::SCENE_MODE_AUTO,           CAMERA_BESTSHOT_OFF },
    { QCameraParameters::SCENE_MODE_ASD,            CAMERA_BESTSHOT_AUTO },
    { QCameraParameters::SCENE_MODE_ACTION,         CAMERA_BESTSHOT_ACTION },
    { QCameraParameters::SCENE_MODE_PORTRAIT,       CAMERA_BESTSHOT_PORTRAIT },
    { QCameraParameters::SCENE_MODE_LANDSCAPE,      CAMERA_BESTSHOT_LANDSCAPE },
    { QCameraParameters::SCENE_MODE_NIGHT,          CAMERA_BESTSHOT_NIGHT },
    { QCameraParameters::SCENE_MODE_NIGHT_PORTRAIT, CAMERA_BESTSHOT_NIGHT_PORTRAIT },
    { QCameraParameters::SCENE_MODE_THEATRE,        CAMERA_BESTSHOT_THEATRE },
    { QCameraParameters::SCENE_MODE_BEACH,          CAMERA_BESTSHOT_BEACH },
    { QCameraParameters::SCENE_MODE_SNOW,           CAMERA_BESTSHOT_SNOW },
    { QCameraParameters::SCENE_MODE_SUNSET,         CAMERA_BESTSHOT_SUNSET },
    { QCameraParameters::SCENE_MODE_STEADYPHOTO,    CAMERA_BESTSHOT_ANTISHAKE },
    { QCameraParameters::SCENE_MODE_FIREWORKS ,     CAMERA_BESTSHOT_FIREWORKS },
    { QCameraParameters::SCENE_MODE_SPORTS ,        CAMERA_BESTSHOT_SPORTS },
    { QCameraParameters::SCENE_MODE_PARTY,          CAMERA_BESTSHOT_PARTY },
    { QCameraParameters::SCENE_MODE_CANDLELIGHT,    CAMERA_BESTSHOT_CANDLELIGHT },
    { QCameraParameters::SCENE_MODE_BACKLIGHT,      CAMERA_BESTSHOT_BACKLIGHT },
    { QCameraParameters::SCENE_MODE_FLOWERS,        CAMERA_BESTSHOT_FLOWERS },
    { QCameraParameters::SCENE_MODE_AR,             CAMERA_BESTSHOT_AR },
};

static const str_map scenedetect[] = {
    { QCameraParameters::SCENE_DETECT_OFF, FALSE  },
    { QCameraParameters::SCENE_DETECT_ON, TRUE },
};

#define DONT_CARE AF_MODE_MAX
static const str_map focus_modes[] = {
    { QCameraParameters::FOCUS_MODE_AUTO,     AF_MODE_AUTO},
    { QCameraParameters::FOCUS_MODE_INFINITY, AF_MODE_INFINITY },
    { QCameraParameters::FOCUS_MODE_NORMAL,   AF_MODE_NORMAL },
    { QCameraParameters::FOCUS_MODE_MACRO,    AF_MODE_MACRO },
    { QCameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE, AF_MODE_CAF},
    { QCameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO, AF_MODE_CAF }
};

static const str_map selectable_zone_af[] = {
    { QCameraParameters::SELECTABLE_ZONE_AF_AUTO,  AUTO },
    { QCameraParameters::SELECTABLE_ZONE_AF_SPOT_METERING, SPOT },
    { QCameraParameters::SELECTABLE_ZONE_AF_CENTER_WEIGHTED, CENTER_WEIGHTED },
    { QCameraParameters::SELECTABLE_ZONE_AF_FRAME_AVERAGE, AVERAGE }
};

// from qcamera/common/camera.h
static const str_map autoexposure[] = {
    { QCameraParameters::AUTO_EXPOSURE_FRAME_AVG,  CAMERA_AEC_FRAME_AVERAGE },
    { QCameraParameters::AUTO_EXPOSURE_CENTER_WEIGHTED, CAMERA_AEC_CENTER_WEIGHTED },
    { QCameraParameters::AUTO_EXPOSURE_SPOT_METERING, CAMERA_AEC_SPOT_METERING }
};

// from aeecamera.h
static const str_map whitebalance[] = {
    { QCameraParameters::WHITE_BALANCE_AUTO,            CAMERA_WB_AUTO },
    { QCameraParameters::WHITE_BALANCE_INCANDESCENT,    CAMERA_WB_INCANDESCENT },
    { QCameraParameters::WHITE_BALANCE_FLUORESCENT,     CAMERA_WB_FLUORESCENT },
    { QCameraParameters::WHITE_BALANCE_DAYLIGHT,        CAMERA_WB_DAYLIGHT },
    { QCameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT, CAMERA_WB_CLOUDY_DAYLIGHT }
};

static const str_map antibanding[] = {
    { QCameraParameters::ANTIBANDING_OFF,  CAMERA_ANTIBANDING_OFF },
    { QCameraParameters::ANTIBANDING_50HZ, CAMERA_ANTIBANDING_50HZ },
    { QCameraParameters::ANTIBANDING_60HZ, CAMERA_ANTIBANDING_60HZ },
    { QCameraParameters::ANTIBANDING_AUTO, CAMERA_ANTIBANDING_AUTO }
};

static const str_map frame_rate_modes[] = {
        {QCameraParameters::KEY_QC_PREVIEW_FRAME_RATE_AUTO_MODE, FPS_MODE_AUTO},
        {QCameraParameters::KEY_QC_PREVIEW_FRAME_RATE_FIXED_MODE, FPS_MODE_FIXED}
};

static const str_map touchafaec[] = {
    { QCameraParameters::TOUCH_AF_AEC_OFF, FALSE },
    { QCameraParameters::TOUCH_AF_AEC_ON, TRUE }
};

static const str_map hfr[] = {
    { QCameraParameters::VIDEO_HFR_OFF, CAMERA_HFR_MODE_OFF },
    { QCameraParameters::VIDEO_HFR_2X, CAMERA_HFR_MODE_60FPS },
    { QCameraParameters::VIDEO_HFR_3X, CAMERA_HFR_MODE_90FPS },
    { QCameraParameters::VIDEO_HFR_4X, CAMERA_HFR_MODE_120FPS },
};
static const int HFR_VALUES_COUNT = (sizeof(hfr)/sizeof(str_map));

static const str_map flash[] = {
    { QCameraParameters::FLASH_MODE_OFF,  LED_MODE_OFF },
    { QCameraParameters::FLASH_MODE_AUTO, LED_MODE_AUTO },
    { QCameraParameters::FLASH_MODE_ON, LED_MODE_ON },
    { QCameraParameters::FLASH_MODE_TORCH, LED_MODE_TORCH}
};

static const str_map lensshade[] = {
    { QCameraParameters::LENSSHADE_ENABLE, TRUE },
    { QCameraParameters::LENSSHADE_DISABLE, FALSE }
};

static const str_map mce[] = {
    { QCameraParameters::MCE_ENABLE, TRUE },
    { QCameraParameters::MCE_DISABLE, FALSE }
};

static const str_map histogram[] = {
    { QCameraParameters::HISTOGRAM_ENABLE, TRUE },
    { QCameraParameters::HISTOGRAM_DISABLE, FALSE }
};

static const str_map skinToneEnhancement[] = {
    { QCameraParameters::SKIN_TONE_ENHANCEMENT_ENABLE, TRUE },
    { QCameraParameters::SKIN_TONE_ENHANCEMENT_DISABLE, FALSE }
};

static const str_map denoise[] = {
    { QCameraParameters::DENOISE_OFF, FALSE },
    { QCameraParameters::DENOISE_ON, TRUE }
};

static const str_map facedetection[] = {
    { QCameraParameters::FACE_DETECTION_OFF, FALSE },
    { QCameraParameters::FACE_DETECTION_ON, TRUE }
};

static const str_map redeye_reduction[] = {
    { QCameraParameters::REDEYE_REDUCTION_ENABLE, TRUE },
    { QCameraParameters::REDEYE_REDUCTION_DISABLE, FALSE }
};

static const str_map picture_formats[] = {
        {QCameraParameters::PIXEL_FORMAT_JPEG, PICTURE_FORMAT_JPEG},
        {QCameraParameters::PIXEL_FORMAT_RAW, PICTURE_FORMAT_RAW}
};

static const str_map recording_Hints[] = {
        {"false", FALSE},
        {"true",  TRUE}
};

static const str_map preview_formats[] = {
        {QCameraParameters::PIXEL_FORMAT_YUV420SP,   HAL_PIXEL_FORMAT_YCrCb_420_SP},
        {QCameraParameters::PIXEL_FORMAT_YUV420SP_ADRENO, HAL_PIXEL_FORMAT_YCrCb_420_SP_ADRENO},
        {QCameraParameters::PIXEL_FORMAT_YV12, HAL_PIXEL_FORMAT_YV12},
        {QCameraParameters::PIXEL_FORMAT_YUV420P,HAL_PIXEL_FORMAT_YV12},
        {QCameraParameters::PIXEL_FORMAT_NV12, HAL_PIXEL_FORMAT_YCbCr_420_SP}
};

static const preview_format_info_t preview_format_info_list[] = {
  {HAL_PIXEL_FORMAT_YCrCb_420_SP, CAMERA_YUV_420_NV21, CAMERA_PAD_TO_WORD, 2},
  {HAL_PIXEL_FORMAT_YCrCb_420_SP_ADRENO, CAMERA_YUV_420_NV21, CAMERA_PAD_TO_4K, 2},
  {HAL_PIXEL_FORMAT_YCbCr_420_SP, CAMERA_YUV_420_NV12, CAMERA_PAD_TO_WORD, 2},
  {HAL_PIXEL_FORMAT_YV12,         CAMERA_YUV_420_YV12, CAMERA_PAD_TO_WORD, 3}
};

static const str_map zsl_modes[] = {
    { QCameraParameters::ZSL_OFF, FALSE },
    { QCameraParameters::ZSL_ON, TRUE },
};


static const str_map hdr_bracket[] = {
    { QCameraParameters::AE_BRACKET_HDR_OFF,HDR_BRACKETING_OFF},
    { QCameraParameters::AE_BRACKET_HDR,HDR_MODE },
    { QCameraParameters::AE_BRACKET,EXP_BRACKETING_MODE }
};

typedef enum {
    NORMAL_POWER,
    LOW_POWER
} power_mode;

static const str_map power_modes[] = {
    { QCameraParameters::NORMAL_POWER,NORMAL_POWER },
    { QCameraParameters::LOW_POWER,LOW_POWER }
};

/**************************************************************************/
static int attr_lookup(const str_map arr[], int len, const char *name)
{
    if (name) {
        for (int i = 0; i < len; i++) {
            if (!strcmp(arr[i].desc, name))
                return arr[i].val;
        }
    }
    return NOT_FOUND;
}

bool QCameraHardwareInterface::native_set_parms(
    mm_camera_parm_type_t type, uint16_t length, void *value)
{
    ALOGV("%s : type : %d Value : %d",__func__,type,*((int *)value));
    if(mCameraHandle->ops->set_parm(mCameraHandle->camera_handle, type,value ) < 0) {
        ALOGE("native_set_parms failed: type %d length %d error %s",
            type, length, strerror(errno));
        return false;
    }

    return true;

}

bool QCameraHardwareInterface::native_set_parms(
    mm_camera_parm_type_t type, uint16_t length, void *value, int *result)
{
    *result= mCameraHandle->ops->set_parm(mCameraHandle->camera_handle, type,value );
    if( *result >=0 ) {
        ALOGE("native_set_parms: succeeded : %d", *result);
        return true;
    }

    ALOGV("native_set_parms failed: type %d length %d error str %s error# %d",
        type, length, strerror(errno), errno);
    return false;
}

//Filter Picture sizes based on max width and height
/* TBD: do we still need this - except for ZSL? */
void QCameraHardwareInterface::filterPictureSizes(){
    unsigned int i;
    if(mPictureSizeCount <= 0)
        return;
    maxSnapshotWidth = mPictureSizes[0].width;
    maxSnapshotHeight = mPictureSizes[0].height;
   // Iterate through all the width and height to find the max value
    for(i =0; i<mPictureSizeCount;i++){
        if(((maxSnapshotWidth < mPictureSizes[i].width) &&
            (maxSnapshotHeight <= mPictureSizes[i].height))){
            maxSnapshotWidth = mPictureSizes[i].width;
            maxSnapshotHeight = mPictureSizes[i].height;
        }
    }
    if(myMode & CAMERA_ZSL_MODE){
        // due to lack of PMEM we restrict to lower resolution
        mPictureSizesPtr = zsl_picture_sizes;
        mSupportedPictureSizesCount = 7;
    }else{
        mPictureSizesPtr = mPictureSizes;
        mSupportedPictureSizesCount = mPictureSizeCount;
    }
}

static String8 create_sizes_str(const camera_size_type *sizes, int len) {
    String8 str;
    char buffer[32];

    if (len > 0) {
        snprintf(buffer, sizeof(buffer), "%dx%d", sizes[0].width, sizes[0].height);
        str.append(buffer);
    }
    for (int i = 1; i < len; i++) {
        snprintf(buffer, sizeof(buffer), ",%dx%d", sizes[i].width, sizes[i].height);
        str.append(buffer);
    }
    return str;
}

String8 QCameraHardwareInterface::create_values_str(const str_map *values, int len) {
    String8 str;

    if (len > 0) {
        str.append(values[0].desc);
    }
    for (int i = 1; i < len; i++) {
        str.append(",");
        str.append(values[i].desc);
    }
    return str;
}

static String8 create_fps_str(const android:: FPSRange* fps, int len) {
    String8 str;
    char buffer[32];

    if (len > 0) {
        snprintf(buffer, sizeof(buffer), "(%d,%d)", fps[0].minFPS, fps[0].maxFPS);
        str.append(buffer);
    }
    for (int i = 1; i < len; i++) {
        snprintf(buffer, sizeof(buffer), ",(%d,%d)", fps[i].minFPS, fps[i].maxFPS);
        str.append(buffer);
    }
    return str;
}

static String8 create_values_range_str(int min, int max){
    String8 str;
    char buffer[32];

    if(min <= max){
        snprintf(buffer, sizeof(buffer), "%d", min);
        str.append(buffer);

        for (int i = min + 1; i <= max; i++) {
            snprintf(buffer, sizeof(buffer), ",%d", i);
            str.append(buffer);
        }
    }
    return str;
}

static int parse_size(const char *str, int &width, int &height)
{
    // Find the width.
    char *end;
    int w = (int)strtol(str, &end, 10);
    // If an 'x' or 'X' does not immediately follow, give up.
    if ( (*end != 'x') && (*end != 'X') )
        return -1;

    // Find the height, immediately after the 'x'.
    int h = (int)strtol(end+1, 0, 10);

    width = w;
    height = h;

    return 0;
}

bool QCameraHardwareInterface::isValidDimension(int width, int height) {
    bool retVal = FALSE;
    /* This function checks if a given resolution is valid or not.
     * A particular resolution is considered valid if it satisfies
     * the following conditions:
     * 1. width & height should be multiple of 16.
     * 2. width & height should be less than/equal to the dimensions
     *    supported by the camera sensor.
     * 3. the aspect ratio is a valid aspect ratio and is among the
     *    commonly used aspect ratio as determined by the thumbnail_sizes
     *    data structure.
     */

    if( (width == CEILING16(width)) && (height == CEILING16(height))
     && (width <= maxSnapshotWidth)
    && (height <= maxSnapshotHeight) )
    {
        uint32_t pictureAspectRatio = (uint32_t)((width * Q12)/height);
        for(uint32_t i = 0; i < THUMBNAIL_SIZE_COUNT; i++ ) {
            if(thumbnail_sizes[i].aspect_ratio == pictureAspectRatio) {
                retVal = TRUE;
                break;
            }
        }
    }
    return retVal;
}

void QCameraHardwareInterface::hasAutoFocusSupport(){

    ALOGV("%s",__func__);
    mHasAutoFocusSupport = false;

    if(isZSLMode()){
        mHasAutoFocusSupport = false;
        return;
    }

    if(mCameraHandle->ops->is_op_supported(mCameraHandle->camera_handle, MM_CAMERA_OPS_FOCUS)) {
        mHasAutoFocusSupport = true;
    }
    else {
        ALOGE("AutoFocus is not supported");
        mHasAutoFocusSupport = false;
    }
    ALOGV("%s:rc= %d",__func__, mHasAutoFocusSupport);

}

bool QCameraHardwareInterface::supportsSceneDetection() {
   bool rc ;
   mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,MM_CAMERA_PARM_ASD_ENABLE,(uint8_t*)&rc,(uint8_t*)&rc);
   return rc;
}

bool QCameraHardwareInterface::supportsFaceDetection() {
    bool rc ;
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,MM_CAMERA_PARM_FD,(uint8_t*)&rc,(uint8_t*)&rc);
    return rc;
}

bool QCameraHardwareInterface::supportsSelectableZoneAf() {
   bool rc ;
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,MM_CAMERA_PARM_FOCUS_RECT,(uint8_t*)&rc,(uint8_t*)&rc);
   return rc;
}

bool QCameraHardwareInterface::supportsRedEyeReduction() {
   bool rc ;
   mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,MM_CAMERA_PARM_REDEYE_REDUCTION,(uint8_t*)&rc,(uint8_t*)&rc);
   return rc;
}

static String8 create_str(int16_t *arr, int length){
    String8 str;
    char buffer[32] = {0};

    if(length > 0){
        snprintf(buffer, sizeof(buffer), "%d", arr[0]);
        str.append(buffer);
    }

    for (int i =1;i<length;i++){
        snprintf(buffer, sizeof(buffer), ",%d",arr[i]);
        str.append(buffer);
    }
    return str;
}

bool QCameraHardwareInterface::getMaxPictureDimension(mm_camera_dimension_t *maxDim)
{
    bool ret = NO_ERROR;
    mm_camera_dimension_t dim;

    ret = mCameraHandle->ops->get_parm(mCameraHandle->camera_handle,
                              MM_CAMERA_PARM_MAX_PICTURE_SIZE, &dim);
    if (ret != NO_ERROR)
        return ret;

    /* Find the first dimension in the mPictureSizes
     * array which is smaller than the max dimension.
     * This will be the valid max picture resolution */
    for (unsigned int i = 0; i < mPictureSizeCount; i++) {
        if ((mPictureSizes[i].width <= dim.width) &&
            (mPictureSizes[i].height <= dim.height)) {
            maxDim->height = mPictureSizes[i].height;
            maxDim->width  = mPictureSizes[i].width;
            break;
        }
    }
    ALOGD("%s: Found Max Picture dimension: %d x %d", __func__,
          maxDim->width, maxDim->height);
    return ret;
}
void QCameraHardwareInterface::loadTables()
{

    bool ret = NO_ERROR;
    ALOGV("%s: E", __func__);

    ret = mCameraHandle->ops->get_parm(mCameraHandle->camera_handle,
            MM_CAMERA_PARM_PREVIEW_SIZES_CNT, &preview_sizes_count);

    default_sizes_tbl_t preview_sizes_tbl;
    preview_sizes_tbl.tbl_size=preview_sizes_count;
    preview_sizes_tbl.sizes_tbl=&default_preview_sizes[0];
    if(mCameraHandle->ops->get_parm(mCameraHandle->camera_handle,
                            MM_CAMERA_PARM_DEF_PREVIEW_SIZES, &preview_sizes_tbl)<0){
        ALOGE("%s:Failed to get default preview sizes",__func__);
    }
    ret = mCameraHandle->ops->get_parm(mCameraHandle->camera_handle,
                MM_CAMERA_PARM_VIDEO_SIZES_CNT, &video_sizes_count);

    default_sizes_tbl_t video_sizes_tbl;
    video_sizes_tbl.tbl_size=video_sizes_count;
    video_sizes_tbl.sizes_tbl=&default_video_sizes[0];
    if(mCameraHandle->ops->get_parm(mCameraHandle->camera_handle,
                            MM_CAMERA_PARM_DEF_VIDEO_SIZES, &video_sizes_tbl)<0){
        ALOGE("%s:Failed to get default video sizes",__func__);
    }

    ret = mCameraHandle->ops->get_parm(mCameraHandle->camera_handle,
                MM_CAMERA_PARM_THUMB_SIZES_CNT, &thumbnail_sizes_count);

    default_sizes_tbl_t thumbnail_sizes_tbl;
    thumbnail_sizes_tbl.tbl_size=thumbnail_sizes_count;
    thumbnail_sizes_tbl.sizes_tbl=&default_thumbnail_sizes[0];
    if(mCameraHandle->ops->get_parm(mCameraHandle->camera_handle,
                            MM_CAMERA_PARM_DEF_THUMB_SIZES, &thumbnail_sizes_tbl)<0){
        ALOGE("%s:Failed to get default thumbnail sizes",__func__);
    }

    ret = mCameraHandle->ops->get_parm(mCameraHandle->camera_handle,
                MM_CAMERA_PARM_HFR_SIZES_CNT, &hfr_sizes_count);

    default_sizes_tbl_t hfr_sizes_tbl;
    hfr_sizes_tbl.tbl_size=hfr_sizes_count;
    hfr_sizes_tbl.sizes_tbl=&default_hfr_sizes[0];
    if(mCameraHandle->ops->get_parm(mCameraHandle->camera_handle,
                            MM_CAMERA_PARM_DEF_HFR_SIZES, &hfr_sizes_tbl)<0){
        ALOGE("%s:Failed to get default HFR  sizes",__func__);
    }
    ALOGE("%s: X", __func__);
}
void QCameraHardwareInterface::initDefaultParameters()
{
    bool ret;
    char prop[PROPERTY_VALUE_MAX];
    mm_camera_dimension_t maxDim;
    int rc = 0;
    ALOGI("%s: E", __func__);
    uint8_t supported;

    memset(&maxDim, 0, sizeof(mm_camera_dimension_t));
    ret = getMaxPictureDimension(&maxDim);

    if (ret != NO_ERROR) {
        ALOGE("%s: Cannot get Max picture size supported", __func__);
        return;
    }
    if (!maxDim.width || !maxDim.height) {
        maxDim.width = DEFAULT_LIVESHOT_WIDTH;
        maxDim.height = DEFAULT_LIVESHOT_HEIGHT;
    }

    memset(prop, 0, sizeof(prop));
    property_get("persist.camera.snap.format", prop, "0");
    mSnapshotFormat = atoi(prop);
    ALOGV("%s: prop =(%s), snap_format=%d", __func__, prop, mSnapshotFormat);

    //cam_ctrl_dimension_t dim;
    mHFRLevel = 0;
    memset(&mDimension, 0, sizeof(cam_ctrl_dimension_t));
    memset(&mPreviewFormatInfo, 0, sizeof(preview_format_info_t));
    mDimension.video_width     = DEFAULT_VIDEO_WIDTH;
    mDimension.video_height    = DEFAULT_VIDEO_HEIGHT;
    mDimension.picture_width   = maxDim.width;
    mDimension.picture_height  = maxDim.height;
    mDimension.display_width   = DEFAULT_STREAM_WIDTH;
    mDimension.display_height  = DEFAULT_STREAM_HEIGHT;
    mDimension.orig_picture_dx = mDimension.picture_width;
    mDimension.orig_picture_dy = mDimension.picture_height;
    mDimension.ui_thumbnail_width = DEFAULT_STREAM_WIDTH;
    mDimension.ui_thumbnail_height = DEFAULT_STREAM_HEIGHT;
    mDimension.orig_video_width = DEFAULT_STREAM_WIDTH;
    mDimension.orig_video_height = DEFAULT_STREAM_HEIGHT;

    mDimension.prev_format     = CAMERA_YUV_420_NV21;
    mDimension.enc_format      = CAMERA_YUV_420_NV12;
    if (mSnapshotFormat == 1) {
      mDimension.main_img_format = CAMERA_YUV_422_NV61;
    } else {
      mDimension.main_img_format = CAMERA_YUV_420_NV21;
    }
    mDimension.thumb_format    = CAMERA_YUV_420_NV21;
    ALOGV("%s: main_img_format =%d, thumb_format=%d", __func__,
         mDimension.main_img_format, mDimension.thumb_format);
    mDimension.prev_padding_format = CAMERA_PAD_TO_WORD;

    ret = native_set_parms(MM_CAMERA_PARM_DIMENSION,
                              sizeof(cam_ctrl_dimension_t), (void *) &mDimension);
    if(!ret) {
      ALOGE("MM_CAMERA_PARM_DIMENSION Failed.");
      return;
    }

    hasAutoFocusSupport();

    // Initialize constant parameter strings. This will happen only once in the
    // lifetime of the mediaserver process.
    if (true/*!mParamStringInitialized*/) {
        //filter picture sizes
        filterPictureSizes();
        mPictureSizeValues = create_sizes_str(
                mPictureSizesPtr, mSupportedPictureSizesCount);
        mPreviewSizeValues = create_sizes_str(
                mPreviewSizes,  mPreviewSizeCount);
        mVideoSizeValues = create_sizes_str(
                mVideoSizes,  mVideoSizeCount);

        //Query for max HFR value
        camera_hfr_mode_t maxHFR;
        mCameraHandle->ops->get_parm(mCameraHandle->camera_handle, MM_CAMERA_PARM_MAX_HFR_MODE, (void *)&maxHFR);
        //Filter HFR values and build parameter string
        String8 str;
        for(int i=0; i<HFR_VALUES_COUNT; i++){
            if(hfr[i].val <= maxHFR){
                if(i>0)	str.append(",");
                str.append(hfr[i].desc);
            }
        }
        mHfrValues = str;
        mHfrSizeValues = create_sizes_str(
                default_hfr_sizes, hfr_sizes_count);

        //Query for min and max fps values from lower layer
        if(MM_CAMERA_OK != mCameraHandle->ops->get_parm(mCameraHandle->camera_handle,
                                     MM_CAMERA_PARM_FPS_RANGE, (void *)&mSensorFpsRange)){
            ALOGE("error: failed to get fps range from sensor");
            return;
        } else {
            ALOGD("sensor fps range = (%f, %f)", mSensorFpsRange.min_fps,
                            mSensorFpsRange.max_fps);

            mSupportedFpsRanges = new android::FPSRange[ALL_FPS_RANGES_COUNT+1];

            //min and max fps in android format
            int minFps = (int)(mSensorFpsRange.min_fps * 1000);
            int maxFps = (int)(mSensorFpsRange.max_fps * 1000);
            int idx=0;
            //filter supported fps ranges according to sensor fps range
            for(int i=0; i<(int)ALL_FPS_RANGES_COUNT; i++) {
                if(allFpsRanges[i].maxFPS <= maxFps && allFpsRanges[i].minFPS >= minFps) {
                    memcpy(&mSupportedFpsRanges[idx], &allFpsRanges[i], sizeof(android::FPSRange));
                    idx++;
                }
            }
            mSupportedFpsRangesCount = idx;
            //insert sensor absolute fps range as one entry, if
            //its not already there in table
            //find insertion point
            int insertIndex = mSupportedFpsRangesCount;
            for(int i=0; i<mSupportedFpsRangesCount; i++) {
                if(mSupportedFpsRanges[i].maxFPS < maxFps) {
                   continue;
                } else if (mSupportedFpsRanges[i].maxFPS == maxFps) {
                    if(mSupportedFpsRanges[i].minFPS > minFps) {
                        insertIndex = i;
                        break;
                    } else if (mSupportedFpsRanges[i].minFPS == minFps) {
                        //entry already exists, no need to insert
                        insertIndex = -1;
                        break;
                    }
                }
            }
            if(insertIndex != -1) {
                for(int i=mSupportedFpsRangesCount; i>=0; i--) {
                    if(insertIndex == i) {
                        mSupportedFpsRanges[i].maxFPS = maxFps;
                        mSupportedFpsRanges[i].minFPS = minFps;
                        mSupportedFpsRangesCount++;
                        break;
                    }
                    mSupportedFpsRanges[i].maxFPS = mSupportedFpsRanges[i-1].maxFPS;
                    mSupportedFpsRanges[i].minFPS = mSupportedFpsRanges[i-1].minFPS;
                }
            }
            mFpsRangesSupportedValues = create_fps_str(mSupportedFpsRanges, mSupportedFpsRangesCount);

            ALOGD("supported fps ranges = %s", mFpsRangesSupportedValues.string());
            mParameters.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE,
                mFpsRangesSupportedValues);
            mParameters.setPreviewFpsRange(minFps, maxFps);
        }

        mFlashValues = create_values_str(
            flash, sizeof(flash) / sizeof(str_map));
        mLensShadeValues = create_values_str(
            lensshade,sizeof(lensshade)/sizeof(str_map));
        mMceValues = create_values_str(
            mce,sizeof(mce)/sizeof(str_map));
        mEffectValues = create_values_str(effects, sizeof(effects) / sizeof(str_map));
        mAntibandingValues = create_values_str(
            antibanding, sizeof(antibanding) / sizeof(str_map));
        mIsoValues = create_values_str(iso,sizeof(iso)/sizeof(str_map));
        mAutoExposureValues = create_values_str(
            autoexposure, sizeof(autoexposure) / sizeof(str_map));
        mWhitebalanceValues = create_values_str(
            whitebalance, sizeof(whitebalance) / sizeof(str_map));

        if(mHasAutoFocusSupport){
            mFocusModeValues = create_values_str(
                    focus_modes, sizeof(focus_modes) / sizeof(str_map));
        }

        mSceneModeValues = create_values_str(scenemode, sizeof(scenemode) / sizeof(str_map));

        if(mHasAutoFocusSupport){
            mTouchAfAecValues = create_values_str(
                touchafaec,sizeof(touchafaec)/sizeof(str_map));
        }
        //Currently Enabling Histogram for 8x60
        mHistogramValues = create_values_str(
            histogram,sizeof(histogram)/sizeof(str_map));

        mSkinToneEnhancementValues = create_values_str(
            skinToneEnhancement,sizeof(skinToneEnhancement)/sizeof(str_map));

        mPictureFormatValues = create_values_str(
            picture_formats, sizeof(picture_formats)/sizeof(str_map));

        mZoomSupported=false;
        mMaxZoom=0;
        mm_camera_zoom_tbl_t zmt;
        if( mCameraHandle->ops->get_parm(mCameraHandle->camera_handle,
                             MM_CAMERA_PARM_MAXZOOM, &mMaxZoom)<0){
            ALOGE("%s:Failed to get max zoom",__func__);
        }else{

            ALOGE("Max Zoom:%d",mMaxZoom);
            /* Kernel driver limits the max amount of data that can be retreived through a control
            command to 260 bytes hence we conservatively limit to 110 zoom ratios */
            if(mMaxZoom>MAX_ZOOM_RATIOS) {
                ALOGE("%s:max zoom is larger than sizeof zoomRatios table",__func__);
                mMaxZoom=MAX_ZOOM_RATIOS-1;
            }
            zmt.size=mMaxZoom;
            zmt.zoom_ratio_tbl=&zoomRatios[0];
            if( mCameraHandle->ops->get_parm(mCameraHandle->camera_handle,
                                 MM_CAMERA_PARM_ZOOM_RATIO, &zmt)<0){
                ALOGE("%s:Failed to get max zoom ratios",__func__);
            }else{
                mZoomSupported=true;
                mZoomRatioValues =  create_str(zoomRatios, mMaxZoom);
            }
        }

        ALOGE("Zoom supported:%d",mZoomSupported);

        denoise_value = create_values_str(
            denoise, sizeof(denoise) / sizeof(str_map));

       if(supportsFaceDetection()) {
            mFaceDetectionValues = create_values_str(
                facedetection, sizeof(facedetection) / sizeof(str_map));
        }

        if(mHasAutoFocusSupport){
            mSelectableZoneAfValues = create_values_str(
                selectable_zone_af, sizeof(selectable_zone_af) / sizeof(str_map));
        }

        mSceneDetectValues = create_values_str(scenedetect, sizeof(scenedetect) / sizeof(str_map));

        mRedeyeReductionValues = create_values_str(
            redeye_reduction, sizeof(redeye_reduction) / sizeof(str_map));

        mZslValues = create_values_str(
            zsl_modes,sizeof(zsl_modes)/sizeof(str_map));

        mParamStringInitialized = true;
    }

    //set supported video sizes
    mParameters.set(QCameraParameters::KEY_SUPPORTED_VIDEO_SIZES, mVideoSizeValues.string());

    //set default video size to first one in supported table
    String8 vSize = create_sizes_str(&mVideoSizes[0], 1);
    mParameters.set(QCameraParameters::KEY_VIDEO_SIZE, vSize.string());

    //Set Preview size
    int default_preview_width, default_preview_height;
    mCameraHandle->ops->get_parm(mCameraHandle->camera_handle, MM_CAMERA_PARM_DEFAULT_PREVIEW_WIDTH,
            &default_preview_width);
    mCameraHandle->ops->get_parm(mCameraHandle->camera_handle, MM_CAMERA_PARM_DEFAULT_PREVIEW_HEIGHT,
            &default_preview_height);
    mParameters.setPreviewSize(default_preview_width, default_preview_height);
    mParameters.set(QCameraParameters::KEY_SUPPORTED_PREVIEW_SIZES,
                    mPreviewSizeValues.string());
    mDimension.display_width = default_preview_width;
    mDimension.display_height = default_preview_height;

    mPreviewFrameRateValues = create_values_range_str(
        MINIMUM_FPS, MAXIMUM_FPS);


    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle, MM_CAMERA_PARM_FPS,&supported,&supported);
    if (supported) {
        mParameters.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES,
                        mPreviewFrameRateValues.string());
     } else {
        mParameters.set(
            QCameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES,
            DEFAULT_FIXED_FPS);
    }
    //Set Preview Format
    //mParameters.setPreviewFormat("yuv420sp"); // informative
    mParameters.setPreviewFormat(QCameraParameters::PIXEL_FORMAT_YUV420SP);

    mPreviewFormatValues = create_values_str(
        preview_formats, sizeof(preview_formats) / sizeof(str_map));
    mParameters.set(QCameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS,
            mPreviewFormatValues.string());

    //Set Overlay Format
    mParameters.set("overlay-format", HAL_PIXEL_FORMAT_YCbCr_420_SP);
    mParameters.set("max-num-detected-faces-hw", "2");

    // Set supported max faces
    int maxNumFaces = 0;
    if (supportsFaceDetection()) {
        //Query the maximum number of faces supported by hardware.
        if(MM_CAMERA_OK != mCameraHandle->ops->get_parm(mCameraHandle->camera_handle,
                               MM_CAMERA_PARM_MAX_NUM_FACES_DECT, &maxNumFaces)){
            ALOGE("%s:Failed to get max number of faces supported",__func__);
        }
    }
    mParameters.set(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW, maxNumFaces);
    //This paramtere is set to default here. This will be changed by application
    //if it needs to support specific number of faces. See also setParameters.
    mParameters.set(QCameraParameters::KEY_QC_MAX_NUM_REQUESTED_FACES, 2);

    // Set camera features supported flag
    int32_t featureFlag = 0;
    if (supportsFaceDetection()) {
        featureFlag |= 0x00000001; // bit 0 indicate faciral feature
    }
    mParameters.set(QCameraParameters::KEY_QC_SUPPORTED_CAMERA_FEATURES, featureFlag);

    //Set Picture Size
    mParameters.setPictureSize(DEFAULT_PICTURE_WIDTH, DEFAULT_PICTURE_HEIGHT);
    mParameters.set(QCameraParameters::KEY_SUPPORTED_PICTURE_SIZES,
                    mPictureSizeValues.string());

    //Set Preview Frame Rate
    if(mFps >= MINIMUM_FPS && mFps <= MAXIMUM_FPS) {
        mParameters.setPreviewFrameRate(mFps);
    }else{
        mParameters.setPreviewFrameRate(DEFAULT_FIXED_FPS);
    }

    //Set Picture Format
    mParameters.setPictureFormat("jpeg"); // informative
    mParameters.set(QCameraParameters::KEY_SUPPORTED_PICTURE_FORMATS,
                    mPictureFormatValues);

    mParameters.set(QCameraParameters::KEY_JPEG_QUALITY, "85"); // max quality
    mJpegQuality = 85;
    //Set Video Format
    mParameters.set(QCameraParameters::KEY_VIDEO_FRAME_FORMAT, "yuv420sp");

    //Set Thumbnail parameters
    mParameters.set(QCameraParameters::KEY_JPEG_THUMBNAIL_WIDTH,
                    THUMBNAIL_WIDTH_STR); // informative
    mParameters.set(QCameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT,
                    THUMBNAIL_HEIGHT_STR); // informative
    mDimension.ui_thumbnail_width =
            thumbnail_sizes[DEFAULT_THUMBNAIL_SETTING].width;
    mDimension.ui_thumbnail_height =
            thumbnail_sizes[DEFAULT_THUMBNAIL_SETTING].height;
    mParameters.set(QCameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, "90");
    String8 valuesStr = create_sizes_str(default_thumbnail_sizes, thumbnail_sizes_count);
    mParameters.set(QCameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES,
                valuesStr.string());
    // Define CAMERA_SMOOTH_ZOOM in Android.mk file , to enable smoothzoom
#ifdef CAMERA_SMOOTH_ZOOM
    mParameters.set(QCameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, "true");
#endif
    if(mZoomSupported){
        mParameters.set(QCameraParameters::KEY_ZOOM_SUPPORTED, "true");
        ALOGE("max zoom is %d", mMaxZoom-1);
        /* mMaxZoom value that the query interface returns is the size
        ALOGV("max zoom is %d", mMaxZoom-1);
        * mMaxZoom value that the query interface returns is the size
         * of zoom table. So the actual max zoom value will be one
         * less than that value.          */

        mParameters.set("max-zoom",mMaxZoom-1);
        mParameters.set(QCameraParameters::KEY_ZOOM_RATIOS,
                            mZoomRatioValues);
    } else
        {
        mParameters.set(QCameraParameters::KEY_ZOOM_SUPPORTED, "false");
    }

    /* Enable zoom support for video application if VPE enabled */
    if(mZoomSupported) {
        mParameters.set("video-zoom-support", "true");
    } else {
        mParameters.set("video-zoom-support", "false");
    }

    //8960 supports Power modes : Low power, Normal Power.
    mParameters.set("power-mode-supported", "true");
    //Set Live shot support
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_LIVESHOT_MAIN,
                                          &supported,&supported);
    if(!supported) {
        ALOGE("%s:LIVESHOT is  not supported", __func__);
        mParameters.set("video-snapshot-supported", "false");
        mFullLiveshotEnabled = rc;
    } else {
        mParameters.set("video-snapshot-supported", "true");
    }

    //Set default power mode
    mParameters.set(QCameraParameters::KEY_QC_POWER_MODE,"Normal_Power");

    //Set Camera Mode
    mParameters.set(QCameraParameters::KEY_QC_CAMERA_MODE,0);
    mParameters.set(QCameraParameters::KEY_QC_AE_BRACKET_HDR,"Off");

    //Set Antibanding
    mParameters.set(QCameraParameters::KEY_ANTIBANDING,
                    QCameraParameters::ANTIBANDING_OFF);
    mParameters.set(QCameraParameters::KEY_SUPPORTED_ANTIBANDING,
                    mAntibandingValues);

    //Set Effect
    mParameters.set(QCameraParameters::KEY_EFFECT,
                    QCameraParameters::EFFECT_NONE);
    mParameters.set(QCameraParameters::KEY_SUPPORTED_EFFECTS, mEffectValues);

    //Set Auto Exposure
    mParameters.set(QCameraParameters::KEY_QC_AUTO_EXPOSURE,
                    QCameraParameters::AUTO_EXPOSURE_FRAME_AVG);
    mParameters.set(QCameraParameters::KEY_QC_SUPPORTED_AUTO_EXPOSURE, mAutoExposureValues);

    //Set WhiteBalance
    mParameters.set(QCameraParameters::KEY_WHITE_BALANCE,
                    QCameraParameters::WHITE_BALANCE_AUTO);
    mParameters.set(QCameraParameters::KEY_SUPPORTED_WHITE_BALANCE,mWhitebalanceValues);

    //Set AEC_LOCK
    mParameters.set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK, "false");
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_AEC_LOCK,&supported,&supported);
    if(supported){
        mParameters.set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK_SUPPORTED, "true");
    } else {
        mParameters.set(QCameraParameters::KEY_AUTO_EXPOSURE_LOCK_SUPPORTED, "false");
    }
    //Set AWB_LOCK
    mParameters.set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK, "false");
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_AWB_LOCK,&supported,&supported);
    if(supported)
        mParameters.set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK_SUPPORTED, "true");
    else
        mParameters.set(QCameraParameters::KEY_AUTO_WHITEBALANCE_LOCK_SUPPORTED, "false");

    //Set Focus Mode
    if(mHasAutoFocusSupport){
       mParameters.set(QCameraParameters::KEY_FOCUS_MODE,
                          QCameraParameters::FOCUS_MODE_AUTO);
       mFocusMode = AF_MODE_AUTO;
       mParameters.set(QCameraParameters::KEY_SUPPORTED_FOCUS_MODES,
                          mFocusModeValues);
       mParameters.set(QCameraParameters::KEY_MAX_NUM_FOCUS_AREAS, "1");
       mParameters.set(QCameraParameters::KEY_MAX_NUM_METERING_AREAS, "1");
   } else {
       mParameters.set(QCameraParameters::KEY_FOCUS_MODE,
       QCameraParameters::FOCUS_MODE_INFINITY);
       mFocusMode = DONT_CARE;
       mParameters.set(QCameraParameters::KEY_SUPPORTED_FOCUS_MODES,
       QCameraParameters::FOCUS_MODE_INFINITY);
       mParameters.set(QCameraParameters::KEY_MAX_NUM_FOCUS_AREAS, "0");
       mParameters.set(QCameraParameters::KEY_MAX_NUM_METERING_AREAS, "0");
   }

    mParameters.set(QCameraParameters::KEY_FOCUS_AREAS, DEFAULT_CAMERA_AREA);
    mParameters.set(QCameraParameters::KEY_METERING_AREAS, DEFAULT_CAMERA_AREA);

    //Set Flash
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_LED_MODE,&supported,&supported);
    if(supported){
        mParameters.set(CameraParameters::KEY_FLASH_MODE,
                        CameraParameters::FLASH_MODE_OFF);
        mParameters.set(CameraParameters::KEY_SUPPORTED_FLASH_MODES,
                        mFlashValues);
    }

    //Set Sharpness
    mParameters.set(QCameraParameters::KEY_QC_MAX_SHARPNESS,
            CAMERA_MAX_SHARPNESS);
    mParameters.set(QCameraParameters::KEY_QC_SHARPNESS,
                    CAMERA_DEF_SHARPNESS);

    //Set Contrast
    mParameters.set(QCameraParameters::KEY_QC_MAX_CONTRAST,
            CAMERA_MAX_CONTRAST);
    mParameters.set(QCameraParameters::KEY_QC_CONTRAST,
                    CAMERA_DEF_CONTRAST);

    //Set Saturation
    mParameters.set(QCameraParameters::KEY_QC_MAX_SATURATION,
            CAMERA_MAX_SATURATION);
    mParameters.set(QCameraParameters::KEY_QC_SATURATION,
                    CAMERA_DEF_SATURATION);

    //Set Brightness/luma-adaptaion
    mParameters.set("luma-adaptation", "3");

    mParameters.set(QCameraParameters::KEY_PICTURE_FORMAT,
                    QCameraParameters::PIXEL_FORMAT_JPEG);

    //Set Lensshading
    mParameters.set(QCameraParameters::KEY_QC_LENSSHADE,
                    QCameraParameters::LENSSHADE_ENABLE);
    mParameters.set(QCameraParameters::KEY_QC_SUPPORTED_LENSSHADE_MODES,
                    mLensShadeValues);

    //Set ISO Mode
    mParameters.set(QCameraParameters::KEY_QC_ISO_MODE,
                    QCameraParameters::ISO_AUTO);
    mParameters.set(QCameraParameters::KEY_QC_SUPPORTED_ISO_MODES,
                    mIsoValues);

    //Set MCE
    mParameters.set(QCameraParameters::KEY_QC_MEMORY_COLOR_ENHANCEMENT,
                    QCameraParameters::MCE_ENABLE);
    mParameters.set(QCameraParameters::KEY_QC_SUPPORTED_MEM_COLOR_ENHANCE_MODES,
                    mMceValues);

    //Set HFR
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_HFR,&supported,&supported);
    if(supported){
        mParameters.set(QCameraParameters::KEY_QC_VIDEO_HIGH_FRAME_RATE,
                    QCameraParameters::VIDEO_HFR_OFF);
        mParameters.set(QCameraParameters::KEY_QC_SUPPORTED_HFR_SIZES,
                    mHfrSizeValues.string());
        mParameters.set(QCameraParameters::KEY_QC_SUPPORTED_VIDEO_HIGH_FRAME_RATE_MODES,
                    mHfrValues);
    } else{
        mParameters.set(QCameraParameters::KEY_QC_SUPPORTED_HFR_SIZES,"");
    }

    //Set Histogram
    mParameters.set(QCameraParameters::KEY_QC_HISTOGRAM,
                    QCameraParameters::HISTOGRAM_DISABLE);
    mParameters.set(QCameraParameters::KEY_QC_SUPPORTED_HISTOGRAM_MODES,
                    mHistogramValues);

    //Set SkinTone Enhancement
    mParameters.set(QCameraParameters::KEY_QC_SKIN_TONE_ENHANCEMENT,
                    QCameraParameters::SKIN_TONE_ENHANCEMENT_DISABLE);
    mParameters.set("skinToneEnhancement", "0");
    mParameters.set(QCameraParameters::KEY_QC_SUPPORTED_SKIN_TONE_ENHANCEMENT_MODES,
                    mSkinToneEnhancementValues);

    //Set Scene Mode
    mParameters.set(QCameraParameters::KEY_SCENE_MODE,
                    QCameraParameters::SCENE_MODE_AUTO);
    mParameters.set(QCameraParameters::KEY_SUPPORTED_SCENE_MODES,
                    mSceneModeValues);

    //Set Streaming Textures
    mParameters.set("strtextures", "OFF");

    //Set Denoise
    mParameters.set(QCameraParameters::KEY_QC_DENOISE,
                    QCameraParameters::DENOISE_OFF);
    mParameters.set(QCameraParameters::KEY_QC_SUPPORTED_DENOISE,
                        denoise_value);
    //Set Touch AF/AEC
    mParameters.set(QCameraParameters::KEY_QC_TOUCH_AF_AEC,
                    QCameraParameters::TOUCH_AF_AEC_OFF);
    mParameters.set(QCameraParameters::KEY_QC_SUPPORTED_TOUCH_AF_AEC,
                    mTouchAfAecValues);
    mParameters.set("touchAfAec-dx","100");
    mParameters.set("touchAfAec-dy","100");

    //Set Scene Detection
    mParameters.set(QCameraParameters::KEY_QC_SCENE_DETECT,
                   QCameraParameters::SCENE_DETECT_OFF);
    mParameters.set(QCameraParameters::KEY_QC_SUPPORTED_SCENE_DETECT,
                    mSceneDetectValues);

    //Set Selectable Zone AF
    mParameters.set(QCameraParameters::KEY_QC_SELECTABLE_ZONE_AF,
                    QCameraParameters::SELECTABLE_ZONE_AF_AUTO);
    mParameters.set(QCameraParameters::KEY_QC_SUPPORTED_SELECTABLE_ZONE_AF,
                    mSelectableZoneAfValues);

    //Set Face Detection
    mParameters.set(QCameraParameters::KEY_QC_FACE_DETECTION,
                    QCameraParameters::FACE_DETECTION_OFF);
    mParameters.set(QCameraParameters::KEY_QC_SUPPORTED_FACE_DETECTION,
                    mFaceDetectionValues);

    //Set Red Eye Reduction
    mParameters.set(QCameraParameters::KEY_QC_REDEYE_REDUCTION,
                    QCameraParameters::REDEYE_REDUCTION_DISABLE);
    mParameters.set(QCameraParameters::KEY_QC_SUPPORTED_REDEYE_REDUCTION,
                    mRedeyeReductionValues);

    //Set ZSL
    mParameters.set(QCameraParameters::KEY_QC_ZSL,
                    QCameraParameters::ZSL_OFF);
    mParameters.set(QCameraParameters::KEY_QC_SUPPORTED_ZSL_MODES,
                    mZslValues);

    //Set Focal length, horizontal and vertical view angles
    float focalLength = 0.0f;
    float horizontalViewAngle = 0.0f;
    float verticalViewAngle = 0.0f;
    mCameraHandle->ops->get_parm(mCameraHandle->camera_handle, MM_CAMERA_PARM_FOCAL_LENGTH,
            (void *)&focalLength);
    mParameters.setFloat(QCameraParameters::KEY_FOCAL_LENGTH,
                    focalLength);
    mCameraHandle->ops->get_parm(mCameraHandle->camera_handle, MM_CAMERA_PARM_HORIZONTAL_VIEW_ANGLE,
            (void *)&horizontalViewAngle);
    mParameters.setFloat(QCameraParameters::KEY_HORIZONTAL_VIEW_ANGLE,
                    horizontalViewAngle);
    mCameraHandle->ops->get_parm(mCameraHandle->camera_handle, MM_CAMERA_PARM_VERTICAL_VIEW_ANGLE,
            (void *)&verticalViewAngle);
    mParameters.setFloat(QCameraParameters::KEY_VERTICAL_VIEW_ANGLE,
                    verticalViewAngle);

    //Set Exposure Compensation
    mParameters.set(
            QCameraParameters::KEY_MAX_EXPOSURE_COMPENSATION,
            EXPOSURE_COMPENSATION_MAXIMUM_NUMERATOR);
    mParameters.set(
            QCameraParameters::KEY_MIN_EXPOSURE_COMPENSATION,
            EXPOSURE_COMPENSATION_MINIMUM_NUMERATOR);
    mParameters.set(
            QCameraParameters::KEY_EXPOSURE_COMPENSATION,
            EXPOSURE_COMPENSATION_DEFAULT_NUMERATOR);
    mParameters.setFloat(
            QCameraParameters::KEY_EXPOSURE_COMPENSATION_STEP,
            EXPOSURE_COMPENSATION_STEP);

    mParameters.set("num-snaps-per-shutter", 1);

    mParameters.set("capture-burst-captures-values", getZSLQueueDepth());
    mParameters.set("capture-burst-interval-supported", "true");
    mParameters.set("capture-burst-interval-max", BURST_INTREVAL_MAX); /*skip frames*/
    mParameters.set("capture-burst-interval-min", BURST_INTREVAL_MIN); /*skip frames*/
    mParameters.set("capture-burst-interval", BURST_INTREVAL_DEFAULT); /*skip frames*/
    mParameters.set("capture-burst-retroactive", 0);
    mParameters.set("capture-burst-retroactive-max", getZSLQueueDepth());
    mParameters.set("capture-burst-exposures", "");
    mParameters.set("capture-burst-exposures-values",
      "-12,-11,-10,-9,-8,-7,-6,-5,-4,-3,-2,-1,0,1,2,3,4,5,6,7,8,9,10,11,12");
    {
      String8 CamModeStr;
      char buffer[32];
      int flag = 0;

      for (int i = 0; i < HAL_CAM_MODE_MAX; i++) {
        if ( 0 ) { /*exclude some conflicting case*/
        } else {
          if (flag == 0) { /*first item*/
          snprintf(buffer, sizeof(buffer), "%d", i);
          } else {
            snprintf(buffer, sizeof(buffer), ",%d", i);
          }
          flag = 1;
          CamModeStr.append(buffer);
        }
      }
      mParameters.set("camera-mode-values", CamModeStr);
    }

    mParameters.set("ae-bracket-hdr-values",
      create_values_str(hdr_bracket, sizeof(hdr_bracket)/sizeof(str_map) ));

    mParameters.set("no-display-mode", 0);
    mParameters.set("zoom", 0);

    int mNuberOfVFEOutputs;
    ret = mCameraHandle->ops->get_parm(mCameraHandle->camera_handle,
                                       MM_CAMERA_PARM_VFE_OUTPUT_ENABLE, &mNuberOfVFEOutputs);
    if(ret < 0) {
        ALOGE("get parm MM_CAMERA_PARM_VFE_OUTPUT_ENABLE  failed");
        ret = BAD_VALUE;
    }
    if(mNuberOfVFEOutputs == 1)
    {
       mParameters.set(QCameraParameters::KEY_QC_SINGLE_ISP_OUTPUT_ENABLED, "true");
    } else {
       mParameters.set(QCameraParameters::KEY_QC_SINGLE_ISP_OUTPUT_ENABLED, "false");
    }

    if (setParameters(mParameters) != NO_ERROR) {
        ALOGE("Failed to set default parameters?!");
    }
    mInitialized = true;
    strTexturesOn = false;

    ALOGI("%s: X", __func__);
    return;
}

/**
 * Set the camera parameters. This returns BAD_VALUE if any parameter is
 * invalid or not supported.
 */

int QCameraHardwareInterface::setParameters(const char *parms)
{
    QCameraParameters param;
    String8 str = String8(parms);
    param.unflatten(str);
    status_t ret = setParameters(param);
	if(ret == NO_ERROR)
		return 0;
	else
		return -1;
}

/**
 * Set the camera parameters. This returns BAD_VALUE if any parameter is
 * invalid or not supported. */
status_t QCameraHardwareInterface::setParameters(const QCameraParameters& params)
{
    status_t ret = NO_ERROR;

    ALOGI("%s: E", __func__);
//    Mutex::Autolock l(&mLock);
    status_t rc, final_rc = NO_ERROR;
    mRestartPreview = false;

    if ((rc = setCameraMode(params)))                   final_rc = rc;
//    if ((rc = setChannelInterfaceMask(params)))         final_rc = rc;
    if ((rc = setPowerMode(params)))                    final_rc = rc;
    if ((rc = setPreviewSize(params)))                  final_rc = rc;
    if ((rc = setVideoSize(params)))                    final_rc = rc;
    if ((rc = setPictureSize(params)))                  final_rc = rc;
    if ((rc = setJpegThumbnailSize(params)))            final_rc = rc;
    if ((rc = setJpegQuality(params)))                  final_rc = rc;
    if ((rc = setEffect(params)))                       final_rc = rc;
    if ((rc = setGpsLocation(params)))                  final_rc = rc;
    if ((rc = setRotation(params)))                     final_rc = rc;
    if ((rc = setZoom(params)))                         final_rc = rc;
    if ((rc = setOrientation(params)))                  final_rc = rc;
    if ((rc = setLensshadeValue(params)))               final_rc = rc;
    if ((rc = setMCEValue(params)))                     final_rc = rc;
    if ((rc = setPictureFormat(params)))                final_rc = rc;
    if ((rc = setSharpness(params)))                    final_rc = rc;
    if ((rc = setSaturation(params)))                   final_rc = rc;
    if ((rc = setSceneMode(params)))                    final_rc = rc;
    if ((rc = setContrast(params)))                     final_rc = rc;
    if ((rc = setFaceDetect(params)))                   final_rc = rc;
    if ((rc = setStrTextures(params)))                  final_rc = rc;
    if ((rc = setPreviewFormat(params)))                final_rc = rc;
    if ((rc = setSkinToneEnhancement(params)))          final_rc = rc;
    if ((rc = setWaveletDenoise(params)))               final_rc = rc;
    if ((rc = setAntibanding(params)))                  final_rc = rc;
    //    if ((rc = setOverlayFormats(params)))         final_rc = rc;
    if ((rc = setRedeyeReduction(params)))              final_rc = rc;
    if ((rc = setCaptureBurstExp()))                    final_rc = rc;
    if ((rc = setRDIMode(params)))                      final_rc = rc;

    const char *str_val = params.get("capture-burst-exposures");
    if ( str_val == NULL || strlen(str_val)==0 ) {
        char burst_exp[PROPERTY_VALUE_MAX];
        memset(burst_exp, 0, sizeof(burst_exp));
        property_get("persist.capture.burst.exposures", burst_exp, "");
        if ( strlen(burst_exp)>0 ) {
            mParameters.set("capture-burst-exposures", burst_exp);
        }
    } else {
      mParameters.set("capture-burst-exposures", str_val);
    }
    mParameters.set("num-snaps-per-shutter", params.get("num-snaps-per-shutter"));

    if ((rc = setAEBracket(params)))              final_rc = rc;
    //    if ((rc = setDenoise(params)))                final_rc = rc;
    if ((rc = setPreviewFpsRange(params)))              final_rc = rc;
    if((rc = setRecordingHint(params)))                 final_rc = rc;
    if ((rc = setNumOfSnapshot(params)))                final_rc = rc;
    if ((rc = setAecAwbLock(params)))                   final_rc = rc;

    const char *str = params.get(QCameraParameters::KEY_SCENE_MODE);
    int32_t value = attr_lookup(scenemode, sizeof(scenemode) / sizeof(str_map), str);

    if((value != NOT_FOUND) && (value == CAMERA_BESTSHOT_OFF )) {
        //if ((rc = setPreviewFrameRateMode(params)))     final_rc = rc;
        if ((rc = setPreviewFrameRate(params)))         final_rc = rc;
        if ((rc = setAutoExposure(params)))             final_rc = rc;
        if ((rc = setExposureCompensation(params)))     final_rc = rc;
        if ((rc = setWhiteBalance(params)))             final_rc = rc;
        if ((rc = setFlash(params)))                    final_rc = rc;
        if ((rc = setFocusMode(params)))                final_rc = rc;
        if ((rc = setBrightness(params)))               final_rc = rc;
        if ((rc = setISOValue(params)))                 final_rc = rc;
        if ((rc = setFocusAreas(params)))               final_rc = rc;
        if ((rc = setMeteringAreas(params)))            final_rc = rc;
    }
    //selectableZoneAF needs to be invoked after continuous AF
    if ((rc = setSelectableZoneAf(params)))             final_rc = rc;
    // setHighFrameRate needs to be done at end, as there can
    // be a preview restart, and need to use the updated parameters
    if ((rc = setHighFrameRate(params)))  final_rc = rc;
    if ((rc = setZSLBurstLookBack(params))) final_rc = rc;
    if ((rc = setZSLBurstInterval(params))) final_rc = rc;
    if ((rc = setNoDisplayMode(params))) final_rc = rc;
    if ((rc = setMobiCat(params)))       final_rc = rc;

    //Update Exiftag values.
    setExifTags();

    if (mRestartPreview) {
        ALOGI("%s: need to restart preview", __func__);
        restartPreview();
        mRestartPreview = false;
    }

    ALOGI("%s: X", __func__);
    return final_rc;
}

/** Retrieve the camera parameters.  The buffer returned by the camera HAL
	must be returned back to it with put_parameters, if put_parameters
	is not NULL.
 */
int QCameraHardwareInterface::getParameters(char **parms)
{
    char* rc = NULL;
    String8 str;
    QCameraParameters param = getParameters();
    //param.dump();
    str = param.flatten( );
    rc = (char *)malloc(sizeof(char)*(str.length()+1));
    if(rc != NULL){
        memset(rc, 0, sizeof(char)*(str.length()+1));
        strncpy(rc, str.string(), str.length());
	rc[str.length()] = 0;
	*parms = rc;
    }
    return 0;
}

/** The camera HAL uses its own memory to pass us the parameters when we
	call get_parameters.  Use this function to return the memory back to
	the camera HAL, if put_parameters is not NULL.  If put_parameters
	is NULL, then you have to use free() to release the memory.
*/
void QCameraHardwareInterface::putParameters(char *rc)
{
    free(rc);
    rc = NULL;
}

QCameraParameters& QCameraHardwareInterface::getParameters()
{
    Mutex::Autolock lock(mLock);
    mParameters.set(QCameraParameters::KEY_FOCUS_DISTANCES, mFocusDistance.string());
    return mParameters;
}

status_t QCameraHardwareInterface::runFaceDetection()
{
    bool ret = true;

    const char *str = mParameters.get(QCameraParameters::KEY_QC_FACE_DETECTION);
    if (str != NULL) {
        int value = attr_lookup(facedetection,
                sizeof(facedetection) / sizeof(str_map), str);
        fd_set_parm_t fd_set_parm;
        int requested_faces = mParameters.getInt(QCameraParameters::KEY_QC_MAX_NUM_REQUESTED_FACES);
        fd_set_parm.fd_mode = value;
        fd_set_parm.num_fd = requested_faces;
        ret = native_set_parms(MM_CAMERA_PARM_FD, sizeof(int8_t), (void *)&value);
        return ret ? NO_ERROR : UNKNOWN_ERROR;
    }
    ALOGE("Invalid Face Detection value: %s", (str == NULL) ? "NULL" : str);
    return BAD_VALUE;
}

status_t QCameraHardwareInterface::setSharpness(const QCameraParameters& params)
{
    bool ret = false;
    int rc = MM_CAMERA_OK;
    ALOGV("%s",__func__);
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_SHARPNESS,(uint8_t*)&rc,(uint8_t*)&rc);
    if(!rc) {
        ALOGE("%s:CONTRAST not supported", __func__);
        return NO_ERROR;
    }
    int sharpness = params.getInt(QCameraParameters::KEY_QC_SHARPNESS);
    if((sharpness < CAMERA_MIN_SHARPNESS
            || sharpness > CAMERA_MAX_SHARPNESS))
        return UNKNOWN_ERROR;

    ALOGV("setting sharpness %d", sharpness);
    mParameters.set(QCameraParameters::KEY_QC_SHARPNESS, sharpness);
    ret = native_set_parms(MM_CAMERA_PARM_SHARPNESS, sizeof(sharpness),
                               (void *)&sharpness);
    return ret ? NO_ERROR : UNKNOWN_ERROR;
}

status_t QCameraHardwareInterface::setSaturation(const QCameraParameters& params)
{
    bool ret = false;
    int rc = MM_CAMERA_OK;
    ALOGV("%s",__func__);
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_SATURATION,(uint8_t*)&rc,(uint8_t*)&rc);
    if(!rc) {
        ALOGE("%s:MM_CAMERA_PARM_SATURATION not supported", __func__);
        return NO_ERROR;
    }
    int result;
    int saturation = params.getInt(QCameraParameters::KEY_QC_SATURATION);

    if((saturation < CAMERA_MIN_SATURATION)
        || (saturation > CAMERA_MAX_SATURATION))
    return UNKNOWN_ERROR;

    ALOGV("Setting saturation %d", saturation);
    mParameters.set(QCameraParameters::KEY_QC_SATURATION, saturation);
    ret = native_set_parms(MM_CAMERA_PARM_SATURATION, sizeof(saturation),
        (void *)&saturation, (int *)&result);
    if(result != MM_CAMERA_OK)
        ALOGI("Saturation Value: %d is not set as the selected value is not supported", saturation);
    return ret ? NO_ERROR : UNKNOWN_ERROR;
}

status_t QCameraHardwareInterface::setContrast(const QCameraParameters& params)
{
   ALOGV("%s E", __func__ );
   int rc = MM_CAMERA_OK;
   mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                         MM_CAMERA_PARM_CONTRAST,(uint8_t*)&rc,(uint8_t*)&rc);
   if(!rc) {
        ALOGE("%s:CONTRAST not supported", __func__);
        return NO_ERROR;
    }
   const char *str = params.get(QCameraParameters::KEY_SCENE_MODE);
   ALOGE("Contrast : %s",str);
   int32_t value = attr_lookup(scenemode, sizeof(scenemode) / sizeof(str_map), str);
   if(value == CAMERA_BESTSHOT_OFF) {
        int contrast = params.getInt(QCameraParameters::KEY_QC_CONTRAST);
        if((contrast < CAMERA_MIN_CONTRAST)
                || (contrast > CAMERA_MAX_CONTRAST))
        {
            ALOGE("Contrast Value not matching");
            return UNKNOWN_ERROR;
        }
        ALOGV("setting contrast %d", contrast);
        mParameters.set(QCameraParameters::KEY_QC_CONTRAST, contrast);
        ALOGE("Calling Contrast set on Lower layer");
        bool ret = native_set_parms(MM_CAMERA_PARM_CONTRAST, sizeof(contrast),
                                   (void *)&contrast);
        ALOGE("Lower layer returned %d", ret);
        int bestshot_reconfigure;
        mCameraHandle->ops->get_parm(mCameraHandle->camera_handle,
                                     MM_CAMERA_PARM_BESTSHOT_RECONFIGURE,
                            &bestshot_reconfigure);
        if(bestshot_reconfigure) {
             if (mContrast != contrast) {
                  mContrast = contrast;
                 if (mPreviewState == QCAMERA_HAL_PREVIEW_STARTED && ret) {
                      mRestartPreview = true;
                  }
             }
        }
        return ret ? NO_ERROR : UNKNOWN_ERROR;
    } else {
          ALOGI(" Contrast value will not be set " \
          "when the scenemode selected is %s", str);
          return NO_ERROR;
    }
    return BAD_VALUE;
}

status_t QCameraHardwareInterface::setSceneDetect(const QCameraParameters& params)
{
    ALOGV("%s",__func__);
    bool retParm;
    int rc = 0;

    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_ASD_ENABLE,(uint8_t*)&rc,(uint8_t*)&rc);
    if(!rc) {
        ALOGE("%s:MM_CAMERA_PARM_ASD_ENABLE not supported", __func__);
        return NO_ERROR;
    }

    const char *str = params.get(QCameraParameters::KEY_QC_SCENE_DETECT);
    ALOGE("Scene Detect string : %s",str);
    if (str != NULL) {
        int32_t value = attr_lookup(scenedetect, sizeof(scenedetect) / sizeof(str_map), str);
        ALOGE("Scenedetect Value : %d",value);
        if (value != NOT_FOUND) {
            mParameters.set(QCameraParameters::KEY_QC_SCENE_DETECT, str);

            retParm = native_set_parms(MM_CAMERA_PARM_ASD_ENABLE, sizeof(value),
                                       (void *)&value);

            return retParm ? NO_ERROR : UNKNOWN_ERROR;
        }
    }
   return BAD_VALUE;
}

status_t QCameraHardwareInterface::setZoom(const QCameraParameters& params)
{
    status_t rc = NO_ERROR;

    ALOGV("%s: E",__func__);
    uint8_t supported;

    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_ZOOM,&supported,&supported);
    if(!supported){
        ALOGE("%s:MM_CAMERA_PARM_ZOOM not supported", __func__);
        return NO_ERROR;
    }
    // No matter how many different zoom values the driver can provide, HAL
    // provides applictations the same number of zoom levels. The maximum driver
    // zoom value depends on sensor output (VFE input) and preview size (VFE
    // output) because VFE can only crop and cannot upscale. If the preview size
    // is bigger, the maximum zoom ratio is smaller. However, we want the
    // zoom ratio of each zoom level is always the same whatever the preview
    // size is. Ex: zoom level 1 is always 1.2x, zoom level 2 is 1.44x, etc. So,
    // we need to have a fixed maximum zoom value and do read it from the
    // driver.
    static const int ZOOM_STEP = 1;
    int32_t zoom_level = params.getInt("zoom");
    if(zoom_level >= 0 && zoom_level <= mMaxZoom-1) {
        mParameters.set("zoom", zoom_level);
        int32_t zoom_value = ZOOM_STEP * zoom_level;
        bool ret = native_set_parms(MM_CAMERA_PARM_ZOOM,
            sizeof(zoom_value), (void *)&zoom_value);
        if(ret) {
            mCurrentZoom=zoom_level;
        }
        rc = ret ? NO_ERROR : UNKNOWN_ERROR;
    } else {
        rc = BAD_VALUE;
    }
    ALOGV("%s X",__func__);
    return rc;

}

status_t  QCameraHardwareInterface::setISOValue(const QCameraParameters& params) {

    ALOGV("%s",__func__);
    uint8_t supported;
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_ISO,&supported,&supported);
    if(!supported) {
        ALOGE("%s:MM_CAMERA_PARM_ISO not supported", __func__);
        return NO_ERROR;
    }
    const char *str = params.get(QCameraParameters::KEY_QC_ISO_MODE);
    ALOGV("ISO string : %s",str);
    int8_t temp_hjr;
    if (str != NULL) {
        int value = (camera_iso_mode_type)attr_lookup(
          iso, sizeof(iso) / sizeof(str_map), str);
        ALOGE("ISO Value : %d",value);
        if (value != NOT_FOUND) {
            camera_iso_mode_type temp = (camera_iso_mode_type) value;
            if (value == CAMERA_ISO_DEBLUR) {
               temp_hjr = true;
               native_set_parms(MM_CAMERA_PARM_HJR, sizeof(int8_t), (void*)&temp_hjr);
               mHJR = value;
            }
            else {
               if (mHJR == CAMERA_ISO_DEBLUR) {
                   temp_hjr = false;
                   native_set_parms(MM_CAMERA_PARM_HJR, sizeof(int8_t), (void*)&temp_hjr);
                   mHJR = value;
               }
            }

            mParameters.set(QCameraParameters::KEY_QC_ISO_MODE, str);
            native_set_parms(MM_CAMERA_PARM_ISO, sizeof(camera_iso_mode_type), (void *)&temp);
            return NO_ERROR;
        }
    }
    return BAD_VALUE;
}

status_t QCameraHardwareInterface::updateFocusDistances()
{
    ALOGV("%s: IN", __FUNCTION__);
    focus_distances_info_t focusDistances;
    if(mCameraHandle->ops->get_parm(mCameraHandle->camera_handle, MM_CAMERA_PARM_FOCUS_DISTANCES,
      &focusDistances) >= 0) {
        String8 str;
        char buffer[32] = {0};
        //set all distances to infinity if focus mode is infinity
        if(mFocusMode == AF_MODE_INFINITY) {
            snprintf(buffer, sizeof(buffer), "Infinity,");
            str.append(buffer);
            snprintf(buffer, sizeof(buffer), "Infinity,");
            str.append(buffer);
            snprintf(buffer, sizeof(buffer), "Infinity");
            str.append(buffer);
        } else {
            snprintf(buffer, sizeof(buffer), "%f", focusDistances.focus_distance[0]);
            str.append(buffer);
            snprintf(buffer, sizeof(buffer), ",%f", focusDistances.focus_distance[1]);
            str.append(buffer);
            snprintf(buffer, sizeof(buffer), ",%f", focusDistances.focus_distance[2]);
            str.append(buffer);
        }
        ALOGE("%s: setting KEY_FOCUS_DISTANCES as %s", __FUNCTION__, str.string());
        mFocusDistance = str;
        return NO_ERROR;
    }
    ALOGV("%s: get CAMERA_PARM_FOCUS_DISTANCES failed!!!", __FUNCTION__);
    return BAD_VALUE;
}

// Parse string like "(1, 2, 3, 4, ..., N)"
// num is pointer to an allocated array of size N
static int parseNDimVector(const char *str, int *num, int N, char delim = ',')
{
    char *start, *end;
    if(num == NULL) {
        ALOGE("Invalid output array (num == NULL)");
        return -1;
    }
    //check if string starts and ends with parantheses
    if(str[0] != '(' || str[strlen(str)-1] != ')') {
        ALOGE("Invalid format of string %s, valid format is (n1, n2, n3, n4 ...)", str);
        return -1;
    }
    start = (char*) str;
    start++;
    for(int i=0; i<N; i++) {
        *(num+i) = (int) strtol(start, &end, 10);
        if(*end != delim && i < N-1) {
            ALOGE("Cannot find delimeter '%c' in string \"%s\". end = %c", delim, str, *end);
            return -1;
        }
        start = end+1;
    }
    return 0;
}

// parse string like "(1, 2, 3, 4, 5),(1, 2, 3, 4, 5),..."
static int parseCameraAreaString(const char* str, int max_num_areas,
                                 camera_area_t *pAreas, int *num_areas_found)
{
    char area_str[32];
    const char *start, *end, *p;
    start = str; end = NULL;
    int values[5], index=0;
    *num_areas_found = 0;

    while(start != NULL) {
       if(*start != '(') {
            ALOGE("%s: error: Ill formatted area string: %s", __func__, str);
            return -1;
       }
       end = strchr(start, ')');
       if(end == NULL) {
            ALOGE("%s: error: Ill formatted area string: %s", __func__, str);
            return -1;
       }
       int i;
       for (i=0,p=start; p<=end; p++, i++) {
           area_str[i] = *p;
       }
       area_str[i] = '\0';
       if(parseNDimVector(area_str, values, 5) < 0){
            ALOGE("%s: error: Failed to parse the area string: %s", __func__, area_str);
            return -1;
       }
       // no more areas than max_num_areas are accepted.
       if(index >= max_num_areas) {
            ALOGE("%s: error: too many areas specified %s", __func__, str);
            return -1;
       }
       pAreas[index].x1 = values[0];
       pAreas[index].y1 = values[1];
       pAreas[index].x2 = values[2];
       pAreas[index].y2 = values[3];
       pAreas[index].weight = values[4];

       index++;
       start = strchr(end, '('); // serach for next '('
    }
    (*num_areas_found) = index;
    return 0;
}
static bool validateCameraAreas(camera_area_t *areas, int num_areas)
{
    for(int i=0; i<num_areas; i++) {

        // handle special case (0, 0, 0, 0, 0)
        if((areas[i].x1 == 0) && (areas[i].y1 == 0)
            && (areas[i].x2 == 0) && (areas[i].y2 == 0) && (areas[i].weight == 0)) {
            continue;
        }
        if(areas[i].x1 < -1000) return false;               // left should be >= -1000
        if(areas[i].y1 < -1000) return false;               // top  should be >= -1000
        if(areas[i].x2 > 1000) return false;                // right  should be <= 1000
        if(areas[i].y2 > 1000) return false;                // bottom should be <= 1000
        if(areas[i].weight <= 0 || areas[i].weight > 1000)  // weight should be in [1, 1000]
            return false;
        if(areas[i].x1 >= areas[i].x2) {                    // left should be < right
            return false;
        }
        if(areas[i].y1 >= areas[i].y2)                      // top should be < bottom
            return false;
    }
    return true;
}

status_t QCameraHardwareInterface::setFocusAreas(const QCameraParameters& params)
{
    ALOGV("%s: E", __func__);
    status_t rc;
    int max_num_af_areas = mParameters.getInt(QCameraParameters::KEY_MAX_NUM_FOCUS_AREAS);
    if(max_num_af_areas == 0) {
        return NO_ERROR;
    }
    const char *str = params.get(QCameraParameters::KEY_FOCUS_AREAS);
    if (str == NULL) {
        ALOGE("%s: Parameter string is null", __func__);
        rc = NO_ERROR;
    } else {
        camera_area_t *areas = new camera_area_t[max_num_af_areas];
        int num_areas_found=0;
        if(parseCameraAreaString(str, max_num_af_areas, areas, &num_areas_found) < 0) {
            ALOGE("%s: Failed to parse the string: %s", __func__, str);
            delete areas;
            return BAD_VALUE;
        }
        for(int i=0; i<num_areas_found; i++) {
            ALOGD("FocusArea[%d] = (%d, %d, %d, %d, %d)", i, (areas[i].x1), (areas[i].y1),
                        (areas[i].x2), (areas[i].y2), (areas[i].weight));
        }
        if(validateCameraAreas(areas, num_areas_found) == false) {
            ALOGE("%s: invalid areas specified : %s", __func__, str);
            delete areas;
            return BAD_VALUE;
        }
        mParameters.set(QCameraParameters::KEY_FOCUS_AREAS, str);
        num_areas_found = 1; //temp; need to change after the multi-roi is enabled

        //if the native_set_parms is called when preview is not started, it
        //crashes in lower layer, so return of preview is not started
        if(mPreviewState == QCAMERA_HAL_PREVIEW_STOPPED) {
            delete areas;
            return NO_ERROR;
        }

        //for special area string (0, 0, 0, 0, 0), set the num_areas_found to 0,
        //so no action is takenby the lower layer
        if(num_areas_found == 1 && (areas[0].x1 == 0) && (areas[0].y1 == 0)
            && (areas[0].x2 == 0) && (areas[0].y2 == 0) && (areas[0].weight == 0)) {
            num_areas_found = 0;
        }
        roi_info_t af_roi_value;
        memset(&af_roi_value, 0, sizeof(roi_info_t));
        uint16_t x1, x2, y1, y2, dx, dy;
        int previewWidth, previewHeight;
        this->getPreviewSize(&previewWidth, &previewHeight);
        //transform the coords from (-1000, 1000) to (0, previewWidth or previewHeight)
        x1 = (uint16_t)((areas[0].x1 + 1000.0f)*(previewWidth/2000.0f));
        y1 = (uint16_t)((areas[0].y1 + 1000.0f)*(previewHeight/2000.0f));
        x2 = (uint16_t)((areas[0].x2 + 1000.0f)*(previewWidth/2000.0f));
        y2 = (uint16_t)((areas[0].y2 + 1000.0f)*(previewHeight/2000.0f));
        dx = x2 - x1;
        dy = y2 - y1;

        af_roi_value.num_roi = num_areas_found;
        af_roi_value.roi[0].x = x1;
        af_roi_value.roi[0].y = y1;
        af_roi_value.roi[0].dx = dx;
        af_roi_value.roi[0].dy = dy;
        af_roi_value.is_multiwindow = 0;
        if (native_set_parms(MM_CAMERA_PARM_AF_ROI, sizeof(roi_info_t), (void*)&af_roi_value))
            rc = NO_ERROR;
        else
            rc = BAD_VALUE;
        delete areas;
    }
    ALOGE("%s: X", __func__);
    return rc;
}

status_t QCameraHardwareInterface::setMeteringAreas(const QCameraParameters& params)
{
    ALOGV("%s: E", __func__);
    status_t rc;
    int max_num_mtr_areas = mParameters.getInt(QCameraParameters::KEY_MAX_NUM_METERING_AREAS);
    if(max_num_mtr_areas == 0) {
        return NO_ERROR;
    }

    const char *str = params.get(QCameraParameters::KEY_METERING_AREAS);
    if (str == NULL) {
        ALOGE("%s: Parameter string is null", __func__);
        rc = NO_ERROR;
    } else {
        camera_area_t *areas = new camera_area_t[max_num_mtr_areas];
        int num_areas_found=0;
        if(parseCameraAreaString(str, max_num_mtr_areas, areas, &num_areas_found) < 0) {
            ALOGE("%s: Failed to parse the string: %s", __func__, str);
            delete areas;
            return BAD_VALUE;
        }
        for(int i=0; i<num_areas_found; i++) {
            ALOGD("MeteringArea[%d] = (%d, %d, %d, %d, %d)", i, (areas[i].x1), (areas[i].y1),
                        (areas[i].x2), (areas[i].y2), (areas[i].weight));
        }
        if(validateCameraAreas(areas, num_areas_found) == false) {
            ALOGE("%s: invalid areas specified : %s", __func__, str);
            delete areas;
            return BAD_VALUE;
        }
        mParameters.set(QCameraParameters::KEY_METERING_AREAS, str);

        //if the native_set_parms is called when preview is not started, it
        //crashes in lower layer, so return of preview is not started
        if(mPreviewState == QCAMERA_HAL_PREVIEW_STOPPED) {
            delete areas;
            return NO_ERROR;
        }

        num_areas_found = 1; //temp; need to change after the multi-roi is enabled

        //for special area string (0, 0, 0, 0, 0), set the num_areas_found to 0,
        //so no action is takenby the lower layer
        if(num_areas_found == 1 && (areas[0].x1 == 0) && (areas[0].y1 == 0)
             && (areas[0].x2 == 0) && (areas[0].y2 == 0) && (areas[0].weight == 0)) {
            num_areas_found = 0;
        }
        cam_set_aec_roi_t aec_roi_value;
        uint16_t x1, x2, y1, y2;
        int previewWidth, previewHeight;
        this->getPreviewSize(&previewWidth, &previewHeight);
        //transform the coords from (-1000, 1000) to (0, previewWidth or previewHeight)
        x1 = (uint16_t)((areas[0].x1 + 1000.0f)*(previewWidth/2000.0f));
        y1 = (uint16_t)((areas[0].y1 + 1000.0f)*(previewHeight/2000.0f));
        x2 = (uint16_t)((areas[0].x2 + 1000.0f)*(previewWidth/2000.0f));
        y2 = (uint16_t)((areas[0].y2 + 1000.0f)*(previewHeight/2000.0f));
        delete areas;

        if(num_areas_found == 1) {
            aec_roi_value.aec_roi_enable = AEC_ROI_ON;
            aec_roi_value.aec_roi_type = AEC_ROI_BY_COORDINATE;
            aec_roi_value.aec_roi_position.coordinate.x = (x1+x2)/2;
            aec_roi_value.aec_roi_position.coordinate.y = (y1+y2)/2;
        } else {
            aec_roi_value.aec_roi_enable = AEC_ROI_OFF;
            aec_roi_value.aec_roi_type = AEC_ROI_BY_COORDINATE;
            aec_roi_value.aec_roi_position.coordinate.x = DONT_CARE_COORDINATE;
            aec_roi_value.aec_roi_position.coordinate.y = DONT_CARE_COORDINATE;
        }

        if(native_set_parms(MM_CAMERA_PARM_AEC_ROI, sizeof(cam_set_aec_roi_t), (void *)&aec_roi_value))
            rc = NO_ERROR;
        else
            rc = BAD_VALUE;
    }
    ALOGV("%s: X", __func__);
    return rc;
}

status_t QCameraHardwareInterface::setFocusMode(const QCameraParameters& params)
{
    const char *str = params.get(QCameraParameters::KEY_FOCUS_MODE);
    const char *prev_str = mParameters.get(QCameraParameters::KEY_FOCUS_MODE);
    ALOGV("%s",__func__);
    if (str != NULL) {
        ALOGE("Focus mode %s",str);
        int32_t value = attr_lookup(focus_modes,
                                    sizeof(focus_modes) / sizeof(str_map), str);
        if (value != NOT_FOUND) {
            mParameters.set(QCameraParameters::KEY_FOCUS_MODE, str);
            mFocusMode = value;

            if(updateFocusDistances() != NO_ERROR) {
               ALOGE("%s: updateFocusDistances failed for %s", __FUNCTION__, str);
               return UNKNOWN_ERROR;
            }
            mParameters.set(QCameraParameters::KEY_FOCUS_DISTANCES, mFocusDistance.string());
            if(mHasAutoFocusSupport){
                bool ret = native_set_parms(MM_CAMERA_PARM_FOCUS_MODE,
                                      sizeof(value),
                                      (void *)&value);

                int cafSupport = FALSE;
                if(!strcmp(str, QCameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO) ||
                   !strcmp(str, QCameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE)){
                    cafSupport = TRUE;
                }
                ALOGE("Continuous Auto Focus %d", cafSupport);
                ret = native_set_parms(MM_CAMERA_PARM_CONTINUOUS_AF, sizeof(cafSupport),
                                       (void *)&cafSupport);
            }

            return NO_ERROR;
        }
        ALOGE("%s:Could not look up str value",__func__);
    }
    ALOGE("Invalid focus mode value: %s", (str == NULL) ? "NULL" : str);
    return BAD_VALUE;
}

status_t QCameraHardwareInterface::setSceneMode(const QCameraParameters& params)
{
    ALOGV("%s",__func__);
    uint8_t supported;
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_BESTSHOT_MODE,&supported,&supported);
    if(!supported) {
        ALOGE("%s:Parameter Scenemode is not supported for this sensor", __func__);
        return NO_ERROR;
    }
    const char *str = params.get(QCameraParameters::KEY_SCENE_MODE);
    ALOGE("Scene Mode string : %s",str);

    if (str != NULL) {
        int32_t value = attr_lookup(scenemode, sizeof(scenemode) / sizeof(str_map), str);
        ALOGE("Setting Scenemode value = %d",value );
        if (value != NOT_FOUND) {
            if((value != CAMERA_BESTSHOT_OFF ) && (mColorEffects != CAMERA_EFFECT_OFF )) {
               int result;
               mColorEffects = CAMERA_EFFECT_OFF;
               native_set_parms(MM_CAMERA_PARM_EFFECT, sizeof(mColorEffects),
                                (void *)&mColorEffects,(int *)&result);
               if(result != MM_CAMERA_OK) {
                  ALOGI("Camera Effect is not set as the EFFECT_NONE and result is not OK");
               }
            }
            mParameters.set(QCameraParameters::KEY_SCENE_MODE, str);
            bool ret = native_set_parms(MM_CAMERA_PARM_BESTSHOT_MODE, sizeof(value),
                                       (void *)&value);
            int bestshot_reconfigure;
            mCameraHandle->ops->get_parm(mCameraHandle->camera_handle,
                                         MM_CAMERA_PARM_BESTSHOT_RECONFIGURE,
                                &bestshot_reconfigure);
            if(bestshot_reconfigure) {
                if (mBestShotMode != value) {
                     mBestShotMode = value;
                     if (mPreviewState == QCAMERA_HAL_PREVIEW_STARTED && ret) {
                           mRestartPreview = true;
                      }
                 }
            }
            return ret ? NO_ERROR : UNKNOWN_ERROR;
        }
    }
    ALOGE("Invalid scenemode value: %s", (str == NULL) ? "NULL" : str);
    return BAD_VALUE;
}

status_t QCameraHardwareInterface::setSelectableZoneAf(const QCameraParameters& params)
{
    ALOGV("%s",__func__);
    uint8_t supported;
    if(mHasAutoFocusSupport) {
        const char *str = params.get(QCameraParameters::KEY_QC_SELECTABLE_ZONE_AF);
        if (str != NULL) {
            int32_t value = attr_lookup(selectable_zone_af, sizeof(selectable_zone_af) / sizeof(str_map), str);
            if (value != NOT_FOUND) {
                 mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                                       MM_CAMERA_PARM_FOCUS_RECT,&supported,&supported);
                 if(!supported) {
                    ALOGE("SelectableZoneAF  is not supported for this sensor");
                    return NO_ERROR;
                 }else {
                    mParameters.set(QCameraParameters::KEY_QC_SELECTABLE_ZONE_AF, str);
                    bool ret = native_set_parms(MM_CAMERA_PARM_FOCUS_RECT, sizeof(value),
                            (void *)&value);
                    return ret ? NO_ERROR : UNKNOWN_ERROR;
                 }
            }
        }
        ALOGE("Invalid selectable zone af value: %s", (str == NULL) ? "NULL" : str);
        return BAD_VALUE;

    }
    return NO_ERROR;
}

status_t QCameraHardwareInterface::setEffect(const QCameraParameters& params)
{
    ALOGV("%s",__func__);
    uint8_t supported;
    const char *str = params.get(CameraParameters::KEY_EFFECT);
    int result;
    mColorEffects = CAMERA_EFFECT_OFF;
    if (str != NULL) {
        ALOGE("Setting effect %s",str);
        int32_t value = attr_lookup(effects, sizeof(effects) / sizeof(str_map), str);
        if (value != NOT_FOUND) {
            mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                                  MM_CAMERA_PARM_EFFECT,&supported,&supported);
           if(!supported) {
               ALOGE("Camera Effect - %s mode is not supported for this sensor",str);
               return NO_ERROR;
           }else {
               mParameters.set(QCameraParameters::KEY_EFFECT, str);
               ALOGE("Setting effect to lower HAL : %d",value);
               mColorEffects = value;
               bool ret = native_set_parms(MM_CAMERA_PARM_EFFECT, sizeof(value),
                                           (void *)&value,(int *)&result);
                if(result != 0) {
                    ALOGI("Camera Effect: %s is not set as the selected value is not supported ", str);
                }
                int bestshot_reconfigure;
                mCameraHandle->ops->get_parm(mCameraHandle->camera_handle,
                                             MM_CAMERA_PARM_BESTSHOT_RECONFIGURE,
                                    &bestshot_reconfigure);
                if(bestshot_reconfigure) {
                     if (mEffects != value) {
                         mEffects = value;
                         if (mPreviewState == QCAMERA_HAL_PREVIEW_STARTED && ret) {
                               mRestartPreview = true;
                          }
                   }
               }
               return ret ? NO_ERROR : UNKNOWN_ERROR;
          }
        }
    }
    ALOGE("Invalid effect value: %s", (str == NULL) ? "NULL" : str);
    ALOGV("setEffect X");
    return BAD_VALUE;
}

status_t QCameraHardwareInterface::setBrightness(const QCameraParameters& params) {

    ALOGV("%s",__func__);
    uint8_t supported;
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_BRIGHTNESS,&supported,&supported);
   if(!supported) {
       ALOGE("MM_CAMERA_PARM_BRIGHTNESS mode is not supported for this sensor");
       return NO_ERROR;
   }
   int brightness = params.getInt("luma-adaptation");
   if (mBrightness !=  brightness) {
       ALOGV(" new brightness value : %d ", brightness);
       mBrightness =  brightness;
       mParameters.set("luma-adaptation", brightness);
       bool ret = native_set_parms(MM_CAMERA_PARM_BRIGHTNESS, sizeof(mBrightness),
                                   (void *)&mBrightness);
        return ret ? NO_ERROR : UNKNOWN_ERROR;
   }

    return NO_ERROR;
}

status_t QCameraHardwareInterface::setAutoExposure(const QCameraParameters& params)
{

    ALOGV("%s",__func__);
    uint8_t supported;
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_EXPOSURE,&supported,&supported);
   if(!supported) {
       ALOGE("MM_CAMERA_PARM_EXPOSURE mode is not supported for this sensor");
       return NO_ERROR;
   }
   const char *str = params.get(QCameraParameters::KEY_QC_AUTO_EXPOSURE);
    if (str != NULL) {
        int32_t value = attr_lookup(autoexposure, sizeof(autoexposure) / sizeof(str_map), str);
        if (value != NOT_FOUND) {
            mParameters.set(QCameraParameters::KEY_QC_AUTO_EXPOSURE, str);
            bool ret = native_set_parms(MM_CAMERA_PARM_EXPOSURE, sizeof(value),
                                       (void *)&value);
            return ret ? NO_ERROR : UNKNOWN_ERROR;
        }
    }
    ALOGE("Invalid auto exposure value: %s", (str == NULL) ? "NULL" : str);
    return BAD_VALUE;
}

status_t QCameraHardwareInterface::setExposureCompensation(
        const QCameraParameters & params){
    ALOGV("%s",__func__);
    uint8_t supported;
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_EXPOSURE_COMPENSATION,&supported,&supported);
    if(!supported) {
       ALOGE("MM_CAMERA_PARM_EXPOSURE_COMPENSATION mode is not supported for this sensor");
       return NO_ERROR;
    }
    int numerator = params.getInt(QCameraParameters::KEY_EXPOSURE_COMPENSATION);
    if(EXPOSURE_COMPENSATION_MINIMUM_NUMERATOR <= numerator &&
            numerator <= EXPOSURE_COMPENSATION_MAXIMUM_NUMERATOR){
        int16_t  numerator16 = (int16_t)(numerator & 0x0000ffff);
        uint16_t denominator16 = EXPOSURE_COMPENSATION_DENOMINATOR;
        uint32_t  value = 0;
        value = numerator16 << 16 | denominator16;

        mParameters.set(QCameraParameters::KEY_EXPOSURE_COMPENSATION,
                            numerator);
       bool ret = native_set_parms(MM_CAMERA_PARM_EXPOSURE_COMPENSATION,
                                    sizeof(value), (void *)&value);
        return ret ? NO_ERROR : UNKNOWN_ERROR;
    }
    ALOGE("Invalid Exposure Compensation");
    return BAD_VALUE;
}

status_t QCameraHardwareInterface::setWhiteBalance(const QCameraParameters& params)
{

    ALOGV("%s",__func__);
    status_t rc = NO_ERROR;
    uint8_t supported;
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_WHITE_BALANCE,&supported,&supported);
    if(!supported) {
       ALOGE("MM_CAMERA_PARM_WHITE_BALANCE mode is not supported for this sensor");
       return NO_ERROR;
    }
     int result;

    const char *str = params.get(QCameraParameters::KEY_WHITE_BALANCE);
    if (str != NULL) {
        int32_t value = attr_lookup(whitebalance, sizeof(whitebalance) / sizeof(str_map), str);
        if (value != NOT_FOUND) {
            mParameters.set(QCameraParameters::KEY_WHITE_BALANCE, str);
            bool ret = native_set_parms(MM_CAMERA_PARM_WHITE_BALANCE, sizeof(value),
                                       (void *)&value, (int *)&result);
            if(result != MM_CAMERA_OK) {
                ALOGI("WhiteBalance Value: %s is not set as the selected value is not supported ", str);
            }
            return ret ? NO_ERROR : UNKNOWN_ERROR;
        }
    }
    ALOGE("Invalid whitebalance value: %s", (str == NULL) ? "NULL" : str);
    return BAD_VALUE;
}

int QCameraHardwareInterface::getAutoFlickerMode()
{
    /* Enable Advanced Auto Antibanding where we can set
       any of the following option
       ie. CAMERA_ANTIBANDING_AUTO
           CAMERA_ANTIBANDING_AUTO_50HZ
           CAMERA_ANTIBANDING_AUTO_60HZ
      Currently setting it to default    */
    return CAMERA_ANTIBANDING_AUTO;
}

status_t QCameraHardwareInterface::setAntibanding(const QCameraParameters& params)
{
    int result;

    ALOGV("%s",__func__);
    status_t rc = NO_ERROR;
    uint8_t supported;
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_ANTIBANDING,&supported,&supported);
    if(!supported) {
       ALOGE("ANTIBANDING mode is not supported for this sensor");
       return NO_ERROR;
    }
    const char *str = params.get(QCameraParameters::KEY_ANTIBANDING);
    if (str != NULL) {
        int value = (camera_antibanding_type)attr_lookup(
          antibanding, sizeof(antibanding) / sizeof(str_map), str);
        if (value != NOT_FOUND) {
            camera_antibanding_type temp = (camera_antibanding_type) value;
            ALOGE("Antibanding Value : %d",value);
            mParameters.set(QCameraParameters::KEY_ANTIBANDING, str);
            if(value == CAMERA_ANTIBANDING_AUTO) {
                 value = getAutoFlickerMode();
            }
            bool ret = native_set_parms(MM_CAMERA_PARM_ANTIBANDING,
                       sizeof(camera_antibanding_type), (void *)&value ,(int *)&result);
            if(result != MM_CAMERA_OK) {
                ALOGI("AntiBanding Value: %s is not supported for the given BestShot Mode", str);
            }
            return ret ? NO_ERROR : UNKNOWN_ERROR;
        }
    }
    ALOGE("Invalid antibanding value: %s", (str == NULL) ? "NULL" : str);

    return BAD_VALUE;
}

status_t QCameraHardwareInterface::setPreviewFrameRate(const QCameraParameters& params)
{
    ALOGV("%s: E",__func__);
    status_t rc = NO_ERROR;
    uint16_t fps = (uint16_t)params.getPreviewFrameRate();
    ALOGV("%s: requested preview frame rate  is %d", __func__, fps);

    mParameters.setPreviewFrameRate(fps);
    ALOGV("%s: X",__func__);
    return NO_ERROR;
}

status_t QCameraHardwareInterface::setPreviewFrameRateMode(const QCameraParameters& params) {

    ALOGV("%s",__func__);
    uint8_t supported;
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_FPS,&supported,&supported);
    if(!supported) {
       ALOGE(" CAMERA FPS mode is not supported for this sensor");
       return NO_ERROR;
    }
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_FPS_MODE,&supported,&supported);
    if(!supported) {
       ALOGE("CAMERA FPS MODE mode is not supported for this sensor");
       return NO_ERROR;
    }

    const char *previousMode = mParameters.getPreviewFrameRateMode();
    const char *str = params.getPreviewFrameRateMode();
    if (NULL == previousMode) {
        ALOGE("Preview Frame Rate Mode is NULL\n");
        return NO_ERROR;
    }
    if (NULL == str) {
        ALOGE("Preview Frame Rate Mode is NULL\n");
        return NO_ERROR;
    }
    if( mInitialized && !strcmp(previousMode, str)) {
        ALOGE("frame rate mode same as previous mode %s", previousMode);
        return NO_ERROR;
    }
    int32_t frameRateMode = attr_lookup(frame_rate_modes, sizeof(frame_rate_modes) / sizeof(str_map),str);
    if(frameRateMode != NOT_FOUND) {
        ALOGV("setPreviewFrameRateMode: %s ", str);
        mParameters.setPreviewFrameRateMode(str);
        bool ret = native_set_parms(MM_CAMERA_PARM_FPS_MODE, sizeof(frameRateMode), (void *)&frameRateMode);
        if(!ret) return ret;
        //set the fps value when chaging modes
        int16_t fps = (uint16_t)params.getPreviewFrameRate();
        if(MINIMUM_FPS <= fps && fps <=MAXIMUM_FPS){
            mParameters.setPreviewFrameRate(fps);
            ret = native_set_parms(MM_CAMERA_PARM_FPS,
                                        sizeof(fps), (void *)&fps);
            return ret ? NO_ERROR : UNKNOWN_ERROR;
        }
        ALOGE("Invalid preview frame rate value: %d", fps);
        return BAD_VALUE;
    }
    ALOGE("Invalid preview frame rate mode value: %s", (str == NULL) ? "NULL" : str);

    return BAD_VALUE;
}

status_t QCameraHardwareInterface::setSkinToneEnhancement(const QCameraParameters& params) {
    ALOGV("%s",__func__);
    uint8_t supported;
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_SCE_FACTOR,&supported,&supported);
    if(!supported) {
       ALOGE("SkinToneEnhancement is not supported for this sensor");
       return NO_ERROR;
    }
     int skinToneValue = params.getInt("skinToneEnhancement");
     if (mSkinToneEnhancement != skinToneValue) {
          ALOGV(" new skinTone correction value : %d ", skinToneValue);
          mSkinToneEnhancement = skinToneValue;
          mParameters.set("skinToneEnhancement", skinToneValue);
          bool ret = native_set_parms(MM_CAMERA_PARM_SCE_FACTOR, sizeof(mSkinToneEnhancement),
                        (void *)&mSkinToneEnhancement);
          return ret ? NO_ERROR : UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

status_t QCameraHardwareInterface::setWaveletDenoise(const QCameraParameters& params) {
    ALOGV("%s",__func__);
    uint8_t supported;
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_WAVELET_DENOISE,&supported,&supported);
    if( !supported ) {
        ALOGE("Wavelet Denoise is not supported for this sensor");
        /* TO DO */
//        return NO_ERROR;
    }
    const char *str = params.get(QCameraParameters::KEY_QC_DENOISE);
    if (str != NULL) {
        int value = attr_lookup(denoise,
                sizeof(denoise) / sizeof(str_map), str);
        if ((value != NOT_FOUND) &&  (mDenoiseValue != value)) {
            mDenoiseValue =  value;
            mParameters.set(QCameraParameters::KEY_QC_DENOISE, str);

            char prop[PROPERTY_VALUE_MAX];
            memset(prop, 0, sizeof(prop));
            property_get("persist.denoise.process.plates", prop, "0");

            denoise_param_t temp;
            memset(&temp, 0, sizeof(denoise_param_t));
            temp.denoise_enable = value;
            switch(atoi(prop)) {
                case 0:
                    temp.process_plates = WAVELET_DENOISE_YCBCR_PLANE;
                    break;
                case 1:
                    temp.process_plates = WAVELET_DENOISE_CBCR_ONLY;
                    break;
                case 2:
                    temp.process_plates = WAVELET_DENOISE_STREAMLINE_YCBCR;
                    break;
                case 3:
                    temp.process_plates = WAVELET_DENOISE_STREAMLINED_CBCR;
                    break;
                default:
                    temp.process_plates = WAVELET_DENOISE_STREAMLINE_YCBCR;
                    break;
                }
            ALOGE("Denoise enable=%d, plates=%d", temp.denoise_enable, temp.process_plates);
            bool ret = native_set_parms(MM_CAMERA_PARM_WAVELET_DENOISE, sizeof(temp),
                    (void *)&temp);
            return ret ? NO_ERROR : UNKNOWN_ERROR;
        }
        return NO_ERROR;
    }
    ALOGE("Invalid Denoise value: %s", (str == NULL) ? "NULL" : str);
    return BAD_VALUE;
}

status_t QCameraHardwareInterface::setVideoSize(const QCameraParameters& params)
{
    const char *str= NULL;
    const char *str_t= NULL;
    int old_vid_w = 0, old_vid_h = 0;
    ALOGV("%s: E", __func__);
    str = params.get(QCameraParameters::KEY_VIDEO_SIZE);
    str_t = mParameters.get(CameraParameters::KEY_VIDEO_SIZE);
    if(!str) {
        mParameters.set(QCameraParameters::KEY_VIDEO_SIZE, "");
        //If application didn't set this parameter string, use the values from
        //getPreviewSize() as video dimensions.
        ALOGE("No Record Size requested, use the preview dimensions");
        videoWidth = mPreviewWidth;
        videoHeight = mPreviewHeight;
    } else {
        //Extract the record witdh and height that application requested.
        ALOGI("%s: requested record size %s", __func__, str);
        if(!parse_size(str, videoWidth, videoHeight)) {
            parse_size(str_t, old_vid_w, old_vid_h);
            mParameters.set(QCameraParameters::KEY_VIDEO_SIZE, str);
            if((QCAMERA_HAL_PREVIEW_STARTED == mPreviewState) &&
               (old_vid_w != videoWidth || old_vid_h != videoHeight)) {
                ALOGE("%s: Video sizes changes to %s, Restart preview...", __func__, str);
                mRestartPreview = true;
            }
        } else {
            mParameters.set(QCameraParameters::KEY_VIDEO_SIZE, "");
            ALOGE("%s: error :failed to parse parameter record-size (%s)", __func__, str);
            return BAD_VALUE;
        }
    }
    ALOGV("%s: preview dimensions: %dx%d", __func__, mPreviewWidth, mPreviewHeight);
    ALOGV("%s: video dimensions: %dx%d", __func__, videoWidth, videoHeight);
    mDimension.orig_video_width = videoWidth;
    mDimension.orig_video_height = videoHeight;
    mDimension.video_width = videoWidth;
    mDimension.video_height = videoHeight;

    ALOGV("%s: X", __func__);
    return NO_ERROR;
}

status_t QCameraHardwareInterface::setCameraMode(const QCameraParameters& params) {
    int32_t value = params.getInt(QCameraParameters::KEY_QC_CAMERA_MODE);
    mParameters.set(QCameraParameters::KEY_QC_CAMERA_MODE,value);

    ALOGI("ZSL is enabled  %d", value);
    if (value == 1) {
        myMode = (camera_mode_t)(myMode | CAMERA_ZSL_MODE);
    } else {
        myMode = (camera_mode_t)(myMode & ~CAMERA_ZSL_MODE);
    }
    return NO_ERROR;
}

status_t QCameraHardwareInterface::setPowerMode(const QCameraParameters& params) {
    uint32_t value = NORMAL_POWER;
    const char *powermode = NULL;

    powermode = params.get(QCameraParameters::KEY_QC_POWER_MODE);
    if (powermode != NULL) {
        value = attr_lookup(power_modes,
                sizeof(power_modes) / sizeof(str_map), powermode);
        if((value == LOW_POWER) || mHFRLevel > 1) {
            ALOGI("Enable Low Power Mode");
            value = LOW_POWER;
            mPowerMode = value;
            mParameters.set(QCameraParameters::KEY_QC_POWER_MODE,"Low_Power");
        } else {
            ALOGE("Enable Normal Power Mode");
            mPowerMode = value;
            mParameters.set(QCameraParameters::KEY_QC_POWER_MODE,"Normal_Power");
        }
    }

    ALOGI("%s Low power mode %s value = %d", __func__,
          value ? "Enabled" : "Disabled", value);
    native_set_parms(MM_CAMERA_PARM_LOW_POWER_MODE, sizeof(value),
                                               (void *)&value);
    return NO_ERROR;
}


status_t QCameraHardwareInterface::setPreviewSize(const QCameraParameters& params)
{
    int width, height;
    params.getPreviewSize(&width, &height);
    ALOGV("################requested preview size %d x %d", width, height);

    // Validate the preview size
    for (size_t i = 0; i <  mPreviewSizeCount; ++i) {
        if (width ==  mPreviewSizes[i].width
           && height ==  mPreviewSizes[i].height) {
            mParameters.setPreviewSize(width, height);
            ALOGE("setPreviewSize:  width: %d   heigh: %d", width, height);
            mPreviewWidth = width;
            mPreviewHeight = height;
            mDimension.display_width = width;
            mDimension.display_height = height;
            return NO_ERROR;
        }
    }
    ALOGE("Invalid preview size requested: %dx%d", width, height);
    return BAD_VALUE;
}
status_t QCameraHardwareInterface::setPreviewFpsRange(const QCameraParameters& params)
{
    ALOGV("%s: E", __func__);
    int minFps,maxFps;
    int prevMinFps, prevMaxFps;
    int rc = NO_ERROR;
    bool found = false;

    mParameters.getPreviewFpsRange(&prevMinFps, &prevMaxFps);
    ALOGV("%s: Existing FpsRange Values:(%d, %d)", __func__, prevMinFps, prevMaxFps);
    params.getPreviewFpsRange(&minFps,&maxFps);
    ALOGV("%s: Requested FpsRange Values:(%d, %d)", __func__, minFps, maxFps);

    if(mInitialized && (minFps == prevMinFps && maxFps == prevMaxFps)) {
        ALOGE("%s: No change in FpsRange", __func__);
        rc = NO_ERROR;
        goto end;
    }
    for(int i=0; i<mSupportedFpsRangesCount; i++) {
        // if the value is in the supported list
        if(minFps >= mSupportedFpsRanges[i].minFPS && maxFps <= mSupportedFpsRanges[i].maxFPS) {
            found = true;
            ALOGE("FPS: i=%d : minFps = %d, maxFps = %d ", i, minFps, maxFps);
            mParameters.setPreviewFpsRange(minFps,maxFps);
            // validate the values
            bool valid = true;
            // FPS can not be negative
            if(minFps < 0 || maxFps < 0) valid = false;
            // minFps must be >= maxFps
            if(minFps > maxFps) valid = false;

            if(valid) {
                //Set the FPS mode
                const char *str = (minFps == maxFps) ?
                    QCameraParameters::KEY_QC_PREVIEW_FRAME_RATE_FIXED_MODE:
                    QCameraParameters::KEY_QC_PREVIEW_FRAME_RATE_AUTO_MODE;
                ALOGE("%s FPS_MODE = %s", __func__, str);
                int32_t frameRateMode = attr_lookup(frame_rate_modes,
                        sizeof(frame_rate_modes) / sizeof(str_map),str);
                bool ret;
                ret = native_set_parms(MM_CAMERA_PARM_FPS_MODE, sizeof(int32_t),
                            (void *)&frameRateMode);

                //set FPS values
                uint32_t fps;  //lower 2 bytes specify maxFps and higher 2 bytes specify minFps
                fps = ((uint32_t)(minFps/1000) << 16) + ((uint16_t)(maxFps/1000));

                ret = native_set_parms(MM_CAMERA_PARM_FPS, sizeof(uint32_t), (void *)&fps);
                mParameters.setPreviewFpsRange(minFps, maxFps);
                if(ret)
                    rc = NO_ERROR;
                else {
                    rc = BAD_VALUE;
                    ALOGE("%s: error: native_set_params failed", __func__);
                }
            } else {
                ALOGE("%s: error: invalid FPS range value", __func__);
                rc = BAD_VALUE;
            }
        }
    }
    if(found == false){
            ALOGE("%s: error: FPS range value not supported", __func__);
            rc = BAD_VALUE;
    }
end:
    ALOGV("%s: X", __func__);
    return rc;
}

status_t QCameraHardwareInterface::setJpegThumbnailSize(const QCameraParameters& params){
    int width = params.getInt(QCameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
    int height = params.getInt(QCameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);

    ALOGV("requested jpeg thumbnail size %d x %d", width, height);

    // Validate the picture size
    for (unsigned int i = 0; i < thumbnail_sizes_count; ++i) {
       if (width == default_thumbnail_sizes[i].width
         && height == default_thumbnail_sizes[i].height) {
           thumbnailWidth = width;
           thumbnailHeight = height;
           mParameters.set(QCameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, width);
           mParameters.set(QCameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, height);
           return NO_ERROR;
       }
    }
    ALOGE("error: setting jpeg thumbnail size");
    return BAD_VALUE;
}
status_t QCameraHardwareInterface::setPictureSize(const QCameraParameters& params)
{
    int width, height;
    ALOGV("QualcommCameraHardware::setPictureSize E");
    params.getPictureSize(&width, &height);
    ALOGE("requested picture size %d x %d", width, height);

    // Validate the picture size
    for (int i = 0; i < mSupportedPictureSizesCount; ++i) {
        if (width == mPictureSizesPtr[i].width
          && height == mPictureSizesPtr[i].height) {
            int old_width, old_height;
            mParameters.getPictureSize(&old_width,&old_height);
            mParameters.setPictureSize(width, height);
            mDimension.picture_width = width;
            mDimension.picture_height = height;
            if((QCAMERA_HAL_PREVIEW_STARTED == mPreviewState) &&
               (width != old_width || height != old_height)) {
                mRestartPreview = true;
            }
            return NO_ERROR;
        }
    }
    /* Dimension not among the ones in the list. Check if
     * its a valid dimension, if it is, then configure the
     * camera accordingly. else reject it.
     */
    if( isValidDimension(width, height) ) {
        mParameters.setPictureSize(width, height);
        mDimension.picture_width = width;
        mDimension.picture_height = height;
        return NO_ERROR;
    } else
        ALOGE("Invalid picture size requested: %dx%d", width, height);
    return BAD_VALUE;
}

status_t QCameraHardwareInterface::setJpegRotation(int isZsl) {
//    return mm_jpeg_encoder_setRotation(mRotation, isZsl);
      return NO_ERROR;
}

int QCameraHardwareInterface::getJpegRotation(void) {
    ALOGE("%s : rotation is %d", __func__, mRotation);
    return mRotation;
}

int QCameraHardwareInterface::getISOSpeedValue()
{
    const char *iso_str = mParameters.get(QCameraParameters::KEY_QC_ISO_MODE);
    int iso_index = attr_lookup(iso, sizeof(iso) / sizeof(str_map), iso_str);
    int iso_value = iso_speed_values[iso_index];
    return iso_value;
}


status_t QCameraHardwareInterface::setJpegQuality(const QCameraParameters& params) {
    status_t rc = NO_ERROR;
    int quality = params.getInt(QCameraParameters::KEY_JPEG_QUALITY);
    ALOGV("setJpegQuality E");
    if (quality >= 0 && quality <= 100) {
        mParameters.set(QCameraParameters::KEY_JPEG_QUALITY, quality);
        mJpegQuality = quality;
    } else {
        ALOGE("Invalid jpeg quality=%d", quality);
        rc = BAD_VALUE;
    }

    quality = params.getInt(QCameraParameters::KEY_JPEG_THUMBNAIL_QUALITY);
    if (quality >= 0 && quality <= 100) {
        mParameters.set(QCameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, quality);
    } else {
        ALOGE("Invalid jpeg thumbnail quality=%d", quality);
        rc = BAD_VALUE;
    }
    ALOGV("setJpegQuality X");
    return rc;
}

status_t QCameraHardwareInterface::
setNumOfSnapshot(const QCameraParameters& params) {
    status_t rc = NO_ERROR;

    int num_of_snapshot = getNumOfSnapshots(params);

    if (num_of_snapshot <= 0) {
        num_of_snapshot = 1;
    }
    ALOGI("number of snapshots = %d", num_of_snapshot);
    mParameters.set("num-snaps-per-shutter", num_of_snapshot);

    bool result = native_set_parms(MM_CAMERA_PARM_SNAPSHOT_BURST_NUM,
                                   sizeof(int),
                                   (void *)&num_of_snapshot);
    if(!result)
        ALOGI("%s:Failure setting number of snapshots!!!", __func__);
    return rc;
}

status_t QCameraHardwareInterface::setPreviewFormat(const QCameraParameters& params) {
    const char *str = params.getPreviewFormat();
    int32_t previewFormat = attr_lookup(preview_formats, sizeof(preview_formats) / sizeof(str_map), str);
    if(previewFormat != NOT_FOUND) {
        int num = sizeof(preview_format_info_list)/sizeof(preview_format_info_t);
        int i;

        for (i = 0; i < num; i++) {
          if (preview_format_info_list[i].Hal_format == previewFormat) {
            mPreviewFormatInfo = preview_format_info_list[i];
            break;
          }
        }

        if (i == num) {
          mPreviewFormatInfo.mm_cam_format = CAMERA_YUV_420_NV21;
          mPreviewFormatInfo.padding = CAMERA_PAD_TO_WORD;
          return BAD_VALUE;
        }
        bool ret = native_set_parms(MM_CAMERA_PARM_PREVIEW_FORMAT, sizeof(cam_format_t),
                                   (void *)&mPreviewFormatInfo.mm_cam_format);
        mParameters.set(QCameraParameters::KEY_PREVIEW_FORMAT, str);
        mPreviewFormat = mPreviewFormatInfo.mm_cam_format;
        ALOGI("Setting preview format to %d, i =%d, num=%d, hal_format=%d",
             mPreviewFormat, i, num, mPreviewFormatInfo.Hal_format);
        return NO_ERROR;
    } else if ( strTexturesOn ) {
      mPreviewFormatInfo.mm_cam_format = CAMERA_YUV_420_NV21;
      mPreviewFormatInfo.padding = CAMERA_PAD_TO_4K;
    } else {
      mPreviewFormatInfo.mm_cam_format = CAMERA_YUV_420_NV21;
      mPreviewFormatInfo.padding = CAMERA_PAD_TO_WORD;
    }
    ALOGE("Invalid preview format value: %s", (str == NULL) ? "NULL" : str);
    return BAD_VALUE;
}

status_t QCameraHardwareInterface::setStrTextures(const QCameraParameters& params) {
    const char *str = params.get("strtextures");
    const char *prev_str = mParameters.get("strtextures");

    if(str != NULL) {
        if(!strcmp(str,prev_str)) {
            return NO_ERROR;
        }
        int str_size = strlen(str);
        mParameters.set("strtextures", str);
        if(str_size == 2) {
            if(!strncmp(str, "on", str_size) || !strncmp(str, "ON", str_size)){
                ALOGI("Resetting mUseOverlay to false");
                strTexturesOn = true;
                mUseOverlay = false;
            }
        }else if(str_size == 3){
            if (!strncmp(str, "off", str_size) || !strncmp(str, "OFF", str_size)) {
                strTexturesOn = false;
                mUseOverlay = true;
            }
        }

    }
    return NO_ERROR;
}

status_t QCameraHardwareInterface::setFlash(const QCameraParameters& params)
{
    ALOGI("%s: E",__func__);
    uint8_t supported;
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_LED_MODE,&supported,&supported);
    if(!supported) {
        ALOGE("%s:LED FLASH not supported", __func__);
        return NO_ERROR;
    }

    const char *str = params.get(QCameraParameters::KEY_FLASH_MODE);
    if (str != NULL) {
        int32_t value = attr_lookup(flash, sizeof(flash) / sizeof(str_map), str);
        if (value != NOT_FOUND) {
            mParameters.set(QCameraParameters::KEY_FLASH_MODE, str);
            bool ret = native_set_parms(MM_CAMERA_PARM_LED_MODE,
                                       sizeof(value), (void *)&value);
            return ret ? NO_ERROR : UNKNOWN_ERROR;
        }
    }
    ALOGE("Invalid flash mode value: %s", (str == NULL) ? "NULL" : str);

    return BAD_VALUE;
}

status_t QCameraHardwareInterface::setAecAwbLock(const QCameraParameters & params)
{
    ALOGD("%s : E", __func__);
    status_t rc = NO_ERROR;
    int32_t value;
    const char* str;

    //for AEC lock
    str = params.get(QCameraParameters::KEY_AUTO_EXPOSURE_LOCK);
    value = (strcmp(str, "true") == 0)? 1 : 0;
    mParameters.set(QCameraParameters::KEY_AUTO_EXPOSURE_LOCK, str);
    rc = (native_set_parms(MM_CAMERA_PARM_AEC_LOCK, sizeof(int32_t), (void *)(&value))) ?
                            NO_ERROR : UNKNOWN_ERROR;

    //for AWB lock
    str = params.get(QCameraParameters::KEY_AUTO_WHITEBALANCE_LOCK);
    value = (strcmp(str, "true") == 0)? 1 : 0;
    mParameters.set(QCameraParameters::KEY_AUTO_WHITEBALANCE_LOCK, str);
    rc = (native_set_parms(MM_CAMERA_PARM_AWB_LOCK, sizeof(int32_t), (void *)(&value))) ?
                        NO_ERROR : UNKNOWN_ERROR;
    ALOGD("%s : X", __func__);
    return rc;
}

status_t QCameraHardwareInterface::setOverlayFormats(const QCameraParameters& params)
{
    mParameters.set("overlay-format", HAL_PIXEL_FORMAT_YCbCr_420_SP);
    if(mIs3DModeOn == true) {
       int ovFormat = HAL_PIXEL_FORMAT_YCrCb_420_SP|HAL_3D_IN_SIDE_BY_SIDE_L_R|HAL_3D_OUT_SIDE_BY_SIDE;
        mParameters.set("overlay-format", ovFormat);
    }
    return NO_ERROR;
}

status_t QCameraHardwareInterface::setMCEValue(const QCameraParameters& params)
{
    ALOGE("%s",__func__);
    uint8_t supported;

    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_MCE,&supported,&supported);
   if(!supported) {
       ALOGE("MM_CAMERA_PARM_MCE mode is not supported for this sensor");
       return NO_ERROR;
   }
   const char *str = params.get(QCameraParameters::KEY_QC_MEMORY_COLOR_ENHANCEMENT);
    if (str != NULL) {
        int value = attr_lookup(mce, sizeof(mce) / sizeof(str_map), str);
        if (value != NOT_FOUND) {
            int temp = (int8_t)value;
            ALOGI("%s: setting MCE value of %s", __FUNCTION__, str);
            mParameters.set(QCameraParameters::KEY_QC_MEMORY_COLOR_ENHANCEMENT, str);

            native_set_parms(MM_CAMERA_PARM_MCE, sizeof(int8_t), (void *)&temp);
            return NO_ERROR;
        }
    }
    ALOGE("Invalid MCE value: %s", (str == NULL) ? "NULL" : str);

    return NO_ERROR;
}

status_t QCameraHardwareInterface::setHighFrameRate(const QCameraParameters& params)
{
    uint8_t supported;
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_HFR,&supported,&supported);
    if(!supported) {
        ALOGE("%s: MM_CAMERA_PARM_HFR not supported", __func__);
        return NO_ERROR;
    }

    const char *str = params.get(QCameraParameters::KEY_QC_VIDEO_HIGH_FRAME_RATE);
    if (str != NULL) {
        int value = attr_lookup(hfr, sizeof(hfr) / sizeof(str_map), str);
        if (value != NOT_FOUND) {
            mHFRLevel = (int32_t)value;
            //Check for change in HFR value
            const char *oldHfr = mParameters.get(QCameraParameters::KEY_QC_VIDEO_HIGH_FRAME_RATE);
            if(strcmp(oldHfr, str)){
                mParameters.set(QCameraParameters::KEY_QC_VIDEO_HIGH_FRAME_RATE, str);
                if(QCAMERA_HAL_PREVIEW_STARTED == mPreviewState) {
                    stopPreviewInternal();
                    mPreviewState = QCAMERA_HAL_PREVIEW_STOPPED;
                    native_set_parms(MM_CAMERA_PARM_HFR, sizeof(int32_t), (void *)&mHFRLevel);
                    mRestartPreview = true;
                    return NO_ERROR;
                }
            }
            native_set_parms(MM_CAMERA_PARM_HFR, sizeof(int32_t), (void *)&mHFRLevel);
            return NO_ERROR;
        }
    }
    ALOGE("Invalid HFR value: %s", (str == NULL) ? "NULL" : str);
    return NO_ERROR;
}

status_t QCameraHardwareInterface::setLensshadeValue(const QCameraParameters& params)
{

    uint8_t supported;
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_ROLLOFF,&supported,&supported);
    if(!supported) {
        ALOGE("%s:LENS SHADING not supported", __func__);
        return NO_ERROR;
    }

    const char *str = params.get(QCameraParameters::KEY_QC_LENSSHADE);
    if (str != NULL) {
        int value = attr_lookup(lensshade,
                                    sizeof(lensshade) / sizeof(str_map), str);
        if (value != NOT_FOUND) {
            int8_t temp = (int8_t)value;
            mParameters.set(QCameraParameters::KEY_QC_LENSSHADE, str);
            native_set_parms(MM_CAMERA_PARM_ROLLOFF, sizeof(int8_t), (void *)&temp);
            return NO_ERROR;
        }
    }
    ALOGE("Invalid lensShade value: %s", (str == NULL) ? "NULL" : str);
    return BAD_VALUE;
}

status_t QCameraHardwareInterface::setFaceDetect(const QCameraParameters& params)
{
    int requested_faces = params.getInt(QCameraParameters::KEY_QC_MAX_NUM_REQUESTED_FACES);
    int hardware_supported_faces = mParameters.getInt(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW);
    if (requested_faces > hardware_supported_faces) {
        requested_faces = hardware_supported_faces;
    }
    mParameters.set(QCameraParameters::KEY_QC_MAX_NUM_REQUESTED_FACES, requested_faces);
    const char *str = params.get(QCameraParameters::KEY_QC_FACE_DETECTION);
    ALOGE("setFaceDetect: %s", str);
    if (str != NULL) {
        fd_set_parm_t fd_set_parm;
        int value = attr_lookup(facedetection,
                sizeof(facedetection) / sizeof(str_map), str);
        mFaceDetectOn = value;
        fd_set_parm.fd_mode = value;
        fd_set_parm.num_fd = requested_faces;
        ALOGE("%s Face detection value = %d, num_fd = %d",__func__, value, requested_faces);
        native_set_parms(MM_CAMERA_PARM_FD, sizeof(fd_set_parm_t), (void *)&fd_set_parm);
        mParameters.set(QCameraParameters::KEY_QC_FACE_DETECTION, str);
        return NO_ERROR;
    }
    ALOGE("Invalid Face Detection value: %s", (str == NULL) ? "NULL" : str);
    return BAD_VALUE;
}
status_t QCameraHardwareInterface::setFaceDetection(const char *str)
{
    if(supportsFaceDetection() == false){
        ALOGE("Face detection is not enabled");
        return NO_ERROR;
    }
    if (str != NULL) {
        int requested_faces = mParameters.getInt(QCameraParameters::KEY_QC_MAX_NUM_REQUESTED_FACES);
        int value = attr_lookup(facedetection,
                                    sizeof(facedetection) / sizeof(str_map), str);
        if (value != NOT_FOUND) {
            fd_set_parm_t fd_set_parm;
            mFaceDetectOn = value;
            fd_set_parm.fd_mode = value;
            fd_set_parm.num_fd = requested_faces;
            ALOGE("%s Face detection value = %d, num_fd = %d",__func__, value, requested_faces);
            native_set_parms(MM_CAMERA_PARM_FD, sizeof(fd_set_parm_t), (void *)&fd_set_parm);
            mParameters.set(QCameraParameters::KEY_QC_FACE_DETECTION, str);
            return NO_ERROR;
        }
    }
    ALOGE("Invalid Face Detection value: %s", (str == NULL) ? "NULL" : str);
    return BAD_VALUE;
}

status_t QCameraHardwareInterface::setAEBracket(const QCameraParameters& params)
{
    uint8_t supported;
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_HDR,&supported,&supported);
    if(!supported || (myMode & CAMERA_ZSL_MODE)) {
        ALOGI("Parameter HDR is not supported for this sensor/ ZSL mode");

        if (myMode & CAMERA_ZSL_MODE) {
            ALOGE("In ZSL mode, reset AEBBracket to HDR_OFF mode");
            exp_bracketing_t temp;
            memset(&temp, 0, sizeof(temp));
            mHdrMode = HDR_BRACKETING_OFF;
            temp.hdr_enable= FALSE;
            temp.mode = HDR_BRACKETING_OFF;
            native_set_parms(MM_CAMERA_PARM_HDR, sizeof(exp_bracketing_t), (void *)&temp);
        }
        return NO_ERROR;
    }
    const char *str = params.get(QCameraParameters::KEY_QC_AE_BRACKET_HDR);

    if (str != NULL) {
        int value = attr_lookup(hdr_bracket,
                                    sizeof(hdr_bracket) / sizeof(str_map), str);
        exp_bracketing_t temp;
        memset(&temp, 0, sizeof(temp));
        switch (value) {
            case HDR_MODE:
                {
                    mHdrMode = HDR_MODE;
                    ALOGE("%s, mHdrMode == HDR_MODE", __func__);
                  /*  temp.hdr_enable= TRUE;
                    temp.mode = HDR_MODE;
                    temp.total_frames = 3;
                    temp.total_hal_frames = getNumOfSnapshots();
                    ALOGI("%s: setting HDR frames (%d)", __FUNCTION__, temp.total_hal_frames);
                    native_set_parms(MM_CAMERA_PARM_HDR, sizeof(exp_bracketing_t), (void *)&temp); */
                }
                break;
            case EXP_BRACKETING_MODE:
                {
                    ALOGE("%s, mHdrMode == EXP_BRACKETING_MODE", __func__);
                    int numFrames = getNumOfSnapshots();
                    const char *str_val = params.get("capture-burst-exposures");
                    if ((str_val != NULL) && (strlen(str_val)>0)) {
                        ALOGI("%s: capture-burst-exposures %s", __FUNCTION__, str_val);

                        mHdrMode = EXP_BRACKETING_MODE;
                        temp.hdr_enable = FALSE;
                        temp.mode = EXP_BRACKETING_MODE;
                        temp.total_frames = (numFrames >  MAX_SNAPSHOT_BUFFERS -2) ? MAX_SNAPSHOT_BUFFERS -2 : numFrames;
                        temp.total_hal_frames = temp.total_frames;
                        strlcpy(temp.values, str_val, MAX_EXP_BRACKETING_LENGTH);
                        ALOGI("%s: setting Exposure Bracketing value of %s, frame (%d)", __FUNCTION__, temp.values, temp.total_hal_frames);
                        native_set_parms(MM_CAMERA_PARM_HDR, sizeof(exp_bracketing_t), (void *)&temp);
                    }
                    else {
                        /* Apps not set capture-burst-exposures, error case fall into bracketing off mode */
                        ALOGI("%s: capture-burst-exposures not set, back to HDR OFF mode", __FUNCTION__);
                        mHdrMode = HDR_BRACKETING_OFF;
                        temp.hdr_enable= FALSE;
                        temp.mode = HDR_BRACKETING_OFF;
                        native_set_parms(MM_CAMERA_PARM_HDR, sizeof(exp_bracketing_t), (void *)&temp);
                    }
                }
                break;
            case HDR_BRACKETING_OFF:
                ALOGE("%s, mHdrMode == HDR_BRACKETING_OFF", __func__);
            default:
                {
                    ALOGE("%s, mHdrMode == HDR_BRACKETING_OFF", __func__);
                    mHdrMode = HDR_BRACKETING_OFF;
                    temp.hdr_enable= FALSE;
                    temp.mode = HDR_BRACKETING_OFF;
                    native_set_parms(MM_CAMERA_PARM_HDR, sizeof(exp_bracketing_t), (void *)&temp);
                }
                break;
        }

        /* save the value*/
        mParameters.set(QCameraParameters::KEY_QC_AE_BRACKET_HDR, str);
    }
    return NO_ERROR;
}

status_t QCameraHardwareInterface::setCaptureBurstExp()
{
    char burst_exp[PROPERTY_VALUE_MAX];
    memset(burst_exp, 0, sizeof(burst_exp));
    property_get("persist.capture.burst.exposures", burst_exp, "");
    if (NULL != burst_exp)
      mParameters.set("capture-burst-exposures", burst_exp);
    return NO_ERROR;
}

status_t QCameraHardwareInterface::setRedeyeReduction(const QCameraParameters& params)
{
    if(supportsRedEyeReduction() == false) {
        ALOGE("Parameter Redeye Reduction is not supported for this sensor");
        return NO_ERROR;
    }

    const char *str = params.get(QCameraParameters::KEY_QC_REDEYE_REDUCTION);
    if (str != NULL) {
        int value = attr_lookup(redeye_reduction, sizeof(redeye_reduction) / sizeof(str_map), str);
        if (value != NOT_FOUND) {
            int8_t temp = (int8_t)value;
            ALOGI("%s: setting Redeye Reduction value of %s", __FUNCTION__, str);
            mParameters.set(QCameraParameters::KEY_QC_REDEYE_REDUCTION, str);

            native_set_parms(MM_CAMERA_PARM_REDEYE_REDUCTION, sizeof(int8_t), (void *)&temp);
            return NO_ERROR;
        }
    }
    ALOGE("Invalid Redeye Reduction value: %s", (str == NULL) ? "NULL" : str);
    return BAD_VALUE;
}

status_t QCameraHardwareInterface::setGpsLocation(const QCameraParameters& params)
{
    const char *method = params.get(QCameraParameters::KEY_GPS_PROCESSING_METHOD);
    if (method) {
        mParameters.set(QCameraParameters::KEY_GPS_PROCESSING_METHOD, method);
    }else {
         mParameters.remove(QCameraParameters::KEY_GPS_PROCESSING_METHOD);
    }

    const char *latitude = params.get(QCameraParameters::KEY_GPS_LATITUDE);
    if (latitude) {
        ALOGE("latitude %s",latitude);
        mParameters.set(QCameraParameters::KEY_GPS_LATITUDE, latitude);
    }else {
         mParameters.remove(QCameraParameters::KEY_GPS_LATITUDE);
    }

    const char *latitudeRef = params.get(QCameraParameters::KEY_QC_GPS_LATITUDE_REF);
    if (latitudeRef) {
        mParameters.set(QCameraParameters::KEY_QC_GPS_LATITUDE_REF, latitudeRef);
    }else {
         mParameters.remove(QCameraParameters::KEY_QC_GPS_LATITUDE_REF);
    }

    const char *longitude = params.get(QCameraParameters::KEY_GPS_LONGITUDE);
    if (longitude) {
        mParameters.set(QCameraParameters::KEY_GPS_LONGITUDE, longitude);
    }else {
         mParameters.remove(QCameraParameters::KEY_GPS_LONGITUDE);
    }

    const char *longitudeRef = params.get(QCameraParameters::KEY_QC_GPS_LONGITUDE_REF);
    if (longitudeRef) {
        mParameters.set(QCameraParameters::KEY_QC_GPS_LONGITUDE_REF, longitudeRef);
    }else {
         mParameters.remove(QCameraParameters::KEY_QC_GPS_LONGITUDE_REF);
    }

    const char *altitudeRef = params.get(QCameraParameters::KEY_QC_GPS_ALTITUDE_REF);
    if (altitudeRef) {
        mParameters.set(QCameraParameters::KEY_QC_GPS_ALTITUDE_REF, altitudeRef);
    }else {
         mParameters.remove(QCameraParameters::KEY_QC_GPS_ALTITUDE_REF);
    }

    const char *altitude = params.get(QCameraParameters::KEY_GPS_ALTITUDE);
    if (altitude) {
        mParameters.set(QCameraParameters::KEY_GPS_ALTITUDE, altitude);
    }else {
         mParameters.remove(QCameraParameters::KEY_GPS_ALTITUDE);
    }

    const char *status = params.get(QCameraParameters::KEY_QC_GPS_STATUS);
    if (status) {
        mParameters.set(QCameraParameters::KEY_QC_GPS_STATUS, status);
    }

    const char *dateTime = params.get(QCameraParameters::KEY_QC_EXIF_DATETIME);
    if (dateTime) {
        mParameters.set(QCameraParameters::KEY_QC_EXIF_DATETIME, dateTime);
    }else {
         mParameters.remove(QCameraParameters::KEY_QC_EXIF_DATETIME);
    }

    const char *timestamp = params.get(QCameraParameters::KEY_GPS_TIMESTAMP);
    if (timestamp) {
        mParameters.set(QCameraParameters::KEY_GPS_TIMESTAMP, timestamp);
    }else {
         mParameters.remove(QCameraParameters::KEY_GPS_TIMESTAMP);
    }
    ALOGE("setGpsLocation X");
    return NO_ERROR;
}

status_t QCameraHardwareInterface::setRotation(const QCameraParameters& params)
{
    status_t rc = NO_ERROR;
    int rotation = params.getInt(QCameraParameters::KEY_ROTATION);
    if (rotation != NOT_FOUND) {
        if (rotation == 0 || rotation == 90 || rotation == 180
            || rotation == 270) {
          mParameters.set(QCameraParameters::KEY_ROTATION, rotation);
          mRotation = rotation;
        } else {
            ALOGE("Invalid rotation value: %d", rotation);
            rc = BAD_VALUE;
        }
    }
    ALOGE("setRotation");
    return rc;
}

status_t QCameraHardwareInterface::setDenoise(const QCameraParameters& params)
{
    return BAD_VALUE;
}

status_t QCameraHardwareInterface::setOrientation(const QCameraParameters& params)
{
    const char *str = params.get("orientation");

    if (str != NULL) {
        if (strcmp(str, "portrait") == 0 || strcmp(str, "landscape") == 0) {
            // Camera service needs this to decide if the preview frames and raw
            // pictures should be rotated.
            mParameters.set("orientation", str);
        } else {
            ALOGE("Invalid orientation value: %s", str);
            return BAD_VALUE;
        }
    }
    return NO_ERROR;
}

status_t QCameraHardwareInterface::setPictureFormat(const QCameraParameters& params)
{
    const char * str = params.get(QCameraParameters::KEY_PICTURE_FORMAT);
    if(str != NULL){
        int32_t value = attr_lookup(picture_formats,
                                    sizeof(picture_formats) / sizeof(str_map), str);
        if(value != NOT_FOUND){
            if (isZSLMode() && !strcmp(str, "raw")) {
                mParameters.set(QCameraParameters::KEY_PICTURE_FORMAT, "jpeg");
            } else {
                mParameters.set(QCameraParameters::KEY_PICTURE_FORMAT, str);
            }
        } else {
            ALOGE("Invalid Picture Format value: %s", str);
            return BAD_VALUE;
        }
    }
    return NO_ERROR;
}

status_t QCameraHardwareInterface::setRecordingHintValue(const int32_t value)
{
    native_set_parms(MM_CAMERA_PARM_RECORDING_HINT, sizeof(value),
                                           (void *)&value);
    if (value == TRUE){
        native_set_parms(MM_CAMERA_PARM_CAF_ENABLE, sizeof(value),
                                           (void *)&value);
    }
    setDISMode();
    setFullLiveshot();
    return NO_ERROR;
}

status_t QCameraHardwareInterface::setRecordingHint(const QCameraParameters& params)
{

  const char * str = params.get(QCameraParameters::KEY_RECORDING_HINT);

  if(str != NULL){
      int32_t value = attr_lookup(recording_Hints,
                                  sizeof(recording_Hints) / sizeof(str_map), str);
      if(value != NOT_FOUND){
          mRecordingHint = value;
          setRecordingHintValue(mRecordingHint);
          mParameters.set(QCameraParameters::KEY_RECORDING_HINT, str);
          return NO_ERROR;
      } else {
          ALOGE("Invalid Picture Format value: %s", str);
          setDISMode();
          setFullLiveshot();
          return BAD_VALUE;
      }
  }
  setDISMode();
  setFullLiveshot();
  return NO_ERROR;
}

status_t QCameraHardwareInterface::setDISMode() {
  /* Enable DIS only if
   * - Camcorder mode AND
   * - DIS property is set AND
   * - Not in Low power mode. */
  uint32_t value = mRecordingHint && mDisEnabled
                   && !isLowPowerCamcorder();

  ALOGI("%s DIS is %s value = %d", __func__,
          value ? "Enabled" : "Disabled", value);
  native_set_parms(MM_CAMERA_PARM_DIS_ENABLE, sizeof(value),
                                               (void *)&value);
  return NO_ERROR;
}

status_t QCameraHardwareInterface::setFullLiveshot()
{
  /* Enable full size liveshot only if
   * - Camcorder mode AND
   * - Full size liveshot is enabled. */
  uint32_t value = mRecordingHint && mFullLiveshotEnabled
                   && !isLowPowerCamcorder();

  if (((mDimension.picture_width == mDimension.video_width) &&
      (mDimension.picture_height == mDimension.video_height))) {
    /* If video size matches the live snapshot size
     * turn off full size liveshot to get higher fps. */
    value = 0;
  }

  ALOGI("%s Full size liveshot %s value = %d", __func__,
          value ? "Enabled" : "Disabled", value);
  native_set_parms(MM_CAMERA_PARM_FULL_LIVESHOT, sizeof(value),
                                               (void *)&value);
  return NO_ERROR;
}

status_t QCameraHardwareInterface::setMobiCat(const QCameraParameters& params)
{
    mm_cam_mobicat_info_t mbc_info;
    char mbc_prop[PROPERTY_VALUE_MAX];
    int propval;
    memset(mbc_prop, 0, sizeof(mbc_prop));
    property_get("persist.camera.mobicat", mbc_prop, "0");
    propval = atoi(mbc_prop);
    mbc_info.enable = (mParameters.getInt("mobicat") == 1) ? 1 : 0;
    ALOGV("%s:%d] prop %d %d", __func__, __LINE__, mbc_info.enable, propval);
    mbc_info.enable |= propval;

    if (mbc_info.enable != mMobiCatEnabled) {
        ALOGV("%s:%d] enable %d", __func__, __LINE__, mbc_info.enable);
        native_set_parms(MM_CAMERA_PARM_MOBICAT, sizeof(mm_cam_mobicat_info_t),
                        (void *)&mbc_info);
         mMobiCatEnabled = mbc_info.enable;
    }
    return NO_ERROR;
}


isp3a_af_mode_t QCameraHardwareInterface::getAutoFocusMode(
  const QCameraParameters& params)
{
  isp3a_af_mode_t afMode = AF_MODE_MAX;
  afMode = (isp3a_af_mode_t)mFocusMode;
  return afMode;
}

void QCameraHardwareInterface::getPictureSize(int *picture_width,
                                              int *picture_height) const
{
    mParameters.getPictureSize(picture_width, picture_height);
}

void QCameraHardwareInterface::getPreviewSize(int *preview_width,
                                              int *preview_height) const
{
    mParameters.getPreviewSize(preview_width, preview_height);
}

void QCameraHardwareInterface::getVideoSize(int *video_width,
                                              int *video_height) const
{
    mParameters.getVideoSize(video_width, video_height);
}

void QCameraHardwareInterface::getThumbnailSize(int *thumb_width,
                                              int *thumb_height) const
{
    *thumb_width = mParameters.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
    *thumb_height = mParameters.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
}

cam_format_t QCameraHardwareInterface::getPreviewFormat() const
{
    cam_format_t foramt = CAMERA_YUV_420_NV21;
    const char *str = mParameters.getPreviewFormat();
    int32_t value = attr_lookup(preview_formats,
                                sizeof(preview_formats)/sizeof(str_map),
                                str);

    if(value != NOT_FOUND) {
        int num = sizeof(preview_format_info_list)/sizeof(preview_format_info_t);
        int i;
        for (i = 0; i < num; i++) {
          if (preview_format_info_list[i].Hal_format == value) {
            foramt = preview_format_info_list[i].mm_cam_format;
            break;
          }
        }
    }

    return foramt;
}

cam_pad_format_t QCameraHardwareInterface::getPreviewPadding() const
{
  return mPreviewFormatInfo.padding;
}

int QCameraHardwareInterface::getJpegQuality() const
{
    return mJpegQuality;
}

int QCameraHardwareInterface::getNumOfSnapshots(void) const
{
    char prop[PROPERTY_VALUE_MAX];
    memset(prop, 0, sizeof(prop));
    property_get("persist.camera.snapshot.number", prop, "0");
    ALOGI("%s: prop enable/disable = %d", __func__, atoi(prop));
    if (atoi(prop)) {
        ALOGE("%s: Reading maximum no of snapshots = %d"
             "from properties", __func__, atoi(prop));
        return atoi(prop);
    } else {
        return mParameters.getInt("num-snaps-per-shutter");
    }
}

int QCameraHardwareInterface::getNumOfSnapshots(const QCameraParameters& params)
{
    char prop[PROPERTY_VALUE_MAX];
    memset(prop, 0, sizeof(prop));
    property_get("persist.camera.snapshot.number", prop, "0");
    ALOGI("%s: prop enable/disable = %d", __func__, atoi(prop));
    if (atoi(prop)) {
        ALOGI("%s: Reading maximum no of snapshots = %d"
             "from properties", __func__, atoi(prop));
        return atoi(prop);
    } else {
        return params.getInt("num-snaps-per-shutter");
    }

}

int QCameraHardwareInterface::
getThumbSizesFromAspectRatio(uint32_t aspect_ratio,
                             int *picture_width,
                             int *picture_height)
{
    for(unsigned int i = 0; i < THUMBNAIL_SIZE_COUNT; i++ ){
        if(thumbnail_sizes[i].aspect_ratio == aspect_ratio)
        {
            *picture_width = thumbnail_sizes[i].width;
            *picture_height = thumbnail_sizes[i].height;
            return NO_ERROR;
        }
    }

    return BAD_VALUE;
}

bool QCameraHardwareInterface::isRawSnapshot()
{
  const char *format = mParameters.getPictureFormat();
    if( format!= NULL &&
       !strcmp(format, QCameraParameters::PIXEL_FORMAT_RAW)){
        return true;
    }
    else{
        return false;
    }
}

status_t QCameraHardwareInterface::setPreviewSizeTable(void)
{
    status_t ret = NO_ERROR;
    mm_camera_dimension_t dim;
    struct camera_size_type* preview_size_table;
    int preview_table_size;
    int i = 0;
    char str[10] = {0};

    /* Initialize table with default values */
    preview_size_table = default_preview_sizes;
    preview_table_size = preview_sizes_count;


    /* Get maximum preview size supported by sensor*/
    memset(&dim, 0, sizeof(mm_camera_dimension_t));
    ret = mCameraHandle->ops->get_parm(mCameraHandle->camera_handle,
                              MM_CAMERA_PARM_MAX_PREVIEW_SIZE, &dim);
    if (ret != NO_ERROR) {
        ALOGE("%s: Failure getting Max Preview Size supported by camera",
             __func__);
        goto end;
    }

    ALOGD("%s: Max Preview Sizes Supported: %d X %d", __func__,
         dim.width, dim.height);

    for (i = 0; i < preview_table_size; i++) {
        if ((preview_size_table->width <= dim.width) &&
            (preview_size_table->height <= dim.height)) {
            ALOGD("%s: Camera Preview Size Table "
                 "Max width: %d height %d table_size: %d",
                 __func__, preview_size_table->width,
                 preview_size_table->height, preview_table_size - i);
            break;
        }
        preview_size_table++;
    }
    //set preferred preview size to maximum preview size
    sprintf(str, "%dx%d", preview_size_table->width, preview_size_table->height);
    mParameters.set(QCameraParameters::KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO, str);
    ALOGD("KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO = %s", str);

end:
    /* Save the table in global member*/
    mPreviewSizes = preview_size_table;
    mPreviewSizeCount = preview_table_size - i;

    return ret;
}

status_t QCameraHardwareInterface::setPictureSizeTable(void)
{
    status_t ret = NO_ERROR;
    mm_camera_dimension_t dim;
    struct camera_size_type* picture_size_table;
    int picture_table_size;
    int i = 0, count = 0;

    /* Initialize table with default values */
    picture_table_size = sizeof(default_picture_sizes)/
        sizeof(default_picture_sizes[0]);
    picture_size_table = default_picture_sizes;
    mPictureSizes =
        ( struct camera_size_type *)malloc(picture_table_size *
                                           sizeof(struct camera_size_type));
    if (mPictureSizes == NULL) {
        ALOGE("%s: Failre allocating memory to store picture size table",__func__);
        goto end;
    }

    /* Get maximum picture size supported by sensor*/
    memset(&dim, 0, sizeof(mm_camera_dimension_t));
    ret = mCameraHandle->ops->get_parm(mCameraHandle->camera_handle,
                              MM_CAMERA_PARM_MAX_PICTURE_SIZE, &dim);
    if (ret != NO_ERROR) {
        ALOGE("%s: Failure getting Max Picture Size supported by camera",
             __func__);
        ret = NO_MEMORY;
        free(mPictureSizes);
        mPictureSizes = NULL;
        goto end;
    }

    ALOGD("%s: Max Picture Sizes Supported: %d X %d", __func__,
         dim.width, dim.height);

    for (i = 0; i < picture_table_size; i++) {
        /* We'll store those dimensions whose width AND height
           are less than or equal to maximum supported */
        if ((picture_size_table->width <= dim.width) &&
            (picture_size_table->height <= dim.height)) {
            ALOGD("%s: Camera Picture Size Table "
                 "Max width: %d height %d table_size: %d",
                 __func__, picture_size_table->width,
                 picture_size_table->height, count+1);
            mPictureSizes[count].height = picture_size_table->height;
            mPictureSizes[count].width = picture_size_table->width;
            count++;
        }
        picture_size_table++;
    }
    mPictureSizeCount = count;

end:
     /* In case of error, we use default picture sizes */
     if (ret != NO_ERROR) {
        mPictureSizes = default_picture_sizes;
        mPictureSizeCount = picture_table_size;
    }
    return ret;
}

status_t QCameraHardwareInterface::setVideoSizeTable(void)
{
    status_t ret = NO_ERROR;
    mm_camera_dimension_t dim;
    struct camera_size_type* video_size_table;
    int video_table_size;
    int i = 0, count = 0;
    ALOGE("%s: E", __func__);

    /* Initialize table with default values */
    video_table_size = video_sizes_count;
    video_size_table = default_video_sizes;
    mVideoSizes =
        (struct camera_size_type *)malloc(video_table_size *
                                           sizeof(struct camera_size_type));
    if(mVideoSizes == NULL) {
        ALOGE("%s: error allocating memory to store video size table",__func__);
        ret = BAD_VALUE;
        goto end;
    }

    /* Get maximum video size supported by sensor*/
    memset(&dim, 0, sizeof(mm_camera_dimension_t));
    ret = mCameraHandle->ops->get_parm(mCameraHandle->camera_handle,
                              MM_CAMERA_PARM_MAX_VIDEO_SIZE, &dim);
    if(ret != NO_ERROR) {
        ALOGE("%s: error getting Max Video Size supported by camera",
             __func__);
        ret = NO_MEMORY;
        free(mVideoSizes);
        mVideoSizes = NULL;
        ret = BAD_VALUE;
        goto end;
    }

    ALOGD("%s: Max Video Size Supported: %d X %d", __func__,
         dim.width, dim.height);

    for(i=0; i < video_table_size; i++) {
        /* We'll store those dimensions whose width AND height
           are less than or equal to maximum supported */
        if((video_size_table->width <= dim.width) &&
            (video_size_table->height <= dim.height)) {
            ALOGD("%s: Supported Video Size [%d] = %dx%d", __func__, count, video_size_table->width,
                                    video_size_table->height);
            mVideoSizes[count].height = video_size_table->height;
            mVideoSizes[count].width = video_size_table->width;
            count++;
        }
        video_size_table++;
    }
    mVideoSizeCount = count;

end:
    ALOGE("%s: X", __func__);
    return ret;
}

void QCameraHardwareInterface::freeVideoSizeTable(void)
{
    if(mVideoSizes != NULL)
    {
        free(mVideoSizes);
    }
    mVideoSizeCount = 0;
}


void QCameraHardwareInterface::freePictureTable(void)
{
    /* If we couldn't allocate memory to store picture table
       we use the picture table pointer to point to default
       picture table array. In that case we cannot free it.*/
    if ((mPictureSizes != default_picture_sizes) && mPictureSizes) {
        free(mPictureSizes);
    }
}

status_t QCameraHardwareInterface::setHistogram(int histogram_en)
{
    status_t ret = NO_ERROR;

    ALOGV("setHistogram: E");
    if(mStatsOn == histogram_en) {
        return ret;
    }

    status_t rc = NO_ERROR;
    mCameraHandle->ops->is_parm_supported(mCameraHandle->camera_handle,
                                          MM_CAMERA_PARM_HISTOGRAM,
                                          (uint8_t*)&rc,(uint8_t*)&rc);
    if(!rc) {
        ALOGE(" Histogram is not supported for this");
        return NO_ERROR;
    }
    mSendData = histogram_en;
    mStatsOn = histogram_en;

    if (histogram_en == QCAMERA_PARM_ENABLE) {
        ret = initHistogramBuffers();
        if (ret != NO_ERROR) {
          ALOGE("%s Error initializing Histogram buffers ", __func__);
          return ret;
        }
    }
    ALOGI(" Histogram -> %s", histogram_en ? "Enabled" : "Disabled");
    native_set_parms(MM_CAMERA_PARM_HISTOGRAM, sizeof(int), &histogram_en);
    if(histogram_en == QCAMERA_PARM_DISABLE) {
        ret = deInitHistogramBuffers();
        if (ret != NO_ERROR) {
          ALOGE("%s Error deInitializing Histogram buffers ", __func__);
          return ret;
        }
    }
    ALOGV("setHistogram: X");
    return ret;
}

status_t QCameraHardwareInterface::setZSLBurstLookBack(const QCameraParameters& params)
{
  const char *v = params.get("capture-burst-retroactive");
  if (v) {
    int look_back = atoi(v);
    ALOGI("%s: look_back =%d", __func__, look_back);
    mParameters.set("capture-burst-retroactive", look_back);
  }
  return NO_ERROR;
}

status_t QCameraHardwareInterface::setZSLBurstInterval(const QCameraParameters& params)
{
  mZslInterval = BURST_INTREVAL_DEFAULT;
  const char *v = params.get("capture-burst-interval");
  if (v) {
    int interval = atoi(v);
    ALOGI("%s: Interval =%d", __func__, interval);
    if(interval < BURST_INTREVAL_MIN ||interval > BURST_INTREVAL_MAX ) {
      return BAD_VALUE;
    }
    mZslInterval =  interval;
  }
  return NO_ERROR;
}

int QCameraHardwareInterface::getZSLBurstInterval( void )
{
  int val;

  if (mZslInterval == BURST_INTREVAL_DEFAULT) {
    char prop[PROPERTY_VALUE_MAX];
    memset(prop, 0, sizeof(prop));
    property_get("persist.camera.zsl.interval", prop, "1");
    val = atoi(prop);
    ALOGD("%s: prop interval = %d", __func__, val);
  } else {
    val = mZslInterval;
  }
  return val;
}


int QCameraHardwareInterface::getZSLQueueDepth(void) const
{
    char prop[PROPERTY_VALUE_MAX];
    memset(prop, 0, sizeof(prop));
    property_get("persist.camera.zsl.queuedepth", prop, "2");
    ALOGI("%s: prop = %d", __func__, atoi(prop));
    return atoi(prop);
}

int QCameraHardwareInterface::getZSLBackLookCount(void) const
{
    int look_back;
    char prop[PROPERTY_VALUE_MAX];
    memset(prop, 0, sizeof(prop));
    property_get("persist.camera.zsl.backlookcnt", prop, "0");
    ALOGI("%s: prop = %d", __func__, atoi(prop));
    look_back = atoi(prop);
    if (look_back == 0 ) {
      look_back = mParameters.getInt("capture-burst-retroactive");
      ALOGE("%s: look_back = %d", __func__, look_back);
    }
    return look_back;
}

//EXIF functions
void QCameraHardwareInterface::deinitExifData()
{
    ALOGD("Clearing EXIF data");
    memset(mExifData, 0, sizeof(exif_tags_info_t) * MAX_EXIF_TABLE_ENTRIES);
    mExifTableNumEntries = 0;
}

void QCameraHardwareInterface::addExifTag(exif_tag_id_t tagid, exif_tag_type_t type,
                        uint32_t count, uint8_t copy, void *data) {

    if(mExifTableNumEntries >= MAX_EXIF_TABLE_ENTRIES) {
        ALOGE("%s: Number of entries exceeded limit", __func__);
        return;
    }
    int index = mExifTableNumEntries;
    mExifData[index].tag_id = tagid;
    mExifData[index].tag_entry.type = type;
    mExifData[index].tag_entry.count = count;
    mExifData[index].tag_entry.copy = copy;
    if((type == EXIF_RATIONAL) && (count > 1))
        mExifData[index].tag_entry.data._rats = (rat_t *)data;
    if((type == EXIF_RATIONAL) && (count == 1))
        mExifData[index].tag_entry.data._rat = *(rat_t *)data;
    else if(type == EXIF_ASCII)
        mExifData[index].tag_entry.data._ascii = (char *)data;
    else if(type == EXIF_BYTE)
        mExifData[index].tag_entry.data._byte = *(uint8_t *)data;
    else if((type == EXIF_SHORT) && (count > 1))
        mExifData[index].tag_entry.data._shorts = (uint16_t *)data;
    else if((type == EXIF_SHORT) && (count == 1))
        mExifData[index].tag_entry.data._short = *(uint16_t *)data;
    // Increase number of entries
    mExifTableNumEntries++;
}

rat_t getRational(int num, int denom)
{
    rat_t temp = {num, denom};
    return temp;
}

void QCameraHardwareInterface::initExifData(){
    if(mExifValues.dateTime) {
        addExifTag(EXIFTAGID_EXIF_DATE_TIME_ORIGINAL, EXIF_ASCII,
                  20, 1, (void *)mExifValues.dateTime);
    }
    addExifTag(EXIFTAGID_FOCAL_LENGTH, EXIF_RATIONAL, 1, 1, (void *)&(mExifValues.focalLength));
    addExifTag(EXIFTAGID_ISO_SPEED_RATING,EXIF_SHORT,1,1,(void *)&(mExifValues.isoSpeed));

    if(mExifValues.mGpsProcess) {
        addExifTag(EXIFTAGID_GPS_PROCESSINGMETHOD, EXIF_ASCII,
           EXIF_ASCII_PREFIX_SIZE + strlen(mExifValues.gpsProcessingMethod + EXIF_ASCII_PREFIX_SIZE) + 1,
           1, (void *)mExifValues.gpsProcessingMethod);
    }

    if(mExifValues.mLatitude) {
        addExifTag(EXIFTAGID_GPS_LATITUDE, EXIF_RATIONAL, 3, 1, (void *)mExifValues.latitude);

        if(mExifValues.latRef) {
            addExifTag(EXIFTAGID_GPS_LATITUDE_REF, EXIF_ASCII, 2,
                                    1, (void *)mExifValues.latRef);
        }
    }

    if(mExifValues.mLongitude) {
        addExifTag(EXIFTAGID_GPS_LONGITUDE, EXIF_RATIONAL, 3, 1, (void *)mExifValues.longitude);

        if(mExifValues.lonRef) {
            addExifTag(EXIFTAGID_GPS_LONGITUDE_REF, EXIF_ASCII, 2,
                                1, (void *)mExifValues.lonRef);
        }
    }

    if(mExifValues.mAltitude) {
        addExifTag(EXIFTAGID_GPS_ALTITUDE, EXIF_RATIONAL, 1,
                    1, (void *)&(mExifValues.altitude));

        addExifTag(EXIFTAGID_GPS_ALTITUDE_REF, EXIF_BYTE, 1, 1, (void *)&mExifValues.mAltitude_ref);
    }

    if(mExifValues.mTimeStamp) {
        time_t unixTime;
        struct tm *UTCTimestamp;

        unixTime = (time_t)mExifValues.mGPSTimestamp;
        UTCTimestamp = gmtime(&unixTime);

        strftime(mExifValues.gpsDateStamp, sizeof(mExifValues.gpsDateStamp), "%Y:%m:%d", UTCTimestamp);
        addExifTag(EXIFTAGID_GPS_DATESTAMP, EXIF_ASCII,
                          strlen(mExifValues.gpsDateStamp)+1 , 1, (void *)mExifValues.gpsDateStamp);

        mExifValues.gpsTimeStamp[0] = getRational(UTCTimestamp->tm_hour, 1);
        mExifValues.gpsTimeStamp[1] = getRational(UTCTimestamp->tm_min, 1);
        mExifValues.gpsTimeStamp[2] = getRational(UTCTimestamp->tm_sec, 1);

        addExifTag(EXIFTAGID_GPS_TIMESTAMP, EXIF_RATIONAL,
                  3, 1, (void *)mExifValues.gpsTimeStamp);
        ALOGE("EXIFTAGID_GPS_TIMESTAMP set");
    }

}

//Add all exif tags in this function
void QCameraHardwareInterface::setExifTags()
{
    const char *str;

    //set TimeStamp
    str = mParameters.get(QCameraParameters::KEY_QC_EXIF_DATETIME);
    if(str != NULL) {
      strncpy(mExifValues.dateTime, str, 19);
      mExifValues.dateTime[19] = '\0';
    }

    //Set focal length
    int focalLengthValue = (int) (mParameters.getFloat(
                QCameraParameters::KEY_FOCAL_LENGTH) * FOCAL_LENGTH_DECIMAL_PRECISION);

    mExifValues.focalLength = getRational(focalLengthValue, FOCAL_LENGTH_DECIMAL_PRECISION);

    //Set ISO Speed
    mExifValues.isoSpeed = getISOSpeedValue();

    //get time and date from system
    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    timeinfo = localtime (&rawtime);
    //Write datetime according to EXIF Spec
    //"YYYY:MM:DD HH:MM:SS" (20 chars including \0)
    snprintf(mExifValues.dateTime, 20, "%04d:%02d:%02d %02d:%02d:%02d",
                timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,
                timeinfo->tm_mday, timeinfo->tm_hour,
                timeinfo->tm_min, timeinfo->tm_sec);

    //set gps tags
    setExifTagsGPS();
}

void QCameraHardwareInterface::setExifTagsGPS()
{
    const char *str = NULL;

    //Set GPS processing method
    str = mParameters.get(QCameraParameters::KEY_GPS_PROCESSING_METHOD);
    if(str != NULL) {
       memcpy(mExifValues.gpsProcessingMethod, ExifAsciiPrefix, EXIF_ASCII_PREFIX_SIZE);
       strncpy(mExifValues.gpsProcessingMethod + EXIF_ASCII_PREFIX_SIZE, str,
           GPS_PROCESSING_METHOD_SIZE - 1);
       mExifValues.gpsProcessingMethod[EXIF_ASCII_PREFIX_SIZE + GPS_PROCESSING_METHOD_SIZE-1] = '\0';
       ALOGE("EXIFTAGID_GPS_PROCESSINGMETHOD = %s %s", mExifValues.gpsProcessingMethod,
                                                    mExifValues.gpsProcessingMethod+8);
       mExifValues.mGpsProcess  = true;
    }else{
        mExifValues.mGpsProcess = false;
    }
    str = NULL;

    //Set Latitude
    str = mParameters.get(QCameraParameters::KEY_GPS_LATITUDE);
    if(str != NULL) {
        parseGPSCoordinate(str, mExifValues.latitude);
        ALOGE("EXIFTAGID_GPS_LATITUDE = %s", str);

        //set Latitude Ref
        float latitudeValue = mParameters.getFloat(QCameraParameters::KEY_GPS_LATITUDE);
        if(latitudeValue < 0.0f) {
            mExifValues.latRef[0] = 'S';
        } else {
            mExifValues.latRef[0] = 'N';
        }
        mExifValues.latRef[1] = '\0';
        mExifValues.mLatitude = true;
        mParameters.set(QCameraParameters::KEY_QC_GPS_LATITUDE_REF,mExifValues.latRef);
        ALOGE("EXIFTAGID_GPS_LATITUDE_REF = %s", mExifValues.latRef);
    }else{
        mExifValues.mLatitude = false;
    }

    //set Longitude
    str = NULL;
    str = mParameters.get(QCameraParameters::KEY_GPS_LONGITUDE);
    if(str != NULL) {
        parseGPSCoordinate(str, mExifValues.longitude);
        ALOGE("EXIFTAGID_GPS_LONGITUDE = %s", str);

        //set Longitude Ref
        float longitudeValue = mParameters.getFloat(QCameraParameters::KEY_GPS_LONGITUDE);
        if(longitudeValue < 0.0f) {
            mExifValues.lonRef[0] = 'W';
        } else {
            mExifValues.lonRef[0] = 'E';
        }
        mExifValues.lonRef[1] = '\0';
        mExifValues.mLongitude = true;
        ALOGE("EXIFTAGID_GPS_LONGITUDE_REF = %s", mExifValues.lonRef);
        mParameters.set(QCameraParameters::KEY_QC_GPS_LONGITUDE_REF, mExifValues.lonRef);
    }else{
        mExifValues.mLongitude = false;
    }

    //set Altitude
    str = mParameters.get(QCameraParameters::KEY_GPS_ALTITUDE);
    if(str != NULL) {
        double value = atof(str);
        mExifValues.mAltitude_ref = 0;
        if(value < 0){
            mExifValues.mAltitude_ref = 1;
            value = -value;
        }
        mExifValues.altitude = getRational(value*1000, 1000);
        mExifValues.mAltitude = true;
        //set AltitudeRef
        mParameters.set(QCameraParameters::KEY_QC_GPS_ALTITUDE_REF, mExifValues.mAltitude_ref);
        ALOGE("EXIFTAGID_GPS_ALTITUDE = %f", value);
    }else{
        mExifValues.mAltitude = false;
    }

    //set Gps TimeStamp
    str = NULL;
    str = mParameters.get(QCameraParameters::KEY_GPS_TIMESTAMP);
    if(str != NULL) {
      mExifValues.mTimeStamp = true;
      mExifValues.mGPSTimestamp = atol(str);
    }else{
         mExifValues.mTimeStamp = false;
    }
}

//latlonString is string formatted coordinate
//coord is rat_t[3]
void QCameraHardwareInterface::parseGPSCoordinate(const char *latlonString, rat_t* coord)
{
    if(coord == NULL) {
        ALOGE("%s: error, invalid argument coord == NULL", __func__);
        return;
    }
    float degF = fabs(atof(latlonString));
    float minF = (degF- (int) degF) * 60;
    float secF = (minF - (int) minF) * 60;

    coord[0] = getRational((int) degF, 1);
    coord[1] = getRational((int) minF, 1);
    coord[2] = getRational((int) (secF * 10000), 10000);
}

bool QCameraHardwareInterface::isLowPowerCamcorder() {

    if (mPowerMode == LOW_POWER)
        return true;

    if(mHFRLevel > 1) /* hard code the value now. Need to move tgtcommon to camear.h */
      return true;

      return false;
}

status_t QCameraHardwareInterface::setNoDisplayMode(const QCameraParameters& params)
{
  char prop[PROPERTY_VALUE_MAX];
  memset(prop, 0, sizeof(prop));
  property_get("persist.camera.nodisplay", prop, "0");
  int prop_val = atoi(prop);

  if (prop_val == 0) {
    const char *str_val  = params.get("no-display-mode");
    if(str_val && strlen(str_val) > 0) {
      mNoDisplayMode = atoi(str_val);
    } else {
      mNoDisplayMode = 0;
    }
    ALOGD("Param mNoDisplayMode =%d", mNoDisplayMode);
  } else {
    mNoDisplayMode = prop_val;
    ALOGD("prop mNoDisplayMode =%d", mNoDisplayMode);
  }
  return NO_ERROR;
}

status_t QCameraHardwareInterface::setRDIMode(const QCameraParameters& params)
{
    char prop[PROPERTY_VALUE_MAX];
    memset(prop, 0, sizeof(prop));
    property_get("persist.camera.rdi.mode", prop, "0");
    int prop_val = atoi(prop);
    rdiMode = prop_val;
    ALOGE("RDI Mode = %d",rdiMode);
    return NO_ERROR;
}

status_t QCameraHardwareInterface::setDimension()
{
    ALOGV("%s: E", __func__);
    cam_ctrl_dimension_t dim;
    int ret = MM_CAMERA_OK;
    int postviewWidth,postviewHeight;

    memset(&dim, 0, sizeof(cam_ctrl_dimension_t));
    ret = mCameraHandle->ops->get_parm(mCameraHandle->camera_handle, MM_CAMERA_PARM_DIMENSION,&dim);

    if (MM_CAMERA_OK != ret) {
      ALOGE("%s: error - can't get preview dimension!", __func__);
      ALOGE("%s: X", __func__);
      return BAD_VALUE;
    }

    getPreviewSize(&mPreviewWidth,  &mPreviewHeight);
    dim.display_width  = mPreviewWidth;
    dim.display_height = mPreviewHeight;

    cam_format_t value = getPreviewFormat();
    if(value != NOT_FOUND && value != dim.prev_format ) {
        //Setting to Parameter requested by the Upper layer
        dim.prev_format = value;
    }else{
        //Setting to default Format.
        dim.prev_format = CAMERA_YUV_420_NV21;
    }
    dim.prev_padding_format =  getPreviewPadding( );

    getVideoSize(&videoWidth,  &videoHeight);
    dim.enc_format = CAMERA_YUV_420_NV12;
    dim.orig_video_width = videoWidth;
    dim.orig_video_height = videoHeight;
    dim.video_width = videoWidth;
    dim.video_height = videoHeight;
    dim.video_chroma_width = videoWidth;
    dim.video_chroma_height  = videoHeight;

    getPictureSize(&mPictureWidth, &mPictureHeight);
    dim.picture_width = mPictureWidth;
    dim.picture_height  = mPictureHeight;

    getThumbnailSize(&thumbnailWidth,&thumbnailHeight);
    dim.ui_thumbnail_width = thumbnailWidth;
    dim.ui_thumbnail_height = thumbnailHeight;

    /* Reset the Main image and thumbnail formats here,
     * since they might have been changed when video size
     * livesnapshot was taken. */
    if (mSnapshotFormat == 1)
      dim.main_img_format = CAMERA_YUV_422_NV61;
    else
      dim.main_img_format = CAMERA_YUV_420_NV21;
    dim.thumb_format = CAMERA_YUV_420_NV21;

    //RDI Format
    dim.rdi0_format = CAMERA_BAYER_SBGGR10;

    /*Code to handle different limitations*/
    if (mRecordingHint && mFullLiveshotEnabled){
        dim.ui_thumbnail_height = dim.display_height;
        dim.ui_thumbnail_width = dim.display_width;
    }
    if (isZSLMode()){
        postviewWidth = mPreviewWidth;
        postviewHeight = mPreviewHeight;
    } else {
        postviewWidth = thumbnailWidth;
        postviewHeight = thumbnailHeight;
    }
    if (mPictureWidth < postviewWidth || mPictureHeight < postviewHeight)
    {
        //Changes to handle VFE limitation when primary o/p is main image
        dim.picture_width = postviewWidth;
        dim.picture_height = postviewHeight;
    }

    //VFE output1 shouldn't be greater than VFE output2.
    if( (dim.display_width > dim.video_width) ||
        (dim.display_height > dim.video_height)) {
        //Set preview sizes as record sizes.
        dim.display_width = dim.video_width;
        dim.display_height = dim.video_height;
    }
    if (mRecordingHint && mFullLiveshotEnabled){
        if( (dim.picture_width < dim.video_width) ||
            (dim.picture_height < dim.video_height)) {
            dim.picture_width = dim.video_width;
            dim.picture_height = dim.video_height;
        }
    }

    if(mIs3DModeOn == true) {
        /* As preview and video frames are same in 3D mode,
         * preview size should be same as video size. This
         * cahnge is needed to take of video resolutions
         * like 720P and 1080p where the application can
         * request different preview sizes like 768x432
         */
        ALOGE("3D mod is on");
        dim.display_width = dim.video_width;
        dim.display_height = dim.video_height;
    }

    /*End of limitation code*/

    ret = mCameraHandle->ops->set_parm(mCameraHandle->camera_handle, MM_CAMERA_PARM_DIMENSION,&dim);
    if (MM_CAMERA_OK != ret) {
      ALOGE("%s X: error - can't config preview parms!", __func__);
      return BAD_VALUE;
    }
    if(mStreams[MM_CAMERA_PREVIEW]) {
        mStreams[MM_CAMERA_PREVIEW]->mFormat = dim.prev_format;
        mStreams[MM_CAMERA_PREVIEW]->mWidth = dim.display_width;
        mStreams[MM_CAMERA_PREVIEW]->mHeight = dim.display_height;
    }
    if(mStreams[MM_CAMERA_VIDEO]) {
        mStreams[MM_CAMERA_VIDEO]->mFormat = dim.enc_format;
        mStreams[MM_CAMERA_VIDEO]->mWidth = dim.video_width;
        mStreams[MM_CAMERA_VIDEO]->mHeight = dim.video_height;
    }
    if(mStreams[MM_CAMERA_SNAPSHOT_MAIN]) {
        mStreams[MM_CAMERA_SNAPSHOT_MAIN]->mFormat = dim.main_img_format;
        if (!isRawSnapshot()) {
                mStreams[MM_CAMERA_SNAPSHOT_MAIN]->mWidth = dim.picture_width;
                mStreams[MM_CAMERA_SNAPSHOT_MAIN]->mHeight = dim.picture_height;
        } else {
            cam_frame_resolution_t raw_res;

            /* the raw_picture_width in dimension is for RDI dump.
             * here, raw snapshot size is from camif. */
            memset(&raw_res, 0,  sizeof(raw_res));
            raw_res.image_mode = MSM_V4L2_EXT_CAPTURE_MODE_RAW;
            raw_res.padding_format = CAMERA_PAD_TO_WORD;
            ret = mCameraHandle->ops->get_parm(
               mCameraHandle->camera_handle,
               MM_CAMERA_PARM_FRAME_RESOLUTION,&raw_res);
            if (MM_CAMERA_OK != ret) {
              ALOGE("%s error - config raw snapshot parms, rc = %d",
                    __func__, ret);
              return BAD_VALUE;
            }
            mStreams[MM_CAMERA_SNAPSHOT_MAIN]->mWidth = raw_res.width;
            mStreams[MM_CAMERA_SNAPSHOT_MAIN]->mHeight = raw_res.height;
            mStreams[MM_CAMERA_SNAPSHOT_MAIN]->mFormat = raw_res.format;
        }
    }
    if(mStreams[MM_CAMERA_SNAPSHOT_THUMBNAIL]) {
        mStreams[MM_CAMERA_SNAPSHOT_THUMBNAIL]->mFormat = dim.thumb_format;
        mStreams[MM_CAMERA_SNAPSHOT_THUMBNAIL]->mWidth = dim.ui_thumbnail_width;
        mStreams[MM_CAMERA_SNAPSHOT_THUMBNAIL]->mHeight = dim.ui_thumbnail_height;
    }

    memcpy(&mDimension, &dim, sizeof(mDimension));
    ALOGV("%s: X", __func__);
    return NO_ERROR;
}
}; /*namespace android */
