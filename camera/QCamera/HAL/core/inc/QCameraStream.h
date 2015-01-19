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

#ifndef ANDROID_HARDWARE_QCAMERA_STREAM_H
#define ANDROID_HARDWARE_QCAMERA_STREAM_H


#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <pthread.h>
#include <semaphore.h>


#include "QCameraHWI.h"
#include "QCameraHWI_Mem.h"
#include "mm_camera_interface.h"
#include "mm_jpeg_interface.h"

extern "C" {
#define DEFAULT_STREAM_WIDTH 320
#define DEFAULT_STREAM_HEIGHT 240
#define DEFAULT_LIVESHOT_WIDTH 2592
#define DEFAULT_LIVESHOT_HEIGHT 1944

#define MM_CAMERA_CH_PREVIEW_MASK    (0x01 << MM_CAMERA_CH_PREVIEW)
#define MM_CAMERA_CH_VIDEO_MASK      (0x01 << MM_CAMERA_CH_VIDEO)
#define MM_CAMERA_CH_SNAPSHOT_MASK   (0x01 << MM_CAMERA_CH_SNAPSHOT)
#define MM_CAMERA_CH_RDI_MASK        (0x01 << MM_CAMERA_CH_RDI)

} /* extern C*/

namespace android {

class QCameraHardwareInterface;

class QCameraStream { //: public virtual RefBase

public:
    bool mInit;
    bool mActive;

    uint32_t mCameraHandle;
    uint32_t mChannelId;
    uint32_t mStreamId;
    uint32_t mWidth;
    uint32_t mHeight;
    uint32_t mFormat;
    uint8_t mNumBuffers;
    mm_camera_frame_len_offset mFrameOffsetInfo;
    mm_camera_vtbl_t *p_mm_ops;
    mm_camera_img_mode mExtImgMode;
    uint8_t m_flag_no_cb;
    uint8_t m_flag_stream_on;
    image_crop_t mCrop;
    QCameraHardwareInterface*  mHalCamCtrl;

    void setResolution(mm_camera_dimension_t *res);
    bool isResolutionSame(mm_camera_dimension_t *res);
    void getResolution(mm_camera_dimension_t *res);
    virtual void        release();

    status_t setFormat();

    //static status_t     openChannel(mm_camera_t *, mm_camera_channel_type_t ch_type);
    void dataCallback(mm_camera_super_buf_t *bufs);
    virtual int32_t streamOn();
    virtual int32_t streamOff(bool aSync);
    virtual status_t    initStream(uint8_t no_cb_needed, uint8_t stream_on);
    virtual status_t    deinitStream();
    virtual void releaseRecordingFrame(const void *opaque)
    {
      ;
    }
    virtual sp<IMemoryHeap> getHeap() const{return NULL;}
    virtual sp<IMemoryHeap> getRawHeap() const {return NULL;}
    virtual void *getLastQueuedFrame(void){return NULL;}
    virtual void notifyROIEvent(fd_roi_t roi) {;}
    virtual void notifyWDenoiseEvent(cam_ctrl_status_t status, void * cookie) {;}
    virtual void notifyHdrEvent(cam_ctrl_status_t status, void * cookie){};
    virtual int32_t setCrop();
    virtual int getBuf(mm_camera_frame_len_offset *frame_offset_info,
                       uint8_t num_bufs,
                       uint8_t *initial_reg_flag,
                       mm_camera_buf_def_t  *bufs) = 0;
    virtual int putBuf(uint8_t num_bufs,
                       mm_camera_buf_def_t *bufs) = 0;

    QCameraStream();
    QCameraStream(uint32_t CameraHandle,
                  uint32_t ChannelId,
                  uint32_t Width,
                  uint32_t Height,
                  uint32_t Format,
                  uint8_t NumBuffers,
                  mm_camera_vtbl_t *mm_ops,
                  mm_camera_img_mode imgmode,
                  camera_mode_t mode,
                  QCameraHardwareInterface* camCtrl);
    virtual ~QCameraStream();

};
/*
*   Record Class
*/
class QCameraStream_record : public QCameraStream {
public:
  void        release() ;

  QCameraStream_record(uint32_t CameraHandle,
                       uint32_t ChannelId,
                       uint32_t Width,
                       uint32_t Height,
                       uint32_t Format,
                       uint8_t NumBuffers,
                       mm_camera_vtbl_t *mm_ops,
                       mm_camera_img_mode imgmode,
                       camera_mode_t mode,
                       QCameraHardwareInterface* camCtrl);
  virtual             ~QCameraStream_record();

  status_t processRecordFrame(mm_camera_super_buf_t *data);
  status_t initEncodeBuffers();
  status_t getBufferInfo(sp<IMemory>& Frame, size_t *alignedSize);

  void releaseRecordingFrame(const void *opaque);
  void debugShowVideoFPS() const;

  void releaseEncodeBuffer();

  int getBuf(mm_camera_frame_len_offset *frame_offset_info,
             uint8_t num_bufs,
             uint8_t *initial_reg_flag,
             mm_camera_buf_def_t  *bufs);
  int putBuf(uint8_t num_bufs,
             mm_camera_buf_def_t *bufs);
  status_t initStream(uint8_t no_cb_needed, uint8_t stream_on);

private:
  bool mDebugFps;
  mm_camera_super_buf_t          mRecordedFrames[MM_CAMERA_MAX_NUM_FRAMES];
  mm_camera_buf_def_t            mRecordBuf[2*VIDEO_BUFFER_COUNT];
  int mJpegMaxSize;
  QCameraStream *mStreamSnap;
};

class QCameraStream_preview : public QCameraStream {
public:
    QCameraStream_preview(uint32_t CameraHandle,
                          uint32_t ChannelId,
                          uint32_t Width,
                          uint32_t Height,
                          uint32_t Format,
                          uint8_t NumBuffers,
                          mm_camera_vtbl_t *mm_ops,
                          mm_camera_img_mode imgmode,
                          camera_mode_t mode,
                          QCameraHardwareInterface* camCtrl);
    virtual             ~QCameraStream_preview();
    void        release() ;
    void *getLastQueuedFrame(void);
    /*init preview buffers with display case*/
    status_t initDisplayBuffers();
    /*init preview buffers without display case*/
    status_t initPreviewOnlyBuffers();
    status_t initStream(uint8_t no_cb_needed, uint8_t stream_on);
    status_t processPreviewFrame(mm_camera_super_buf_t *frame);
    /*init preview buffers with display case*/
    status_t processPreviewFrameWithDisplay(mm_camera_super_buf_t *frame);
    /*init preview buffers without display case*/
    status_t processPreviewFrameWithOutDisplay(mm_camera_super_buf_t *frame);

    void notifyROIEvent(fd_roi_t roi);
    int32_t setCrop();
    int getBuf(mm_camera_frame_len_offset *frame_offset_info,
               uint8_t num_bufs,
               uint8_t *initial_reg_flag,
               mm_camera_buf_def_t  *bufs);
    int putBuf(uint8_t num_bufs,
               mm_camera_buf_def_t *bufs);

    friend class QCameraHardwareInterface;

private:
    /*allocate and free buffers with display case*/
    status_t                 getBufferFromSurface();
    status_t                 putBufferToSurface();

    /*allocate and free buffers without display case*/
    status_t                 getBufferNoDisplay();
    status_t                 freeBufferNoDisplay();

    void                     dumpFrameToFile(mm_camera_buf_def_t* newFrame);
    bool                     mFirstFrameRcvd;

    int8_t                   my_id;
    mm_camera_op_mode_type_t op_mode;
    mm_camera_buf_def_t    *mLastQueuedFrame;
    mm_camera_buf_def_t     mDisplayBuf[2*PREVIEW_BUFFER_COUNT];
    Mutex                   mDisplayLock;
    preview_stream_ops_t   *mPreviewWindow;
    mm_camera_super_buf_t   mNotifyBuffer[2*PREVIEW_BUFFER_COUNT];
    int8_t                  mNumFDRcvd;
    int                     mVFEOutputs;
    int                     mHFRFrameCnt;
    int                     mHFRFrameSkip;
};

class QCameraStream_SnapshotMain : public QCameraStream {
public:
    void        release();
    status_t    initStream(uint8_t no_cb_needed, uint8_t stream_on);
    status_t    initMainBuffers();
    void        deInitMainBuffers();
    mm_camera_buf_def_t mSnapshotStreamBuf[MM_CAMERA_MAX_NUM_FRAMES];
    QCameraStream_SnapshotMain(uint32_t CameraHandle,
                               uint32_t ChannelId,
                               uint32_t Width,
                               uint32_t Height,
                               uint32_t Format,
                               uint8_t NumBuffers,
                               mm_camera_vtbl_t *mm_ops,
                               mm_camera_img_mode imgmode,
                               camera_mode_t mode,
                               QCameraHardwareInterface* camCtrl);
    ~QCameraStream_SnapshotMain();
    int getBuf(mm_camera_frame_len_offset *frame_offset_info,
               uint8_t num_bufs,
               uint8_t *initial_reg_flag,
               mm_camera_buf_def_t  *bufs);
    int putBuf(uint8_t num_bufs,
               mm_camera_buf_def_t *bufs);

private:
    /*Member variables*/
    int mSnapshotState;
};

class QCameraStream_SnapshotThumbnail : public QCameraStream {
public:
    void        release();
    QCameraStream_SnapshotThumbnail(uint32_t CameraHandle,
                                    uint32_t ChannelId,
                                    uint32_t Width,
                                    uint32_t Height,
                                    uint32_t Format,
                                    uint8_t NumBuffers,
                                    mm_camera_vtbl_t *mm_ops,
                                    mm_camera_img_mode imgmode,
                                    camera_mode_t mode,
                                    QCameraHardwareInterface* camCtrl);
    ~QCameraStream_SnapshotThumbnail();
    int getBuf(mm_camera_frame_len_offset *frame_offset_info,
               uint8_t num_bufs,
               uint8_t *initial_reg_flag,
               mm_camera_buf_def_t  *bufs);
    int putBuf(uint8_t num_bufs,
               mm_camera_buf_def_t *bufs);
    status_t initStream(uint8_t no_cb_needed, uint8_t stream_on);

private:
    mm_camera_buf_def_t mPostviewStreamBuf[MM_CAMERA_MAX_NUM_FRAMES];
};

class QCameraStream_Rdi : public QCameraStream {
public:
    void release() ;

    QCameraStream_Rdi(uint32_t CameraHandle,
                      uint32_t ChannelId,
                      uint32_t Width,
                      uint32_t Height,
                      uint32_t Format,
                      uint8_t NumBuffers,
                      mm_camera_vtbl_t *mm_ops,
                      mm_camera_img_mode imgmode,
                      camera_mode_t mode,
                      QCameraHardwareInterface* camCtrl);
    virtual ~QCameraStream_Rdi();
    status_t processRdiFrame (mm_camera_super_buf_t *data);
    int getBuf(mm_camera_frame_len_offset *frame_offset_info,
               uint8_t num_bufs,
               uint8_t *initial_reg_flag,
               mm_camera_buf_def_t  *bufs);
    int putBuf(uint8_t num_bufs,
               mm_camera_buf_def_t *bufs);
    status_t initStream(uint8_t no_cb_needed, uint8_t stream_on);

    friend class QCameraHardwareInterface;

private:
    void dumpFrameToFile(mm_camera_buf_def_t* newFrame);

    int8_t my_id;
    mm_camera_op_mode_type_t op_mode;
    mm_camera_buf_def_t mRdiBuf[PREVIEW_BUFFER_COUNT];
};

}; // namespace android

#endif
