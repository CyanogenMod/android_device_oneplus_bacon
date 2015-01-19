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

#ifndef __QCAMERA_STATEMACHINE_H__
#define __QCAMERA_STATEMACHINE_H__

#include <pthread.h>

#include <cam_semaphore.h>
extern "C" {
#include <mm_camera_interface.h>
}

#include "QCameraQueue.h"
#include "QCameraChannel.h"

namespace qcamera {

class QCamera2HardwareInterface;

typedef enum {
    /*******BEGIN OF: API EVT*********/
    QCAMERA_SM_EVT_SET_PREVIEW_WINDOW = 1,   // set preview window
    QCAMERA_SM_EVT_SET_CALLBACKS,            // set callbacks
    QCAMERA_SM_EVT_ENABLE_MSG_TYPE,          // enable msg type
    QCAMERA_SM_EVT_DISABLE_MSG_TYPE,         // disable msg type
    QCAMERA_SM_EVT_MSG_TYPE_ENABLED,         // query certain msg type is enabled

    QCAMERA_SM_EVT_SET_PARAMS,               // set parameters
    QCAMERA_SM_EVT_GET_PARAMS,               // get parameters
    QCAMERA_SM_EVT_PUT_PARAMS,               // put parameters, release param buf

    QCAMERA_SM_EVT_START_PREVIEW,            // start preview (zsl, camera mode, camcorder mode)
    QCAMERA_SM_EVT_START_NODISPLAY_PREVIEW,  // start no display preview (zsl, camera mode, camcorder mode)
    QCAMERA_SM_EVT_STOP_PREVIEW,             // stop preview (zsl, camera mode, camcorder mode)
    QCAMERA_SM_EVT_PREVIEW_ENABLED,          // query if preview is running

    QCAMERA_SM_EVT_STORE_METADATA_IN_BUFS,   // request to store meta data in video buffers
    QCAMERA_SM_EVT_START_RECORDING,          // start recording
    QCAMERA_SM_EVT_STOP_RECORDING,           // stop recording
    QCAMERA_SM_EVT_RECORDING_ENABLED,        // query if recording is running
    QCAMERA_SM_EVT_RELEASE_RECORIDNG_FRAME,  // release recording frame

    QCAMERA_SM_EVT_PREPARE_SNAPSHOT,         // prepare snapshot in case LED needs to be flashed
    QCAMERA_SM_EVT_TAKE_PICTURE,             // take picutre (zsl, regualr capture, live snapshot
    QCAMERA_SM_EVT_CANCEL_PICTURE,           // cancel picture

    QCAMERA_SM_EVT_START_AUTO_FOCUS,         // start auto focus
    QCAMERA_SM_EVT_STOP_AUTO_FOCUS,          // stop auto focus
    QCAMERA_SM_EVT_SEND_COMMAND,             // send command

    QCAMERA_SM_EVT_RELEASE,                  // release camera resource
    QCAMERA_SM_EVT_DUMP,                     // dump
    QCAMERA_SM_EVT_REG_FACE_IMAGE,           // register a face image in imaging lib
    /*******END OF: API EVT*********/

    QCAMERA_SM_EVT_EVT_INTERNAL,             // internal evt notify
    QCAMERA_SM_EVT_EVT_NOTIFY,               // evt notify from server
    QCAMERA_SM_EVT_JPEG_EVT_NOTIFY,          // evt notify from jpeg
    QCAMERA_SM_EVT_SNAPSHOT_DONE,            // internal evt that snapshot is done
    QCAMERA_SM_EVT_THERMAL_NOTIFY,           // evt notify from thermal daemon
    QCAMERA_SM_EVT_STOP_CAPTURE_CHANNEL,     // stop capture channel
    QCAMERA_SM_EVT_RESTART_PERVIEW,          // internal preview restart
    QCAMERA_SM_EVT_MAX
} qcamera_sm_evt_enum_t;

typedef enum {
    QCAMERA_API_RESULT_TYPE_DEF,             // default type, no additional info
    QCAMERA_API_RESULT_TYPE_ENABLE_FLAG,     // msg_enabled, preview_enabled, recording_enabled
    QCAMERA_API_RESULT_TYPE_PARAMS,          // returned parameters in string
    QCAMERA_API_RESULT_TYPE_HANDLE,          // returned handle in int
    QCAMERA_API_RESULT_TYPE_MAX
} qcamera_api_result_type_t;

typedef struct {
    int32_t status;                          // api call status
    qcamera_sm_evt_enum_t request_api;       // api evt requested
    qcamera_api_result_type_t result_type;   // result type
    union {
        int enabled;                          // result_type == QCAMERA_API_RESULT_TYPE_ENABLE_FLAG
        char *params;                         // result_type == QCAMERA_API_RESULT_TYPE_PARAMS
        int handle;                           // result_type ==QCAMERA_API_RESULT_TYPE_HANDLE
    };
} qcamera_api_result_t;

typedef struct api_result_list {
   qcamera_api_result_t result;
   struct api_result_list *next;
}api_result_list;

// definition for payload type of setting callback
typedef struct {
    camera_notify_callback notify_cb;
    camera_data_callback data_cb;
    camera_data_timestamp_callback data_cb_timestamp;
    camera_request_memory get_memory;
    void *user;
} qcamera_sm_evt_setcb_payload_t;

// definition for payload type of sending command
typedef struct {
    int32_t cmd;
    int32_t arg1;
    int32_t arg2;
} qcamera_sm_evt_command_payload_t;

// definition for payload type of sending command
typedef struct {
    void *img_ptr;
    cam_pp_offline_src_config_t *config;
} qcamera_sm_evt_reg_face_payload_t;

typedef enum {
    QCAMERA_INTERNAL_EVT_FOCUS_UPDATE,       // focus updating result
    QCAMERA_INTERNAL_EVT_PREP_SNAPSHOT_DONE, // prepare snapshot done
    QCAMERA_INTERNAL_EVT_FACE_DETECT_RESULT, // face detection result
    QCAMERA_INTERNAL_EVT_HISTOGRAM_STATS,    // histogram
    QCAMERA_INTERNAL_EVT_CROP_INFO,          // crop info
    QCAMERA_INTERNAL_EVT_ASD_UPDATE,         // asd update result
    QCAMERA_INTERNAL_EVT_AWB_UPDATE,         // awb update result
    QCAMERA_INTERNAL_EVT_MAX
} qcamera_internal_evt_type_t;

typedef struct {
    qcamera_internal_evt_type_t evt_type;
    union {
        cam_auto_focus_data_t focus_data;
        cam_prep_snapshot_state_t prep_snapshot_state;
        cam_face_detection_data_t faces_data;
        cam_hist_stats_t stats_data;
        cam_crop_data_t crop_data;
        cam_auto_scene_t asd_data;
        cam_awb_params_t awb_data;
    };
} qcamera_sm_internal_evt_payload_t;

class QCameraStateMachine
{
public:
    QCameraStateMachine(QCamera2HardwareInterface *ctrl);
    virtual ~QCameraStateMachine();
    int32_t procAPI(qcamera_sm_evt_enum_t evt, void *api_payload);
    int32_t procEvt(qcamera_sm_evt_enum_t evt, void *evt_payload);

    bool isPreviewRunning(); // check if preview is running
    bool isPreviewReady(); // check if preview is ready
    bool isCaptureRunning(); // check if image capture is running
    bool isNonZSLCaptureRunning(); // check if image capture is running in non ZSL mode

private:
    typedef enum {
        QCAMERA_SM_STATE_PREVIEW_STOPPED,          // preview is stopped
        QCAMERA_SM_STATE_PREVIEW_READY,            // preview started but preview window is not set yet
        QCAMERA_SM_STATE_PREVIEWING,               // previewing
        QCAMERA_SM_STATE_PREPARE_SNAPSHOT,         // prepare snapshot in case aec estimation is
                                                   // needed for LED flash
        QCAMERA_SM_STATE_PIC_TAKING,               // taking picture (preview stopped)
        QCAMERA_SM_STATE_RECORDING,                // recording (preview running)
        QCAMERA_SM_STATE_VIDEO_PIC_TAKING,         // taking live snapshot during recording (preview running)
        QCAMERA_SM_STATE_PREVIEW_PIC_TAKING        // taking ZSL/live snapshot (recording stopped but preview running)
    } qcamera_state_enum_t;

    typedef enum
    {
        QCAMERA_SM_CMD_TYPE_API,                   // cmd from API
        QCAMERA_SM_CMD_TYPE_EVT,                   // cmd from mm-camera-interface/mm-jpeg-interface event
        QCAMERA_SM_CMD_TYPE_EXIT,                  // cmd for exiting statemachine cmdThread
        QCAMERA_SM_CMD_TYPE_MAX
    } qcamera_sm_cmd_type_t;

    typedef struct {
        qcamera_sm_cmd_type_t cmd;                  // cmd type (where it comes from)
        qcamera_sm_evt_enum_t evt;                  // event type
        void *evt_payload;                          // ptr to payload
    } qcamera_sm_cmd_t;

    int32_t stateMachine(qcamera_sm_evt_enum_t evt, void *payload);
    int32_t procEvtPreviewStoppedState(qcamera_sm_evt_enum_t evt, void *payload);
    int32_t procEvtPreviewReadyState(qcamera_sm_evt_enum_t evt, void *payload);
    int32_t procEvtPreviewingState(qcamera_sm_evt_enum_t evt, void *payload);
    int32_t procEvtPrepareSnapshotState(qcamera_sm_evt_enum_t evt, void *payload);
    int32_t procEvtPicTakingState(qcamera_sm_evt_enum_t evt, void *payload);
    int32_t procEvtRecordingState(qcamera_sm_evt_enum_t evt, void *payload);
    int32_t procEvtVideoPicTakingState(qcamera_sm_evt_enum_t evt, void *payload);
    int32_t procEvtPreviewPicTakingState(qcamera_sm_evt_enum_t evt, void *payload);

    // main statemachine process routine
    static void *smEvtProcRoutine(void *data);

    QCamera2HardwareInterface *m_parent;  // ptr to HWI
    qcamera_state_enum_t m_state;         // statemachine state
    QCameraQueue api_queue;               // cmd queue for APIs
    QCameraQueue evt_queue;               // cmd queue for evt from mm-camera-intf/mm-jpeg-intf
    pthread_t cmd_pid;                    // cmd thread ID
    cam_semaphore_t cmd_sem;              // semaphore for cmd thread

};

}; // namespace qcamera

#endif /* __QCAMERA_STATEMACHINE_H__ */
