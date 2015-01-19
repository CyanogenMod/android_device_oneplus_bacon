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

#ifndef __MM_CAMERA_H__
#define __MM_CAMERA_H__

#include "mm_camera_interface.h"

/**********************************************************************************
* Data structure declare
***********************************************************************************/
/* num of streams allowed in a channel obj */
//TODO: change for Stats
#define MM_CAMEAR_STRAEM_NUM_MAX (16)
/* num of channels allowed in a camera obj */
#define MM_CAMERA_CHANNEL_MAX 1
/* num of callbacks allowed for an event type */
#define MM_CAMERA_EVT_ENTRY_MAX 4
/* num of data callbacks allowed in a stream obj */
#define MM_CAMERA_STREAM_BUF_CB_MAX 4
/* num of data poll threads allowed in a channel obj */
#define MM_CAMERA_CHANNEL_POLL_THREAD_MAX 1

#define MM_CAMERA_DEV_NAME_LEN 32
#define MM_CAMERA_DEV_OPEN_TRIES 2
#define MM_CAMERA_DEV_OPEN_RETRY_SLEEP 20

struct mm_channel;
struct mm_stream;
struct mm_camera_obj;

/* common use */
typedef struct {
    struct cam_list list;
    void* data;
} mm_camera_q_node_t;

typedef struct {
    mm_camera_q_node_t head; /* dummy head */
    uint32_t size;
    pthread_mutex_t lock;
} mm_camera_queue_t;

typedef enum
{
    MM_CAMERA_ASYNC_CMD_TYPE_STOP,    /* async stop */
    MM_CAMERA_ASYNC_CMD_TYPE_MAX
} mm_camera_async_cmd_type_t;

typedef struct {
    struct mm_channel* ch_obj;
    uint8_t num_streams;
    uint32_t stream_ids[MM_CAMEAR_STRAEM_NUM_MAX];
} mm_camera_async_stop_cmd_t;

typedef struct {
    mm_camera_async_cmd_type_t cmd_type;
    union {
        mm_camera_async_stop_cmd_t stop_cmd;
    } u;
} mm_camera_async_cmd_t;

typedef enum
{
    MM_CAMERA_CMD_TYPE_DATA_CB,    /* dataB CMD */
    MM_CAMERA_CMD_TYPE_EVT_CB,     /* evtCB CMD */
    MM_CAMERA_CMD_TYPE_ASYNC_CB,   /* asyncCB CMD */
    MM_CAMERA_CMD_TYPE_EXIT,       /* EXIT */
    MM_CAMERA_CMD_TYPE_REQ_DATA_CB,/* request data */
    MM_CAMERA_CMD_TYPE_SUPER_BUF_DATA_CB,    /* superbuf dataB CMD */
    MM_CAMERA_CMD_TYPE_MAX
} mm_camera_cmdcb_type_t;

typedef struct {
    uint32_t stream_id;
    uint32_t frame_idx;
    mm_camera_buf_def_t *buf; /* ref to buf */
} mm_camera_buf_info_t;

typedef struct {
    uint32_t num_buf_requested;
} mm_camera_req_buf_t;

typedef struct {
    mm_camera_cmdcb_type_t cmd_type;
    union {
        mm_camera_buf_info_t buf;    /* frame buf if dataCB */
        mm_camera_event_t evt;       /* evt if evtCB */
        mm_camera_async_cmd_t async; /* async cmd */
        mm_camera_super_buf_t superbuf; /* superbuf if superbuf dataCB*/
        mm_camera_req_buf_t req_buf; /* num of buf requested */
    } u;
} mm_camera_cmdcb_t;

typedef void (*mm_camera_cmd_cb_t)(mm_camera_cmdcb_t * cmd_cb, void* user_data);

typedef struct {
    mm_camera_queue_t cmd_queue; /* cmd queue (queuing dataCB, asyncCB, or exitCMD) */
    pthread_t cmd_pid;           /* cmd thread ID */
    sem_t cmd_sem;               /* semaphore for cmd thread */
    mm_camera_cmd_cb_t cb;       /* cb for cmd */
    void* user_data;             /* user_data for cb */
} mm_camera_cmd_thread_t;

typedef enum {
    MM_CAMERA_POLL_TYPE_EVT,
    MM_CAMERA_POLL_TYPE_CH,
    MM_CAMERA_POLL_TYPE_MAX
} mm_camera_poll_thread_type_t;

/* function ptr defined for poll notify CB,
 * registered at poll thread with poll fd */
typedef void (*mm_camera_poll_notify_t)(void *user_data);

typedef struct {
    int32_t fd;
    mm_camera_poll_notify_t notify_cb;
    uint32_t handler;
    void* user_data;
} mm_camera_poll_entry_t;

typedef struct {
    mm_camera_poll_thread_type_t poll_type;
    /* array to store poll fd and cb info
     * for MM_CAMERA_POLL_TYPE_EVT, only index 0 is valid;
     * for MM_CAMERA_POLL_TYPE_CH, depends on valid stream fd */
    mm_camera_poll_entry_t poll_entries[MM_CAMEAR_STRAEM_NUM_MAX];
    int32_t pfds[2];
    pthread_t pid;
    int32_t state;
    int timeoutms;
    uint32_t cmd;
    struct pollfd poll_fds[MM_CAMEAR_STRAEM_NUM_MAX+1];
    uint8_t num_fds;
    pthread_mutex_t mutex;
    pthread_cond_t cond_v;
    int32_t status;
    //void *my_obj;
} mm_camera_poll_thread_t;

/* mm_stream */
typedef enum {
    MM_STREAM_STATE_NOTUSED = 0,      /* not used */
    MM_STREAM_STATE_INITED,           /* inited  */
    MM_STREAM_STATE_ACQUIRED,         /* acquired, fd opened  */
    MM_STREAM_STATE_CFG,              /* fmt & dim configured */
    MM_STREAM_STATE_BUFFED,           /* buf allocated */
    MM_STREAM_STATE_REG,              /* buf regged, stream off */
    MM_STREAM_STATE_ACTIVE_STREAM_ON, /* active with stream on */
    MM_STREAM_STATE_ACTIVE_STREAM_OFF, /* active with stream off */
    MM_STREAM_STATE_MAX
} mm_stream_state_type_t;

typedef enum {
    MM_STREAM_EVT_ACQUIRE,
    MM_STREAM_EVT_RELEASE,
    MM_STREAM_EVT_SET_FMT,
    MM_STREAM_EVT_GET_BUF,
    MM_STREAM_EVT_PUT_BUF,
    MM_STREAM_EVT_REG_BUF,
    MM_STREAM_EVT_UNREG_BUF,
    MM_STREAM_EVT_START,
    MM_STREAM_EVT_STOP,
    MM_STREAM_EVT_QBUF,
    MM_STREAM_EVT_SET_PARM,
    MM_STREAM_EVT_GET_PARM,
    MM_STREAM_EVT_MAX
} mm_stream_evt_type_t;

typedef struct {
    mm_camera_buf_notify_t cb;
    void *user_data;
    /* cb_count = -1: infinite
     * cb_count > 0: register only for required times */
    int8_t cb_count;
} mm_stream_data_cb_t;

typedef struct {
    /* buf reference count */
    uint8_t buf_refcnt;

    /* This flag is to indicate if after allocation,
     * the corresponding buf needs to qbuf into kernel
     * (e.g. for preview usecase, display needs to hold two bufs,
     * so no need to qbuf these two bufs initially) */
    uint8_t initial_reg_flag;

    /* indicate if buf is in kernel(1) or client(0) */
    uint8_t in_kernel;
} mm_stream_buf_status_t;

typedef struct mm_stream {
    uint32_t my_hdl;
    uint32_t inst_hdl;
    int32_t fd;
    mm_stream_state_type_t state;

    /* ext_image_mode used as id for stream obj */
    uint32_t ext_image_mode;

    /* sensor index used */
    uint32_t sensor_idx;

    mm_camera_image_fmt_t fmt;

    mm_camera_cmd_thread_t cmd_thread;

    /* dataCB registered on this stream obj */
    pthread_mutex_t cb_lock; /* cb lock to protect buf_cb */
    mm_stream_data_cb_t buf_cb[MM_CAMERA_STREAM_BUF_CB_MAX];

    /* stream buffer management */
    pthread_mutex_t buf_lock;
    uint8_t buf_num; /* num of buffers allocated */
    mm_camera_buf_def_t* buf; /* ptr to buf array */
    mm_stream_buf_status_t* buf_status; /* ptr to buf status array */

    /* reference to parent channel_obj */
    struct mm_channel* ch_obj;

    uint8_t is_bundled; /* flag if stream is bundled */
    uint8_t is_local_buf; /* flag if buf is local copy, no need to qbuf to kernel */
    uint8_t hal_requested_num_bufs;
    uint8_t need_stream_on; /* flag if stream need streamon when start */
    uint8_t num_stream_cb_times; /* how many times to register for other stream data CB */
    uint8_t local_buf_idx; /* idx to local buf that can be used for non-stream-on cb */

    mm_camera_frame_len_offset frame_offset; /*Stream buffer offset information*/
} mm_stream_t;

/* mm_channel */
typedef enum {
    MM_CHANNEL_STATE_NOTUSED = 0,   /* not used */
    MM_CHANNEL_STATE_STOPPED,       /* stopped */
    MM_CHANNEL_STATE_ACTIVE,        /* active, at least one stream active */
    MM_CHANNEL_STATE_PAUSED,        /* paused */
    MM_CHANNEL_STATE_MAX
} mm_channel_state_type_t;

typedef enum {
    MM_CHANNEL_EVT_ADD_STREAM,
    MM_CHANNEL_EVT_DEL_STREAM,
    MM_CHANNEL_EVT_START_STREAM,
    MM_CHANNEL_EVT_STOP_STREAM,
    MM_CHANNEL_EVT_TEARDOWN_STREAM,
    MM_CHANNEL_EVT_CONFIG_STREAM,
    MM_CHANNEL_EVT_PAUSE,
    MM_CHANNEL_EVT_RESUME,
    MM_CHANNEL_EVT_INIT_BUNDLE,
    MM_CHANNEL_EVT_DESTROY_BUNDLE,
    MM_CHANNEL_EVT_REQUEST_SUPER_BUF,
    MM_CHANNEL_EVT_CANCEL_REQUEST_SUPER_BUF,
    MM_CHANNEL_EVT_START_FOCUS,
    MM_CHANNEL_EVT_ABORT_FOCUS,
    MM_CHANNEL_EVT_PREPARE_SNAPSHOT,
    MM_CHANNEL_EVT_SET_STREAM_PARM,
    MM_CHANNEL_EVT_GET_STREAM_PARM,
    MM_CHANNEL_EVT_DELETE,
    MM_CHANNEL_EVT_OPEN_REPRO_ISP,
    MM_CHANNEL_EVT_CONFIG_REPRO_ISP,
    MM_CHANNEL_EVT_ATTACH_STREAM_TO_REPRO_ISP,
    MM_CHANNEL_EVT_START_REPRO_ISP,
    MM_CHANNEL_EVT_REPROCESS,
    MM_CHANNEL_EVT_STOP_REPRO_ISP,
    MM_CHANNEL_EVT_DETACH_STREAM_FROM_REPRO_ISP,
    MM_CHANNEL_EVT_CLOSE_REPRO_ISP,
    MM_CHANNEL_EVT_MAX
} mm_channel_evt_type_t;

typedef struct {
    mm_camera_buf_notify_t buf_cb;
    void *user_data;
    uint32_t ext_image_mode;
    uint32_t sensor_idx;
} mm_evt_paylod_add_stream_t;

typedef struct {
    uint32_t stream_id;
    mm_camera_stream_config_t *config;
} mm_evt_paylod_config_stream_t;

typedef struct {
    mm_camera_stream_parm_t parm_type;
    void *value;
} mm_evt_paylod_stream_parm_t;

typedef struct {
    mm_camera_buf_notify_t super_frame_notify_cb;
    void *user_data;
    mm_camera_bundle_attr_t *attr;
    uint8_t num_streams;
    uint32_t *stream_ids;
} mm_evt_payload_bundle_stream_t;

typedef struct {
    uint8_t num_streams;
    uint32_t *stream_ids;
} mm_evt_payload_start_stream_t;

typedef struct {
    uint8_t num_streams;
    uint32_t *stream_ids;
} mm_evt_payload_stop_stream_t;

typedef struct {
    uint32_t sensor_idx;
    uint32_t focus_mode;
} mm_evt_payload_start_focus_t;

typedef struct {
    uint32_t repro_isp_handle;
    mm_camera_repro_isp_config_t *config;
} mm_evt_paylod_config_repro_isp_t;

typedef struct {
    uint32_t repro_isp_handle;
    uint32_t stream_id;
} mm_evt_paylod_stream_to_repro_isp_t;

typedef struct {
    uint32_t repro_isp_handle;
    mm_camera_repro_data_t *repro_data;
} mm_evt_paylod_reprocess_t;

typedef struct {
    uint32_t repro_isp_handle;
    uint32_t stream_id;
} mm_evt_paylod_repro_start_stop_t;

typedef struct {
    uint8_t num_of_bufs;
    mm_camera_buf_info_t super_buf[MM_CAMEAR_MAX_STRAEM_BUNDLE];
    uint8_t matched;
} mm_channel_queue_node_t;

typedef struct {
    mm_camera_queue_t que;
    uint8_t num_streams;
    /* container for bundled stream handlers */
    uint32_t bundled_streams[MM_CAMEAR_MAX_STRAEM_BUNDLE];
    mm_camera_bundle_attr_t attr;
    uint32_t expected_frame_id;
    uint32_t match_cnt;
} mm_channel_queue_t;

typedef struct {
    uint8_t is_active; /* flag to indicate if bundle is valid */
    /* queue to store bundled super buffers */
    mm_channel_queue_t superbuf_queue;
    mm_camera_buf_notify_t super_buf_notify_cb;
    void *user_data;
} mm_channel_bundle_t;

typedef struct mm_channel {
    uint32_t my_hdl;
    mm_channel_state_type_t state;
    pthread_mutex_t ch_lock; /* channel lock */

    /* stream bundle info in the channel */
    mm_channel_bundle_t bundle;

    /* num of pending suferbuffers */
    uint32_t pending_cnt;

    /* cmd thread for superbuffer dataCB and async stop*/
    mm_camera_cmd_thread_t cmd_thread;

    /* cb thread for sending data cb */
    mm_camera_cmd_thread_t cb_thread;

    /* data poll thread
    * currently one data poll thread per channel
    * could extended to support one data poll thread per stream in the channel */
    mm_camera_poll_thread_t poll_thread[MM_CAMERA_CHANNEL_POLL_THREAD_MAX];

    /* container for all streams in channel
    * stream is indexed by ext_image_mode */
    mm_stream_t streams[MM_CAMEAR_STRAEM_NUM_MAX];

    /* reference to parent cam_obj */
    struct mm_camera_obj* cam_obj;
} mm_channel_t;

/* struct to store information about pp cookie*/
typedef struct {
    uint32_t cam_hdl;
    uint32_t ch_hdl;
    uint32_t stream_hdl;
    mm_channel_queue_node_t* super_buf;
} mm_channel_pp_info_t;

/* mm_camera */
typedef struct {
    mm_camera_event_notify_t evt_cb;
    void *user_data;
} mm_camera_evt_entry_t;

typedef struct {
    mm_camera_evt_entry_t evt[MM_CAMERA_EVT_ENTRY_MAX];
    /* reg_count <=0: infinite
     * reg_count > 0: register only for required times */
    int reg_count;
} mm_camera_evt_obj_t;

typedef struct mm_camera_obj {
    uint32_t my_hdl;
    int ref_count;
    int32_t ctrl_fd;
    int32_t ds_fd; /* domain socket fd */
    cam_prop_t properties;
    mm_camera_2nd_sensor_t second_sensor; /*second sensor info */
    pthread_mutex_t cam_lock;
    pthread_mutex_t cb_lock; /* lock for evt cb */
    mm_channel_t ch[MM_CAMERA_CHANNEL_MAX];
    mm_camera_evt_obj_t evt[MM_CAMERA_EVT_TYPE_MAX];
    uint32_t evt_type_mask;
    mm_camear_mem_vtbl_t *mem_vtbl; /* vtable for memory management */
    mm_camera_poll_thread_t evt_poll_thread; /* evt poll thread */
    mm_camera_cmd_thread_t evt_thread;       /* thread for evt CB */
    mm_camera_cmd_thread_t async_cmd_thread; /* thread for async cmd */
    mm_camera_vtbl_t vtbl;

    /* some local variables */
    uint32_t snap_burst_num_by_user;
    camera_mode_t current_mode;
    uint32_t op_mode;
    cam_ctrl_dimension_t dim;
} mm_camera_obj_t;

typedef struct {
    mm_camera_info_t camera[MSM_MAX_CAMERA_SENSORS];
    int8_t num_cam;
    char video_dev_name[MSM_MAX_CAMERA_SENSORS][MM_CAMERA_DEV_NAME_LEN];
    mm_camera_obj_t *cam_obj[MSM_MAX_CAMERA_SENSORS];
} mm_camera_ctrl_t;

/**********************************************************************************
* external function declare
***********************************************************************************/
/* utility functions */
/* set int32_t value */
extern int32_t mm_camera_util_s_ctrl(int32_t fd,
                                     uint32_t id,
                                     int32_t value);

extern int32_t mm_camera_util_private_s_ctrl( int32_t fd,
                                              uint32_t id, void* value);

/* get int32_t value */
extern int32_t mm_camera_util_g_ctrl(int32_t fd,
                                     uint32_t id,int32_t *value);

/* send msg throught domain socket for fd mapping */
extern int32_t mm_camera_util_sendmsg(mm_camera_obj_t *my_obj,
                                      void *msg,
                                      uint32_t buf_size,
                                      int sendfd);

/* mm-camera */
extern int32_t mm_camera_open(mm_camera_obj_t *my_obj);
extern int32_t mm_camera_close(mm_camera_obj_t *my_obj);
extern uint8_t mm_camera_is_event_supported(mm_camera_obj_t *my_obj,
                                            mm_camera_event_type_t evt_type);
extern int32_t mm_camera_register_event_notify(mm_camera_obj_t *my_obj,
                                               mm_camera_event_notify_t evt_cb,
                                               void * user_data,
                                               mm_camera_event_type_t evt_type);
extern int32_t mm_camera_qbuf(mm_camera_obj_t *my_obj,
                              uint32_t ch_id,
                              mm_camera_buf_def_t *buf);
extern mm_camera_2nd_sensor_t * mm_camera_query_2nd_sensor_info(mm_camera_obj_t *my_obj);
extern int32_t mm_camera_sync(mm_camera_obj_t *my_obj);
extern int32_t mm_camera_is_op_supported(mm_camera_obj_t *my_obj,
                                              mm_camera_ops_type_t opcode);
extern int32_t mm_camera_is_parm_supported(mm_camera_obj_t *my_obj,
                                           mm_camera_parm_type_t parm_type,
                                           uint8_t *support_set_parm,
                                           uint8_t *support_get_parm);
extern int32_t mm_camera_set_parm(mm_camera_obj_t *my_obj,
                                  mm_camera_parm_type_t parm_type,
                                  void* p_value);
extern int32_t mm_camera_get_parm(mm_camera_obj_t *my_obj,
                                  mm_camera_parm_type_t parm_type,
                                  void* p_value);
extern uint32_t mm_camera_add_channel(mm_camera_obj_t *my_obj);
extern void mm_camera_del_channel(mm_camera_obj_t *my_obj,
                                  uint32_t ch_id);
extern uint32_t mm_camera_add_stream(mm_camera_obj_t *my_obj,
                                     uint32_t ch_id,
                                     mm_camera_buf_notify_t buf_cb, void *user_data,
                                     uint32_t ext_image_mode, uint32_t sensor_idx);
extern int32_t mm_camera_del_stream(mm_camera_obj_t *my_obj,
                                    uint32_t ch_id,
                                    uint32_t stream_id);
extern int32_t mm_camera_config_stream(mm_camera_obj_t *my_obj,
                                       uint32_t ch_id,
                                       uint32_t stream_id,
                                       mm_camera_stream_config_t *config);
extern int32_t mm_camera_bundle_streams(mm_camera_obj_t *my_obj,
                                        uint32_t ch_id,
                                        mm_camera_buf_notify_t super_frame_notify_cb,
                                        void *user_data,
                                        mm_camera_bundle_attr_t *attr,
                                        uint8_t num_streams,
                                        uint32_t *stream_ids);
extern int32_t mm_camera_destroy_bundle(mm_camera_obj_t *my_obj,
                                        uint32_t ch_id);
extern int32_t mm_camera_start_streams(mm_camera_obj_t *my_obj,
                                       uint32_t ch_id,
                                       uint8_t num_streams,
                                       uint32_t *stream_ids);
extern int32_t mm_camera_stop_streams(mm_camera_obj_t *my_obj,
                                      uint32_t ch_id,
                                      uint8_t num_streams,
                                      uint32_t *stream_ids);
extern int32_t mm_camera_async_teardown_streams(mm_camera_obj_t *my_obj,
                                                uint32_t ch_id,
                                                uint8_t num_streams,
                                                uint32_t *stream_ids);
extern int32_t mm_camera_request_super_buf(mm_camera_obj_t *my_obj,
                                           uint32_t ch_id,
                                           uint32_t num_buf_requested);
extern int32_t mm_camera_cancel_super_buf_request(mm_camera_obj_t *my_obj,
                                                  uint32_t ch_id);
extern int32_t mm_camera_start_focus(mm_camera_obj_t *my_obj,
                                     uint32_t ch_id,
                                     uint32_t sensor_idx,
                                     uint32_t focus_mode);
extern int32_t mm_camera_abort_focus(mm_camera_obj_t *my_obj,
                                     uint32_t ch_id,
                                     uint32_t sensor_idx);
extern int32_t mm_camera_prepare_snapshot(mm_camera_obj_t *my_obj,
                                          uint32_t ch_id,
                                          uint32_t sensor_idx);
extern int32_t mm_camera_set_stream_parm(mm_camera_obj_t *my_obj,
                                  uint32_t ch_id,
                                  uint32_t s_id,
                                  mm_camera_stream_parm_t parm_type,
                                  void* p_value);

extern int32_t mm_camera_get_stream_parm(mm_camera_obj_t *my_obj,
                                  uint32_t ch_id,
                                  uint32_t s_id,
                                  mm_camera_stream_parm_t parm_type,
                                  void* p_value);

extern int32_t mm_camera_register_event_notify_internal(
                                   mm_camera_obj_t *my_obj,
                                   mm_camera_event_notify_t evt_cb,
                                   void * user_data,
                                   mm_camera_event_type_t evt_type);
extern int32_t mm_camera_map_buf(mm_camera_obj_t *my_obj,
                                 int ext_mode,
                                 int idx,
                                 int fd,
                                 uint32_t size);
extern int32_t mm_camera_unmap_buf(mm_camera_obj_t *my_obj,
                                   int ext_mode,
                                   int idx);
extern int32_t mm_camera_send_ch_event(mm_camera_obj_t *my_obj,
                                       uint32_t ch_id,
                                       uint32_t stream_id,
                                       mm_camera_ch_event_type_t evt);
extern int32_t mm_camera_send_private_ioctl(mm_camera_obj_t *my_obj,
                                            uint32_t cmd_id,
                                            uint32_t cmd_length,
                                            void *cmd);
extern int32_t mm_camera_open_repro_isp(mm_camera_obj_t *my_obj,
                                        uint32_t ch_id,
                                        mm_camera_repro_isp_type_t repro_isp_type,
                                        uint32_t *repro_isp_handle);
extern int32_t mm_camera_config_repro_isp(mm_camera_obj_t *my_obj,
                                          uint32_t ch_id,
                                          uint32_t repro_isp_handle,
                                          mm_camera_repro_isp_config_t *config);
extern int32_t mm_camera_attach_stream_to_repro_isp(mm_camera_obj_t *my_obj,
                                                    uint32_t ch_id,
                                                    uint32_t repro_isp_handle,
                                                    uint32_t stream_id);
extern int32_t mm_camera_start_repro_isp(mm_camera_obj_t *my_obj,
                                         uint32_t ch_id,
                                         uint32_t repro_isp_handle,
                                         uint32_t stream_id);
extern int32_t mm_camera_reprocess(mm_camera_obj_t *my_obj,
                                   uint32_t ch_id,
                                   uint32_t repro_isp_handle,
                                   mm_camera_repro_data_t *repo_data);
extern int32_t mm_camera_stop_repro_isp(mm_camera_obj_t *my_obj,
                                        uint32_t ch_id,
                                        uint32_t repro_isp_handle,
                                        uint32_t stream_id);
extern int32_t mm_camera_detach_stream_from_repro_isp(mm_camera_obj_t *my_obj,
                                                      uint32_t ch_id,
                                                      uint32_t repro_isp_handle,
                                                      uint32_t stream_id);
extern int32_t mm_camera_close_repro_isp(mm_camera_obj_t *my_obj,
                                         uint32_t ch_id,
                                         uint32_t repro_isp_handle);

extern uint8_t mm_camera_util_get_pp_mask(mm_camera_obj_t *my_obj);

/* mm_channel */
extern int32_t mm_channel_fsm_fn(mm_channel_t *my_obj,
                                 mm_channel_evt_type_t evt,
                                 void * in_val,
                                 void * out_val);

/* qbuf is a special case that not going through state machine.
 * This is to avoid deadlock when trying to aquire ch_lock,
 * from the context of dataCB, but async stop is holding ch_lock */
extern int32_t mm_channel_qbuf(mm_channel_t *my_obj,
                               mm_camera_buf_def_t *buf);

/* Allow other stream to register dataCB at certain stream.
 * This is for use case of video sized live snapshot,
 * because snapshot stream need register one time CB at video stream.
 * ext_image_mode and sensor_idx are used to identify the destinate stream
 * to be register with dataCB. */
extern int32_t mm_channel_reg_stream_cb(mm_channel_t *my_obj,
                                        mm_stream_data_cb_t *cb,
                                        uint32_t ext_image_mode,
                                        uint32_t sensor_idx);

/* mm_stream */
extern int32_t mm_stream_fsm_fn(mm_stream_t *my_obj,
                                mm_stream_evt_type_t evt,
                                void * in_val,
                                void * out_val);
/* Allow other stream to register dataCB at certain stream.
 * This is for use case of video sized live snapshot,
 * because snapshot stream need register one time CB at video stream.
 * ext_image_mode and sensor_idx are used to identify the destinate stream
 * to be register with dataCB. */
extern int32_t mm_stream_reg_buf_cb(mm_stream_t *my_obj,
                                    mm_stream_data_cb_t *val);

/* utiltity fucntion declared in mm-camera-inteface2.c
 * and need be used by mm-camera and below*/
uint32_t mm_camera_util_generate_handler(uint8_t index);
const char * mm_camera_util_get_dev_name(uint32_t cam_handler);
uint8_t mm_camera_util_get_index_by_handler(uint32_t handler);

/* queue functions */
extern int32_t mm_camera_queue_init(mm_camera_queue_t* queue);
extern int32_t mm_camera_queue_enq(mm_camera_queue_t* queue, void* node);
extern void* mm_camera_queue_deq(mm_camera_queue_t* queue);
extern int32_t mm_camera_queue_deinit(mm_camera_queue_t* queue);
extern int32_t mm_camera_queue_flush(mm_camera_queue_t* queue);

/* poll/cmd thread functions */
extern int32_t mm_camera_poll_thread_launch(
                                mm_camera_poll_thread_t * poll_cb,
                                mm_camera_poll_thread_type_t poll_type);
extern int32_t mm_camera_poll_thread_release(mm_camera_poll_thread_t *poll_cb);
extern int32_t mm_camera_poll_thread_add_poll_fd(
                                mm_camera_poll_thread_t * poll_cb,
                                uint32_t handler,
                                int32_t fd,
                                mm_camera_poll_notify_t nofity_cb,
                                void *userdata);
extern int32_t mm_camera_poll_thread_del_poll_fd(
                                mm_camera_poll_thread_t * poll_cb,
                                uint32_t handler);
extern int32_t mm_camera_cmd_thread_launch(
                                mm_camera_cmd_thread_t * cmd_thread,
                                mm_camera_cmd_cb_t cb,
                                void* user_data);
extern int32_t mm_camera_cmd_thread_release(mm_camera_cmd_thread_t * cmd_thread);

#endif /* __MM_CAMERA_H__ */
