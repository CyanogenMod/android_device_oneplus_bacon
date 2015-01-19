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

#define ALOG_NDEBUG 0
#define ALOG_NIDEBUG 0
#define LOG_TAG __FILE__
#include <utils/Log.h>

#include "QCameraHWI.h"
#include "QCameraStream.h"

/* QCameraStream class implementation goes here*/
/* following code implement the control logic of this class*/

namespace android {

// ---------------------------------------------------------------------------
// QCameraStream
// ---------------------------------------------------------------------------

void stream_cb_routine(mm_camera_super_buf_t *bufs,
                       void *userdata)
{
    ALOGD("%s E ", __func__);
    QCameraStream *p_obj=(QCameraStream*) userdata;
    ALOGD("DEBUG4:ExtMode:%d,streamid:%d",p_obj->mExtImgMode,bufs->bufs[0]->stream_id);
    switch(p_obj->mExtImgMode)
    {
        case MM_CAMERA_PREVIEW:
            ALOGD("%s : callback for MM_CAMERA_PREVIEW", __func__);
            ((QCameraStream_preview *)p_obj)->processPreviewFrame(bufs);
            break;
        case MM_CAMERA_VIDEO:
            ((QCameraStream_record *)p_obj)->processRecordFrame(bufs);
            break;
        case MM_CAMERA_RDI:
            ((QCameraStream_Rdi *)p_obj)->processRdiFrame(bufs);
            break;
        case MM_CAMERA_SNAPSHOT_MAIN:
            break;
        case MM_CAMERA_SNAPSHOT_THUMBNAIL:
            break;
        default:
            break;

    }
    ALOGD("%s X ", __func__);
}

void QCameraStream::dataCallback(mm_camera_super_buf_t *bufs)
{
}


void QCameraStream::setResolution(mm_camera_dimension_t *res)
{
    mWidth = res->width;
    mHeight = res->height;
}
bool QCameraStream::isResolutionSame(mm_camera_dimension_t *res)
{
    if (mWidth != res->width || mHeight != res->height)
        return false;
    else
        return true;
}
void QCameraStream::getResolution(mm_camera_dimension_t *res)
{
    res->width = mWidth;
    res->height = mHeight;
}
int32_t QCameraStream::streamOn()
{
   status_t rc = NO_ERROR;
   ALOGD("%s: mActive = %d, streamid = %d, image_mode = %d",__func__, mActive, mStreamId, mExtImgMode);
   if(mActive){
       ALOGE("%s: Stream:%d is already active",
            __func__,mStreamId);
       return rc;
   }

   if (mInit == true) {
       /* this is the restart case, for now we need config again */
       rc = setFormat();
       ALOGD("%s: config_stream, rc = %d", __func__, rc);
   }
   rc = p_mm_ops->ops->start_streams(mCameraHandle,
                              mChannelId,
                              1,
                              &mStreamId);
   if(rc == NO_ERROR) {
       mActive = true;
   }
   ALOGD("%s: X, mActive = %d, mInit = %d, streamid = %d, image_mode = %d",
         __func__, mActive, mInit, mStreamId, mExtImgMode);
   return rc;
}

int32_t QCameraStream::streamOff(bool isAsyncCmd)
{
    status_t rc = NO_ERROR;
    ALOGD("%s: mActive = %d, streamid = %d, image_mode = %d",__func__, mActive, mStreamId, mExtImgMode);
    if(!mActive) {
        ALOGE("%s: Stream:%d is not active",
              __func__,mStreamId);
        return rc;
    }
    mActive = false;

    rc = p_mm_ops->ops->stop_streams(mCameraHandle,
                              mChannelId,
                              1,
                              &mStreamId);

    ALOGD("%s: X, mActive = %d, mInit = %d, streamid = %d, image_mode = %d",
          __func__, mActive, mInit, mStreamId, mExtImgMode);
    return rc;
}

/* initialize a streaming channel*/
status_t QCameraStream::initStream(uint8_t no_cb_needed, uint8_t stream_on)
{

    int rc = MM_CAMERA_OK;

    /* save local copy */
    m_flag_no_cb = no_cb_needed;
    m_flag_stream_on = stream_on;

    ALOGD("%s: E, mActive = %d, mInit = %d, streamid = %d, image_mode = %d",
          __func__, mActive, mInit, mStreamId, mExtImgMode);

    if(mInit == true) {
        ALOGE("%s: alraedy initted, mActive = %d, mInit = %d, streamid = %d, image_mode = %d",
              __func__, mActive, mInit, mStreamId, mExtImgMode);
        return rc;
    }
    /***********Allocate Stream**************/
    mStreamId = p_mm_ops->ops->add_stream(mCameraHandle,
                        mChannelId,
                        no_cb_needed? NULL :  stream_cb_routine,
                        no_cb_needed? NULL : (void *)this,
                        mExtImgMode,
                        0/*sensor_idx*/);
    if (mStreamId == 0) {
        ALOGE("%s: err in add_stream, mActive = %d, mInit = %d, streamid = %d, image_mode = %d",
              __func__, mActive, mInit, mStreamId, mExtImgMode);
       return BAD_VALUE;
    }

    /***********Config Stream*****************/
    rc = setFormat();
    if(MM_CAMERA_OK != rc) {
        ALOGE("%s: err in config_stream, mActive = %d, streamid = %d, image_mode = %d",
              __func__, mActive, mStreamId, mExtImgMode);
        p_mm_ops->ops->del_stream(mCameraHandle,
                                  mChannelId,
                                  mStreamId);
        return BAD_VALUE;
    }

    mInit=true;
    ALOGE("%s: X, mActive = %d, streamid = %d, image_mode = %d",
          __func__, mActive, mStreamId, mExtImgMode);
    return NO_ERROR;
}

status_t QCameraStream::deinitStream()
{

    int rc = MM_CAMERA_OK;

    ALOGD("%s: E, mActive = %d, mInit = %d, streamid = %d, image_mode = %d",__func__, mActive, mInit, mStreamId, mExtImgMode);

    if(mInit == false) {
        /* stream has not been initted. nop */
        if (mStreamId > 0) {
            ALOGE("%s: bug. mStreamId = %d, mInit = %d", __func__, mStreamId, mInit);
            rc = -1;
        }
        return rc;
    }
    rc= p_mm_ops->ops->del_stream(mCameraHandle,mChannelId,
                              mStreamId);

    ALOGV("%s: X, Stream = %d\n", __func__, mStreamId);
    mInit = false;
    mStreamId = 0;
    ALOGD("%s: X, mActive = %d, mInit = %d, streamid = %d, image_mode = %d",
          __func__, mActive, mInit, mStreamId, mExtImgMode);
    return NO_ERROR;
}

status_t QCameraStream::setFormat()
{
    int rc = MM_CAMERA_OK;
    mm_camera_stream_config_t stream_config;

    ALOGV("%s: E, mActive = %d, streamid = %d, image_mode = %d",__func__, mActive, mStreamId, mExtImgMode);
    memset(&stream_config, 0, sizeof(mm_camera_stream_config_t));

    switch(mExtImgMode)
    {
    case MM_CAMERA_PREVIEW:
            /* Get mFormat */
            rc = p_mm_ops->ops->get_parm(p_mm_ops->camera_handle,
                                MM_CAMERA_PARM_PREVIEW_FORMAT,
                                &mFormat);
            if (MM_CAMERA_OK != rc) {
                ALOGE("%s: error - can't get preview format!", __func__);
                ALOGD("%s: X", __func__);
                return rc;
            }
            break;
        case MM_CAMERA_VIDEO:
            break;
        case MM_CAMERA_SNAPSHOT_MAIN:
            stream_config.fmt.rotation = mHalCamCtrl->getJpegRotation();
            break;
        case MM_CAMERA_SNAPSHOT_THUMBNAIL:
            stream_config.fmt.rotation = mHalCamCtrl->getJpegRotation();
            break;
        case MM_CAMERA_RDI:
            mWidth = 0;
            mHeight = 0;
            mFormat = CAMERA_BAYER_SBGGR10;
        default:
            break;
    }

    stream_config.fmt.fmt = (cam_format_t)mFormat;
    stream_config.fmt.meta_header = MM_CAMEAR_META_DATA_TYPE_DEF;
    stream_config.fmt.width = mWidth;
    stream_config.fmt.height = mHeight;
    stream_config.num_of_bufs = mNumBuffers;
    stream_config.need_stream_on = m_flag_stream_on;
    rc = p_mm_ops->ops->config_stream(mCameraHandle,
                              mChannelId,
                              mStreamId,
                              &stream_config);

    if(mHalCamCtrl->rdiMode != STREAM_IMAGE) {
        mHalCamCtrl->mRdiWidth = stream_config.fmt.width;
        mHalCamCtrl->mRdiHeight = stream_config.fmt.height;
    }

    mWidth = stream_config.fmt.width;
    mHeight = stream_config.fmt.height;
    ALOGD("%s: X",__func__);
    return rc;
}

QCameraStream::QCameraStream (){
    mInit = false;
    mActive = false;
    /* memset*/
    memset(&mCrop, 0, sizeof(mm_camera_rect_t));
    m_flag_no_cb = FALSE;
    m_flag_stream_on = TRUE;
}

QCameraStream::QCameraStream(uint32_t CameraHandle,
                             uint32_t ChannelId,
                             uint32_t Width,
                             uint32_t Height,
                             uint32_t Format,
                             uint8_t NumBuffers,
                             mm_camera_vtbl_t *mm_ops,
                             mm_camera_img_mode imgmode,
                             camera_mode_t mode,
                             QCameraHardwareInterface* camCtrl)
              : mCameraHandle(CameraHandle),
                mChannelId(ChannelId),
                mWidth(Width),
                mHeight(Height),
                mFormat(Format),
                mNumBuffers(NumBuffers),
                p_mm_ops(mm_ops),
                mExtImgMode(imgmode),
                mHalCamCtrl(camCtrl)
{
    mInit = false;
    mActive = false;

    mStreamId = 0;
    m_flag_no_cb = FALSE;
    m_flag_stream_on = TRUE;

    /* memset*/
    memset(&mCrop, 0, sizeof(mm_camera_rect_t));
}

QCameraStream::~QCameraStream ()
{
}

void QCameraStream::release() {
    return;
}

int32_t QCameraStream::setCrop()
{
    mm_camera_rect_t v4l2_crop;
    int32_t rc = 0;
    memset(&v4l2_crop,0,sizeof(v4l2_crop));

    if(!mActive) {
        ALOGE("%s: Stream:%d is not active", __func__, mStreamId);
        return -1;
    }

    rc = p_mm_ops->ops->get_stream_parm(mCameraHandle,
                                   mChannelId,
                                   mStreamId,
                                   MM_CAMERA_STREAM_CROP,
                                   &v4l2_crop);
    ALOGI("%s: Crop info received: %d, %d, %d, %d ",
                                 __func__,
                                 v4l2_crop.left,
                                 v4l2_crop.top,
                                 v4l2_crop.width,
                                 v4l2_crop.height);
    if (rc == 0) {
        mCrop.offset_x = v4l2_crop.left;
        mCrop.offset_y = v4l2_crop.top;
        mCrop.width = v4l2_crop.width;
        mCrop.height = v4l2_crop.height;
    }
    return rc;
}

}; // namespace android
