/*
** Copyright (c) 2011-2012 The Linux Foundation. All rights reserved.
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

//#define ALOG_NDEBUG 0
#define ALOG_NIDEBUG 0
#define LOG_TAG "QCameraHWI_Record"
#include <utils/Log.h>
#include <utils/threads.h>
#include <cutils/properties.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "QCameraStream.h"


#define LIKELY(exp)   __builtin_expect(!!(exp), 1)
#define UNLIKELY(exp) __builtin_expect(!!(exp), 0)

/* QCameraStream_record class implementation goes here*/
/* following code implement the video streaming capture & encoding logic of this class*/
// ---------------------------------------------------------------------------
// QCameraStream_record createInstance()
// ---------------------------------------------------------------------------
namespace android {


// ---------------------------------------------------------------------------
// QCameraStream_record Constructor
// ---------------------------------------------------------------------------
QCameraStream_record::QCameraStream_record(uint32_t CameraHandle,
                                           uint32_t ChannelId,
                                           uint32_t Width,
                                           uint32_t Height,
                                           uint32_t Format,
                                           uint8_t NumBuffers,
                                           mm_camera_vtbl_t *mm_ops,
                                           mm_camera_img_mode imgmode,
                                           camera_mode_t mode,
                                           QCameraHardwareInterface* camCtrl)
//  : QCameraStream(cameraId,mode),
  :QCameraStream(CameraHandle,
                 ChannelId,
                 Width,
                 Height,
                 Format,
                 NumBuffers,
                 mm_ops,
                 imgmode,
                 mode,
                 camCtrl),
                 mDebugFps(false)
{
  char value[PROPERTY_VALUE_MAX];
  ALOGV("%s: BEGIN", __func__);

  property_get("persist.debug.sf.showfps", value, "0");
  mDebugFps = atoi(value);

  ALOGV("%s: END", __func__);
}

// ---------------------------------------------------------------------------
// QCameraStream_record Destructor
// ---------------------------------------------------------------------------
QCameraStream_record::~QCameraStream_record() {
    ALOGV("%s: BEGIN", __func__);
    release();
    ALOGV("%s: END", __func__);
}

int QCameraStream_record::putBuf(uint8_t num_bufs, mm_camera_buf_def_t *bufs)
{
    for(int cnt = 0; cnt < mHalCamCtrl->mRecordingMemory.buffer_count; cnt++) {
        if (mHalCamCtrl->mStoreMetaDataInFrame) {
            struct encoder_media_buffer_type * packet =
                (struct encoder_media_buffer_type  *)mHalCamCtrl->mRecordingMemory.metadata_memory[cnt]->data;
            native_handle_delete(const_cast<native_handle_t *>(packet->meta_handle));
            mHalCamCtrl->mRecordingMemory.metadata_memory[cnt]->release(
               mHalCamCtrl->mRecordingMemory.metadata_memory[cnt]);
        }
        mHalCamCtrl->mRecordingMemory.camera_memory[cnt]->release(
           mHalCamCtrl->mRecordingMemory.camera_memory[cnt]);

#ifdef USE_ION
        mHalCamCtrl->deallocate_ion_memory(&mHalCamCtrl->mRecordingMemory.mem_info[cnt]);
#endif
    }
    memset(&mHalCamCtrl->mRecordingMemory, 0, sizeof(mHalCamCtrl->mRecordingMemory));
    return NO_ERROR;
}

// ---------------------------------------------------------------------------
// QCameraStream_record
// ---------------------------------------------------------------------------
void QCameraStream_record::release()
{
    ALOGV("%s: BEGIN", __func__);
    streamOff(0);
    deinitStream();
    ALOGV("%s: END", __func__);
}

status_t QCameraStream_record::processRecordFrame(mm_camera_super_buf_t *frame)
{
    ALOGV("%s : BEGIN",__func__);
    int video_buf_idx = frame->bufs[0]->buf_idx;

    if(!mActive) {
      ALOGE("Recording Stopped. Returning callback");
      return NO_ERROR;
    }

    if (UNLIKELY(mDebugFps)) {
        debugShowVideoFPS();
    }
    ALOGE("DEBUG4:%d",frame->bufs[0]->stream_id);
    ALOGE("<DEBUG4>: Timestamp: %ld %ld",frame->bufs[0]->ts.tv_sec,frame->bufs[0]->ts.tv_nsec);
    mHalCamCtrl->dumpFrameToFile(frame->bufs[0], HAL_DUMP_FRM_VIDEO);
    camera_data_timestamp_callback rcb = mHalCamCtrl->mDataCbTimestamp;
    void *rdata = mHalCamCtrl->mCallbackCookie;
	  nsecs_t timeStamp = nsecs_t(frame->bufs[0]->ts.tv_sec)*1000000000LL + \
                      frame->bufs[0]->ts.tv_nsec;

    ALOGE("Send Video frame to services/encoder TimeStamp : %lld",timeStamp);
    mRecordedFrames[video_buf_idx] = *frame;

    mHalCamCtrl->cache_ops(&mHalCamCtrl->mRecordingMemory.mem_info[video_buf_idx],
                           (void *)mHalCamCtrl->mRecordingMemory.camera_memory[video_buf_idx]->data,
                           ION_IOC_CLEAN_CACHES);

    if (mHalCamCtrl->mStoreMetaDataInFrame) {
        if((rcb != NULL) && (mHalCamCtrl->mMsgEnabled & CAMERA_MSG_VIDEO_FRAME)) {
            ALOGE("Calling video callback:%d", video_buf_idx);
            rcb(timeStamp, CAMERA_MSG_VIDEO_FRAME,
                mHalCamCtrl->mRecordingMemory.metadata_memory[video_buf_idx],
                0, mHalCamCtrl->mCallbackCookie);
        }
    } else {
        if((rcb != NULL) && (mHalCamCtrl->mMsgEnabled & CAMERA_MSG_VIDEO_FRAME)) {
            ALOGE("Calling video callback2");
            rcb(timeStamp, CAMERA_MSG_VIDEO_FRAME,
                mHalCamCtrl->mRecordingMemory.camera_memory[video_buf_idx],
                0, mHalCamCtrl->mCallbackCookie);
        }
    }

    ALOGV("%s : END",__func__);
    return NO_ERROR;
}

//Record Related Functions
status_t QCameraStream_record::initStream(uint8_t no_cb_needed, uint8_t stream_on)
{
    status_t ret = NO_ERROR;

    ALOGI(" %s : E ", __FUNCTION__);
    mNumBuffers = VIDEO_BUFFER_COUNT;
    if(mHalCamCtrl->isLowPowerCamcorder()) {
        ALOGE("%s: lower power camcorder selected", __func__);
        mNumBuffers = VIDEO_BUFFER_COUNT_LOW_POWER_CAMCORDER;
    }
    ret = QCameraStream::initStream(no_cb_needed, stream_on);
end:
    ALOGI(" %s : X ", __FUNCTION__);
    return ret;
}

status_t QCameraStream_record::getBuf(mm_camera_frame_len_offset *frame_offset_info,
                                      uint8_t num_bufs,
                                      uint8_t *initial_reg_flag,
                                      mm_camera_buf_def_t  *bufs)
{
    ALOGE("%s : BEGIN",__func__);

    if (num_bufs > mNumBuffers) {
        mNumBuffers = num_bufs;
    }
    if ((mNumBuffers == 0) || (mNumBuffers > MM_CAMERA_MAX_NUM_FRAMES)) {
        ALOGE("%s: Invalid number of buffers (=%d) requested!",
             __func__, mNumBuffers);
        return BAD_VALUE;
    }

    memset(mRecordBuf, 0, sizeof(mRecordBuf));
    memcpy(&mFrameOffsetInfo, frame_offset_info, sizeof(mFrameOffsetInfo));
    if (mHalCamCtrl->initHeapMem(&mHalCamCtrl->mRecordingMemory,
                                 mNumBuffers,
                                 mFrameOffsetInfo.frame_len,
                                 MSM_PMEM_VIDEO,
                                 &mFrameOffsetInfo,
                                 mRecordBuf) < 0) {
        ALOGE("%s: Error getting heap mem for recording", __func__);
        return NO_MEMORY;
    }

    if (mHalCamCtrl->mStoreMetaDataInFrame) {
        for (int cnt = 0; cnt < mNumBuffers; cnt++) {
            mHalCamCtrl->mRecordingMemory.metadata_memory[cnt] =
                mHalCamCtrl->mGetMemory(-1,
                                        sizeof(struct encoder_media_buffer_type),
                                        1,
                                        (void *)this);
            struct encoder_media_buffer_type * packet =
                (struct encoder_media_buffer_type *)mHalCamCtrl->mRecordingMemory.metadata_memory[cnt]->data;
            packet->meta_handle = native_handle_create(1, 2); //1 fd, 1 offset and 1 size
            packet->buffer_type = kMetadataBufferTypeCameraSource;
            native_handle_t * nh = const_cast<native_handle_t *>(packet->meta_handle);
            nh->data[0] = mHalCamCtrl->mRecordingMemory.mem_info[cnt].fd;
            nh->data[1] = 0;
            nh->data[2] = mHalCamCtrl->mRecordingMemory.mem_info[cnt].size;
        }
    }

    for(int i = 0; i < num_bufs; i++) {
        bufs[i] = mRecordBuf[i];
        initial_reg_flag[i] = true;
    }

    ALOGE("%s : END",__func__);
    return NO_ERROR;
}

void QCameraStream_record::releaseRecordingFrame(const void *opaque)
{
    ALOGV("%s : BEGIN, opaque = 0x%p",__func__, opaque);
    if(!mActive)
    {
        ALOGE("%s : Recording already stopped!!! Leak???",__func__);
        return;
    }
    for(int cnt = 0; cnt < mHalCamCtrl->mRecordingMemory.buffer_count; cnt++) {
      if (mHalCamCtrl->mStoreMetaDataInFrame) {
        if(mHalCamCtrl->mRecordingMemory.metadata_memory[cnt] &&
                mHalCamCtrl->mRecordingMemory.metadata_memory[cnt]->data == opaque) {
            /* found the match */
            ALOGE("Releasing video frames:%d",cnt);
            if(MM_CAMERA_OK != p_mm_ops->ops->qbuf(mCameraHandle, mChannelId, mRecordedFrames[cnt].bufs[0]))
                ALOGE("%s : Buf Done Failed",__func__);
            mHalCamCtrl->cache_ops(&mHalCamCtrl->mRecordingMemory.mem_info[cnt],
                                   (void *)mRecordedFrames[cnt].bufs[0]->buffer,
                                   ION_IOC_INV_CACHES);
            ALOGV("%s : END",__func__);
            return;
        }
      } else {
        if(mHalCamCtrl->mRecordingMemory.camera_memory[cnt] &&
                mHalCamCtrl->mRecordingMemory.camera_memory[cnt]->data == opaque) {
            /* found the match */
            ALOGE("Releasing video frames2");
            if(MM_CAMERA_OK != p_mm_ops->ops->qbuf(mCameraHandle, mChannelId, mRecordedFrames[cnt].bufs[0]))
                ALOGE("%s : Buf Done Failed",__func__);
            mHalCamCtrl->cache_ops(&mHalCamCtrl->mRecordingMemory.mem_info[cnt],
                                   (void *)mRecordedFrames[cnt].bufs[0]->buffer,
                                   ION_IOC_INV_CACHES);
            ALOGV("%s : END",__func__);
            return;
        }
      }
    }
	ALOGE("%s: cannot find the matched frame with opaue = 0x%p", __func__, opaque);
}

void QCameraStream_record::debugShowVideoFPS() const
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
    ALOGI("Video Frames Per Second: %.4f", mFps);
    mLastFpsTime = now;
    mLastFrameCount = mFrameCount;
  }
}

}//namespace android

