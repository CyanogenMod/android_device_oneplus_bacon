/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#ifndef MM_JPEG_H_
#define MM_JPEG_H_

#include <cam_semaphore.h>
#include "mm_jpeg_interface.h"
#include "cam_list.h"
#include "OMX_Types.h"
#include "OMX_Index.h"
#include "OMX_Core.h"
#include "OMX_Component.h"
#include "QOMX_JpegExtensions.h"
#include "mm_jpeg_ionbuf.h"

#define MM_JPEG_MAX_THREADS 30
#define MM_JPEG_CIRQ_SIZE 30
#define MM_JPEG_MAX_SESSION 10
#define MAX_EXIF_TABLE_ENTRIES 50
#define MAX_JPEG_SIZE 20000000
#define MAX_OMX_HANDLES (5)


/** mm_jpeg_abort_state_t:
 *  @MM_JPEG_ABORT_NONE: Abort is not issued
 *  @MM_JPEG_ABORT_INIT: Abort is issued from the client
 *  @MM_JPEG_ABORT_DONE: Abort is completed
 *
 *  State representing the abort state
 **/
typedef enum {
  MM_JPEG_ABORT_NONE,
  MM_JPEG_ABORT_INIT,
  MM_JPEG_ABORT_DONE,
} mm_jpeg_abort_state_t;


/* define max num of supported concurrent jpeg jobs by OMX engine.
 * Current, only one per time */
#define NUM_MAX_JPEG_CNCURRENT_JOBS 2

#define JOB_ID_MAGICVAL 0x1
#define JOB_HIST_MAX 10000

/** DUMP_TO_FILE:
 *  @filename: file name
 *  @p_addr: address of the buffer
 *  @len: buffer length
 *
 *  dump the image to the file
 **/
#define DUMP_TO_FILE(filename, p_addr, len) ({ \
  size_t rc = 0; \
  FILE *fp = fopen(filename, "w+"); \
  if (fp) { \
    rc = fwrite(p_addr, 1, len, fp); \
    CDBG_ERROR("%s:%d] written size %zu", __func__, __LINE__, len); \
    fclose(fp); \
  } else { \
    CDBG_ERROR("%s:%d] open %s failed", __func__, __LINE__, filename); \
  } \
})

/** DUMP_TO_FILE2:
 *  @filename: file name
 *  @p_addr: address of the buffer
 *  @len: buffer length
 *
 *  dump the image to the file if the memory is non-contiguous
 **/
#define DUMP_TO_FILE2(filename, p_addr1, len1, paddr2, len2) ({ \
  size_t rc = 0; \
  FILE *fp = fopen(filename, "w+"); \
  if (fp) { \
    rc = fwrite(p_addr1, 1, len1, fp); \
    rc = fwrite(p_addr2, 1, len2, fp); \
    CDBG_ERROR("%s:%d] written %zu %zu", __func__, __LINE__, len1, len2); \
    fclose(fp); \
  } else { \
    CDBG_ERROR("%s:%d] open %s failed", __func__, __LINE__, filename); \
  } \
})

/** MM_JPEG_CHK_ABORT:
 *  @p: client pointer
 *  @ret: return value
 *  @label: label to jump to
 *
 *  check the abort failure
 **/
#define MM_JPEG_CHK_ABORT(p, ret, label) ({ \
  if (MM_JPEG_ABORT_INIT == p->abort_state) { \
    CDBG_ERROR("%s:%d] jpeg abort", __func__, __LINE__); \
    ret = OMX_ErrorNone; \
    goto label; \
  } \
})

#define GET_CLIENT_IDX(x) ((x) & 0xff)
#define GET_SESSION_IDX(x) (((x) >> 8) & 0xff)
#define GET_JOB_IDX(x) (((x) >> 16) & 0xff)

typedef struct {
  union {
    int i_data[MM_JPEG_CIRQ_SIZE];
    void *p_data[MM_JPEG_CIRQ_SIZE];
  };
  int front;
  int rear;
  int count;
  pthread_mutex_t lock;
} mm_jpeg_cirq_t;

/** cirq_reset:
 *
 *  Arguments:
 *    @q: circular queue
 *
 *  Return:
 *       none
 *
 *  Description:
 *       Resets the circular queue
 *
 **/
static inline void cirq_reset(mm_jpeg_cirq_t *q)
{
  q->front = 0;
  q->rear = 0;
  q->count = 0;
  pthread_mutex_init(&q->lock, NULL);
}

/** cirq_empty:
 *
 *  Arguments:
 *    @q: circular queue
 *
 *  Return:
 *       none
 *
 *  Description:
 *       check if the curcular queue is empty
 *
 **/
#define cirq_empty(q) (q->count == 0)

/** cirq_full:
 *
 *  Arguments:
 *    @q: circular queue
 *
 *  Return:
 *       none
 *
 *  Description:
 *       check if the curcular queue is full
 *
 **/
#define cirq_full(q) (q->count == MM_JPEG_CIRQ_SIZE)

/** cirq_enqueue:
 *
 *  Arguments:
 *    @q: circular queue
 *    @data: data to be inserted
 *
 *  Return:
 *       true/false
 *
 *  Description:
 *       enqueue an element into circular queue
 *
 **/
#define cirq_enqueue(q, type, data) ({ \
  int rc = 0; \
  pthread_mutex_lock(&q->lock); \
  if (cirq_full(q)) { \
    rc = -1; \
  } else { \
    q->type[q->rear] = data; \
    q->rear = (q->rear + 1) % MM_JPEG_CIRQ_SIZE; \
    q->count++; \
  } \
  pthread_mutex_unlock(&q->lock); \
  rc; \
})

/** cirq_dequeue:
 *
 *  Arguments:
 *    @q: circular queue
 *    @data: data to be popped
 *
 *  Return:
 *       true/false
 *
 *  Description:
 *       dequeue an element from the circular queue
 *
 **/
#define cirq_dequeue(q, type, data) ({ \
  int rc = 0; \
  pthread_mutex_lock(&q->lock); \
  if (cirq_empty(q)) { \
    pthread_mutex_unlock(&q->lock); \
    rc = -1; \
  } else { \
    data = q->type[q->front]; \
    q->count--; \
  } \
  pthread_mutex_unlock(&q->lock); \
  rc; \
})


typedef union {
  uint32_t u32;
  void* p;
} mm_jpeg_q_data_t;

  typedef struct {
  struct cam_list list;
  mm_jpeg_q_data_t data;
} mm_jpeg_q_node_t;

typedef struct {
  mm_jpeg_q_node_t head; /* dummy head */
  uint32_t size;
  pthread_mutex_t lock;
} mm_jpeg_queue_t;

typedef enum {
  MM_JPEG_CMD_TYPE_JOB,          /* job cmd */
  MM_JPEG_CMD_TYPE_EXIT,         /* EXIT cmd for exiting jobMgr thread */
  MM_JPEG_CMD_TYPE_DECODE_JOB,
  MM_JPEG_CMD_TYPE_MAX
} mm_jpeg_cmd_type_t;

typedef struct mm_jpeg_job_session {
  uint32_t client_hdl;           /* client handler */
  uint32_t jobId;                /* job ID */
  uint32_t sessionId;            /* session ID */
  mm_jpeg_encode_params_t params; /* encode params */
  mm_jpeg_decode_params_t dec_params; /* encode params */
  mm_jpeg_encode_job_t encode_job;             /* job description */
  mm_jpeg_decode_job_t decode_job;
  pthread_t encode_pid;          /* encode thread handler*/

  void *jpeg_obj;                /* ptr to mm_jpeg_obj */
  jpeg_job_status_t job_status;  /* job status */

  int state_change_pending;      /* flag to indicate if state change is pending */
  OMX_ERRORTYPE error_flag;      /* variable to indicate error during encoding */
  mm_jpeg_abort_state_t abort_state; /* variable to indicate abort during encoding */

  /* OMX related */
  OMX_HANDLETYPE omx_handle;                      /* handle to omx engine */
  OMX_CALLBACKTYPE omx_callbacks;                 /* callbacks to omx engine */

  /* buffer headers */
  OMX_BUFFERHEADERTYPE *p_in_omx_buf[MM_JPEG_MAX_BUF];
  OMX_BUFFERHEADERTYPE *p_in_omx_thumb_buf[MM_JPEG_MAX_BUF];
  OMX_BUFFERHEADERTYPE *p_out_omx_buf[MM_JPEG_MAX_BUF];

  OMX_PARAM_PORTDEFINITIONTYPE inputPort;
  OMX_PARAM_PORTDEFINITIONTYPE outputPort;
  OMX_PARAM_PORTDEFINITIONTYPE inputTmbPort;

  /* event locks */
  pthread_mutex_t lock;
  pthread_cond_t cond;

  QEXIF_INFO_DATA exif_info_local[MAX_EXIF_TABLE_ENTRIES];  //all exif tags for JPEG encoder
  int exif_count_local;

  mm_jpeg_cirq_t cb_q;
  int32_t ebd_count;
  int32_t fbd_count;

  /* this flag represents whether the job is active */
  OMX_BOOL active;

  /* this flag indicates if the configration is complete */
  OMX_BOOL config;

  /* job history count to generate unique id */
  unsigned int job_hist;

  OMX_BOOL encoding;

  buffer_t work_buffer;

  OMX_EVENTTYPE omxEvent;
  int event_pending;

  uint8_t *meta_enc_key;
  size_t meta_enc_keylen;

  struct mm_jpeg_job_session *next_session;

  uint32_t curr_out_buf_idx;

  uint32_t num_omx_sessions;
  OMX_BOOL auto_out_buf;

  mm_jpeg_queue_t *session_handle_q;
  mm_jpeg_queue_t *out_buf_q;
} mm_jpeg_job_session_t;

typedef struct {
  mm_jpeg_encode_job_t encode_job;
  uint32_t job_id;
  uint32_t client_handle;
} mm_jpeg_encode_job_info_t;

typedef struct {
  mm_jpeg_decode_job_t decode_job;
  uint32_t job_id;
  uint32_t client_handle;
} mm_jpeg_decode_job_info_t;

typedef struct {
  mm_jpeg_cmd_type_t type;
  union {
    mm_jpeg_encode_job_info_t enc_info;
    mm_jpeg_decode_job_info_t dec_info;
  };
} mm_jpeg_job_q_node_t;

typedef struct {
  uint8_t is_used;                /* flag: if is a valid client */
  uint32_t client_handle;         /* client handle */
  mm_jpeg_job_session_t session[MM_JPEG_MAX_SESSION];
  pthread_mutex_t lock;           /* job lock */
} mm_jpeg_client_t;

typedef struct {
  pthread_t pid;                  /* job cmd thread ID */
  cam_semaphore_t job_sem;        /* semaphore for job cmd thread */
  mm_jpeg_queue_t job_queue;      /* queue for job to do */
} mm_jpeg_job_cmd_thread_t;

#define MAX_JPEG_CLIENT_NUM 8
typedef struct mm_jpeg_obj_t {
  /* ClientMgr */
  int num_clients;                                /* num of clients */
  mm_jpeg_client_t clnt_mgr[MAX_JPEG_CLIENT_NUM]; /* client manager */

  /* JobMkr */
  pthread_mutex_t job_lock;                       /* job lock */
  mm_jpeg_job_cmd_thread_t job_mgr;               /* job mgr thread including todo_q*/
  mm_jpeg_queue_t ongoing_job_q;                  /* queue for ongoing jobs */
  buffer_t ionBuffer[MM_JPEG_CONCURRENT_SESSIONS_COUNT];


  /* Max pic dimension for work buf calc*/
  uint32_t max_pic_w;
  uint32_t max_pic_h;
  uint32_t work_buf_cnt;

#ifdef LOAD_ADSP_RPC_LIB
  void *adsprpc_lib_handle;
#endif

  uint32_t num_sessions;

} mm_jpeg_obj;

/** mm_jpeg_pending_func_t:
 *
 * Intermediate function for transition change
 **/
typedef OMX_ERRORTYPE (*mm_jpeg_transition_func_t)(void *);

extern int32_t mm_jpeg_init(mm_jpeg_obj *my_obj);
extern int32_t mm_jpeg_deinit(mm_jpeg_obj *my_obj);
extern uint32_t mm_jpeg_new_client(mm_jpeg_obj *my_obj);
extern int32_t mm_jpeg_start_job(mm_jpeg_obj *my_obj,
  mm_jpeg_job_t* job,
  uint32_t* jobId);
extern int32_t mm_jpeg_abort_job(mm_jpeg_obj *my_obj,
  uint32_t jobId);
extern int32_t mm_jpeg_close(mm_jpeg_obj *my_obj,
  uint32_t client_hdl);
extern int32_t mm_jpeg_create_session(mm_jpeg_obj *my_obj,
  uint32_t client_hdl,
  mm_jpeg_encode_params_t *p_params,
  uint32_t* p_session_id);
extern int32_t mm_jpeg_destroy_session_by_id(mm_jpeg_obj *my_obj,
  uint32_t session_id);

extern int32_t mm_jpegdec_init(mm_jpeg_obj *my_obj);
extern int32_t mm_jpegdec_deinit(mm_jpeg_obj *my_obj);
extern int32_t mm_jpeg_jobmgr_thread_release(mm_jpeg_obj * my_obj);
extern int32_t mm_jpeg_jobmgr_thread_launch(mm_jpeg_obj *my_obj);
extern int32_t mm_jpegdec_start_decode_job(mm_jpeg_obj *my_obj,
  mm_jpeg_job_t* job,
  uint32_t* jobId);

extern int32_t mm_jpegdec_create_session(mm_jpeg_obj *my_obj,
  uint32_t client_hdl,
  mm_jpeg_decode_params_t *p_params,
  uint32_t* p_session_id);

extern int32_t mm_jpegdec_destroy_session_by_id(mm_jpeg_obj *my_obj,
  uint32_t session_id);

extern int32_t mm_jpegdec_abort_job(mm_jpeg_obj *my_obj,
  uint32_t jobId);

int32_t mm_jpegdec_process_decoding_job(mm_jpeg_obj *my_obj,
    mm_jpeg_job_q_node_t* job_node);

/* utiltity fucntion declared in mm-camera-inteface2.c
 * and need be used by mm-camera and below*/
uint32_t mm_jpeg_util_generate_handler(uint8_t index);
uint8_t mm_jpeg_util_get_index_by_handler(uint32_t handler);

/* basic queue functions */
extern int32_t mm_jpeg_queue_init(mm_jpeg_queue_t* queue);
extern int32_t mm_jpeg_queue_enq(mm_jpeg_queue_t* queue,
    mm_jpeg_q_data_t data);
extern int32_t mm_jpeg_queue_enq_head(mm_jpeg_queue_t* queue,
    mm_jpeg_q_data_t data);
extern mm_jpeg_q_data_t mm_jpeg_queue_deq(mm_jpeg_queue_t* queue);
extern int32_t mm_jpeg_queue_deinit(mm_jpeg_queue_t* queue);
extern int32_t mm_jpeg_queue_flush(mm_jpeg_queue_t* queue);
extern uint32_t mm_jpeg_queue_get_size(mm_jpeg_queue_t* queue);
extern mm_jpeg_q_data_t mm_jpeg_queue_peek(mm_jpeg_queue_t* queue);
extern int32_t addExifEntry(QOMX_EXIF_INFO *p_exif_info, exif_tag_id_t tagid,
  exif_tag_type_t type, uint32_t count, void *data);
extern int32_t releaseExifEntry(QEXIF_INFO_DATA *p_exif_data);
extern int process_meta_data(cam_metadata_info_t *p_meta,
  QOMX_EXIF_INFO *exif_info, mm_jpeg_exif_params_t *p_cam3a_params);

OMX_ERRORTYPE mm_jpeg_session_change_state(mm_jpeg_job_session_t* p_session,
  OMX_STATETYPE new_state,
  mm_jpeg_transition_func_t p_exec);

int map_jpeg_format(mm_jpeg_color_format color_fmt);

OMX_BOOL mm_jpeg_session_abort(mm_jpeg_job_session_t *p_session);
/**
 *
 * special queue functions for job queue
 **/
mm_jpeg_job_q_node_t* mm_jpeg_queue_remove_job_by_client_id(
  mm_jpeg_queue_t* queue, uint32_t client_hdl);
mm_jpeg_job_q_node_t* mm_jpeg_queue_remove_job_by_job_id(
  mm_jpeg_queue_t* queue, uint32_t job_id);
mm_jpeg_job_q_node_t* mm_jpeg_queue_remove_job_by_session_id(
  mm_jpeg_queue_t* queue, uint32_t session_id);
mm_jpeg_job_q_node_t* mm_jpeg_queue_remove_job_unlk(
  mm_jpeg_queue_t* queue, uint32_t job_id);


/** mm_jpeg_queue_func_t:
 *
 * Intermediate function for queue operation
 **/
typedef void (*mm_jpeg_queue_func_t)(void *);


#endif /* MM_JPEG_H_ */


