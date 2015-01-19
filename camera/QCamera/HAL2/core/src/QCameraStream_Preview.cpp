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

#define ALOG_TAG "QCameraHWI_Preview"
#include <utils/Log.h>
#include <utils/threads.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "QCameraHAL.h"
#include "QCameraHWI.h"
#include <genlock.h>
#include <gralloc_priv.h>

#define UNLIKELY(exp) __builtin_expect(!!(exp), 0)

/* QCameraHWI_Preview class implementation goes here*/
/* following code implement the preview mode's image capture & display logic of this class*/

namespace android {

status_t QCameraStream_preview::setPreviewWindow(const camera2_stream_ops_t* window)
{
    status_t retVal = NO_ERROR;
    ALOGE(" %s: E ", __FUNCTION__);
    if( window == NULL) {
        ALOGW(" Setting NULL preview window ");
        /* TODO: Current preview window will be invalidated.
         * Release all the buffers back */
       // relinquishBuffers();
    }
    Mutex::Autolock lock(mLock);
    mPreviewWindow = window;
    ALOGV(" %s : X ", __FUNCTION__ );
    return retVal;
}

int QCameraStream_preview::registerStreamBuffers(int num_buffers,
        buffer_handle_t *buffers)
{
    int err;

    mNumBuffers = num_buffers;
    mPreviewMemory.buffer_count = num_buffers;
    for (int i = 0; i < num_buffers; i++) {
        mPreviewMemory.buffer_handle[i] = buffers[i];
        mPreviewMemory.private_buffer_handle[i] = (struct private_handle_t *)buffers[i];
        ALOGD("%s:Buffer Size:%d",__func__, mPreviewMemory.private_buffer_handle[i]->size);
    }
    mDisplayBuf = new mm_camera_buf_def_t[num_buffers];
    if (!mDisplayBuf) {
        ALOGE("Unable to allocate mDisplayBuf");
        return NO_MEMORY;
    }

   return OK;
}

status_t QCameraStream_preview::initBuffers()
{
    status_t ret = NO_ERROR;
    int width = mWidth;  /* width of channel  */
    int height = mHeight; /* height of channel */
    uint32_t frame_len = mFrameOffsetInfo.frame_len; /* frame planner length */
    int buffer_num = mNumBuffers; /* number of buffers for display */
    const char *pmem_region;
    uint8_t num_planes = mFrameOffsetInfo.num_planes;
    uint32_t planes[VIDEO_MAX_PLANES];
    void *vaddr = NULL;
    buffer_handle_t *buffer_handle;

    ALOGE("%s:BEGIN",__func__);
    memset(mNotifyBuffer, 0, sizeof(mNotifyBuffer));

    this->myMode=myMode; /*Need to assign this in constructor after translating from mask*/
    num_planes = mFrameOffsetInfo.num_planes;
    for(int i=0; i < num_planes; i++)
        planes[i] = mFrameOffsetInfo.mp[i].len;


    mPreviewMemoryLock.lock();
    memset(mDisplayBuf, 0, sizeof(mm_camera_buf_def_t) * mNumBuffers);

    /*allocate memory for the buffers*/
    for(int i = 0; i < mNumBuffers; i++){
        mDisplayBuf[i].num_planes = num_planes;
        mDisplayBuf[i].buf_idx = i;

        /* Plane 0 needs to be set seperately. Set other planes
         * in a loop. */
        mDisplayBuf[i].planes[0].length = planes[0];
        mDisplayBuf[i].planes[0].m.userptr = mPreviewMemory.private_buffer_handle[i]->fd;
        mDisplayBuf[i].planes[0].data_offset = mFrameOffsetInfo.mp[0].offset;
        mDisplayBuf[i].planes[0].reserved[0] =0;
        for (int j = 1; j < num_planes; j++) {
            mDisplayBuf[i].planes[j].length = planes[j];
            mDisplayBuf[i].planes[j].m.userptr = mPreviewMemory.private_buffer_handle[i]->fd;
            mDisplayBuf[i].planes[j].data_offset = mFrameOffsetInfo.mp[j].offset;
            mDisplayBuf[i].planes[j].reserved[0] =
            mDisplayBuf[i].planes[j-1].reserved[0] +
            mDisplayBuf[i].planes[j-1].length;
        }

        for (int j = 0; j < num_planes; j++)
            ALOGE("Planes: %d length: %d userptr: %lu offset: %d\n", j,
                     mDisplayBuf[i].planes[j].length,
                     mDisplayBuf[i].planes[j].m.userptr,
                     mDisplayBuf[i].planes[j].reserved[0]);

        mDisplayBuf[i].stream_id = mMmStreamId;
        mDisplayBuf[i].fd = mPreviewMemory.private_buffer_handle[i]->fd;
        ALOGE("DEBUG2:Display buf[%d] fd:%d",i,mDisplayBuf[i].fd);
        mDisplayBuf[i].frame_len = mFrameOffsetInfo.frame_len;
    }/*end of for loop*/

    /* Dequeue N frames from native window and queue into interface. Only dequeue our requested buffers */
    for (int i = 0; i < mNumBuffers; i++)
        mPreviewMemory.local_flag[i] = BUFFER_NOT_REGGED;
    for (int i = 0; i < OPAQUE_BUFFER_COUNT; i++) {
        ALOGE("mPreview Window %p",mPreviewWindow);
        int err = mPreviewWindow->dequeue_buffer(mPreviewWindow, &buffer_handle);
        if (err == NO_ERROR && buffer_handle) {
            int j;
            for (j = 0; j < mPreviewMemory.buffer_count; j++) {
                if (mPreviewMemory.buffer_handle[j] == *buffer_handle) {
                    mPreviewMemory.local_flag[j] = BUFFER_OWNED;
                    ALOGE("%s: [%d]: local_flag = 1", __func__, j);
                    break;
                }
            }
            if (j == mPreviewMemory.buffer_count) {
                ALOGE("Cannot find matching handle in the table.");
                return INVALID_OPERATION;
            }
        } else {
            ALOGE("dequeue_buffer failed.");
            return INVALID_OPERATION;
        }
    }

    /* register the streaming buffers for the channel*/
    mPreviewMemoryLock.unlock();
    ALOGE("%s:END",__func__);
    return NO_ERROR;

error:
    mPreviewMemoryLock.unlock();

    ALOGV("%s: X", __func__);
    return ret;
}

void QCameraStream_preview::deinitBuffers()
{
    mPreviewMemoryLock.lock();
    for (int i = 0; i < mNumBuffers; i++) {
        if (mPreviewMemory.local_flag[i] == BUFFER_OWNED) {
            mPreviewWindow->cancel_buffer(mPreviewWindow,
                &mPreviewMemory.buffer_handle[i]);
            mPreviewMemory.local_flag[i] = BUFFER_NOT_OWNED;
        }
    }
    mPreviewMemoryLock.unlock();
}

void QCameraStream_preview::dumpFrameToFile(mm_camera_buf_def_t *newFrame)
{
  int32_t enabled = 0;
  int frm_num;
  uint32_t  skip_mode;
  char value[PROPERTY_VALUE_MAX];
  char buf[32];
  int w, h;
  static int count = 0;
  int file_fd;
  int rc = 0;
  int len;
  unsigned long addr;
  unsigned long * tmp = (unsigned long *)newFrame->buffer;
  addr = *tmp;

  len = newFrame->frame_len;
  count++;
  if(count < 100) {
    snprintf(buf, sizeof(buf), "/data/mzhu%d.yuv", count);
    file_fd = open(buf, O_RDWR | O_CREAT, 0777);

    rc = write(file_fd, (const void *)addr, len);
    ALOGE("%s: file='%s', vaddr_old=0x%x, addr_map = 0x%p, len = %d, rc = %d",
          __func__, buf, (uint32_t)newFrame->buffer, (void *)addr, len, rc);
    close(file_fd);
    ALOGE("%s: dump %s, rc = %d, len = %d", __func__, buf, rc, len);
  }
}

int QCameraStream_preview::prepareStream()
{
    mm_camera_img_mode img_mode;
    cam_format_t format;
    struct private_handle_t *private_handle = mPreviewMemory.private_buffer_handle[0];

    ALOGE("%s: private_handle->format = %d, flags = %d", __func__,
        private_handle->format, private_handle->flags);
    if (private_handle->flags & private_handle_t::PRIV_FLAGS_VIDEO_ENCODER)
        img_mode = MM_CAMERA_VIDEO;
    else if (private_handle->flags & private_handle_t::PRIV_FLAGS_CAMERA_WRITE)
        img_mode = MM_CAMERA_PREVIEW;
    else {
        ALOGE("%s: Invalid flags %d\n", __func__, private_handle->flags);
        return BAD_VALUE;
    }

    switch (private_handle->format) {
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
        format = CAMERA_YUV_420_NV12;
        break;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        format = CAMERA_YUV_420_NV21;
        break;
    default:
        ALOGE("%s: Invalid format!!!", __func__);
        return BAD_VALUE;
    }
    if(NO_ERROR!=initStream(img_mode, format)) {
        ALOGE("Init stream failed");
        return BAD_VALUE;
    }
    return OK;
}

status_t QCameraStream_preview::processPreviewFrame (
  mm_camera_super_buf_t *frame)
{
    ALOGV("%s",__func__);
    int err = 0;
    int msgType = 0;
    int i;
    camera_frame_metadata_t *metadata = NULL;

    Mutex::Autolock lock(mLock);
    if(!mActive) {
        ALOGE("Preview Stopped. Returning callback");
        return NO_ERROR;
    }

    if(mHalCamCtrl==NULL) {
        ALOGE("%s: X: HAL control object not set",__func__);
       /*Call buf done*/
       return BAD_VALUE;
    }
    nsecs_t timeStamp = seconds_to_nanoseconds(frame->bufs[0]->ts.tv_sec) ;
    timeStamp += frame->bufs[0]->ts.tv_nsec;

    if(mFirstFrameRcvd == false) {
        //mm_camera_util_profile("HAL: First preview frame received");
        mFirstFrameRcvd = true;
    }

    //  dumpFrameToFile(frame->bufs[0]);

    mPreviewMemoryLock.lock();
    mNotifyBuffer[frame->bufs[0]->buf_idx] = *frame;
    ALOGE("processPreviewFrame: timeStamp = %lld", (int64_t)timeStamp);
    err = mPreviewWindow->enqueue_buffer(mPreviewWindow, (int64_t)timeStamp,
                  &mPreviewMemory.buffer_handle[frame->bufs[0]->buf_idx]);
    if(err != 0) {
        ALOGE("%s: enqueue_buffer failed, err = %d", __func__, err);
    }
    mPreviewMemory.local_flag[frame->bufs[0]->buf_idx] = BUFFER_NOT_OWNED;

    buffer_handle_t *buffer_handle = NULL;
    err = mPreviewWindow->dequeue_buffer(mPreviewWindow, &buffer_handle);
    if (err == NO_ERROR && buffer_handle != NULL) {
        int rc = MM_CAMERA_OK;
        ALOGD("%s: dequed buf hdl =%p", __func__, *buffer_handle);
        for(i = 0; i < mPreviewMemory.buffer_count; i++) {
            if(mPreviewMemory.buffer_handle[i] == *buffer_handle) {
                ALOGE("<DEBUG2>:Found buffer in idx:%d",i);
                break;
            }
        }
        if (mPreviewMemory.local_flag[i] == BUFFER_NOT_REGGED) {
            mm_camera_buf_def_t buf = mDisplayBuf[i];
            mPreviewMemory.local_flag[i] = BUFFER_OWNED;
            rc = p_mm_ops->ops->qbuf(mCameraHandle, mChannelId, &buf);
        } else {
            mPreviewMemory.local_flag[i] = BUFFER_OWNED;
            rc = p_mm_ops->ops->qbuf(mCameraHandle, mChannelId, mNotifyBuffer[i].bufs[0]);
        }

        if(rc != MM_CAMERA_OK) {
            /* how to handle the error of qbuf? */
            ALOGE("BUF DONE FAILED");
        }
    }
    /* Save the last displayed frame. We'll be using it to fill the gap between
       when preview stops and postview start during snapshot.*/
    mLastQueuedFrame = &(mDisplayBuf[frame->bufs[0]->buf_idx]);
    mPreviewMemoryLock.unlock();

    return NO_ERROR;
}

// ---------------------------------------------------------------------------
// QCameraStream_preview
// ---------------------------------------------------------------------------

QCameraStream_preview::QCameraStream_preview(uint32_t CameraHandle,
                        uint32_t ChannelId,
                        uint32_t Width,
                        uint32_t Height,
                        int requestedFormat,
                        mm_camera_vtbl_t *mm_ops,
                        camera_mode_t mode) :
                 QCameraStream(CameraHandle,
                        ChannelId,
                        Width,
                        Height,
                        mm_ops,
                        mode),
                 mLastQueuedFrame(NULL),
                 mDisplayBuf(NULL),
                 mNumFDRcvd(0)
{
    mHalCamCtrl = NULL;
    ALOGE("%s: E", __func__);

    mStreamId = allocateStreamId();

    switch (requestedFormat) {
    case CAMERA2_HAL_PIXEL_FORMAT_OPAQUE:
        mMaxBuffers = 5;
        break;
    case HAL_PIXEL_FORMAT_BLOB:
        mMaxBuffers = 1;
        break;
    default:
        ALOGE("Unsupported requested format %d", requestedFormat);
        mMaxBuffers = 1;
        break;
    }
    /*TODO: There has to be a better way to do this*/
    ALOGE("%s: X", __func__);
}
// ---------------------------------------------------------------------------
// QCameraStream_preview
// ---------------------------------------------------------------------------

QCameraStream_preview::~QCameraStream_preview() {
    ALOGV("%s: E", __func__);
	if(mActive) {
		streamOff(0);
	}
	if(mInit) {
		deinitStream();
	}
	mInit = false;
	mActive = false;
    if (mDisplayBuf) {
        delete[] mDisplayBuf;
        mDisplayBuf = NULL;
    }
    deallocateStreamId(mStreamId);
    ALOGV("%s: X", __func__);

}
// ---------------------------------------------------------------------------
// QCameraStream_preview
// ---------------------------------------------------------------------------

status_t QCameraStream_preview::init()
{
    status_t ret = NO_ERROR;
    ALOGV("%s: E", __func__);

#if 0
    if (!(myMode & CAMERA_ZSL_MODE)) {
        ALOGE("Setting OP MODE to MM_CAMERA_OP_MODE_VIDEO");
        mm_camera_op_mode_type_t op_mode=MM_CAMERA_OP_MODE_VIDEO;
        ret = p_mm_ops->ops->set_parm (mCameraHandle, MM_CAMERA_PARM_OP_MODE,
                                        &op_mode);
        ALOGE("OP Mode Set");

        if(MM_CAMERA_OK != ret) {
          ALOGE("%s: X :set mode MM_CAMERA_OP_MODE_VIDEO err=%d\n", __func__, ret);
          return BAD_VALUE;
        }
    }else {
        ALOGE("Setting OP MODE to MM_CAMERA_OP_MODE_ZSL");
        mm_camera_op_mode_type_t op_mode=MM_CAMERA_OP_MODE_ZSL;
        ret =p_mm_ops->ops->set_parm (mCameraHandle, MM_CAMERA_PARM_OP_MODE,
                                        &op_mode);
        if(MM_CAMERA_OK != ret) {
          ALOGE("%s: X :set mode MM_CAMERA_OP_MODE_ZSL err=%d\n", __func__, ret);
          return BAD_VALUE;
        }
     }

    setFormat();
    ret = QCameraStream::initStream();
    if (NO_ERROR!=ret) {
        ALOGE("%s E: can't init native cammera preview ch\n",__func__);
        return ret;
    }

  mInit = true;
#endif
  return ret;
}
// ---------------------------------------------------------------------------
// QCameraStream_preview
// ---------------------------------------------------------------------------

status_t QCameraStream_preview::start()
{
    ALOGV("%s: E", __func__);
    status_t ret = NO_ERROR;

    Mutex::Autolock lock(mLock);

    /* call start() in parent class to start the monitor thread*/
    //QCameraStream::start ();

#if 0
    ret = cam_config_prepare_buf(mCameraId, &mDisplayBuf);
    ALOGE("Debug : %s : cam_config_prepare_buf",__func__);
    if(ret != MM_CAMERA_OK) {
        ALOGV("%s:reg preview buf err=%d\n", __func__, ret);
        ret = BAD_VALUE;
        goto error;
    }else {
        ret = NO_ERROR;
    }
#endif
    /* For preview, the OP_MODE we set is dependent upon whether we are
       starting camera or camcorder. For snapshot, anyway we disable preview.
       However, for ZSL we need to set OP_MODE to OP_MODE_ZSL and not
       OP_MODE_VIDEO. We'll set that for now in CamCtrl. So in case of
       ZSL we skip setting Mode here */


    /* call mm_camera action start(...)  */
    ALOGE("Starting Preview/Video Stream. ");
    mFirstFrameRcvd = false;

    ALOGE("Starting Preview/Video Stream. ");
    ret = streamOn();
    if (MM_CAMERA_OK != ret) {
      ALOGE ("%s: preview streaming start err=%d\n", __func__, ret);
      ret = BAD_VALUE;
      goto end;
    }

    ALOGE("Debug : %s : Preview streaming Started",__func__);
    ret = NO_ERROR;

    mActive =  true;
    goto end;

end:
    ALOGE("%s: X", __func__);
    return ret;
  }


// ---------------------------------------------------------------------------
// QCameraStream_preview
// ---------------------------------------------------------------------------
  void QCameraStream_preview::stop() {
    ALOGE("%s: E", __func__);
    int ret=MM_CAMERA_OK;

    if(!mActive) {
      return;
    }
    Mutex::Autolock lock(mLock);
    mActive =  false;
    /* unregister the notify fn from the mmmm_camera_t object*/

    ALOGI("%s: Stop the thread \n", __func__);
    /* call stop() in parent class to stop the monitor thread*/
    //stream_info = mHalCamCtrl->getChannelInterface();

    ret = streamOff(0);
    if(MM_CAMERA_OK != ret) {
      ALOGE ("%s: camera preview stop err=%d\n", __func__, ret);
    }
#if 0
    ret = cam_config_unprepare_buf(mCameraId, MM_CAMERA_CH_PREVIEW);
    if(ret != MM_CAMERA_OK) {
      ALOGE("%s:Unreg preview buf err=%d\n", __func__, ret);
      //ret = BAD_VALUE;
    }
#endif
    /* In case of a clean stop, we need to clean all buffers*/
    ALOGE("Debug : %s : Buffer Unprepared",__func__);
    /*free camera_memory handles and return buffer back to surface*/
    ret= QCameraStream::deinitStream();
    ALOGE(": %s : De init Channel",__func__);
    if(ret != MM_CAMERA_OK) {
      ALOGE("%s:Deinit preview channel failed=%d\n", __func__, ret);
      //ret = BAD_VALUE;
    }
    ALOGE("%s: X", __func__);

  }
// ---------------------------------------------------------------------------
// QCameraStream_preview
// ---------------------------------------------------------------------------
  void QCameraStream_preview::release() {

    ALOGE("%s : BEGIN",__func__);
    int ret=MM_CAMERA_OK,i;

    if(!mInit)
    {
      ALOGE("%s : Stream not Initalized",__func__);
      return;
    }

    if(mActive) {
      this->streamOff(0);
    }


    if(mInit) {
        deinitStream();
    }
    mInit = false;
    ALOGE("%s: END", __func__);

  }

QCameraStream*
QCameraStream_preview::createInstance(uint32_t CameraHandle,
                        uint32_t ChannelId,
                        uint32_t Width,
                        uint32_t Height,
                        int requestedFormat,
                        mm_camera_vtbl_t *mm_ops,
                        camera_mode_t mode)
{
  QCameraStream* pme = new QCameraStream_preview(CameraHandle,
                        ChannelId,
                        Width,
                        Height,
                        requestedFormat,
                        mm_ops,
                        mode);
  return pme;
}
// ---------------------------------------------------------------------------
// QCameraStream_preview
// ---------------------------------------------------------------------------

void QCameraStream_preview::deleteInstance(QCameraStream *p)
{
  if (p){
    ALOGV("%s: BEGIN", __func__);
    p->release();
    delete p;
    p = NULL;
    ALOGV("%s: END", __func__);
  }
}


/* Temp helper function */
void *QCameraStream_preview::getLastQueuedFrame(void)
{
    return mLastQueuedFrame;
}

// ---------------------------------------------------------------------------
// No code beyone this line
// ---------------------------------------------------------------------------
}; // namespace android
