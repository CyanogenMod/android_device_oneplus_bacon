/* Copyright (c) 2012-2014, The Linux Foundataion. All rights reserved.
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
#define ATRACE_TAG ATRACE_TAG_CAMERA

#include <utils/Log.h>
#include <cutils/properties.h>
#include <hardware/camera.h>
#include <stdlib.h>
#include <utils/Errors.h>
#include <utils/Trace.h>
#include <gralloc_priv.h>
#include <gui/Surface.h>

#include "QCamera2HWI.h"
#include "QCameraMem.h"

#define MAP_TO_DRIVER_COORDINATE(val, base, scale, offset) \
  ((int32_t)val * (int32_t)scale / (int32_t)base + (int32_t)offset)
#define CAMERA_MIN_STREAMING_BUFFERS     3
#define EXTRA_ZSL_PREVIEW_STREAM_BUF     2
#define CAMERA_MIN_JPEG_ENCODING_BUFFERS 2
#define CAMERA_MIN_VIDEO_BUFFERS         9
#define CAMERA_LONGSHOT_STAGES           4

#define HDR_CONFIDENCE_THRESHOLD 0.4

namespace qcamera {

cam_capability_t *gCamCapability[MM_CAMERA_MAX_NUM_SENSORS];
qcamera_saved_sizes_list savedSizes[MM_CAMERA_MAX_NUM_SENSORS];

static pthread_mutex_t g_camlock = PTHREAD_MUTEX_INITIALIZER;
volatile uint32_t gCamHalLogLevel = 0;

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
    ATRACE_CALL();
    int rc = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("%s: NULL camera device", __func__);
        return BAD_VALUE;
    }

    hw->lockAPI();
    qcamera_api_result_t apiResult;
    rc = hw->processAPI(QCAMERA_SM_EVT_SET_PREVIEW_WINDOW, (void *)window);
    if (rc == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_SET_PREVIEW_WINDOW, &apiResult);
        rc = apiResult.status;
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
    ATRACE_CALL();
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
    qcamera_api_result_t apiResult;
    int32_t rc = hw->processAPI(QCAMERA_SM_EVT_SET_CALLBACKS, (void *)&payload);
    if (rc == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_SET_CALLBACKS, &apiResult);
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
    ATRACE_CALL();
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return;
    }
    hw->lockAPI();
    qcamera_api_result_t apiResult;
    int32_t rc = hw->processAPI(QCAMERA_SM_EVT_ENABLE_MSG_TYPE, (void *)&msg_type);
    if (rc == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_ENABLE_MSG_TYPE, &apiResult);
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
    ATRACE_CALL();
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return;
    }
    hw->lockAPI();
    qcamera_api_result_t apiResult;
    int32_t rc = hw->processAPI(QCAMERA_SM_EVT_DISABLE_MSG_TYPE, (void *)&msg_type);
    if (rc == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_DISABLE_MSG_TYPE, &apiResult);
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
    ATRACE_CALL();
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    hw->lockAPI();
    qcamera_api_result_t apiResult;
    ret = hw->processAPI(QCAMERA_SM_EVT_MSG_TYPE_ENABLED, (void *)&msg_type);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_MSG_TYPE_ENABLED, &apiResult);
        ret = apiResult.enabled;
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
    ATRACE_CALL();
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    CDBG_HIGH("[KPI Perf] %s: E PROFILE_START_PREVIEW", __func__);
    hw->lockAPI();
    qcamera_api_result_t apiResult;
    qcamera_sm_evt_enum_t evt = QCAMERA_SM_EVT_START_PREVIEW;
    if (hw->isNoDisplayMode()) {
        evt = QCAMERA_SM_EVT_START_NODISPLAY_PREVIEW;
    }
    ret = hw->processAPI(evt, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(evt, &apiResult);
        ret = apiResult.status;
    }
    hw->unlockAPI();
    hw->m_bPreviewStarted = true;
    CDBG_HIGH("[KPI Perf] %s: X", __func__);
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
    ATRACE_CALL();
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return;
    }
    CDBG_HIGH("[KPI Perf] %s: E PROFILE_STOP_PREVIEW", __func__);
    hw->lockAPI();
    qcamera_api_result_t apiResult;
    int32_t ret = hw->processAPI(QCAMERA_SM_EVT_STOP_PREVIEW, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_STOP_PREVIEW, &apiResult);
    }
    hw->unlockAPI();
    CDBG_HIGH("[KPI Perf] %s: X", __func__);
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
    ATRACE_CALL();
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }

    hw->lockAPI();
    qcamera_api_result_t apiResult;
    ret = hw->processAPI(QCAMERA_SM_EVT_PREVIEW_ENABLED, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_PREVIEW_ENABLED, &apiResult);
        ret = apiResult.enabled;
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
    qcamera_api_result_t apiResult;
    ret = hw->processAPI(QCAMERA_SM_EVT_STORE_METADATA_IN_BUFS, (void *)&enable);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_STORE_METADATA_IN_BUFS, &apiResult);
        ret = apiResult.status;
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
    ATRACE_CALL();
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    CDBG_HIGH("[KPI Perf] %s: E PROFILE_START_RECORDING", __func__);
    hw->lockAPI();
    qcamera_api_result_t apiResult;
    ret = hw->processAPI(QCAMERA_SM_EVT_START_RECORDING, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_START_RECORDING, &apiResult);
        ret = apiResult.status;
    }
    hw->unlockAPI();
    hw->m_bRecordStarted = true;
    CDBG_HIGH("[KPI Perf] %s: X", __func__);
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
    ATRACE_CALL();
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return;
    }
    CDBG_HIGH("[KPI Perf] %s: E PROFILE_STOP_RECORDING", __func__);
    hw->lockAPI();
    qcamera_api_result_t apiResult;
    int32_t ret = hw->processAPI(QCAMERA_SM_EVT_STOP_RECORDING, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_STOP_RECORDING, &apiResult);
    }
    hw->unlockAPI();
    CDBG_HIGH("[KPI Perf] %s: X", __func__);
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
    ATRACE_CALL();
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    hw->lockAPI();
    qcamera_api_result_t apiResult;
    ret = hw->processAPI(QCAMERA_SM_EVT_RECORDING_ENABLED, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_RECORDING_ENABLED, &apiResult);
        ret = apiResult.enabled;
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
    ATRACE_CALL();
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return;
    }
    if (opaque == NULL) {
        ALOGE("%s: Error!! Frame info is NULL", __func__);
        return;
    }
    CDBG_HIGH("%s: E", __func__);
    hw->lockAPI();
    qcamera_api_result_t apiResult;
    int32_t ret = hw->processAPI(QCAMERA_SM_EVT_RELEASE_RECORIDNG_FRAME, (void *)opaque);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_RELEASE_RECORIDNG_FRAME, &apiResult);
    }
    hw->unlockAPI();
    CDBG_HIGH("%s: X", __func__);
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
    ATRACE_INT("Camera:AutoFocus", 1);
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    CDBG_HIGH("[KPI Perf] %s : E PROFILE_AUTO_FOCUS", __func__);
    hw->lockAPI();
    qcamera_api_result_t apiResult;
    ret = hw->processAPI(QCAMERA_SM_EVT_START_AUTO_FOCUS, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_START_AUTO_FOCUS, &apiResult);
        ret = apiResult.status;
    }
    hw->unlockAPI();
    CDBG_HIGH("[KPI Perf] %s : X", __func__);

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
    ATRACE_CALL();
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    CDBG_HIGH("[KPI Perf] %s : E PROFILE_CANCEL_AUTO_FOCUS", __func__);
    hw->lockAPI();
    qcamera_api_result_t apiResult;
    ret = hw->processAPI(QCAMERA_SM_EVT_STOP_AUTO_FOCUS, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_STOP_AUTO_FOCUS, &apiResult);
        ret = apiResult.status;
    }
    hw->unlockAPI();
    CDBG_HIGH("[KPI Perf] %s : X", __func__);
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
    ATRACE_CALL();
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    CDBG_HIGH("[KPI Perf] %s: E PROFILE_TAKE_PICTURE", __func__);
    hw->lockAPI();
    qcamera_api_result_t apiResult;

    /* Prepare snapshot in case LED needs to be flashed */
    if (hw->mFlashNeeded == true || hw->mParameters.isChromaFlashEnabled()) {
        ret = hw->processAPI(QCAMERA_SM_EVT_PREPARE_SNAPSHOT, NULL);
        if (ret == NO_ERROR) {
            hw->waitAPIResult(QCAMERA_SM_EVT_PREPARE_SNAPSHOT, &apiResult);
            ret = apiResult.status;
        }
        hw->mPrepSnapRun = true;
    }

    /* Regardless what the result value for prepare_snapshot,
     * go ahead with capture anyway. Just like the way autofocus
     * is handled in capture case. */

    /* capture */
    ret = hw->processAPI(QCAMERA_SM_EVT_TAKE_PICTURE, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_TAKE_PICTURE, &apiResult);
        ret = apiResult.status;
    }

    hw->unlockAPI();
    CDBG_HIGH("[KPI Perf] %s: X", __func__);
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
    ATRACE_CALL();
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    CDBG_HIGH("[KPI Perf] %s: E PROFILE_CANCEL_PICTURE", __func__);
    hw->lockAPI();
    qcamera_api_result_t apiResult;
    ret = hw->processAPI(QCAMERA_SM_EVT_CANCEL_PICTURE, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_CANCEL_PICTURE, &apiResult);
        ret = apiResult.status;
    }
    hw->unlockAPI();
    CDBG_HIGH("[KPI Perf] %s: X", __func__);
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
    ATRACE_CALL();
    int ret = NO_ERROR;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    hw->lockAPI();
    qcamera_api_result_t apiResult;
    ret = hw->processAPI(QCAMERA_SM_EVT_SET_PARAMS, (void *)parms);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_SET_PARAMS, &apiResult);
        ret = apiResult.status;
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
    ATRACE_CALL();
    char *ret = NULL;
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return NULL;
    }
    hw->lockAPI();
    qcamera_api_result_t apiResult;
    int32_t rc = hw->processAPI(QCAMERA_SM_EVT_GET_PARAMS, NULL);
    if (rc == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_GET_PARAMS, &apiResult);
        ret = apiResult.params;
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
    ATRACE_CALL();
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return;
    }
    hw->lockAPI();
    qcamera_api_result_t apiResult;
    int32_t ret = hw->processAPI(QCAMERA_SM_EVT_PUT_PARAMS, (void *)parm);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_PUT_PARAMS, &apiResult);
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
    ATRACE_CALL();
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
    qcamera_api_result_t apiResult;
    ret = hw->processAPI(QCAMERA_SM_EVT_SEND_COMMAND, (void *)&payload);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_SEND_COMMAND, &apiResult);
        ret = apiResult.status;
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
    ATRACE_CALL();
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return;
    }
    hw->lockAPI();
    qcamera_api_result_t apiResult;
    int32_t ret = hw->processAPI(QCAMERA_SM_EVT_RELEASE, NULL);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_RELEASE, &apiResult);
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

    //Log level property is read when "adb shell dumpsys media.camera" is
    //called so that the log level can be controlled without restarting
    //media server
    getLogLevel();
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(device->priv);
    if (!hw) {
        ALOGE("NULL camera device");
        return BAD_VALUE;
    }
    hw->lockAPI();
    qcamera_api_result_t apiResult;
    ret = hw->processAPI(QCAMERA_SM_EVT_DUMP, (void *)&fd);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_DUMP, &apiResult);
        ret = apiResult.status;
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
    ATRACE_CALL();
    int ret = NO_ERROR;
    CDBG_HIGH("[KPI Perf] %s: E",__func__);
    QCamera2HardwareInterface *hw =
        reinterpret_cast<QCamera2HardwareInterface *>(
            reinterpret_cast<camera_device_t *>(hw_dev)->priv);
    if (!hw) {
        ALOGE("%s: NULL camera device", __func__);
        return BAD_VALUE;
    }
    delete hw;
    CDBG_HIGH("[KPI Perf] %s: X",__func__);
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
    ATRACE_CALL();
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
    qcamera_api_result_t apiResult;
    ret = hw->processAPI(QCAMERA_SM_EVT_REG_FACE_IMAGE, (void *)&payload);
    if (ret == NO_ERROR) {
        hw->waitAPIResult(QCAMERA_SM_EVT_REG_FACE_IMAGE, &apiResult);
        ret = apiResult.handle;
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
QCamera2HardwareInterface::QCamera2HardwareInterface(uint32_t cameraId)
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
      m_bPreviewStarted(false),
      m_bRecordStarted(false),
      m_currentFocusState(CAM_AF_SCANNING),
      m_pPowerModule(NULL),
      mDumpFrmCnt(0U),
      mDumpSkipCnt(0U),
      mThermalLevel(QCAMERA_THERMAL_NO_ADJUSTMENT),
      mCancelAutoFocus(false),
      mActiveAF(false),
      m_HDRSceneEnabled(false),
      mLongshotEnabled(false),
      m_max_pic_width(0),
      m_max_pic_height(0),
      mLiveSnapshotThread(0),
      mIntPicThread(0),
      mFlashNeeded(false),
      mCaptureRotation(0U),
      mIs3ALocked(false),
      mPrepSnapRun(false),
      mZoomLevel(0),
      m_bIntEvtPending(false),
      mSnapshotJob(-1),
      mPostviewJob(-1),
      mMetadataJob(-1),
      mReprocJob(-1),
      mRawdataJob(-1),
      mPreviewFrameSkipValid(0),
      mAdvancedCaptureConfigured(false),
      mNumPreviewFaces(-1)
{
    getLogLevel();
    ATRACE_CALL();
    mCameraDevice.common.tag = HARDWARE_DEVICE_TAG;
    mCameraDevice.common.version = HARDWARE_DEVICE_API_VERSION(1, 0);
    mCameraDevice.common.close = close_camera_device;
    mCameraDevice.ops = &mCameraOps;
    mCameraDevice.priv = this;

    pthread_mutex_init(&m_lock, NULL);
    pthread_cond_init(&m_cond, NULL);

    m_apiResultList = NULL;

    pthread_mutex_init(&m_evtLock, NULL);
    pthread_cond_init(&m_evtCond, NULL);
    memset(&m_evtResult, 0, sizeof(qcamera_api_result_t));

    pthread_mutex_init(&m_parm_lock, NULL);

    pthread_mutex_init(&m_int_lock, NULL);
    pthread_cond_init(&m_int_cond, NULL);

    memset(m_channels, 0, sizeof(m_channels));
    memset(&mExifParams, 0, sizeof(mm_jpeg_exif_params_t));

#ifdef HAS_MULTIMEDIA_HINTS
    if (hw_get_module(POWER_HARDWARE_MODULE_ID, (const hw_module_t **)&m_pPowerModule)) {
        ALOGE("%s: %s module not found", __func__, POWER_HARDWARE_MODULE_ID);
    }
#endif

    memset(mDeffOngoingJobs, 0, sizeof(mDeffOngoingJobs));

    //reset preview frame skip
    memset(&mPreviewFrameSkipIdxRange, 0, sizeof(cam_frame_idx_range_t));

    mDefferedWorkThread.launch(defferedWorkRoutine, this);
    mDefferedWorkThread.sendCmd(CAMERA_CMD_TYPE_START_DATA_PROC, FALSE, FALSE);
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
    CDBG_HIGH("%s: E", __func__);
    mDefferedWorkThread.sendCmd(CAMERA_CMD_TYPE_STOP_DATA_PROC, TRUE, TRUE);
    mDefferedWorkThread.exit();

    closeCamera();
    pthread_mutex_destroy(&m_lock);
    pthread_cond_destroy(&m_cond);
    pthread_mutex_destroy(&m_evtLock);
    pthread_cond_destroy(&m_evtCond);
    pthread_mutex_destroy(&m_parm_lock);
    pthread_mutex_destroy(&m_int_lock);
    pthread_cond_destroy(&m_int_cond);
    CDBG_HIGH("%s: X", __func__);
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
    ATRACE_CALL();
    int rc = NO_ERROR;
    if (mCameraOpened) {
        *hw_device = NULL;
        return PERMISSION_DENIED;
    }
    CDBG_HIGH("[KPI Perf] %s: E PROFILE_OPEN_CAMERA camera id %d", __func__,mCameraId);
    rc = openCamera();
    if (rc == NO_ERROR){
        *hw_device = &mCameraDevice.common;
        if (m_thermalAdapter.init(this) != 0) {
          ALOGE("Init thermal adapter failed");
        }
    }
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
    int32_t l_curr_width = 0;
    int32_t l_curr_height = 0;
    m_max_pic_width = 0;
    m_max_pic_height = 0;
    char value[PROPERTY_VALUE_MAX];
    int enable_4k2k;
    size_t i;

    if (mCameraHandle) {
        ALOGE("Failure: Camera already opened");
        return ALREADY_EXISTS;
    }
    mCameraHandle = camera_open((uint8_t)mCameraId);
    if (!mCameraHandle) {
        ALOGE("camera_open failed.");
        return UNKNOWN_ERROR;
    }
    if (NULL == gCamCapability[mCameraId])
        initCapabilities(mCameraId,mCameraHandle);

    mCameraHandle->ops->register_event_notify(mCameraHandle->camera_handle,
                                              camEvtHandle,
                                              (void *) this);

    /* get max pic size for jpeg work buf calculation*/
    for(i = 0; i < gCamCapability[mCameraId]->picture_sizes_tbl_cnt - 1; i++)
    {
      l_curr_width = gCamCapability[mCameraId]->picture_sizes_tbl[i].width;
      l_curr_height = gCamCapability[mCameraId]->picture_sizes_tbl[i].height;

      if ((l_curr_width * l_curr_height) >
        (m_max_pic_width * m_max_pic_height)) {
        m_max_pic_width = l_curr_width;
        m_max_pic_height = l_curr_height;
      }
    }
    //reset the preview and video sizes tables in case they were changed earlier
    copyList(savedSizes[mCameraId].all_preview_sizes, gCamCapability[mCameraId]->preview_sizes_tbl,
             savedSizes[mCameraId].all_preview_sizes_cnt);
    gCamCapability[mCameraId]->preview_sizes_tbl_cnt = savedSizes[mCameraId].all_preview_sizes_cnt;
    copyList(savedSizes[mCameraId].all_video_sizes, gCamCapability[mCameraId]->video_sizes_tbl,
             savedSizes[mCameraId].all_video_sizes_cnt);
    gCamCapability[mCameraId]->video_sizes_tbl_cnt = savedSizes[mCameraId].all_video_sizes_cnt;

    //check if video size 4k x 2k support is enabled
    property_get("persist.camera.4k2k.enable", value, "0");
    enable_4k2k = atoi(value) > 0 ? 1 : 0;
    ALOGD("%s: enable_4k2k is %d", __func__, enable_4k2k);
    if (!enable_4k2k) {
       //if the 4kx2k size exists in the supported preview size or
       //supported video size remove it
       bool found;
       cam_dimension_t true_size_4k_2k;
       cam_dimension_t size_4k_2k;
       true_size_4k_2k.width = 4096;
       true_size_4k_2k.height = 2160;
       size_4k_2k.width = 3840;
       size_4k_2k.height = 2160;

       found = removeSizeFromList(gCamCapability[mCameraId]->preview_sizes_tbl,
                                  gCamCapability[mCameraId]->preview_sizes_tbl_cnt,
                                  true_size_4k_2k);
       if (found) {
          gCamCapability[mCameraId]->preview_sizes_tbl_cnt--;
       }

       found = removeSizeFromList(gCamCapability[mCameraId]->preview_sizes_tbl,
                                  gCamCapability[mCameraId]->preview_sizes_tbl_cnt,
                                  size_4k_2k);
       if (found) {
          gCamCapability[mCameraId]->preview_sizes_tbl_cnt--;
       }


       found = removeSizeFromList(gCamCapability[mCameraId]->video_sizes_tbl,
                                  gCamCapability[mCameraId]->video_sizes_tbl_cnt,
                                  true_size_4k_2k);
       if (found) {
          gCamCapability[mCameraId]->video_sizes_tbl_cnt--;
       }

       found = removeSizeFromList(gCamCapability[mCameraId]->video_sizes_tbl,
                                  gCamCapability[mCameraId]->video_sizes_tbl_cnt,
                                  size_4k_2k);
       if (found) {
          gCamCapability[mCameraId]->video_sizes_tbl_cnt--;
       }
    }

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

    mParameters.init(gCamCapability[mCameraId], mCameraHandle, this, this);

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
    CDBG_HIGH("%s: E", __func__);
    if (!mCameraOpened) {
        return NO_ERROR;
    }

    pthread_mutex_lock(&m_parm_lock);

    // set open flag to false
    mCameraOpened = false;

    // deinit Parameters
    mParameters.deinit();

    pthread_mutex_unlock(&m_parm_lock);

    // exit notifier
    m_cbNotifier.exit();

    // stop and deinit postprocessor
    m_postprocessor.stop();
    m_postprocessor.deinit();

    //free all pending api results here
    if(m_apiResultList != NULL) {
        api_result_list *apiResultList = m_apiResultList;
        api_result_list *apiResultListNext;
        while (apiResultList != NULL) {
            apiResultListNext = apiResultList->next;
            free(apiResultList);
            apiResultList = apiResultListNext;
        }
    }

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
    CDBG_HIGH("%s: X", __func__);
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
int QCamera2HardwareInterface::initCapabilities(uint32_t cameraId,
        mm_camera_vtbl_t *cameraHandle)
{
    ATRACE_CALL();
    int rc = NO_ERROR;
    QCameraHeapMemory *capabilityHeap = NULL;

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

    //copy the preview sizes and video sizes lists because they
    //might be changed later
    copyList(gCamCapability[cameraId]->preview_sizes_tbl, savedSizes[cameraId].all_preview_sizes,
             gCamCapability[cameraId]->preview_sizes_tbl_cnt);
    savedSizes[cameraId].all_preview_sizes_cnt = gCamCapability[cameraId]->preview_sizes_tbl_cnt;
    copyList(gCamCapability[cameraId]->video_sizes_tbl, savedSizes[cameraId].all_video_sizes,
             gCamCapability[cameraId]->video_sizes_tbl_cnt);
    savedSizes[cameraId].all_video_sizes_cnt = gCamCapability[cameraId]->video_sizes_tbl_cnt;

    rc = NO_ERROR;

query_failed:
    cameraHandle->ops->unmap_buf(cameraHandle->camera_handle,
                            CAM_MAPPING_BUF_TYPE_CAPABILITY);
map_failed:
    capabilityHeap->deallocate();
    delete capabilityHeap;
allocate_failed:
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
 * RETURN     : int type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::getCapabilities(uint32_t cameraId,
        struct camera_info *info)
{
    ATRACE_CALL();
    int rc = NO_ERROR;
    struct  camera_info *p_info;
    pthread_mutex_lock(&g_camlock);
    p_info = get_cam_info(cameraId);
    memcpy(info, p_info, sizeof (struct camera_info));
    pthread_mutex_unlock(&g_camlock);
    return rc;
}

/*===========================================================================
 * FUNCTION   : prepareTorchCamera
 *
 * DESCRIPTION: initializes the camera ( if needed )
 *              so torch can be configured.
 *
 * PARAMETERS :
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::prepareTorchCamera()
{
    int rc = NO_ERROR;

    if ( ( !m_stateMachine.isPreviewRunning() ) &&
            !m_stateMachine.isPreviewReady() &&
            ( m_channels[QCAMERA_CH_TYPE_PREVIEW] == NULL ) ) {
        rc = addChannel(QCAMERA_CH_TYPE_PREVIEW);
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : releaseTorchCamera
 *
 * DESCRIPTION: releases all previously acquired camera resources ( if any )
 *              needed for torch configuration.
 *
 * PARAMETERS :
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::releaseTorchCamera()
{
    if ( !m_stateMachine.isPreviewRunning() &&
            !m_stateMachine.isPreviewReady() &&
            ( m_channels[QCAMERA_CH_TYPE_PREVIEW] != NULL ) ) {
        delete m_channels[QCAMERA_CH_TYPE_PREVIEW];
        m_channels[QCAMERA_CH_TYPE_PREVIEW] = NULL;
    }

    return NO_ERROR;
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

    int minCircularBufNum = mParameters.getMaxUnmatchedFramesInQueue() +
                            CAMERA_MIN_JPEG_ENCODING_BUFFERS;

    int minUndequeCount = 0;
    int minPPBufs = mParameters.getMinPPBufs();
    int maxStreamBuf = zslQBuffers + minCircularBufNum +
        mParameters.getNumOfExtraHDRInBufsIfNeeded() -
        mParameters.getNumOfExtraHDROutBufsIfNeeded() +
        mParameters.getNumOfExtraBuffersForImageProc() +
        EXTRA_ZSL_PREVIEW_STREAM_BUF;

    if (!isNoDisplayMode()) {
        if(mPreviewWindow != NULL) {
            if (mPreviewWindow->get_min_undequeued_buffer_count(mPreviewWindow,&minUndequeCount)
                != 0) {
                ALOGE("get_min_undequeued_buffer_count  failed");
            }
        } else {
            //preview window might not be set at this point. So, query directly
            //from BufferQueue implementation of gralloc buffers.
#ifdef USE_KK_CODE
            minUndequeCount = BufferQueue::MIN_UNDEQUEUED_BUFFERS;
#else
            minUndequeCount = 2;
#endif
        }
    }

    // Get buffer count for the particular stream type
    switch (stream_type) {
    case CAM_STREAM_TYPE_PREVIEW:
        {
            if (mParameters.isZSLMode()) {
                /* We need to add two extra streming buffers to add
                  flexibility in forming matched super buf in ZSL queue.
                  with number being 'zslQBuffers + minCircularBufNum'
                  we see preview buffers sometimes get dropped at CPP
                  and super buf is not forming in ZSL Q for long time. */

                bufferCnt = zslQBuffers + minCircularBufNum +
                        mParameters.getNumOfExtraBuffersForImageProc() +
                        EXTRA_ZSL_PREVIEW_STREAM_BUF +
                        mParameters.getNumOfExtraBuffersForPreview();
            } else {
                bufferCnt = CAMERA_MIN_STREAMING_BUFFERS +
                        mParameters.getMaxUnmatchedFramesInQueue() +
                        mParameters.getNumOfExtraBuffersForPreview();
            }
            bufferCnt += minUndequeCount;
        }
        break;
    case CAM_STREAM_TYPE_POSTVIEW:
        {
            bufferCnt = minCaptureBuffers +
                        mParameters.getNumOfExtraHDRInBufsIfNeeded() -
                        mParameters.getNumOfExtraHDROutBufsIfNeeded() +
                        minPPBufs +
                        mParameters.getNumOfExtraBuffersForImageProc();

            if (bufferCnt > maxStreamBuf) {
                bufferCnt = maxStreamBuf;
            }
            bufferCnt += minUndequeCount;
        }
        break;
    case CAM_STREAM_TYPE_SNAPSHOT:
        {
            if (mParameters.isZSLMode() || mLongshotEnabled) {
                if (minCaptureBuffers == 1 && !mLongshotEnabled) {
                    // Single ZSL snapshot case
                    bufferCnt = zslQBuffers + CAMERA_MIN_STREAMING_BUFFERS +
                            mParameters.getNumOfExtraBuffersForImageProc();
                }
                else {
                    // ZSL Burst or Longshot case
                    bufferCnt = zslQBuffers + minCircularBufNum +
                            mParameters.getNumOfExtraBuffersForImageProc();
                }
            } else {
                bufferCnt = minCaptureBuffers +
                            mParameters.getNumOfExtraHDRInBufsIfNeeded() -
                            mParameters.getNumOfExtraHDROutBufsIfNeeded() +
                            mParameters.getNumOfExtraBuffersForImageProc();

                if (bufferCnt > maxStreamBuf) {
                    bufferCnt = maxStreamBuf;
                }
            }
        }
        break;
    case CAM_STREAM_TYPE_RAW:
        if (mParameters.isZSLMode()) {
            bufferCnt = zslQBuffers + minCircularBufNum;
        } else {
            bufferCnt = minCaptureBuffers +
                        mParameters.getNumOfExtraHDRInBufsIfNeeded() -
                        mParameters.getNumOfExtraHDROutBufsIfNeeded() +
                        mParameters.getNumOfExtraBuffersForImageProc();

            if (bufferCnt > maxStreamBuf) {
                bufferCnt = maxStreamBuf;
            }
        }
        break;
    case CAM_STREAM_TYPE_VIDEO:
        {
            bufferCnt = CAMERA_MIN_VIDEO_BUFFERS +
                    mParameters.getNumOfExtraBuffersForVideo();
        }
        break;
    case CAM_STREAM_TYPE_METADATA:
        {
            if (mParameters.isZSLMode()) {
                // MetaData buffers should be >= (Preview buffers-minUndequeCount)
                bufferCnt = zslQBuffers + minCircularBufNum +
                            mParameters.getNumOfExtraHDRInBufsIfNeeded() -
                            mParameters.getNumOfExtraHDROutBufsIfNeeded() +
                            mParameters.getNumOfExtraBuffersForImageProc() +
                            EXTRA_ZSL_PREVIEW_STREAM_BUF;
            } else {
                bufferCnt = minCaptureBuffers +
                            mParameters.getNumOfExtraHDRInBufsIfNeeded() -
                            mParameters.getNumOfExtraHDROutBufsIfNeeded() +
                            mParameters.getMaxUnmatchedFramesInQueue() +
                            CAMERA_MIN_STREAMING_BUFFERS +
                            mParameters.getNumOfExtraBuffersForImageProc();
            }
            if (bufferCnt > maxStreamBuf) {
                bufferCnt = maxStreamBuf;
            }
            bufferCnt += minUndequeCount;
        }
        break;
    case CAM_STREAM_TYPE_OFFLINE_PROC:
        {
            bufferCnt = minCaptureBuffers;
            if (mLongshotEnabled) {
                char prop[PROPERTY_VALUE_MAX];
                memset(prop, 0, sizeof(prop));
                property_get("persist.camera.longshot.stages", prop, "0");
                int longshotStages = atoi(prop);
                if (longshotStages > 0 && longshotStages < CAMERA_LONGSHOT_STAGES) {
                    bufferCnt = longshotStages;
                }
                else {
                    bufferCnt = CAMERA_LONGSHOT_STAGES;
                }
            }
            if (bufferCnt > maxStreamBuf) {
                bufferCnt = maxStreamBuf;
            }
        }
        break;
    case CAM_STREAM_TYPE_DEFAULT:
    case CAM_STREAM_TYPE_MAX:
    default:
        bufferCnt = 0;
        break;
    }

    ALOGD("%s: Allocating %d buffers for streamtype %d",__func__,bufferCnt,stream_type);
    return (uint8_t)bufferCnt;
}

/*===========================================================================
 * FUNCTION   : allocateStreamBuf
 *
 * DESCRIPTION: alocate stream buffers
 *
 * PARAMETERS :
 *   @stream_type  : type of stream
 *   @size         : size of buffer
 *   @stride       : stride of buffer
 *   @scanline     : scanline of buffer
 *   @bufferCnt    : [IN/OUT] minimum num of buffers to be allocated.
 *                   could be modified during allocation if more buffers needed
 *
 * RETURN     : ptr to a memory obj that holds stream buffers.
 *              NULL if failed
 *==========================================================================*/
QCameraMemory *QCamera2HardwareInterface::allocateStreamBuf(
        cam_stream_type_t stream_type, size_t size, int stride, int scanline,
        uint8_t &bufferCnt)
{
    int rc = NO_ERROR;
    QCameraMemory *mem = NULL;
    bool bCachedMem = QCAMERA_ION_USE_CACHE;
    bool bPoolMem = false;
    char value[PROPERTY_VALUE_MAX];
    property_get("persist.camera.mem.usepool", value, "1");
    if (atoi(value) == 1) {
        bPoolMem = true;
    }

    // Allocate stream buffer memory object
    switch (stream_type) {
    case CAM_STREAM_TYPE_PREVIEW:
        {
            if (isNoDisplayMode()) {
                mem = new QCameraStreamMemory(mGetMemory,
                        bCachedMem,
                        (bPoolMem) ? &m_memoryPool : NULL,
                        stream_type);
            } else {
                cam_dimension_t dim;
                QCameraGrallocMemory *grallocMemory =
                    new QCameraGrallocMemory(mGetMemory);

                mParameters.getStreamDimension(stream_type, dim);
                if (grallocMemory)
                    grallocMemory->setWindowInfo(mPreviewWindow, dim.width,
                        dim.height, stride, scanline,
                        mParameters.getPreviewHalPixelFormat());
                mem = grallocMemory;
            }
        }
        break;
    case CAM_STREAM_TYPE_POSTVIEW:
        {
            if (isPreviewRestartEnabled() || isNoDisplayMode()) {
                mem = new QCameraStreamMemory(mGetMemory, bCachedMem);
            } else {
                cam_dimension_t dim;
                QCameraGrallocMemory *grallocMemory =
                    new QCameraGrallocMemory(mGetMemory);

                mParameters.getStreamDimension(stream_type, dim);
                if (grallocMemory) {
                    grallocMemory->setWindowInfo(mPreviewWindow,
                        dim.width,
                        dim.height,
                        stride,
                        scanline,
                        mParameters.getPreviewHalPixelFormat());
                }
                mem = grallocMemory;
            }
        }
        break;
    case CAM_STREAM_TYPE_SNAPSHOT:
    case CAM_STREAM_TYPE_RAW:
    case CAM_STREAM_TYPE_METADATA:
    case CAM_STREAM_TYPE_OFFLINE_PROC:
        mem = new QCameraStreamMemory(mGetMemory,
                bCachedMem,
                (bPoolMem) ? &m_memoryPool : NULL,
                stream_type);
        break;
    case CAM_STREAM_TYPE_VIDEO:
        {
            //Use uncached allocation by default
            bCachedMem = QCAMERA_ION_USE_NOCACHE;
            char value[PROPERTY_VALUE_MAX];
            property_get("persist.camera.mem.usecache", value, "0");
            //LLV needs cached video buffers
            if ((atoi(value) == 1) || mParameters.isSeeMoreEnabled()) {
                bCachedMem = QCAMERA_ION_USE_CACHE;
            }
            ALOGD("%s: vidoe buf using cached memory = %d", __func__, bCachedMem);
            mem = new QCameraVideoMemory(mGetMemory, bCachedMem);
        }
        break;
    case CAM_STREAM_TYPE_DEFAULT:
    case CAM_STREAM_TYPE_MAX:
    default:
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
 * FUNCTION   : allocateMoreStreamBuf
 *
 * DESCRIPTION: alocate more stream buffers from the memory object
 *
 * PARAMETERS :
 *   @mem_obj      : memory object ptr
 *   @size         : size of buffer
 *   @bufferCnt    : [IN/OUT] additional number of buffers to be allocated.
 *                   output will be the number of total buffers
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::allocateMoreStreamBuf(
        QCameraMemory *mem_obj, size_t size, uint8_t &bufferCnt)
{
    int rc = NO_ERROR;

    if (bufferCnt > 0) {
        rc = mem_obj->allocateMore(bufferCnt, size);
        bufferCnt = mem_obj->getCnt();
    }
    return rc;
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
    rc = mParameters.getStreamRotation(stream_type, streamInfo->pp_config, streamInfo->dim);
    streamInfo->num_bufs = getBufNumRequired(stream_type);
    streamInfo->streaming_mode = CAM_STREAMING_MODE_CONTINUOUS;
    ALOGD("%s: stream_type %d, stream format %d,stream dimension %dx%d, num_bufs %d\n",
           __func__, stream_type, streamInfo->fmt, streamInfo->dim.width,
           streamInfo->dim.height, streamInfo->num_bufs);
    switch (stream_type) {
    case CAM_STREAM_TYPE_SNAPSHOT:
    case CAM_STREAM_TYPE_RAW:
        if ((mParameters.isZSLMode() && mParameters.getRecordingHintValue() != true) ||
                 mLongshotEnabled) {
            streamInfo->streaming_mode = CAM_STREAMING_MODE_CONTINUOUS;
        } else {
            streamInfo->streaming_mode = CAM_STREAMING_MODE_BURST;
            streamInfo->num_of_burst = (uint8_t)
                    (mParameters.getNumOfSnapshots()
                        + mParameters.getNumOfExtraHDRInBufsIfNeeded()
                        - mParameters.getNumOfExtraHDROutBufsIfNeeded()
                        + mParameters.getNumOfExtraBuffersForImageProc());
        }
        break;
    case CAM_STREAM_TYPE_POSTVIEW:
        if (mLongshotEnabled) {
            streamInfo->streaming_mode = CAM_STREAMING_MODE_CONTINUOUS;
        } else {
            streamInfo->streaming_mode = CAM_STREAMING_MODE_BURST;
            streamInfo->num_of_burst = (uint8_t)(mParameters.getNumOfSnapshots()
                + mParameters.getNumOfExtraHDRInBufsIfNeeded()
                - mParameters.getNumOfExtraHDROutBufsIfNeeded()
                + mParameters.getNumOfExtraBuffersForImageProc());
        }
        break;
    case CAM_STREAM_TYPE_VIDEO:
        streamInfo->useAVTimer = mParameters.isAVTimerEnabled();
        streamInfo->dis_enable = mParameters.isDISEnabled();
        if (mParameters.isSeeMoreEnabled()) {
            streamInfo->pp_config.feature_mask |= CAM_QCOM_FEATURE_LLVD;
        }

    case CAM_STREAM_TYPE_PREVIEW:
        if (mParameters.getRecordingHintValue()) {
            const char* dis_param = mParameters.get(QCameraParameters::KEY_QC_DIS);
            bool disEnabled = (dis_param != NULL)
                    && !strcmp(dis_param,QCameraParameters::VALUE_ENABLE);
            if(disEnabled) {
                char value[PROPERTY_VALUE_MAX];
                property_get("persist.camera.is_type", value, "0");
                streamInfo->is_type = static_cast<cam_is_type_t>(atoi(value));
            } else {
                streamInfo->is_type = IS_TYPE_NONE;
            }
            if (mParameters.isSeeMoreEnabled()) {
                streamInfo->pp_config.feature_mask |= CAM_QCOM_FEATURE_LLVD;
            }
        }
        break;
    default:
        break;
    }

    if ((!isZSLMode() ||
        (isZSLMode() && (stream_type != CAM_STREAM_TYPE_SNAPSHOT))) &&
        !mParameters.isHDREnabled()) {
        //set flip mode based on Stream type;
        int flipMode = mParameters.getFlipMode(stream_type);
        if (flipMode > 0) {
            streamInfo->pp_config.feature_mask |= CAM_QCOM_FEATURE_FLIP;
            streamInfo->pp_config.flip = (uint32_t)flipMode;
        }
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
    CDBG_HIGH("%s (0x%x) : mMsgEnabled = 0x%x", __func__, msg_type , mMsgEnabled );
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
    CDBG_HIGH("%s (0x%x) : mMsgEnabled = 0x%x", __func__, msg_type , mMsgEnabled );
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
    ATRACE_CALL();
    int32_t rc = NO_ERROR;
    CDBG_HIGH("%s: E", __func__);
    // start preview stream
    if (mParameters.isZSLMode() && mParameters.getRecordingHintValue() !=true) {
        rc = startChannel(QCAMERA_CH_TYPE_ZSL);
    } else {
        rc = startChannel(QCAMERA_CH_TYPE_PREVIEW);
        /*
          CAF needs cancel auto focus to resume after snapshot.
          Focus should be locked till take picture is done.
          In Non-zsl case if focus mode is CAF then calling cancel auto focus
          to resume CAF.
        */
        cam_focus_mode_type focusMode = mParameters.getFocusMode();
        if (focusMode == CAM_FOCUS_MODE_CONTINOUS_PICTURE)
            mCameraHandle->ops->cancel_auto_focus(mCameraHandle->camera_handle);
    }
    CDBG_HIGH("%s: X", __func__);
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
    ATRACE_CALL();
    CDBG_HIGH("%s: E", __func__);
    mActiveAF = false;
    mNumPreviewFaces = -1;
    // stop preview stream
    stopChannel(QCAMERA_CH_TYPE_ZSL);
    stopChannel(QCAMERA_CH_TYPE_PREVIEW);

    //reset preview frame skip
    mPreviewFrameSkipValid = 0;
    memset(&mPreviewFrameSkipIdxRange, 0, sizeof(cam_frame_idx_range_t));

    m_cbNotifier.flushPreviewNotifications();
    // delete all channels from preparePreview
    unpreparePreview();
    CDBG_HIGH("%s: X", __func__);
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
    CDBG_HIGH("%s: E", __func__);
    if (mParameters.getRecordingHintValue() == false) {
        CDBG_HIGH("%s: start recording when hint is false, stop preview first", __func__);
        stopPreview();

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
    CDBG_HIGH("%s: X", __func__);
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
    CDBG_HIGH("%s: E", __func__);
    int rc = stopChannel(QCAMERA_CH_TYPE_VIDEO);

#ifdef HAS_MULTIMEDIA_HINTS
    if (m_pPowerModule) {
        if (m_pPowerModule->powerHint) {
            m_pPowerModule->powerHint(m_pPowerModule, POWER_HINT_VIDEO_ENCODE, (void *)"state=0");
        }
    }
#endif
    CDBG_HIGH("%s: X", __func__);
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
    CDBG_HIGH("%s: opaque data = %p", __func__,opaque);
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
    setCancelAutoFocus(false);
    mActiveAF = true;
    cam_focus_mode_type focusMode = mParameters.getFocusMode();
    CDBG_HIGH("[AF_DBG] %s: focusMode=%d, m_currentFocusState=%d, m_bAFRunning=%d",
          __func__, focusMode, m_currentFocusState, isAFRunning());

    switch (focusMode) {
    case CAM_FOCUS_MODE_AUTO:
    case CAM_FOCUS_MODE_MACRO:
    case CAM_FOCUS_MODE_CONTINOUS_VIDEO:
    case CAM_FOCUS_MODE_CONTINOUS_PICTURE:
        rc = mCameraHandle->ops->do_auto_focus(mCameraHandle->camera_handle);
        break;
    case CAM_FOCUS_MODE_INFINITY:
    case CAM_FOCUS_MODE_FIXED:
    case CAM_FOCUS_MODE_EDOF:
    default:
        ALOGE("%s: No ops in focusMode (%d)", __func__, focusMode);
        rc = sendEvtNotify(CAMERA_MSG_FOCUS, true, 0);
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
    setCancelAutoFocus(true);
    mActiveAF = false;
    cam_focus_mode_type focusMode = mParameters.getFocusMode();

    switch (focusMode) {
    case CAM_FOCUS_MODE_AUTO:
    case CAM_FOCUS_MODE_MACRO:
    case CAM_FOCUS_MODE_CONTINOUS_VIDEO:
    case CAM_FOCUS_MODE_CONTINOUS_PICTURE:
        rc = mCameraHandle->ops->cancel_auto_focus(mCameraHandle->camera_handle);
        break;
    case CAM_FOCUS_MODE_INFINITY:
    case CAM_FOCUS_MODE_FIXED:
    case CAM_FOCUS_MODE_EDOF:
    default:
        CDBG("%s: No ops in focusMode (%d)", __func__, focusMode);
        break;
    }
    return rc;
}

/*===========================================================================
 * FUNCTION   : processUFDumps
 *
 * DESCRIPTION: process UF jpeg dumps for refocus support
 *
 * PARAMETERS :
 *   @evt     : payload of jpeg event, including information about jpeg encoding
 *              status, jpeg size and so on.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *
 * NOTE       : none
 *==========================================================================*/
bool QCamera2HardwareInterface::processUFDumps(qcamera_jpeg_evt_payload_t *evt)
{
   bool ret = true;
   if (mParameters.isUbiRefocus()) {
       int index = (int)getOutputImageCount();
       bool allFocusImage = (index == ((int)mParameters.UfOutputCount()-1));
       char name[CAM_FN_CNT];

       camera_memory_t *jpeg_mem = NULL;
       omx_jpeg_ouput_buf_t *jpeg_out = NULL;
       size_t dataLen;
       uint8_t *dataPtr;
       if (!m_postprocessor.getJpegMemOpt()) {
           dataLen = evt->out_data.buf_filled_len;
           dataPtr = evt->out_data.buf_vaddr;
       } else {
           jpeg_out  = (omx_jpeg_ouput_buf_t*) evt->out_data.buf_vaddr;
           jpeg_mem = (camera_memory_t *)jpeg_out->mem_hdl;
           dataPtr = (uint8_t *)jpeg_mem->data;
           dataLen = jpeg_mem->size;
       }

       if (allFocusImage)  {
           snprintf(name, CAM_FN_CNT, "AllFocusImage");
           index = -1;
       } else {
           snprintf(name, CAM_FN_CNT, "%d", 0);
       }
#ifdef USE_KK_CODE
       CAM_DUMP_TO_FILE("/data/local/ubifocus", name, index, "jpg",
           dataPtr, dataLen);
#else
       CAM_DUMP_TO_FILE("/data/misc/camera/ubifocus", name, index, "jpg",
           dataPtr, dataLen);
#endif
       CDBG_HIGH("%s:%d] Dump the image %d %d allFocusImage %d", __func__, __LINE__,
           getOutputImageCount(), index, allFocusImage);
       setOutputImageCount(getOutputImageCount() + 1);
       if (!allFocusImage) {
           ret = false;
       }
   }
   return ret;
}

/*===========================================================================
 * FUNCTION   : processMTFDumps
 *
 * DESCRIPTION: process MTF jpeg dumps for refocus support
 *
 * PARAMETERS :
 *   @evt     : payload of jpeg event, including information about jpeg encoding
 *              status, jpeg size and so on.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *
 * NOTE       : none
 *==========================================================================*/
bool QCamera2HardwareInterface::processMTFDumps(qcamera_jpeg_evt_payload_t *evt)
{
   bool ret = true;
   if (mParameters.isMTFRefocus()) {
       int index = (int) getOutputImageCount();
       bool allFocusImage = (index == ((int)mParameters.MTFOutputCount()-1));
       char name[CAM_FN_CNT];

       camera_memory_t *jpeg_mem = NULL;
       omx_jpeg_ouput_buf_t *jpeg_out = NULL;
       size_t dataLen;
       uint8_t *dataPtr;
       if (!m_postprocessor.getJpegMemOpt()) {
           dataLen = evt->out_data.buf_filled_len;
           dataPtr = evt->out_data.buf_vaddr;
       } else {
           jpeg_out  = (omx_jpeg_ouput_buf_t*) evt->out_data.buf_vaddr;
           jpeg_mem = (camera_memory_t *)jpeg_out->mem_hdl;
           dataPtr = (uint8_t *)jpeg_mem->data;
           dataLen = jpeg_mem->size;
       }

       if (allFocusImage)  {
           strncpy(name, "AllFocusImage", CAM_FN_CNT - 1);
           index = -1;
       } else {
           strncpy(name, "0", CAM_FN_CNT - 1);
       }
#ifdef USE_KK_CODE
       CAM_DUMP_TO_FILE("/data/local/multiTouchFocus", name, index, "jpg",
               dataPtr, dataLen);
#else
       CAM_DUMP_TO_FILE("/data/misc/camera/multiTouchFocus", name, index, "jpg",
               dataPtr, dataLen);
#endif
       CDBG("%s:%d] Dump the image %d %d allFocusImage %d", __func__, __LINE__,
               getOutputImageCount(), index, allFocusImage);
       setOutputImageCount(getOutputImageCount() + 1);
       if (!allFocusImage) {
           ret = false;
       }
   }
   return ret;
}

/*===========================================================================
 * FUNCTION   : unconfigureAdvancedCapture
 *
 * DESCRIPTION: unconfigure Advanced Capture.
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *
 *==========================================================================*/
int32_t QCamera2HardwareInterface::unconfigureAdvancedCapture()
{
    int32_t rc = NO_ERROR;

    if (mAdvancedCaptureConfigured) {

        mAdvancedCaptureConfigured = false;

        if(mIs3ALocked) {
            mParameters.set3ALock(QCameraParameters::VALUE_FALSE);
            mIs3ALocked = false;
        }
        if ( mParameters.isHDREnabled() || mParameters.isAEBracketEnabled()) {
            rc = mParameters.stopAEBracket();
        } else if (mParameters.isUbiFocusEnabled() || mParameters.isUbiRefocus()) {
            rc = configureAFBracketing(false);
        } else if (mParameters.isChromaFlashEnabled()) {
            rc = configureFlashBracketing(false);
        } else  if (mParameters.isOptiZoomEnabled() ||
                mParameters.isfssrEnabled()) {
            rc = mParameters.setAndCommitZoom(mZoomLevel);
        } else if (mParameters.isMultiTouchFocusEnabled()) {
            configureMTFBracketing(false);
        } else {
            ALOGE("%s: No Advanced Capture feature enabled!! ", __func__);
            rc = BAD_VALUE;
        }
        if (mParameters.isMultiTouchFocusSelected()) {
            mParameters.resetMultiTouchFocusParam();
        }
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : configureAdvancedCapture
 *
 * DESCRIPTION: configure Advanced Capture.
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::configureAdvancedCapture()
{
    CDBG_HIGH("%s: E",__func__);
    int32_t rc = NO_ERROR;

    setOutputImageCount(0);
    mParameters.setDisplayFrame(FALSE);
    if (mParameters.isUbiFocusEnabled()) {
        rc = configureAFBracketing();
    } else if (mParameters.isMultiTouchFocusEnabled()) {
        rc = configureMTFBracketing();
    } else if (mParameters.isOptiZoomEnabled()) {
        rc = configureOptiZoom();
    } else if (mParameters.isfssrEnabled()) {
        rc = configureFssr();
    } else if (mParameters.isChromaFlashEnabled()) {
        rc = configureFlashBracketing();
    } else if (mParameters.isHDREnabled()) {
        rc = configureZSLHDRBracketing();
    } else if (mParameters.isAEBracketEnabled()) {
        rc = configureAEBracketing();
    } else {
        ALOGE("%s: No Advanced Capture feature enabled!! ", __func__);
        rc = BAD_VALUE;
    }

    if (NO_ERROR == rc) {
        mAdvancedCaptureConfigured = true;
    } else {
        mAdvancedCaptureConfigured = false;
    }

    CDBG_HIGH("%s: X",__func__);
    return rc;
}

/*===========================================================================
 * FUNCTION   : configureAFBracketing
 *
 * DESCRIPTION: configure AF Bracketing.
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::configureAFBracketing(bool enable)
{
    CDBG_HIGH("%s: E",__func__);
    int32_t rc = NO_ERROR;
    cam_af_bracketing_t *af_bracketing_need;
    af_bracketing_need =
        &gCamCapability[mCameraId]->ubifocus_af_bracketing_need;

    //Enable AF Bracketing.
    cam_af_bracketing_t afBracket;
    memset(&afBracket, 0, sizeof(cam_af_bracketing_t));
    afBracket.enable = enable;
    afBracket.burst_count = af_bracketing_need->burst_count;

    for(int8_t i = 0; i < MAX_AF_BRACKETING_VALUES; i++) {
        afBracket.focus_steps[i] = af_bracketing_need->focus_steps[i];
        CDBG_HIGH("%s: focus_step[%d] = %d", __func__, i, afBracket.focus_steps[i]);
    }
    //Send cmd to backend to set AF Bracketing for Ubi Focus.
    rc = mParameters.commitAFBracket(afBracket);
    if ( NO_ERROR != rc ) {
        ALOGE("%s: cannot configure AF bracketing", __func__);
        return rc;
    }
    if (enable) {
        mParameters.set3ALock(QCameraParameters::VALUE_TRUE);
        mIs3ALocked = true;
    }
    CDBG_HIGH("%s: X",__func__);
    return rc;
}

/*===========================================================================
 * FUNCTION   : configureMTFBracketing
 *
 * DESCRIPTION: configure multi-touch focus AF Bracketing.
 *
 * PARAMETERS :
 *   @enable  : bool flag if MTF should be enabled
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::configureMTFBracketing(bool enable)
{
    int32_t rc = NO_ERROR;
    cam_af_bracketing_t *mtf_bracketing_need;
    mtf_bracketing_need = &mParameters.m_MTFBracketInfo;

    //Enable AF Bracketing.
    cam_af_bracketing_t afBracket;
    memset(&afBracket, 0, sizeof(cam_af_bracketing_t));
    afBracket.enable = enable;
    afBracket.burst_count = mtf_bracketing_need->burst_count;

    for(int8_t i = 0; i < MAX_AF_BRACKETING_VALUES; i++) {
        if (mtf_bracketing_need->focus_steps[i] != -1) {
           afBracket.focus_steps[i] = mtf_bracketing_need->focus_steps[i];
        }
        CDBG_HIGH("%s: MTF focus_step[%d] = %d",
                  __func__, i, afBracket.focus_steps[i]);
    }
    //Send cmd to backend to set AF Bracketing for MTF.
    rc = mParameters.commitMTFBracket(afBracket);
    mParameters.m_currNumBufMTF = afBracket.burst_count;
    if ( NO_ERROR != rc ) {
        ALOGE("%s: cannot configure MTF bracketing", __func__);
        return rc;
    }
    if (enable) {
        mParameters.set3ALock(QCameraParameters::VALUE_TRUE);
        mIs3ALocked = true;
    }
    if (!enable) {
        mParameters.m_currNumBufMTF = 0;
    }
    //reset multi-touch focus parameters for next use.
    mParameters.resetMultiTouchFocusParam();
    return rc;
}

/*===========================================================================
 * FUNCTION   : configureFlashBracketing
 *
 * DESCRIPTION: configure Flash Bracketing.
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::configureFlashBracketing(bool enable)
{
    CDBG_HIGH("%s: E",__func__);
    int32_t rc = NO_ERROR;

    cam_flash_bracketing_t flashBracket;
    memset(&flashBracket, 0, sizeof(cam_flash_bracketing_t));
    flashBracket.enable = enable;
    //TODO: Hardcoded value.
    flashBracket.burst_count = 2;
    //Send cmd to backend to set Flash Bracketing for chroma flash.
    rc = mParameters.commitFlashBracket(flashBracket);
    if ( NO_ERROR != rc ) {
        ALOGE("%s: cannot configure AF bracketing", __func__);
    }
    CDBG_HIGH("%s: X",__func__);
    return rc;
}

/*===========================================================================
 * FUNCTION   : configureZSLHDRBracketing
 *
 * DESCRIPTION: configure HDR Bracketing.
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::configureZSLHDRBracketing()
{
    CDBG_HIGH("%s: E",__func__);
    int32_t rc = NO_ERROR;

    // 'values' should be in "idx1,idx2,idx3,..." format
    uint32_t hdrFrameCount = gCamCapability[mCameraId]->hdr_bracketing_setting.num_frames;
    CDBG_HIGH("%s : HDR values %d, %d frame count: %u",
          __func__,
          (int) gCamCapability[mCameraId]->hdr_bracketing_setting.exp_val.values[0],
          (int) gCamCapability[mCameraId]->hdr_bracketing_setting.exp_val.values[1],
          hdrFrameCount);

    // Enable AE Bracketing for HDR
    cam_exp_bracketing_t aeBracket;
    memset(&aeBracket, 0, sizeof(cam_exp_bracketing_t));
    aeBracket.mode =
        gCamCapability[mCameraId]->hdr_bracketing_setting.exp_val.mode;
    String8 tmp;
    for (uint32_t i = 0; i < hdrFrameCount; i++) {
        tmp.appendFormat("%d",
            (int8_t) gCamCapability[mCameraId]->hdr_bracketing_setting.exp_val.values[i]);
        tmp.append(",");
    }
    if (mParameters.isHDR1xFrameEnabled()
        && mParameters.isHDR1xExtraBufferNeeded()) {
            tmp.appendFormat("%d", 0);
            tmp.append(",");
    }

    if( !tmp.isEmpty() &&
        ( MAX_EXP_BRACKETING_LENGTH > tmp.length() ) ) {
        //Trim last comma
        memset(aeBracket.values, '\0', MAX_EXP_BRACKETING_LENGTH);
        memcpy(aeBracket.values, tmp.string(), tmp.length() - 1);
    }

    CDBG_HIGH("%s : HDR config values %s",
          __func__,
          aeBracket.values);
    rc = mParameters.setHDRAEBracket(aeBracket);
    if ( NO_ERROR != rc ) {
        ALOGE("%s: cannot configure HDR bracketing", __func__);
        return rc;
    }
    CDBG_HIGH("%s: X",__func__);
    return rc;
}

/*===========================================================================
 * FUNCTION   : configureAEBracketing
 *
 * DESCRIPTION: configure AE Bracketing.
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::configureAEBracketing()
{
    CDBG_HIGH("%s: E",__func__);
    int32_t rc = NO_ERROR;

    rc = mParameters.setAEBracketing();
    if ( NO_ERROR != rc ) {
        ALOGE("%s: cannot configure AE bracketing", __func__);
        return rc;
    }
    CDBG_HIGH("%s: X",__func__);
    return rc;
}

/*===========================================================================
 * FUNCTION   : configureOptiZoom
 *
 * DESCRIPTION: configure Opti Zoom.
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::configureOptiZoom()
{
    int32_t rc = NO_ERROR;

    //store current zoom level.
    mZoomLevel = (uint8_t) mParameters.getInt(CameraParameters::KEY_ZOOM);

    //set zoom level to 1x;
    mParameters.setAndCommitZoom(0);

    mParameters.set3ALock(QCameraParameters::VALUE_TRUE);
    mIs3ALocked = true;

    return rc;
}

/*===========================================================================
 * FUNCTION   : configureFssr
 *
 * DESCRIPTION: configure fssr.
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*///TODO_fssr: place holder check the req for fssr
int32_t QCamera2HardwareInterface::configureFssr()
{
    int32_t rc = NO_ERROR;

    //store current zoom level.
    mZoomLevel = (uint8_t) mParameters.getInt(CameraParameters::KEY_ZOOM);

    //set zoom level to 1x;
    mParameters.setAndCommitZoom(0);

    mParameters.set3ALock(QCameraParameters::VALUE_TRUE);
    mIs3ALocked = true;

    return rc;
}

/*===========================================================================
 * FUNCTION   : startAdvancedCapture
 *
 * DESCRIPTION: starts advanced capture based on capture type
 *
 * PARAMETERS :
 *   @pChannel : channel.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::startAdvancedCapture(
    QCameraPicChannel *pChannel)
{
    CDBG_HIGH("%s: Start bracketig",__func__);
    int32_t rc = NO_ERROR;

    if(mParameters.isUbiFocusEnabled()) {
        rc = pChannel->startAdvancedCapture(MM_CAMERA_AF_BRACKETING);
    } else if (mParameters.isMultiTouchFocusEnabled()) {
        rc = pChannel->startAdvancedCapture(MM_CAMERA_MTF_BRACKETING);
    } else if (mParameters.isChromaFlashEnabled()) {
        rc = pChannel->startAdvancedCapture(MM_CAMERA_FLASH_BRACKETING);
    } else if (mParameters.isHDREnabled() || mParameters.isAEBracketEnabled()) {
        rc = pChannel->startAdvancedCapture(MM_CAMERA_AE_BRACKETING);
    } else if (mParameters.isOptiZoomEnabled()) {
        rc = pChannel->startAdvancedCapture(MM_CAMERA_ZOOM_1X);
    } else if (mParameters.isfssrEnabled()) {
        rc = pChannel->startAdvancedCapture(MM_CAMERA_ZOOM_1X);
    } else {
        ALOGE("%s: No Advanced Capture feature enabled!",__func__);
        rc = BAD_VALUE;
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

    if (mParameters.isUbiFocusEnabled() ||
            mParameters.isMultiTouchFocusEnabled() ||
            mParameters.isOptiZoomEnabled() ||
            mParameters.isfssrEnabled() ||
            mParameters.isHDREnabled() ||
            mParameters.isChromaFlashEnabled() ||
            mParameters.isAEBracketEnabled()) {
        rc = configureAdvancedCapture();
        if (rc == NO_ERROR) {
            numSnapshots = mParameters.getBurstCountForAdvancedCapture();
        }
    }
    CDBG_HIGH("%s: numSnapshot = %d",__func__, numSnapshots);

    getOrientation();
    CDBG_HIGH("%s: E", __func__);
    if (mParameters.isZSLMode()) {
        QCameraPicChannel *pZSLChannel =
            (QCameraPicChannel *)m_channels[QCAMERA_CH_TYPE_ZSL];
        if (NULL != pZSLChannel) {
            // start postprocessor
            rc = m_postprocessor.start(pZSLChannel);
            if (rc != NO_ERROR) {
                ALOGE("%s: cannot start postprocessor", __func__);
                return rc;
            }
            if (mParameters.isUbiFocusEnabled() ||
                    mParameters.isMultiTouchFocusEnabled() ||
                    mParameters.isOptiZoomEnabled() ||
                    mParameters.isHDREnabled() ||
                    mParameters.isfssrEnabled() ||
                    mParameters.isChromaFlashEnabled() ||
                    mParameters.isAEBracketEnabled()) {
                rc = startAdvancedCapture(pZSLChannel);
                if (rc != NO_ERROR) {
                    ALOGE("%s: cannot start zsl advanced capture", __func__);
                    return rc;
                }
            }
            if (mLongshotEnabled && mPrepSnapRun) {
                mCameraHandle->ops->start_zsl_snapshot(
                        mCameraHandle->camera_handle,
                        pZSLChannel->getMyHandle());
            }
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

        // start snapshot
        if (mParameters.isJpegPictureFormat() ||
            mParameters.isNV16PictureFormat() ||
            mParameters.isNV21PictureFormat()) {

            if (!isLongshotEnabled()) {
                rc = addCaptureChannel();

                // normal capture case
                // need to stop preview channel
                stopChannel(QCAMERA_CH_TYPE_PREVIEW);
                delChannel(QCAMERA_CH_TYPE_PREVIEW);

                waitDefferedWork(mSnapshotJob);
                waitDefferedWork(mMetadataJob);
                waitDefferedWork(mRawdataJob);

                {
                    DefferWorkArgs args;
                    DefferAllocBuffArgs allocArgs;

                    memset(&args, 0, sizeof(DefferWorkArgs));
                    memset(&allocArgs, 0, sizeof(DefferAllocBuffArgs));

                    allocArgs.ch = m_channels[QCAMERA_CH_TYPE_CAPTURE];
                    allocArgs.type = CAM_STREAM_TYPE_POSTVIEW;
                    args.allocArgs = allocArgs;

                    mPostviewJob = queueDefferedWork(CMD_DEFF_ALLOCATE_BUFF,
                            args);

                    if ( mPostviewJob == -1)
                        rc = UNKNOWN_ERROR;
                }

                waitDefferedWork(mPostviewJob);
            } else {
                // normal capture case
                // need to stop preview channel
                stopChannel(QCAMERA_CH_TYPE_PREVIEW);
                delChannel(QCAMERA_CH_TYPE_PREVIEW);

                rc = addCaptureChannel();
            }

            if ((rc == NO_ERROR) &&
                (NULL != m_channels[QCAMERA_CH_TYPE_CAPTURE])) {

                // configure capture channel
                rc = m_channels[QCAMERA_CH_TYPE_CAPTURE]->config();
                if (rc != NO_ERROR) {
                    ALOGE("%s: cannot configure capture channel", __func__);
                    delChannel(QCAMERA_CH_TYPE_CAPTURE);
                    return rc;
                }

                DefferWorkArgs args;
                memset(&args, 0, sizeof(DefferWorkArgs));

                args.pprocArgs = m_channels[QCAMERA_CH_TYPE_CAPTURE];
                mReprocJob = queueDefferedWork(CMD_DEFF_PPROC_START,
                        args);

                // start catpure channel
                rc =  m_channels[QCAMERA_CH_TYPE_CAPTURE]->start();
                if (rc != NO_ERROR) {
                    ALOGE("%s: cannot start capture channel", __func__);
                    delChannel(QCAMERA_CH_TYPE_CAPTURE);
                    return rc;
                }

                QCameraPicChannel *pCapChannel =
                    (QCameraPicChannel *)m_channels[QCAMERA_CH_TYPE_CAPTURE];
                if (NULL != pCapChannel) {
                    if (mParameters.isUbiFocusEnabled()|
                        mParameters.isChromaFlashEnabled()) {
                        rc = startAdvancedCapture(pCapChannel);
                        if (rc != NO_ERROR) {
                            ALOGE("%s: cannot start advanced capture", __func__);
                            return rc;
                        }
                    }
                }
                if ( mLongshotEnabled ) {
                    rc = longShot();
                    if (NO_ERROR != rc) {
                        delChannel(QCAMERA_CH_TYPE_CAPTURE);
                        return rc;
                    }
                }
            } else {
                ALOGE("%s: cannot add capture channel", __func__);
                return rc;
            }
        } else {

            stopChannel(QCAMERA_CH_TYPE_PREVIEW);
            delChannel(QCAMERA_CH_TYPE_PREVIEW);

            rc = addRawChannel();
            if (rc == NO_ERROR) {
                // start postprocessor
                rc = m_postprocessor.start(m_channels[QCAMERA_CH_TYPE_RAW]);
                if (rc != NO_ERROR) {
                    ALOGE("%s: cannot start postprocessor", __func__);
                    delChannel(QCAMERA_CH_TYPE_RAW);
                    return rc;
                }

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
    CDBG_HIGH("%s: X", __func__);
    return rc;
}

/*===========================================================================
 * FUNCTION   : longShot
 *
 * DESCRIPTION: Queue one more ZSL frame
 *              in the longshot pipe.
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::longShot()
{
    int32_t rc = NO_ERROR;
    uint8_t numSnapshots = mParameters.getNumOfSnapshots();
    QCameraPicChannel *pChannel = NULL;

    if (mParameters.isZSLMode()) {
        pChannel = (QCameraPicChannel *)m_channels[QCAMERA_CH_TYPE_ZSL];
    } else {
        pChannel = (QCameraPicChannel *)m_channels[QCAMERA_CH_TYPE_CAPTURE];
    }

    if (NULL != pChannel) {
        rc = pChannel->takePicture(numSnapshots);
    } else {
        ALOGE(" %s : Capture channel not initialized!", __func__);
        rc = NO_INIT;
        goto end;
    }

end:
    return rc;
}

/*===========================================================================
 * FUNCTION   : stopCaptureChannel
 *
 * DESCRIPTION: Stops capture channel
 *
 * PARAMETERS :
 *   @destroy : Set to true to stop and delete camera channel.
 *              Set to false to only stop capture channel.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::stopCaptureChannel(bool destroy)
{
    if (mParameters.isJpegPictureFormat() ||
        mParameters.isNV16PictureFormat() ||
        mParameters.isNV21PictureFormat()) {
        stopChannel(QCAMERA_CH_TYPE_CAPTURE);
        if (destroy) {
            // Destroy camera channel but dont release context
            delChannel(QCAMERA_CH_TYPE_CAPTURE, false);
        }
    }

    return NO_ERROR;
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
    CDBG_HIGH("%s:%d] ",__func__, __LINE__);
    waitDefferedWork(mReprocJob);

    //stop post processor
    m_postprocessor.stop();

    unconfigureAdvancedCapture();

    mParameters.setDisplayFrame(TRUE);

    if (mParameters.isZSLMode()) {
        QCameraPicChannel *pZSLChannel =
            (QCameraPicChannel *)m_channels[QCAMERA_CH_TYPE_ZSL];
        if (NULL != pZSLChannel) {
            pZSLChannel->cancelPicture();
        }
    } else {

        // normal capture case
        if (mParameters.isJpegPictureFormat() ||
            mParameters.isNV16PictureFormat() ||
            mParameters.isNV21PictureFormat()) {
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
 * FUNCTION   : captureDone
 *
 * DESCRIPTION: Function called when the capture is completed before encoding
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera2HardwareInterface::captureDone()
{
    if (++mOutputCount >= mParameters.getBurstCountForAdvancedCapture()) {
        unconfigureAdvancedCapture();
    }
}

/*===========================================================================
 * FUNCTION   : Live_Snapshot_thread
 *
 * DESCRIPTION: Seperate thread for taking live snapshot during recording
 *
 * PARAMETERS : @data - pointer to QCamera2HardwareInterface class object
 *
 * RETURN     : none
 *==========================================================================*/
void* Live_Snapshot_thread (void* data)
{

    QCamera2HardwareInterface *hw = reinterpret_cast<QCamera2HardwareInterface *>(data);
    if (!hw) {
        ALOGE("take_picture_thread: NULL camera device");
        return (void *)BAD_VALUE;
    }
    hw->takeLiveSnapshot_internal();
    return (void* )NULL;
}

/*===========================================================================
 * FUNCTION   : Int_Pic_thread
 *
 * DESCRIPTION: Seperate thread for taking snapshot triggered by camera backend
 *
 * PARAMETERS : @data - pointer to QCamera2HardwareInterface class object
 *
 * RETURN     : none
 *==========================================================================*/
void* Int_Pic_thread (void* data)
{

    QCamera2HardwareInterface *hw = reinterpret_cast<QCamera2HardwareInterface *>(data);

    if (!hw) {
        ALOGE("take_picture_thread: NULL camera device");
        return (void *)BAD_VALUE;
    }

    bool JpegMemOpt = false;

    hw->takeBackendPic_internal(&JpegMemOpt);
    hw->checkIntPicPending(JpegMemOpt);

    return (void* )NULL;
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
    rc= pthread_create(&mLiveSnapshotThread, NULL, Live_Snapshot_thread, (void *) this);
    return rc;
}

/*===========================================================================
 * FUNCTION   : takePictureInternal
 *
 * DESCRIPTION: take snapshot triggered by backend
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::takePictureInternal()
{
    int rc = NO_ERROR;
    rc= pthread_create(&mIntPicThread, NULL, Int_Pic_thread, (void *) this);
    return rc;
}

/*===========================================================================
 * FUNCTION   : checkIntPicPending
 *
 * DESCRIPTION: timed wait for jpeg completion event, and send
 *                        back completion event to backend
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
void QCamera2HardwareInterface::checkIntPicPending(bool JpegMemOpt)
{
    cam_int_evt_params_t params;
    int rc = NO_ERROR;

    struct timespec ts;
    struct timeval tp;
    gettimeofday(&tp, NULL);
    ts.tv_sec  = tp.tv_sec;
    ts.tv_nsec = tp.tv_usec * 1000 + 1000 * 1000000;

    if (true == m_bIntEvtPending) {
        //wait on the eztune condition variable
        pthread_mutex_lock(&m_int_lock);
        rc = pthread_cond_timedwait(&m_int_cond, &m_int_lock, &ts);
        m_bIntEvtPending = false;
        pthread_mutex_unlock(&m_int_lock);
        if (ETIMEDOUT == rc) {
            return;
        }

        params.dim = m_postprocessor.m_dst_dim;
        //send event back to server with the file path
        memcpy(&params.path[0], &m_BackendFileName[0], 50);
        params.size = mBackendFileSize;
        pthread_mutex_lock(&m_parm_lock);
        rc = mParameters.setIntEvent(params);
        pthread_mutex_unlock(&m_parm_lock);

        lockAPI();
        rc = processAPI(QCAMERA_SM_EVT_SNAPSHOT_DONE, NULL);
        unlockAPI();
        if (false == mParameters.isZSLMode()) {
            lockAPI();
            rc = processAPI(QCAMERA_SM_EVT_START_PREVIEW, NULL);
            unlockAPI();
        }

        m_postprocessor.setJpegMemOpt(JpegMemOpt);
    }

    return;
}

/*===========================================================================
 * FUNCTION   : takeBackendPic_internal
 *
 * DESCRIPTION: take snapshot triggered by backend
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::takeBackendPic_internal(bool *JpegMemOpt)
{
    int rc;

    *JpegMemOpt = m_postprocessor.getJpegMemOpt();
    m_postprocessor.setJpegMemOpt(false);

    lockAPI();
    rc = processAPI(QCAMERA_SM_EVT_TAKE_PICTURE, NULL);
    if (rc == NO_ERROR) {
        qcamera_api_result_t apiResult;
        waitAPIResult(QCAMERA_SM_EVT_TAKE_PICTURE, &apiResult);
    }
    unlockAPI();

    return rc;
}

/*===========================================================================
 * FUNCTION   : takeLiveSnapshot_internal
 *
 * DESCRIPTION: take live snapshot during recording
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::takeLiveSnapshot_internal()
{
    int rc = NO_ERROR;
    getOrientation();
    QCameraChannel *pChannel = NULL;

    cam_dimension_t videoSize;
    mParameters.getVideoSize(&videoSize.width, &videoSize.height);

    if (is4k2kResolution(&videoSize)) {
        pChannel = m_channels[QCAMERA_CH_TYPE_VIDEO];
    } else {
        pChannel = m_channels[QCAMERA_CH_TYPE_SNAPSHOT];
    }

    // start post processor
    rc = m_postprocessor.start(pChannel);
    if (NO_ERROR != rc) {
        ALOGE("%s: Post-processor start failed %d", __func__, rc);
        goto end;
    }

    if (is4k2kResolution(&videoSize)) {
        return ((QCameraVideoChannel*)pChannel)->takePicture(1);
    }

    if (NULL == pChannel) {
        ALOGE("%s: Snapshot channel not initialized", __func__);
        rc = NO_INIT;
        goto end;
    }

    // start snapshot channel
    if ((rc == NO_ERROR) && (NULL != pChannel)) {

        // Find and try to link a metadata stream from preview channel
        QCameraChannel *pMetaChannel = NULL;
        QCameraStream *pMetaStream = NULL;

        if (m_channels[QCAMERA_CH_TYPE_PREVIEW] != NULL) {
            pMetaChannel = m_channels[QCAMERA_CH_TYPE_PREVIEW];
            uint32_t streamNum = pMetaChannel->getNumOfStreams();
            QCameraStream *pStream = NULL;
            for (uint32_t i = 0 ; i < streamNum ; i++ ) {
                pStream = pMetaChannel->getStreamByIndex(i);
                if ((NULL != pStream) &&
                        (CAM_STREAM_TYPE_METADATA == pStream->getMyType())) {
                    pMetaStream = pStream;
                    break;
                }
            }
        }

        if ((NULL != pMetaChannel) && (NULL != pMetaStream)) {
            rc = pChannel->linkStream(pMetaChannel, pMetaStream);
            if (NO_ERROR != rc) {
                ALOGE("%s : Metadata stream link failed %d", __func__, rc);
            }
        }

        rc = pChannel->start();
    }

end:
    if (rc != NO_ERROR) {
        rc = processAPI(QCAMERA_SM_EVT_CANCEL_PICTURE, NULL);
        rc = sendEvtNotify(CAMERA_MSG_ERROR, UNKNOWN_ERROR, 0);
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
    if (mLiveSnapshotThread != 0) {
        pthread_join(mLiveSnapshotThread,NULL);
        mLiveSnapshotThread = 0;
    }
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

    int cur_width, cur_height;
    pthread_mutex_lock(&m_parm_lock);
    //Need take care Scale picture size
    if(mParameters.m_reprocScaleParam.isScaleEnabled() &&
        mParameters.m_reprocScaleParam.isUnderScaling()){
        int scale_width, scale_height;

        mParameters.m_reprocScaleParam.getPicSizeFromAPK(scale_width,scale_height);
        mParameters.getPictureSize(&cur_width, &cur_height);

        String8 pic_size;
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%dx%d", scale_width, scale_height);
        pic_size.append(buffer);
        mParameters.set(CameraParameters::KEY_PICTURE_SIZE, pic_size);
    }

    str = mParameters.flatten( );
    strParams = (char *)malloc(sizeof(char)*(str.length()+1));
    if(strParams != NULL){
        memset(strParams, 0, sizeof(char)*(str.length()+1));
        strncpy(strParams, str.string(), str.length());
        strParams[str.length()] = 0;
    }

    if(mParameters.m_reprocScaleParam.isScaleEnabled() &&
        mParameters.m_reprocScaleParam.isUnderScaling()){
        //need set back picture size
        String8 pic_size;
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%dx%d", cur_width, cur_height);
        pic_size.append(buffer);
        mParameters.set(CameraParameters::KEY_PICTURE_SIZE, pic_size);
    }
    pthread_mutex_unlock(&m_parm_lock);
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
int QCamera2HardwareInterface::sendCommand(int32_t command,
        int32_t &arg1, int32_t &/*arg2*/)
{
    int rc = NO_ERROR;

    switch (command) {
#ifndef VANILLA_HAL
    case CAMERA_CMD_LONGSHOT_ON:
        arg1 = 0;
        // Longshot can only be enabled when image capture
        // is not active.
        if ( !m_stateMachine.isCaptureRunning() ) {
            CDBG_HIGH("%s: Longshot Enabled", __func__);
            mLongshotEnabled = true;

            // Due to recent buffer count optimizations
            // ZSL might run with considerably less buffers
            // when not in longshot mode. Preview needs to
            // restart in this case.
            if (isZSLMode() && m_stateMachine.isPreviewRunning()) {
                QCameraChannel *pChannel = NULL;
                QCameraStream *pSnapStream = NULL;
                pChannel = m_channels[QCAMERA_CH_TYPE_ZSL];
                if (NULL != pChannel) {
                    QCameraStream *pStream = NULL;
                    for (uint32_t i = 0; i < pChannel->getNumOfStreams(); i++) {
                        pStream = pChannel->getStreamByIndex(i);
                        if (pStream != NULL) {
                            if (pStream->isTypeOf(CAM_STREAM_TYPE_SNAPSHOT)) {
                                pSnapStream = pStream;
                                break;
                            }
                        }
                    }
                    if (NULL != pSnapStream) {
                        uint8_t required = 0;
                        required = getBufNumRequired(CAM_STREAM_TYPE_SNAPSHOT);
                        if (pSnapStream->getBufferCount() < required) {
                            // We restart here, to reset the FPS and no
                            // of buffers as per the requirement of longshot usecase.
                            arg1 = QCAMERA_SM_EVT_RESTART_PERVIEW;
                        }
                    }
                }
            }
            rc = mParameters.setLongshotEnable(mLongshotEnabled);
            mPrepSnapRun = false;
        } else {
            rc = NO_INIT;
        }
        break;
    case CAMERA_CMD_LONGSHOT_OFF:
        if ( mLongshotEnabled && m_stateMachine.isCaptureRunning() ) {
            cancelPicture();
            processEvt(QCAMERA_SM_EVT_SNAPSHOT_DONE, NULL);
            QCameraChannel *pZSLChannel = m_channels[QCAMERA_CH_TYPE_ZSL];
            if (isZSLMode() && (NULL != pZSLChannel) && mPrepSnapRun) {
                mCameraHandle->ops->stop_zsl_snapshot(
                        mCameraHandle->camera_handle,
                        pZSLChannel->getMyHandle());
            }
        }
        CDBG_HIGH("%s: Longshot Disabled", __func__);
        mPrepSnapRun = false;
        mLongshotEnabled = false;
        rc = mParameters.setLongshotEnable(mLongshotEnabled);
        break;
    case CAMERA_CMD_HISTOGRAM_ON:
    case CAMERA_CMD_HISTOGRAM_OFF:
        rc = setHistogram(command == CAMERA_CMD_HISTOGRAM_ON? true : false);
        CDBG_HIGH("%s: Histogram -> %s", __func__,
              mParameters.isHistogramEnabled() ? "Enabled" : "Disabled");
        break;
#endif
    case CAMERA_CMD_START_FACE_DETECTION:
    case CAMERA_CMD_STOP_FACE_DETECTION:
        rc = setFaceDetection(command == CAMERA_CMD_START_FACE_DETECTION? true : false);
        CDBG_HIGH("%s: FaceDetection -> %s", __func__,
              mParameters.isFaceDetectionEnabled() ? "Enabled" : "Disabled");
        break;
#ifndef VANILLA_HAL
    case CAMERA_CMD_HISTOGRAM_SEND_DATA:
#endif
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

    ssize_t bufSize = imgBuf->getSize(0);
    if (BAD_INDEX != bufSize) {
        rc = pChannel->doReprocess(imgBuf->getFd(0), (size_t)bufSize, faceID);
    } else {
        ALOGE("Failed to retrieve buffer size (bad index)");
        return UNKNOWN_ERROR;
    }

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
            //peek into the event, if this is an eztune event from server,
            //then we don't need to post it to the SM Qs, we shud directly
            //spawn a thread and get the job done (jpeg or raw snapshot)
            if (CAM_EVENT_TYPE_INT_TAKE_PIC == payload->server_event_type) {
                pthread_mutex_lock(&obj->m_int_lock);
                obj->m_bIntEvtPending = true;
                pthread_mutex_unlock(&obj->m_int_lock);
                obj->takePictureInternal();
                free(payload);
            } else {
                obj->processEvt(QCAMERA_SM_EVT_EVT_NOTIFY, payload);
            }
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
            obj->processUFDumps(payload);
            obj->processMTFDumps(payload);
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
    if (!mCameraOpened) {
        CDBG("%s: Camera is not opened, no need to handle thermal evt", __func__);
        return NO_ERROR;
    }

    // Make sure thermal events are logged
    CDBG("%s: level = %d, userdata = %p, data = %p",
        __func__, level, userdata, data);
    //We don't need to lockAPI, waitAPI here. QCAMERA_SM_EVT_THERMAL_NOTIFY
    // becomes an aync call. This also means we can only pass payload
    // by value, not by address.
    return processAPI(QCAMERA_SM_EVT_THERMAL_NOTIFY, (void *)&level);
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
    CDBG_HIGH("%s: E",__func__);

    m_currentFocusState = focus_data.focus_state;

    cam_focus_mode_type focusMode = mParameters.getFocusMode();
    CDBG_HIGH("[AF_DBG] %s: focusMode=%d, m_currentFocusState=%d, m_bAFRunning=%d",
         __func__, focusMode, m_currentFocusState, isAFRunning());

    switch (focusMode) {
    case CAM_FOCUS_MODE_AUTO:
    case CAM_FOCUS_MODE_MACRO:
        if (getCancelAutoFocus()) {
            // auto focus has canceled, just ignore it
            break;
        }

        if (focus_data.focus_state == CAM_AF_PASSIVE_SCANNING ||
            focus_data.focus_state == CAM_AF_PASSIVE_FOCUSED ||
            focus_data.focus_state == CAM_AF_PASSIVE_UNFOCUSED) {
            //ignore passive(CAF) events in Auto/Macro AF modes
            break;
        }

        if (focus_data.focus_state == CAM_AF_SCANNING ||
            focus_data.focus_state == CAM_AF_INACTIVE) {
            // in the middle of focusing, just ignore it
            break;
        }

        // update focus distance
        mParameters.updateFocusDistances(&focus_data.focus_dist);
        if (mParameters.isZSLMode()) {
          QCameraPicChannel *pZSLChannel =
            (QCameraPicChannel *)m_channels[QCAMERA_CH_TYPE_ZSL];
          if (NULL != pZSLChannel) {
            //flush the zsl-buffer
            uint32_t flush_frame_idx = focus_data.focused_frame_idx;
            ALOGD("%s, flush the zsl-buffer before frame = %d.", __func__, flush_frame_idx);
            pZSLChannel->flushSuperbuffer(flush_frame_idx);
          }
        }
        ret = sendEvtNotify(CAMERA_MSG_FOCUS,
                            (focus_data.focus_state == CAM_AF_FOCUSED)? true : false,
                            0);
        // multi-touch focus feature, record current lens position when focused.
        if (mParameters.isTouchFocusing() &&
                focus_data.focus_state == CAM_AF_FOCUSED &&
                mParameters.isMultiTouchFocusSelected()) {
            mParameters.updateMTFInfo(focus_data.focus_pos);
        }
        break;
    case CAM_FOCUS_MODE_CONTINOUS_VIDEO:
    case CAM_FOCUS_MODE_CONTINOUS_PICTURE:
        if (mActiveAF &&
            (focus_data.focus_state == CAM_AF_PASSIVE_FOCUSED ||
            focus_data.focus_state == CAM_AF_PASSIVE_UNFOCUSED)) {
            //ignore passive(CAF) events during AF triggered by app/HAL
            break;
        }

        if (focus_data.focus_state == CAM_AF_PASSIVE_FOCUSED ||
            focus_data.focus_state == CAM_AF_PASSIVE_UNFOCUSED ||
            focus_data.focus_state == CAM_AF_FOCUSED ||
            focus_data.focus_state == CAM_AF_NOT_FOCUSED) {

            // update focus distance
            mParameters.updateFocusDistances(&focus_data.focus_dist);

            ret = sendEvtNotify(CAMERA_MSG_FOCUS,
                  (focus_data.focus_state == CAM_AF_PASSIVE_FOCUSED ||
                   focus_data.focus_state == CAM_AF_FOCUSED)? true : false,
                  0);
        }
        ret = sendEvtNotify(CAMERA_MSG_FOCUS_MOVE,
                (focus_data.focus_state == CAM_AF_PASSIVE_SCANNING)? true : false,
                0);
        break;
    case CAM_FOCUS_MODE_INFINITY:
    case CAM_FOCUS_MODE_FIXED:
    case CAM_FOCUS_MODE_EDOF:
    default:
        CDBG_HIGH("%s: no ops for autofocus event in focusmode %d", __func__, focusMode);
        break;
    }

    //Reset mActiveAF once we receive focus done event
    if (focus_data.focus_state == CAM_AF_FOCUSED ||
        focus_data.focus_state == CAM_AF_NOT_FOCUSED) {
        mActiveAF = false;
    }

    // we save cam_auto_focus_data_t.focus_pos to parameters,
    // in any focus mode.
    CDBG_HIGH("%s, update focus position: %d", __func__, focus_data.focus_pos);
    mParameters.updateCurrentFocusPosition(focus_data.focus_pos);

    CDBG_HIGH("%s: X",__func__);
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
 * FUNCTION   : processHDRData
 *
 * DESCRIPTION: process HDR scene events
 *
 * PARAMETERS :
 *   @hdr_scene : HDR scene event data
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::processHDRData(cam_asd_hdr_scene_data_t hdr_scene)
{
    int rc = NO_ERROR;

#ifndef VANILLA_HAL
    if (hdr_scene.is_hdr_scene &&
      (hdr_scene.hdr_confidence > HDR_CONFIDENCE_THRESHOLD) &&
      mParameters.isAutoHDREnabled()) {
        m_HDRSceneEnabled = true;
    } else {
        m_HDRSceneEnabled = false;
    }
    pthread_mutex_lock(&m_parm_lock);
    mParameters.setHDRSceneEnable(m_HDRSceneEnabled);
    pthread_mutex_unlock(&m_parm_lock);

    if ( msgTypeEnabled(CAMERA_MSG_META_DATA) ) {

        size_t data_len = sizeof(int);
        size_t buffer_len = 1 *sizeof(int)       //meta type
                          + 1 *sizeof(int)       //data len
                          + 1 *sizeof(int);      //data
        camera_memory_t *hdrBuffer = mGetMemory(-1,
                                                 buffer_len,
                                                 1,
                                                 mCallbackCookie);
        if ( NULL == hdrBuffer ) {
            ALOGE("%s: Not enough memory for auto HDR data",
                  __func__);
            return NO_MEMORY;
        }

        int *pHDRData = (int *)hdrBuffer->data;
        if (pHDRData == NULL) {
            ALOGE("%s: memory data ptr is NULL", __func__);
            return UNKNOWN_ERROR;
        }

        pHDRData[0] = CAMERA_META_DATA_HDR;
        pHDRData[1] = (int)data_len;
        pHDRData[2] = m_HDRSceneEnabled;

        qcamera_callback_argm_t cbArg;
        memset(&cbArg, 0, sizeof(qcamera_callback_argm_t));
        cbArg.cb_type = QCAMERA_DATA_CALLBACK;
        cbArg.msg_type = CAMERA_MSG_META_DATA;
        cbArg.data = hdrBuffer;
        cbArg.user_data = hdrBuffer;
        cbArg.cookie = this;
        cbArg.release_cb = releaseCameraMemory;
        rc = m_cbNotifier.notifyCallback(cbArg);
        if (rc != NO_ERROR) {
            ALOGE("%s: fail sending auto HDR notification", __func__);
            hdrBuffer->release(hdrBuffer);
        }
    }

    CDBG("%s : hdr_scene_data: processHDRData: %d %f",
          __func__,
          hdr_scene.is_hdr_scene,
          hdr_scene.hdr_confidence);

#endif
  return rc;
}

/*===========================================================================
 * FUNCTION   : transAwbMetaToParams
 *
 * DESCRIPTION: translate awb params from metadata callback to QCameraParameters
 *
 * PARAMETERS :
 *   @awb_params : awb params from metadata callback
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::transAwbMetaToParams(cam_awb_params_t &awb_params)
{
    CDBG("%s, cct value: %d", __func__, awb_params.cct_value);

    return mParameters.updateCCTValue(awb_params.cct_value);
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
        CDBG_HIGH("%s: already handled in mm-camera-intf, no ops here", __func__);
    }
    return ret;
}

/*===========================================================================
 * FUNCTION   : processASDUpdate
 *
 * DESCRIPTION: process ASD update event
 *
 * PARAMETERS :
 *   @scene: selected scene mode
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::processASDUpdate(cam_auto_scene_t scene)
{
    //set ASD parameter
    mParameters.set(QCameraParameters::KEY_SELECTED_AUTO_SCENE, mParameters.getASDStateString(scene));

    size_t data_len = sizeof(cam_auto_scene_t);
    size_t buffer_len = 1 *sizeof(int)       //meta type
                      + 1 *sizeof(int)       //data len
                      + data_len;            //data
    camera_memory_t *asdBuffer = mGetMemory(-1,
                                             buffer_len,
                                             1,
                                             mCallbackCookie);
    if ( NULL == asdBuffer ) {
        ALOGE("%s: Not enough memory for histogram data", __func__);
        return NO_MEMORY;
    }

    int *pASDData = (int *)asdBuffer->data;
    if (pASDData == NULL) {
        ALOGE("%s: memory data ptr is NULL", __func__);
        return UNKNOWN_ERROR;
    }

#ifndef VANILLA_HAL
    pASDData[0] = CAMERA_META_DATA_ASD;
    pASDData[1] = (int)data_len;
    pASDData[2] = scene;

    qcamera_callback_argm_t cbArg;
    memset(&cbArg, 0, sizeof(qcamera_callback_argm_t));
    cbArg.cb_type = QCAMERA_DATA_CALLBACK;
    cbArg.msg_type = CAMERA_MSG_META_DATA;
    cbArg.data = asdBuffer;
    cbArg.user_data = asdBuffer;
    cbArg.cookie = this;
    cbArg.release_cb = releaseCameraMemory;
    int32_t rc = m_cbNotifier.notifyCallback(cbArg);
    if (rc != NO_ERROR) {
        ALOGE("%s: fail sending notification", __func__);
        asdBuffer->release(asdBuffer);
    }
#endif
    return NO_ERROR;

}

/*===========================================================================
 * FUNCTION   : processAWBUpdate
 *
 * DESCRIPTION: process AWB update event
 *
 * PARAMETERS :
 *   @awb_params: current awb parameters from back-end.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::processAWBUpdate(cam_awb_params_t &awb_params)
{
    return transAwbMetaToParams(awb_params);
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
void QCamera2HardwareInterface::waitAPIResult(qcamera_sm_evt_enum_t api_evt,
        qcamera_api_result_t *apiResult)
{
    CDBG("%s: wait for API result of evt (%d)", __func__, api_evt);
    int resultReceived = 0;
    while  (!resultReceived) {
        pthread_cond_wait(&m_cond, &m_lock);
        if (m_apiResultList != NULL) {
            api_result_list *apiResultList = m_apiResultList;
            api_result_list *apiResultListPrevious = m_apiResultList;
            while (apiResultList != NULL) {
                if (apiResultList->result.request_api == api_evt) {
                    resultReceived = 1;
                    *apiResult = apiResultList->result;
                    apiResultListPrevious->next = apiResultList->next;
                    if (apiResultList == m_apiResultList) {
                        m_apiResultList = apiResultList->next;
                    }
                    free(apiResultList);
                    break;
                }
                else {
                    apiResultListPrevious = apiResultList;
                    apiResultList = apiResultList->next;
                }
            }
        }
    }
    CDBG("%s: return (%d) from API result wait for evt (%d)",
          __func__, apiResult->status, api_evt);
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
    api_result_list *apiResult = (api_result_list *)malloc(sizeof(api_result_list));
    if (apiResult == NULL) {
        ALOGE("%s: ERROR: malloc for api result failed", __func__);
        ALOGE("%s: ERROR: api thread will wait forever fot this lost result", __func__);
        goto malloc_failed;
    }
    apiResult->result = *result;
    apiResult->next = NULL;
    if (m_apiResultList == NULL) m_apiResultList = apiResult;
    else {
        api_result_list *apiResultList = m_apiResultList;
        while(apiResultList->next != NULL) apiResultList = apiResultList->next;
        apiResultList->next = apiResult;
    }
malloc_failed:
    pthread_cond_broadcast(&m_cond);
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

int32_t QCamera2HardwareInterface::prepareRawStream(QCameraChannel *curChannel)
{
    int32_t rc = NO_ERROR;
    cam_dimension_t str_dim,max_dim;
    QCameraChannel *pChannel;

    max_dim.width = 0;
    max_dim.height = 0;

    for (int j = 0; j < QCAMERA_CH_TYPE_MAX; j++) {
        if (m_channels[j] != NULL) {
            pChannel = m_channels[j];
            for (uint8_t i = 0; i < pChannel->getNumOfStreams(); i++) {
                QCameraStream *pStream = pChannel->getStreamByIndex(i);
                if (pStream != NULL) {
                    if (pStream->isTypeOf(CAM_STREAM_TYPE_METADATA)) {
                        continue;
                    }
                    pStream->getFrameDimension(str_dim);
                    if (str_dim.width > max_dim.width) {
                        max_dim.width = str_dim.width;
                    }
                    if (str_dim.height > max_dim.height) {
                        max_dim.height = str_dim.height;
                    }
                }
            }
        }
    }

    for (uint8_t i = 0; i < curChannel->getNumOfStreams(); i++) {
        QCameraStream *pStream = curChannel->getStreamByIndex(i);
        if (pStream != NULL) {
            if (pStream->isTypeOf(CAM_STREAM_TYPE_METADATA)) {
                continue;
            }
            pStream->getFrameDimension(str_dim);
            if (str_dim.width > max_dim.width) {
                max_dim.width = str_dim.width;
            }
            if (str_dim.height > max_dim.height) {
                max_dim.height = str_dim.height;
            }
        }
    }
    rc = mParameters.updateRAW(max_dim);
    return rc;
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

    if (streamType == CAM_STREAM_TYPE_RAW) {
        prepareRawStream(pChannel);
    }
    QCameraHeapMemory *pStreamInfo = allocateStreamInfoBuf(streamType);
    if (pStreamInfo == NULL) {
        ALOGE("%s: no mem for stream info buf", __func__);
        return NO_MEMORY;
    }
    uint8_t minStreamBufNum = getBufNumRequired(streamType);
    bool bDynAllocBuf = false;
    if (isZSLMode() && streamType == CAM_STREAM_TYPE_SNAPSHOT) {
        bDynAllocBuf = true;
    }

    if ( ( streamType == CAM_STREAM_TYPE_SNAPSHOT ||
            streamType == CAM_STREAM_TYPE_POSTVIEW ||
            streamType == CAM_STREAM_TYPE_METADATA ||
            streamType == CAM_STREAM_TYPE_RAW) &&
            !isZSLMode() &&
            !isLongshotEnabled() &&
            !mParameters.getRecordingHintValue()) {
        rc = pChannel->addStream(*this,
                pStreamInfo,
                minStreamBufNum,
                &gCamCapability[mCameraId]->padding_info,
                streamCB, userData,
                bDynAllocBuf,
                true);

        // Queue buffer allocation for Snapshot and Metadata streams
        if ( !rc ) {
            DefferWorkArgs args;
            DefferAllocBuffArgs allocArgs;

            memset(&args, 0, sizeof(DefferWorkArgs));
            memset(&allocArgs, 0, sizeof(DefferAllocBuffArgs));
            allocArgs.type = streamType;
            allocArgs.ch = pChannel;
            args.allocArgs = allocArgs;

            if (streamType == CAM_STREAM_TYPE_SNAPSHOT) {
                mSnapshotJob = queueDefferedWork(CMD_DEFF_ALLOCATE_BUFF,
                        args);

                if ( mSnapshotJob == -1) {
                    rc = UNKNOWN_ERROR;
                }
            } else if (streamType == CAM_STREAM_TYPE_METADATA) {
                mMetadataJob = queueDefferedWork(CMD_DEFF_ALLOCATE_BUFF,
                        args);

                if ( mMetadataJob == -1) {
                    rc = UNKNOWN_ERROR;
                }
            } else if (streamType == CAM_STREAM_TYPE_RAW) {
                mRawdataJob = queueDefferedWork(CMD_DEFF_ALLOCATE_BUFF,
                        args);

                if ( mRawdataJob == -1) {
                    rc = UNKNOWN_ERROR;
                }
            }
        }
    } else {
        rc = pChannel->addStream(*this,
                pStreamInfo,
                minStreamBufNum,
                &gCamCapability[mCameraId]->padding_info,
                streamCB, userData,
                bDynAllocBuf,
                false);
    }

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
        // Using the no preview torch WA it is possible
        // to already have a preview channel present before
        // start preview gets called.
        CDBG_HIGH(" %s : Preview Channel already added!", __func__);
        return NO_ERROR;
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

    bool recordingHint = mParameters.getRecordingHintValue();
    cam_dimension_t videoSize;
    mParameters.getVideoSize(&videoSize.width, &videoSize.height);
    if (recordingHint && is4k2kResolution(&videoSize)) {
        // Find and try to link a metadata stream from preview channel
        QCameraChannel *pMetaChannel = NULL;
        QCameraStream *pMetaStream = NULL;
        QCameraChannel *pVideoChannel = m_channels[QCAMERA_CH_TYPE_VIDEO];

        if (m_channels[QCAMERA_CH_TYPE_PREVIEW] != NULL) {
            pMetaChannel = m_channels[QCAMERA_CH_TYPE_PREVIEW];
            uint32_t streamNum = pMetaChannel->getNumOfStreams();
            QCameraStream *pStream = NULL;
            for (uint32_t i = 0 ; i < streamNum ; i++ ) {
                pStream = pMetaChannel->getStreamByIndex(i);
                if ((NULL != pStream) &&
                        (CAM_STREAM_TYPE_METADATA == pStream->getMyType())) {
                    pMetaStream = pStream;
                    break;
                }
            }
        }

        if ((NULL != pMetaChannel) && (NULL != pMetaStream)) {
            rc = pVideoChannel->linkStream(pMetaChannel, pMetaStream);
            if (NO_ERROR != rc) {
                ALOGE("%s : Metadata stream link failed %d", __func__, rc);
            }
        }
    }
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

    cam_dimension_t videoSize;
    mParameters.getVideoSize(&videoSize.width, &videoSize.height);
    if (!is4k2kResolution(&videoSize)) {
        // preview only channel, don't need bundle attr and cb
        rc = pChannel->init(NULL, NULL, NULL);
    } else {
        mm_camera_channel_attr_t attr;
        memset(&attr, 0, sizeof(mm_camera_channel_attr_t));
        attr.notify_mode = MM_CAMERA_SUPER_BUF_NOTIFY_BURST;
        attr.look_back = mParameters.getZSLBackLookCount();
        attr.post_frame_skip = mParameters.getZSLBurstInterval();
        attr.water_mark = mParameters.getZSLQueueDepth();
        attr.max_unmatched_frames = mParameters.getMaxUnmatchedFramesInQueue();
        rc = pChannel->init(&attr, snapshot_channel_cb_routine, this);
    }

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

    mm_camera_channel_attr_t attr;
    memset(&attr, 0, sizeof(mm_camera_channel_attr_t));
    attr.notify_mode = MM_CAMERA_SUPER_BUF_NOTIFY_CONTINUOUS;
    attr.look_back = mParameters.getZSLBackLookCount();
    attr.post_frame_skip = mParameters.getZSLBurstInterval();
    attr.water_mark = mParameters.getZSLQueueDepth();
    attr.max_unmatched_frames = mParameters.getMaxUnmatchedFramesInQueue();
    rc = pChannel->init(&attr, snapshot_channel_cb_routine, this);
    if (rc != NO_ERROR) {
        ALOGE("%s: init snapshot channel failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    rc = addStreamToChannel(pChannel, CAM_STREAM_TYPE_SNAPSHOT,
            NULL, NULL);
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
    waitDefferedWork(mMetadataJob);

    rc = addStreamToChannel(pChannel, CAM_STREAM_TYPE_RAW,
                            raw_stream_cb_routine, this);
    if (rc != NO_ERROR) {
        ALOGE("%s: add snapshot stream failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }
    waitDefferedWork(mRawdataJob);
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
    char value[PROPERTY_VALUE_MAX];
    bool raw_yuv = false;

    if (m_channels[QCAMERA_CH_TYPE_ZSL] != NULL) {
        // if we had ZSL channel before, delete it first
        delete m_channels[QCAMERA_CH_TYPE_ZSL];
        m_channels[QCAMERA_CH_TYPE_ZSL] = NULL;
    }

     if (m_channels[QCAMERA_CH_TYPE_PREVIEW] != NULL) {
        // if we had ZSL channel before, delete it first
        delete m_channels[QCAMERA_CH_TYPE_PREVIEW];
        m_channels[QCAMERA_CH_TYPE_PREVIEW] = NULL;
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

    property_get("persist.camera.raw_yuv", value, "0");
    raw_yuv = atoi(value) > 0 ? true : false;
    if ( raw_yuv ) {
        rc = addStreamToChannel(pChannel,
                                CAM_STREAM_TYPE_RAW,
                                NULL,
                                this);
        if (rc != NO_ERROR) {
            ALOGE("%s: add raw stream failed, ret = %d", __func__, rc);
            delete pChannel;
            return rc;
        }
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
    QCameraPicChannel *pChannel = NULL;
    char value[PROPERTY_VALUE_MAX];
    bool raw_yuv = false;

    if (m_channels[QCAMERA_CH_TYPE_CAPTURE] != NULL) {
        delete m_channels[QCAMERA_CH_TYPE_CAPTURE];
        m_channels[QCAMERA_CH_TYPE_CAPTURE] = NULL;
    }

    pChannel = new QCameraPicChannel(mCameraHandle->camera_handle,
                                  mCameraHandle->ops);
    if (NULL == pChannel) {
        ALOGE("%s: no mem for capture channel", __func__);
        return NO_MEMORY;
    }

    // Capture channel, only need snapshot and postview streams start together
    mm_camera_channel_attr_t attr;
    memset(&attr, 0, sizeof(mm_camera_channel_attr_t));
    if ( mLongshotEnabled ) {
        attr.notify_mode = MM_CAMERA_SUPER_BUF_NOTIFY_BURST;
        attr.look_back = mParameters.getZSLBackLookCount();
        attr.water_mark = mParameters.getZSLQueueDepth();
    } else {
        attr.notify_mode = MM_CAMERA_SUPER_BUF_NOTIFY_CONTINUOUS;
    }
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

    if (!mLongshotEnabled) {
        rc = addStreamToChannel(pChannel, CAM_STREAM_TYPE_POSTVIEW,
                                NULL, this);

        if (rc != NO_ERROR) {
            ALOGE("%s: add postview stream failed, ret = %d", __func__, rc);
            delete pChannel;
            return rc;
        }
    } else {
        rc = addStreamToChannel(pChannel, CAM_STREAM_TYPE_PREVIEW,
                                preview_stream_cb_routine, this);

      if (rc != NO_ERROR) {
          ALOGE("%s: add preview stream failed, ret = %d", __func__, rc);
          delete pChannel;
          return rc;
      }
    }

    rc = addStreamToChannel(pChannel, CAM_STREAM_TYPE_SNAPSHOT,
                            NULL, this);
    if (rc != NO_ERROR) {
        ALOGE("%s: add snapshot stream failed, ret = %d", __func__, rc);
        delete pChannel;
        return rc;
    }

    property_get("persist.camera.raw_yuv", value, "0");
    raw_yuv = atoi(value) > 0 ? true : false;
    if ( raw_yuv ) {
        rc = addStreamToChannel(pChannel,
                                CAM_STREAM_TYPE_RAW,
                                snapshot_raw_stream_cb_routine,
                                this);
        if (rc != NO_ERROR) {
            ALOGE("%s: add raw stream failed, ret = %d", __func__, rc);
            delete pChannel;
            return rc;
        }
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
 * FUNCTION   : addReprocChannel
 *
 * DESCRIPTION: add a reprocess channel that will do reprocess on frames
 *              coming from input channel
 *
 * PARAMETERS :
 *   @pInputChannel : ptr to input channel whose frames will be post-processed
 *
 * RETURN     : Ptr to the newly created channel obj. NULL if failed.
 *==========================================================================*/
QCameraReprocessChannel *QCamera2HardwareInterface::addReprocChannel(
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

    CDBG_HIGH("%s: Before pproc config check, ret = %x", __func__, gCamCapability[mCameraId]->min_required_pp_mask);

    // pp feature config
    cam_pp_feature_config_t pp_config;
    uint32_t required_mask = gCamCapability[mCameraId]->min_required_pp_mask;
    memset(&pp_config, 0, sizeof(cam_pp_feature_config_t));
    if (mParameters.isZSLMode() || (required_mask & CAM_QCOM_FEATURE_CPP)) {
        if (gCamCapability[mCameraId]->min_required_pp_mask & CAM_QCOM_FEATURE_EFFECT) {
            pp_config.feature_mask |= CAM_QCOM_FEATURE_EFFECT;
            pp_config.effect = mParameters.getEffectValue();
        }
        if ((gCamCapability[mCameraId]->min_required_pp_mask & CAM_QCOM_FEATURE_SHARPNESS) &&
                !mParameters.isOptiZoomEnabled()) {
            pp_config.feature_mask |= CAM_QCOM_FEATURE_SHARPNESS;
            pp_config.sharpness = mParameters.getInt(QCameraParameters::KEY_QC_SHARPNESS);
        }

        if (gCamCapability[mCameraId]->min_required_pp_mask & CAM_QCOM_FEATURE_CROP) {
            pp_config.feature_mask |= CAM_QCOM_FEATURE_CROP;
        }

        if (mParameters.isWNREnabled()) {
            pp_config.feature_mask |= CAM_QCOM_FEATURE_DENOISE2D;
            pp_config.denoise2d.denoise_enable = 1;
            pp_config.denoise2d.process_plates = mParameters.getWaveletDenoiseProcessPlate();
        }

        if (required_mask & CAM_QCOM_FEATURE_CPP) {
            pp_config.feature_mask |= CAM_QCOM_FEATURE_CPP;
        }

    }

    if (isCACEnabled()) {
        pp_config.feature_mask |= CAM_QCOM_FEATURE_CAC;
    }

    if (needRotationReprocess()) {
        pp_config.feature_mask |= CAM_QCOM_FEATURE_CPP;
        uint32_t rotation = getJpegRotation();
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

    uint8_t minStreamBufNum = getBufNumRequired(CAM_STREAM_TYPE_OFFLINE_PROC);

    if (mParameters.isHDREnabled()){
        pp_config.feature_mask |= CAM_QCOM_FEATURE_HDR;
        pp_config.hdr_param.hdr_enable = 1;
        pp_config.hdr_param.hdr_need_1x = mParameters.isHDR1xFrameEnabled();
        pp_config.hdr_param.hdr_mode = CAM_HDR_MODE_MULTIFRAME;
    } else {
        pp_config.feature_mask &= ~CAM_QCOM_FEATURE_HDR;
        pp_config.hdr_param.hdr_enable = 0;
    }

    if(needScaleReprocess()){
        pp_config.feature_mask |= CAM_QCOM_FEATURE_SCALE;
        mParameters.m_reprocScaleParam.getPicSizeFromAPK(
              pp_config.scale_param.output_width, pp_config.scale_param.output_height);
    }

    CDBG_HIGH("%s: After pproc config check, ret = %x", __func__, pp_config.feature_mask);

    if(mParameters.isUbiFocusEnabled()) {
        pp_config.feature_mask |= CAM_QCOM_FEATURE_UBIFOCUS;
    } else {
        pp_config.feature_mask &= ~CAM_QCOM_FEATURE_UBIFOCUS;
    }

    if(mParameters.isMultiTouchFocusEnabled()) {
        pp_config.feature_mask |= CAM_QCOM_FEATURE_MULTI_TOUCH_FOCUS;
    } else {
        pp_config.feature_mask &= ~CAM_QCOM_FEATURE_MULTI_TOUCH_FOCUS;
    }

    if(mParameters.isChromaFlashEnabled()) {
        pp_config.feature_mask |= CAM_QCOM_FEATURE_CHROMA_FLASH;
        //TODO: check flash value for captured image, then assign.
        pp_config.flash_value = CAM_FLASH_ON;
    } else {
        pp_config.feature_mask &= ~CAM_QCOM_FEATURE_CHROMA_FLASH;
    }

    if(mParameters.isOptiZoomEnabled()) {
        pp_config.feature_mask |= CAM_QCOM_FEATURE_OPTIZOOM;
        pp_config.zoom_level =
                (uint8_t) mParameters.getInt(CameraParameters::KEY_ZOOM);
    } else {
        pp_config.feature_mask &= ~CAM_QCOM_FEATURE_OPTIZOOM;
    }

    if (mParameters.isTruePortraitEnabled()) {
        pp_config.feature_mask |= CAM_QCOM_FEATURE_TRUEPORTRAIT;
        pp_config.tp_param.enable = mParameters.isTruePortraitEnabled();
        pp_config.tp_param.meta_max_size = mParameters.TpMaxMetaSize();
    } else {
        pp_config.feature_mask &= ~CAM_QCOM_FEATURE_TRUEPORTRAIT;
        pp_config.tp_param.enable = 0;
    }

    //WNR and HDR happen inline. No extra buffers needed.
    uint32_t temp_feature_mask = pp_config.feature_mask;
    temp_feature_mask &= ~CAM_QCOM_FEATURE_DENOISE2D;
    temp_feature_mask &= ~CAM_QCOM_FEATURE_HDR;
    if (temp_feature_mask && mParameters.isHDREnabled()) {
        minStreamBufNum = (uint8_t)(1 + mParameters.getNumOfExtraHDRInBufsIfNeeded());
    }

    // Add non inplace image lib buffers only when ppproc is present,
    // becuase pproc is non inplace and input buffers for img lib
    // are output for pproc and this number of extra buffers is required
    // If pproc is not there, input buffers for imglib are from snapshot stream
    uint8_t imglib_extra_bufs = mParameters.getNumOfExtraBuffersForImageProc();
    if (temp_feature_mask && imglib_extra_bufs) {
        // 1 is added because getNumOfExtraBuffersForImageProc returns extra
        // buffers assuming number of capture is already added
        minStreamBufNum = (uint8_t)(minStreamBufNum + imglib_extra_bufs + 1);
    }

    CDBG_HIGH("%s: Allocating %d reproc buffers",__func__,minStreamBufNum);

    bool offlineReproc = isRegularCapture();
    rc = pChannel->addReprocStreamsFromSource(*this,
                                              pp_config,
                                              pInputChannel,
                                              minStreamBufNum,
                                              mParameters.getNumOfSnapshots(),
                                              &gCamCapability[mCameraId]->padding_info,
                                              mParameters,
                                              mLongshotEnabled,
                                              offlineReproc);
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
                             stream_cb, userdata, false);

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
 * FUNCTION   : addDualReprocChannel
 *
 * DESCRIPTION: add a second reprocess channel that will do reprocess on frames
 *              coming from another reproc channel
 *
 * PARAMETERS :
 *   @pInputChannel : ptr to input channel whose frames will be post-processed
 *
 * RETURN     : Ptr to the newly created channel obj. NULL if failed.
 *==========================================================================*/
QCameraReprocessChannel *QCamera2HardwareInterface::addDualReprocChannel(
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
                        dual_reproc_channel_cb_routine,
                        this);
    if (rc != NO_ERROR) {
        ALOGE("%s: init reprocess channel failed, ret = %d", __func__, rc);
        delete pChannel;
        return NULL;
    }

    // pp feature config
    cam_pp_feature_config_t pp_config;
    memset(&pp_config, 0, sizeof(cam_pp_feature_config_t));

    if(mParameters.isfssrEnabled()) {
        pp_config.feature_mask |= CAM_QCOM_FEATURE_FSSR;
        pp_config.zoom_level =
                (uint8_t) mParameters.getInt(CameraParameters::KEY_ZOOM);
    } else {
        pp_config.feature_mask &= ~CAM_QCOM_FEATURE_FSSR;
    }

    uint8_t minStreamBufNum = getBufNumRequired(CAM_STREAM_TYPE_OFFLINE_PROC);;

    CDBG_HIGH("%s: Allocating %d dual reproc buffers",__func__,minStreamBufNum);

    bool offlineReproc = isRegularCapture();
    rc = pChannel->addReprocStreamsFromSource(*this,
                                              pp_config,
                                              pInputChannel,
                                              minStreamBufNum,
                                              mParameters.getNumOfSnapshots(),
                                              &gCamCapability[mCameraId]->padding_info,
                                              mParameters,
                                              mLongshotEnabled,
                                              offlineReproc);
    if (rc != NO_ERROR) {
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
 *   @destroy : delete context as well
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::delChannel(qcamera_ch_type_enum_t ch_type,
                                              bool destroy)
{
    if (m_channels[ch_type] != NULL) {
        if (destroy) {
            delete m_channels[ch_type];
            m_channels[ch_type] = NULL;
        } else {
            m_channels[ch_type]->deleteChannel();
        }
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
        rc = m_channels[ch_type]->config();
        if (NO_ERROR == rc) {
            rc = m_channels[ch_type]->start();
        }
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
    ATRACE_CALL();
    int32_t rc = NO_ERROR;

    if (mParameters.isZSLMode() && mParameters.getRecordingHintValue() !=true) {
        rc = addChannel(QCAMERA_CH_TYPE_ZSL);
        if (rc != NO_ERROR) {
            return rc;
        }
    } else {
        bool recordingHint = mParameters.getRecordingHintValue();
        if(recordingHint) {
            cam_dimension_t videoSize;
            mParameters.getVideoSize(&videoSize.width, &videoSize.height);
            if (!is4k2kResolution(&videoSize)) {
               rc = addChannel(QCAMERA_CH_TYPE_SNAPSHOT);
               if (rc != NO_ERROR) {
                   return rc;
               }
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

        if (!recordingHint) {
            waitDefferedWork(mMetadataJob);
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
    delChannel(QCAMERA_CH_TYPE_ZSL);
    delChannel(QCAMERA_CH_TYPE_PREVIEW);
    delChannel(QCAMERA_CH_TYPE_VIDEO);
    delChannel(QCAMERA_CH_TYPE_SNAPSHOT);
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
         CDBG("%s: shutter msg not enabled or NULL cb", __func__);
         return;
     }
     CDBG_HIGH("%s: CAMERA_MSG_SHUTTER ", __func__);
     qcamera_callback_argm_t cbArg;
     memset(&cbArg, 0, sizeof(qcamera_callback_argm_t));
     cbArg.cb_type = QCAMERA_NOTIFY_CALLBACK;
     cbArg.msg_type = CAMERA_MSG_SHUTTER;
     cbArg.ext1 = 0;
     cbArg.ext2 = false;
     m_cbNotifier.notifyCallback(cbArg);
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
 * FUNCTION   : needPreviewFDCallback
 *
 * DESCRIPTION: decides if needPreviewFDCallback
 *
 * PARAMETERS :
 *   @fd_data : number of faces
 *
 * RETURN     : bool type of status
 *              true  -- success
 *              fale -- failure code
 *==========================================================================*/
bool QCamera2HardwareInterface::needPreviewFDCallback(uint8_t num_faces)
{
    if (num_faces == 0 && mNumPreviewFaces == 0) {
        return false;
    }

    return true;
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
        CDBG_HIGH("%s: FaceDetection not enabled, no ops here", __func__);
        return NO_ERROR;
    }

    qcamera_face_detect_type_t fd_type = fd_data->fd_type;
    if ((NULL == mDataCb) ||
        (fd_type == QCAMERA_FD_PREVIEW && (!msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA) ||
        (!needPreviewFDCallback(fd_data->num_faces_detected))))
#ifndef VANILLA_HAL
        || (fd_type == QCAMERA_FD_SNAPSHOT && !msgTypeEnabled(CAMERA_MSG_META_DATA))
#endif
        ) {
        CDBG_HIGH("%s: metadata msgtype not enabled, no ops here", __func__);
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
    // need separate face detection in preview or snapshot type
    size_t faceResultSize = 0;
    size_t data_len = 0;
    if(fd_type == QCAMERA_FD_PREVIEW){
        //fd for preview frames
        faceResultSize = sizeof(camera_frame_metadata_t);
        faceResultSize += sizeof(camera_face_t) * MAX_ROI;
    }else if(fd_type == QCAMERA_FD_SNAPSHOT){
        // fd for snapshot frames
        //check if face is detected in this frame
        if(fd_data->num_faces_detected > 0){
            data_len = sizeof(camera_frame_metadata_t) +
                         sizeof(camera_face_t) * fd_data->num_faces_detected;
        }else{
            //no face
            data_len = 0;
        }
        faceResultSize = 1 *sizeof(int)    //meta data type
                       + 1 *sizeof(int)    // meta data len
                       + data_len;         //data
    }

    camera_memory_t *faceResultBuffer = mGetMemory(-1,
                                                   faceResultSize,
                                                   1,
                                                   mCallbackCookie);
    if ( NULL == faceResultBuffer ) {
        ALOGE("%s: Not enough memory for face result data",
              __func__);
        return NO_MEMORY;
    }

    unsigned char *pFaceResult = ( unsigned char * ) faceResultBuffer->data;
    memset(pFaceResult, 0, faceResultSize);
    unsigned char *faceData = NULL;
    if(fd_type == QCAMERA_FD_PREVIEW){
        faceData = pFaceResult;
        mNumPreviewFaces = fd_data->num_faces_detected;
    }else if(fd_type == QCAMERA_FD_SNAPSHOT){
#ifndef VANILLA_HAL
        //need fill meta type and meta data len first
        int *data_header = (int* )pFaceResult;
        data_header[0] = CAMERA_META_DATA_FD;
        data_header[1] = (int)data_len;

        if(data_len <= 0){
            //if face is not valid or do not have face, return
            qcamera_callback_argm_t cbArg;
            memset(&cbArg, 0, sizeof(qcamera_callback_argm_t));
            cbArg.cb_type = QCAMERA_DATA_CALLBACK;
            cbArg.msg_type = CAMERA_MSG_META_DATA;
            cbArg.data = faceResultBuffer;
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
#endif
        faceData = pFaceResult + 2 *sizeof(int); //skip two int length
    }

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

#ifndef VANILLA_HAL
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
#endif

        }
    }

    qcamera_callback_argm_t cbArg;
    memset(&cbArg, 0, sizeof(qcamera_callback_argm_t));
    cbArg.cb_type = QCAMERA_DATA_CALLBACK;
    if(fd_type == QCAMERA_FD_PREVIEW){
        cbArg.msg_type = CAMERA_MSG_PREVIEW_METADATA;
    }
#ifndef VANILLA_HAL
    else if(fd_type == QCAMERA_FD_SNAPSHOT){
        cbArg.msg_type = CAMERA_MSG_META_DATA;
    }
#endif
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
 *   @cbStatus: callback status
 *
 * RETURN     : None
 *==========================================================================*/
void QCamera2HardwareInterface::releaseCameraMemory(void *data,
                                                    void */*cookie*/,
                                                    int32_t /*cbStatus*/)
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
 *   @cbStatus: callback status
 *
 * RETURN     : None
 *==========================================================================*/
void QCamera2HardwareInterface::returnStreamBuffer(void *data,
                                                   void *cookie,
                                                   int32_t /*cbStatus*/)
{
    QCameraStream *stream = ( QCameraStream * ) cookie;
    int idx = *((int *)data);
    if ((NULL != stream) && (0 <= idx)) {
        stream->bufDone((uint32_t)idx);
    } else {
        ALOGE("%s: Cannot return buffer %d %p", __func__, idx, cookie);
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
#ifndef VANILLA_HAL
    if (!mParameters.isHistogramEnabled()) {
        CDBG("%s: Histogram not enabled, no ops here", __func__);
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
#endif
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : calcThermalLevel
 *
 * DESCRIPTION: Calculates the target fps range depending on
 *              the thermal level.
 *
 * PARAMETERS :
 *   @level    : received thermal level
 *   @minFPS   : minimum configured fps range
 *   @maxFPS   : maximum configured fps range
 *   @adjustedRange : target fps range
 *   @skipPattern : target skip pattern
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::calcThermalLevel(
            qcamera_thermal_level_enum_t level,
            const int minFPSi,
            const int maxFPSi,
            cam_fps_range_t &adjustedRange,
            enum msm_vfe_frame_skip_pattern &skipPattern)
{
    const float minFPS = (float)minFPSi;
    const float maxFPS = (float)maxFPSi;

    // Initialize video fps to preview fps
    float minVideoFps = minFPS, maxVideoFps = maxFPS;
    cam_fps_range_t videoFps;
    // If HFR mode, update video fps accordingly
    if(isHFRMode()) {
        mParameters.getHfrFps(videoFps);
        minVideoFps = videoFps.video_min_fps;
        maxVideoFps = videoFps.video_max_fps;
    }

    CDBG_HIGH("%s: level: %d, preview minfps %f, preview maxfpS %f, "
              "video minfps %f, video maxfpS %f",
            __func__, level, minFPS, maxFPS, minVideoFps, maxVideoFps);

    switch(level) {
    case QCAMERA_THERMAL_NO_ADJUSTMENT:
        {
            adjustedRange.min_fps = minFPS / 1000.0f;
            adjustedRange.max_fps = maxFPS / 1000.0f;
            adjustedRange.video_min_fps = minVideoFps / 1000.0f;
            adjustedRange.video_max_fps = maxVideoFps / 1000.0f;
            skipPattern = NO_SKIP;
        }
        break;
    case QCAMERA_THERMAL_SLIGHT_ADJUSTMENT:
        {
            adjustedRange.min_fps = (minFPS / 2) / 1000.0f;
            adjustedRange.max_fps = (maxFPS / 2) / 1000.0f;
            adjustedRange.video_min_fps = (minVideoFps / 2) / 1000.0f;
            adjustedRange.video_max_fps = (maxVideoFps / 2 ) / 1000.0f;
            if ( adjustedRange.min_fps < 1 ) {
                adjustedRange.min_fps = 1;
            }
            if ( adjustedRange.max_fps < 1 ) {
                adjustedRange.max_fps = 1;
            }
            if ( adjustedRange.video_min_fps < 1 ) {
                adjustedRange.video_min_fps = 1;
            }
            if ( adjustedRange.video_max_fps < 1 ) {
                adjustedRange.video_max_fps = 1;
            }
            skipPattern = EVERY_2FRAME;
        }
        break;
    case QCAMERA_THERMAL_BIG_ADJUSTMENT:
        {
            adjustedRange.min_fps = (minFPS / 4) / 1000.0f;
            adjustedRange.max_fps = (maxFPS / 4) / 1000.0f;
            adjustedRange.video_min_fps = (minVideoFps / 4) / 1000.0f;
            adjustedRange.video_max_fps = (maxVideoFps / 4 ) / 1000.0f;
            if ( adjustedRange.min_fps < 1 ) {
                adjustedRange.min_fps = 1;
            }
            if ( adjustedRange.max_fps < 1 ) {
                adjustedRange.max_fps = 1;
            }
            if ( adjustedRange.video_min_fps < 1 ) {
                adjustedRange.video_min_fps = 1;
            }
            if ( adjustedRange.video_max_fps < 1 ) {
                adjustedRange.video_max_fps = 1;
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
            for (size_t i = 0 ; i < gCamCapability[mCameraId]->fps_ranges_tbl_cnt ; i++) {
                if (gCamCapability[mCameraId]->fps_ranges_tbl[i].min_fps < adjustedRange.min_fps) {
                    adjustedRange.min_fps = gCamCapability[mCameraId]->fps_ranges_tbl[i].min_fps;
                    adjustedRange.max_fps = adjustedRange.min_fps;
                }
            }
            skipPattern = MAX_SKIP;
            adjustedRange.video_min_fps = adjustedRange.min_fps;
            adjustedRange.video_max_fps = adjustedRange.max_fps;
        }
        break;
    default:
        {
            CDBG("%s: Invalid thermal level %d", __func__, level);
            return BAD_VALUE;
        }
        break;
    }
    CDBG_HIGH("%s: Thermal level %d, FPS [%3.2f,%3.2f, %3.2f,%3.2f], frameskip %d",
          __func__, level, adjustedRange.min_fps, adjustedRange.max_fps,
          adjustedRange.video_min_fps, adjustedRange.video_max_fps,skipPattern);

    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : recalcFPSRange
 *
 * DESCRIPTION: adjust the configured fps range regarding
 *              the last thermal level.
 *
 * PARAMETERS :
 *   @minFPS   : minimum configured fps range
 *   @maxFPS   : maximum configured fps range
 *   @adjustedRange : target fps range
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCamera2HardwareInterface::recalcFPSRange(int &minFPS, int &maxFPS,
        cam_fps_range_t &adjustedRange)
{
    enum msm_vfe_frame_skip_pattern skipPattern;
    calcThermalLevel(mThermalLevel,
                     minFPS,
                     maxFPS,
                     adjustedRange,
                     skipPattern);
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
    enum msm_vfe_frame_skip_pattern skipPattern;

    pthread_mutex_lock(&m_parm_lock);

    if (!mCameraOpened) {
        CDBG("%s: Camera is not opened, no need to update camera parameters", __func__);
        pthread_mutex_unlock(&m_parm_lock);
        return NO_ERROR;
    }

    mParameters.getPreviewFpsRange(&minFPS, &maxFPS);
    qcamera_thermal_mode thermalMode = mParameters.getThermalMode();
    calcThermalLevel(level, minFPS, maxFPS, adjustedRange, skipPattern);
    mThermalLevel = level;

    if (thermalMode == QCAMERA_THERMAL_ADJUST_FPS)
        ret = mParameters.adjustPreviewFpsRange(&adjustedRange);
    else if (thermalMode == QCAMERA_THERMAL_ADJUST_FRAMESKIP)
        ret = mParameters.setFrameSkip(skipPattern);
    else
        ALOGE("%s: Incorrect thermal mode %d", __func__, thermalMode);

    pthread_mutex_unlock(&m_parm_lock);

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
    int rc = NO_ERROR;
    pthread_mutex_lock(&m_parm_lock);
    String8 str = String8(parms);
    QCameraParameters param(str);
    rc =  mParameters.updateParameters(param, needRestart);

    // update stream based parameter settings
    for (int i = 0; i < QCAMERA_CH_TYPE_MAX; i++) {
        if (m_channels[i] != NULL) {
            m_channels[i]->UpdateStreamBasedParameters(mParameters);
        }
    }
    pthread_mutex_unlock(&m_parm_lock);

    return rc;
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
    int rc = NO_ERROR;
    pthread_mutex_lock(&m_parm_lock);
    rc = mParameters.commitParameters();
    if (rc == NO_ERROR) {
        // update number of snapshot based on committed parameters setting
        rc = mParameters.setNumOfSnapshot();
    }
    pthread_mutex_unlock(&m_parm_lock);
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
    bool needFps = false;
    pthread_mutex_lock(&m_parm_lock);
    needFps = mParameters.isFpsDebugEnabled();
    pthread_mutex_unlock(&m_parm_lock);
    return needFps;
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
 * FUNCTION   : is4k2kResolution
 *
 * DESCRIPTION: if resolution is 4k x 2k or true 4k x 2k
 *
 * PARAMETERS : none
 *
 * RETURN     : true: needed
 *              false: no need
 *==========================================================================*/
bool QCamera2HardwareInterface::is4k2kResolution(cam_dimension_t* resolution)
{
   bool enabled = false;
   if ((resolution->width == 4096 && resolution->height == 2160) ||
       (resolution->width == 3840 && resolution->height == 2160) ) {
      enabled = true;
   }
   return enabled;
}


/*===========================================================================
 *
 * FUNCTION   : isPreviewRestartEnabled
 *
 * DESCRIPTION: Check whether preview should be restarted automatically
 *              during image capture.
 *
 * PARAMETERS : none
 *
 * RETURN     : true: needed
 *              false: no need
 *==========================================================================*/
bool QCamera2HardwareInterface::isPreviewRestartEnabled()
{
    char prop[PROPERTY_VALUE_MAX];
    memset(prop, 0, sizeof(prop));
    property_get("persist.camera.feature.restart", prop, "0");
    int earlyRestart = atoi(prop);
    return earlyRestart == 1;
}

/*===========================================================================
=======
 * FUNCTION   : isAFRunning
 *
 * DESCRIPTION: if AF is in progress while in Auto/Macro focus modes
 *
 * PARAMETERS : none
 *
 * RETURN     : true: AF in progress
 *              false: AF not in progress
 *==========================================================================*/
bool QCamera2HardwareInterface::isAFRunning()
{
    bool isAFInProgress = (m_currentFocusState == CAM_AF_SCANNING &&
            (mParameters.getFocusMode() == CAM_FOCUS_MODE_AUTO ||
            mParameters.getFocusMode() == CAM_FOCUS_MODE_MACRO));

    return isAFInProgress;
}

bool QCamera2HardwareInterface::needDualReprocess()
{
    bool ret = false;
    pthread_mutex_lock(&m_parm_lock);
    if (mParameters.isfssrEnabled()) {
        CDBG_HIGH("%s: need do reprocess for FSSR", __func__);
        ret = true;
    }
    pthread_mutex_unlock(&m_parm_lock);
    return ret;
}

/*===========================================================================

>>>>>>> c709c9a... Camera: Block CancelAF till HAL receives AF event.
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
    pthread_mutex_lock(&m_parm_lock);
    if (!mParameters.isJpegPictureFormat() &&
        !mParameters.isNV21PictureFormat()) {
        // RAW image, no need to reprocess
        pthread_mutex_unlock(&m_parm_lock);
        return false;
    }

    if (mParameters.isHDREnabled()) {
        CDBG_HIGH("%s: need do reprocess for HDR", __func__);
        pthread_mutex_unlock(&m_parm_lock);
        return true;
    }

    uint32_t feature_mask = 0;
    uint32_t required_mask = 0;
    feature_mask = gCamCapability[mCameraId]->qcom_supported_feature_mask;
    required_mask = gCamCapability[mCameraId]->min_required_pp_mask;
    if (((feature_mask & CAM_QCOM_FEATURE_CPP) > 0) &&
        (getJpegRotation() > 0)) {
            // current rotation is not zero, and pp has the capability to process rotation
            CDBG_HIGH("%s: need to do reprocess for rotation=%d", __func__, getJpegRotation());
            pthread_mutex_unlock(&m_parm_lock);
            return true;
    }

    if (isZSLMode()) {
        if (((gCamCapability[mCameraId]->min_required_pp_mask > 0) ||
             mParameters.isWNREnabled() || isCACEnabled())) {
            // TODO: add for ZSL HDR later
            CDBG_HIGH("%s: need do reprocess for ZSL WNR or min PP reprocess", __func__);
            pthread_mutex_unlock(&m_parm_lock);
            return true;
        }

        int snapshot_flipMode =
            mParameters.getFlipMode(CAM_STREAM_TYPE_SNAPSHOT);
        if (snapshot_flipMode > 0) {
            CDBG_HIGH("%s: Need do flip for snapshot in ZSL mode", __func__);
            pthread_mutex_unlock(&m_parm_lock);
            return true;
        }
    } else {
        if (required_mask & CAM_QCOM_FEATURE_CPP) {
            CDBG_HIGH("%s: Need CPP in non-ZSL mode", __func__);
            pthread_mutex_unlock(&m_parm_lock);
            return true;
        }
    }

    if ((gCamCapability[mCameraId]->qcom_supported_feature_mask & CAM_QCOM_FEATURE_SCALE) > 0 &&
        mParameters.m_reprocScaleParam.isScaleEnabled() &&
        mParameters.m_reprocScaleParam.isUnderScaling()) {
        // Reproc Scale is enaled and also need Scaling to current Snapshot
        CDBG_HIGH("%s: need do reprocess for scale", __func__);
        pthread_mutex_unlock(&m_parm_lock);
        return true;
    }

    if (mParameters.isUbiFocusEnabled() |
        mParameters.isMultiTouchFocusEnabled() |
        mParameters.isChromaFlashEnabled() |
        mParameters.isHDREnabled() |
        mParameters.isfssrEnabled() |
        mParameters.isOptiZoomEnabled()) {
        CDBG_HIGH("%s: need reprocess for |UbiFocus=%d|ChramaFlash=%d"
                  "|OptiZoom=%d|fssr=%d|MultiTouchFocus=%d",__func__,
                  mParameters.isUbiFocusEnabled(),
                  mParameters.isChromaFlashEnabled(),
                  mParameters.isOptiZoomEnabled(),
                  mParameters.isfssrEnabled(),
                  mParameters.isMultiTouchFocusEnabled());
        pthread_mutex_unlock(&m_parm_lock);
        return true;
    }

    pthread_mutex_unlock(&m_parm_lock);
    return false;
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
    pthread_mutex_lock(&m_parm_lock);
    if (!mParameters.isJpegPictureFormat() &&
        !mParameters.isNV21PictureFormat()) {
        // RAW image, no need to reprocess
        pthread_mutex_unlock(&m_parm_lock);
        return false;
    }

    uint32_t feature_mask = 0;
    feature_mask = gCamCapability[mCameraId]->qcom_supported_feature_mask;
    if (((feature_mask & CAM_QCOM_FEATURE_CPP) > 0) &&
        (getJpegRotation() > 0)) {
        // current rotation is not zero
        // and pp has the capability to process rotation
        CDBG_HIGH("%s: need to do reprocess for rotation=%d",
              __func__,
              getJpegRotation());
        pthread_mutex_unlock(&m_parm_lock);
        return true;
    }

    pthread_mutex_unlock(&m_parm_lock);
    return false;
}

/*===========================================================================
 * FUNCTION   : needScaleReprocess
 *
 * DESCRIPTION: if scale needs to be done by reprocess in pp
 *
 * PARAMETERS : none
 *
 * RETURN     : true: needed
 *              false: no need
 *==========================================================================*/
bool QCamera2HardwareInterface::needScaleReprocess()
{
    pthread_mutex_lock(&m_parm_lock);
    if (!mParameters.isJpegPictureFormat() &&
        !mParameters.isNV21PictureFormat()) {
        // RAW image, no need to reprocess
        pthread_mutex_unlock(&m_parm_lock);
        return false;
    }

    if ((gCamCapability[mCameraId]->qcom_supported_feature_mask & CAM_QCOM_FEATURE_SCALE) > 0 &&
        mParameters.m_reprocScaleParam.isScaleEnabled() &&
        mParameters.m_reprocScaleParam.isUnderScaling()) {
        // Reproc Scale is enaled and also need Scaling to current Snapshot
        CDBG_HIGH("%s: need do reprocess for scale", __func__);
        pthread_mutex_unlock(&m_parm_lock);
        return true;
    }

    pthread_mutex_unlock(&m_parm_lock);
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
    pthread_mutex_lock(&m_parm_lock);
    mParameters.getThumbnailSize(&dim.width, &dim.height);
    pthread_mutex_unlock(&m_parm_lock);
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
uint32_t QCamera2HardwareInterface::getJpegQuality()
{
    uint32_t quality = 0;
    pthread_mutex_lock(&m_parm_lock);
    quality =  mParameters.getJpegQuality();
    pthread_mutex_unlock(&m_parm_lock);
    return quality;
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
uint32_t QCamera2HardwareInterface::getJpegRotation() {
    return mCaptureRotation;
}

/*===========================================================================
 * FUNCTION   : getOrientation
 *
 * DESCRIPTION: get rotation information from camera parameters
 *
 * PARAMETERS : none
 *
 * RETURN     : rotation information
 *==========================================================================*/
void QCamera2HardwareInterface::getOrientation() {
    pthread_mutex_lock(&m_parm_lock);
    mCaptureRotation = mParameters.getJpegRotation();
    pthread_mutex_unlock(&m_parm_lock);
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

    pthread_mutex_lock(&m_parm_lock);

    //set flash value
    mFlash = mParameters.getFlashValue();
    mRedEye = mParameters.getRedEyeValue();
    mFlashPresence = mParameters.getSupportedFlashModes();

    // add exif entries
    String8 dateTime;
    rc = mParameters.getExifDateTime(dateTime);
    if(rc == NO_ERROR) {
        exif->addEntry(EXIFTAGID_EXIF_DATE_TIME_ORIGINAL, EXIF_ASCII,
                (uint32_t)(dateTime.length() + 1), (void *)dateTime.string());
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
    if (getSensorType() != CAM_SENSOR_YUV) {
        exif->addEntry(EXIFTAGID_ISO_SPEED_RATING,
                       EXIF_SHORT,
                       1,
                       (void *)&(isoSpeed));
    }

    char gpsProcessingMethod[EXIF_ASCII_PREFIX_SIZE + GPS_PROCESSING_METHOD_SIZE];
    uint32_t count = 0;
    rc = mParameters.getExifGpsProcessingMethod(gpsProcessingMethod, count);
    if(rc == NO_ERROR) {
        exif->addEntry(EXIFTAGID_GPS_PROCESSINGMETHOD,
                       EXIF_ASCII,
                       count,
                       (void *)gpsProcessingMethod);
    } else {
        CDBG("%s: getExifGpsProcessingMethod failed", __func__);
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
        CDBG("%s: getExifLatitude failed", __func__);
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
        CDBG("%s: getExifLongitude failed", __func__);
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
        CDBG("%s: getExifAltitude failed", __func__);
    }

    char gpsDateStamp[20];
    rat_t gpsTimeStamp[3];
    rc = mParameters.getExifGpsDateTimeStamp(gpsDateStamp, 20, gpsTimeStamp);
    if(rc == NO_ERROR) {
        exif->addEntry(EXIFTAGID_GPS_DATESTAMP,
                       EXIF_ASCII,
                       (uint32_t)(strlen(gpsDateStamp) + 1),
                       (void *)gpsDateStamp);

        exif->addEntry(EXIFTAGID_GPS_TIMESTAMP,
                       EXIF_RATIONAL,
                       3,
                       (void *)gpsTimeStamp);
    } else {
        ALOGE("%s: getExifGpsDataTimeStamp failed", __func__);
    }

    pthread_mutex_unlock(&m_parm_lock);

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
 * FUNCTION   : needProcessPreviewFrame
 *
 * DESCRIPTION: returns whether preview frame need to be displayed
 *
 * PARAMETERS :
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
bool QCamera2HardwareInterface::needProcessPreviewFrame()
{
    return m_stateMachine.isPreviewRunning()
            && mParameters.isDisplayFrameNeeded();
};

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
    ATRACE_CALL();
    CDBG_HIGH("[KPI Perf] %s: Prepare hardware such as LED",__func__);
    return mCameraHandle->ops->prepare_snapshot(mCameraHandle->camera_handle,
                                                afNeeded);
}

/*===========================================================================
 * FUNCTION   : needFDMetadata
 *
 * DESCRIPTION: check whether we need process Face Detection metadata in this chanel
 *
 * PARAMETERS :
 *   @channel_type: channel type
 *
  * RETURN     : true: needed
 *              false: no need
 *==========================================================================*/
bool QCamera2HardwareInterface::needFDMetadata(qcamera_ch_type_enum_t channel_type)
{
    //Note: Currently we only process ZSL channel
    bool value = false;
    if(channel_type == QCAMERA_CH_TYPE_ZSL){
        //check if FD requirement is enabled
        if(mParameters.isSnapshotFDNeeded() &&
           mParameters.isFaceDetectionEnabled()){
            value = true;
            CDBG_HIGH("%s: Face Detection metadata is required in ZSL mode.", __func__);
        }
    }

    return value;
}

bool QCamera2HardwareInterface::removeSizeFromList(cam_dimension_t* size_list,
        size_t length, cam_dimension_t size)
{
   bool found = false;
   size_t index = 0;
   for (size_t i = 0; i < length; i++) {
      if ((size_list[i].width == size.width
           && size_list[i].height == size.height)) {
         found = true;
         index = i;
         break;
      }

   }
   if (found) {
      for (size_t i = index; i < length; i++) {
         size_list[i] = size_list[i+1];
      }
   }
   return found;
}

void QCamera2HardwareInterface::copyList(cam_dimension_t *src_list,
        cam_dimension_t *dst_list, size_t len) {
    for (size_t i = 0; i < len; i++) {
        dst_list[i] = src_list[i];
    }
}

/*===========================================================================
 * FUNCTION   : defferedWorkRoutine
 *
 * DESCRIPTION: data process routine that executes deffered tasks
 *
 * PARAMETERS :
 *   @data    : user data ptr (QCamera2HardwareInterface)
 *
 * RETURN     : None
 *==========================================================================*/
void *QCamera2HardwareInterface::defferedWorkRoutine(void *obj)
{
    int running = 1;
    int ret;
    uint8_t is_active = FALSE;

    QCamera2HardwareInterface *pme = (QCamera2HardwareInterface *)obj;
    QCameraCmdThread *cmdThread = &pme->mDefferedWorkThread;
    cmdThread->setName("CAM_defrdWrk");

    do {
        do {
            ret = cam_sem_wait(&cmdThread->cmd_sem);
            if (ret != 0 && errno != EINVAL) {
                ALOGE("%s: cam_sem_wait error (%s)",
                        __func__, strerror(errno));
                return NULL;
            }
        } while (ret != 0);

        // we got notified about new cmd avail in cmd queue
        camera_cmd_type_t cmd = cmdThread->getCmd();
        switch (cmd) {
        case CAMERA_CMD_TYPE_START_DATA_PROC:
            CDBG_HIGH("%s: start data proc", __func__);
            is_active = TRUE;
            break;
        case CAMERA_CMD_TYPE_STOP_DATA_PROC:
            CDBG_HIGH("%s: stop data proc", __func__);
            is_active = FALSE;
            // signal cmd is completed
            cam_sem_post(&cmdThread->sync_sem);
            break;
        case CAMERA_CMD_TYPE_DO_NEXT_JOB:
            {
                DeffWork *dw =
                    reinterpret_cast<DeffWork *>(pme->mCmdQueue.dequeue());

                if ( NULL == dw ) {
                    ALOGE("%s : Invalid deferred work", __func__);
                    break;
                }

                switch( dw->cmd ) {
                case CMD_DEFF_ALLOCATE_BUFF:
                    {
                        QCameraChannel * pChannel = dw->args.allocArgs.ch;

                        if ( NULL == pChannel ) {
                            ALOGE("%s : Invalid deferred work channel",
                                    __func__);
                            break;
                        }

                        cam_stream_type_t streamType = dw->args.allocArgs.type;

                        uint32_t iNumOfStreams = pChannel->getNumOfStreams();
                        QCameraStream *pStream = NULL;
                        for ( uint32_t i = 0; i < iNumOfStreams; ++i) {
                            pStream = pChannel->getStreamByIndex(i);

                            if ( NULL == pStream ) {
                                break;
                            }

                            if ( pStream->isTypeOf(streamType)) {
                                if ( pStream->allocateBuffers() ) {
                                    ALOGE("%s: Error allocating buffers !!!",
                                            __func__);
                                }
                                break;
                            }
                        }
                        {
                            Mutex::Autolock l(pme->mDeffLock);
                            pme->mDeffOngoingJobs[dw->id] = false;
                            delete dw;
                            pme->mDeffCond.signal();
                        }

                    }
                    break;
                case CMD_DEFF_PPROC_START:
                    {
                        QCameraChannel * pChannel = dw->args.pprocArgs;
                        assert(pChannel);

                        if (pme->m_postprocessor.start(pChannel) != NO_ERROR) {
                            ALOGE("%s: cannot start postprocessor", __func__);
                            pme->delChannel(QCAMERA_CH_TYPE_CAPTURE);
                        }
                        {
                            Mutex::Autolock l(pme->mDeffLock);
                            pme->mDeffOngoingJobs[dw->id] = false;
                            delete dw;
                            pme->mDeffCond.signal();
                        }
                    }
                    break;
                default:
                    ALOGE("%s[%d]:  Incorrect command : %d",
                          __func__,
                          __LINE__,
                          dw->cmd);
                }
            }
            break;
        case CAMERA_CMD_TYPE_EXIT:
            running = 0;
            break;
        default:
            break;
        }
    } while (running);

    return NULL;
}

/*===========================================================================
 * FUNCTION   : isCaptureShutterEnabled
 *
 * DESCRIPTION: Check whether shutter should be triggered immediately after
 *              capture
 *
 * PARAMETERS :
 *
 * RETURN     : true - regular capture
 *              false - other type of capture
 *==========================================================================*/
bool QCamera2HardwareInterface::isCaptureShutterEnabled()
{
    char prop[PROPERTY_VALUE_MAX];
    memset(prop, 0, sizeof(prop));
    property_get("persist.camera.feature.shutter", prop, "0");
    int enableShutter = atoi(prop);
    return enableShutter == 1;
}

/*===========================================================================
 * FUNCTION   : queueDefferedWork
 *
 * DESCRIPTION: function which queues deferred tasks
 *
 * PARAMETERS :
 *   @cmd     : deferred task
 *   @args    : deffered task arguments
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::queueDefferedWork(DefferedWorkCmd cmd,
                                                     DefferWorkArgs args)
{
    Mutex::Autolock l(mDeffLock);
    for (uint32_t i = 0; i < MAX_ONGOING_JOBS; ++i) {
        if (!mDeffOngoingJobs[i]) {
            mCmdQueue.enqueue(new DeffWork(cmd, i, args));
            mDeffOngoingJobs[i] = true;
            mDefferedWorkThread.sendCmd(CAMERA_CMD_TYPE_DO_NEXT_JOB,
                    FALSE,
                    FALSE);
            return (int32_t)i;
        }
    }
    return -1;
}

/*===========================================================================
 * FUNCTION   : waitDefferedWork
 *
 * DESCRIPTION: waits for a deffered task to finish
 *
 * PARAMETERS :
 *   @job_id  : deferred task id
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCamera2HardwareInterface::waitDefferedWork(int32_t &job_id)
{
    Mutex::Autolock l(mDeffLock);

    if ((MAX_ONGOING_JOBS <= job_id) || (0 > job_id)) {
        return NO_ERROR;
    }

    while ( mDeffOngoingJobs[job_id] == true ) {
        mDeffCond.wait(mDeffLock);
    }

    job_id = MAX_ONGOING_JOBS;
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : isRegularCapture
 *
 * DESCRIPTION: Check configuration for regular catpure
 *
 * PARAMETERS :
 *
 * RETURN     : true - regular capture
 *              false - other type of capture
 *==========================================================================*/
bool QCamera2HardwareInterface::isRegularCapture()
{
    bool ret = false;

    if (numOfSnapshotsExpected() == 1 &&
        !isLongshotEnabled() &&
        !mParameters.getRecordingHintValue() &&
        !isZSLMode() && !(mParameters.isHDREnabled())) {
            ret = true;
    }
    return ret;
}

/*===========================================================================
 * FUNCTION   : getLogLevel
 *
 * DESCRIPTION: Reads the log level property into a variable
 *
 * PARAMETERS :
 *   None
 *
 * RETURN     :
 *   None
 *==========================================================================*/
void QCamera2HardwareInterface::getLogLevel()
{
    char prop[PROPERTY_VALUE_MAX];
    memset(prop, 0, sizeof(prop));

    /*  Higher 4 bits : Value of Debug log level (Default level is 1 to print all CDBG_HIGH)
        Lower 28 bits : Control mode for sub module logging(Only 3 sub modules in HAL)
                        0x1 for HAL
                        0x10 for mm-camera-interface
                        0x100 for mm-jpeg-interface  */
    property_get("persist.camera.hal.debug.mask", prop, "268435463"); // 0x10000007=268435463
    uint32_t temp = (uint32_t) atoi(prop);
    uint32_t log_level = ((temp >> 28) & 0xF);
    uint32_t debug_mask = (temp & HAL_DEBUG_MASK_HAL);
    if (debug_mask > 0)
        gCamHalLogLevel = log_level;
    else
        gCamHalLogLevel = 0; // Debug logs are not required if debug_mask is zero

    ALOGI("%s gCamHalLogLevel=%d",__func__, gCamHalLogLevel);
    return;
}

/*===========================================================================
 * FUNCTION   : getSensorType
 *
 * DESCRIPTION: Returns the type of sensor being used whether YUV or Bayer
 *
 * PARAMETERS :
 *   None
 *
 * RETURN     : Type of sensor - bayer or YUV
 *
 *==========================================================================*/
cam_sensor_t QCamera2HardwareInterface::getSensorType()
{
    return gCamCapability[mCameraId]->sensor_type.sens_type;
}

}; // namespace qcamera
