/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
 */

//#define ALOG_NDEBUG 0
#define ALOG_NIDEBUG 0
#define LOG_TAG "QCameraUsbParm"
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
#include "QualcommUsbCamera.h"
#include "QCameraUsbPriv.h"
#include "QCameraUsbParm.h"

namespace android {

/********************************************************************/
static const str_map preview_formats[] = {
    {QCameraParameters::PIXEL_FORMAT_YUV420SP, HAL_PIXEL_FORMAT_YCrCb_420_SP},
};

static const preview_format_info_t preview_format_info_list[] = {
    {HAL_PIXEL_FORMAT_YV12, CAMERA_YUV_420_YV12, CAMERA_PAD_TO_WORD, 3}
};

static struct camera_size_type previewSizes[] = {
    { 1920, 1088}, //1080p
    { 1280, 720}, // 720P,
    { 640, 480}, // VGA
    { 512, 384},
    { 480, 320},
    { 320, 240}, // QVGA
};

// All fps ranges which can be supported. This list will be filtered according
// to the min and max fps supported by hardware
// this list must be sorted first by max_fps and then min_fps
// fps values are multiplied by 1000
static android::FPSRange prevFpsRanges[] = {
    android::FPSRange(5000, 121000),
};

/* TBR: Is frame rate mode mandatory */
static const str_map frame_rate_modes[] = {
    {QCameraParameters::KEY_QC_PREVIEW_FRAME_RATE_AUTO_MODE, FPS_MODE_AUTO},
    {QCameraParameters::KEY_QC_PREVIEW_FRAME_RATE_FIXED_MODE, FPS_MODE_FIXED}
};

static const str_map picture_formats[] = {
    {QCameraParameters::PIXEL_FORMAT_JPEG, PICTURE_FORMAT_JPEG},
    //{QCameraParameters::PIXEL_FORMAT_RAW, PICTURE_FORMAT_RAW}
};

static camera_size_type picture_sizes[] = {
    { 1920, 1088}, //HD1080
    { 1280, 720}, //HD720
    { 640, 480}, // VGA
    { 320, 240}, // QVGA
};

/* aspect ratio removed */
static camera_size_type thumbnail_sizes[] = {
    { 512, 288 }, //1.777778
    { 480, 288 }, //1.666667
    { 256, 154 }, //1.66233
    { 432, 288 }, //1.5
    { 512, 384 }, //1.333333
    { 352, 288 }, //1.222222
    { 320, 240 }, //1.33333
    { 176, 144 }, //1.222222
};

static const str_map recording_Hints[] = {
    {"false", FALSE},
    {"true",  TRUE}
};

/* Static functions list */
static String8 create_sizes_str(const camera_size_type *sizes, int len);
static String8 create_values_str(const str_map *values, int len);
static String8 create_fps_str(const android:: FPSRange* fps, int len);
static String8 create_values_range_str(int min, int max);
static int usbCamSetPrvwSize(   camera_hardware_t           *camHal,
                                const QCameraParameters&    params);
static int usbCamSetPictSize(   camera_hardware_t           *camHal,
                                const QCameraParameters&    params);
static int usbCamSetThumbnailSize(  camera_hardware_t           *camHal,
                                    const QCameraParameters&    params);
static int usbCamSetJpegQlty(   camera_hardware_t           *camHal,
                                const QCameraParameters&    params);

/******************************************************************************
 * Function: usbCamInitDefaultParameters
 * Description: This function sets default parameters to camera HAL context
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *      None
 *
 * Notes: none
 *****************************************************************************/
int usbCamInitDefaultParameters(camera_hardware_t *camHal)
{
    ALOGD("%s: E", __func__);
    int rc = 0;
    char tempStr[FILENAME_LENGTH];

    /* Default initializations */
    camHal->prevFormat          = DEFAULT_USBCAM_PRVW_FMT;
    camHal->prevWidth           = DEFAULT_USBCAM_PRVW_WD;
    camHal->prevHeight          = DEFAULT_USBCAM_PRVW_HT;
    camHal->dispFormat          = camHal->prevFormat;
    camHal->dispWidth           = camHal->prevWidth;
    camHal->dispHeight          = camHal->prevHeight;
    camHal->pictFormat          = DEFAULT_USBCAM_PICT_FMT;
    camHal->pictWidth           = DEFAULT_USBCAM_PICT_WD;
    camHal->pictHeight          = DEFAULT_USBCAM_PICT_HT;
    camHal->pictJpegQlty        = DEFAULT_USBCAM_PICT_QLTY;
    camHal->thumbnailWidth      = DEFAULT_USBCAM_THUMBNAIL_WD;
    camHal->thumbnailHeight     = DEFAULT_USBCAM_THUMBNAIL_HT;
    camHal->thumbnailJpegQlty   = DEFAULT_USBCAM_THUMBNAIL_QLTY;
    camHal->previewEnabledFlag  = 0;
    camHal->prvwStoppedForPicture = 0;
    camHal->prvwCmdPending      = 0;
    camHal->takePictInProgress  = 0;

    //Set picture size values
    camHal->pictSizeValues = create_sizes_str(
        picture_sizes, sizeof(picture_sizes) / sizeof(camera_size_type));
    camHal->qCamParams.set(QCameraParameters::KEY_SUPPORTED_PICTURE_SIZES,
        camHal->pictSizeValues.string());
    camHal->qCamParams.setPictureSize(camHal->pictWidth, camHal->pictHeight);

    //Set picture format
    camHal->pictFormatValues = create_values_str(
        picture_formats, sizeof(picture_formats) / sizeof(str_map));
    camHal->qCamParams.set(QCameraParameters::KEY_SUPPORTED_PICTURE_FORMATS,
                    camHal->pictFormatValues.string());
    if(PICTURE_FORMAT_JPEG == camHal->pictFormat)
        camHal->qCamParams.setPictureFormat(QCameraParameters::PIXEL_FORMAT_JPEG);

    //Set picture quality
    sprintf(tempStr, "%d", camHal->pictJpegQlty);
    camHal->qCamParams.set(QCameraParameters::KEY_JPEG_QUALITY, tempStr);

    //Set Thumbnail size
    camHal->thumbnailSizeValues = create_sizes_str(
        thumbnail_sizes, sizeof(thumbnail_sizes) /sizeof(camera_size_type));
    camHal->qCamParams.set(QCameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES,
                    camHal->thumbnailSizeValues.string());
    sprintf(tempStr, "%d", camHal->thumbnailWidth);
    camHal->qCamParams.set(QCameraParameters::KEY_JPEG_THUMBNAIL_WIDTH,
                                                tempStr);
    sprintf(tempStr, "%d", camHal->thumbnailHeight);
    camHal->qCamParams.set(QCameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT,
                                                tempStr);

    //Set Thumbnail quality
    sprintf(tempStr, "%d", camHal->thumbnailJpegQlty);
    camHal->qCamParams.set(QCameraParameters::KEY_JPEG_THUMBNAIL_QUALITY,
                                                tempStr);

    //Set Preview Format
    camHal->prevFormatValues = create_values_str(
        preview_formats, sizeof(preview_formats) / sizeof(str_map));
    camHal->qCamParams.set(QCameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS,
        camHal->prevFormatValues.string());
    if(HAL_PIXEL_FORMAT_YCrCb_420_SP == camHal->prevFormat)
        camHal->qCamParams.setPreviewFormat(QCameraParameters::PIXEL_FORMAT_YUV420SP);

    //Set Preview size
    camHal->prevSizeValues = create_sizes_str(
        previewSizes,  sizeof(previewSizes) / sizeof(camera_size_type));
    camHal->qCamParams.set(QCameraParameters::KEY_SUPPORTED_PREVIEW_SIZES,
                    camHal->prevSizeValues.string());
    camHal->qCamParams.setPreviewSize(camHal->prevWidth, camHal->prevHeight);

    //Set Preivew fps range
    camHal->prevFpsRangesValues = create_fps_str(
        prevFpsRanges, sizeof(prevFpsRanges) / sizeof(android::FPSRange));

    camHal->qCamParams.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE,
                        camHal->prevFpsRangesValues);
    camHal->qCamParams.setPreviewFpsRange(MIN_PREV_FPS, MAX_PREV_FPS);

    ALOGD("%s: X", __func__);

    return rc;
} /* usbCamInitDefaultParameters */

/******************************************************************************
 * Function: usbCamSetParameters
 * Description: This function parses the parameter string and stores the
 *              parameters in the camera HAL handle
 *
 * Input parameters:
 *  camHal              - camera HAL handle
 *  params              - pointer to parameter string
 *
 * Return values:
 *      0   Success
 *      -1  Error
 * Notes: none
 *****************************************************************************/
int usbCamSetParameters(camera_hardware_t *camHal, const char *params)
{
    int             rc      = 0;
    String8         str     = String8(params);
    QCameraParameters qParam;

    ALOGD("%s: E", __func__);

    if(params)
        PRINT_PARAM_STR(params);

    qParam.unflatten(str);

    if(usbCamSetPrvwSize(camHal, qParam))
        rc = -1;
    if(usbCamSetPictSize(camHal, qParam))
        rc = -1;
    if(usbCamSetThumbnailSize(camHal, qParam))
        rc = -1;
    if(usbCamSetJpegQlty(camHal, qParam))
        rc = -1;

    ALOGD("%s: X", __func__);
    return rc;
} /* usbCamSetParameters */

/******************************************************************************
 * Function: usbCamGetParameters
 * Description: This function allocates memory for parameter string,
 *              composes and returns the parameter string
 *
 * Input parameters:
 *   camHal             - camera HAL handle
 *
 * Return values:
 *      Address to the parameter string
 *
 * Notes: none
 *****************************************************************************/
char* usbCamGetParameters(camera_hardware_t *camHal)
{
    ALOGD("%s: E", __func__);
    char *parms = NULL;
    char* rc = NULL;
    String8 str;

    QCameraParameters qParam = camHal->qCamParams;
    //qParam.dump();
    str = qParam.flatten( );
    rc = (char *)malloc(sizeof(char)*(str.length()+1));
    if(rc != NULL){
        memset(rc, 0, sizeof(char)*(str.length()+1));
        strncpy(rc, str.string(), str.length());
    rc[str.length()] = 0;
    parms = rc;
    }

    PRINT_PARAM_STR(parms);

    ALOGD("%s: X", __func__);
    return (parms);
} /* usbCamGetParameters */

/******************************************************************************
 * Function: usbCamPutParameters
 * Description: This function frees the memory allocated for parameter string
 *
 * Input parameters:
 *  camHal              - camera HAL handle
 *  parms               - Parameter string
 *
 * Return values:
 *      None
 *
 * Notes: none
 *****************************************************************************/
void usbCamPutParameters(camera_hardware_t *camHal, char *parms)
{
    ALOGD("%s: E", __func__);
    if(parms)
        free(parms);
    parms = NULL;
    ALOGD("%s: X", __func__);
} /* usbCamPutParameters */

/******************************************************************************
 * Function: create_sizes_str
 * Description: This function loops through /dev/video entries and probes with
 *              UVCIOC query. If the device responds to the query, then it is
 *              detected as UVC webcam
 * Input parameters:
 *   devname             - String pointer. The function return dev entry
 *                          name in this string
 * Return values:
 *      0   Success
 *      -1  Error
 * Notes: none
 *****************************************************************************/
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

/******************************************************************************
 * Function: create_values_str
 * Description: This function loops through /dev/video entries and probes with
 *              UVCIOC query. If the device responds to the query, then it is
 *              detected as UVC webcam
 * Input parameters:
 *   devname             - String pointer. The function return dev entry
 *                          name in this string
 * Return values:
 *      0   Success
 *      -1  Error
 * Notes: none
 *****************************************************************************/
static String8 create_values_str(const str_map *values, int len) {
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

/******************************************************************************
 * Function: create_fps_str
 * Description: This function loops through /dev/video entries and probes with
 *              UVCIOC query. If the device responds to the query, then it is
 *              detected as UVC webcam
 * Input parameters:
 *   devname             - String pointer. The function return dev entry
 *                          name in this string
 * Return values:
 *      0   Success
 *      -1  Error
 * Notes: none
 *****************************************************************************/
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

/******************************************************************************
 * Function: create_values_range_str
 * Description: This function loops through /dev/video entries and probes with
 *              UVCIOC query. If the device responds to the query, then it is
 *              detected as UVC webcam
 * Input parameters:
 *   devname             - String pointer. The function return dev entry
 *                          name in this string
 * Return values:
 *      0   Success
 *      -1  Error
 * Notes: none
 *****************************************************************************/
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

/******************************************************************************
 * Function: usbCamSetPrvwSize
 * Description: This function parses preview width and height from the input
 *              parameters and stores into the context
 *
 * Input parameters:
 *  camHal              - camera HAL handle
 *  params              - QCameraParameters reference
 *
 * Return values:
 *      0   If parameters are valid
 *      -1  If parameters are invalid
 *
 * Notes: none
 *****************************************************************************/
static int usbCamSetPrvwSize(   camera_hardware_t           *camHal,
                                const QCameraParameters&    params)
{
    int rc = 0, width, height, i, numPrvwSizes, validSize;
    ALOGD("%s: E", __func__);

    params.getPreviewSize(&width, &height);
    ALOGI("%s: Requested preview size %d x %d", __func__, width, height);

    // Validate the preview size
    numPrvwSizes = sizeof(previewSizes) / sizeof(camera_size_type);
    for (i = 0, validSize = 0; i <  numPrvwSizes; i++) {
        if (width ==  previewSizes[i].width
           && height ==  previewSizes[i].height) {
            validSize = 1;

            camHal->qCamParams.setPreviewSize(width, height);
            ALOGD("%s: setPreviewSize:  width: %d   height: %d",
                __func__, width, height);

            camHal->prevWidth   = width;
            camHal->prevHeight  = height;
            camHal->dispWidth   = width;
            camHal->dispHeight  = height;

            /* TBD: restrict pictures size and video to preview size */
        }
    }
    if(!validSize)
        ALOGE("%s: Invalid preview size %dx%d requested", __func__,
            width, height);

    rc = (validSize == 0)? -1:0;
    ALOGD("%s: X", __func__);

    return rc;
} /* usbCamSetPrvwSize */

/******************************************************************************
 * Function: usbCamSetPictSize
 * Description: This function parses picture width and height from the input
 *              parameters and stores into the context
 *
 * Input parameters:
 *  camHal              - camera HAL handle
 *  params              - QCameraParameters reference
 *
 * Return values:
 *      0   If parameters are valid
 *      -1  If parameters are invalid
 *
 * Notes: none
 *****************************************************************************/
static int usbCamSetPictSize(   camera_hardware_t           *camHal,
                                const QCameraParameters&    params)
{
    int rc = 0, width, height, i, numPictSizes, validSize;
    ALOGD("%s: E", __func__);

    /* parse for picture width and height */
    params.getPictureSize(&width, &height);
    ALOGI("%s: Requested picture size %d x %d", __func__, width, height);

    // Validate the picture size
    numPictSizes = sizeof(picture_sizes) / sizeof(camera_size_type);
    for (i = 0, validSize = 0; i <  numPictSizes; i++) {
        if (width ==  picture_sizes[i].width
           && height ==  picture_sizes[i].height) {
            validSize = 1;

            camHal->qCamParams.setPictureSize(width, height);
            ALOGD("%s: setPictureSize:  width: %d   height: %d",
                __func__, width, height);

            /* TBD: If new pictSize is different from old size, restart prvw */
            camHal->pictWidth   = width;
            camHal->pictHeight  = height;
        }
    }
    if(!validSize)
        ALOGE("%s: Invalid picture size %dx%d requested", __func__,
            width, height);
    rc = (validSize == 0)? -1:0;
    ALOGD("%s: X", __func__);

    return rc;
} /* usbCamSetPictSize */

/******************************************************************************
 * Function: usbCamSetThumbnailSize
 * Description: This function parses picture width and height from the input
 *              parameters and stores into the context
 *
 * Input parameters:
 *  camHal              - camera HAL handle
 *  params              - QCameraParameters reference
 *
 * Return values:
 *      0   If parameters are valid
 *      -1  If parameters are invalid
 *
 * Notes: none
 *****************************************************************************/
static int usbCamSetThumbnailSize(  camera_hardware_t           *camHal,
                                    const QCameraParameters&    params)
{
    int rc = 0, width, height, i, numThumbnailSizes, validSize;
    char tempStr[FILENAME_LENGTH];
    ALOGD("%s: E", __func__);

    /* parse for thumbnail width and height */
    width = params.getInt(QCameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
    height = params.getInt(QCameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
    ALOGI("%s: Requested thumbnail size %d x %d", __func__, width, height);

    // Validate the thumbnail size
    numThumbnailSizes = sizeof(thumbnail_sizes) / sizeof(camera_size_type);
    for (i = 0, validSize = 0; i <  numThumbnailSizes; i++) {
        if (width ==  thumbnail_sizes[i].width
           && height ==  thumbnail_sizes[i].height) {
            validSize = 1;

            camHal->thumbnailWidth   = width;
            camHal->thumbnailHeight  = height;
            sprintf(tempStr, "%d", camHal->thumbnailWidth);
            camHal->qCamParams.set(QCameraParameters::KEY_JPEG_THUMBNAIL_WIDTH,
                                                        width);
            sprintf(tempStr, "%d", camHal->thumbnailHeight);
            camHal->qCamParams.set(QCameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT,
                                                        height);

        }
    }
    if(!validSize)
        ALOGE("%s: Invalid picture size %dx%d requested", __func__,
            width, height);
    rc = (validSize == 0)? -1:0;
    ALOGD("%s: X", __func__);

    return rc;
} /* usbCamSetThumbnailSize */

/******************************************************************************
 * Function: usbCamSetJpegQlty
 * Description: This function parses picture and thumbnail JPEG quality and
 *              validates before storing in the context
 *
 * Input parameters:
 *  camHal              - camera HAL handle
 *  params              - QCameraParameters reference
 *
 * Return values:
 *      0   If parameters are valid
 *      -1  If parameters are invalid
 *
 * Notes: none
 *****************************************************************************/
static int usbCamSetJpegQlty(   camera_hardware_t           *camHal,
                                const QCameraParameters&    params)
{
    int rc = 0, quality = 0;
    char tempStr[FILENAME_LENGTH];
    ALOGD("%s: E", __func__);

    /**/
    quality = params.getInt(QCameraParameters::KEY_JPEG_QUALITY);
    ALOGI("%s: Requested picture qlty %d", __func__, quality);

    if (quality >= 0 && quality <= 100) {
        camHal->pictJpegQlty = quality;
        sprintf(tempStr, "%d", camHal->pictJpegQlty);
        camHal->qCamParams.set(QCameraParameters::KEY_JPEG_QUALITY, quality);
    } else {
        ALOGE("Invalid jpeg quality=%d", quality);
        rc = -1;
    }

    quality = params.getInt(QCameraParameters::KEY_JPEG_THUMBNAIL_QUALITY);
    ALOGI("%s: Requested thumbnail qlty %d", __func__, quality);

    if (quality >= 0 && quality <= 100) {
        camHal->thumbnailJpegQlty = quality;
        sprintf(tempStr, "%d", camHal->thumbnailJpegQlty);
        camHal->qCamParams.set(QCameraParameters::KEY_JPEG_THUMBNAIL_QUALITY,
                                    tempStr);
    } else {
        ALOGE("Invalid jpeg thumbnail quality=%d", quality);
        rc = -1;
    }

    ALOGD("%s: X rc:%d", __func__, rc);

    return rc;
}

}; /*namespace android */
