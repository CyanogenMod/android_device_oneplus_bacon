/*
Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __MM_CAMERA_INTERFACE_H__
#define __MM_CAMERA_INTERFACE_H__
#include <linux/msm_ion.h>
#include <camera.h>
#include "QCamera_Intf.h"
#include "cam_list.h"

#define MM_CAMERA_MAX_NUM_FRAMES 16
#define MM_CAMERA_MAX_2ND_SENSORS 4

typedef enum {
    MM_CAMERA_OPS_LOCAL = -1,  /*no need to query mm-camera*/
    MM_CAMERA_OPS_STREAMING_PREVIEW = 0,
    MM_CAMERA_OPS_STREAMING_ZSL,
    MM_CAMERA_OPS_STREAMING_VIDEO,
    MM_CAMERA_OPS_CAPTURE, /*not supported*/
    MM_CAMERA_OPS_FOCUS,
    MM_CAMERA_OPS_GET_PICTURE, /*5*/
    MM_CAMERA_OPS_PREPARE_SNAPSHOT,
    MM_CAMERA_OPS_SNAPSHOT,
    MM_CAMERA_OPS_LIVESHOT,
    MM_CAMERA_OPS_RAW_SNAPSHOT,
    MM_CAMERA_OPS_VIDEO_RECORDING, /*10*/
    MM_CAMERA_OPS_REGISTER_BUFFER,
    MM_CAMERA_OPS_UNREGISTER_BUFFER,
    MM_CAMERA_OPS_CAPTURE_AND_ENCODE,
    MM_CAMERA_OPS_RAW_CAPTURE,
    MM_CAMERA_OPS_ENCODE, /*15*/
    MM_CAMERA_OPS_ZSL_STREAMING_CB,
    /* add new above*/
    MM_CAMERA_OPS_MAX
}mm_camera_ops_type_t;

typedef enum {
    MM_CAMERA_STREAM_OFFSET,
    MM_CAMERA_STREAM_CID,
    MM_CAMERA_STREAM_CROP
}mm_camera_stream_parm_t;

typedef struct {
   int32_t width;
   int32_t height;
} mm_camera_dimension_t;

typedef struct{
    uint32_t tbl_size;
    struct camera_size_type *sizes_tbl;
}default_sizes_tbl_t;

typedef struct  {
    int32_t left;
    int32_t top;
    int32_t width;
    int32_t height;
} mm_camera_rect_t;

/* TBD: meta header needs to move to msm_camear.h */
typedef enum {
    /* no meta data header used */
    MM_CAMEAR_META_DATA_TYPE_DEF,
    /* has focus flag, exposure idx,
     * flash flag, etc. TBD */
    MM_CAMEAR_META_DATA_TYPE_BRACKETING,
    MM_CAMEAR_META_DATA_TYPE_MAX,
} mm_camera_meta_header_t;

typedef struct {
    cam_format_t fmt;
    mm_camera_meta_header_t meta_header;
    uint32_t width;
    uint32_t height;
    uint8_t rotation;
} mm_camera_image_fmt_t;

typedef struct {
    /* image format */
    mm_camera_image_fmt_t fmt;

    /* num of buffers needed */
    uint8_t num_of_bufs;

    /* flag to indicate if this stream need stream on */
    uint8_t need_stream_on;

    /* num of CB needs to be registered on other stream,
     * this field is valid only when need_stream_on is 0.
     * When need_stream_on = 1, num_stream_cb_times will be ignored. */
    uint8_t num_stream_cb_times;
} mm_camera_stream_config_t;

typedef struct {
    uint8_t name[32];
    int32_t min_value;
    int32_t max_value;
    int32_t step;
    int32_t default_value;
} mm_camera_ctrl_cap_sharpness_t;

typedef struct {
    int16_t *zoom_ratio_tbl;
    int32_t size;
} mm_camera_zoom_tbl_t;

typedef enum {
    /* ZSL use case: get burst of frames */
    MM_CAMERA_SUPER_BUF_NOTIFY_BURST = 0,
    /* get continuous frames: when the super buf is
     * ready dispatch it to HAL */
    MM_CAMERA_SUPER_BUF_NOTIFY_CONTINUOUS,
    MM_CAMERA_SUPER_BUF_NOTIFY_MAX
} mm_camera_super_buf_notify_mode_t;

typedef enum {
    /* save the frame. No matter focused or not */
    MM_CAMERA_SUPER_BUF_PRIORITY_NORMAL = 0,
    /* only queue the frame that is focused. Will enable
     * meta data header to carry focus info*/
    MM_CAMERA_SUPER_BUF_PRIORITY_FOCUS,
    /* after shutter, only queue matched exposure index */
    MM_CAMERA_SUPER_BUF_PRIORITY_EXPOSURE_BRACKETING,
    MM_CAMERA_SUPER_BUF_PRIORITY_MAX
} mm_camera_super_buf_priority_t;

typedef struct {
    mm_camera_super_buf_notify_mode_t notify_mode;
    /* queue depth. Only for burst mode */
    uint8_t water_mark;
    /* look back how many frames from last buf */
    uint8_t look_back;
    /* after send first frame to HAL, how many frames
     * needing to be skipped for next delivery? */
    uint8_t post_frame_skip;
    /* save matched priority frames only */
    mm_camera_super_buf_priority_t priority;
    /* in burst mode, how many super
     * bufs for each shutter press? */
    uint8_t burst_num;
} mm_camera_bundle_attr_t;

typedef struct {
    uint8_t camera_id;                   /* camera id */
    camera_info_t camera_info;           /* postion, mount_angle, etc. */
    enum sensor_type_t main_sensor_type; /* BAYER, YUV, JPEG_SOC, etc. */
    char *video_dev_name;                /* device node name, e.g. /dev/video1 */
} mm_camera_info_t;

typedef struct {
  uint8_t cid;
  uint8_t dt;
}stream_cid_t;

/* the stats struct will need rework after
 * defining the stats buf sharing logic */
typedef struct {
    int type;
    uint32_t length;
    void *value;
} mm_camera_stats_t;

typedef struct {
    uint32_t stream_id; /* stream handler */
    int8_t buf_idx; /* buf idx within the stream bufs */

    struct timespec ts; /* time stamp, to be filled when DQBUF*/
    uint32_t frame_idx; /* frame sequence num, to be filled when DQBUF */

    int8_t num_planes; /* num of planes, to be filled during mem allocation */
    struct v4l2_plane planes[VIDEO_MAX_PLANES]; /* plane info, to be filled during mem allocation*/
    int fd; /* fd of the frame, to be filled during mem allocation */
    void *buffer; /* ptr to real frame buffer, to be filled during mem allocation */
    uint32_t frame_len; /* len of the whole frame, to be filled during mem allocation */
    void *mem_info; /* reserved for pointing to mem info */
    cam_exif_tags_t *p_mobicat_info; /*for mobicat info*/
} mm_camera_buf_def_t;

typedef struct {
    uint32_t camera_handle;
    uint32_t ch_id;
    uint8_t num_bufs;
    mm_camera_buf_def_t* bufs[MM_CAMEAR_MAX_STRAEM_BUNDLE];
} mm_camera_super_buf_t;

typedef void (*mm_camera_event_notify_t)(uint32_t camera_handle,
                                         mm_camera_event_t *evt,
                                         void *user_data);

typedef void (*mm_camera_buf_notify_t) (mm_camera_super_buf_t *bufs,
                                        void *user_data);
typedef cam_frame_len_offset_t mm_camera_frame_len_offset;

typedef struct {
  void *user_data;
  int32_t (*get_buf) (uint32_t camera_handle,
                      uint32_t ch_id, uint32_t stream_id,
                      void *user_data,
                      mm_camera_frame_len_offset *frame_offset_info,
                      uint8_t num_bufs,
                      uint8_t *initial_reg_flag,
                      mm_camera_buf_def_t *bufs);
  int32_t (*put_buf) (uint32_t camera_handle,
                      uint32_t ch_id, uint32_t stream_id,
                      void *user_data, uint8_t num_bufs,
                      mm_camera_buf_def_t *bufs);
} mm_camear_mem_vtbl_t;

typedef struct {
    uint8_t num_2nd_sensors;
    uint8_t sensor_idxs[MM_CAMERA_MAX_2ND_SENSORS];
} mm_camera_2nd_sensor_t;

typedef enum {
    NATIVE_CMD_ID_SOCKET_MAP,
    NATIVE_CMD_ID_SOCKET_UNMAP,
    NATIVE_CMD_ID_IOCTL_CTRL,
    NATIVE_CMD_ID_MAX
} mm_camera_native_cmd_id_t;

typedef enum {
    MM_CAMERA_CMD_TYPE_PRIVATE,   /* OEM private ioctl */
    MM_CAMERA_CMD_TYPE_NATIVE     /* native ctrl cmd through ioctl */
} mm_camera_cmd_type_t;

typedef struct {
    mm_camera_buf_def_t *src_frame; /* src frame */
} mm_camera_repro_data_t;

typedef mm_camera_repro_cmd_config_t mm_camera_repro_isp_config_t;

typedef struct {
    /* to sync the internal camera settings */
    int32_t (*sync) (uint32_t camera_handle);
    uint8_t (*is_event_supported) (uint32_t camera_handle,
                                 mm_camera_event_type_t evt_type);
    int32_t (*register_event_notify) (uint32_t camera_handle,
                                 mm_camera_event_notify_t evt_cb,
                                 void * user_data,
                                 mm_camera_event_type_t evt_type);
    int32_t (*qbuf) (uint32_t camera_handle, uint32_t ch_id,
                                 mm_camera_buf_def_t *buf);
    void (*camera_close) (uint32_t camera_handle);
    /* Only fo supporting advanced 2nd sensors. If no secondary sensor needed
     * HAL can ignore this function */
    mm_camera_2nd_sensor_t * (*query_2nd_sensor_info) (uint32_t camera_handle);

    /* if the operation is supported: TRUE - support, FALSE - not support */
    uint8_t (*is_op_supported)(uint32_t camera_handle, mm_camera_ops_type_t opcode);

    /* if the parm is supported: TRUE - support, FALSE - not support */
    int32_t (*is_parm_supported) (uint32_t camera_handle,
                                 mm_camera_parm_type_t parm_type,
                                 uint8_t *support_set_parm,
                                 uint8_t *support_get_parm);
    /* set a parm current value */
    int32_t (*set_parm) (uint32_t camera_handle,
                         mm_camera_parm_type_t parm_type,
                         void* p_value);
    /* get a parm current value */
    int32_t (*get_parm) (uint32_t camera_handle,
                         mm_camera_parm_type_t parm_type,
                         void* p_value);
    /* ch_id returned, zero is invalid ch_id */
    uint32_t (*ch_acquire) (uint32_t camera_handle);
    /* relaese channel */
    void (*ch_release) (uint32_t camera_handle, uint32_t ch_id);
    /* return stream_id. zero is invalid stream_id
     * default set to preview: ext_image_mode = 0
     * default set to primary sensor: sensor_idx = 0
     * value of ext_image_mode is defined in msm_camera.h
     */
    uint32_t (*add_stream) (uint32_t camera_handle, uint32_t ch_id,
                           mm_camera_buf_notify_t buf_cb, void *user_data,
                           uint32_t ext_image_mode, uint32_t sensor_idx);
    /* delete stream */
    int32_t (*del_stream) (uint32_t camera_handle, uint32_t ch_id,
                           uint32_t stream_id);
    /* set straem format This will trigger getting bufs from HAL */
    int32_t (*config_stream) (uint32_t camera_handle, uint32_t ch_id,
                              uint32_t stream_id,
                              mm_camera_stream_config_t *config);
    /* setup super buf bundle for ZSL(with burst mode) or other use cases */
    int32_t (*init_stream_bundle) (uint32_t camera_handle, uint32_t ch_id,
                                   mm_camera_buf_notify_t super_frame_notify_cb,
                                   void *user_data,  mm_camera_bundle_attr_t *attr,
                                   uint8_t num_streams, uint32_t *stream_ids);
    /* remove the super buf bundle */
    int32_t (*destroy_stream_bundle) (uint32_t camera_handle, uint32_t ch_id);
    /* start streaming */
    int32_t (*start_streams) (uint32_t camera_handle, uint32_t ch_id,
                              uint8_t num_streams, uint32_t *stream_ids);
    /* stop streaming */
    int32_t (*stop_streams) (uint32_t camera_handle, uint32_t ch_id,
                             uint8_t num_streams, uint32_t *stream_ids);
    /* tear down streams asyncly */
    int32_t (*async_teardown_streams) (uint32_t camera_handle, uint32_t ch_id,
                                       uint8_t num_streams, uint32_t *stream_ids);
    /* get super bufs. for burst mode only */
    int32_t (*request_super_buf) (uint32_t camera_handle,
                                  uint32_t ch_id,
                                  uint32_t num_buf_requested);
    /* abort the super buf dispatching. for burst mode only  */
    int32_t (*cancel_super_buf_request) (uint32_t camera_handle,
                                         uint32_t ch_id);
    /* start focus: by default sensor_idx=0 */
    int32_t (*start_focus) (uint32_t camera_handle,
                            uint32_t ch_id,
                            uint32_t sensor_idx,
                            uint32_t focus_mode);
    /* abort focus: by default sensor_idx=0 */
    int32_t (*abort_focus) (uint32_t camera_handle,
                            uint32_t ch_id,
                            uint32_t sensor_idx);
    /* prepare hardware will settle aec and flash.
     * by default sensor_idx=0 */
    int32_t (*prepare_snapshot) (uint32_t camera_handle,
                                 uint32_t ch_id,
                                 uint32_t sensor_idx);
    /* set a parm current value of a stream */
    int32_t (*set_stream_parm) (uint32_t camera_handle,
                                uint32_t ch_id,
                                uint32_t s_id,
                                mm_camera_stream_parm_t parm_type,
                                void *p_value);
    /* get a parm current value of a stream */
    int32_t (*get_stream_parm) (uint32_t camera_handle,
                                uint32_t ch_id,
                                uint32_t s_id,
                                mm_camera_stream_parm_t parm_type,
                                void *p_value);
    /* private communication tunnel */
    int32_t (*send_command) (uint32_t camera_handle,
                             mm_camera_cmd_type_t cmd_type,
                             uint32_t cmd_id,
                             uint32_t cmd_length,
                             void *cmd);
    /* open a re-process isp, return handler for repro isp (>0).
     * if failed, reutrn 0 */
    uint32_t (*open_repro_isp) (uint32_t camera_handle,
                               uint32_t ch_id,
                               mm_camera_repro_isp_type_t repro_isp_type);
    /* config the re-process isp */
    int32_t (*config_repro_isp) (uint32_t camera_handle,
                                 uint32_t ch_id,
                                 uint32_t repro_isp_handle,
                                 mm_camera_repro_isp_config_t *config);
    /* attach output stream to the re-process isp */
    int32_t (*attach_stream_to_repro_isp) (uint32_t camera_handle,
                                           uint32_t ch_id,
                                           uint32_t repro_isp_handle,
                                           uint32_t stream_id);
    /* start a re-process isp. */
    int32_t (*start_repro_isp) (uint32_t camera_handle,
                                uint32_t ch_id,
                                uint32_t repro_isp_handle,
                                uint32_t stream_id);
    /* start a reprocess job for a src frame.
     * Only after repo_isp is started, reprocess API can be called */
    int32_t (*reprocess) (uint32_t camera_handle,
                          uint32_t ch_id,
                          uint32_t repro_isp_handle,
                          mm_camera_repro_data_t *repo_data);
    /* stop a re-process isp */
    int32_t (*stop_repro_isp) (uint32_t camera_handle,
                               uint32_t ch_id,
                               uint32_t repro_isp_handle,
                               uint32_t stream_id);
    /* detach an output stream from the re-process isp.
     * Can only be called after the re-process isp is stopped */
    int32_t (*detach_stream_from_repro_isp) (uint32_t camera_handle,
                                             uint32_t ch_id,
                                             uint32_t repro_isp_handle,
                                             uint32_t stream_id);
    /* close a re-process isp.
     * Can only close after all dest streams are detached from it */
    int32_t (*close_repro_isp) (uint32_t camera_handle,
                                uint32_t ch_id,
                                uint32_t repro_isp_handle);
} mm_camera_ops_t;

typedef struct {
    uint32_t camera_handle;        /* camera object handle */
    mm_camera_info_t *camera_info; /* reference pointer of camear info */
    mm_camera_ops_t *ops;          /* API call table */
} mm_camera_vtbl_t;

mm_camera_info_t * camera_query(uint8_t *num_cameras);
mm_camera_vtbl_t * camera_open(uint8_t camera_idx,
                               mm_camear_mem_vtbl_t *mem_vtbl);

//extern void mm_camera_util_profile(const char *str);

typedef enum {
    MM_CAMERA_PREVIEW,
    MM_CAMERA_VIDEO,
    MM_CAMERA_SNAPSHOT_MAIN,
    MM_CAMERA_SNAPSHOT_THUMBNAIL,
    MM_CAMERA_SNAPSHOT_RAW,
    MM_CAMERA_RDI,
    MM_CAMERA_RDI1,
    MM_CAMERA_RDI2,
    MM_CAMERA_SAEC,
    MM_CAMERA_SAWB,
    MM_CAMERA_SAFC,
    MM_CAMERA_IHST,
    MM_CAMERA_CS,
    MM_CAMERA_RS,
    MM_CAMERA_CSTA,
    MM_CAMERA_ISP_PIX_OUTPUT1,
    MM_CAMERA_ISP_PIX_OUTPUT2,
    MM_CAMERA_IMG_MODE_MAX
} mm_camera_img_mode;

/* may remove later */
typedef enum {
    MM_CAMERA_OP_MODE_NOTUSED,
    MM_CAMERA_OP_MODE_CAPTURE,
    MM_CAMERA_OP_MODE_VIDEO,
    MM_CAMERA_OP_MODE_ZSL,
    MM_CAMERA_OP_MODE_RAW,
    MM_CAMERA_OP_MODE_MAX
} mm_camera_op_mode_type_t;

#endif /*__MM_CAMERA_INTERFACE_H__*/
