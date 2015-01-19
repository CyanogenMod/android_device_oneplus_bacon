/*
** Copyright (c) 2011-2012 The Linux Foundation. All rights reserved.
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

#define LOG_NIDEBUG 0
//#define LOG_NDEBUG 0

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
extern void superbuf_cb_routine(mm_camera_super_buf_t *bufs,
                       void *userdata);


/*Command Thread startup function*/

void *command_thread(void *obj)
{
    QCameraHardwareInterface *pme = (QCameraHardwareInterface *)obj;
    ALOGD("%s: E", __func__);
    if (pme != NULL) {
        pme->runCommandThread(obj);
    }
    else ALOGW("not starting command thread: the object went away!");
    ALOGD("%s: X", __func__);
    return NULL;
}

int QCameraHardwareInterface::tryRestartStreams(
    camera_metadata_entry_t& streams)
{
    int rc = 0;
    bool needRestart = false;
    bool needRecordingHint = false;

    for (uint32_t i = 0; i < streams.count; i++) {
        int streamId = streams.data.u8[i];
        QCameraStream *stream = QCameraStream::getStreamAtId(streamId);
        if (!stream->mInit) {
            needRestart = true;
            if (stream->mFormat == CAMERA_YUV_420_NV12)
                needRecordingHint = true;
        }
    }

    if (!needRestart)
        goto end;

    QCameraStream::streamOffAll();

    if (needRecordingHint) {
        uint32_t recordingHint = 1;
        rc = mCameraHandle->ops->set_parm(mCameraHandle->camera_handle,
            MM_CAMERA_PARM_RECORDING_HINT, &recordingHint);
        if (rc < 0) {
            ALOGE("set_parm MM_CAMERA_PARM_RECORDING_HINT returns %d", rc);
            return rc;
        }
    }

end:
    for (uint32_t i = 0; i < streams.count; i++) {
        int streamId = streams.data.u8[i];
        QCameraStream *stream = QCameraStream::getStreamAtId(streamId);
        if (!stream->mInit) {
            rc = stream->prepareStream();
            if (rc < 0) {
                ALOGE("prepareStream for stream %d failed %d", streamId, rc);
                return rc;
            }
        }
    }
    for (uint32_t i = 0; i < streams.count; i++) {
        int streamId = streams.data.u8[i];
        QCameraStream *stream = QCameraStream::getStreamAtId(streamId);

        if (!stream->mActive) {
            rc = stream->streamOn();
            if (rc < 0) {
                ALOGE("streamOn for stream %d failed %d", streamId, rc);
                return rc;
            }
        }
    }
    return rc;
}

void QCameraHardwareInterface::runCommandThread(void *data)
{
   
    /**
     * This function implements the main service routine for the incoming
     * frame requests, this thread routine is started everytime we get a 
     * notify_request_queue_not_empty trigger, this thread makes the 
     * assumption that once it receives a NULL on a dequest_request call 
     * there will be a fresh notify_request_queue_not_empty call that is
     * invoked thereby launching a new instance of this thread. Therefore,
     * once we get a NULL on a dequeue request we simply let this thread die
     */ 
    int res;
    camera_metadata_t *request=NULL;
    mPendingRequests=0;

    while(mRequestQueueSrc) {
        ALOGV("%s:Dequeue request using mRequestQueueSrc:%p",__func__,mRequestQueueSrc);
        mRequestQueueSrc->dequeue_request(mRequestQueueSrc,&request);
        if(request==NULL) {
            ALOGE("%s:No more requests available from src command \
                    thread dying",__func__);
            return;
        }
        mPendingRequests++;

        /* Set the metadata values */

        /* Wait for the SOF for the new metadata values to be applied */

        /* Check the streams that need to be active in the stream request */
        sort_camera_metadata(request);

        camera_metadata_entry_t streams;
        res = find_camera_metadata_entry(request,
                ANDROID_REQUEST_OUTPUT_STREAMS,
                &streams);
        if (res != NO_ERROR) {
            ALOGE("%s: error reading output stream tag", __FUNCTION__);
            return;
        }

        res = tryRestartStreams(streams);
        if (res != NO_ERROR) {
            ALOGE("error tryRestartStreams %d", res);
            return;
        }

        /* 3rd pass: Turn on all streams requested */
        for (uint32_t i = 0; i < streams.count; i++) {
            int streamId = streams.data.u8[i];
            QCameraStream *stream = QCameraStream::getStreamAtId(streamId);

            /* Increment the frame pending count in each stream class */

            /* Assuming we will have the stream obj in had at this point may be
             * may be multiple objs in which case we loop through array of streams */
            stream->onNewRequest();
        }
        ALOGV("%s:Freeing request using mRequestQueueSrc:%p",__func__,mRequestQueueSrc);
        /* Free the request buffer */
        mRequestQueueSrc->free_request(mRequestQueueSrc,request);
        mPendingRequests--;
        ALOGV("%s:Completed request",__func__);
    }
 
    QCameraStream::streamOffAll();
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
    return pme->getBuf(camera_handle, ch_id, stream_id,
                user_data, frame_offset_info,
                num_bufs,initial_reg_flag,
                bufs);

}

int32_t put_buffer_hook(uint32_t camera_handle,
                        uint32_t ch_id, uint32_t stream_id,
                        void *user_data, uint8_t num_bufs,
                        mm_camera_buf_def_t *bufs)
{
    QCameraHardwareInterface *pme=(QCameraHardwareInterface *)user_data;
    return pme->putBuf(camera_handle, ch_id, stream_id,
                user_data, num_bufs, bufs);

}

int QCameraHardwareInterface::getBuf(uint32_t camera_handle,
                        uint32_t ch_id, uint32_t mm_stream_id,
                        void *user_data,
                        mm_camera_frame_len_offset *frame_offset_info,
                        uint8_t num_bufs,
                        uint8_t *initial_reg_flag,
                        mm_camera_buf_def_t  *bufs)
{
    status_t ret=NO_ERROR;
    ALOGE("%s: E, stream_id = %d\n", __func__, mm_stream_id);
    QCameraStream_preview *stream = (QCameraStream_preview *)
                        QCameraStream::getStreamAtMmId(mm_stream_id);

    ALOGE("%s: len:%d, y_off:%d, cbcr:%d num buffers: %d planes:%d streamid:%d",
        __func__,
        frame_offset_info->frame_len,
	frame_offset_info->mp[0].len,
	frame_offset_info->mp[1].len,
        num_bufs,frame_offset_info->num_planes,
        mm_stream_id);
    /*************Preiew Stream*****************/
    ALOGE("Interface requesting Preview Buffers");
    stream->mFrameOffsetInfo=*frame_offset_info;
    if(NO_ERROR!=stream->initBuffers()){
        return BAD_VALUE;
    } 
    ALOGE("Debug : %s : initDisplayBuffers",__func__);
    for(int i=0;i<num_bufs;i++) {
        bufs[i] = stream->mDisplayBuf[i];
        initial_reg_flag[i] =
            (stream->mPreviewMemory.local_flag[i] == BUFFER_OWNED);
        ALOGE("initial_reg_flag[%d]:%d",i,initial_reg_flag[i]);
    }
    return 0;
}

int QCameraHardwareInterface::putBuf(uint32_t camera_handle,
                        uint32_t ch_id, uint32_t mm_stream_id,
                        void *user_data, uint8_t num_bufs,
                        mm_camera_buf_def_t *bufs)
{
    ALOGE("%s:E, stream_id = %d",__func__, mm_stream_id);
    QCameraStream_preview *stream = (QCameraStream_preview *)
                        QCameraStream::getStreamAtMmId(mm_stream_id);
    stream->deinitBuffers();
    return 0;
}


QCameraHardwareInterface::
QCameraHardwareInterface(int cameraId, int mode)
                  : mCameraId(cameraId)
{

    cam_ctrl_dimension_t mDimension;

    /* Open camera stack! */
    memset(&mMemHooks, 0, sizeof(mm_camear_mem_vtbl_t));
    mMemHooks.user_data=this;
    mMemHooks.get_buf=get_buffer_hook;
    mMemHooks.put_buf=put_buffer_hook;

    mCameraHandle=camera_open(mCameraId, &mMemHooks);
    ALOGV("Cam open returned %p",mCameraHandle);
    if(mCameraHandle == NULL) {
        ALOGE("startCamera: cam_ops_open failed: id = %d", mCameraId);
        return;
    }
    mCameraHandle->ops->sync(mCameraHandle->camera_handle);

    mChannelId=mCameraHandle->ops->ch_acquire(mCameraHandle->camera_handle);
    if(mChannelId<=0)
    {
        ALOGE("%s:Channel aquire failed",__func__);
        mCameraHandle->ops->camera_close(mCameraHandle->camera_handle);
        return;
    }

    /* Initialize # of frame requests the HAL is handling to zero*/
    mPendingRequests=0;
}

QCameraHardwareInterface::~QCameraHardwareInterface()
{
    mCameraHandle->ops->camera_close(mCameraHandle->camera_handle);
}
 
void QCameraHardwareInterface::release()
{
}

bool QCameraHardwareInterface::isCameraReady()
{
    return true;
}

int QCameraHardwareInterface::set_request_queue_src_ops(
    const camera2_request_queue_src_ops_t *request_src_ops)
{
    ALOGE("%s:E mRequestQueueSrc:%p",__func__,request_src_ops);
    mRequestQueueSrc = request_src_ops;
    ALOGE("%s:X",__func__);
    return 0;
}

int QCameraHardwareInterface::notify_request_queue_not_empty()
{

    pthread_attr_t attr;
    if(pthread_attr_init(&attr)!=0) { 
        ALOGE("%s:pthread_attr_init failed",__func__);
        return BAD_VALUE;
    }
    if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)!=0){
        ALOGE("%s:pthread_attr_setdetachstate failed",__func__);
        return BAD_VALUE;
    }
    if(pthread_create(&mCommandThread,&attr,
                   command_thread, (void *)this)!=0) {
        ALOGE("%s:pthread_create failed to launch command_thread",__func__);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

int QCameraHardwareInterface::set_frame_queue_dst_ops(
    const camera2_frame_queue_dst_ops_t *frame_dst_ops)
{
    mFrameQueueDst = frame_dst_ops;
    return OK;
}

int QCameraHardwareInterface::get_in_progress_count()
{
    return mPendingRequests;
}

int QCameraHardwareInterface::construct_default_request(
    int request_template, camera_metadata_t **request)
{
    status_t ret;
    ALOGD("%s:E:request_template:%d ",__func__,request_template);

    if (request == NULL)
        return BAD_VALUE;
    if (request_template < 0 || request_template >= CAMERA2_TEMPLATE_COUNT)
        return BAD_VALUE;

    ret = constructDefaultRequest(request_template, request, true);
    if (ret != OK) {
        ALOGE("%s: Unable to allocate default request: %s (%d)",
            __FUNCTION__, strerror(-ret), ret);
        return ret;
    }
    ret = constructDefaultRequest(request_template, request, false);
    if (ret != OK) {
        ALOGE("%s: Unable to fill in default request: %s (%d)",
            __FUNCTION__, strerror(-ret), ret);
        return ret;
    }

    ALOGE("Setting OP MODE to MM_CAMERA_OP_MODE_VIDEO");
    mm_camera_op_mode_type_t op_mode=MM_CAMERA_OP_MODE_VIDEO;
    ret = mCameraHandle->ops->set_parm(
                         mCameraHandle->camera_handle,
                         MM_CAMERA_PARM_OP_MODE,
                         &op_mode);
    ALOGE("OP Mode Set");

    return ret;
}

int QCameraHardwareInterface::allocate_stream(
    uint32_t width,
    uint32_t height, int format,
    const camera2_stream_ops_t *stream_ops,
    uint32_t *stream_id,
    uint32_t *format_actual,
    uint32_t *usage,
    uint32_t *max_buffers)
{
    int ret = OK;
    QCameraStream *stream = NULL;
    camera_mode_t myMode = (camera_mode_t)(CAMERA_MODE_2D|CAMERA_NONZSL_MODE);
    ALOGE("%s : BEGIN",__func__);

    ALOGE("Mymode Preview = %d",myMode);
    stream = QCameraStream_preview::createInstance(
                        mCameraHandle->camera_handle,
                        mChannelId,
                        width,
                        height,
                        format,
                        mCameraHandle,
                        myMode);
    ALOGE("%s: createInstance done", __func__);
    if (!stream) {
        ALOGE("%s: error - can't creat preview stream!", __func__);
        return BAD_VALUE;
    }

    stream->setPreviewWindow(stream_ops);
    *stream_id = stream->getStreamId();
    *max_buffers= stream->getMaxBuffers();
    ALOGE("%s: stream_id = %d\n", __func__, *stream_id);
    *usage = GRALLOC_USAGE_HW_CAMERA_WRITE | CAMERA_GRALLOC_HEAP_ID
        | CAMERA_GRALLOC_FALLBACK_HEAP_ID;
    /* Set to an arbitrary format SUPPORTED by gralloc */
    //*format_actual = HAL_PIXEL_FORMAT_YCbCr_422_SP;
    *format_actual = HAL_PIXEL_FORMAT_YCrCb_420_SP;
    /*TODO: For hardware encoder, add CAMERA_GRALLOC_CACHING_ID */
    stream->setHALCameraControl(this);

    ALOGV("%s : END",__func__);
    return ret;
}

int QCameraHardwareInterface::register_stream_buffers(
    uint32_t stream_id, int num_buffers,
    buffer_handle_t *buffers)
{
    struct private_handle_t *private_handle =
                        (struct private_handle_t *)buffers[0];
    QCameraStream_preview *stream = (QCameraStream_preview *)
                        QCameraStream::getStreamAtId(stream_id);

    if(!stream) {
        ALOGE("%s: Request for unknown stream",__func__);
        return BAD_VALUE;
    }

    if(NO_ERROR!=stream->registerStreamBuffers(num_buffers, buffers)) {
        ALOGE("%s:registerStreamBuffers failed",__func__);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

int QCameraHardwareInterface::release_stream(uint32_t stream_id)
{
    QCameraStream *stream = QCameraStream::getStreamAtId(stream_id);

    QCameraStream_preview::deleteInstance(stream);
 
    return OK;
}

int QCameraHardwareInterface::allocate_reprocess_stream(
    uint32_t width,
    uint32_t height,
    uint32_t format,
    const camera2_stream_in_ops_t *reprocess_stream_ops,
    uint32_t *stream_id,
    uint32_t *consumer_usage,
    uint32_t *max_buffers)
{
    return INVALID_OPERATION;
}

int QCameraHardwareInterface::release_reprocess_stream(uint32_t stream_id)
{
    return INVALID_OPERATION;
}

int QCameraHardwareInterface::get_metadata_vendor_tag_ops(vendor_tag_query_ops_t **ops)
{
    *ops = NULL;
    return OK;
}

int QCameraHardwareInterface::set_notify_callback(camera2_notify_callback notify_cb,
            void *user)
{
    mNotifyCb = notify_cb;
    mNotifyUserPtr = user;
    return OK;
}
}; // namespace android
