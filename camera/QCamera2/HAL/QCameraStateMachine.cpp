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

#define LOG_TAG "QCameraStateMachine"

#include <utils/Errors.h>
#include "QCamera2HWI.h"
#include "QCameraStateMachine.h"

namespace qcamera {

/*===========================================================================
 * FUNCTION   : smEvtProcRoutine
 *
 * DESCRIPTION: Statemachine process thread routine to handle events
 *              in different state.
 *
 * PARAMETERS :
 *   @data    : ptr to QCameraStateMachine object
 *
 * RETURN     : none
 *==========================================================================*/
void *QCameraStateMachine::smEvtProcRoutine(void *data)
{
    int running = 1, ret;
    QCameraStateMachine *pme = (QCameraStateMachine *)data;

    CDBG_HIGH("%s: E", __func__);
    do {
        do {
            ret = cam_sem_wait(&pme->cmd_sem);
            if (ret != 0 && errno != EINVAL) {
                ALOGE("%s: cam_sem_wait error (%s)",
                           __func__, strerror(errno));
                return NULL;
            }
        } while (ret != 0);

        // we got notified about new cmd avail in cmd queue
        // first check API cmd queue
        qcamera_sm_cmd_t *node = (qcamera_sm_cmd_t *)pme->api_queue.dequeue();
        if (node == NULL) {
            // no API cmd, then check evt cmd queue
            node = (qcamera_sm_cmd_t *)pme->evt_queue.dequeue();
        }
        if (node != NULL) {
            switch (node->cmd) {
            case QCAMERA_SM_CMD_TYPE_API:
                pme->stateMachine(node->evt, node->evt_payload);
                // API is in a way sync call, so evt_payload is managed by HWI
                // no need to free payload for API
                break;
            case QCAMERA_SM_CMD_TYPE_EVT:
                pme->stateMachine(node->evt, node->evt_payload);

                // EVT is async call, so payload need to be free after use
                free(node->evt_payload);
                node->evt_payload = NULL;
                break;
            case QCAMERA_SM_CMD_TYPE_EXIT:
                running = 0;
                break;
            default:
                break;
            }
            free(node);
            node = NULL;
        }
    } while (running);
    CDBG_HIGH("%s: X", __func__);
    return NULL;
}

/*===========================================================================
 * FUNCTION   : QCameraStateMachine
 *
 * DESCRIPTION: constructor of QCameraStateMachine. Will start process thread
 *
 * PARAMETERS :
 *   @ctrl    : ptr to HWI object
 *
 * RETURN     : none
 *==========================================================================*/
QCameraStateMachine::QCameraStateMachine(QCamera2HardwareInterface *ctrl) :
    api_queue(),
    evt_queue()
{
    m_parent = ctrl;
    m_state = QCAMERA_SM_STATE_PREVIEW_STOPPED;
    cmd_pid = 0;
    cam_sem_init(&cmd_sem, 0);
    pthread_create(&cmd_pid,
                   NULL,
                   smEvtProcRoutine,
                   this);
    pthread_setname_np(cmd_pid, "CAM_stMachine");
}

/*===========================================================================
 * FUNCTION   : ~QCameraStateMachine
 *
 * DESCRIPTION: desctructor of QCameraStateMachine. Will stop process thread.
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
QCameraStateMachine::~QCameraStateMachine()
{
    if (cmd_pid != 0) {
        qcamera_sm_cmd_t *node =
            (qcamera_sm_cmd_t *)malloc(sizeof(qcamera_sm_cmd_t));
        if (NULL != node) {
            memset(node, 0, sizeof(qcamera_sm_cmd_t));
            node->cmd = QCAMERA_SM_CMD_TYPE_EXIT;

            api_queue.enqueue((void *)node);
            cam_sem_post(&cmd_sem);

            /* wait until cmd thread exits */
            if (pthread_join(cmd_pid, NULL) != 0) {
                CDBG_HIGH("%s: pthread dead already\n", __func__);
            }
        }
        cmd_pid = 0;
    }
    cam_sem_destroy(&cmd_sem);
}

/*===========================================================================
 * FUNCTION   : procAPI
 *
 * DESCRIPTION: process incoming API request from framework layer.
 *
 * PARAMETERS :
 *   @evt          : event to be processed
 *   @api_payload  : API payload. Can be NULL if not needed.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraStateMachine::procAPI(qcamera_sm_evt_enum_t evt,
                                     void *api_payload)
{
    qcamera_sm_cmd_t *node =
        (qcamera_sm_cmd_t *)malloc(sizeof(qcamera_sm_cmd_t));
    if (NULL == node) {
        ALOGE("%s: No memory for qcamera_sm_cmd_t", __func__);
        return NO_MEMORY;
    }

    memset(node, 0, sizeof(qcamera_sm_cmd_t));
    node->cmd = QCAMERA_SM_CMD_TYPE_API;
    node->evt = evt;
    node->evt_payload = api_payload;
    if (api_queue.enqueue((void *)node)) {
        cam_sem_post(&cmd_sem);
        return NO_ERROR;
    } else {
        free(node);
        return UNKNOWN_ERROR;
    }
}

/*===========================================================================
 * FUNCTION   : procEvt
 *
 * DESCRIPTION: process incoming envent from mm-camera-interface and
 *              mm-jpeg-interface.
 *
 * PARAMETERS :
 *   @evt          : event to be processed
 *   @evt_payload  : event payload. Can be NULL if not needed.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraStateMachine::procEvt(qcamera_sm_evt_enum_t evt,
                                     void *evt_payload)
{
    qcamera_sm_cmd_t *node =
        (qcamera_sm_cmd_t *)malloc(sizeof(qcamera_sm_cmd_t));
    if (NULL == node) {
        ALOGE("%s: No memory for qcamera_sm_cmd_t", __func__);
        return NO_MEMORY;
    }

    memset(node, 0, sizeof(qcamera_sm_cmd_t));
    node->cmd = QCAMERA_SM_CMD_TYPE_EVT;
    node->evt = evt;
    node->evt_payload = evt_payload;
    if (evt_queue.enqueue((void *)node)) {
        cam_sem_post(&cmd_sem);
        return NO_ERROR;
    } else {
        free(node);
        return UNKNOWN_ERROR;
    }
}

/*===========================================================================
 * FUNCTION   : stateMachine
 *
 * DESCRIPTION: finite state machine entry function. Depends on state,
 *              incoming event will be handled differently.
 *
 * PARAMETERS :
 *   @evt      : event to be processed
 *   @payload  : event payload. Can be NULL if not needed.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraStateMachine::stateMachine(qcamera_sm_evt_enum_t evt, void *payload)
{
    int32_t rc = NO_ERROR;
    switch (m_state) {
    case QCAMERA_SM_STATE_PREVIEW_STOPPED:
        rc = procEvtPreviewStoppedState(evt, payload);
        break;
    case QCAMERA_SM_STATE_PREVIEW_READY:
        rc = procEvtPreviewReadyState(evt, payload);
        break;
    case QCAMERA_SM_STATE_PREVIEWING:
        rc = procEvtPreviewingState(evt, payload);
        break;
    case QCAMERA_SM_STATE_PREPARE_SNAPSHOT:
        rc = procEvtPrepareSnapshotState(evt, payload);
        break;
    case QCAMERA_SM_STATE_PIC_TAKING:
        rc = procEvtPicTakingState(evt, payload);
        break;
    case QCAMERA_SM_STATE_RECORDING:
        rc = procEvtRecordingState(evt, payload);
        break;
    case QCAMERA_SM_STATE_VIDEO_PIC_TAKING:
        rc = procEvtVideoPicTakingState(evt, payload);
        break;
    case QCAMERA_SM_STATE_PREVIEW_PIC_TAKING:
        rc = procEvtPreviewPicTakingState(evt, payload);
        break;
    default:
        break;
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : procEvtPreviewStoppedState
 *
 * DESCRIPTION: finite state machine function to handle event in state of
 *              QCAMERA_SM_STATE_PREVIEW_STOPPED.
 *
 * PARAMETERS :
 *   @evt      : event to be processed
 *   @payload  : event payload. Can be NULL if not needed.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraStateMachine::procEvtPreviewStoppedState(qcamera_sm_evt_enum_t evt,
                                                        void *payload)
{
    int32_t rc = NO_ERROR;
    qcamera_api_result_t result;
    memset(&result, 0, sizeof(qcamera_api_result_t));

    switch (evt) {
    case QCAMERA_SM_EVT_SET_PREVIEW_WINDOW:
        {
            rc = m_parent->setPreviewWindow((struct preview_stream_ops *)payload);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_SET_CALLBACKS:
        {
            qcamera_sm_evt_setcb_payload_t *setcbs =
                (qcamera_sm_evt_setcb_payload_t *)payload;
            rc = m_parent->setCallBacks(setcbs->notify_cb,
                                        setcbs->data_cb,
                                        setcbs->data_cb_timestamp,
                                        setcbs->get_memory,
                                        setcbs->user);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_ENABLE_MSG_TYPE:
        {
            rc = m_parent->enableMsgType(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_DISABLE_MSG_TYPE:
        {
            rc = m_parent->disableMsgType(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_MSG_TYPE_ENABLED:
        {
            int enabled = m_parent->msgTypeEnabled(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_ENABLE_FLAG;
            result.enabled = enabled;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_SET_PARAMS:
        {
            bool needRestart = false;
            rc = m_parent->updateParameters((char*)payload, needRestart);
            if (needRestart) {
                // Clear memory pools
                m_parent->m_memoryPool.clear();
            }
            if (rc == NO_ERROR) {
                rc = m_parent->commitParameterChanges();
            }
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_GET_PARAMS:
        {
            result.params = m_parent->getParameters();
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_PARAMS;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_PUT_PARAMS:
        {
            rc = m_parent->putParameters((char*)payload);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_START_PREVIEW:
        {
            if (m_parent->mPreviewWindow == NULL) {
                rc = m_parent->preparePreview();
                if(rc == NO_ERROR) {
                    // preview window is not set yet, move to previewReady state
                    m_state = QCAMERA_SM_STATE_PREVIEW_READY;
                } else {
                    ALOGE("%s: preparePreview failed",__func__);
                }
            } else {
                rc = m_parent->preparePreview();
                if (rc == NO_ERROR) {
                    rc = m_parent->startPreview();
                    if (rc != NO_ERROR) {
                        m_parent->unpreparePreview();
                    } else {
                        // start preview success, move to previewing state
                        m_state = QCAMERA_SM_STATE_PREVIEWING;
                    }
                }
            }
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_START_NODISPLAY_PREVIEW:
        {
            rc = m_parent->preparePreview();
            if (rc == NO_ERROR) {
                rc = m_parent->startPreview();
                if (rc != NO_ERROR) {
                    m_parent->unpreparePreview();
                } else {
                    m_state = QCAMERA_SM_STATE_PREVIEWING;
                }
            }
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
    break;
    case QCAMERA_SM_EVT_STOP_PREVIEW:
        {
            // no op needed here
            CDBG_HIGH("%s: already in preview stopped state, do nothing", __func__);
            result.status = NO_ERROR;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_PREVIEW_ENABLED:
    case QCAMERA_SM_EVT_RECORDING_ENABLED:
        {
            result.status = NO_ERROR;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_ENABLE_FLAG;
            result.enabled = 0;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_RELEASE:
        {
            rc = m_parent->release();
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STORE_METADATA_IN_BUFS:
        {
            rc = m_parent->storeMetaDataInBuffers(*((int *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_DUMP:
        {
            rc = m_parent->dump(*((int *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_SEND_COMMAND:
        {
            qcamera_sm_evt_command_payload_t *cmd_payload =
                (qcamera_sm_evt_command_payload_t *)payload;
            rc = m_parent->sendCommand(cmd_payload->cmd,
                                       cmd_payload->arg1,
                                       cmd_payload->arg2);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_START_RECORDING:
    case QCAMERA_SM_EVT_STOP_RECORDING:
    case QCAMERA_SM_EVT_RELEASE_RECORIDNG_FRAME:
    case QCAMERA_SM_EVT_PREPARE_SNAPSHOT:
    case QCAMERA_SM_EVT_TAKE_PICTURE:
        {
            ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
            rc = INVALID_OPERATION;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_START_AUTO_FOCUS:
    case QCAMERA_SM_EVT_CANCEL_PICTURE:
        {
            // no op needed here
            CDBG_HIGH("%s: No ops for evt(%d) in state(%d)", __func__, evt, m_state);
            result.status = NO_ERROR;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STOP_AUTO_FOCUS:
        {
            rc = m_parent->cancelAutoFocus();
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_REG_FACE_IMAGE:
        {
            int32_t faceID = 0;
            qcamera_sm_evt_reg_face_payload_t *reg_payload =
                (qcamera_sm_evt_reg_face_payload_t *)payload;
            rc = m_parent->registerFaceImage(reg_payload->img_ptr,
                                             reg_payload->config,
                                             faceID);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_HANDLE;
            result.handle = faceID;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_THERMAL_NOTIFY:
        {
            rc = m_parent->updateThermalLevel(
                    *((qcamera_thermal_level_enum_t *)payload));
        }
        break;
    case QCAMERA_SM_EVT_EVT_NOTIFY:
        {
            mm_camera_event_t *cam_evt = (mm_camera_event_t *)payload;
            switch (cam_evt->server_event_type) {
            case CAM_EVENT_TYPE_DAEMON_DIED:
                {
                    m_parent->sendEvtNotify(CAMERA_MSG_ERROR,
                                            CAMERA_ERROR_SERVER_DIED,
                                            0);
                }
                break;
            default:
                ALOGE("%s: Invalid internal event %d in state(%d)",
                            __func__, cam_evt->server_event_type, m_state);
                break;
            }
        }
        break;
    case QCAMERA_SM_EVT_SNAPSHOT_DONE:
        {
            // No ops, but need to notify
            ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalEvtResult(&result);
        }
       break;
    case QCAMERA_SM_EVT_EVT_INTERNAL:
    case QCAMERA_SM_EVT_JPEG_EVT_NOTIFY:
    default:
        ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
        break;
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : procEvtPreviewReadyState
 *
 * DESCRIPTION: finite state machine function to handle event in state of
 *              QCAMERA_SM_STATE_PREVIEW_READY.
 *
 * PARAMETERS :
 *   @evt      : event to be processed
 *   @payload  : event payload. Can be NULL if not needed.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraStateMachine::procEvtPreviewReadyState(qcamera_sm_evt_enum_t evt,
                                                      void *payload)
{
    int32_t rc = NO_ERROR;
    qcamera_api_result_t result;
    memset(&result, 0, sizeof(qcamera_api_result_t));

    switch (evt) {
    case QCAMERA_SM_EVT_SET_PREVIEW_WINDOW:
        {
            m_parent->setPreviewWindow((struct preview_stream_ops *)payload);
            if (m_parent->mPreviewWindow != NULL) {
                rc = m_parent->startPreview();
                if (rc != NO_ERROR) {
                    m_parent->unpreparePreview();
                    m_state = QCAMERA_SM_STATE_PREVIEW_STOPPED;
                } else {
                    m_state = QCAMERA_SM_STATE_PREVIEWING;
                }
            }

            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_SET_CALLBACKS:
        {
            qcamera_sm_evt_setcb_payload_t *setcbs =
                (qcamera_sm_evt_setcb_payload_t *)payload;
            rc = m_parent->setCallBacks(setcbs->notify_cb,
                                        setcbs->data_cb,
                                        setcbs->data_cb_timestamp,
                                        setcbs->get_memory,
                                        setcbs->user);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_ENABLE_MSG_TYPE:
        {
            rc = m_parent->enableMsgType(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_DISABLE_MSG_TYPE:
        {
            rc = m_parent->disableMsgType(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_MSG_TYPE_ENABLED:
        {
            int enabled = m_parent->msgTypeEnabled(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_ENABLE_FLAG;
            result.enabled = enabled;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_SET_PARAMS:
        {
            bool needRestart = false;
            rc = m_parent->updateParameters((char*)payload, needRestart);
            if (rc == NO_ERROR) {
                if (needRestart) {
                    // need restart preview for parameters to take effect
                    m_parent->unpreparePreview();
                    // Clear memory pools
                    m_parent->m_memoryPool.clear();
                    // commit parameter changes to server
                    m_parent->commitParameterChanges();
                    // prepare preview again
                    rc = m_parent->preparePreview();
                    if (rc != NO_ERROR) {
                        m_state = QCAMERA_SM_STATE_PREVIEW_STOPPED;
                    }
                } else {
                    rc = m_parent->commitParameterChanges();
                }
            }

            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_GET_PARAMS:
        {
            result.params = m_parent->getParameters();
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_PARAMS;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_PUT_PARAMS:
        {
            rc = m_parent->putParameters((char*)payload);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_START_PREVIEW:
        {
            // no ops here
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STOP_PREVIEW:
        {
            m_parent->unpreparePreview();
            rc = 0;
            m_state = QCAMERA_SM_STATE_PREVIEW_STOPPED;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_PREVIEW_ENABLED:
        {
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_ENABLE_FLAG;
            result.enabled = 1;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_RECORDING_ENABLED:
        {
            rc = 0;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_ENABLE_FLAG;
            result.enabled = 0;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STORE_METADATA_IN_BUFS:
        {
            rc = m_parent->storeMetaDataInBuffers(*((int *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_DUMP:
        {
            rc = m_parent->dump(*((int *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_START_AUTO_FOCUS:
        {
            rc = m_parent->autoFocus();
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STOP_AUTO_FOCUS:
        {
            rc = m_parent->cancelAutoFocus();
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_SEND_COMMAND:
        {
            qcamera_sm_evt_command_payload_t *cmd_payload =
                (qcamera_sm_evt_command_payload_t *)payload;
            rc = m_parent->sendCommand(cmd_payload->cmd,
                                       cmd_payload->arg1,
                                       cmd_payload->arg2);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_REG_FACE_IMAGE:
        {
            int32_t faceID = 0;
            qcamera_sm_evt_reg_face_payload_t *reg_payload =
                (qcamera_sm_evt_reg_face_payload_t *)payload;
            rc = m_parent->registerFaceImage(reg_payload->img_ptr,
                                             reg_payload->config,
                                             faceID);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_HANDLE;
            result.handle = faceID;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_START_NODISPLAY_PREVIEW:
    case QCAMERA_SM_EVT_START_RECORDING:
    case QCAMERA_SM_EVT_STOP_RECORDING:
    case QCAMERA_SM_EVT_PREPARE_SNAPSHOT:
    case QCAMERA_SM_EVT_TAKE_PICTURE:
    case QCAMERA_SM_EVT_CANCEL_PICTURE:
    case QCAMERA_SM_EVT_RELEASE_RECORIDNG_FRAME:
    case QCAMERA_SM_EVT_RELEASE:
        {
            ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
            rc = INVALID_OPERATION;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_EVT_NOTIFY:
        {
            mm_camera_event_t *cam_evt = (mm_camera_event_t *)payload;
            switch (cam_evt->server_event_type) {
            case CAM_EVENT_TYPE_DAEMON_DIED:
                {
                    m_parent->sendEvtNotify(CAMERA_MSG_ERROR,
                                            CAMERA_ERROR_SERVER_DIED,
                                            0);
                }
                break;
            default:
                ALOGE("%s: Invalid internal event %d in state(%d)",
                            __func__, cam_evt->server_event_type, m_state);
                break;
            }
        }
        break;
    case QCAMERA_SM_EVT_SNAPSHOT_DONE:
        {
            // No ops, but need to notify
            ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalEvtResult(&result);
        }
       break;
    case QCAMERA_SM_EVT_EVT_INTERNAL:
    case QCAMERA_SM_EVT_JPEG_EVT_NOTIFY:
    case QCAMERA_SM_EVT_THERMAL_NOTIFY:
    default:
        ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
        break;
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : procEvtPreviewingState
 *
 * DESCRIPTION: finite state machine function to handle event in state of
 *              QCAMERA_SM_STATE_PREVIEWING.
 *
 * PARAMETERS :
 *   @evt      : event to be processed
 *   @payload  : event payload. Can be NULL if not needed.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraStateMachine::procEvtPreviewingState(qcamera_sm_evt_enum_t evt,
                                                    void *payload)
{
    int32_t rc = NO_ERROR;
    qcamera_api_result_t result;
    memset(&result, 0, sizeof(qcamera_api_result_t));

    switch (evt) {
    case QCAMERA_SM_EVT_SET_PREVIEW_WINDOW:
        {
            // Error setting preview window during previewing
            ALOGE("Cannot set preview window when preview is running");
            rc = INVALID_OPERATION;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_SET_CALLBACKS:
        {
            qcamera_sm_evt_setcb_payload_t *setcbs =
                (qcamera_sm_evt_setcb_payload_t *)payload;
            rc = m_parent->setCallBacks(setcbs->notify_cb,
                                        setcbs->data_cb,
                                        setcbs->data_cb_timestamp,
                                        setcbs->get_memory,
                                        setcbs->user);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_ENABLE_MSG_TYPE:
        {
            rc = m_parent->enableMsgType(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_DISABLE_MSG_TYPE:
        {
            rc = m_parent->disableMsgType(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_MSG_TYPE_ENABLED:
        {
            int enabled = m_parent->msgTypeEnabled(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_ENABLE_FLAG;
            result.enabled = enabled;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_SET_PARAMS:
        {
            bool needRestart = false;
            rc = m_parent->updateParameters((char*)payload, needRestart);
            if (rc == NO_ERROR) {
                if (needRestart) {
                    // need restart preview for parameters to take effect
                    // stop preview
                    m_parent->stopPreview();
                    // Clear memory pools
                    m_parent->m_memoryPool.clear();
                    // commit parameter changes to server
                    m_parent->commitParameterChanges();
                    // start preview again
                    rc = m_parent->preparePreview();
                    if (rc == NO_ERROR) {
                        rc = m_parent->startPreview();
                        if (rc != NO_ERROR) {
                            m_parent->unpreparePreview();
                        }
                    }
                    if (rc != NO_ERROR) {
                        m_state = QCAMERA_SM_STATE_PREVIEW_STOPPED;
                    }
                } else {
                    rc = m_parent->commitParameterChanges();
                }
            }
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_GET_PARAMS:
        {
            result.params = m_parent->getParameters();
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_PARAMS;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_PUT_PARAMS:
        {
            rc = m_parent->putParameters((char*)payload);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_START_PREVIEW:
    case QCAMERA_SM_EVT_START_NODISPLAY_PREVIEW:
        {
            // no ops here
            CDBG_HIGH("%s: Already in previewing, no ops here to start preview", __func__);
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STOP_PREVIEW:
        {
            rc = m_parent->stopPreview();
            m_state = QCAMERA_SM_STATE_PREVIEW_STOPPED;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_PREVIEW_ENABLED:
        {
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_ENABLE_FLAG;
            result.enabled = 1;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_RECORDING_ENABLED:
        {
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_ENABLE_FLAG;
            result.enabled = 0;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STORE_METADATA_IN_BUFS:
        {
            rc = m_parent->storeMetaDataInBuffers(*((int *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_DUMP:
        {
            rc = m_parent->dump(*((int *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_START_AUTO_FOCUS:
        {
            rc = m_parent->autoFocus();
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STOP_AUTO_FOCUS:
        {
            rc = m_parent->cancelAutoFocus();
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_START_RECORDING:
        {
            rc = m_parent->startRecording();
            if (rc == NO_ERROR) {
                // move state to recording state
                m_state = QCAMERA_SM_STATE_RECORDING;
            }
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_PREPARE_SNAPSHOT:
        {
            rc = m_parent->prepareHardwareForSnapshot(FALSE);
            if (rc == NO_ERROR) {
                // Do not signal API result in this case.
                // Need to wait for snapshot done in metadta.
                m_state = QCAMERA_SM_STATE_PREPARE_SNAPSHOT;
            } else {
                // Do not change state in this case.
                ALOGE("%s: prepareHardwareForSnapshot failed %d",
                    __func__, rc);

                result.status = rc;
                result.request_api = evt;
                result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
                m_parent->signalAPIResult(&result);
            }
        }
        break;
    case QCAMERA_SM_EVT_TAKE_PICTURE:
       {
           if ( m_parent->mParameters.getRecordingHintValue() == false) {
               if (m_parent->isZSLMode() || m_parent->isLongshotEnabled()) {
                   m_state = QCAMERA_SM_STATE_PREVIEW_PIC_TAKING;
                   rc = m_parent->takePicture();
                   if (rc != NO_ERROR) {
                       // move state to previewing state
                       m_state = QCAMERA_SM_STATE_PREVIEWING;
                   }
               } else {
                   m_state = QCAMERA_SM_STATE_PIC_TAKING;
                   rc = m_parent->takePicture();
                   if (rc != NO_ERROR) {
                       // move state to preview stopped state
                       m_state = QCAMERA_SM_STATE_PREVIEW_STOPPED;
                   }
               }

               result.status = rc;
               result.request_api = evt;
               result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
               m_parent->signalAPIResult(&result);
           } else {
               m_state = QCAMERA_SM_STATE_PREVIEW_PIC_TAKING;
               rc = m_parent->takeLiveSnapshot();
               if (rc != NO_ERROR ) {
                   m_state = QCAMERA_SM_STATE_PREVIEWING;
               }
               result.status = rc;
               result.request_api = evt;
               result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
               m_parent->signalAPIResult(&result);
           }
        }
        break;
    case QCAMERA_SM_EVT_SEND_COMMAND:
        {
            qcamera_sm_evt_command_payload_t *cmd_payload =
                (qcamera_sm_evt_command_payload_t *)payload;
            rc = m_parent->sendCommand(cmd_payload->cmd,
                                       cmd_payload->arg1,
                                       cmd_payload->arg2);
            if (CAMERA_CMD_LONGSHOT_ON == cmd_payload->cmd) {
                if (QCAMERA_SM_EVT_RESTART_PERVIEW == cmd_payload->arg1) {
                    m_parent->stopPreview();
                    // Clear memory pools
                    m_parent->m_memoryPool.clear();
                    // start preview again
                    rc = m_parent->preparePreview();
                    if (rc == NO_ERROR) {
                        rc = m_parent->startPreview();
                        if (rc != NO_ERROR) {
                            m_parent->unpreparePreview();
                        }
                    }
                }
            }
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_REG_FACE_IMAGE:
        {
            int32_t faceID = 0;
            qcamera_sm_evt_reg_face_payload_t *reg_payload =
                (qcamera_sm_evt_reg_face_payload_t *)payload;
            rc = m_parent->registerFaceImage(reg_payload->img_ptr,
                                             reg_payload->config,
                                             faceID);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_HANDLE;
            result.handle = faceID;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_CANCEL_PICTURE:
    case QCAMERA_SM_EVT_STOP_RECORDING:
    case QCAMERA_SM_EVT_RELEASE_RECORIDNG_FRAME:
    case QCAMERA_SM_EVT_RELEASE:
        {
            ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
            rc = INVALID_OPERATION;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_EVT_INTERNAL:
        {
            qcamera_sm_internal_evt_payload_t *internal_evt =
                (qcamera_sm_internal_evt_payload_t *)payload;
            switch (internal_evt->evt_type) {
            case QCAMERA_INTERNAL_EVT_FOCUS_UPDATE:
                rc = m_parent->processAutoFocusEvent(internal_evt->focus_data);
                break;
            case QCAMERA_INTERNAL_EVT_PREP_SNAPSHOT_DONE:
                break;
            case QCAMERA_INTERNAL_EVT_FACE_DETECT_RESULT:
                rc = m_parent->processFaceDetectionResult(&internal_evt->faces_data);
                break;
            case QCAMERA_INTERNAL_EVT_HISTOGRAM_STATS:
                rc = m_parent->processHistogramStats(internal_evt->stats_data);
                break;
            case QCAMERA_INTERNAL_EVT_CROP_INFO:
                rc = m_parent->processZoomEvent(internal_evt->crop_data);
                break;
            case QCAMERA_INTERNAL_EVT_ASD_UPDATE:
                rc = m_parent->processASDUpdate(internal_evt->asd_data);
                break;
            case QCAMERA_INTERNAL_EVT_AWB_UPDATE:
                rc = m_parent->processAWBUpdate(internal_evt->awb_data);
                break;
            default:
                ALOGE("%s: Invalid internal event %d in state(%d)",
                            __func__, internal_evt->evt_type, m_state);
                break;
            }
        }
        break;
    case QCAMERA_SM_EVT_EVT_NOTIFY:
        {
            mm_camera_event_t *cam_evt = (mm_camera_event_t *)payload;
            switch (cam_evt->server_event_type) {
            case CAM_EVENT_TYPE_DAEMON_DIED:
                {
                    m_parent->sendEvtNotify(CAMERA_MSG_ERROR,
                                            CAMERA_ERROR_SERVER_DIED,
                                            0);
                }
                break;
            default:
                CDBG_HIGH("%s: no handling for server evt (%d) at this state",
                      __func__, cam_evt->server_event_type);
                break;
            }
        }
        break;
    case QCAMERA_SM_EVT_THERMAL_NOTIFY:
        {
            rc = m_parent->updateThermalLevel(
                    *((qcamera_thermal_level_enum_t *)payload));
        }
        break;
    case QCAMERA_SM_EVT_SNAPSHOT_DONE:
        {
            // No ops, but need to notify
            ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalEvtResult(&result);
        }
       break;
    case QCAMERA_SM_EVT_JPEG_EVT_NOTIFY:
    default:
        ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
        break;
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : procEvtPrepareSnapshotState
 *
 * DESCRIPTION: finite state machine function to handle event in state of
 *              QCAMERA_SM_STATE_PREPARE_SNAPSHOT.
 *
 * PARAMETERS :
 *   @evt      : event to be processed
 *   @payload  : event payload. Can be NULL if not needed.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraStateMachine::procEvtPrepareSnapshotState(qcamera_sm_evt_enum_t evt,
                                                    void *payload)
{
    int32_t rc = NO_ERROR;
    qcamera_api_result_t result;
    memset(&result, 0, sizeof(qcamera_api_result_t));

    switch (evt) {
    case QCAMERA_SM_EVT_SET_PREVIEW_WINDOW:
    case QCAMERA_SM_EVT_SET_CALLBACKS:
    case QCAMERA_SM_EVT_ENABLE_MSG_TYPE:
    case QCAMERA_SM_EVT_DISABLE_MSG_TYPE:
    case QCAMERA_SM_EVT_MSG_TYPE_ENABLED:
    case QCAMERA_SM_EVT_SET_PARAMS:
    case QCAMERA_SM_EVT_GET_PARAMS:
    case QCAMERA_SM_EVT_PUT_PARAMS:
    case QCAMERA_SM_EVT_START_PREVIEW:
    case QCAMERA_SM_EVT_START_NODISPLAY_PREVIEW:
    case QCAMERA_SM_EVT_STOP_PREVIEW:
    case QCAMERA_SM_EVT_PREVIEW_ENABLED:
    case QCAMERA_SM_EVT_RECORDING_ENABLED:
    case QCAMERA_SM_EVT_STORE_METADATA_IN_BUFS:
    case QCAMERA_SM_EVT_DUMP:
    case QCAMERA_SM_EVT_START_AUTO_FOCUS:
    case QCAMERA_SM_EVT_STOP_AUTO_FOCUS:
    case QCAMERA_SM_EVT_START_RECORDING:
    case QCAMERA_SM_EVT_TAKE_PICTURE:
    case QCAMERA_SM_EVT_PREPARE_SNAPSHOT:
    case QCAMERA_SM_EVT_SEND_COMMAND:
    case QCAMERA_SM_EVT_CANCEL_PICTURE:
    case QCAMERA_SM_EVT_STOP_RECORDING:
    case QCAMERA_SM_EVT_RELEASE_RECORIDNG_FRAME:
    case QCAMERA_SM_EVT_RELEASE:
        {
            ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
            rc = INVALID_OPERATION;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_EVT_INTERNAL:
        {
            qcamera_sm_internal_evt_payload_t *internal_evt =
                (qcamera_sm_internal_evt_payload_t *)payload;
            switch (internal_evt->evt_type) {
            case QCAMERA_INTERNAL_EVT_FOCUS_UPDATE:
                rc = m_parent->processAutoFocusEvent(internal_evt->focus_data);
                break;
            case QCAMERA_INTERNAL_EVT_PREP_SNAPSHOT_DONE:
                CDBG("%s: Received QCAMERA_INTERNAL_EVT_PREP_SNAPSHOT_DONE event",
                    __func__);
                m_parent->processPrepSnapshotDoneEvent(internal_evt->prep_snapshot_state);
                m_state = QCAMERA_SM_STATE_PREVIEWING;

                result.status = NO_ERROR;
                result.request_api = QCAMERA_SM_EVT_PREPARE_SNAPSHOT;
                result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
                m_parent->signalAPIResult(&result);
                break;
            case QCAMERA_INTERNAL_EVT_FACE_DETECT_RESULT:
                rc = m_parent->processFaceDetectionResult(&internal_evt->faces_data);
                break;
            case QCAMERA_INTERNAL_EVT_HISTOGRAM_STATS:
                rc = m_parent->processHistogramStats(internal_evt->stats_data);
                break;
            case QCAMERA_INTERNAL_EVT_CROP_INFO:
                rc = m_parent->processZoomEvent(internal_evt->crop_data);
                break;
            case QCAMERA_INTERNAL_EVT_ASD_UPDATE:
                rc = m_parent->processASDUpdate(internal_evt->asd_data);
                break;
            case QCAMERA_INTERNAL_EVT_AWB_UPDATE:
                rc = m_parent->processAWBUpdate(internal_evt->awb_data);
                break;

            default:
                ALOGE("%s: Invalid internal event %d in state(%d)",
                            __func__, internal_evt->evt_type, m_state);
                break;
            }
        }
        break;
    case QCAMERA_SM_EVT_EVT_NOTIFY:
        {
            mm_camera_event_t *cam_evt = (mm_camera_event_t *)payload;
            switch (cam_evt->server_event_type) {
            case CAM_EVENT_TYPE_DAEMON_DIED:
                {
                    m_parent->sendEvtNotify(CAMERA_MSG_ERROR,
                                            CAMERA_ERROR_SERVER_DIED,
                                            0);
                }
                break;
            default:
                ALOGE("%s: Invalid internal event %d in state(%d)",
                            __func__, cam_evt->server_event_type, m_state);
                break;
            }
        }
        break;
    case QCAMERA_SM_EVT_SNAPSHOT_DONE:
        {
            // No ops, but need to notify
            ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalEvtResult(&result);
        }
       break;
    case QCAMERA_SM_EVT_THERMAL_NOTIFY:
    case QCAMERA_SM_EVT_JPEG_EVT_NOTIFY:
    default:
        ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
        break;
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : procEvtPicTakingState
 *
 * DESCRIPTION: finite state machine function to handle event in state of
 *              QCAMERA_SM_STATE_PIC_TAKING.
 *
 * PARAMETERS :
 *   @evt      : event to be processed
 *   @payload  : event payload. Can be NULL if not needed.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraStateMachine::procEvtPicTakingState(qcamera_sm_evt_enum_t evt,
                                                   void *payload)
{
    int32_t rc = NO_ERROR;
    qcamera_api_result_t result;
    memset(&result, 0, sizeof(qcamera_api_result_t));

    switch (evt) {
    case QCAMERA_SM_EVT_SET_PREVIEW_WINDOW:
        {
            // Error setting preview window during previewing
            ALOGE("Cannot set preview window when preview is running");
            rc = INVALID_OPERATION;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_SET_CALLBACKS:
        {
            qcamera_sm_evt_setcb_payload_t *setcbs =
                (qcamera_sm_evt_setcb_payload_t *)payload;
            rc = m_parent->setCallBacks(setcbs->notify_cb,
                                        setcbs->data_cb,
                                        setcbs->data_cb_timestamp,
                                        setcbs->get_memory,
                                        setcbs->user);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_ENABLE_MSG_TYPE:
        {
            rc = m_parent->enableMsgType(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_DISABLE_MSG_TYPE:
        {
            rc = m_parent->disableMsgType(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_MSG_TYPE_ENABLED:
        {
            int enabled = m_parent->msgTypeEnabled(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_ENABLE_FLAG;
            result.enabled = enabled;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_SET_PARAMS:
        {
            bool needRestart = false;
            rc = m_parent->updateParameters((char*)payload, needRestart);
            if (rc == NO_ERROR) {
                rc = m_parent->commitParameterChanges();
            }
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_GET_PARAMS:
        {
            result.params = m_parent->getParameters();
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_PARAMS;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_PUT_PARAMS:
        {
            rc = m_parent->putParameters((char*)payload);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STOP_PREVIEW:
        {
            // cancel picture first
            rc = m_parent->cancelPicture();
            m_state = QCAMERA_SM_STATE_PREVIEW_STOPPED;

            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_PREVIEW_ENABLED:
        {
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_ENABLE_FLAG;
            result.enabled = 0;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_RECORDING_ENABLED:
        {
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_ENABLE_FLAG;
            result.enabled = 0;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STORE_METADATA_IN_BUFS:
        {
            rc = m_parent->storeMetaDataInBuffers(*((int *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_DUMP:
        {
            rc = m_parent->dump(*((int *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_START_AUTO_FOCUS:
        {
            rc = m_parent->autoFocus();
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STOP_AUTO_FOCUS:
        {
            rc = m_parent->cancelAutoFocus();
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_SEND_COMMAND:
        {
            qcamera_sm_evt_command_payload_t *cmd_payload =
                (qcamera_sm_evt_command_payload_t *)payload;
            rc = m_parent->sendCommand(cmd_payload->cmd,
                                       cmd_payload->arg1,
                                       cmd_payload->arg2);
#ifndef VANILLA_HAL
            if ( CAMERA_CMD_LONGSHOT_OFF == cmd_payload->cmd ) {
                // move state to previewing state
                m_state = QCAMERA_SM_STATE_PREVIEWING;
            }
#endif
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_CANCEL_PICTURE:
        {
            rc = m_parent->cancelPicture();
            m_state = QCAMERA_SM_STATE_PREVIEW_STOPPED;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_REG_FACE_IMAGE:
        {
            int32_t faceID = 0;
            qcamera_sm_evt_reg_face_payload_t *reg_payload =
                (qcamera_sm_evt_reg_face_payload_t *)payload;
            rc = m_parent->registerFaceImage(reg_payload->img_ptr,
                                             reg_payload->config,
                                             faceID);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_HANDLE;
            result.handle = faceID;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_TAKE_PICTURE:
        {
           if ( m_parent->isLongshotEnabled() ) {
               rc = m_parent->longShot();
            } else {
                ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
                rc = INVALID_OPERATION;
            }

            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_PREPARE_SNAPSHOT:
    case QCAMERA_SM_EVT_START_RECORDING:
    case QCAMERA_SM_EVT_STOP_RECORDING:
    case QCAMERA_SM_EVT_RELEASE_RECORIDNG_FRAME:
    case QCAMERA_SM_EVT_START_PREVIEW:
    case QCAMERA_SM_EVT_START_NODISPLAY_PREVIEW:
    case QCAMERA_SM_EVT_RELEASE:
        {
            ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
            rc = INVALID_OPERATION;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_EVT_INTERNAL:
        {
            qcamera_sm_internal_evt_payload_t *internal_evt =
                (qcamera_sm_internal_evt_payload_t *)payload;
            switch (internal_evt->evt_type) {
            case QCAMERA_INTERNAL_EVT_FOCUS_UPDATE:
                rc = m_parent->processAutoFocusEvent(internal_evt->focus_data);
                break;
            case QCAMERA_INTERNAL_EVT_PREP_SNAPSHOT_DONE:
                break;
            case QCAMERA_INTERNAL_EVT_FACE_DETECT_RESULT:
                break;
            case QCAMERA_INTERNAL_EVT_HISTOGRAM_STATS:
                break;
            case QCAMERA_INTERNAL_EVT_CROP_INFO:
                rc = m_parent->processZoomEvent(internal_evt->crop_data);
                break;
            case QCAMERA_INTERNAL_EVT_ASD_UPDATE:
                rc = m_parent->processASDUpdate(internal_evt->asd_data);
                break;
            case QCAMERA_INTERNAL_EVT_AWB_UPDATE:
                rc = m_parent->processAWBUpdate(internal_evt->awb_data);
                break;
            default:
                break;
            }
        }
        break;
    case QCAMERA_SM_EVT_EVT_NOTIFY:
        {
            mm_camera_event_t *cam_evt = (mm_camera_event_t *)payload;
            switch (cam_evt->server_event_type) {
            case CAM_EVENT_TYPE_REPROCESS_STAGE_DONE:
                {
                    if ( m_parent->isLongshotEnabled() ) {
                        if(!m_parent->m_postprocessor.getMultipleStages()) {
                            m_parent->m_postprocessor.setMultipleStages(true);
                        }
                        m_parent->playShutter();
                    }
                }
                break;
            case CAM_EVENT_TYPE_DAEMON_DIED:
                {
                    m_parent->sendEvtNotify(CAMERA_MSG_ERROR,
                                            CAMERA_ERROR_SERVER_DIED,
                                            0);
                }
                break;
            default:
                CDBG_HIGH("%s: no handling for server evt (%d) at this state",
                      __func__, cam_evt->server_event_type);
                break;
            }
        }
        break;
    case QCAMERA_SM_EVT_JPEG_EVT_NOTIFY:
        {
            qcamera_jpeg_evt_payload_t *jpeg_job =
                (qcamera_jpeg_evt_payload_t *)payload;
            rc = m_parent->processJpegNotify(jpeg_job);
        }
        break;
    case QCAMERA_SM_EVT_STOP_CAPTURE_CHANNEL:
        {
            bool restartPreview = m_parent->isPreviewRestartEnabled();
            rc = m_parent->stopCaptureChannel(restartPreview);

            if (restartPreview && (NO_ERROR == rc)) {
                rc = m_parent->preparePreview();
                if (NO_ERROR == rc) {
                    m_parent->m_bPreviewStarted = true;
                    rc = m_parent->startPreview();
                }
            }

        }
        break;
    case QCAMERA_SM_EVT_SNAPSHOT_DONE:
        {
            rc = m_parent->cancelPicture();

            bool restartPreview = m_parent->isPreviewRestartEnabled();
            if (restartPreview) {
                m_state = QCAMERA_SM_STATE_PREVIEWING;
            } else {
                m_state = QCAMERA_SM_STATE_PREVIEW_STOPPED;
            }

            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalEvtResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_THERMAL_NOTIFY:
        {
            rc = m_parent->updateThermalLevel(
                    *((qcamera_thermal_level_enum_t *)payload));
        }
        break;
    default:
        ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
        break;
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : procEvtRecordingState
 *
 * DESCRIPTION: finite state machine function to handle event in state of
 *              QCAMERA_SM_STATE_RECORDING.
 *
 * PARAMETERS :
 *   @evt      : event to be processed
 *   @payload  : event payload. Can be NULL if not needed.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraStateMachine::procEvtRecordingState(qcamera_sm_evt_enum_t evt,
                                                   void *payload)
{
    int32_t rc = NO_ERROR;
    qcamera_api_result_t result;
    memset(&result, 0, sizeof(qcamera_api_result_t));

    switch (evt) {
    case QCAMERA_SM_EVT_START_PREVIEW:
    case QCAMERA_SM_EVT_SET_PREVIEW_WINDOW:
        {
            // WA: CTS test VideoSnapshot will try to
            //     start preview during video recording.
            CDBG_HIGH("CTS video restart op");
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_SET_CALLBACKS:
        {
            qcamera_sm_evt_setcb_payload_t *setcbs =
                (qcamera_sm_evt_setcb_payload_t *)payload;
            rc = m_parent->setCallBacks(setcbs->notify_cb,
                                        setcbs->data_cb,
                                        setcbs->data_cb_timestamp,
                                        setcbs->get_memory,
                                        setcbs->user);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_ENABLE_MSG_TYPE:
        {
            rc = m_parent->enableMsgType(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_DISABLE_MSG_TYPE:
        {
            rc = m_parent->disableMsgType(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_MSG_TYPE_ENABLED:
        {
            int enabled = m_parent->msgTypeEnabled(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_ENABLE_FLAG;
            result.enabled = enabled;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_SET_PARAMS:
        {
            bool needRestart = false;
            rc = m_parent->updateParameters((char*)payload, needRestart);
            if (rc == NO_ERROR) {
                if (needRestart) {
                    // cannot set parameters that requires restart during recording
                    ALOGE("%s: Cannot set parameters that requires restart during recording",
                          __func__);
                    rc = BAD_VALUE;
                } else {
                    rc = m_parent->commitParameterChanges();
                }
            }
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_GET_PARAMS:
        {
            result.params = m_parent->getParameters();
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_PARAMS;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_PUT_PARAMS:
        {
            rc = m_parent->putParameters((char*)payload);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_PREVIEW_ENABLED:
        {
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_ENABLE_FLAG;
            result.enabled = 0;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_RECORDING_ENABLED:
        {
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_ENABLE_FLAG;
            result.enabled = 1;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STORE_METADATA_IN_BUFS:
        {
            rc = m_parent->storeMetaDataInBuffers(*((int *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_DUMP:
        {
            rc = m_parent->dump(*((int *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_START_AUTO_FOCUS:
        {
            rc = m_parent->autoFocus();
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STOP_AUTO_FOCUS:
        {
            rc = m_parent->cancelAutoFocus();
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_SEND_COMMAND:
        {
            qcamera_sm_evt_command_payload_t *cmd_payload =
                (qcamera_sm_evt_command_payload_t *)payload;
            rc = m_parent->sendCommand(cmd_payload->cmd,
                                       cmd_payload->arg1,
                                       cmd_payload->arg2);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_TAKE_PICTURE:
        {
            m_state = QCAMERA_SM_STATE_VIDEO_PIC_TAKING;
            rc = m_parent->takeLiveSnapshot();
            if (rc != NO_ERROR) {
                m_state = QCAMERA_SM_STATE_RECORDING;
            }
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_START_RECORDING:
        {
            // no ops here
            CDBG_HIGH("%s: already in recording state, no ops for start_recording", __func__);
            rc = 0;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STOP_RECORDING:
        {
            rc = m_parent->stopRecording();
            m_state = QCAMERA_SM_STATE_PREVIEWING;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STOP_PREVIEW:
        {
            rc = m_parent->stopRecording();
            m_state = QCAMERA_SM_STATE_PREVIEWING;

            rc = m_parent->stopPreview();
            m_state = QCAMERA_SM_STATE_PREVIEW_STOPPED;

            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_RELEASE_RECORIDNG_FRAME:
        {
            rc = m_parent->releaseRecordingFrame((const void *)payload);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_REG_FACE_IMAGE:
        {
            int32_t faceID = 0;
            qcamera_sm_evt_reg_face_payload_t *reg_payload =
                (qcamera_sm_evt_reg_face_payload_t *)payload;
            rc = m_parent->registerFaceImage(reg_payload->img_ptr,
                                             reg_payload->config,
                                             faceID);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_HANDLE;
            result.handle = faceID;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_PREPARE_SNAPSHOT:
        {
            //In Video snapshot, prepare hardware is a no-op.
            result.status = NO_ERROR;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_CANCEL_PICTURE:
    case QCAMERA_SM_EVT_START_NODISPLAY_PREVIEW:
    case QCAMERA_SM_EVT_RELEASE:
        {
            ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
            rc = INVALID_OPERATION;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_EVT_INTERNAL:
        {
            qcamera_sm_internal_evt_payload_t *internal_evt =
                (qcamera_sm_internal_evt_payload_t *)payload;
            switch (internal_evt->evt_type) {
            case QCAMERA_INTERNAL_EVT_FOCUS_UPDATE:
                rc = m_parent->processAutoFocusEvent(internal_evt->focus_data);
                break;
            case QCAMERA_INTERNAL_EVT_PREP_SNAPSHOT_DONE:
                break;
            case QCAMERA_INTERNAL_EVT_FACE_DETECT_RESULT:
                rc = m_parent->processFaceDetectionResult(&internal_evt->faces_data);
                break;
            case QCAMERA_INTERNAL_EVT_HISTOGRAM_STATS:
                rc = m_parent->processHistogramStats(internal_evt->stats_data);
                break;
            case QCAMERA_INTERNAL_EVT_CROP_INFO:
                rc = m_parent->processZoomEvent(internal_evt->crop_data);
                break;
            case QCAMERA_INTERNAL_EVT_ASD_UPDATE:
                rc = m_parent->processASDUpdate(internal_evt->asd_data);
                break;
            case QCAMERA_INTERNAL_EVT_AWB_UPDATE:
                rc = m_parent->processAWBUpdate(internal_evt->awb_data);
                break;
            default:
                break;
            }
        }
        break;
    case QCAMERA_SM_EVT_EVT_NOTIFY:
        {
            mm_camera_event_t *cam_evt = (mm_camera_event_t *)payload;
            switch (cam_evt->server_event_type) {
            case CAM_EVENT_TYPE_DAEMON_DIED:
                {
                    m_parent->sendEvtNotify(CAMERA_MSG_ERROR,
                                            CAMERA_ERROR_SERVER_DIED,
                                            0);
                }
                break;
            default:
                ALOGE("%s: Invalid internal event %d in state(%d)",
                            __func__, cam_evt->server_event_type, m_state);
                break;
            }
        }
        break;
    case QCAMERA_SM_EVT_THERMAL_NOTIFY:
        {
            rc = m_parent->updateThermalLevel(
                    *((qcamera_thermal_level_enum_t *)payload));
        }
        break;
    case QCAMERA_SM_EVT_SNAPSHOT_DONE:
        {
            // No ops, but need to notify
            ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalEvtResult(&result);
        }
       break;
    case QCAMERA_SM_EVT_JPEG_EVT_NOTIFY:
    default:
        ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
        break;
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : procEvtVideoPicTakingState
 *
 * DESCRIPTION: finite state machine function to handle event in state of
 *              QCAMERA_SM_STATE_VIDEO_PIC_TAKING.
 *
 * PARAMETERS :
 *   @evt      : event to be processed
 *   @payload  : event payload. Can be NULL if not needed.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraStateMachine::procEvtVideoPicTakingState(qcamera_sm_evt_enum_t evt,
                                                        void *payload)
{
    int32_t rc = NO_ERROR;
    qcamera_api_result_t result;
    memset(&result, 0, sizeof(qcamera_api_result_t));

    switch (evt) {
    case QCAMERA_SM_EVT_SET_PREVIEW_WINDOW:
        {
            // Error setting preview window during previewing
            ALOGE("Cannot set preview window when preview is running");
            rc = INVALID_OPERATION;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_SET_CALLBACKS:
        {
            qcamera_sm_evt_setcb_payload_t *setcbs =
                (qcamera_sm_evt_setcb_payload_t *)payload;
            rc = m_parent->setCallBacks(setcbs->notify_cb,
                                        setcbs->data_cb,
                                        setcbs->data_cb_timestamp,
                                        setcbs->get_memory,
                                        setcbs->user);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_ENABLE_MSG_TYPE:
        {
            rc = m_parent->enableMsgType(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_DISABLE_MSG_TYPE:
        {
            rc = m_parent->disableMsgType(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_MSG_TYPE_ENABLED:
        {
            int enabled = m_parent->msgTypeEnabled(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_ENABLE_FLAG;
            result.enabled = enabled;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_SET_PARAMS:
        {
            bool needRestart = false;
            rc = m_parent->updateParameters((char*)payload, needRestart);
            if (rc == NO_ERROR) {
                if (needRestart) {
                    // cannot set parameters that requires restart during recording
                    ALOGE("%s: Cannot set parameters that requires restart during recording",
                          __func__);
                    rc = BAD_VALUE;
                } else {
                    rc = m_parent->commitParameterChanges();
                }
            }
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_GET_PARAMS:
        {
            result.params = m_parent->getParameters();
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_PARAMS;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_PUT_PARAMS:
        {
            rc = m_parent->putParameters((char*)payload);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_PREVIEW_ENABLED:
        {
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_ENABLE_FLAG;
            result.enabled = 1;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_RECORDING_ENABLED:
        {
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_ENABLE_FLAG;
            result.enabled = 1;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STORE_METADATA_IN_BUFS:
        {
            rc = m_parent->storeMetaDataInBuffers(*((int *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_DUMP:
        {
            rc = m_parent->dump(*((int *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_START_AUTO_FOCUS:
        {
            rc = m_parent->autoFocus();
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STOP_AUTO_FOCUS:
        {
            rc = m_parent->cancelAutoFocus();
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_SEND_COMMAND:
        {
            qcamera_sm_evt_command_payload_t *cmd_payload =
                (qcamera_sm_evt_command_payload_t *)payload;
            rc = m_parent->sendCommand(cmd_payload->cmd,
                                       cmd_payload->arg1,
                                       cmd_payload->arg2);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STOP_RECORDING:
        {
            rc = m_parent->cancelLiveSnapshot();
            m_state = QCAMERA_SM_STATE_RECORDING;

            rc = m_parent->stopRecording();
            m_state = QCAMERA_SM_STATE_PREVIEWING;

            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_RELEASE_RECORIDNG_FRAME:
        {
            rc = m_parent->releaseRecordingFrame((const void *)payload);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_CANCEL_PICTURE:
        {
            rc = m_parent->cancelLiveSnapshot();
            m_state = QCAMERA_SM_STATE_RECORDING;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_REG_FACE_IMAGE:
        {
            int32_t faceID = 0;
            qcamera_sm_evt_reg_face_payload_t *reg_payload =
                (qcamera_sm_evt_reg_face_payload_t *)payload;
            rc = m_parent->registerFaceImage(reg_payload->img_ptr,
                                             reg_payload->config,
                                             faceID);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_HANDLE;
            result.handle = faceID;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STOP_PREVIEW:
        {
            rc = m_parent->cancelLiveSnapshot();
            m_state = QCAMERA_SM_STATE_RECORDING;

            rc = m_parent->stopRecording();
            m_state = QCAMERA_SM_STATE_PREVIEWING;

            rc = m_parent->stopPreview();
            m_state = QCAMERA_SM_STATE_PREVIEW_STOPPED;

            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_START_RECORDING:
    case QCAMERA_SM_EVT_START_PREVIEW:
    case QCAMERA_SM_EVT_START_NODISPLAY_PREVIEW:
    case QCAMERA_SM_EVT_PREPARE_SNAPSHOT:
    case QCAMERA_SM_EVT_TAKE_PICTURE:
    case QCAMERA_SM_EVT_RELEASE:
        {
            ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
            rc = INVALID_OPERATION;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_EVT_INTERNAL:
        {
            qcamera_sm_internal_evt_payload_t *internal_evt =
                (qcamera_sm_internal_evt_payload_t *)payload;
            switch (internal_evt->evt_type) {
            case QCAMERA_INTERNAL_EVT_FOCUS_UPDATE:
                rc = m_parent->processAutoFocusEvent(internal_evt->focus_data);
                break;
            case QCAMERA_INTERNAL_EVT_PREP_SNAPSHOT_DONE:
                break;
            case QCAMERA_INTERNAL_EVT_FACE_DETECT_RESULT:
                rc = m_parent->processFaceDetectionResult(&internal_evt->faces_data);
                break;
            case QCAMERA_INTERNAL_EVT_HISTOGRAM_STATS:
                rc = m_parent->processHistogramStats(internal_evt->stats_data);
                break;
            case QCAMERA_INTERNAL_EVT_CROP_INFO:
                rc = m_parent->processZoomEvent(internal_evt->crop_data);
                break;
            case QCAMERA_INTERNAL_EVT_ASD_UPDATE:
                rc = m_parent->processASDUpdate(internal_evt->asd_data);
                break;
            case QCAMERA_INTERNAL_EVT_AWB_UPDATE:
                rc = m_parent->processAWBUpdate(internal_evt->awb_data);
                break;
            default:
                break;
            }
        }
        break;
    case QCAMERA_SM_EVT_EVT_NOTIFY:
        {
            mm_camera_event_t *cam_evt = (mm_camera_event_t *)payload;
            switch (cam_evt->server_event_type) {
            case CAM_EVENT_TYPE_DAEMON_DIED:
                {
                    m_parent->sendEvtNotify(CAMERA_MSG_ERROR,
                                            CAMERA_ERROR_SERVER_DIED,
                                            0);
                }
                break;
            default:
                ALOGE("%s: Invalid internal event %d in state(%d)",
                            __func__, cam_evt->server_event_type, m_state);
                break;
            }
        }
        break;
    case QCAMERA_SM_EVT_JPEG_EVT_NOTIFY:
        {
            qcamera_jpeg_evt_payload_t *jpeg_job =
                (qcamera_jpeg_evt_payload_t *)payload;
            rc = m_parent->processJpegNotify(jpeg_job);
        }
        break;
    case QCAMERA_SM_EVT_SNAPSHOT_DONE:
        {
            rc = m_parent->cancelLiveSnapshot();
            m_state = QCAMERA_SM_STATE_RECORDING;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalEvtResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_THERMAL_NOTIFY:
        {
            rc = m_parent->updateThermalLevel(
                    *((qcamera_thermal_level_enum_t *)payload));
        }
        break;
    default:
        ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
        break;
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : procEvtPreviewPicTakingState
 *
 * DESCRIPTION: finite state machine function to handle event in state of
 *              QCAMERA_SM_STATE_PREVIEW_PIC_TAKING.
 *
 * PARAMETERS :
 *   @evt      : event to be processed
 *   @payload  : event payload. Can be NULL if not needed.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraStateMachine::procEvtPreviewPicTakingState(qcamera_sm_evt_enum_t evt,
                                                          void *payload)
{
    int32_t rc = NO_ERROR;
    qcamera_api_result_t result;
    memset(&result, 0, sizeof(qcamera_api_result_t));

    switch (evt) {
    case QCAMERA_SM_EVT_SET_CALLBACKS:
        {
            qcamera_sm_evt_setcb_payload_t *setcbs =
                (qcamera_sm_evt_setcb_payload_t *)payload;
            rc = m_parent->setCallBacks(setcbs->notify_cb,
                                        setcbs->data_cb,
                                        setcbs->data_cb_timestamp,
                                        setcbs->get_memory,
                                        setcbs->user);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_ENABLE_MSG_TYPE:
        {
            rc = m_parent->enableMsgType(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_DISABLE_MSG_TYPE:
        {
            rc = m_parent->disableMsgType(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_MSG_TYPE_ENABLED:
        {
            int enabled = m_parent->msgTypeEnabled(*((int32_t *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_ENABLE_FLAG;
            result.enabled = enabled;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_SET_PARAMS:
        {
            bool needRestart = false;
            rc = m_parent->updateParameters((char*)payload, needRestart);
            if (rc == NO_ERROR) {
                if (needRestart) {
                    // need restart preview for parameters to take effect
                    // stop preview
                    m_parent->stopPreview();
                    // Clear memory pools
                    m_parent->m_memoryPool.clear();
                    // commit parameter changes to server
                    m_parent->commitParameterChanges();
                    // start preview again
                    rc = m_parent->preparePreview();
                    if (rc == NO_ERROR) {
                        rc = m_parent->startPreview();
                        if (rc != NO_ERROR) {
                            m_parent->unpreparePreview();
                        }
                    }
                    if (rc != NO_ERROR) {
                        m_state = QCAMERA_SM_STATE_PIC_TAKING;
                    }
                } else {
                    rc = m_parent->commitParameterChanges();
                }
            }
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_GET_PARAMS:
        {
            result.params = m_parent->getParameters();
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_PARAMS;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_PUT_PARAMS:
        {
            rc = m_parent->putParameters((char*)payload);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_PREVIEW_ENABLED:
        {
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_ENABLE_FLAG;
            result.enabled = 1;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_RECORDING_ENABLED:
        {
            rc = NO_ERROR;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_ENABLE_FLAG;
            result.enabled = 0;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STORE_METADATA_IN_BUFS:
        {
            rc = m_parent->storeMetaDataInBuffers(*((int *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_DUMP:
        {
            rc = m_parent->dump(*((int *)payload));
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_START_AUTO_FOCUS:
        {
            rc = m_parent->autoFocus();
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STOP_AUTO_FOCUS:
        {
            rc = m_parent->cancelAutoFocus();
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_SEND_COMMAND:
        {
            qcamera_sm_evt_command_payload_t *cmd_payload =
                (qcamera_sm_evt_command_payload_t *)payload;
            rc = m_parent->sendCommand(cmd_payload->cmd,
                                       cmd_payload->arg1,
                                       cmd_payload->arg2);
#ifndef VANILLA_HAL
            if ( CAMERA_CMD_LONGSHOT_OFF == cmd_payload->cmd ) {
                // move state to previewing state
                m_state = QCAMERA_SM_STATE_PREVIEWING;
            }
#endif
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_RELEASE_RECORIDNG_FRAME:
        {
            rc = m_parent->releaseRecordingFrame((const void *)payload);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_CANCEL_PICTURE:
        {
            if (m_parent->isZSLMode() || m_parent->isLongshotEnabled()) {
                rc = m_parent->cancelPicture();
            } else {
                rc = m_parent->cancelLiveSnapshot();
            }
            m_state = QCAMERA_SM_STATE_PREVIEWING;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_STOP_PREVIEW:
        {
            if (m_parent->isZSLMode()) {
                // cancel picture first
                rc = m_parent->cancelPicture();
                m_parent->stopChannel(QCAMERA_CH_TYPE_ZSL);
            } else if (m_parent->isLongshotEnabled()) {
                // just cancel picture
                rc = m_parent->cancelPicture();
            } else {
                rc = m_parent->cancelLiveSnapshot();
                m_parent->stopChannel(QCAMERA_CH_TYPE_PREVIEW);
            }
            // unprepare preview
            m_parent->unpreparePreview();
            m_state = QCAMERA_SM_STATE_PREVIEW_STOPPED;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_START_RECORDING:
        {
            if (m_parent->isZSLMode()) {
                ALOGE("%s: cannot handle evt(%d) in state(%d) in ZSL mode",
                      __func__, evt, m_state);
                rc = INVALID_OPERATION;
            } else if (m_parent->isLongshotEnabled()) {
                ALOGE("%s: cannot handle evt(%d) in state(%d) in Longshot mode",
                      __func__, evt, m_state);
                rc = INVALID_OPERATION;
            } else {
                rc = m_parent->startRecording();
                if (rc == NO_ERROR) {
                    m_state = QCAMERA_SM_STATE_VIDEO_PIC_TAKING;
                }
            }
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_REG_FACE_IMAGE:
        {
            int32_t faceID = 0;
            qcamera_sm_evt_reg_face_payload_t *reg_payload =
                (qcamera_sm_evt_reg_face_payload_t *)payload;
            rc = m_parent->registerFaceImage(reg_payload->img_ptr,
                                             reg_payload->config,
                                             faceID);
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_HANDLE;
            result.handle = faceID;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_TAKE_PICTURE:
        {
            if ( m_parent->isLongshotEnabled() ) {
               rc = m_parent->longShot();
            } else {
                ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
                rc = INVALID_OPERATION;
            }

            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_PREPARE_SNAPSHOT:
    case QCAMERA_SM_EVT_STOP_RECORDING:
    case QCAMERA_SM_EVT_START_PREVIEW:
    case QCAMERA_SM_EVT_START_NODISPLAY_PREVIEW:
    case QCAMERA_SM_EVT_SET_PREVIEW_WINDOW:
    case QCAMERA_SM_EVT_RELEASE:
        {
            ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
            rc = INVALID_OPERATION;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalAPIResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_EVT_INTERNAL:
        {
            qcamera_sm_internal_evt_payload_t *internal_evt =
                (qcamera_sm_internal_evt_payload_t *)payload;
            switch (internal_evt->evt_type) {
            case QCAMERA_INTERNAL_EVT_FOCUS_UPDATE:
                rc = m_parent->processAutoFocusEvent(internal_evt->focus_data);
                break;
            case QCAMERA_INTERNAL_EVT_PREP_SNAPSHOT_DONE:
                break;
            case QCAMERA_INTERNAL_EVT_FACE_DETECT_RESULT:
                rc = m_parent->processFaceDetectionResult(&internal_evt->faces_data);
                break;
            case QCAMERA_INTERNAL_EVT_HISTOGRAM_STATS:
                rc = m_parent->processHistogramStats(internal_evt->stats_data);
                break;
            case QCAMERA_INTERNAL_EVT_CROP_INFO:
                rc = m_parent->processZoomEvent(internal_evt->crop_data);
                break;
            case QCAMERA_INTERNAL_EVT_ASD_UPDATE:
                rc = m_parent->processASDUpdate(internal_evt->asd_data);
                break;
            case QCAMERA_INTERNAL_EVT_AWB_UPDATE:
                rc = m_parent->processAWBUpdate(internal_evt->awb_data);
                break;
            default:
                break;
            }
        }
        break;
    case QCAMERA_SM_EVT_EVT_NOTIFY:
        {
            mm_camera_event_t *cam_evt = (mm_camera_event_t *)payload;
            switch (cam_evt->server_event_type) {
            case CAM_EVENT_TYPE_REPROCESS_STAGE_DONE:
                {
                    if ( m_parent->isLongshotEnabled() ) {
                        if(!m_parent->m_postprocessor.getMultipleStages()) {
                            m_parent->m_postprocessor.setMultipleStages(true);
                        }
                        m_parent->playShutter();
                    }
                }
                break;
            case CAM_EVENT_TYPE_DAEMON_DIED:
                {
                    m_parent->sendEvtNotify(CAMERA_MSG_ERROR,
                                            CAMERA_ERROR_SERVER_DIED,
                                            0);
                }
                break;
            default:
                ALOGE("%s: Invalid internal event %d in state(%d)",
                            __func__, cam_evt->server_event_type, m_state);
                break;
            }
        }
        break;
    case QCAMERA_SM_EVT_JPEG_EVT_NOTIFY:
        {
            qcamera_jpeg_evt_payload_t *jpeg_job =
                (qcamera_jpeg_evt_payload_t *)payload;
            rc = m_parent->processJpegNotify(jpeg_job);
        }
        break;
    case QCAMERA_SM_EVT_SNAPSHOT_DONE:
        {
            if (m_parent->isZSLMode() || m_parent->isLongshotEnabled()) {
                rc = m_parent->cancelPicture();
            } else {
                rc = m_parent->cancelLiveSnapshot();
            }
            m_state = QCAMERA_SM_STATE_PREVIEWING;
            result.status = rc;
            result.request_api = evt;
            result.result_type = QCAMERA_API_RESULT_TYPE_DEF;
            m_parent->signalEvtResult(&result);
        }
        break;
    case QCAMERA_SM_EVT_THERMAL_NOTIFY:
        {
            rc = m_parent->updateThermalLevel(
                    *((qcamera_thermal_level_enum_t *)payload));
        }
        break;
    default:
        ALOGE("%s: cannot handle evt(%d) in state(%d)", __func__, evt, m_state);
        break;
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : isPreviewRunning
 *
 * DESCRIPTION: check if preview is in process.
 *
 * PARAMETERS : None
 *
 * RETURN     : true -- preview running
 *              false -- preview stopped
 *==========================================================================*/
bool QCameraStateMachine::isPreviewRunning()
{
    switch (m_state) {
    case QCAMERA_SM_STATE_PREVIEWING:
    case QCAMERA_SM_STATE_RECORDING:
    case QCAMERA_SM_STATE_VIDEO_PIC_TAKING:
    case QCAMERA_SM_STATE_PREVIEW_PIC_TAKING:
    case QCAMERA_SM_STATE_PREPARE_SNAPSHOT:
    case QCAMERA_SM_STATE_PREVIEW_READY:
        return true;
    default:
        return false;
    }
}

/*===========================================================================
 * FUNCTION   : isPreviewReady
 *
 * DESCRIPTION: check if preview is in ready state.
 *
 * PARAMETERS : None
 *
 * RETURN     : true -- preview is in ready state
 *              false -- preview is stopped
 *==========================================================================*/
bool QCameraStateMachine::isPreviewReady()
{
    switch (m_state) {
    case QCAMERA_SM_STATE_PREVIEW_READY:
        return true;
    default:
        return false;
    }
}

/*===========================================================================
 * FUNCTION   : isCaptureRunning
 *
 * DESCRIPTION: check if image capture is in process.
 *
 * PARAMETERS : None
 *
 * RETURN     : true -- capture running
 *              false -- capture stopped
 *==========================================================================*/
bool QCameraStateMachine::isCaptureRunning()
{
    switch (m_state) {
    case QCAMERA_SM_STATE_PIC_TAKING:
    case QCAMERA_SM_STATE_VIDEO_PIC_TAKING:
    case QCAMERA_SM_STATE_PREVIEW_PIC_TAKING:
        return true;
    default:
        return false;
    }
}
/*===========================================================================
 * FUNCTION   : isNonZSLCaptureRunning
 *
 * DESCRIPTION: check if image capture is in process in non ZSL mode.
 *
 * PARAMETERS : None
 *
 * RETURN     : true -- capture running in non ZSL mode
 *              false -- Either in not capture mode or captur is not in non ZSL mode
 *==========================================================================*/
bool QCameraStateMachine::isNonZSLCaptureRunning()
{
    switch (m_state) {
    case QCAMERA_SM_STATE_PIC_TAKING:
        return true;
    default:
        return false;
    }
}


}; // namespace qcamera
