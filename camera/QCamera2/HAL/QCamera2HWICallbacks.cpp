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

#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <utils/Errors.h>
#include <utils/Trace.h>
#include <utils/Timers.h>
#include "QCamera2HWI.h"

namespace qcamera {

/*===========================================================================
 * FUNCTION   : zsl_channel_cb
 *
 * DESCRIPTION: helper function to handle ZSL superbuf callback directly from
 *              mm-camera-interface
 *
 * PARAMETERS :
 *   @recvd_frame : received super buffer
 *   @userdata    : user data ptr
 *
 * RETURN    : None
 *
 * NOTE      : recvd_frame will be released after this call by caller, so if
 *             async operation needed for recvd_frame, it's our responsibility
 *             to save a copy for this variable to be used later.
 *==========================================================================*/
void QCamera2HardwareInterface::zsl_channel_cb(mm_camera_super_buf_t *recvd_frame,
                                               void *userdata)
{
    ATRACE_CALL();
    CDBG_HIGH("[KPI Perf] %s: E",__func__);
    char value[PROPERTY_VALUE_MAX];
    bool dump_raw = false;
    bool dump_yuv = false;
    bool log_matching = false;
    QCamera2HardwareInterface *pme = (QCamera2HardwareInterface *)userdata;
    if (pme == NULL ||
        pme->mCameraHandle == NULL ||
        pme->mCameraHandle->camera_handle != recvd_frame->camera_handle){
       ALOGE("%s: camera obj not valid", __func__);
       return;
    }

    QCameraChannel *pChannel = pme->m_channels[QCAMERA_CH_TYPE_ZSL];
    if (pChannel == NULL ||
        pChannel->getMyHandle() != recvd_frame->ch_id) {
        ALOGE("%s: ZSL channel doesn't exist, return here", __func__);
        return;
    }

    /* indicate the parent that capture is done */
    pme->captureDone();

    // save a copy for the superbuf
    mm_camera_super_buf_t* frame =
               (mm_camera_super_buf_t *)malloc(sizeof(mm_camera_super_buf_t));
    if (frame == NULL) {
        ALOGE("%s: Error allocating memory to save received_frame structure.", __func__);
        pChannel->bufDone(recvd_frame);
        return;
    }
    *frame = *recvd_frame;

    if (recvd_frame->num_bufs > 0) {
        CDBG_HIGH("[KPI Perf] %s: superbuf frame_idx %d", __func__,
            recvd_frame->bufs[0]->frame_idx);
    }

    // DUMP RAW if available
    property_get("persist.camera.zsl_raw", value, "0");
    dump_raw = atoi(value) > 0 ? true : false;
    if (dump_raw) {
        for (uint32_t i = 0; i < recvd_frame->num_bufs; i++) {
            if (recvd_frame->bufs[i]->stream_type == CAM_STREAM_TYPE_RAW) {
                mm_camera_buf_def_t * raw_frame = recvd_frame->bufs[i];
                QCameraStream *pStream = pChannel->getStreamByHandle(raw_frame->stream_id);
                if (NULL != pStream) {
                    pme->dumpFrameToFile(pStream, raw_frame, QCAMERA_DUMP_FRM_RAW);
                }
                break;
            }
        }
    }
    //

    // DUMP YUV before reprocess if needed
    property_get("persist.camera.zsl_yuv", value, "0");
    dump_yuv = atoi(value) > 0 ? true : false;
    if (dump_yuv) {
        for (uint32_t i = 0; i < recvd_frame->num_bufs; i++) {
            if (recvd_frame->bufs[i]->stream_type == CAM_STREAM_TYPE_SNAPSHOT) {
                mm_camera_buf_def_t * yuv_frame = recvd_frame->bufs[i];
                QCameraStream *pStream = pChannel->getStreamByHandle(yuv_frame->stream_id);
                if (NULL != pStream) {
                    pme->dumpFrameToFile(pStream, yuv_frame, QCAMERA_DUMP_FRM_SNAPSHOT);
                }
                break;
            }
        }
    }
    //
    // whether need FD Metadata along with Snapshot frame in ZSL mode
    if(pme->needFDMetadata(QCAMERA_CH_TYPE_ZSL)){
        //Need Face Detection result for snapshot frames
        //Get the Meta Data frames
        mm_camera_buf_def_t *pMetaFrame = NULL;
        for (uint32_t i = 0; i < frame->num_bufs; i++) {
            QCameraStream *pStream = pChannel->getStreamByHandle(frame->bufs[i]->stream_id);
            if (pStream != NULL) {
                if (pStream->isTypeOf(CAM_STREAM_TYPE_METADATA)) {
                    pMetaFrame = frame->bufs[i]; //find the metadata
                    break;
                }
            }
        }

        if(pMetaFrame != NULL){
            cam_metadata_info_t *pMetaData = (cam_metadata_info_t *)pMetaFrame->buffer;
            pMetaData->faces_data.fd_type = QCAMERA_FD_SNAPSHOT; //HARD CODE here before MCT can support
            if(!pMetaData->is_faces_valid){
                pMetaData->faces_data.num_faces_detected = 0;
            }else if(pMetaData->faces_data.num_faces_detected > MAX_ROI){
                ALOGE("%s: Invalid number of faces %d",
                    __func__, pMetaData->faces_data.num_faces_detected);
            }

            qcamera_sm_internal_evt_payload_t *payload =
                (qcamera_sm_internal_evt_payload_t *)malloc(sizeof(qcamera_sm_internal_evt_payload_t));
            if (NULL != payload) {
                memset(payload, 0, sizeof(qcamera_sm_internal_evt_payload_t));
                payload->evt_type = QCAMERA_INTERNAL_EVT_FACE_DETECT_RESULT;
                payload->faces_data = pMetaData->faces_data;
                int32_t rc = pme->processEvt(QCAMERA_SM_EVT_EVT_INTERNAL, payload);
                if (rc != NO_ERROR) {
                    ALOGE("%s: processEvt prep_snapshot failed", __func__);
                    free(payload);
                    payload = NULL;
                }
            } else {
                ALOGE("%s: No memory for prep_snapshot qcamera_sm_internal_evt_payload_t", __func__);
            }
        }
    }

    property_get("persist.camera.dumpmetadata", value, "0");
    int32_t enabled = atoi(value);
    if (enabled) {
        mm_camera_buf_def_t *pMetaFrame = NULL;
        QCameraStream *pStream = NULL;
        for (uint32_t i = 0; i < frame->num_bufs; i++) {
            pStream = pChannel->getStreamByHandle(frame->bufs[i]->stream_id);
            if (pStream != NULL) {
                if (pStream->isTypeOf(CAM_STREAM_TYPE_METADATA)) {
                    pMetaFrame = frame->bufs[i];
                    if (pMetaFrame != NULL &&
                            ((cam_metadata_info_t *)pMetaFrame->buffer)->is_tuning_params_valid) {
                        pme->dumpMetadataToFile(pStream, pMetaFrame, (char *) "ZSL_Snapshot");
                    }
                    break;
                }
            }
        }
    }

    property_get("persist.camera.zsl_matching", value, "0");
    log_matching = atoi(value) > 0 ? true : false;
    if (log_matching) {
        CDBG_HIGH("%s : ZSL super buffer contains:", __func__);
        QCameraStream *pStream = NULL;
        for (uint32_t i = 0; i < frame->num_bufs; i++) {
            pStream = pChannel->getStreamByHandle(frame->bufs[i]->stream_id);
            if (pStream != NULL ) {
                CDBG_HIGH("%s: Buffer with V4Lindex %d frame index %d of type %dTimestamp: %ld %ld",
                        __func__,
                        frame->bufs[i]->buf_idx,
                        frame->bufs[i]->frame_idx,
                        pStream->getMyType(),
                        frame->bufs[i]->ts.tv_sec,
                        frame->bufs[i]->ts.tv_nsec);
            }
        }
    }

    // send to postprocessor
    pme->m_postprocessor.processData(frame);

    CDBG_HIGH("[KPI Perf] %s: X", __func__);
}

/*===========================================================================
 * FUNCTION   : capture_channel_cb_routine
 *
 * DESCRIPTION: helper function to handle snapshot superbuf callback directly from
 *              mm-camera-interface
 *
 * PARAMETERS :
 *   @recvd_frame : received super buffer
 *   @userdata    : user data ptr
 *
 * RETURN    : None
 *
 * NOTE      : recvd_frame will be released after this call by caller, so if
 *             async operation needed for recvd_frame, it's our responsibility
 *             to save a copy for this variable to be used later.
*==========================================================================*/
void QCamera2HardwareInterface::capture_channel_cb_routine(mm_camera_super_buf_t *recvd_frame,
                                                           void *userdata)
{
    ATRACE_CALL();
    char value[PROPERTY_VALUE_MAX];
    CDBG_HIGH("[KPI Perf] %s: E PROFILE_YUV_CB_TO_HAL", __func__);
    bool dump_yuv = false;
    QCamera2HardwareInterface *pme = (QCamera2HardwareInterface *)userdata;
    if (pme == NULL ||
        pme->mCameraHandle == NULL ||
        pme->mCameraHandle->camera_handle != recvd_frame->camera_handle){
        ALOGE("%s: camera obj not valid", __func__);
        return;
    }

    QCameraChannel *pChannel = pme->m_channels[QCAMERA_CH_TYPE_CAPTURE];
    if (pChannel == NULL ||
        pChannel->getMyHandle() != recvd_frame->ch_id) {
        ALOGE("%s: Capture channel doesn't exist, return here", __func__);
        return;
    }

    // save a copy for the superbuf
    mm_camera_super_buf_t* frame =
               (mm_camera_super_buf_t *)malloc(sizeof(mm_camera_super_buf_t));
    if (frame == NULL) {
        ALOGE("%s: Error allocating memory to save received_frame structure.", __func__);
        pChannel->bufDone(recvd_frame);
        return;
    }
    *frame = *recvd_frame;

    // DUMP YUV before reprocess if needed
    property_get("persist.camera.nonzsl.yuv", value, "0");
    dump_yuv = atoi(value) > 0 ? true : false;
    if (dump_yuv) {
        for (uint32_t i= 0 ; i < recvd_frame->num_bufs ; i++) {
            if (recvd_frame->bufs[i]->stream_type == CAM_STREAM_TYPE_SNAPSHOT) {
                mm_camera_buf_def_t * yuv_frame = recvd_frame->bufs[i];
                QCameraStream *pStream = pChannel->getStreamByHandle(yuv_frame->stream_id);
                if (NULL != pStream) {
                    pme->dumpFrameToFile(pStream, yuv_frame, QCAMERA_DUMP_FRM_SNAPSHOT);
                }
                break;
            }
        }
    }

    property_get("persist.camera.dumpmetadata", value, "0");
    int32_t enabled = atoi(value);
    if (enabled) {
        mm_camera_buf_def_t *pMetaFrame = NULL;
        QCameraStream *pStream = NULL;
        for (uint32_t i = 0; i < frame->num_bufs; i++) {
            pStream = pChannel->getStreamByHandle(frame->bufs[i]->stream_id);
            if (pStream != NULL) {
                if (pStream->isTypeOf(CAM_STREAM_TYPE_METADATA)) {
                    pMetaFrame = frame->bufs[i]; //find the metadata
                    if (pMetaFrame != NULL &&
                            ((cam_metadata_info_t *)pMetaFrame->buffer)->is_tuning_params_valid){
                        pme->dumpMetadataToFile(pStream, pMetaFrame, (char *) "Snapshot");
                    }
                    break;
                }
            }
        }
    }

    // Wait on Postproc initialization if needed
    pme->waitDefferedWork(pme->mReprocJob);

    // send to postprocessor
    pme->m_postprocessor.processData(frame);

/* START of test register face image for face authentication */
#ifdef QCOM_TEST_FACE_REGISTER_FACE
    static uint8_t bRunFaceReg = 1;

    if (bRunFaceReg > 0) {
        // find snapshot frame
        QCameraStream *main_stream = NULL;
        mm_camera_buf_def_t *main_frame = NULL;
        for (uint32_t i = 0; i < recvd_frame->num_bufs; i++) {
            QCameraStream *pStream =
                pChannel->getStreamByHandle(recvd_frame->bufs[i]->stream_id);
            if (pStream != NULL) {
                if (pStream->isTypeOf(CAM_STREAM_TYPE_SNAPSHOT)) {
                    main_stream = pStream;
                    main_frame = recvd_frame->bufs[i];
                    break;
                }
            }
        }
        if (main_stream != NULL && main_frame != NULL) {
            int32_t faceId = -1;
            cam_pp_offline_src_config_t config;
            memset(&config, 0, sizeof(cam_pp_offline_src_config_t));
            config.num_of_bufs = 1;
            main_stream->getFormat(config.input_fmt);
            main_stream->getFrameDimension(config.input_dim);
            main_stream->getFrameOffset(config.input_buf_planes.plane_info);
            CDBG_HIGH("DEBUG: registerFaceImage E");
            int32_t rc = pme->registerFaceImage(main_frame->buffer, &config, faceId);
            CDBG_HIGH("DEBUG: registerFaceImage X, ret=%d, faceId=%d", rc, faceId);
            bRunFaceReg = 0;
        }
    }

#endif
/* END of test register face image for face authentication */

    CDBG_HIGH("[KPI Perf] %s: X", __func__);
}

/*===========================================================================
 * FUNCTION   : postproc_channel_cb_routine
 *
 * DESCRIPTION: helper function to handle postprocess superbuf callback directly from
 *              mm-camera-interface
 *
 * PARAMETERS :
 *   @recvd_frame : received super buffer
 *   @userdata    : user data ptr
 *
 * RETURN    : None
 *
 * NOTE      : recvd_frame will be released after this call by caller, so if
 *             async operation needed for recvd_frame, it's our responsibility
 *             to save a copy for this variable to be used later.
*==========================================================================*/
void QCamera2HardwareInterface::postproc_channel_cb_routine(mm_camera_super_buf_t *recvd_frame,
                                                            void *userdata)
{
    ATRACE_CALL();
    CDBG_HIGH("[KPI Perf] %s: E", __func__);
    QCamera2HardwareInterface *pme = (QCamera2HardwareInterface *)userdata;
    if (pme == NULL ||
        pme->mCameraHandle == NULL ||
        pme->mCameraHandle->camera_handle != recvd_frame->camera_handle){
        ALOGE("%s: camera obj not valid", __func__);
        return;
    }

    // save a copy for the superbuf
    mm_camera_super_buf_t* frame =
               (mm_camera_super_buf_t *)malloc(sizeof(mm_camera_super_buf_t));
    if (frame == NULL) {
        ALOGE("%s: Error allocating memory to save received_frame structure.", __func__);
        return;
    }
    *frame = *recvd_frame;

    // send to postprocessor
    if (pme->needDualReprocess()) {
        //send for reprocess again
        pme->m_postprocessor.processData(frame);
    } else {
        pme->m_postprocessor.processPPData(frame);
    }

    CDBG_HIGH("[KPI Perf] %s: X", __func__);
}

/*===========================================================================
 * FUNCTION   : dual_reproc_channel_cb_routine
 *
 * DESCRIPTION: helper function to handle postprocess superbuf callback directly from
 *              mm-camera-interface
 *
 * PARAMETERS :
 *   @recvd_frame : received super buffer
 *   @userdata    : user data ptr
 *
 * RETURN    : None
 *
 * NOTE      : recvd_frame will be released after this call by caller, so if
 *             async operation needed for recvd_frame, it's our responsibility
 *             to save a copy for this variable to be used later.
*==========================================================================*/
void QCamera2HardwareInterface::dual_reproc_channel_cb_routine(mm_camera_super_buf_t *recvd_frame,
                                                            void *userdata)
{
    CDBG_HIGH("[KPI Perf] %s: E", __func__);
    QCamera2HardwareInterface *pme = (QCamera2HardwareInterface *)userdata;
    if (pme == NULL ||
        pme->mCameraHandle == NULL ||
        pme->mCameraHandle->camera_handle != recvd_frame->camera_handle){
        ALOGE("%s: camera obj not valid", __func__);
        return;
    }

    // save a copy for the superbuf
    mm_camera_super_buf_t* frame =
               (mm_camera_super_buf_t *)malloc(sizeof(mm_camera_super_buf_t));
    if (frame == NULL) {
        ALOGE("%s: Error allocating memory to save received_frame structure.", __func__);
        return;
    }
    *frame = *recvd_frame;

    // send to postprocessor
    pme->m_postprocessor.processPPData(frame);

    ATRACE_INT("Camera:Reprocess", 0);
    CDBG_HIGH("[KPI Perf] %s: X", __func__);
}

/*===========================================================================
 * FUNCTION   : preview_stream_cb_routine
 *
 * DESCRIPTION: helper function to handle preview frame from preview stream in
 *              normal case with display.
 *
 * PARAMETERS :
 *   @super_frame : received super buffer
 *   @stream      : stream object
 *   @userdata    : user data ptr
 *
 * RETURN    : None
 *
 * NOTE      : caller passes the ownership of super_frame, it's our
 *             responsibility to free super_frame once it's done. The new
 *             preview frame will be sent to display, and an older frame
 *             will be dequeued from display and needs to be returned back
 *             to kernel for future use.
 *==========================================================================*/
void QCamera2HardwareInterface::preview_stream_cb_routine(mm_camera_super_buf_t *super_frame,
                                                          QCameraStream * stream,
                                                          void *userdata)
{
    ATRACE_CALL();
    CDBG("[KPI Perf] %s : BEGIN", __func__);
    int err = NO_ERROR;
    QCamera2HardwareInterface *pme = (QCamera2HardwareInterface *)userdata;
    QCameraGrallocMemory *memory = (QCameraGrallocMemory *)super_frame->bufs[0]->mem_info;

    if (pme == NULL) {
        ALOGE("%s: Invalid hardware object", __func__);
        free(super_frame);
        return;
    }
    if (memory == NULL) {
        ALOGE("%s: Invalid memory object", __func__);
        free(super_frame);
        return;
    }

    mm_camera_buf_def_t *frame = super_frame->bufs[0];
    if (NULL == frame) {
        ALOGE("%s: preview frame is NLUL", __func__);
        free(super_frame);
        return;
    }

    if (!pme->needProcessPreviewFrame()) {
        ALOGE("%s: preview is not running, no need to process", __func__);
        stream->bufDone(frame->buf_idx);
        free(super_frame);
        return;
    }

    if (pme->needDebugFps()) {
        pme->debugShowPreviewFPS();
    }

    uint32_t idx = frame->buf_idx;
    pme->dumpFrameToFile(stream, frame, QCAMERA_DUMP_FRM_PREVIEW);

    if (pme->mPreviewFrameSkipValid) {
        uint32_t min_frame_idx = pme->mPreviewFrameSkipIdxRange.min_frame_idx;
        uint32_t max_frame_idx = pme->mPreviewFrameSkipIdxRange.max_frame_idx;
        uint32_t current_frame_idx = frame->frame_idx;
        if (current_frame_idx >= max_frame_idx) {
            // Reset the flags when current frame ID >= max frame ID
            pme->mPreviewFrameSkipValid = 0;
            pme->mPreviewFrameSkipIdxRange.min_frame_idx = 0;
            pme->mPreviewFrameSkipIdxRange.max_frame_idx = 0;
        }
        if (current_frame_idx >= min_frame_idx && current_frame_idx <= max_frame_idx) {
            CDBG_HIGH("%s: Skip Preview frame ID %d during flash", __func__, current_frame_idx);
            stream->bufDone(frame->buf_idx);
            free(super_frame);
            return;
        }
    }

    if(pme->m_bPreviewStarted) {
       CDBG_HIGH("[KPI Perf] %s : PROFILE_FIRST_PREVIEW_FRAME", __func__);
       pme->m_bPreviewStarted = false ;
    }

    // Display the buffer.
    CDBG("%p displayBuffer %d E", pme, idx);
    int dequeuedIdx = memory->displayBuffer(idx);
    if (dequeuedIdx < 0 || dequeuedIdx >= memory->getCnt()) {
        CDBG_HIGH("%s: Invalid dequeued buffer index %d from display",
              __func__, dequeuedIdx);
    } else {
        // Return dequeued buffer back to driver
        err = stream->bufDone((uint32_t)dequeuedIdx);
        if ( err < 0) {
            ALOGE("stream bufDone failed %d", err);
        }
    }

    // Handle preview data callback
    if (pme->mDataCb != NULL && pme->msgTypeEnabledWithLock(CAMERA_MSG_PREVIEW_FRAME) > 0) {
        camera_memory_t *previewMem = NULL;
        camera_memory_t *data = NULL;
        size_t previewBufSize;
        cam_dimension_t preview_dim;
        cam_format_t previewFmt;
        stream->getFrameDimension(preview_dim);
        stream->getFormat(previewFmt);

        /* The preview buffer size in the callback should be (width*height*bytes_per_pixel)
         * As all preview formats we support, use 12 bits per pixel, buffer size = previewWidth * previewHeight * 3/2.
         * We need to put a check if some other formats are supported in future. */
        if ((previewFmt == CAM_FORMAT_YUV_420_NV21) ||
            (previewFmt == CAM_FORMAT_YUV_420_NV12) ||
            (previewFmt == CAM_FORMAT_YUV_420_YV12)) {
            if(previewFmt == CAM_FORMAT_YUV_420_YV12) {
                previewBufSize = (((size_t)preview_dim.width + 15) / 16) * 16 *
                            (size_t)preview_dim.height +
                        (((size_t)preview_dim.width / 2 + 15) / 16) * 16 *
                            (size_t)preview_dim.height;
            } else {
                previewBufSize = (size_t)preview_dim.width *
                        (size_t)preview_dim.height * 3 / 2;
            }
            ssize_t bufSize = memory->getSize(idx);
            if((BAD_INDEX != bufSize) && (previewBufSize != (size_t)bufSize)) {
                previewMem = pme->mGetMemory(memory->getFd(idx),
                        previewBufSize, 1, pme->mCallbackCookie);
                if (!previewMem || !previewMem->data) {
                    ALOGE("%s: mGetMemory failed.\n", __func__);
                } else {
                    data = previewMem;
                }
            } else {
                data = memory->getMemory(idx, false);
            }
        } else {
            data = memory->getMemory(idx, false);
            ALOGE("%s: Invalid preview format, buffer size in preview callback may be wrong.",
                    __func__);
        }
        qcamera_callback_argm_t cbArg;
        memset(&cbArg, 0, sizeof(qcamera_callback_argm_t));
        cbArg.cb_type = QCAMERA_DATA_CALLBACK;
        cbArg.msg_type = CAMERA_MSG_PREVIEW_FRAME;
        cbArg.data = data;
        if ( previewMem ) {
            cbArg.user_data = previewMem;
            cbArg.release_cb = releaseCameraMemory;
        }
        cbArg.cookie = pme;
        int32_t rc = pme->m_cbNotifier.notifyCallback(cbArg);
        if (rc != NO_ERROR) {
            ALOGE("%s: fail sending notification", __func__);
            if (previewMem) {
                previewMem->release(previewMem);
            }
        }
    }

    free(super_frame);
    CDBG("[KPI Perf] %s : END", __func__);
    return;
}

/*===========================================================================
 * FUNCTION   : nodisplay_preview_stream_cb_routine
 *
 * DESCRIPTION: helper function to handle preview frame from preview stream in
 *              no-display case
 *
 * PARAMETERS :
 *   @super_frame : received super buffer
 *   @stream      : stream object
 *   @userdata    : user data ptr
 *
 * RETURN    : None
 *
 * NOTE      : caller passes the ownership of super_frame, it's our
 *             responsibility to free super_frame once it's done.
 *==========================================================================*/
void QCamera2HardwareInterface::nodisplay_preview_stream_cb_routine(
                                                          mm_camera_super_buf_t *super_frame,
                                                          QCameraStream *stream,
                                                          void * userdata)
{
    ATRACE_CALL();
    CDBG_HIGH("[KPI Perf] %s E",__func__);
    QCamera2HardwareInterface *pme = (QCamera2HardwareInterface *)userdata;
    if (pme == NULL ||
        pme->mCameraHandle == NULL ||
        pme->mCameraHandle->camera_handle != super_frame->camera_handle){
        ALOGE("%s: camera obj not valid", __func__);
        // simply free super frame
        free(super_frame);
        return;
    }
    mm_camera_buf_def_t *frame = super_frame->bufs[0];
    if (NULL == frame) {
        ALOGE("%s: preview frame is NLUL", __func__);
        free(super_frame);
        return;
    }

    if (!pme->needProcessPreviewFrame()) {
        CDBG_HIGH("%s: preview is not running, no need to process", __func__);
        stream->bufDone(frame->buf_idx);
        free(super_frame);
        return;
    }

    if (pme->needDebugFps()) {
        pme->debugShowPreviewFPS();
    }

    if (pme->mPreviewFrameSkipValid) {
        uint32_t min_frame_idx = pme->mPreviewFrameSkipIdxRange.min_frame_idx;
        uint32_t max_frame_idx = pme->mPreviewFrameSkipIdxRange.max_frame_idx;
        uint32_t current_frame_idx = frame->frame_idx;
        if (current_frame_idx >= max_frame_idx) {
            // Reset the flags when current frame ID >= max frame ID
            pme->mPreviewFrameSkipValid = 0;
            pme->mPreviewFrameSkipIdxRange.min_frame_idx = 0;
            pme->mPreviewFrameSkipIdxRange.max_frame_idx = 0;
        }
        if (current_frame_idx >= min_frame_idx && current_frame_idx <= max_frame_idx) {
            CDBG_HIGH("%s: Skip Preview frame ID %d during flash", __func__, current_frame_idx);
            stream->bufDone(frame->buf_idx);
            free(super_frame);
            return;
        }
    }

    QCameraMemory *previewMemObj = (QCameraMemory *)frame->mem_info;
    camera_memory_t *preview_mem = NULL;
    if (previewMemObj != NULL) {
        preview_mem = previewMemObj->getMemory(frame->buf_idx, false);
    }
    if (NULL != previewMemObj && NULL != preview_mem) {
        pme->dumpFrameToFile(stream, frame, QCAMERA_DUMP_FRM_PREVIEW);

        if (pme->needProcessPreviewFrame() &&
            pme->mDataCb != NULL &&
            pme->msgTypeEnabledWithLock(CAMERA_MSG_PREVIEW_FRAME) > 0 ) {
            qcamera_callback_argm_t cbArg;
            memset(&cbArg, 0, sizeof(qcamera_callback_argm_t));
            cbArg.cb_type = QCAMERA_DATA_CALLBACK;
            cbArg.msg_type = CAMERA_MSG_PREVIEW_FRAME;
            cbArg.data = preview_mem;
            cbArg.user_data = (void *) &frame->buf_idx;
            cbArg.cookie = stream;
            cbArg.release_cb = returnStreamBuffer;
            int32_t rc = pme->m_cbNotifier.notifyCallback(cbArg);
            if (rc != NO_ERROR) {
                ALOGE("%s: fail sending data notify", __func__);
                stream->bufDone(frame->buf_idx);
            }
        } else {
            stream->bufDone(frame->buf_idx);
        }
    }
    free(super_frame);
    CDBG_HIGH("[KPI Perf] %s X",__func__);
}

/*===========================================================================
 * FUNCTION   : postview_stream_cb_routine
 *
 * DESCRIPTION: helper function to handle post frame from postview stream
 *
 * PARAMETERS :
 *   @super_frame : received super buffer
 *   @stream      : stream object
 *   @userdata    : user data ptr
 *
 * RETURN    : None
 *
 * NOTE      : caller passes the ownership of super_frame, it's our
 *             responsibility to free super_frame once it's done.
 *==========================================================================*/
void QCamera2HardwareInterface::postview_stream_cb_routine(mm_camera_super_buf_t *super_frame,
                                                           QCameraStream *stream,
                                                           void *userdata)
{
    ATRACE_CALL();
    int err = NO_ERROR;
    QCamera2HardwareInterface *pme = (QCamera2HardwareInterface *)userdata;
    QCameraGrallocMemory *memory = (QCameraGrallocMemory *)super_frame->bufs[0]->mem_info;

    if (pme == NULL) {
        ALOGE("%s: Invalid hardware object", __func__);
        free(super_frame);
        return;
    }
    if (memory == NULL) {
        ALOGE("%s: Invalid memory object", __func__);
        free(super_frame);
        return;
    }

    CDBG_HIGH("[KPI Perf] %s : BEGIN", __func__);

    mm_camera_buf_def_t *frame = super_frame->bufs[0];
    if (NULL == frame) {
        ALOGE("%s: preview frame is NLUL", __func__);
        free(super_frame);
        return;
    }

    QCameraMemory *memObj = (QCameraMemory *)frame->mem_info;
    if (NULL != memObj) {
        pme->dumpFrameToFile(stream, frame, QCAMERA_DUMP_FRM_THUMBNAIL);
    }

    // Return buffer back to driver
    err = stream->bufDone(frame->buf_idx);
    if ( err < 0) {
        ALOGE("stream bufDone failed %d", err);
    }

    free(super_frame);
    CDBG_HIGH("[KPI Perf] %s : END", __func__);
    return;
}

/*===========================================================================
 * FUNCTION   : video_stream_cb_routine
 *
 * DESCRIPTION: helper function to handle video frame from video stream
 *
 * PARAMETERS :
 *   @super_frame : received super buffer
 *   @stream      : stream object
 *   @userdata    : user data ptr
 *
 * RETURN    : None
 *
 * NOTE      : caller passes the ownership of super_frame, it's our
 *             responsibility to free super_frame once it's done. video
 *             frame will be sent to video encoder. Once video encoder is
 *             done with the video frame, it will call another API
 *             (release_recording_frame) to return the frame back
 *==========================================================================*/
void QCamera2HardwareInterface::video_stream_cb_routine(mm_camera_super_buf_t *super_frame,
                                                        QCameraStream *stream,
                                                        void *userdata)
{
    ATRACE_CALL();
    CDBG("[KPI Perf] %s : BEGIN", __func__);
    QCamera2HardwareInterface *pme = (QCamera2HardwareInterface *)userdata;
    if (pme == NULL ||
        pme->mCameraHandle == NULL ||
        pme->mCameraHandle->camera_handle != super_frame->camera_handle){
        ALOGE("%s: camera obj not valid", __func__);
        // simply free super frame
        free(super_frame);
        return;
    }
    mm_camera_buf_def_t *frame = super_frame->bufs[0];

    if (pme->needDebugFps()) {
        pme->debugShowVideoFPS();
    }
    if(pme->m_bRecordStarted) {
       CDBG_HIGH("[KPI Perf] %s : PROFILE_FIRST_RECORD_FRAME", __func__);
       pme->m_bRecordStarted = false ;
    }
    CDBG_HIGH("%s: Stream(%d), Timestamp: %ld %ld",
          __func__,
          frame->stream_id,
          frame->ts.tv_sec,
          frame->ts.tv_nsec);
    nsecs_t timeStamp;
    if(pme->mParameters.isAVTimerEnabled() == true) {
        timeStamp = (nsecs_t)((frame->ts.tv_sec * 1000000LL) + frame->ts.tv_nsec) * 1000;
    } else {
        timeStamp = nsecs_t(frame->ts.tv_sec) * 1000000000LL + frame->ts.tv_nsec;
    }
    CDBG_HIGH("Send Video frame to services/encoder TimeStamp : %lld", timeStamp);
    QCameraMemory *videoMemObj = (QCameraMemory *)frame->mem_info;
    camera_memory_t *video_mem = NULL;
    if (NULL != videoMemObj) {
        video_mem = videoMemObj->getMemory(frame->buf_idx, (pme->mStoreMetaDataInFrame > 0)? true : false);
    }
    if (NULL != videoMemObj && NULL != video_mem) {
        pme->dumpFrameToFile(stream, frame, QCAMERA_DUMP_FRM_VIDEO);
        if ((pme->mDataCbTimestamp != NULL) &&
            pme->msgTypeEnabledWithLock(CAMERA_MSG_VIDEO_FRAME) > 0) {
            qcamera_callback_argm_t cbArg;
            memset(&cbArg, 0, sizeof(qcamera_callback_argm_t));
            cbArg.cb_type = QCAMERA_DATA_TIMESTAMP_CALLBACK;
            cbArg.msg_type = CAMERA_MSG_VIDEO_FRAME;
            cbArg.data = video_mem;
            cbArg.timestamp = timeStamp;
            int32_t rc = pme->m_cbNotifier.notifyCallback(cbArg);
            if (rc != NO_ERROR) {
                ALOGE("%s: fail sending data notify", __func__);
                stream->bufDone(frame->buf_idx);
            }
        }
    }
    free(super_frame);
    CDBG("[KPI Perf] %s : END", __func__);
}

/*===========================================================================
 * FUNCTION   : snapshot_channel_cb_routine
 *
 * DESCRIPTION: helper function to handle snapshot frame from snapshot channel
 *
 * PARAMETERS :
 *   @super_frame : received super buffer
 *   @userdata    : user data ptr
 *
 * RETURN    : None
 *
 * NOTE      : recvd_frame will be released after this call by caller, so if
 *             async operation needed for recvd_frame, it's our responsibility
 *             to save a copy for this variable to be used later.
 *==========================================================================*/
void QCamera2HardwareInterface::snapshot_channel_cb_routine(mm_camera_super_buf_t *super_frame,
       void *userdata)
{
    ATRACE_CALL();
    char value[PROPERTY_VALUE_MAX];

    CDBG_HIGH("[KPI Perf] %s: E", __func__);
    QCamera2HardwareInterface *pme = (QCamera2HardwareInterface *)userdata;
    if (pme == NULL ||
        pme->mCameraHandle == NULL ||
        pme->mCameraHandle->camera_handle != super_frame->camera_handle){
        ALOGE("%s: camera obj not valid", __func__);
        // simply free super frame
        free(super_frame);
        return;
    }

    QCameraChannel *pChannel = pme->m_channels[QCAMERA_CH_TYPE_SNAPSHOT];
    if (pChannel == NULL) {
        //for 4k2k recording use video channel since snapshot channel doesnt exist.
        pChannel = pme->m_channels[QCAMERA_CH_TYPE_VIDEO];
    }
    if (pChannel == NULL ||
        pChannel->getMyHandle() != super_frame->ch_id) {
        ALOGE("%s: Snapshot channel doesn't exist, return here", __func__);
        return;
    }

    property_get("persist.camera.dumpmetadata", value, "0");
    int32_t enabled = atoi(value);
    if (enabled) {
        QCameraChannel *pChannel = pme->m_channels[QCAMERA_CH_TYPE_SNAPSHOT];
        if (pChannel == NULL ||
            pChannel->getMyHandle() != super_frame->ch_id) {
            ALOGE("%s: Capture channel doesn't exist, return here", __func__);
            return;
        }
        mm_camera_buf_def_t *pMetaFrame = NULL;
        QCameraStream *pStream = NULL;
        for (uint32_t i = 0; i < super_frame->num_bufs; i++) {
            pStream = pChannel->getStreamByHandle(super_frame->bufs[i]->stream_id);
            if (pStream != NULL) {
                if (pStream->isTypeOf(CAM_STREAM_TYPE_METADATA)) {
                    pMetaFrame = super_frame->bufs[i]; //find the metadata
                    if (pMetaFrame != NULL &&
                            ((cam_metadata_info_t *)pMetaFrame->buffer)->is_tuning_params_valid) {
                        pme->dumpMetadataToFile(pStream, pMetaFrame, (char *) "Snapshot");
                    }
                    break;
                }
            }
        }
    }

    // save a copy for the superbuf
    mm_camera_super_buf_t* frame =
               (mm_camera_super_buf_t *)malloc(sizeof(mm_camera_super_buf_t));
    if (frame == NULL) {
        ALOGE("%s: Error allocating memory to save received_frame structure.",
                __func__);
        pChannel->bufDone(super_frame);
        return;
    }
    *frame = *super_frame;

    pme->m_postprocessor.processData(frame);

    CDBG_HIGH("[KPI Perf] %s: X", __func__);
}

/*===========================================================================
 * FUNCTION   : raw_stream_cb_routine
 *
 * DESCRIPTION: helper function to handle raw dump frame from raw stream
 *
 * PARAMETERS :
 *   @super_frame : received super buffer
 *   @stream      : stream object
 *   @userdata    : user data ptr
 *
 * RETURN    : None
 *
 * NOTE      : caller passes the ownership of super_frame, it's our
 *             responsibility to free super_frame once it's done. For raw
 *             frame, there is no need to send to postprocessor for jpeg
 *             encoding. this function will play shutter and send the data
 *             callback to upper layer. Raw frame buffer will be returned
 *             back to kernel, and frame will be free after use.
 *==========================================================================*/
void QCamera2HardwareInterface::raw_stream_cb_routine(mm_camera_super_buf_t * super_frame,
                                                      QCameraStream * /*stream*/,
                                                      void * userdata)
{
    ATRACE_CALL();
    CDBG_HIGH("[KPI Perf] %s : BEGIN", __func__);
    QCamera2HardwareInterface *pme = (QCamera2HardwareInterface *)userdata;
    if (pme == NULL ||
        pme->mCameraHandle == NULL ||
        pme->mCameraHandle->camera_handle != super_frame->camera_handle){
        ALOGE("%s: camera obj not valid", __func__);
        // simply free super frame
        free(super_frame);
        return;
    }

    pme->m_postprocessor.processRawData(super_frame);
    CDBG_HIGH("[KPI Perf] %s : END", __func__);
}

/*===========================================================================
 * FUNCTION   : preview_raw_stream_cb_routine
 *
 * DESCRIPTION: helper function to handle raw frame during standard preview
 *
 * PARAMETERS :
 *   @super_frame : received super buffer
 *   @stream      : stream object
 *   @userdata    : user data ptr
 *
 * RETURN    : None
 *
 * NOTE      : caller passes the ownership of super_frame, it's our
 *             responsibility to free super_frame once it's done.
 *==========================================================================*/
void QCamera2HardwareInterface::preview_raw_stream_cb_routine(mm_camera_super_buf_t * super_frame,
                                                              QCameraStream * stream,
                                                              void * userdata)
{
    ATRACE_CALL();
    CDBG_HIGH("[KPI Perf] %s : BEGIN", __func__);
    char value[PROPERTY_VALUE_MAX];
    bool dump_raw = false;

    QCamera2HardwareInterface *pme = (QCamera2HardwareInterface *)userdata;
    if (pme == NULL ||
        pme->mCameraHandle == NULL ||
        pme->mCameraHandle->camera_handle != super_frame->camera_handle){
        ALOGE("%s: camera obj not valid", __func__);
        // simply free super frame
        free(super_frame);
        return;
    }

    property_get("persist.camera.preview_raw", value, "0");
    dump_raw = atoi(value) > 0 ? true : false;

    for (uint32_t i = 0; i < super_frame->num_bufs; i++) {
        if (super_frame->bufs[i]->stream_type == CAM_STREAM_TYPE_RAW) {
            mm_camera_buf_def_t * raw_frame = super_frame->bufs[i];
            if (NULL != stream) {
                if (dump_raw) {
                    pme->dumpFrameToFile(stream, raw_frame, QCAMERA_DUMP_FRM_RAW);
                }
                stream->bufDone(super_frame->bufs[i]->buf_idx);
            }
            break;
        }
    }

    free(super_frame);

    CDBG_HIGH("[KPI Perf] %s : END", __func__);
}

/*===========================================================================
 * FUNCTION   : snapshot_raw_stream_cb_routine
 *
 * DESCRIPTION: helper function to handle raw frame during standard capture
 *
 * PARAMETERS :
 *   @super_frame : received super buffer
 *   @stream      : stream object
 *   @userdata    : user data ptr
 *
 * RETURN    : None
 *
 * NOTE      : caller passes the ownership of super_frame, it's our
 *             responsibility to free super_frame once it's done.
 *==========================================================================*/
void QCamera2HardwareInterface::snapshot_raw_stream_cb_routine(mm_camera_super_buf_t * super_frame,
                                                               QCameraStream * stream,
                                                               void * userdata)
{
    ATRACE_CALL();
    CDBG_HIGH("[KPI Perf] %s : BEGIN", __func__);
    char value[PROPERTY_VALUE_MAX];
    bool dump_raw = false;

    QCamera2HardwareInterface *pme = (QCamera2HardwareInterface *)userdata;
    if (pme == NULL ||
        pme->mCameraHandle == NULL ||
        pme->mCameraHandle->camera_handle != super_frame->camera_handle){
        ALOGE("%s: camera obj not valid", __func__);
        // simply free super frame
        free(super_frame);
        return;
    }

    property_get("persist.camera.snapshot_raw", value, "0");
    dump_raw = atoi(value) > 0 ? true : false;

    for (uint32_t i = 0; i < super_frame->num_bufs; i++) {
        if (super_frame->bufs[i]->stream_type == CAM_STREAM_TYPE_RAW) {
            mm_camera_buf_def_t * raw_frame = super_frame->bufs[i];
            if (NULL != stream) {
                if (dump_raw) {
                    pme->dumpFrameToFile(stream, raw_frame, QCAMERA_DUMP_FRM_RAW);
                }
                stream->bufDone(super_frame->bufs[i]->buf_idx);
            }
            break;
        }
    }

    free(super_frame);

    CDBG_HIGH("[KPI Perf] %s : END", __func__);
}

/*===========================================================================
 * FUNCTION   : metadata_stream_cb_routine
 *
 * DESCRIPTION: helper function to handle metadata frame from metadata stream
 *
 * PARAMETERS :
 *   @super_frame : received super buffer
 *   @stream      : stream object
 *   @userdata    : user data ptr
 *
 * RETURN    : None
 *
 * NOTE      : caller passes the ownership of super_frame, it's our
 *             responsibility to free super_frame once it's done. Metadata
 *             could have valid entries for face detection result or
 *             histogram statistics information.
 *==========================================================================*/
void QCamera2HardwareInterface::metadata_stream_cb_routine(mm_camera_super_buf_t * super_frame,
                                                           QCameraStream * stream,
                                                           void * userdata)
{
    ATRACE_CALL();
    CDBG("[KPI Perf] %s : BEGIN", __func__);
    QCamera2HardwareInterface *pme = (QCamera2HardwareInterface *)userdata;
    if (pme == NULL ||
        pme->mCameraHandle == NULL ||
        pme->mCameraHandle->camera_handle != super_frame->camera_handle){
        ALOGE("%s: camera obj not valid", __func__);
        // simply free super frame
        free(super_frame);
        return;
    }

    mm_camera_buf_def_t *frame = super_frame->bufs[0];
    cam_metadata_info_t *pMetaData = (cam_metadata_info_t *)frame->buffer;

    if(pme->m_stateMachine.isNonZSLCaptureRunning()&&
       (pMetaData->is_meta_valid == 1) &&
       !pme->mLongshotEnabled) {
       //Make shutter call back in non ZSL mode once raw frame is received from VFE.
       pme->playShutter();
    }

    if (pMetaData->is_preview_frame_skip_valid) {
        pme->mPreviewFrameSkipValid = 1;
        pme->mPreviewFrameSkipIdxRange = pMetaData->preview_frame_skip_idx_range;
        CDBG_HIGH("%s: Skip preview frame ID range min = %d max = %d", __func__,
                   pme->mPreviewFrameSkipIdxRange.min_frame_idx,
                   pme->mPreviewFrameSkipIdxRange.max_frame_idx);
    }

    if (pMetaData->is_tuning_params_valid && pme->mParameters.getRecordingHintValue() == true) {
        //Dump Tuning data for video
        pme->dumpMetadataToFile(stream,frame,(char *)"Video");
    }

    if (pMetaData->is_faces_valid) {
        if (pMetaData->faces_data.num_faces_detected > MAX_ROI) {
            ALOGE("%s: Invalid number of faces %d",
                __func__, pMetaData->faces_data.num_faces_detected);
        } else {
            // process face detection result
            if (pMetaData->faces_data.num_faces_detected)
                CDBG_HIGH("[KPI Perf] %s: PROFILE_NUMBER_OF_FACES_DETECTED %d",__func__,
                           pMetaData->faces_data.num_faces_detected);
            pMetaData->faces_data.fd_type = QCAMERA_FD_PREVIEW; //HARD CODE here before MCT can support
            qcamera_sm_internal_evt_payload_t *payload =
                (qcamera_sm_internal_evt_payload_t *)malloc(sizeof(qcamera_sm_internal_evt_payload_t));
            if (NULL != payload) {
                memset(payload, 0, sizeof(qcamera_sm_internal_evt_payload_t));
                payload->evt_type = QCAMERA_INTERNAL_EVT_FACE_DETECT_RESULT;
                payload->faces_data = pMetaData->faces_data;
                int32_t rc = pme->processEvt(QCAMERA_SM_EVT_EVT_INTERNAL, payload);
                if (rc != NO_ERROR) {
                    ALOGE("%s: processEvt face detection failed", __func__);
                    free(payload);
                    payload = NULL;

                }
            } else {
                ALOGE("%s: No memory for face detect qcamera_sm_internal_evt_payload_t", __func__);
            }
        }
    }

    if (pMetaData->is_stats_valid) {
        // process histogram statistics info
        qcamera_sm_internal_evt_payload_t *payload =
            (qcamera_sm_internal_evt_payload_t *)malloc(sizeof(qcamera_sm_internal_evt_payload_t));
        if (NULL != payload) {
            memset(payload, 0, sizeof(qcamera_sm_internal_evt_payload_t));
            payload->evt_type = QCAMERA_INTERNAL_EVT_HISTOGRAM_STATS;
            payload->stats_data = pMetaData->stats_data;
            int32_t rc = pme->processEvt(QCAMERA_SM_EVT_EVT_INTERNAL, payload);
            if (rc != NO_ERROR) {
                ALOGE("%s: processEvt histogram failed", __func__);
                free(payload);
                payload = NULL;

            }
        } else {
            ALOGE("%s: No memory for histogram qcamera_sm_internal_evt_payload_t", __func__);
        }
    }

    if (pMetaData->is_focus_valid) {
        // process focus info
        qcamera_sm_internal_evt_payload_t *payload =
            (qcamera_sm_internal_evt_payload_t *)malloc(sizeof(qcamera_sm_internal_evt_payload_t));
        if (NULL != payload) {
            memset(payload, 0, sizeof(qcamera_sm_internal_evt_payload_t));
            payload->evt_type = QCAMERA_INTERNAL_EVT_FOCUS_UPDATE;
            payload->focus_data = pMetaData->focus_data;
            payload->focus_data.focused_frame_idx = frame->frame_idx;
            int32_t rc = pme->processEvt(QCAMERA_SM_EVT_EVT_INTERNAL, payload);
            if (rc != NO_ERROR) {
                ALOGE("%s: processEvt focus failed", __func__);
                free(payload);
                payload = NULL;

            }
        } else {
            ALOGE("%s: No memory for focus qcamera_sm_internal_evt_payload_t", __func__);
        }
    }

    if (pMetaData->is_crop_valid) {
        if (pMetaData->crop_data.num_of_streams > MAX_NUM_STREAMS) {
            ALOGE("%s: Invalid num_of_streams %d in crop_data", __func__,
                pMetaData->crop_data.num_of_streams);
        } else {
            qcamera_sm_internal_evt_payload_t *payload =
                (qcamera_sm_internal_evt_payload_t *)malloc(sizeof(qcamera_sm_internal_evt_payload_t));
            if (NULL != payload) {
                memset(payload, 0, sizeof(qcamera_sm_internal_evt_payload_t));
                payload->evt_type = QCAMERA_INTERNAL_EVT_CROP_INFO;
                payload->crop_data = pMetaData->crop_data;
                int32_t rc = pme->processEvt(QCAMERA_SM_EVT_EVT_INTERNAL, payload);
                if (rc != NO_ERROR) {
                    ALOGE("%s: processEvt crop info failed", __func__);
                    free(payload);
                    payload = NULL;

                }
            } else {
                ALOGE("%s: No memory for crop info qcamera_sm_internal_evt_payload_t", __func__);
            }
        }
    }

    if (pMetaData->is_prep_snapshot_done_valid) {
        qcamera_sm_internal_evt_payload_t *payload =
            (qcamera_sm_internal_evt_payload_t *)malloc(sizeof(qcamera_sm_internal_evt_payload_t));
        if (NULL != payload) {
            memset(payload, 0, sizeof(qcamera_sm_internal_evt_payload_t));
            payload->evt_type = QCAMERA_INTERNAL_EVT_PREP_SNAPSHOT_DONE;
            payload->prep_snapshot_state = pMetaData->prep_snapshot_done_state;
            int32_t rc = pme->processEvt(QCAMERA_SM_EVT_EVT_INTERNAL, payload);
            if (rc != NO_ERROR) {
                ALOGE("%s: processEvt prep_snapshot failed", __func__);
                free(payload);
                payload = NULL;

            }
        } else {
            ALOGE("%s: No memory for prep_snapshot qcamera_sm_internal_evt_payload_t", __func__);
        }
    }
    if (pMetaData->is_hdr_scene_data_valid) {
        CDBG("%s: hdr_scene_data: %d %d %f\n",
                   __func__,
                   pMetaData->is_hdr_scene_data_valid,
                   pMetaData->hdr_scene_data.is_hdr_scene,
                   pMetaData->hdr_scene_data.hdr_confidence);
    }
    //Handle this HDR meta data only if capture is not in process
    if (pMetaData->is_hdr_scene_data_valid && !pme->m_stateMachine.isCaptureRunning()) {
        int32_t rc = pme->processHDRData(pMetaData->hdr_scene_data);
        if (rc != NO_ERROR) {
            ALOGE("%s: processHDRData failed", __func__);
        }
    }

    /* Update 3a info */
    if(pMetaData->is_ae_params_valid) {
        pme->mExifParams.ae_params = pMetaData->ae_params;
        pme->mFlashNeeded = pMetaData->ae_params.flash_needed ? true : false;
    }
    if(pMetaData->is_awb_params_valid) {
        pme->mExifParams.awb_params = pMetaData->awb_params;
    }
    if(pMetaData->is_focus_valid) {
        pme->mExifParams.af_params = pMetaData->focus_data;
    }

    if (pMetaData->is_awb_params_valid) {
        CDBG("%s, metadata for awb params.", __func__);
        qcamera_sm_internal_evt_payload_t *payload =
            (qcamera_sm_internal_evt_payload_t *)malloc(sizeof(qcamera_sm_internal_evt_payload_t));
        if (NULL != payload) {
            memset(payload, 0, sizeof(qcamera_sm_internal_evt_payload_t));
            payload->evt_type = QCAMERA_INTERNAL_EVT_AWB_UPDATE;
            payload->awb_data = pMetaData->awb_params;
            int32_t rc = pme->processEvt(QCAMERA_SM_EVT_EVT_INTERNAL, payload);
            if (rc != NO_ERROR) {
                ALOGE("%s: processEvt awb_update failed", __func__);
                free(payload);
                payload = NULL;

            }
        } else {
            ALOGE("%s: No memory for awb_update qcamera_sm_internal_evt_payload_t", __func__);
        }
    }

    /* Update 3A debug info */
    if (pMetaData->is_ae_exif_debug_valid) {
        pme->mExifParams.ae_debug_params_valid = TRUE;
        pme->mExifParams.ae_debug_params = pMetaData->ae_exif_debug_params;
    }
    if (pMetaData->is_awb_exif_debug_valid) {
        pme->mExifParams.awb_debug_params_valid = TRUE;
        pme->mExifParams.awb_debug_params = pMetaData->awb_exif_debug_params;
    }
    if (pMetaData->is_af_exif_debug_valid) {
        pme->mExifParams.af_debug_params_valid = TRUE;
        pme->mExifParams.af_debug_params = pMetaData->af_exif_debug_params;
    }
    if (pMetaData->is_asd_exif_debug_valid) {
        pme->mExifParams.asd_debug_params_valid = TRUE;
        pme->mExifParams.asd_debug_params = pMetaData->asd_exif_debug_params;
    }
    if (pMetaData->is_stats_buffer_exif_debug_valid) {
        pme->mExifParams.stats_debug_params_valid = TRUE;
        pme->mExifParams.stats_debug_params = pMetaData->stats_buffer_exif_debug_params;
    }

    /*Update Sensor info*/
    if (pMetaData->is_sensor_params_valid) {
        pme->mExifParams.sensor_params = pMetaData->sensor_params;
    }

    if (pMetaData->is_asd_decision_valid) {
        qcamera_sm_internal_evt_payload_t *payload =
            (qcamera_sm_internal_evt_payload_t *)malloc(sizeof(qcamera_sm_internal_evt_payload_t));
        if (NULL != payload) {
            memset(payload, 0, sizeof(qcamera_sm_internal_evt_payload_t));
            payload->evt_type = QCAMERA_INTERNAL_EVT_ASD_UPDATE;
            payload->asd_data = pMetaData->scene;
            int32_t rc = pme->processEvt(QCAMERA_SM_EVT_EVT_INTERNAL, payload);
            if (rc != NO_ERROR) {
                ALOGE("%s: processEvt prep_snapshot failed", __func__);
                free(payload);
                payload = NULL;

            }
        } else {
            ALOGE("%s: No memory for prep_snapshot qcamera_sm_internal_evt_payload_t", __func__);
        }
    }

    if (pMetaData->is_chromatix_mobicat_af_valid) {
        memcpy(pme->mExifParams.af_mobicat_params,
            pMetaData->chromatix_mobicat_af_data.private_mobicat_af_data,
            sizeof(pme->mExifParams.af_mobicat_params));
    }

    stream->bufDone(frame->buf_idx);
    free(super_frame);

    CDBG("[KPI Perf] %s : END", __func__);
}

/*===========================================================================
 * FUNCTION   : reprocess_stream_cb_routine
 *
 * DESCRIPTION: helper function to handle reprocess frame from reprocess stream
                (after reprocess, e.g., ZSL snapshot frame after WNR if
 *              WNR is enabled)
 *
 * PARAMETERS :
 *   @super_frame : received super buffer
 *   @stream      : stream object
 *   @userdata    : user data ptr
 *
 * RETURN    : None
 *
 * NOTE      : caller passes the ownership of super_frame, it's our
 *             responsibility to free super_frame once it's done. In this
 *             case, reprocessed frame need to be passed to postprocessor
 *             for jpeg encoding.
 *==========================================================================*/
void QCamera2HardwareInterface::reprocess_stream_cb_routine(mm_camera_super_buf_t * super_frame,
                                                            QCameraStream * /*stream*/,
                                                            void * userdata)
{
    ATRACE_CALL();
    CDBG_HIGH("[KPI Perf] %s: E", __func__);
    QCamera2HardwareInterface *pme = (QCamera2HardwareInterface *)userdata;
    if (pme == NULL ||
        pme->mCameraHandle == NULL ||
        pme->mCameraHandle->camera_handle != super_frame->camera_handle){
        ALOGE("%s: camera obj not valid", __func__);
        // simply free super frame
        free(super_frame);
        return;
    }

    pme->m_postprocessor.processPPData(super_frame);

    CDBG_HIGH("[KPI Perf] %s: X", __func__);
}

/*===========================================================================
 * FUNCTION   : dumpFrameToFile
 *
 * DESCRIPTION: helper function to dump jpeg into file for debug purpose.
 *
 * PARAMETERS :
 *    @data : data ptr
 *    @size : length of data buffer
 *    @index : identifier for data
 *
 * RETURN     : None
 *==========================================================================*/
void QCamera2HardwareInterface::dumpJpegToFile(const void *data,
        size_t size, uint32_t index)
{
    char value[PROPERTY_VALUE_MAX];
    property_get("persist.camera.dumpimg", value, "0");
    uint32_t enabled = (uint32_t) atoi(value);
    uint32_t frm_num = 0;
    uint32_t skip_mode = 0;

    char buf[32];
    cam_dimension_t dim;
    memset(buf, 0, sizeof(buf));
    memset(&dim, 0, sizeof(dim));

    if(((enabled & QCAMERA_DUMP_FRM_JPEG) && data) ||
        ((true == m_bIntEvtPending) && data)) {
        frm_num = ((enabled & 0xffff0000) >> 16);
        if(frm_num == 0) {
            frm_num = 10; //default 10 frames
        }
        if(frm_num > 256) {
            frm_num = 256; //256 buffers cycle around
        }
        skip_mode = ((enabled & 0x0000ff00) >> 8);
        if(skip_mode == 0) {
            skip_mode = 1; //no-skip
        }

        if( mDumpSkipCnt % skip_mode == 0) {
            if((frm_num == 256) && (mDumpFrmCnt >= frm_num)) {
                // reset frame count if cycling
                mDumpFrmCnt = 0;
            }
            if (mDumpFrmCnt <= frm_num) {
#ifdef USE_KK_CODE
                snprintf(buf, sizeof(buf), "/data/%d_%d.jpg", mDumpFrmCnt, index);
#else
                snprintf(buf, sizeof(buf), "/data/misc/camera/%d_%d.jpg", mDumpFrmCnt, index);
#endif
                if (true == m_bIntEvtPending) {
                    strncpy(m_BackendFileName, buf, sizeof(buf));
                    mBackendFileSize = size;
                }

                int file_fd = open(buf, O_RDWR | O_CREAT, 0777);
                if (file_fd >= 0) {
                    ssize_t written_len = write(file_fd, data, size);
                    fchmod(file_fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                    CDBG_HIGH("%s: written number of bytes %d\n", __func__, written_len);
                    close(file_fd);
                } else {
                    ALOGE("%s: fail t open file for image dumping", __func__);
                }
                if (false == m_bIntEvtPending) {
                    mDumpFrmCnt++;
                }
            }
        }
        mDumpSkipCnt++;
    }
}


void QCamera2HardwareInterface::dumpMetadataToFile(QCameraStream *stream,
                                                   mm_camera_buf_def_t *frame,char *type)
{
    char value[PROPERTY_VALUE_MAX];
    uint32_t frm_num = 0;
    cam_metadata_info_t *metadata = (cam_metadata_info_t *)frame->buffer;
    property_get("persist.camera.dumpmetadata", value, "0");
    uint32_t enabled = (uint32_t) atoi(value);
    if (stream == NULL) {
        ALOGE("No op");
        return;
    }

    uint32_t dumpFrmCnt = stream->mDumpMetaFrame;
    if(enabled){
        frm_num = ((enabled & 0xffff0000) >> 16);
        if (frm_num == 0) {
            frm_num = 10; //default 10 frames
        }
        if (frm_num > 256) {
            frm_num = 256; //256 buffers cycle around
        }
        if ((frm_num == 256) && (dumpFrmCnt >= frm_num)) {
            // reset frame count if cycling
            dumpFrmCnt = 0;
        }
        CDBG_HIGH("dumpFrmCnt= %u, frm_num = %u", dumpFrmCnt, frm_num);
        if (dumpFrmCnt < frm_num) {
            char timeBuf[128];
            char buf[32];
            memset(buf, 0, sizeof(buf));
            memset(timeBuf, 0, sizeof(timeBuf));
            time_t current_time;
            struct tm * timeinfo;
            time (&current_time);
            timeinfo = localtime (&current_time);
            if (timeinfo != NULL)
#ifdef USE_KK_CODE
                strftime (timeBuf, sizeof(timeBuf),"/data/%Y%m%d%H%M%S", timeinfo);
#else
                strftime (timeBuf, sizeof(timeBuf),"/data/misc/camera/%Y%m%d%H%M%S", timeinfo);
#endif
            String8 filePath(timeBuf);
            snprintf(buf, sizeof(buf), "%um_%s_%d.bin", dumpFrmCnt, type, frame->frame_idx);
            filePath.append(buf);
            int file_fd = open(filePath.string(), O_RDWR | O_CREAT, 0777);
            if (file_fd > 0) {
                ssize_t written_len = 0;
                metadata->tuning_params.tuning_data_version = TUNING_DATA_VERSION;
                void *data = (void *)((uint8_t *)&metadata->tuning_params.tuning_data_version);
                written_len += write(file_fd, data, sizeof(uint32_t));
                data = (void *)((uint8_t *)&metadata->tuning_params.tuning_sensor_data_size);
                CDBG_HIGH("tuning_sensor_data_size %d",(int)(*(int *)data));
                written_len += write(file_fd, data, sizeof(uint32_t));
                data = (void *)((uint8_t *)&metadata->tuning_params.tuning_vfe_data_size);
                CDBG_HIGH("tuning_vfe_data_size %d",(int)(*(int *)data));
                written_len += write(file_fd, data, sizeof(uint32_t));
                data = (void *)((uint8_t *)&metadata->tuning_params.tuning_cpp_data_size);
                CDBG_HIGH("tuning_cpp_data_size %d",(int)(*(int *)data));
                written_len += write(file_fd, data, sizeof(uint32_t));
                data = (void *)((uint8_t *)&metadata->tuning_params.tuning_cac_data_size);
                CDBG_HIGH("tuning_cac_data_size %d",(int)(*(int *)data));
                written_len += write(file_fd, data, sizeof(uint32_t));
                size_t total_size = metadata->tuning_params.tuning_sensor_data_size;
                data = (void *)((uint8_t *)&metadata->tuning_params.data);
                written_len += write(file_fd, data, total_size);
                total_size = metadata->tuning_params.tuning_vfe_data_size;
                data = (void *)((uint8_t *)&metadata->tuning_params.data[TUNING_VFE_DATA_OFFSET]);
                written_len += write(file_fd, data, total_size);
                total_size = metadata->tuning_params.tuning_cpp_data_size;
                data = (void *)((uint8_t *)&metadata->tuning_params.data[TUNING_CPP_DATA_OFFSET]);
                written_len += write(file_fd, data, total_size);
                total_size = metadata->tuning_params.tuning_cac_data_size;
                data = (void *)((uint8_t *)&metadata->tuning_params.data[TUNING_CAC_DATA_OFFSET]);
                written_len += write(file_fd, data, total_size);
                close(file_fd);
            }else {
                ALOGE("%s: fail t open file for image dumping", __func__);
            }
            dumpFrmCnt++;
        }
    }
    stream->mDumpMetaFrame = dumpFrmCnt;
}
/*===========================================================================
 * FUNCTION   : dumpFrameToFile
 *
 * DESCRIPTION: helper function to dump frame into file for debug purpose.
 *
 * PARAMETERS :
 *    @data : data ptr
 *    @size : length of data buffer
 *    @index : identifier for data
 *    @dump_type : type of the frame to be dumped. Only such
 *                 dump type is enabled, the frame will be
 *                 dumped into a file.
 *
 * RETURN     : None
 *==========================================================================*/
void QCamera2HardwareInterface::dumpFrameToFile(QCameraStream *stream,
        mm_camera_buf_def_t *frame, uint32_t dump_type)
{
    char value[PROPERTY_VALUE_MAX];
    property_get("persist.camera.dumpimg", value, "0");
    uint32_t enabled = (uint32_t) atoi(value);
    uint32_t frm_num = 0;
    uint32_t skip_mode = 0;

    if (stream)
        mDumpFrmCnt = stream->mDumpFrame;

    if(enabled & QCAMERA_DUMP_FRM_MASK_ALL) {
        if((enabled & dump_type) && stream && frame) {
            frm_num = ((enabled & 0xffff0000) >> 16);
            if(frm_num == 0) {
                frm_num = 10; //default 10 frames
            }
            if(frm_num > 256) {
                frm_num = 256; //256 buffers cycle around
            }
            skip_mode = ((enabled & 0x0000ff00) >> 8);
            if(skip_mode == 0) {
                skip_mode = 1; //no-skip
            }
            if(stream->mDumpSkipCnt == 0)
                stream->mDumpSkipCnt = 1;

            if( stream->mDumpSkipCnt % skip_mode == 0) {
                if((frm_num == 256) && (mDumpFrmCnt >= frm_num)) {
                    // reset frame count if cycling
                    mDumpFrmCnt = 0;
                }
                if (mDumpFrmCnt <= frm_num) {
                    char buf[32];
                    char timeBuf[128];
                    time_t current_time;
                    struct tm * timeinfo;


                    time (&current_time);
                    timeinfo = localtime (&current_time);
                    memset(buf, 0, sizeof(buf));

                    cam_dimension_t dim;
                    memset(&dim, 0, sizeof(dim));
                    stream->getFrameDimension(dim);

                    cam_frame_len_offset_t offset;
                    memset(&offset, 0, sizeof(cam_frame_len_offset_t));
                    stream->getFrameOffset(offset);

                    if (timeinfo != NULL)
#ifdef USE_KK_CODE
                        strftime (timeBuf, sizeof(timeBuf),"/data/%Y%m%d%H%M%S", timeinfo);
#else
                        strftime (timeBuf, sizeof(timeBuf),"/data/misc/camera/%Y%m%d%H%M%S", timeinfo);
#endif
                    String8 filePath(timeBuf);
                    switch (dump_type) {
                    case QCAMERA_DUMP_FRM_PREVIEW:
                        {
                            snprintf(buf, sizeof(buf), "%dp_%dx%d_%d.yuv",
                                     mDumpFrmCnt, dim.width, dim.height, frame->frame_idx);
                        }
                        break;
                    case QCAMERA_DUMP_FRM_THUMBNAIL:
                        {
                            snprintf(buf, sizeof(buf), "%dt_%dx%d_%d.yuv",
                                     mDumpFrmCnt, dim.width, dim.height, frame->frame_idx);
                        }
                        break;
                    case QCAMERA_DUMP_FRM_SNAPSHOT:
                        {
                            snprintf(buf, sizeof(buf), "%ds_%dx%d_%d.yuv",
                                     mDumpFrmCnt, dim.width, dim.height, frame->frame_idx);
                        }
                        break;
                    case QCAMERA_DUMP_FRM_VIDEO:
                        {
                            snprintf(buf, sizeof(buf), "%dv_%dx%d_%d.yuv",
                                     mDumpFrmCnt, dim.width, dim.height, frame->frame_idx);
                        }
                        break;
                    case QCAMERA_DUMP_FRM_RAW:
                        {
                            snprintf(buf, sizeof(buf), "%dr_%dx%d_%d.raw",
                                     mDumpFrmCnt, offset.mp[0].stride,
                                     offset.mp[0].scanline, frame->frame_idx);
                        }
                        break;
                    default:
                        ALOGE("%s: Not supported for dumping stream type %d",
                              __func__, dump_type);
                        return;
                    }

                    filePath.append(buf);
                    int file_fd = open(filePath.string(), O_RDWR | O_CREAT, 0777);
                    if (file_fd > 0) {
                        void *data = NULL;
                        ssize_t written_len = 0;

                        for (uint32_t i = 0; i < offset.num_planes; i++) {
                            uint32_t index = offset.mp[i].offset;
                            if (i > 0) {
                                index += offset.mp[i-1].len;
                            }
                            for (int j = 0; j < offset.mp[i].height; j++) {
                                data = (void *)((uint8_t *)frame->buffer + index);
                                written_len += write(file_fd, data,
                                        (size_t)offset.mp[i].width);
                                index += (uint32_t)offset.mp[i].stride;
                            }
                        }

                        CDBG_HIGH("%s: written number of bytes %d\n", __func__, written_len);
                        close(file_fd);
                    } else {
                        ALOGE("%s: fail t open file for image dumping", __func__);
                    }
                    mDumpFrmCnt++;
                }
            }
            stream->mDumpSkipCnt++;
        }
    } else {
        mDumpFrmCnt = 0;
    }
    if (stream)
        stream->mDumpFrame = mDumpFrmCnt;
}

/*===========================================================================
 * FUNCTION   : debugShowVideoFPS
 *
 * DESCRIPTION: helper function to log video frame FPS for debug purpose.
 *
 * PARAMETERS : None
 *
 * RETURN     : None
 *==========================================================================*/
void QCamera2HardwareInterface::debugShowVideoFPS()
{
    static int n_vFrameCount = 0;
    static int n_vLastFrameCount = 0;
    static nsecs_t n_vLastFpsTime = 0;
    static double n_vFps = 0;
    n_vFrameCount++;
    nsecs_t now = systemTime();
    nsecs_t diff = now - n_vLastFpsTime;
    if (diff > ms2ns(250)) {
        n_vFps = (((double)(n_vFrameCount - n_vLastFrameCount)) *
                (double)(s2ns(1))) / (double)diff;
        ALOGE("Video Frames Per Second: %.4f", n_vFps);
        n_vLastFpsTime = now;
        n_vLastFrameCount = n_vFrameCount;
    }
}

/*===========================================================================
 * FUNCTION   : debugShowPreviewFPS
 *
 * DESCRIPTION: helper function to log preview frame FPS for debug purpose.
 *
 * PARAMETERS : None
 *
 * RETURN     : None
 *==========================================================================*/
void QCamera2HardwareInterface::debugShowPreviewFPS()
{
    static int n_pFrameCount = 0;
    static int n_pLastFrameCount = 0;
    static nsecs_t n_pLastFpsTime = 0;
    static double n_pFps = 0;
    n_pFrameCount++;
    nsecs_t now = systemTime();
    nsecs_t diff = now - n_pLastFpsTime;
    if (diff > ms2ns(250)) {
        n_pFps = (((double)(n_pFrameCount - n_pLastFrameCount)) *
                (double)(s2ns(1))) / (double)diff;
        CDBG_HIGH("[KPI Perf] %s: PROFILE_PREVIEW_FRAMES_PER_SECOND : %.4f", __func__, n_pFps);
        n_pLastFpsTime = now;
        n_pLastFrameCount = n_pFrameCount;
    }
}

/*===========================================================================
 * FUNCTION   : ~QCameraCbNotifier
 *
 * DESCRIPTION: Destructor for exiting the callback context.
 *
 * PARAMETERS : None
 *
 * RETURN     : None
 *==========================================================================*/
QCameraCbNotifier::~QCameraCbNotifier()
{
}

/*===========================================================================
 * FUNCTION   : exit
 *
 * DESCRIPTION: exit notify thread.
 *
 * PARAMETERS : None
 *
 * RETURN     : None
 *==========================================================================*/
void QCameraCbNotifier::exit()
{
    mActive = false;
    mProcTh.exit();
}

/*===========================================================================
 * FUNCTION   : releaseNotifications
 *
 * DESCRIPTION: callback for releasing data stored in the callback queue.
 *
 * PARAMETERS :
 *   @data      : data to be released
 *   @user_data : context data
 *
 * RETURN     : None
 *==========================================================================*/
void QCameraCbNotifier::releaseNotifications(void *data, void *user_data)
{
    qcamera_callback_argm_t *arg = ( qcamera_callback_argm_t * ) data;

    if ( ( NULL != arg ) && ( NULL != user_data ) ) {
        if ( arg->release_cb ) {
            arg->release_cb(arg->user_data, arg->cookie, FAILED_TRANSACTION);
        }
    }
}

/*===========================================================================
 * FUNCTION   : matchSnapshotNotifications
 *
 * DESCRIPTION: matches snapshot data callbacks
 *
 * PARAMETERS :
 *   @data      : data to match
 *   @user_data : context data
 *
 * RETURN     : bool match
 *              true - match found
 *              false- match not found
 *==========================================================================*/
bool QCameraCbNotifier::matchSnapshotNotifications(void *data,
                                                   void */*user_data*/)
{
    qcamera_callback_argm_t *arg = ( qcamera_callback_argm_t * ) data;
    if ( NULL != arg ) {
        if ( QCAMERA_DATA_SNAPSHOT_CALLBACK == arg->cb_type ) {
            return true;
        }
    }

    return false;
}

/*===========================================================================
 * FUNCTION   : matchPreviewNotifications
 *
 * DESCRIPTION: matches preview data callbacks
 *
 * PARAMETERS :
 *   @data      : data to match
 *   @user_data : context data
 *
 * RETURN     : bool match
 *              true - match found
 *              false- match not found
 *==========================================================================*/
bool QCameraCbNotifier::matchPreviewNotifications(void *data,
        void */*user_data*/)
{
    qcamera_callback_argm_t *arg = ( qcamera_callback_argm_t * ) data;
    if (NULL != arg) {
        if ((QCAMERA_DATA_CALLBACK == arg->cb_type) &&
                (CAMERA_MSG_PREVIEW_FRAME == arg->msg_type)) {
            return true;
        }
    }

    return false;
}

/*===========================================================================
 * FUNCTION   : cbNotifyRoutine
 *
 * DESCRIPTION: callback thread which interfaces with the upper layers
 *              given input commands.
 *
 * PARAMETERS :
 *   @data    : context data
 *
 * RETURN     : None
 *==========================================================================*/
void * QCameraCbNotifier::cbNotifyRoutine(void * data)
{
    int running = 1;
    int ret;
    QCameraCbNotifier *pme = (QCameraCbNotifier *)data;
    QCameraCmdThread *cmdThread = &pme->mProcTh;
    cmdThread->setName("CAM_cbNotify");
    uint8_t isSnapshotActive = FALSE;
    bool longShotEnabled = false;
    uint32_t numOfSnapshotExpected = 0;
    uint32_t numOfSnapshotRcvd = 0;
    int32_t cbStatus = NO_ERROR;

    CDBG("%s: E", __func__);
    do {
        do {
            ret = cam_sem_wait(&cmdThread->cmd_sem);
            if (ret != 0 && errno != EINVAL) {
                CDBG("%s: cam_sem_wait error (%s)",
                           __func__, strerror(errno));
                return NULL;
            }
        } while (ret != 0);

        camera_cmd_type_t cmd = cmdThread->getCmd();
        CDBG("%s: get cmd %d", __func__, cmd);
        switch (cmd) {
        case CAMERA_CMD_TYPE_START_DATA_PROC:
            {
                isSnapshotActive = TRUE;
                numOfSnapshotExpected = pme->mParent->numOfSnapshotsExpected();
                longShotEnabled = pme->mParent->isLongshotEnabled();
                numOfSnapshotRcvd = 0;
            }
            break;
        case CAMERA_CMD_TYPE_STOP_DATA_PROC:
            {
                pme->mDataQ.flushNodes(matchSnapshotNotifications);
                isSnapshotActive = FALSE;

                numOfSnapshotExpected = 0;
                numOfSnapshotRcvd = 0;
            }
            break;
        case CAMERA_CMD_TYPE_DO_NEXT_JOB:
            {
                qcamera_callback_argm_t *cb =
                    (qcamera_callback_argm_t *)pme->mDataQ.dequeue();
                cbStatus = NO_ERROR;
                if (NULL != cb) {
                    CDBG("%s: cb type %d received",
                          __func__,
                          cb->cb_type);

                    if (pme->mParent->msgTypeEnabledWithLock(cb->msg_type)) {
                        switch (cb->cb_type) {
                        case QCAMERA_NOTIFY_CALLBACK:
                            {
                                if (cb->msg_type == CAMERA_MSG_FOCUS) {
                                    ATRACE_INT("Camera:AutoFocus", 0);
                                    CDBG_HIGH("[KPI Perf] %s : PROFILE_SENDING_FOCUS_EVT_TO APP",
                                            __func__);
                                }
                                if (pme->mNotifyCb) {
                                    pme->mNotifyCb(cb->msg_type,
                                                  cb->ext1,
                                                  cb->ext2,
                                                  pme->mCallbackCookie);
                                } else {
                                    ALOGE("%s : notify callback not set!",
                                          __func__);
                                }
                            }
                            break;
                        case QCAMERA_DATA_CALLBACK:
                            {
                                if (pme->mDataCb) {
                                    pme->mDataCb(cb->msg_type,
                                                 cb->data,
                                                 cb->index,
                                                 cb->metadata,
                                                 pme->mCallbackCookie);
                                } else {
                                    ALOGE("%s : data callback not set!",
                                          __func__);
                                }
                            }
                            break;
                        case QCAMERA_DATA_TIMESTAMP_CALLBACK:
                            {
                                if(pme->mDataCbTimestamp) {
                                    pme->mDataCbTimestamp(cb->timestamp,
                                                          cb->msg_type,
                                                          cb->data,
                                                          cb->index,
                                                          pme->mCallbackCookie);
                                } else {
                                    ALOGE("%s:data cb with tmp not set!",
                                          __func__);
                                }
                            }
                            break;
                        case QCAMERA_DATA_SNAPSHOT_CALLBACK:
                            {
                                if (TRUE == isSnapshotActive && pme->mDataCb ) {
                                    if (!longShotEnabled) {
                                        numOfSnapshotRcvd++;
                                        if (numOfSnapshotExpected > 0 &&
                                            numOfSnapshotExpected == numOfSnapshotRcvd) {
                                            // notify HWI that snapshot is done
                                            pme->mParent->processSyncEvt(QCAMERA_SM_EVT_SNAPSHOT_DONE,
                                                                         NULL);
                                        }
                                    }
                                    pme->mDataCb(cb->msg_type,
                                                 cb->data,
                                                 cb->index,
                                                 cb->metadata,
                                                 pme->mCallbackCookie);
                                }
                            }
                            break;
                        default:
                            {
                                ALOGE("%s : invalid cb type %d",
                                      __func__,
                                      cb->cb_type);
                                cbStatus = BAD_VALUE;
                            }
                            break;
                        };
                    } else {
                        ALOGE("%s : cb message type %d not enabled!",
                              __func__,
                              cb->msg_type);
                        cbStatus = INVALID_OPERATION;
                    }
                    if ( cb->release_cb ) {
                        cb->release_cb(cb->user_data, cb->cookie, cbStatus);
                    }
                    delete cb;
                } else {
                    ALOGE("%s: invalid cb type passed", __func__);
                }
            }
            break;
        case CAMERA_CMD_TYPE_EXIT:
            {
                running = 0;
                pme->mDataQ.flush();
            }
            break;
        default:
            break;
        }
    } while (running);
    CDBG("%s: X", __func__);

    return NULL;
}

/*===========================================================================
 * FUNCTION   : notifyCallback
 *
 * DESCRIPTION: Enqueus pending callback notifications for the upper layers.
 *
 * PARAMETERS :
 *   @cbArgs  : callback arguments
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraCbNotifier::notifyCallback(qcamera_callback_argm_t &cbArgs)
{
    if (!mActive) {
        ALOGE("%s: notify thread is not active", __func__);
        return UNKNOWN_ERROR;
    }

    qcamera_callback_argm_t *cbArg = new qcamera_callback_argm_t();
    if (NULL == cbArg) {
        ALOGE("%s: no mem for qcamera_callback_argm_t", __func__);
        return NO_MEMORY;
    }
    memset(cbArg, 0, sizeof(qcamera_callback_argm_t));
    *cbArg = cbArgs;

    if (mDataQ.enqueue((void *)cbArg)) {
        return mProcTh.sendCmd(CAMERA_CMD_TYPE_DO_NEXT_JOB, FALSE, FALSE);
    } else {
        ALOGE("%s: Error adding cb data into queue", __func__);
        delete cbArg;
        return UNKNOWN_ERROR;
    }
}

/*===========================================================================
 * FUNCTION   : setCallbacks
 *
 * DESCRIPTION: Initializes the callback functions, which would be used for
 *              communication with the upper layers and launches the callback
 *              context in which the callbacks will occur.
 *
 * PARAMETERS :
 *   @notifyCb          : notification callback
 *   @dataCb            : data callback
 *   @dataCbTimestamp   : data with timestamp callback
 *   @callbackCookie    : callback context data
 *
 * RETURN     : None
 *==========================================================================*/
void QCameraCbNotifier::setCallbacks(camera_notify_callback notifyCb,
                                     camera_data_callback dataCb,
                                     camera_data_timestamp_callback dataCbTimestamp,
                                     void *callbackCookie)
{
    if ( ( NULL == mNotifyCb ) &&
         ( NULL == mDataCb ) &&
         ( NULL == mDataCbTimestamp ) &&
         ( NULL == mCallbackCookie ) ) {
        mNotifyCb = notifyCb;
        mDataCb = dataCb;
        mDataCbTimestamp = dataCbTimestamp;
        mCallbackCookie = callbackCookie;
        mActive = true;
        mProcTh.launch(cbNotifyRoutine, this);
    } else {
        ALOGE("%s : Camera callback notifier already initialized!",
              __func__);
    }
}

/*===========================================================================
 * FUNCTION   : flushPreviewNotifications
 *
 * DESCRIPTION: flush all pending preview notifications
 *              from the notifier queue
 *
 * PARAMETERS : None
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraCbNotifier::flushPreviewNotifications()
{
    if (!mActive) {
        ALOGE("%s: notify thread is not active", __func__);
        return UNKNOWN_ERROR;
    }

    mDataQ.flushNodes(matchPreviewNotifications);

    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : startSnapshots
 *
 * DESCRIPTION: Enables snapshot mode
 *
 * PARAMETERS : None
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraCbNotifier::startSnapshots()
{
    return mProcTh.sendCmd(CAMERA_CMD_TYPE_START_DATA_PROC, FALSE, TRUE);
}

/*===========================================================================
 * FUNCTION   : stopSnapshots
 *
 * DESCRIPTION: Disables snapshot processing mode
 *
 * PARAMETERS : None
 *
 * RETURN     : None
 *==========================================================================*/
void QCameraCbNotifier::stopSnapshots()
{
    mProcTh.sendCmd(CAMERA_CMD_TYPE_STOP_DATA_PROC, FALSE, TRUE);
}

}; // namespace qcamera
