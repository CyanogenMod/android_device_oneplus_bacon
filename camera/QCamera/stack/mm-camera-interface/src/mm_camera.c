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
#include <semaphore.h>

#include "mm_camera_dbg.h"
#include "mm_camera_sock.h"
#include "mm_camera_interface.h"
#include "mm_camera.h"

#define SET_PARM_BIT32(parm, parm_arr) \
    (parm_arr[parm/32] |= (1<<(parm%32)))

#define GET_PARM_BIT32(parm, parm_arr) \
    ((parm_arr[parm/32]>>(parm%32))& 0x1)

/* internal function declare */
int32_t mm_camera_send_native_ctrl_cmd(mm_camera_obj_t * my_obj,
                                       cam_ctrl_type type,
                                       uint32_t length,
                                       void *value);
int32_t mm_camera_send_native_ctrl_timeout_cmd(mm_camera_obj_t * my_obj,
                                               cam_ctrl_type type,
                                               uint32_t length,
                                               void *value,
                                               int timeout);
int mm_camera_evt_sub(mm_camera_obj_t * my_obj,
                      mm_camera_event_type_t evt_type,
                      int reg_count);
int32_t mm_camera_set_general_parm(mm_camera_obj_t * my_obj,
                                   mm_camera_parm_type_t parm_type,
                                   void* p_value);
int32_t mm_camera_enqueue_evt(mm_camera_obj_t *my_obj,
                              mm_camera_event_t *event);
extern int32_t mm_channel_init(mm_channel_t *my_obj);

mm_channel_t * mm_camera_util_get_channel_by_handler(
                                    mm_camera_obj_t * cam_obj,
                                    uint32_t handler)
{
    int i;
    mm_channel_t *ch_obj = NULL;
    uint8_t ch_idx = mm_camera_util_get_index_by_handler(handler);

    for(i = 0; i < MM_CAMERA_CHANNEL_MAX; i++) {
        if (handler == cam_obj->ch[i].my_hdl) {
            ch_obj = &cam_obj->ch[i];
            break;
        }
    }
    return ch_obj;
}

static void mm_camera_dispatch_app_event(mm_camera_cmdcb_t *cmd_cb,
                                         void* user_data)
{
    int i;
    mm_camera_event_type_t evt_type = cmd_cb->u.evt.event_type;
    mm_camera_event_t *event = &cmd_cb->u.evt;
    mm_camera_obj_t * my_obj = (mm_camera_obj_t *)user_data;
    if (NULL != my_obj) {
        if (evt_type < MM_CAMERA_EVT_TYPE_MAX) {
            pthread_mutex_lock(&my_obj->cb_lock);
            for(i = 0; i < MM_CAMERA_EVT_ENTRY_MAX; i++) {
                if(my_obj->evt[evt_type].evt[i].evt_cb) {
                    my_obj->evt[evt_type].evt[i].evt_cb(
                        my_obj->my_hdl,
                        event,
                        my_obj->evt[evt_type].evt[i].user_data);
                }
            }
            pthread_mutex_unlock(&my_obj->cb_lock);
        }
    }
}

static void mm_camera_handle_async_cmd(mm_camera_cmdcb_t *cmd_cb,
                                       void* user_data)
{
    int i;
    mm_camera_async_cmd_t *async_cmd = &cmd_cb->u.async;
    mm_camera_obj_t * my_obj = NULL;
    mm_evt_payload_stop_stream_t payload;

    my_obj = (mm_camera_obj_t *)user_data;
    if (NULL != my_obj) {
        if (MM_CAMERA_ASYNC_CMD_TYPE_STOP == async_cmd->cmd_type) {
            memset(&payload, 0, sizeof(mm_evt_payload_stop_stream_t));
            payload.num_streams = async_cmd->u.stop_cmd.num_streams;
            payload.stream_ids = async_cmd->u.stop_cmd.stream_ids;
            mm_channel_fsm_fn(async_cmd->u.stop_cmd.ch_obj,
                                   MM_CHANNEL_EVT_TEARDOWN_STREAM,
                                   (void*)&payload,
                                   NULL);
        }
    }
}

static void mm_camera_event_notify(void* user_data)
{
    struct v4l2_event ev;
    int rc;
    mm_camera_event_t *evt = NULL;
    mm_camera_cmdcb_t *node = NULL;

    mm_camera_obj_t *my_obj = (mm_camera_obj_t*)user_data;
    if (NULL != my_obj) {
        /* read evt */
        memset(&ev, 0, sizeof(ev));
        rc = ioctl(my_obj->ctrl_fd, VIDIOC_DQEVENT, &ev);
        evt = (mm_camera_event_t *)ev.u.data;

        if (rc >= 0) {
            if(ev.type == V4L2_EVENT_PRIVATE_START+MSM_CAM_APP_NOTIFY_ERROR_EVENT) {
                evt->event_type = MM_CAMERA_EVT_TYPE_CTRL;
                evt->e.ctrl.evt = MM_CAMERA_CTRL_EVT_ERROR;
                node = (mm_camera_cmdcb_t *)malloc(sizeof(mm_camera_cmdcb_t));
                if (NULL != node) {
                    memset(node, 0, sizeof(mm_camera_cmdcb_t));
                    node->cmd_type = MM_CAMERA_CMD_TYPE_EVT_CB;
                    memcpy(&node->u.evt, evt, sizeof(mm_camera_event_t));
                }
            } else {
                switch (evt->event_type) {
                case MM_CAMERA_EVT_TYPE_CH:
                case MM_CAMERA_EVT_TYPE_CTRL:
                case MM_CAMERA_EVT_TYPE_STATS:
                case MM_CAMERA_EVT_TYPE_INFO:
                    {
                        node = (mm_camera_cmdcb_t *)malloc(sizeof(mm_camera_cmdcb_t));
                        if (NULL != node) {
                            memset(node, 0, sizeof(mm_camera_cmdcb_t));
                            node->cmd_type = MM_CAMERA_CMD_TYPE_EVT_CB;
                            memcpy(&node->u.evt, evt, sizeof(mm_camera_event_t));
                        }
                    }
                    break;
                case MM_CAMERA_EVT_TYPE_PRIVATE_EVT:
                    {
                        CDBG("%s: MM_CAMERA_EVT_TYPE_PRIVATE_EVT", __func__);
                        struct msm_camera_v4l2_ioctl_t v4l2_ioctl;
                        int32_t length =
                            sizeof(mm_camera_cmdcb_t) + evt->e.pri_evt.data_length;
                        node = (mm_camera_cmdcb_t *)malloc(length);
                        if (NULL != node) {
                            memset(node, 0, length);
                            node->cmd_type = MM_CAMERA_CMD_TYPE_EVT_CB;
                            memcpy(&node->u.evt, evt, sizeof(mm_camera_event_t));

                            if (evt->e.pri_evt.data_length > 0) {
                                CDBG("%s: data_length =%d (trans_id=%d), dequeue payload",
                                     __func__, evt->e.pri_evt.data_length,
                                     node->u.evt.e.pri_evt.trans_id);
                                /* dequeue event payload if length > 0 */
                                memset(&v4l2_ioctl, 0, sizeof(v4l2_ioctl));
                                v4l2_ioctl.trans_code = node->u.evt.e.pri_evt.trans_id;
                                v4l2_ioctl.len = node->u.evt.e.pri_evt.data_length;
                                v4l2_ioctl.ioctl_ptr = &(node->u.evt.e.pri_evt.evt_data[0]);
                                rc = ioctl(my_obj->ctrl_fd, MSM_CAM_V4L2_IOCTL_GET_EVENT_PAYLOAD,
                                           &v4l2_ioctl);
                                if (rc < 0) {
                                    CDBG_ERROR("%s: get event payload returns error = %d",
                                               __func__, rc);
                                    free(node);
                                    node = NULL;
                                } else {
                                    CDBG("%s: data_length =%d (trans_id=%d) (payload=%s)",
                                         __func__, evt->e.pri_evt.data_length,
                                         node->u.evt.e.pri_evt.trans_id,
                                         (char*)&node->u.evt.e.pri_evt.evt_data[0]);
                                }
                            }
                        }
                    }
                    break;
                default:
                    break;
                }
            }
            if (NULL != node) {
                /* enqueue to evt cmd thread */
                mm_camera_queue_enq(&(my_obj->evt_thread.cmd_queue), node);
                /* wake up evt cmd thread */
                sem_post(&(my_obj->evt_thread.cmd_sem));
            }
        }
    }
}

int32_t mm_camera_enqueue_evt(mm_camera_obj_t *my_obj,
                              mm_camera_event_t *event)
{
    int32_t rc = 0;
    mm_camera_cmdcb_t *node = NULL;

    node = (mm_camera_cmdcb_t *)malloc(sizeof(mm_camera_cmdcb_t));
    if (NULL != node) {
        memset(node, 0, sizeof(mm_camera_cmdcb_t));
        node->cmd_type = MM_CAMERA_CMD_TYPE_EVT_CB;
        memcpy(&node->u.evt, event, sizeof(mm_camera_event_t));

        /* enqueue to evt cmd thread */
        mm_camera_queue_enq(&(my_obj->evt_thread.cmd_queue), node);
        /* wake up evt cmd thread */
        sem_post(&(my_obj->evt_thread.cmd_sem));
    } else {
        CDBG_ERROR("%s: No memory for mm_camera_node_t", __func__);
        rc = -1;
    }

    return rc;
}

/* send local CH evt to HAL
 * may not needed since we have return val for each channel/stream operation */
int32_t mm_camera_send_ch_event(mm_camera_obj_t *my_obj,
                                uint32_t ch_id,
                                uint32_t stream_id,
                                mm_camera_ch_event_type_t evt)
{
    int rc = 0;
    mm_camera_event_t event;
    event.event_type = MM_CAMERA_EVT_TYPE_CH;
    event.e.ch.evt = evt;
    /* TODO: need to change the ch evt struct to include ch_id and stream_id. */
    event.e.ch.ch = stream_id;
    CDBG("%s: stream on event, type=0x%x, ch=%d, evt=%d",
         __func__, event.event_type, event.e.ch.ch, event.e.ch.evt);
    rc = mm_camera_enqueue_evt(my_obj, &event);
    return rc;
}

int32_t mm_camera_open(mm_camera_obj_t *my_obj)
{
    char dev_name[MM_CAMERA_DEV_NAME_LEN];
    int32_t rc = 0;
    int8_t n_try=MM_CAMERA_DEV_OPEN_TRIES;
    uint8_t sleep_msec=MM_CAMERA_DEV_OPEN_RETRY_SLEEP;
    uint8_t i;
    uint8_t cam_idx = mm_camera_util_get_index_by_handler(my_obj->my_hdl);

    CDBG("%s:  begin\n", __func__);

    snprintf(dev_name, sizeof(dev_name), "/dev/%s",
             mm_camera_util_get_dev_name(my_obj->my_hdl));

    do{
        n_try--;
        my_obj->ctrl_fd = open(dev_name, O_RDWR | O_NONBLOCK);
        CDBG("%s:  ctrl_fd = %d, errno == %d", __func__, my_obj->ctrl_fd, errno);
        if((my_obj->ctrl_fd > 0) || (errno != EIO) || (n_try <= 0 )) {
            CDBG_ERROR("%s:  opened, break out while loop", __func__);
            break;
        }
        CDBG("%s:failed with I/O error retrying after %d milli-seconds",
             __func__,sleep_msec);
        usleep(sleep_msec*1000);
    }while(n_try>0);

    if (my_obj->ctrl_fd <= 0) {
        CDBG_ERROR("%s: cannot open control fd of '%s' (%s)\n",
                 __func__, dev_name, strerror(errno));
        rc = -1;
        goto on_error;
    }

    /* open domain socket*/
    n_try=MM_CAMERA_DEV_OPEN_TRIES;
    do{
        n_try--;
        my_obj->ds_fd = mm_camera_socket_create(cam_idx, MM_CAMERA_SOCK_TYPE_UDP);
        CDBG("%s:  ds_fd = %d, errno = %d", __func__, my_obj->ds_fd, errno);
        if((my_obj->ds_fd > 0) || (n_try <= 0 )) {
            CDBG("%s:  opened, break out while loop", __func__);
            break;
        }
        CDBG("%s:failed with I/O error retrying after %d milli-seconds",
             __func__,sleep_msec);
        usleep(sleep_msec*1000);
    }while(n_try>0);

    if (my_obj->ds_fd <= 0) {
        CDBG_ERROR("%s: cannot open domain socket fd of '%s'(%s)\n",
                 __func__, dev_name, strerror(errno));
        rc = -1;
        goto on_error;
    }

    /* set ctrl_fd to be the mem_mapping fd */
    rc =  mm_camera_util_s_ctrl(my_obj->ctrl_fd,
                        MSM_V4L2_PID_MMAP_INST, 0);
    if (rc < 0) {
        CDBG_ERROR("error: ioctl VIDIOC_S_CTRL MSM_V4L2_PID_MMAP_INST failed: %s\n",
                   strerror(errno));
        goto on_error;
    }

    /* set geo mode to 2D by default */
    my_obj->current_mode = CAMERA_MODE_2D;

    pthread_mutex_init(&my_obj->cb_lock, NULL);

    CDBG("%s : Launch async cmd Thread in Cam Open",__func__);
    mm_camera_cmd_thread_launch(&my_obj->async_cmd_thread,
                                mm_camera_handle_async_cmd,
                                (void *)my_obj);

    CDBG("%s : Launch evt Thread in Cam Open",__func__);
    mm_camera_cmd_thread_launch(&my_obj->evt_thread,
                                mm_camera_dispatch_app_event,
                                (void *)my_obj);

    /* launch event poll thread
     * we will add evt fd into event poll thread upon user first register for evt */
    CDBG("%s : Launch evt Poll Thread in Cam Open",__func__);
    mm_camera_poll_thread_launch(&my_obj->evt_poll_thread,
                                 MM_CAMERA_POLL_TYPE_EVT);

    CDBG("%s:  end (rc = %d)\n", __func__, rc);
    /* we do not need to unlock cam_lock here before return
     * because for open, it's done within intf_lock */
    return rc;

on_error:
    if (my_obj->ctrl_fd > 0) {
        close(my_obj->ctrl_fd);
        my_obj->ctrl_fd = -1;
    }
    if (my_obj->ds_fd > 0) {
        mm_camera_socket_close(my_obj->ds_fd);
       my_obj->ds_fd = -1;
    }

    /* we do not need to unlock cam_lock here before return
     * because for open, it's done within intf_lock */
    return rc;
}

int32_t mm_camera_close(mm_camera_obj_t *my_obj)
{
    CDBG("%s : Close evt Poll Thread in Cam Close",__func__);
    mm_camera_poll_thread_release(&my_obj->evt_poll_thread);

    CDBG("%s : Close evt cmd Thread in Cam Close",__func__);
    mm_camera_cmd_thread_release(&my_obj->evt_thread);

    CDBG("%s : Close asyn cmd Thread in Cam Close",__func__);
    mm_camera_cmd_thread_release(&my_obj->async_cmd_thread);

    if(my_obj->ctrl_fd > 0) {
        close(my_obj->ctrl_fd);
        my_obj->ctrl_fd = -1;
    }
    if(my_obj->ds_fd > 0) {
        mm_camera_socket_close(my_obj->ds_fd);
        my_obj->ds_fd = -1;
    }

    pthread_mutex_destroy(&my_obj->cb_lock);

    pthread_mutex_unlock(&my_obj->cam_lock);
    return 0;
}

uint8_t mm_camera_is_event_supported(mm_camera_obj_t *my_obj, mm_camera_event_type_t evt_type)
{
    switch(evt_type) {
    case MM_CAMERA_EVT_TYPE_CH:
    case MM_CAMERA_EVT_TYPE_CTRL:
    case MM_CAMERA_EVT_TYPE_STATS:
    case MM_CAMERA_EVT_TYPE_INFO:
      return 1;
    default:
      return 0;
    }
    return 0;
}

int32_t mm_camera_register_event_notify_internal(
                                   mm_camera_obj_t *my_obj,
                                   mm_camera_event_notify_t evt_cb,
                                   void * user_data,
                                   mm_camera_event_type_t evt_type)
{
    int i;
    int rc = -1;
    mm_camera_evt_obj_t *evt_array = NULL;

    pthread_mutex_lock(&my_obj->cb_lock);
    evt_array = &my_obj->evt[evt_type];
    if(evt_cb) {
        /* this is reg case */
        for(i = 0; i < MM_CAMERA_EVT_ENTRY_MAX; i++) {
            if(evt_array->evt[i].user_data == NULL) {
                evt_array->evt[i].evt_cb = evt_cb;
                evt_array->evt[i].user_data = user_data;
                evt_array->reg_count++;
                rc = 0;
                break;
            }
        }
    } else {
        /* this is unreg case */
        for(i = 0; i < MM_CAMERA_EVT_ENTRY_MAX; i++) {
            if(evt_array->evt[i].user_data == user_data) {
                evt_array->evt[i].evt_cb = NULL;
                evt_array->evt[i].user_data = NULL;
                evt_array->reg_count--;
                rc = 0;
                break;
            }
        }
    }

    if(rc == 0 && evt_array->reg_count <= 1) {
        /* subscribe/unsubscribe event to kernel */
        rc = mm_camera_evt_sub(my_obj, evt_type, evt_array->reg_count);
    }

    pthread_mutex_unlock(&my_obj->cb_lock);
    return rc;
}

int32_t mm_camera_register_event_notify(mm_camera_obj_t *my_obj,
                                   mm_camera_event_notify_t evt_cb,
                                   void * user_data,
                                   mm_camera_event_type_t evt_type)
{
    int i;
    int rc = -1;
    mm_camera_evt_obj_t *evt_array = &my_obj->evt[evt_type];

    rc = mm_camera_register_event_notify_internal(my_obj, evt_cb,
                                                  user_data, evt_type);

    pthread_mutex_unlock(&my_obj->cam_lock);
    return rc;
}

int32_t mm_camera_qbuf(mm_camera_obj_t *my_obj,
                       uint32_t ch_id,
                       mm_camera_buf_def_t *buf)
{
    int rc = -1;
    mm_channel_t * ch_obj = NULL;
    ch_obj = mm_camera_util_get_channel_by_handler(my_obj, ch_id);

    pthread_mutex_unlock(&my_obj->cam_lock);

    /* we always assume qbuf will be done before channel/stream is fully stopped
     * because qbuf is done within dataCB context
     * in order to avoid deadlock, we are not locking ch_lock for qbuf */
    if (NULL != ch_obj) {
        rc = mm_channel_qbuf(ch_obj, buf);
    }

    return rc;
}

mm_camera_2nd_sensor_t * mm_camera_query_2nd_sensor_info(mm_camera_obj_t *my_obj)
{
    /* TODO: need to sync with backend how to get 2nd sensor info */
    return NULL;
}

int32_t mm_camera_sync(mm_camera_obj_t *my_obj)
{
    int32_t rc = 0;

    /* get camera capabilities */
    memset(&my_obj->properties, 0, sizeof(cam_prop_t));
    rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                        CAMERA_GET_CAPABILITIES,
                                        sizeof(cam_prop_t),
                                        (void *)&my_obj->properties);
    if (rc != 0) {
        CDBG_ERROR("%s: cannot get camera capabilities\n", __func__);
        goto on_error;
    }

on_error:
    pthread_mutex_unlock(&my_obj->cam_lock);
    return rc;

}

int32_t mm_camera_is_op_supported(mm_camera_obj_t *my_obj,
                                   mm_camera_ops_type_t op_code)
{
    int32_t rc = 0;
    int32_t is_ops_supported = false;
    int index = 0;

    if (op_code != MM_CAMERA_OPS_LOCAL) {
        index = op_code/32;
        is_ops_supported = ((my_obj->properties.ops[index] &
            (1<<op_code)) != 0);

    }
    pthread_mutex_unlock(&my_obj->cam_lock);
    return is_ops_supported;
}

int32_t mm_camera_is_parm_supported(mm_camera_obj_t *my_obj,
                                   mm_camera_parm_type_t parm_type,
                                   uint8_t *support_set_parm,
                                   uint8_t *support_get_parm)
{
    /* TODO: need to sync with backend if it can support set/get */
    int32_t rc = 0;
    *support_set_parm = GET_PARM_BIT32(parm_type,
                                       my_obj->properties.parm);
    *support_get_parm = GET_PARM_BIT32(parm_type,
                                       my_obj->properties.parm);
    pthread_mutex_unlock(&my_obj->cam_lock);

    return rc;
}

int32_t mm_camera_util_set_op_mode(mm_camera_obj_t * my_obj,
                                   mm_camera_op_mode_type_t *op_mode)
{
    int32_t rc = 0;
    int32_t v4l2_op_mode = MSM_V4L2_CAM_OP_DEFAULT;

    if (my_obj->op_mode == *op_mode)
        goto end;
    switch(*op_mode) {
    case MM_CAMERA_OP_MODE_ZSL:
        v4l2_op_mode = MSM_V4L2_CAM_OP_ZSL;
            break;
    case MM_CAMERA_OP_MODE_CAPTURE:
        v4l2_op_mode = MSM_V4L2_CAM_OP_CAPTURE;
            break;
    case MM_CAMERA_OP_MODE_VIDEO:
        v4l2_op_mode = MSM_V4L2_CAM_OP_VIDEO;
            break;
    case MM_CAMERA_OP_MODE_RAW:
        v4l2_op_mode = MSM_V4L2_CAM_OP_RAW;
            break;
    default:
        rc = - 1;
        goto end;
        break;
    }
    if(0 != (rc =  mm_camera_util_s_ctrl(my_obj->ctrl_fd,
            MSM_V4L2_PID_CAM_MODE, v4l2_op_mode))){
        CDBG_ERROR("%s: input op_mode=%d, s_ctrl rc=%d\n", __func__, *op_mode, rc);
        goto end;
    }
    /* if success update mode field */
    my_obj->op_mode = *op_mode;
end:
    CDBG("%s: op_mode=%d,rc=%d\n", __func__, *op_mode, rc);
    return rc;
}

int32_t mm_camera_set_parm(mm_camera_obj_t *my_obj,
                           mm_camera_parm_type_t parm_type,
                           void* p_value)
{
    int32_t rc = 0;
    CDBG("%s type =%d", __func__, parm_type);
    switch(parm_type) {
    case MM_CAMERA_PARM_OP_MODE:
        rc = mm_camera_util_set_op_mode(my_obj,
                        (mm_camera_op_mode_type_t *)p_value);
        break;
    case MM_CAMERA_PARM_DIMENSION:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                    CAMERA_SET_PARM_DIMENSION, sizeof(cam_ctrl_dimension_t), p_value);
        if(rc != 0) {
            CDBG("%s: mm_camera_send_native_ctrl_cmd err=%d\n", __func__, rc);
            break;
        }
        memcpy(&my_obj->dim, (cam_ctrl_dimension_t *)p_value,
                     sizeof(cam_ctrl_dimension_t));
        CDBG("%s: dw=%d,dh=%d,vw=%d,vh=%d,pw=%d,ph=%d,tw=%d,th=%d,raw_w=%d,raw_h=%d\n",
                 __func__,
                 my_obj->dim.display_width,my_obj->dim.display_height,
                 my_obj->dim.video_width, my_obj->dim.video_height,
                 my_obj->dim.picture_width,my_obj->dim.picture_height,
                 my_obj->dim.ui_thumbnail_width,my_obj->dim.ui_thumbnail_height,
                 my_obj->dim.raw_picture_width,my_obj->dim.raw_picture_height);
        break;
    case MM_CAMERA_PARM_SNAPSHOT_BURST_NUM:
        my_obj->snap_burst_num_by_user = *((uint32_t *)p_value);
        break;
    default:
        rc = mm_camera_set_general_parm(my_obj, parm_type, p_value);
        break;
    }
    pthread_mutex_unlock(&my_obj->cam_lock);
    return rc;
}

int32_t mm_camera_get_parm(mm_camera_obj_t *my_obj,
                           mm_camera_parm_type_t parm_type,
                           void* p_value)
{
    int32_t rc = 0;

    switch(parm_type) {
    case MM_CAMERA_PARM_FRAME_RESOLUTION:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_GET_PARM_FRAME_RESOLUTION,
                                            sizeof(cam_frame_resolution_t),
                                            p_value);
        if (rc < 0)
            CDBG_ERROR("%s: ERROR in CAMERA_GET_PARM_FRAME_RESOLUTION, rc = %d",
                 __func__, rc);
        break;
    case MM_CAMERA_PARM_MAX_PICTURE_SIZE:
        {
            mm_camera_dimension_t *dim =
                (mm_camera_dimension_t *)p_value;
            dim->height = my_obj->properties.max_pict_height;
            dim->width = my_obj->properties.max_pict_width;
            CDBG("%s: Max Picture Size: %d X %d\n", __func__,
                 dim->width, dim->height);
        }
        break;
    case MM_CAMERA_PARM_PREVIEW_FORMAT:
        *((int *)p_value) = my_obj->properties.preview_format;
        break;
    case MM_CAMERA_PARM_PREVIEW_SIZES_CNT:
        *((int *)p_value) = my_obj->properties.preview_sizes_cnt;
        break;
    case MM_CAMERA_PARM_VIDEO_SIZES_CNT:
        *((int *)p_value) = my_obj->properties.video_sizes_cnt;
        break;
    case MM_CAMERA_PARM_THUMB_SIZES_CNT:
        *((int *)p_value) = my_obj->properties.thumb_sizes_cnt;
        break;
    case MM_CAMERA_PARM_HFR_SIZES_CNT:
        *((int *)p_value) = my_obj->properties.hfr_sizes_cnt;
        break;
    case MM_CAMERA_PARM_HFR_FRAME_SKIP:
        *((int *)p_value) = my_obj->properties.hfr_frame_skip;
        break;
    case MM_CAMERA_PARM_DEFAULT_PREVIEW_WIDTH:
        *((int *)p_value) = my_obj->properties.default_preview_width;
        break;
    case MM_CAMERA_PARM_DEFAULT_PREVIEW_HEIGHT:
        *((int *)p_value) = my_obj->properties.default_preview_height;
        break;
    case MM_CAMERA_PARM_MAX_PREVIEW_SIZE:
        {
            mm_camera_dimension_t *dim =
                (mm_camera_dimension_t *)p_value;
            dim->height = my_obj->properties.max_preview_height;
            dim->width = my_obj->properties.max_preview_width;
            CDBG("%s: Max Preview Size: %d X %d\n", __func__,
                 dim->width, dim->height);
        }
        break;
    case MM_CAMERA_PARM_MAX_VIDEO_SIZE:
        {
            mm_camera_dimension_t *dim =
                (mm_camera_dimension_t *)p_value;
            dim->height = my_obj->properties.max_video_height;
            dim->width = my_obj->properties.max_video_width;
            CDBG("%s: Max Video Size: %d X %d\n", __func__,
                 dim->width, dim->height);
        }
        break;
    case MM_CAMERA_PARM_MAX_HFR_MODE:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_GET_PARM_MAX_HFR_MODE,
                                            sizeof(camera_hfr_mode_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_FOCAL_LENGTH:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_GET_PARM_FOCAL_LENGTH,
                                            sizeof(float),
                                            p_value);
        break;
    case MM_CAMERA_PARM_HORIZONTAL_VIEW_ANGLE:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_GET_PARM_HORIZONTAL_VIEW_ANGLE,
                                            sizeof(float),
                                            p_value);
        break;
    case MM_CAMERA_PARM_VERTICAL_VIEW_ANGLE:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_GET_PARM_VERTICAL_VIEW_ANGLE,
                                            sizeof(float),
                                            p_value);
        break;
    case MM_CAMERA_PARM_FOCUS_DISTANCES:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_GET_PARM_FOCUS_DISTANCES,
                                            sizeof(focus_distances_info_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_QUERY_FALSH4SNAP:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_QUERY_FLASH_FOR_SNAPSHOT,
                                            sizeof(int),
                                            p_value);
        break;
    case MM_CAMERA_PARM_3D_FRAME_FORMAT:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_GET_PARM_3D_FRAME_FORMAT,
                                            sizeof(camera_3d_frame_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_MAXZOOM:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_GET_PARM_MAXZOOM,
                                            sizeof(int),
                                            p_value);
        break;
    case MM_CAMERA_PARM_ZOOM_RATIO:
        {
            mm_camera_zoom_tbl_t *tbl = (mm_camera_zoom_tbl_t *)p_value;
            rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                                CAMERA_GET_PARM_ZOOMRATIOS,
                                                sizeof(int16_t)*tbl->size,
                                                (void *)(tbl->zoom_ratio_tbl));
        }
        break;
    case MM_CAMERA_PARM_DEF_PREVIEW_SIZES:
        {
            default_sizes_tbl_t *tbl = (default_sizes_tbl_t*)p_value;
            rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                                CAMERA_GET_PARM_DEF_PREVIEW_SIZES,
                                                sizeof(struct camera_size_type)*tbl->tbl_size,
                                                (void* )(tbl->sizes_tbl));
        }
        break;
    case MM_CAMERA_PARM_DEF_VIDEO_SIZES:
        {
            default_sizes_tbl_t *tbl = (default_sizes_tbl_t*)p_value;
            rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                                CAMERA_GET_PARM_DEF_VIDEO_SIZES,
                                                sizeof(struct camera_size_type)*tbl->tbl_size,
                                                (void *)(tbl->sizes_tbl));
        }
        break;
    case MM_CAMERA_PARM_DEF_THUMB_SIZES:
        {
            default_sizes_tbl_t *tbl = (default_sizes_tbl_t*)p_value;
            rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                                CAMERA_GET_PARM_DEF_THUMB_SIZES,
                                                sizeof(struct camera_size_type)*tbl->tbl_size,
                                                (void *)(tbl->sizes_tbl));
        }
        break;
    case MM_CAMERA_PARM_DEF_HFR_SIZES:
        {
            default_sizes_tbl_t *tbl = (default_sizes_tbl_t*)p_value;
            rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                                CAMERA_GET_PARM_DEF_HFR_SIZES,
                                                sizeof(struct camera_size_type)*tbl->tbl_size,
                                                (void *)(tbl->sizes_tbl));
        }
        break;
    case MM_CAMERA_PARM_SNAPSHOT_BURST_NUM:
        *((int *)p_value) = my_obj->snap_burst_num_by_user;
        break;
    case MM_CAMERA_PARM_VFE_OUTPUT_ENABLE:
        *((int *)p_value) = my_obj->properties.vfe_output_enable;
        break;
    case MM_CAMERA_PARM_DIMENSION:
        memcpy(p_value, &my_obj->dim, sizeof(my_obj->dim));
        CDBG("%s: dw=%d,dh=%d,vw=%d,vh=%d,pw=%d,ph=%d,tw=%d,th=%d,ovx=%x,ovy=%d,opx=%d,opy=%d, m_fmt=%d, t_ftm=%d\n",
                 __func__,
                 my_obj->dim.display_width,my_obj->dim.display_height,
                 my_obj->dim.video_width,my_obj->dim.video_height,
                 my_obj->dim.picture_width,my_obj->dim.picture_height,
                 my_obj->dim.ui_thumbnail_width,my_obj->dim.ui_thumbnail_height,
                 my_obj->dim.orig_video_width,my_obj->dim.orig_video_height,
                 my_obj->dim.orig_picture_width,my_obj->dim.orig_picture_height,
                 my_obj->dim.main_img_format, my_obj->dim.thumb_format);
        break;
    case MM_CAMERA_PARM_OP_MODE:
        *((mm_camera_op_mode_type_t *)p_value) = my_obj->op_mode;
        break;
    case MM_CAMERA_PARM_MAX_NUM_FACES_DECT:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_GET_MAX_NUM_FACES_DECT,
                                            sizeof(int),
                                            p_value);
        break;
    case MM_CAMERA_PARM_HDR:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_GET_PARM_HDR,
                                            sizeof(exp_bracketing_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_FPS_RANGE:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_GET_PARM_FPS_RANGE,
                                            sizeof(cam_sensor_fps_range_t),
                                            p_value);
        break;

    case MM_CAMERA_PARM_BESTSHOT_RECONFIGURE:
        *((int *)p_value) = my_obj->properties.bestshot_reconfigure;
        break;

    case MM_CAMERA_PARM_MOBICAT:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_GET_PARM_MOBICAT,
                                            sizeof(cam_exif_tags_t),
                                            p_value);
        break;
    default:
        /* needs to add more implementation */
        rc = -1;
        break;
    }

    pthread_mutex_unlock(&my_obj->cam_lock);
    return rc;
}

uint32_t mm_camera_add_channel(mm_camera_obj_t *my_obj)
{
    mm_channel_t *ch_obj = NULL;
    uint8_t ch_idx = 0;
    uint32_t ch_hdl = 0;

    for(ch_idx = 0; ch_idx < MM_CAMERA_CHANNEL_MAX; ch_idx++) {
        if (MM_CHANNEL_STATE_NOTUSED == my_obj->ch[ch_idx].state) {
            ch_obj = &my_obj->ch[ch_idx];
            break;
        }
    }

    if (NULL != ch_obj) {
        /* initialize channel obj */
        memset(ch_obj, 0, sizeof(mm_channel_t));
        ch_hdl = mm_camera_util_generate_handler(ch_idx);
        ch_obj->my_hdl = ch_hdl;
        ch_obj->state = MM_CHANNEL_STATE_STOPPED;
        ch_obj->cam_obj = my_obj;
        pthread_mutex_init(&ch_obj->ch_lock, NULL);
    }

    mm_channel_init(ch_obj);
    pthread_mutex_unlock(&my_obj->cam_lock);

    return ch_hdl;
}

void mm_camera_del_channel(mm_camera_obj_t *my_obj,
                           uint32_t ch_id)
{
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_DELETE,
                               NULL,
                               NULL);

        pthread_mutex_destroy(&ch_obj->ch_lock);
        memset(ch_obj, 0, sizeof(mm_channel_t));
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
    }
}

uint32_t mm_camera_add_stream(mm_camera_obj_t *my_obj,
                              uint32_t ch_id,
                              mm_camera_buf_notify_t buf_cb, void *user_data,
                              uint32_t ext_image_mode, uint32_t sensor_idx)
{
    uint32_t s_hdl = 0;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);
    mm_evt_paylod_add_stream_t payload;

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        memset(&payload, 0, sizeof(mm_evt_paylod_add_stream_t));
        payload.buf_cb = buf_cb;
        payload.user_data = user_data;
        payload.ext_image_mode = ext_image_mode;
        payload.sensor_idx = sensor_idx;
        mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_ADD_STREAM,
                               (void*)&payload,
                               (void*)&s_hdl);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
    }

    return s_hdl;
}

int32_t mm_camera_del_stream(mm_camera_obj_t *my_obj,
                             uint32_t ch_id,
                             uint32_t stream_id)
{
    int32_t rc = -1;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_DEL_STREAM,
                               (void*)&stream_id,
                               NULL);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
    }

    return rc;
}

int32_t mm_camera_config_stream(mm_camera_obj_t *my_obj,
                                uint32_t ch_id,
                                uint32_t stream_id,
                                mm_camera_stream_config_t *config)
{
    int32_t rc = -1;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);
    mm_evt_paylod_config_stream_t payload;

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        memset(&payload, 0, sizeof(mm_evt_paylod_config_stream_t));
        payload.stream_id = stream_id;
        payload.config = config;
        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_CONFIG_STREAM,
                               (void*)&payload,
                               NULL);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
    }

    return rc;
}

int32_t mm_camera_bundle_streams(mm_camera_obj_t *my_obj,
                                 uint32_t ch_id,
                                 mm_camera_buf_notify_t super_frame_notify_cb,
                                 void *user_data,
                                 mm_camera_bundle_attr_t *attr,
                                 uint8_t num_streams,
                                 uint32_t *stream_ids)
{
    int32_t rc = -1;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);
    mm_evt_payload_bundle_stream_t payload;

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        memset(&payload, 0, sizeof(mm_evt_payload_bundle_stream_t));
        payload.super_frame_notify_cb = super_frame_notify_cb;
        payload.user_data = user_data;
        payload.attr = attr;
        payload.num_streams = num_streams;
        payload.stream_ids = stream_ids;
        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_INIT_BUNDLE,
                               (void*)&payload,
                               NULL);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
    }

    return rc;
}

int32_t mm_camera_destroy_bundle(mm_camera_obj_t *my_obj, uint32_t ch_id)
{
    int32_t rc = -1;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_DESTROY_BUNDLE,
                               NULL,
                               NULL);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
    }

    return rc;
}

int32_t mm_camera_start_streams(mm_camera_obj_t *my_obj,
                                uint32_t ch_id,
                                uint8_t num_streams,
                                uint32_t *stream_ids)
{
    int32_t rc = -1;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);
    mm_evt_payload_start_stream_t payload;

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        memset(&payload, 0, sizeof(mm_evt_payload_start_stream_t));
        payload.num_streams = num_streams;
        payload.stream_ids = stream_ids;
        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_START_STREAM,
                               (void*)&payload,
                               NULL);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
    }

    return rc;
}

int32_t mm_camera_stop_streams(mm_camera_obj_t *my_obj,
                               uint32_t ch_id,
                               uint8_t num_streams,
                               uint32_t *stream_ids)
{
    int32_t rc = 0;
    mm_evt_payload_stop_stream_t payload;
    mm_camera_cmdcb_t * node = NULL;

    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        memset(&payload, 0, sizeof(mm_evt_payload_stop_stream_t));
        payload.num_streams = num_streams;
        payload.stream_ids = stream_ids;

        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_STOP_STREAM,
                               (void*)&payload,
                               NULL);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
    }
    return rc;
}

int32_t mm_camera_async_teardown_streams(mm_camera_obj_t *my_obj,
                                          uint32_t ch_id,
                                          uint8_t num_streams,
                                          uint32_t *stream_ids)
{
    int32_t rc = 0;
    mm_evt_payload_stop_stream_t payload;
    mm_camera_cmdcb_t * node = NULL;

    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        /* enqueu asyn stop cmd to async_cmd_thread */
        node = (mm_camera_cmdcb_t *)malloc(sizeof(mm_camera_cmdcb_t));
        if (NULL != node) {
            memset(node, 0, sizeof(mm_camera_cmdcb_t));
            node->cmd_type = MM_CAMERA_CMD_TYPE_ASYNC_CB;
            node->u.async.cmd_type = MM_CAMERA_ASYNC_CMD_TYPE_STOP;
            node->u.async.u.stop_cmd.ch_obj = ch_obj;
            node->u.async.u.stop_cmd.num_streams = num_streams;
            memcpy(node->u.async.u.stop_cmd.stream_ids, stream_ids, sizeof(uint32_t)*num_streams);

            /* enqueue to async cmd thread */
            mm_camera_queue_enq(&(my_obj->async_cmd_thread.cmd_queue), node);
            /* wake up async cmd thread */
            sem_post(&(my_obj->async_cmd_thread.cmd_sem));
        } else {
            CDBG_ERROR("%s: No memory for mm_camera_cmdcb_t", __func__);
            pthread_mutex_unlock(&ch_obj->ch_lock);
            rc = -1;
            return rc;
        }
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
    }
    return rc;
}

int32_t mm_camera_request_super_buf(mm_camera_obj_t *my_obj,
                                    uint32_t ch_id,
                                    uint32_t num_buf_requested)
{
    int32_t rc = -1;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_REQUEST_SUPER_BUF,
                               (void*)num_buf_requested,
                               NULL);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
    }

    return rc;
}

int32_t mm_camera_cancel_super_buf_request(mm_camera_obj_t *my_obj, uint32_t ch_id)
{
    int32_t rc = -1;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_CANCEL_REQUEST_SUPER_BUF,
                               NULL,
                               NULL);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
    }

    return rc;
}

int32_t mm_camera_start_focus(mm_camera_obj_t *my_obj,
                              uint32_t ch_id,
                              uint32_t sensor_idx,
                              uint32_t focus_mode)
{
    int32_t rc = -1;
    mm_evt_payload_start_focus_t payload;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        memset(&payload, 0, sizeof(mm_evt_payload_start_focus_t));
        payload.sensor_idx = sensor_idx;
        payload.focus_mode = focus_mode;
        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_START_FOCUS,
                               (void *)&payload,
                               NULL);
        if (0 != rc) {
            mm_camera_event_t event;
            event.event_type = MM_CAMERA_EVT_TYPE_CTRL;
            event.e.ctrl.evt = MM_CAMERA_CTRL_EVT_AUTO_FOCUS_DONE;
            event.e.ctrl.status = CAM_CTRL_FAILED;
            rc = mm_camera_enqueue_evt(my_obj, &event);
        }
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
    }

    return rc;
}

int32_t mm_camera_abort_focus(mm_camera_obj_t *my_obj,
                              uint32_t ch_id,
                              uint32_t sensor_idx)
{
    int32_t rc = -1;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_ABORT_FOCUS,
                               (void*)sensor_idx,
                               NULL);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
    }

    return rc;
}

int32_t mm_camera_prepare_snapshot(mm_camera_obj_t *my_obj,
                                   uint32_t ch_id,
                                   uint32_t sensor_idx)
{
    int32_t rc = -1;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_PREPARE_SNAPSHOT,
                               (void *)sensor_idx,
                               NULL);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
    }

    return rc;
}

int32_t mm_camera_set_stream_parm(mm_camera_obj_t *my_obj,
                                  uint32_t ch_id,
                                  uint32_t s_id,
                                  mm_camera_stream_parm_t parm_type,
                                  void* p_value)
{
    int32_t rc = -1;
    mm_evt_paylod_stream_parm_t payload;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        memset(&payload,0,sizeof(mm_evt_paylod_stream_parm_t));
        payload.parm_type = parm_type;
        payload.value = p_value;
        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_SET_STREAM_PARM,
                               (void *)s_id,
                               &payload);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
    }

    return rc;
}

int32_t mm_camera_get_stream_parm(mm_camera_obj_t *my_obj,
                                  uint32_t ch_id,
                                  uint32_t s_id,
                                  mm_camera_stream_parm_t parm_type,
                                  void* p_value)
{
    int32_t rc = -1;
    mm_evt_paylod_stream_parm_t payload;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        memset(&payload,0,sizeof(mm_evt_paylod_stream_parm_t));
        payload.parm_type = parm_type;
        payload.value = p_value;
        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_GET_STREAM_PARM,
                               (void *)s_id,
                               &payload);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
    }

    return rc;
}

int32_t mm_camera_send_private_ioctl(mm_camera_obj_t *my_obj,
                                     uint32_t cmd_id,
                                     uint32_t cmd_length,
                                     void *cmd)
{
    int32_t rc = -1;

    struct msm_camera_v4l2_ioctl_t v4l2_ioctl;

    CDBG("%s: cmd = %p, length = %d",
               __func__, cmd, cmd_length);
    memset(&v4l2_ioctl, 0, sizeof(v4l2_ioctl));
    v4l2_ioctl.id = cmd_id;
    v4l2_ioctl.len = cmd_length;
    v4l2_ioctl.ioctl_ptr = cmd;
    rc = ioctl (my_obj->ctrl_fd, MSM_CAM_V4L2_IOCTL_PRIVATE_GENERAL, &v4l2_ioctl);

    if(rc < 0) {
        CDBG_ERROR("%s: cmd = %p, id = %d, length = %d, rc = %d\n",
                   __func__, cmd, cmd_id, cmd_length, rc);
    } else {
        rc = 0;
    }

    return rc;
}

int32_t mm_camera_ctrl_set_specialEffect (mm_camera_obj_t *my_obj, int32_t effect) {
    struct v4l2_control ctrl;
    if (effect == CAMERA_EFFECT_MAX)
        effect = CAMERA_EFFECT_OFF;
    int rc = 0;

    ctrl.id = MSM_V4L2_PID_EFFECT;
    ctrl.value = effect;
    rc = ioctl(my_obj->ctrl_fd, VIDIOC_S_CTRL, &ctrl);
    return (rc >= 0)? 0: -1;;
}

int32_t mm_camera_ctrl_set_auto_focus (mm_camera_obj_t *my_obj, int32_t value)
{
    int32_t rc = 0;
    struct v4l2_queryctrl queryctrl;

    memset (&queryctrl, 0, sizeof (queryctrl));
    queryctrl.id = V4L2_CID_FOCUS_AUTO;

    if(value != 0 && value != 1) {
        CDBG("%s:boolean required, invalid value = %d\n",__func__, value);
        return -1;
    }
    if (-1 == ioctl (my_obj->ctrl_fd, VIDIOC_QUERYCTRL, &queryctrl)) {
        CDBG ("V4L2_CID_FOCUS_AUTO is not supported\n");
    } else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
        CDBG ("%s:V4L2_CID_FOCUS_AUTO is not supported\n", __func__);
    } else {
        if(0 != (rc =  mm_camera_util_s_ctrl(my_obj->ctrl_fd,
                V4L2_CID_FOCUS_AUTO, value))){
            CDBG("%s: error, id=0x%x, value=%d, rc = %d\n",
                     __func__, V4L2_CID_FOCUS_AUTO, value, rc);
            rc = -1;
        }
    }
    return rc;
}

int32_t mm_camera_ctrl_set_whitebalance (mm_camera_obj_t *my_obj, int32_t mode) {

    int32_t rc = 0, auto_wb, temperature;
    uint32_t id_auto_wb, id_temperature;

    id_auto_wb = V4L2_CID_AUTO_WHITE_BALANCE;
    id_temperature = V4L2_CID_WHITE_BALANCE_TEMPERATURE;

    switch(mode) {
    case CAMERA_WB_DAYLIGHT:
        auto_wb = 0;
        temperature = 6500;
        break;
    case CAMERA_WB_INCANDESCENT:
        auto_wb = 0;
        temperature = 2800;
        break;
    case CAMERA_WB_FLUORESCENT:
        auto_wb = 0;
        temperature = 4200;
        break;
    case CAMERA_WB_CLOUDY_DAYLIGHT:
        auto_wb = 0;
        temperature = 7500;
        break;
    case CAMERA_WB_AUTO:
    default:
        auto_wb = 1; /* TRUE */
        temperature = 0;
        break;
    }

    rc =  mm_camera_util_s_ctrl(my_obj->ctrl_fd, id_auto_wb, auto_wb);
    if(0 != rc){
        CDBG_ERROR("%s: error, V4L2_CID_AUTO_WHITE_BALANCE value = %d, rc = %d\n",
            __func__, auto_wb, rc);
        return rc;
    }
    if (!auto_wb) {
        rc = mm_camera_util_s_ctrl(my_obj->ctrl_fd, id_temperature, temperature);
        if (0 != rc) {
            CDBG_ERROR("%s: error, V4L2_CID_WHITE_BALANCE_TEMPERATURE value = %d, rc = %d\n",
                __func__, temperature, rc);
            return rc;
        }
    }
    return rc;
}

int32_t mm_camera_set_general_parm(mm_camera_obj_t * my_obj,
                                   mm_camera_parm_type_t parm_type,
                                   void* p_value)
{
    int rc = -1;
    int isZSL =0;

    switch(parm_type)  {
    case MM_CAMERA_PARM_EXPOSURE:
        rc = mm_camera_util_s_ctrl(my_obj->ctrl_fd,
                                   MSM_V4L2_PID_EXP_METERING,
                                   *((int32_t *)p_value));
        break;
    case MM_CAMERA_PARM_SHARPNESS:
        rc = mm_camera_util_s_ctrl(my_obj->ctrl_fd,
                                   V4L2_CID_SHARPNESS,
                                   *((int32_t *)p_value));
        break;
    case MM_CAMERA_PARM_CONTRAST:
        rc = mm_camera_util_s_ctrl(my_obj->ctrl_fd,
                                   V4L2_CID_CONTRAST,
                                   *((int32_t *)p_value));
        break;
    case MM_CAMERA_PARM_SATURATION:
        rc = mm_camera_util_s_ctrl(my_obj->ctrl_fd,
                                   V4L2_CID_SATURATION,
                                   *((int32_t *)p_value));
        break;
    case MM_CAMERA_PARM_BRIGHTNESS:
        rc = mm_camera_util_s_ctrl(my_obj->ctrl_fd,
                                   V4L2_CID_BRIGHTNESS,
                                   *((int32_t *)p_value));
        break;
    case MM_CAMERA_PARM_WHITE_BALANCE:
        rc = mm_camera_ctrl_set_whitebalance (my_obj, *((int32_t *)p_value));
        break;
    case MM_CAMERA_PARM_ISO:
        rc = mm_camera_util_s_ctrl(my_obj->ctrl_fd,
                                   MSM_V4L2_PID_ISO,
                                   *((int32_t *)p_value));
        break;
    case MM_CAMERA_PARM_ZOOM:
        rc = mm_camera_util_s_ctrl(my_obj->ctrl_fd,
                                   V4L2_CID_ZOOM_ABSOLUTE,
                                   *((int32_t *)p_value));
        break;
    case MM_CAMERA_PARM_LUMA_ADAPTATION:
        rc = mm_camera_util_s_ctrl(my_obj->ctrl_fd,
                                   MSM_V4L2_PID_LUMA_ADAPTATION,
                                   *((int32_t *)p_value));
        break;
    case MM_CAMERA_PARM_ANTIBANDING:
        rc = mm_camera_util_s_ctrl(my_obj->ctrl_fd,
                                   V4L2_CID_POWER_LINE_FREQUENCY,
                                   *((int32_t *)p_value));
        break;
    case MM_CAMERA_PARM_CONTINUOUS_AF:
        rc = mm_camera_ctrl_set_auto_focus(my_obj,
                                           *((int32_t *)p_value));
        break;
    case MM_CAMERA_PARM_HJR:
        rc = mm_camera_util_s_ctrl(my_obj->ctrl_fd,
                                   MSM_V4L2_PID_HJR,
                                   *((int32_t *)p_value));
        break;
    case MM_CAMERA_PARM_EFFECT:
        rc = mm_camera_ctrl_set_specialEffect (my_obj,
                                               *((int32_t *)p_value));
        break;
    case MM_CAMERA_PARM_FPS:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_FPS,
                                            sizeof(uint32_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_FPS_MODE:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_FPS_MODE,
                                            sizeof(int32_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_EXPOSURE_COMPENSATION:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_EXPOSURE_COMPENSATION,
                                            sizeof(int32_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_LED_MODE:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_LED_MODE,
                                            sizeof(int32_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_ROLLOFF:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_ROLLOFF,
                                            sizeof(int32_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_MODE:
        my_obj->current_mode = *((camera_mode_t *)p_value);
        break;
    case MM_CAMERA_PARM_FOCUS_RECT:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_FOCUS_RECT,
                                            sizeof(int32_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_AEC_ROI:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_AEC_ROI,
                                            sizeof(cam_set_aec_roi_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_AF_ROI:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_AF_ROI,
                                            sizeof(roi_info_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_FOCUS_MODE:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_AF_MODE,
                                            sizeof(int32_t),
                                            p_value);
        break;
#if 0 /* to be enabled later: @punits */
    case MM_CAMERA_PARM_AF_MTR_AREA:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_AF_MTR_AREA,
                                            sizeof(af_mtr_area_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_AEC_MTR_AREA:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_AEC_MTR_AREA,
                                            sizeof(aec_mtr_area_t),
                                            p_value);
        break;
#endif
    case MM_CAMERA_PARM_CAF_ENABLE:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_CAF,
                                            sizeof(uint32_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_BESTSHOT_MODE:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_BESTSHOT_MODE,
                                            sizeof(int32_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_VIDEO_DIS:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_VIDEO_DIS_PARAMS,
                                            sizeof(video_dis_param_ctrl_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_VIDEO_ROT:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_VIDEO_ROT_PARAMS,
                                            sizeof(video_rotation_param_ctrl_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_SCE_FACTOR:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_SCE_FACTOR,
                                            sizeof(int32_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_FD:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_FD,
                                            sizeof(fd_set_parm_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_AEC_LOCK:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_AEC_LOCK,
                                            sizeof(int32_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_AWB_LOCK:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_AWB_LOCK,
                                            sizeof(int32_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_MCE:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_MCE,
                                            sizeof(int32_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_HORIZONTAL_VIEW_ANGLE:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_GET_PARM_HORIZONTAL_VIEW_ANGLE,
                                            sizeof(focus_distances_info_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_VERTICAL_VIEW_ANGLE:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_GET_PARM_VERTICAL_VIEW_ANGLE,
                                            sizeof(focus_distances_info_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_RESET_LENS_TO_INFINITY:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_RESET_LENS_TO_INFINITY,
                                            0, NULL);
        break;
    case MM_CAMERA_PARM_SNAPSHOTDATA:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_GET_PARM_SNAPSHOTDATA,
                                            sizeof(snapshotData_info_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_HFR:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_HFR,
                                            sizeof(int32_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_REDEYE_REDUCTION:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_REDEYE_REDUCTION,
                                            sizeof(int32_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_WAVELET_DENOISE:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_WAVELET_DENOISE,
                                            sizeof(denoise_param_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_3D_DISPLAY_DISTANCE:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_3D_DISPLAY_DISTANCE,
                                            sizeof(float),
                                            p_value);
        break;
    case MM_CAMERA_PARM_3D_VIEW_ANGLE:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_3D_VIEW_ANGLE,
                                            sizeof(uint32_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_ZOOM_RATIO:
        break;
    case MM_CAMERA_PARM_HISTOGRAM:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_HISTOGRAM,
                                            sizeof(int8_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_ASD_ENABLE:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                          CAMERA_SET_ASD_ENABLE,
                                          sizeof(uint32_t),
                                          p_value);
        break;
    case MM_CAMERA_PARM_RECORDING_HINT:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_RECORDING_HINT,
                                            sizeof(uint32_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_PREVIEW_FORMAT:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_PREVIEW_FORMAT,
                                            sizeof(uint32_t),
                                            p_value);
        break;
    /* TODO: need code review to determine any of the three is redundent
     * MM_CAMERA_PARM_DIS_ENABLE,
     * MM_CAMERA_PARM_FULL_LIVESHOT,
     * MM_CAMERA_PARM_LOW_POWER_MODE*/
    case MM_CAMERA_PARM_DIS_ENABLE:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_DIS_ENABLE,
                                            sizeof(uint32_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_FULL_LIVESHOT:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_FULL_LIVESHOT,
                                            sizeof(uint32_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_LOW_POWER_MODE:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_LOW_POWER_MODE,
                                            sizeof(uint32_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_HDR:
        rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_SET_PARM_HDR,
                                            sizeof(exp_bracketing_t),
                                            p_value);
        break;
    case MM_CAMERA_PARM_MOBICAT:
	    rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                            CAMERA_ENABLE_MOBICAT,
                                            sizeof(mm_cam_mobicat_info_t),
                                            p_value);
    default:
        CDBG("%s: default: parm %d not supported\n", __func__, parm_type);
        break;
    }
    return rc;
}

int32_t mm_camera_util_private_s_ctrl(int32_t fd, uint32_t id, void* value)
{
    int rc = -1;
    struct msm_camera_v4l2_ioctl_t v4l2_ioctl;

    memset(&v4l2_ioctl, 0, sizeof(v4l2_ioctl));
    v4l2_ioctl.id = id;
    v4l2_ioctl.ioctl_ptr = value;
    rc = ioctl (fd, MSM_CAM_V4L2_IOCTL_PRIVATE_S_CTRL, &v4l2_ioctl);

    if(rc < 0) {
        CDBG_ERROR("%s: fd=%d, S_CTRL, id=0x%x, value = 0x%x, rc = %d\n",
                   __func__, fd, id, (uint32_t)value, rc);
        rc = -1;
    } else {
        rc = 0;
    }
    return rc;
}

int32_t mm_camera_send_native_ctrl_cmd(mm_camera_obj_t * my_obj,
                                              cam_ctrl_type type,
                                              uint32_t length,
                                              void *value)
{
    return mm_camera_send_native_ctrl_timeout_cmd(my_obj, type,
                                                  length, value,
                                                  1000);
}

int32_t mm_camera_send_native_ctrl_timeout_cmd(mm_camera_obj_t * my_obj,
                                                      cam_ctrl_type type,
                                                      uint32_t length,
                                                      void *value,
                                                      int timeout)
{
    int rc = -1;
    struct msm_ctrl_cmd ctrl_cmd;

    memset(&ctrl_cmd, 0, sizeof(ctrl_cmd));
    ctrl_cmd.type = type;
    ctrl_cmd.length = (uint16_t)length;
    ctrl_cmd.timeout_ms = timeout;
    ctrl_cmd.value = value;
    ctrl_cmd.status = (uint16_t)CAM_CTRL_SUCCESS;
    rc = mm_camera_util_private_s_ctrl(my_obj->ctrl_fd,
                               MSM_V4L2_PID_CTRL_CMD,
                               (void*)&ctrl_cmd);
    CDBG("%s: type=%d, rc = %d, status = %d\n",
        __func__, type, rc, ctrl_cmd.status);
    if(rc != 0 || ((ctrl_cmd.status != CAM_CTRL_ACCEPTED) &&
        (ctrl_cmd.status != CAM_CTRL_SUCCESS) &&
        (ctrl_cmd.status != CAM_CTRL_INVALID_PARM)))
        rc = -1;
    return rc;
}

int mm_camera_evt_sub(mm_camera_obj_t * my_obj,
                      mm_camera_event_type_t evt_type,
                      int reg_count)
{
    int rc = 0;
    struct v4l2_event_subscription sub;

    memset(&sub, 0, sizeof(sub));
    sub.type = V4L2_EVENT_PRIVATE_START+MSM_CAM_APP_NOTIFY_EVENT;
    if(reg_count == 0) {
        /* unsubscribe */
        if(my_obj->evt_type_mask == (uint32_t)(1 << evt_type)) {
            rc = ioctl(my_obj->ctrl_fd, VIDIOC_UNSUBSCRIBE_EVENT, &sub);
            CDBG("%s: unsubscribe event 0x%x, rc = %d", __func__, sub.type, rc);
            sub.type = V4L2_EVENT_PRIVATE_START+MSM_CAM_APP_NOTIFY_ERROR_EVENT;
            rc = ioctl(my_obj->ctrl_fd, VIDIOC_UNSUBSCRIBE_EVENT, &sub);
            CDBG("%s: unsubscribe event 0x%x, rc = %d", __func__, sub.type, rc);
        }
        my_obj->evt_type_mask &= ~(1 << evt_type);
        if(my_obj->evt_type_mask == 0) {
            /* remove evt fd from the polling thraed when unreg the last event */
            mm_camera_poll_thread_del_poll_fd(&my_obj->evt_poll_thread, my_obj->my_hdl);
        }
    } else {
        if(!my_obj->evt_type_mask) {
            /* this is the first reg event */
            rc = ioctl(my_obj->ctrl_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
            CDBG("%s: subscribe event 0x%x, rc = %d", __func__, sub.type, rc);
            if (rc < 0)
                goto end;
            sub.type = V4L2_EVENT_PRIVATE_START+MSM_CAM_APP_NOTIFY_ERROR_EVENT;
            rc = ioctl(my_obj->ctrl_fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
            CDBG("%s: subscribe event 0x%x, rc = %d", __func__, sub.type, rc);
            if (rc < 0)
                goto end;
        }
        my_obj->evt_type_mask |= (1 << evt_type);
        if(my_obj->evt_type_mask == (uint32_t)(1 << evt_type)) {
            /* add evt fd to polling thread when subscribe the first event */
            rc = mm_camera_poll_thread_add_poll_fd(&my_obj->evt_poll_thread,
                                                   my_obj->my_hdl,
                                                   my_obj->ctrl_fd,
                                                   mm_camera_event_notify,
                                                   (void*)my_obj);
        }
    }
end:
    return rc;
}

int32_t mm_camera_util_sendmsg(mm_camera_obj_t *my_obj, void *msg, uint32_t buf_size, int sendfd)
{
    return mm_camera_socket_sendmsg(my_obj->ds_fd, msg, buf_size, sendfd);
}

int32_t mm_camera_map_buf(mm_camera_obj_t *my_obj,
                          int ext_mode,
                          int idx,
                          int fd,
                          uint32_t size)
{
    cam_sock_packet_t packet;
    memset(&packet, 0, sizeof(cam_sock_packet_t));
    packet.msg_type = CAM_SOCK_MSG_TYPE_FD_MAPPING;
    packet.payload.frame_fd_map.ext_mode = ext_mode;
    packet.payload.frame_fd_map.frame_idx = idx;
    packet.payload.frame_fd_map.fd = fd;
    packet.payload.frame_fd_map.size = size;

    return mm_camera_util_sendmsg(my_obj, &packet,
                                  sizeof(cam_sock_packet_t),
                                  packet.payload.frame_fd_map.fd);
}

int32_t mm_camera_unmap_buf(mm_camera_obj_t *my_obj,
                            int ext_mode,
                            int idx)
{
    cam_sock_packet_t packet;
    memset(&packet, 0, sizeof(cam_sock_packet_t));
    packet.msg_type = CAM_SOCK_MSG_TYPE_FD_UNMAPPING;
    packet.payload.frame_fd_unmap.ext_mode = ext_mode;
    packet.payload.frame_fd_unmap.frame_idx = idx;
    return mm_camera_util_sendmsg(my_obj, &packet,
                                  sizeof(cam_sock_packet_t),
                                  packet.payload.frame_fd_map.fd);
}

int32_t mm_camera_util_s_ctrl(int32_t fd,  uint32_t id, int32_t value)
{
    int rc = 0;
    struct v4l2_control control;

    memset(&control, 0, sizeof(control));
    control.id = id;
    control.value = value;
    rc = ioctl (fd, VIDIOC_S_CTRL, &control);

    CDBG("%s: fd=%d, S_CTRL, id=0x%x, value = 0x%x, rc = %d\n",
         __func__, fd, id, (uint32_t)value, rc);
    return (rc >= 0)? 0 : -1;
}

int32_t mm_camera_util_g_ctrl( int32_t fd, uint32_t id, int32_t *value)
{
    int rc = 0;
    struct v4l2_control control;

    memset(&control, 0, sizeof(control));
    control.id = id;
    control.value = (int32_t)value;
    rc = ioctl (fd, VIDIOC_G_CTRL, &control);
    *value = control.value;
    CDBG("%s: fd=%d, G_CTRL, id=0x%x, rc = %d\n", __func__, fd, id, rc);
    return (rc >= 0)? 0 : -1;
}

uint8_t mm_camera_util_get_pp_mask(mm_camera_obj_t *my_obj)
{
    uint8_t pp_mask = 0;
    int32_t rc = 0;

    /* query pp mask from mctl */
    rc = mm_camera_send_native_ctrl_cmd(my_obj,
                                        CAMERA_GET_PP_MASK,
                                        sizeof(uint8_t),
                                        (void *)&pp_mask);
    if (0 != rc) {
        CDBG_ERROR("%s: error getting post processing mask (rc=%d)",
                   __func__, rc);
    }

    return pp_mask;
}

int32_t mm_camera_open_repro_isp(mm_camera_obj_t *my_obj,
                                 uint32_t ch_id,
                                 mm_camera_repro_isp_type_t repro_isp_type,
                                 uint32_t *repro_isp_handle)
{
    int32_t rc = -1;
    uint32_t repro_hdl = 0;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_OPEN_REPRO_ISP,
                               (void*)repro_isp_type,
                               (void*)&repro_hdl);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
        CDBG_ERROR("%s: no channel obj exist", __func__);
    }

    if (NULL != repro_isp_handle) {
        *repro_isp_handle = repro_hdl;
    }
    return rc;
}

int32_t mm_camera_config_repro_isp(mm_camera_obj_t *my_obj,
                                   uint32_t ch_id,
                                   uint32_t repro_isp_handle,
                                   mm_camera_repro_isp_config_t *config)
{
    int32_t rc = -1;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);
    mm_evt_paylod_config_repro_isp_t payload;

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        memset(&payload, 0, sizeof(mm_evt_paylod_config_repro_isp_t));
        payload.repro_isp_handle = repro_isp_handle;
        payload.config = config;
        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_CONFIG_REPRO_ISP,
                               (void*)&payload,
                               NULL);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
        CDBG_ERROR("%s: no channel obj exist", __func__);
    }

    return rc;
}

int32_t mm_camera_attach_stream_to_repro_isp(mm_camera_obj_t *my_obj,
                                             uint32_t ch_id,
                                             uint32_t repro_isp_handle,
                                             uint32_t stream_id)
{
    int32_t rc = -1;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);
    mm_evt_paylod_stream_to_repro_isp_t payload;

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        memset(&payload, 0, sizeof(mm_evt_paylod_stream_to_repro_isp_t));
        payload.repro_isp_handle = repro_isp_handle;
        payload.stream_id = stream_id;
        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_ATTACH_STREAM_TO_REPRO_ISP,
                               (void*)&payload,
                               NULL);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
        CDBG_ERROR("%s: no channel obj exist", __func__);
    }

    return rc;
}

int32_t mm_camera_start_repro_isp(mm_camera_obj_t *my_obj,
                                  uint32_t ch_id,
                                  uint32_t repro_isp_handle,
                                  uint32_t stream_id)
{
    int32_t rc = -1;
    mm_evt_paylod_repro_start_stop_t payload;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        memset(&payload, 0, sizeof(mm_evt_paylod_repro_start_stop_t));
        payload.repro_isp_handle = repro_isp_handle;
        payload.stream_id = stream_id;
        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_START_REPRO_ISP,
                               (void*)&payload,
                               NULL);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
        CDBG_ERROR("%s: no channel obj exist", __func__);
    }

    return rc;
}

int32_t mm_camera_reprocess(mm_camera_obj_t *my_obj,
                            uint32_t ch_id,
                            uint32_t repro_isp_handle,
                            mm_camera_repro_data_t *repro_data)
{
    int32_t rc = -1;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);
    mm_evt_paylod_reprocess_t payload;

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        memset(&payload, 0, sizeof(mm_evt_paylod_reprocess_t));
        payload.repro_isp_handle = repro_isp_handle;
        payload.repro_data = repro_data;
        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_ATTACH_STREAM_TO_REPRO_ISP,
                               (void*)&payload,
                               NULL);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
        CDBG_ERROR("%s: no channel obj exist", __func__);
    }

    return rc;
}

int32_t mm_camera_stop_repro_isp(mm_camera_obj_t *my_obj,
                                 uint32_t ch_id,
                                 uint32_t repro_isp_handle,
                                 uint32_t stream_id)
{
    int32_t rc = -1;
    mm_evt_paylod_repro_start_stop_t payload;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        memset(&payload, 0, sizeof(mm_evt_paylod_repro_start_stop_t));
        payload.repro_isp_handle = repro_isp_handle;
        payload.stream_id = stream_id;
        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_STOP_REPRO_ISP,
                               (void*)&payload,
                               NULL);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
        CDBG_ERROR("%s: no channel obj exist", __func__);
    }

    return rc;
}

int32_t mm_camera_detach_stream_from_repro_isp(mm_camera_obj_t *my_obj,
                                               uint32_t ch_id,
                                               uint32_t repro_isp_handle,
                                               uint32_t stream_id)
{
    int32_t rc = -1;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);
    mm_evt_paylod_stream_to_repro_isp_t payload;

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        memset(&payload, 0, sizeof(mm_evt_paylod_stream_to_repro_isp_t));
        payload.repro_isp_handle = repro_isp_handle;
        payload.stream_id = stream_id;
        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_DETACH_STREAM_FROM_REPRO_ISP,
                               (void*)&payload,
                               NULL);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
        CDBG_ERROR("%s: no channel obj exist", __func__);
    }

    return rc;
}

int32_t mm_camera_close_repro_isp(mm_camera_obj_t *my_obj,
                                  uint32_t ch_id,
                                  uint32_t repro_isp_handle)
{
    int32_t rc = -1;
    mm_channel_t * ch_obj =
        mm_camera_util_get_channel_by_handler(my_obj, ch_id);

    if (NULL != ch_obj) {
        pthread_mutex_lock(&ch_obj->ch_lock);
        pthread_mutex_unlock(&my_obj->cam_lock);

        rc = mm_channel_fsm_fn(ch_obj,
                               MM_CHANNEL_EVT_CLOSE_REPRO_ISP,
                               (void*)repro_isp_handle,
                               NULL);
    } else {
        pthread_mutex_unlock(&my_obj->cam_lock);
        CDBG_ERROR("%s: no channel obj exist", __func__);
    }

    return rc;
}
