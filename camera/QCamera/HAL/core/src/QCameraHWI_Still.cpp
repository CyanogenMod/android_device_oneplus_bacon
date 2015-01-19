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

#define LOG_NDEBUG 0
#define LOG_NDDEBUG 0
#define LOG_NIDEBUG 0
#define LOG_TAG "QCameraHWI_Still"
#include <utils/Log.h>
#include <utils/threads.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <media/mediarecorder.h>
#include <math.h>
#include "QCameraHAL.h"
#include "QCameraHWI.h"
#include "mm_jpeg_interface.h"


/* following code implement the still image capture & encoding logic of this class*/
namespace android {

QCameraStream_SnapshotMain::
QCameraStream_SnapshotMain(uint32_t CameraHandle,
                           uint32_t ChannelId,
                           uint32_t Width,
                           uint32_t Height,
                           uint32_t Format,
                           uint8_t NumBuffers,
                           mm_camera_vtbl_t *mm_ops,
                           mm_camera_img_mode imgmode,
                           camera_mode_t mode,
                           QCameraHardwareInterface* camCtrl)
  :QCameraStream(CameraHandle,
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
}

QCameraStream_SnapshotMain::~QCameraStream_SnapshotMain()
{
    release();
}

void QCameraStream_SnapshotMain::release()
{
    streamOff(0);
    deinitStream();

}

status_t QCameraStream_SnapshotMain::initStream(uint8_t no_cb_needed, uint8_t stream_on)
{
    status_t ret = NO_ERROR;

    if(mHalCamCtrl->isZSLMode()) {
        mNumBuffers = mHalCamCtrl->getZSLQueueDepth() + 3;
    } else if (mHalCamCtrl->mHdrInfo.hdr_on) {
        mNumBuffers = mHalCamCtrl->mHdrInfo.num_frame;
    } else {
        mNumBuffers = mHalCamCtrl->getNumOfSnapshots();
    }
    ret = QCameraStream::initStream(no_cb_needed, stream_on);
    return ret;;
}

int QCameraStream_SnapshotMain::getBuf(mm_camera_frame_len_offset *frame_offset_info,
                                       uint8_t num_bufs,
                                       uint8_t *initial_reg_flag,
                                       mm_camera_buf_def_t  *bufs)
{
    int ret = MM_CAMERA_OK;
    ALOGE("%s : E", __func__);

    if (mNumBuffers < num_bufs) {
        mNumBuffers = num_bufs;
    }
    if ((mNumBuffers == 0) || (mNumBuffers > MM_CAMERA_MAX_NUM_FRAMES)) {
        ALOGE("%s: Invalid number of buffers (=%d) requested!",
             __func__, mNumBuffers);
        return BAD_VALUE;
    }

    memset(mSnapshotStreamBuf, 0, sizeof(mSnapshotStreamBuf));
    memcpy(&mFrameOffsetInfo, frame_offset_info, sizeof(mFrameOffsetInfo));
    ret = mHalCamCtrl->initHeapMem(&mHalCamCtrl->mSnapshotMemory,
                                   mNumBuffers,
                                   mFrameOffsetInfo.frame_len,
                                   MSM_PMEM_MAINIMG,
                                   &mFrameOffsetInfo,
                                   mSnapshotStreamBuf);

    if(MM_CAMERA_OK == ret) {
        for(int i = 0; i < num_bufs; i++) {
            bufs[i] = mSnapshotStreamBuf[i];
            initial_reg_flag[i] = (TRUE == m_flag_stream_on)? TRUE : FALSE;
        }
    }

    /* If we have reached here successfully, we have allocated buffer.
       Set state machine.*/
    ALOGD("%s: X", __func__);
    return ret;

}

int QCameraStream_SnapshotMain::putBuf(uint8_t num_bufs, mm_camera_buf_def_t *bufs)
{
    ALOGE("%s: Release Snapshot main Memory",__func__);
    return mHalCamCtrl->releaseHeapMem(&mHalCamCtrl->mSnapshotMemory);
}

int QCameraStream_SnapshotThumbnail::putBuf(uint8_t num_bufs, mm_camera_buf_def_t *bufs)
{
     ALOGE("%s: Release Snapshot thumbnail Memory",__func__);
     return mHalCamCtrl->releaseHeapMem(&mHalCamCtrl->mThumbnailMemory);
}

QCameraStream_SnapshotThumbnail::
QCameraStream_SnapshotThumbnail(uint32_t CameraHandle,
                                uint32_t ChannelId,
                                uint32_t Width,
                                uint32_t Height,
                                uint32_t Format,
                                uint8_t NumBuffers,
                                mm_camera_vtbl_t *mm_ops,
                                mm_camera_img_mode imgmode,
                                camera_mode_t mode,
                                QCameraHardwareInterface* camCtrl)
  :QCameraStream(CameraHandle,
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

}

QCameraStream_SnapshotThumbnail::~QCameraStream_SnapshotThumbnail()
{
     release();
}

void QCameraStream_SnapshotThumbnail::release()
{
    streamOff(0);
    deinitStream();
}

status_t QCameraStream_SnapshotThumbnail::initStream(uint8_t no_cb_needed, uint8_t stream_on)
{
    status_t ret = NO_ERROR;

    if(mHalCamCtrl->isZSLMode()) {
        mNumBuffers = mHalCamCtrl->getZSLQueueDepth() + 3;
    } else if (mHalCamCtrl->mHdrInfo.hdr_on) {
        mNumBuffers = mHalCamCtrl->mHdrInfo.num_frame;
    } else {
        mNumBuffers = mHalCamCtrl->getNumOfSnapshots();
    }
    ret = QCameraStream::initStream(no_cb_needed, stream_on);
    return ret;;
}

status_t QCameraStream_SnapshotThumbnail::getBuf(mm_camera_frame_len_offset *frame_offset_info,
                                                 uint8_t num_bufs,
                                                 uint8_t *initial_reg_flag,
                                                 mm_camera_buf_def_t  *bufs)
{
    int ret = MM_CAMERA_OK;

    if (mNumBuffers < num_bufs) {
        mNumBuffers = num_bufs;
    }
    if ((mNumBuffers == 0) || (mNumBuffers > MM_CAMERA_MAX_NUM_FRAMES)) {
        ALOGE("%s: Invalid number of buffers (=%d) requested!",
             __func__, mNumBuffers);
        return BAD_VALUE;
    }

    memset(mPostviewStreamBuf, 0, sizeof(mPostviewStreamBuf));
    memcpy(&mFrameOffsetInfo, frame_offset_info, sizeof(mFrameOffsetInfo));
    ret = mHalCamCtrl->initHeapMem(
                         &mHalCamCtrl->mThumbnailMemory,
                         mNumBuffers,
                         mFrameOffsetInfo.frame_len,
                         MSM_PMEM_THUMBNAIL,
                         &mFrameOffsetInfo,
                         mPostviewStreamBuf);
    if(MM_CAMERA_OK == ret) {
        for(int i = 0; i < num_bufs; i++) {
            bufs[i] = mPostviewStreamBuf[i];
            initial_reg_flag[i] = true;
        }
    }

    return ret;
}


}; // namespace android

