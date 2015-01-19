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

#ifndef ANDROID_HARDWARE_QCAMERA_USB_PARM_H
#define ANDROID_HARDWARE_QCAMERA_USB_PARM_H


#include <utils/threads.h>
#include <hardware/camera.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <utils/threads.h>
#include <cutils/properties.h>
#include <camera/Camera.h>
#include <camera/QCameraParameters.h>
#include <system/window.h>
#include <system/camera.h>
#include <hardware/camera.h>
#include <gralloc_priv.h>
#include <hardware/power.h>

extern "C" {
#include <linux/android_pmem.h>
#include <linux/msm_ion.h>
#include <camera.h>
#include <camera_defs_i.h>
} //extern C

//Error codes
#define NOT_FOUND       -1

/******************************************************************************
* Macro definitions
******************************************************************************/
/* enum definitions for picture formats */
static const int PICTURE_FORMAT_JPEG = 1;
static const int PICTURE_FORMAT_RAW = 2;

/* Default preview width in pixels */
#define DEFAULT_USBCAM_PRVW_WD  1280//640

/* Default preview height in pixels */
#define DEFAULT_USBCAM_PRVW_HT  720//480

/* Default picture format */
#define DEFAULT_USBCAM_PICT_FMT     PICTURE_FORMAT_JPEG

/* Default picture width in pixels */
#define DEFAULT_USBCAM_PICT_WD  640

/* Default picture height in pixels */
#define DEFAULT_USBCAM_PICT_HT  480

/* Default picture JPEG quality 0-100 */
#define DEFAULT_USBCAM_PICT_QLTY  85

/* Default thumbnail width in pixels */
#define DEFAULT_USBCAM_THUMBNAIL_WD    432

/* Default thumbnail height in pixels */
#define DEFAULT_USBCAM_THUMBNAIL_HT    288

/* Default thumbnail JPEG quality 0-100 */
#define DEFAULT_USBCAM_THUMBNAIL_QLTY  85

/* Default preview format */
#define DEFAULT_USBCAM_PRVW_FMT HAL_PIXEL_FORMAT_YCrCb_420_SP

/* minimum of the default preview fps range in milli-Hz */
#define MIN_PREV_FPS            5000

/* maximum of the default preview fps range in milli-Hz */
#define MAX_PREV_FPS            121000

//for histogram stats
#define HISTOGRAM_STATS_SIZE 257
#define NUM_HISTOGRAM_BUFFERS 3

namespace android {

/******************************************************************************
* Structure definitions
******************************************************************************/
typedef struct {
    uint32_t aspect_ratio;
    uint32_t width;
    uint32_t height;
} thumbnail_size_type;

/******************************************************************************
 * Function: usbCamInitDefaultParameters
 * Description: This function sets default parameters to camera HAL context
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *      0   No error
 *      -1  Error
 *
 * Notes: none
 *****************************************************************************/
int usbCamInitDefaultParameters(camera_hardware_t *camHal);

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
int usbCamSetParameters(camera_hardware_t *camHal, const char *params);

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
char* usbCamGetParameters(camera_hardware_t *camHal);

/******************************************************************************
 * Function: usbCamPutParameters
 * Description: This function frees the memory allocated for parameter string
 *
 * Input parameters:
 *  camHal              - camera HAL handle
 *  parms               - Parameter string
 *
 * Return values:
 *      0   Success
 *      -1  Error
 * Notes: none
 *****************************************************************************/
void usbCamPutParameters(camera_hardware_t *camHal, char *parms);

}; // namespace android

#endif /* ANDROID_HARDWARE_QCAMERA_USB_PARM_H */
