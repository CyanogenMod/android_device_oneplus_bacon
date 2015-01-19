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

#define LOG_TAG "QCameraHWI_Preview"
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

status_t QCameraStream_preview::getBufferFromSurface() {
    int err = 0;
    status_t ret = NO_ERROR;
    int gralloc_usage;
    struct ion_fd_data ion_info_fd;
    preview_stream_ops_t *previewWindow = mHalCamCtrl->mPreviewWindow;

    ALOGI(" %s : E ", __FUNCTION__);

    if( previewWindow == NULL) {
        ALOGE("%s: PreviewWindow = NULL", __func__);
            return INVALID_OPERATION;
    }

    mHalCamCtrl->mPreviewMemoryLock.lock();
    mHalCamCtrl->mPreviewMemory.buffer_count = mNumBuffers;
    err = previewWindow->set_buffer_count(previewWindow, mHalCamCtrl->mPreviewMemory.buffer_count );
    if (err != 0) {
         ALOGE("set_buffer_count failed: %s (%d)",
                    strerror(-err), -err);
         ret = UNKNOWN_ERROR;
         goto end;
    }

    err = previewWindow->set_buffers_geometry(previewWindow, mWidth, mHeight,
                                              mHalCamCtrl->getPreviewFormatInfo().Hal_format);
    if (err != 0) {
         ALOGE("set_buffers_geometry failed: %s (%d)",
                    strerror(-err), -err);
         ret = UNKNOWN_ERROR;
         goto end;
    }

    ret = p_mm_ops->ops->get_parm(mCameraHandle, MM_CAMERA_PARM_VFE_OUTPUT_ENABLE, &mVFEOutputs);
    if(ret != MM_CAMERA_OK) {
        ALOGE("get parm MM_CAMERA_PARM_VFE_OUTPUT_ENABLE  failed");
        ret = BAD_VALUE;
        goto end;
    }

    //as software encoder is used to encode 720p, to enhance the performance
    //cashed pmem is used here
    if(mVFEOutputs == 1)
        gralloc_usage = CAMERA_GRALLOC_HEAP_ID | CAMERA_GRALLOC_FALLBACK_HEAP_ID;
    else
        gralloc_usage = CAMERA_GRALLOC_HEAP_ID | CAMERA_GRALLOC_FALLBACK_HEAP_ID |
                    CAMERA_GRALLOC_CACHING_ID;
    err = previewWindow->set_usage(previewWindow, gralloc_usage);
    if(err != 0) {
    /* set_usage error out */
        ALOGE("%s: set_usage rc = %d", __func__, err);
        ret = UNKNOWN_ERROR;
        goto end;
    }
    ret = p_mm_ops->ops->get_parm(mCameraHandle, MM_CAMERA_PARM_HFR_FRAME_SKIP, &mHFRFrameSkip);
    if(ret != MM_CAMERA_OK) {
        ALOGE("get parm MM_CAMERA_PARM_HFR_FRAME_SKIP  failed");
        ret = BAD_VALUE;
        goto end;
    }
	for (int cnt = 0; cnt < mHalCamCtrl->mPreviewMemory.buffer_count; cnt++) {
		int stride;
		err = previewWindow->dequeue_buffer(previewWindow,
										&mHalCamCtrl->mPreviewMemory.buffer_handle[cnt],
										&mHalCamCtrl->mPreviewMemory.stride[cnt]);
		if(!err) {
            ALOGV("%s: dequeue buf hdl =%p", __func__, *mHalCamCtrl->mPreviewMemory.buffer_handle[cnt]);
            err = previewWindow->lock_buffer(previewWindow,
                                       mHalCamCtrl->mPreviewMemory.buffer_handle[cnt]);
            // lock the buffer using genlock
            ALOGV("%s: camera call genlock_lock, hdl=%p",
                          __FUNCTION__, (*mHalCamCtrl->mPreviewMemory.buffer_handle[cnt]));
            if (GENLOCK_NO_ERROR != genlock_lock_buffer((native_handle_t *)(*mHalCamCtrl->mPreviewMemory.buffer_handle[cnt]),
                                                      GENLOCK_WRITE_LOCK, GENLOCK_MAX_TIMEOUT)) {
                ALOGE("%s: genlock_lock_buffer(WRITE) failed", __func__);
                mHalCamCtrl->mPreviewMemory.local_flag[cnt] = BUFFER_UNLOCKED;
            } else {
                ALOGV("%s: genlock_lock_buffer hdl =%p", __func__, *mHalCamCtrl->mPreviewMemory.buffer_handle[cnt]);
                mHalCamCtrl->mPreviewMemory.local_flag[cnt] = BUFFER_LOCKED;
            }
		} else {
            mHalCamCtrl->mPreviewMemory.local_flag[cnt] = BUFFER_NOT_OWNED;
            ALOGE("%s: dequeue_buffer idx = %d err = %d", __func__, cnt, err);
        }

		ALOGV("%s: dequeue buf: %p\n", __func__, mHalCamCtrl->mPreviewMemory.buffer_handle[cnt]);

		if(err != 0) {
            ALOGE("%s: dequeue_buffer failed: %s (%d)", __func__,
                    strerror(-err), -err);
            ret = UNKNOWN_ERROR;
			for(int i = 0; i < cnt; i++) {
                if (BUFFER_LOCKED == mHalCamCtrl->mPreviewMemory.local_flag[i]) {
                      ALOGD("%s: camera call genlock_unlock", __func__);
                     if (GENLOCK_FAILURE == genlock_unlock_buffer((native_handle_t *)
                                                  (*(mHalCamCtrl->mPreviewMemory.buffer_handle[i])))) {
                        ALOGE("%s: genlock_unlock_buffer failed: hdl =%p", __func__, (*(mHalCamCtrl->mPreviewMemory.buffer_handle[i])) );
                     } else {
                       mHalCamCtrl->mPreviewMemory.local_flag[i] = BUFFER_UNLOCKED;
                     }
                }
                if( mHalCamCtrl->mPreviewMemory.local_flag[i] != BUFFER_NOT_OWNED) {
                  err = previewWindow->cancel_buffer(previewWindow,
                                          mHalCamCtrl->mPreviewMemory.buffer_handle[i]);
                }
                mHalCamCtrl->mPreviewMemory.local_flag[i] = BUFFER_NOT_OWNED;
                ALOGD("%s: cancel_buffer: hdl =%p", __func__,  (*mHalCamCtrl->mPreviewMemory.buffer_handle[i]));
				mHalCamCtrl->mPreviewMemory.buffer_handle[i] = NULL;
			}
            memset(&mHalCamCtrl->mPreviewMemory, 0, sizeof(mHalCamCtrl->mPreviewMemory));
			goto end;
		}

		mHalCamCtrl->mPreviewMemory.private_buffer_handle[cnt] =
		    (struct private_handle_t *)(*mHalCamCtrl->mPreviewMemory.buffer_handle[cnt]);
#ifdef USE_ION
        mHalCamCtrl->mPreviewMemory.mem_info[cnt].main_ion_fd = open("/dev/ion", O_RDONLY);
        if (mHalCamCtrl->mPreviewMemory.mem_info[cnt].main_ion_fd < 0) {
            ALOGE("%s: failed: could not open ion device\n", __func__);
        } else {
            memset(&ion_info_fd, 0, sizeof(ion_info_fd));
            ion_info_fd.fd = mHalCamCtrl->mPreviewMemory.private_buffer_handle[cnt]->fd;
            if (ioctl(mHalCamCtrl->mPreviewMemory.mem_info[cnt].main_ion_fd,
                      ION_IOC_IMPORT, &ion_info_fd) < 0) {
                ALOGE("ION import failed\n");
            }
        }
#endif
		mHalCamCtrl->mPreviewMemory.camera_memory[cnt] =
		    mHalCamCtrl->mGetMemory(mHalCamCtrl->mPreviewMemory.private_buffer_handle[cnt]->fd,
                                    mHalCamCtrl->mPreviewMemory.private_buffer_handle[cnt]->size,
                                    1,
                                    (void *)this);
		ALOGD("%s: idx = %d, fd = %d, size = %d, offset = %d", __func__,
              cnt, mHalCamCtrl->mPreviewMemory.private_buffer_handle[cnt]->fd,
              mHalCamCtrl->mPreviewMemory.private_buffer_handle[cnt]->size,
              mHalCamCtrl->mPreviewMemory.private_buffer_handle[cnt]->offset);

        mHalCamCtrl->mPreviewMemory.mem_info[cnt].fd =
            mHalCamCtrl->mPreviewMemory.private_buffer_handle[cnt]->fd;
        mHalCamCtrl->mPreviewMemory.mem_info[cnt].size =
            mHalCamCtrl->mPreviewMemory.private_buffer_handle[cnt]->size;
        mHalCamCtrl->mPreviewMemory.mem_info[cnt].handle = ion_info_fd.handle;
    }

    memset(&mHalCamCtrl->mMetadata, 0, sizeof(mHalCamCtrl->mMetadata));
    memset(mHalCamCtrl->mFace, 0, sizeof(mHalCamCtrl->mFace));

end:
    mHalCamCtrl->mPreviewMemoryLock.unlock();

    ALOGI(" %s : X ",__func__);
    return ret;
}

status_t QCameraStream_preview::putBufferToSurface() {
    int err = 0;
    status_t ret = NO_ERROR;

    ALOGI(" %s : E ", __FUNCTION__);

    mHalCamCtrl->mPreviewMemoryLock.lock();
	for (int cnt = 0; cnt < mHalCamCtrl->mPreviewMemory.buffer_count; cnt++) {
        mHalCamCtrl->mPreviewMemory.camera_memory[cnt]->release(mHalCamCtrl->mPreviewMemory.camera_memory[cnt]);
#ifdef USE_ION
        struct ion_handle_data ion_handle;
        memset(&ion_handle, 0, sizeof(ion_handle));
        ion_handle.handle = mHalCamCtrl->mPreviewMemory.mem_info[cnt].handle;
        if (ioctl(mHalCamCtrl->mPreviewMemory.mem_info[cnt].main_ion_fd, ION_IOC_FREE, &ion_handle) < 0) {
            ALOGE("%s: ion free failed\n", __func__);
        }
        close(mHalCamCtrl->mPreviewMemory.mem_info[cnt].main_ion_fd);
#endif
        if (BUFFER_LOCKED == mHalCamCtrl->mPreviewMemory.local_flag[cnt]) {
                ALOGD("%s: camera call genlock_unlock", __FUNCTION__);
	      if (GENLOCK_FAILURE == genlock_unlock_buffer((native_handle_t *)
                                                    (*(mHalCamCtrl->mPreviewMemory.buffer_handle[cnt])))) {
            ALOGE("%s: genlock_unlock_buffer failed, handle =%p", __FUNCTION__, (*(mHalCamCtrl->mPreviewMemory.buffer_handle[cnt])));
              continue;
          } else {

            ALOGD("%s: genlock_unlock_buffer, handle =%p", __FUNCTION__, (*(mHalCamCtrl->mPreviewMemory.buffer_handle[cnt])));
              mHalCamCtrl->mPreviewMemory.local_flag[cnt] = BUFFER_UNLOCKED;
          }
        }
        if( mHalCamCtrl->mPreviewMemory.local_flag[cnt] != BUFFER_NOT_OWNED) {
            if (mHalCamCtrl->mPreviewWindow) {
                err = mHalCamCtrl->mPreviewWindow->cancel_buffer(mHalCamCtrl->mPreviewWindow,
                                                                 mHalCamCtrl->mPreviewMemory.buffer_handle[cnt]);
                ALOGD("%s: cancel_buffer: hdl =%p", __func__,  (*mHalCamCtrl->mPreviewMemory.buffer_handle[cnt]));
            } else {
                ALOGE("%s: Preview window is NULL, cannot cancel_buffer: hdl =%p",
                      __func__,  (*mHalCamCtrl->mPreviewMemory.buffer_handle[cnt]));
            }
        }
        mHalCamCtrl->mPreviewMemory.local_flag[cnt] = BUFFER_NOT_OWNED;
		ALOGD(" put buffer %d successfully", cnt);
	}
    mHalCamCtrl->mPreviewMemoryLock.unlock();
	memset(&mHalCamCtrl->mPreviewMemory, 0, sizeof(mHalCamCtrl->mPreviewMemory));
    ALOGI(" %s : X ",__FUNCTION__);
    return NO_ERROR;
}

status_t QCameraStream_preview::initStream(uint8_t no_cb_needed, uint8_t stream_on)
{
    int format = 0;
    status_t ret = NO_ERROR;
    int numMinUndequeuedBufs;
    int err = 0;

    ALOGI(" %s : E ", __FUNCTION__);
    if (mHalCamCtrl->isNoDisplayMode()) {
        mNumBuffers = PREVIEW_BUFFER_COUNT;
        if(mHalCamCtrl->isZSLMode()) {
            if(mNumBuffers < mHalCamCtrl->getZSLQueueDepth() + 3) {
                mNumBuffers = mHalCamCtrl->getZSLQueueDepth() + 3;
            }
        }
    } else {
        if( mHalCamCtrl->mPreviewWindow == NULL) {
            ALOGE("%s: PreviewWindow = NULL", __func__);
            return INVALID_OPERATION;
        }
        numMinUndequeuedBufs = 0;
        if(mHalCamCtrl->mPreviewWindow->get_min_undequeued_buffer_count) {
            err = mHalCamCtrl->mPreviewWindow->get_min_undequeued_buffer_count(
                            mHalCamCtrl->mPreviewWindow, &numMinUndequeuedBufs);
            if (err != 0) {
                ALOGE("get_min_undequeued_buffer_count  failed: %s (%d)",
                      strerror(-err), -err);
                ret = UNKNOWN_ERROR;
                goto end;
            }
        }
        mNumBuffers = PREVIEW_BUFFER_COUNT + numMinUndequeuedBufs;
        if(mHalCamCtrl->isZSLMode()) {
          if(mHalCamCtrl->getZSLQueueDepth() > numMinUndequeuedBufs)
            mNumBuffers += mHalCamCtrl->getZSLQueueDepth() - numMinUndequeuedBufs;
        }
    }
    ret = QCameraStream::initStream(no_cb_needed, stream_on);
end:
    ALOGI(" %s : X ", __FUNCTION__);
    return ret;
}


status_t  QCameraStream_preview::getBufferNoDisplay( )
{
    status_t ret = NO_ERROR;
    uint32_t planes[VIDEO_MAX_PLANES];

    ALOGI("%s : E ", __FUNCTION__);

    mHalCamCtrl->mPreviewMemoryLock.lock();
    memset(mDisplayBuf, 0, sizeof(mDisplayBuf));
    if (mHalCamCtrl->initHeapMem(&mHalCamCtrl->mNoDispPreviewMemory,
                                 mNumBuffers,
                                 mFrameOffsetInfo.frame_len,
                                 MSM_PMEM_MAINIMG,
                                 &mFrameOffsetInfo,
                                 mDisplayBuf) < 0) {
        ret = NO_MEMORY;
        goto end;
    }

    memset(&mHalCamCtrl->mMetadata, 0, sizeof(mHalCamCtrl->mMetadata));
    memset(mHalCamCtrl->mFace, 0, sizeof(mHalCamCtrl->mFace));

    ALOGI(" %s : X ",__FUNCTION__);
end:
    mHalCamCtrl->mPreviewMemoryLock.unlock();
    return ret;
}

status_t QCameraStream_preview::freeBufferNoDisplay()
{
    ALOGI(" %s : E ", __FUNCTION__);
    mHalCamCtrl->mPreviewMemoryLock.lock();
    mHalCamCtrl->releaseHeapMem(&mHalCamCtrl->mNoDispPreviewMemory);
    memset(&mHalCamCtrl->mNoDispPreviewMemory, 0, sizeof(mHalCamCtrl->mNoDispPreviewMemory));
    mHalCamCtrl->mPreviewMemoryLock.unlock();
    ALOGI(" %s : X ",__FUNCTION__);
    return NO_ERROR;
}

void QCameraStream_preview::notifyROIEvent(fd_roi_t roi)
{
    switch (roi.type) {
    case FD_ROI_TYPE_HEADER:
        {
            mDisplayLock.lock();
            mNumFDRcvd = 0;
            memset(mHalCamCtrl->mFace, 0, sizeof(mHalCamCtrl->mFace));
            mHalCamCtrl->mMetadata.faces = mHalCamCtrl->mFace;
            mHalCamCtrl->mMetadata.number_of_faces = roi.d.hdr.num_face_detected;
            if(mHalCamCtrl->mMetadata.number_of_faces > MAX_ROI)
              mHalCamCtrl->mMetadata.number_of_faces = MAX_ROI;
            mDisplayLock.unlock();

            if (mHalCamCtrl->mMetadata.number_of_faces == 0) {
                // Clear previous faces
                if (mHalCamCtrl->mDataCb && (mHalCamCtrl->mMsgEnabled & CAMERA_MSG_PREVIEW_METADATA)){
                    ALOGE("%s: Face detection RIO callback", __func__);
                    mHalCamCtrl->mDataCb(CAMERA_MSG_PREVIEW_METADATA, NULL, 0,
                        &mHalCamCtrl->mMetadata, mHalCamCtrl->mCallbackCookie);
                }
            }
        }
        break;
    case FD_ROI_TYPE_DATA:
        {
            mDisplayLock.lock();
            int idx = roi.d.data.idx;
            if (idx >= mHalCamCtrl->mMetadata.number_of_faces) {
                mDisplayLock.unlock();
                ALOGE("%s: idx %d out of boundary %d",
                      __func__, idx, mHalCamCtrl->mMetadata.number_of_faces);
                break;
            }

            mHalCamCtrl->mFace[idx].id = roi.d.data.face.id;
            mHalCamCtrl->mFace[idx].score = roi.d.data.face.score;

            // top
            mHalCamCtrl->mFace[idx].rect[0] =
               roi.d.data.face.face_boundary.x*2000/mHalCamCtrl->mDimension.display_width - 1000;
            //right
            mHalCamCtrl->mFace[idx].rect[1] =
               roi.d.data.face.face_boundary.y*2000/mHalCamCtrl->mDimension.display_height - 1000;
            //bottom
            mHalCamCtrl->mFace[idx].rect[2] =  mHalCamCtrl->mFace[idx].rect[0] +
               roi.d.data.face.face_boundary.dx*2000/mHalCamCtrl->mDimension.display_width;
            //left
            mHalCamCtrl->mFace[idx].rect[3] = mHalCamCtrl->mFace[idx].rect[1] +
               roi.d.data.face.face_boundary.dy*2000/mHalCamCtrl->mDimension.display_height;

            // Center of left eye
            mHalCamCtrl->mFace[idx].left_eye[0] =
              roi.d.data.face.left_eye_center[0]*2000/mHalCamCtrl->mDimension.display_width - 1000;
            mHalCamCtrl->mFace[idx].left_eye[1] =
              roi.d.data.face.left_eye_center[1]*2000/mHalCamCtrl->mDimension.display_height - 1000;

            // Center of right eye
            mHalCamCtrl->mFace[idx].right_eye[0] =
              roi.d.data.face.right_eye_center[0]*2000/mHalCamCtrl->mDimension.display_width - 1000;
            mHalCamCtrl->mFace[idx].right_eye[1] =
              roi.d.data.face.right_eye_center[1]*2000/mHalCamCtrl->mDimension.display_height - 1000;

            // Center of mouth
            mHalCamCtrl->mFace[idx].mouth[0] =
              roi.d.data.face.mouth_center[0]*2000/mHalCamCtrl->mDimension.display_width - 1000;
            mHalCamCtrl->mFace[idx].mouth[1] =
              roi.d.data.face.mouth_center[1]*2000/mHalCamCtrl->mDimension.display_height - 1000;

            mHalCamCtrl->mFace[idx].smile_degree = roi.d.data.face.smile_degree;
            mHalCamCtrl->mFace[idx].smile_score = roi.d.data.face.smile_confidence;
            mHalCamCtrl->mFace[idx].blink_detected = roi.d.data.face.blink_detected;
            mHalCamCtrl->mFace[idx].face_recognised = roi.d.data.face.is_face_recognised;
            mHalCamCtrl->mFace[idx].gaze_angle = roi.d.data.face.gaze_angle;

            /* newly added */
            // upscale by 2 to recover from demaen downscaling
            mHalCamCtrl->mFace[idx].updown_dir = roi.d.data.face.updown_dir * 2;
            mHalCamCtrl->mFace[idx].leftright_dir = roi.d.data.face.leftright_dir * 2;
            mHalCamCtrl->mFace[idx].roll_dir = roi.d.data.face.roll_dir * 2;

            mHalCamCtrl->mFace[idx].leye_blink = roi.d.data.face.left_blink;
            mHalCamCtrl->mFace[idx].reye_blink = roi.d.data.face.right_blink;
            mHalCamCtrl->mFace[idx].left_right_gaze = roi.d.data.face.left_right_gaze;
            mHalCamCtrl->mFace[idx].top_bottom_gaze = roi.d.data.face.top_bottom_gaze;
            ALOGE("%s: Face(%d, %d, %d, %d), leftEye(%d, %d), rightEye(%d, %d), mouth(%d, %d), smile(%d, %d), face_recg(%d)", __func__,
               mHalCamCtrl->mFace[idx].rect[0],  mHalCamCtrl->mFace[idx].rect[1],
               mHalCamCtrl->mFace[idx].rect[2],  mHalCamCtrl->mFace[idx].rect[3],
               mHalCamCtrl->mFace[idx].left_eye[0], mHalCamCtrl->mFace[idx].left_eye[1],
               mHalCamCtrl->mFace[idx].right_eye[0], mHalCamCtrl->mFace[idx].right_eye[1],
               mHalCamCtrl->mFace[idx].mouth[0], mHalCamCtrl->mFace[idx].mouth[1],
               mHalCamCtrl->mFace[idx].smile_degree, mHalCamCtrl->mFace[idx].smile_score,
               mHalCamCtrl->mFace[idx].face_recognised);
            ALOGE("%s: gaze(%d, %d, %d), updown(%d), leftright(%d), roll(%d), blink(%d, %d, %d)", __func__,
               mHalCamCtrl->mFace[idx].gaze_angle,  mHalCamCtrl->mFace[idx].left_right_gaze,
               mHalCamCtrl->mFace[idx].top_bottom_gaze,  mHalCamCtrl->mFace[idx].updown_dir,
               mHalCamCtrl->mFace[idx].leftright_dir, mHalCamCtrl->mFace[idx].roll_dir,
               mHalCamCtrl->mFace[idx].blink_detected,
               mHalCamCtrl->mFace[idx].leye_blink, mHalCamCtrl->mFace[idx].reye_blink);

             mNumFDRcvd++;
             mDisplayLock.unlock();

             if (mNumFDRcvd == mHalCamCtrl->mMetadata.number_of_faces) {
                 if (mHalCamCtrl->mDataCb && (mHalCamCtrl->mMsgEnabled & CAMERA_MSG_PREVIEW_METADATA)){
                     ALOGE("%s: Face detection RIO callback with %d faces detected (score=%d)", __func__, mNumFDRcvd, mHalCamCtrl->mFace[idx].score);
                     mHalCamCtrl->mDataCb(CAMERA_MSG_PREVIEW_METADATA, NULL,
                                          0, &mHalCamCtrl->mMetadata, mHalCamCtrl->mCallbackCookie);
                 }
             }
        }
        break;
    }
}

status_t QCameraStream_preview::initDisplayBuffers()
{
  status_t ret = NO_ERROR;
  uint8_t num_planes = mFrameOffsetInfo.num_planes;
  uint32_t planes[VIDEO_MAX_PLANES];

  ALOGI("%s:BEGIN",__func__);
  memset(&mHalCamCtrl->mMetadata, 0, sizeof(camera_frame_metadata_t));
  mHalCamCtrl->mPreviewMemoryLock.lock();
  memset(&mHalCamCtrl->mPreviewMemory, 0, sizeof(mHalCamCtrl->mPreviewMemory));
  mHalCamCtrl->mPreviewMemoryLock.unlock();
  memset(mNotifyBuffer, 0, sizeof(mNotifyBuffer));

  ret = getBufferFromSurface();
  if(ret != NO_ERROR) {
    ALOGE("%s: cannot get memory from surface texture client, ret = %d", __func__, ret);
    return ret;
  }

  /* set 4 buffers for display */
  mHalCamCtrl->mPreviewMemoryLock.lock();
  for(int i=0; i < num_planes; i++)
      planes[i] = mFrameOffsetInfo.mp[i].len;
  memset(mDisplayBuf, 0, sizeof(mm_camera_buf_def_t) * 2 * PREVIEW_BUFFER_COUNT);
  /*allocate memory for the buffers*/
  for(int i = 0; i < mNumBuffers; i++){
	  if (mHalCamCtrl->mPreviewMemory.private_buffer_handle[i] == NULL)
		  continue;
	  mHalCamCtrl->mPreviewMemory.addr_offset[i] =
	      mHalCamCtrl->mPreviewMemory.private_buffer_handle[i]->offset;
    ALOGD("%s: idx = %d, fd = %d, size = %d, cbcr_offset = %d, y_offset = %d, "
      "offset = %d, vaddr = 0x%lx", __func__, i, mHalCamCtrl->mPreviewMemory.private_buffer_handle[i]->fd,
      mHalCamCtrl->mPreviewMemory.private_buffer_handle[i]->size,
      planes[0], 0,
      mHalCamCtrl->mPreviewMemory.addr_offset[i],
      (long unsigned int)mHalCamCtrl->mPreviewMemory.camera_memory[i]->data);
    mDisplayBuf[i].num_planes = num_planes;

    /* Plane 0 needs to be set seperately. Set other planes
     * in a loop. */
    mDisplayBuf[i].planes[0].length = planes[0];
    mDisplayBuf[i].planes[0].m.userptr = mHalCamCtrl->mPreviewMemory.private_buffer_handle[i]->fd;
    mDisplayBuf[i].planes[0].data_offset = mFrameOffsetInfo.mp[0].offset;
    mDisplayBuf[i].planes[0].reserved[0] =0;// mHalCamCtrl->mPreviewMemory.addr_offset[i]; //     mDisplayBuf.preview.buf.mp[i].frame_offset;
    for (int j = 1; j < num_planes; j++) {
      mDisplayBuf[i].planes[j].length = planes[j];
      mDisplayBuf[i].planes[j].m.userptr = mHalCamCtrl->mPreviewMemory.private_buffer_handle[i]->fd;
      mDisplayBuf[i].planes[j].data_offset = mFrameOffsetInfo.mp[j].offset;
      mDisplayBuf[i].planes[j].reserved[0] =
        mDisplayBuf[i].planes[j-1].reserved[0] +
        mDisplayBuf[i].planes[j-1].length;
    }

    for (int j = 0; j < num_planes; j++)
      ALOGD("Planes: %d length: %d userptr: %lu offset: %d\n", j,
        mDisplayBuf[i].planes[j].length,
        mDisplayBuf[i].planes[j].m.userptr,
        mDisplayBuf[i].planes[j].reserved[0]);

    mDisplayBuf[i].fd = mHalCamCtrl->mPreviewMemory.private_buffer_handle[i]->fd;
    ALOGD("DEBUG2:Display buf[%d] fd:%d",i,mDisplayBuf[i].fd);
    mDisplayBuf[i].frame_len = mFrameOffsetInfo.frame_len;
    mDisplayBuf[i].buffer = (void *)mHalCamCtrl->mPreviewMemory.camera_memory[i]->data;
    mDisplayBuf[i].mem_info = (void *)&mHalCamCtrl->mPreviewMemory.mem_info[i];
  }/*end of for loop*/

 /* register the streaming buffers for the channel*/
  mHalCamCtrl->mPreviewMemoryLock.unlock();
  ALOGI("%s:END",__func__);
  return NO_ERROR;

error:
    mHalCamCtrl->mPreviewMemoryLock.unlock();
    putBufferToSurface();

    ALOGV("%s: X", __func__);
    return ret;
}

status_t QCameraStream_preview::initPreviewOnlyBuffers()
{
    status_t ret = NO_ERROR;
    uint8_t num_planes = mFrameOffsetInfo.num_planes;
    uint32_t planes[VIDEO_MAX_PLANES];

    ALOGI("%s:BEGIN",__func__);
    memset(&mHalCamCtrl->mMetadata, 0, sizeof(camera_frame_metadata_t));
    mHalCamCtrl->mPreviewMemoryLock.lock();
    memset(&mHalCamCtrl->mNoDispPreviewMemory, 0, sizeof(mHalCamCtrl->mNoDispPreviewMemory));
    mHalCamCtrl->mPreviewMemoryLock.unlock();

    memset(mNotifyBuffer, 0, sizeof(mNotifyBuffer));
    memset(mDisplayBuf, 0, sizeof(mDisplayBuf));

    ret = getBufferNoDisplay( );
    if(ret != NO_ERROR) {
        ALOGE("%s: cannot get memory from surface texture client, ret = %d", __func__, ret);
        return ret;
    }

    ALOGI("%s:END",__func__);
    return NO_ERROR;
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
  cam_ctrl_dimension_t dim;
  int file_fd;
  int rc = 0;
  int len;
  unsigned long addr;
  unsigned long * tmp = (unsigned long *)newFrame->buffer;
  addr = *tmp;
  status_t ret = p_mm_ops->ops->get_parm(mCameraHandle,
                 MM_CAMERA_PARM_DIMENSION, &dim);

  w = dim.display_width;
  h = dim.display_height;
  len = (w * h)*3/2;
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

status_t QCameraStream_preview::processPreviewFrameWithDisplay(mm_camera_super_buf_t *frame)
{
  ALOGV("%s",__func__);
  int err = 0;
  int msgType = 0;
  int i;
  camera_memory_t *data = NULL;
  camera_frame_metadata_t *metadata = NULL;
  int preview_buf_idx = frame->bufs[0]->buf_idx;

  if(!mActive) {
    ALOGE("Preview Stopped. Returning callback");
    return NO_ERROR;
  }

  if(mHalCamCtrl==NULL) {
    ALOGE("%s: X: HAL control object not set",__func__);
    /*Call buf done*/
    return BAD_VALUE;
  }

  if(mFirstFrameRcvd == false) {
      mFirstFrameRcvd = true;
  }

  if (UNLIKELY(mHalCamCtrl->mDebugFps)) {
      mHalCamCtrl->debugShowPreviewFPS();
  }
  mHalCamCtrl->dumpFrameToFile(frame->bufs[0], HAL_DUMP_FRM_PREVIEW);

  mHalCamCtrl->mPreviewMemoryLock.lock();
  mNotifyBuffer[preview_buf_idx] = *frame;

  ALOGD("<DEBUG2>: Received Frame fd:%d placed in index:%d db[index].fd:%d",
        frame->bufs[0]->fd, preview_buf_idx,
        mHalCamCtrl->mPreviewMemory.private_buffer_handle[preview_buf_idx]->fd);

  ALOGI("Enqueue buf handle %p\n",
  mHalCamCtrl->mPreviewMemory.buffer_handle[preview_buf_idx]);
  ALOGD("%s: camera call genlock_unlock", __FUNCTION__);
  if (BUFFER_LOCKED == mHalCamCtrl->mPreviewMemory.local_flag[preview_buf_idx]) {
    ALOGD("%s: genlock_unlock_buffer hdl =%p", __FUNCTION__, (*mHalCamCtrl->mPreviewMemory.buffer_handle[preview_buf_idx]));
      if (GENLOCK_FAILURE == genlock_unlock_buffer((native_handle_t*)
	            (*mHalCamCtrl->mPreviewMemory.buffer_handle[preview_buf_idx]))) {
            ALOGE("%s: genlock_unlock_buffer failed", __FUNCTION__);
      } else {
          mHalCamCtrl->mPreviewMemory.local_flag[preview_buf_idx] = BUFFER_UNLOCKED;
      }
    } else {
        ALOGE("%s: buffer to be enqueued is not locked", __FUNCTION__);
  }

  mHalCamCtrl->cache_ops(&mHalCamCtrl->mPreviewMemory.mem_info[preview_buf_idx],
                         (void *)mHalCamCtrl->mPreviewMemory.camera_memory[preview_buf_idx]->data,
                         ION_IOC_CLEAN_CACHES);
  mHalCamCtrl->cache_ops(&mHalCamCtrl->mPreviewMemory.mem_info[preview_buf_idx],
                         (void *)mHalCamCtrl->mPreviewMemory.camera_memory[preview_buf_idx]->data,
                         ION_IOC_CLEAN_INV_CACHES);

  if(mHFRFrameSkip == 1)
  {
      ALOGE("In HFR Frame skip");
      const char *str = mHalCamCtrl->mParameters.get(
                          QCameraParameters::KEY_QC_VIDEO_HIGH_FRAME_RATE);
      if(str != NULL){
      int is_hfr_off = 0;
      mHFRFrameCnt++;
      if(!strcmp(str, QCameraParameters::VIDEO_HFR_OFF)) {
          is_hfr_off = 1;
          err = mHalCamCtrl->mPreviewWindow->enqueue_buffer(mHalCamCtrl->mPreviewWindow,
            (buffer_handle_t *)mHalCamCtrl->mPreviewMemory.buffer_handle[preview_buf_idx]);
      } else if (!strcmp(str, QCameraParameters::VIDEO_HFR_2X)) {
          mHFRFrameCnt %= 2;
      } else if (!strcmp(str, QCameraParameters::VIDEO_HFR_3X)) {
          mHFRFrameCnt %= 3;
      } else if (!strcmp(str, QCameraParameters::VIDEO_HFR_4X)) {
          mHFRFrameCnt %= 4;
      }
      if(mHFRFrameCnt == 0)
          err = mHalCamCtrl->mPreviewWindow->enqueue_buffer(mHalCamCtrl->mPreviewWindow,
            (buffer_handle_t *)mHalCamCtrl->mPreviewMemory.buffer_handle[preview_buf_idx]);
      else if(!is_hfr_off)
          err = mHalCamCtrl->mPreviewWindow->cancel_buffer(mHalCamCtrl->mPreviewWindow,
            (buffer_handle_t *)mHalCamCtrl->mPreviewMemory.buffer_handle[preview_buf_idx]);
      } else
          err = mHalCamCtrl->mPreviewWindow->enqueue_buffer(mHalCamCtrl->mPreviewWindow,
            (buffer_handle_t *)mHalCamCtrl->mPreviewMemory.buffer_handle[preview_buf_idx]);
  } else {
      err = mHalCamCtrl->mPreviewWindow->enqueue_buffer(mHalCamCtrl->mPreviewWindow,
          (buffer_handle_t *)mHalCamCtrl->mPreviewMemory.buffer_handle[preview_buf_idx]);
  }
  if(err != 0) {
    ALOGE("%s: enqueue_buffer failed, err = %d", __func__, err);
  } else {
   ALOGD("%s: enqueue_buffer hdl=%p", __func__, *mHalCamCtrl->mPreviewMemory.buffer_handle[preview_buf_idx]);
    mHalCamCtrl->mPreviewMemory.local_flag[preview_buf_idx] = BUFFER_NOT_OWNED;
  }
  buffer_handle_t *buffer_handle = NULL;
  int tmp_stride = 0;
  err = mHalCamCtrl->mPreviewWindow->dequeue_buffer(mHalCamCtrl->mPreviewWindow,
              &buffer_handle, &tmp_stride);
  if (err == NO_ERROR && buffer_handle != NULL) {

    ALOGD("%s: dequed buf hdl =%p", __func__, *buffer_handle);
    for(i = 0; i < mHalCamCtrl->mPreviewMemory.buffer_count; i++) {
        if(mHalCamCtrl->mPreviewMemory.buffer_handle[i] == buffer_handle) {
          ALOGE("<DEBUG2>:Found buffer in idx:%d",i);
          mHalCamCtrl->mPreviewMemory.local_flag[i] = BUFFER_UNLOCKED;
          break;
        }
    }
     if (i < mHalCamCtrl->mPreviewMemory.buffer_count ) {
      err = mHalCamCtrl->mPreviewWindow->lock_buffer(mHalCamCtrl->mPreviewWindow, buffer_handle);
      ALOGD("%s: camera call genlock_lock: hdl =%p", __func__, *buffer_handle);
      if (GENLOCK_FAILURE == genlock_lock_buffer((native_handle_t*)(*buffer_handle), GENLOCK_WRITE_LOCK,
                                                 GENLOCK_MAX_TIMEOUT)) {
            ALOGE("%s: genlock_lock_buffer(WRITE) failed", __func__);
      } else  {
        mHalCamCtrl->mPreviewMemory.local_flag[i] = BUFFER_LOCKED;

        if(MM_CAMERA_OK != p_mm_ops->ops->qbuf(mCameraHandle, mChannelId, mNotifyBuffer[i].bufs[0])) {

            ALOGE("BUF DONE FAILED");
        }
        mHalCamCtrl->cache_ops((QCameraHalMemInfo_t *)(mNotifyBuffer[i].bufs[0]->mem_info),
                               mNotifyBuffer[i].bufs[0]->buffer,
                               ION_IOC_INV_CACHES);
        }
     }
  } else
      ALOGE("%s: enqueue_buffer idx = %d, no free buffer from display now", __func__, frame->bufs[0]->buf_idx);
  /* Save the last displayed frame. We'll be using it to fill the gap between
     when preview stops and postview start during snapshot.*/
  mLastQueuedFrame = &(mDisplayBuf[preview_buf_idx]);
  mHalCamCtrl->mPreviewMemoryLock.unlock();

  nsecs_t timeStamp = seconds_to_nanoseconds(frame->bufs[0]->ts.tv_sec) ;
  timeStamp += frame->bufs[0]->ts.tv_nsec;
  ALOGD("Message enabled = 0x%x", mHalCamCtrl->mMsgEnabled);

  camera_memory_t *previewMem = NULL;

  if (mHalCamCtrl->mDataCb != NULL) {
       ALOGD("%s: mMsgEnabled =0x%x, preview format =%d", __func__,
            mHalCamCtrl->mMsgEnabled, mHalCamCtrl->mPreviewFormat);
      //Sending preview callback if corresponding Msgs are enabled
      if(mHalCamCtrl->mMsgEnabled & CAMERA_MSG_PREVIEW_FRAME) {
          ALOGE("Q%s: PCB callback enabled", __func__);
          msgType |=  CAMERA_MSG_PREVIEW_FRAME;
          int previewBufSize;
          /* The preview buffer size sent back in the callback should be (width*height*bytes_per_pixel)
           * As all preview formats we support, use 12 bits per pixel, buffer size = previewWidth * previewHeight * 3/2.
           * We need to put a check if some other formats are supported in future. (punits) */
          if ((mHalCamCtrl->mPreviewFormat == CAMERA_YUV_420_NV21) ||
              (mHalCamCtrl->mPreviewFormat == CAMERA_YUV_420_NV12) ||
              (mHalCamCtrl->mPreviewFormat == CAMERA_YUV_420_YV12))
          {
              if(mHalCamCtrl->mPreviewFormat == CAMERA_YUV_420_YV12) {
                  previewBufSize = ((mHalCamCtrl->mPreviewWidth+15)/16) * 16 * mHalCamCtrl->mPreviewHeight +
                                   ((mHalCamCtrl->mPreviewWidth/2+15)/16) * 16* mHalCamCtrl->mPreviewHeight;
              } else {
                  previewBufSize = mHalCamCtrl->mPreviewWidth * mHalCamCtrl->mPreviewHeight * 3/2;
              }
              if(previewBufSize != mHalCamCtrl->mPreviewMemory.private_buffer_handle[preview_buf_idx]->size) {
                  previewMem = mHalCamCtrl->mGetMemory(mHalCamCtrl->mPreviewMemory.private_buffer_handle[preview_buf_idx]->fd,
                                                       previewBufSize, 1, mHalCamCtrl->mCallbackCookie);
                  if (!previewMem || !previewMem->data) {
                      ALOGE("%s: mGetMemory failed.\n", __func__);
                  } else {
                      data = previewMem;
                  }
              } else
                    data = mHalCamCtrl->mPreviewMemory.camera_memory[preview_buf_idx];
          } else {
                data = mHalCamCtrl->mPreviewMemory.camera_memory[preview_buf_idx];
                ALOGE("Invalid preview format, buffer size in preview callback may be wrong.");
          }
      } else {
          data = NULL;
      }
      if(msgType && mActive) {
          mHalCamCtrl->mDataCb(msgType, data, 0, metadata, mHalCamCtrl->mCallbackCookie);
      }
      if (previewMem) {
          previewMem->release(previewMem);
      }
	  ALOGD("end of cb");
  } else {
    ALOGD("%s PCB is not enabled", __func__);
  }
  if(mHalCamCtrl->mDataCbTimestamp != NULL && mVFEOutputs == 1)
  {
      int flagwait = 1;
      if(mHalCamCtrl->mStartRecording == true &&
              ( mHalCamCtrl->mMsgEnabled & CAMERA_MSG_VIDEO_FRAME))
      {
        if (mHalCamCtrl->mStoreMetaDataInFrame)
        {
          if(mHalCamCtrl->mRecordingMemory.metadata_memory[preview_buf_idx])
          {
              flagwait = 1;
              mHalCamCtrl->mDataCbTimestamp(timeStamp, CAMERA_MSG_VIDEO_FRAME,
                                            mHalCamCtrl->mRecordingMemory.metadata_memory[preview_buf_idx],
                                            0, mHalCamCtrl->mCallbackCookie);
          }else
              flagwait = 0;
      }
      else
      {
          mHalCamCtrl->mDataCbTimestamp(timeStamp, CAMERA_MSG_VIDEO_FRAME,
                                        mHalCamCtrl->mPreviewMemory.camera_memory[preview_buf_idx],
                                        0, mHalCamCtrl->mCallbackCookie);
      }

      if(flagwait){
          Mutex::Autolock rLock(&mHalCamCtrl->mRecordFrameLock);
          if (mHalCamCtrl->mReleasedRecordingFrame != true) {
              mHalCamCtrl->mRecordWait.wait(mHalCamCtrl->mRecordFrameLock);
          }
          mHalCamCtrl->mReleasedRecordingFrame = false;
      }
      }
  }
  return NO_ERROR;
}


status_t QCameraStream_preview::processPreviewFrameWithOutDisplay(
  mm_camera_super_buf_t *frame)
{
  ALOGV("%s",__func__);
  int err = 0;
  int msgType = 0;
  int i;
  camera_memory_t *data = NULL;
  camera_frame_metadata_t *metadata = NULL;
  int preview_buf_idx = frame->bufs[0]->buf_idx;

  if(mHalCamCtrl==NULL) {
    ALOGE("%s: X: HAL control object not set",__func__);
    /*Call buf done*/
    return BAD_VALUE;
  }

  if (UNLIKELY(mHalCamCtrl->mDebugFps)) {
      mHalCamCtrl->debugShowPreviewFPS();
  }
  mHalCamCtrl->dumpFrameToFile(frame->bufs[0], HAL_DUMP_FRM_PREVIEW);

  mHalCamCtrl->mPreviewMemoryLock.lock();
  mNotifyBuffer[preview_buf_idx] = *frame;

  /* Save the last displayed frame. We'll be using it to fill the gap between
     when preview stops and postview start during snapshot.*/
  mLastQueuedFrame = &(mDisplayBuf[frame->bufs[0]->buf_idx]);
  mHalCamCtrl->mPreviewMemoryLock.unlock();

  ALOGD("Message enabled = 0x%x", mHalCamCtrl->mMsgEnabled);

  mHalCamCtrl->cache_ops(&mHalCamCtrl->mNoDispPreviewMemory.mem_info[preview_buf_idx],
                         (void *)mHalCamCtrl->mNoDispPreviewMemory.camera_memory[preview_buf_idx]->data,
                         ION_IOC_CLEAN_CACHES);

  if (mHalCamCtrl->mDataCb != NULL) {
      //Sending preview callback if corresponding Msgs are enabled
      if(mHalCamCtrl->mMsgEnabled & CAMERA_MSG_PREVIEW_FRAME) {
          msgType |=  CAMERA_MSG_PREVIEW_FRAME;
          int previewBufSize;
          /* For CTS : Forcing preview memory buffer lenth to be
             'previewWidth * previewHeight * 3/2'.
              Needed when gralloc allocated extra memory.*/
          //Can add this check for other formats as well.
        data = mHalCamCtrl->mNoDispPreviewMemory.camera_memory[preview_buf_idx];//mPreviewHeap->mBuffers[frame->bufs[0]->buf_idx];
      } else {
          data = NULL;
      }

      if(mHalCamCtrl->mMsgEnabled & CAMERA_MSG_PREVIEW_METADATA){
          msgType  |= CAMERA_MSG_PREVIEW_METADATA;
          metadata = &mHalCamCtrl->mMetadata;
      } else {
          metadata = NULL;
      }
      if(mActive && msgType) {
          mHalCamCtrl->mDataCb(msgType, data, 0, metadata, mHalCamCtrl->mCallbackCookie);
      }

      if(MM_CAMERA_OK !=p_mm_ops->ops->qbuf(mCameraHandle, mChannelId, mNotifyBuffer[preview_buf_idx].bufs[0])) {
          ALOGE("BUF DONE FAILED");
      }
      mHalCamCtrl->cache_ops((QCameraHalMemInfo_t *)(mNotifyBuffer[preview_buf_idx].bufs[0]->mem_info),
                             mNotifyBuffer[preview_buf_idx].bufs[0]->buffer,
                             ION_IOC_INV_CACHES);
      ALOGD("end of cb");
  }

  return NO_ERROR;
}

status_t QCameraStream_preview::processPreviewFrame (
  mm_camera_super_buf_t *frame)
{
  if (mHalCamCtrl->isNoDisplayMode()) {
    return processPreviewFrameWithOutDisplay(frame);
  } else {
    return processPreviewFrameWithDisplay(frame);
  }
}

// ---------------------------------------------------------------------------
// QCameraStream_preview
// ---------------------------------------------------------------------------

QCameraStream_preview::QCameraStream_preview(uint32_t CameraHandle,
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
                 camCtrl),
    mLastQueuedFrame(NULL),
    mNumFDRcvd(0)
  {
  }
// ---------------------------------------------------------------------------
// QCameraStream_preview
// ---------------------------------------------------------------------------

QCameraStream_preview::~QCameraStream_preview() {
    ALOGV("%s: E", __func__);
    release();
    ALOGV("%s: X", __func__);

}

// ---------------------------------------------------------------------------
// QCameraStream_preview
// ---------------------------------------------------------------------------
void QCameraStream_preview::release() {
    streamOff(0);
    deinitStream();
}

void *QCameraStream_preview::getLastQueuedFrame(void)
{
    return mLastQueuedFrame;
}

int32_t QCameraStream_preview::setCrop()
{
    int32_t rc = QCameraStream::setCrop();
    if (0 == rc) {
        if (!mHalCamCtrl->mSmoothZoomRunning && mHalCamCtrl->mPreviewWindow) {
            rc = mHalCamCtrl->mPreviewWindow->set_crop(
                            mHalCamCtrl->mPreviewWindow,
                            mCrop.offset_x,
                            mCrop.offset_y,
                            mCrop.offset_x + mCrop.width,
                            mCrop.offset_y + mCrop.height);
        }
    }
    return rc;
}

int QCameraStream_preview::getBuf(mm_camera_frame_len_offset *frame_offset_info,
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

    memcpy(&mFrameOffsetInfo, frame_offset_info, sizeof(mFrameOffsetInfo));
    if (mHalCamCtrl->isNoDisplayMode()) {
        ret = initPreviewOnlyBuffers();
    } else {
        ret = initDisplayBuffers();
    }
    ALOGD("Debug : %s : initDisplayBuffers",__func__);
    for(int i = 0; i < num_bufs; i++) {
        bufs[i] = mDisplayBuf[i];
        initial_reg_flag[i] = true;
    }

    ALOGV("%s: X - ret = %d ", __func__, ret);
    return ret;
}

int QCameraStream_preview::putBuf(uint8_t num_bufs, mm_camera_buf_def_t *bufs)
{
    int ret = 0;
    ALOGE(" %s : E ", __func__);
    if (mHalCamCtrl->isNoDisplayMode()) {
        ret = freeBufferNoDisplay();
    } else {
        ret = putBufferToSurface();
    }
    ALOGI(" %s : X ",__func__);
    return ret;
}

// ---------------------------------------------------------------------------
// No code beyone this line
// ---------------------------------------------------------------------------
}; // namespace android
