/* Copyright (c) 2012-2013, The Linux Foundataion. All rights reserved.
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

#define LOG_TAG "QCamera2HWI"

#include <cutils/properties.h>
#include <hardware/camera.h>
#include <stdlib.h>
#include <utils/Errors.h>
#include <gralloc_priv.h>

#include "QCamera2HWI.h"
#include "QCameraMem.h"

#define MAP_TO_DRIVER_COORDINATE(val, base, scale, offset) (val * scale / base + offset)
#define CAMERA_MIN_STREAMING_BUFFERS     3
#define CAMERA_MIN_JPEG_ENCODING_BUFFERS 2
#define CAMERA_MIN_VIDEO_BUFFERS         9

namespace qcamera {

cam_capability_t *gCamCapability[MM_CAMERA_MAX_NUM_SENSORS];
static pthread_mutex_t g_camlock = PTHREAD_MUTEX_INITIALIZER;

camera_device_ops_t QCamera2HardwareInterface::mCameraOps = {
    set_preview_window:         QCamera2HardwareInterface::set_preview_window,
    set_callbacks:              QCamera2HardwareInterface::set_CallBacks,
    enable_msg_type:            QCamera2HardwareInterface::enable_msg_type,
    disable_msg_type:           QCamera2HardwareInterface::disable_msg_type,
    msg_type_enabled:           QCamera2HardwareInterface::msg_type_enabled,

    start_preview:              QCamera2HardwareInterface::start_preview,
    stop_preview:               QCamera2HardwareInterface::stop_preview,
    preview_enabled:            QCamera2HardwareInterface::preview_enabled,
    store_meta_data_in_buffers: QCamera2HardwareInterface::store_meta_data_in_buffers,

    start_recording:            QCamera2HardwareInterface::start_recording,
    stop_recording:             QCamera2HardwareInterface::stop_recording,
    recording_enabled:          QCamera2HardwareInterface::recording_enabled,
    release_recording_frame:    QCamera2HardwareInterface::release_recording_frame,

    auto_focus:                 QCamera2HardwareInterface::auto_focus,
    cancel_auto_focus:          QCamera2HardwareInterface::cancel_auto_focus,

    take_picture:               QCamera2HardwareInterface::take_picture,
    cancel_picture:             QCamera2HardwareInterface::cancel_picture,

    set_parameters:             QCamera2HardwareInterface::set_parameters,
    get_parameters:             QCamera2HardwareInterface::get_parameters,
    put_parameters:             QCamera2HardwareInterface::put_parameters,
    send_command:               QCamera2HardwareInterface::send_command,

    release:                    QCamera2HardwareInterface::release,
    dump:                       QCamera2HardwareInterface::dump,
};

/*===========================================================================
 * FUNCTION   : set_preview_window
 *
 * DESCRIPTION: set preview window.
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *   @window  : window ops table
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::set_preview_window(struct camera_device *device,
        struct preview_stream_ops *window)
{
    int rc = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("%s: NULL camera device", __func__);
        return BAD_VALUE;
    }

    hw->lockAPI();
    rc = hw->processAPI(QCAMERA_SM_EVT_SET_PREVIEW_WINDOW, (void *)window);
    if (rc == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_SET_PREVIEW_WINDOW);
        rc = hw->m_apiResult.status;
    }
    hw->unlockAPI();

    return rc;
}

/*===========================================================================
 * FUNCTION   : set_CallBacks
 *
 * DESCRIPTION: set callbacks for notify and data
 *
 * PARAMETERS :
 *   @device     : ptr to camera device struct
 *   @notify_cb  : notify cb
 *   @data_cb    : data cb
 *   @data_cb_timestamp  : video data cd with timestamp
 *   @get_memory : ops table for request gralloc memory
 *   @user       : user data ptr
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera2HardwareInterface::set_CallBacks(struct camera_device *device,
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void *user)
{
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return;
    }

    qcamera_sm_evt_setcb_payload_t payload;
    payload.notify_cb = notify_cb;
    payload.data_cb = data_cb;
    payload.data_cb_timestamp = data_cb_timestamp;
    payload.get_memory = get_memory;
    payload.user = user;

    hw->lockAPI();
    int32_t rc = hw->processAPI(QCAMERA_SM_EVT_SET_CALLBACKS, (void *)&payload);
    if (rc == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_SET_CALLBACKS);
    }
    hw->unlockAPI();
}

/*===========================================================================
 * FUNCTION   : enable_msg_type
 *
 * DESCRIPTION: enable certain msg type
 *
 * PARAMETERS :
 *   @device     : ptr to camera device struct
 *   @msg_type   : msg type mask
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera2HardwareInterface::enable_msg_type(struct camera_device *device, int32_t msg_type)
{
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return;
    }
    hw->lockAPI();
    int32_t rc = hw->processAPI(QCAMERA_SM_EVT_ENABLE_MSG_TYPE, (void *)msg_type);
    if (rc == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_ENABLE_MSG_TYPE);
    }
    hw->unlockAPI();
}

/*===========================================================================
 * FUNCTION   : disable_msg_type
 *
 * DESCRIPTION: disable certain msg type
 *
 * PARAMETERS :
 *   @device     : ptr to camera device struct
 *   @msg_type   : msg type mask
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera2HardwareInterface::disable_msg_type(struct camera_device *device, int32_t msg_type)
{
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return;
    }
    hw->lockAPI();
    int32_t rc = hw->processAPI(QCAMERA_SM_EVT_DISABLE_MSG_TYPE, (void *)msg_type);
    if (rc == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_DISABLE_MSG_TYPE);
    }
    hw->unlockAPI();
}

/*===========================================================================
 * FUNCTION   : msg_type_enabled
 *
 * DESCRIPTION: if certain msg type is enabled
 *
 * PARAMETERS :
 *   @device     : ptr to camera device struct
 *   @msg_type   : msg type mask
 *
 * RETURN     : 1 -- enabled
 *              0 -- not enabled
 *==========================================================================*/
int QCamera2HardwareInterface::msg_type_enabled(struct camera_device *device, int32_t msg_type)
{
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    hw->lockAPI();
    ret = hw->processAPI(QCAMERA_SM_EVT_MSG_TYPE_ENABLED, (void *)msg_type);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_MSG_TYPE_ENABLED);
        ret = hw->m_apiResult.enabled;
    }
    hw->unlockAPI();

   return ret;
}

/*===========================================================================
 * FUNCTION   : start_preview
 *
 * DESCRIPTION: start preview
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::start_preview(struct camera_device *device)
{
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    ALOGD("[KPI Perf] %s: E", __func__);
    hw->lockAPI();
    qcamera_sm_evt_enum_t evt = QCAMERA_SM_EVT_START_PREVIEW;
    if (hw->isNoDisplayMode()) {
        evt = QCAMERA_SM_EVT_START_NODISPLAY_PREVIEW;
    }
    ret = hw->processAPI(evt, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(evt);
        ret = hw->m_apiResult.status;
    }
    hw->unlockAPI();
    ALOGD("[KPI Perf] %s: X", __func__);
    return ret;
}

/*===========================================================================
 * FUNCTION   : stop_preview
 *
 * DESCRIPTION: stop preview
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera2HardwareInterface::stop_preview(struct camera_device *device)
{
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return;
    }
    ALOGD("[KPI Perf] %s: E", __func__);
    hw->lockAPI();
    int32_t ret = hw->processAPI(QCAMERA_SM_EVT_STOP_PREVIEW, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_STOP_PREVIEW);
    }
    hw->unlockAPI();
    ALOGD("[KPI Perf] %s: X", __func__);
}

/*===========================================================================
 * FUNCTION   : preview_enabled
 *
 * DESCRIPTION: if preview is running
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *
 * RETURN     : 1 -- running
 *              0 -- not running
 *==========================================================================*/
int QCamera2HardwareInterface::preview_enabled(struct camera_device *device)
{
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }

    hw->lockAPI();
    ret = hw->processAPI(QCAMERA_SM_EVT_PREVIEW_ENABLED, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_PREVIEW_ENABLED);
        ret = hw->m_apiResult.enabled;
    }
    hw->unlockAPI();

    return ret;
}

/*===========================================================================
 * FUNCTION   : store_meta_data_in_buffers
 *
 * DESCRIPTION: if need to store meta data in buffers for video frame
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *   @enable  : flag if enable
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::store_meta_data_in_buffers(
                struct camera_device *device, int enable)
{
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }

    hw->lockAPI();
    ret = hw->processAPI(QCAMERA_SM_EVT_STORE_METADATA_IN_BUFS, (void *)enable);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_STORE_METADATA_IN_BUFS);
        ret = hw->m_apiResult.status;
    }
    hw->unlockAPI();

    return ret;
}

/*===========================================================================
 * FUNCTION   : start_recording
 *
 * DESCRIPTION: start recording
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::start_recording(struct camera_device *device)
{
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    ALOGD("[KPI Perf] %s: E", __func__);
    hw->lockAPI();
    ret = hw->processAPI(QCAMERA_SM_EVT_START_RECORDING, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_START_RECORDING);
        ret = hw->m_apiResult.status;
    }
    hw->unlockAPI();
    ALOGD("[KPI Perf] %s: X", __func__);
    return ret;
}

/*===========================================================================
 * FUNCTION   : stop_recording
 *
 * DESCRIPTION: stop recording
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera2HardwareInterface::stop_recording(struct camera_device *device)
{
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return;
    }
    ALOGD("[KPI Perf] %s: E", __func__);
    hw->lockAPI();
    int32_t ret = hw->processAPI(QCAMERA_SM_EVT_STOP_RECORDING, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_STOP_RECORDING);
    }
    hw->unlockAPI();
    ALOGD("[KPI Perf] %s: X", __func__);
}

/*===========================================================================
 * FUNCTION   : recording_enabled
 *
 * DESCRIPTION: if recording is running
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *
 * RETURN     : 1 -- running
 *              0 -- not running
 *==========================================================================*/
int QCamera2HardwareInterface::recording_enabled(struct camera_device *device)
{
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    hw->lockAPI();
    ret = hw->processAPI(QCAMERA_SM_EVT_RECORDING_ENABLED, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_RECORDING_ENABLED);
        ret = hw->m_apiResult.enabled;
    }
    hw->unlockAPI();

    return ret;
}

/*===========================================================================
 * FUNCTION   : release_recording_frame
 *
 * DESCRIPTION: return recording frame back
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *   @opaque  : ptr to frame to be returned
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera2HardwareInterface::release_recording_frame(
            struct camera_device *device, const void *opaque)
{
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return;
    }
    ALOGD("%s: E", __func__);
    hw->lockAPI();
    int32_t ret = hw->processAPI(QCAMERA_SM_EVT_RELEASE_RECORIDNG_FRAME, (void *)opaque);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_RELEASE_RECORIDNG_FRAME);
    }
    hw->unlockAPI();
    ALOGD("%s: X", __func__);
}

/*===========================================================================
 * FUNCTION   : auto_focus
 *
 * DESCRIPTION: start auto focus
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::auto_focus(struct camera_device *device)
{
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    ALOGD("[KPI Perf] %s : E", __func__);
    hw->lockAPI();
    ret = hw->processAPI(QCAMERA_SM_EVT_START_AUTO_FOCUS, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_START_AUTO_FOCUS);
        ret = hw->m_apiResult.status;
    }
    hw->unlockAPI();
    ALOGD("[KPI Perf] %s : X", __func__);

    return ret;
}

/*===========================================================================
 * FUNCTION   : cancel_auto_focus
 *
 * DESCRIPTION: cancel auto focus
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::cancel_auto_focus(struct camera_device *device)
{
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    hw->lockAPI();
    ret = hw->processAPI(QCAMERA_SM_EVT_STOP_AUTO_FOCUS, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_STOP_AUTO_FOCUS);
        ret = hw->m_apiResult.status;
    }
    hw->unlockAPI();

    return ret;
}

/*===========================================================================
 * FUNCTION   : take_picture
 *
 * DESCRIPTION: take picture
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::take_picture(struct camera_device *device)
{
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    ALOGD("[KPI Perf] %s: E", __func__);
    hw->lockAPI();

    /* Prepare snapshot in case LED needs to be flashed */
    ret = hw->processAPI(QCAMERA_SM_EVT_PREPARE_SNAPSHOT, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_PREPARE_SNAPSHOT);
        ret = hw->m_apiResult.status;
    }

    /* Regardless what the result value for prepare_snapshot,
     * go ahead with capture anyway. Just like the way autofocus
     * is handled in capture case. */

    /* capture */
    ret = hw->processAPI(QCAMERA_SM_EVT_TAKE_PICTURE, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_TAKE_PICTURE);
        ret = hw->m_apiResult.status;
    }

    hw->unlockAPI();
    ALOGD("[KPI Perf] %s: X", __func__);
    return ret;
}

/*===========================================================================
 * FUNCTION   : cancel_picture
 *
 * DESCRIPTION: cancel current take picture request
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::cancel_picture(struct camera_device *device)
{
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    hw->lockAPI();
    ret = hw->processAPI(QCAMERA_SM_EVT_CANCEL_PICTURE, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_CANCEL_PICTURE);
        ret = hw->m_apiResult.status;
    }
    hw->unlockAPI();

    return ret;
}

/*===========================================================================
 * FUNCTION   : set_parameters
 *
 * DESCRIPTION: set camera parameters
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *   @parms   : string of packed parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::set_parameters(struct camera_device *device,
                                              const char *parms)
{
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    hw->lockAPI();
    ret = hw->processAPI(QCAMERA_SM_EVT_SET_PARAMS, (void *)parms);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_SET_PARAMS);
        ret = hw->m_apiResult.status;
    }
    hw->unlockAPI();

    return ret;
}

/*===========================================================================
 * FUNCTION   : get_parameters
 *
 * DESCRIPTION: query camera parameters
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *
 * RETURN     : packed parameters in a string
 *==========================================================================*/
char* QCamera2HardwareInterface::get_parameters(struct camera_device *device)
{
    char *ret = NULL;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return NULL;
    }
    hw->lockAPI();
    int32_t rc = hw->processAPI(QCAMERA_SM_EVT_GET_PARAMS, NULL);
    if (rc == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_GET_PARAMS);
        ret = hw->m_apiResult.params;
    }
    hw->unlockAPI();

    return ret;
}

/*===========================================================================
 * FUNCTION   : put_parameters
 *
 * DESCRIPTION: return camera parameters string back to HAL
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *   @parm    : ptr to parameter string to be returned
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera2HardwareInterface::put_parameters(struct camera_device *device,
                                               char *parm)
{
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return;
    }
    hw->lockAPI();
    int32_t ret = hw->processAPI(QCAMERA_SM_EVT_PUT_PARAMS, (void *)parm);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_PUT_PARAMS);
    }
    hw->unlockAPI();
}

/*===========================================================================
 * FUNCTION   : send_command
 *
 * DESCRIPTION: command to be executed
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *   @cmd     : cmd to be executed
 *   @arg1    : ptr to optional argument1
 *   @arg2    : ptr to optional argument2
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::send_command(struct camera_device *device,
                                            int32_t cmd,
                                            int32_t arg1,
                                            int32_t arg2)
{
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }

    qcamera_sm_evt_command_payload_t payload;
    memset(&payload, 0, sizeof(qcamera_sm_evt_command_payload_t));
    payload.cmd = cmd;
    payload.arg1 = arg1;
    payload.arg2 = arg2;
    hw->lockAPI();
    ret = hw->processAPI(QCAMERA_SM_EVT_SEND_COMMAND, (void *)&payload);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_SEND_COMMAND);
        ret = hw->m_apiResult.status;
    }
    hw->unlockAPI();

    return ret;
}

/*===========================================================================
 * FUNCTION   : release
 *
 * DESCRIPTION: release camera resource
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera2HardwareInterface::release(struct camera_device *device)
{
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return;
    }
    hw->lockAPI();
    int32_t ret = hw->processAPI(QCAMERA_SM_EVT_RELEASE, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_RELEASE);
    }
    hw->unlockAPI();
}

/*===========================================================================
 * FUNCTION   : dump
 *
 * DESCRIPTION: dump camera status
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *   @fd      : fd for status to be dumped to
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::dump(struct camera_device *device, int fd)
{
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    hw->lockAPI();
    ret = hw->processAPI(QCAMERA_SM_EVT_DUMP, (void *)fd);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_DUMP);
        ret = hw->m_apiResult.status;
    }
    hw->unlockAPI();

    return ret;
}

/*===========================================================================
 * FUNCTION   : close_camera_device
 *
 * DESCRIPTION: close camera device
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::close_camera_device(hw_device_t *hw_dev)
{
    int ret = NO_ERROR;
    ALOGD("[KPI Perf] %s: E",__func__);
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(
            reinterpret_cast<camera_device_t *>(hw_dev)->priv);
    if (!hw) {
        ALOGE("%s: NULL camera device", __func__);
        return BAD_VALUE;
    }
    delete hw;
    ALOGD("[KPI Perf] %s: X",__func__);
    return ret;
}

/*===========================================================================
 * FUNCTION   : register_face_image
 *
 * DESCRIPTION: register a face image into imaging lib for face authenticatio/
 *              face recognition
 *
 * PARAMETERS :
 *   @device  : ptr to camera device struct
 *   @img_ptr : ptr to image buffer
 *   @config  : ptr to config about input image, i.e., format, dimension, and etc.
 *
 * RETURN     : >=0 unique ID of face registerd.
 *              <0  failure.
 *==========================================================================*/
int QCamera2HardwareInterface::register_face_image(struct camera_device *device,
                                                   void *img_ptr,
                                                   cam_pp_offline_src_config_t *config)
{
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    qcamera_sm_evt_reg_face_payload_t payload;
    memset(&payload, 0, sizeof(qcamera_sm_evt_reg_face_payload_t));
    payload.img_ptr = img_ptr;
    payload.config = config;
    hw->lockAPI();
    ret = hw->processAPI(QCAMERA_SM_EVT_REG_FACE_IMAGE, (void *)&payload);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_REG_FACE_IMAGE);
        ret = hw->m_apiResult.handle;
    }
    hw->unlockAPI();

    return ret;
}

/*===========================================================================
 * FUNCTION   : QCamera2HardwareInterface
 *
 * DESCRIPTION: constructor of QCamera2HardwareInterface
 *
 * PARAMETERS :
 *   @cameraId  : camera ID
 *
 * RETURN     : none
 *==========================================================================*/
QCamera2HardwareInterface::QCamera2HardwareInterface(int cameraId)
    : mCameraId(cameraId),
      mCameraHandle(NULL),
      mCameraOpened(false),
      mPreviewWindow(NULL),
      mMsgEnabled(0),
      mStoreMetaDataInFrame(0),
      m_stateMachine(this),
      m_postprocessor(this),
      m_thermalAdapter(QCameraThermalAdapter::getInstance()),
      m_cbNotifier(this),
      m_bShutterSoundPlayed(false),
      m_currentFocusState(CAM_AF_NOT_FOCUSED),
      m_bStartZSLSnapshotCalled(false),
      m_pPowerModule(NULL),
      mDumpFrmCnt(0),
      mDumpSkipCnt(0)
{
    mCameraDevice.common.tag = HARDWARE_DEVICE_TAG;
    mCameraDevice.common.version = HARDWARE_DEVICE_API_VERSION(1, 0);
    mCameraDevice.common.close = close_camera_device;
    mCameraDevice.ops = &mCameraOps;
    mCameraDevice.priv = this;


    pthread_mutex_init(&m_lock, NULL);
    pthread_cond_init(&m_cond, NULL);
    memset(&m_apiResult, 0, sizeof(qcamera_api_result_t));

    pthread_mutex_init(&m_evtLock, NULL);
    pthread_cond_init(&m_evtCond, NULL);
    memset(&m_evtResult, 0, sizeof(qcamera_api_result_t));

    memset(m_channels, 0, sizeof(m_channels));

#ifdef HAS_MULTIMEDIA_HINTS
    if (hw_get_module(POWER_HARDWARE_MODULE_ID, (const hw_module_t **)&m_pPowerModule)) {
        ALOGE("%s: %s module not found", __func__, POWER_HARDWARE_MODULE_ID);
    }
#endif

}

/*===========================================================================
 * FUNCTION   : ~QCamera2HardwareInterface
 *
 * DESCRIPTION: destructor of QCamera2HardwareInterface
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
QCamera2HardwareInterface::~QCamera2HardwareInterface()
{
    closeCamera();
    pthread_mutex_destroy(&m_lock);
    pthread_cond_destroy(&m_cond);
    pthread_mutex_destroy(&m_evtLock);
    pthread_cond_destroy(&m_evtCond);
}

/*===========================================================================
 * FUNCTION   : openCamera
 *
 * DESCRIPTION: open camera
 *
 * PARAMETERS :
 *   @hw_device  : double ptr for camera device struct
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::openCamera(struct hw_device_t **hw_device)
{
    int rc = NO_ERROR;
    if (mCameraOpened) {
        *hw_device = NULL;
        return PERMISSION_DENIED;
    }

    rc = openCamera();
    if (rc == NO_ERROR)
        *hw_device = &mCameraDevice.common;
    else
        *hw_device = NULL;
    return rc;
}

/*===========================================================================
 * FUNCTION   : openCamera
 *
 * DESCRIPTION: open camera
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::openCamera()
{
    if (mCameraHandle) {
        ALOGE("Failure: Camera already opened");
        return ALREADY_EXISTS;
    }

    mCameraHandle = camera_open(mCameraId);
    if (!mCameraHandle) {
        ALOGE("camera_open failed.");
        return UNKNOWN_ERROR;
    }

    mCameraHandle->ops->register_event_notify(mCameraHandle->camera_handle,
                                              camEvtHandle,
                                              (void *) this);

    int32_t rc = m_postprocessor.init(jpegEvtHandle, this);
    if (rc != 0) {
        ALOGE("Init Postprocessor failed");
        mCameraHandle->ops->close_camera(mCameraHandle->camera_handle);
        mCameraHandle = NULL;
        return UNKNOWN_ERROR;
    }

    // update padding info from jpeg
    cam_padding_info_t padding_info;
    m_postprocessor.getJpegPaddingReq(padding_info);
    if (gCamCapability[mCameraId]->padding_info.width_padding < padding_info.width_padding) {
        gCamCapability[mCameraId]->padding_info.width_padding = padding_info.width_padding;
    }
    if (gCamCapability[mCameraId]->padding_info.height_padding < padding_info.height_padding) {
        gCamCapability[mCameraId]->padding_info.height_padding = padding_info.height_padding;
    }
    if (gCamCapability[mCameraId]->padding_info.plane_padding < padding_info.plane_padding) {
        gCamCapability[mCameraId]->padding_info.plane_padding = padding_info.plane_padding;
    }

    mParameters.init(gCamCapability[mCameraId], mCameraHandle);

    rc = m_thermalAdapter.init(this);
    if (rc != 0) {
        ALOGE("Init thermal adapter failed");
    }

    mCameraOpened = true;

    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : closeCamera
 *
 * DESCRIPTION: close camera
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::closeCamera()
{
    int rc = NO_ERROR;
    int i;

    if (!mCameraOpened) {
        return NO_ERROR;
    }

    // deinit Parameters
    mParameters.deinit();

    // exit notifier
    m_cbNotifier.exit();

    // stop and deinit postprocessor
    m_postprocessor.stop();
    m_postprocessor.deinit();

    m_thermalAdapter.deinit();

    // delete all channels if not already deleted
    for (i = 0; i < QCAMERA_CH_TYPE_MAX; i++) {
        if (m_channels[i] != NULL) {
            m_channels[i]->stop();
            delete m_channels[i];
            m_channels[i] = NULL;
        }
    }

    rc = mCameraHandle->ops->close_camera(mCameraHandle->camera_handle);
    mCameraHandle = NULL;
    mCameraOpened = false;

    return rc;
}

#define DATA_PTR(MEM_OBJ,INDEX) MEM_OBJ->getPtr( INDEX )

/*===========================================================================
 * FUNCTION   : initCapabilities
 *
 * DESCRIPTION: initialize camera capabilities in static data struct
 *
 * PARAMETERS :
 *   @cameraId  : camera Id
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::initCapabilities(int cameraId)
{
    int rc = NO_ERROR;
    mm_camera_vtbl_t *cameraHandle = NULL;
    QCameraHeapMemory *capabilityHeap = NULL;

    cameraHandle = camera_open(cameraId);
    if (!cameraHandle) {
        ALOGE("%s: camera_open failed", __func__);
        rc = UNKNOWN_ERROR;
        goto open_failed;
    }

    /* Allocate memory for capability buffer */
    capabilityHeap = new QCameraHeapMemory(QCAMERA_ION_USE_CACHE);
    rc = capabilityHeap->allocate(1, sizeof(cam_capability_t));
    if(rc != OK) {
        ALOGE("%s: No memory for cappability", __func__);
        goto allocate_failed;
    }

    /* Map memory for capability buffer */
    memset(DATA_PTR(capabilityHeap,0), 0, sizeof(cam_capability_t));
    rc = cameraHandle->ops->map_buf(cameraHandle->camera_handle,
                                CAM_MAPPING_BUF_TYPE_CAPABILITY,
                                capabilityHeap->getFd(0),
                                sizeof(cam_capability_t));
    if(rc < 0) {
        ALOGE("%s: failed to map capability buffer", __func__);
        goto map_failed;
    }

    /* Query Capability */
    rc = cameraHandle->ops->query_capability(cameraHandle->camera_handle);
    if(rc < 0) {
        ALOGE("%s: failed to query capability",__func__);
        goto query_failed;
    }
    gCamCapability[cameraId] = (cam_capability_t *)malloc(sizeof(cam_capability_t));
    if (!gCamCapability[cameraId]) {
        ALOGE("%s: out of memory", __func__);
        goto query_failed;
    }
    memcpy(gCamCapability[cameraId], DATA_PTR(capabilityHeap,0),
                                        sizeof(cam_capability_t));

    rc = NO_ERROR;

query_failed:
    cameraHandle->ops->unmap_buf(cameraHandle->camera_handle,
                            CAM_MAPPING_BUF_TYPE_CAPABILITY);
map_failed:
    capabilityHeap->deallocate();
    delete capabilityHeap;
allocate_failed:
    cameraHandle->ops->close_camera(cameraHandle->camera_handle);
    cameraHandle = NULL;
open_failed:
    return rc;
}

/*===========================================================================
 * FUNCTION   : getCapabilities
 *
 * DESCRIPTION: query camera capabilities
 *
 * PARAMETERS :
 *   @cameraId  : camera Id
 *   @info      : camera info struct to be filled in with camera capabilities
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::getCapabilities(int cameraId,
                                    struct camera_info *info)
{
    int rc = NO_ERROR;

    pthread_mutex_lock(&g_camlock);
    if (NULL == gCamCapability[cameraId]) {
        rc = initCapabilities(cameraId);
        if (rc < 0) {
            pthread_mutex_unlock(&g_camlock);
            return rc;
        }
    }

    switch(gCamCapability[cameraId]->position) {
    case CAM_POSITION_BACK:
        info->facing = CAMERA_FACING_BACK;
        break;

    case CAM_POSITION_FRONT:
        info->facing = CAMERA_FACING_FRONT;
        break;

    default:
        ALOGE("%s:Unknown position type for camera id:%d", __func__, cameraId);
        rc = BAD_VALUE;
        break;
    }

    info->orientation = gCamCapability[cameraId]->sensor_mount_angle;
    pthread_mutex_unlock(&g_camlock);
    return rc;
}

/*===========================================================================
 * FUNCTION   : getBufNumRequired
 *
 * DESCRIPTION: return number of stream buffers needed for given stream type
 *
 * PARAMETERS :
 *   @stream_type  : type of stream
 *
 * RETURN     : number of buffers needed
 *==========================================================================*/
uint8_t QCamera2HardwareInterface::getBufNumRequired(cam_stream_type_t stream_type)
{
    int bufferCnt = 0;
    int minCaptureBuffers = mParameters.getNumOfSnapshots();

    int zslQBuffers = mParameters.getZSLQueueDepth();

    int minCircularBufNum = CAMERA_MIN_STREAMING_BUFFERS +
                            CAMERA_MIN_JPEG_ENCODING_BUFFERS +
                            mParameters.getMaxUnmatchedFramesInQueue() +
                            mParameters.getNumOfHDRBufsIfNeeded();

    // Get buffer count for the particular stream type
    switch (stream_type) {
    case CAM_STREAM_TYPE_PREVIEW:
        {
            if (mParameters.isZSLMode()) {
                bufferCnt = zslQBuffers + minCircularBufNum;
            } else {
                bufferCnt = CAMERA_MIN_STREAMING_BUFFERS +
                            mParameters.getMaxUnmatchedFramesInQueue();
            }
        }
        break;
    case CAM_STREAM_TYPE_POSTVIEW:
        {
            bufferCnt = minCaptureBuffers +
                        mParameters.getMaxUnmatchedFramesInQueue() +
                        mParameters.getNumOfExtraHDRBufsIfNeeded() +
                        CAMERA_MIN_STREAMING_BUFFERS;
        }
        break;
    case CAM_STREAM_TYPE_SNAPSHOT:
        {
            if (mParameters.isZSLMode()) {
                bufferCnt = zslQBuffers + minCircularBufNum;
            } else {
                bufferCnt = minCaptureBuffers +
                            mParameters.getMaxUnmatchedFramesInQueue() +
                            mParameters.getNumOfExtraHDRBufsIfNeeded() +
                            CAMERA_MIN_STREAMING_BUFFERS;
            }
        }
        break;
    case CAM_STREAM_TYPE_RAW:
        if (mParameters.isZSLMode()) {
            bufferCnt = zslQBuffers + CAMERA_MIN_STREAMING_BUFFERS;
        } else {
            bufferCnt = minCaptureBuffers +
                        mParameters.getMaxUnmatchedFramesInQueue() +
                        mParameters.getNumOfExtraHDRBufsIfNeeded() +
                        CAMERA_MIN_STREAMING_BUFFERS;
        }
        break;
    case CAM_STREAM_TYPE_VIDEO:
        {
            bufferCnt = CAMERA_MIN_VIDEO_BUFFERS +
                        mParameters.getMaxUnmatchedFramesInQueue() +
                        CAMERA_MIN_STREAMING_BUFFERS;
        }
        break;
    case CAM_STREAM_TYPE_METADATA:
        {
            bufferCnt = minCaptureBuffers +
                        mParameters.getMaxUnmatchedFramesInQueue() +
                        mParameters.getNumOfExtraHDRBufsIfNeeded() +
                        CAMERA_MIN_STREAMING_BUFFERS;
            if (bufferCnt < zslQBuffers + minCircularBufNum) {
                bufferCnt = zslQBuffers + minCircularBufNum;
            }
        }
        break;
    case CAM_STREAM_TYPE_OFFLINE_PROC:
        {
            bufferCnt = minCaptureBuffers +
                        mParameters.getMaxUnmatchedFramesInQueue();
            if (bufferCnt < CAMERA_MIN_STREAMING_BUFFERS) {
                bufferCnt = CAMERA_MIN_STREAMING_BUFFERS;
            }
        }
        break;
    case CAM_STREAM_TYPE_DEFAULT:
    case CAM_STREAM_TYPE_MAX:
    default:
        bufferCnt = 0;
        break;
    }

    return bufferCnt;
}

/*===========================================================================
 * FUNCTION   : allocateStreamBuf
 *
 * DESCRIPTION: alocate stream buffers
 *
 * PARAMETERS :
 *   @stream_type  : type of stream
 *   @size         : size of buffer
 *   @bufferCnt    : [IN/OUT] minimum num of buffers to be allocated.
 *                   could be modified during allocation if more buffers needed
 *
 * RETURN     : ptr to a memory obj that holds stream buffers.
 *              NULL if failed
 *==========================================================================*/
QCameraMemory *QCamera2HardwareInterface::allocateStreamBuf(cam_stream_type_t stream_type,
                                                            int size,
                                                            uint8_t &bufferCnt)
{
    int rc = NO_ERROR;
    QCameraMemory *mem = NULL;
    bool bCachedMem = QCAMERA_ION_USE_CACHE;

    // Allocate stream buffer memory object
    switch (stream_type) {
    case CAM_STREAM_TYPE_PREVIEW:
        {
            if (isNoDisplayMode()) {
                mem = new QCameraStreamMemory(mGetMemory, bCachedMem);
            } else {
                cam_dimension_t dim;
                QCameraGrallocMemory *grallocMemory =
                    new QCameraGrallocMemory(mGetMemory);

                mParameters.getStreamDimension(stream_type, dim);
                if (grallocMemory)
                    grallocMemory->setWindowInfo(mPreviewWindow, dim.width, dim.height,
                            mParameters.getPreviewHalPixelFormat());
                mem = grallocMemory;
            }
        }
        break;
    case CAM_STREAM_TYPE_POSTVIEW:
        {
            cam_dimension_t dim;
            QCameraGrallocMemory *grallocMemory =
                new QCameraGrallocMemory(mGetMemory);

            mParameters.getStreamDimension(stream_type, dim);
            if (grallocMemory)
                grallocMemory->setWindowInfo(mPreviewWindow, dim.width, dim.height,
                        mParameters.getPreviewHalPixelFormat());
            mem = grallocMemory;
        }
        break;
    case CAM_STREAM_TYPE_SNAPSHOT:
    case CAM_STREAM_TYPE_RAW:
    case CAM_STREAM_TYPE_METADATA:
    case CAM_STREAM_TYPE_OFFLINE_PROC:
        mem = new QCameraStreamMemory(mGetMemory, bCachedMem);
        break;
    case CAM_STREAM_TYPE_VIDEO:
        {
            char value[32];
            property_get("persist.camera.mem.usecache", value, "1");
            if (atoi(value) == 0) {
                bCachedMem = QCAMERA_ION_USE_NOCACHE;
            }
            ALOGD("%s: vidoe buf using cached memory = %d", __func__, bCachedMem);
            mem = new QCameraVideoMemory(mGetMemory, bCachedMem);
        }
        break;
    case CAM_STREAM_TYPE_DEFAULT:
    case CAM_STREAM_TYPE_MAX:
        break;
    }
    if (!mem) {
        return NULL;
    }

    if (bufferCnt > 0) {
        rc = mem->allocate(bufferCnt, size);
        if (rc < 0) {
            delete mem;
            return NULL;
        }
        bufferCnt = mem->getCnt();
    }
    return mem;
}

/*===========================================================================
 * FUNCTION   : allocateStreamInfoBuf
 *
 * DESCRIPTION: alocate stream info buffer
 *
 * PARAMETERS :
 *   @stream_type  : type of stream
 *
 * RETURN     : ptr to a memory obj that holds stream info buffer.
 *              NULL if failed
 *==========================================================================*/
QCameraHeapMemory *QCamera2HardwareInterface::allocateStreamInfoBuf(
    cam_stream_type_t stream_type)
{
    int rc = NO_ERROR;

    QCameraHeapMemory *streamInfoBuf = new QCameraHeapMemory(QCAMERA_ION_USE_CACHE);
    if (!streamInfoBuf) {
        ALOGE("allocateStreamInfoBuf: Unable to allocate streamInfo object");
        return NULL;
    }

    rc = streamInfoBuf->allocate(1, sizeof(cam_stream_info_t));
    if (rc < 0) {
        ALOGE("allocateStreamInfoBuf: Failed to allocate stream info memory");
        delete streamInfoBuf;
        return NULL;
    }

    cam_stream_info_t *streamInfo = (cam_stream_info_t *)streamInfoBuf->getPtr(0);
    memset(streamInfo, 0, sizeof(cam_stream_info_t));
    streamInfo->stream_type = stream_type;
    rc = mParameters.getStreamFormat(stream_type, streamInfo->fmt);
    rc = mParameters.getStreamDimension(stream_type, streamInfo->dim);

    streamInfo->streaming_mode = CAM_STREAMING_MODE_CONTINUOUS;
    switch (stream_type) {
    case CAM_STREAM_TYPE_SNAPSHOT:
    case CAM_STREAM_TYPE_RAW:
        if (mParameters.isZSLMode() && mParameters.getRecordingHintValue() != true) {
            streamInfo->streaming_mode = CAM_STREAMING_MODE_CONTINUOUS;
        } else {
            streamInfo->streaming_mode = CAM_STREAMING_MODE_BURST;
            streamInfo->num_of_burst = mParameters.getNumOfSnapshots();
        }
        break;
    case CAM_STREAM_TYPE_POSTVIEW:
        streamInfo->streaming_mode = CAM_STREAMING_MODE_BURST;
        streamInfo->num_of_burst = mParameters.getNumOfSnapshots();
        break;
    default:
        break;
    }

    //set flip mode based on Stream type;
    int flipMode = mParameters.getFlipMode(stream_type);
    if (flipMode > 0) {
        streamInfo->pp_config.feature_mask |= CAM_QCOM_FEATURE_FLIP;
        streamInfo->pp_config.flip = flipMode;
    }

    return streamInfoBuf;
}

/*===========================================================================
 * FUNCTION   : setPreviewWindow
 *
 * DESCRIPTION: set preview window impl
 *
 * PARAMETERS :
 *   @window  : ptr to window ops table struct
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::setPreviewWindow(
        struct preview_stream_ops *window)
{
    mPreviewWindow = window;
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setCallBacks
 *
 * DESCRIPTION: set callbacks impl
 *
 * PARAMETERS :
 *   @notify_cb  : notify cb
 *   @data_cb    : data cb
 *   @data_cb_timestamp : data cb with time stamp
 *   @get_memory : request memory ops table
 *   @user       : user data ptr
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::setCallBacks(camera_notify_callback notify_cb,
                                            camera_data_callback data_cb,
                                            camera_data_timestamp_callback data_cb_timestamp,
                                            camera_request_memory get_memory,
                                            void *user)
{
    mNotifyCb        = notify_cb;
    mDataCb          = data_cb;
    mDataCbTimestamp = data_cb_timestamp;
    mGetMemory       = get_memory;
    mCallbackCookie  = user;
    m_cbNotifier.setCallbacks(notify_cb, data_cb, data_cb_timestamp, user);
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : enableMsgType
 *
 * DESCRIPTION: enable msg type impl
 *
 * PARAMETERS :
 *   @msg_type  : msg type mask to be enabled
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::enableMsgType(int32_t msg_type)
{
    mMsgEnabled |= msg_type;
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : disableMsgType
 *
 * DESCRIPTION: disable msg type impl
 *
 * PARAMETERS :
 *   @msg_type  : msg type mask to be disabled
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::disableMsgType(int32_t msg_type)
{
    mMsgEnabled &= ~msg_type;
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : msgTypeEnabled
 *
 * DESCRIPTION: impl to determine if certain msg_type is enabled
 *
 * PARAMETERS :
 *   @msg_type  : msg type mask
 *
 * RETURN     : 0 -- not enabled
 *              none 0 -- enabled
 *==========================================================================*/
int QCamera2HardwareInterface::msgTypeEnabled(int32_t msg_type)
{
    return (mMsgEnabled & msg_type);
}

/*===========================================================================
 * FUNCTION   : msgTypeEnabledWithLock
 *
 * DESCRIPTION: impl to determine if certain msg_type is enabled with lock
 *
 * PARAMETERS :
 *   @msg_type  : msg type mask
 *
 * RETURN     : 0 -- not enabled
 *              none 0 -- enabled
 *==========================================================================*/
int QCamera2HardwareInterface::msgTypeEnabledWithLock(int32_t msg_type)
{
    int enabled = 0;
    lockAPI();
    enabled = mMsgEnabled & msg_type;
    unlockAPI();
    return enabled;
}

/*===========================================================================
 * FUNCTION   : startPreview
 *
 * DESCRIPTION: start preview impl
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::startPreview()
{
    int32_t rc = NO_ERROR;
    ALOGD("%s: E", __func__);
    // start preview stream
    if (mParameters.isZSLMode() && mParameters.getRecordingHintValue() !=true) {
        rc = startChannel(QCAMERA_CH_TYPE_ZSL);
    } else {
        rc = startChannel(QCAMERA_CH_TYPE_PREVIEW);
    }
    ALOGD("%s: X", __func__);
    return rc;
}

/*===========================================================================
 * FUNCTION   : stopPreview
 *
 * DESCRIPTION: stop preview impl
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::stopPreview()
{
    ALOGD("%s: E", __func__);
    // stop preview stream
    if (mParameters.isZSLMode() && mParameters.getRecordingHintValue() !=true) {
        stopChannel(QCAMERA_CH_TYPE_ZSL);
    } else {
        stopChannel(QCAMERA_CH_TYPE_PREVIEW);
    }

    // delete all channels from preparePreview
    unpreparePreview();
    ALOGD("%s: X", __func__);
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : storeMetaDataInBuffers
 *
 * DESCRIPTION: enable store meta data in buffers for video frames impl
 *
 * PARAMETERS :
 *   @enable  : flag if need enable
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::storeMetaDataInBuffers(int enable)
{
    mStoreMetaDataInFrame = enable;
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : startRecording
 *
 * DESCRIPTION: start recording impl
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::startRecording()
{
    int32_t rc = NO_ERROR;
    ALOGD("%s: E", __func__);
    if (mParameters.getRecordingHintValue() == false) {
        ALOGE("%s: start recording when hint is false, stop preview first", __func__);
        stopChannel(QCAMERA_CH_TYPE_PREVIEW);
        delChannel(QCAMERA_CH_TYPE_PREVIEW);

        // Set recording hint to TRUE
        mParameters.updateRecordingHintValue(TRUE);
        rc = preparePreview();
        if (rc == NO_ERROR) {
            rc = startChannel(QCAMERA_CH_TYPE_PREVIEW);
        }
    }

    if (rc == NO_ERROR) {
        rc = startChannel(QCAMERA_CH_TYPE_VIDEO);
    }

#ifdef HAS_MULTIMEDIA_HINTS
    if (rc == NO_ERROR) {
        if (m_pPowerModule) {
            if (m_pPowerModule->powerHint) {
                m_pPowerModule->powerHint(m_pPowerModule, POWER_HINT_VIDEO_ENCODE, (void *)"state=1");
            }
        }
    }
#endif
    ALOGD("%s: X", __func__);
    return rc;
}

/*===========================================================================
 * FUNCTION   : stopRecording
 *
 * DESCRIPTION: stop recording impl
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::stopRecording()
{
    int rc = stopChannel(QCAMERA_CH_TYPE_VIDEO);
    ALOGD("%s: E", __func__);
#ifdef HAS_MULTIMEDIA_HINTS
    if (m_pPowerModule) {
        if (m_pPowerModule->powerHint) {
            m_pPowerModule->powerHint(m_pPowerModule, POWER_HINT_VIDEO_ENCODE, (void *)"state=0");
        }
    }
#endif
    ALOGD("%s: X", __func__);
    return rc;
}

/*===========================================================================
 * FUNCTION   : releaseRecordingFrame
 *
 * DESCRIPTION: return video frame impl
 *
 * PARAMETERS :
 *   @opaque  : ptr to video frame to be returned
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::releaseRecordingFrame(const void * opaque)
{
    int32_t rc = UNKNOWN_ERROR;
    QCameraVideoChannel *pChannel =
        (QCameraVideoChannel *)m_channels[QCAMERA_CH_TYPE_VIDEO];
    ALOGD("%s: opaque data = %p", __func__,opaque);
    if(pChannel != NULL) {
        rc = pChannel->releaseFrame(opaque, mStoreMetaDataInFrame > 0);
    }
    return rc;
}

/*===========================================================================
 * FUNCTION   : autoFocus
 *
 * DESCRIPTION: start auto focus impl
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::autoFocus()
{
    int rc = NO_ERROR;
    cam_focus_mode_type focusMode = mParameters.getFocusMode();

    switch (focusMode) {
    case CAM_FOCUS_MODE_AUTO:
    case CAM_FOCUS_MODE_MACRO:
        {
            rc = mCameraHandle->ops->do_auto_focus(mCameraHandle->camera_handle);
            if (rc == NO_ERROR) {
                mParameters.setAFRunning(true);
            }
        }
        break;
    case CAM_FOCUS_MODE_CONTINOUS_VIDEO:
        // According to Google API definition, the focus callback will immediately
        // return with a boolean that indicates whether the focus is sharp or not.
        // The focus position is locked after autoFocus call.
        // in this sense, the effect is the same as cancel_auto_focus
        {
            rc = mParameters.setLockCAF(true);

            // send evt notify that foucs is done
            sendEvtNotify(CAMERA_MSG_FOCUS,
                          (m_currentFocusState == CAM_AF_FOCUSED)? true : false,
                          0);
        }
        break;
    case CAM_FOCUS_MODE_CONTINOUS_PICTURE:
        // According to Google API definition, if the autofocus is in the middle
        // of scanning, the focus callback will return when it completes. If the
        // autofocus is not scanning, focus callback will immediately return with
        // a boolean that indicates whether the focus is sharp or not. The apps
        // can then decide if they want to take a picture immediately or to change
        // the focus mode to auto, and run a full autofocus cycle. The focus position
        // is locked after autoFocus call.
        if (m_currentFocusState != CAM_AF_SCANNING) {
            // lock focus
            rc = mParameters.setLockCAF(true);

            // send evt notify that foucs is done
            sendEvtNotify(CAMERA_MSG_FOCUS,
                          (m_currentFocusState == CAM_AF_FOCUSED)? true : false,
                          0);
        } else {
            // set flag that lock CAF is needed once focus state becomes focsued/not focused
            mParameters.setLockCAFNeeded(true);
            rc = NO_ERROR;
        }
        break;
    case CAM_FOCUS_MODE_INFINITY:
    case CAM_FOCUS_MODE_FIXED:
    case CAM_FOCUS_MODE_EDOF:
    default:
        ALOGE("%s: No ops in focusMode (%d)", __func__, focusMode);
        rc = BAD_VALUE;
        break;
    }
    return rc;
}

/*===========================================================================
 * FUNCTION   : cancelAutoFocus
 *
 * DESCRIPTION: cancel auto focus impl
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::cancelAutoFocus()
{
    int rc = NO_ERROR;
    cam_focus_mode_type focusMode = mParameters.getFocusMode();

    switch (focusMode) {
    case CAM_FOCUS_MODE_AUTO:
    case CAM_FOCUS_MODE_MACRO:
        if (mParameters.isAFRunning()) {
            rc = mCameraHandle->ops->cancel_auto_focus(mCameraHandle->camera_handle);
            if (rc == NO_ERROR) {
                mParameters.setAFRunning(false);
            }
        }
        break;
    case CAM_FOCUS_MODE_CONTINOUS_VIDEO:
    case CAM_FOCUS_MODE_CONTINOUS_PICTURE:
        if (mParameters.isCAFLocked()) {
            // resume CAF by unlock CAF
            rc = mParameters.setLockCAF(false);;
            mParameters.setLockCAFNeeded(false);
        }
        break;
    case CAM_FOCUS_MODE_INFINITY:
    case CAM_FOCUS_MODE_FIXED:
    case CAM_FOCUS_MODE_EDOF:
    default:
        ALOGI("%s: No ops in focusMode (%d)", __func__, focusMode);
        break;
    }
    return rc;
}

/*===========================================================================
 * FUNCTION   : takePicture
 *
 * DESCRIPTION: take picture impl
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::takePicture()
{
    int rc = NO_ERROR;
    uint8_t numSnapshots = mParameters.getNumOfSnapshots();
    ALOGD("%s: E", __func__);
    if (mParameters.isZSLMode()) {
        QCameraPicChannel *pZSLChannel =
            (QCameraPicChannel *)m_channels[QCAMERA_CH_TYPE_ZSL];
        if (NULL != pZSLChannel) {
            // start postprocessor
            m_postprocessor.start(pZSLChannel);

            rc = pZSLChannel->takePicture(numSnapshots);
            if (rc != NO_ERROR) {
                ALOGE("%s: cannot take ZSL picture", __func__);
                m_postprocessor.stop();
                return rc;
            }
        } else {
            ALOGE("%s: ZSL channel is NULL", __func__);
            return UNKNOWN_ERROR;
        }
    } else {
        // normal capture case
        // need to stop preview channel
        stopChannel(QCAMERA_CH_TYPE_PREVIEW);
        delChannel(QCAMERA_CH_TYPE_PREVIEW);

        // start snapshot
        if (mParameters.isJpegPictureFormat() ||
            mParameters.isNV16PictureFormat() ) {
            rc = addCaptureChannel();
            if (rc == NO_ERROR) {
                // start postprocessor
                m_postprocessor.start(m_channels[QCAMERA_CH_TYPE_CAPTURE]);

                // start catpure channel
                rc = startChannel(QCAMERA_CH_TYPE_CAPTURE);
                if (rc != NO_ERROR) {
                    ALOGE("%s: cannot start capture channel", __func__);
                    m_postprocessor.stop();
                    delChannel(QCAMERA_CH_TYPE_CAPTURE);
                    return rc;
                }
            } else {
                ALOGE("%s: cannot add capture channel", __func__);
                return rc;
            }
        } else {
            rc = addRawChannel();
            if (rc == NO_ERROR) {
                // start postprocessor
                m_postprocessor.start(m_channels[QCAMERA_CH_TYPE_RAW]);
                rc = startChannel(QCAMERA_CH_TYPE_RAW);
                if (rc != NO_ERROR) {
                    ALOGE("%s: cannot start raw channel", __func__);
                    m_postprocessor.stop();
                    delChannel(QCAMERA_CH_TYPE_RAW);
                    return rc;
                }
            } else {
                ALOGE("%s: cannot add raw channel", __func__);
                return rc;
            }
        }
    }
    ALOGD("%s: X", __func__);
    return rc;
}

/*===========================================================================
 * FUNCTION   : cancelPicture
 *
 * DESCRIPTION: cancel picture impl
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::cancelPicture()
{
    //stop post processor
    m_postprocessor.stop();

    if (mParameters.isZSLMode()) {
        QCameraPicChannel *pZSLChannel =
            (QCameraPicChannel *)m_channels[QCAMERA_CH_TYPE_ZSL];
        if (NULL != pZSLChannel) {
            if (m_bStartZSLSnapshotCalled) {
                mCameraHandle->ops->stop_zsl_snapshot(
                        mCameraHandle->camera_handle);
                m_bStartZSLSnapshotCalled = false;
            }
            pZSLChannel->cancelPicture();
        }
    } else {
        // normal capture case
        if (mParameters.isJpegPictureFormat() ||
            mParameters.isNV16PictureFormat() ) {
            stopChannel(QCAMERA_CH_TYPE_CAPTURE);
            delChannel(QCAMERA_CH_TYPE_CAPTURE);
        } else {
            stopChannel(QCAMERA_CH_TYPE_RAW);
            delChannel(QCAMERA_CH_TYPE_RAW);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : takeLiveSnapshot
 *
 * DESCRIPTION: take live snapshot during recording
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::takeLiveSnapshot()
{
    int rc = NO_ERROR;

    // start post processor
    rc = m_postprocessor.start(m_channels[QCAMERA_CH_TYPE_SNAPSHOT]);

    // start snapshot channel
    if (rc == NO_ERROR) {
        rc = startChannel(QCAMERA_CH_TYPE_SNAPSHOT);
    }
    return rc;
}

/*===========================================================================
 * FUNCTION   : cancelLiveSnapshot
 *
 * DESCRIPTION: cancel current live snapshot request
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::cancelLiveSnapshot()
{
    int rc = NO_ERROR;

    //stop post processor
    m_postprocessor.stop();

    // stop snapshot channel
    rc = stopChannel(QCAMERA_CH_TYPE_SNAPSHOT);

    return rc;
}

/*===========================================================================
 * FUNCTION   : getParameters
 *
 * DESCRIPTION: get parameters impl
 *
 * PARAMETERS : none
 *
 * RETURN     : a string containing parameter pairs
 *==========================================================================*/
char* QCamera2HardwareInterface::getParameters()
{
    char* strParams = NULL;
    String8 str;
    str = mParameters.flatten( );
    strParams = (char *)malloc(sizeof(char)*(str.length()+1));
    if(strParams != NULL){
        memset(strParams, 0, sizeof(char)*(str.length()+1));
        strncpy(strParams, str.string(), str.length());
        strParams[str.length()] = 0;
    }
    return strParams;
}

/*===========================================================================
 * FUNCTION   : putParameters
 *
 * DESCRIPTION: put parameters string impl
 *
 * PARAMETERS :
 *   @parms   : parameters string to be released
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::putParameters(char *parms)
{
    free(parms);
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : sendCommand
 *
 * DESCRIPTION: send command impl
 *
 * PARAMETERS :
 *   @command : command to be executed
 *   @arg1    : optional argument 1
 *   @arg2    : optional argument 2
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::sendCommand(int32_t command, int32_t /*arg1*/, int32_t /*arg2*/)
{
    int rc = NO_ERROR;

    switch (command) {
    case CAMERA_CMD_HISTOGRAM_ON:
    case CAMERA_CMD_HISTOGRAM_OFF:
        rc = setHistogram(command == CAMERA_CMD_HISTOGRAM_ON? true : false);
        break;
    case CAMERA_CMD_START_FACE_DETECTION:
    case CAMERA_CMD_STOP_FACE_DETECTION:
        rc = setFaceDetection(command == CAMERA_CMD_START_FACE_DETECTION? true : false);
        break;
    case CAMERA_CMD_HISTOGRAM_SEND_DATA:
    default:
        rc = NO_ERROR;
        break;
    }
    return rc;
}

/*===========================================================================
 * FUNCTION   : registerFaceImage
 *
 * DESCRIPTION: register face image impl
 *
 * PARAMETERS :
 *   @img_ptr : ptr to image buffer
 *   @config  : ptr to config struct about input image info
 *   @faceID  : [OUT] face ID to uniquely identifiy the registered face image
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::registerFaceImage(void *img_ptr,
                                                 cam_pp_offline_src_config_t *config,
                                                 int32_t &faceID)
{
    int rc = NO_ERROR;
    faceID = -1;

    if (img_ptr == NULL || config == NULL) {
        ALOGE("%s: img_ptr or config is NULL", __func__);
        return BAD_VALUE;
    }

    // allocate ion memory for source image
    QCameraHeapMemory *imgBuf = new QCameraHeapMemory(QCAMERA_ION_USE_CACHE);
    if (imgBuf == NULL) {
        ALOGE("%s: Unable to new heap memory obj for image buf", __func__);
        return NO_MEMORY;
    }

    rc = imgBuf->allocate(1, config->input_buf_planes.plane_info.frame_len);
    if (rc < 0) {
        ALOGE("%s: Unable to allocate heap memory for image buf", __func__);
        delete imgBuf;
        return NO_MEMORY;
    }

    void *pBufPtr = imgBuf->getPtr(0);
    if (pBufPtr == NULL) {
        ALOGE("%s: image buf is NULL", __func__);
        imgBuf->deallocate();
        delete imgBuf;
        return NO_MEMORY;
    }
    memcpy(pBufPtr, img_ptr, config->input_buf_planes.plane_info.frame_len);

    cam_pp_feature_config_t pp_feature;
    memset(&pp_feature, 0, sizeof(cam_pp_feature_config_t));
    pp_feature.feature_mask = CAM_QCOM_FEATURE_REGISTER_FACE;
    QCameraReprocessChannel *pChannel =
        addOfflineReprocChannel(*config, pp_feature, NULL, NULL);

    if (pChannel == NULL) {
        ALOGE("%s: fail to add offline reprocess channel", __func__);
        imgBuf->deallocate();
        delete imgBuf;
        return UNKNOWN_ERROR;
    }

    rc = pChannel->start();
    if (rc != NO_ERROR) {
        ALOGE("%s: Cannot start reprocess channel", __func__);
        imgBuf->deallocate();
        delete imgBuf;
        delete pChannel;
        return rc;
    }

    rc = pChannel->doReprocess(imgBuf->getFd(0), imgBuf->getSize(0), faceID);

    // done with register face image, free imgbuf and delete reprocess channel
    imgBuf->deallocate();
    delete imgBuf;
    imgBuf = NULL;
    pChannel->stop();
    delete pChannel;
    pChannel = NULL;

    return rc;
}

/*===========================================================================
 * FUNCTION   : release
 *
 * DESCRIPTION: release camera resource impl
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::release()
{
    // stop and delete all channels
    for (int i = 0; i <QCAMERA_CH_TYPE_MAX ; i++) {
        if (m_channels[i] != NULL) {
            stopChannel((qcamera_ch_type_enum_t)i);
            delChannel((qcamera_ch_type_enum_t)i);
        }
    }

    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : dump
 *
 * DESCRIPTION: camera status dump impl
 *
 * PARAMETERS :
 *   @fd      : fd for the buffer to be dumped with camera status
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::dump(int /*fd*/)
{
    ALOGE("%s: not supported yet", __func__);
    return INVALID_OPERATION;
}

/*===========================================================================
 * FUNCTION   : processAPI
 *
 * DESCRIPTION: process API calls from upper layer
 *
 * PARAMETERS :
 *   @api         : API to be processed
 *   @api_payload : ptr to API payload if any
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::processAPI(qcamera_sm_evt_enum_t api, void *api_payload)
{
    return m_stateMachine.procAPI(api, api_payload);
}

/*===========================================================================
 * FUNCTION   : processEvt
 *
 * DESCRIPTION: process Evt from backend via mm-camera-interface
 *
 * PARAMETERS :
 *   @evt         : event type to be processed
 *   @evt_payload : ptr to event payload if any
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::processEvt(qcamera_sm_evt_enum_t evt, void *evt_payload)
{
    return m_stateMachine.procEvt(evt, evt_payload);
}

/*===========================================================================
 * FUNCTION   : processSyncEvt
 *
 * DESCRIPTION: process synchronous Evt from backend
 *
 * PARAMETERS :
 *   @evt         : event type to be processed
 *   @evt_payload : ptr to event payload if any
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::processSyncEvt(qcamera_sm_evt_enum_t evt, void *evt_payload)
{
    int rc = NO_ERROR;

    pthread_mutex_lock(&m_evtLock);
    rc =  processEvt(evt, evt_payload);
    if (rc == NO_ERROR) {
        memset(&m_evtResult, 0, sizeof(qcamera_api_result_t));
        while (m_evtResult.request_api != evt) {
            pthread_cond_wait(&m_evtCond, &m_evtLock);
        }
        rc =  m_evtResult.status;
    }
    pthread_mutex_unlock(&m_evtLock);

    return rc;
}

/*===========================================================================
 * FUNCTION   : evtHandle
 *
 * DESCRIPTION: Function registerd to mm-camera-interface to handle backend events
 *
 * PARAMETERS :
 *   @camera_handle : event type to be processed
 *   @evt           : ptr to event
 *   @user_data     : user data ptr
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera2HardwareInterface::camEvtHandle(uint32_t /*camera_handle*/,
                                          mm_camera_event_t *evt,
                                          void *user_data)
{
    QCamera2HardwareInterface *obj = (QCamera2HardwareInterface *)user_data;
    if (obj && evt) {
        mm_camera_event_t *payload =
            (mm_camera_event_t *)malloc(sizeof(mm_camera_event_t));
        if (NULL != payload) {
            *payload = *evt;
            obj->processEvt(QCAMERA_SM_EVT_EVT_NOTIFY, payload);
        }
    } else {
        ALOGE("%s: NULL user_data", __func__);
    }
}

/*===========================================================================
 * FUNCTION   : jpegEvtHandle
 *
 * DESCRIPTION: Function registerd to mm-jpeg-interface to handle jpeg events
 *
 * PARAMETERS :
 *   @status    : status of jpeg job
 *   @client_hdl: jpeg client handle
 *   @jobId     : jpeg job Id
 *   @p_ouput   : ptr to jpeg output result struct
 *   @userdata  : user data ptr
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera2HardwareInterface::jpegEvtHandle(jpeg_job_status_t status,
                                              uint32_t /*client_hdl*/,
                                              uint32_t jobId,
                                              mm_jpeg_output_t *p_output,
                                              void *userdata)
{
    QCamera2HardwareInterface *obj = (QCamera2HardwareInterface *)userdata;
    if (obj) {
        qcamera_jpeg_evt_payload_t *payload =
            (qcamera_jpeg_evt_payload_t *)malloc(sizeof(qcamera_jpeg_evt_payload_t));
        if (NULL != payload) {
            memset(payload, 0, sizeof(qcamera_jpeg_evt_payload_t));
            payload->status = status;
            payload->jobId = jobId;
            if (p_output != NULL) {
                payload->out_data = *p_output;
            }
            obj->processEvt(QCAMERA_SM_EVT_JPEG_EVT_NOTIFY, payload);
        }
    } else {
        ALOGE("%s: NULL user_data", __func__);
    }
}

/*===========================================================================
 * FUNCTION   : thermalEvtHandle
 *
 * DESCRIPTION: routine to handle thermal event notification
 *
 * PARAMETERS :
 *   @level      : thermal level
 *   @userdata   : userdata passed in during registration
 *   @data       : opaque data from thermal client
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::thermalEvtHandle(
        qcamera_thermal_level_enum_t level, void *userdata, void *data)
{
    // Make sure thermal events are logged
    ALOGI("%s: level = %d, userdata = %p, data = %p",
        __func__, level, userdata, data);
    //We don't need to lockAPI, waitAPI here. QCAMERA_SM_EVT_THERMAL_NOTIFY
    // becomes an aync call. This also means we can only pass payload
    // by value, not by address.
    return processAPI(QCAMERA_SM_EVT_THERMAL_NOTIFY, (void *)level);
}

/*===========================================================================
 * FUNCTION   : sendEvtNotify
 *
 * DESCRIPTION: send event notify to notify thread
 *
 * PARAMETERS :
 *   @msg_type: msg type to be sent
 *   @ext1    : optional extension1
 *   @ext2    : optional extension2
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::sendEvtNotify(int32_t msg_type,
                                                 int32_t ext1,
                                                 int32_t ext2)
{
    qcamera_callback_argm_t cbArg;
    memset(&cbArg, 0, sizeof(qcamera_callback_argm_t));
    cbArg.cb_type = QCAMERA_NOTIFY_CALLBACK;
    cbArg.msg_type = msg_type;
    cbArg.ext1 = ext1;
    cbArg.ext2 = ext2;
    return m_cbNotifier.notifyCallback(cbArg);
}

/*===========================================================================
 * FUNCTION   : processAutoFocusEvent
 *
 * DESCRIPTION: process auto focus event
 *
 * PARAMETERS :
 *   @focus_data: struct containing auto focus result info
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::processAutoFocusEvent(cam_auto_focus_data_t &focus_data)
{
    int32_t ret = NO_ERROR;
    ALOGD("%s: E",__func__);
    m_currentFocusState = focus_data.focus_state;

    cam_focus_mode_type focusMode = mParameters.getFocusMode();
    switch (focusMode) {
    case CAM_FOCUS_MODE_AUTO:
    case CAM_FOCUS_MODE_MACRO:
        if (mParameters.isAFRunning()) {
            if (focus_data.focus_state == CAM_AF_SCANNING) {
                // in the middle of focusing, just ignore it
                break;
            }

            // update focus distance
            mParameters.updateFocusDistances(&focus_data.focus_dist);
            ret = sendEvtNotify(CAMERA_MSG_FOCUS,
                                (focus_data.focus_state == CAM_AF_FOCUSED)? true : false,
                                0);
            mParameters.setAFRunning(false);
        } else {
            ret = UNKNOWN_ERROR;
            ALOGE("%s: autoFocusEvent when no auto_focus running", __func__);
        }
        break;
    case CAM_FOCUS_MODE_CONTINOUS_VIDEO:
    case CAM_FOCUS_MODE_CONTINOUS_PICTURE:
        if (focus_data.focus_state == CAM_AF_FOCUSED ||
            focus_data.focus_state == CAM_AF_NOT_FOCUSED) {
            // update focus distance
            mParameters.updateFocusDistances(&focus_data.focus_dist);
            if (mParameters.isLockCAFNeeded()) {
                mParameters.setLockCAFNeeded(false);
                ret = mParameters.setLockCAF(true);
            }

            ret = sendEvtNotify(CAMERA_MSG_FOCUS,
                  (focus_data.focus_state == CAM_AF_FOCUSED)? true : false,
                  0);
        }
        ret = sendEvtNotify(CAMERA_MSG_FOCUS_MOVE,
                (focus_data.focus_state == CAM_AF_SCANNING)? true : false,
                0);
        break;
    case CAM_FOCUS_MODE_INFINITY:
    case CAM_FOCUS_MODE_FIXED:
    case CAM_FOCUS_MODE_EDOF:
    default:
        ALOGD("%s: no ops for autofocus event in focusmode %d", __func__, focusMode);
        break;
    }
    ALOGD("%s: X",__func__);
    return ret;
}

/*===========================================================================
 * FUNCTION   : processZoomEvent
 *
 * DESCRIPTION: process zoom event
 *
 * PARAMETERS :
 *   @crop_info : crop info as a result of zoom operation
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::processZoomEvent(cam_crop_data_t &crop_info)
{
    int32_t ret = NO_ERROR;

    for (int i = 0; i < QCAMERA_CH_TYPE_MAX; i++) {
        if (m_channels[i] != NULL) {
            ret = m_channels[i]->processZoomDone(mPreviewWindow, crop_info);
        }
    }
    return ret;
}

/*===========================================================================
 * FUNCTION   : processPrepSnapshotDone
 *
 * DESCRIPTION: process prep snapshot done event
 *
 * PARAMETERS :
 *   @prep_snapshot_state  : state of prepare snapshot done. In other words,
 *                           i.e. whether need future frames for capture.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::processPrepSnapshotDoneEvent(
                        cam_prep_snapshot_state_t prep_snapshot_state)
{
    int32_t ret = NO_ERROR;

    if (m_channels[QCAMERA_CH_TYPE_ZSL] &&
        prep_snapshot_state == NEED_FUTURE_FRAME) {

        ret = mCameraHandle->ops->start_zsl_snapshot(
                            mCameraHandle->camera_handle);
        if (ret < 0) {
            ALOGE("%s: start_led_zsl_capture failed %d",
                            __func__, ret);
            return ret;
        }
        m_bStartZSLSnapshotCalled = true;
    }
    return ret;
}

/*===========================================================================
 * FUNCTION   : processJpegNotify
 *
 * DESCRIPTION: process jpeg event
 *
 * PARAMETERS :
 *   @jpeg_evt: ptr to jpeg event payload
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::processJpegNotify(qcamera_jpeg_evt_payload_t *jpeg_evt)
{
    return m_postprocessor.processJpegEvt(jpeg_evt);
}

/*===========================================================================
 * FUNCTION   : lockAPI
 *
 * DESCRIPTION: lock to process API
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera2HardwareInterface::lockAPI()
{
    pthread_mutex_lock(&m_lock);
}

/*===========================================================================
 * FUNCTION   : waitAPIResult
 *
 * DESCRIPTION: wait for API result coming back. This is a blocking call, it will
 *              return only cerntain API event type arrives
 *
 * PARAMETERS :
 *   @api_evt : API event type
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera2HardwareInterface::waitAPIResult(qcamera_sm_evt_enum_t api_evt)
{
    ALOGV("%s: wait for API result of evt (%d)", __func__, api_evt);
    memset(&m_apiResult, 0, sizeof(qcamera_api_result_t));
    while (m_apiResult.request_api != api_evt) {
        pthread_cond_wait(&m_cond, &m_lock);
    }
    ALOGV("%s: return (%d) from API result wait for evt (%d)",
          __func__, m_apiResult.status, api_evt);
}

/*===========================================================================
 * FUNCTION   : unlockAPI
 *
 * DESCRIPTION: API processing is done, unlock
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera2HardwareInterface::unlockAPI()
{
    pthread_mutex_unlock(&m_lock);
}

/*===========================================================================
 * FUNCTION   : signalAPIResult
 *
 * DESCRIPTION: signal condition viarable that cerntain API event type arrives
 *
 * PARAMETERS :
 *   @result  : API result
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera2HardwareInterface::signalAPIResult(qcamera_api_result_t *result)
{
    pthread_mutex_lock(&m_lock);
    m_apiResult = *result;
    pthread_cond_signal(&m_cond);
    pthread_mutex_unlock(&m_lock);
}

/*===========================================================================
 * FUNCTION   : signalEvtResult
 *
 * DESCRIPTION: signal condition variable that certain event was processed
 *
 * PARAMETERS :
 *   @result  : Event result
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera2HardwareInterface::signalEvtResult(qcamera_api_result_t *result)
{
    pthread_mutex_lock(&m_evtLock);
    m_evtResult = *result;
    pthread_cond_signal(&m_evtCond);
    pthread_mutex_unlock(&m_evtLock);
}

/*===========================================================================
 * FUNCTION   : addStreamToChannel
 *
 * DESCRIPTION: add a stream into a channel
 *
 * PARAMETERS :
 *   @pChannel   : ptr to channel obj
 *   @streamType : type of stream to be added
 *   @streamCB   : callback of stream
 *   @userData   : user data ptr to callback
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::addStreamToChannel(QCameraChannel *pChannel,
                                                      cam_stream_type_t streamType,
                                                      stream_cb_routine streamCB,
                                                      void *userData)
{
    int32_t rc = NO_ERROR;
    QCameraHeapMemory *pStreamInfo = allocateStreamInfoBuf(streamType);
    if (pStreamInfo == NULL) {
        ALOGE("%s: no mem for stream info buf", __func__);
        return NO_MEMORY;
    }
    uint8_t minStreamBufNum = getBufNumRequired(streamType);
    rc = pChannel->addStream(*this,
                             pStreamInfo,
                             minStreamBufNum,
                             &gCamCapability[mCameraId]->padding_info,
                             streamCB, userData);
    if (rc != NO_ERROR) {
        ALOGE("%s: add stream type (%d) failed, ret = %d",
              __func__, streamType, rc);
        pStreamInfo->deallocate();
        delete pStreamInfo;
        return rc;
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : addPreviewChannel
 *
 * DESCRIPTION: add a preview channel that contains a preview stream
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::addPreviewChannel()
{
    int32_t rc = NO_ERROR;
    QCameraChannel *pChannel = NULL;

    if (m_channels[QCAMERA_CH_TYPE_PREVIEW] != NULL) {
        // if we had preview channel before, delete it first
        delete m_channels[QCAMERA_CH_TYPE_PREVIEW];
        m_channels[QCAMERA_CH_TYPE_PREVIEW] = NULL;
    }

    pChannel = new QCameraChannel(mCameraHandle->camera_handle,
                                  mCameraHandle->ops);
    if (NULL == pChannel) {
        ALOGE("%s: no mem for preview channel", __func__);
        return NO_MEMORY;
    }

    // preview only channel, don't need bundle attr and cb
    rc = pChannel->init(NULL, NULL, NULL);
    if (rc != NO_ERROR) {
        ALOGE("%s: init preview channel failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    // meta data stream always coexists with preview if applicable
    rc = addStreamToChannel(pChannel, CAM_STREAM_TYPE_METADATA,
                            metadata_stream_cb_routine, this);
    if (rc != NO_ERROR) {
        ALOGE("%s: add metadata stream failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    if (isNoDisplayMode()) {
        rc = addStreamToChannel(pChannel, CAM_STREAM_TYPE_PREVIEW,
                                nodisplay_preview_stream_cb_routine, this);
    } else {
        rc = addStreamToChannel(pChannel, CAM_STREAM_TYPE_PREVIEW,
                                preview_stream_cb_routine, this);
    }
    if (rc != NO_ERROR) {
        ALOGE("%s: add preview stream failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    m_channels[QCAMERA_CH_TYPE_PREVIEW] = pChannel;
    return rc;
}

/*===========================================================================
 * FUNCTION   : addVideoChannel
 *
 * DESCRIPTION: add a video channel that contains a video stream
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::addVideoChannel()
{
    int32_t rc = NO_ERROR;
    QCameraVideoChannel *pChannel = NULL;

    if (m_channels[QCAMERA_CH_TYPE_VIDEO] != NULL) {
        // if we had video channel before, delete it first
        delete m_channels[QCAMERA_CH_TYPE_VIDEO];
        m_channels[QCAMERA_CH_TYPE_VIDEO] = NULL;
    }

    pChannel = new QCameraVideoChannel(mCameraHandle->camera_handle,
                                       mCameraHandle->ops);
    if (NULL == pChannel) {
        ALOGE("%s: no mem for video channel", __func__);
        return NO_MEMORY;
    }

    // preview only channel, don't need bundle attr and cb
    rc = pChannel->init(NULL, NULL, NULL);
    if (rc != 0) {
        ALOGE("%s: init video channel failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    rc = addStreamToChannel(pChannel, CAM_STREAM_TYPE_VIDEO,
                            video_stream_cb_routine, this);
    if (rc != NO_ERROR) {
        ALOGE("%s: add video stream failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    m_channels[QCAMERA_CH_TYPE_VIDEO] = pChannel;
    return rc;
}

/*===========================================================================
 * FUNCTION   : addSnapshotChannel
 *
 * DESCRIPTION: add a snapshot channel that contains a snapshot stream
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 * NOTE       : Add this channel for live snapshot usecase. Regular capture will
 *              use addCaptureChannel.
 *==========================================================================*/
int32_t QCamera2HardwareInterface::addSnapshotChannel()
{
    int32_t rc = NO_ERROR;
    QCameraChannel *pChannel = NULL;

    if (m_channels[QCAMERA_CH_TYPE_SNAPSHOT] != NULL) {
        // if we had ZSL channel before, delete it first
        delete m_channels[QCAMERA_CH_TYPE_SNAPSHOT];
        m_channels[QCAMERA_CH_TYPE_SNAPSHOT] = NULL;
    }

    pChannel = new QCameraChannel(mCameraHandle->camera_handle,
                                  mCameraHandle->ops);
    if (NULL == pChannel) {
        ALOGE("%s: no mem for snapshot channel", __func__);
        return NO_MEMORY;
    }

    rc = pChannel->init(NULL, NULL, NULL);
    if (rc != NO_ERROR) {
        ALOGE("%s: init snapshot channel failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    rc = addStreamToChannel(pChannel, CAM_STREAM_TYPE_SNAPSHOT,
                            snapshot_stream_cb_routine, this);
    if (rc != NO_ERROR) {
        ALOGE("%s: add snapshot stream failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    m_channels[QCAMERA_CH_TYPE_SNAPSHOT] = pChannel;
    return rc;
}

/*===========================================================================
 * FUNCTION   : addRawChannel
 *
 * DESCRIPTION: add a raw channel that contains a raw image stream
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::addRawChannel()
{
    int32_t rc = NO_ERROR;
    QCameraChannel *pChannel = NULL;

    if (m_channels[QCAMERA_CH_TYPE_RAW] != NULL) {
        // if we had raw channel before, delete it first
        delete m_channels[QCAMERA_CH_TYPE_RAW];
        m_channels[QCAMERA_CH_TYPE_RAW] = NULL;
    }

    pChannel = new QCameraChannel(mCameraHandle->camera_handle,
                                  mCameraHandle->ops);
    if (NULL == pChannel) {
        ALOGE("%s: no mem for raw channel", __func__);
        return NO_MEMORY;
    }

    rc = pChannel->init(NULL, NULL, NULL);
    if (rc != NO_ERROR) {
        ALOGE("%s: init raw channel failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    // meta data stream always coexists with snapshot in regular RAW capture case
    rc = addStreamToChannel(pChannel, CAM_STREAM_TYPE_METADATA,
                            metadata_stream_cb_routine, this);
    if (rc != NO_ERROR) {
        ALOGE("%s: add metadata stream failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    rc = addStreamToChannel(pChannel, CAM_STREAM_TYPE_RAW,
                            raw_stream_cb_routine, this);
    if (rc != NO_ERROR) {
        ALOGE("%s: add snapshot stream failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    m_channels[QCAMERA_CH_TYPE_RAW] = pChannel;
    return rc;
}

/*===========================================================================
 * FUNCTION   : addZSLChannel
 *
 * DESCRIPTION: add a ZSL channel that contains a preview stream and
 *              a snapshot stream
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::addZSLChannel()
{
    int32_t rc = NO_ERROR;
    QCameraPicChannel *pChannel = NULL;

    if (m_channels[QCAMERA_CH_TYPE_ZSL] != NULL) {
        // if we had ZSL channel before, delete it first
        delete m_channels[QCAMERA_CH_TYPE_ZSL];
        m_channels[QCAMERA_CH_TYPE_ZSL] = NULL;
    }

    pChannel = new QCameraPicChannel(mCameraHandle->camera_handle,
                                     mCameraHandle->ops);
    if (NULL == pChannel) {
        ALOGE("%s: no mem for ZSL channel", __func__);
        return NO_MEMORY;
    }

    // ZSL channel, init with bundle attr and cb
    mm_camera_channel_attr_t attr;
    memset(&attr, 0, sizeof(mm_camera_channel_attr_t));
    attr.notify_mode = MM_CAMERA_SUPER_BUF_NOTIFY_BURST;
    attr.look_back = mParameters.getZSLBackLookCount();
    attr.post_frame_skip = mParameters.getZSLBurstInterval();
    attr.water_mark = mParameters.getZSLQueueDepth();
    attr.max_unmatched_frames = mParameters.getMaxUnmatchedFramesInQueue();
    rc = pChannel->init(&attr,
                        zsl_channel_cb,
                        this);
    if (rc != 0) {
        ALOGE("%s: init ZSL channel failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    // meta data stream always coexists with preview if applicable
    rc = addStreamToChannel(pChannel, CAM_STREAM_TYPE_METADATA,
                            metadata_stream_cb_routine, this);
    if (rc != NO_ERROR) {
        ALOGE("%s: add metadata stream failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    if (isNoDisplayMode()) {
        rc = addStreamToChannel(pChannel, CAM_STREAM_TYPE_PREVIEW,
                                nodisplay_preview_stream_cb_routine, this);
    } else {
        rc = addStreamToChannel(pChannel, CAM_STREAM_TYPE_PREVIEW,
                                preview_stream_cb_routine, this);
    }
    if (rc != NO_ERROR) {
        ALOGE("%s: add preview stream failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    rc = addStreamToChannel(pChannel, CAM_STREAM_TYPE_SNAPSHOT,
                            NULL, this);
    if (rc != NO_ERROR) {
        ALOGE("%s: add snapshot stream failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    m_channels[QCAMERA_CH_TYPE_ZSL] = pChannel;
    return rc;
}

/*===========================================================================
 * FUNCTION   : addCaptureChannel
 *
 * DESCRIPTION: add a capture channel that contains a snapshot stream
 *              and a postview stream
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 * NOTE       : Add this channel for regular capture usecase.
 *              For Live snapshot usecase, use addSnapshotChannel.
 *==========================================================================*/
int32_t QCamera2HardwareInterface::addCaptureChannel()
{
    int32_t rc = NO_ERROR;
    QCameraChannel *pChannel = NULL;

    if (m_channels[QCAMERA_CH_TYPE_CAPTURE] != NULL) {
        delete m_channels[QCAMERA_CH_TYPE_CAPTURE];
        m_channels[QCAMERA_CH_TYPE_CAPTURE] = NULL;
    }

    pChannel = new QCameraChannel(mCameraHandle->camera_handle,
                                  mCameraHandle->ops);
    if (NULL == pChannel) {
        ALOGE("%s: no mem for capture channel", __func__);
        return NO_MEMORY;
    }

    // Capture channel, only need snapshot and postview streams start together
    mm_camera_channel_attr_t attr;
    memset(&attr, 0, sizeof(mm_camera_channel_attr_t));
    attr.notify_mode = MM_CAMERA_SUPER_BUF_NOTIFY_CONTINUOUS;
    attr.max_unmatched_frames = mParameters.getMaxUnmatchedFramesInQueue();

    rc = pChannel->init(&attr,
                        capture_channel_cb_routine,
                        this);
    if (rc != NO_ERROR) {
        ALOGE("%s: init capture channel failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }


    // meta data stream always coexists with snapshot in regular capture case
    rc = addStreamToChannel(pChannel, CAM_STREAM_TYPE_METADATA,
                            metadata_stream_cb_routine, this);
    if (rc != NO_ERROR) {
        ALOGE("%s: add metadata stream failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    rc = addStreamToChannel(pChannel, CAM_STREAM_TYPE_POSTVIEW,
                            postview_stream_cb_routine, this);

    if (rc != NO_ERROR) {
        ALOGE("%s: add postview stream failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    rc = addStreamToChannel(pChannel, CAM_STREAM_TYPE_SNAPSHOT,
                            NULL, this);
    if (rc != NO_ERROR) {
        ALOGE("%s: add snapshot stream failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    m_channels[QCAMERA_CH_TYPE_CAPTURE] = pChannel;
    return rc;
}

/*===========================================================================
 * FUNCTION   : addMetaDataChannel
 *
 * DESCRIPTION: add a meta data channel that contains a metadata stream
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::addMetaDataChannel()
{
    int32_t rc = NO_ERROR;
    QCameraChannel *pChannel = NULL;

    if (m_channels[QCAMERA_CH_TYPE_METADATA] != NULL) {
        delete m_channels[QCAMERA_CH_TYPE_METADATA];
        m_channels[QCAMERA_CH_TYPE_METADATA] = NULL;
    }

    pChannel = new QCameraChannel(mCameraHandle->camera_handle,
                                  mCameraHandle->ops);
    if (NULL == pChannel) {
        ALOGE("%s: no mem for metadata channel", __func__);
        return NO_MEMORY;
    }

    rc = pChannel->init(NULL,
                        NULL,
                        NULL);
    if (rc != NO_ERROR) {
        ALOGE("%s: init metadata channel failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    rc = addStreamToChannel(pChannel, CAM_STREAM_TYPE_METADATA,
                            metadata_stream_cb_routine, this);
    if (rc != NO_ERROR) {
        ALOGE("%s: add metadata stream failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    m_channels[QCAMERA_CH_TYPE_METADATA] = pChannel;
    return rc;
}

/*===========================================================================
 * FUNCTION   : addOnlineReprocChannel
 *
 * DESCRIPTION: add a online reprocess channel that will do reprocess on frames
 *              coming from input channel
 *
 * PARAMETERS :
 *   @pInputChannel : ptr to input channel whose frames will be post-processed
 *
 * RETURN     : Ptr to the newly created channel obj. NULL if failed.
 *==========================================================================*/
QCameraReprocessChannel *QCamera2HardwareInterface::addOnlineReprocChannel(
                                                      QCameraChannel *pInputChannel)
{
    int32_t rc = NO_ERROR;
    QCameraReprocessChannel *pChannel = NULL;

    if (pInputChannel == NULL) {
        ALOGE("%s: input channel obj is NULL", __func__);
        return NULL;
    }

    pChannel = new QCameraReprocessChannel(mCameraHandle->camera_handle,
                                           mCameraHandle->ops);
    if (NULL == pChannel) {
        ALOGE("%s: no mem for reprocess channel", __func__);
        return NULL;
    }

    // Capture channel, only need snapshot and postview streams start together
    mm_camera_channel_attr_t attr;
    memset(&attr, 0, sizeof(mm_camera_channel_attr_t));
    attr.notify_mode = MM_CAMERA_SUPER_BUF_NOTIFY_CONTINUOUS;
    attr.max_unmatched_frames = mParameters.getMaxUnmatchedFramesInQueue();
    rc = pChannel->init(&attr,
                        postproc_channel_cb_routine,
                        this);
    if (rc != NO_ERROR) {
        ALOGE("%s: init reprocess channel failed, ret = %d", __func__, rc);
        delete pChannel;
        return NULL;
    }

    // pp feature config
    cam_pp_feature_config_t pp_config;
    memset(&pp_config, 0, sizeof(cam_pp_feature_config_t));
    if (gCamCapability[mCameraId]->min_required_pp_mask & CAM_QCOM_FEATURE_SHARPNESS) {
        pp_config.feature_mask |= CAM_QCOM_FEATURE_SHARPNESS;
        pp_config.sharpness = mParameters.getInt(QCameraParameters::KEY_QC_SHARPNESS);
    }

    if (mParameters.isWNREnabled()) {
        pp_config.feature_mask |= CAM_QCOM_FEATURE_DENOISE2D;
        pp_config.denoise2d.denoise_enable = 1;
        pp_config.denoise2d.process_plates = mParameters.getWaveletDenoiseProcessPlate();
    }

    if (isCACEnabled()) {
        pp_config.feature_mask |= CAM_QCOM_FEATURE_CAC;
    }

    if (needRotationReprocess()) {
        pp_config.feature_mask |= CAM_QCOM_FEATURE_ROTATION;
        int rotation = mParameters.getJpegRotation();
        if (rotation == 0) {
            pp_config.rotation = ROTATE_0;
        } else if (rotation == 90) {
            pp_config.rotation = ROTATE_90;
        } else if (rotation == 180) {
            pp_config.rotation = ROTATE_180;
        } else if (rotation == 270) {
            pp_config.rotation = ROTATE_270;
        }
    }

    uint8_t minStreamBufNum = mParameters.getNumOfSnapshots();
    rc = pChannel->addReprocStreamsFromSource(*this,
                                              pp_config,
                                              pInputChannel,
                                              minStreamBufNum,
                                              &gCamCapability[mCameraId]->padding_info);
    if (rc != NO_ERROR) {
        delete pChannel;
        return NULL;
    }

    return pChannel;
}

/*===========================================================================
 * FUNCTION   : addOfflineReprocChannel
 *
 * DESCRIPTION: add a offline reprocess channel contains one reproc stream,
 *              that will do reprocess on frames coming from external images
 *
 * PARAMETERS :
 *   @img_config  : offline reporcess image info
 *   @pp_feature  : pp feature config
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
QCameraReprocessChannel *QCamera2HardwareInterface::addOfflineReprocChannel(
                                            cam_pp_offline_src_config_t &img_config,
                                            cam_pp_feature_config_t &pp_feature,
                                            stream_cb_routine stream_cb,
                                            void *userdata)
{
    int32_t rc = NO_ERROR;
    QCameraReprocessChannel *pChannel = NULL;

    pChannel = new QCameraReprocessChannel(mCameraHandle->camera_handle,
                                           mCameraHandle->ops);
    if (NULL == pChannel) {
        ALOGE("%s: no mem for reprocess channel", __func__);
        return NULL;
    }

    rc = pChannel->init(NULL, NULL, NULL);
    if (rc != NO_ERROR) {
        ALOGE("%s: init reprocess channel failed, ret = %d", __func__, rc);
        delete pChannel;
        return NULL;
    }

    QCameraHeapMemory *pStreamInfo = allocateStreamInfoBuf(CAM_STREAM_TYPE_OFFLINE_PROC);
    if (pStreamInfo == NULL) {
        ALOGE("%s: no mem for stream info buf", __func__);
        delete pChannel;
        return NULL;
    }

    cam_stream_info_t *streamInfoBuf = (cam_stream_info_t *)pStreamInfo->getPtr(0);
    memset(streamInfoBuf, 0, sizeof(cam_stream_info_t));
    streamInfoBuf->stream_type = CAM_STREAM_TYPE_OFFLINE_PROC;
    streamInfoBuf->fmt = img_config.input_fmt;
    streamInfoBuf->dim = img_config.input_dim;
    streamInfoBuf->buf_planes = img_config.input_buf_planes;
    streamInfoBuf->streaming_mode = CAM_STREAMING_MODE_BURST;
    streamInfoBuf->num_of_burst = img_config.num_of_bufs;

    streamInfoBuf->reprocess_config.pp_type = CAM_OFFLINE_REPROCESS_TYPE;
    streamInfoBuf->reprocess_config.offline = img_config;
    streamInfoBuf->reprocess_config.pp_feature_config = pp_feature;

    rc = pChannel->addStream(*this,
                             pStreamInfo, img_config.num_of_bufs,
                             &gCamCapability[mCameraId]->padding_info,
                             stream_cb, userdata);

    if (rc != NO_ERROR) {
        ALOGE("%s: add reprocess stream failed, ret = %d", __func__, rc);
        pStreamInfo->deallocate();
        delete pStreamInfo;
        delete pChannel;
        return NULL;
    }

    return pChannel;
}

/*===========================================================================
 * FUNCTION   : addChannel
 *
 * DESCRIPTION: add a channel by its type
 *
 * PARAMETERS :
 *   @ch_type : channel type
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::addChannel(qcamera_ch_type_enum_t ch_type)
{
    int32_t rc = UNKNOWN_ERROR;
    switch (ch_type) {
    case QCAMERA_CH_TYPE_ZSL:
        rc = addZSLChannel();
        break;
    case QCAMERA_CH_TYPE_CAPTURE:
        rc = addCaptureChannel();
        break;
    case QCAMERA_CH_TYPE_PREVIEW:
        rc = addPreviewChannel();
        break;
    case QCAMERA_CH_TYPE_VIDEO:
        rc = addVideoChannel();
        break;
    case QCAMERA_CH_TYPE_SNAPSHOT:
        rc = addSnapshotChannel();
        break;
    case QCAMERA_CH_TYPE_RAW:
        rc = addRawChannel();
        break;
    case QCAMERA_CH_TYPE_METADATA:
        rc = addMetaDataChannel();
        break;
    default:
        break;
    }
    return rc;
}

/*===========================================================================
 * FUNCTION   : delChannel
 *
 * DESCRIPTION: delete a channel by its type
 *
 * PARAMETERS :
 *   @ch_type : channel type
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::delChannel(qcamera_ch_type_enum_t ch_type)
{
    if (m_channels[ch_type] != NULL) {
        delete m_channels[ch_type];
        m_channels[ch_type] = NULL;
    }

    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : startChannel
 *
 * DESCRIPTION: start a channel by its type
 *
 * PARAMETERS :
 *   @ch_type : channel type
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::startChannel(qcamera_ch_type_enum_t ch_type)
{
    int32_t rc = UNKNOWN_ERROR;
    if (m_channels[ch_type] != NULL) {
        rc = m_channels[ch_type]->start();
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : stopChannel
 *
 * DESCRIPTION: stop a channel by its type
 *
 * PARAMETERS :
 *   @ch_type : channel type
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::stopChannel(qcamera_ch_type_enum_t ch_type)
{
    int32_t rc = UNKNOWN_ERROR;
    if (m_channels[ch_type] != NULL) {
        rc = m_channels[ch_type]->stop();
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : preparePreview
 *
 * DESCRIPTION: add channels needed for preview
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::preparePreview()
{
    int32_t rc = NO_ERROR;

    if (mParameters.isZSLMode() && mParameters.getRecordingHintValue() !=true) {
        rc = addChannel(QCAMERA_CH_TYPE_ZSL);
        if (rc != NO_ERROR) {
            return rc;
        }
    } else {
        bool recordingHint = mParameters.getRecordingHintValue();
        if(recordingHint) {
            rc = addChannel(QCAMERA_CH_TYPE_SNAPSHOT);
            if (rc != NO_ERROR) {
                return rc;
            }

            rc = addChannel(QCAMERA_CH_TYPE_VIDEO);
            if (rc != NO_ERROR) {
                delChannel(QCAMERA_CH_TYPE_SNAPSHOT);
                return rc;
            }
        }

        rc = addChannel(QCAMERA_CH_TYPE_PREVIEW);
        if (rc != NO_ERROR) {
            if (recordingHint) {
                delChannel(QCAMERA_CH_TYPE_SNAPSHOT);
                delChannel(QCAMERA_CH_TYPE_VIDEO);
            }
            return rc;
        }

    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : unpreparePreview
 *
 * DESCRIPTION: delete channels for preview
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera2HardwareInterface::unpreparePreview()
{
    if (mParameters.isZSLMode() && mParameters.getRecordingHintValue() !=true) {
        delChannel(QCAMERA_CH_TYPE_ZSL);
    } else {
        delChannel(QCAMERA_CH_TYPE_PREVIEW);
        if(mParameters.getRecordingHintValue() == true) {
            delChannel(QCAMERA_CH_TYPE_VIDEO);
            delChannel(QCAMERA_CH_TYPE_SNAPSHOT);
        }
    }
}

/*===========================================================================
 * FUNCTION   : playShutter
 *
 * DESCRIPTION: send request to play shutter sound
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera2HardwareInterface::playShutter(){
     if (mNotifyCb == NULL ||
         msgTypeEnabledWithLock(CAMERA_MSG_SHUTTER) == 0){
         ALOGV("%s: shutter msg not enabled or NULL cb", __func__);
         return;
     }

     qcamera_callback_argm_t cbArg;
     memset(&cbArg, 0, sizeof(qcamera_callback_argm_t));
     cbArg.cb_type = QCAMERA_NOTIFY_CALLBACK;
     cbArg.msg_type = CAMERA_MSG_SHUTTER;
     cbArg.ext1 = 0;

     if(!m_bShutterSoundPlayed){
         cbArg.ext2 = true;
         m_cbNotifier.notifyCallback(cbArg);
     }
     cbArg.ext2 = false;
     m_cbNotifier.notifyCallback(cbArg);
     m_bShutterSoundPlayed = false;
}

/*===========================================================================
 * FUNCTION   : getChannelByHandle
 *
 * DESCRIPTION: return a channel by its handle
 *
 * PARAMETERS :
 *   @channelHandle : channel handle
 *
 * RETURN     : a channel obj if found, NULL if not found
 *==========================================================================*/
QCameraChannel *QCamera2HardwareInterface::getChannelByHandle(uint32_t channelHandle)
{
    for(int i = 0; i < QCAMERA_CH_TYPE_MAX; i++) {
        if (m_channels[i] != NULL &&
            m_channels[i]->getMyHandle() == channelHandle) {
            return m_channels[i];
        }
    }

    return NULL;
}

/*===========================================================================
 * FUNCTION   : processFaceDetectionReuslt
 *
 * DESCRIPTION: process face detection reuslt
 *
 * PARAMETERS :
 *   @fd_data : ptr to face detection result struct
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::processFaceDetectionResult(cam_face_detection_data_t *fd_data)
{
    if (!mParameters.isFaceDetectionEnabled()) {
        ALOGD("%s: FaceDetection not enabled, no ops here", __func__);
        return NO_ERROR;
    }

    if ((NULL == mDataCb) || (msgTypeEnabledWithLock(CAMERA_MSG_PREVIEW_METADATA) == 0)) {
        ALOGD("%s: prevew metadata msgtype not enabled, no ops here", __func__);
        return NO_ERROR;
    }

    cam_dimension_t display_dim;
    mParameters.getStreamDimension(CAM_STREAM_TYPE_PREVIEW, display_dim);
    if (display_dim.width <= 0 || display_dim.height <= 0) {
        ALOGE("%s: Invalid preview width or height (%d x %d)",
              __func__, display_dim.width, display_dim.height);
        return UNKNOWN_ERROR;
    }

    // process face detection result
    size_t faceResultSize = sizeof(camera_frame_metadata_t);
    faceResultSize += sizeof(camera_face_t) * MAX_ROI;
    camera_memory_t *faceResultBuffer = mGetMemory(-1,
                                                   faceResultSize,
                                                   1,
                                                   mCallbackCookie);
    if ( NULL == faceResultBuffer ) {
        ALOGE("%s: Not enough memory for face result data",
              __func__);
        return NO_MEMORY;
    }

    unsigned char *faceData = ( unsigned char * ) faceResultBuffer->data;
    memset(faceData, 0, faceResultSize);
    camera_frame_metadata_t *roiData = (camera_frame_metadata_t * ) faceData;
    camera_face_t *faces = (camera_face_t *) ( faceData + sizeof(camera_frame_metadata_t) );

    roiData->number_of_faces = fd_data->num_faces_detected;
    roiData->faces = faces;
    if (roiData->number_of_faces > 0) {
        for (int i = 0; i < roiData->number_of_faces; i++) {
            faces[i].id = fd_data->faces[i].face_id;
            faces[i].score = fd_data->faces[i].score;

            // left
            faces[i].rect[0] =
                MAP_TO_DRIVER_COORDINATE(fd_data->faces[i].face_boundary.left, display_dim.width, 2000, -1000);

            // top
            faces[i].rect[1] =
                MAP_TO_DRIVER_COORDINATE(fd_data->faces[i].face_boundary.top, display_dim.height, 2000, -1000);

            // right
            faces[i].rect[2] = faces[i].rect[0] +
                MAP_TO_DRIVER_COORDINATE(fd_data->faces[i].face_boundary.width, display_dim.width, 2000, 0);

             // bottom
            faces[i].rect[3] = faces[i].rect[1] +
                MAP_TO_DRIVER_COORDINATE(fd_data->faces[i].face_boundary.height, display_dim.height, 2000, 0);

            // Center of left eye
            faces[i].left_eye[0] =
                MAP_TO_DRIVER_COORDINATE(fd_data->faces[i].left_eye_center.x, display_dim.width, 2000, -1000);

            faces[i].left_eye[1] =
                MAP_TO_DRIVER_COORDINATE(fd_data->faces[i].left_eye_center.y, display_dim.height, 2000, -1000);

            // Center of right eye
            faces[i].right_eye[0] =
                MAP_TO_DRIVER_COORDINATE(fd_data->faces[i].right_eye_center.x, display_dim.width, 2000, -1000);

            faces[i].right_eye[1] =
                MAP_TO_DRIVER_COORDINATE(fd_data->faces[i].right_eye_center.y, display_dim.height, 2000, -1000);

            // Center of mouth
            faces[i].mouth[0] =
                MAP_TO_DRIVER_COORDINATE(fd_data->faces[i].mouth_center.x, display_dim.width, 2000, -1000);

            faces[i].mouth[1] =
                MAP_TO_DRIVER_COORDINATE(fd_data->faces[i].mouth_center.y, display_dim.height, 2000, -1000);

            faces[i].smile_degree = fd_data->faces[i].smile_degree;
            faces[i].smile_score = fd_data->faces[i].smile_confidence;
            faces[i].blink_detected = fd_data->faces[i].blink_detected;
            faces[i].face_recognised = fd_data->faces[i].face_recognised;
            faces[i].gaze_angle = fd_data->faces[i].gaze_angle;

            // upscale by 2 to recover from demaen downscaling
            faces[i].updown_dir = fd_data->faces[i].updown_dir * 2;
            faces[i].leftright_dir = fd_data->faces[i].leftright_dir * 2;
            faces[i].roll_dir = fd_data->faces[i].roll_dir * 2;

            faces[i].leye_blink = fd_data->faces[i].left_blink;
            faces[i].reye_blink = fd_data->faces[i].right_blink;
            faces[i].left_right_gaze = fd_data->faces[i].left_right_gaze;
            faces[i].top_bottom_gaze = fd_data->faces[i].top_bottom_gaze;

        }
    }

    qcamera_callback_argm_t cbArg;
    memset(&cbArg, 0, sizeof(qcamera_callback_argm_t));
    cbArg.cb_type = QCAMERA_DATA_CALLBACK;
    cbArg.msg_type = CAMERA_MSG_PREVIEW_METADATA;
    cbArg.data = faceResultBuffer;
    cbArg.metadata = roiData;
    cbArg.user_data = faceResultBuffer;
    cbArg.cookie = this;
    cbArg.release_cb = releaseCameraMemory;
    int32_t rc = m_cbNotifier.notifyCallback(cbArg);
    if (rc != NO_ERROR) {
        ALOGE("%s: fail sending notification", __func__);
        faceResultBuffer->release(faceResultBuffer);
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : releaseCameraMemory
 *
 * DESCRIPTION: releases camera memory objects
 *
 * PARAMETERS :
 *   @data    : buffer to be released
 *   @cookie  : context data
 *
 * RETURN     : None
 *==========================================================================*/
void QCamera2HardwareInterface::releaseCameraMemory(void *data, void */*cookie*/)
{
    camera_memory_t *mem = ( camera_memory_t * ) data;
    if ( NULL != mem ) {
        mem->release(mem);
    }
}

/*===========================================================================
 * FUNCTION   : returnStreamBuffer
 *
 * DESCRIPTION: returns back a stream buffer
 *
 * PARAMETERS :
 *   @data    : buffer to be released
 *   @cookie  : context data
 *
 * RETURN     : None
 *==========================================================================*/
void QCamera2HardwareInterface::returnStreamBuffer(void *data, void *cookie)
{
    QCameraStream *stream = ( QCameraStream * ) cookie;
    int idx = ( int ) data;
    if ( ( NULL != stream )) {
        stream->bufDone(idx);
    }
}

/*===========================================================================
 * FUNCTION   : processHistogramStats
 *
 * DESCRIPTION: process histogram stats
 *
 * PARAMETERS :
 *   @hist_data : ptr to histogram stats struct
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::processHistogramStats(cam_hist_stats_t &stats_data)
{
    if (!mParameters.isHistogramEnabled()) {
        ALOGD("%s: Histogram not enabled, no ops here", __func__);
        return NO_ERROR;
    }

    camera_memory_t *histBuffer = mGetMemory(-1,
                                             sizeof(cam_histogram_data_t),
                                             1,
                                             mCallbackCookie);
    if ( NULL == histBuffer ) {
        ALOGE("%s: Not enough memory for histogram data",
              __func__);
        return NO_MEMORY;
    }

    cam_histogram_data_t *pHistData = (cam_histogram_data_t *)histBuffer->data;
    if (pHistData == NULL) {
        ALOGE("%s: memory data ptr is NULL", __func__);
        return UNKNOWN_ERROR;
    }

    switch (stats_data.type) {
    case CAM_HISTOGRAM_TYPE_BAYER:
        *pHistData = stats_data.bayer_stats.gb_stats;
        break;
    case CAM_HISTOGRAM_TYPE_YUV:
        *pHistData = stats_data.yuv_stats;
        break;
    }

    qcamera_callback_argm_t cbArg;
    memset(&cbArg, 0, sizeof(qcamera_callback_argm_t));
    cbArg.cb_type = QCAMERA_DATA_CALLBACK;
    cbArg.msg_type = CAMERA_MSG_STATS_DATA;
    cbArg.data = histBuffer;
    cbArg.user_data = histBuffer;
    cbArg.cookie = this;
    cbArg.release_cb = releaseCameraMemory;
    int32_t rc = m_cbNotifier.notifyCallback(cbArg);
    if (rc != NO_ERROR) {
        ALOGE("%s: fail sending notification", __func__);
        histBuffer->release(histBuffer);
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : updateThermalLevel
 *
 * DESCRIPTION: update thermal level depending on thermal events
 *
 * PARAMETERS :
 *   @level   : thermal level
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::updateThermalLevel(
            qcamera_thermal_level_enum_t level)
{
    int ret = NO_ERROR;
    cam_fps_range_t adjustedRange;
    int minFPS, maxFPS;
    qcamera_thermal_mode thermalMode = mParameters.getThermalMode();
    enum msm_vfe_frame_skip_pattern skipPattern;

    mParameters.getPreviewFpsRange(&minFPS, &maxFPS);

    switch(level) {
    case QCAMERA_THERMAL_NO_ADJUSTMENT:
        {
            adjustedRange.min_fps = minFPS / 1000.0f;
            adjustedRange.max_fps = maxFPS / 1000.0f;
            skipPattern = NO_SKIP;
        }
        break;
    case QCAMERA_THERMAL_SLIGHT_ADJUSTMENT:
        {
            adjustedRange.min_fps = (minFPS / 2) / 1000.0f;
            adjustedRange.max_fps = (maxFPS / 2) / 1000.0f;
            if ( adjustedRange.min_fps < 1 ) {
                adjustedRange.min_fps = 1;
            }
            if ( adjustedRange.max_fps < 1 ) {
                adjustedRange.max_fps = 1;
            }
            skipPattern = EVERY_2FRAME;
        }
        break;
    case QCAMERA_THERMAL_BIG_ADJUSTMENT:
        {
            adjustedRange.min_fps = (minFPS / 4) / 1000.0f;
            adjustedRange.max_fps = (maxFPS / 4) / 1000.0f;
            if ( adjustedRange.min_fps < 1 ) {
                adjustedRange.min_fps = 1;
            }
            if ( adjustedRange.max_fps < 1 ) {
                adjustedRange.max_fps = 1;
            }
            skipPattern = EVERY_4FRAME;
        }
        break;
    case QCAMERA_THERMAL_SHUTDOWN:
        {
            // Stop Preview?
            // Set lowest min FPS for now
            adjustedRange.min_fps = minFPS/1000.0f;
            adjustedRange.max_fps = minFPS/1000.0f;
            for ( int i = 0 ; i < gCamCapability[mCameraId]->fps_ranges_tbl_cnt ; i++ ) {
                if ( gCamCapability[mCameraId]->fps_ranges_tbl[i].min_fps < adjustedRange.min_fps ) {
                    adjustedRange.min_fps = gCamCapability[mCameraId]->fps_ranges_tbl[i].min_fps;
                    adjustedRange.max_fps = adjustedRange.min_fps;
                }
            }
            skipPattern = MAX_SKIP;
        }
        break;
    default:
        {
            ALOGE("%s: Invalid thermal level %d", __func__, level);
            return BAD_VALUE;
        }
        break;
    }

    ALOGI("%s: Thermal level %d, FPS range [%3.2f,%3.2f], frameskip %d",
          __func__,
          level,
          adjustedRange.min_fps,
          adjustedRange.max_fps,
          skipPattern);

    if (thermalMode == QCAMERA_THERMAL_ADJUST_FPS)
        ret = mParameters.adjustPreviewFpsRange(&adjustedRange);
    else if (thermalMode == QCAMERA_THERMAL_ADJUST_FRAMESKIP)
        ret = mParameters.setFrameSkip(skipPattern);
    else
        ALOGE("%s: Incorrect thermal mode %d", __func__, thermalMode);

    return ret;

}

/*===========================================================================
 * FUNCTION   : updateParameters
 *
 * DESCRIPTION: update parameters
 *
 * PARAMETERS :
 *   @parms       : input parameters string
 *   @needRestart : output, flag to indicate if preview restart is needed
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::updateParameters(const char *parms, bool &needRestart)
{
    String8 str = String8(parms);
    QCameraParameters param(str);
    return mParameters.updateParameters(param, needRestart);
}

/*===========================================================================
 * FUNCTION   : commitParameterChanges
 *
 * DESCRIPTION: commit parameter changes to the backend to take effect
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 * NOTE       : This function must be called after updateParameters.
 *              Otherwise, no change will be passed to backend to take effect.
 *==========================================================================*/
int QCamera2HardwareInterface::commitParameterChanges()
{
    int rc = mParameters.commitParameters();
    if (rc == NO_ERROR) {
        // update number of snapshot based on committed parameters setting
        rc = mParameters.setNumOfSnapshot();
    }
    return rc;
}

/*===========================================================================
 * FUNCTION   : needDebugFps
 *
 * DESCRIPTION: if fps log info need to be printed out
 *
 * PARAMETERS : none
 *
 * RETURN     : true: need print out fps log
 *              false: no need to print out fps log
 *==========================================================================*/
bool QCamera2HardwareInterface::needDebugFps()
{
    return mParameters.isFpsDebugEnabled();
}

/*===========================================================================
 * FUNCTION   : isCACEnabled
 *
 * DESCRIPTION: if CAC is enabled
 *
 * PARAMETERS : none
 *
 * RETURN     : true: needed
 *              false: no need
 *==========================================================================*/
bool QCamera2HardwareInterface::isCACEnabled()
{
    char prop[PROPERTY_VALUE_MAX];
    memset(prop, 0, sizeof(prop));
    property_get("persist.camera.feature.cac", prop, "0");
    int enableCAC = atoi(prop);
    return enableCAC == 1;
}

/*===========================================================================
 * FUNCTION   : needReprocess
 *
 * DESCRIPTION: if reprocess is needed
 *
 * PARAMETERS : none
 *
 * RETURN     : true: needed
 *              false: no need
 *==========================================================================*/
bool QCamera2HardwareInterface::needReprocess()
{
    if (!mParameters.isJpegPictureFormat()) {
        // RAW image, no need to reprocess
        return false;
    }

    if (((gCamCapability[mCameraId]->min_required_pp_mask > 0) ||
         mParameters.isWNREnabled() || isCACEnabled())) {
        // TODO: add for ZSL HDR later
        ALOGD("%s: need do reprocess for ZSL WNR or min PP reprocess", __func__);
        return true;
    }

    return needRotationReprocess();
}

/*===========================================================================
 * FUNCTION   : needRotationReprocess
 *
 * DESCRIPTION: if rotation needs to be done by reprocess in pp
 *
 * PARAMETERS : none
 *
 * RETURN     : true: needed
 *              false: no need
 *==========================================================================*/
bool QCamera2HardwareInterface::needRotationReprocess()
{
    if (!mParameters.isJpegPictureFormat()) {
        // RAW image, no need to reprocess
        return false;
    }

    if ((gCamCapability[mCameraId]->qcom_supported_feature_mask & CAM_QCOM_FEATURE_ROTATION) > 0 &&
        mParameters.getJpegRotation() > 0) {
        // current rotation is not zero, and pp has the capability to process rotation
        ALOGD("%s: need do reprocess for rotation", __func__);
        return true;
    }

    return false;
}

/*===========================================================================
 * FUNCTION   : getThumbnailSize
 *
 * DESCRIPTION: get user set thumbnail size
 *
 * PARAMETERS :
 *   @dim     : output of thumbnail dimension
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera2HardwareInterface::getThumbnailSize(cam_dimension_t &dim)
{
    mParameters.getThumbnailSize(&dim.width, &dim.height);
}

/*===========================================================================
 * FUNCTION   : getJpegQuality
 *
 * DESCRIPTION: get user set jpeg quality
 *
 * PARAMETERS : none
 *
 * RETURN     : jpeg quality setting
 *==========================================================================*/
int QCamera2HardwareInterface::getJpegQuality()
{
    return mParameters.getJpegQuality();
}

/*===========================================================================
 * FUNCTION   : getJpegRotation
 *
 * DESCRIPTION: get rotation information to be passed into jpeg encoding
 *
 * PARAMETERS : none
 *
 * RETURN     : rotation information
 *==========================================================================*/
int QCamera2HardwareInterface::getJpegRotation() {
    return mParameters.getJpegRotation();
}

/*===========================================================================
 * FUNCTION   : getExifData
 *
 * DESCRIPTION: get exif data to be passed into jpeg encoding
 *
 * PARAMETERS : none
 *
 * RETURN     : exif data from user setting and GPS
 *==========================================================================*/
QCameraExif *QCamera2HardwareInterface::getExifData()
{
    QCameraExif *exif = new QCameraExif();
    if (exif == NULL) {
        ALOGE("%s: No memory for QCameraExif", __func__);
        return NULL;
    }

    int32_t rc = NO_ERROR;
    uint32_t count = 0;

    // add exif entries
    char dateTime[20];
    memset(dateTime, 0, sizeof(dateTime));
    count = 20;
    rc = mParameters.getExifDateTime(dateTime, count);
    if(rc == NO_ERROR) {
        exif->addEntry(EXIFTAGID_EXIF_DATE_TIME_ORIGINAL,
                       EXIF_ASCII,
                       count,
                       (void *)dateTime);
    } else {
        ALOGE("%s: getExifDateTime failed", __func__);
    }

    rat_t focalLength;
    rc = mParameters.getExifFocalLength(&focalLength);
    if (rc == NO_ERROR) {
        exif->addEntry(EXIFTAGID_FOCAL_LENGTH,
                       EXIF_RATIONAL,
                       1,
                       (void *)&(focalLength));
    } else {
        ALOGE("%s: getExifFocalLength failed", __func__);
    }

    uint16_t isoSpeed = mParameters.getExifIsoSpeed();
    exif->addEntry(EXIFTAGID_ISO_SPEED_RATING,
                   EXIF_SHORT,
                   1,
                   (void *)&(isoSpeed));

    char gpsProcessingMethod[EXIF_ASCII_PREFIX_SIZE + GPS_PROCESSING_METHOD_SIZE];
    count = 0;
    rc = mParameters.getExifGpsProcessingMethod(gpsProcessingMethod, count);
    if(rc == NO_ERROR) {
        exif->addEntry(EXIFTAGID_GPS_PROCESSINGMETHOD,
                       EXIF_ASCII,
                       count,
                       (void *)gpsProcessingMethod);
    } else {
        ALOGE("%s: getExifGpsProcessingMethod failed", __func__);
    }

    rat_t latitude[3];
    char latRef[2];
    rc = mParameters.getExifLatitude(latitude, latRef);
    if(rc == NO_ERROR) {
        exif->addEntry(EXIFTAGID_GPS_LATITUDE,
                       EXIF_RATIONAL,
                       3,
                       (void *)latitude);
        exif->addEntry(EXIFTAGID_GPS_LATITUDE_REF,
                       EXIF_ASCII,
                       2,
                       (void *)latRef);
    } else {
        ALOGE("%s: getExifLatitude failed", __func__);
    }

    rat_t longitude[3];
    char lonRef[2];
    rc = mParameters.getExifLongitude(longitude, lonRef);
    if(rc == NO_ERROR) {
        exif->addEntry(EXIFTAGID_GPS_LONGITUDE,
                       EXIF_RATIONAL,
                       3,
                       (void *)longitude);

        exif->addEntry(EXIFTAGID_GPS_LONGITUDE_REF,
                       EXIF_ASCII,
                       2,
                       (void *)lonRef);
    } else {
        ALOGE("%s: getExifLongitude failed", __func__);
    }

    rat_t altitude;
    char altRef;
    rc = mParameters.getExifAltitude(&altitude, &altRef);
    if(rc == NO_ERROR) {
        exif->addEntry(EXIFTAGID_GPS_ALTITUDE,
                       EXIF_RATIONAL,
                       1,
                       (void *)&(altitude));

        exif->addEntry(EXIFTAGID_GPS_ALTITUDE_REF,
                       EXIF_BYTE,
                       1,
                       (void *)&altRef);
    } else {
        ALOGE("%s: getExifAltitude failed", __func__);
    }

    char gpsDateStamp[20];
    rat_t gpsTimeStamp[3];
    rc = mParameters.getExifGpsDateTimeStamp(gpsDateStamp, 20, gpsTimeStamp);
    if(rc == NO_ERROR) {
        exif->addEntry(EXIFTAGID_GPS_DATESTAMP,
                       EXIF_ASCII,
                       strlen(gpsDateStamp) + 1,
                       (void *)gpsDateStamp);

        exif->addEntry(EXIFTAGID_GPS_TIMESTAMP,
                       EXIF_RATIONAL,
                       3,
                       (void *)gpsTimeStamp);
    } else {
        ALOGE("%s: getExifGpsDataTimeStamp failed", __func__);
    }

    return exif;
}

/*===========================================================================
 * FUNCTION   : setHistogram
 *
 * DESCRIPTION: set if histogram should be enabled
 *
 * PARAMETERS :
 *   @histogram_en : bool flag if histogram should be enabled
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::setHistogram(bool histogram_en)
{
    return mParameters.setHistogram(histogram_en);
}

/*===========================================================================
 * FUNCTION   : setFaceDetection
 *
 * DESCRIPTION: set if face detection should be enabled
 *
 * PARAMETERS :
 *   @enabled : bool flag if face detection should be enabled
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::setFaceDetection(bool enabled)
{
    return mParameters.setFaceDetection(enabled);
}

/*===========================================================================
 * FUNCTION   : prepareHardwareForSnapshot
 *
 * DESCRIPTION: prepare hardware for snapshot, such as LED
 *
 * PARAMETERS :
 *   @afNeeded: flag indicating if Auto Focus needs to be done during preparation
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::prepareHardwareForSnapshot(int32_t afNeeded)
{
    ALOGD("[KPI Perf] %s: Prepare hardware such as LED",__func__);
    return mCameraHandle->ops->prepare_snapshot(mCameraHandle->camera_handle,
                                                afNeeded);
}

}; // namespace qcamera
