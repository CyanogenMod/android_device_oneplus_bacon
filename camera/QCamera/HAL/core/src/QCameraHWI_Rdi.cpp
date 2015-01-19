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

#define LOG_TAG "QCameraHWI_Rdi"
#include <utils/Log.h>
#include <utils/threads.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "QCameraHAL.h"
#include "QCameraHWI.h"
#include <gralloc_priv.h>
#include <genlock.h>

#define UNLIKELY(exp) __builtin_expect(!!(exp), 0)

/* QCameraHWI_Raw class implementation goes here*/
/* following code implement the RDI logic of this class*/

namespace android {
status_t QCameraStream_Rdi::initStream(uint8_t no_cb_needed, uint8_t stream_on)
{
    status_t ret = NO_ERROR;

    ALOGI(" %s : E ", __FUNCTION__);
    mNumBuffers = PREVIEW_BUFFER_COUNT;
    if(mHalCamCtrl->isZSLMode()) {
        if(mNumBuffers < mHalCamCtrl->getZSLQueueDepth() + 3) {
            mNumBuffers = mHalCamCtrl->getZSLQueueDepth() + 3;
        }
    }
    ret = QCameraStream::initStream(no_cb_needed, stream_on);
end:
    ALOGI(" %s : X ", __FUNCTION__);
    return ret;
}

int QCameraStream_Rdi::getBuf(mm_camera_frame_len_offset *frame_offset_info,
                              uint8_t num_bufs,
                              uint8_t *initial_reg_flag,
                              mm_camera_buf_def_t  *bufs)
{
    int ret = 0;
    ALOGE("%s:BEGIN",__func__);

    if (num_bufs > mNumBuffers) {
        mNumBuffers = num_bufs;
    }
    if ((mNumBuffers == 0) || (mNumBuffers > MM_CAMERA_MAX_NUM_FRAMES)) {
        ALOGE("%s: Invalid number of buffers (=%d) requested!",
             __func__, mNumBuffers);
        return BAD_VALUE;
    }

    memset(mRdiBuf, 0, sizeof(mRdiBuf));
    memcpy(&mFrameOffsetInfo, frame_offset_info, sizeof(mFrameOffsetInfo));
    ret = mHalCamCtrl->initHeapMem(&mHalCamCtrl->mRdiMemory,
                                   mNumBuffers,
                                   mFrameOffsetInfo.frame_len,
                                   MSM_PMEM_MAINIMG,
                                   &mFrameOffsetInfo,
                                   mRdiBuf);
    if(MM_CAMERA_OK == ret) {
        for(int i = 0; i < num_bufs; i++) {
            bufs[i] = mRdiBuf[i];
            initial_reg_flag[i] = true;
        }
    }

    ALOGV("%s: X - ret = %d ", __func__, ret);
    return ret;
}

int QCameraStream_Rdi::putBuf(uint8_t num_bufs, mm_camera_buf_def_t *bufs)
{
    int ret = 0;
    ALOGE(" %s : E ", __FUNCTION__);
    ret = mHalCamCtrl->releaseHeapMem(&mHalCamCtrl->mRdiMemory);
    ALOGI(" %s : X ",__FUNCTION__);
    return ret;
}

void QCameraStream_Rdi::dumpFrameToFile(mm_camera_buf_def_t* newFrame)
{
    char buf[32];
    int file_fd;
    int i;
    char *ext = "yuv";
    int w,h,main_422;
    static int count = 0;
    char *name = "rdi";

    w = mHalCamCtrl->mRdiWidth;
    h = mHalCamCtrl->mRdiHeight;
    main_422 = 1;

    if ( newFrame != NULL) {
        char * str;
        snprintf(buf, sizeof(buf), "/data/%s_%d.%s", name,count,ext);
        file_fd = open(buf, O_RDWR | O_CREAT, 0777);
        if (file_fd < 0) {
            ALOGE("%s: cannot open file\n", __func__);
        } else {
            void* y_off = newFrame->buffer + newFrame->planes[0].data_offset;
            void* cbcr_off = newFrame->buffer + newFrame->planes[0].length;

            write(file_fd, (const void *)(y_off), newFrame->planes[0].length);
            write(file_fd, (const void *)(cbcr_off),
                  (newFrame->planes[1].length * newFrame->num_planes));
            close(file_fd);
        }
        count++;
    }
}

status_t QCameraStream_Rdi::processRdiFrame(
  mm_camera_super_buf_t *frame)
{
    ALOGE("%s",__func__);
    status_t err = NO_ERROR;
    int msgType = 0;
    camera_memory_t *data = NULL;

    if(!mActive) {
        ALOGE("RDI Streaming Stopped. Returning callback");
        return NO_ERROR;
    }

    if(mHalCamCtrl==NULL) {
        ALOGE("%s: X: HAL control object not set",__func__);
        /*Call buf done*/
        return BAD_VALUE;
    }

    mHalCamCtrl->dumpFrameToFile(frame->bufs[0], HAL_DUMP_FRM_RDI);

    if (mHalCamCtrl->mDataCb != NULL) {
      //Sending rdi callback if corresponding Msgs are enabled
      if(mHalCamCtrl->mMsgEnabled & CAMERA_MSG_PREVIEW_FRAME) {
          msgType |=  CAMERA_MSG_PREVIEW_FRAME;
          data = mHalCamCtrl->mRdiMemory.camera_memory[frame->bufs[0]->buf_idx];
      } else {
          data = NULL;
      }

      ALOGD("Message enabled = 0x%x", msgType);
      if(mActive && msgType) {
          mHalCamCtrl->mDataCb(msgType, data, 0, NULL, mHalCamCtrl->mCallbackCookie);
      }
      ALOGD("end of cb");
    }
    if (MM_CAMERA_OK != p_mm_ops->ops->qbuf(mCameraHandle, mChannelId,
                                            frame->bufs[0])) {
        ALOGE("%s: Failed in Preview Qbuf\n", __func__);
        err = BAD_VALUE;
    }
    mHalCamCtrl->cache_ops((QCameraHalMemInfo_t *)(frame->bufs[0]->mem_info),
                           frame->bufs[0]->buffer,
                           ION_IOC_INV_CACHES);

    return err;
}


// ---------------------------------------------------------------------------
// QCameraStream_Rdi
// ---------------------------------------------------------------------------

QCameraStream_Rdi::
QCameraStream_Rdi(uint32_t CameraHandle,
                  uint32_t ChannelId,
                  uint32_t Width,
                  uint32_t Height,
                  uint32_t Format,
                  uint8_t NumBuffers,
                  mm_camera_vtbl_t *mm_ops,
                  mm_camera_img_mode imgmode,
                  camera_mode_t mode,
                  QCameraHardwareInterface* camCtrl)
  : QCameraStream(CameraHandle,
                 ChannelId,
                 Width,
                 Height,
                 Format,
                 NumBuffers,
                 mm_ops,
                 imgmode,
                 mode,
                  camCtrl)
{
    ALOGE("%s: E", __func__);
    ALOGE("%s: X", __func__);
}
// ---------------------------------------------------------------------------
// QCameraStream_Rdi
// ---------------------------------------------------------------------------

QCameraStream_Rdi::~QCameraStream_Rdi() {
    ALOGV("%s: E", __func__);
    release();
    ALOGV("%s: X", __func__);

}
// ---------------------------------------------------------------------------
// QCameraStream_Rdi
// ---------------------------------------------------------------------------
  void QCameraStream_Rdi::release() {

    ALOGE("%s : BEGIN",__func__);
    streamOff(0);
    deinitStream();
    ALOGE("%s: END", __func__);
  }

// ---------------------------------------------------------------------------
// No code beyone this line
// ---------------------------------------------------------------------------
}; // namespace android
