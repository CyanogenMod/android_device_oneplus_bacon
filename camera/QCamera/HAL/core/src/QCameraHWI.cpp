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

/*#error uncomment this for compiler test!*/

#define ALOG_NIDEBUG 0

#define LOG_TAG "QCameraHWI"
#include <utils/Log.h>
#include <utils/threads.h>
#include <cutils/properties.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "QCameraHAL.h"
#include "QCameraHWI.h"

/* QCameraHardwareInterface class implementation goes here*/
/* following code implement the contol logic of this class*/

namespace android {

extern void stream_cb_routine(mm_camera_super_buf_t *bufs,
                       void *userdata);

void QCameraHardwareInterface::superbuf_cb_routine(mm_camera_super_buf_t *recvd_frame, void *userdata)
{
    ALOGE("%s: E",__func__);
    QCameraHardwareInterface *pme = (QCameraHardwareInterface *)userdata;
    if(pme == NULL){
       ALOGE("%s: pme is null", __func__);
       return;
    }

    mm_camera_super_buf_t* frame =
           (mm_camera_super_buf_t *)malloc(sizeof(mm_camera_super_buf_t));
    if (frame == NULL) {
        ALOGE("%s: Error allocating memory to save received_frame structure.", __func__);
        for (int i=0; i<recvd_frame->num_bufs; i++) {
             if (recvd_frame->bufs[i] != NULL) {
                 if (recvd_frame->bufs[i]->p_mobicat_info) {
                    free(recvd_frame->bufs[i]->p_mobicat_info);
                    recvd_frame->bufs[i]->p_mobicat_info = NULL;
                 }
                 pme->mCameraHandle->ops->qbuf(recvd_frame->camera_handle,
                                               recvd_frame->ch_id,
                                               recvd_frame->bufs[i]);
                 pme->cache_ops((QCameraHalMemInfo_t *)(recvd_frame->bufs[i]->mem_info),
                                recvd_frame->bufs[i]->buffer,
                                ION_IOC_INV_CACHES);
             }
        }
        return;
    }
    memcpy(frame, recvd_frame, sizeof(mm_camera_super_buf_t));
    if(pme->mHdrInfo.hdr_on) {
        pme->mHdrInfo.recvd_frame[pme->mHdrInfo.num_raw_received] = frame;

        ALOGE("hdl %d, ch_id %d, buf_num %d, bufidx0 %d, bufidx1 %d",
              pme->mHdrInfo.recvd_frame[pme->mHdrInfo.num_raw_received]->
              camera_handle, pme->mHdrInfo.recvd_frame[pme->mHdrInfo.
              num_raw_received]->ch_id, pme->mHdrInfo.recvd_frame[pme->
              mHdrInfo.num_raw_received]->num_bufs, pme->mHdrInfo.
              recvd_frame[pme->mHdrInfo.num_raw_received]->bufs[0]->buf_idx,
              pme->mHdrInfo.recvd_frame[pme->mHdrInfo.num_raw_received]->
              bufs[1]->buf_idx);

        pme->mHdrInfo.num_raw_received++;

        ALOGE("%s Total %d Received %d frames, still need to receive %d frames",
              __func__, pme->mHdrInfo.num_frame, pme->mHdrInfo.num_raw_received,
              (pme->mHdrInfo.num_frame - pme->mHdrInfo.num_raw_received));

        if (pme->mHdrInfo.num_raw_received == pme->mHdrInfo.num_frame) {
            ALOGE(" Received all %d YUV frames, Invoke HDR",
                  pme->mHdrInfo.num_raw_received);
            pme->doHdrProcessing();
        }
    } else {
        /* enqueu to superbuf queue */
        pme->mSuperBufQueue.enqueue(frame);

        /* notify dataNotify thread that new super buf is avail
         * check if it's done with current JPEG notification and
         * a new encoding job could be conducted*/
        pme->mNotifyTh->sendCmd(CAMERA_CMD_TYPE_DO_NEXT_JOB, FALSE, FALSE);
    }
   ALOGE("%s: X", __func__);

}

void QCameraHardwareInterface::snapshot_jpeg_cb(jpeg_job_status_t status,
                             uint8_t thumbnailDroppedFlag,
                             uint32_t client_hdl,
                             uint32_t jobId,
                             uint8_t* out_data,
                             uint32_t data_size,
                             void *userdata)
{
    ALOGE("%s: E", __func__);
    camera_jpeg_encode_cookie_t *cookie =
        (camera_jpeg_encode_cookie_t *)userdata;
    if(cookie == NULL){
       ALOGE("%s: userdata is null", __func__);
       return;
    }
    QCameraHardwareInterface *pme = (QCameraHardwareInterface *)cookie->userdata;
    if(pme == NULL){
       ALOGE("%s: pme is null", __func__);
       return;
    }

    /* no use of src frames, return them to kernel */
    for(int i = 0; i< cookie->src_frame->num_bufs; i++) {
        if (cookie->src_frame->bufs[i]->p_mobicat_info) {
            free(cookie->src_frame->bufs[i]->p_mobicat_info);
            cookie->src_frame->bufs[i]->p_mobicat_info = NULL;
        }
        pme->mCameraHandle->ops->qbuf(cookie->src_frame->camera_handle,
                                      cookie->src_frame->ch_id,
                                      cookie->src_frame->bufs[i]);
        pme->cache_ops((QCameraHalMemInfo_t *)(cookie->src_frame->bufs[i]->mem_info),
                      cookie->src_frame->bufs[i]->buffer,
                      ION_IOC_INV_CACHES);
    }
    free(cookie->src_frame);
    cookie->src_frame = NULL;

    receiveCompleteJpegPicture(status,
                               thumbnailDroppedFlag,
                               client_hdl,
                               jobId,
                               out_data,
                               data_size,
                               pme);

    /* free sink frame */
    free(out_data);
    /* free cookie */
    free(cookie);

    ALOGE("%s: X", __func__);
}

void QCameraHardwareInterface::releaseAppCBData(app_notify_cb_t *app_cb)
{
    if (app_cb->argm_data_cb.user_data != NULL) {
        QCameraHalHeap_t *heap = (QCameraHalHeap_t *)app_cb->argm_data_cb.user_data;
        releaseHeapMem(heap);
    }
}

void QCameraHardwareInterface::releaseNofityData(void *data, void *user_data)
{
    QCameraHardwareInterface *pme = (QCameraHardwareInterface *)user_data;
    if (NULL != pme) {
        pme->releaseAppCBData((app_notify_cb_t *)data);
    }
}

void QCameraHardwareInterface::releaseSuperBuf(mm_camera_super_buf_t *super_buf)
{
    if (NULL != super_buf) {
        for(int i = 0; i< super_buf->num_bufs; i++) {
            if (super_buf->bufs[i]->p_mobicat_info) {
                free(super_buf->bufs[i]->p_mobicat_info);
                super_buf->bufs[i]->p_mobicat_info = NULL;
            }
            mCameraHandle->ops->qbuf(super_buf->camera_handle,
                                     super_buf->ch_id,
                                     super_buf->bufs[i]);
            cache_ops((QCameraHalMemInfo_t *)(super_buf->bufs[i]->mem_info),
                      super_buf->bufs[i]->buffer,
                      ION_IOC_INV_CACHES);
        }
    }
}

void QCameraHardwareInterface::releaseProcData(void *data, void *user_data)
{
    QCameraHardwareInterface *pme = (QCameraHardwareInterface *)user_data;
    if (NULL != pme) {
        pme->releaseSuperBuf((mm_camera_super_buf_t *)data);
    }
}

void *QCameraHardwareInterface::dataNotifyRoutine(void *data)
{
    int running = 1;
    int ret;
    QCameraHardwareInterface *pme = (QCameraHardwareInterface *)data;
    QCameraCmdThread *cmdThread = pme->mNotifyTh;
    uint8_t isEncoding = FALSE;
    uint8_t isActive = FALSE;
    uint32_t numOfSnapshotExpected = 0;
    uint32_t numOfSnapshotRcvd = 0;

    ALOGD("%s: E", __func__);
    do {
        do {
            ret = sem_wait(&cmdThread->cmd_sem);
            if (ret != 0 && errno != EINVAL) {
                ALOGE("%s: sem_wait error (%s)",
                           __func__, strerror(errno));
                return NULL;
            }
        } while (ret != 0);

        /* we got notified about new cmd avail in cmd queue */
        camera_cmd_type_t cmd = cmdThread->getCmd();
        ALOGD("%s: get cmd %d", __func__, cmd);
        switch (cmd) {
        case CAMERA_CMD_TYPE_START_DATA_PROC:
            isActive = TRUE;
            /* init flag to FALSE */
            isEncoding = FALSE;
            numOfSnapshotExpected = pme->getNumOfSnapshots();
            numOfSnapshotRcvd = 0;
            break;
        case CAMERA_CMD_TYPE_STOP_DATA_PROC:
            /* flush jpeg data queue */
            pme->mNotifyDataQueue.flush();

            isActive = FALSE;
            /* set flag to FALSE */
            isEncoding = FALSE;
            numOfSnapshotExpected = 0;
            numOfSnapshotRcvd = 0;
            break;
        case CAMERA_CMD_TYPE_DO_NEXT_JOB:
            {
                if (TRUE == isActive) {
                    /* first check if there is any pending jpeg notify */
                    app_notify_cb_t *app_cb =
                        (app_notify_cb_t *)pme->mNotifyDataQueue.dequeue();
                    if (NULL != app_cb) {
                        /* send notify to upper layer */
                        if (app_cb->notifyCb) {
                            ALOGE("%s: evt notify cb", __func__);
                            app_cb->notifyCb(app_cb->argm_notify.msg_type,
                                             app_cb->argm_notify.ext1,
                                             app_cb->argm_notify.ext2,
                                             app_cb->argm_notify.cookie);
                        }
                        if (app_cb->dataCb) {
                            ALOGE("%s: data notify cb", __func__);
                            app_cb->dataCb(app_cb->argm_data_cb.msg_type,
                                           app_cb->argm_data_cb.data,
                                           app_cb->argm_data_cb.index,
                                           app_cb->argm_data_cb.metadata,
                                           app_cb->argm_data_cb.cookie);
                            if (CAMERA_MSG_COMPRESSED_IMAGE == app_cb->argm_data_cb.msg_type) {
                                numOfSnapshotRcvd++;
                                isEncoding = FALSE;
                            }
                        }

                        /* free app_cb */
                        pme->releaseAppCBData(app_cb);
                        free(app_cb);
                    }

                    if ((FALSE == isEncoding) && !pme->mSuperBufQueue.is_empty()) {
                        isEncoding = TRUE;
                        /* notify processData thread to do next encoding job */
                        pme->mDataProcTh->sendCmd(CAMERA_CMD_TYPE_DO_NEXT_JOB, FALSE, FALSE);
                    }

                    if (numOfSnapshotExpected > 0 &&
                        numOfSnapshotExpected == numOfSnapshotRcvd) {
                        pme->cancelPictureInternal();
                    }
                } else {
                    /* do no op if not active */
                    app_notify_cb_t *app_cb =
                        (app_notify_cb_t *)pme->mNotifyDataQueue.dequeue();
                    if (NULL != app_cb) {
                        /* free app_cb */
                        pme->releaseAppCBData(app_cb);
                        free(app_cb);
                    }
                }
            }
            break;
        case CAMERA_CMD_TYPE_EXIT:
            {
                /* flush jpeg data queue */
                pme->mNotifyDataQueue.flush();
                running = 0;
            }
            break;
        default:
            break;
        }
    } while (running);
    ALOGD("%s: X", __func__);
    return NULL;
}

void *QCameraHardwareInterface::dataProcessRoutine(void *data)
{
    int running = 1;
    int ret;
    uint8_t is_active = FALSE;
    QCameraHardwareInterface *pme = (QCameraHardwareInterface *)data;
    QCameraCmdThread *cmdThread = pme->mDataProcTh;
    uint32_t current_jobId = 0;

    ALOGD("%s: E", __func__);
    do {
        do {
            ret = sem_wait(&cmdThread->cmd_sem);
            if (ret != 0 && errno != EINVAL) {
                ALOGE("%s: sem_wait error (%s)",
                           __func__, strerror(errno));
                return NULL;
            }
        } while (ret != 0);

        /* we got notified about new cmd avail in cmd queue */
        camera_cmd_type_t cmd = cmdThread->getCmd();
        ALOGD("%s: get cmd %d", __func__, cmd);
        switch (cmd) {
        case CAMERA_CMD_TYPE_START_DATA_PROC:
            is_active = TRUE;
            break;
        case CAMERA_CMD_TYPE_STOP_DATA_PROC:
            {
                is_active = FALSE;
                /* abort current job if it's running */
                if (current_jobId > 0) {
                    pme->mJpegHandle.abort_job(pme->mJpegClientHandle, current_jobId);
                    current_jobId = 0;
                }
                /* flush superBufQueue */
                pme->mSuperBufQueue.flush();
                /* signal cmd is completed */
                sem_post(&cmdThread->sync_sem);
            }
            break;
        case CAMERA_CMD_TYPE_DO_NEXT_JOB:
            {
                ALOGD("%s: active is %d", __func__, is_active);
                if (is_active == TRUE) {
                    /* first check if there is any pending jpeg notify */
                    mm_camera_super_buf_t *super_buf =
                        (mm_camera_super_buf_t *)pme->mSuperBufQueue.dequeue();
                    if (NULL != super_buf) {
                        //play shutter sound
                        if(!pme->mShutterSoundPlayed){
                            pme->notifyShutter(true);
                        }
                        pme->notifyShutter(false);
                        pme->mShutterSoundPlayed = false;

                        if (pme->isRawSnapshot()) {
                            receiveRawPicture(super_buf, pme);

                            /*free superbuf*/
                            pme->releaseSuperBuf(super_buf);
                            free(super_buf);
                        } else{
                            ret = pme->encodeData(super_buf, &current_jobId);

                             if (NO_ERROR != ret) {
                                 pme->releaseSuperBuf(super_buf);
                                 free(super_buf);
                                 pme->sendDataNotify(CAMERA_MSG_COMPRESSED_IMAGE,
                                                     NULL,
                                                     0,
                                                     NULL,
                                                     NULL);
                             }
                        }
                    }
                } else {
                    /* not active, simply return buf and do no op */
                    mm_camera_super_buf_t *super_buf =
                        (mm_camera_super_buf_t *)pme->mSuperBufQueue.dequeue();
                    if (NULL != super_buf) {
                        pme->releaseSuperBuf(super_buf);
                        free(super_buf);
                    }
                }
            }
            break;
        case CAMERA_CMD_TYPE_EXIT:
            /* abort current job if it's running */
            if (current_jobId > 0) {
                pme->mJpegHandle.abort_job(pme->mJpegClientHandle, current_jobId);
                current_jobId = 0;
            }
            /* flush super buf queue */
            pme->mSuperBufQueue.flush();
            running = 0;
            break;
        default:
            break;
        }
    } while (running);
    ALOGD("%s: X", __func__);
    return NULL;
}

void QCameraHardwareInterface::notifyShutter(bool play_shutter_sound){
     ALOGV("%s : E", __func__);
     if(mNotifyCb){
         mNotifyCb(CAMERA_MSG_SHUTTER, 0, play_shutter_sound, mCallbackCookie);
     }
     ALOGV("%s : X", __func__);
}

status_t QCameraHardwareInterface::encodeData(mm_camera_super_buf_t* recvd_frame,
                                              uint32_t *jobId)
{
    ALOGV("%s : E", __func__);
    int32_t ret = NO_ERROR;
    mm_jpeg_job jpg_job;
    mm_camera_buf_def_t *main_frame = NULL;
    mm_camera_buf_def_t *thumb_frame = NULL;
    src_image_buffer_info *main_buf_info = NULL;
    src_image_buffer_info *thumb_buf_info = NULL;
    QCameraHalMemInfo_t *main_mem_info = NULL;
    QCameraHalMemInfo_t *thumb_mem_info = NULL;

    uint8_t src_img_num = recvd_frame->num_bufs;
    int i;

    *jobId = 0;

    QCameraStream *main_stream = mStreams[MM_CAMERA_SNAPSHOT_MAIN];
    for (i = 0; i < recvd_frame->num_bufs; i++) {
        if (main_stream->mStreamId == recvd_frame->bufs[i]->stream_id) {
            main_frame = recvd_frame->bufs[i];
            break;
        }
    }
    if(main_frame == NULL){
       ALOGE("%s : Main frame is NULL", __func__);
       return ret;
    }
    main_mem_info = &mSnapshotMemory.mem_info[main_frame->buf_idx];

    // send upperlayer callback for raw image (data or notify, not both)
    app_notify_cb_t *app_cb = (app_notify_cb_t *)malloc(sizeof(app_notify_cb_t));
    if (app_cb != NULL) {
        memset(app_cb, 0, sizeof(app_notify_cb_t));

        if((mDataCb) && (mMsgEnabled & CAMERA_MSG_RAW_IMAGE)){
            app_cb->dataCb = mDataCb;
            app_cb->argm_data_cb.msg_type = CAMERA_MSG_RAW_IMAGE;
            app_cb->argm_data_cb.cookie = mCallbackCookie;
            app_cb->argm_data_cb.data = mSnapshotMemory.camera_memory[main_frame->buf_idx];
            app_cb->argm_data_cb.index = 1;
            app_cb->argm_data_cb.metadata = NULL;
            app_cb->argm_data_cb.user_data = NULL;
        }
        if((mNotifyCb) && (mMsgEnabled & CAMERA_MSG_RAW_IMAGE_NOTIFY)){
            app_cb->notifyCb = mNotifyCb;
            app_cb->argm_notify.msg_type = CAMERA_MSG_RAW_IMAGE_NOTIFY;
            app_cb->argm_notify.cookie = mCallbackCookie;
            app_cb->argm_notify.ext1 = 0;
            app_cb->argm_notify.ext2 = 0;
        }

        /* enqueue jpeg_data into jpeg data queue */
        if ((app_cb->dataCb || app_cb->notifyCb) && mNotifyDataQueue.enqueue((void *)app_cb)) {
            mNotifyTh->sendCmd(CAMERA_CMD_TYPE_DO_NEXT_JOB, FALSE, FALSE);
        } else {
            free(app_cb);
        }
    } else {
        ALOGE("%s: No mem for app_notify_cb_t", __func__);
    }

    camera_jpeg_encode_cookie_t *cookie =
        (camera_jpeg_encode_cookie_t *)malloc(sizeof(camera_jpeg_encode_cookie_t));
    if (NULL == cookie) {
        ALOGE("%s : no mem for cookie", __func__);
        return -1;
    }
    cookie->src_frame = recvd_frame;
    cookie->userdata = this;

    dumpFrameToFile(main_frame, HAL_DUMP_FRM_MAIN);

    QCameraStream *thumb_stream = NULL;
    if (recvd_frame->num_bufs > 1) {
        /* has thumbnail */
        if(!isZSLMode()) {
            thumb_stream = mStreams[MM_CAMERA_SNAPSHOT_THUMBNAIL]; //mStreamSnapThumb;
        } else {
            thumb_stream = mStreams[MM_CAMERA_PREVIEW]; //mStreamDisplay;
        }
        for (i = 0; i < recvd_frame->num_bufs; i++) {
            if (thumb_stream->mStreamId == recvd_frame->bufs[i]->stream_id) {
                thumb_frame = recvd_frame->bufs[i];
                break;
            }
        }
        if (NULL != thumb_frame) {
            if(thumb_stream == mStreams[MM_CAMERA_SNAPSHOT_THUMBNAIL]) {
                thumb_mem_info = &mThumbnailMemory.mem_info[thumb_frame->buf_idx];
            } else {
                if (isNoDisplayMode()) {
                    thumb_mem_info = &mNoDispPreviewMemory.mem_info[thumb_frame->buf_idx];
                } else {
                    thumb_mem_info = &mPreviewMemory.mem_info[thumb_frame->buf_idx];
                }
            }
        }
    }  else if(mStreams[MM_CAMERA_SNAPSHOT_THUMBNAIL]->mWidth &&
               mStreams[MM_CAMERA_SNAPSHOT_THUMBNAIL]->mHeight) {
        /*thumbnail is required, not YUV thumbnail, borrow main image*/
        thumb_stream = main_stream;
        thumb_frame = main_frame;
        src_img_num++;
    }

    if (thumb_stream) {
        dumpFrameToFile(thumb_frame, HAL_DUMP_FRM_THUMBNAIL);
    }

    int jpeg_quality = getJpegQuality();
    if (jpeg_quality <= 0) {
        jpeg_quality = 85;
    }

    memset(&jpg_job, 0, sizeof(mm_jpeg_job));
    jpg_job.job_type = JPEG_JOB_TYPE_ENCODE;
    jpg_job.encode_job.userdata = cookie;
    jpg_job.encode_job.jpeg_cb = QCameraHardwareInterface::snapshot_jpeg_cb;
    jpg_job.encode_job.encode_parm.exif_data = getExifData();
    jpg_job.encode_job.encode_parm.exif_numEntries = getExifTableNumEntries();
    jpg_job.encode_job.encode_parm.rotation = getJpegRotation();
    ALOGV("%s: jpeg rotation is set to %d", __func__, jpg_job.encode_job.encode_parm.rotation);
    jpg_job.encode_job.encode_parm.buf_info.src_imgs.src_img_num = src_img_num;
    jpg_job.encode_job.encode_parm.buf_info.src_imgs.is_video_frame = FALSE;

    if (mMobiCatEnabled) {
        main_frame->p_mobicat_info = (cam_exif_tags_t*)malloc(sizeof(cam_exif_tags_t));
        if ((main_frame->p_mobicat_info != NULL) &&
             mCameraHandle->ops->get_parm(mCameraHandle->camera_handle, MM_CAMERA_PARM_MOBICAT,
                 main_frame->p_mobicat_info)
                 == MM_CAMERA_OK) {
                 ALOGV("%s:%d] Mobicat enabled %p %d", __func__, __LINE__,
                       main_frame->p_mobicat_info->tags,
                       main_frame->p_mobicat_info->data_len);
        } else {
              ALOGE("MM_CAMERA_PARM_MOBICAT get failed");
        }
    }
    if (mMobiCatEnabled && main_frame->p_mobicat_info) {
        jpg_job.encode_job.encode_parm.hasmobicat = 1;
        jpg_job.encode_job.encode_parm.mobicat_data = (uint8_t *)main_frame->p_mobicat_info->tags;
        jpg_job.encode_job.encode_parm.mobicat_data_length = main_frame->p_mobicat_info->data_len;
     } else {
        jpg_job.encode_job.encode_parm.hasmobicat = 0;
     }

     // fill in the src_img info
    //main img
    main_buf_info = &jpg_job.encode_job.encode_parm.buf_info.src_imgs.src_img[JPEG_SRC_IMAGE_TYPE_MAIN];
    main_buf_info->type = JPEG_SRC_IMAGE_TYPE_MAIN;
    main_buf_info->color_format = getColorfmtFromImgFmt(main_stream->mFormat);
    main_buf_info->quality = jpeg_quality;
    main_buf_info->src_image[0].fd = main_frame->fd;
    main_buf_info->src_image[0].buf_vaddr = (uint8_t*) main_frame->buffer;
    main_buf_info->src_dim.width = main_stream->mWidth;
    main_buf_info->src_dim.height = main_stream->mHeight;
    main_buf_info->out_dim.width = mPictureWidth;
    main_buf_info->out_dim.height = mPictureHeight;
    memcpy(&main_buf_info->crop, &main_stream->mCrop, sizeof(image_crop_t));

    ALOGD("%s : Main Image :Input Dimension %d x %d output Dimension = %d X %d",
          __func__, main_buf_info->src_dim.width, main_buf_info->src_dim.height,
          main_buf_info->out_dim.width, main_buf_info->out_dim.height);
    ALOGD("%s : Main Image :Crop %d x %d, offset = (%d, %d)",
          __func__, main_buf_info->crop.width, main_buf_info->crop.height,
          main_buf_info->crop.offset_x, main_buf_info->crop.offset_y);
    main_buf_info->img_fmt = JPEG_SRC_IMAGE_FMT_YUV;
    main_buf_info->num_bufs = 1;
    main_buf_info->src_image[0].offset = main_stream->mFrameOffsetInfo;
    ALOGD("%s : setting main image offset info, len = %d, offset = %d",
          __func__, main_stream->mFrameOffsetInfo.mp[0].len,
          main_stream->mFrameOffsetInfo.mp[0].offset);

    cache_ops(main_mem_info, main_frame->buffer, ION_IOC_CLEAN_INV_CACHES);

    if (thumb_frame && thumb_stream) {
        /* fill in thumbnail src img encode param */
        thumb_buf_info = &jpg_job.encode_job.encode_parm.buf_info.src_imgs.src_img[JPEG_SRC_IMAGE_TYPE_THUMB];
        thumb_buf_info->type = JPEG_SRC_IMAGE_TYPE_THUMB;
        thumb_buf_info->color_format = getColorfmtFromImgFmt(thumb_stream->mFormat);
        //thumb_buf_info->quality = jpeg_quality;
        thumb_buf_info->quality = 75; //hardcoded for now, will be calculated in encoder code later
        thumb_buf_info->src_dim.width = thumb_stream->mWidth;
        thumb_buf_info->src_dim.height = thumb_stream->mHeight;
        thumb_buf_info->out_dim.width = thumbnailWidth;
        thumb_buf_info->out_dim.height = thumbnailHeight;
        memcpy(&thumb_buf_info->crop, &thumb_stream->mCrop, sizeof(image_crop_t));
        ALOGD("%s : Thumanail :Input Dimension %d x %d output Dimension = %d X %d",
          __func__, thumb_buf_info->src_dim.width, thumb_buf_info->src_dim.height,
              thumb_buf_info->out_dim.width,thumb_buf_info->out_dim.height);
        thumb_buf_info->img_fmt = JPEG_SRC_IMAGE_FMT_YUV;
        thumb_buf_info->num_bufs = 1;
        thumb_buf_info->src_image[0].fd = thumb_frame->fd;
        thumb_buf_info->src_image[0].buf_vaddr = (uint8_t*) thumb_frame->buffer;
        thumb_buf_info->src_image[0].offset = thumb_stream->mFrameOffsetInfo;
        ALOGD("%s : setting thumb image offset info, len = %d, offset = %d",
              __func__, thumb_stream->mFrameOffsetInfo.mp[0].len, thumb_stream->mFrameOffsetInfo.mp[0].offset);

        cache_ops(thumb_mem_info, thumb_frame->buffer, ION_IOC_CLEAN_INV_CACHES);
    }

    uint32_t buf_len = main_stream->mFrameOffsetInfo.frame_len;
    if (main_stream->m_flag_stream_on == FALSE) {
        //if video-sized livesnapshot
        jpg_job.encode_job.encode_parm.buf_info.src_imgs.is_video_frame = TRUE;

        //use the same output resolution as input
        main_buf_info->out_dim.width = main_buf_info->src_dim.width;
        main_buf_info->out_dim.height = main_buf_info->src_dim.height;

        if (thumb_buf_info->out_dim.width > thumb_buf_info->src_dim.width ||
            thumb_buf_info->out_dim.height > thumb_buf_info->src_dim.height ) {
            thumb_buf_info->out_dim.width = thumb_buf_info->src_dim.width;
            thumb_buf_info->out_dim.height = thumb_buf_info->src_dim.height;
        }

        uint32_t len = main_buf_info->out_dim.width * main_buf_info->out_dim.height * 1.5;
        if (len > buf_len) {
            buf_len = len;
        }
    }

    //fill in the sink img info
    jpg_job.encode_job.encode_parm.buf_info.sink_img.buf_len = buf_len;
    jpg_job.encode_job.encode_parm.buf_info.sink_img.buf_vaddr = (uint8_t *)malloc(buf_len);
    if (NULL == jpg_job.encode_job.encode_parm.buf_info.sink_img.buf_vaddr) {
        ALOGE("%s: ERROR: no memory for sink_img buf", __func__);
        free(cookie);
        cookie = NULL;
        return -1;
    }

    if (mJpegClientHandle > 0) {
        ret = mJpegHandle.start_job(mJpegClientHandle, &jpg_job, jobId);
    } else {
        ALOGE("%s: Error: bug here, mJpegClientHandle is 0", __func__);
        free(cookie);
        cookie = NULL;
        return -1;
    }

    ALOGV("%s : X", __func__);
    return ret;

}

status_t QCameraHardwareInterface::sendDataNotify(int32_t msg_type,
                                                 camera_memory_t *data,
                                                 uint8_t index,
                                                 camera_frame_metadata_t *metadata,
                                                 QCameraHalHeap_t *heap)
{
    app_notify_cb_t *app_cb = (app_notify_cb_t *)malloc(sizeof(app_notify_cb_t));
    if (NULL == app_cb) {
        ALOGE("%s: no mem for app_notify_cb_t", __func__);
        return BAD_VALUE;
    }
    memset(app_cb, 0, sizeof(app_notify_cb_t));
    app_cb->dataCb = mDataCb;
    app_cb->argm_data_cb.msg_type = msg_type;
    app_cb->argm_data_cb.cookie = mCallbackCookie;
    app_cb->argm_data_cb.data = data;
    app_cb->argm_data_cb.index = index;
    app_cb->argm_data_cb.metadata = metadata;
    app_cb->argm_data_cb.user_data = (void *)heap;

    /* enqueue jpeg_data into jpeg data queue */
    if (mNotifyDataQueue.enqueue((void *)app_cb)) {
        mNotifyTh->sendCmd(CAMERA_CMD_TYPE_DO_NEXT_JOB, FALSE, FALSE);
    } else {
        free(app_cb);
        return BAD_VALUE;
    }
    return NO_ERROR;
}

void QCameraHardwareInterface::receiveRawPicture(mm_camera_super_buf_t* recvd_frame,
                                                 QCameraHardwareInterface *pme)
{
     ALOGV("%s : E", __func__);
     status_t rc = NO_ERROR;
     int buf_index = 0;

     ALOGV("%s: is a raw snapshot", __func__);
     /*RAW snapshot*/
     if (recvd_frame->bufs[0] == NULL) {
         ALOGE("%s: The main frame buffer is null", __func__);
         return;
     }

     if (pme->mDataCb && (pme->mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE)) {
         if (pme->initHeapMem(&pme->mRawMemory,
                              1,
                              recvd_frame->bufs[0]->frame_len,
                              MSM_PMEM_RAW_MAINIMG,
                              NULL,
                              NULL) < 0) {
             ALOGE("%s : initHeapMem for raw, ret = NO_MEMORY", __func__);
             return;
         }

         buf_index = recvd_frame->bufs[0]->buf_idx;
         memcpy(pme->mRawMemory.camera_memory[0]->data,
                pme->mSnapshotMemory.camera_memory[buf_index]->data,
                recvd_frame->bufs[0]->frame_len);

         rc = pme->sendDataNotify(CAMERA_MSG_COMPRESSED_IMAGE,
                                  pme->mRawMemory.camera_memory[0],
                                  0,
                                  NULL,
                                  &pme->mRawMemory);
         if (rc != NO_ERROR) {
             pme->releaseHeapMem(&pme->mRawMemory);
         }
     }
     ALOGV("%s : X", __func__);
}

void QCameraHardwareInterface::receiveCompleteJpegPicture(jpeg_job_status_t status,
                                                          uint8_t thumbnailDroppedFlag,
                                                          uint32_t client_hdl,
                                                          uint32_t jobId,
                                                          uint8_t* out_data,
                                                          uint32_t data_size,
                                                          QCameraHardwareInterface *pme)
{
    status_t rc = NO_ERROR;
    ALOGE("%s: E", __func__);

    pme->deinitExifData();

    if(status == JPEG_JOB_STATUS_ERROR) {
        ALOGE("Error event handled from jpeg");
        if(pme->mDataCb && (pme->mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE)){
            pme->sendDataNotify(CAMERA_MSG_COMPRESSED_IMAGE,
                                NULL,
                                0,
                                NULL,
                                NULL);
        }
        return;
    }

    if(thumbnailDroppedFlag) {
        ALOGE("%s : Error in thumbnail encoding, no ops here", __func__);
    }

    pme->dumpFrameToFile(out_data,
                         data_size,
                         (char *)"debug",
                         (char *)"jpg",
                         jobId);

    ALOGE("%s: jpeg_size=%d", __func__, data_size);

    if(pme->mDataCb && (pme->mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE)){
        if (pme->initHeapMem(&pme->mJpegMemory,
                             1,
                             data_size,
                             MSM_PMEM_MAX,
                             NULL,
                             NULL) < 0) {
            ALOGE("%s : initHeapMem for jpeg, ret = NO_MEMORY", __func__);
            if(pme->mDataCb && (pme->mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE)){
                pme->sendDataNotify(CAMERA_MSG_COMPRESSED_IMAGE,
                                    NULL,
                                    0,
                                    NULL,
                                    NULL);
            }
            return;
        }

        memcpy(pme->mJpegMemory.camera_memory[0]->data, out_data, data_size);

        ALOGE("%s : Calling upperlayer callback to store JPEG image", __func__);
        rc = pme->sendDataNotify(CAMERA_MSG_COMPRESSED_IMAGE,
                                 pme->mJpegMemory.camera_memory[0],
                                 0,
                                 NULL,
                                 &pme->mJpegMemory);
        if (rc != NO_ERROR) {
            pme->releaseHeapMem(&pme->mJpegMemory);
        }
    }

    ALOGE("%s: X", __func__);
}

static void HAL_event_cb(uint32_t camera_handle, mm_camera_event_t *evt, void *user_data)
{
  QCameraHardwareInterface *obj = (QCameraHardwareInterface *)user_data;
  if (obj) {
    obj->processEvent(evt);
  } else {
    ALOGE("%s: NULL user_data", __func__);
  }
}

int32_t QCameraHardwareInterface::createRdi()
{
    int32_t ret = MM_CAMERA_OK;
    ALOGV("%s : BEGIN",__func__);
    mStreams[MM_CAMERA_RDI] = new QCameraStream_Rdi(mCameraHandle->camera_handle,
                                                    mChannelId,
                                                    640/*Width*/,
                                                    480/*Height*/,
                                                    CAMERA_BAYER_SBGGR10/*Format*/,
                                                    PREVIEW_BUFFER_COUNT/*NumBuffers*/,
                                                    mCameraHandle,
                                                    MM_CAMERA_RDI,
                                                    myMode,
                                                    this);
    if (!mStreams[MM_CAMERA_RDI]) {
        ALOGE("%s: error - can't creat RDI stream!", __func__);
        return BAD_VALUE;
    }

    ALOGV("%s : END",__func__);
    return ret;
}

int32_t QCameraHardwareInterface::createRecord()
{
    int32_t ret = MM_CAMERA_OK;
    ALOGV("%s : BEGIN",__func__);

    /*
    * Creating Instance of record stream.
    */
    ALOGE("Mymode Record = %d",myMode);
    mStreams[MM_CAMERA_VIDEO] = new QCameraStream_record(
                                        mCameraHandle->camera_handle,
                                        mChannelId,
                                        640/*Width*/,
                                        480/*Height*/,
                                        0/*Format*/,
                                        VIDEO_BUFFER_COUNT/*NumBuffers*/,
                                        mCameraHandle,
                                        MM_CAMERA_VIDEO,
                                        myMode,
                                        this);

    if (!mStreams[MM_CAMERA_VIDEO]) {
        ALOGE("%s: error - can't creat record stream!", __func__);
        return BAD_VALUE;
    }

    /*Init Channel */
    ALOGV("%s : END",__func__);
    return ret;
}
int32_t QCameraHardwareInterface::createSnapshot()
{
    int32_t ret = MM_CAMERA_OK;
    ALOGE("%s : BEGIN",__func__);
    uint8_t NumBuffers = 1;

    if(mHdrMode) {
        ALOGE("%s mHdrMode = %d, setting NumBuffers to 3", __func__, mHdrMode);
        NumBuffers = 3;
    }

    /*
    * Creating Instance of Snapshot Main stream.
    */
    ALOGE("Mymode Snap = %d",myMode);
    ALOGE("%s : before creating an instance of SnapshotMain, num buffers = %d", __func__, NumBuffers);
    mStreams[MM_CAMERA_SNAPSHOT_MAIN] = new QCameraStream_SnapshotMain(
                                                mCameraHandle->camera_handle,
                                                mChannelId,
                                                640,
                                                480,
                                                CAMERA_YUV_420_NV21,
                                                NumBuffers,
                                                mCameraHandle,
                                                MM_CAMERA_SNAPSHOT_MAIN,
                                                myMode,
                                                this);
    if (!mStreams[MM_CAMERA_SNAPSHOT_MAIN]) {
        ALOGE("%s: error - can't creat snapshot stream!", __func__);
        return BAD_VALUE;
    }

    /*
     * Creating Instance of Snapshot Thumb stream.
    */
    ALOGE("Mymode Snap = %d",myMode);
    mStreams[MM_CAMERA_SNAPSHOT_THUMBNAIL] = new QCameraStream_SnapshotThumbnail(
                                                    mCameraHandle->camera_handle,
                                                    mChannelId,
                                                    512,
                                                    384,
                                                    CAMERA_YUV_420_NV21,
                                                    NumBuffers,
                                                    mCameraHandle,
                                                    MM_CAMERA_SNAPSHOT_THUMBNAIL,
                                                    myMode,
                                                    this);
    if (!mStreams[MM_CAMERA_SNAPSHOT_THUMBNAIL]) {
        ALOGE("%s: error - can't creat snapshot stream!", __func__);
        return BAD_VALUE;
    }

    ALOGV("%s : END",__func__);
    return ret;
}
int32_t QCameraHardwareInterface::createPreview()
{
    int32_t ret = MM_CAMERA_OK;
    ALOGV("%s : BEGIN",__func__);

    ALOGE("Mymode Preview = %d",myMode);
    mStreams[MM_CAMERA_PREVIEW] = new QCameraStream_preview(
                                        mCameraHandle->camera_handle,
                                        mChannelId,
                                        640/*Width*/,
                                        480/*Height*/,
                                        0/*Format*/,
                                        7/*NumBuffers*/,
                                        mCameraHandle,
                                        MM_CAMERA_PREVIEW,
                                        myMode,
                                        this);
    if (!mStreams[MM_CAMERA_PREVIEW]) {
        ALOGE("%s: error - can't creat preview stream!", __func__);
        return BAD_VALUE;
    }

    ALOGV("%s : END",__func__);
    return ret;
}

/*Mem Hooks*/
int32_t get_buffer_hook(uint32_t camera_handle,
                        uint32_t ch_id, uint32_t stream_id,
                        void *user_data,
                        mm_camera_frame_len_offset *frame_offset_info,
                        uint8_t num_bufs,
                        uint8_t *initial_reg_flag,
                        mm_camera_buf_def_t  *bufs)
{
    QCameraHardwareInterface *pme=(QCameraHardwareInterface *)user_data;
    pme->getBuf(camera_handle, ch_id, stream_id,
                user_data, frame_offset_info,
                num_bufs,initial_reg_flag,
                bufs);

    return 0;
}

int32_t put_buffer_hook(uint32_t camera_handle,
                        uint32_t ch_id, uint32_t stream_id,
                        void *user_data, uint8_t num_bufs,
                        mm_camera_buf_def_t *bufs)
{
    QCameraHardwareInterface *pme=(QCameraHardwareInterface *)user_data;
    pme->putBuf(camera_handle, ch_id, stream_id,
                user_data, num_bufs, bufs);

    return 0;
}

int QCameraHardwareInterface::getBuf(uint32_t camera_handle,
                        uint32_t ch_id, uint32_t stream_id,
                        void *user_data,
                        mm_camera_frame_len_offset *frame_offset_info,
                        uint8_t num_bufs,
                        uint8_t *initial_reg_flag,
                        mm_camera_buf_def_t *bufs)
{
    int ret = BAD_VALUE;
    ALOGE("%s: len:%d, y_off:%d, cbcr:%d num buffers: %d planes:%d streamid:%d",
          __func__,
          frame_offset_info->frame_len,
          frame_offset_info->mp[0].len,
          frame_offset_info->mp[1].len,
          num_bufs,frame_offset_info->num_planes,
          stream_id);

    for (int i = 0; i < MM_CAMERA_IMG_MODE_MAX; i++) {
        if (mStreams[i] != NULL && mStreams[i]->mStreamId == stream_id) {
            ret = mStreams[i]->getBuf(frame_offset_info, num_bufs, initial_reg_flag, bufs);
            break;
        }
    }

    return ret;
}

int QCameraHardwareInterface::putBuf(uint32_t camera_handle,
                        uint32_t ch_id, uint32_t stream_id,
                        void *user_data, uint8_t num_bufs,
                        mm_camera_buf_def_t *bufs)
{
    int ret = BAD_VALUE;
    ALOGE("%s:E",__func__);
    for (int i = 0; i < MM_CAMERA_IMG_MODE_MAX; i++) {
        if (mStreams[i] && (mStreams[i]->mStreamId == stream_id)) {
            ret = mStreams[i]->putBuf(num_bufs, bufs);
            break;
        }
    }
    return ret;
}


/* constructor */
QCameraHardwareInterface::
QCameraHardwareInterface(int cameraId, int mode)
                  : mCameraId(cameraId),
                    mParameters(),
                    mMsgEnabled(0),
                    mNotifyCb(0),
                    mDataCb(0),
                    mDataCbTimestamp(0),
                    mCallbackCookie(0),
                    mPreviewFormat(CAMERA_YUV_420_NV21),
                    mFps(0),
                    mDebugFps(0),
    mBrightness(0),
    mContrast(0),
    mBestShotMode(0),
    mEffects(0),
    mSkinToneEnhancement(0),
    mDenoiseValue(0),
    mHJR(0),
    mRotation(0),
    mMaxZoom(0),
    mCurrentZoom(0),
    mSupportedPictureSizesCount(15),
    mFaceDetectOn(0),
    mDumpFrmCnt(0), mDumpSkipCnt(0),
    mFocusMode(AF_MODE_MAX),
    mPictureSizeCount(15),
    mPreviewSizeCount(13),
    mVideoSizeCount(0),
    mAutoFocusRunning(false),
    mHasAutoFocusSupport(false),
    mInitialized(false),
    mDisEnabled(0),
    mIs3DModeOn(0),
    mSmoothZoomRunning(false),
    mParamStringInitialized(false),
    mZoomSupported(false),
    mFullLiveshotEnabled(true),
    mRecordingHint(0),
    mStartRecording(0),
    mReleasedRecordingFrame(false),
    mHdrMode(HDR_BRACKETING_OFF),
    mSnapshotFormat(0),
    mZslInterval(1),
    mRestartPreview(false),
    mStatsOn(0), mCurrentHisto(-1), mSendData(false), mStatHeap(NULL),
    mZslLookBackMode(0),
    mZslLookBackValue(0),
    mZslEmptyQueueFlag(FALSE),
    mPictureSizes(NULL),
    mVideoSizes(NULL),
    mCameraState(CAMERA_STATE_UNINITED),
    mExifTableNumEntries(0),
    mNoDisplayMode(0),
    rdiMode(STREAM_IMAGE),
    mSupportedFpsRanges(NULL),
    mSupportedFpsRangesCount(0),
    mPowerModule(0),
    mNeedToUnlockCaf(false),
    mSuperBufQueue(releaseProcData, this),
    mNotifyDataQueue(releaseNofityData, this)
{
    ALOGI("QCameraHardwareInterface: E");
    int32_t result = MM_CAMERA_E_GENERAL;
    mMobiCatEnabled = false;
    char value[PROPERTY_VALUE_MAX];

    pthread_mutex_init(&mAsyncCmdMutex, NULL);
    pthread_cond_init(&mAsyncCmdWait, NULL);

    property_get("persist.debug.sf.showfps", value, "0");
    mDebugFps = atoi(value);
    mPreviewState = QCAMERA_HAL_PREVIEW_STOPPED;
    mPreviewWindow = NULL;
    property_get("camera.hal.fps", value, "0");
    mFps = atoi(value);

    ALOGI("Init mPreviewState = %d", mPreviewState);

    property_get("persist.camera.hal.multitouchaf", value, "0");
    mMultiTouch = atoi(value);

    property_get("persist.camera.full.liveshot", value, "1");
    mFullLiveshotEnabled = atoi(value);

    property_get("persist.camera.hal.dis", value, "0");
    mDisEnabled = atoi(value);

    memset(&mem_hooks,0,sizeof(mm_camear_mem_vtbl_t));

    mem_hooks.user_data=this;
    mem_hooks.get_buf=get_buffer_hook;
    mem_hooks.put_buf=put_buffer_hook;

    /* Open camera stack! */
    mCameraHandle=camera_open(mCameraId, &mem_hooks);
    ALOGV("Cam open returned %p",mCameraHandle);
    if(mCameraHandle == NULL) {
          ALOGE("startCamera: cam_ops_open failed: id = %d", mCameraId);
          return;
    }
    //sync API for mm-camera-interface
    mCameraHandle->ops->sync(mCameraHandle->camera_handle);

    mChannelId=mCameraHandle->ops->ch_acquire(mCameraHandle->camera_handle);
    if(mChannelId<=0)
    {
        mCameraHandle->ops->camera_close(mCameraHandle->camera_handle);
        return;
    }
    mm_camera_event_type_t evt;
    for (int i = 0; i < MM_CAMERA_EVT_TYPE_MAX; i++) {
        if(mCameraHandle->ops->is_event_supported(mCameraHandle->camera_handle,
                                 (mm_camera_event_type_t )i )){
            mCameraHandle->ops->register_event_notify(mCameraHandle->camera_handle,
					   HAL_event_cb,
					   (void *) this,
					   (mm_camera_event_type_t) i);
        }
    }

    loadTables();
    /* Setup Picture Size and Preview size tables */
    setPictureSizeTable();
    ALOGD("%s: Picture table size: %d", __func__, mPictureSizeCount);
    ALOGD("%s: Picture table: ", __func__);
    for(unsigned int i=0; i < mPictureSizeCount;i++) {
      ALOGD(" %d  %d", mPictureSizes[i].width, mPictureSizes[i].height);
    }

    setPreviewSizeTable();
    ALOGD("%s: Preview table size: %d", __func__, mPreviewSizeCount);
    ALOGD("%s: Preview table: ", __func__);
    for(unsigned int i=0; i < mPreviewSizeCount;i++) {
      ALOGD(" %d  %d", mPreviewSizes[i].width, mPreviewSizes[i].height);
    }

    setVideoSizeTable();
    ALOGD("%s: Video table size: %d", __func__, mVideoSizeCount);
    ALOGD("%s: Video table: ", __func__);
    for(unsigned int i=0; i < mVideoSizeCount;i++) {
      ALOGD(" %d  %d", mVideoSizes[i].width, mVideoSizes[i].height);
    }
    memset(&mHistServer, 0, sizeof(mHistServer));

    /* set my mode - update myMode member variable due to difference in
     enum definition between upper and lower layer*/
    setMyMode(mode);
    initDefaultParameters();

    //Create Stream Objects
    memset(mStreams, 0, sizeof(mStreams));

    //Preview
    result = createPreview();
    if(result != MM_CAMERA_OK) {
        ALOGE("%s X: Failed to create Preview Object",__func__);
        return;
    }

    //Record
    result = createRecord();
    if(result != MM_CAMERA_OK) {
        ALOGE("%s X: Failed to create Record Object",__func__);
        return;
    }
    //Snapshot
    result = createSnapshot();
    if(result != MM_CAMERA_OK) {
        ALOGE("%s X: Failed to create Record Object",__func__);
        return;
    }

    memset(&mJpegHandle, 0, sizeof(mJpegHandle));
    mJpegClientHandle = jpeg_open(&mJpegHandle);
    if(!mJpegClientHandle) {
        ALOGE("%s : jpeg_open did not work", __func__);
        return;
    }

    /* launch jpeg notify thread and raw data proc thread */
    mNotifyTh = new QCameraCmdThread();
    if (mNotifyTh != NULL) {
        mNotifyTh->launch(dataNotifyRoutine, this);
    } else {
        ALOGE("%s : no mem for mNotifyTh", __func__);
        return;
    }
    mDataProcTh = new QCameraCmdThread();
    if (mDataProcTh != NULL) {
        mDataProcTh->launch(dataProcessRoutine, this);
    } else {
        ALOGE("%s : no mem for mDataProcTh", __func__);
        return;
    }

    memset(&mHdrInfo, 0, sizeof(snap_hdr_record_t));
    // initialize the mMetadata to set num_of_faces to 0
    memset(&mMetadata, 0, sizeof(mMetadata));

    if (hw_get_module(POWER_HARDWARE_MODULE_ID, (const hw_module_t **)&mPowerModule)) {
        ALOGE("%s module not found", POWER_HARDWARE_MODULE_ID);
    }

    mCameraState = CAMERA_STATE_READY;
    ALOGI("QCameraHardwareInterface: X");
}

QCameraHardwareInterface::~QCameraHardwareInterface()
{
    ALOGI("~QCameraHardwareInterface: E");

    if (CAMERA_STATE_READY == mCameraState) {
        stopPreview();
        mPreviewState = QCAMERA_HAL_PREVIEW_STOPPED;
    }

    if(mSupportedFpsRanges != NULL) {
        delete mSupportedFpsRanges;
        mSupportedFpsRanges = NULL;
    }

    freePictureTable();
    freeVideoSizeTable();

    deInitHistogramBuffers();
    if(mStatHeap != NULL) {
      mStatHeap.clear( );
      mStatHeap = NULL;
    }

    if(mMobiCatEnabled) {
      deInitMobicatBuffers();
    }

    for (int i = 0; i < MM_CAMERA_IMG_MODE_MAX; i++) {
        if (mStreams[i] != NULL) {
            delete mStreams[i];
            mStreams[i] = NULL;
        }
    }

    for (int i = 0; i < MAX_HDR_EXP_FRAME_NUM; i++) {
        if (mHdrInfo.recvd_frame[i] != NULL) {
            free(mHdrInfo.recvd_frame[i]);
            mHdrInfo.recvd_frame[i] = NULL;
        }
    }

    if (mNotifyTh != NULL) {
        mNotifyTh->exit();
        delete mNotifyTh;
        mNotifyTh = NULL;
    }
    if (mDataProcTh != NULL) {
        mDataProcTh->exit();
        delete mDataProcTh;
        mDataProcTh = NULL;
    }
    if(mJpegClientHandle > 0) {
        int rc = mJpegHandle.close(mJpegClientHandle);
        ALOGE("%s: Jpeg closed, rc = %d, mJpegClientHandle = %x",
              __func__, rc, mJpegClientHandle);
        mJpegClientHandle = 0;
        memset(&mJpegHandle, 0, sizeof(mJpegHandle));
    }

    if (NULL != mCameraHandle) {
        mCameraHandle->ops->ch_release(mCameraHandle->camera_handle,
                                       mChannelId);

        mCameraHandle->ops->camera_close(mCameraHandle->camera_handle);
    }

    pthread_mutex_destroy(&mAsyncCmdMutex);
    pthread_cond_destroy(&mAsyncCmdWait);

    ALOGE("~QCameraHardwareInterface: X");
}

bool QCameraHardwareInterface::isCameraReady()
{
    ALOGE("isCameraReady mCameraState %d", mCameraState);
    return (mCameraState == CAMERA_STATE_READY);
}

void QCameraHardwareInterface::release()
{
    ALOGI("release: E");
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
    ALOGI("release: X");
}

void QCameraHardwareInterface::setCallbacks(
    camera_notify_callback notify_cb,
    camera_data_callback data_cb,
    camera_data_timestamp_callback data_cb_timestamp,
    camera_request_memory get_memory,
    void *user)
{
    ALOGE("setCallbacks: E");
    Mutex::Autolock lock(mLock);
    mNotifyCb        = notify_cb;
    mDataCb          = data_cb;
    mDataCbTimestamp = data_cb_timestamp;
    mGetMemory       = get_memory;
    mCallbackCookie  = user;
    ALOGI("setCallbacks: X");
}

void QCameraHardwareInterface::enableMsgType(int32_t msgType)
{
    ALOGI("enableMsgType: E, msgType =0x%x", msgType);
    Mutex::Autolock lock(mLock);
    mMsgEnabled |= msgType;
    ALOGI("enableMsgType: X, msgType =0x%x, mMsgEnabled=0x%x", msgType, mMsgEnabled);
}

void QCameraHardwareInterface::disableMsgType(int32_t msgType)
{
    ALOGI("disableMsgType: E");
    Mutex::Autolock lock(mLock);
    mMsgEnabled &= ~msgType;
    ALOGI("disableMsgType: X, msgType =0x%x, mMsgEnabled=0x%x", msgType, mMsgEnabled);
}

int QCameraHardwareInterface::msgTypeEnabled(int32_t msgType)
{
    ALOGI("msgTypeEnabled: E");
    Mutex::Autolock lock(mLock);
    return (mMsgEnabled & msgType);
    ALOGI("msgTypeEnabled: X");
}

int QCameraHardwareInterface::dump(int fd)
{
    ALOGE("%s: not supported yet", __func__);
    return -1;
}

status_t QCameraHardwareInterface::sendCommand(int32_t command, int32_t arg1,
                                         int32_t arg2)
{
    ALOGI("sendCommand: E");
    status_t rc = NO_ERROR;
    Mutex::Autolock l(&mLock);

    switch (command) {
        case CAMERA_CMD_HISTOGRAM_ON:
            ALOGE("histogram set to on");
            rc = setHistogram(1);
            break;
        case CAMERA_CMD_HISTOGRAM_OFF:
            ALOGE("histogram set to off");
            rc = setHistogram(0);
            break;
        case CAMERA_CMD_HISTOGRAM_SEND_DATA:
            ALOGE("histogram send data");
            mSendData = true;
            rc = NO_ERROR;
            break;
        case CAMERA_CMD_START_FACE_DETECTION:
           if(supportsFaceDetection() == false){
                ALOGE("Face detection support is not available");
                return NO_ERROR;
           }
           setFaceDetection("on");
           return runFaceDetection();
        case CAMERA_CMD_STOP_FACE_DETECTION:
           if(supportsFaceDetection() == false){
                ALOGE("Face detection support is not available");
                return NO_ERROR;
           }
           setFaceDetection("off");
           return runFaceDetection();
        default:
            break;
    }
    ALOGI("sendCommand: X");
    return rc;
}

void QCameraHardwareInterface::setMyMode(int mode)
{
    ALOGI("setMyMode: E");
    if (mode & CAMERA_SUPPORT_MODE_3D) {
        myMode = CAMERA_MODE_3D;
    }else {
        /* default mode is 2D */
        myMode = CAMERA_MODE_2D;
    }

    if (mode & CAMERA_SUPPORT_MODE_ZSL) {
        myMode = (camera_mode_t)(myMode |CAMERA_ZSL_MODE);
    }else {
       myMode = (camera_mode_t) (myMode | CAMERA_NONZSL_MODE);
    }
    ALOGI("setMyMode: Set mode to %d (passed mode: %d)", myMode, mode);
    ALOGI("setMyMode: X");
}
/* static factory function */
QCameraHardwareInterface *QCameraHardwareInterface::createInstance(int cameraId, int mode)
{
    ALOGI("createInstance: E");
    QCameraHardwareInterface *cam = new QCameraHardwareInterface(cameraId, mode);
    if (cam ) {
      if (cam->mCameraState != CAMERA_STATE_READY) {
        ALOGE("createInstance: Failed");
        delete cam;
        cam = NULL;
      }
    }

    if (cam) {
      ALOGI("createInstance: X");
      return cam;
    } else {
      return NULL;
    }
}
/* external plug in function */
extern "C" void *
QCameraHAL_openCameraHardware(int  cameraId, int mode)
{
    ALOGI("QCameraHAL_openCameraHardware: E");
    return (void *) QCameraHardwareInterface::createInstance(cameraId, mode);
}

bool QCameraHardwareInterface::isPreviewRunning() {
    ALOGI("isPreviewRunning: E");
    bool ret = false;

    switch(mPreviewState) {
    case QCAMERA_HAL_PREVIEW_START:
    case QCAMERA_HAL_PREVIEW_STARTED:
    case QCAMERA_HAL_RECORDING_STARTED:
        ret = true;
        break;
    default:
        break;
    }
    ALOGI("isPreviewRunning: X");
    return ret;
}

bool QCameraHardwareInterface::isRecordingRunning() {
    ALOGE("isRecordingRunning: E");
    bool ret = false;
    if(QCAMERA_HAL_RECORDING_STARTED == mPreviewState)
      ret = true;
    ALOGE("isRecordingRunning: X");
    return ret;
}

bool QCameraHardwareInterface::isSnapshotRunning() {
    ALOGE("isSnapshotRunning: E");
    bool ret = false;
    switch(mPreviewState) {
    case QCAMERA_HAL_PREVIEW_STOPPED:
    case QCAMERA_HAL_PREVIEW_START:
    case QCAMERA_HAL_PREVIEW_STARTED:
    case QCAMERA_HAL_RECORDING_STARTED:
    default:
        break;
    case QCAMERA_HAL_TAKE_PICTURE:
        ret = true;
        break;
    }
    ALOGI("isSnapshotRunning: X");
    return ret;
}

bool QCameraHardwareInterface::isZSLMode() {
    return (myMode & CAMERA_ZSL_MODE);
}

int QCameraHardwareInterface::getHDRMode() {
    ALOGE("%s, mHdrMode = %d", __func__, mHdrMode);
    return mHdrMode;
}

void QCameraHardwareInterface::debugShowPreviewFPS() const
{
    static int mFrameCount;
    static int mLastFrameCount = 0;
    static nsecs_t mLastFpsTime = 0;
    static float mFps = 0;
    mFrameCount++;
    nsecs_t now = systemTime();
    nsecs_t diff = now - mLastFpsTime;
    if (diff > ms2ns(250)) {
        mFps =  ((mFrameCount - mLastFrameCount) * float(s2ns(1))) / diff;
        ALOGE("Preview Frames Per Second: %.4f", mFps);
        mLastFpsTime = now;
        mLastFrameCount = mFrameCount;
    }
}


void QCameraHardwareInterface::
processPreviewChannelEvent(mm_camera_ch_event_type_t channelEvent, app_notify_cb_t *app_cb) {
    ALOGI("processPreviewChannelEvent: E");
    switch(channelEvent) {
        case MM_CAMERA_CH_EVT_STREAMING_ON:
            break;
        case MM_CAMERA_CH_EVT_STREAMING_OFF:
            break;
        case MM_CAMERA_CH_EVT_DATA_DELIVERY_DONE:
            break;
        default:
            break;
    }
    ALOGI("processPreviewChannelEvent: X");
    return;
}

void QCameraHardwareInterface::processRecordChannelEvent(
  mm_camera_ch_event_type_t channelEvent, app_notify_cb_t *app_cb) {
    ALOGI("processRecordChannelEvent: E");
    switch(channelEvent) {
        case MM_CAMERA_CH_EVT_STREAMING_ON:
            break;
        case MM_CAMERA_CH_EVT_STREAMING_OFF:
            break;
        case MM_CAMERA_CH_EVT_DATA_DELIVERY_DONE:
            break;
        default:
            break;
    }
    ALOGI("processRecordChannelEvent: X");
    return;
}

void QCameraHardwareInterface::
processSnapshotChannelEvent(mm_camera_ch_event_type_t channelEvent, app_notify_cb_t *app_cb) {
    ALOGI("processSnapshotChannelEvent: E evt=%d state=%d", channelEvent,
      mCameraState);
    switch(channelEvent) {
        case MM_CAMERA_CH_EVT_STREAMING_ON:
            break;
        case MM_CAMERA_CH_EVT_STREAMING_OFF:
            break;
        case MM_CAMERA_CH_EVT_DATA_DELIVERY_DONE:
            break;
        case MM_CAMERA_CH_EVT_DATA_REQUEST_MORE:
            break;
        default:
            break;
    }
    ALOGI("processSnapshotChannelEvent: X");
    return;
}

void QCameraHardwareInterface::
processRdiChannelEvent(mm_camera_ch_event_type_t channelEvent, app_notify_cb_t *app_cb) {
    ALOGI("processRdiChannelEvent: E evt=%d state=%d", channelEvent,
      mCameraState);
    switch(channelEvent) {
        case MM_CAMERA_CH_EVT_STREAMING_ON:
            break;
        case MM_CAMERA_CH_EVT_STREAMING_OFF:
            break;
        case MM_CAMERA_CH_EVT_DATA_DELIVERY_DONE:
            break;
        case MM_CAMERA_CH_EVT_DATA_REQUEST_MORE:
            break;
        default:
            break;
    }
    ALOGI("processRdiChannelEvent: X");
    return;
}

void QCameraHardwareInterface::processChannelEvent(
  mm_camera_ch_event_t *event, app_notify_cb_t *app_cb)
{
    ALOGI("processChannelEvent: E");
    Mutex::Autolock lock(mLock);
    switch(event->ch) {
        case MM_CAMERA_PREVIEW:
            processPreviewChannelEvent(event->evt, app_cb);
            break;
        case MM_CAMERA_RDI:
            processRdiChannelEvent(event->evt, app_cb);
            break;
        case MM_CAMERA_VIDEO:
            processRecordChannelEvent(event->evt, app_cb);
            break;
        case MM_CAMERA_SNAPSHOT_MAIN:
        case MM_CAMERA_SNAPSHOT_THUMBNAIL:
            processSnapshotChannelEvent(event->evt, app_cb);
            break;
        default:
            break;
    }
    ALOGI("processChannelEvent: X");
    return;
}

void QCameraHardwareInterface::processCtrlEvent(mm_camera_ctrl_event_t *event, app_notify_cb_t *app_cb)
{
    ALOGI("processCtrlEvent: %d, E",event->evt);
    ALOGE("processCtrlEvent: MM_CAMERA_CTRL_EVT_HDR_DONE is %d", MM_CAMERA_CTRL_EVT_HDR_DONE);
    if(rdiMode == STREAM_RAW) {
        return;
    }
    Mutex::Autolock lock(mLock);
    switch(event->evt)
    {
        case MM_CAMERA_CTRL_EVT_ZOOM_DONE:
            ALOGI("processCtrlEvent: MM_CAMERA_CTRL_EVT_ZOOM_DONE");
            zoomEvent(&event->status, app_cb);
            break;
        case MM_CAMERA_CTRL_EVT_AUTO_FOCUS_DONE:
            ALOGI("processCtrlEvent: MM_CAMERA_CTRL_EVT_AUTO_FOCUS_DONE");
            autoFocusEvent(&event->status, app_cb);
            break;
        case MM_CAMERA_CTRL_EVT_PREP_SNAPSHOT:
            ALOGI("processCtrlEvent: MM_CAMERA_CTRL_EVT_ZOOM_DONE");
            break;
        case MM_CAMERA_CTRL_EVT_WDN_DONE:
            ALOGI("processCtrlEvent: MM_CAMERA_CTRL_EVT_WDN_DONE");
            wdenoiseEvent(event->status, (void *)(event->cookie));
            break;
        case MM_CAMERA_CTRL_EVT_HDR_DONE:
            ALOGI("processCtrlEvent:MM_CAMERA_CTRL_EVT_HDR_DONE");
            notifyHdrEvent(event->status, (void*)(event->cookie));
            break;
        case MM_CAMERA_CTRL_EVT_ERROR:
            ALOGI("processCtrlEvent: MM_CAMERA_CTRL_EVT_ERROR");
            app_cb->notifyCb  = mNotifyCb;
            app_cb->argm_notify.msg_type = CAMERA_MSG_ERROR;
            app_cb->argm_notify.ext1 = CAMERA_ERROR_UNKNOWN;
            app_cb->argm_notify.cookie =  mCallbackCookie;
            break;
        case MM_CAMERA_CTRL_EVT_SNAPSHOT_CONFIG_DONE:
            ALOGV("%s: MM_CAMERA_CTRL_EVT_SNAPSHOT_CONFIG_DONE", __func__);
            app_cb->notifyCb  = mNotifyCb;
            app_cb->argm_notify.msg_type = CAMERA_MSG_SHUTTER;
            app_cb->argm_notify.ext1 = 0;
            app_cb->argm_notify.ext2 = TRUE;
            app_cb->argm_notify.cookie =  mCallbackCookie;
            mShutterSoundPlayed = TRUE;
       default:
            break;
    }
    ALOGI("processCtrlEvent: X");
    return;
}

void  QCameraHardwareInterface::processStatsEvent(
  mm_camera_stats_event_t *event, app_notify_cb_t *app_cb)
{
    ALOGI("processStatsEvent: E");
    if (!isPreviewRunning( )) {
        ALOGE("preview is not running");
        return;
    }

    switch (event->event_id) {

        case MM_CAMERA_STATS_EVT_HISTO:
        {
            ALOGE("HAL process Histo: mMsgEnabled=0x%x, mStatsOn=%d, mSendData=%d, mDataCb=%p ",
                  (mMsgEnabled & CAMERA_MSG_STATS_DATA), mStatsOn, mSendData, mDataCb);
            int msgEnabled = mMsgEnabled;
            /*get stats buffer based on index*/
            camera_preview_histogram_info* hist_info =
                (camera_preview_histogram_info*) mHistServer.camera_memory[event->e.stats_histo.index]->data;

            if(mStatsOn == QCAMERA_PARM_ENABLE && mSendData &&
                            mDataCb && (msgEnabled & CAMERA_MSG_STATS_DATA) ) {
                uint32_t *dest;
                mSendData = false;
                mCurrentHisto = (mCurrentHisto + 1) % 3;
                // The first element of the array will contain the maximum hist value provided by driver.
                *(uint32_t *)((unsigned int)(mStatsMapped[mCurrentHisto]->data)) = hist_info->max_value;
                memcpy((uint32_t *)((unsigned int)mStatsMapped[mCurrentHisto]->data + sizeof(int32_t)),
                                                    (uint32_t *)hist_info->buffer,(sizeof(int32_t) * 256));

                app_cb->dataCb  = mDataCb;
                app_cb->argm_data_cb.msg_type = CAMERA_MSG_STATS_DATA;
                app_cb->argm_data_cb.data = mStatsMapped[mCurrentHisto];
                app_cb->argm_data_cb.index = 0;
                app_cb->argm_data_cb.metadata = NULL;
                app_cb->argm_data_cb.cookie =  mCallbackCookie;
            }
            break;

        }
        default:
        break;
    }
  ALOGV("receiveCameraStats X");
}

void  QCameraHardwareInterface::processInfoEvent(
  mm_camera_info_event_t *event, app_notify_cb_t *app_cb) {
    ALOGI("processInfoEvent: %d, E",event->event_id);
    switch(event->event_id)
    {
        case MM_CAMERA_INFO_EVT_ROI:
            roiEvent(event->e.roi, app_cb);
            break;
        default:
            break;
    }
    ALOGI("processInfoEvent: X");
    return;
}

void  QCameraHardwareInterface::processEvent(mm_camera_event_t *event)
{
    app_notify_cb_t app_cb;
    ALOGE("processEvent: type :%d E",event->event_type);
    if(mPreviewState == QCAMERA_HAL_PREVIEW_STOPPED){
	ALOGE("Stop recording issued. Return from process Event");
        return;
    }
    memset(&app_cb, 0, sizeof(app_notify_cb_t));
    switch(event->event_type)
    {
        case MM_CAMERA_EVT_TYPE_CH:
            processChannelEvent(&event->e.ch, &app_cb);
            break;
        case MM_CAMERA_EVT_TYPE_CTRL:
            processCtrlEvent(&event->e.ctrl, &app_cb);
            break;
        case MM_CAMERA_EVT_TYPE_STATS:
            processStatsEvent(&event->e.stats, &app_cb);
            break;
        case MM_CAMERA_EVT_TYPE_INFO:
            processInfoEvent(&event->e.info, &app_cb);
            break;
        default:
            break;
    }
    ALOGE(" App_cb Notify %p, datacb=%p", app_cb.notifyCb, app_cb.dataCb);
    if (app_cb.notifyCb) {
      app_cb.notifyCb(app_cb.argm_notify.msg_type,
        app_cb.argm_notify.ext1, app_cb.argm_notify.ext2,
        app_cb.argm_notify.cookie);
    }
    if (app_cb.dataCb) {
      app_cb.dataCb(app_cb.argm_data_cb.msg_type,
        app_cb.argm_data_cb.data, app_cb.argm_data_cb.index,
        app_cb.argm_data_cb.metadata, app_cb.argm_data_cb.cookie);
    }
    ALOGI("processEvent: X");
    return;
}

status_t QCameraHardwareInterface::startPreview()
{
    status_t retVal = NO_ERROR;

    ALOGE("%s: mPreviewState =%d", __func__, mPreviewState);
    Mutex::Autolock lock(mLock);

    switch(mPreviewState) {
    case QCAMERA_HAL_TAKE_PICTURE:
        /* cancel pic internally */
        cancelPictureInternal();
        mPreviewState = QCAMERA_HAL_PREVIEW_STOPPED;
        /* then continue with start preview */
    case QCAMERA_HAL_PREVIEW_STOPPED:
        mPreviewState = QCAMERA_HAL_PREVIEW_START;
        ALOGE("%s:  HAL::startPreview begin", __func__);

        if(QCAMERA_HAL_PREVIEW_START == mPreviewState &&
           (mPreviewWindow || isNoDisplayMode())) {
            ALOGE("%s:  start preview now", __func__);
            retVal = startPreview2();
            if(retVal == NO_ERROR)
                mPreviewState = QCAMERA_HAL_PREVIEW_STARTED;
        } else {
            ALOGE("%s:  received startPreview, but preview window = null", __func__);
        }
        break;
    case QCAMERA_HAL_PREVIEW_START:
    case QCAMERA_HAL_PREVIEW_STARTED:
    break;
    case QCAMERA_HAL_RECORDING_STARTED:
        ALOGE("%s: cannot start preview in recording state", __func__);
        break;
    default:
        ALOGE("%s: unknow state %d received", __func__, mPreviewState);
        retVal = UNKNOWN_ERROR;
        break;
    }
    return retVal;
}

status_t QCameraHardwareInterface::startPreview2()
{
    ALOGV("startPreview2: E");
    status_t ret = NO_ERROR;

    cam_ctrl_dimension_t dim;
    mm_camera_dimension_t maxDim;
    uint32_t stream[2];
    mm_camera_bundle_attr_t attr;

    if (mPreviewState == QCAMERA_HAL_PREVIEW_STARTED) { //isPreviewRunning()){
        ALOGE("%s:Preview already started  mPreviewState = %d!", __func__, mPreviewState);
        ALOGE("%s: X", __func__);
        return NO_ERROR;
    }

    /* config the parmeters and see if we need to re-init the stream*/
    ret = setDimension();
    if (MM_CAMERA_OK != ret) {
      ALOGE("%s: error - can't Set Dimensions!", __func__);
      return BAD_VALUE;
    }

    memset(&mHdrInfo, 0, sizeof(snap_hdr_record_t));

    if(isZSLMode()) {
        ALOGE("<DEBUGMODE>In ZSL mode");
        ALOGE("Setting OP MODE to MM_CAMERA_OP_MODE_VIDEO");
        mm_camera_op_mode_type_t op_mode=MM_CAMERA_OP_MODE_ZSL;
        ret = mCameraHandle->ops->set_parm(
                             mCameraHandle->camera_handle,
                             MM_CAMERA_PARM_OP_MODE,
                             &op_mode);
        ALOGE("OP Mode Set");
        /* Start preview streaming */
        /*now init all the buffers and send to steam object*/
        ret = mStreams[MM_CAMERA_PREVIEW]->initStream(FALSE, TRUE);
        if (MM_CAMERA_OK != ret){
            ALOGE("%s: error - can't init Preview channel!", __func__);
            return BAD_VALUE;
        }

        /* Start ZSL stream */
        ret = mStreams[MM_CAMERA_SNAPSHOT_MAIN]->initStream(TRUE, TRUE);
        if (MM_CAMERA_OK != ret){
            ALOGE("%s: error - can't init Snapshot stream!", __func__);
            mStreams[MM_CAMERA_PREVIEW]->deinitStream();
            return BAD_VALUE;
        }

        stream[0] = mStreams[MM_CAMERA_PREVIEW]->mStreamId;
        stream[1] = mStreams[MM_CAMERA_SNAPSHOT_MAIN]->mStreamId;

        attr.notify_mode = MM_CAMERA_SUPER_BUF_NOTIFY_BURST;
        attr.burst_num = getNumOfSnapshots();
        attr.look_back = getZSLBackLookCount();
        attr.post_frame_skip = getZSLBurstInterval();
        attr.water_mark = getZSLQueueDepth();
        ALOGE("%s: burst_num=%d, look_back=%d, frame_skip=%d, water_mark=%d",
              __func__, attr.burst_num, attr.look_back,
              attr.post_frame_skip, attr.water_mark);

        ret = mCameraHandle->ops->init_stream_bundle(
                  mCameraHandle->camera_handle,
                  mChannelId,
                  superbuf_cb_routine,
                  this,
                  &attr,
                  2,
                  stream);
        if (MM_CAMERA_OK != ret){
            ALOGE("%s: error - can't init zsl preview streams!", __func__);
            mStreams[MM_CAMERA_PREVIEW]->deinitStream();
            mStreams[MM_CAMERA_SNAPSHOT_MAIN]->deinitStream();
            return BAD_VALUE;
        }

        ret = mStreams[MM_CAMERA_PREVIEW]->streamOn();
        if (MM_CAMERA_OK != ret){
            ALOGE("%s: error - can't start preview stream!", __func__);
            mStreams[MM_CAMERA_PREVIEW]->deinitStream();
            mStreams[MM_CAMERA_SNAPSHOT_MAIN]->deinitStream();
            return BAD_VALUE;
        }

        ret = mStreams[MM_CAMERA_SNAPSHOT_MAIN]->streamOn();
        if (MM_CAMERA_OK != ret){
            ALOGE("%s: error - can't start snapshot stream!", __func__);
            mStreams[MM_CAMERA_PREVIEW]->streamOff(0);
            mStreams[MM_CAMERA_PREVIEW]->deinitStream();
            mStreams[MM_CAMERA_SNAPSHOT_MAIN]->deinitStream();
            return BAD_VALUE;
        }
    }else{
        /*now init all the buffers and send to steam object*/
        ALOGE("Setting OP MODE to MM_CAMERA_OP_MODE_VIDEO");
        mm_camera_op_mode_type_t op_mode = MM_CAMERA_OP_MODE_VIDEO;
        ret = mCameraHandle->ops->set_parm(
                         mCameraHandle->camera_handle,
                         MM_CAMERA_PARM_OP_MODE,
                         &op_mode);
        ALOGE("OP Mode Set");

        if (MM_CAMERA_OK != ret){
            ALOGE("%s: X :set mode MM_CAMERA_OP_MODE_VIDEO err=%d\n", __func__, ret);
            return BAD_VALUE;
        }
        if(rdiMode == STREAM_RAW) {
            if(mStreams[MM_CAMERA_RDI] == NULL) {
                createRdi();
            }
            mStreams[MM_CAMERA_RDI]->initStream(FALSE, TRUE);
            ret = mStreams[MM_CAMERA_RDI]->streamOn();
            return ret;
        }
        /*now init all the buffers and send to steam object*/
        ret = mStreams[MM_CAMERA_PREVIEW]->initStream(FALSE, TRUE);
        ALOGE("%s : called initStream from Preview and ret = %d", __func__, ret);
        if (MM_CAMERA_OK != ret){
            ALOGE("%s: error - can't init Preview channel!", __func__);
            return BAD_VALUE;
        }
        if(mRecordingHint == true) {
            ret = mStreams[MM_CAMERA_VIDEO]->initStream(FALSE, TRUE);
            if (MM_CAMERA_OK != ret){
                 ALOGE("%s: error - can't init Record channel!", __func__);
                 mStreams[MM_CAMERA_PREVIEW]->deinitStream();
                 return BAD_VALUE;
            }
            if (!canTakeFullSizeLiveshot()) {
                // video-size live snapshot, config same as video
                mStreams[MM_CAMERA_SNAPSHOT_MAIN]->mFormat = mStreams[MM_CAMERA_VIDEO]->mFormat;
                mStreams[MM_CAMERA_SNAPSHOT_MAIN]->mWidth = mStreams[MM_CAMERA_VIDEO]->mWidth;
                mStreams[MM_CAMERA_SNAPSHOT_MAIN]->mHeight = mStreams[MM_CAMERA_VIDEO]->mHeight;
                ret = mStreams[MM_CAMERA_SNAPSHOT_MAIN]->initStream(FALSE, FALSE);
            } else {
                ret = mStreams[MM_CAMERA_SNAPSHOT_MAIN]->initStream(FALSE, TRUE);
            }
            if (MM_CAMERA_OK != ret){
                 ALOGE("%s: error - can't init Snapshot Main!", __func__);
                 mStreams[MM_CAMERA_PREVIEW]->deinitStream();
                 mStreams[MM_CAMERA_VIDEO]->deinitStream();
                 return BAD_VALUE;
            }

        }
        ret = mStreams[MM_CAMERA_PREVIEW]->streamOn();
        if (MM_CAMERA_OK != ret){
            ALOGE("%s: error - can't start preview stream!", __func__);
            if (mRecordingHint == true) {
                mStreams[MM_CAMERA_SNAPSHOT_MAIN]->deinitStream();
                mStreams[MM_CAMERA_VIDEO]->deinitStream();
                mStreams[MM_CAMERA_PREVIEW]->deinitStream();
            }
            return BAD_VALUE;
        }
    }

    ALOGV("startPreview: X");
    return ret;
}

void QCameraHardwareInterface::stopPreview()
{
    ALOGI("%s: stopPreview: E", __func__);
    Mutex::Autolock lock(mLock);
    //mm_camera_util_profile("HAL: stopPreview(): E");
    mFaceDetectOn = false;

    // reset recording hint to the value passed from Apps
    const char * str = mParameters.get(QCameraParameters::KEY_RECORDING_HINT);
    if((str != NULL) && !strcmp(str, "true")){
        mRecordingHint = TRUE;
    } else {
        mRecordingHint = FALSE;
    }

    switch(mPreviewState) {
        case QCAMERA_HAL_PREVIEW_START:
            //mPreviewWindow = NULL;
            mPreviewState = QCAMERA_HAL_PREVIEW_STOPPED;
            break;
        case QCAMERA_HAL_PREVIEW_STARTED:
            cancelPictureInternal();
            stopPreviewInternal();
            mPreviewState = QCAMERA_HAL_PREVIEW_STOPPED;
            break;
        case QCAMERA_HAL_RECORDING_STARTED:
            cancelPictureInternal();
            stopRecordingInternal();
            stopPreviewInternal();
            mPreviewState = QCAMERA_HAL_PREVIEW_STOPPED;
            break;
        case QCAMERA_HAL_TAKE_PICTURE:
            cancelPictureInternal();
            mPreviewState = QCAMERA_HAL_PREVIEW_STOPPED;
            break;
        case QCAMERA_HAL_PREVIEW_STOPPED:
        default:
            break;
    }
    ALOGI("stopPreview: X, mPreviewState = %d", mPreviewState);
}

void QCameraHardwareInterface::stopPreviewInternal()
{
    ALOGI("stopPreviewInternal: E");
    status_t ret = NO_ERROR;

    if(!mStreams[MM_CAMERA_PREVIEW]) {
        ALOGE("mStreamDisplay is null");
        return;
    }

    if(isZSLMode()) {
        mStreams[MM_CAMERA_PREVIEW]->streamOff(0);
        mStreams[MM_CAMERA_SNAPSHOT_MAIN]->streamOff(0);
        ret = mCameraHandle->ops->destroy_stream_bundle(mCameraHandle->camera_handle,mChannelId);
        if(ret != MM_CAMERA_OK) {
            ALOGE("%s : ZSL destroy_stream_bundle Error",__func__);
        }
    }else{
        if(rdiMode == STREAM_RAW) {
            mStreams[MM_CAMERA_RDI]->streamOff(0);
            mStreams[MM_CAMERA_RDI]->deinitStream();
            return;
        }
        mStreams[MM_CAMERA_PREVIEW]->streamOff(0);
    }
    if (mStreams[MM_CAMERA_VIDEO])
        mStreams[MM_CAMERA_VIDEO]->deinitStream();
    if (mStreams[MM_CAMERA_SNAPSHOT_MAIN])
        mStreams[MM_CAMERA_SNAPSHOT_MAIN]->deinitStream();
    mStreams[MM_CAMERA_PREVIEW]->deinitStream();

    ALOGI("stopPreviewInternal: X");
}

int QCameraHardwareInterface::previewEnabled()
{
    ALOGI("previewEnabled: E");
    Mutex::Autolock lock(mLock);
    ALOGE("%s: mPreviewState = %d", __func__, mPreviewState);
    switch(mPreviewState) {
    case QCAMERA_HAL_PREVIEW_START:
    case QCAMERA_HAL_PREVIEW_STARTED:
    case QCAMERA_HAL_RECORDING_STARTED:
        return true;
    default:
        return false;
    }
}

status_t QCameraHardwareInterface::startRecording()
{
    ALOGI("startRecording: E");
    status_t ret = NO_ERROR;
    Mutex::Autolock lock(mLock);

    switch(mPreviewState) {
    case QCAMERA_HAL_PREVIEW_STOPPED:
        ALOGE("%s: preview has not been started", __func__);
        ret = UNKNOWN_ERROR;
        break;
    case QCAMERA_HAL_PREVIEW_START:
        ALOGE("%s: no preview native window", __func__);
        ret = UNKNOWN_ERROR;
        break;
    case QCAMERA_HAL_PREVIEW_STARTED:
        if (mRecordingHint == FALSE) {
            ALOGE("%s: start recording when hint is false, stop preview first", __func__);
            stopPreviewInternal();
            mPreviewState = QCAMERA_HAL_PREVIEW_STOPPED;

            // Set recording hint to TRUE
            mRecordingHint = TRUE;
            setRecordingHintValue(mRecordingHint);

            // start preview again
            mPreviewState = QCAMERA_HAL_PREVIEW_START;
            if (startPreview2() == NO_ERROR)
                mPreviewState = QCAMERA_HAL_PREVIEW_STARTED;
        }
        ret =  mStreams[MM_CAMERA_VIDEO]->streamOn();
        if (MM_CAMERA_OK != ret){
            ALOGE("%s: error - mStreamRecord->start!", __func__);
            ret = BAD_VALUE;
            break;
        }
        mPreviewState = QCAMERA_HAL_RECORDING_STARTED;

        if (mPowerModule) {
            if (mPowerModule->powerHint) {
                mPowerModule->powerHint(mPowerModule, POWER_HINT_VIDEO_ENCODE, (void *)"state=1");
            }
        }
        break;
    case QCAMERA_HAL_RECORDING_STARTED:
        ALOGE("%s: ", __func__);
        break;
    case QCAMERA_HAL_TAKE_PICTURE:
    default:
       ret = BAD_VALUE;
       break;
    }
    ALOGI("startRecording: X");
    return ret;
}

void QCameraHardwareInterface::stopRecording()
{
    ALOGI("stopRecording: E");
    Mutex::Autolock lock(mLock);
    switch(mPreviewState) {
    case QCAMERA_HAL_PREVIEW_STOPPED:
    case QCAMERA_HAL_PREVIEW_START:
    case QCAMERA_HAL_PREVIEW_STARTED:
        break;
    case QCAMERA_HAL_RECORDING_STARTED:
        cancelPictureInternal();
        stopRecordingInternal();
        mPreviewState = QCAMERA_HAL_PREVIEW_STARTED;

        if (mPowerModule) {
            if (mPowerModule->powerHint) {
	            mPowerModule->powerHint(mPowerModule, POWER_HINT_VIDEO_ENCODE, (void *)"state=0");
            }
        }
        break;
    case QCAMERA_HAL_TAKE_PICTURE:
    default:
        break;
    }
    ALOGI("stopRecording: X");

}
void QCameraHardwareInterface::stopRecordingInternal()
{
    ALOGI("stopRecordingInternal: E");
    status_t ret = NO_ERROR;

    if(!mStreams[MM_CAMERA_VIDEO]) {
        ALOGE("mStreamRecord is null");
        return;
    }

    /*
    * call QCameraStream_record::stop()
    * Unregister Callback, action stop
    */
    mStreams[MM_CAMERA_VIDEO]->streamOff(0);
    mPreviewState = QCAMERA_HAL_PREVIEW_STARTED;
    ALOGI("stopRecordingInternal: X");
    return;
}

int QCameraHardwareInterface::recordingEnabled()
{
    int ret = 0;
    Mutex::Autolock lock(mLock);
    ALOGV("%s: E", __func__);
    switch(mPreviewState) {
    case QCAMERA_HAL_PREVIEW_STOPPED:
    case QCAMERA_HAL_PREVIEW_START:
    case QCAMERA_HAL_PREVIEW_STARTED:
        break;
    case QCAMERA_HAL_RECORDING_STARTED:
        ret = 1;
        break;
    case QCAMERA_HAL_TAKE_PICTURE:
    default:
        break;
    }
    ALOGV("%s: X, ret = %d", __func__, ret);
    return ret;   //isRecordingRunning();
}

/**
* Release a record frame previously returned by CAMERA_MSG_VIDEO_FRAME.
*/
void QCameraHardwareInterface::releaseRecordingFrame(const void *opaque)
{
    ALOGV("%s : BEGIN",__func__);
    if(mStreams[MM_CAMERA_VIDEO] == NULL) {
        ALOGE("Record stream Not Initialized");
        return;
    }
    mStreams[MM_CAMERA_VIDEO]->releaseRecordingFrame(opaque);
    ALOGV("%s : END",__func__);
    return;
}

status_t QCameraHardwareInterface::autoFocusEvent(cam_ctrl_status_t *status, app_notify_cb_t *app_cb)
{
    ALOGE("autoFocusEvent: E");
    int ret = NO_ERROR;
/************************************************************
  BEGIN MUTEX CODE
*************************************************************/

    ALOGE("%s:%d: Trying to acquire AF bit lock",__func__,__LINE__);
    mAutofocusLock.lock();
    ALOGE("%s:%d: Acquired AF bit lock",__func__,__LINE__);

    if(mAutoFocusRunning==false) {
      ALOGE("%s:AF not running, discarding stale event",__func__);
      mAutofocusLock.unlock();
      return ret;
    }

    /* If autofocus call has been made during CAF, CAF will be locked.
	* We specifically need to call cancelAutoFocus to unlock CAF.
    * In that sense, AF is still running.*/
    isp3a_af_mode_t afMode = getAutoFocusMode(mParameters);
    if (afMode == AF_MODE_CAF) {
       mNeedToUnlockCaf = true;
    }
    mAutoFocusRunning = false;
    mAutofocusLock.unlock();

/************************************************************
  END MUTEX CODE
*************************************************************/
    if(status==NULL) {
      ALOGE("%s:NULL ptr received for status",__func__);
      return BAD_VALUE;
    }

    /* update focus distances after autofocus is done */
    if(updateFocusDistances() != NO_ERROR) {
       ALOGE("%s: updateFocusDistances failed for %d", __FUNCTION__, mFocusMode);
    }

    /*(Do?) we need to make sure that the call back is the
      last possible step in the execution flow since the same
      context might be used if a fail triggers another round
      of AF then the mAutoFocusRunning flag and other state
      variables' validity will be under question*/

    if (mNotifyCb && ( mMsgEnabled & CAMERA_MSG_FOCUS)){
      ALOGE("%s:Issuing callback to service",__func__);

      /* "Accepted" status is not appropriate it should be used for
        initial cmd, event reporting should only give use SUCCESS/FAIL
        */

      app_cb->notifyCb  = mNotifyCb;
      app_cb->argm_notify.msg_type = CAMERA_MSG_FOCUS;
      app_cb->argm_notify.ext2 = 0;
      app_cb->argm_notify.cookie =  mCallbackCookie;

      ALOGE("Auto foucs state =%d", *status);
      if(*status==CAM_CTRL_SUCCESS) {
        app_cb->argm_notify.ext1 = true;
      }
      else if(*status==CAM_CTRL_FAILED){
        app_cb->argm_notify.ext1 = false;
      }
      else{
        app_cb->notifyCb  = NULL;
        ALOGE("%s:Unknown AF status (%d) received",__func__,*status);
      }

    }/*(mNotifyCb && ( mMsgEnabled & CAMERA_MSG_FOCUS))*/
    else{
      ALOGE("%s:Call back not enabled",__func__);
    }

    ALOGE("autoFocusEvent: X");
    return ret;

}

status_t QCameraHardwareInterface::cancelPicture()
{
    ALOGI("cancelPicture: E, mPreviewState = %d", mPreviewState);
    status_t ret = MM_CAMERA_OK;
    Mutex::Autolock lock(mLock);

    switch(mPreviewState) {
        case QCAMERA_HAL_PREVIEW_STOPPED:
        case QCAMERA_HAL_PREVIEW_START:
        case QCAMERA_HAL_PREVIEW_STARTED:
        default:
            break;
        case QCAMERA_HAL_TAKE_PICTURE:
            ret = cancelPictureInternal();
            mPreviewState = QCAMERA_HAL_PREVIEW_STOPPED;
            break;
        case QCAMERA_HAL_RECORDING_STARTED:
            ret = cancelPictureInternal();
            break;
    }
    ALOGI("cancelPicture: X");
    return ret;
}

status_t QCameraHardwareInterface::cancelPictureInternal()
{
    ALOGI("%s: E mPreviewState=%d", __func__ , mPreviewState);
    status_t ret = MM_CAMERA_OK;

    /* set rawdata proc thread and jpeg notify thread to inactive state */
    /* no need for notify thread as a sync call for stop cmd */
    mNotifyTh->sendCmd(CAMERA_CMD_TYPE_STOP_DATA_PROC, FALSE, TRUE);
    /* dataProc Thread need to process "stop" as sync call because abort jpeg job should be a sync call*/
    mDataProcTh->sendCmd(CAMERA_CMD_TYPE_STOP_DATA_PROC, TRUE, TRUE);

    if (isZSLMode()) {
        ret = mCameraHandle->ops->cancel_super_buf_request(mCameraHandle->camera_handle, mChannelId);
    } else {
        mStreams[MM_CAMERA_SNAPSHOT_MAIN]->streamOff(0);
        mStreams[MM_CAMERA_SNAPSHOT_THUMBNAIL]->streamOff(0);
        ret = mCameraHandle->ops->destroy_stream_bundle(mCameraHandle->camera_handle, mChannelId);
        if(ret != MM_CAMERA_OK) {
            ALOGE("%s : destroy_stream_bundle Error",__func__);
        }
        if(mPreviewState != QCAMERA_HAL_RECORDING_STARTED) {
            mStreams[MM_CAMERA_SNAPSHOT_MAIN]->deinitStream();
            mStreams[MM_CAMERA_SNAPSHOT_THUMBNAIL]->deinitStream();
        }
    }
    ALOGI("cancelPictureInternal: X");
    return ret;
}

status_t  QCameraHardwareInterface::takePicture()
{
    ALOGE("takePicture: E");
    status_t ret = MM_CAMERA_OK;
    uint32_t stream_info;
    uint32_t stream[2];
    mm_camera_bundle_attr_t attr;
    mm_camera_op_mode_type_t op_mode=MM_CAMERA_OP_MODE_CAPTURE;
    int num_streams = 0;
    Mutex::Autolock lock(mLock);

    /* set rawdata proc thread and jpeg notify thread to active state */
    mNotifyTh->sendCmd(CAMERA_CMD_TYPE_START_DATA_PROC, FALSE, FALSE);
    mDataProcTh->sendCmd(CAMERA_CMD_TYPE_START_DATA_PROC, FALSE, FALSE);

    switch(mPreviewState) {
    case QCAMERA_HAL_PREVIEW_STARTED:
        {
            if (isZSLMode()) {
                ALOGE("ZSL: takePicture");

                ret = mCameraHandle->ops->request_super_buf(
                          mCameraHandle->camera_handle,
                          mChannelId,
                          getNumOfSnapshots());
                if (MM_CAMERA_OK != ret){
                    ALOGE("%s: error - can't start Snapshot streams!", __func__);
                    return BAD_VALUE;
                }
                return ret;
            }

            /*prepare snapshot, e.g LED*/
            takePicturePrepareHardware( );

            /* stop preview */
            stopPreviewInternal();
            /*Currently concurrent streaming is not enabled for snapshot
            So in snapshot mode, we turn of the RDI channel and configure backend
            for only pixel stream*/

            if (!isRawSnapshot()) {
                op_mode=MM_CAMERA_OP_MODE_CAPTURE;
            } else {
                ALOGV("%s: Raw snapshot so setting op mode to raw", __func__);
                op_mode=MM_CAMERA_OP_MODE_RAW;
            }
            ret = mCameraHandle->ops->set_parm(mCameraHandle->camera_handle, MM_CAMERA_PARM_OP_MODE, &op_mode);

            if(MM_CAMERA_OK != ret) {
                ALOGE("%s: X :set mode MM_CAMERA_OP_MODE_CAPTURE err=%d\n", __func__, ret);
                return BAD_VALUE;
            }

            ret = setDimension();
            if (MM_CAMERA_OK != ret) {
                ALOGE("%s: error - can't Set Dimensions!", __func__);
                return BAD_VALUE;
            }

            //added to support hdr
            bool hdr;
            int frm_num = 1;
            int exp[MAX_HDR_EXP_FRAME_NUM];
            hdr = getHdrInfoAndSetExp(MAX_HDR_EXP_FRAME_NUM, &frm_num, exp);
            initHdrInfoForSnapshot(hdr, frm_num, exp); // - for hdr figure out equivalent of mStreamSnap
            memset(&attr, 0, sizeof(mm_camera_bundle_attr_t));
            attr.notify_mode = MM_CAMERA_SUPER_BUF_NOTIFY_CONTINUOUS;

            num_streams = 0;
            mStreams[MM_CAMERA_SNAPSHOT_MAIN]->initStream(TRUE, TRUE);
            if (NO_ERROR!=ret) {
                ALOGE("%s E: can't init native camera snapshot main ch\n",__func__);
                return ret;
            }
            stream[num_streams++] = mStreams[MM_CAMERA_SNAPSHOT_MAIN]->mStreamId;

            if (!isRawSnapshot()) {
                mStreams[MM_CAMERA_SNAPSHOT_THUMBNAIL]->initStream(TRUE, TRUE);
                if (NO_ERROR!=ret) {
                   ALOGE("%s E: can't init native camera snapshot thumb ch\n",__func__);
                   return ret;
                }
                stream[num_streams++] = mStreams[MM_CAMERA_SNAPSHOT_THUMBNAIL]->mStreamId;
            }
            ret = mCameraHandle->ops->init_stream_bundle(
                      mCameraHandle->camera_handle,
                      mChannelId,
                      superbuf_cb_routine,
                      this,
                      &attr,
                      num_streams,
                      stream);
            if (MM_CAMERA_OK != ret){
                ALOGE("%s: error - can't init Snapshot streams!", __func__);
                return BAD_VALUE;
            }
            ret = mStreams[MM_CAMERA_SNAPSHOT_MAIN]->streamOn();
            if (MM_CAMERA_OK != ret){
                ALOGE("%s: error - can't start Snapshot streams!", __func__);
                mCameraHandle->ops->destroy_stream_bundle(
                   mCameraHandle->camera_handle,
                   mChannelId);
                mStreams[MM_CAMERA_SNAPSHOT_MAIN]->deinitStream();
                mStreams[MM_CAMERA_SNAPSHOT_THUMBNAIL]->deinitStream();
                return BAD_VALUE;
            }
            if (!isRawSnapshot()) {
                ret = mStreams[MM_CAMERA_SNAPSHOT_THUMBNAIL]->streamOn();
                if (MM_CAMERA_OK != ret){
                    ALOGE("%s: error - can't start Thumbnail streams!", __func__);
                    mStreams[MM_CAMERA_SNAPSHOT_MAIN]->streamOff(0);
                    mCameraHandle->ops->destroy_stream_bundle(
                       mCameraHandle->camera_handle,
                       mChannelId);
                    mStreams[MM_CAMERA_SNAPSHOT_MAIN]->deinitStream();
                    mStreams[MM_CAMERA_SNAPSHOT_THUMBNAIL]->deinitStream();
                    return BAD_VALUE;
                }
            }
            mPreviewState = QCAMERA_HAL_TAKE_PICTURE;
        }
        break;
    case QCAMERA_HAL_TAKE_PICTURE:
          break;
    case QCAMERA_HAL_PREVIEW_STOPPED:
    case QCAMERA_HAL_PREVIEW_START:
        ret = UNKNOWN_ERROR;
        break;
    case QCAMERA_HAL_RECORDING_STARTED:
        stream[0] = mStreams[MM_CAMERA_SNAPSHOT_MAIN]->mStreamId;
        memset(&attr, 0, sizeof(mm_camera_bundle_attr_t));
        attr.notify_mode = MM_CAMERA_SUPER_BUF_NOTIFY_CONTINUOUS;
        ret = mCameraHandle->ops->init_stream_bundle(
                  mCameraHandle->camera_handle,
                  mChannelId,
                  superbuf_cb_routine,
                  this,
                  &attr,
                  1,
                  stream);
        if (MM_CAMERA_OK != ret){
            ALOGE("%s: error - can't init Snapshot streams!", __func__);
            return BAD_VALUE;
        }
        ret = mStreams[MM_CAMERA_SNAPSHOT_MAIN]->streamOn();
        if (MM_CAMERA_OK != ret){
            ALOGE("%s: error - can't start Snapshot streams!", __func__);
            mCameraHandle->ops->destroy_stream_bundle(
               mCameraHandle->camera_handle,
               mChannelId);
            return BAD_VALUE;
        }
        break;
    default:
        ret = UNKNOWN_ERROR;
        break;
    }
    ALOGI("takePicture: X");
    return ret;
}

status_t QCameraHardwareInterface::autoFocus()
{
    ALOGI("autoFocus: E");
    status_t ret = NO_ERROR;

    Mutex::Autolock lock(mLock);
    ALOGI("autoFocus: Got lock");
    bool status = true;
    isp3a_af_mode_t afMode = getAutoFocusMode(mParameters);

    if(mAutoFocusRunning==true){
      ALOGE("%s:AF already running should not have got this call",__func__);
      return NO_ERROR;
    }

    if (afMode == AF_MODE_MAX) {
      /* This should never happen. We cannot send a
       * callback notifying error from this place because
       * the CameraService has called this function after
       * acquiring the lock. So if we try to issue a callback
       * from this place, the callback will try to acquire
       * the same lock in CameraService and it will result
       * in deadlock. So, let the call go in to the lower
       * layer. The lower layer will anyway return error if
       * the autofocus is not supported or if the focus
       * value is invalid.
       * Just print out the error. */
      ALOGE("%s:Invalid AF mode (%d)", __func__, afMode);
    }

    ALOGI("%s:AF start (mode %d)", __func__, afMode);
    if(MM_CAMERA_OK != mCameraHandle->ops->start_focus(mCameraHandle->camera_handle,
               mChannelId,0,(uint32_t)&afMode)){
      ALOGE("%s: AF command failed err:%d error %s",
           __func__, errno, strerror(errno));
      return UNKNOWN_ERROR;
    }

    mAutoFocusRunning = true;
    ALOGE("autoFocus: X");
    return ret;
}

status_t QCameraHardwareInterface::cancelAutoFocus()
{
    ALOGE("cancelAutoFocus: E");
    status_t ret = NO_ERROR;
    Mutex::Autolock lock(mLock);

/**************************************************************
  BEGIN MUTEX CODE
*************************************************************/

    mAutofocusLock.lock();
    if(mAutoFocusRunning || mNeedToUnlockCaf) {

      mAutoFocusRunning = false;
      mNeedToUnlockCaf = false;
      mAutofocusLock.unlock();

    }else/*(!mAutoFocusRunning)*/{

      mAutofocusLock.unlock();
      ALOGE("%s:Af not running",__func__);
      return NO_ERROR;
    }
/**************************************************************
  END MUTEX CODE
*************************************************************/

    if(MM_CAMERA_OK!= mCameraHandle->ops->abort_focus(mCameraHandle->camera_handle,
               mChannelId,0)){
        ALOGE("%s: AF command failed err:%d error %s",__func__, errno,strerror(errno));
    }
    ALOGE("cancelAutoFocus: X");
    return NO_ERROR;
}

/*==========================================================================
 * FUNCTION    - processprepareSnapshotEvent -
 *
 * DESCRIPTION:  Process the event of preparesnapshot done msg
                 unblock prepareSnapshotAndWait( )
 *=========================================================================*/
void QCameraHardwareInterface::processprepareSnapshotEvent(cam_ctrl_status_t *status)
{
    ALOGI("processprepareSnapshotEvent: E");
    pthread_mutex_lock(&mAsyncCmdMutex);
    pthread_cond_signal(&mAsyncCmdWait);
    pthread_mutex_unlock(&mAsyncCmdMutex);
    ALOGI("processprepareSnapshotEvent: X");
}

void QCameraHardwareInterface::roiEvent(fd_roi_t roi,app_notify_cb_t *app_cb)
{
    ALOGE("roiEvent: E");

    if(mStreams[MM_CAMERA_PREVIEW]) {
        mStreams[MM_CAMERA_PREVIEW]->notifyROIEvent(roi);
    }
    ALOGE("roiEvent: X");
}


void QCameraHardwareInterface::handleZoomEventForSnapshot(void)
{
    ALOGI("%s: E", __func__);

    if (mStreams[MM_CAMERA_SNAPSHOT_MAIN] != NULL) {
        mStreams[MM_CAMERA_SNAPSHOT_MAIN]->setCrop();
    }
    if (mStreams[MM_CAMERA_SNAPSHOT_THUMBNAIL] != NULL) {
        mStreams[MM_CAMERA_SNAPSHOT_THUMBNAIL]->setCrop();
    }
    ALOGI("%s: X", __func__);
}

void QCameraHardwareInterface::handleZoomEventForPreview(app_notify_cb_t *app_cb)
{
    ALOGI("%s: E", __func__);

    /*regular zooming or smooth zoom stopped*/
    if (mStreams[MM_CAMERA_PREVIEW] != NULL) {
        ALOGI("%s: Fetching crop info", __func__);
        mStreams[MM_CAMERA_PREVIEW]->setCrop();
        ALOGI("%s: Currrent zoom :%d",__func__, mCurrentZoom);
    }

    ALOGI("%s: X", __func__);
}

void QCameraHardwareInterface::zoomEvent(cam_ctrl_status_t *status, app_notify_cb_t *app_cb)
{
    ALOGI("zoomEvent: state:%d E",mPreviewState);
    switch (mPreviewState) {
    case QCAMERA_HAL_PREVIEW_STOPPED:
        break;
    case QCAMERA_HAL_PREVIEW_START:
        break;
    case QCAMERA_HAL_PREVIEW_STARTED:
        handleZoomEventForPreview(app_cb);
        if (isZSLMode())
          handleZoomEventForSnapshot();
        break;
    case QCAMERA_HAL_RECORDING_STARTED:
        handleZoomEventForPreview(app_cb);
        if (mFullLiveshotEnabled)
            handleZoomEventForSnapshot();
        break;
    case QCAMERA_HAL_TAKE_PICTURE:
        if(isZSLMode())
            handleZoomEventForPreview(app_cb);
        handleZoomEventForSnapshot();
        break;
    default:
        break;
    }
    ALOGI("zoomEvent: X");
}

void QCameraHardwareInterface::dumpFrameToFile(const void * data, uint32_t size, char* name, char* ext, int index)
{
    char buf[32];
    int file_fd;
    if ( data != NULL) {
        char * str;
        snprintf(buf, sizeof(buf), "/data/%s_%d.%s", name, index, ext);
        ALOGE("marvin, %s size =%d", buf, size);
        file_fd = open(buf, O_RDWR | O_CREAT, 0777);
        write(file_fd, data, size);
        close(file_fd);
    }
}

void QCameraHardwareInterface::dumpFrameToFile(mm_camera_buf_def_t* newFrame,
  HAL_cam_dump_frm_type_t frm_type)
{
  ALOGV("%s: E", __func__);
  int32_t enabled = 0;
  int frm_num;
  uint32_t  skip_mode;
  char value[PROPERTY_VALUE_MAX];
  char buf[32];
  int main_422 = 1;
  property_get("persist.camera.dumpimg", value, "0");
  enabled = atoi(value);

  ALOGV(" newFrame =%p, frm_type = %x, enabled=%x", newFrame, frm_type, enabled);
  if(enabled & HAL_DUMP_FRM_MASK_ALL) {
    if((enabled & frm_type) && newFrame) {
      frm_num = ((enabled & 0xffff0000) >> 16);
      if(frm_num == 0) frm_num = 10; /*default 10 frames*/
      if(frm_num > 256) frm_num = 256; /*256 buffers cycle around*/
      skip_mode = ((enabled & 0x0000ff00) >> 8);
      if(skip_mode == 0) skip_mode = 1; /*no -skip */

      if( mDumpSkipCnt % skip_mode == 0) {
        if (mDumpFrmCnt >= 0 && mDumpFrmCnt <= frm_num) {
          int w, h;
          int file_fd;
          switch (frm_type) {
          case  HAL_DUMP_FRM_PREVIEW:
            w = mDimension.display_width;
            h = mDimension.display_height;
            snprintf(buf, sizeof(buf), "/data/%dp_%dx%d_%d.yuv", mDumpFrmCnt, w, h, newFrame->frame_idx);
            file_fd = open(buf, O_RDWR | O_CREAT, 0777);
            break;
          case HAL_DUMP_FRM_VIDEO:
            w = mDimension.video_width;
            h = mDimension.video_height;
            snprintf(buf, sizeof(buf),"/data/%dv_%dx%d_%d.yuv", mDumpFrmCnt, w, h, newFrame->frame_idx);
            file_fd = open(buf, O_RDWR | O_CREAT, 0777);
            break;
          case HAL_DUMP_FRM_MAIN:
            w = mDimension.picture_width;
            h = mDimension.picture_height;
            snprintf(buf, sizeof(buf), "/data/%dm_%dx%d_%d.yuv", mDumpFrmCnt, w, h, newFrame->frame_idx);
            file_fd = open(buf, O_RDWR | O_CREAT, 0777);
            if (mDimension.main_img_format == CAMERA_YUV_422_NV16 ||
                mDimension.main_img_format == CAMERA_YUV_422_NV61)
              main_422 = 2;
            break;
          case HAL_DUMP_FRM_THUMBNAIL:
            w = mDimension.ui_thumbnail_width;
            h = mDimension.ui_thumbnail_height;
            snprintf(buf, sizeof(buf),"/data/%dt_%dx%d_%d.yuv", mDumpFrmCnt, w, h, newFrame->frame_idx);
            file_fd = open(buf, O_RDWR | O_CREAT, 0777);
            break;
          case HAL_DUMP_FRM_RDI:
              w = mRdiWidth;
              h = mRdiHeight;
              snprintf(buf, sizeof(buf),"/data/%dr_%dx%d.raw", mDumpFrmCnt, w, h);
              file_fd = open(buf, O_RDWR | O_CREAT, 0777);
              break;
          default:
            w = h = 0;
            file_fd = -1;
            break;
          }

          if (file_fd < 0) {
            ALOGE("%s: cannot open file:type=%d\n", __func__, frm_type);
          } else {
            write(file_fd, (const void *)(newFrame->buffer), w * h);
            write(file_fd, (const void *)
              (newFrame->buffer), w * h / 2 * main_422);
            close(file_fd);
            ALOGE("dump %s", buf);
          }
        } else if(frm_num == 256){
          mDumpFrmCnt = 0;
        }
        mDumpFrmCnt++;
      }
      mDumpSkipCnt++;
    }
  }  else {
    mDumpFrmCnt = 0;
  }
  ALOGV("%s: X", __func__);
}

status_t QCameraHardwareInterface::setPreviewWindow(preview_stream_ops_t* window)
{
    status_t retVal = NO_ERROR;
    ALOGE(" %s: E mPreviewState = %d, mStreamDisplay = %p",
          __func__, mPreviewState, mStreams[MM_CAMERA_PREVIEW]);
    if( window == NULL) {
        ALOGE("%s:Received Setting NULL preview window", __func__);
    }
    Mutex::Autolock lock(mLock);
    switch(mPreviewState) {
    case QCAMERA_HAL_PREVIEW_START:
        mPreviewWindow = window;
        if(mPreviewWindow) {
            /* we have valid surface now, start preview */
            ALOGE("%s:  calling startPreview2", __func__);
            retVal = startPreview2();
            if(retVal == NO_ERROR)
                mPreviewState = QCAMERA_HAL_PREVIEW_STARTED;
            ALOGE("%s:  startPreview2 done, mPreviewState = %d", __func__, mPreviewState);
        } else
            ALOGE("%s: null window received, mPreviewState = %d", __func__, mPreviewState);
        break;
    case QCAMERA_HAL_PREVIEW_STARTED:
        /* new window comes */
        ALOGE("%s: bug, cannot handle new window in started state", __func__);
        //retVal = UNKNOWN_ERROR;
        break;
    case QCAMERA_HAL_PREVIEW_STOPPED:
    case QCAMERA_HAL_TAKE_PICTURE:
        mPreviewWindow = window;
        ALOGE("%s: mPreviewWindow = 0x%p, mStreamDisplay = 0x%p",
                                    __func__, mPreviewWindow, mStreams[MM_CAMERA_PREVIEW]);
        break;
    default:
        ALOGE("%s: bug, cannot handle new window in state %d", __func__, mPreviewState);
        retVal = UNKNOWN_ERROR;
        break;
    }
    ALOGE(" %s : X, mPreviewState = %d", __FUNCTION__, mPreviewState);
    return retVal;
}

int QCameraHardwareInterface::storeMetaDataInBuffers(int enable)
{
    /* this is a dummy func now. fix me later */
    mStoreMetaDataInFrame = enable;
    return 0;
}

int QCameraHardwareInterface::allocate_ion_memory(QCameraHalMemInfo_t *mem_info, int ion_type)
{
    int rc = 0;
    struct ion_handle_data handle_data;
    struct ion_allocation_data alloc;
    struct ion_fd_data ion_info_fd;
    int main_ion_fd = 0;

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

int QCameraHardwareInterface::deallocate_ion_memory(QCameraHalMemInfo_t *mem_info)
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

int QCameraHardwareInterface::initHeapMem( QCameraHalHeap_t *heap,
                                           int num_of_buf,
                                           uint32_t buf_len,
                                           int pmem_type,
                                           mm_camera_frame_len_offset* offset,
                                           mm_camera_buf_def_t *buf_def)
{
    int rc = 0;
    int i;
    int path;
    ALOGE("Init Heap =%p. pmem_type =%d, num_of_buf=%d. buf_len=%d",
         heap,  pmem_type, num_of_buf, buf_len);
    if(num_of_buf > MM_CAMERA_MAX_NUM_FRAMES || heap == NULL ||
       mGetMemory == NULL ) {
        ALOGE("Init Heap error");
        rc = -1;
        return rc;
    }

    memset(heap, 0, sizeof(QCameraHalHeap_t));
    heap->buffer_count = num_of_buf;
    for(i = 0; i < num_of_buf; i++) {
        heap->mem_info[i].size = buf_len;
#ifdef USE_ION
        if (isZSLMode()) {
            rc = allocate_ion_memory(&heap->mem_info[i],
                                     ((0x1 << CAMERA_ZSL_ION_HEAP_ID) | (0x1 << CAMERA_ZSL_ION_FALLBACK_HEAP_ID)));
        }
        else {
            rc = allocate_ion_memory(&heap->mem_info[i],
                                     ((0x1 << CAMERA_ION_HEAP_ID) | (0x1 << CAMERA_ION_FALLBACK_HEAP_ID)));
        }

        if (rc < 0) {
            ALOGE("%s: ION allocation failed\n", __func__);
            break;
        }
#else
        if (pmem_type == MSM_PMEM_MAX){
            ALOGE("%s : USE_ION not defined, pmemtype == MSM_PMEM_MAX, so ret -1", __func__);
            rc = -1;
            break;
        }
        else {
            heap->mem_info[i].fd = open("/dev/pmem_adsp", O_RDWR|O_SYNC);
            if ( heap->mem_info[i].fd <= 0) {
                ALOGE("Open fail: heap->fd[%d] =%d", i, heap->mem_info[i].fd);
                rc = -1;
                break;
            }
        }
#endif
        heap->camera_memory[i] = mGetMemory(heap->mem_info[i].fd,
                                            heap->mem_info[i].size,
                                            1,
                                            (void *)this);

        if (heap->camera_memory[i] == NULL ) {
            ALOGE("Getmem fail %d: ", i);
            rc = -1;
            break;
        }

        if(buf_def != NULL && offset != NULL) {
            buf_def[i].fd = heap->mem_info[i].fd;
            buf_def[i].frame_len = heap->mem_info[i].size;
            buf_def[i].buffer = heap->camera_memory[i]->data;
            buf_def[i].mem_info = (void *)&heap->mem_info[i];
            buf_def[i].num_planes = offset->num_planes;
            /* Plane 0 needs to be set seperately. Set other planes
             * in a loop. */
            buf_def[i].planes[0].length = offset->mp[0].len;
            buf_def[i].planes[0].m.userptr = heap->mem_info[i].fd;
            buf_def[i].planes[0].data_offset = offset->mp[0].offset;
            buf_def[i].planes[0].reserved[0] = 0;
            for (int j = 1; j < buf_def[i].num_planes; j++) {
                 buf_def[i].planes[j].length = offset->mp[j].len;
                 buf_def[i].planes[j].m.userptr = heap->mem_info[i].fd;
                 buf_def[i].planes[j].data_offset = offset->mp[j].offset;
                 buf_def[i].planes[j].reserved[0] =
                     buf_def[i].planes[j-1].reserved[0] +
                     buf_def[i].planes[j-1].length;
            }
        }

        ALOGE("heap->fd[%d] =%d, camera_memory=%p", i,
              heap->mem_info[i].fd, heap->camera_memory[i]);
        heap->local_flag[i] = 1;
    }
    if( rc < 0) {
        releaseHeapMem(heap);
    }
    return rc;
}

int QCameraHardwareInterface::releaseHeapMem(QCameraHalHeap_t *heap)
{
	int rc = 0;
	ALOGE("Release %p", heap);
	if (heap != NULL) {
		for (int i = 0; i < heap->buffer_count; i++) {
			if(heap->camera_memory[i] != NULL) {
				heap->camera_memory[i]->release( heap->camera_memory[i] );
				heap->camera_memory[i] = NULL;
			} else if (heap->mem_info[i].fd <= 0) {
				ALOGE("impossible: camera_memory[%d] = %p, fd = %d",
				i, heap->camera_memory[i], heap->mem_info[i].fd);
			}

#ifdef USE_ION
            deallocate_ion_memory(&heap->mem_info[i]);
#endif
		}
        memset(heap, 0, sizeof(QCameraHalHeap_t));
	}
	return rc;
}

preview_format_info_t  QCameraHardwareInterface::getPreviewFormatInfo( )
{
  return mPreviewFormatInfo;
}

void QCameraHardwareInterface::wdenoiseEvent(cam_ctrl_status_t status, void *cookie)
{

}

bool QCameraHardwareInterface::isWDenoiseEnabled()
{
    return mDenoiseValue;
}

void QCameraHardwareInterface::takePicturePrepareHardware()
{
    ALOGV("%s: E", __func__);

    /* Prepare snapshot*/
    mCameraHandle->ops->prepare_snapshot(mCameraHandle->camera_handle,
                  mChannelId,
                  0);
    ALOGV("%s: X", __func__);
}

bool QCameraHardwareInterface::isNoDisplayMode()
{
  return (mNoDisplayMode != 0);
}

void QCameraHardwareInterface::restartPreview()
{
    if (QCAMERA_HAL_PREVIEW_STARTED == mPreviewState) {
        stopPreviewInternal();
        mPreviewState = QCAMERA_HAL_PREVIEW_STOPPED;
    }
    startPreview2();
    mPreviewState = QCAMERA_HAL_PREVIEW_STARTED;
}

// added to support hdr
bool QCameraHardwareInterface::getHdrInfoAndSetExp(int max_num_frm, int *num_frame, int *exp)
{
    bool rc = FALSE;
    ALOGE("%s, mHdrMode = %d, HDR_MODE = %d", __func__, mHdrMode, HDR_MODE);
    if (mHdrMode == HDR_MODE && num_frame != NULL && exp != NULL &&
        mRecordingHint != TRUE &&
        mPreviewState != QCAMERA_HAL_RECORDING_STARTED ) {
        ALOGE("%s : mHdrMode == HDR_MODE", __func__);
        int ret = 0;
        *num_frame = 1;
        exp_bracketing_t temp;
        memset(&temp, 0, sizeof(exp_bracketing_t));
        ret = mCameraHandle->ops->get_parm(mCameraHandle->camera_handle, MM_CAMERA_PARM_HDR, (void *)&temp );
        ALOGE("hdr - %s : ret = %d", __func__, ret);
        if (ret == NO_ERROR && max_num_frm > 0) {
            ALOGE("%s ret == NO_ERROR and max_num_frm = %d", __func__, max_num_frm);
            /*set as AE Bracketing mode*/
            temp.hdr_enable = FALSE;
            temp.mode = HDR_MODE;
            temp.total_hal_frames = temp.total_frames;
            ret = native_set_parms(MM_CAMERA_PARM_HDR,
                                   sizeof(exp_bracketing_t), (void *)&temp);
            ALOGE("%s, ret from set_parm = %d", __func__, ret);
            if (ret) {
                char *val, *exp_value, *prev_value;
                int i;
                exp_value = (char *) temp.values;
                i = 0;
                val = strtok_r(exp_value,",", &prev_value);
                while (val != NULL ){
                    exp[i++] = atoi(val);
                    if(i >= max_num_frm )
                        break;
                    val = strtok_r(NULL, ",", &prev_value);
                }
                *num_frame =temp.total_frames;
                rc = TRUE;
            }
        } else {
            temp.total_frames = 1;
        }
        /* Application waits until this many snapshots before restarting preview */
        mParameters.set("num-snaps-per-shutter", 2);
    }
    ALOGE("%s, hdr - rc = %d, num_frame = %d", __func__, rc, *num_frame);
    return rc;
}

status_t QCameraHardwareInterface::initHistogramBuffers()
{
    int page_size_minus_1 = getpagesize() - 1;
    int statSize = sizeof (camera_preview_histogram_info );
    int32_t mAlignedStatSize = ((statSize + page_size_minus_1)
                                & (~page_size_minus_1));
    mm_camera_frame_map_type map_buf;
    ALOGI("%s E ", __func__);

    if (mHistServer.active) {
        ALOGI("%s Previous buffers not deallocated yet. ", __func__);
        return BAD_VALUE;
    }

    mStatSize = sizeof(uint32_t) * HISTOGRAM_STATS_SIZE;
    mCurrentHisto = -1;

    memset(&map_buf, 0, sizeof(map_buf));
    for(int cnt = 0; cnt < NUM_HISTOGRAM_BUFFERS; cnt++) {
        mStatsMapped[cnt] = mGetMemory(-1, mStatSize, 1, mCallbackCookie);
        if(mStatsMapped[cnt] == NULL) {
            ALOGE("Failed to get camera memory for stats heap index: %d", cnt);
            return NO_MEMORY;
        } else {
           ALOGI("Received following info for stats mapped data:%p,handle:%p,"
                 " size:%d,release:%p", mStatsMapped[cnt]->data,
                 mStatsMapped[cnt]->handle, mStatsMapped[cnt]->size,
                 mStatsMapped[cnt]->release);
        }
        mHistServer.mem_info[cnt].size = sizeof(camera_preview_histogram_info);
#ifdef USE_ION
        int flag = (0x1 << ION_CP_MM_HEAP_ID | 0x1 << ION_IOMMU_HEAP_ID);
        if(allocate_ion_memory(&mHistServer.mem_info[cnt], flag) < 0) {
            ALOGE("%s ION alloc failed for %d\n", __func__, cnt);
            return NO_MEMORY;
        }
#else
        mHistServer.mem_info[cnt].fd = open("/dev/pmem_adsp", O_RDWR|O_SYNC);
        if(mHistServer.mem_info[cnt].fd <= 0) {
            ALOGE("%s: no pmem for frame %d", __func__, cnt);
            return NO_INIT;
        }
#endif
        mHistServer.camera_memory[cnt] = mGetMemory(mHistServer.mem_info[cnt].fd,
                                                    mHistServer.mem_info[cnt].size,
                                                    1,
                                                    mCallbackCookie);
        if(mHistServer.camera_memory[cnt] == NULL) {
            ALOGE("Failed to get camera memory for server side "
                  "histogram index: %d", cnt);
            return NO_MEMORY;
        } else {
            ALOGE("Received following info for server side histogram data:%p,"
                  " handle:%p, size:%d,release:%p",
                  mHistServer.camera_memory[cnt]->data,
                  mHistServer.camera_memory[cnt]->handle,
                  mHistServer.camera_memory[cnt]->size,
                  mHistServer.camera_memory[cnt]->release);
        }
        /*Register buffer at back-end*/
        map_buf.fd = mHistServer.mem_info[cnt].fd;
        map_buf.frame_idx = cnt;
        map_buf.size = mHistServer.mem_info[cnt].size;
        map_buf.ext_mode = 0;
        map_buf.is_hist = TRUE;
        mCameraHandle->ops->send_command(mCameraHandle->camera_handle,
                                         MM_CAMERA_CMD_TYPE_NATIVE,
                                         NATIVE_CMD_ID_SOCKET_MAP,
                                         sizeof(map_buf), &map_buf);
    }
    mHistServer.active = TRUE;
    ALOGI("%s X", __func__);
    return NO_ERROR;
}

status_t QCameraHardwareInterface::deInitHistogramBuffers()
{
    mm_camera_frame_unmap_type unmap_buf;
    memset(&unmap_buf, 0, sizeof(unmap_buf));

    ALOGI("%s E", __func__);

    if (!mHistServer.active) {
        ALOGI("%s Histogram buffers not active. return. ", __func__);
        return NO_ERROR;
    }

    //release memory
    for(int i = 0; i < NUM_HISTOGRAM_BUFFERS; i++) {
        if(mStatsMapped[i] != NULL) {
            mStatsMapped[i]->release(mStatsMapped[i]);
        }

        unmap_buf.ext_mode = 0;
        unmap_buf.frame_idx = i;
        unmap_buf.is_hist = TRUE;
        mCameraHandle->ops->send_command(mCameraHandle->camera_handle,
                                         MM_CAMERA_CMD_TYPE_NATIVE,
                                         NATIVE_CMD_ID_SOCKET_UNMAP,
                                         sizeof(unmap_buf), &unmap_buf);

        if(mHistServer.camera_memory[i] != NULL) {
            mHistServer.camera_memory[i]->release(mHistServer.camera_memory[i]);
        }
#ifdef USE_ION
        deallocate_ion_memory(&mHistServer.mem_info[i]);
#endif
    }
    mHistServer.active = FALSE;
    ALOGI("%s X", __func__);
    return NO_ERROR;
}

mm_jpeg_color_format QCameraHardwareInterface::getColorfmtFromImgFmt(uint32_t img_fmt)
{
    switch (img_fmt) {
    case CAMERA_YUV_420_NV21:
        return MM_JPEG_COLOR_FORMAT_YCRCBLP_H2V2;
    case CAMERA_YUV_420_NV21_ADRENO:
        return MM_JPEG_COLOR_FORMAT_YCRCBLP_H2V2;
    case CAMERA_YUV_420_NV12:
        return MM_JPEG_COLOR_FORMAT_YCBCRLP_H2V2;
    case CAMERA_YUV_420_YV12:
        return MM_JPEG_COLOR_FORMAT_YCBCRLP_H2V2;
    case CAMERA_YUV_422_NV61:
        return MM_JPEG_COLOR_FORMAT_YCRCBLP_H2V1;
    case CAMERA_YUV_422_NV16:
        return MM_JPEG_COLOR_FORMAT_YCBCRLP_H2V1;
    default:
        return MM_JPEG_COLOR_FORMAT_YCRCBLP_H2V2;
    }
}

void QCameraHardwareInterface::doHdrProcessing()
{
    cam_sock_packet_t packet;
    int i;
    memset(&packet, 0, sizeof(cam_sock_packet_t));
    packet.msg_type = CAM_SOCK_MSG_TYPE_HDR_START;

    packet.payload.hdr_pkg.cookie = (long unsigned int) this;
    packet.payload.hdr_pkg.num_hdr_frames = mHdrInfo.num_frame;
    ALOGI("%s num frames = %d ", __func__, mHdrInfo.num_frame);
    for (i = 0; i < mHdrInfo.num_frame; i++) {
        packet.payload.hdr_pkg.hdr_main_idx[i] =
            mHdrInfo.recvd_frame[i]->bufs[0]->buf_idx;
        packet.payload.hdr_pkg.hdr_thm_idx[i] =
            mHdrInfo.recvd_frame[i]->bufs[1]->buf_idx;
        packet.payload.hdr_pkg.exp[i] = mHdrInfo.exp[i];
        ALOGI("%s Adding buffer M %d T %d Exp %d into hdr pkg ", __func__,
              packet.payload.hdr_pkg.hdr_main_idx[i],
              packet.payload.hdr_pkg.hdr_thm_idx[i],
              packet.payload.hdr_pkg.exp[i]);
    }
    mCameraHandle->ops->send_command(mCameraHandle->camera_handle,
                                  MM_CAMERA_CMD_TYPE_NATIVE,
                                  NATIVE_CMD_ID_IOCTL_CTRL,
                                  sizeof(cam_sock_packet_t), &packet);
}

void QCameraHardwareInterface::initHdrInfoForSnapshot(bool Hdr_on, int number_frames, int *exp )
{
    ALOGE("%s E hdr_on = %d", __func__, Hdr_on);
    mHdrInfo.hdr_on = Hdr_on;
    mHdrInfo.num_frame = number_frames;
    mHdrInfo.num_raw_received = 0;
    if(number_frames) {
        memcpy(mHdrInfo.exp, exp, sizeof(int)*number_frames);
    }
    memset(mHdrInfo.recvd_frame, 0,
           sizeof(mm_camera_super_buf_t *)*MAX_HDR_EXP_FRAME_NUM);
    ALOGE("%s X", __func__);
}

void QCameraHardwareInterface::notifyHdrEvent(cam_ctrl_status_t status, void * cookie)
{
    ALOGE("%s E", __func__);
    mm_camera_super_buf_t *frame;
    int i;
    ALOGI("%s: HDR Done status (%d) received",__func__,status);
    for (i = 0; i < 2; i++) {
        frame = mHdrInfo.recvd_frame[i];
        mSuperBufQueue.enqueue(frame);
        /* notify dataNotify thread that new super buf is avail
        * check if it's done with current JPEG notification and
        * a new encoding job could be conducted */
        mNotifyTh->sendCmd(CAMERA_CMD_TYPE_DO_NEXT_JOB, FALSE, FALSE);
        mHdrInfo.recvd_frame[i] = NULL;
    }
    /* qbuf the third frame */
    frame = mHdrInfo.recvd_frame[2];
    for(i = 0; i < frame->num_bufs; i++) {
        mCameraHandle->ops->qbuf(frame->camera_handle,
                                 frame->ch_id,
                                 frame->bufs[i]);
        cache_ops((QCameraHalMemInfo_t *)(frame->bufs[i]->mem_info),
                  frame->bufs[i]->buffer,
                  ION_IOC_INV_CACHES);
    }
    free(frame);
    mHdrInfo.recvd_frame[2] = NULL;

    ALOGE("%s X", __func__);
}

int QCameraHardwareInterface::cache_ops(QCameraHalMemInfo_t *mem_info,
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

    ALOGD("addr = %p, fd = %d, handle = %p length = %d, ION Fd = %d",
         cache_inv_data.vaddr, cache_inv_data.fd,
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

uint8_t QCameraHardwareInterface::canTakeFullSizeLiveshot() {
    if (mFullLiveshotEnabled && !isLowPowerCamcorder()) {
        /* Full size liveshot enabled. */

        /* If Picture size is same as video size, switch to Video size
         * live snapshot */
        if ((mDimension.picture_width == mDimension.video_width) &&
            (mDimension.picture_height == mDimension.video_height)) {
            return FALSE;
        }

        if (mDisEnabled) {
            /* If DIS is enabled and Picture size is
             * less than (video size + 10% DIS Margin)
             * then fall back to Video size liveshot. */
            if ((mDimension.picture_width <
                 (int)(mDimension.video_width * 1.1)) ||
                (mDimension.picture_height <
                 (int)(mDimension.video_height * 1.1))) {
                return FALSE;
            } else {
                /* Go with Full size live snapshot. */
                return TRUE;
            }
        } else {
            /* DIS Disabled. Go with Full size live snapshot */
            return TRUE;
        }
    } else {
        /* Full size liveshot disabled. Fallback to Video size liveshot. */
        return FALSE;
    }
}

QCameraQueue::QCameraQueue()
{
    pthread_mutex_init(&mlock, NULL);
    cam_list_init(&mhead.list);
    msize = 0;
    mdata_rel_fn = NULL;
    muser_data = NULL;
}

QCameraQueue::QCameraQueue(release_data_fn data_rel_fn, void *user_data)
{
    pthread_mutex_init(&mlock, NULL);
    cam_list_init(&mhead.list);
    msize = 0;
    mdata_rel_fn = data_rel_fn;
    muser_data = user_data;
}

QCameraQueue::~QCameraQueue()
{
    flush();
    pthread_mutex_destroy(&mlock);
}

bool QCameraQueue::is_empty()
{
    bool isEmpty = true;
    pthread_mutex_lock(&mlock);
    if (msize > 0) {
        isEmpty = false;
    }
    pthread_mutex_unlock(&mlock);
    return isEmpty;
}

bool QCameraQueue::enqueue(void *data)
{
    camera_q_node *node =
        (camera_q_node *)malloc(sizeof(camera_q_node));
    if (NULL == node) {
        ALOGE("%s: No memory for camera_q_node", __func__);
        return false;
    }

    memset(node, 0, sizeof(camera_q_node));
    node->data = data;

    pthread_mutex_lock(&mlock);
    cam_list_add_tail_node(&node->list, &mhead.list);
    msize++;
    pthread_mutex_unlock(&mlock);
    return true;
}

/* priority queue, will insert into the head of the queue */
bool QCameraQueue::pri_enqueue(void *data)
{
    camera_q_node *node =
        (camera_q_node *)malloc(sizeof(camera_q_node));
    if (NULL == node) {
        ALOGE("%s: No memory for camera_q_node", __func__);
        return false;
    }

    memset(node, 0, sizeof(camera_q_node));
    node->data = data;

    pthread_mutex_lock(&mlock);
    struct cam_list *p_next = mhead.list.next;

    mhead.list.next = &node->list;
    p_next->prev = &node->list;
    node->list.next = p_next;
    node->list.prev = &mhead.list;

    msize++;
    pthread_mutex_unlock(&mlock);
    return true;
}

void* QCameraQueue::dequeue()
{
    camera_q_node* node = NULL;
    void* data = NULL;
    struct cam_list *head = NULL;
    struct cam_list *pos = NULL;

    pthread_mutex_lock(&mlock);
    head = &mhead.list;
    pos = head->next;
    if (pos != head) {
        node = member_of(pos, camera_q_node, list);
        cam_list_del_node(&node->list);
        msize--;
    }
    pthread_mutex_unlock(&mlock);

    if (NULL != node) {
        data = node->data;
        free(node);
    }

    return data;
}

void QCameraQueue::flush(){
    camera_q_node* node = NULL;
    void* data = NULL;
    struct cam_list *head = NULL;
    struct cam_list *pos = NULL;

    pthread_mutex_lock(&mlock);
    head = &mhead.list;
    pos = head->next;

    while(pos != head) {
        node = member_of(pos, camera_q_node, list);
        pos = pos->next;
        cam_list_del_node(&node->list);
        msize--;

        if (NULL != node->data) {
            if (mdata_rel_fn) {
                mdata_rel_fn(node->data, muser_data);
            }
            free(node->data);
        }
        free(node);

    }
    msize = 0;
    pthread_mutex_unlock(&mlock);
}

QCameraCmdThread::QCameraCmdThread() :
    cmd_queue()
{
    sem_init(&sync_sem, 0, 0);
    sem_init(&cmd_sem, 0, 0);
}

QCameraCmdThread::~QCameraCmdThread()
{
    sem_destroy(&sync_sem);
    sem_destroy(&cmd_sem);
}

int32_t QCameraCmdThread::launch(void *(*start_routine)(void *),
                                 void* user_data)
{
    /* launch the thread */
    pthread_create(&cmd_pid,
                   NULL,
                   start_routine,
                   user_data);
    return 0;
}

int32_t QCameraCmdThread::sendCmd(camera_cmd_type_t cmd, uint8_t sync_cmd, uint8_t priority)
{
    camera_cmd_t *node = (camera_cmd_t *)malloc(sizeof(camera_cmd_t));
    if (NULL == node) {
        ALOGE("%s: No memory for camera_cmd_t", __func__);
        return -1;
    }
    memset(node, 0, sizeof(camera_cmd_t));
    node->cmd = cmd;

    ALOGD("%s: enqueue cmd %d", __func__, cmd);
    if (TRUE == priority) {
        cmd_queue.pri_enqueue((void *)node);
    } else {
        cmd_queue.enqueue((void *)node);
    }
    sem_post(&cmd_sem);

    /* if is a sync call, need to wait until it returns */
    if (sync_cmd) {
        sem_wait(&sync_sem);
    }
    return 0;
}

camera_cmd_type_t QCameraCmdThread::getCmd()
{
    camera_cmd_type_t cmd = CAMERA_CMD_TYPE_NONE;
    camera_cmd_t *node = (camera_cmd_t *)cmd_queue.dequeue();
    if (NULL == node) {
        ALOGD("%s: No notify avail", __func__);
        return CAMERA_CMD_TYPE_NONE;
    } else {
        cmd = node->cmd;
        free(node);
    }
    return cmd;
}

int32_t QCameraCmdThread::exit()
{
    int32_t rc = 0;

    rc = sendCmd(CAMERA_CMD_TYPE_EXIT, FALSE, TRUE);
    if (0 != rc) {
        ALOGE("%s: Error during exit, rc = %d", __func__, rc);
        return rc;
    }

    /* wait until cmd thread exits */
    if (pthread_join(cmd_pid, NULL) != 0) {
        ALOGD("%s: pthread dead already\n", __func__);
    }
    cmd_pid = 0;
    return rc;
}

}; // namespace android

