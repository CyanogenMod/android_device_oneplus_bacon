/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
 /*#error uncomment this for compiler test!*/
//#define ALOG_NDEBUG 0
#define ALOG_NIDEBUG 0
#define LOG_TAG "QualcommUsbCamera"

#include <utils/Log.h>
#include <utils/threads.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <pthread.h>
#include <linux/uvcvideo.h>

#include "QCameraHAL.h"
#include "QualcommUsbCamera.h"
#include "QCameraUsbPriv.h"
#include "QCameraMjpegDecode.h"
#include "QCameraUsbParm.h"
#include <gralloc_priv.h>
#include <genlock.h>

extern "C" {
#include <sys/time.h>
}

camera_device_ops_t usbcam_camera_ops = {
  set_preview_window:         android::usbcam_set_preview_window,
  set_callbacks:              android::usbcam_set_CallBacks,
  enable_msg_type:            android::usbcam_enable_msg_type,
  disable_msg_type:           android::usbcam_disable_msg_type,
  msg_type_enabled:           android::usbcam_msg_type_enabled,

  start_preview:              android::usbcam_start_preview,
  stop_preview:               android::usbcam_stop_preview,
  preview_enabled:            android::usbcam_preview_enabled,
  store_meta_data_in_buffers: android::usbcam_store_meta_data_in_buffers,

  start_recording:            android::usbcam_start_recording,
  stop_recording:             android::usbcam_stop_recording,
  recording_enabled:          android::usbcam_recording_enabled,
  release_recording_frame:    android::usbcam_release_recording_frame,

  auto_focus:                 android::usbcam_auto_focus,
  cancel_auto_focus:          android::usbcam_cancel_auto_focus,

  take_picture:               android::usbcam_take_picture,
  cancel_picture:             android::usbcam_cancel_picture,

  set_parameters:             android::usbcam_set_parameters,
  get_parameters:             android::usbcam_get_parameters,
  put_parameters:             android::usbcam_put_parameters,
  send_command:               android::usbcam_send_command,

  release:                    android::usbcam_release,
  dump:                       android::usbcam_dump,
};

#define CAPTURE                 1
#define DISPLAY                 1
#define CALL_BACK               1
#define MEMSET                  0
#define FREAD_JPEG_PICTURE      0
#define JPEG_ON_USB_CAMERA      1
#define FILE_DUMP_CAMERA        0
#define FILE_DUMP_B4_DISP       0

namespace android {

static int initUsbCamera(               camera_hardware_t *camHal,
                                        int width, int height,
                                        int pixelFormat);
static int startUsbCamCapture(          camera_hardware_t *camHal);
static int stopUsbCamCapture(           camera_hardware_t *camHal);
static int initV4L2mmap(                camera_hardware_t *camHal);
static int unInitV4L2mmap(              camera_hardware_t *camHal);
static int launch_preview_thread(       camera_hardware_t *camHal);
static int launchTakePictureThread(     camera_hardware_t *camHal);
static int initDisplayBuffers(          camera_hardware_t *camHal);
static int deInitDisplayBuffers(        camera_hardware_t *camHal);
static int stopPreviewInternal(         camera_hardware_t *camHal);
static int get_buf_from_cam(            camera_hardware_t *camHal);
static int put_buf_to_cam(              camera_hardware_t *camHal);
static int prvwThreadTakePictureInternal(camera_hardware_t *camHal);
static int get_buf_from_display( camera_hardware_t *camHal, int *buffer_id);
static int put_buf_to_display(   camera_hardware_t *camHal, int buffer_id);
static int convert_data_frm_cam_to_disp(camera_hardware_t *camHal, int buffer_id);
static void * previewloop(void *);
static void * takePictureThread(void *);
static int convert_YUYV_to_420_NV12(char *in_buf, char *out_buf, int wd, int ht);
static int get_uvc_device(char *devname);
static int getPreviewCaptureFmt(camera_hardware_t *camHal);
static int allocate_ion_memory(QCameraHalMemInfo_t *mem_info, int ion_type);
static int deallocate_ion_memory(QCameraHalMemInfo_t *mem_info);
static int ioctlLoop(int fd, int ioctlCmd, void *args);
static int readFromFile(char* fileName, char* buffer, int bufferSize);
static int fileDump(const char* fileName, char* data, int length, int* frm_cnt);
static int encodeJpeg(                  camera_hardware_t *camHal);
void jpegEncodeCb   (jpeg_job_status_t status,
                       uint8_t thumbnailDroppedFlag,
                       uint32_t client_hdl,
                       uint32_t jobId,
                       uint8_t* out_data,
                       uint32_t data_size,
                       void *userData);

/* HAL function implementation goes here*/

/**
 * The functions need to be provided by the camera HAL.
 *
 * If getNumberOfCameras() returns N, the valid cameraId for getCameraInfo()
 * and openCameraHardware() is 0 to N-1.
 */

extern "C" int usbcam_get_number_of_cameras()
{
    /* TBR: This is hardcoded currently to 1 USB camera */
    int numCameras = 1;
    ALOGI("%s: E", __func__);
    ALOGI("%s: X", __func__);

    return numCameras;
}

extern "C" int usbcam_get_camera_info(int camera_id, struct camera_info *info)
{
    int rc = -1;
    ALOGI("%s: E", __func__);

    /* TBR: This info is hardcoded currently irrespective of camera_id */
    if(info) {
        struct CameraInfo camInfo;
        memset(&camInfo, -1, sizeof (struct CameraInfo));

        info->facing = CAMERA_FACING_FRONT;//CAMERA_FACING_BACK;
        info->orientation = 0;
        rc = 0;
    }
    ALOGI("%s: X", __func__);
    return rc;
}

/* HAL should return NULL handle if it fails to open camera hardware. */
extern "C" int  usbcam_camera_device_open(
  const struct hw_module_t* module, const char* id,
          struct hw_device_t** hw_device)
{
    int rc = -1;
    camera_device       *device = NULL;
    camera_hardware_t   *camHal;
    char                *dev_name;

    ALOGI("%s: E", __func__);

    /* initialize return handle value to NULL */
    *hw_device = NULL;

    camHal = new camera_hardware_t();
    if(!camHal) {

            ALOGE("%s:  end in no mem", __func__);
            return -1;
    }

    rc = usbCamInitDefaultParameters(camHal);
    if(0 != rc)
    {
        ALOGE("%s: usbCamInitDefaultParameters error", __func__);
        return rc;
    }
#if CAPTURE

    dev_name = camHal->dev_name;

    rc = get_uvc_device(dev_name);
    if(rc || *dev_name == '\0'){
        ALOGE("%s: No UVC node found \n", __func__);
        return -1;
    }

    camHal->fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

    if (camHal->fd <  0) {
        ALOGE("%s: Cannot open '%s'", __func__, dev_name);
        free(camHal);
        rc = -1;
    }else{
        rc = 0;
    }

#else /* CAPTURE */
    rc = 0;
#endif /* CAPTURE */

    device                  = &camHal->hw_dev;
    device->common.close    = usbcam_close_camera_device;
    device->ops             = &usbcam_camera_ops;
    device->priv            = (void *)camHal;
    *hw_device              = &(device->common);

    ALOGD("%s: camHal: %p", __func__, camHal);
    ALOGI("%s: X %d", __func__, rc);

    return rc;
}

extern "C"  int usbcam_close_camera_device( hw_device_t *hw_dev)
{
    ALOGI("%s: device =%p E", __func__, hw_dev);
    int rc =  -1;
    camera_device_t *device     = (camera_device_t *)hw_dev;

    if(device) {
        camera_hardware_t *camHal   = (camera_hardware_t *)device->priv;
        if(camHal) {
            rc = close(camHal->fd);
            if(rc < 0) {
                ALOGE("%s: close failed ", __func__);
            }
            camHal->fd = 0;
            delete camHal;
        }else{
                ALOGE("%s: camHal is NULL pointer ", __func__);
        }
    }
    ALOGI("%s: X device =%p, rc = %d", __func__, hw_dev, rc);
    return rc;
}

int usbcam_set_preview_window(struct camera_device * device,
        struct preview_stream_ops *window)
{
    ALOGI("%s: E", __func__);
    int rc = 0;
    camera_hardware_t *camHal;

    VALIDATE_DEVICE_HDL(camHal, device, -1);
    Mutex::Autolock autoLock(camHal->lock);

    /* if window is already set, then de-init previous buffers */
    if(camHal->window){
        rc = deInitDisplayBuffers(camHal);
        if(rc < 0) {
            ALOGE("%s: deInitDisplayBuffers returned error", __func__);
        }
    }
    camHal->window = window;

    if(camHal->window){
        rc = initDisplayBuffers(camHal);
        if(rc < 0) {
            ALOGE("%s: initDisplayBuffers returned error", __func__);
        }
    }
    ALOGI("%s: X. rc = %d", __func__, rc);
    return rc;
}

void usbcam_set_CallBacks(struct camera_device * device,
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void *user)
{
    ALOGI("%s: E", __func__);
    camera_hardware_t *camHal;

    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return;
    }

    Mutex::Autolock autoLock(camHal->lock);

    camHal->notify_cb           = notify_cb;
    camHal->data_cb             = data_cb;
    camHal->data_cb_timestamp   = data_cb_timestamp;
    camHal->get_memory          = get_memory;
    camHal->cb_ctxt             = user;

    ALOGI("%s: X", __func__);
}

void usbcam_enable_msg_type(struct camera_device * device, int32_t msg_type)
{
    ALOGI("%s: E", __func__);
    ALOGI("%s: msg_type: %d", __func__, msg_type);

    camera_hardware_t *camHal;

    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return;
    }

    Mutex::Autolock autoLock(camHal->lock);

    camHal->msgEnabledFlag |= msg_type;

    ALOGI("%s: X", __func__);
}

void usbcam_disable_msg_type(struct camera_device * device, int32_t msg_type)
{
    ALOGI("%s: E", __func__);
    ALOGI("%s: msg_type: %d", __func__, msg_type);

    camera_hardware_t *camHal;
    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return;
    }

    Mutex::Autolock autoLock(camHal->lock);

    camHal->msgEnabledFlag &= ~msg_type;

    ALOGI("%s: X", __func__);
}

int usbcam_msg_type_enabled(struct camera_device * device, int32_t msg_type)
{
    ALOGI("%s: E", __func__);

    camera_hardware_t *camHal;
    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return -1;
    }

    Mutex::Autolock autoLock(camHal->lock);

    ALOGI("%s: X", __func__);
    return (camHal->msgEnabledFlag & msg_type);
}

int usbcam_start_preview(struct camera_device * device)
{
    ALOGI("%s: E", __func__);

    int rc = -1;
    camera_hardware_t *camHal = NULL;

    VALIDATE_DEVICE_HDL(camHal, device, -1);
    Mutex::Autolock autoLock(camHal->lock);

    /* If preivew is already running, nothing to be done */
    if(camHal->previewEnabledFlag){
        ALOGI("%s: Preview is already running", __func__);
        return 0;
    }

#if CAPTURE
    rc = initUsbCamera(camHal, camHal->prevWidth,
                        camHal->prevHeight, getPreviewCaptureFmt(camHal));
    if(rc < 0) {
        ALOGE("%s: Failed to intialize the device", __func__);
    }else{
        rc = startUsbCamCapture(camHal);
        if(rc < 0) {
            ALOGE("%s: Failed to startUsbCamCapture", __func__);
        }else{
            rc = launch_preview_thread(camHal);
            if(rc < 0) {
                ALOGE("%s: Failed to launch_preview_thread", __func__);
            }
        }
    }
#else /* CAPTURE */
    rc = launch_preview_thread(camHal);
    if(rc < 0) {
        ALOGE("%s: Failed to launch_preview_thread", __func__);
    }
#endif /* CAPTURE */
    /* if no errors, then set the flag */
    if(!rc)
        camHal->previewEnabledFlag = 1;

    ALOGD("%s: X", __func__);
    return rc;
}

void usbcam_stop_preview(struct camera_device * device)
{
    ALOGD("%s: E", __func__);

    int rc = 0;
    camera_hardware_t *camHal;

    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return;
    }

    Mutex::Autolock autoLock(camHal->lock);

    rc = stopPreviewInternal(camHal);
    if(rc)
        ALOGE("%s: stopPreviewInternal returned error", __func__);

    ALOGI("%s: X", __func__);
    return;
}

/* This function is equivalent to is_preview_enabled */
int usbcam_preview_enabled(struct camera_device * device)
{
    ALOGI("%s: E", __func__);
    camera_hardware_t *camHal;

    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return -1;
    }
    Mutex::Autolock autoLock(camHal->lock);

    ALOGI("%s: X", __func__);
    return camHal->previewEnabledFlag;
}

/* TBD */
int usbcam_store_meta_data_in_buffers(struct camera_device * device, int enable)
{
    ALOGI("%s: E", __func__);
    int rc = 0;

    ALOGI("%s: X", __func__);
    return rc;
}

/* TBD */
int usbcam_start_recording(struct camera_device * device)
{
    int rc = 0;
    ALOGD("%s: E", __func__);

    ALOGD("%s: X", __func__);

    return rc;
}

/* TBD */
void usbcam_stop_recording(struct camera_device * device)
{
    ALOGD("%s: E", __func__);

    ALOGD("%s: X", __func__);
}

/* TBD */
int usbcam_recording_enabled(struct camera_device * device)
{
    int rc = 0;
    ALOGD("%s: E", __func__);

    ALOGD("%s: X", __func__);
    return rc;
}

/* TBD */
void usbcam_release_recording_frame(struct camera_device * device,
                const void *opaque)
{
    ALOGV("%s: E", __func__);

    ALOGD("%s: X", __func__);
}

/* TBD */
int usbcam_auto_focus(struct camera_device * device)
{
    ALOGD("%s: E", __func__);
    int rc = 0;

    ALOGD("%s: X", __func__);
    return rc;
}

/* TBD */
int usbcam_cancel_auto_focus(struct camera_device * device)
{
    int rc = 0;
    ALOGD("%s: E", __func__);

    ALOGD("%s: X", __func__);
    return rc;
}

int usbcam_take_picture(struct camera_device * device)
{
    ALOGI("%s: E", __func__);
    int rc = 0;
    camera_hardware_t *camHal;

    VALIDATE_DEVICE_HDL(camHal, device, -1);

    Mutex::Autolock autoLock(camHal->lock);

    /* If take picture is already in progress, nothing t be done */
    if(camHal->takePictInProgress){
        ALOGI("%s: Take picture already in progress", __func__);
        return 0;
    }

    if(camHal->previewEnabledFlag)
    {
        rc = stopPreviewInternal(camHal);
        if(rc){
            ALOGE("%s: stopPreviewInternal returned error", __func__);
        }
        USB_CAM_CLOSE(camHal);
        camHal->prvwStoppedForPicture = 1;
    }
    /* TBD: Need to handle any dependencies on video recording state */
    rc = launchTakePictureThread(camHal);
    if(rc)
        ALOGE("%s: launchTakePictureThread error", __func__);

#if 0
    /* This implementation requests preview thread to take picture */
    if(camHal->previewEnabledFlag)
    {
        camHal->prvwCmdPending++;
        camHal->prvwCmd         = USB_CAM_PREVIEW_TAKEPIC;
        ALOGD("%s: Take picture command set ", __func__);
    }else{
        ALOGE("%s: Take picture without preview started!", __func__);
        rc = -1;
    }
#endif

    if(!rc)
        camHal->takePictInProgress = 1;

    ALOGI("%s: X", __func__);
    return rc;
}

/* TBD */
int usbcam_cancel_picture(struct camera_device * device)

{
    ALOGI("%s: E", __func__);
    int rc = 0;

    ALOGI("%s: X", __func__);
    return rc;
}

int usbcam_set_parameters(struct camera_device * device, const char *params)

{
    ALOGI("%s: E", __func__);
    int rc = 0;
    camera_hardware_t *camHal;

    VALIDATE_DEVICE_HDL(camHal, device, -1);

    Mutex::Autolock autoLock(camHal->lock);

    rc = usbCamSetParameters(camHal, params);

    ALOGI("%s: X", __func__);
    return rc;
}

char* usbcam_get_parameters(struct camera_device * device)
{
    char *parms;
    ALOGI("%s: E", __func__);

    camera_hardware_t *camHal;
    VALIDATE_DEVICE_HDL(camHal, device, NULL);

    Mutex::Autolock autoLock(camHal->lock);

    parms = usbCamGetParameters(camHal);

    ALOGI("%s: X", __func__);
    return parms;
}

void usbcam_put_parameters(struct camera_device * device, char *parm)

{
    ALOGI("%s: E", __func__);

    camera_hardware_t *camHal;

    if(device && device->priv){
        camHal = (camera_hardware_t *)device->priv;
    }else{
        ALOGE("%s: Null device or device->priv", __func__);
        return;
    }

    usbCamPutParameters(camHal, parm);

    ALOGI("%s: X", __func__);
    return;
}

/* TBD */
int usbcam_send_command(struct camera_device * device,
            int32_t cmd, int32_t arg1, int32_t arg2)
{
    int rc = 0;
    ALOGI("%s: E", __func__);
    ALOGI("%d", cmd);

    ALOGI("%s: X", __func__);
    return rc;
}

/* TBD */
void usbcam_release(struct camera_device * device)
{
    ALOGI("%s: E", __func__);
#if 0
    Mutex::Autolock l(&mLock);

    switch(mPreviewState) {
    case QCAMERA_HAL_PREVIEW_STOPPED:
        break;
    case QCAMERA_HAL_PREVIEW_START:
        break;
    case QCAMERA_HAL_PREVIEW_STARTED:
        stopPreviewInternal();
    break;
    case QCAMERA_HAL_RECORDING_STARTED:
        stopRecordingInternal();
        stopPreviewInternal();
        break;
    case QCAMERA_HAL_TAKE_PICTURE:
        cancelPictureInternal();
        break;
    default:
        break;
    }
    mPreviewState = QCAMERA_HAL_PREVIEW_STOPPED;
#endif
    ALOGI("%s: X", __func__);
}

/* TBD */
int usbcam_dump(struct camera_device * device, int fd)
{
    ALOGI("%s: E", __func__);
    int rc = 0;

    ALOGI("%s: X", __func__);
    return rc;
}
/*****************************************************************************
*  Static function definitions below
*****************************************************************************/

/******************************************************************************/
/* No in place conversion supported. Output buffer and input MUST should be   */
/* different input buffer for a 4x4 pixel video                             ***/
/******                  YUYVYUYV          00 01 02 03 04 05 06 07 ************/
/******                  YUYVYUYV          08 09 10 11 12 13 14 15 ************/
/******                  YUYVYUYV          16 17 18 19 20 21 22 23 ************/
/******                  YUYVYUYV          24 25 26 27 28 29 30 31 ************/
/******************************************************************************/
/* output generated by this function ******************************************/
/************************** YYYY            00 02 04 06            ************/
/************************** YYYY            08 10 12 14            ************/
/************************** YYYY            16 18 20 22            ************/
/************************** YYYY            24 26 28 30            ************/
/************************** VUVU            03 01 07 05            ************/
/************************** VUVU            19 17 23 21            ************/
/******************************************************************************/

static int convert_YUYV_to_420_NV12(char *in_buf, char *out_buf, int wd, int ht)
{
    int rc =0;
    int row, col, uv_row;

    ALOGD("%s: E", __func__);
    /* Arrange Y */
    for(row = 0; row < ht; row++)
        for(col = 0; col < wd * 2; col += 2)
        {
            out_buf[row * wd + col / 2] = in_buf[row * wd * 2 + col];
        }

    /* Arrange UV */
    for(row = 0, uv_row = ht; row < ht; row += 2, uv_row++)
        for(col = 1; col < wd * 2; col += 4)
        {
            out_buf[uv_row * wd + col / 2]= in_buf[row * wd * 2 + col + 2];
            out_buf[uv_row * wd + col / 2 + 1]  = in_buf[row * wd * 2 + col];
        }

    ALOGD("%s: X", __func__);
    return rc;
}

/******************************************************************************
 * Function: initDisplayBuffers
 * Description: This function initializes the preview buffers
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *      0   Success
 *      -1  Error
 * Notes: none
 *****************************************************************************/
static int initDisplayBuffers(camera_hardware_t *camHal)
{
    preview_stream_ops    *mPreviewWindow;
    struct ion_fd_data    ion_info_fd;
    int                   numMinUndequeuedBufs = 0;
    int                   rc = 0;
    int                   gralloc_usage = 0;
    int                   err;
    int                   color=30;

    ALOGD("%s: E", __func__);

#if DISPLAY
    if(camHal == NULL) {
        ALOGE("%s: camHal = NULL", __func__);
        return -1;
    }

    mPreviewWindow = camHal->window;
    if(!mPreviewWindow) {
        ALOGE("%s: mPreviewWindow = NULL", __func__);
        return -1;
    }

    /************************************************************************/
    /* - get_min_undequeued_buffer_count                                    */
    /* - set_buffer_count                                                   */
    /* - set_buffers_geometry                                               */
    /* - set_usage                                                          */
    /* - dequeue all the display buffers                                    */
    /* - cancel buffers: release w/o displaying                             */
    /************************************************************************/

    /************************************************************************/
    /* - get_min_undequeued_buffer_count                                    */
    /************************************************************************/
    if(mPreviewWindow->get_min_undequeued_buffer_count) {
        rc = mPreviewWindow->get_min_undequeued_buffer_count(
            mPreviewWindow, &numMinUndequeuedBufs);
        if (0 != rc) {
            ALOGE("%s: get_min_undequeued_buffer_count returned error", __func__);
        }
        else
            ALOGD("%s: get_min_undequeued_buffer_count returned: %d ",
               __func__, numMinUndequeuedBufs);
    }
    else
        ALOGE("%s: get_min_undequeued_buffer_count is NULL pointer", __func__);

    /************************************************************************/
    /* - set_buffer_count                                                   */
    /************************************************************************/
    if(mPreviewWindow->set_buffer_count) {
        camHal->previewMem.buffer_count = numMinUndequeuedBufs
                                            + PRVW_DISP_BUF_CNT;
        rc = mPreviewWindow->set_buffer_count(
            mPreviewWindow,
            camHal->previewMem.buffer_count);
        if (rc != 0) {
            ALOGE("%s: set_buffer_count returned error", __func__);
        }else
            ALOGD("%s: set_buffer_count returned success", __func__);
    }else
        ALOGE("%s: set_buffer_count is NULL pointer", __func__);

    /************************************************************************/
    /* - set_buffers_geometry                                               */
    /************************************************************************/
    if(mPreviewWindow->set_buffers_geometry) {
        rc = mPreviewWindow->set_buffers_geometry(mPreviewWindow,
                                                camHal->dispWidth,
                                                camHal->dispHeight,
                                                camHal->dispFormat);
        if (rc != 0) {
            ALOGE("%s: set_buffers_geometry returned error. %s (%d)",
               __func__, strerror(-rc), -rc);
        }else
            ALOGD("%s: set_buffers_geometry returned success", __func__);
    }else
        ALOGE("%s: set_buffers_geometry is NULL pointer", __func__);

    /************************************************************************/
    /* - set_usage                                                          */
    /************************************************************************/
    gralloc_usage = CAMERA_GRALLOC_HEAP_ID | CAMERA_GRALLOC_FALLBACK_HEAP_ID |
                    GRALLOC_USAGE_PRIVATE_UNCACHED;

    if(mPreviewWindow->set_usage) {
        rc = mPreviewWindow->set_usage(mPreviewWindow, gralloc_usage);
        if (rc != 0) {
            ALOGE("%s: set_usage returned error", __func__);
        }else
            ALOGD("%s: set_usage returned success", __func__);
    }
    else
        ALOGE("%s: set_usage is NULL pointer", __func__);

    /************************************************************************/
    /* - dequeue all the display buffers                                    */
    /************************************************************************/
    for (int cnt = 0; cnt < camHal->previewMem.buffer_count; cnt++) {
        int stride;
        err = mPreviewWindow->dequeue_buffer(
                mPreviewWindow,
                &camHal->previewMem.buffer_handle[cnt],
                &camHal->previewMem.stride[cnt]);
        if(!err) {
            ALOGD("%s: dequeue buf: %p\n",
                 __func__, camHal->previewMem.buffer_handle[cnt]);

            if(mPreviewWindow->lock_buffer) {
                err = mPreviewWindow->lock_buffer(
                    mPreviewWindow,
                    camHal->previewMem.buffer_handle[cnt]);
                ALOGD("%s: mPreviewWindow->lock_buffer success",
                     __func__);
            }

            // lock the buffer using genlock
            ALOGD("%s: camera call genlock_lock, hdl=%p",
                __func__, (*camHal->previewMem.buffer_handle[cnt]));

            if (GENLOCK_NO_ERROR !=
                genlock_lock_buffer(
                    (native_handle_t *) (*camHal->previewMem.buffer_handle[cnt]),
                    GENLOCK_WRITE_LOCK, GENLOCK_MAX_TIMEOUT))
            {
                ALOGE("%s: genlock_lock_buffer(WRITE) failed",
                    __func__);
                camHal->previewMem.local_flag[cnt] = BUFFER_UNLOCKED;
            }else {
                ALOGD("%s: genlock_lock_buffer hdl =%p",
                  __func__, *camHal->previewMem.buffer_handle[cnt]);
                camHal->previewMem.local_flag[cnt] = BUFFER_LOCKED;
            }

            /* Store this buffer details in the context */
            camHal->previewMem.private_buffer_handle[cnt] =
                (struct private_handle_t *) (*camHal->previewMem.buffer_handle[cnt]);

            ALOGD("%s: idx = %d, fd = %d, size = %d, offset = %d", __func__,
                cnt, camHal->previewMem.private_buffer_handle[cnt]->fd,
                camHal->previewMem.private_buffer_handle[cnt]->size,
                camHal->previewMem.private_buffer_handle[cnt]->offset);

            camHal->previewMem.camera_memory[cnt] =
                camHal->get_memory(
                    camHal->previewMem.private_buffer_handle[cnt]->fd,
                    camHal->previewMem.private_buffer_handle[cnt]->size,
                    1, camHal->cb_ctxt);

            ALOGD("%s: data = %p, size = %d, handle = %p", __func__,
                camHal->previewMem.camera_memory[cnt]->data,
                camHal->previewMem.camera_memory[cnt]->size,
                camHal->previewMem.camera_memory[cnt]->handle);

#ifdef USE_ION
            /* In case of ION usage, open ION fd */
            camHal->previewMem.mem_info[cnt].main_ion_fd =
                                                open("/dev/ion", O_RDONLY);
            if (camHal->previewMem.mem_info[cnt].main_ion_fd < 0) {
                ALOGE("%s: failed: could not open ion device\n", __func__);
            }else{
                memset(&ion_info_fd, 0, sizeof(ion_info_fd));
                ion_info_fd.fd =
                    camHal->previewMem.private_buffer_handle[cnt]->fd;
                if (ioctl(camHal->previewMem.mem_info[cnt].main_ion_fd,
                          ION_IOC_IMPORT, &ion_info_fd) < 0) {
                    ALOGE("ION import failed\n");
                }
            }
            camHal->previewMem.mem_info[cnt].fd =
                camHal->previewMem.private_buffer_handle[cnt]->fd;
            camHal->previewMem.mem_info[cnt].size =
                camHal->previewMem.private_buffer_handle[cnt]->size;
            camHal->previewMem.mem_info[cnt].handle = ion_info_fd.handle;

#endif
        }
        else
            ALOGE("%s: dequeue buf %d failed \n", __func__, cnt);
    }
    /************************************************************************/
    /* - cancel buffers: queue w/o displaying                               */
    /************************************************************************/
    for (int cnt = 0; cnt < camHal->previewMem.buffer_count; cnt++) {
        if (GENLOCK_FAILURE == genlock_unlock_buffer(
                (native_handle_t *)(*(camHal->previewMem.buffer_handle[cnt])))){
            ALOGE("%s: genlock_unlock_buffer failed: hdl =%p", __func__,
                (*(camHal->previewMem.buffer_handle[cnt])) );
        } else {
            camHal->previewMem.local_flag[cnt] = BUFFER_UNLOCKED;
            ALOGD("%s: genlock_unlock_buffer success: hdl = %p",
               __func__, (*(camHal->previewMem.buffer_handle[cnt])));
        }

        err = mPreviewWindow->cancel_buffer(mPreviewWindow,
            (buffer_handle_t *)camHal->previewMem.buffer_handle[cnt]);
        if(!err) {
            ALOGD("%s: cancel_buffer successful: %p\n",
                 __func__, camHal->previewMem.buffer_handle[cnt]);
        }else
            ALOGE("%s: cancel_buffer failed: %p\n", __func__,
                 camHal->previewMem.buffer_handle[cnt]);
    }
#else
    rc = 0;
#endif /* #if DISPLAY */
    ALOGD("%s: X", __func__);
    return rc;
}

/******************************************************************************
 * Function: deInitDisplayBuffers
 * Description: This function de-initializes all the display buffers allocated
 *              in initDisplayBuffers
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *      0   Success
 *      -1  Error
 * Notes: none
 *****************************************************************************/
static int deInitDisplayBuffers(camera_hardware_t *camHal)
{
    int rc = 0;
    preview_stream_ops    *previewWindow;

    ALOGD("%s: E", __func__);

    if(!camHal || !camHal->window) {
      ALOGE("%s: camHal = NULL or window = NULL ", __func__);
      return -1;
    }

    previewWindow = camHal->window;

    /************************************************************************/
    /* - Release all buffers that were acquired using get_memory            */
    /* - If using ION memory, free ION related resources                    */
    /* - genUnlock if buffer is genLocked                                   */
    /* - Cancel buffers: queue w/o displaying                               */
    /************************************************************************/

#if DISPLAY
    for (int cnt = 0; cnt < camHal->previewMem.buffer_count; cnt++) {

        /* Release all buffers that were acquired using get_memory */
        camHal->previewMem.camera_memory[cnt]->release(
                                camHal->previewMem.camera_memory[cnt]);

#ifdef USE_ION
        /* If using ION memory, free ION related resources */
        struct ion_handle_data ion_handle;
        memset(&ion_handle, 0, sizeof(ion_handle));
        ion_handle.handle = camHal->previewMem.mem_info[cnt].handle;
        if (ioctl(camHal->previewMem.mem_info[cnt].main_ion_fd,
            ION_IOC_FREE, &ion_handle) < 0) {
            ALOGE("%s: ion free failed\n", __func__);
        }
        close(camHal->previewMem.mem_info[cnt].main_ion_fd);
#endif

        /* genUnlock if buffer is genLocked */
        if(camHal->previewMem.local_flag[cnt] == BUFFER_LOCKED){
            if (GENLOCK_FAILURE == genlock_unlock_buffer(
                    (native_handle_t *)(*(camHal->previewMem.buffer_handle[cnt])))){
                ALOGE("%s: genlock_unlock_buffer failed: hdl =%p", __func__,
                    (*(camHal->previewMem.buffer_handle[cnt])) );
            } else {
                camHal->previewMem.local_flag[cnt] = BUFFER_UNLOCKED;
                ALOGD("%s: genlock_unlock_buffer success: hdl = %p",
                   __func__, (*(camHal->previewMem.buffer_handle[cnt])));
            }
        }
        /* cancel buffers: enqueue w/o displaying */
        rc = previewWindow->cancel_buffer(previewWindow,
            (buffer_handle_t *)camHal->previewMem.buffer_handle[cnt]);
        if(!rc) {
            ALOGD("%s: cancel_buffer successful: %p\n",
                 __func__, camHal->previewMem.buffer_handle[cnt]);
        }else
            ALOGE("%s: cancel_buffer failed: %p\n", __func__,
                 camHal->previewMem.buffer_handle[cnt]);
    }
#endif /* #if DISPLAY */
    memset(&camHal->previewMem, 0, sizeof(camHal->previewMem));

    ALOGD("%s: X", __func__);
    return rc;
}

/******************************************************************************
 * Function: getPreviewCaptureFmt
 * Description: This function implements the logic to decide appropriate
 *              capture format from the USB camera
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *      Capture format. Default (V4L2_PIX_FMT_MJPEG)
 *
 * Notes: none
 *****************************************************************************/
static int getPreviewCaptureFmt(camera_hardware_t *camHal)
{
    int     i = 0, mjpegSupported = 0, h264Supported = 0;
    struct v4l2_fmtdesc fmtdesc;

    memset(&fmtdesc, 0, sizeof(v4l2_fmtdesc));

    /************************************************************************/
    /* - Query the camera for all supported formats                         */
    /* - Based on the resolution, pick an apporpriate format                */
    /************************************************************************/

    /************************************************************************/
    /* - Query the camera for all supported formats                         */
    /************************************************************************/
    for(i = 0; ; i++) {
        fmtdesc.index = i;
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == ioctlLoop(camHal->fd, VIDIOC_ENUM_FMT, &fmtdesc)) {
            if (EINVAL == errno) {
                ALOGI("%s: Queried all formats till index %d\n", __func__, i);
                break;
            } else {
                ALOGE("%s: VIDIOC_ENUM_FMT failed", __func__);
            }
        }
        if(V4L2_PIX_FMT_MJPEG == fmtdesc.pixelformat){
            mjpegSupported = 1;
            ALOGI("%s: V4L2_PIX_FMT_MJPEG is supported", __func__ );
        }
        if(V4L2_PIX_FMT_H264 == fmtdesc.pixelformat){
            h264Supported = 1;
            ALOGI("%s: V4L2_PIX_FMT_H264 is supported", __func__ );
        }

    }

    /************************************************************************/
    /* - Based on the resolution, pick an apporpriate format                */
    /************************************************************************/
    //V4L2_PIX_FMT_MJPEG; V4L2_PIX_FMT_YUYV; V4L2_PIX_FMT_H264 = 0x34363248;
    camHal->captureFormat = V4L2_PIX_FMT_YUYV;
    if(camHal->prevWidth > 640){
        if(1 == mjpegSupported)
            camHal->captureFormat = V4L2_PIX_FMT_MJPEG;
        else if(1 == h264Supported)
            camHal->captureFormat = V4L2_PIX_FMT_H264;
    }
    ALOGI("%s: Capture format chosen: 0x%x. 0x%x:YUYV. 0x%x:MJPEG. 0x%x: H264",
        __func__, camHal->captureFormat, V4L2_PIX_FMT_YUYV,
        V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_H264);

    return camHal->captureFormat;
}

/******************************************************************************
 * Function: getMjpegdOutputFormat
 * Description: This function maps display pixel format enum to JPEG output
 *              format enum
 *
 * Input parameters:
 *   dispFormat              - Display pixel format
 *
 * Return values:
 *      (int)mjpegOutputFormat
 *
 * Notes: none
 *****************************************************************************/
static int getMjpegdOutputFormat(int dispFormat)
{
    int mjpegOutputFormat = YCRCBLP_H2V2;

    if(HAL_PIXEL_FORMAT_YCrCb_420_SP == dispFormat)
        mjpegOutputFormat = YCRCBLP_H2V2;

    return mjpegOutputFormat;
}

/******************************************************************************
 * Function: ioctlLoop
 * Description: This function is a blocking call around ioctl
 *
 * Input parameters:
 *   fd             - IOCTL fd
 *   ioctlCmd       - IOCTL command
 *   args           - IOCTL arguments
 *
 * Return values:
 *      (int)mjpegOutputFormat
 *
 * Notes: none
 *****************************************************************************/
static int ioctlLoop(int fd, int ioctlCmd, void *args)
{
    int rc = -1;

    while(1)
    {
        rc = ioctl(fd, ioctlCmd, args);
        if(!((-1 == rc) && (EINTR == errno)))
            break;
    }
    return rc;
}

/******************************************************************************
 * Function: initV4L2mmap
 * Description: This function requests for V4L2 driver allocated buffers
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int initV4L2mmap(camera_hardware_t *camHal)
{
    int rc = -1;
    struct v4l2_requestbuffers  reqBufs;
    struct v4l2_buffer          tempBuf;

    ALOGD("%s: E", __func__);
    memset(&reqBufs, 0, sizeof(v4l2_requestbuffers));
    reqBufs.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqBufs.memory  = V4L2_MEMORY_MMAP;
    reqBufs.count   = PRVW_CAP_BUF_CNT;

    if (-1 == ioctlLoop(camHal->fd, VIDIOC_REQBUFS, &reqBufs)) {
        if (EINVAL == errno) {
            ALOGE("%s: does not support memory mapping\n", __func__);
        } else {
            ALOGE("%s: VIDIOC_REQBUFS failed", __func__);
        }
    }
    ALOGD("%s: VIDIOC_REQBUFS success", __func__);

    if (reqBufs.count < PRVW_CAP_BUF_CNT) {
        ALOGE("%s: Insufficient buffer memory on\n", __func__);
    }

    camHal->buffers =
        ( bufObj* ) calloc(reqBufs.count, sizeof(bufObj));

    if (!camHal->buffers) {
        ALOGE("%s: Out of memory\n", __func__);
    }

    /* Store the indexes in the context. Useful during releasing */
    for (camHal->n_buffers = 0;
         camHal->n_buffers < reqBufs.count;
         camHal->n_buffers++) {

        memset(&tempBuf, 0, sizeof(tempBuf));

        tempBuf.index       = camHal->n_buffers;
        tempBuf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        tempBuf.memory      = V4L2_MEMORY_MMAP;

        if (-1 == ioctlLoop(camHal->fd, VIDIOC_QUERYBUF, &tempBuf))
            ALOGE("%s: VIDIOC_QUERYBUF failed", __func__);

        ALOGD("%s: VIDIOC_QUERYBUF success", __func__);

        camHal->buffers[camHal->n_buffers].len = tempBuf.length;
        camHal->buffers[camHal->n_buffers].data =
        mmap(NULL /* start anywhere */,
                  tempBuf.length,
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED,
                  camHal->fd, tempBuf.m.offset);

        if (MAP_FAILED == camHal->buffers[camHal->n_buffers].data)
            ALOGE("%s: mmap failed", __func__);
    }
    ALOGD("%s: X", __func__);
    return 0;
}

/******************************************************************************
 * Function: unInitV4L2mmap
 * Description: This function unmaps the V4L2 driver buffers
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int unInitV4L2mmap(camera_hardware_t *camHal)
{
    int i, rc = 0;
    ALOGD("%s: E", __func__);

    for (i = 0; i < camHal->n_buffers; i++)
        if (-1 == munmap(camHal->buffers[i].data, camHal->buffers[i].len)){
            ALOGE("%s: munmap failed for buffer: %d", __func__, i);
            rc = -1;
        }

    ALOGD("%s: X", __func__);
    return rc;
}

/******************************************************************************
 * Function: initUsbCamera
 * Description: This function sets the resolution and pixel format of the
 *              USB camera
 *
 * Input parameters:
 *  camHal              - camera HAL handle
 *  width               - picture width in pixels
 *  height              - picture height in pixels
 *  pixelFormat         - capture format for the camera
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int initUsbCamera(camera_hardware_t *camHal, int width, int height,
                        int pixelFormat)
{
    int     rc = -1;
    struct  v4l2_capability     cap;
    struct  v4l2_cropcap        cropcap;
    struct  v4l2_crop           crop;
    struct  v4l2_format         v4l2format;
    unsigned int                min;

    ALOGI("%s: E", __func__);

    if (-1 == ioctlLoop(camHal->fd, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            ALOGE( "%s: This is not V4L2 device\n", __func__);
            return -1;
        } else {
            ALOGE("%s: VIDIOC_QUERYCAP errno: %d", __func__, errno);
        }
    }
    ALOGD("%s: VIDIOC_QUERYCAP success", __func__);

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        ALOGE("%s: This is not video capture device\n", __func__);
        return -1;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        ALOGE("%s: This does not support streaming i/o\n", __func__);
        return -1;
    }

    /* Select video input, video standard and tune here. */
    memset(&cropcap, 0, sizeof(cropcap));

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == ioctlLoop(camHal->fd, VIDIOC_CROPCAP, &cropcap)) {

        /* reset to default */
        crop.c = cropcap.defrect;
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        ALOGD("%s: VIDIOC_CROPCAP success", __func__);
        if (-1 == ioctlLoop(camHal->fd, VIDIOC_S_CROP, &crop)) {
        switch (errno) {
            case EINVAL:
            /* Cropping not supported. */
                break;
            default:
            /* Errors ignored. */
                break;
            }
        }
                ALOGD("%s: VIDIOC_S_CROP success", __func__);

    } else {
        /* Errors ignored. */
               ALOGE("%s: VIDIOC_S_CROP failed", __func__);
    }


    memset(&v4l2format, 0, sizeof(v4l2format));

    v4l2format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    {
        v4l2format.fmt.pix.field       = V4L2_FIELD_NONE;
        v4l2format.fmt.pix.pixelformat = pixelFormat;
        v4l2format.fmt.pix.width       = width;
        v4l2format.fmt.pix.height      = height;

        if (-1 == ioctlLoop(camHal->fd, VIDIOC_S_FMT, &v4l2format))
        {
            ALOGE("%s: VIDIOC_S_FMT failed", __func__);
            return -1;
        }
        ALOGD("%s: VIDIOC_S_FMT success", __func__);

        /* Note VIDIOC_S_FMT may change width and height. */
    }

    /* TBR: In case of user pointer buffers, v4l2format.fmt.pix.sizeimage */
    /* might have to be calculated as per V4L2 sample application due to */
    /* open source driver bug */

    rc = initV4L2mmap(camHal);
    ALOGI("%s: X", __func__);
    return rc;
}

/******************************************************************************
 * Function: startUsbCamCapture
 * Description: This function queues buffer objects to the driver and sends
 *              STREAM ON command to the USB camera driver
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int startUsbCamCapture(camera_hardware_t *camHal)
{
    int         rc = -1;
    unsigned    int i;
    enum        v4l2_buf_type   v4l2BufType;
    ALOGD("%s: E", __func__);

    for (i = 0; i < camHal->n_buffers; ++i) {
        struct v4l2_buffer tempBuf;

        memset(&tempBuf, 0, sizeof(tempBuf));
        tempBuf.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        tempBuf.memory  = V4L2_MEMORY_MMAP;
        tempBuf.index   = i;

        if (-1 == ioctlLoop(camHal->fd, VIDIOC_QBUF, &tempBuf))
            ALOGE("%s: VIDIOC_QBUF for %d buffer failed", __func__, i);
        else
            ALOGD("%s: VIDIOC_QBUF for %d buffer success", __func__, i);
    }

    v4l2BufType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctlLoop(camHal->fd, VIDIOC_STREAMON, &v4l2BufType))
        ALOGE("%s: VIDIOC_STREAMON failed", __func__);
    else
    {
        ALOGD("%s: VIDIOC_STREAMON success", __func__);
        rc = 0;
    }

    ALOGD("%s: X", __func__);
    return rc;
}

/******************************************************************************
 * Function: stopUsbCamCapture
 * Description: This function sends STREAM OFF command to the USB camera driver
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int stopUsbCamCapture(camera_hardware_t *camHal)
{
    int         rc = -1;
    unsigned    int i;
    enum        v4l2_buf_type   v4l2BufType;
    ALOGD("%s: E", __func__);

    if(!camHal->fd){
        ALOGE("%s: camHal->fd = NULL ", __func__);
        return -1;
    }
    v4l2BufType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctlLoop(camHal->fd, VIDIOC_STREAMOFF, &v4l2BufType)){
        ALOGE("%s: VIDIOC_STREAMOFF failed", __func__);
        rc = -1;
    }else{
        ALOGD("%s: VIDIOC_STREAMOFF success", __func__);
        rc = 0;
    }

    ALOGD("%s: X", __func__);
    return rc;
}

/******************************************************************************
 * Function: stopPreviewInternal
 * Description: This function sends EXIT command to prview loop thread,
 *              stops usb camera capture and uninitializes MMAP. This function
 *              assumes that calling function has locked camHal->lock
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int stopPreviewInternal(camera_hardware_t *camHal)
{
    int rc = 0;
    ALOGD("%s: E", __func__);

    if(camHal->previewEnabledFlag)
    {
        camHal->prvwCmdPending++;
        camHal->prvwCmd         = USB_CAM_PREVIEW_EXIT;

        /* yield lock while waiting for the preview thread to exit */
        camHal->lock.unlock();
        if(pthread_join(camHal->previewThread, NULL)){
            ALOGE("%s: Error in pthread_join preview thread", __func__);
        }
        camHal->lock.lock();

        if(stopUsbCamCapture(camHal)){
            ALOGE("%s: Error in stopUsbCamCapture", __func__);
            rc = -1;
        }
        if(unInitV4L2mmap(camHal)){
            ALOGE("%s: Error in stopUsbCamCapture", __func__);
            rc = -1;
        }
        camHal->previewEnabledFlag = 0;
    }

    ALOGD("%s: X, rc: %d", __func__, rc);
    return rc;
}
#if 1
/******************************************************************************
 * Function: prvwThreadTakePictureInternal
 * Description: This function processes one camera frame to get JPEG encoded
 *              picture.
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int prvwThreadTakePictureInternal(camera_hardware_t *camHal)
{
    int     rc = 0;
    QCameraHalMemInfo_t     *mem_info;
    ALOGD("%s: E", __func__);

    /************************************************************************/
    /* - If requested for shutter notfication, callback                     */
    /* - Dequeue capture buffer from USB camera                             */
    /* - Send capture buffer to JPEG encoder for JPEG compression           */
    /* - If jpeg frames callback is requested, callback with jpeg buffers   */
    /* - Enqueue capture buffer back to USB camera                          */
    /************************************************************************/

    /************************************************************************/
    /* - If requested for shutter notfication, callback                     */
    /************************************************************************/
    if (camHal->msgEnabledFlag & CAMERA_MSG_SHUTTER){
        camHal->lock.unlock();
        camHal->notify_cb(CAMERA_MSG_SHUTTER, 0, 0, camHal->cb_ctxt);
        camHal->lock.lock();
    }

#if CAPTURE
    /************************************************************************/
    /* - Dequeue capture buffer from USB camera                             */
    /************************************************************************/
    if (0 == get_buf_from_cam(camHal))
        ALOGD("%s: get_buf_from_cam success", __func__);
    else
        ALOGE("%s: get_buf_from_cam error", __func__);
#endif

    /************************************************************************/
    /* - Send capture buffer to JPEG encoder for JPEG compression           */
    /************************************************************************/
    /* Optimization: If camera capture is JPEG format, need not compress! */
    /* instead, just data copy from capture buffer to picture buffer */
    if(V4L2_PIX_FMT_MJPEG == camHal->captureFormat){
        /* allocate heap memory for JPEG output */
        mem_info = &camHal->pictMem.mem_info[0];
        mem_info->size = camHal->curCaptureBuf.bytesused;
        /* TBD: allocate_ion_memory
        rc = QCameraHardwareInterface::allocate_ion_memory(mem_info,
                            ((0x1 << CAMERA_ZSL_ION_HEAP_ID) |
                            (0x1 << CAMERA_ZSL_ION_FALLBACK_HEAP_ID)));
        */
        if(rc)
            ALOGE("%s: ION memory allocation failed", __func__);

        camHal->pictMem.camera_memory[0] = camHal->get_memory(
                            mem_info->fd, mem_info->size, 1, camHal->cb_ctxt);
        if(!camHal->pictMem.camera_memory[0])
            ALOGE("%s: get_mem failed", __func__);

        memcpy( camHal->pictMem.camera_memory[0]->data,
                (char *)camHal->buffers[camHal->curCaptureBuf.index].data,
                camHal->curCaptureBuf.bytesused);
    }

    /************************************************************************/
    /* - If jpeg frames callback is requested, callback with jpeg buffers   */
    /************************************************************************/
    if ((camHal->msgEnabledFlag & CAMERA_MSG_COMPRESSED_IMAGE) &&
            (camHal->data_cb)){
        camHal->lock.unlock();
        camHal->data_cb(CAMERA_MSG_COMPRESSED_IMAGE,
                        camHal->pictMem.camera_memory[0],
                        0, NULL, camHal->cb_ctxt);
        camHal->lock.lock();
    }
    /* release heap memory after the call back */
    if(camHal->pictMem.camera_memory[0])
        camHal->pictMem.camera_memory[0]->release(
            camHal->pictMem.camera_memory[0]);

    /* TBD: deallocate_ion_memory */
    //rc = QCameraHardwareInterface::deallocate_ion_memory(mem_info);
    if(rc)
        ALOGE("%s: ION memory de-allocation failed", __func__);

#if CAPTURE
    /************************************************************************/
    /* - Enqueue capture buffer back to USB camera                          */
    /************************************************************************/
       if(0 == put_buf_to_cam(camHal)) {
            ALOGD("%s: put_buf_to_cam success", __func__);
        }
        else
            ALOGE("%s: put_buf_to_cam error", __func__);
#endif

    ALOGD("%s: X, rc: %d", __func__, rc);
    return rc;
}
#endif //#if 0
/******************************************************************************
 * Function: cache_ops
 * Description: This function calls ION ioctl for cache related operations
 *
 * Input parameters:
 *  mem_info                - QCameraHalMemInfo_t structure with ION info
 *  buf_ptr                 - Buffer pointer that needs to be cache operated
 *  cmd                     - Cache command - clean/invalidate
 *
 * Return values:
 *   MM_CAMERA_OK       No error
 *   -1                 Error
 *
 * Notes: none
 *****************************************************************************/
int cache_ops(QCameraHalMemInfo_t *mem_info,
                                    void *buf_ptr,
                                    unsigned int cmd)
{
    struct ion_flush_data cache_inv_data;
    struct ion_custom_data custom_data;
    int ret = MM_CAMERA_OK;

#ifdef USE_ION
    if (NULL == mem_info) {
        ALOGE("%s: mem_info is NULL, return here", __func__);
        return -1;
    }

    memset(&cache_inv_data, 0, sizeof(cache_inv_data));
    memset(&custom_data, 0, sizeof(custom_data));
    cache_inv_data.vaddr = buf_ptr;
    cache_inv_data.fd = mem_info->fd;
    cache_inv_data.handle = mem_info->handle;
    cache_inv_data.length = mem_info->size;
    custom_data.cmd = cmd;
    custom_data.arg = (unsigned long)&cache_inv_data;

    ALOGD("%s: addr = %p, fd = %d, handle = %p length = %d, ION Fd = %d",
         __func__, cache_inv_data.vaddr, cache_inv_data.fd,
         cache_inv_data.handle, cache_inv_data.length,
         mem_info->main_ion_fd);
    if(mem_info->main_ion_fd > 0) {
        if(ioctl(mem_info->main_ion_fd, ION_IOC_CUSTOM, &custom_data) < 0) {
            ALOGE("%s: Cache Invalidate failed\n", __func__);
            ret = -1;
        }
    }
#endif

    return ret;
}

/******************************************************************************
 * Function: get_buf_from_cam
 * Description: This funtions gets/acquires 1 capture buffer from the camera
 *              driver. The fetched buffer is stored in curCaptureBuf
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int get_buf_from_cam(camera_hardware_t *camHal)
{
    int rc = -1;

    ALOGD("%s: E", __func__);
    {
        memset(&camHal->curCaptureBuf, 0, sizeof(camHal->curCaptureBuf));

        camHal->curCaptureBuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        camHal->curCaptureBuf.memory = V4L2_MEMORY_MMAP;

        if (-1 == ioctlLoop(camHal->fd, VIDIOC_DQBUF, &camHal->curCaptureBuf)){
            switch (errno) {
            case EAGAIN:
                ALOGE("%s: EAGAIN error", __func__);
                return 1;

            case EIO:
            /* Could ignore EIO, see spec. */

            /* fall through */

            default:
            ALOGE("%s: VIDIOC_DQBUF error", __func__);
            }
        }
        else
        {
            rc = 0;
            ALOGD("%s: VIDIOC_DQBUF: %d successful, %d bytes",
                 __func__, camHal->curCaptureBuf.index,
                 camHal->curCaptureBuf.bytesused);
        }
    }
    ALOGD("%s: X", __func__);
    return rc;
}

/******************************************************************************
 * Function: put_buf_to_cam
 * Description: This funtion puts/releases 1 capture buffer back to the camera
 *              driver
 *
 * Input parameters:
 *   camHal              - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int put_buf_to_cam(camera_hardware_t *camHal)
{
    ALOGD("%s: E", __func__);

    camHal->curCaptureBuf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    camHal->curCaptureBuf.memory      = V4L2_MEMORY_MMAP;


    if (-1 == ioctlLoop(camHal->fd, VIDIOC_QBUF, &camHal->curCaptureBuf))
    {
        ALOGE("%s: VIDIOC_QBUF failed ", __func__);
        return 1;
    }
    ALOGD("%s: X", __func__);
    return 0;
}

/******************************************************************************
 * Function: put_buf_to_cam
 * Description: This funtion gets/acquires 1 display buffer from the display
 *              window
 *
 * Input parameters:
 *  camHal                  - camera HAL handle
 *  buffer_id               - Buffer id pointer. The id of buffer obtained
 *                              by this function is returned in this arg
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int get_buf_from_display(camera_hardware_t *camHal, int *buffer_id)
{
    int                     err = 0;
    preview_stream_ops      *mPreviewWindow = NULL;
    int                     stride = 0, cnt = 0;
    buffer_handle_t         *buffer_handle = NULL;
    struct private_handle_t *private_buffer_handle = NULL;

    ALOGD("%s: E", __func__);

    if (camHal == NULL) {
        ALOGE("%s: camHal = NULL", __func__);
        return -1;
    }

    mPreviewWindow = camHal->window;
    if( mPreviewWindow == NULL) {
        ALOGE("%s: mPreviewWindow = NULL", __func__);
        return -1;
    }
    err = mPreviewWindow->dequeue_buffer(mPreviewWindow,
                                    &buffer_handle,
                                    &stride);
    if(!err) {
        ALOGD("%s: dequeue buf buffer_handle: %p\n", __func__, buffer_handle);

        ALOGD("%s: mPreviewWindow->lock_buffer: %p",
             __func__, mPreviewWindow->lock_buffer);
        if(mPreviewWindow->lock_buffer) {
            err = mPreviewWindow->lock_buffer(mPreviewWindow, buffer_handle);
            ALOGD("%s: mPreviewWindow->lock_buffer success", __func__);
        }
        ALOGD("%s: camera call genlock_lock, hdl=%p",
             __func__, (*buffer_handle));

        if (GENLOCK_NO_ERROR !=
            genlock_lock_buffer((native_handle_t *)(*buffer_handle),
                                GENLOCK_WRITE_LOCK, GENLOCK_MAX_TIMEOUT)) {
           ALOGE("%s: genlock_lock_buffer(WRITE) failed", __func__);
       } else {
         ALOGD("%s: genlock_lock_buffer hdl =%p", __func__, *buffer_handle);
       }

        private_buffer_handle = (struct private_handle_t *)(*buffer_handle);

        ALOGD("%s: fd = %d, size = %d, offset = %d, stride = %d",
             __func__, private_buffer_handle->fd,
        private_buffer_handle->size, private_buffer_handle->offset, stride);

        for(cnt = 0; cnt < camHal->previewMem.buffer_count + 2; cnt++) {
            if(private_buffer_handle->fd ==
               camHal->previewMem.private_buffer_handle[cnt]->fd) {
                *buffer_id = cnt;
                ALOGD("%s: deQueued fd = %d, index: %d",
                     __func__, private_buffer_handle->fd, cnt);
                break;
            }
        }
    }
    else
        ALOGE("%s: dequeue buf failed \n", __func__);

    ALOGD("%s: X", __func__);

    return err;
}

/******************************************************************************
 * Function: put_buf_to_display
 * Description: This funtion puts/enqueues 1 buffer back to the display window
 *
 * Input parameters:
 *  camHal                  - camera HAL handle
 *  buffer_id               - id of the buffer that needs to be enqueued
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int put_buf_to_display(camera_hardware_t *camHal, int buffer_id)
{
    int err = 0;
    preview_stream_ops    *mPreviewWindow;

    ALOGD("%s: E", __func__);

    if (camHal == NULL) {
        ALOGE("%s: camHal = NULL", __func__);
        return -1;
    }

    mPreviewWindow = camHal->window;
    if( mPreviewWindow == NULL) {
        ALOGE("%s: mPreviewWindow = NULL", __func__);
        return -1;
    }

    if (GENLOCK_FAILURE ==
        genlock_unlock_buffer(
            (native_handle_t *)
            (*(camHal->previewMem.buffer_handle[buffer_id])))) {
       ALOGE("%s: genlock_unlock_buffer failed: hdl =%p",
            __func__, (*(camHal->previewMem.buffer_handle[buffer_id])) );
    } else {
      ALOGD("%s: genlock_unlock_buffer success: hdl =%p",
           __func__, (*(camHal->previewMem.buffer_handle[buffer_id])) );
    }

    /* Cache clean the output buffer so that cache is written back */
    cache_ops(&camHal->previewMem.mem_info[buffer_id],
                         (void *)camHal->previewMem.camera_memory[buffer_id]->data,
                         ION_IOC_CLEAN_CACHES);
                         /*
    cache_ops(&camHal->previewMem.mem_info[buffer_id],
                         (void *)camHal->previewMem.camera_memory[buffer_id]->data,
                         ION_IOC_CLEAN_INV_CACHES);
*/
    err = mPreviewWindow->enqueue_buffer(mPreviewWindow,
      (buffer_handle_t *)camHal->previewMem.buffer_handle[buffer_id]);
    if(!err) {
        ALOGD("%s: enqueue buf successful: %p\n",
             __func__, camHal->previewMem.buffer_handle[buffer_id]);
    }else
        ALOGE("%s: enqueue buf failed: %p\n",
             __func__, camHal->previewMem.buffer_handle[buffer_id]);

    ALOGD("%s: X", __func__);

    return err;
}

/******************************************************************************
 * Function: put_buf_to_display
 * Description: This funtion transfers the content from capture buffer to
 *              preiew display buffer after appropriate conversion
 *
 * Input parameters:
 *  camHal                  - camera HAL handle
 *  buffer_id               - id of the buffer that needs to be enqueued
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int convert_data_frm_cam_to_disp(camera_hardware_t *camHal, int buffer_id)
{
    int rc = -1;

    if(!camHal) {
        ALOGE("%s: camHal is NULL", __func__);
        return -1;
    }
    /* If input and output are raw formats, but different color format, */
    /* call color conversion routine                                    */
    if( (V4L2_PIX_FMT_YUYV == camHal->captureFormat) &&
        (HAL_PIXEL_FORMAT_YCrCb_420_SP == camHal->dispFormat))
    {
        convert_YUYV_to_420_NV12(
            (char *)camHal->buffers[camHal->curCaptureBuf.index].data,
            (char *)camHal->previewMem.camera_memory[buffer_id]->data,
            camHal->prevWidth,
            camHal->prevHeight);
        ALOGD("%s: Copied %d bytes from camera buffer %d to display buffer: %d",
             __func__, camHal->curCaptureBuf.bytesused,
             camHal->curCaptureBuf.index, buffer_id);
        rc = 0;
    }

    /* If camera buffer is MJPEG encoded, call mjpeg decode call */
    if(V4L2_PIX_FMT_MJPEG == camHal->captureFormat)
    {
        if(NULL == camHal->mjpegd)
        {
            rc = mjpegDecoderInit(&camHal->mjpegd);
            if(rc < 0)
                ALOGE("%s: mjpegDecoderInit Error: %d", __func__, rc);
        }
        if(camHal->mjpegd)
        {
            rc = mjpegDecode(
                (void*)camHal->mjpegd,
                (char *)camHal->buffers[camHal->curCaptureBuf.index].data,
                camHal->curCaptureBuf.bytesused,
                (char *)camHal->previewMem.camera_memory[buffer_id]->data,
                (char *)camHal->previewMem.camera_memory[buffer_id]->data +
                    camHal->prevWidth * camHal->prevHeight,
                getMjpegdOutputFormat(camHal->dispFormat));
            if(rc < 0)
                ALOGE("%s: mjpegDecode Error: %d", __func__, rc);
        }
    }
    return rc;
}

/******************************************************************************
 * Function: launch_preview_thread
 * Description: This is a wrapper function to start preview thread
 *
 * Input parameters:
 *  camHal                  - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int launch_preview_thread(camera_hardware_t *camHal)
{
    ALOGD("%s: E", __func__);
    int rc = 0;

    if(!camHal) {
        ALOGE("%s: camHal is NULL", __func__);
        return -1;
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&camHal->previewThread, &attr, previewloop, camHal);

    ALOGD("%s: X", __func__);
    return rc;
}

/******************************************************************************
 * Function: launch_preview_thread
 * Description: This is thread funtion for preivew loop
 *
 * Input parameters:
 *  hcamHal                 - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static void * previewloop(void *hcamHal)
{
    int                 rc;
    int                 buffer_id   = 0;
    pid_t               tid         = 0;
    camera_hardware_t   *camHal     = NULL;
    int                 msgType     = 0;
    camera_memory_t     *data       = NULL;
    camera_frame_metadata_t *metadata= NULL;
    camera_memory_t     *previewMem = NULL;

    camHal = (camera_hardware_t *)hcamHal;
    ALOGD("%s: E", __func__);

    if(!camHal) {
        ALOGE("%s: camHal is NULL", __func__);
        return NULL ;
    }

    tid  = gettid();
    /* TBR: Set appropriate thread priority */
    androidSetThreadPriority(tid, ANDROID_PRIORITY_NORMAL);
    prctl(PR_SET_NAME, (unsigned long)"Camera HAL preview thread", 0, 0, 0);

    /************************************************************************/
    /* - Time wait (select) on camera fd for input read buffer              */
    /* - Check if any preview thread commands are set. If set, process      */
    /* - Dequeue display buffer from surface                                */
    /* - Dequeue capture buffer from USB camera                             */
    /* - Convert capture format to display format                           */
    /* - If preview frames callback is requested, callback with prvw buffers*/
    /* - Enqueue display buffer back to surface                             */
    /* - Enqueue capture buffer back to USB camera                          */
    /************************************************************************/
    while(1) {
        fd_set fds;
        struct timeval tv;
        int r = 0;

        FD_ZERO(&fds);
#if CAPTURE
        FD_SET(camHal->fd, &fds);
#endif /* CAPTURE */

    /************************************************************************/
    /* - Time wait (select) on camera fd for input read buffer              */
    /************************************************************************/
        tv.tv_sec = 0;
        tv.tv_usec = 500000;

        ALOGD("%s: b4 select on camHal->fd + 1,fd: %d", __func__, camHal->fd);
#if CAPTURE
        r = select(camHal->fd + 1, &fds, NULL, NULL, &tv);
#else
        r = select(1, NULL, NULL, NULL, &tv);
#endif /* CAPTURE */
        ALOGD("%s: after select : %d", __func__, camHal->fd);

        if (-1 == r) {
            if (EINTR == errno)
                continue;
            ALOGE("%s: FDSelect error: %d", __func__, errno);
        }

        if (0 == r) {
            ALOGD("%s: select timeout\n", __func__);
        }

        /* Protect the context for one iteration of preview loop */
        /* this gets unlocked at the end of the while */
        Mutex::Autolock autoLock(camHal->lock);

    /************************************************************************/
    /* - Check if any preview thread commands are set. If set, process      */
    /************************************************************************/
        if(camHal->prvwCmdPending)
        {
            /* command is serviced. Hence command pending = 0  */
            camHal->prvwCmdPending--;
            //sempost(ack)
            if(USB_CAM_PREVIEW_EXIT == camHal->prvwCmd){
                /* unlock before exiting the thread */
                camHal->lock.unlock();
                ALOGI("%s: Exiting coz USB_CAM_PREVIEW_EXIT", __func__);
                return (void *)0;
            }else if(USB_CAM_PREVIEW_TAKEPIC == camHal->prvwCmd){
                rc = prvwThreadTakePictureInternal(camHal);
                if(rc)
                    ALOGE("%s: prvwThreadTakePictureInternal returned error",
                    __func__);
            }
        }

        /* Null check on preview window. If null, sleep */
        if(!camHal->window) {
            ALOGD("%s: sleeping coz camHal->window = NULL",__func__);
            camHal->lock.unlock();
            sleep(2);
            continue;
        }
#if DISPLAY
    /************************************************************************/
    /* - Dequeue display buffer from surface                                */
    /************************************************************************/
        if(0 == get_buf_from_display(camHal, &buffer_id)) {
            ALOGD("%s: get_buf_from_display success: %d",
                 __func__, buffer_id);
        }else{
            ALOGE("%s: get_buf_from_display failed. Skipping the loop",
                 __func__);
            continue;
        }
#endif

#if CAPTURE
    /************************************************************************/
    /* - Dequeue capture buffer from USB camera                             */
    /************************************************************************/
        if (0 == get_buf_from_cam(camHal))
            ALOGD("%s: get_buf_from_cam success", __func__);
        else
            ALOGE("%s: get_buf_from_cam error", __func__);
#endif

#if FILE_DUMP_CAMERA
        /* Debug code to dump frames from camera */
        {
            static int frame_cnt = 0;
            /* currently hardcoded for Bytes-Per-Pixel = 1.5 */
            fileDump("/data/USBcam.yuv",
            (char*)camHal->buffers[camHal->curCaptureBuf.index].data,
            camHal->prevWidth * camHal->prevHeight * 1.5,
            &frame_cnt);
        }
#endif

#if MEMSET
        static int color = 30;
        color += 50;
        if(color > 200) {
            color = 30;
        }
        ALOGE("%s: Setting to the color: %d\n", __func__, color);
        /* currently hardcoded for format of type Bytes-Per-Pixel = 1.5 */
        memset(camHal->previewMem.camera_memory[buffer_id]->data,
               color, camHal->dispWidth * camHal->dispHeight * 1.5 + 2 * 1024);
#else
        convert_data_frm_cam_to_disp(camHal, buffer_id);
        ALOGD("%s: Copied data to buffer_id: %d", __func__, buffer_id);
#endif

#if FILE_DUMP_B4_DISP
        /* Debug code to dump display buffers */
        {
            static int frame_cnt = 0;
            /* currently hardcoded for Bytes-Per-Pixel = 1.5 */
            fileDump("/data/display.yuv",
                (char*) camHal->previewMem.camera_memory[buffer_id]->data,
                camHal->dispWidth * camHal->dispHeight * 1.5,
                &frame_cnt);
            ALOGD("%s: Written buf_index: %d ", __func__, buffer_id);
        }
#endif

#if DISPLAY
    /************************************************************************/
    /* - Enqueue display buffer back to surface                             */
    /************************************************************************/
       if(0 == put_buf_to_display(camHal, buffer_id)) {
            ALOGD("%s: put_buf_to_display success: %d", __func__, buffer_id);
        }
        else
            ALOGE("%s: put_buf_to_display error", __func__);
#endif

#if CAPTURE
     /************************************************************************/
    /* - Enqueue capture buffer back to USB camera                          */
    /************************************************************************/
       if(0 == put_buf_to_cam(camHal)) {
            ALOGD("%s: put_buf_to_cam success", __func__);
        }
        else
            ALOGE("%s: put_buf_to_cam error", __func__);
#endif

#if CALL_BACK
    /************************************************************************/
    /* - If preview frames callback is requested, callback with prvw buffers*/
    /************************************************************************/
        /* TBD: change the 1.5 hardcoding to Bytes Per Pixel */
        int previewBufSize = camHal->prevWidth * camHal->prevHeight * 1.5;

        msgType |=  CAMERA_MSG_PREVIEW_FRAME;

        if(previewBufSize !=
            camHal->previewMem.private_buffer_handle[buffer_id]->size) {

            previewMem = camHal->get_memory(
                camHal->previewMem.private_buffer_handle[buffer_id]->fd,
                previewBufSize,
                1,
                camHal->cb_ctxt);

              if (!previewMem || !previewMem->data) {
                  ALOGE("%s: get_memory failed.\n", __func__);
              }
              else {
                  data = previewMem;
                  ALOGD("%s: GetMemory successful. data = %p",
                            __func__, data);
                  ALOGD("%s: previewBufSize = %d, priv_buf_size: %d",
                    __func__, previewBufSize,
                    camHal->previewMem.private_buffer_handle[buffer_id]->size);
              }
        }
        else{
            data =   camHal->previewMem.camera_memory[buffer_id];
            ALOGD("%s: No GetMemory, no invalid fmt. data = %p, idx=%d",
                __func__, data, buffer_id);
        }
        /* Unlock and lock around the callback. */
        /* Sometimes 'disable_msg' is issued in the callback context, */
        /* leading to deadlock */
        camHal->lock.unlock();
        if((camHal->msgEnabledFlag & CAMERA_MSG_PREVIEW_FRAME) &&
            camHal->data_cb){
            ALOGD("%s: before data callback", __func__);
            camHal->data_cb(msgType, data, 0,metadata, camHal->cb_ctxt);
            ALOGD("%s: after data callback: %p", __func__, camHal->data_cb);
        }
        camHal->lock.lock();
        if (previewMem)
            previewMem->release(previewMem);
#endif

    }//while(1)
    ALOGD("%s: X", __func__);
    return (void *)0;
}

/******************************************************************************
 * Function: get_uvc_device
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
static int get_uvc_device(char *devname)
{
    char    temp_devname[FILENAME_LENGTH];
    FILE    *fp = NULL;
    int     i = 0, ret = 0, fd;

    ALOGD("%s: E", __func__);
#if 1
    strncpy(devname, "/dev/video1", FILENAME_LENGTH);

/*
    struct          stat st;

    strncpy(dev_name, "/dev/video1", FILENAME_LENGTH);
    if (-1 == stat(dev_name, &st)) {
        ALOGE("%s: Cannot identify '%s': %d, %s\n",
             __func__, dev_name, errno, strerror(errno));
    }

    if (!S_ISCHR(st.st_mode)) {
        ALOGE("%s: %s is no device\n", __func__, dev_name);
        rc = -1;
    }
*/

#else

    *devname = '\0';
    /************************************************************************/
    /* - List all /dev/video* entries to a file                             */
    /* - Open the video list file and loop through the list                 */
    /* - Send UVC specific control query and check the response             */
    /* - If device responds to the query as success, device is UVC webcam   */
    /************************************************************************/

    /************************************************************************/
    /* - List all /dev/video* entries to a file                             */
    /************************************************************************/
    /* Temporarily commented out. This logic doesnt seem to be working */
    //system("ls > /data/video_dev_list");

    /************************************************************************/
    /* - Open the video list file and loop through the list                 */
    /************************************************************************/

    /* Temporarily commented out. This logic doesnt seem to be working */
    /*
    fp = fopen("/data/video_dev_list", "rb");
    if(!fp) {
        ALOGE("%s: Error in opening /data/video_dev_list ", __func__);
        return -1;
    }
    */

    /* Temporarily commented out. Looping logic changed due to issue in */
    /* executing system("ls > /data/video_dev_list") */
    //while(EOF != fscanf(fp, "%s", devname)){
    while(1){
        uvc_xu_control_query    xqry;

        sprintf(temp_devname, "/dev/video%d", i);
        ALOGD("%s: Probing %s \n", __func__, temp_devname);

        fd = open(temp_devname, O_RDWR /* required */ | O_NONBLOCK, 0);
        if(-1 != fd){
            memset(&xqry, 0, sizeof(uvc_xu_control_query));
            ret = ioctl(fd, UVCIOC_CTRL_QUERY, &xqry);
            ALOGD("%s: UVCIOC ret: %d, errno: %d", __func__, ret, errno);
            /****************************************************************/
            /* if UVCIOC is executed successfully, ret = 0                  */
            /* if UVCIOC is executed but Control Unit = 0 does not exist,   */
            /*      ret = -1 and errno = ENOENT                             */
            /* if UVCIOC doesnot execute, ret = -1 and errno = EINVAL       */
            /****************************************************************/
            if((0 == ret) || (ret && (ENOENT == errno))){
                ALOGD("%s: Found UVC node: %s\n", __func__, temp_devname);
                strncpy(devname, temp_devname, FILENAME_LENGTH);
                /* Exit the loop at the first UVC node detection */
                break;
            }
            close(fd);
        }
        /* Temporarily logic to probe video0 to video10 nodes */
        if(i++ > 10)
        {
            if(fp)
                fclose(fp);
            break;
        }
    }
#endif /* #if 0 */
    ALOGD("%s: X", __func__);
    return 0;
} /* get_uvc_device */

/******************************************************************************
 * Function: fileDump
 * Description: This is a utility function to dump buffers into a file
 *
 * Input parameters:
 *  fn              - File name string
 *  data            - pointer to character buffer that needs to be dumped
 *  length          - Length of the buffer to be dumped
 *  frm_cnt         - Pointer to frame count. This count is incremented by this
 *                      function on successful file write
 * Return values:
 *      0   Success
 *      -1  Error
 * Notes: none
 *****************************************************************************/
static int fileDump(const char* fn, char* data, int length, int* frm_cnt)
{

    FILE *fp = NULL;
    if (0 == *frm_cnt) {
        fp = fopen(fn, "wb");
        if (NULL == fp) {
            ALOGE("%s: Error in opening %s", __func__, fn);
        }
        fclose(fp);
    }
    fp = fopen(fn, "ab");
    if (NULL == fp) {
        ALOGE("%s: Error in opening %s", __func__, fn);
    }
    fwrite(data, 1, length, fp);
    fclose(fp);
    (*frm_cnt)++;
    ALOGD("%s: Written %d bytes for frame:%d, in %s",
        __func__, length, *frm_cnt, fn);

    return 0;
}

/******************************************************************************
 * Function: launchTakePictureThread
 * Description: This is a wrapper function to start take picture thread
 *
 * Input parameters:
 *  camHal                  - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int launchTakePictureThread(camera_hardware_t *camHal)
{
    ALOGD("%s: E", __func__);
    int rc = 0;

    if(!camHal) {
        ALOGE("%s: camHal is NULL", __func__);
        return -1;
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    /* create the thread in detatched state, when the thread exits all */
    /* memory resources are freed up */
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&camHal->takePictureThread, &attr, takePictureThread, camHal);

    ALOGD("%s: X", __func__);
    return rc;
}

/******************************************************************************
 * Function: takePictureThread
 * Description: This function is associated with take picture thread
 *
 * Input parameters:
 *  camHal                  - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static void * takePictureThread(void *hcamHal)
{
    int                 rc = 0;
    int                 buffer_id   = 0;
    pid_t               tid         = 0;
    camera_hardware_t   *camHal     = NULL;
    int                 msgType     = 0;
    int                 jpegLength  = 0;
    QCameraHalMemInfo_t *mem_info   = NULL;

    camHal = (camera_hardware_t *)hcamHal;
    ALOGI("%s: E", __func__);

    if(!camHal) {
        ALOGE("%s: camHal is NULL", __func__);
        return NULL ;
    }

    tid  = gettid();
    /* TBR: Set appropriate thread priority */
    androidSetThreadPriority(tid, ANDROID_PRIORITY_NORMAL);
    prctl(PR_SET_NAME, (unsigned long)"Camera HAL preview thread", 0, 0, 0);

    /************************************************************************/
    /* - If requested for shutter notfication, notify                       */
    /* - Initialize USB camera with snapshot parameters                     */
    /* - Time wait (select) on camera fd for camera frame availability      */
    /* - Dequeue capture buffer from USB camera                             */
    /* - Send capture buffer to JPEG encoder for JPEG compression           */
    /* - If jpeg frames callback is requested, callback with jpeg buffers   */
    /* - Enqueue capture buffer back to USB camera                          */
    /* - Free USB camera resources and close camera                         */
    /* - If preview was stopped for taking picture, restart the preview     */
    /************************************************************************/

    Mutex::Autolock autoLock(camHal->lock);
    /************************************************************************/
    /* - If requested for shutter notfication, notify                       */
    /************************************************************************/
#if 0 /* TBD: Temporarily commented out due to an issue. Sometimes it takes */
    /* long time to get back the lock once unlocked and notify callback */
    if (camHal->msgEnabledFlag & CAMERA_MSG_SHUTTER){
        camHal->lock.unlock();
        camHal->notify_cb(CAMERA_MSG_SHUTTER, 0, 0, camHal->cb_ctxt);
        camHal->lock.lock();
    }
#endif
    /************************************************************************/
    /* - Initialize USB camera with snapshot parameters                     */
    /************************************************************************/
    USB_CAM_OPEN(camHal);

#if JPEG_ON_USB_CAMERA
    rc = initUsbCamera(camHal, camHal->pictWidth, camHal->pictHeight,
                        V4L2_PIX_FMT_MJPEG);
#else
    rc = initUsbCamera(camHal, camHal->pictWidth, camHal->pictHeight,
                        V4L2_PIX_FMT_YUYV);
#endif
    ERROR_CHECK_EXIT_THREAD(rc, "initUsbCamera");

    rc = startUsbCamCapture(camHal);
    ERROR_CHECK_EXIT_THREAD(rc, "startUsbCamCapture");

    /************************************************************************/
    /* - Time wait (select) on camera fd for camera frame availability      */
    /************************************************************************/
    {
        fd_set fds;
        struct timeval tv;
        int r = 0;

        FD_ZERO(&fds);
        FD_SET(camHal->fd, &fds);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        do{
            ALOGD("%s: b4 select on camHal->fd : %d", __func__, camHal->fd);
            r = select(camHal->fd + 1, &fds, NULL, NULL, &tv);
            ALOGD("%s: after select", __func__);
        }while((0 == r) || ((-1 == r) && (EINTR == errno)));

        if ((-1 == r) && (EINTR != errno)){
            ALOGE("%s: FDSelect ret = %d error: %d", __func__, r, errno);
            return (void *)-1;
        }

    }
    /************************************************************************/
    /* - Dequeue capture buffer from USB camera                             */
    /************************************************************************/
    if (0 == get_buf_from_cam(camHal))
        ALOGD("%s: get_buf_from_cam success", __func__);
    else
        ALOGE("%s: get_buf_from_cam error", __func__);

    /************************************************************************/
    /* - Send capture buffer to JPEG encoder for JPEG compression           */
    /************************************************************************/
    mem_info = &camHal->pictMem.mem_info[0];
    mem_info->size = MAX_JPEG_BUFFER_SIZE;

    rc = allocate_ion_memory(mem_info,
                        ((0x1 << CAMERA_ZSL_ION_HEAP_ID) |
                        (0x1 << CAMERA_ZSL_ION_FALLBACK_HEAP_ID)));
    if(rc)
        ALOGE("%s: ION memory allocation failed", __func__);

    camHal->pictMem.camera_memory[0] = camHal->get_memory(
                        mem_info->fd, mem_info->size, 1, camHal->cb_ctxt);
    if(!camHal->pictMem.camera_memory[0])
        ALOGE("%s: get_mem failed", __func__);

#if FREAD_JPEG_PICTURE
    jpegLength = readFromFile("/data/tempVGA.jpeg",
                    (char*)camHal->pictMem.camera_memory[0]->data,
                    camHal->pictMem.camera_memory[0]->size);
    camHal->pictMem.camera_memory[0]->size = jpegLength;

#elif JPEG_ON_USB_CAMERA
    memcpy((char*)camHal->pictMem.camera_memory[0]->data,
            (char *)camHal->buffers[camHal->curCaptureBuf.index].data,
            camHal->curCaptureBuf.bytesused);
    camHal->pictMem.camera_memory[0]->size = camHal->curCaptureBuf.bytesused;
    jpegLength = camHal->curCaptureBuf.bytesused;

#else
    rc = encodeJpeg(camHal);
    ERROR_CHECK_EXIT_THREAD(rc, "jpeg_encode");
#endif
    if(jpegLength <= 0)
        ALOGI("%s: jpegLength : %d", __func__, jpegLength);

     ALOGD("%s: jpegLength : %d", __func__, jpegLength);
    /************************************************************************/
    /* - If jpeg frames callback is requested, callback with jpeg buffers   */
    /************************************************************************/
    /* TBD: CAMERA_MSG_RAW_IMAGE data call back */

    if ((camHal->msgEnabledFlag & CAMERA_MSG_COMPRESSED_IMAGE) &&
            (camHal->data_cb)){
        /* Unlock temporarily, callback might call HAL api in turn */
        camHal->lock.unlock();

        camHal->data_cb(CAMERA_MSG_COMPRESSED_IMAGE,
                        camHal->pictMem.camera_memory[0],
                        0, NULL, camHal->cb_ctxt);
        camHal->lock.lock();
    }

    /* release heap memory after the call back */
    if(camHal->pictMem.camera_memory[0])
        camHal->pictMem.camera_memory[0]->release(
            camHal->pictMem.camera_memory[0]);

    rc = deallocate_ion_memory(mem_info);
    if(rc)
        ALOGE("%s: ION memory de-allocation failed", __func__);

    /************************************************************************/
    /* - Enqueue capture buffer back to USB camera                          */
    /************************************************************************/
    if(0 == put_buf_to_cam(camHal)) {
        ALOGD("%s: put_buf_to_cam success", __func__);
    }
    else
        ALOGE("%s: put_buf_to_cam error", __func__);

    /************************************************************************/
    /* - Free USB camera resources and close camera                         */
    /************************************************************************/
    rc = stopUsbCamCapture(camHal);
    ERROR_CHECK_EXIT_THREAD(rc, "stopUsbCamCapture");

    rc = unInitV4L2mmap(camHal);
    ERROR_CHECK_EXIT_THREAD(rc, "unInitV4L2mmap");

    USB_CAM_CLOSE(camHal);
    /************************************************************************/
    /* - If preview was stopped for taking picture, restart the preview     */
    /************************************************************************/
    if(camHal->prvwStoppedForPicture)
    {
        struct camera_device    device;
        device.priv = (void *)camHal;

        USB_CAM_OPEN(camHal);
        /* Unlock temporarily coz usbcam_start_preview has a lock */
        camHal->lock.unlock();
        rc = usbcam_start_preview(&device);
        if(rc)
            ALOGE("%s: start_preview error after take picture", __func__);
        camHal->lock.lock();
        camHal->prvwStoppedForPicture = 0;
    }

    /* take picture activity is done */
    camHal->takePictInProgress = 0;

    ALOGI("%s: X", __func__);
    return (void *)0;
}

/******************************************************************************
 * Function: allocate_ion_memory
 * Description: This function is allocates ION memory
 *
 * Input parameters:
 *  camHal                  - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int allocate_ion_memory(QCameraHalMemInfo_t *mem_info, int ion_type)
{
    int                         rc = 0;
    struct ion_handle_data      handle_data;
    struct ion_allocation_data  alloc;
    struct ion_fd_data          ion_info_fd;
    int                         main_ion_fd = 0;

    main_ion_fd = open("/dev/ion", O_RDONLY);
    if (main_ion_fd <= 0) {
        ALOGE("Ion dev open failed %s\n", strerror(errno));
        goto ION_OPEN_FAILED;
    }

    memset(&alloc, 0, sizeof(alloc));
    alloc.len = mem_info->size;
    /* to make it page size aligned */
    alloc.len = (alloc.len + 4095) & (~4095);
    alloc.align = 4096;
    alloc.flags = ION_FLAG_CACHED;
    alloc.heap_mask = ion_type;
    rc = ioctl(main_ion_fd, ION_IOC_ALLOC, &alloc);
    if (rc < 0) {
        ALOGE("ION allocation failed\n");
        goto ION_ALLOC_FAILED;
    }

    memset(&ion_info_fd, 0, sizeof(ion_info_fd));
    ion_info_fd.handle = alloc.handle;
    rc = ioctl(main_ion_fd, ION_IOC_SHARE, &ion_info_fd);
    if (rc < 0) {
        ALOGE("ION map failed %s\n", strerror(errno));
        goto ION_MAP_FAILED;
    }

    mem_info->main_ion_fd = main_ion_fd;
    mem_info->fd = ion_info_fd.fd;
    mem_info->handle = ion_info_fd.handle;
    mem_info->size = alloc.len;
    return 0;

ION_MAP_FAILED:
    memset(&handle_data, 0, sizeof(handle_data));
    handle_data.handle = ion_info_fd.handle;
    ioctl(main_ion_fd, ION_IOC_FREE, &handle_data);
ION_ALLOC_FAILED:
    close(main_ion_fd);
ION_OPEN_FAILED:
    return -1;
}

/******************************************************************************
 * Function: deallocate_ion_memory
 * Description: This function de allocates ION memory
 *
 * Input parameters:
 *  camHal                  - camera HAL handle
 *
 * Return values:
 *   0      No error
 *   -1     Error
 *
 * Notes: none
 *****************************************************************************/
static int deallocate_ion_memory(QCameraHalMemInfo_t *mem_info)
{
  struct ion_handle_data handle_data;
  int rc = 0;

  if (mem_info->fd > 0) {
      close(mem_info->fd);
      mem_info->fd = 0;
  }

  if (mem_info->main_ion_fd > 0) {
      memset(&handle_data, 0, sizeof(handle_data));
      handle_data.handle = mem_info->handle;
      ioctl(mem_info->main_ion_fd, ION_IOC_FREE, &handle_data);
      close(mem_info->main_ion_fd);
      mem_info->main_ion_fd = 0;
  }
  return rc;
}

/******************************************************************************
 * Function: readFromFile
 * Description: This function reads data from the given file into given buffer
 *
 * Input parameters:
 *  camHal                  - camera HAL handle
 *
 * Return values:
 *   int    bytesRead
 *
 * Notes: none
 *****************************************************************************/
static int readFromFile(char* fileName, char* buffer, int bufferSize)
{
    int bytesRead = 0, fileSize = 0;
    FILE *fp;

    fp = fopen(fileName, "rb");
    if(!fp){
        ALOGE("%s: Error in opening %s ", __func__, fileName);
        return bytesRead;
    }

    /* If file is bigger for given buffer, exit */
    if (fileSize > bufferSize){
        ALOGE("%s: Error %d > %d", __func__, fileSize, bufferSize);
        return bytesRead;
    }

    bytesRead = fread(buffer, 1, bufferSize, fp);
    ALOGD(" %s: bytesRead: %d", __func__, bytesRead);

    return bytesRead;
}

/******************************************************************************
 * Function: encodeJpeg
 * Description: This function initializes Jpeg encoder and calls jpeg encoder
 *              call and waits for the encode to complete
 *
 * Input parameters:
 *  camHal                  - camera HAL handle
 *
 * Return values:
 *   0  No Error
 *  -1  Error
 *
 * Notes: none
 *****************************************************************************/
int encodeJpeg(camera_hardware_t *camHal)
{
    int                 rc = 0;
    mm_jpeg_ops_t       mmJpegOps;
    int                 jpegEncHdl  = 0;
    mm_jpeg_job         mmJpegJob;
    src_image_buffer_info   *srcBuf = NULL;
    QCameraHalMemInfo_t jpegInMemInfo;
    camera_memory_t*    jpegInMem;
    uint32_t            jobId;

    ALOGI("%s: E", __func__);

    /************************************************************************/
    /* - Allocate Jpeg input buffer from ION memory                         */
    /************************************************************************/
    jpegInMemInfo.size = camHal->pictWidth * camHal->pictHeight * 2;
    rc = allocate_ion_memory(&jpegInMemInfo,
                        ((0x1 << CAMERA_ZSL_ION_HEAP_ID) |
                        (0x1 << CAMERA_ZSL_ION_FALLBACK_HEAP_ID)));
    ERROR_CHECK_EXIT(rc, "allocate_ion_memory");

    jpegInMem = camHal->get_memory(
                        jpegInMemInfo.fd, jpegInMemInfo.size, 1, camHal->cb_ctxt);
    if(!jpegInMem){
        ALOGE("%s: get_mem failed", __func__);
        return -1;
    }

    rc = convert_YUYV_to_420_NV12(
        (char *)camHal->buffers[camHal->curCaptureBuf.index].data,
        (char *)jpegInMem->data, camHal->pictWidth, camHal->pictHeight);
    ERROR_CHECK_EXIT(rc, "convert_YUYV_to_420_NV12");
    /************************************************************************/
    /* - Populate JPEG encoding parameters from the camHal context          */
    /************************************************************************/
    memset(&mmJpegJob, 0, sizeof(mmJpegJob));

    mmJpegJob.job_type              = JPEG_JOB_TYPE_ENCODE;
    mmJpegJob.encode_job.jpeg_cb    = jpegEncodeCb;
    mmJpegJob.encode_job.userdata   = (void *)camHal;
    /* TBD: Rotation to be set from settings sent from app */
    mmJpegJob.encode_job.encode_parm.rotation           = 0;
    mmJpegJob.encode_job.encode_parm.exif_numEntries    = 0;
    mmJpegJob.encode_job.encode_parm.exif_data          = NULL;

    /* TBD: Add thumbnail support */
    mmJpegJob.encode_job.encode_parm.buf_info.src_imgs.src_img_num = 1;
    mmJpegJob.encode_job.encode_parm.buf_info.src_imgs.is_video_frame = 0;

    /* Fill main image information */
    srcBuf = &mmJpegJob.encode_job.encode_parm.buf_info.src_imgs.src_img[0];
    srcBuf->type                = JPEG_SRC_IMAGE_TYPE_MAIN;
    srcBuf->img_fmt             = JPEG_SRC_IMAGE_FMT_YUV;
    /* TBD: convert from YUYV to CRCBH2V2 */
    srcBuf->color_format        = MM_JPEG_COLOR_FORMAT_YCRCBLP_H2V2;
    srcBuf->num_bufs            = 1;
    srcBuf->src_image[0].fd        = jpegInMemInfo.fd;
    srcBuf->src_image[0].buf_vaddr = (uint8_t*)jpegInMem->data;
    //srcBuf->src_image[0].offset    = 0;
    srcBuf->src_dim.width       = camHal->pictWidth;
    srcBuf->src_dim.height      = camHal->pictHeight;
    srcBuf->out_dim.width       = camHal->pictWidth;
    srcBuf->out_dim.height      = camHal->pictHeight;
    srcBuf->crop.offset_x       = 0;
    srcBuf->crop.offset_y       = 0;
    srcBuf->crop.width          = srcBuf->src_dim.width;
    srcBuf->crop.height         = srcBuf->src_dim.height;
    srcBuf->quality             = camHal->pictJpegQlty;

    /* TBD:Fill thumbnail image information */

    /* Fill out buf information */
    mmJpegJob.encode_job.encode_parm.buf_info.sink_img.buf_vaddr =
                            (uint8_t*)camHal->pictMem.camera_memory[0]->data;
    mmJpegJob.encode_job.encode_parm.buf_info.sink_img.fd = 0;
    /* TBD: hard coded for 1.5 bytes per pixel */
    mmJpegJob.encode_job.encode_parm.buf_info.sink_img.buf_len =
                            camHal->pictWidth * camHal->pictHeight * 1.5;

    /************************************************************************/
    /* - Initialize jpeg encoder and call Jpeg encoder start                */
    /************************************************************************/
    memset(&mmJpegOps, 0, sizeof(mm_jpeg_ops_t));
    jpegEncHdl = jpeg_open(&mmJpegOps);
    if(!jpegEncHdl){
        ALOGE("%s: Failed to open Jpeg Encoder instance", __func__);
    }else
        ALOGD("%s: jpegEncHdl = %d", __func__, jpegEncHdl);

    camHal->jpegEncInProgress = 1;
    rc = mmJpegOps.start_job(jpegEncHdl, &mmJpegJob, &jobId);

    /************************************************************************/
    /* - Wait for JPEG encoder to complete encoding                         */
    /************************************************************************/
    pthread_mutex_init(&camHal->jpegEncMutex, NULL);
    pthread_cond_init(&camHal->jpegEncCond, NULL);

    pthread_mutex_lock(&camHal->jpegEncMutex);
    while(camHal->jpegEncInProgress)
        pthread_cond_wait(&camHal->jpegEncCond, &camHal->jpegEncMutex);
    pthread_mutex_unlock(&camHal->jpegEncMutex);

    /************************************************************************/
    /* - De-allocate Jpeg input buffer from ION memory                      */
    /************************************************************************/
    if(jpegInMem)
        jpegInMem->release(jpegInMem);

    rc = deallocate_ion_memory(&jpegInMemInfo);
    if(rc)
        ALOGE("%s: ION memory de-allocation failed", __func__);

    ALOGI("%s: X rc = %d", __func__, rc);
    return rc;
}

/******************************************************************************
 * Function: jpegEncodeCb
 * Description: This is a call back function registered with JPEG encoder.
 *              Jpeg encoder calls this function on completion of encoding
 *
 * Input parameters:
 *  camHal                  - camera HAL handle
 *
 * Return values:
 *   0  No Error
 *  -1  Error
 *
 * Notes: none
 *****************************************************************************/
void jpegEncodeCb   (jpeg_job_status_t status,
                       uint8_t thumbnailDroppedFlag,
                       uint32_t client_hdl,
                       uint32_t jobId,
                       uint8_t* out_data,
                       uint32_t data_size,
                       void *userData)
{
    int rc = 0;
    camera_hardware_t *camHal = NULL;

    ALOGI("%s: E status = %d", __func__, status);

    camHal = (camera_hardware_t*) userData;

    if(JPEG_JOB_STATUS_DONE == status){
        ALOGD("%s: JPEG encode successful. out_data:%p, size: %d", __func__,
            out_data, data_size);
        camHal->jpegEncInProgress = 0;
    }

    pthread_mutex_lock(&camHal->jpegEncMutex);
    pthread_cond_signal(&camHal->jpegEncCond);
    pthread_mutex_unlock(&camHal->jpegEncMutex);

    ALOGI("%s: X", __func__);
    return;
}

/******************************************************************************/
}; // namespace android
