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

#ifndef ANDROID_HARDWARE_QCAMERA_HARDWARE_INTERFACE_H
#define ANDROID_HARDWARE_QCAMERA_HARDWARE_INTERFACE_H


#include <utils/threads.h>
#include <hardware/camera2.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <utils/threads.h>
#include <cutils/properties.h>
#include <camera/Camera.h>
//#include <QCameraParameters.h>
#include <system/window.h>
#include <system/camera.h>
#include <gralloc_priv.h>
//#include <QComOMXMetadata.h>

extern "C" {
#include <linux/android_pmem.h>
#include <linux/msm_ion.h>
#include <mm_camera_interface.h>
#include "mm_jpeg_interface.h"
} //extern C

#include "QCameraStream.h"
#include "QCamera_Intf.h"

//Error codes
#define  NOT_FOUND -1
#define MAX_ZOOM_RATIOS 62

#ifdef Q12
#undef Q12
#endif

#define Q12 4096
#define QCAMERA_PARM_ENABLE   1
#define QCAMERA_PARM_DISABLE  0
#define PREVIEW_TBL_MAX_SIZE  14
#define VIDEO_TBL_MAX_SIZE    14
#define THUMB_TBL_MAX_SIZE    16
#define HFR_TBL_MAX_SIZE      2
//Default Video Width
#define DEFAULT_VIDEO_WIDTH 1920
#define DEFAULT_VIDEO_HEIGHT 1088

#define DEFAULT_STREAM_WIDTH 640
#define DEFAULT_STREAM_HEIGHT 480
#define DEFAULT_LIVESHOT_WIDTH 2592
#define DEFAULT_LIVESHOT_HEIGHT 1944

struct str_map {
    const char *const desc;
    int val;
};

struct preview_format_info_t {
   int Hal_format;
   cam_format_t mm_cam_format;
   cam_pad_format_t padding;
   int num_planar;
};


typedef enum {
  CAMERA_STATE_UNINITED,
  CAMERA_STATE_READY,
  CAMERA_STATE_PREVIEW_START_CMD_SENT,
  CAMERA_STATE_PREVIEW_STOP_CMD_SENT,
  CAMERA_STATE_PREVIEW,
  CAMERA_STATE_RECORD_START_CMD_SENT,  /*5*/
  CAMERA_STATE_RECORD_STOP_CMD_SENT,
  CAMERA_STATE_RECORD,
  CAMERA_STATE_SNAP_START_CMD_SENT,
  CAMERA_STATE_SNAP_STOP_CMD_SENT,
  CAMERA_STATE_SNAP_CMD_ACKED,  /*10 - snapshot comd acked, snapshot not done yet*/
  CAMERA_STATE_ZSL_START_CMD_SENT,
  CAMERA_STATE_ZSL,
  CAMERA_STATE_AF_START_CMD_SENT,
  CAMERA_STATE_AF_STOP_CMD_SENT,
  CAMERA_STATE_ERROR, /*15*/

  /*Add any new state above*/
  CAMERA_STATE_MAX
} HAL_camera_state_type_t;

enum {
  BUFFER_NOT_REGGED,
  BUFFER_NOT_OWNED,
  BUFFER_OWNED,
  BUFFER_UNLOCKED,
  BUFFER_LOCKED,
};

typedef enum {
  HAL_DUMP_FRM_PREVIEW = 1,
  HAL_DUMP_FRM_VIDEO = 1<<1,
  HAL_DUMP_FRM_MAIN = 1<<2,
  HAL_DUMP_FRM_THUMBNAIL = 1<<3,

  /*8 bits mask*/
  HAL_DUMP_FRM_MAX = 1 << 8
} HAL_cam_dump_frm_type_t;


typedef enum {
  HAL_CAM_MODE_ZSL = 1,

  /*add new entry before and update the max entry*/
  HAL_CAM_MODE_MAX = HAL_CAM_MODE_ZSL << 1,
} qQamera_mode_t;


typedef enum {
    MM_CAMERA_OK,
    MM_CAMERA_E_GENERAL,
    MM_CAMERA_E_NO_MEMORY,
    MM_CAMERA_E_NOT_SUPPORTED,
    MM_CAMERA_E_INVALID_INPUT,
    MM_CAMERA_E_INVALID_OPERATION, /* 5 */
    MM_CAMERA_E_ENCODE,
    MM_CAMERA_E_BUFFER_REG,
    MM_CAMERA_E_PMEM_ALLOC,
    MM_CAMERA_E_CAPTURE_FAILED,
    MM_CAMERA_E_CAPTURE_TIMEOUT, /* 10 */
}mm_camera_status_type_t;



#define HAL_DUMP_FRM_MASK_ALL ( HAL_DUMP_FRM_PREVIEW + HAL_DUMP_FRM_VIDEO + \
    HAL_DUMP_FRM_MAIN + HAL_DUMP_FRM_THUMBNAIL)
#define QCAMERA_HAL_PREVIEW_STOPPED    0
#define QCAMERA_HAL_PREVIEW_START      1
#define QCAMERA_HAL_PREVIEW_STARTED    2
#define QCAMERA_HAL_RECORDING_STARTED  3
#define QCAMERA_HAL_TAKE_PICTURE       4

//EXIF globals
static const char ExifAsciiPrefix[] = { 0x41, 0x53, 0x43, 0x49, 0x49, 0x0, 0x0, 0x0 };          // "ASCII\0\0\0"
static const char ExifUndefinedPrefix[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };   // "\0\0\0\0\0\0\0\0"

//EXIF detfines
#define MAX_EXIF_TABLE_ENTRIES           14
#define GPS_PROCESSING_METHOD_SIZE       101
#define FOCAL_LENGTH_DECIMAL_PRECISION   100
#define EXIF_ASCII_PREFIX_SIZE           8   //(sizeof(ExifAsciiPrefix))

typedef struct{
    //GPS tags
    rat_t       latitude[3];
    rat_t       longitude[3];
    char        lonRef[2];
    char        latRef[2];
    rat_t       altitude;
    rat_t       gpsTimeStamp[3];
    char        gpsDateStamp[20];
    char        gpsProcessingMethod[EXIF_ASCII_PREFIX_SIZE+GPS_PROCESSING_METHOD_SIZE];
    //Other tags
    char        dateTime[20];
    rat_t       focalLength;
    uint16_t    flashMode;
    uint16_t    isoSpeed;

    bool        mAltitude;
    bool        mLongitude;
    bool        mLatitude;
    bool        mTimeStamp;
    bool        mGpsProcess;

    int         mAltitude_ref;
    long        mGPSTimestamp;

} exif_values_t;

namespace android {

class QCameraStream;

class QCameraHardwareInterface : public virtual RefBase {
public:

    QCameraHardwareInterface(int  cameraId, int mode);

    int set_request_queue_src_ops(
        const camera2_request_queue_src_ops_t *request_src_ops);
    int notify_request_queue_not_empty();
    int set_frame_queue_dst_ops(const camera2_frame_queue_dst_ops_t *frame_dst_ops);
    int get_in_progress_count();
    int construct_default_request(int request_template, camera_metadata_t **request);
    int allocate_stream(uint32_t width,
        uint32_t height, int format,
        const camera2_stream_ops_t *stream_ops,
        uint32_t *stream_id,
        uint32_t *format_actual,
        uint32_t *usage,
        uint32_t *max_buffers);
    int register_stream_buffers(uint32_t stream_id, int num_buffers,
        buffer_handle_t *buffers);
    int release_stream(uint32_t stream_id);
    int allocate_reprocess_stream(
        uint32_t width,
        uint32_t height,
        uint32_t format,
        const camera2_stream_in_ops_t *reprocess_stream_ops,
        uint32_t *stream_id,
        uint32_t *consumer_usage,
        uint32_t *max_buffers);
    int release_reprocess_stream(
        uint32_t stream_id);
    int get_metadata_vendor_tag_ops(vendor_tag_query_ops_t **ops);
    int set_notify_callback(camera2_notify_callback notify_cb,
            void *user);


    void runCommandThread(void *);

    int getBuf(uint32_t camera_handle,
                        uint32_t ch_id, uint32_t stream_id,
                        void *user_data,
                        mm_camera_frame_len_offset *frame_offset_info,
                        uint8_t num_bufs,
                        uint8_t *initial_reg_flag,
                        mm_camera_buf_def_t  *bufs);
    int putBuf(uint32_t camera_handle,
                        uint32_t ch_id, uint32_t stream_id,
                        void *user_data, uint8_t num_bufs,
                        mm_camera_buf_def_t *bufs);

#if 0
    static QCameraHardwareInterface *createInstance(int, int);
    /** Set the ANativeWindow to which preview frames are sent */
    int setPreviewWindow(preview_stream_ops_t* window);

    /** Set the notification and data callbacks */
    void setCallbacks(camera_notify_callback notify_cb,
            camera_data_callback data_cb,
            camera_data_timestamp_callback data_cb_timestamp,
            camera_request_memory get_memory,
            void *user);

    /**
     * The following three functions all take a msg_type, which is a bitmask of
     * the messages defined in include/ui/Camera.h
     */

    /**
     * Enable a message, or set of messages.
     */
    void enableMsgType(int32_t msg_type);

    /**
     * Disable a message, or a set of messages.
     *
     * Once received a call to disableMsgType(CAMERA_MSG_VIDEO_FRAME), camera
     * HAL should not rely on its client to call releaseRecordingFrame() to
     * release video recording frames sent out by the cameral HAL before and
     * after the disableMsgType(CAMERA_MSG_VIDEO_FRAME) call. Camera HAL
     * clients must not modify/access any video recording frame after calling
     * disableMsgType(CAMERA_MSG_VIDEO_FRAME).
     */
    void disableMsgType(int32_t msg_type);

    /**
     * Query whether a message, or a set of messages, is enabled.  Note that
     * this is operates as an AND, if any of the messages queried are off, this
     * will return false.
     */
    int msgTypeEnabled(int32_t msg_type);

    /**
     * Start preview mode.
     */
    int startPreview();
    int startPreview2();

    /**
     * Stop a previously started preview.
     */
    void stopPreview();

    /**
     * Returns true if preview is enabled.
     */
    int previewEnabled();


    /**
     * Request the camera HAL to store meta data or real YUV data in the video
     * buffers sent out via CAMERA_MSG_VIDEO_FRAME for a recording session. If
     * it is not called, the default camera HAL behavior is to store real YUV
     * data in the video buffers.
     *
     * This method should be called before startRecording() in order to be
     * effective.
     *
     * If meta data is stored in the video buffers, it is up to the receiver of
     * the video buffers to interpret the contents and to find the actual frame
     * data with the help of the meta data in the buffer. How this is done is
     * outside of the scope of this method.
     *
     * Some camera HALs may not support storing meta data in the video buffers,
     * but all camera HALs should support storing real YUV data in the video
     * buffers. If the camera HAL does not support storing the meta data in the
     * video buffers when it is requested to do do, INVALID_OPERATION must be
     * returned. It is very useful for the camera HAL to pass meta data rather
     * than the actual frame data directly to the video encoder, since the
     * amount of the uncompressed frame data can be very large if video size is
     * large.
     *
     * @param enable if true to instruct the camera HAL to store
     *        meta data in the video buffers; false to instruct
     *        the camera HAL to store real YUV data in the video
     *        buffers.
     *
     * @return OK on success.
     */
    int storeMetaDataInBuffers(int enable);

    /**
     * Start record mode. When a record image is available, a
     * CAMERA_MSG_VIDEO_FRAME message is sent with the corresponding
     * frame. Every record frame must be released by a camera HAL client via
     * releaseRecordingFrame() before the client calls
     * disableMsgType(CAMERA_MSG_VIDEO_FRAME). After the client calls
     * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
     * responsibility to manage the life-cycle of the video recording frames,
     * and the client must not modify/access any video recording frames.
     */
    int startRecording();

    /**
     * Stop a previously started recording.
     */
    void stopRecording();

    /**
     * Returns true if recording is enabled.
     */
    int recordingEnabled();

    /**
     * Release a record frame previously returned by CAMERA_MSG_VIDEO_FRAME.
     *
     * It is camera HAL client's responsibility to release video recording
     * frames sent out by the camera HAL before the camera HAL receives a call
     * to disableMsgType(CAMERA_MSG_VIDEO_FRAME). After it receives the call to
     * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
     * responsibility to manage the life-cycle of the video recording frames.
     */
    void releaseRecordingFrame(const void *opaque);

    /**
     * Start auto focus, the notification callback routine is called with
     * CAMERA_MSG_FOCUS once when focusing is complete. autoFocus() will be
     * called again if another auto focus is needed.
     */
    int autoFocus();

    /**
     * Cancels auto-focus function. If the auto-focus is still in progress,
     * this function will cancel it. Whether the auto-focus is in progress or
     * not, this function will return the focus position to the default.  If
     * the camera does not support auto-focus, this is a no-op.
     */
    int cancelAutoFocus();

    /**
     * Take a picture.
     */
    int takePicture();

    /**
     * Cancel a picture that was started with takePicture. Calling this method
     * when no picture is being taken is a no-op.
     */
    int cancelPicture();

    /**
     * Set the camera parameters. This returns BAD_VALUE if any parameter is
     * invalid or not supported.
     */
    int setParameters(const char *parms);

    //status_t setParameters(const QCameraParameters& params);
    /** Retrieve the camera parameters.  The buffer returned by the camera HAL
        must be returned back to it with put_parameters, if put_parameters
        is not NULL.
     */
    int getParameters(char **parms);

    /** The camera HAL uses its own memory to pass us the parameters when we
        call get_parameters.  Use this function to return the memory back to
        the camera HAL, if put_parameters is not NULL.  If put_parameters
        is NULL, then you have to use free() to release the memory.
    */
    void putParameters(char *);

    /**
     * Send command to camera driver.
     */
    int sendCommand(int32_t cmd, int32_t arg1, int32_t arg2);


    /**
     * Dump state of the camera hardware
     */
    int dump(int fd);

    //virtual sp<IMemoryHeap> getPreviewHeap() const;
    //virtual sp<IMemoryHeap> getRawHeap() const;


    status_t    takeLiveSnapshot();
    status_t    takeFullSizeLiveshot();
    bool        canTakeFullSizeLiveshot();

    //virtual status_t          getBufferInfo( sp<IMemory>& Frame,
    //size_t *alignedSize);
    void         getPictureSize(int *picture_width, int *picture_height) const;
    void         getPreviewSize(int *preview_width, int *preview_height) const;
    cam_format_t getPreviewFormat() const;

    cam_pad_format_t getPreviewPadding() const;

    //bool     useOverlay(void);
    //virtual status_t setOverlay(const sp<Overlay> &overlay);
    void processEvent(mm_camera_event_t *);
    int  getJpegQuality() const;
    int  getNumOfSnapshots(void) const;
    int  getNumOfSnapshots(const QCameraParameters& params);
    int  getThumbSizesFromAspectRatio(uint32_t aspect_ratio,
                                     int *picture_width,
                                     int *picture_height);
    bool isRawSnapshot();
    bool mShutterSoundPlayed;
    void dumpFrameToFile(mm_camera_buf_def_t*, HAL_cam_dump_frm_type_t);

    status_t setZSLBurstLookBack(const QCameraParameters& params);
    status_t setZSLBurstInterval(const QCameraParameters& params);
    int getZSLBurstInterval(void);
    int getZSLQueueDepth(void) const;
    int getZSLBackLookCount(void) const;
#endif
    /**
     * Release the hardware resources owned by this object.  Note that this is
     * *not* done in the destructor.
     */
    void release();

    bool isCameraReady();
    ~QCameraHardwareInterface();

    mm_camear_mem_vtbl_t mMemHooks;
    mm_camera_vtbl_t *mCameraHandle;
    uint32_t mChannelId;

#if 0
    int initHeapMem( QCameraHalHeap_t *heap,
                            int num_of_buf,
                            int buf_len,
                            int y_off,
                            int cbcr_off,
                            int pmem_type,
                            mm_camera_buf_def_t *buf_def,
                            uint8_t num_planes,
                            uint32_t *planes);
    int releaseHeapMem( QCameraHalHeap_t *heap);
    status_t sendMappingBuf(int ext_mode, int idx, int fd, uint32_t size,
      int cameraid, mm_camera_socket_msg_type msg_type);
    status_t sendUnMappingBuf(int ext_mode, int idx, int cameraid,
      mm_camera_socket_msg_type msg_type);

    int allocate_ion_memory(QCameraHalHeap_t *p_camera_memory, int cnt,
      int ion_type);
    int deallocate_ion_memory(QCameraHalHeap_t *p_camera_memory, int cnt);

    int allocate_ion_memory(QCameraStatHeap_t *p_camera_memory, int cnt,
      int ion_type);
    int deallocate_ion_memory(QCameraStatHeap_t *p_camera_memory, int cnt);

    int cache_ops(int ion_fd, struct ion_flush_data *cache_inv_data, int type);

    void dumpFrameToFile(const void * data, uint32_t size, char* name,
      char* ext, int index);
    preview_format_info_t getPreviewFormatInfo( );
    bool isNoDisplayMode();

    int getBuf(uint32_t camera_handle,
                        uint32_t ch_id, uint32_t stream_id,
                        void *user_data,
                        mm_camera_frame_len_offset *frame_offset_info,
                        uint8_t num_bufs,
                        uint8_t *initial_reg_flag,
                        mm_camera_buf_def_t  *bufs);
    int putBuf(uint32_t camera_handle,
                        uint32_t ch_id, uint32_t stream_id,
                        void *user_data, uint8_t num_bufs,
                        mm_camera_buf_def_t *bufs);


private:
    int16_t  zoomRatios[MAX_ZOOM_RATIOS];
    struct camera_size_type default_preview_sizes[PREVIEW_TBL_MAX_SIZE];
    struct camera_size_type default_video_sizes[VIDEO_TBL_MAX_SIZE];
    struct camera_size_type default_hfr_sizes[HFR_TBL_MAX_SIZE];
    struct camera_size_type default_thumbnail_sizes[THUMB_TBL_MAX_SIZE];
    unsigned int preview_sizes_count;
    unsigned int video_sizes_count;
    unsigned int thumbnail_sizes_count;
    unsigned int hfr_sizes_count;


    bool mUseOverlay;

    void loadTables();
    void initDefaultParameters();
    bool getMaxPictureDimension(mm_camera_dimension_t *dim);

    status_t updateFocusDistances();

    bool native_set_parms(mm_camera_parm_type_t type, uint16_t length, void *value);
    bool native_set_parms( mm_camera_parm_type_t type, uint16_t length, void *value, int *result);

    void hasAutoFocusSupport();
    void debugShowPreviewFPS() const;
    //void prepareSnapshotAndWait();

    bool isPreviewRunning();
    bool isRecordingRunning();
    bool isSnapshotRunning();

    void processChannelEvent(mm_camera_ch_event_t *, app_notify_cb_t *);
    void processPreviewChannelEvent(mm_camera_ch_event_type_t channelEvent, app_notify_cb_t *);
    void processRecordChannelEvent(mm_camera_ch_event_type_t channelEvent, app_notify_cb_t *);
    void processSnapshotChannelEvent(mm_camera_ch_event_type_t channelEvent, app_notify_cb_t *);
    void processCtrlEvent(mm_camera_ctrl_event_t *, app_notify_cb_t *);
    void processStatsEvent(mm_camera_stats_event_t *, app_notify_cb_t *);
    void processInfoEvent(mm_camera_info_event_t *event, app_notify_cb_t *);
    void processprepareSnapshotEvent(cam_ctrl_status_t *);
    void roiEvent(fd_roi_t roi, app_notify_cb_t *);
    void zoomEvent(cam_ctrl_status_t *status, app_notify_cb_t *);
    void autofocusevent(cam_ctrl_status_t *status, app_notify_cb_t *);
    void handleZoomEventForPreview(app_notify_cb_t *);
    void handleZoomEventForSnapshot(void);
    status_t autoFocusEvent(cam_ctrl_status_t *, app_notify_cb_t *);

    void filterPictureSizes();
    bool supportsSceneDetection();
    bool supportsSelectableZoneAf();
    bool supportsFaceDetection();
    bool supportsRedEyeReduction();
    bool preview_parm_config (cam_ctrl_dimension_t* dim,QCameraParameters& parm);

    void stopPreviewInternal();
    void stopRecordingInternal();
    //void stopPreviewZSL();
    status_t cancelPictureInternal();
    //status_t startPreviewZSL();
    void pausePreviewForSnapshot();
    void pausePreviewForZSL();
    status_t resumePreviewAfterSnapshot();

    status_t runFaceDetection();

    status_t           setParameters(const QCameraParameters& params);
    QCameraParameters&  getParameters() ;

    status_t setCameraMode(const QCameraParameters& params);
    status_t setPictureSizeTable(void);
    status_t setPreviewSizeTable(void);
    status_t setVideoSizeTable(void);
    status_t setPreviewSize(const QCameraParameters& params);
    status_t setJpegThumbnailSize(const QCameraParameters& params);
    status_t setPreviewFpsRange(const QCameraParameters& params);
    status_t setPreviewFrameRate(const QCameraParameters& params);
    status_t setPreviewFrameRateMode(const QCameraParameters& params);
    status_t setVideoSize(const QCameraParameters& params);
    status_t setPictureSize(const QCameraParameters& params);
    status_t setJpegQuality(const QCameraParameters& params);
    status_t setNumOfSnapshot(const QCameraParameters& params);
    status_t setJpegRotation(int isZSL);
    int getJpegRotation(void);
    int getISOSpeedValue();
    status_t setAntibanding(const QCameraParameters& params);
    status_t setEffect(const QCameraParameters& params);
    status_t setExposureCompensation(const QCameraParameters &params);
    status_t setAutoExposure(const QCameraParameters& params);
    status_t setWhiteBalance(const QCameraParameters& params);
    status_t setFlash(const QCameraParameters& params);
    status_t setGpsLocation(const QCameraParameters& params);
    status_t setRotation(const QCameraParameters& params);
    status_t setZoom(const QCameraParameters& params);
    status_t setFocusMode(const QCameraParameters& params);
    status_t setBrightness(const QCameraParameters& params);
    status_t setSkinToneEnhancement(const QCameraParameters& params);
    status_t setOrientation(const QCameraParameters& params);
    status_t setLensshadeValue(const QCameraParameters& params);
    status_t setMCEValue(const QCameraParameters& params);
    status_t setISOValue(const QCameraParameters& params);
    status_t setPictureFormat(const QCameraParameters& params);
    status_t setSharpness(const QCameraParameters& params);
    status_t setContrast(const QCameraParameters& params);
    status_t setSaturation(const QCameraParameters& params);
    status_t setWaveletDenoise(const QCameraParameters& params);
    status_t setSceneMode(const QCameraParameters& params);
    status_t setContinuousAf(const QCameraParameters& params);
    status_t setFaceDetection(const char *str);
    status_t setSceneDetect(const QCameraParameters& params);
    status_t setStrTextures(const QCameraParameters& params);
    status_t setPreviewFormat(const QCameraParameters& params);
    status_t setSelectableZoneAf(const QCameraParameters& params);
    status_t setOverlayFormats(const QCameraParameters& params);
    status_t setHighFrameRate(const QCameraParameters& params);
    status_t setRedeyeReduction(const QCameraParameters& params);
    status_t setAEBracket(const QCameraParameters& params);
    status_t setFaceDetect(const QCameraParameters& params);
    status_t setDenoise(const QCameraParameters& params);
    status_t setAecAwbLock(const QCameraParameters & params);
    status_t setHistogram(int histogram_en);
    status_t setRecordingHint(const QCameraParameters& params);
    status_t setRecordingHintValue(const int32_t value);
    status_t setFocusAreas(const QCameraParameters& params);
    status_t setMeteringAreas(const QCameraParameters& params);
    status_t setFullLiveshot(void);
    status_t setDISMode(void);
    status_t setCaptureBurstExp(void);
    status_t setPowerMode(const QCameraParameters& params);
    void takePicturePrepareHardware( );
    status_t setNoDisplayMode(const QCameraParameters& params);

    isp3a_af_mode_t getAutoFocusMode(const QCameraParameters& params);
    bool isValidDimension(int w, int h);

    String8 create_values_str(const str_map *values, int len);

    void setMyMode(int mode);
    bool isZSLMode();
    bool isWDenoiseEnabled();
    void wdenoiseEvent(cam_ctrl_status_t status, void *cookie);
    bool isLowPowerCamcorder();
    void freePictureTable(void);
    void freeVideoSizeTable(void);

    int32_t createPreview();
    int32_t createRecord();
    int32_t createSnapshot();

    int getHDRMode();
    //EXIF
    void addExifTag(exif_tag_id_t tagid, exif_tag_type_t type,
                        uint32_t count, uint8_t copy, void *data);
    void setExifTags();
    void initExifData();
    void deinitExifData();
    void setExifTagsGPS();
    exif_tags_info_t* getExifData(){ return mExifData; }
    int getExifTableNumEntries() { return mExifTableNumEntries; }
    void parseGPSCoordinate(const char *latlonString, rat_t* coord);
    //added to support hdr
    bool getHdrInfoAndSetExp(int max_num_frm, int *num_frame, int *exp);
    void hdrEvent(cam_ctrl_status_t status, void *cookie);

    camera_mode_t myMode;




    QCameraParameters    mParameters;
    //sp<Overlay>         mOverlay;
    int32_t             mMsgEnabled;

    camera_notify_callback         mNotifyCb;
    camera_data_callback           mDataCb;
    camera_data_timestamp_callback mDataCbTimestamp;
    camera_request_memory          mGetMemory;
    void                           *mCallbackCookie;

    //sp<MemoryHeapBase>  mPreviewHeap;  //@Guru : Need to remove
    sp<AshmemPool>      mMetaDataHeap;

    mutable Mutex       mLock;
    //mutable Mutex       eventLock;
    Mutex         mCallbackLock;
    Mutex         mRecordingMemoryLock;
    Mutex         mAutofocusLock;
    Mutex         mMetaDataWaitLock;
    Mutex         mRecordFrameLock;
    Mutex         mRecordLock;
    Condition     mRecordWait;
    pthread_mutex_t     mAsyncCmdMutex;
    pthread_cond_t      mAsyncCmdWait;

    cam_ctrl_dimension_t mDimension;
    int  mPreviewWidth, mPreviewHeight;
    int  videoWidth, videoHeight;
    int  thumbnailWidth, thumbnailHeight;
    int  maxSnapshotWidth, maxSnapshotHeight;
    int  mPreviewFormat;
    int  mFps;
    int  mDebugFps;
    int  mBrightness;
    int  mContrast;
    int  mBestShotMode;
    int  mEffects;
    int  mSkinToneEnhancement;
    int  mDenoiseValue;
    int  mHJR;
    int  mRotation;
    int  mJpegQuality;
    int  mThumbnailQuality;
    int  mTargetSmoothZoom;
    int  mSmoothZoomStep;
    int  mMaxZoom;
    int  mCurrentZoom;
    int  mSupportedPictureSizesCount;
    int  mFaceDetectOn;
    int  mDumpFrmCnt;
    int  mDumpSkipCnt;
    int  mFocusMode;

    unsigned int mPictureSizeCount;
    unsigned int mPreviewSizeCount;
    int mPowerMode;
    unsigned int mVideoSizeCount;

    bool mAutoFocusRunning;
    bool mMultiTouch;
    bool mHasAutoFocusSupport;
    bool mInitialized;
    bool mDisEnabled;
    bool strTexturesOn;
    bool mIs3DModeOn;
    bool mSmoothZoomRunning;
    bool mPreparingSnapshot;
    bool mParamStringInitialized;
    bool mZoomSupported;
    bool mSendMetaData;
    bool mFullLiveshotEnabled;
    bool mRecordingHint;
    bool mStartRecording;
    bool mReleasedRecordingFrame;
    int mHdrMode;
    int mSnapshotFormat;
    int mZslInterval;
    bool mRestartPreview;

/*for histogram*/
    int            mStatsOn;
    int            mCurrentHisto;
    bool           mSendData;
    sp<AshmemPool> mStatHeap;
    camera_memory_t *mStatsMapped[3];
    QCameraStatHeap_t mHistServer;
    int32_t        mStatSize;

    bool mZslLookBackMode;
    int mZslLookBackValue;
	int mHFRLevel;
    bool mZslEmptyQueueFlag;
    String8 mEffectValues;
    String8 mIsoValues;
    String8 mSceneModeValues;
    String8 mSceneDetectValues;
    String8 mFocusModeValues;
    String8 mSelectableZoneAfValues;
    String8 mAutoExposureValues;
    String8 mWhitebalanceValues;
    String8 mAntibandingValues;
    String8 mFrameRateModeValues;
    String8 mTouchAfAecValues;
    String8 mPreviewSizeValues;
    String8 mPictureSizeValues;
    String8 mVideoSizeValues;
    String8 mFlashValues;
    String8 mLensShadeValues;
    String8 mMceValues;
    String8 mHistogramValues;
    String8 mSkinToneEnhancementValues;
    String8 mPictureFormatValues;
    String8 mDenoiseValues;
    String8 mZoomRatioValues;
    String8 mPreviewFrameRateValues;
    String8 mPreviewFormatValues;
    String8 mFaceDetectionValues;
    String8 mHfrValues;
    String8 mHfrSizeValues;
    String8 mRedeyeReductionValues;
    String8 denoise_value;
    String8 mFpsRangesSupportedValues;
    String8 mZslValues;
    String8 mFocusDistance;


    android :: FPSRange* mSupportedFpsRanges;
    int mSupportedFpsRangesCount;

    camera_size_type* mPictureSizes;
    camera_size_type* mPreviewSizes;
    camera_size_type* mVideoSizes;
    const camera_size_type * mPictureSizesPtr;
    HAL_camera_state_type_t mCameraState;

     /* Temporary - can be removed after Honeycomb*/
#ifdef USE_ION
    sp<IonPool>  mPostPreviewHeap;
#else
    sp<PmemPool> mPostPreviewHeap;
#endif
    // mm_cameara_stream_buf_t mPrevForPostviewBuf;
     int mStoreMetaDataInFrame;
     preview_stream_ops_t *mPreviewWindow;
     Mutex                mStateLock;
     int                  mPreviewState;
     /*preview memory without display case: memory is allocated
      directly by camera */
     QCameraHalHeap_t     mNoDispPreviewMemory;

     QCameraHalHeap_t     mSnapshotMemory;
     QCameraHalHeap_t     mThumbnailMemory;
     QCameraHalHeap_t     mRecordingMemory;
     QCameraHalHeap_t     mJpegMemory;
     QCameraHalHeap_t     mRawMemory;
     camera_frame_metadata_t mMetadata;
     camera_face_t           mFace[MAX_ROI];
     preview_format_info_t  mPreviewFormatInfo;
    // friend void liveshot_callback(mm_camera_ch_data_buf_t *frame,void *user_data);
     //EXIF
     exif_tags_info_t       mExifData[MAX_EXIF_TABLE_ENTRIES];  //Exif tags for JPEG encoder
     exif_values_t          mExifValues;                        //Exif values in usable format
     int                    mExifTableNumEntries;            //NUmber of entries in mExifData
     int                 mNoDisplayMode;
#endif
private:
    int           mCameraId;
    camera_metadata_t *mStaticInfo;

    friend class QCameraStream;
    friend class QCameraStream_record;
    friend class QCameraStream_preview;
    friend class QCameraStream_SnapshotMain;
    friend class QCameraStream_SnapshotThumbnail;
    friend class QCameraStream_Rdi;

    friend void stream_cb_routine(mm_camera_super_buf_t *bufs, void *userdata);
    friend void superbuf_cb_routine(mm_camera_super_buf_t *bufs, void *userdata);
    friend void *command_thread(void *);

    int tryRestartStreams(camera_metadata_entry_t& streams);

    QCameraStream       *mStreamDisplay;
    QCameraStream       *mStreamRecord;
    QCameraStream       *mStreamSnapMain;
    QCameraStream       *mStreamSnapThumb;
    QCameraStream       *mStreamLiveSnap;
    QCameraStream       *mStreamRdi;

    const camera2_request_queue_src_ops *mRequestQueueSrc;
    uint8_t mPendingRequests;
    const camera2_frame_queue_dst_ops *mFrameQueueDst;
    camera2_notify_callback mNotifyCb;
    void *mNotifyUserPtr;
    pthread_t mCommandThread;
};

}; // namespace android

#endif
