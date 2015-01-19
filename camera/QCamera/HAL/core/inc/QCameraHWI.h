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

#ifndef ANDROID_HARDWARE_QCAMERA_HARDWARE_INTERFACE_H
#define ANDROID_HARDWARE_QCAMERA_HARDWARE_INTERFACE_H


#include <utils/threads.h>
#include <hardware/camera.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <utils/threads.h>
#include <cutils/properties.h>
#include <camera/Camera.h>
#include <camera/QCameraParameters.h>
#include <system/window.h>
#include <system/camera.h>
#include <hardware/camera.h>
#include <gralloc_priv.h>
#include <QComOMXMetadata.h>
#include <hardware/power.h>

extern "C" {
#include <linux/android_pmem.h>
#include <linux/msm_ion.h>
#include <camera.h>
#include <camera_defs_i.h>
#include <mm_camera_interface.h>
#include "mm_jpeg_interface.h"
} //extern C

#include "QCameraHWI_Mem.h"
#include "QCameraStream.h"

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

#define DEFAULT_STREAM_WIDTH 320
#define DEFAULT_STREAM_HEIGHT 240
#define DEFAULT_LIVESHOT_WIDTH 2592
#define DEFAULT_LIVESHOT_HEIGHT 1944

//for histogram stats
#define HISTOGRAM_STATS_SIZE 257
#define NUM_HISTOGRAM_BUFFERS 3

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
  CAMERA_STATE_MAX
} HAL_camera_state_type_t;

enum {
  BUFFER_NOT_OWNED,
  BUFFER_UNLOCKED,
  BUFFER_LOCKED,
};

typedef enum {
  HAL_DUMP_FRM_PREVIEW = 1,
  HAL_DUMP_FRM_VIDEO = 1<<1,
  HAL_DUMP_FRM_MAIN = 1<<2,
  HAL_DUMP_FRM_THUMBNAIL = 1<<3,
  HAL_DUMP_FRM_RDI = 1<<4,

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

typedef struct {
    int                     fd;
    int                     main_ion_fd;
    ion_user_handle_t       handle;
    uint32_t                size;
} QCameraHalMemInfo_t;

typedef struct {
     int                     buffer_count;
	 buffer_handle_t        *buffer_handle[MM_CAMERA_MAX_NUM_FRAMES];
	 struct private_handle_t *private_buffer_handle[MM_CAMERA_MAX_NUM_FRAMES];
	 int                     stride[MM_CAMERA_MAX_NUM_FRAMES];
	 uint32_t                addr_offset[MM_CAMERA_MAX_NUM_FRAMES];
	 uint8_t                 local_flag[MM_CAMERA_MAX_NUM_FRAMES];
     camera_memory_t        *camera_memory[MM_CAMERA_MAX_NUM_FRAMES];
     QCameraHalMemInfo_t     mem_info[MM_CAMERA_MAX_NUM_FRAMES];
} QCameraHalMemory_t;


typedef struct {
     int                     buffer_count;
	 int                     local_flag[MM_CAMERA_MAX_NUM_FRAMES];
     camera_memory_t *       camera_memory[MM_CAMERA_MAX_NUM_FRAMES];
     camera_memory_t *       metadata_memory[MM_CAMERA_MAX_NUM_FRAMES];
     QCameraHalMemInfo_t     mem_info[MM_CAMERA_MAX_NUM_FRAMES];
} QCameraHalHeap_t;

typedef struct {
     camera_memory_t *       camera_memory[NUM_HISTOGRAM_BUFFERS];
     QCameraHalMemInfo_t     mem_info[NUM_HISTOGRAM_BUFFERS];
     int active;
} QCameraStatHeap_t;

typedef struct {
  int32_t msg_type;
  int32_t ext1;
  int32_t ext2;
  void    *cookie;
} argm_notify_t;

typedef struct {
  int32_t                  msg_type;
  camera_memory_t         *data;
  unsigned int             index;
  camera_frame_metadata_t *metadata;
  void                    *cookie;
  void                    *user_data;
} argm_data_cb_t;

typedef struct {
  camera_notify_callback notifyCb;
  camera_data_callback   dataCb;
  argm_notify_t argm_notify;
  argm_data_cb_t        argm_data_cb;
} app_notify_cb_t;

/* camera_area_t
 * rectangle with weight to store the focus and metering areas.
 * x1, y1, x2, y2: from -1000 to 1000
 * weight: 0 to 1000
 */
typedef struct {
    int x1, y1, x2, y2;
    int weight;
} camera_area_t;

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

typedef struct {
    jpeg_job_status_t status;
    uint8_t thumbnailDroppedFlag;
    uint32_t client_hdl;
    uint32_t jobId;
    uint8_t* out_data;
    uint32_t data_size;
    mm_camera_super_buf_t* src_frame;
} camera_jpeg_data_t;

typedef struct {
    mm_camera_super_buf_t* src_frame;
    void* userdata;
} camera_jpeg_encode_cookie_t;

namespace android {

class QCameraStream;

typedef void (*release_data_fn)(void* data, void *user_data);

class QCameraQueue {
public:
    QCameraQueue();
    QCameraQueue(release_data_fn data_rel_fn, void *user_data);
    virtual ~QCameraQueue();
    bool enqueue(void *data);
    bool pri_enqueue(void *data);
    void flush();
    void* dequeue();
    bool is_empty();
private:
    typedef struct {
        struct cam_list list;
        void* data;
    } camera_q_node;

    camera_q_node mhead; /* dummy head */
    uint32_t msize;
    pthread_mutex_t mlock;
    release_data_fn mdata_rel_fn;
    void * muser_data;
};

typedef enum
{
    CAMERA_CMD_TYPE_NONE,
    CAMERA_CMD_TYPE_START_DATA_PROC,
    CAMERA_CMD_TYPE_STOP_DATA_PROC,
    CAMERA_CMD_TYPE_DO_NEXT_JOB,
    CAMERA_CMD_TYPE_EXIT,
    CAMERA_CMD_TYPE_MAX
} camera_cmd_type_t;

typedef struct snap_hdr_record_t_ {
    bool hdr_on;
    int num_frame;
    int num_raw_received;
    /*in terms of 2^*(n/6), e.g 6 means (1/2)x, whole 12 means 4x*/
    int exp[MAX_HDR_EXP_FRAME_NUM];
    mm_camera_super_buf_t *recvd_frame[MAX_HDR_EXP_FRAME_NUM];
} snap_hdr_record_t;

typedef struct {
    camera_cmd_type_t cmd;
} camera_cmd_t;

class QCameraCmdThread {
public:
    QCameraCmdThread();
    ~QCameraCmdThread();

    int32_t launch(void *(*start_routine)(void *), void* user_data);
    int32_t exit();
    int32_t sendCmd(camera_cmd_type_t cmd, uint8_t sync_cmd, uint8_t priority);
    camera_cmd_type_t getCmd();

    QCameraQueue cmd_queue;      /* cmd queue */
    pthread_t cmd_pid;           /* cmd thread ID */
    sem_t cmd_sem;               /* semaphore for cmd thread */
    sem_t sync_sem;              /* semaphore for synchronized call signal */
};

class QCameraHardwareInterface : public virtual RefBase {
public:

    QCameraHardwareInterface(int  cameraId, int mode);

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
     * Release the hardware resources owned by this object.  Note that this is
     * *not* done in the destructor.
     */
    void release();

    /**
     * Dump state of the camera hardware
     */
    int dump(int fd);
    void         getPictureSize(int *picture_width, int *picture_height) const;
    void         getPreviewSize(int *preview_width, int *preview_height) const;
	void         getVideoSize(int *video_width,int *video_height) const;
	void         getThumbnailSize(int *thumb_width, int *thumb_height) const;
    cam_format_t getPreviewFormat() const;
    cam_pad_format_t getPreviewPadding() const;
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

    static QCameraHardwareInterface *createInstance(int, int);
    status_t setZSLBurstLookBack(const QCameraParameters& params);
    status_t setZSLBurstInterval(const QCameraParameters& params);
    int getZSLBurstInterval(void);
    int getZSLQueueDepth(void) const;
    int getZSLBackLookCount(void) const;

    ~QCameraHardwareInterface();
    int initHeapMem(QCameraHalHeap_t *heap,
                    int num_of_buf,
                    uint32_t buf_len,
                    int pmem_type,
                    mm_camera_frame_len_offset* offset,
                    mm_camera_buf_def_t *buf_def);
    int releaseHeapMem( QCameraHalHeap_t *heap);
    int allocate_ion_memory(QCameraHalMemInfo_t * mem_info, int ion_type);
    int deallocate_ion_memory(QCameraHalMemInfo_t *mem_info);

    int cache_ops(QCameraHalMemInfo_t *mem_info,
                  void *buf_ptr,
                  unsigned int cmd);

    void dumpFrameToFile(const void * data, uint32_t size, char* name,
      char* ext, int index);
    preview_format_info_t getPreviewFormatInfo( );
    bool isCameraReady();
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

    mm_camera_vtbl_t *mCameraHandle;
    uint32_t mChannelId;

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
    void processRdiChannelEvent(mm_camera_ch_event_type_t channelEvent, app_notify_cb_t *);
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

    void stopPreviewInternal();
    void stopRecordingInternal();
    status_t cancelPictureInternal();
    void pausePreviewForSnapshot();
    void restartPreview();

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
    int getAutoFlickerMode();
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
    status_t setDimension();
    status_t setRDIMode(const QCameraParameters& params);
    status_t setMobiCat(const QCameraParameters& params);

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
    int32_t createRdi();

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
    status_t initHistogramBuffers();
    status_t deInitHistogramBuffers();
    mm_jpeg_color_format getColorfmtFromImgFmt(uint32_t img_fmt);

    void notifyHdrEvent(cam_ctrl_status_t status, void * cookie);
    void initHdrInfoForSnapshot(bool Hdr_on, int number_frames, int *exp );
    void doHdrProcessing();

    int           mCameraId;
    camera_mode_t myMode;

    mm_camear_mem_vtbl_t mem_hooks;

    QCameraParameters    mParameters;
    int32_t             mMsgEnabled;

    camera_notify_callback         mNotifyCb;
    camera_data_callback           mDataCb;
    camera_data_timestamp_callback mDataCbTimestamp;
    camera_request_memory          mGetMemory;
    void                           *mCallbackCookie;

    mutable Mutex       mLock;
    Mutex         mPreviewMemoryLock;
    Mutex         mAutofocusLock;
    Mutex         mRecordFrameLock;
    Condition     mRecordWait;
    pthread_mutex_t     mAsyncCmdMutex;
    pthread_cond_t      mAsyncCmdWait;

    QCameraStream *     mStreams[MM_CAMERA_IMG_MODE_MAX];
    cam_ctrl_dimension_t mDimension;
    int  mPreviewWidth, mPreviewHeight;
    int  mPictureWidth, mPictureHeight;
    int  videoWidth, videoHeight;
    int  thumbnailWidth, thumbnailHeight;
    int  maxSnapshotWidth, maxSnapshotHeight;
    int  mRdiWidth,mRdiHeight;
    int  mPreviewFormat;
    int  mFps;
    int  mDebugFps;
    int  mBrightness;
    int  mContrast;
    int  mBestShotMode;
    int  mEffects;
    int  mColorEffects;
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
    int  rdiMode;

    unsigned int mPictureSizeCount;
    unsigned int mPreviewSizeCount;
    int mPowerMode;
    unsigned int mVideoSizeCount;

    bool mAutoFocusRunning;
    bool mNeedToUnlockCaf;
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
    bool mMobiCatEnabled;
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

    friend class QCameraStream;
    friend class QCameraStream_record;
    friend class QCameraStream_preview;
    friend class QCameraStream_SnapshotMain;
    friend class QCameraStream_SnapshotThumbnail;
    friend class QCameraStream_Rdi;

    android :: FPSRange* mSupportedFpsRanges;
    int mSupportedFpsRangesCount;

    camera_size_type* mPictureSizes;
    camera_size_type* mPreviewSizes;
    camera_size_type* mVideoSizes;
    const camera_size_type * mPictureSizesPtr;
    HAL_camera_state_type_t mCameraState;

     int mStoreMetaDataInFrame;
     preview_stream_ops_t *mPreviewWindow;
     Mutex                mStateLock;
     int                  mPreviewState;
     /*preview memory with display case: memory is allocated and freed via
     gralloc */
     QCameraHalMemory_t   mPreviewMemory;

     /*preview memory without display case: memory is allocated
      directly by camera */
     QCameraHalHeap_t     mNoDispPreviewMemory;
     QCameraHalHeap_t     mRdiMemory;
     QCameraHalHeap_t     mSnapshotMemory;
     QCameraHalHeap_t     mThumbnailMemory;
     QCameraHalHeap_t     mRecordingMemory;
     QCameraHalHeap_t     mJpegMemory;
     QCameraHalHeap_t     mRawMemory;
     camera_frame_metadata_t mMetadata;
     camera_face_t           mFace[MAX_ROI];
     preview_format_info_t  mPreviewFormatInfo;
     friend void stream_cb_routine(mm_camera_super_buf_t *bufs, void *userdata);
     //EXIF
     exif_tags_info_t       mExifData[MAX_EXIF_TABLE_ENTRIES];  //Exif tags for JPEG encoder
     exif_values_t          mExifValues;                        //Exif values in usable format
     int                    mExifTableNumEntries;            //NUmber of entries in mExifData
     int                 mNoDisplayMode;
     QCameraQueue mSuperBufQueue;     /* queue for raw super buf */
     QCameraQueue mNotifyDataQueue;   /* queue for data notify */
     QCameraCmdThread *mNotifyTh;     /* thread for data notify */
     QCameraCmdThread *mDataProcTh;   /* thread for data process (jpeg encoding) */
     mm_jpeg_ops_t mJpegHandle;
     uint32_t mJpegClientHandle;
     snap_hdr_record_t    mHdrInfo;
     power_module_t*   mPowerModule;
     cam_sensor_fps_range_t mSensorFpsRange;

     static void *dataNotifyRoutine(void *data);
     static void *dataProcessRoutine(void *data);
     static void snapshot_jpeg_cb(jpeg_job_status_t status,
                             uint8_t thumbnailDroppedFlag,
                             uint32_t client_hdl,
                             uint32_t jobId,
                             uint8_t* out_data,
                             uint32_t data_size,
                             void *userdata);
     static void receiveCompleteJpegPicture(jpeg_job_status_t status,
                                            uint8_t thumbnailDroppedFlag,
                                            uint32_t client_hdl,
                                            uint32_t jobId,
                                            uint8_t* out_data,
                                            uint32_t data_size,
                                            QCameraHardwareInterface* pme);
     static void superbuf_cb_routine(mm_camera_super_buf_t *recvd_frame,
                                     void *userdata);
     static void receiveRawPicture(mm_camera_super_buf_t* recvd_frame,
                                   QCameraHardwareInterface *pme);
     status_t encodeData(mm_camera_super_buf_t* recvd_frame,
                         uint32_t *jobId);
     void notifyShutter(bool play_shutter_sound);
     status_t sendDataNotify(int32_t msg_type,
                             camera_memory_t *data,
                             uint8_t index,
                             camera_frame_metadata_t *metadata,
                             QCameraHalHeap_t *heap);

     void releaseSuperBuf(mm_camera_super_buf_t *super_buf);
     void releaseAppCBData(app_notify_cb_t *app_cb);
     static void releaseNofityData(void *data, void *user_data);
     static void releaseProcData(void *data, void *user_data);
     uint8_t canTakeFullSizeLiveshot();
};

}; // namespace android

#endif
