/*
** Copyright 2008, Google Inc.
** Copyright (c) 2009-2012, The Linux Foundation. All rights reserved.
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


#include <utils/threads.h>

#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <utils/threads.h>


#include "QCameraHWI.h"
extern "C" {
#include <mm_camera_interface.h>
#include <mm_jpeg_interface.h>

#define OPAQUE_BUFFER_COUNT 5
#define STREAM_TABLE_SIZE 8

#define MM_CAMERA_CH_PREVIEW_MASK    (0x01 << MM_CAMERA_CH_PREVIEW)
#define MM_CAMERA_CH_VIDEO_MASK      (0x01 << MM_CAMERA_CH_VIDEO)
#define MM_CAMERA_CH_SNAPSHOT_MASK   (0x01 << MM_CAMERA_CH_SNAPSHOT)

} /* extern C*/

namespace android {

class QCameraHardwareInterface;

class StreamQueue {
private:
    Mutex mQueueLock;
    Condition mQueueWait;
    bool mInitialized;

    //Vector<struct msm_frame *> mContainer;
    Vector<void *> mContainer;
public:
    StreamQueue();
    virtual ~StreamQueue();
    bool enqueue(void *element);
    void flush();
    void* dequeue();
    void init();
    void deinit();
    bool isInitialized();
bool isEmpty();
};

typedef struct {
     int                     buffer_count;
     buffer_handle_t         buffer_handle[MM_CAMERA_MAX_NUM_FRAMES];
     struct private_handle_t *private_buffer_handle[MM_CAMERA_MAX_NUM_FRAMES];
     int                     stride[MM_CAMERA_MAX_NUM_FRAMES];
     uint32_t                addr_offset[MM_CAMERA_MAX_NUM_FRAMES];
     uint8_t                 local_flag[MM_CAMERA_MAX_NUM_FRAMES];
     int                     main_ion_fd[MM_CAMERA_MAX_NUM_FRAMES];
     struct ion_fd_data      ion_info_fd[MM_CAMERA_MAX_NUM_FRAMES];
} QCameraStreamMemory_t;

class QCameraStream {

public:
    bool mInit;
    bool mActive;

    uint32_t mCameraHandle;
    uint32_t mChannelId;
    uint32_t mMmStreamId;
    uint32_t mWidth;
    uint32_t mHeight;
    cam_format_t mFormat;
    uint8_t mNumBuffers;
    mm_camera_frame_len_offset mFrameOffsetInfo;
    mm_camera_vtbl_t *p_mm_ops;
    mm_camera_img_mode mExtImgMode;

    uint8_t mPendingCount;

    pthread_mutex_t mFrameDeliveredMutex;
    pthread_cond_t mFrameDeliveredCond;

    virtual status_t    init();
    virtual status_t    start();
    virtual void        stop();
    virtual void        release();

    status_t setFormat();
    status_t setMode(int enable);

    int getStreamId() {return mStreamId;}
    int getMaxBuffers() {return mMaxBuffers;}

    static QCameraStream *getStreamAtId(int id);
    static QCameraStream *getStreamAtMmId(uint32_t mm_id);
    static void streamOffAll();

    virtual void setHALCameraControl(QCameraHardwareInterface* ctrl);

    virtual int prepareStream() {return NO_ERROR;}
    void onNewRequest();
    void dataCallback(mm_camera_super_buf_t *bufs);
    int32_t streamOn();
    int32_t streamOff(bool aSync);
    virtual status_t initStream(mm_camera_img_mode imgmode, cam_format_t format);
    virtual status_t deinitStream();
    virtual status_t initBuffers(){return NO_ERROR;}
    virtual sp<IMemoryHeap> getRawHeap() const {return NULL;}
    virtual void *getLastQueuedFrame(void){return NULL;}
    virtual status_t processPreviewFrame(mm_camera_super_buf_t *frame){return NO_ERROR;}

    /* Set the ANativeWindow */
    virtual int setPreviewWindow(const camera2_stream_ops_t* window) {return NO_ERROR;}
    virtual void notifyWDenoiseEvent(cam_ctrl_status_t status, void * cookie) {;}
    virtual void resetSnapshotCounters(void ){};
    virtual void initHdrInfoForSnapshot(bool HDR_on, int number_frames, int *exp){};
    virtual void notifyHdrEvent(cam_ctrl_status_t status, void * cookie){};
    virtual status_t receiveRawPicture(mm_camera_super_buf_t* recvd_frame){return NO_ERROR;};

    QCameraStream(uint32_t CameraHandle,
                        uint32_t ChannelId,
                        uint32_t Width,
                        uint32_t Height,
                        mm_camera_vtbl_t *mm_ops,
                        camera_mode_t mode);
    virtual ~QCameraStream();
public:
    QCameraHardwareInterface*  mHalCamCtrl;
    mm_camera_rect_t mCrop;

    camera_mode_t myMode;

    mutable Mutex mLock;
protected:

    uint32_t mStreamId;
    int mMaxBuffers;
    int allocateStreamId();
    int deallocateStreamId(int id);

private:
    static QCameraStream *mStreamTable[STREAM_TABLE_SIZE];

    StreamQueue mBusyQueue;
    StreamQueue mFreeQueue;
    
    QCameraStream();
public:
//     friend void liveshot_callback(mm_camera_ch_data_buf_t *frame,void *user_data);
};

class QCameraStream_preview : public QCameraStream {
public:
    status_t    init();
    status_t    start() ;
    void        stop()  ;
    void        release() ;

    static QCameraStream* createInstance(uint32_t CameraHandle,
                        uint32_t ChannelId,
                        uint32_t Width,
                        uint32_t Height,
                        int requestedFormat,
                        mm_camera_vtbl_t *mm_ops,
                        camera_mode_t mode);
    static void deleteInstance(QCameraStream *p);

    virtual ~QCameraStream_preview();
    void *getLastQueuedFrame(void);
    status_t initBuffers();
    void deinitBuffers();

    virtual int prepareStream();
    virtual status_t processPreviewFrame(mm_camera_super_buf_t *frame);

    int setPreviewWindow(const camera2_stream_ops_t* window);
    int registerStreamBuffers(int num_buffers, buffer_handle_t *buffers);
    friend class QCameraHardwareInterface;

private:
    QCameraStream_preview(); 
    QCameraStream_preview(uint32_t CameraHandle,
                        uint32_t ChannelId,
                        uint32_t Width,
                        uint32_t Height,
                        int requestedFormat,
                        mm_camera_vtbl_t *mm_ops,
                        camera_mode_t mode);

    void                     dumpFrameToFile(mm_camera_buf_def_t* newFrame);

private:
    bool                     mFirstFrameRcvd;
    QCameraStreamMemory_t mPreviewMemory;
    Mutex         mPreviewMemoryLock;

    int8_t                   my_id;
    mm_camera_op_mode_type_t op_mode;
    mm_camera_buf_def_t      *mLastQueuedFrame;
    mm_camera_buf_def_t      *mDisplayBuf;
    Mutex                   mDisplayLock;
    const camera2_stream_ops_t   *mPreviewWindow;
    mm_camera_super_buf_t mNotifyBuffer[16];
    int8_t                  mNumFDRcvd;
    int                     mVFEOutputs;
    int                     mHFRFrameCnt;
    int                     mHFRFrameSkip;
};

class QCameraStream_SnapshotMain : public QCameraStream {
public:
    status_t    init();
    status_t    start();
    void        stop();
    void        release();
    status_t    initMainBuffers();
    void        releaseMainBuffers();
    void        initHdrInfoForSnapshot(bool HDR_on, int number_frames, int *exp);
    void        notifyHdrEvent(cam_ctrl_status_t status, void * cookie);
    static void            deleteInstance(QCameraStream *p);
    status_t receiveRawPicture(mm_camera_super_buf_t* recvd_frame);
    mm_camera_buf_def_t mSnapshotStreamBuf[MM_CAMERA_MAX_NUM_FRAMES];
    static QCameraStream* createInstance(uint32_t CameraHandle,
                        uint32_t ChannelId,
                        uint32_t Width,
                        uint32_t Height,
                        uint32_t Format,
                        uint8_t NumBuffers,
                        mm_camera_vtbl_t *mm_ops,
                        mm_camera_img_mode imgmode,
                        camera_mode_t mode);
    QCameraStream_SnapshotMain(uint32_t CameraHandle,
                        uint32_t ChannelId,
                        uint32_t Width,
                        uint32_t Height,
                        uint32_t Format,
                        uint8_t NumBuffers,
                        mm_camera_vtbl_t *mm_ops,
                        mm_camera_img_mode imgmode,
                        camera_mode_t mode);
    ~QCameraStream_SnapshotMain();

private:
    status_t doHdrProcessing();
    status_t encodeData(mm_camera_super_buf_t* recvd_frame);
    /*Member variables*/
    mm_jpeg_ops_t mJpegHandle;
    uint32_t mJpegClientHandle;
    int mSnapshotState;
    StreamQueue mSnapshotQueue;
};

class QCameraStream_SnapshotThumbnail : public QCameraStream {
public:
    status_t    init();
    status_t    start();
    void        stop();
    void        release();
    status_t    initThumbnailBuffers();
    static QCameraStream* createInstance(uint32_t CameraHandle,
                        uint32_t ChannelId,
                        uint32_t Width,
                        uint32_t Height,
                        uint32_t Format,
                        uint8_t NumBuffers,
                        mm_camera_vtbl_t *mm_ops,
                        mm_camera_img_mode imgmode,
                        camera_mode_t mode);
    QCameraStream_SnapshotThumbnail(uint32_t CameraHandle,
                        uint32_t ChannelId,
                        uint32_t Width,
                        uint32_t Height,
                        uint32_t Format,
                        uint8_t NumBuffers,
                        mm_camera_vtbl_t *mm_ops,
                        mm_camera_img_mode imgmode,
                        camera_mode_t mode);
    ~QCameraStream_SnapshotThumbnail();
    static void            deleteInstance(QCameraStream *p);
    mm_camera_buf_def_t mPostviewStreamBuf[MM_CAMERA_MAX_NUM_FRAMES];
};

}; // namespace android

#endif
