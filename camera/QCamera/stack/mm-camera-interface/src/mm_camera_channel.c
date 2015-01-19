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
#include "mm_camera_interface.h"
#include "mm_camera.h"

extern mm_camera_obj_t* mm_camera_util_get_camera_by_handler(uint32_t cam_handler);
extern mm_channel_t * mm_camera_util_get_channel_by_handler(
                                    mm_camera_obj_t * cam_obj,
                                    uint32_t handler);
extern int32_t mm_camera_send_native_ctrl_cmd(mm_camera_obj_t * my_obj,
                                       cam_ctrl_type type,
                                       uint32_t length,
                                       void *value);
extern int32_t mm_camera_send_native_ctrl_timeout_cmd(mm_camera_obj_t * my_obj,
                                               cam_ctrl_type type,
                                               uint32_t length,
                                               void *value,
                                               int timeout);

/* internal function declare goes here */
int32_t mm_channel_qbuf(mm_channel_t *my_obj,
                        mm_camera_buf_def_t *buf);
int32_t mm_channel_init(mm_channel_t *my_obj);
void mm_channel_release(mm_channel_t *my_obj);
uint32_t mm_channel_add_stream(mm_channel_t *my_obj,
                                   mm_camera_buf_notify_t buf_cb, void *user_data,
                                   uint32_t ext_image_mode, uint32_t sensor_idx);
int32_t mm_channel_del_stream(mm_channel_t *my_obj,
                                   uint32_t stream_id);
int32_t mm_channel_config_stream(mm_channel_t *my_obj,
                                 uint32_t stream_id,
                                 mm_camera_stream_config_t *config);
int32_t mm_channel_bundle_stream(mm_channel_t *my_obj,
                                 mm_camera_buf_notify_t super_frame_notify_cb,
                                 void *user_data,
                                 mm_camera_bundle_attr_t *attr,
                                 uint8_t num_streams,
                                 uint32_t *stream_ids);
int32_t mm_channel_destroy_bundle(mm_channel_t *my_obj);
int32_t mm_channel_start_streams(mm_channel_t *my_obj,
                                 uint8_t num_streams,
                                 uint32_t *stream_ids);
int32_t mm_channel_stop_streams(mm_channel_t *my_obj,
                                uint8_t num_streams,
                                uint32_t *stream_ids,
                                uint8_t tear_down_flag);
int32_t mm_channel_request_super_buf(mm_channel_t *my_obj, uint32_t num_buf_requested);
int32_t mm_channel_cancel_super_buf_request(mm_channel_t *my_obj);
int32_t mm_channel_start_focus(mm_channel_t *my_obj,
                               uint32_t sensor_idx,
                               uint32_t focus_mode);
int32_t mm_channel_abort_focus(mm_channel_t *my_obj,
                               uint32_t sensor_idx);
int32_t mm_channel_prepare_snapshot(mm_channel_t *my_obj,
                                    uint32_t sensor_idx);
int32_t mm_channel_set_stream_parm(mm_channel_t *my_obj,
                                   uint32_t s_id,
                                   void *value);
int32_t mm_channel_get_stream_parm(mm_channel_t *my_obj,
                                   uint32_t s_id,
                                   void *value);
int32_t mm_channel_do_post_processing(mm_channel_t *my_obj,
                                      mm_channel_queue_node_t *super_buf);
int32_t mm_channel_superbuf_bufdone_overflow(mm_channel_t *my_obj,
                                             mm_channel_queue_t *queue);
int32_t mm_channel_superbuf_skip(mm_channel_t *my_obj,
                                 mm_channel_queue_t *queue);
uint32_t mm_channel_open_repro_isp(mm_channel_t *my_obj,
                                   mm_camera_repro_isp_type_t isp_type);
int32_t mm_channel_close_repro_isp(mm_channel_t *my_obj,
                                   uint32_t repro_handle);
int32_t mm_channel_config_repro_isp(mm_channel_t *my_obj,
                                    uint32_t repro_handle,
                                    mm_camera_repro_isp_config_t *config);
int32_t mm_channel_repro_isp_dest_stream_ops(mm_channel_t *my_obj,
                                             uint32_t repro_handle,
                                             uint32_t stream_handle,
                                             uint8_t attach_flag);
int32_t mm_channel_repro_isp_ops(mm_channel_t *my_obj,
                                 uint32_t repro_handle,
                                 uint32_t stream_id,
                                 uint8_t start_flag);
int32_t mm_channel_reprocess(mm_channel_t *my_obj,
                             uint32_t repro_handle,
                             mm_camera_repro_data_t *repro_data);

/* state machine function declare */
int32_t mm_channel_fsm_fn_notused(mm_channel_t *my_obj,
                          mm_channel_evt_type_t evt,
                          void * in_val,
                          void * out_val);
int32_t mm_channel_fsm_fn_stopped(mm_channel_t *my_obj,
                          mm_channel_evt_type_t evt,
                          void * in_val,
                          void * out_val);
int32_t mm_channel_fsm_fn_active(mm_channel_t *my_obj,
                          mm_channel_evt_type_t evt,
                          void * in_val,
                          void * out_val);
int32_t mm_channel_fsm_fn_paused(mm_channel_t *my_obj,
                          mm_channel_evt_type_t evt,
                          void * in_val,
                          void * out_val);

/* channel super queue functions */
int32_t mm_channel_superbuf_queue_init(mm_channel_queue_t * queue);
int32_t mm_channel_superbuf_queue_deinit(mm_channel_t *my_obj,
                                         mm_channel_queue_t * queue);
int32_t mm_channel_superbuf_comp_and_enqueue(mm_channel_t *ch_obj,
                                             mm_channel_queue_t * queue,
                                             mm_camera_buf_info_t *buf);
mm_channel_queue_node_t* mm_channel_superbuf_dequeue(mm_channel_queue_t * queue);

/* channel utility function */
mm_stream_t * mm_channel_util_get_stream_by_handler(
                                    mm_channel_t * ch_obj,
                                    uint32_t handler)
{
    int i;
    mm_stream_t *s_obj = NULL;
    uint8_t ch_idx = mm_camera_util_get_index_by_handler(handler);

    for(i = 0; i < MM_CAMEAR_STRAEM_NUM_MAX; i++) {
        if (handler == ch_obj->streams[i].my_hdl) {
            s_obj = &ch_obj->streams[i];
            break;
        }
    }
    return s_obj;
}

static void mm_channel_dispatch_super_buf(mm_camera_cmdcb_t *cmd_cb,
                                          void* user_data)
{
    int i;
    mm_channel_t * my_obj = (mm_channel_t *)user_data;

    if (NULL == my_obj) {
        return;
    }

    if (MM_CAMERA_CMD_TYPE_SUPER_BUF_DATA_CB != cmd_cb->cmd_type) {
        CDBG_ERROR("%s: Wrong cmd_type (%d) for super buf dataCB",
                   __func__, cmd_cb->cmd_type);
        return;
    }

    if (my_obj->bundle.super_buf_notify_cb) {
        my_obj->bundle.super_buf_notify_cb(&cmd_cb->u.superbuf, my_obj->bundle.user_data);
    }
}

/* CB for processing stream buffer */
static void mm_channel_process_stream_buf(mm_camera_cmdcb_t * cmd_cb,
                                          void *user_data)
{
    mm_camera_super_buf_notify_mode_t notify_mode;
    mm_channel_queue_node_t *node = NULL;
    mm_channel_t *ch_obj = (mm_channel_t *)user_data;
    if (NULL == ch_obj) {
        return;
    }

    if (MM_CAMERA_CMD_TYPE_DATA_CB  == cmd_cb->cmd_type) {
        /* comp_and_enqueue */
        mm_channel_superbuf_comp_and_enqueue(
                        ch_obj,
                        &ch_obj->bundle.superbuf_queue,
                        &cmd_cb->u.buf);
    } else if (MM_CAMERA_CMD_TYPE_REQ_DATA_CB  == cmd_cb->cmd_type) {
        /* skip frames if needed */
        ch_obj->pending_cnt = cmd_cb->u.req_buf.num_buf_requested;
        mm_channel_superbuf_skip(ch_obj, &ch_obj->bundle.superbuf_queue);
    }

    notify_mode = ch_obj->bundle.superbuf_queue.attr.notify_mode;

    /* bufdone for overflowed bufs */
    mm_channel_superbuf_bufdone_overflow(ch_obj, &ch_obj->bundle.superbuf_queue);

    /* dispatch frame if pending_cnt>0 or is in continuous streaming mode */
    while ( (ch_obj->pending_cnt > 0) ||
            (MM_CAMERA_SUPER_BUF_NOTIFY_CONTINUOUS == notify_mode) ) {

        /* dequeue */
        node = mm_channel_superbuf_dequeue(&ch_obj->bundle.superbuf_queue);
        if (NULL != node) {
            /* decrease pending_cnt */
            CDBG("%s: Super Buffer received, Call client callback, pending_cnt=%d",
                 __func__, ch_obj->pending_cnt);
            if (MM_CAMERA_SUPER_BUF_NOTIFY_BURST == notify_mode) {
                ch_obj->pending_cnt--;
            }

            /* do post processing if needed */
            /* this is a blocking call */
            mm_channel_do_post_processing(ch_obj, node);

            /* dispatch superbuf */
            if (NULL != ch_obj->bundle.super_buf_notify_cb) {
                uint8_t i;
                mm_camera_cmdcb_t* cb_node = NULL;

                CDBG("%s: Send superbuf to HAL, pending_cnt=%d",
                     __func__, ch_obj->pending_cnt);

                /* send sem_post to wake up cb thread to dispatch super buffer */
                cb_node = (mm_camera_cmdcb_t *)malloc(sizeof(mm_camera_cmdcb_t));
                if (NULL != cb_node) {
                    memset(cb_node, 0, sizeof(mm_camera_cmdcb_t));
                    cb_node->cmd_type = MM_CAMERA_CMD_TYPE_SUPER_BUF_DATA_CB;
                    cb_node->u.superbuf.num_bufs = node->num_of_bufs;
                    for (i=0; i<node->num_of_bufs; i++) {
                        cb_node->u.superbuf.bufs[i] = node->super_buf[i].buf;
                    }
                    cb_node->u.superbuf.camera_handle = ch_obj->cam_obj->my_hdl;
                    cb_node->u.superbuf.ch_id = ch_obj->my_hdl;

                    /* enqueue to cb thread */
                    mm_camera_queue_enq(&(ch_obj->cb_thread.cmd_queue), cb_node);

                    /* wake up cb thread */
                    sem_post(&(ch_obj->cb_thread.cmd_sem));
                } else {
                    CDBG_ERROR("%s: No memory for mm_camera_node_t", __func__);
                    /* buf done with the nonuse super buf */
                    for (i=0; i<node->num_of_bufs; i++) {
                        mm_channel_qbuf(ch_obj, node->super_buf[i].buf);
                    }
                }
            } else {
                /* buf done with the nonuse super buf */
                uint8_t i;
                for (i=0; i<node->num_of_bufs; i++) {
                    mm_channel_qbuf(ch_obj, node->super_buf[i].buf);
                }
            }
            free(node);
        } else {
            /* no superbuf avail, break the loop */
            break;
        }
    }
}

/* state machine entry */
int32_t mm_channel_fsm_fn(mm_channel_t *my_obj,
                          mm_channel_evt_type_t evt,
                          void * in_val,
                          void * out_val)
{
    int32_t rc = -1;

    CDBG("%s : E state = %d",__func__,my_obj->state);
    switch (my_obj->state) {
    case MM_CHANNEL_STATE_NOTUSED:
        rc = mm_channel_fsm_fn_notused(my_obj, evt, in_val, out_val);
        break;
    case MM_CHANNEL_STATE_STOPPED:
        rc = mm_channel_fsm_fn_stopped(my_obj, evt, in_val, out_val);
        break;
    case MM_CHANNEL_STATE_ACTIVE:
        rc = mm_channel_fsm_fn_active(my_obj, evt, in_val, out_val);
        break;
    case MM_CHANNEL_STATE_PAUSED:
        rc = mm_channel_fsm_fn_paused(my_obj, evt, in_val, out_val);
        break;
    default:
        CDBG("%s: Not a valid state (%d)", __func__, my_obj->state);
        break;
    }

    /* unlock ch_lock */
    pthread_mutex_unlock(&my_obj->ch_lock);
    CDBG("%s : X rc = %d",__func__,rc);
    return rc;
}

int32_t mm_channel_fsm_fn_notused(mm_channel_t *my_obj,
                          mm_channel_evt_type_t evt,
                          void * in_val,
                          void * out_val)
{
    int32_t rc = -1;

    switch (evt) {
    default:
        CDBG_ERROR("%s: invalid state (%d) for evt (%d)",
                   __func__, my_obj->state, evt);
        break;
    }

    return rc;
}

int32_t mm_channel_fsm_fn_stopped(mm_channel_t *my_obj,
                          mm_channel_evt_type_t evt,
                          void * in_val,
                          void * out_val)
{
    int32_t rc = 0;
    CDBG("%s : E evt = %d",__func__,evt);
    switch (evt) {
    case MM_CHANNEL_EVT_ADD_STREAM:
        {
            uint32_t s_hdl = 0;
            mm_evt_paylod_add_stream_t *payload =
                (mm_evt_paylod_add_stream_t *)in_val;
            s_hdl = mm_channel_add_stream(my_obj,
                                          payload->buf_cb,
                                          payload->user_data,
                                          payload->ext_image_mode,
                                          payload->sensor_idx);
            *((uint32_t*)out_val) = s_hdl;
            rc = 0;
        }
        break;
    case MM_CHANNEL_EVT_DEL_STREAM:
        {
            uint32_t s_id = *((uint32_t *)in_val);
            rc = mm_channel_del_stream(my_obj, s_id);
        }
        break;
    case MM_CHANNEL_EVT_START_STREAM:
        {
            mm_evt_payload_start_stream_t *payload =
                (mm_evt_payload_start_stream_t *)in_val;
            rc = mm_channel_start_streams(my_obj,
                                          payload->num_streams,
                                          payload->stream_ids);
            /* first stream started in stopped state
             * move to active state */
            if (0 == rc) {
                my_obj->state = MM_CHANNEL_STATE_ACTIVE;
            }
        }
        break;
    case MM_CHANNEL_EVT_CONFIG_STREAM:
        {
            mm_evt_paylod_config_stream_t *payload =
                (mm_evt_paylod_config_stream_t *)in_val;
            rc = mm_channel_config_stream(my_obj,
                                          payload->stream_id,
                                          payload->config);
        }
        break;
    case MM_CHANNEL_EVT_INIT_BUNDLE:
        {
            mm_evt_payload_bundle_stream_t *payload =
                (mm_evt_payload_bundle_stream_t *)in_val;
            rc = mm_channel_bundle_stream(my_obj,
                                          payload->super_frame_notify_cb,
                                          payload->user_data,
                                          payload->attr,
                                          payload->num_streams,
                                          payload->stream_ids);
        }
        break;
    case MM_CHANNEL_EVT_DESTROY_BUNDLE:
        rc = mm_channel_destroy_bundle(my_obj);
        break;
    case MM_CHANNEL_EVT_PREPARE_SNAPSHOT:
        {
            uint32_t sensor_idx = (uint32_t)in_val;
            rc = mm_channel_prepare_snapshot(my_obj, sensor_idx);
        }
        break;
    case MM_CHANNEL_EVT_DELETE:
        mm_channel_release(my_obj);
        rc = 0;
        break;
    case MM_CHANNEL_EVT_SET_STREAM_PARM:
        {
            uint32_t s_id = (uint32_t)in_val;
            rc = mm_channel_set_stream_parm(my_obj, s_id, out_val);
        }
        break;
    case MM_CHANNEL_EVT_GET_STREAM_PARM:
        {
            uint32_t s_id = (uint32_t)in_val;
            rc = mm_channel_get_stream_parm(my_obj, s_id, out_val);
        }
        break;
    case MM_CHANNEL_EVT_OPEN_REPRO_ISP:
        {
            uint32_t repro_hdl = 0;
            mm_camera_repro_isp_type_t isp_type =
                (mm_camera_repro_isp_type_t)in_val;
            repro_hdl = mm_channel_open_repro_isp(my_obj, isp_type);
            *((uint32_t*)out_val) = repro_hdl;
            rc = 0;
        }
        break;
    case MM_CHANNEL_EVT_CONFIG_REPRO_ISP:
        {
            mm_evt_paylod_config_repro_isp_t *payload =
                (mm_evt_paylod_config_repro_isp_t *)in_val;
            rc = mm_channel_config_repro_isp(my_obj,
                                            payload->repro_isp_handle,
                                            payload->config);
        }
        break;
    case MM_CHANNEL_EVT_ATTACH_STREAM_TO_REPRO_ISP:
        {
            mm_evt_paylod_stream_to_repro_isp_t *payload =
                (mm_evt_paylod_stream_to_repro_isp_t *)in_val;
            rc = mm_channel_repro_isp_dest_stream_ops(my_obj,
                                                     payload->repro_isp_handle,
                                                     payload->stream_id,
                                                     TRUE);
        }
        break;
    case MM_CHANNEL_EVT_START_REPRO_ISP:
        {
            mm_evt_paylod_repro_start_stop_t *payload =
                (mm_evt_paylod_repro_start_stop_t*)in_val;
            rc = mm_channel_repro_isp_ops(my_obj,
                                          payload->repro_isp_handle,
                                          payload->stream_id,
                                          TRUE);
        }
        break;
    case MM_CHANNEL_EVT_REPROCESS:
        {
            mm_evt_paylod_reprocess_t *payload =
                (mm_evt_paylod_reprocess_t *)in_val;
            rc = mm_channel_reprocess(my_obj,
                                      payload->repro_isp_handle,
                                      payload->repro_data);
        }
        break;
    case MM_CHANNEL_EVT_STOP_REPRO_ISP:
        {
            mm_evt_paylod_repro_start_stop_t *payload =
                (mm_evt_paylod_repro_start_stop_t*)in_val;
            rc = mm_channel_repro_isp_ops(my_obj,
                                          payload->repro_isp_handle,
                                          payload->stream_id,
                                          FALSE);
        }
        break;
    case MM_CHANNEL_EVT_DETACH_STREAM_FROM_REPRO_ISP:
        {
            mm_evt_paylod_stream_to_repro_isp_t *payload =
                (mm_evt_paylod_stream_to_repro_isp_t *)in_val;
            rc = mm_channel_repro_isp_dest_stream_ops(my_obj,
                                                     payload->repro_isp_handle,
                                                     payload->stream_id,
                                                     FALSE);
        }
        break;
    case MM_CHANNEL_EVT_CLOSE_REPRO_ISP:
        {
            uint32_t repro_handle = (uint32_t)in_val;
            rc = mm_channel_close_repro_isp(my_obj, repro_handle);
        }
        break;
    default:
        CDBG_ERROR("%s: invalid state (%d) for evt (%d)",
                   __func__, my_obj->state, evt);
        break;
    }
    CDBG("%s : E rc = %d",__func__,rc);
    return rc;
}

int32_t mm_channel_fsm_fn_active(mm_channel_t *my_obj,
                          mm_channel_evt_type_t evt,
                          void * in_val,
                          void * out_val)
{
    int32_t rc = 0;

    CDBG("%s : E evt = %d",__func__,evt);
    switch (evt) {
    case MM_CHANNEL_EVT_CONFIG_STREAM:
        {
            mm_evt_paylod_config_stream_t *payload =
                (mm_evt_paylod_config_stream_t *)in_val;
            rc = mm_channel_config_stream(my_obj,
                                          payload->stream_id,
                                          payload->config);
        }
        break;
    case MM_CHANNEL_EVT_START_STREAM:
        {
            mm_evt_payload_start_stream_t *payload =
                (mm_evt_payload_start_stream_t *)in_val;
            rc = mm_channel_start_streams(my_obj,
                                          payload->num_streams,
                                          payload->stream_ids);
        }
        break;
    case MM_CHANNEL_EVT_STOP_STREAM:
    case MM_CHANNEL_EVT_TEARDOWN_STREAM:
        {
            int i;
            uint8_t tear_down_flag = (MM_CHANNEL_EVT_TEARDOWN_STREAM == evt)? 1:0;
            uint8_t all_stopped = 1;
            mm_evt_payload_stop_stream_t *payload =
                (mm_evt_payload_stop_stream_t *)in_val;

            rc = mm_channel_stop_streams(my_obj,
                                         payload->num_streams,
                                         payload->stream_ids,
                                         tear_down_flag);

            /* check if all streams are stopped
             * then we move to stopped state */

            for (i=0; i<MM_CAMEAR_STRAEM_NUM_MAX; i++) {
                if (MM_STREAM_STATE_ACTIVE_STREAM_ON == my_obj->streams[i].state ||
                    MM_STREAM_STATE_ACTIVE_STREAM_OFF == my_obj->streams[i].state) {
                    all_stopped = 0;
                    break;
                }
            }
            if (all_stopped) {
                my_obj->state = MM_CHANNEL_STATE_STOPPED;
            }

        }
        break;
    case MM_CHANNEL_EVT_INIT_BUNDLE:
        {
            mm_evt_payload_bundle_stream_t *payload =
                (mm_evt_payload_bundle_stream_t *)in_val;
            rc = mm_channel_bundle_stream(my_obj,
                                          payload->super_frame_notify_cb,
                                          payload->user_data,
                                          payload->attr,
                                          payload->num_streams,
                                          payload->stream_ids);
        }
        break;
    case MM_CHANNEL_EVT_DESTROY_BUNDLE:
        rc = mm_channel_destroy_bundle(my_obj);
        break;
    case MM_CHANNEL_EVT_REQUEST_SUPER_BUF:
        {
            uint32_t num_buf_requested = (uint32_t)in_val;
            rc = mm_channel_request_super_buf(my_obj, num_buf_requested);
        }
        break;
    case MM_CHANNEL_EVT_CANCEL_REQUEST_SUPER_BUF:
        rc = mm_channel_cancel_super_buf_request(my_obj);
        break;
    case MM_CHANNEL_EVT_START_FOCUS:
        {
            mm_evt_payload_start_focus_t* payload =
                (mm_evt_payload_start_focus_t *)in_val;
            rc = mm_channel_start_focus(my_obj,
                                        payload->sensor_idx,
                                        payload->focus_mode);
        }
        break;
    case MM_CHANNEL_EVT_ABORT_FOCUS:
        {
            uint32_t sensor_idx = (uint32_t)in_val;
            rc = mm_channel_abort_focus(my_obj, sensor_idx);
        }
        break;
    case MM_CHANNEL_EVT_PREPARE_SNAPSHOT:
        {
            uint32_t sensor_idx = (uint32_t)in_val;
            rc = mm_channel_prepare_snapshot(my_obj, sensor_idx);
        }
        break;
    case MM_CHANNEL_EVT_SET_STREAM_PARM:
        {
            uint32_t s_id = (uint32_t)in_val;
            rc = mm_channel_set_stream_parm(my_obj, s_id, out_val);
        }
        break;
    case MM_CHANNEL_EVT_GET_STREAM_PARM:
        {
            uint32_t s_id = (uint32_t)in_val;
            rc = mm_channel_get_stream_parm(my_obj, s_id, out_val);
        }
        break;

    case MM_CHANNEL_EVT_DEL_STREAM:
        {
            uint32_t s_id = *((uint32_t *)in_val);
            rc = mm_channel_del_stream(my_obj, s_id);
        }
        break;
    case MM_CHANNEL_EVT_ATTACH_STREAM_TO_REPRO_ISP:
        {
            mm_evt_paylod_stream_to_repro_isp_t *payload =
                (mm_evt_paylod_stream_to_repro_isp_t *)in_val;
            rc = mm_channel_repro_isp_dest_stream_ops(my_obj,
                                                     payload->repro_isp_handle,
                                                     payload->stream_id,
                                                     TRUE);
        }
        break;
    case MM_CHANNEL_EVT_START_REPRO_ISP:
        {
            mm_evt_paylod_repro_start_stop_t *payload =
                (mm_evt_paylod_repro_start_stop_t*)in_val;
            rc = mm_channel_repro_isp_ops(my_obj,
                                          payload->repro_isp_handle,
                                          payload->stream_id,
                                          TRUE);
        }
        break;
    case MM_CHANNEL_EVT_REPROCESS:
        {
            mm_evt_paylod_reprocess_t *payload =
                (mm_evt_paylod_reprocess_t *)in_val;
            rc = mm_channel_reprocess(my_obj,
                                      payload->repro_isp_handle,
                                      payload->repro_data);
        }
        break;
    case MM_CHANNEL_EVT_STOP_REPRO_ISP:
        {
            mm_evt_paylod_repro_start_stop_t *payload =
                (mm_evt_paylod_repro_start_stop_t*)in_val;
            rc = mm_channel_repro_isp_ops(my_obj,
                                          payload->repro_isp_handle,
                                          payload->stream_id,
                                          FALSE);
        }
        break;
    case MM_CHANNEL_EVT_DETACH_STREAM_FROM_REPRO_ISP:
        {
            mm_evt_paylod_stream_to_repro_isp_t *payload =
                (mm_evt_paylod_stream_to_repro_isp_t *)in_val;
            rc = mm_channel_repro_isp_dest_stream_ops(my_obj,
                                                     payload->repro_isp_handle,
                                                     payload->stream_id,
                                                     FALSE);
        }
        break;
    default:
        CDBG_ERROR("%s: invalid state (%d) for evt (%d)",
                   __func__, my_obj->state, evt);
        break;
    }
    CDBG("%s : X rc = %d",__func__,rc);
    return rc;
}

int32_t mm_channel_fsm_fn_paused(mm_channel_t *my_obj,
                          mm_channel_evt_type_t evt,
                          void * in_val,
                          void * out_val)
{
    int32_t rc = 0;

    /* currently we are not supporting pause/resume channel */
    CDBG_ERROR("%s: evt (%d) not supported in state (%d)",
               __func__, evt, my_obj->state);

    return rc;
}

int32_t mm_channel_init(mm_channel_t *my_obj)
{
    int32_t rc = 0;
    CDBG("%s : Launch data poll thread in channel open",__func__);
    mm_camera_poll_thread_launch(&my_obj->poll_thread[0],
                                 MM_CAMERA_POLL_TYPE_CH);

    /* change state to stopped state */
    my_obj->state = MM_CHANNEL_STATE_STOPPED;
    return rc;
}

void mm_channel_release(mm_channel_t *my_obj)
{
    /* stop data poll thread */
    mm_camera_poll_thread_release(&my_obj->poll_thread[0]);

    /* change state to notused state */
    my_obj->state = MM_CHANNEL_STATE_NOTUSED;
}

uint32_t mm_channel_get_ext_mode_from_img_mode(uint32_t img_mode)
{
    switch (img_mode) {
    case MM_CAMERA_PREVIEW:
        return MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW;
    case MM_CAMERA_VIDEO:
        return MSM_V4L2_EXT_CAPTURE_MODE_VIDEO;
    case MM_CAMERA_SNAPSHOT_MAIN:
        return MSM_V4L2_EXT_CAPTURE_MODE_MAIN;
    case MM_CAMERA_SNAPSHOT_THUMBNAIL:
        return MSM_V4L2_EXT_CAPTURE_MODE_THUMBNAIL;
    case MM_CAMERA_SNAPSHOT_RAW:
        return MSM_V4L2_EXT_CAPTURE_MODE_RAW;
    case MM_CAMERA_RDI:
        return MSM_V4L2_EXT_CAPTURE_MODE_RDI;
    case MM_CAMERA_RDI1:
        return MSM_V4L2_EXT_CAPTURE_MODE_RDI1;
    case MM_CAMERA_RDI2:
        return MSM_V4L2_EXT_CAPTURE_MODE_RDI2;
	case MM_CAMERA_SAEC:
		return MSM_V4L2_EXT_CAPTURE_MODE_AEC;
	case MM_CAMERA_SAWB:
		return MSM_V4L2_EXT_CAPTURE_MODE_AWB;
	case MM_CAMERA_SAFC:
		return MSM_V4L2_EXT_CAPTURE_MODE_AF;
	case MM_CAMERA_IHST:
		return MSM_V4L2_EXT_CAPTURE_MODE_IHIST;
	case MM_CAMERA_CS:
		return MSM_V4L2_EXT_CAPTURE_MODE_CS;
	case MM_CAMERA_RS:
		return MSM_V4L2_EXT_CAPTURE_MODE_RS;
	case MM_CAMERA_CSTA:
		return MSM_V4L2_EXT_CAPTURE_MODE_CSTA;
    case MM_CAMERA_ISP_PIX_OUTPUT1:
        return MSM_V4L2_EXT_CAPTURE_MODE_ISP_PIX_OUTPUT1;
    case MM_CAMERA_ISP_PIX_OUTPUT2:
        return MSM_V4L2_EXT_CAPTURE_MODE_ISP_PIX_OUTPUT2;
    default:
        return MSM_V4L2_EXT_CAPTURE_MODE_DEFAULT;
    }
}

uint32_t mm_channel_add_stream(mm_channel_t *my_obj,
                              mm_camera_buf_notify_t buf_cb, void *user_data,
                              uint32_t ext_image_mode, uint32_t sensor_idx)
{
    int32_t rc = 0;
    uint8_t idx = 0;
    uint32_t s_hdl = 0;
    mm_stream_t *stream_obj = NULL;

    CDBG("%s : image mode = %d",__func__,ext_image_mode);
    /* check available stream */
    for (idx = 0; idx < MM_CAMEAR_STRAEM_NUM_MAX; idx++) {
        if (MM_STREAM_STATE_NOTUSED == my_obj->streams[idx].state) {
            stream_obj = &my_obj->streams[idx];
            break;
        }
    }
    if (NULL == stream_obj) {
        CDBG_ERROR("%s: streams reach max, no more stream allowed to add", __func__);
        return s_hdl;
    }

    /* initialize stream object */
    memset(stream_obj, 0, sizeof(mm_stream_t));
    stream_obj->my_hdl = mm_camera_util_generate_handler(idx);
    stream_obj->ch_obj = my_obj;
    /* cd through intf always palced at idx 0 of buf_cb */
    stream_obj->buf_cb[0].cb = buf_cb;
    stream_obj->buf_cb[0].user_data = user_data;
    stream_obj->buf_cb[0].cb_count = -1; /* infinite by default */
    stream_obj->ext_image_mode =
        mm_channel_get_ext_mode_from_img_mode(ext_image_mode);
    stream_obj->sensor_idx = sensor_idx;
    stream_obj->fd = -1;
    pthread_mutex_init(&stream_obj->buf_lock, NULL);
    pthread_mutex_init(&stream_obj->cb_lock, NULL);
    stream_obj->state = MM_STREAM_STATE_INITED;

    /* acquire stream */
    rc = mm_stream_fsm_fn(stream_obj, MM_STREAM_EVT_ACQUIRE, NULL, NULL);
    if (0 == rc) {
        s_hdl = stream_obj->my_hdl;
    } else {
        /* error during acquire, de-init */
        pthread_mutex_destroy(&stream_obj->buf_lock);
        pthread_mutex_destroy(&stream_obj->cb_lock);
        memset(stream_obj, 0, sizeof(mm_stream_t));
    }
    CDBG("%s : stream handle = %d",__func__,s_hdl);
    return s_hdl;
}

int32_t mm_channel_del_stream(mm_channel_t *my_obj,
                              uint32_t stream_id)
{
    int rc = -1;
    mm_stream_t * stream_obj = NULL;
    stream_obj = mm_channel_util_get_stream_by_handler(my_obj, stream_id);

    if (NULL == stream_obj) {
        CDBG_ERROR("%s :Invalid Stream Object for stream_id = %d",__func__, stream_id);
        return rc;
    }

    rc = mm_stream_fsm_fn(stream_obj,
                          MM_STREAM_EVT_RELEASE,
                          NULL,
                          NULL);

    return rc;
}

int32_t mm_channel_config_stream(mm_channel_t *my_obj,
                                   uint32_t stream_id,
                                   mm_camera_stream_config_t *config)
{
    int rc = -1;
    mm_stream_t * stream_obj = NULL;
    CDBG("%s : E stream ID = %d",__func__,stream_id);
    stream_obj = mm_channel_util_get_stream_by_handler(my_obj, stream_id);

    if (NULL == stream_obj) {
        CDBG_ERROR("%s :Invalid Stream Object for stream_id = %d",__func__, stream_id);
        return rc;
    }

    /* set stream fmt */
    rc = mm_stream_fsm_fn(stream_obj,
                          MM_STREAM_EVT_SET_FMT,
                          (void *)config,
                          NULL);
    CDBG("%s : X rc = %d",__func__,rc);
    return rc;
}

int32_t mm_channel_bundle_stream(mm_channel_t *my_obj,
                                 mm_camera_buf_notify_t super_frame_notify_cb,
                                 void *user_data,
                                 mm_camera_bundle_attr_t *attr,
                                 uint8_t num_streams,
                                 uint32_t *stream_ids)
{
    int32_t rc = 0;
    int i;
    cam_stream_bundle_t bundle;
    mm_stream_t* s_objs[MM_CAMEAR_MAX_STRAEM_BUNDLE] = {NULL};

    /* first check if all streams to be bundled are valid */
    for (i=0; i < num_streams; i++) {
        s_objs[i] = mm_channel_util_get_stream_by_handler(my_obj, stream_ids[i]);
        if (NULL == s_objs[i]) {
            CDBG_ERROR("%s: invalid stream handler %d (idx=%d) to be bundled",
                       __func__, stream_ids[i], i);
            return -1;
        }
    }

    /* init superbuf queue */
    mm_channel_superbuf_queue_init(&my_obj->bundle.superbuf_queue);

    memset(&bundle, 0, sizeof(bundle));
    /* save bundle config */
    memcpy(&my_obj->bundle.superbuf_queue.attr, attr, sizeof(mm_camera_bundle_attr_t));
    my_obj->bundle.super_buf_notify_cb = super_frame_notify_cb;
    my_obj->bundle.user_data = user_data;
    my_obj->bundle.superbuf_queue.num_streams = num_streams;
    my_obj->bundle.superbuf_queue.expected_frame_id = 0;

    for (i=0; i < num_streams; i++) {
        /* set bundled flag to streams */
        s_objs[i]->is_bundled = 1;
        /* init bundled streams to invalid value -1 */
        my_obj->bundle.superbuf_queue.bundled_streams[i] = stream_ids[i];
        bundle.stream_handles[bundle.num++] =
            s_objs[i]->inst_hdl;
    }
    /* in the case of 1 bundle , current implementation is nop */
    if (bundle.num > 1) {
        rc = mm_camera_send_native_ctrl_cmd(my_obj->cam_obj,
                                                    CAMERA_SET_BUNDLE,
                                                    sizeof(bundle),
                                                    (void *)&bundle);
        if (0 != rc) {
            CDBG_ERROR("%s: set_bundle failed (rc=%d)", __func__, rc);
            return -1;
        }
    }

    /* launch cb thread for dispatching super buf through cb */
    mm_camera_cmd_thread_launch(&my_obj->cb_thread,
                                mm_channel_dispatch_super_buf,
                                (void*)my_obj);

    /* launch cmd thread for super buf dataCB */
    mm_camera_cmd_thread_launch(&my_obj->cmd_thread,
                                mm_channel_process_stream_buf,
                                (void*)my_obj);

    /* set flag to TRUE */
    my_obj->bundle.is_active = TRUE;

    return rc;
}

/* bundled streams must all be stopped before bundle can be destroyed */
int32_t mm_channel_destroy_bundle(mm_channel_t *my_obj)
{
    mm_stream_t* s_obj = NULL;
    uint8_t i;

    if (FALSE == my_obj->bundle.is_active) {
        CDBG("%s: bundle not active, no need to destroy", __func__);
        return 0;
    }

    /* first check all bundled streams should be stopped already */
    for (i=0; i < my_obj->bundle.superbuf_queue.num_streams; i++) {
        s_obj = mm_channel_util_get_stream_by_handler(my_obj,
                        my_obj->bundle.superbuf_queue.bundled_streams[i]);
        if (NULL != s_obj) {
            if (MM_STREAM_STATE_ACTIVE_STREAM_ON == s_obj->state ||
                MM_STREAM_STATE_ACTIVE_STREAM_OFF == s_obj->state) {
                CDBG_ERROR("%s: at least one of the bundled streams (%d) is still active",
                           __func__, my_obj->bundle.superbuf_queue.bundled_streams[i]);
                return -1;
            }
        }
    }

    /* deinit superbuf queue */
    mm_channel_superbuf_queue_deinit(my_obj, &my_obj->bundle.superbuf_queue);

    /* memset bundle info */
    memset(&my_obj->bundle, 0, sizeof(mm_channel_bundle_t));
    my_obj->bundle.is_active = FALSE;
    return 0;
}

int32_t mm_channel_start_streams(mm_channel_t *my_obj,
                                 uint8_t num_streams,
                                 uint32_t *stream_ids)
{
    int32_t rc = 0;
    int i, j;
    mm_stream_t* s_objs[MM_CAMEAR_MAX_STRAEM_BUNDLE] = {NULL};
    uint8_t num_streams_to_start = num_streams;
    uint32_t streams_to_start[MM_CAMEAR_MAX_STRAEM_BUNDLE];
    uint8_t bundle_to_start = 0;

    /* check if any bundled stream to be start,
     * then all bundled stream should be started */
    memcpy(streams_to_start, stream_ids, sizeof(uint32_t) * num_streams);
    for (i=0; i < num_streams; i++) {
        for (j=0; j < my_obj->bundle.superbuf_queue.num_streams; j++) {
            if (stream_ids[i] == my_obj->bundle.superbuf_queue.bundled_streams[j]) {
                bundle_to_start = 1;
                break;
            }
        }
    }

    if (bundle_to_start) {
        uint8_t need_add;
        /* add bundled streams into the start list if not already added*/
        for (i=0; i<my_obj->bundle.superbuf_queue.num_streams; i++) {
            need_add = 1;
            for (j=0; j<num_streams; j++) {
                if (stream_ids[j] == my_obj->bundle.superbuf_queue.bundled_streams[i]) {
                    need_add = 0;
                    break;
                }
            }
            if (need_add) {
                streams_to_start[num_streams_to_start++] =
                    my_obj->bundle.superbuf_queue.bundled_streams[i];
            }
        }
    }

    /* check if all streams to be started are valid */
    for (i=0; i<num_streams_to_start; i++) {
        s_objs[i] = mm_channel_util_get_stream_by_handler(my_obj, streams_to_start[i]);

        if (NULL == s_objs[i]) {
            CDBG_ERROR("%s: invalid stream handler %d (idx=%d) to be started",
                       __func__, streams_to_start[i], i);
            return -1;
        }
    }

    for (i=0; i<num_streams_to_start; i++) {
        /* if stream is already started, there is no need to start it */
        if (s_objs[i]->state == MM_STREAM_STATE_ACTIVE_STREAM_ON ||
            s_objs[i]->state == MM_STREAM_STATE_ACTIVE_STREAM_OFF) {
            continue;
        }

        /* allocate buf */
        rc = mm_stream_fsm_fn(s_objs[i],
                              MM_STREAM_EVT_GET_BUF,
                              NULL,
                              NULL);
        if (0 != rc) {
            CDBG_ERROR("%s: get buf failed at idx(%d)", __func__, i);
            break;
        }

        /* reg buf */
        rc = mm_stream_fsm_fn(s_objs[i],
                              MM_STREAM_EVT_REG_BUF,
                              NULL,
                              NULL);
        if (0 != rc) {
            CDBG_ERROR("%s: reg buf failed at idx(%d)", __func__, i);
            break;
        }

        /* start stream */
        rc = mm_stream_fsm_fn(s_objs[i],
                              MM_STREAM_EVT_START,
                              NULL,
                              NULL);
        if (0 != rc) {
            CDBG_ERROR("%s: start stream failed at idx(%d)", __func__, i);
            break;
        }
    }

    /* error handling */
    if (0 != rc) {
        for (j=0; j<=i; j++) {
            /* stop streams*/
            mm_stream_fsm_fn(s_objs[i],
                             MM_STREAM_EVT_STOP,
                             NULL,
                             NULL);

            /* unreg buf */
            mm_stream_fsm_fn(s_objs[i],
                             MM_STREAM_EVT_UNREG_BUF,
                             NULL,
                             NULL);

            /* put buf back */
            mm_stream_fsm_fn(s_objs[i],
                             MM_STREAM_EVT_PUT_BUF,
                             NULL,
                             NULL);
        }
    }

    return rc;
}

/* Required: bundled streams need to be stopped together */
int32_t mm_channel_stop_streams(mm_channel_t *my_obj,
                                uint8_t num_streams,
                                uint32_t *stream_ids,
                                uint8_t tear_down_flag)
{
    int32_t rc = 0;
    int i, j;
    mm_stream_t* s_objs[MM_CAMEAR_MAX_STRAEM_BUNDLE] = {NULL};
    uint8_t num_streams_to_stop = num_streams;
    uint32_t streams_to_stop[MM_CAMEAR_MAX_STRAEM_BUNDLE];
    uint8_t bundle_to_stop = 0;

    /* make sure bundled streams are stopped together */
    memcpy(streams_to_stop, stream_ids, sizeof(uint32_t) * num_streams);
    for (i=0; i<num_streams; i++) {
        for (j=0; j<my_obj->bundle.superbuf_queue.num_streams; j++) {
            if (stream_ids[i] == my_obj->bundle.superbuf_queue.bundled_streams[j]) {
                bundle_to_stop = 1;
                break;
            }
        }
    }
    if (bundle_to_stop) {
        /* first stop bundle thread */
        mm_camera_cmd_thread_stop(&my_obj->cmd_thread);
        mm_camera_cmd_thread_stop(&my_obj->cb_thread);

        uint8_t need_add;
        /* add bundled streams into the start list if not already added*/
        for (i=0; i<my_obj->bundle.superbuf_queue.num_streams; i++) {
            need_add = 1;
            for (j=0; j<num_streams; j++) {
                if (stream_ids[j] == my_obj->bundle.superbuf_queue.bundled_streams[i]) {
                    need_add = 0;
                    break;
                }
            }
            if (need_add) {
                streams_to_stop[num_streams_to_stop++] =
                    my_obj->bundle.superbuf_queue.bundled_streams[i];
            }
        }
    }

    for (i=0; i<num_streams_to_stop; i++) {
        s_objs[i] = mm_channel_util_get_stream_by_handler(my_obj, streams_to_stop[i]);

        if (NULL != s_objs[i]) {
            /* stream off */
            mm_stream_fsm_fn(s_objs[i],
                             MM_STREAM_EVT_STOP,
                             NULL,
                             NULL);

            /* unreg buf at kernel */
            mm_stream_fsm_fn(s_objs[i],
                             MM_STREAM_EVT_UNREG_BUF,
                             NULL,
                             NULL);
        }
    }

    /* destroy super buf cmd thread */
    if (bundle_to_stop) {
        /* first stop bundle thread */
        mm_camera_cmd_thread_destroy(&my_obj->cmd_thread);
        mm_camera_cmd_thread_destroy(&my_obj->cb_thread);
    }

    /* since all streams are stopped, we are safe to
     * release all buffers allocated in stream */
    for (i=0; i<num_streams_to_stop; i++) {
        if (NULL != s_objs[i]) {
            /* put buf back */
            mm_stream_fsm_fn(s_objs[i],
                             MM_STREAM_EVT_PUT_BUF,
                             NULL,
                             NULL);

            if (tear_down_flag) {
                /* to tear down stream totally, stream needs to be released */
                mm_stream_fsm_fn(s_objs[i],
                                 MM_STREAM_EVT_RELEASE,
                                 NULL,
                                 NULL);
            }
        }
    }

    return rc;
}

int32_t mm_channel_request_super_buf(mm_channel_t *my_obj, uint32_t num_buf_requested)
{
    int32_t rc = 0;
    mm_camera_cmdcb_t* node = NULL;

    /* set pending_cnt
     * will trigger dispatching super frames if pending_cnt > 0 */
    /* send sem_post to wake up cmd thread to dispatch super buffer */
    node = (mm_camera_cmdcb_t *)malloc(sizeof(mm_camera_cmdcb_t));
    if (NULL != node) {
        memset(node, 0, sizeof(mm_camera_cmdcb_t));
        node->cmd_type = MM_CAMERA_CMD_TYPE_REQ_DATA_CB;
        node->u.req_buf.num_buf_requested = num_buf_requested;

        /* enqueue to cmd thread */
        mm_camera_queue_enq(&(my_obj->cmd_thread.cmd_queue), node);

        /* wake up cmd thread */
        sem_post(&(my_obj->cmd_thread.cmd_sem));
    } else {
        CDBG_ERROR("%s: No memory for mm_camera_node_t", __func__);
        rc = -1;
    }

    return rc;
}

int32_t mm_channel_cancel_super_buf_request(mm_channel_t *my_obj)
{
    int32_t rc = 0;
    /* reset pending_cnt */
    my_obj->pending_cnt = 0;

    return rc;
}

int32_t mm_channel_qbuf(mm_channel_t *my_obj,
                        mm_camera_buf_def_t *buf)
{
    int32_t rc = -1;
    mm_stream_t* s_obj = mm_channel_util_get_stream_by_handler(my_obj, buf->stream_id);

    if (NULL != s_obj) {
        rc = mm_stream_fsm_fn(s_obj,
                              MM_STREAM_EVT_QBUF,
                              (void *)buf,
                              NULL);
    }

    return rc;
}

int32_t mm_channel_start_focus(mm_channel_t *my_obj,
                               uint32_t sensor_idx,
                               uint32_t focus_mode)
{
    return mm_camera_send_native_ctrl_cmd(my_obj->cam_obj,
                                          CAMERA_SET_PARM_AUTO_FOCUS,
                                          sizeof(uint32_t), (void*)focus_mode);
}

int32_t mm_channel_abort_focus(mm_channel_t *my_obj, uint32_t sensor_idx)
{
    return mm_camera_send_native_ctrl_cmd(my_obj->cam_obj,
                                          CAMERA_AUTO_FOCUS_CANCEL,
                                          0, NULL);
}

int32_t mm_channel_prepare_snapshot(mm_channel_t *my_obj, uint32_t sensor_idx)
{
    return mm_camera_send_native_ctrl_timeout_cmd(my_obj->cam_obj,
                                                CAMERA_PREPARE_SNAPSHOT,
                                                0, NULL, 2000);
}

int32_t mm_channel_set_stream_parm(mm_channel_t *my_obj,
                                   uint32_t s_id,
                                   void *value)
{
    int32_t rc = -1;
    mm_stream_t* s_obj = mm_channel_util_get_stream_by_handler(my_obj, s_id);
    if (NULL != s_obj) {
        rc = mm_stream_fsm_fn(s_obj,
                              MM_STREAM_EVT_SET_PARM,
                              value,
                              NULL);
    }

    return rc;
}

int32_t mm_channel_get_stream_parm(mm_channel_t *my_obj,
                                   uint32_t s_id,
                                   void *value)
{
    int32_t rc = -1;
    mm_stream_t* s_obj = mm_channel_util_get_stream_by_handler(my_obj, s_id);
    if (NULL != s_obj) {
        rc = mm_stream_fsm_fn(s_obj,
                              MM_STREAM_EVT_GET_PARM,
                              NULL,
                              value);
    }

    return rc;
}

int32_t mm_channel_do_post_processing(mm_channel_t *my_obj,
                                      mm_channel_queue_node_t *super_buf)
{
    int32_t rc = 0;
    uint8_t pp_mask = 0;

    pp_mask = mm_camera_util_get_pp_mask(my_obj->cam_obj);

    if (pp_mask & CAMERA_PP_MASK_TYPE_WNR) {
        mm_camera_wnr_info_t wnr_info;
        uint8_t i, j = 0;
        mm_stream_t* s_obj = NULL;

        memset(&wnr_info, 0, sizeof(mm_camera_wnr_info_t));
        for (i = 0; i < super_buf->num_of_bufs; i++) {
            s_obj = mm_channel_util_get_stream_by_handler(my_obj, super_buf->super_buf[i].stream_id);
            if (NULL != s_obj) {
                wnr_info.frames[j].instance_hdl = s_obj->inst_hdl;
                wnr_info.frames[j].frame_idx = super_buf->super_buf[i].buf->buf_idx;
                wnr_info.frames[j].frame_width = s_obj->fmt.width;
                wnr_info.frames[j].frame_height = s_obj->fmt.height;
                memcpy(&wnr_info.frames[j].frame_offset,
                       &s_obj->frame_offset,
                       sizeof(cam_frame_len_offset_t));
                j++;
            }
        }
        wnr_info.num_frames = j;

        rc = mm_camera_send_native_ctrl_cmd(my_obj->cam_obj,
                                            CAMERA_DO_PP_WNR,
                                            sizeof(mm_camera_wnr_info_t),
                                            (void *)&wnr_info);
        if (0 != rc) {
            CDBG_ERROR("%s: do_pp_wnr failed (rc=%d)", __func__, rc);
        }
    }

    return rc;
}

int32_t mm_channel_reg_stream_cb(mm_channel_t *my_obj,
                                 mm_stream_data_cb_t *cb,
                                 uint32_t ext_image_mode,
                                 uint32_t sensor_idx)
{
    uint8_t idx;
    mm_stream_t *dest_stream = NULL;
    int32_t rc = -1;

    /* browse all streams in channel to find the destination */
    for (idx=0; idx < MM_CAMEAR_STRAEM_NUM_MAX; idx++) {
        if (my_obj->streams[idx].state != MM_STREAM_STATE_NOTUSED &&
            my_obj->streams[idx].ext_image_mode == ext_image_mode &&
            my_obj->streams[idx].sensor_idx == sensor_idx) {
            /* find matching stream as the destination */
            dest_stream = &my_obj->streams[idx];
            break;
        }
    }

    if (NULL != dest_stream) {
        rc = mm_stream_reg_buf_cb(dest_stream, cb);
    }

    return rc;
}

int32_t mm_channel_superbuf_queue_init(mm_channel_queue_t * queue)
{
    return mm_camera_queue_init(&queue->que);
}

int32_t mm_channel_superbuf_queue_deinit(mm_channel_t *my_obj,
                                         mm_channel_queue_t * queue)
{
    return mm_camera_queue_deinit(&queue->que);
}

int8_t mm_channel_util_seq_comp_w_rollover(uint32_t v1,
                                           uint32_t v2)
{
    int8_t ret = 0;

    /* TODO: need to handle the case if v2 roll over to 0 */
    if (v1 > v2) {
        ret = 1;
    } else if (v1 < v2) {
        ret = -1;
    }

    return ret;
}

int32_t mm_channel_superbuf_comp_and_enqueue(
                        mm_channel_t* ch_obj,
                        mm_channel_queue_t * queue,
                        mm_camera_buf_info_t *buf_info)
{
    mm_camera_q_node_t* node = NULL;
    struct cam_list *head = NULL;
    struct cam_list *pos = NULL;
    mm_channel_queue_node_t* super_buf = NULL;
    uint8_t buf_s_idx, i;

    CDBG("%s: E", __func__);
    for (buf_s_idx=0; buf_s_idx < queue->num_streams; buf_s_idx++) {
        if (buf_info->stream_id == queue->bundled_streams[buf_s_idx]) {
            break;
        }
    }
    if (buf_s_idx == queue->num_streams) {
        CDBG_ERROR("%s: buf from stream (%d) not bundled", __func__, buf_info->stream_id);
        return -1;
    }

    if (mm_channel_util_seq_comp_w_rollover(buf_info->frame_idx,
                                            queue->expected_frame_id) < 0) {
        /* incoming buf is older than expected buf id, will discard it */
        mm_channel_qbuf(ch_obj, buf_info->buf);
        return 0;
    }

    if (MM_CAMERA_SUPER_BUF_PRIORITY_NORMAL != queue->attr.priority) {
        /* TODO */
        /* need to decide if we want to queue the frame based on focus or exposure
         * if frame not to be queued, we need to qbuf it back */
    }

    /* comp */
    pthread_mutex_lock(&queue->que.lock);
    head = &queue->que.head.list;
    /* get the last one in the queue which is possibly having no matching */
    pos = head->next;
    while (pos != head) {
        node = member_of(pos, mm_camera_q_node_t, list);
        super_buf = (mm_channel_queue_node_t*)node->data;
        if (NULL != super_buf) {
            if (super_buf->matched) {
                /* find a matched super buf, move to next one */
                pos = pos->next;
                continue;
            } else {
                /* have an unmatched super buf, break the loop */
                break;
            }
        }
    }

    if (pos == head) {
        CDBG("%s: all nodes in queue are mtached, or no node in queue, create a new node", __func__);
        /* all nodes in queue are mtached, or no node in queue
         * create a new node */
        mm_channel_queue_node_t *new_buf = NULL;
        mm_camera_q_node_t* new_node = NULL;

        new_buf = (mm_channel_queue_node_t*)malloc(sizeof(mm_channel_queue_node_t));
        new_node = (mm_camera_q_node_t*)malloc(sizeof(mm_camera_q_node_t));
        if (NULL != new_buf && NULL != new_node) {
            memset(new_buf, 0, sizeof(mm_channel_queue_node_t));
            memset(new_node, 0, sizeof(mm_camera_q_node_t));
            new_node->data = (void *)new_buf;
            new_buf->num_of_bufs = queue->num_streams;
            memcpy(&new_buf->super_buf[buf_s_idx], buf_info, sizeof(mm_camera_buf_info_t));

            /* enqueue */
            //cam_list_add_tail_node(&node->list, &queue->que.head.list);
            cam_list_add_tail_node(&new_node->list, &queue->que.head.list);
            queue->que.size++;

            if(queue->num_streams == 1) {
                //TODO : Check. Live snapshot will have one stream in bundle?
                new_buf->matched = 1;

                if (new_buf->matched) {
                    queue->expected_frame_id = buf_info->frame_idx + queue->attr.post_frame_skip;
                    queue->match_cnt++;
                }
            }
        } else {
            /* No memory */
            if (NULL != new_buf) {
                free(new_buf);
            }
            if (NULL != new_node) {
                free(new_node);
            }
            /* qbuf the new buf since we cannot enqueue */
            mm_channel_qbuf(ch_obj, buf_info->buf);
        }
    } else {
        CDBG("%s: find an unmatched super buf", __func__);
        /* find an unmatched super buf */
        if (super_buf->super_buf[buf_s_idx].frame_idx == 0) {
            /* new frame from the stream_id */
            uint8_t is_new = 1;
            uint32_t frame_idx;

            for (i=0; i < super_buf->num_of_bufs; i++) {
            //for (i=0; i < buf_s_idx; i++) {
                if(super_buf->super_buf[i].buf == NULL) {
                    continue;
                }
                frame_idx = super_buf->super_buf[i].buf->frame_idx;
                if (frame_idx == 0) {
                    continue;
                }
                if (frame_idx < buf_info->frame_idx) {
                    /* existing frame is older than the new frame, qbuf it */
                    mm_channel_qbuf(ch_obj, super_buf->super_buf[i].buf);
                    memset(&super_buf->super_buf[i], 0, sizeof(mm_camera_buf_info_t));
                } else if (frame_idx > buf_info->frame_idx) {
                    /* new frame is older */
                    is_new = 0;
                    break;
                }else{
                    //TODO: reveiw again
                    break;
                }
            }
            if (is_new) {
                CDBG("%s: add stream = %d frame id = %d ",
                     __func__, buf_info->stream_id, buf_info->frame_idx);
                memcpy(&super_buf->super_buf[buf_s_idx], buf_info, sizeof(mm_camera_buf_info_t));

                /* check if superbuf is all matched */
                super_buf->matched = 1;
                for (i=0; i < super_buf->num_of_bufs; i++) {
                    if (super_buf->super_buf[i].frame_idx == 0) {
                        super_buf->matched = 0;
                        break;
                    }
                }

                if (super_buf->matched) {
                    queue->expected_frame_id = buf_info->frame_idx + queue->attr.post_frame_skip;
                    queue->match_cnt++;
                    CDBG("%s, match_cnt = %d", __func__, queue->match_cnt);
                }
            } else {
                mm_channel_qbuf(ch_obj, buf_info->buf);
            }
        } else {
            if (super_buf->super_buf[buf_s_idx].frame_idx < buf_info->frame_idx) {
                CDBG("%s: new frame is newer, add into superbuf", __func__);
                /* current frames in superbuf are older than the new frame
                 * qbuf all current frames */
                for (i=0; i<super_buf->num_of_bufs; i++) {
                    if (super_buf->super_buf[i].frame_idx != 0) {
                            mm_channel_qbuf(ch_obj, super_buf->super_buf[i].buf);
                            memset(&super_buf->super_buf[i], 0, sizeof(mm_camera_buf_info_t));
                    }
                }
                /* add the new frame into the superbuf */
                memcpy(&super_buf->super_buf[buf_s_idx], buf_info, sizeof(mm_camera_buf_info_t));
            } else {
                /* the new frame is older, just ignor */
                mm_channel_qbuf(ch_obj, buf_info->buf);
            }
        }
    }
    pthread_mutex_unlock(&queue->que.lock);

    CDBG("%s: X", __func__);
    return 0;
}

mm_channel_queue_node_t* mm_channel_superbuf_dequeue_internal(mm_channel_queue_t * queue)
{
    mm_camera_q_node_t* node = NULL;
    struct cam_list *head = NULL;
    struct cam_list *pos = NULL;
    mm_channel_queue_node_t* super_buf = NULL;

    head = &queue->que.head.list;
    pos = head->next;
    if (pos != head) {
        /* get the first node */
        node = member_of(pos, mm_camera_q_node_t, list);
        super_buf = (mm_channel_queue_node_t*)node->data;
        if (NULL != super_buf) {
            if (super_buf->matched) {
                /* we found a mtaching super buf, dequeue it */
                cam_list_del_node(&node->list);
                queue->que.size--;
                queue->match_cnt--;
                free(node);
            } else {
                super_buf = NULL;
            }
        }
    }

    return super_buf;
}

mm_channel_queue_node_t* mm_channel_superbuf_dequeue(mm_channel_queue_t * queue)
{
    mm_camera_q_node_t* node = NULL;
    struct cam_list *head = NULL;
    struct cam_list *pos = NULL;
    mm_channel_queue_node_t* super_buf = NULL;

    pthread_mutex_lock(&queue->que.lock);
    super_buf = mm_channel_superbuf_dequeue_internal(queue);
    pthread_mutex_unlock(&queue->que.lock);

    return super_buf;
}

int32_t mm_channel_superbuf_bufdone_overflow(mm_channel_t* my_obj,
                                             mm_channel_queue_t * queue)
{
    int32_t rc = 0, i;
    mm_channel_queue_node_t* super_buf = NULL;
    if (MM_CAMERA_SUPER_BUF_NOTIFY_CONTINUOUS == queue->attr.notify_mode) {
        /* for continuous streaming mode, no overflow is needed */
        return 0;
    }

    CDBG("%s: before match_cnt=%d, water_mark=%d",
         __func__, queue->match_cnt, queue->attr.water_mark);
    /* bufdone overflowed bufs */
    pthread_mutex_lock(&queue->que.lock);
    while (queue->match_cnt > queue->attr.water_mark) {
        super_buf = mm_channel_superbuf_dequeue_internal(queue);
        if (NULL != super_buf) {
            for (i=0; i<super_buf->num_of_bufs; i++) {
                if (NULL != super_buf->super_buf[i].buf) {
                    mm_channel_qbuf(my_obj, super_buf->super_buf[i].buf);
                }
            }
            free(super_buf);
        }
    }
    pthread_mutex_unlock(&queue->que.lock);
    CDBG("%s: after match_cnt=%d, water_mark=%d",
         __func__, queue->match_cnt, queue->attr.water_mark);

    return rc;
}

int32_t mm_channel_superbuf_skip(mm_channel_t* my_obj,
                                 mm_channel_queue_t * queue)
{
    int32_t rc = 0, i, count;
    mm_channel_queue_node_t* super_buf = NULL;
    if (MM_CAMERA_SUPER_BUF_NOTIFY_CONTINUOUS == queue->attr.notify_mode) {
        /* for continuous streaming mode, no skip is needed */
        return 0;
    }

    /* bufdone overflowed bufs */
    pthread_mutex_lock(&queue->que.lock);
    while (queue->match_cnt > queue->attr.look_back) {
        super_buf = mm_channel_superbuf_dequeue_internal(queue);
        if (NULL != super_buf) {
            for (i=0; i<super_buf->num_of_bufs; i++) {
                if (NULL != super_buf->super_buf[i].buf) {
                    mm_channel_qbuf(my_obj, super_buf->super_buf[i].buf);
                }
            }
            free(super_buf);
        }
    }
    pthread_mutex_unlock(&queue->que.lock);

    return rc;
}

uint32_t mm_channel_open_repro_isp(mm_channel_t *my_obj,
                                   mm_camera_repro_isp_type_t isp_type)
{
    int32_t rc = -1;
    mm_camera_repro_cmd_t repro_cmd;

    memset(&repro_cmd, 0, sizeof(mm_camera_repro_cmd_t));
    repro_cmd.cmd = MM_CAMERA_REPRO_CMD_OPEN;
    repro_cmd.payload.open.isp_type = isp_type;
    rc = mm_camera_send_native_ctrl_cmd(my_obj->cam_obj,
                                        CAMERA_SEND_PP_PIPELINE_CMD,
                                        sizeof(mm_camera_repro_cmd_t),
                                        (void *)&repro_cmd);
    if (0 != rc) {
        CDBG_ERROR("%s: open repo isp failed (rc=%d)", __func__, rc);
    }
    return repro_cmd.payload.open.repro_handle;
}

int32_t mm_channel_close_repro_isp(mm_channel_t *my_obj,
                                   uint32_t repro_handle)
{
    int32_t rc = -1;
    mm_camera_repro_cmd_t repro_cmd;

    memset(&repro_cmd, 0, sizeof(mm_camera_repro_cmd_t));
    repro_cmd.cmd = MM_CAMERA_REPRO_CMD_CLOSE;
    repro_cmd.payload.repro_handle = repro_handle;
    rc = mm_camera_send_native_ctrl_cmd(my_obj->cam_obj,
                                        CAMERA_SEND_PP_PIPELINE_CMD,
                                        sizeof(mm_camera_repro_cmd_t),
                                        (void *)&repro_cmd);
    if (0 != rc) {
        CDBG_ERROR("%s: close repo isp failed (rc=%d)", __func__, rc);
    }
    return rc;
}

int32_t mm_channel_config_repro_isp(mm_channel_t *my_obj,
                                    uint32_t repro_handle,
                                    mm_camera_repro_isp_config_t *config)
{
    int32_t rc = -1;
    mm_camera_repro_cmd_t repro_cmd;
    mm_stream_t *src_stream = NULL;
    mm_stream_t *dest_streams[MM_CAMERA_MAX_NUM_REPROCESS_DEST];
    uint8_t i;

    /* first check if src and dest streams are valid */
    src_stream = mm_channel_util_get_stream_by_handler(my_obj, config->src.inst_handle);
    if (NULL == src_stream) {
        CDBG_ERROR("%s: src stream obj not found %d", __func__, config->src.inst_handle);
        return -1;
    }
    for (i = 0; i <  config->num_dest; i++) {
        dest_streams[i] =
            mm_channel_util_get_stream_by_handler(my_obj, config->dest[i].inst_handle);
        if (NULL == dest_streams[i]) {
            CDBG_ERROR("%s: dest stream obj not found %d", __func__, config->dest[i].inst_handle);
            return -1;
        }
    }

    /* fill in config */
    memset(&repro_cmd, 0, sizeof(mm_camera_repro_cmd_t));
    repro_cmd.cmd = MM_CAMERA_REPRO_CMD_CONFIG;
    repro_cmd.payload.config.repro_handle = repro_handle;
    /* using instant handler which can be understandable by mctl */
    repro_cmd.payload.config.src.inst_handle = src_stream->inst_hdl;
    /* translate to V4L2 ext image mode */
    repro_cmd.payload.config.src.image_mode =
        mm_channel_get_ext_mode_from_img_mode(config->src.image_mode);
    repro_cmd.payload.config.src.format = config->src.format;
    repro_cmd.payload.config.src.width = config->src.width;
    repro_cmd.payload.config.src.height = config->src.height;
    CDBG("%s: src: inst_hdl=0x%x, image_mode=%d, format=%d, width x height=%dx%d",
         __func__,
         repro_cmd.payload.config.src.inst_handle,
         repro_cmd.payload.config.src.image_mode,
         repro_cmd.payload.config.src.format,
         repro_cmd.payload.config.src.width,
         repro_cmd.payload.config.src.height);
    repro_cmd.payload.config.num_dest = config->num_dest;
    for (i = 0; i < repro_cmd.payload.config.num_dest; i++) {
        /* using instant handler which can be understandable by mctl */
        repro_cmd.payload.config.dest[i].inst_handle = dest_streams[i]->inst_hdl;
        /* translate into V4L2 ext image mode */
        repro_cmd.payload.config.dest[i].image_mode =
            mm_channel_get_ext_mode_from_img_mode(config->dest[i].image_mode);
        repro_cmd.payload.config.dest[i].format = config->dest[i].format;
        repro_cmd.payload.config.dest[i].width = config->dest[i].width;
        repro_cmd.payload.config.dest[i].height = config->dest[i].height;
        CDBG("%s: dest %d: inst_hdl=0x%x, image_mode=%d, format=%d, width x height=%dx%d",
             __func__, i,
             repro_cmd.payload.config.dest[i].inst_handle,
             repro_cmd.payload.config.dest[i].image_mode,
             repro_cmd.payload.config.dest[i].format,
             repro_cmd.payload.config.dest[i].width,
             repro_cmd.payload.config.dest[i].height);
    }
    rc = mm_camera_send_native_ctrl_cmd(my_obj->cam_obj,
                                        CAMERA_SEND_PP_PIPELINE_CMD,
                                        sizeof(mm_camera_repro_cmd_t),
                                        (void *)&repro_cmd);
    if (0 != rc) {
        CDBG_ERROR("%s: config repo isp failed (rc=%d)", __func__, rc);
    }
    return rc;
}

int32_t mm_channel_repro_isp_dest_stream_ops(mm_channel_t *my_obj,
                                             uint32_t repro_handle,
                                             uint32_t stream_handle,
                                             uint8_t attach_flag)
{
    int32_t rc = -1;
    mm_stream_t *stream_obj = NULL;
    mm_camera_repro_cmd_t repro_cmd;

    /* first check if streams are valid */
    stream_obj = mm_channel_util_get_stream_by_handler(my_obj, stream_handle);
    if (NULL == stream_obj) {
        CDBG_ERROR("%s: stream obj not found %d", __func__, stream_handle);
        return -1;
    }

    memset(&repro_cmd, 0, sizeof(mm_camera_repro_cmd_t));
    repro_cmd.cmd = MM_CAMERA_REPRO_CMD_ATTACH_DETACH;
    repro_cmd.payload.attach_detach.repro_handle = repro_handle;
    repro_cmd.payload.attach_detach.attach_flag = attach_flag;
    repro_cmd.payload.attach_detach.inst_handle = stream_obj->inst_hdl;
    rc = mm_camera_send_native_ctrl_cmd(my_obj->cam_obj,
                                        CAMERA_SEND_PP_PIPELINE_CMD,
                                        sizeof(mm_camera_repro_cmd_t),
                                        (void *)&repro_cmd);
    if (0 != rc) {
        CDBG_ERROR("%s: repo isp stream operation failed (flag=%d, rc=%d)",
                   __func__, attach_flag, rc);
    }
    return rc;
}

int32_t mm_channel_repro_isp_ops(mm_channel_t *my_obj,
                                 uint32_t repro_handle,
                                 uint32_t stream_id,
                                 uint8_t start_flag)
{
    int32_t rc = -1;
    mm_camera_repro_cmd_t repro_cmd;
    mm_stream_t *stream_obj = NULL;

    /* first check if streams are valid */
    stream_obj = mm_channel_util_get_stream_by_handler(my_obj, stream_id);
    if (NULL == stream_obj) {
        CDBG_ERROR("%s: stream obj not found %d", __func__, stream_id);
        return -1;
    }

    memset(&repro_cmd, 0, sizeof(mm_camera_repro_cmd_t));
    repro_cmd.cmd = MM_CAMERA_REPRO_CMD_START_STOP;
    repro_cmd.payload.start_stop.repro_handle = repro_handle;
    repro_cmd.payload.start_stop.dest_handle = stream_obj->inst_hdl;
    repro_cmd.payload.start_stop.start_flag = start_flag;
    rc = mm_camera_send_native_ctrl_cmd(my_obj->cam_obj,
                                        CAMERA_SEND_PP_PIPELINE_CMD,
                                        sizeof(mm_camera_repro_cmd_t),
                                        (void *)&repro_cmd);
    if (0 != rc) {
        CDBG_ERROR("%s: repo isp stream operation failed (flag=%d, rc=%d)",
                   __func__, start_flag, rc);
    }
    return rc;
}

int32_t mm_channel_reprocess(mm_channel_t *my_obj,
                             uint32_t repro_handle,
                             mm_camera_repro_data_t *repro_data)
{
    int32_t rc = -1;
    mm_camera_repro_cmd_t repro_cmd;
    mm_stream_t *stream_obj = NULL;
    uint8_t i;

    /* check if input param is valid */
    if (NULL == repro_data || NULL == repro_data->src_frame) {
        CDBG_ERROR("%s: invalid input param", __func__);
        return -1;
    }

    /* check if streams are valid */
    stream_obj = mm_channel_util_get_stream_by_handler(my_obj,
                                                       repro_data->src_frame->stream_id);
    if (NULL == stream_obj) {
        CDBG_ERROR("%s: stream obj not found %d",
                   __func__, repro_data->src_frame->stream_id);
        return -1;
    }

    memset(&repro_cmd, 0, sizeof(mm_camera_repro_cmd_t));
    repro_cmd.cmd = MM_CAMERA_REPRO_CMD_START_STOP;
    repro_cmd.payload.reprocess.repro_handle = repro_handle;
    repro_cmd.payload.reprocess.inst_handle = stream_obj->inst_hdl;
    repro_cmd.payload.reprocess.buf_idx = repro_data->src_frame->buf_idx;
    repro_cmd.payload.reprocess.frame_id = repro_data->src_frame->frame_idx;
    repro_cmd.payload.reprocess.frame_len = repro_data->src_frame->frame_len;
    repro_cmd.payload.reprocess.timestamp.tv_sec =
        repro_data->src_frame->ts.tv_sec;
    repro_cmd.payload.reprocess.timestamp.tv_usec =
        repro_data->src_frame->ts.tv_nsec/1000;
    repro_cmd.payload.reprocess.num_planes = repro_data->src_frame->num_planes;
    for (i = 0; i < repro_cmd.payload.reprocess.num_planes; i++) {
        repro_cmd.payload.reprocess.planes[i].addr_offset =
            repro_data->src_frame->planes[i].reserved[0];
        repro_cmd.payload.reprocess.planes[i].data_offset =
            repro_data->src_frame->planes[i].data_offset;
        repro_cmd.payload.reprocess.planes[i].length =
            repro_data->src_frame->planes[i].length;
    }
    rc = mm_camera_send_native_ctrl_cmd(my_obj->cam_obj,
                                        CAMERA_SEND_PP_PIPELINE_CMD,
                                        sizeof(mm_camera_repro_cmd_t),
                                        (void *)&repro_cmd);
    if (0 != rc) {
        CDBG_ERROR("%s: do reprocess failed (rc=%d)", __func__, rc);
    }
    return rc;
}

