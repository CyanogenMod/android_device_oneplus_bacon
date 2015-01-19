
/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
 */

#ifndef MM_JPEG_H_
#define MM_JPEG_H_

#include "mm_jpeg_interface.h"
#include "cam_list.h"
#include "OMX_Types.h"
#include "OMX_Index.h"
#include "OMX_Core.h"
#include "OMX_Component.h"
#include "omx_jpeg_ext.h"
#include <semaphore.h>

typedef struct {
    struct cam_list list;
    void* data;
} mm_jpeg_q_node_t;

typedef struct {
    mm_jpeg_q_node_t head; /* dummy head */
    uint32_t size;
    pthread_mutex_t lock;
} mm_jpeg_queue_t;

typedef enum
{
    MM_JPEG_CMD_TYPE_JOB,          /* job cmd */
    MM_JPEG_CMD_TYPE_EXIT,         /* EXIT cmd for exiting jobMgr thread */
    MM_JPEG_CMD_TYPE_MAX
} mm_jpeg_cmd_type_t;

typedef struct {
    OMX_BUFFERHEADERTYPE* buf_header;
    uint32_t portIdx;
} mm_jpeg_omx_buf_info;

typedef struct {
    uint8_t num_bufs;
    mm_jpeg_omx_buf_info bufs[MAX_SRC_BUF_NUM];
} mm_jpeg_omx_src_buf;

typedef struct {
    uint32_t client_hdl;           /* client handler */
    uint32_t jobId;                /* job ID */
    mm_jpeg_job job;               /* job description */
    pthread_t cb_pid;              /* cb thread heandler*/

    void* jpeg_obj;                /* ptr to mm_jpeg_obj */
    jpeg_job_status_t job_status;  /* job status */
    uint8_t thumbnail_dropped;     /* flag indicating if thumbnail is dropped */
    int32_t jpeg_size;             /* the size of jpeg output after job is done */

    mm_jpeg_omx_src_buf src_bufs[JPEG_SRC_IMAGE_TYPE_MAX];
    mm_jpeg_omx_buf_info sink_buf;
} mm_jpeg_job_entry;

typedef struct {
    mm_jpeg_cmd_type_t type;
    union {
        mm_jpeg_job_entry  entry;
    };
} mm_jpeg_job_q_node_t;

typedef struct {
    uint8_t is_used;                /* flag: if is a valid client */
    uint32_t client_handle;         /* client handle */
} mm_jpeg_client_t;

typedef struct {
    pthread_t pid;                  /* job cmd thread ID */
    sem_t job_sem;                  /* semaphore for job cmd thread */
    mm_jpeg_queue_t job_queue;      /* queue for job to do */
} mm_jpeg_job_cmd_thread_t;

typedef enum {
    MM_JPEG_EVENT_MASK_JPEG_DONE    = 0x00000001, /* jpeg job is done */
    MM_JPEG_EVENT_MASK_JPEG_ABORT   = 0x00000002, /* jpeg job is aborted */
    MM_JPEG_EVENT_MASK_JPEG_ERROR   = 0x00000004, /* jpeg job has error */
    MM_JPEG_EVENT_MASK_CMD_COMPLETE = 0x00000100  /* omx cmd complete evt */
} mm_jpeg_event_mask_t;

typedef struct {
    uint32_t evt;
    int omx_value1;  /* only valid when evt_mask == MM_JPEG_EVENT_MASK_CMD_COMPLETE */
    int omx_value2;  /* only valid when evt_mask == MM_JPEG_EVENT_MASK_CMD_COMPLETE */
} mm_jpeg_evt_t;

#define MAX_JPEG_CLIENT_NUM 8
typedef struct mm_jpeg_obj_t {
    /* ClientMgr */
    int num_clients;                                /* num of clients */
    mm_jpeg_client_t clnt_mgr[MAX_JPEG_CLIENT_NUM]; /* client manager */

    /* JobMkr */
    pthread_mutex_t job_lock;                       /* job lock */
    mm_jpeg_job_cmd_thread_t job_mgr;               /* job mgr thread including todo_q*/
    mm_jpeg_queue_t ongoing_job_q;                  /* queue for ongoing jobs */

    /* Notifier */
    mm_jpeg_queue_t cb_q;                           /* queue for CB threads */

    /* OMX related */
    OMX_HANDLETYPE omx_handle;                      /* handle to omx engine */
    OMX_CALLBACKTYPE omx_callbacks;                 /* callbacks to omx engine */

    pthread_mutex_t omx_evt_lock;
    pthread_cond_t omx_evt_cond;
    mm_jpeg_evt_t omx_evt_rcvd;
} mm_jpeg_obj;

extern int32_t mm_jpeg_init(mm_jpeg_obj *my_obj);
extern int32_t mm_jpeg_deinit(mm_jpeg_obj *my_obj);
extern uint32_t mm_jpeg_new_client(mm_jpeg_obj *my_obj);
extern int32_t mm_jpeg_start_job(mm_jpeg_obj *my_obj,
                                 uint32_t client_hdl,
                                 mm_jpeg_job* job,
                                 uint32_t* jobId);
extern int32_t mm_jpeg_abort_job(mm_jpeg_obj *my_obj,
                                 uint32_t client_hdl,
                                 uint32_t jobId);
extern int32_t mm_jpeg_close(mm_jpeg_obj *my_obj,
                             uint32_t client_hdl);

/* utiltity fucntion declared in mm-camera-inteface2.c
 * and need be used by mm-camera and below*/
uint32_t mm_jpeg_util_generate_handler(uint8_t index);
uint8_t mm_jpeg_util_get_index_by_handler(uint32_t handler);

/* basic queue functions */
extern int32_t mm_jpeg_queue_init(mm_jpeg_queue_t* queue);
extern int32_t mm_jpeg_queue_enq(mm_jpeg_queue_t* queue, void* node);
extern void* mm_jpeg_queue_peek(mm_jpeg_queue_t* queue);
extern void* mm_jpeg_queue_deq(mm_jpeg_queue_t* queue);
extern int32_t mm_jpeg_queue_deinit(mm_jpeg_queue_t* queue);
extern int32_t mm_jpeg_queue_flush(mm_jpeg_queue_t* queue);
extern uint32_t mm_jpeg_queue_get_size(mm_jpeg_queue_t* queue);

#endif /* MM_JPEG_H_ */


