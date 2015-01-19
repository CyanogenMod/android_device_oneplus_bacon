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

#define ALOG_NDEBUG 0
#define ALOG_NIDEBUG 0
#define LOG_TAG __FILE__
#include <utils/Log.h>
#include <utils/threads.h>

#include "QCameraHWI.h"
#include "QCameraStream.h"

/* QCameraStream class implementation goes here*/
/* following code implement the control logic of this class*/

namespace android {

StreamQueue::StreamQueue(){
    mInitialized = false;
}

StreamQueue::~StreamQueue(){
    flush();
}

void StreamQueue::init(){
    Mutex::Autolock l(&mQueueLock);
    mInitialized = true;
    mQueueWait.signal();
}

void StreamQueue::deinit(){
    Mutex::Autolock l(&mQueueLock);
    mInitialized = false;
    mQueueWait.signal();
}

bool StreamQueue::isInitialized(){
   Mutex::Autolock l(&mQueueLock);
   return mInitialized;
}

bool StreamQueue::enqueue(
                 void * element){
    Mutex::Autolock l(&mQueueLock);
    if(mInitialized == false)
        return false;

    mContainer.add(element);
    mQueueWait.signal();
    return true;
}

bool StreamQueue::isEmpty(){
    return (mInitialized && mContainer.isEmpty());
}
void* StreamQueue::dequeue(){

    void *frame;
    mQueueLock.lock();
    while(mInitialized && mContainer.isEmpty()){
        mQueueWait.wait(mQueueLock);
    }

    if(!mInitialized){
        mQueueLock.unlock();
        return NULL;
    }

    frame = mContainer.itemAt(0);
    mContainer.removeAt(0);
    mQueueLock.unlock();
    return frame;
}

void StreamQueue::flush(){
    Mutex::Autolock l(&mQueueLock);
    mContainer.clear();
}


// ---------------------------------------------------------------------------
// QCameraStream
// ---------------------------------------------------------------------------

void superbuf_cb_routine(mm_camera_super_buf_t *bufs,
                       void *userdata)
{
    QCameraHardwareInterface *p_obj=(QCameraHardwareInterface*) userdata;
    ALOGE("%s: E",__func__);

    //Implement call to JPEG routine in Snapshot here
     if(bufs->bufs[0]->stream_id == p_obj->mStreamSnapMain->mMmStreamId){
         ALOGE("%s : jpeg callback for MM_CAMERA_SNAPSHOT_MAIN", __func__);
         p_obj->mStreamSnapMain->receiveRawPicture(bufs);
     }


    for(int i=0;i<bufs->num_bufs;i++) {

        p_obj->mCameraHandle->ops->qbuf(p_obj->mCameraHandle->camera_handle,
                                       p_obj->mChannelId,
                                       bufs->bufs[i]);

    }


}

void stream_cb_routine(mm_camera_super_buf_t *bufs,
                       void *userdata)
{
    ALOGE("%s E ", __func__);
    QCameraStream *p_obj=(QCameraStream*) userdata;
    ALOGE("DEBUG4:ExtMode:%d,streamid:%d",p_obj->mExtImgMode,bufs->bufs[0]->stream_id);
    switch(p_obj->mExtImgMode) {
    case MM_CAMERA_PREVIEW:
        ALOGE("%s : callback for MM_CAMERA_PREVIEW", __func__);
        ((QCameraStream_preview *)p_obj)->dataCallback(bufs);
        break;
    case MM_CAMERA_VIDEO:
        ALOGE("%s : callback for MM_CAMERA_VIDEO", __func__);
        ((QCameraStream_preview *)p_obj)->dataCallback(bufs);
        break;
    case MM_CAMERA_SNAPSHOT_MAIN:
#if 0
                if(p_obj->mHalCamCtrl->getHDRMode()) {
                    ALOGE("%s: Skipping Q Buf for HDR mode",__func__);
                    break;
                }
#endif

                ALOGE("%s : callback for MM_CAMERA_SNAPSHOT_MAIN", __func__);
                p_obj->p_mm_ops->ops->qbuf(p_obj->mCameraHandle,
                                           p_obj->mChannelId,
                                           bufs->bufs[0]);
                break;
         case MM_CAMERA_SNAPSHOT_THUMBNAIL:
                break;
         default:
                break;
    
    }
    ALOGE("%s X ", __func__);
}

QCameraStream *QCameraStream::mStreamTable[STREAM_TABLE_SIZE];

void QCameraStream::dataCallback(mm_camera_super_buf_t *bufs)
{
    if(mPendingCount!=0) {
        ALOGD("Got frame request");
        pthread_mutex_lock(&mFrameDeliveredMutex);
        mPendingCount--;
        ALOGD("Completed frame request");
        pthread_cond_signal(&mFrameDeliveredCond);
        pthread_mutex_unlock(&mFrameDeliveredMutex);
        processPreviewFrame(bufs);
    } else {
        p_mm_ops->ops->qbuf(mCameraHandle,
                mChannelId, bufs->bufs[0]);
    }
}

void QCameraStream::onNewRequest()
{
    ALOGI("%s:E",__func__);
    pthread_mutex_lock(&mFrameDeliveredMutex);
    ALOGI("Sending Frame request");
    mPendingCount++;
    pthread_cond_wait(&mFrameDeliveredCond,&mFrameDeliveredMutex);
    ALOGV("Got frame");
    pthread_mutex_unlock(&mFrameDeliveredMutex);
    ALOGV("%s:X",__func__);
}

int32_t QCameraStream::streamOn()
{
   status_t rc=NO_ERROR;
   mm_camera_stream_config_t stream_config;
   ALOGE("%s:streamid:%d",__func__,mMmStreamId);
   Mutex::Autolock lock(mLock);
   if(mActive){
       ALOGE("%s: Stream:%d is already active",
            __func__,mMmStreamId);
       return rc;
   }

   if (mInit == true) {
       /* this is the restart case */
       memset(&stream_config, 0, sizeof(mm_camera_stream_config_t));
       stream_config.fmt.fmt=(cam_format_t)mFormat;
       stream_config.fmt.meta_header=MM_CAMEAR_META_DATA_TYPE_DEF;
       stream_config.fmt.width=mWidth;
       stream_config.fmt.height=mHeight;
       stream_config.fmt.rotation = 0;
       ALOGE("<DEBUG>::%s: Width :%d Height:%d Format:%d",__func__,mWidth,mHeight,mFormat);
       stream_config.num_of_bufs=mNumBuffers;
       stream_config.need_stream_on=true;
       rc=p_mm_ops->ops->config_stream(mCameraHandle,
                                 mChannelId,
                                 mMmStreamId,
                                 &stream_config);
       ALOGE("%s: config_stream, rc = %d", __func__, rc);
   }

   rc = p_mm_ops->ops->start_streams(mCameraHandle,
                              mChannelId,
                              1,
                              &mMmStreamId);
   if(rc==NO_ERROR)
       mActive = true;
   return rc;
}

int32_t QCameraStream::streamOff(bool isAsyncCmd)
{
    status_t rc=NO_ERROR;
    Mutex::Autolock lock(mLock);
    if(!mActive) {
        ALOGE("%s: Stream:%d is not active",
              __func__,mMmStreamId);
        return rc;
    }

    rc = p_mm_ops->ops->stop_streams(mCameraHandle,
                              mChannelId,
                              1,
                              &mMmStreamId);

    mActive=false;
    return rc;

}

/* initialize a streaming channel*/
status_t QCameraStream::initStream(mm_camera_img_mode imgmode,
    cam_format_t format)
{
    int rc = MM_CAMERA_OK;
    status_t ret = NO_ERROR;
    mm_camera_op_mode_type_t op_mode=MM_CAMERA_OP_MODE_VIDEO;
    cam_ctrl_dimension_t dim;
    int i;
    mm_camera_stream_config_t stream_config;

    ALOGE("QCameraStream::initStream : E");

    mFormat = format;
    /*TODO: Convert between TEMPLATE to img_mode */
    mExtImgMode = imgmode;
    /***********Allocate Stream**************/

    rc=p_mm_ops->ops->add_stream(mCameraHandle,
                        mChannelId,
                        stream_cb_routine,
                        (void *)this,
                        mExtImgMode,
                        0/*sensor_idx*/);

    if (rc < 0)
       goto error1;

    mMmStreamId=rc;
    ALOGE("%s: mMmStreamId = %d\n", __func__, mMmStreamId);

    memset(&stream_config, 0, sizeof(mm_camera_stream_config_t));
    stream_config.fmt.fmt=format;
    stream_config.fmt.meta_header=MM_CAMEAR_META_DATA_TYPE_DEF;
    stream_config.fmt.width=mWidth;
    stream_config.fmt.height=mHeight;
    ALOGE("<DEBUG>::%s: Width :%d Height:%d Format:%d",__func__,mWidth,mHeight,format);
    stream_config.num_of_bufs=mNumBuffers;
    stream_config.need_stream_on=true;
    rc=p_mm_ops->ops->config_stream(mCameraHandle,
                              mChannelId,
                              mMmStreamId,
                              &stream_config);
    if(MM_CAMERA_OK != rc)
        goto error2;

    goto end;


error2:
    ALOGE("%s: Error configuring stream",__func__);
    p_mm_ops->ops->del_stream(mCameraHandle,mChannelId,
                              mMmStreamId);

error1: 
    return BAD_VALUE;      
end:
    ALOGE("Setting mInit to true");
    mInit=true;
    return NO_ERROR;

}

status_t QCameraStream::deinitStream()
{

    int rc = MM_CAMERA_OK;

    ALOGI("%s: E, Stream = %d\n", __func__, mMmStreamId);
    
    rc= p_mm_ops->ops->del_stream(mCameraHandle,mChannelId,
                              mMmStreamId);

    ALOGI("%s: X, Stream = %d\n", __func__, mMmStreamId);
    mInit=false;
    return NO_ERROR;
}

status_t QCameraStream::setMode(int enable) {
  ALOGE("%s :myMode %x ", __func__, myMode);
  if (enable) {
      myMode = (camera_mode_t)(myMode | CAMERA_ZSL_MODE);
  } else {
      myMode = (camera_mode_t)(myMode & ~CAMERA_ZSL_MODE);
  }
  return NO_ERROR;
}

status_t QCameraStream::setFormat()
{
    ALOGE("%s: E",__func__);

    char mDeviceName[PROPERTY_VALUE_MAX];
    property_get("ro.product.device",mDeviceName," ");

    ALOGE("%s: X",__func__);
    return NO_ERROR;
}

QCameraStream::QCameraStream (){
    mInit = false;
    mActive = false;
    /* memset*/
    memset(&mCrop, 0, sizeof(mm_camera_rect_t));
}

QCameraStream::QCameraStream(uint32_t CameraHandle,
                       uint32_t ChannelId,
                        uint32_t Width,
                        uint32_t Height,
                        mm_camera_vtbl_t *mm_ops,
                        camera_mode_t mode)
              :myMode(mode)
{
    mInit = false;
    mActive = false;

    mCameraHandle=CameraHandle;
    mChannelId=ChannelId;
    mWidth=Width;
    mHeight=Height;
    p_mm_ops=mm_ops;

    /* memset*/
    memset(&mCrop, 0, sizeof(mm_camera_rect_t));

    mPendingCount=0;
    pthread_mutex_init(&mFrameDeliveredMutex, NULL);
    pthread_cond_init(&mFrameDeliveredCond, NULL);
}

QCameraStream::~QCameraStream () {;}

int QCameraStream::allocateStreamId() {
    int i = 0;
    for (i = 0; i < STREAM_TABLE_SIZE; i++)
        if (mStreamTable[i] == NULL) {
            mStreamTable[i] = this; 
            break;
        }
    if (i == STREAM_TABLE_SIZE)
        return INVALID_OPERATION; 
    ALOGE("%s: mStreamTable[%d] = %p\n", __func__, i, mStreamTable[i]);
    return i;
}

int QCameraStream::deallocateStreamId(int id) {
    if (id < 0 || id >= STREAM_TABLE_SIZE)
        return BAD_VALUE;
    mStreamTable[id] = NULL;
    return OK;
}

QCameraStream *QCameraStream::getStreamAtId(int id) {
    if (id < 0 || id >= STREAM_TABLE_SIZE)
        return NULL;
    else
        return mStreamTable[id];
}

QCameraStream *QCameraStream::getStreamAtMmId(uint32_t mm_id) {
    /*TODO: More efficient to do direct lookup. But it requires
     *mm-camera-interface to expose a macro for handle-index mapping*/
    for (int i = 0; i < STREAM_TABLE_SIZE; i++) {
        ALOGE("%s: %d: ", __func__, i);
        if (mStreamTable[i])
            ALOGE("%s: mMmStreamId = %d", __func__, mStreamTable[i]->mMmStreamId);
        if (mStreamTable[i] && (mStreamTable[i]->mMmStreamId == mm_id))
            return mStreamTable[i];
    }
    ALOGE("%s: Cannot find stream with interface id %d", __func__, mm_id);
    return NULL;
}

void QCameraStream::streamOffAll()
{
    for (int i = 0; i < STREAM_TABLE_SIZE; i++) {
        if (mStreamTable[i]) {
            if (mStreamTable[i]->mActive) {
                ALOGI("%s: stream off stream[%d]", __func__, i);
                mStreamTable[i]->streamOff(0);
                ALOGI("%s: stream off stream[%d] done", __func__, i);
            }
            if (mStreamTable[i]->mInit) {
                ALOGI("%s: deinit stream[%d]", __func__, i);
                mStreamTable[i]->deinitStream();
                ALOGI("%s: deinit stream[%d] done", __func__, i);
            }
        }
    }
}

status_t QCameraStream::init() {
    return NO_ERROR;
}

status_t QCameraStream::start() {
    return NO_ERROR;
}

void QCameraStream::stop() {
    return;
}

void QCameraStream::release() {
    return;
}

void QCameraStream::setHALCameraControl(QCameraHardwareInterface* ctrl) {

    /* provide a frame data user,
    for the  queue monitor thread to call the busy queue is not empty*/
    mHalCamCtrl = ctrl;
}

}; // namespace android
