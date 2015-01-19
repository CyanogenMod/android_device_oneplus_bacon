/* Copyright (c) 2012-2014, The Linux Foundataion. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __QCAMERA_CHANNEL_H__
#define __QCAMERA_CHANNEL_H__

#include <hardware/camera.h>
#include "QCameraStream.h"

extern "C" {
#include <mm_camera_interface.h>
}

namespace qcamera {

class QCameraChannel
{
public:
    QCameraChannel(uint32_t cam_handle,
                   mm_camera_ops_t *cam_ops);
    QCameraChannel();
    virtual ~QCameraChannel();
    virtual int32_t init(mm_camera_channel_attr_t *attr,
                         mm_camera_buf_notify_t dataCB, // data CB for channel data
                         void *userData);
    // Owner of memory is transferred from the caller to the caller with this call.
    virtual int32_t addStream(QCameraAllocator& allocator,
                              QCameraHeapMemory *streamInfoBuf,
                              uint8_t minStreamBufnum,
                              cam_padding_info_t *paddingInfo,
                              stream_cb_routine stream_cb,
                              void *userdata,
                              bool bDynAllocBuf,
                              bool bDeffAlloc = false);
    virtual int32_t linkStream(QCameraChannel *ch, QCameraStream *stream);

    virtual int32_t start();
    virtual int32_t stop();
    virtual int32_t bufDone(mm_camera_super_buf_t *recvd_frame);
    virtual int32_t processZoomDone(preview_stream_ops_t *previewWindow,
                                    cam_crop_data_t &crop_info);
    int32_t config();
    QCameraStream *getStreamByHandle(uint32_t streamHandle);
    uint32_t getMyHandle() const {return m_handle;};
    uint32_t getNumOfStreams() const {
        return (uint32_t) mStreams.size();
    }
    QCameraStream *getStreamByIndex(uint32_t index);
    QCameraStream *getStreamByServerID(uint32_t serverID);
    int32_t UpdateStreamBasedParameters(QCameraParameters &param);
    void deleteChannel();

protected:
    uint32_t m_camHandle;
    mm_camera_ops_t *m_camOps;
    bool m_bIsActive;
    bool m_bAllowDynBufAlloc; // if buf allocation can be in two steps

    uint32_t m_handle;
    Vector<QCameraStream *> mStreams;
    mm_camera_buf_notify_t mDataCB;
    void *mUserData;
};

// burst pic channel: i.e. zsl burst mode
class QCameraPicChannel : public QCameraChannel
{
public:
    QCameraPicChannel(uint32_t cam_handle,
                      mm_camera_ops_t *cam_ops);
    QCameraPicChannel();
    virtual ~QCameraPicChannel();
    int32_t takePicture(uint8_t num_of_snapshot);
    int32_t cancelPicture();
    int32_t startAdvancedCapture(mm_camera_advanced_capture_t type);
    int32_t flushSuperbuffer(uint32_t frame_idx);
};

// video channel class
class QCameraVideoChannel : public QCameraChannel
{
public:
    QCameraVideoChannel(uint32_t cam_handle,
                        mm_camera_ops_t *cam_ops);
    QCameraVideoChannel();
    virtual ~QCameraVideoChannel();
    int32_t takePicture(uint8_t num_of_snapshot);
    int32_t cancelPicture();
    int32_t releaseFrame(const void *opaque, bool isMetaData);
};

// reprocess channel class
class QCameraReprocessChannel : public QCameraChannel
{
public:
    QCameraReprocessChannel(uint32_t cam_handle,
                            mm_camera_ops_t *cam_ops);
    QCameraReprocessChannel();
    virtual ~QCameraReprocessChannel();
    int32_t addReprocStreamsFromSource(QCameraAllocator& allocator,
                                       cam_pp_feature_config_t &config,
                                       QCameraChannel *pSrcChannel,
                                       uint8_t minStreamBufNum,
                                       uint8_t burstNum,
                                       cam_padding_info_t *paddingInfo,
                                       QCameraParameters &param,
                                       bool contStream,
                                       bool offline);
    // online reprocess
    int32_t doReprocess(mm_camera_super_buf_t *frame);
    // offline reprocess
    int32_t doReprocess(int buf_fd, size_t buf_length, int32_t &ret_val);
    int32_t doReprocessOffline(mm_camera_super_buf_t *frame);
    int32_t stop();
    QCameraChannel *getSourceChannel() { return m_pSrcChannel; }

private:
    QCameraStream *getStreamBySrouceHandle(uint32_t srcHandle);

    typedef struct {
        QCameraStream *stream;
        cam_mapping_buf_type type;
        uint32_t index;
    } OfflineBuffer;

    uint32_t mSrcStreamHandles[MAX_STREAM_NUM_IN_BUNDLE];
    QCameraChannel *m_pSrcChannel; // ptr to source channel for reprocess
    android::List<OfflineBuffer> mOfflineBuffers;

};

}; // namespace qcamera

#endif /* __QCAMERA_CHANNEL_H__ */
