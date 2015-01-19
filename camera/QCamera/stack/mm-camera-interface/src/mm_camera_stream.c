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

#include <pthread.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <semaphore.h>

#include "mm_camera_dbg.h"
#include "mm_camera_interface.h"
#include "mm_camera.h"

#define MM_CAMERA_MAX_NUM_FRAMES     16

/* internal function decalre */
int32_t mm_stream_qbuf(mm_stream_t *my_obj,
                       mm_camera_buf_def_t *buf);
int32_t mm_stream_set_ext_mode(mm_stream_t * my_obj);
int32_t mm_stream_set_fmt(mm_stream_t * my_obj);
int32_t mm_stream_get_offset(mm_stream_t *my_obj);
int32_t mm_stream_set_cid(mm_stream_t *my_obj,stream_cid_t *in_value);
int32_t mm_stream_init_bufs(mm_stream_t * my_obj);
int32_t mm_stream_deinit_bufs(mm_stream_t * my_obj);
int32_t mm_stream_request_buf(mm_stream_t * my_obj);
int32_t mm_stream_unreg_buf(mm_stream_t * my_obj);
int32_t mm_stream_release(mm_stream_t *my_obj);
int32_t mm_stream_get_crop(mm_stream_t *my_obj,
                         mm_camera_rect_t *crop);
int32_t mm_stream_get_cid(mm_stream_t *my_obj,
                          stream_cid_t *out_value);
int32_t mm_stream_set_parm_acquire(mm_stream_t *my_obj,
                         void *value);
int32_t mm_stream_get_parm_acquire(mm_stream_t *my_obj,
                         void *value);
int32_t mm_stream_set_parm_config(mm_stream_t *my_obj,
                         void *value);
int32_t mm_stream_get_parm_config(mm_stream_t *my_obj,
                         void *value);
int32_t mm_stream_set_parm_start(mm_stream_t *my_obj,
                         void *value);
int32_t mm_stream_get_parm_start(mm_stream_t *my_obj,
                         void *value);
int32_t mm_stream_streamon(mm_stream_t *my_obj);
int32_t mm_stream_streamoff(mm_stream_t *my_obj);
int32_t mm_stream_read_msm_frame(mm_stream_t * my_obj,
                                 mm_camera_buf_info_t* buf_info);
int32_t mm_stream_config(mm_stream_t *my_obj,
                         mm_camera_stream_config_t *config);
int32_t mm_stream_reg_buf(mm_stream_t * my_obj);
int32_t mm_stream_buf_done(mm_stream_t * my_obj,
                           mm_camera_buf_def_t *frame);


/* state machine function declare */
int32_t mm_stream_fsm_inited(mm_stream_t * my_obj,
                             mm_stream_evt_type_t evt,
                             void * in_val,
                             void * out_val);
int32_t mm_stream_fsm_acquired(mm_stream_t * my_obj,
                               mm_stream_evt_type_t evt,
                               void * in_val,
                               void * out_val);
int32_t mm_stream_fsm_cfg(mm_stream_t * my_obj,
                          mm_stream_evt_type_t evt,
                          void * in_val,
                          void * out_val);
int32_t mm_stream_fsm_buffed(mm_stream_t * my_obj,
                             mm_stream_evt_type_t evt,
                             void * in_val,
                             void * out_val);
int32_t mm_stream_fsm_reg(mm_stream_t * my_obj,
                          mm_stream_evt_type_t evt,
                          void * in_val,
                          void * out_val);
int32_t mm_stream_fsm_active_stream_on(mm_stream_t * my_obj,
                                       mm_stream_evt_type_t evt,
                                       void * in_val,
                                       void * out_val);
int32_t mm_stream_fsm_active_stream_off(mm_stream_t * my_obj,
                                        mm_stream_evt_type_t evt,
                                        void * in_val,
                                        void * out_val);

extern int32_t mm_camera_send_native_ctrl_cmd(mm_camera_obj_t * my_obj,
                                       cam_ctrl_type type,
                                       uint32_t length,
                                       void *value);

static int get_stream_inst_handle(mm_stream_t *my_obj)
{
  int rc = 0;
  uint32_t inst_handle;
  struct msm_camera_v4l2_ioctl_t v4l2_ioctl;

  v4l2_ioctl.id = MSM_V4L2_PID_INST_HANDLE;
  v4l2_ioctl.ioctl_ptr = &inst_handle;
  v4l2_ioctl.len = sizeof(inst_handle);
  rc = ioctl(my_obj->fd, MSM_CAM_V4L2_IOCTL_PRIVATE_G_CTRL, &v4l2_ioctl);
  if (rc) {
    CDBG_ERROR("%s Error getting mctl pp inst handle", __func__);
    return rc;
  }

  my_obj->inst_hdl = inst_handle;
  CDBG("%s: X, inst_handle = 0x%x, my_handle = 0x%x, "
       "image_mode = %d, fd = %d, state = %d, rc = %d",
       __func__, my_obj->inst_hdl, my_obj->my_hdl,
       my_obj->ext_image_mode, my_obj->fd, my_obj->state, rc);
  return rc;
}

void mm_stream_handle_rcvd_buf(mm_stream_t *my_obj,
                                  mm_camera_buf_info_t *buf_info)
{
    int32_t i;
    uint8_t has_cb = 0;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);

    /* enqueue to super buf thread */
    if (my_obj->is_bundled) {
        mm_camera_cmdcb_t* node = NULL;

        /* send sem_post to wake up channel cmd thread to enqueue to super buffer */
        node = (mm_camera_cmdcb_t *)malloc(sizeof(mm_camera_cmdcb_t));
        if (NULL != node) {
            memset(node, 0, sizeof(mm_camera_cmdcb_t));
            node->cmd_type = MM_CAMERA_CMD_TYPE_DATA_CB;
            memcpy(&node->u.buf, buf_info, sizeof(mm_camera_buf_info_t));

            /* enqueue to cmd thread */
            mm_camera_queue_enq(&(my_obj->ch_obj->cmd_thread.cmd_queue), node);

            /* wake up cmd thread */
            sem_post(&(my_obj->ch_obj->cmd_thread.cmd_sem));
        } else {
            CDBG_ERROR("%s: No memory for mm_camera_node_t", __func__);
        }
    }

    /* check if has CB */
    for (i=0 ; i< MM_CAMERA_STREAM_BUF_CB_MAX; i++) {
        if(NULL != my_obj->buf_cb[i].cb) {
            has_cb = 1;
        }
    }
    if(has_cb) {
        mm_camera_cmdcb_t* node = NULL;

        /* send sem_post to wake up cmd thread to dispatch dataCB */
        node = (mm_camera_cmdcb_t *)malloc(sizeof(mm_camera_cmdcb_t));
        if (NULL != node) {
            memset(node, 0, sizeof(mm_camera_cmdcb_t));
            node->cmd_type = MM_CAMERA_CMD_TYPE_DATA_CB;
            memcpy(&node->u.buf, buf_info, sizeof(mm_camera_buf_info_t));

            /* enqueue to cmd thread */
            mm_camera_queue_enq(&(my_obj->cmd_thread.cmd_queue), node);

            /* wake up cmd thread */
            sem_post(&(my_obj->cmd_thread.cmd_sem));
        } else {
            CDBG_ERROR("%s: No memory for mm_camera_node_t", __func__);
        }
    }
}

static void mm_stream_data_notify(void* user_data)
{
    mm_stream_t *my_obj = (mm_stream_t*)user_data;
    int32_t idx = -1, i, rc;
    uint8_t has_cb = 0;
    mm_camera_buf_info_t buf_info;

    if (NULL == my_obj) {
        return;
    }

    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);
    if (MM_STREAM_STATE_ACTIVE_STREAM_ON != my_obj->state) {
        /* this Cb will only received in active_stream_on state
         * if not so, return here */
        CDBG_ERROR("%s: ERROR!! Wrong state (%d) to receive data notify!",
                   __func__, my_obj->state);
        return;
    }

    memset(&buf_info, 0, sizeof(mm_camera_buf_info_t));

    pthread_mutex_lock(&my_obj->buf_lock);
    rc = mm_stream_read_msm_frame(my_obj, &buf_info);
    if (rc != 0) {
        pthread_mutex_unlock(&my_obj->buf_lock);
        return;
    }
    idx = buf_info.buf->buf_idx;
    /* update buffer location */
    my_obj->buf_status[idx].in_kernel = 0;

    /* update buf ref count */
    if (my_obj->is_bundled) {
        /* need to add into super buf since bundled, add ref count */
        my_obj->buf_status[idx].buf_refcnt++;
    }

    for (i=0; i < MM_CAMERA_STREAM_BUF_CB_MAX; i++) {
        if(NULL != my_obj->buf_cb[i].cb) {
            /* for every CB, add ref count */
            my_obj->buf_status[idx].buf_refcnt++;
            has_cb = 1;
        }
    }
    pthread_mutex_unlock(&my_obj->buf_lock);

    mm_stream_handle_rcvd_buf(my_obj, &buf_info);
}

/* special function for dataCB registered at other stream */
static void mm_stream_buf_notify(mm_camera_super_buf_t *super_buf,
                                 void *user_data)
{
    mm_stream_t * my_obj = (mm_stream_t*)user_data;
    mm_camera_buf_info_t buf_info;
    int8_t i;
    mm_camera_buf_def_t *buf = super_buf->bufs[0];

    CDBG("%s : E",__func__);
    if (my_obj == NULL) {
        return;
    }
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);

    if (MM_STREAM_STATE_ACTIVE_STREAM_OFF != my_obj->state) {
        /* this CB will only received in active_stream_off state
         * if not so, return here */
        return;
    }

    /* 1) copy buf into local buf */
    if (my_obj->buf_num <= 0) {
        CDBG_ERROR("%s: Local buf is not allocated yet", __func__);
        return;
    }

    my_obj->buf[my_obj->local_buf_idx].buf_idx = 0;
    my_obj->buf[my_obj->local_buf_idx].stream_id = my_obj->my_hdl;
    my_obj->buf[my_obj->local_buf_idx].frame_idx = buf->frame_idx;

    memcpy(&my_obj->buf[my_obj->local_buf_idx].ts, &buf->ts, sizeof(buf->ts));

    memcpy(my_obj->buf[my_obj->local_buf_idx].buffer, buf->buffer, buf->frame_len);

    /* set flag to indicate buf be to sent out is from local */
    my_obj->is_local_buf = 1;

    /* 2) buf_done the buf from other stream */
    mm_channel_qbuf(my_obj->ch_obj, buf);

    /* 3) handle received buf */
    memset(&buf_info, 0, sizeof(mm_camera_buf_info_t));
    buf_info.frame_idx =my_obj->buf[my_obj->local_buf_idx].frame_idx;
    buf_info.buf = &my_obj->buf[my_obj->local_buf_idx];
    buf_info.stream_id = my_obj->my_hdl;

    my_obj->local_buf_idx++;
    if (my_obj->local_buf_idx >= my_obj->buf_num) {
        my_obj->local_buf_idx = 0;
    }
    mm_stream_handle_rcvd_buf(my_obj, &buf_info);
}

static void mm_stream_dispatch_app_data(mm_camera_cmdcb_t *cmd_cb,
                                        void* user_data)
{
    int i;
    mm_stream_t * my_obj = (mm_stream_t *)user_data;
    mm_camera_buf_info_t* buf_info = NULL;
    mm_camera_super_buf_t super_buf;

    if (NULL == my_obj) {
        return;
    }
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);

    if (MM_CAMERA_CMD_TYPE_DATA_CB != cmd_cb->cmd_type) {
        CDBG_ERROR("%s: Wrong cmd_type (%d) for dataCB",
                   __func__, cmd_cb->cmd_type);
        return;
    }

    buf_info = &cmd_cb->u.buf;
    memset(&super_buf, 0, sizeof(mm_camera_super_buf_t));
    super_buf.num_bufs = 1;
    super_buf.bufs[0] = buf_info->buf;
    super_buf.camera_handle = my_obj->ch_obj->cam_obj->my_hdl;
    super_buf.ch_id = my_obj->ch_obj->my_hdl;


    pthread_mutex_lock(&my_obj->cb_lock);
    for(i = 0; i < MM_CAMERA_STREAM_BUF_CB_MAX; i++) {
        if(NULL != my_obj->buf_cb[i].cb) {
            if (my_obj->buf_cb[i].cb_count != 0) {
                /* if <0, means infinite CB
                 * if >0, means CB for certain times
                 * both case we need to call CB */
                my_obj->buf_cb[i].cb(&super_buf,
                                     my_obj->buf_cb[i].user_data);
            }

            /* if >0, reduce count by 1 every time we called CB until reaches 0
             * when count reach 0, reset the buf_cb to have no CB */
            if (my_obj->buf_cb[i].cb_count > 0) {
                my_obj->buf_cb[i].cb_count--;
                if (0 == my_obj->buf_cb[i].cb_count) {
                    my_obj->buf_cb[i].cb = NULL;
                    my_obj->buf_cb[i].user_data = NULL;
                }
            }
        }
    }
    pthread_mutex_unlock(&my_obj->cb_lock);
}

/* state machine entry */
int32_t mm_stream_fsm_fn(mm_stream_t *my_obj,
                         mm_stream_evt_type_t evt,
                         void * in_val,
                         void * out_val)
{
    int32_t rc = -1;

    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d, event = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state, evt);
    switch (my_obj->state) {
    case MM_STREAM_STATE_NOTUSED:
        CDBG("%s: Not handling evt in unused state", __func__);
        break;
    case MM_STREAM_STATE_INITED:
        rc = mm_stream_fsm_inited(my_obj, evt, in_val, out_val);
        break;
    case MM_STREAM_STATE_ACQUIRED:
        rc = mm_stream_fsm_acquired(my_obj, evt, in_val, out_val);
        break;
    case MM_STREAM_STATE_CFG:
        rc = mm_stream_fsm_cfg(my_obj, evt, in_val, out_val);
        break;
    case MM_STREAM_STATE_BUFFED:
        rc = mm_stream_fsm_buffed(my_obj, evt, in_val, out_val);
        break;
    case MM_STREAM_STATE_REG:
        rc = mm_stream_fsm_reg(my_obj, evt, in_val, out_val);
        break;
    case MM_STREAM_STATE_ACTIVE_STREAM_ON:
        rc = mm_stream_fsm_active_stream_on(my_obj, evt, in_val, out_val);
        break;
    case MM_STREAM_STATE_ACTIVE_STREAM_OFF:
        rc = mm_stream_fsm_active_stream_off(my_obj, evt, in_val, out_val);
        break;
    default:
        CDBG("%s: Not a valid state (%d)", __func__, my_obj->state);
        break;
    }
    CDBG("%s : X rc =%d",__func__,rc);
    return rc;
}

int32_t mm_stream_fsm_inited(mm_stream_t *my_obj,
                             mm_stream_evt_type_t evt,
                             void * in_val,
                             void * out_val)
{
    int32_t rc = 0;
    char dev_name[MM_CAMERA_DEV_NAME_LEN];

    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d, event = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state, evt);
    switch(evt) {
    case MM_STREAM_EVT_ACQUIRE:
        if ((NULL == my_obj->ch_obj) || (NULL == my_obj->ch_obj->cam_obj)) {
            CDBG_ERROR("%s: NULL channel or camera obj\n", __func__);
            rc = -1;
            break;
        }

        snprintf(dev_name, sizeof(dev_name), "/dev/%s",
                 mm_camera_util_get_dev_name(my_obj->ch_obj->cam_obj->my_hdl));

        my_obj->fd = open(dev_name, O_RDWR | O_NONBLOCK);
        if (my_obj->fd <= 0) {
            CDBG_ERROR("%s: open dev returned %d\n", __func__, my_obj->fd);
            rc = -1;
            break;
        }
        CDBG("%s: open dev fd = %d, ext_image_mode = %d, sensor_idx = %d\n",
                 __func__, my_obj->fd, my_obj->ext_image_mode, my_obj->sensor_idx);
        rc = mm_stream_set_ext_mode(my_obj);
        if (0 == rc) {
            my_obj->state = MM_STREAM_STATE_ACQUIRED;
        } else {
            /* failed setting ext_mode
             * close fd */
            if(my_obj->fd > 0) {
                close(my_obj->fd);
                my_obj->fd = -1;
            }
            break;
        }
        rc = get_stream_inst_handle(my_obj);
        if(rc) {
            if(my_obj->fd > 0) {
                close(my_obj->fd);
                my_obj->fd = -1;
            }
        }
        break;
    default:
        CDBG_ERROR("%s: Invalid evt=%d, stream_state=%d",
                   __func__,evt,my_obj->state);
        break;
    }
    return rc;
}

int32_t mm_stream_fsm_acquired(mm_stream_t *my_obj,
                               mm_stream_evt_type_t evt,
                               void * in_val,
                               void * out_val)
{
    int32_t rc = 0;

    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d, event = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state, evt);
    switch(evt) {
    case MM_STREAM_EVT_SET_FMT:
        {
            mm_camera_stream_config_t *config =
                (mm_camera_stream_config_t *)in_val;

            rc = mm_stream_config(my_obj, config);

            /* change state to configed */
            my_obj->state = MM_STREAM_STATE_CFG;

            break;
        }
    case MM_STREAM_EVT_RELEASE:
        rc = mm_stream_release(my_obj);
        /* change state to not used */
         my_obj->state = MM_STREAM_STATE_NOTUSED;
        break;
    case MM_STREAM_EVT_SET_PARM:
        rc = mm_stream_set_parm_acquire(my_obj, in_val);
        break;
    case MM_STREAM_EVT_GET_PARM:
        rc = mm_stream_get_parm_acquire(my_obj,out_val);
        break;
    default:
        CDBG_ERROR("%s: Invalid evt=%d, stream_state=%d",
                   __func__, evt, my_obj->state);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

int32_t mm_stream_fsm_cfg(mm_stream_t * my_obj,
                          mm_stream_evt_type_t evt,
                          void * in_val,
                          void * out_val)
{
    int32_t rc = 0;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d, event = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state, evt);
    switch(evt) {
    case MM_STREAM_EVT_SET_FMT:
        {
            mm_camera_stream_config_t *config =
                (mm_camera_stream_config_t *)in_val;

            rc = mm_stream_config(my_obj, config);

            /* change state to configed */
            my_obj->state = MM_STREAM_STATE_CFG;

            break;
        }
    case MM_STREAM_EVT_RELEASE:
        rc = mm_stream_release(my_obj);
        my_obj->state = MM_STREAM_STATE_NOTUSED;
        break;
    case MM_STREAM_EVT_SET_PARM:
        rc = mm_stream_set_parm_config(my_obj, in_val);
        break;
    case MM_STREAM_EVT_GET_PARM:
        rc = mm_stream_get_parm_config(my_obj, out_val);
        break;
    case MM_STREAM_EVT_GET_BUF:
        rc = mm_stream_init_bufs(my_obj);
        /* change state to buff allocated */
        if(0 == rc) {
            my_obj->state = MM_STREAM_STATE_BUFFED;
        }
        break;
    default:
        CDBG_ERROR("%s: Invalid evt=%d, stream_state=%d",
                   __func__, evt, my_obj->state);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

int32_t mm_stream_fsm_buffed(mm_stream_t * my_obj,
                             mm_stream_evt_type_t evt,
                             void * in_val,
                             void * out_val)
{
    int32_t rc = 0;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d, event = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state, evt);
    switch(evt) {
    case MM_STREAM_EVT_PUT_BUF:
        rc = mm_stream_deinit_bufs(my_obj);
        /* change state to configed */
        if(0 == rc) {
            my_obj->state = MM_STREAM_STATE_CFG;
        }
        break;
    case MM_STREAM_EVT_REG_BUF:
        rc = mm_stream_reg_buf(my_obj);
        /* change state to regged */
        if(0 == rc) {
            my_obj->state = MM_STREAM_STATE_REG;
        }
        break;
    case MM_STREAM_EVT_SET_PARM:
        rc = mm_stream_set_parm_config(my_obj, in_val);
        break;
    case MM_STREAM_EVT_GET_PARM:
        rc = mm_stream_get_parm_config(my_obj, out_val);
        break;
    default:
        CDBG_ERROR("%s: Invalid evt=%d, stream_state=%d",
                   __func__, evt, my_obj->state);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

int32_t mm_stream_fsm_reg(mm_stream_t * my_obj,
                          mm_stream_evt_type_t evt,
                          void * in_val,
                          void * out_val)
{
    int32_t rc = 0;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d, event = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state, evt);

    switch(evt) {
    case MM_STREAM_EVT_UNREG_BUF:
        rc = mm_stream_unreg_buf(my_obj);

        /* change state to buffed */
        my_obj->state = MM_STREAM_STATE_BUFFED;
        break;
    case MM_STREAM_EVT_START:
        {
            /* launch cmd thread if CB is not null */
            if (NULL != my_obj->buf_cb) {
                mm_camera_cmd_thread_launch(&my_obj->cmd_thread,
                                mm_stream_dispatch_app_data,
                                (void *)my_obj);

            }

            if(my_obj->need_stream_on) {
                rc = mm_stream_streamon(my_obj);
                if (0 != rc) {
                    /* failed stream on, need to release cmd thread if it's launched */
                    if (NULL != my_obj->buf_cb) {
                        mm_camera_cmd_thread_release(&my_obj->cmd_thread);

                    }
                    break;
                }
                my_obj->state = MM_STREAM_STATE_ACTIVE_STREAM_ON;
            } else {
                /* register CB at video fd */
                CDBG("%s : Video Size snapshot Enabled",__func__);
                mm_stream_data_cb_t cb;
                memset(&cb, 0, sizeof(mm_stream_data_cb_t));
                cb.cb_count = my_obj->num_stream_cb_times; /* reigstration cb times */
                if (cb.cb_count == 0) {
                    cb.cb_count = 1;
                }
                cb.user_data = (void*)my_obj;
                cb.cb = mm_stream_buf_notify;
                rc = mm_channel_reg_stream_cb(my_obj->ch_obj, &cb,
                                              MSM_V4L2_EXT_CAPTURE_MODE_VIDEO,
                                              my_obj->sensor_idx);
                my_obj->state = MM_STREAM_STATE_ACTIVE_STREAM_OFF;
            }
        }
        break;
    case MM_STREAM_EVT_SET_PARM:
        rc = mm_stream_set_parm_config(my_obj, in_val);
        break;
    case MM_STREAM_EVT_GET_PARM:
        rc = mm_stream_get_parm_config(my_obj, out_val);
        break;
    default:
        CDBG_ERROR("%s: Invalid evt=%d, stream_state=%d",
                   __func__, evt, my_obj->state);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

int32_t mm_stream_fsm_active_stream_on(mm_stream_t * my_obj,
                                              mm_stream_evt_type_t evt,
                                              void * in_val,
                                              void * out_val)
{
    int32_t rc = 0;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d, event = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state, evt);
    switch(evt) {
    case MM_STREAM_EVT_QBUF:
        rc = mm_stream_buf_done(my_obj, (mm_camera_buf_def_t *)in_val);
        break;
    case MM_STREAM_EVT_STOP:
        {
            rc = mm_stream_streamoff(my_obj);
            if (NULL != my_obj->buf_cb) {
                mm_camera_cmd_thread_release(&my_obj->cmd_thread);

            }
            my_obj->state = MM_STREAM_STATE_REG;
        }
        break;
    case MM_STREAM_EVT_SET_PARM:
        rc = mm_stream_set_parm_start(my_obj, in_val);
        break;
    case MM_STREAM_EVT_GET_PARM:
        rc = mm_stream_get_parm_start(my_obj, out_val);
        break;
    default:
        CDBG_ERROR("%s: Invalid evt=%d, stream_state=%d",
                   __func__, evt, my_obj->state);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

int32_t mm_stream_fsm_active_stream_off(mm_stream_t * my_obj,
                                               mm_stream_evt_type_t evt,
                                               void * in_val,
                                               void * out_val)
{
    int32_t rc = 0;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d, event = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state, evt);
    switch(evt) {
    case MM_STREAM_EVT_QBUF:
        rc = mm_stream_buf_done(my_obj, (mm_camera_buf_def_t *)in_val);
        break;
    case MM_STREAM_EVT_STOP:
        {
            if (NULL != my_obj->buf_cb) {
                rc = mm_camera_cmd_thread_release(&my_obj->cmd_thread);

            }
            my_obj->state = MM_STREAM_STATE_REG;
        }
        break;
   case MM_STREAM_EVT_SET_PARM:
        rc = mm_stream_set_parm_config(my_obj, in_val);
        break;
    case MM_STREAM_EVT_GET_PARM:
        rc = mm_stream_get_parm_config(my_obj, out_val);
        break;
    default:
        CDBG_ERROR("%s: Invalid evt=%d, stream_state=%d",
                   __func__, evt, my_obj->state);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

int32_t mm_stream_config(mm_stream_t *my_obj,
                         mm_camera_stream_config_t *config)
{
    int32_t rc = 0;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);
    memcpy(&my_obj->fmt, &config->fmt, sizeof(mm_camera_image_fmt_t));
    my_obj->hal_requested_num_bufs = config->num_of_bufs;
    my_obj->need_stream_on = config->need_stream_on;
    my_obj->num_stream_cb_times = config->num_stream_cb_times;

    rc = mm_stream_get_offset(my_obj);
    if(rc != 0) {
        CDBG_ERROR("%s: Error in offset query",__func__);
        return rc;
    }
    /* write back width and height to config in case mctl has modified the value */
    config->fmt.width = my_obj->fmt.width;
    config->fmt.height = my_obj->fmt.height;
    if(my_obj->need_stream_on) {
        /* only send fmt to backend if we need streamon */
        rc = mm_stream_set_fmt(my_obj);
    }
    return rc;
}

int32_t mm_stream_release(mm_stream_t *my_obj)
{
    int32_t rc;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);

    /* close fd */
    if(my_obj->fd > 0)
    {
        close(my_obj->fd);
    }

    /* destroy mutex */
    pthread_mutex_destroy(&my_obj->buf_lock);
    pthread_mutex_destroy(&my_obj->cb_lock);

    /* reset stream obj */
    memset(my_obj, 0, sizeof(mm_stream_t));
    my_obj->fd = -1;

    return 0;
}

int32_t mm_stream_streamon(mm_stream_t *my_obj)
{
    int32_t rc;
    enum v4l2_buf_type buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);
    /* Add fd to data poll thread */
    rc = mm_camera_poll_thread_add_poll_fd(&my_obj->ch_obj->poll_thread[0],
                                           my_obj->my_hdl,
                                           my_obj->fd,
                                           mm_stream_data_notify,
                                           (void*)my_obj);
    if (rc < 0) {
        return rc;
    }
    rc = ioctl(my_obj->fd, VIDIOC_STREAMON, &buf_type);
    if (rc < 0) {
        CDBG_ERROR("%s: ioctl VIDIOC_STREAMON failed: rc=%d\n",
                   __func__, rc);
        /* remove fd from data poll thread in case of failure */
        mm_camera_poll_thread_del_poll_fd(&my_obj->ch_obj->poll_thread[0], my_obj->my_hdl);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

int32_t mm_stream_streamoff(mm_stream_t *my_obj)
{
    int32_t rc;
    enum v4l2_buf_type buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);

    /* step1: remove fd from data poll thread */
    mm_camera_poll_thread_del_poll_fd(&my_obj->ch_obj->poll_thread[0], my_obj->my_hdl);

    /* step2: stream off */
    rc = ioctl(my_obj->fd, VIDIOC_STREAMOFF, &buf_type);
    if (rc < 0) {
        CDBG_ERROR("%s: STREAMOFF failed: %s\n",
                __func__, strerror(errno));
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static uint32_t mm_stream_util_get_v4l2_fmt(cam_format_t fmt,
                                            uint8_t *num_planes)
{
    uint32_t val;
    switch(fmt) {
    case CAMERA_YUV_420_NV12:
        val = V4L2_PIX_FMT_NV12;
        *num_planes = 2;
        break;
    case CAMERA_YUV_420_NV21:
        val = V4L2_PIX_FMT_NV21;
        *num_planes = 2;
        break;
    case CAMERA_BAYER_SBGGR10:
    case CAMERA_RDI:
        val= V4L2_PIX_FMT_SBGGR10;
        *num_planes = 1;
        break;
    case CAMERA_YUV_422_NV61:
        val= V4L2_PIX_FMT_NV61;
        *num_planes = 2;
        break;
    case CAMERA_SAEC:
        val = V4L2_PIX_FMT_STATS_AE;
        *num_planes = 1;
        break;
   case CAMERA_SAWB:
        val = V4L2_PIX_FMT_STATS_AWB;
        *num_planes = 1;
        break;
   case CAMERA_SAFC:
        val = V4L2_PIX_FMT_STATS_AF;
        *num_planes = 1;
        break;
   case CAMERA_SHST:
        val = V4L2_PIX_FMT_STATS_IHST;
        *num_planes = 1;
        break;
    case CAMERA_YUV_422_YUYV:
        val= V4L2_PIX_FMT_YUYV;
        *num_planes = 1;
        break;
    case CAMERA_YUV_420_YV12:
        val= V4L2_PIX_FMT_NV12;
        *num_planes = 3;
        break;
    default:
        val = 0;
        *num_planes = 0;
        CDBG_ERROR("%s: Unknown fmt=%d", __func__, fmt);
        break;
    }
    CDBG("%s: fmt=%d, val =%d, num_planes=%d", __func__, fmt, val , *num_planes);
    return val;
}

int32_t mm_stream_read_msm_frame(mm_stream_t * my_obj,
                                 mm_camera_buf_info_t* buf_info)
{
    int32_t idx = -1, rc = 0;
    struct v4l2_buffer vb;
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    uint32_t i = 0;
    uint8_t num_planes = 0;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);

    mm_stream_util_get_v4l2_fmt(my_obj->fmt.fmt,
                                &num_planes);

    memset(&vb,  0,  sizeof(vb));
    vb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    vb.memory = V4L2_MEMORY_USERPTR;
    vb.m.planes = &planes[0];
    vb.length = num_planes;

    rc = ioctl(my_obj->fd, VIDIOC_DQBUF, &vb);
    if (rc < 0) {
        CDBG_ERROR("%s: VIDIOC_DQBUF ioctl call failed (rc=%d)\n",
                   __func__, rc);
    } else {
        int8_t idx = vb.index;
        buf_info->buf = &my_obj->buf[idx];
        buf_info->frame_idx = vb.sequence;
        buf_info->stream_id = my_obj->my_hdl;

        buf_info->buf->stream_id = my_obj->my_hdl;
        buf_info->buf->buf_idx = idx;
        buf_info->buf->frame_idx = vb.sequence;
        buf_info->buf->ts.tv_sec  = vb.timestamp.tv_sec;
        buf_info->buf->ts.tv_nsec = vb.timestamp.tv_usec * 1000;

        for(i = 0; i < vb.length; i++) {
            CDBG("%s plane %d addr offset: %d data offset:%d\n",
                 __func__, i, vb.m.planes[i].reserved[0],
                 vb.m.planes[i].data_offset);
            buf_info->buf->planes[i].reserved[0] =
                vb.m.planes[i].reserved[0];
            buf_info->buf->planes[i].data_offset =
                vb.m.planes[i].data_offset;
        }


    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

int32_t mm_stream_get_crop(mm_stream_t *my_obj,
                           mm_camera_rect_t *crop)
{
    struct v4l2_crop crop_info;
    int32_t rc = 0;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);

    memset(&crop_info, 0, sizeof(crop_info));
    crop_info.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    rc = ioctl(my_obj->fd, VIDIOC_G_CROP, &crop_info);
    if (0 == rc) {
        crop->left = crop_info.c.left;
        crop->top = crop_info.c.top;
        crop->width = crop_info.c.width;
        crop->height = crop_info.c.height;
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

int32_t mm_stream_set_parm_acquire(mm_stream_t *my_obj,
                         void *in_value)
{
    int32_t rc = 0;
    mm_evt_paylod_stream_parm_t *payload = (mm_evt_paylod_stream_parm_t *)in_value;
    mm_camera_stream_parm_t parm_type = payload->parm_type;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d, parm_type = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state, parm_type);

    switch(parm_type) {
    case MM_CAMERA_STREAM_CID:{
            stream_cid_t *value = (stream_cid_t *)in_value;
            mm_stream_set_cid(my_obj,value);
            break;
        }
        default:
            CDBG_ERROR("%s : Parm -%d set is not supported here",__func__,(int)parm_type);
            break;
    }
    return rc;
}
int32_t mm_stream_get_parm_acquire(mm_stream_t *my_obj,
                         void *out_value)
{
    int32_t rc = 0;
    mm_evt_paylod_stream_parm_t *payload = (mm_evt_paylod_stream_parm_t *)out_value;
    mm_camera_stream_parm_t parm_type = payload->parm_type;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d, parm_type = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state, parm_type);

    switch(parm_type) {
    case MM_CAMERA_STREAM_CID:{
            stream_cid_t *value = (stream_cid_t *)out_value;
            rc = mm_stream_get_cid(my_obj,value);
            break;
        }
        case MM_CAMERA_STREAM_CROP:{
            mm_camera_rect_t *crop = (mm_camera_rect_t *)out_value;
            rc = mm_stream_get_crop(my_obj, crop);
            break;
        }
        default:
            CDBG_ERROR("%s : Parm -%d get is not supported here",__func__,(int)parm_type);
            break;
    }
    return rc;
}

int32_t mm_stream_set_parm_config(mm_stream_t *my_obj,
                         void *in_value)
{
    int32_t rc = 0;
    mm_evt_paylod_stream_parm_t *payload = (mm_evt_paylod_stream_parm_t *)in_value;
    mm_camera_stream_parm_t parm_type = payload->parm_type;
    void *value = payload->value;

    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d, parm_type = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state, parm_type);
    switch(parm_type) {
        default:
            CDBG_ERROR("%s : Parm -%d set is not supported here",__func__,(int)parm_type);
            break;
    }
    return rc;
}
int32_t mm_stream_get_parm_config(mm_stream_t *my_obj,
                         void *out_value)
{
    int32_t rc = 0;
    mm_evt_paylod_stream_parm_t *payload = (mm_evt_paylod_stream_parm_t *)out_value;

    if(payload == NULL) {
        CDBG_ERROR("%s : Invalid Argument",__func__);
        return -1;
    }
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
     "image_mode = %d, fd = %d, state = %d, parm_type = %d",
     __func__, my_obj->inst_hdl, my_obj->my_hdl,
     my_obj->ext_image_mode, my_obj->fd, my_obj->state, (int)payload->parm_type);
    switch(payload->parm_type) {
        case MM_CAMERA_STREAM_OFFSET:
            memcpy(payload->value,(void *)&my_obj->frame_offset,sizeof(mm_camera_frame_len_offset));
            break;
        case MM_CAMERA_STREAM_CROP:{
            mm_camera_rect_t *crop = (mm_camera_rect_t *)payload->value;
            rc = mm_stream_get_crop(my_obj, crop);
            break;
        }
        default:
            CDBG_ERROR("%s : Parm -%d get is not supported here",__func__,(int)payload->parm_type);
            break;
    }
    return rc;
}

int32_t mm_stream_set_parm_start(mm_stream_t *my_obj,
                         void *in_value)
{
    int32_t rc = 0;
    mm_evt_paylod_stream_parm_t *payload = (mm_evt_paylod_stream_parm_t *)in_value;
    mm_camera_stream_parm_t parm_type = payload->parm_type;
    void *value = payload->value;

    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d, parm_type = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state, parm_type);
    switch(parm_type) {
        default:
            CDBG_ERROR("%s : Parm -%d set is not supported here",__func__,(int)parm_type);
            break;
    }
    return rc;
}
int32_t mm_stream_get_parm_start(mm_stream_t *my_obj,
                         void *out_value)
{
    int32_t rc = 0;
    mm_evt_paylod_stream_parm_t *payload = (mm_evt_paylod_stream_parm_t *)out_value;

    if(payload == NULL) {
        CDBG_ERROR("%s : Invalid Argument",__func__);
        return -1;
    }
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
     "image_mode = %d, fd = %d, state = %d, parm_type = %d",
     __func__, my_obj->inst_hdl, my_obj->my_hdl,
     my_obj->ext_image_mode, my_obj->fd, my_obj->state, (int)payload->parm_type);
    switch(payload->parm_type) {
        case MM_CAMERA_STREAM_OFFSET:
            memcpy(payload->value,(void *)&my_obj->frame_offset,sizeof(mm_camera_frame_len_offset));
            break;
        case MM_CAMERA_STREAM_CROP:{
            mm_camera_rect_t *crop = (mm_camera_rect_t *)payload->value;
            rc = mm_stream_get_crop(my_obj, crop);
            break;
        }
        default:
            CDBG_ERROR("%s : Parm -%d get is not supported here",__func__,(int)payload->parm_type);
            break;
    }
    return rc;
}
int32_t mm_stream_set_ext_mode(mm_stream_t * my_obj)
{
    int32_t rc = 0;
    struct v4l2_streamparm s_parm;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);

    s_parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    s_parm.parm.capture.extendedmode = my_obj->ext_image_mode;

    rc = ioctl(my_obj->fd, VIDIOC_S_PARM, &s_parm);
        CDBG("%s:stream fd=%d, rc=%d, extended_mode=%d\n",
                 __func__, my_obj->fd, rc,
                 s_parm.parm.capture.extendedmode);
    return rc;
}

int32_t mm_stream_qbuf(mm_stream_t *my_obj, mm_camera_buf_def_t *buf)
{
    int32_t i, rc = 0;
    int *ret;
    struct v4l2_buffer buffer;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);

    memset(&buffer, 0, sizeof(buffer));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buffer.memory = V4L2_MEMORY_USERPTR;
    buffer.index = buf->buf_idx;
    buffer.m.planes = &buf->planes[0];
    buffer.length = buf->num_planes;

    CDBG("%s:stream_hdl=%d,fd=%d,frame idx=%d,num_planes = %d\n", __func__,
         buf->stream_id, buf->fd, buffer.index, buffer.length);

    rc = ioctl(my_obj->fd, VIDIOC_QBUF, &buffer);
    CDBG("%s: qbuf idx:%d, rc:%d", __func__, buffer.index, rc);
    return rc;
}

/* This function let kernel know amount of buffers will be registered */
int32_t mm_stream_request_buf(mm_stream_t * my_obj)
{
    int32_t rc = 0;
    uint8_t i,reg = 0;
    struct v4l2_requestbuffers bufreq;
    uint8_t buf_num = my_obj->buf_num;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);

    if(buf_num > MM_CAMERA_MAX_NUM_FRAMES) {
        CDBG_ERROR("%s: buf num %d > max limit %d\n",
                 __func__, buf_num, MM_CAMERA_MAX_NUM_FRAMES);
        return -1;
    }
    pthread_mutex_lock(&my_obj->buf_lock);
    for(i = 0; i < buf_num; i++){
        if (my_obj->buf_status[i].initial_reg_flag){
            reg = 1;
            break;
        }
    }
    pthread_mutex_unlock(&my_obj->buf_lock);
    if(!reg) {
        //No need to register a buffer
        CDBG_ERROR("No Need to register this buffer");
        return rc;
    }
    memset(&bufreq, 0, sizeof(bufreq));
    bufreq.count = buf_num;
    bufreq.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    bufreq.memory = V4L2_MEMORY_USERPTR;
    rc = ioctl(my_obj->fd, VIDIOC_REQBUFS, &bufreq);
    if (rc < 0) {
      CDBG_ERROR("%s: fd=%d, ioctl VIDIOC_REQBUFS failed: rc=%d\n",
           __func__, my_obj->fd, rc);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

int32_t mm_stream_init_bufs(mm_stream_t * my_obj)
{
    int32_t i, rc = 0, j;
    int image_type;
    mm_camear_mem_vtbl_t *mem_vtbl = NULL;
    mm_camera_frame_len_offset frame_offset;
    uint8_t *reg_flags = NULL;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);

    /* deinit buf if it's not NULL*/
    if (NULL != my_obj->buf) {
        mm_stream_deinit_bufs(my_obj);
    }

    my_obj->buf_num = my_obj->hal_requested_num_bufs;
    if (mm_camera_util_get_pp_mask(my_obj->ch_obj->cam_obj) > 0) {
        /* reserve extra one buf for pp */
        my_obj->buf_num++;
    }

    my_obj->buf =
        (mm_camera_buf_def_t*)malloc(sizeof(mm_camera_buf_def_t) * my_obj->buf_num);
    my_obj->buf_status =
        (mm_stream_buf_status_t*)malloc(sizeof(mm_stream_buf_status_t) * my_obj->buf_num);
    reg_flags = (uint8_t *)malloc(sizeof(uint8_t) * my_obj->buf_num);

    if (NULL == my_obj->buf ||
        NULL == my_obj->buf_status ||
        NULL == reg_flags) {
        CDBG_ERROR("%s: No memory for buf", __func__);
        rc = -1;
        goto error_malloc;
    }

    memset(my_obj->buf, 0, sizeof(mm_camera_buf_def_t) * my_obj->buf_num);
    memset(my_obj->buf_status, 0, sizeof(mm_stream_buf_status_t) * my_obj->buf_num);
    memset(reg_flags, 0, sizeof(uint8_t) * my_obj->buf_num);

    mem_vtbl = my_obj->ch_obj->cam_obj->mem_vtbl;
    rc = mem_vtbl->get_buf(my_obj->ch_obj->cam_obj->my_hdl,
                           my_obj->ch_obj->my_hdl,
                           my_obj->my_hdl,
                           mem_vtbl->user_data,
                           &my_obj->frame_offset,
                           my_obj->buf_num,
                           reg_flags,
                           my_obj->buf);

    if (0 != rc) {
        CDBG_ERROR("%s: Error get buf, rc = %d\n", __func__, rc);
        goto error_malloc;
    }

    for (i=0; i < my_obj->buf_num; i++) {
        my_obj->buf_status[i].initial_reg_flag = reg_flags[i];
        my_obj->buf[i].stream_id = my_obj->my_hdl;
    }

    free(reg_flags);
    reg_flags = NULL;

    for (i=0; i < my_obj->buf_num; i++) {
        if (my_obj->buf[i].fd > 0) {
            if(0 >= (rc = mm_camera_map_buf(my_obj->ch_obj->cam_obj,
                          my_obj->ext_image_mode,
                          i,
                          my_obj->buf[i].fd,
                          my_obj->buf[i].frame_len)))
            {
                CDBG_ERROR("%s: Error mapping buf (rc = %d)", __func__, rc);
                goto error_map;
            }
        } else {
            CDBG_ERROR("%s: Invalid fd for buf idx (%d)", __func__, i);
        }
    }

    return 0;

error_map:
    /* error, unmapping previously mapped bufs */
    for (j=0; j<i; j++) {
        if (my_obj->buf[j].fd > 0) {
            mm_camera_unmap_buf(my_obj->ch_obj->cam_obj,
                                my_obj->ext_image_mode,
                                j);
        }
    }

    /* put buf back */
    mem_vtbl->put_buf(my_obj->ch_obj->cam_obj->my_hdl,
                      my_obj->ch_obj->my_hdl,
                      my_obj->my_hdl,
                      mem_vtbl->user_data,
                      my_obj->buf_num,
                      my_obj->buf);

error_malloc:
    if (NULL != my_obj->buf) {
        free(my_obj->buf);
        my_obj->buf = NULL;
    }
    if (NULL != my_obj->buf_status) {
        free(my_obj->buf_status);
        my_obj->buf_status = NULL;
    }
    if (NULL != reg_flags) {
        free(reg_flags);
        reg_flags = NULL;
    }

    return rc;
}

/* return buffers to surface or release buffers allocated */
int32_t mm_stream_deinit_bufs(mm_stream_t * my_obj)
{
    int32_t rc = 0, i;
    mm_camear_mem_vtbl_t *mem_vtbl = NULL;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);

    if (NULL == my_obj->buf) {
        CDBG("%s: Buf is NULL, no need to deinit", __func__);
        return rc;
    }

    /* IOMMU unmapping */
    for (i=0; i<my_obj->buf_num; i++) {
        if (my_obj->buf[i].fd > 0) {
            rc = mm_camera_unmap_buf(my_obj->ch_obj->cam_obj,
                                     my_obj->ext_image_mode,
                                     i);
            if (rc < 0 ) {
                CDBG_ERROR("%s: Error unmapping bufs at idx(%d) rc=%d",
                           __func__, i, rc);
            }
        }
    }

    /* release bufs */
    mem_vtbl = my_obj->ch_obj->cam_obj->mem_vtbl;
    if (NULL != mem_vtbl) {
        rc = mem_vtbl->put_buf(my_obj->ch_obj->cam_obj->my_hdl,
                          my_obj->ch_obj->my_hdl,
                          my_obj->my_hdl,
                          mem_vtbl->user_data,
                          my_obj->buf_num,
                          my_obj->buf);
    } else {
        CDBG_ERROR("%s: mem table is NULL, cannot release buf", __func__);
        rc = -1;
    }
    free(my_obj->buf);
    my_obj->buf = NULL;
    free(my_obj->buf_status);
    my_obj->buf_status = NULL;

    return rc;
}

int32_t mm_stream_reg_buf(mm_stream_t * my_obj)
{
    int32_t rc = 0;
    uint8_t i;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);

    rc = mm_stream_request_buf(my_obj);
    if (rc != 0) {
        return rc;
    }

    pthread_mutex_lock(&my_obj->buf_lock);
    for(i = 0; i < my_obj->buf_num; i++){
        my_obj->buf[i].buf_idx = i;

        /* check if need to qbuf initially */
        if (my_obj->buf_status[i].initial_reg_flag) {
            rc = mm_stream_qbuf(my_obj, &my_obj->buf[i]);
            if (rc != 0) {
                CDBG_ERROR("%s: VIDIOC_QBUF rc = %d\n", __func__, rc);
                break;
            }
            my_obj->buf_status[i].buf_refcnt = 0;
            my_obj->buf_status[i].in_kernel = 1;
        } else {
            /* the buf is held by upper layer, will not queue into kernel.
             * add buf reference count */
            my_obj->buf_status[i].buf_refcnt = 1;
            my_obj->buf_status[i].in_kernel = 0;
        }
    }

    pthread_mutex_unlock(&my_obj->buf_lock);
    return rc;
}

int32_t mm_stream_unreg_buf(mm_stream_t * my_obj)
{
    struct v4l2_requestbuffers bufreq;
    int32_t i, rc = 0,reg = 0;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);

    pthread_mutex_lock(&my_obj->buf_lock);
    if (NULL != my_obj->buf_status) {
        for(i = 0; i < my_obj->buf_num; i++){
            if (my_obj->buf_status[i].initial_reg_flag){
                reg = 1;
                break;
            }
        }
    }
    pthread_mutex_unlock(&my_obj->buf_lock);
    if(!reg) {
        //No need to unregister a buffer
        goto end;
    }
    bufreq.count = 0;
    bufreq.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    bufreq.memory = V4L2_MEMORY_USERPTR;
    rc = ioctl(my_obj->fd, VIDIOC_REQBUFS, &bufreq);
    if (rc < 0) {
        CDBG_ERROR("%s: fd=%d, VIDIOC_REQBUFS failed, rc=%d\n",
              __func__, my_obj->fd, rc);
    }

end:
    /* reset buf reference count */
    pthread_mutex_lock(&my_obj->buf_lock);
    if (NULL != my_obj->buf_status) {
        for(i = 0; i < my_obj->buf_num; i++){
            my_obj->buf_status[i].buf_refcnt = 0;
            my_obj->buf_status[i].in_kernel = 0;
        }
    }
    pthread_mutex_unlock(&my_obj->buf_lock);

    return rc;
}

int32_t mm_stream_set_fmt(mm_stream_t *my_obj)
{
    int32_t rc = 0;
    struct v4l2_format fmt;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);

    if(my_obj->fmt.width == 0 || my_obj->fmt.height == 0) {
        CDBG_ERROR("%s:invalid input[w=%d,h=%d,fmt=%d]\n",
                 __func__, my_obj->fmt.width, my_obj->fmt.height, my_obj->fmt.fmt);
        return -1;
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = my_obj->fmt.width;
    fmt.fmt.pix_mp.height= my_obj->fmt.height;
    fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
    fmt.fmt.pix_mp.pixelformat =
            mm_stream_util_get_v4l2_fmt(my_obj->fmt.fmt,
                                      &(fmt.fmt.pix_mp.num_planes));
    rc = ioctl(my_obj->fd, VIDIOC_S_FMT, &fmt);
    CDBG("%s:fd=%d, ext_image_mode=%d, rc=%d, fmt.fmt.pix_mp.pixelformat : 0x%x\n",
       __func__, my_obj->fd, my_obj->ext_image_mode, rc,
       fmt.fmt.pix_mp.pixelformat);
    return rc;
}

int32_t mm_stream_get_offset(mm_stream_t *my_obj)
{
    int32_t rc = 0;
    cam_frame_resolution_t frame_offset;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);

    memset(&my_obj->frame_offset,0,sizeof(mm_camera_frame_len_offset));

    frame_offset.format = my_obj->fmt.fmt;
    frame_offset.rotation = my_obj->fmt.rotation;
    frame_offset.width = my_obj->fmt.width;
    frame_offset.height = my_obj->fmt.height;
    frame_offset.image_mode = my_obj->ext_image_mode;
    if (!my_obj->need_stream_on &&
        my_obj->ext_image_mode == MSM_V4L2_EXT_CAPTURE_MODE_MAIN) {
        /* in case of video-sized snapshot,
         * image_mode should be video when querying frame offset*/
        frame_offset.image_mode = MSM_V4L2_EXT_CAPTURE_MODE_VIDEO;
    }

    switch (frame_offset.image_mode) {
    case MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW:
        if (CAMERA_YUV_420_YV12 == frame_offset.format) {
            frame_offset.padding_format = CAMERA_PAD_TO_2K;
        } else {
            frame_offset.padding_format = CAMERA_PAD_TO_WORD;
        }
        break;
    case MSM_V4L2_EXT_CAPTURE_MODE_MAIN:
    case MSM_V4L2_EXT_CAPTURE_MODE_RAW:
    case MSM_V4L2_EXT_CAPTURE_MODE_THUMBNAIL:
    case MSM_V4L2_EXT_CAPTURE_MODE_RDI:
        frame_offset.padding_format = CAMERA_PAD_TO_WORD;
        break;
    case MSM_V4L2_EXT_CAPTURE_MODE_AEC:
    case MSM_V4L2_EXT_CAPTURE_MODE_AWB:
    case MSM_V4L2_EXT_CAPTURE_MODE_AF:
    case MSM_V4L2_EXT_CAPTURE_MODE_IHIST:
        break;
    case MSM_V4L2_EXT_CAPTURE_MODE_VIDEO:
    default:
        frame_offset.padding_format = CAMERA_PAD_TO_2K;
        break;
    }

    CDBG("%s: format = %d, image_mode = %d, padding_format = %d, rotation = %d,"
            "width = %d height = %d",
             __func__,frame_offset.format,frame_offset.image_mode,frame_offset.padding_format,
             frame_offset.rotation,frame_offset.width,frame_offset.height);

    rc = mm_camera_send_native_ctrl_cmd(my_obj->ch_obj->cam_obj,
                                            CAMERA_GET_PARM_FRAME_RESOLUTION,
                                            sizeof(cam_frame_resolution_t),
                                            &frame_offset);
    if(rc != 0) {
        CDBG_ERROR("%s: Failed to get the stream offset and frame length",__func__);
        return rc;
    }
    my_obj->fmt.width = frame_offset.width;
    my_obj->fmt.height = frame_offset.height;
    memcpy(&my_obj->frame_offset,&frame_offset.frame_offset,sizeof(mm_camera_frame_len_offset));
    CDBG("%s: Frame length = %d width = %d, height = %d, rc = %d",
         __func__,my_obj->frame_offset.frame_len,my_obj->fmt.width,my_obj->fmt.height,rc);
    return rc;
}


int32_t mm_stream_set_cid(mm_stream_t *my_obj,stream_cid_t *value)
{
    int32_t rc = 0;
    cam_cid_info_t cam_cid_info;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);

    cam_cid_info.num_cids = 1;
    cam_cid_info.cid_entries[0].cid = value->cid;
    cam_cid_info.cid_entries[0].dt = value->dt;
    cam_cid_info.cid_entries[0].inst_handle = my_obj->inst_hdl;

    rc = mm_camera_send_native_ctrl_cmd(my_obj->ch_obj->cam_obj,
                                            CAMERA_SET_PARM_CID,
                                            sizeof(cam_cid_info_t),
                                            &cam_cid_info);
    if(rc != 0) {
        CDBG_ERROR("%s: Failed to set the CID",__func__);
        return rc;
    }
    return rc;
}

int32_t mm_stream_get_cid(mm_stream_t *my_obj,stream_cid_t *out_value)
{
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);
    //TODO: Need to use sensor structure init in camera query
    int32_t rc = 0;
    return rc;
}

int32_t mm_stream_buf_done(mm_stream_t * my_obj,
                           mm_camera_buf_def_t *frame)
{
    int32_t rc = 0;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);

    if (my_obj->is_local_buf) {
        /* special case for video-sized live snapshot
         * buf is local, no need to qbuf to kernel */
        return 0;
    }

    pthread_mutex_lock(&my_obj->buf_lock);

    if(my_obj->buf_status[frame->buf_idx].buf_refcnt == 0) {
        CDBG("%s: Error Trying to free second time?(idx=%d) count=%d, ext_image_mode=%d\n",
                   __func__, frame->buf_idx,
                   my_obj->buf_status[frame->buf_idx].buf_refcnt,
                   my_obj->ext_image_mode);
        rc = -1;
    }else{
        my_obj->buf_status[frame->buf_idx].buf_refcnt--;
        if (0 == my_obj->buf_status[frame->buf_idx].buf_refcnt) {
            CDBG("<DEBUG> : Buf done for buffer:%d:%d", my_obj->ext_image_mode, frame->buf_idx);
            rc = mm_stream_qbuf(my_obj, frame);
            if(rc < 0) {
                CDBG_ERROR("%s: mm_camera_stream_qbuf(idx=%d) err=%d\n",
                     __func__, frame->buf_idx, rc);
            } else {
                my_obj->buf_status[frame->buf_idx].in_kernel = 1;
            }
        }else{
            CDBG("<DEBUG> : Still ref count pending count :%d",
                 my_obj->buf_status[frame->buf_idx].buf_refcnt);
            CDBG("<DEBUG> : for buffer:%p:%d, ext_image_mode=%d",
                 my_obj, frame->buf_idx, my_obj->ext_image_mode);
        }
    }
    pthread_mutex_unlock(&my_obj->buf_lock);
    return rc;
}

int32_t mm_stream_reg_buf_cb(mm_stream_t *my_obj,
                             mm_stream_data_cb_t *val)
{
    int32_t rc = -1;
    uint8_t i;
    CDBG("%s: E, inst_handle = 0x%x, my_handle = 0x%x, "
         "image_mode = %d, fd = %d, state = %d",
         __func__, my_obj->inst_hdl, my_obj->my_hdl,
         my_obj->ext_image_mode, my_obj->fd, my_obj->state);

    pthread_mutex_lock(&my_obj->cb_lock);
    for (i=0 ;i < MM_CAMERA_STREAM_BUF_CB_MAX; i++) {
        if(NULL == my_obj->buf_cb[i].cb) {
            memcpy(&my_obj->buf_cb[i], val, sizeof(mm_stream_data_cb_t));
            rc = 0;
            break;
        }
    }
    pthread_mutex_unlock(&my_obj->cb_lock);

    return rc;
}
