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

#ifndef __QCAMERA2HARDWAREINTERFACE_H__
#define __QCAMERA2HARDWAREINTERFACE_H__

#include <hardware/camera.h>
#include <hardware/power.h>
#include <utils/Log.h>
#include <utils/Mutex.h>
#include <utils/Condition.h>
#include <QCameraParameters.h>

#include "QCameraQueue.h"
#include "QCameraCmdThread.h"
#include "QCameraChannel.h"
#include "QCameraStream.h"
#include "QCameraStateMachine.h"
#include "QCameraAllocator.h"
#include "QCameraPostProc.h"
#include "QCameraThermalAdapter.h"
#include "QCameraMem.h"
#include "cam_intf.h"

extern "C" {
#include <mm_camera_interface.h>
#include <mm_jpeg_interface.h>
}

#if DISABLE_DEBUG_LOG

inline void __null_log(int, const char *, const char *, ...) {}

#ifdef ALOGD
#undef ALOGD
#define ALOGD(...) do { __null_log(0, LOG_TAG,__VA_ARGS__); } while (0)
#endif

#ifdef ALOGI
#undef ALOGI
#define ALOGI(...) do { __null_log(0, LOG_TAG,__VA_ARGS__); } while (0)
#endif

#ifdef CDBG
#undef CDBG
#define CDBG(...) do{} while(0)
#endif

#else

#ifdef CDBG
#undef CDBG
#endif //#ifdef CDBG
#define CDBG(fmt, args...) ALOGD_IF(gCamHalLogLevel >= 2, fmt, ##args)

#ifdef CDBG_HIGH
#undef CDBG_HIGH
#endif //#ifdef CDBG_HIGH
#define CDBG_HIGH(fmt, args...) ALOGD_IF(gCamHalLogLevel >= 1, fmt, ##args)

#endif // DISABLE_DEBUG_LOG

namespace qcamera {

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

typedef enum {
    QCAMERA_CH_TYPE_ZSL,
    QCAMERA_CH_TYPE_CAPTURE,
    QCAMERA_CH_TYPE_PREVIEW,
    QCAMERA_CH_TYPE_VIDEO,
    QCAMERA_CH_TYPE_SNAPSHOT,
    QCAMERA_CH_TYPE_RAW,
    QCAMERA_CH_TYPE_METADATA,
    QCAMERA_CH_TYPE_MAX
} qcamera_ch_type_enum_t;

typedef struct {
    int32_t msg_type;
    int32_t ext1;
    int32_t ext2;
} qcamera_evt_argm_t;

#define QCAMERA_DUMP_FRM_PREVIEW    1
#define QCAMERA_DUMP_FRM_VIDEO      (1<<1)
#define QCAMERA_DUMP_FRM_SNAPSHOT   (1<<2)
#define QCAMERA_DUMP_FRM_THUMBNAIL  (1<<3)
#define QCAMERA_DUMP_FRM_RAW        (1<<4)
#define QCAMERA_DUMP_FRM_JPEG       (1<<5)

#define QCAMERA_DUMP_FRM_MASK_ALL    0x000000ff

#define QCAMERA_ION_USE_CACHE   true
#define QCAMERA_ION_USE_NOCACHE false
#define MAX_ONGOING_JOBS 25
#define QCAMERA_MAX_FILEPATH_LENGTH 50

extern volatile uint32_t gCamHalLogLevel;

/** IMG_SWAP
 *  @a: input a
 *  @b: input b
 *
 *  Swaps the input values
 **/
#define IMG_SWAP(a, b) ({typeof(a) c; c=a; a=b; b=c;})

typedef enum {
    QCAMERA_NOTIFY_CALLBACK,
    QCAMERA_DATA_CALLBACK,
    QCAMERA_DATA_TIMESTAMP_CALLBACK,
    QCAMERA_DATA_SNAPSHOT_CALLBACK
} qcamera_callback_type_m;

typedef void (*camera_release_callback)(void *user_data,
                                        void *cookie,
                                        int32_t cb_status);

typedef struct {
    qcamera_callback_type_m  cb_type;    // event type
    int32_t                  msg_type;   // msg type
    int32_t                  ext1;       // extended parameter
    int32_t                  ext2;       // extended parameter
    camera_memory_t *        data;       // ptr to data memory struct
    unsigned int             index;      // index of the buf in the whole buffer
    int64_t                  timestamp;  // buffer timestamp
    camera_frame_metadata_t *metadata;   // meta data
    void                    *user_data;  // any data needs to be released after callback
    void                    *cookie;     // release callback cookie
    camera_release_callback  release_cb; // release callback
} qcamera_callback_argm_t;

typedef struct {
   cam_dimension_t all_preview_sizes[MAX_SIZES_CNT];
   size_t all_preview_sizes_cnt;
   cam_dimension_t all_video_sizes[MAX_SIZES_CNT];
   size_t all_video_sizes_cnt;
} qcamera_saved_sizes_list;

class QCameraCbNotifier {
public:
    QCameraCbNotifier(QCamera2HardwareInterface *parent) :
                          mNotifyCb (NULL),
                          mDataCb (NULL),
                          mDataCbTimestamp (NULL),
                          mCallbackCookie (NULL),
                          mParent (parent),
                          mDataQ(releaseNotifications, this),
                          mActive(false){}

    virtual ~QCameraCbNotifier();

    virtual int32_t notifyCallback(qcamera_callback_argm_t &cbArgs);
    virtual void setCallbacks(camera_notify_callback notifyCb,
                              camera_data_callback dataCb,
                              camera_data_timestamp_callback dataCbTimestamp,
                              void *callbackCookie);
    virtual int32_t startSnapshots();
    virtual void stopSnapshots();
    virtual void exit();
    static void * cbNotifyRoutine(void * data);
    static void releaseNotifications(void *data, void *user_data);
    static bool matchSnapshotNotifications(void *data, void *user_data);
    static bool matchPreviewNotifications(void *data, void *user_data);
    virtual int32_t flushPreviewNotifications();
private:

    camera_notify_callback         mNotifyCb;
    camera_data_callback           mDataCb;
    camera_data_timestamp_callback mDataCbTimestamp;
    void                          *mCallbackCookie;
    QCamera2HardwareInterface     *mParent;

    QCameraQueue     mDataQ;
    QCameraCmdThread mProcTh;
    bool             mActive;
};

class QCamera2HardwareInterface : public QCameraAllocator,
                                  public QCameraThermalCallback,
                                  public QCameraAdjustFPS,
                                  public QCameraTorchInterface
{
public:
    /* static variable and functions accessed by camera service */
    static camera_device_ops_t mCameraOps;

    static int set_preview_window(struct camera_device *,
        struct preview_stream_ops *window);
    static void set_CallBacks(struct camera_device *,
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void *user);
    static void enable_msg_type(struct camera_device *, int32_t msg_type);
    static void disable_msg_type(struct camera_device *, int32_t msg_type);
    static int msg_type_enabled(struct camera_device *, int32_t msg_type);
    static int start_preview(struct camera_device *);
    static void stop_preview(struct camera_device *);
    static int preview_enabled(struct camera_device *);
    static int store_meta_data_in_buffers(struct camera_device *, int enable);
    static int start_recording(struct camera_device *);
    static void stop_recording(struct camera_device *);
    static int recording_enabled(struct camera_device *);
    static void release_recording_frame(struct camera_device *, const void *opaque);
    static int auto_focus(struct camera_device *);
    static int cancel_auto_focus(struct camera_device *);
    static int take_picture(struct camera_device *);
    int takeLiveSnapshot_internal();
    int takeBackendPic_internal(bool *JpegMemOpt);
    void checkIntPicPending(bool JpegMemOpt);
    static int cancel_picture(struct camera_device *);
    static int set_parameters(struct camera_device *, const char *parms);
    static char* get_parameters(struct camera_device *);
    static void put_parameters(struct camera_device *, char *);
    static int send_command(struct camera_device *,
              int32_t cmd, int32_t arg1, int32_t arg2);
    static void release(struct camera_device *);
    static int dump(struct camera_device *, int fd);
    static int close_camera_device(hw_device_t *);

    static int register_face_image(struct camera_device *,
                                   void *img_ptr,
                                   cam_pp_offline_src_config_t *config);
public:
    QCamera2HardwareInterface(uint32_t cameraId);
    virtual ~QCamera2HardwareInterface();
    int openCamera(struct hw_device_t **hw_device);

    static int getCapabilities(uint32_t cameraId, struct camera_info *info);
    static int initCapabilities(uint32_t cameraId, mm_camera_vtbl_t *cameraHandle);

    // Implementation of QCameraAllocator
    virtual QCameraMemory *allocateStreamBuf(cam_stream_type_t stream_type,
            size_t size, int stride, int scanline, uint8_t &bufferCnt);
    virtual int32_t allocateMoreStreamBuf(QCameraMemory *mem_obj,
            size_t size, uint8_t &bufferCnt);

    virtual QCameraHeapMemory *allocateStreamInfoBuf(cam_stream_type_t stream_type);

    // Implementation of QCameraThermalCallback
    virtual int thermalEvtHandle(qcamera_thermal_level_enum_t level,
            void *userdata, void *data);

    virtual int recalcFPSRange(int &minFPS, int &maxFPS,
            cam_fps_range_t &adjustedRange);

    // Implementation of QCameraTorchInterface
    virtual int prepareTorchCamera();
    virtual int releaseTorchCamera();

    friend class QCameraStateMachine;
    friend class QCameraPostProcessor;
    friend class QCameraCbNotifier;

private:
    int setPreviewWindow(struct preview_stream_ops *window);
    int setCallBacks(
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void *user);
    int enableMsgType(int32_t msg_type);
    int disableMsgType(int32_t msg_type);
    int msgTypeEnabled(int32_t msg_type);
    int msgTypeEnabledWithLock(int32_t msg_type);
    int startPreview();
    int stopPreview();
    int storeMetaDataInBuffers(int enable);
    int startRecording();
    int stopRecording();
    int releaseRecordingFrame(const void *opaque);
    int autoFocus();
    int cancelAutoFocus();
    int takePicture();
    int stopCaptureChannel(bool destroy);
    int cancelPicture();
    int takeLiveSnapshot();
    int takePictureInternal();
    int cancelLiveSnapshot();
    char* getParameters();
    int putParameters(char *);
    int sendCommand(int32_t cmd, int32_t &arg1, int32_t &arg2);
    int release();
    int dump(int fd);
    int registerFaceImage(void *img_ptr,
                          cam_pp_offline_src_config_t *config,
                          int32_t &faceID);
    int32_t longShot();

    int openCamera();
    int closeCamera();

    int processAPI(qcamera_sm_evt_enum_t api, void *api_payload);
    int processEvt(qcamera_sm_evt_enum_t evt, void *evt_payload);
    int processSyncEvt(qcamera_sm_evt_enum_t evt, void *evt_payload);
    void lockAPI();
    void waitAPIResult(qcamera_sm_evt_enum_t api_evt, qcamera_api_result_t *apiResult);
    void unlockAPI();
    void signalAPIResult(qcamera_api_result_t *result);
    void signalEvtResult(qcamera_api_result_t *result);

    int calcThermalLevel(qcamera_thermal_level_enum_t level,
            const int minFPSi, const int maxFPSi, cam_fps_range_t &adjustedRange,
            enum msm_vfe_frame_skip_pattern &skipPattern);
    int updateThermalLevel(qcamera_thermal_level_enum_t level);

    // update entris to set parameters and check if restart is needed
    int updateParameters(const char *parms, bool &needRestart);
    // send request to server to set parameters
    int commitParameterChanges();

    bool needDebugFps();
    bool isRegularCapture();
    bool isCACEnabled();
    bool isPreviewRestartEnabled();
    bool is4k2kResolution(cam_dimension_t* resolution);
    bool isCaptureShutterEnabled();
    bool isAFRunning();
    bool needReprocess();
    bool needDualReprocess();
    bool needRotationReprocess();
    bool needScaleReprocess();
    void debugShowVideoFPS();
    void debugShowPreviewFPS();
    void dumpJpegToFile(const void *data, size_t size, uint32_t index);
    void dumpFrameToFile(QCameraStream *stream,
            mm_camera_buf_def_t *frame, uint32_t dump_type);
    void dumpMetadataToFile(QCameraStream *stream,
                            mm_camera_buf_def_t *frame,char *type);
    void releaseSuperBuf(mm_camera_super_buf_t *super_buf);
    void playShutter();
    void getThumbnailSize(cam_dimension_t &dim);
    uint32_t getJpegQuality();
    uint32_t getJpegRotation();
    void getOrientation();
    inline int getFlash(){ return mFlash; }
    inline int getFlashPresence(){ return mFlashPresence; }
    inline int getRedeye(){ return mRedEye; }
    inline bool getCancelAutoFocus(){ return mCancelAutoFocus; }
    inline void setCancelAutoFocus(bool flag){ mCancelAutoFocus = flag; }
    QCameraExif *getExifData();
    cam_sensor_t getSensorType();

    int32_t processAutoFocusEvent(cam_auto_focus_data_t &focus_data);
    int32_t processZoomEvent(cam_crop_data_t &crop_info);
    int32_t processPrepSnapshotDoneEvent(cam_prep_snapshot_state_t prep_snapshot_state);
    int32_t processASDUpdate(cam_auto_scene_t scene);
    int32_t processJpegNotify(qcamera_jpeg_evt_payload_t *jpeg_job);
    int32_t processHDRData(cam_asd_hdr_scene_data_t hdr_scene);
    int32_t transAwbMetaToParams(cam_awb_params_t &awb_params);
    int32_t processAWBUpdate(cam_awb_params_t &awb_params);

    int32_t sendEvtNotify(int32_t msg_type, int32_t ext1, int32_t ext2);
    int32_t sendDataNotify(int32_t msg_type,
                           camera_memory_t *data,
                           uint8_t index,
                           camera_frame_metadata_t *metadata);

    int32_t addChannel(qcamera_ch_type_enum_t ch_type);
    int32_t startChannel(qcamera_ch_type_enum_t ch_type);
    int32_t stopChannel(qcamera_ch_type_enum_t ch_type);
    int32_t delChannel(qcamera_ch_type_enum_t ch_type, bool destroy = true);
    int32_t addPreviewChannel();
    int32_t addSnapshotChannel();
    int32_t addVideoChannel();
    int32_t addZSLChannel();
    int32_t addCaptureChannel();
    int32_t addRawChannel();
    int32_t addMetaDataChannel();
    QCameraReprocessChannel *addReprocChannel(QCameraChannel *pInputChannel);
    QCameraReprocessChannel *addDualReprocChannel(QCameraChannel *pInputChannel);
    QCameraReprocessChannel *addOfflineReprocChannel(
                                                cam_pp_offline_src_config_t &img_config,
                                                cam_pp_feature_config_t &pp_feature,
                                                stream_cb_routine stream_cb,
                                                void *userdata);
    int32_t addStreamToChannel(QCameraChannel *pChannel,
                               cam_stream_type_t streamType,
                               stream_cb_routine streamCB,
                               void *userData);
    int32_t preparePreview();
    void unpreparePreview();
    int32_t prepareRawStream(QCameraChannel *pChannel);
    QCameraChannel *getChannelByHandle(uint32_t channelHandle);
    mm_camera_buf_def_t *getSnapshotFrame(mm_camera_super_buf_t *recvd_frame);
    int32_t processFaceDetectionResult(cam_face_detection_data_t *fd_data);
    bool needPreviewFDCallback(uint8_t num_faces);
    int32_t processHistogramStats(cam_hist_stats_t &stats_data);
    int32_t setHistogram(bool histogram_en);
    int32_t setFaceDetection(bool enabled);
    int32_t prepareHardwareForSnapshot(int32_t afNeeded);
    bool needProcessPreviewFrame();
    bool isNoDisplayMode() {return mParameters.isNoDisplayMode();};
    bool isZSLMode() {return mParameters.isZSLMode();};
    bool isHFRMode() {return mParameters.isHfrMode();};
    uint8_t numOfSnapshotsExpected() {
        return (uint8_t) ((mParameters.isUbiRefocus() ||
                    mParameters.isMTFRefocus()) ?
                1 : mParameters.getNumOfSnapshots());
    }
    bool isLongshotEnabled() { return mLongshotEnabled; };
    uint8_t getBufNumRequired(cam_stream_type_t stream_type);
    bool needFDMetadata(qcamera_ch_type_enum_t channel_type);
    bool removeSizeFromList(cam_dimension_t *size_list, size_t length,
            cam_dimension_t size);
    int32_t unconfigureAdvancedCapture();
    int32_t configureAdvancedCapture();
    int32_t configureAFBracketing(bool enable = true);
    int32_t configureMTFBracketing(bool enable = true);
    int32_t configureFlashBracketing(bool enable = true);
    int32_t startAdvancedCapture(QCameraPicChannel *pChannel);
    int32_t configureZSLHDRBracketing();
    int32_t startZslAdvancedCapture(QCameraPicChannel *pChannel);
    int32_t configureOptiZoom();
    int32_t configureFssr();
    int32_t configureAEBracketing();
    inline void setOutputImageCount(uint32_t aCount) {mOutputCount = aCount;}
    inline uint32_t getOutputImageCount() {return mOutputCount;}
    bool processUFDumps(qcamera_jpeg_evt_payload_t *evt);
    bool processMTFDumps(qcamera_jpeg_evt_payload_t *evt);
    void captureDone();
    static void copyList(cam_dimension_t *src_list, cam_dimension_t *dst_list,
            size_t len);
    static void camEvtHandle(uint32_t camera_handle,
                          mm_camera_event_t *evt,
                          void *user_data);
    static void jpegEvtHandle(jpeg_job_status_t status,
                              uint32_t client_hdl,
                              uint32_t jobId,
                              mm_jpeg_output_t *p_buf,
                              void *userdata);

    static void *evtNotifyRoutine(void *data);

    // functions for different data notify cb
    static void zsl_channel_cb(mm_camera_super_buf_t *recvd_frame, void *userdata);
    static void capture_channel_cb_routine(mm_camera_super_buf_t *recvd_frame,
                                           void *userdata);
    static void postproc_channel_cb_routine(mm_camera_super_buf_t *recvd_frame,
                                            void *userdata);
    static void dual_reproc_channel_cb_routine(mm_camera_super_buf_t *recvd_frame,
                                            void *userdata);
    static void nodisplay_preview_stream_cb_routine(mm_camera_super_buf_t *frame,
                                                    QCameraStream *stream,
                                                    void *userdata);
    static void preview_stream_cb_routine(mm_camera_super_buf_t *frame,
                                          QCameraStream *stream,
                                          void *userdata);
    static void postview_stream_cb_routine(mm_camera_super_buf_t *frame,
                                           QCameraStream *stream,
                                           void *userdata);
    static void video_stream_cb_routine(mm_camera_super_buf_t *frame,
                                        QCameraStream *stream,
                                        void *userdata);
    static void snapshot_channel_cb_routine(mm_camera_super_buf_t *frame,
           void *userdata);
    static void raw_stream_cb_routine(mm_camera_super_buf_t *frame,
                                      QCameraStream *stream,
                                      void *userdata);
    static void preview_raw_stream_cb_routine(mm_camera_super_buf_t * super_frame,
                                              QCameraStream * stream,
                                              void * userdata);
    static void snapshot_raw_stream_cb_routine(mm_camera_super_buf_t * super_frame,
                                               QCameraStream * stream,
                                               void * userdata);
    static void metadata_stream_cb_routine(mm_camera_super_buf_t *frame,
                                           QCameraStream *stream,
                                           void *userdata);
    static void reprocess_stream_cb_routine(mm_camera_super_buf_t *frame,
                                            QCameraStream *stream,
                                            void *userdata);

    static void releaseCameraMemory(void *data,
                                    void *cookie,
                                    int32_t cbStatus);
    static void returnStreamBuffer(void *data,
                                   void *cookie,
                                   int32_t cbStatus);
    static void getLogLevel();

private:
    camera_device_t   mCameraDevice;
    uint32_t          mCameraId;
    mm_camera_vtbl_t *mCameraHandle;
    bool mCameraOpened;

    preview_stream_ops_t *mPreviewWindow;
    QCameraParameters mParameters;
    int32_t               mMsgEnabled;
    int                   mStoreMetaDataInFrame;

    camera_notify_callback         mNotifyCb;
    camera_data_callback           mDataCb;
    camera_data_timestamp_callback mDataCbTimestamp;
    camera_request_memory          mGetMemory;
    void                          *mCallbackCookie;

    QCameraStateMachine m_stateMachine;   // state machine
    QCameraPostProcessor m_postprocessor; // post processor
    QCameraThermalAdapter &m_thermalAdapter;
    QCameraCbNotifier m_cbNotifier;
    pthread_mutex_t m_lock;
    pthread_cond_t m_cond;
    api_result_list *m_apiResultList;
    QCameraMemoryPool m_memoryPool;

    pthread_mutex_t m_evtLock;
    pthread_cond_t m_evtCond;
    qcamera_api_result_t m_evtResult;

    pthread_mutex_t m_parm_lock;

    QCameraChannel *m_channels[QCAMERA_CH_TYPE_MAX]; // array holding channel ptr

    bool m_bShutterSoundPlayed;         // if shutter sound had been played
    bool m_bPreviewStarted;             //flag indicates first preview frame callback is received
    bool m_bRecordStarted;             //flag indicates Recording is started for first time


    // if auto focus is running, in other words, when auto_focus is called from service,
    // and beforeany focus callback/cancel_focus happens. This flag is not an indication
    // of whether lens is moving or not.
    bool m_bAutoFocusRunning;
    cam_autofocus_state_t m_currentFocusState;

    power_module_t *m_pPowerModule;   // power module

    uint32_t mDumpFrmCnt;  // frame dump count
    uint32_t mDumpSkipCnt; // frame skip count
    mm_jpeg_exif_params_t mExifParams;
    qcamera_thermal_level_enum_t mThermalLevel;
    bool mCancelAutoFocus;
    bool mActiveAF;
    bool m_HDRSceneEnabled;
    bool mLongshotEnabled;
    int32_t m_max_pic_width;
    int32_t m_max_pic_height;
    pthread_t mLiveSnapshotThread;
    pthread_t mIntPicThread;
    bool mFlashNeeded;
    uint32_t mCaptureRotation;
    int32_t mFlash;
    int32_t mRedEye;
    int32_t mFlashPresence;
    bool mIs3ALocked;
    bool mPrepSnapRun;
    int32_t mZoomLevel;

    //eztune variables for communication with eztune server at backend
    bool m_bIntEvtPending;
    char m_BackendFileName[QCAMERA_MAX_FILEPATH_LENGTH];
    size_t mBackendFileSize;
    pthread_mutex_t m_int_lock;
    pthread_cond_t m_int_cond;

    enum DefferedWorkCmd {
        CMD_DEFF_ALLOCATE_BUFF,
        CMD_DEFF_PPROC_START,
        CMD_DEFF_MAX
    };

    typedef struct {
        QCameraChannel *ch;
        cam_stream_type_t type;
    } DefferAllocBuffArgs;

    typedef union {
        DefferAllocBuffArgs allocArgs;
        QCameraChannel *pprocArgs;
    } DefferWorkArgs;

    bool mDeffOngoingJobs[MAX_ONGOING_JOBS];

    struct DeffWork
    {
        DeffWork(DefferedWorkCmd cmd,
                 uint32_t id,
                 DefferWorkArgs args)
            : cmd(cmd),
              id(id),
              args(args){};

        DefferedWorkCmd cmd;
        uint32_t id;
        DefferWorkArgs args;
    };

    QCameraCmdThread      mDefferedWorkThread;
    QCameraQueue          mCmdQueue;

    Mutex                 mDeffLock;
    Condition             mDeffCond;

    int32_t queueDefferedWork(DefferedWorkCmd cmd,
                              DefferWorkArgs args);
    int32_t waitDefferedWork(int32_t &job_id);
    static void *defferedWorkRoutine(void *obj);

    int32_t mSnapshotJob;
    int32_t mPostviewJob;
    int32_t mMetadataJob;
    int32_t mReprocJob;
    int32_t mRawdataJob;
    uint32_t mOutputCount;
    bool mPreviewFrameSkipValid;
    cam_frame_idx_range_t mPreviewFrameSkipIdxRange;
    bool mAdvancedCaptureConfigured;
    int32_t mNumPreviewFaces;
};

}; // namespace qcamera

#endif /* __QCAMERA2HARDWAREINTERFACE_H__ */
