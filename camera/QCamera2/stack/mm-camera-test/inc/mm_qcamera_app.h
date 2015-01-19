/* Copyright (c) 2012, 2014, The Linux Foundation. All rights reserved.
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

#ifndef __MM_QCAMERA_APP_H__
#define __MM_QCAMERA_APP_H__

#include <pthread.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <linux/fb.h>
#include <linux/msm_mdp.h>
#include <semaphore.h>

#include "mm_camera_interface.h"
#include "mm_jpeg_interface.h"
#include "mm_qcamera_socket.h"

#define MM_QCAMERA_APP_INTERATION 1

#define MM_APP_MAX_DUMP_FRAME_NUM 1000

#define PREVIEW_BUF_NUM 7
#define VIDEO_BUF_NUM 7
#define ISP_PIX_BUF_NUM 9
#define STATS_BUF_NUM 4
#define RDI_BUF_NUM 8
#define CAPTURE_BUF_NUM 5

#define DEFAULT_PREVIEW_FORMAT    CAM_FORMAT_YUV_420_NV21
#define DEFAULT_PREVIEW_WIDTH     640
#define DEFAULT_PREVIEW_HEIGHT    480
#define DEFAULT_PREVIEW_PADDING   CAM_PAD_TO_WORD
#define DEFAULT_VIDEO_FORMAT      CAM_FORMAT_YUV_420_NV12
#define DEFAULT_VIDEO_WIDTH       800
#define DEFAULT_VIDEO_HEIGHT      480
#define DEFAULT_VIDEO_PADDING     CAM_PAD_TO_2K
#define DEFAULT_SNAPSHOT_FORMAT   CAM_FORMAT_YUV_420_NV21
#define DEFAULT_RAW_FORMAT        CAM_FORMAT_BAYER_QCOM_RAW_10BPP_GBRG

#define DEFAULT_SNAPSHOT_WIDTH    1024
#define DEFAULT_SNAPSHOT_HEIGHT   768
#define DEFAULT_SNAPSHOT_PADDING  CAM_PAD_TO_WORD

#define DEFAULT_OV_FORMAT         MDP_Y_CRCB_H2V2
#define DEFAULT_OV_FORMAT_BPP     3/2
#define DEFAULT_CAMERA_FORMAT_BPP 3/2
#define FB_PATH                   "/dev/graphics/fb0"
#define BACKLIGHT_CONTROL         "/sys/class/leds/lcd-backlight/brightness"
#define BACKLIGHT_LEVEL           "205"

#define ENABLE_REPROCESSING       1

#define INVALID_KEY_PRESS 0
#define BASE_OFFSET  ('Z' - 'A' + 1)
#define BASE_OFFSET_NUM  ('Z' - 'A' + 2)

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

typedef enum {
    TUNE_CMD_INIT,
    TUNE_CMD_GET_LIST,
    TUNE_CMD_GET_PARAMS,
    TUNE_CMD_SET_PARAMS,
    TUNE_CMD_MISC,
    TUNE_CMD_DEINIT,
} mm_camera_tune_cmd_t;

typedef enum {
    TUNE_PREVCMD_INIT,
    TUNE_PREVCMD_SETDIM,
    TUNE_PREVCMD_GETINFO,
    TUNE_PREVCMD_GETCHUNKSIZE,
    TUNE_PREVCMD_GETFRAME,
    TUNE_PREVCMD_UNSUPPORTED,
    TUNE_PREVCMD_DEINIT,
} mm_camera_tune_prevcmd_t;

typedef void (*prev_callback) (mm_camera_buf_def_t *preview_frame);

typedef struct {
  char *send_buf;
  uint32_t send_len;
  void *next;
} eztune_prevcmd_rsp;

typedef struct {
    int (*command_process) (void *recv, mm_camera_tune_cmd_t cmd,
      void *param, char *send_buf, uint32_t send_len);
    int (*prevcommand_process) (void *recv, mm_camera_tune_prevcmd_t cmd,
      void *param, char **send_buf, uint32_t *send_len);
    void (*prevframe_callback) (mm_camera_buf_def_t *preview_frame);
} mm_camera_tune_func_t;

typedef struct {
    mm_camera_tune_func_t *func_tbl;
    void *lib_handle;
}mm_camera_tuning_lib_params_t;

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
} mm_camera_status_type_t;

typedef enum {
    MM_CHANNEL_TYPE_ZSL,      /* preview, and snapshot main */
    MM_CHANNEL_TYPE_CAPTURE,  /* snapshot main, and postview */
    MM_CHANNEL_TYPE_PREVIEW,  /* preview only */
    MM_CHANNEL_TYPE_SNAPSHOT, /* snapshot main only */
    MM_CHANNEL_TYPE_VIDEO,    /* video only */
    MM_CHANNEL_TYPE_RDI,      /* rdi only */
    MM_CHANNEL_TYPE_REPROCESS,/* offline reprocess */
    MM_CHANNEL_TYPE_MAX
} mm_camera_channel_type_t;

typedef struct {
    int                     fd;
    int                     main_ion_fd;
    ion_user_handle_t       handle;
    size_t                  size;
    void *                  data;
} mm_camera_app_meminfo_t;

typedef struct {
    mm_camera_buf_def_t buf;
    mm_camera_app_meminfo_t mem_info;
} mm_camera_app_buf_t;

typedef struct {
    uint32_t s_id;
    mm_camera_stream_config_t s_config;
    cam_frame_len_offset_t offset;
    uint8_t num_of_bufs;
    uint32_t multipleOf;
    mm_camera_app_buf_t s_bufs[MM_CAMERA_MAX_NUM_FRAMES];
    mm_camera_app_buf_t s_info_buf;
} mm_camera_stream_t;

typedef struct {
    uint32_t ch_id;
    uint8_t num_streams;
    mm_camera_stream_t streams[MAX_STREAM_NUM_IN_BUNDLE];
} mm_camera_channel_t;

typedef void (*release_data_fn)(void* data, void *user_data);

typedef struct {
    struct cam_list list;
    void* data;
} camera_q_node;

typedef struct {
    camera_q_node m_head;
    int m_size;
    pthread_mutex_t m_lock;
    release_data_fn m_dataFn;
    void * m_userData;
} mm_camera_queue_t;

typedef struct {
    uint16_t user_input_display_width;
    uint16_t user_input_display_height;
} USER_INPUT_DISPLAY_T;

typedef struct {
    mm_camera_vtbl_t *cam;
    uint8_t num_channels;
    mm_camera_channel_t channels[MM_CHANNEL_TYPE_MAX];
    mm_jpeg_ops_t jpeg_ops;
    uint32_t jpeg_hdl;
    mm_camera_app_buf_t cap_buf;
    mm_camera_app_buf_t parm_buf;

    uint32_t current_jpeg_sess_id;
    mm_camera_super_buf_t* current_job_frames;
    uint32_t current_job_id;
    mm_camera_app_buf_t jpeg_buf;

    int fb_fd;
    struct fb_var_screeninfo vinfo;
    struct mdp_overlay data_overlay;
    uint32_t slice_size;
    uint32_t buffer_width, buffer_height;
    uint32_t buffer_size;
    cam_format_t buffer_format;
    uint32_t frame_size;
    uint32_t frame_count;
    int encodeJpeg;
    int zsl_enabled;
    int8_t focus_supported;
    prev_callback user_preview_cb;
    parm_buffer_new_t *params_buffer;
    USER_INPUT_DISPLAY_T preview_resolution;

    //Reprocess params&stream
    int8_t enable_reproc;
    int32_t reproc_sharpness;
    cam_denoise_param_t reproc_wnr;
    int8_t enable_CAC;
    mm_camera_queue_t pp_frames;
    mm_camera_stream_t *reproc_stream;
    cam_metadata_info_t *metadata;
    int8_t is_chromatix_reload;
    tune_chromatix_t tune_data;
} mm_camera_test_obj_t;

typedef struct {
  void *ptr;
  void* ptr_jpeg;

  uint8_t (*get_num_of_cameras) ();
  mm_camera_vtbl_t *(*mm_camera_open) (uint8_t camera_idx);
  uint32_t (*jpeg_open) (mm_jpeg_ops_t *ops, mm_dimension picture_size);

} hal_interface_lib_t;

typedef struct {
    uint8_t num_cameras;
    hal_interface_lib_t hal_lib;
} mm_camera_app_t;

typedef struct {
    uint32_t width;
    uint32_t height;
} mm_camera_lib_snapshot_params;

typedef enum {
    MM_CAMERA_LIB_NO_ACTION = 0,
    MM_CAMERA_LIB_RAW_CAPTURE,
    MM_CAMERA_LIB_JPEG_CAPTURE,
    MM_CAMERA_LIB_SET_FOCUS_MODE,
    MM_CAMERA_LIB_DO_AF,
    MM_CAMERA_LIB_CANCEL_AF,
    MM_CAMERA_LIB_LOCK_AE,
    MM_CAMERA_LIB_UNLOCK_AE,
    MM_CAMERA_LIB_LOCK_AWB,
    MM_CAMERA_LIB_UNLOCK_AWB,
    MM_CAMERA_LIB_GET_CHROMATIX,
    MM_CAMERA_LIB_SET_RELOAD_CHROMATIX,
    MM_CAMERA_LIB_GET_AFTUNE,
    MM_CAMERA_LIB_SET_RELOAD_AFTUNE,
    MM_CAMERA_LIB_SET_AUTOFOCUS_TUNING,
    MM_CAMERA_LIB_ZSL_ENABLE,
    MM_CAMERA_LIB_EV,
    MM_CAMERA_LIB_ANTIBANDING,
    MM_CAMERA_LIB_SET_VFE_COMMAND,
    MM_CAMERA_LIB_SET_POSTPROC_COMMAND,
    MM_CAMERA_LIB_SET_3A_COMMAND,
    MM_CAMERA_LIB_AEC_ENABLE,
    MM_CAMERA_LIB_AEC_DISABLE,
    MM_CAMERA_LIB_AF_ENABLE,
    MM_CAMERA_LIB_AF_DISABLE,
    MM_CAMERA_LIB_AWB_ENABLE,
    MM_CAMERA_LIB_AWB_DISABLE,
    MM_CAMERA_LIB_AEC_FORCE_LC,
    MM_CAMERA_LIB_AEC_FORCE_GAIN,
    MM_CAMERA_LIB_AEC_FORCE_EXP,
    MM_CAMERA_LIB_AEC_FORCE_SNAP_LC,
    MM_CAMERA_LIB_AEC_FORCE_SNAP_GAIN,
    MM_CAMERA_LIB_AEC_FORCE_SNAP_EXP,
    MM_CAMERA_LIB_WB,
    MM_CAMERA_LIB_EXPOSURE_METERING,
    MM_CAMERA_LIB_BRIGHTNESS,
    MM_CAMERA_LIB_CONTRAST,
    MM_CAMERA_LIB_SATURATION,
    MM_CAMERA_LIB_SHARPNESS,
    MM_CAMERA_LIB_ISO,
    MM_CAMERA_LIB_ZOOM,
    MM_CAMERA_LIB_BESTSHOT,
    MM_CAMERA_LIB_FLASH,
    MM_CAMERA_LIB_FPS_RANGE,
    MM_CAMERA_LIB_WNR_ENABLE,
    MM_CAMERA_LIB_SET_TINTLESS,
} mm_camera_lib_commands;

typedef struct {
    int32_t stream_width, stream_height;
    cam_focus_mode_type af_mode;
} mm_camera_lib_params;

typedef struct {
  tuneserver_protocol_t *proto;
  int clientsocket_id;
  prserver_protocol_t *pr_proto;
  int pr_clientsocket_id;
  mm_camera_tuning_lib_params_t tuning_params;
} tuningserver_t;

typedef struct {
    mm_camera_app_t app_ctx;
    mm_camera_test_obj_t test_obj;
    mm_camera_lib_params current_params;
    int stream_running;
    tuningserver_t tsctrl;
} mm_camera_lib_ctx;

typedef mm_camera_lib_ctx mm_camera_lib_handle;

typedef int (*mm_app_test_t) (mm_camera_app_t *cam_apps);
typedef struct {
    mm_app_test_t f;
    int r;
} mm_app_tc_t;

extern int mm_app_unit_test_entry(mm_camera_app_t *cam_app);
extern int mm_app_dual_test_entry(mm_camera_app_t *cam_app);
extern void mm_app_dump_frame(mm_camera_buf_def_t *frame,
                              char *name,
                              char *ext,
                              uint32_t frame_idx);
extern void mm_app_dump_jpeg_frame(const void * data,
                                   size_t size,
                                   char* name,
                                   char* ext,
                                   uint32_t index);
extern int mm_camera_app_timedwait(uint8_t seconds);
extern int mm_camera_app_wait();
extern void mm_camera_app_done();
extern int mm_app_alloc_bufs(mm_camera_app_buf_t* app_bufs,
                             cam_frame_len_offset_t *frame_offset_info,
                             uint8_t num_bufs,
                             uint8_t is_streambuf,
                             size_t multipleOf);
extern int mm_app_release_bufs(uint8_t num_bufs,
                               mm_camera_app_buf_t* app_bufs);
extern int mm_app_stream_initbuf(cam_frame_len_offset_t *frame_offset_info,
                                 uint8_t *num_bufs,
                                 uint8_t **initial_reg_flag,
                                 mm_camera_buf_def_t **bufs,
                                 mm_camera_map_unmap_ops_tbl_t *ops_tbl,
                                 void *user_data);
extern int32_t mm_app_stream_deinitbuf(mm_camera_map_unmap_ops_tbl_t *ops_tbl,
                                       void *user_data);
extern int mm_app_cache_ops(mm_camera_app_meminfo_t *mem_info, int cmd);
extern int32_t mm_app_stream_clean_invalidate_buf(uint32_t index, void *user_data);
extern int32_t mm_app_stream_invalidate_buf(uint32_t index, void *user_data);
extern int mm_app_open(mm_camera_app_t *cam_app,
                       int cam_id,
                       mm_camera_test_obj_t *test_obj);
extern int mm_app_close(mm_camera_test_obj_t *test_obj);
extern mm_camera_channel_t * mm_app_add_channel(
                                         mm_camera_test_obj_t *test_obj,
                                         mm_camera_channel_type_t ch_type,
                                         mm_camera_channel_attr_t *attr,
                                         mm_camera_buf_notify_t channel_cb,
                                         void *userdata);
extern int mm_app_del_channel(mm_camera_test_obj_t *test_obj,
                              mm_camera_channel_t *channel);
extern mm_camera_stream_t * mm_app_add_stream(mm_camera_test_obj_t *test_obj,
                                              mm_camera_channel_t *channel);
extern int mm_app_del_stream(mm_camera_test_obj_t *test_obj,
                             mm_camera_channel_t *channel,
                             mm_camera_stream_t *stream);
extern int mm_app_config_stream(mm_camera_test_obj_t *test_obj,
                                mm_camera_channel_t *channel,
                                mm_camera_stream_t *stream,
                                mm_camera_stream_config_t *config);
extern int mm_app_start_channel(mm_camera_test_obj_t *test_obj,
                                mm_camera_channel_t *channel);
extern int mm_app_stop_channel(mm_camera_test_obj_t *test_obj,
                               mm_camera_channel_t *channel);
extern mm_camera_channel_t *mm_app_get_channel_by_type(
                                    mm_camera_test_obj_t *test_obj,
                                    mm_camera_channel_type_t ch_type);

extern int mm_app_start_preview(mm_camera_test_obj_t *test_obj);
extern int mm_app_stop_preview(mm_camera_test_obj_t *test_obj);
extern int mm_app_start_preview_zsl(mm_camera_test_obj_t *test_obj);
extern int mm_app_stop_preview_zsl(mm_camera_test_obj_t *test_obj);
extern mm_camera_channel_t * mm_app_add_preview_channel(
                                mm_camera_test_obj_t *test_obj);
extern mm_camera_stream_t * mm_app_add_raw_stream(mm_camera_test_obj_t *test_obj,
                                                mm_camera_channel_t *channel,
                                                mm_camera_buf_notify_t stream_cb,
                                                void *userdata,
                                                uint8_t num_bufs,
                                                uint8_t num_burst);
extern int mm_app_stop_and_del_channel(mm_camera_test_obj_t *test_obj,
                                       mm_camera_channel_t *channel);
extern mm_camera_channel_t * mm_app_add_snapshot_channel(
                                               mm_camera_test_obj_t *test_obj);
extern mm_camera_stream_t * mm_app_add_snapshot_stream(
                                                mm_camera_test_obj_t *test_obj,
                                                mm_camera_channel_t *channel,
                                                mm_camera_buf_notify_t stream_cb,
                                                void *userdata,
                                                uint8_t num_bufs,
                                                uint8_t num_burst);
extern mm_camera_stream_t * mm_app_add_metadata_stream(mm_camera_test_obj_t *test_obj,
                                               mm_camera_channel_t *channel,
                                               mm_camera_buf_notify_t stream_cb,
                                               void *userdata,
                                               uint8_t num_bufs);
extern int mm_app_start_record_preview(mm_camera_test_obj_t *test_obj);
extern int mm_app_stop_record_preview(mm_camera_test_obj_t *test_obj);
extern int mm_app_start_record(mm_camera_test_obj_t *test_obj);
extern int mm_app_stop_record(mm_camera_test_obj_t *test_obj);
extern int mm_app_start_live_snapshot(mm_camera_test_obj_t *test_obj);
extern int mm_app_stop_live_snapshot(mm_camera_test_obj_t *test_obj);
extern int mm_app_start_capture(mm_camera_test_obj_t *test_obj,
                                uint8_t num_snapshots);
extern int mm_app_stop_capture(mm_camera_test_obj_t *test_obj);
extern int mm_app_start_capture_raw(mm_camera_test_obj_t *test_obj,
                                    uint8_t num_snapshots);
extern int mm_app_stop_capture_raw(mm_camera_test_obj_t *test_obj);
extern int mm_app_start_rdi(mm_camera_test_obj_t *test_obj, uint8_t num_burst);
extern int mm_app_stop_rdi(mm_camera_test_obj_t *test_obj);
extern int mm_app_initialize_fb(mm_camera_test_obj_t *test_obj);
extern int mm_app_close_fb(mm_camera_test_obj_t *test_obj);
extern int mm_app_fb_write(mm_camera_test_obj_t *test_obj, char *buffer);
extern int mm_app_overlay_display(mm_camera_test_obj_t *test_obj, int bufferFd);
extern int mm_app_allocate_ion_memory(mm_camera_app_buf_t *buf, unsigned int ion_type);
extern int mm_app_deallocate_ion_memory(mm_camera_app_buf_t *buf);
extern int mm_app_set_params(mm_camera_test_obj_t *test_obj,
                      cam_intf_parm_type_t param_type,
                      int32_t value);
extern int mm_app_set_preview_fps_range(mm_camera_test_obj_t *test_obj,
                        cam_fps_range_t *fpsRange);
/* JIG camera lib interface */

int mm_camera_lib_open(mm_camera_lib_handle *handle, int cam_id);
int mm_camera_lib_get_caps(mm_camera_lib_handle *handle,
                           cam_capability_t *caps);
int mm_camera_lib_start_stream(mm_camera_lib_handle *handle);
int mm_camera_lib_send_command(mm_camera_lib_handle *handle,
                               mm_camera_lib_commands cmd,
                               void *data, void *out_data);
int mm_camera_lib_stop_stream(mm_camera_lib_handle *handle);
int mm_camera_lib_number_of_cameras(mm_camera_lib_handle *handle);
int mm_camera_lib_close(mm_camera_lib_handle *handle);
int32_t mm_camera_load_tuninglibrary(
  mm_camera_tuning_lib_params_t *tuning_param);
int mm_camera_lib_set_preview_usercb(
  mm_camera_lib_handle *handle, prev_callback cb);
//

int mm_app_start_regression_test(int run_tc);
int mm_app_load_hal(mm_camera_app_t *my_cam_app);

extern int createEncodingSession(mm_camera_test_obj_t *test_obj,
                          mm_camera_stream_t *m_stream,
                          mm_camera_buf_def_t *m_frame);
extern int encodeData(mm_camera_test_obj_t *test_obj, mm_camera_super_buf_t* recvd_frame,
               mm_camera_stream_t *m_stream);
extern int mm_app_take_picture(mm_camera_test_obj_t *test_obj, uint8_t);

extern mm_camera_channel_t * mm_app_add_reprocess_channel(mm_camera_test_obj_t *test_obj,
                                                   mm_camera_stream_t *source_stream);
extern int mm_app_start_reprocess(mm_camera_test_obj_t *test_obj);
extern int mm_app_stop_reprocess(mm_camera_test_obj_t *test_obj);
extern int mm_app_do_reprocess(mm_camera_test_obj_t *test_obj,
        mm_camera_buf_def_t *frame,
        uint32_t meta_idx,
        mm_camera_super_buf_t *super_buf,
        mm_camera_stream_t *src_meta);
extern void mm_app_release_ppinput(void *data, void *user_data);

extern int mm_camera_queue_init(mm_camera_queue_t *queue,
                         release_data_fn data_rel_fn,
                         void *user_data);
extern int mm_qcamera_queue_release(mm_camera_queue_t *queue);
extern int mm_qcamera_queue_isempty(mm_camera_queue_t *queue);
extern int mm_qcamera_queue_enqueue(mm_camera_queue_t *queue, void *data);
extern void* mm_qcamera_queue_dequeue(mm_camera_queue_t *queue,
                                      int bFromHead);
extern void mm_qcamera_queue_flush(mm_camera_queue_t *queue);

#endif /* __MM_QCAMERA_APP_H__ */









