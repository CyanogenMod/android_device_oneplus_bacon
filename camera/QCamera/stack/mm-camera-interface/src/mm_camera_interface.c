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
#include <linux/media.h>
#include <semaphore.h>

#include "mm_camera_dbg.h"
#include "mm_camera_interface.h"
#include "mm_camera_sock.h"
#include "mm_camera.h"

static pthread_mutex_t g_intf_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_oem_lock = PTHREAD_MUTEX_INITIALIZER;

static mm_camera_ctrl_t g_cam_ctrl = {{{0, {0, 0, 0, 0}, 0, 0}}, 0, {{0}}, {0}};

static pthread_mutex_t g_handler_lock = PTHREAD_MUTEX_INITIALIZER;
static uint16_t g_handler_history_count = 0; /* history count for handler */

extern int32_t mm_camera_send_native_ctrl_cmd(mm_camera_obj_t * my_obj,
                                       cam_ctrl_type type,
                                       uint32_t length,
                                       void *value);

/* utility function to generate handler */
uint32_t mm_camera_util_generate_handler(uint8_t index)
{
    uint32_t handler = 0;
    pthread_mutex_lock(&g_handler_lock);
    g_handler_history_count++;
    if (0 == g_handler_history_count) {
        g_handler_history_count++;
    }
    handler = g_handler_history_count;
    handler = (handler<<8) | index;
    pthread_mutex_unlock(&g_handler_lock);
    return handler;
}

uint8_t mm_camera_util_get_index_by_handler(uint32_t handler)
{
    return (handler&0x000000ff);
}

const char *mm_camera_util_get_dev_name(uint32_t cam_handler)
{
    char *dev_name = NULL;
    uint8_t cam_idx = mm_camera_util_get_index_by_handler(cam_handler);
    dev_name = g_cam_ctrl.camera[cam_idx].video_dev_name;
    return dev_name;
}

mm_camera_obj_t* mm_camera_util_get_camera_by_handler(uint32_t cam_handler)
{
    mm_camera_obj_t *cam_obj = NULL;
    uint8_t cam_idx = mm_camera_util_get_index_by_handler(cam_handler);

    if ((NULL != g_cam_ctrl.cam_obj[cam_idx]) &&
        (cam_handler == g_cam_ctrl.cam_obj[cam_idx]->my_hdl)) {
        cam_obj = g_cam_ctrl.cam_obj[cam_idx];
    }
    return cam_obj;
}

static int32_t mm_camera_intf_sync(uint32_t camera_handler)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s E: camera_handler = %d ",__func__,camera_handler);

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_sync(my_obj);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

/* check if the operation is supported */
static int32_t mm_camera_intf_is_op_supported(uint32_t camera_handler,mm_camera_ops_type_t opcode)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_is_op_supported(my_obj, opcode);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    return rc;
}

/* check if the parm is supported */
static int32_t mm_camera_intf_is_parm_supported(uint32_t camera_handler,
                                   mm_camera_parm_type_t parm_type,
                                   uint8_t *support_set_parm,
                                   uint8_t *support_get_parm)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);
    *support_set_parm = 0;
    *support_get_parm = 0;

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_is_parm_supported(my_obj, parm_type, support_set_parm, support_get_parm);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    return rc;
}

/* set a parm’s current value */
static int32_t mm_camera_intf_set_parm(uint32_t camera_handler,
                                   mm_camera_parm_type_t parm_type,
                                   void* p_value)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_set_parm(my_obj, parm_type, p_value);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    return rc;
}

/* get a parm’s current value */
static int32_t mm_camera_intf_get_parm(uint32_t camera_handler,
                                mm_camera_parm_type_t parm_type,
                                void* p_value)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_get_parm(my_obj, parm_type, p_value);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    return rc;
}

static void mm_camera_intf_close(uint32_t camera_handler)
{
    int32_t rc = -1;
    uint8_t cam_idx = camera_handler & 0x00ff;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s E: camera_handler = %d ",__func__,camera_handler);

    /* first we need to get oem_lock,
     * in case oem private ioctl is still processing,
     * we need to wait until it finishes */
    pthread_mutex_lock(&g_oem_lock);

    /* then we get intf_lock */
    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if (my_obj){
        my_obj->ref_count--;

        if(my_obj->ref_count > 0) {
            /* still have reference to obj, return here */
            CDBG("%s: ref_count=%d\n", __func__, my_obj->ref_count);
            pthread_mutex_unlock(&g_intf_lock);
        } else {
            /* need close camera here as no other reference
             * first empty g_cam_ctrl's referent to cam_obj */
            g_cam_ctrl.cam_obj[cam_idx] = NULL;

            pthread_mutex_lock(&my_obj->cam_lock);
            pthread_mutex_unlock(&g_intf_lock);

            mm_camera_close(my_obj);

            pthread_mutex_destroy(&my_obj->cam_lock);
            free(my_obj);
        }
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }

    /* unlock oem_lock before we leave */
    pthread_mutex_unlock(&g_oem_lock);
}

static uint32_t mm_camera_intf_add_channel(uint32_t camera_handler)
{
    uint32_t ch_id = 0;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E camera_handler = %d",__func__,camera_handler);
    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        ch_id = mm_camera_add_channel(my_obj);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X ch_id = %d",__func__,ch_id);
    return ch_id;
}

static void mm_camera_intf_del_channel(uint32_t camera_handler, uint32_t ch_id)
{
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E ch_id = %d",__func__,ch_id);
    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        mm_camera_del_channel(my_obj, ch_id);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X",__func__);
}

static uint8_t mm_camera_intf_is_event_supported(uint32_t camera_handler,
                                    mm_camera_event_type_t evt_type)
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

static int32_t mm_camera_intf_register_event_notify(
                                    uint32_t camera_handler,
                                    mm_camera_event_notify_t evt_cb,
                                    void * user_data,
                                    mm_camera_event_type_t evt_type)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E evt_type = %d",__func__,evt_type);
    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_register_event_notify(my_obj, evt_cb, user_data, evt_type);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :E rc = %d",__func__,rc);
    return rc;
}

static mm_camera_2nd_sensor_t * mm_camera_intf_query_2nd_sensor_info(uint32_t camera_handler)
{
    mm_camera_2nd_sensor_t *sensor_info = NULL;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E camera_handler = %d",__func__,camera_handler);
    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        sensor_info = mm_camera_query_2nd_sensor_info(my_obj);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X",__func__);
    return sensor_info;
}

static int32_t mm_camera_intf_qbuf(uint32_t camera_handler,
                                    uint32_t ch_id,
                                    mm_camera_buf_def_t *buf)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_qbuf(my_obj, ch_id, buf);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X evt_type = %d",__func__,rc);
    return rc;
}

static uint32_t mm_camera_intf_add_stream(
                                    uint32_t camera_handler,
                                    uint32_t ch_id,
                                    mm_camera_buf_notify_t buf_cb, void *user_data,
                                    uint32_t ext_image_mode, uint32_t sensor_idx)
{
    uint32_t stream_id = 0;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s : E handle = %d ch_id = %d",__func__,camera_handler,
         ch_id);

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        stream_id = mm_camera_add_stream(my_obj, ch_id, buf_cb,
                                  user_data, ext_image_mode, sensor_idx);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X stream_id = %d",__func__,stream_id);
    return stream_id;
}

static int32_t mm_camera_intf_del_stream(
                                    uint32_t camera_handler,
                                    uint32_t ch_id,
                                    uint32_t stream_id)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s : E handle = %d ch_id = %d stream_id = %d",__func__,camera_handler,
         ch_id,stream_id);

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_del_stream(my_obj, ch_id, stream_id);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static int32_t mm_camera_intf_config_stream(
                                    uint32_t camera_handler,
                                    uint32_t ch_id,
                                    uint32_t stream_id,
                                    mm_camera_stream_config_t *config)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E handle = %d, ch_id = %d,stream_id = %d",__func__,
         camera_handler,ch_id,stream_id);

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    CDBG("%s :mm_camera_intf_config_stream stream_id = %d",__func__,stream_id);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_config_stream(my_obj, ch_id, stream_id, config);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static int32_t mm_camera_intf_bundle_streams(
                                    uint32_t camera_handler,
                                    uint32_t ch_id,
                                    mm_camera_buf_notify_t super_frame_notify_cb,
                                    void *user_data,
                                    mm_camera_bundle_attr_t *attr,
                                    uint8_t num_streams,
                                    uint32_t *stream_ids)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E handle = %d, ch_id = %d",__func__,
         camera_handler,ch_id);

    if (MM_CAMEAR_MAX_STRAEM_BUNDLE < num_streams) {
        CDBG_ERROR("%s: number of streams (%d) exceeds max (%d)",
                   __func__, num_streams, MM_CAMEAR_MAX_STRAEM_BUNDLE);
        return rc;
    }

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_bundle_streams(my_obj, ch_id,
                                      super_frame_notify_cb,
                                      user_data, attr,
                                      num_streams, stream_ids);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static int32_t mm_camera_intf_destroy_bundle(uint32_t camera_handler,
                                    uint32_t ch_id)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E handle = %d, ch_id = %d",__func__,
         camera_handler,ch_id);
    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_destroy_bundle(my_obj, ch_id);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static int32_t mm_camera_intf_start_streams(
                                    uint32_t camera_handler,
                                    uint32_t ch_id,
                                    uint8_t num_streams,
                                    uint32_t *stream_ids)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E camera_handler = %d,ch_id = %d, num_streams = %d",
         __func__,camera_handler,ch_id,num_streams);
    if (MM_CAMEAR_STRAEM_NUM_MAX < num_streams) {
        CDBG_ERROR("%s: num of streams (%d) exceeds MAX (%d)",
                   __func__, num_streams, MM_CAMEAR_STRAEM_NUM_MAX);
        return rc;
    }

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_start_streams(my_obj, ch_id, num_streams, stream_ids);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static int32_t mm_camera_intf_stop_streams(
                                    uint32_t camera_handler,
                                    uint32_t ch_id,
                                    uint8_t num_streams,
                                    uint32_t *stream_ids)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E camera_handler = %d,ch_id = %d, num_streams = %d",
         __func__,camera_handler,ch_id,num_streams);

    if (MM_CAMEAR_STRAEM_NUM_MAX < num_streams) {
        CDBG_ERROR("%s: num of streams (%d) exceeds MAX (%d)",
                   __func__, num_streams, MM_CAMEAR_STRAEM_NUM_MAX);
        return rc;
    }

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_stop_streams(my_obj, ch_id,
                                    num_streams, stream_ids);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static int32_t mm_camera_intf_async_teardown_streams(
                                    uint32_t camera_handler,
                                    uint32_t ch_id,
                                    uint8_t num_streams,
                                    uint32_t *stream_ids)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E camera_handler = %d,ch_id = %d, num_streams = %d",
         __func__,camera_handler,ch_id,num_streams);

    if (MM_CAMEAR_STRAEM_NUM_MAX < num_streams) {
        CDBG_ERROR("%s: num of streams (%d) exceeds MAX (%d)",
                   __func__, num_streams, MM_CAMEAR_STRAEM_NUM_MAX);
        return rc;
    }

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_async_teardown_streams(my_obj, ch_id,
                                              num_streams, stream_ids);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static int32_t mm_camera_intf_request_super_buf(
                                    uint32_t camera_handler,
                                    uint32_t ch_id,
                                    uint32_t num_buf_requested)
{
    int32_t rc = -1;
    CDBG("%s :E camera_handler = %d,ch_id = %d",
         __func__,camera_handler,ch_id);
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_request_super_buf(my_obj, ch_id, num_buf_requested);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static int32_t mm_camera_intf_cancel_super_buf_request(
                                    uint32_t camera_handler,
                                    uint32_t ch_id)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E camera_handler = %d,ch_id = %d",
         __func__,camera_handler,ch_id);
    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_cancel_super_buf_request(my_obj, ch_id);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static int32_t mm_camera_intf_start_focus(
                                    uint32_t camera_handler,
                                    uint32_t ch_id,
                                    uint32_t sensor_idx,
                                    uint32_t focus_mode)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E camera_handler = %d,ch_id = %d, focus_mode = %d",
         __func__,camera_handler,ch_id,focus_mode);
    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_start_focus(my_obj, ch_id, sensor_idx, focus_mode);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static int32_t mm_camera_intf_abort_focus(
                                    uint32_t camera_handler,
                                    uint32_t ch_id,
                                    uint32_t sensor_idx)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_abort_focus(my_obj, ch_id, sensor_idx);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static int32_t mm_camera_intf_prepare_snapshot(
                                    uint32_t camera_handler,
                                    uint32_t ch_id,
                                    uint32_t sensor_idx)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handler);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_prepare_snapshot(my_obj, ch_id, sensor_idx);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static int32_t mm_camera_intf_set_stream_parm(
                                     uint32_t camera_handle,
                                     uint32_t ch_id,
                                     uint32_t s_id,
                                     mm_camera_stream_parm_t parm_type,
                                     void* p_value)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    CDBG("%s :E camera_handle = %d,ch_id = %d,s_id = %d,parm_type = %d",__func__,
            camera_handle,ch_id,s_id,parm_type);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_set_stream_parm(my_obj,ch_id,s_id,
                                       parm_type,
                                       p_value);
    }else{
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static int32_t mm_camera_intf_get_stream_parm(
                                     uint32_t camera_handle,
                                     uint32_t ch_id,
                                     uint32_t s_id,
                                     mm_camera_stream_parm_t parm_type,
                                     void* p_value)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    CDBG("%s :E camera_handle = %d,ch_id = %d,s_id = %d,parm_type = %d",__func__,
            camera_handle,ch_id,s_id,parm_type);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_get_stream_parm(my_obj,ch_id,s_id,
                                       parm_type,
                                       p_value);
    }else{
        pthread_mutex_unlock(&g_intf_lock);
    }

    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static int32_t mm_camera_intf_send_private_ioctl(uint32_t camera_handle,
                                                 uint32_t cmd_id,
                                                 uint32_t cmd_length,
                                                 void *cmd)
{
    int32_t rc = -1;
    mm_camera_obj_t *my_obj = NULL;

    pthread_mutex_lock(&g_oem_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if (my_obj) {
        rc = mm_camera_send_private_ioctl(my_obj, cmd_id, cmd_length, cmd);
    }

    pthread_mutex_unlock(&g_oem_lock);
    CDBG("%s :X rc = %d", __func__, rc);
    return rc;
}

static int32_t mm_camera_intf_send_native_cmd(uint32_t camera_handle,
                                              uint32_t cmd_id,
                                              uint32_t cmd_length,
                                              void *cmd)
{
    int32_t rc = -1;
    mm_camera_obj_t *my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if (!cmd) {
        CDBG_ERROR("%s Null cmd ", __func__);
        return -EINVAL;
    }

    if (my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        switch(cmd_id) {
        case NATIVE_CMD_ID_SOCKET_MAP: {
            cam_sock_packet_t packet;
            memset(&packet, 0, sizeof(cam_sock_packet_t));
            memcpy(&packet.payload.frame_fd_map, cmd, sizeof(mm_camera_frame_map_type));
            if(packet.payload.frame_fd_map.is_hist) {
                packet.msg_type = CAM_SOCK_MSG_TYPE_HIST_MAPPING;
            } else {
                packet.msg_type = CAM_SOCK_MSG_TYPE_FD_MAPPING;
            }
            rc = mm_camera_util_sendmsg(my_obj, &packet,
                                      sizeof(cam_sock_packet_t),
                                      packet.payload.frame_fd_map.fd);
          }
          break;
        case NATIVE_CMD_ID_SOCKET_UNMAP: {
            cam_sock_packet_t packet;
            memset(&packet, 0, sizeof(cam_sock_packet_t));
            memcpy(&packet.payload.frame_fd_unmap, cmd, sizeof(mm_camera_frame_unmap_type));
            if(packet.payload.frame_fd_unmap.is_hist) {
                packet.msg_type = CAM_SOCK_MSG_TYPE_HIST_UNMAPPING;
            } else {
                packet.msg_type = CAM_SOCK_MSG_TYPE_FD_UNMAPPING;
            }
            rc = mm_camera_util_sendmsg(my_obj, &packet,
                                      sizeof(cam_sock_packet_t), 0);
          }
          break;
       case NATIVE_CMD_ID_IOCTL_CTRL:
           /* may switch to send through native ctrl cmd ioctl */
           rc = mm_camera_util_sendmsg(my_obj, cmd,
                                      cmd_length, 0);
#if 0
           rc = mm_camera_send_native_ctrl_cmd(my_obj, cmd_id,
                                                cmd_length, cmd);
#endif
          break;
       default:
          CDBG_ERROR("%s Invalid native cmd id %d ", __func__, cmd_id);
          rc = -EINVAL;
          break;
        }
        pthread_mutex_unlock(&my_obj->cam_lock);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }

    CDBG("%s :X rc = %d", __func__, rc);
    return rc;
}

static int32_t mm_camera_intf_send_cmd(uint32_t camera_handle,
                                       mm_camera_cmd_type_t cmd_type,
                                       uint32_t cmd_id,
                                       uint32_t cmd_length,
                                       void *cmd)
{
    int32_t rc = -1;

    switch (cmd_type) {
    case MM_CAMERA_CMD_TYPE_PRIVATE:
        /* OEM private ioctl */
        rc = mm_camera_intf_send_private_ioctl(camera_handle, cmd_id, cmd_length, cmd);
        break;
    case MM_CAMERA_CMD_TYPE_NATIVE:
        rc = mm_camera_intf_send_native_cmd(camera_handle, cmd_id, cmd_length, cmd);
        break;
    }

    return rc;
}

static uint32_t mm_camera_intf_open_repro_isp(uint32_t camera_handle,
                                              uint32_t ch_id,
                                              mm_camera_repro_isp_type_t repro_isp_type)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;
    uint32_t repro_isp_handle = 0;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_open_repro_isp(my_obj,
                                     ch_id,
                                     repro_isp_type,
                                     &repro_isp_handle);
    }else{
        pthread_mutex_unlock(&g_intf_lock);
    }

    CDBG("%s :X rc = %d",__func__,rc);
    return repro_isp_handle;
}

static int32_t mm_camera_intf_config_repro_isp(uint32_t camera_handle,
                                               uint32_t ch_id,
                                               uint32_t repro_isp_handle,
                                               mm_camera_repro_isp_config_t *config)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_config_repro_isp(my_obj,
                                        ch_id,
                                        repro_isp_handle,
                                        config);
    }else{
        pthread_mutex_unlock(&g_intf_lock);
    }

    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static int32_t mm_camera_intf_attach_stream_to_repro_isp(uint32_t camera_handle,
                                                         uint32_t ch_id,
                                                         uint32_t repro_isp_handle,
                                                         uint32_t stream_id)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_attach_stream_to_repro_isp(my_obj,
                                                  ch_id,
                                                  repro_isp_handle,
                                                  stream_id);
    }else{
        pthread_mutex_unlock(&g_intf_lock);
    }

    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static int32_t mm_camera_intf_start_repro_isp(uint32_t camera_handle,
                                              uint32_t ch_id,
                                              uint32_t repro_isp_handle,
                                              uint32_t stream_id)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_start_repro_isp(my_obj,
                                       ch_id,
                                       repro_isp_handle,
                                       stream_id);
    }else{
        pthread_mutex_unlock(&g_intf_lock);
    }

    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static int32_t mm_camera_intf_reprocess(uint32_t camera_handle,
                                        uint32_t ch_id,
                                        uint32_t repro_isp_handle,
                                        mm_camera_repro_data_t *repro_data)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_reprocess(my_obj,
                                 ch_id,
                                 repro_isp_handle,
                                 repro_data);
    }else{
        pthread_mutex_unlock(&g_intf_lock);
    }

    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static int32_t mm_camera_intf_stop_repro_isp(uint32_t camera_handle,
                                             uint32_t ch_id,
                                             uint32_t repro_isp_handle,
                                             uint32_t stream_id)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_stop_repro_isp(my_obj,
                                      ch_id,
                                      repro_isp_handle,
                                      stream_id);
    }else{
        pthread_mutex_unlock(&g_intf_lock);
    }

    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static int32_t mm_camera_intf_detach_stream_from_repro_isp(uint32_t camera_handle,
                                                           uint32_t ch_id,
                                                           uint32_t repro_isp_handle,
                                                           uint32_t stream_id)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_detach_stream_from_repro_isp(my_obj,
                                                    ch_id,
                                                    repro_isp_handle,
                                                    stream_id);
    }else{
        pthread_mutex_unlock(&g_intf_lock);
    }

    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

static int32_t mm_camera_intf_close_repro_isp(uint32_t camera_handle,
                                              uint32_t ch_id,
                                              uint32_t repro_isp_handle)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_close_repro_isp(my_obj,
                                       ch_id,
                                       repro_isp_handle);
    }else{
        pthread_mutex_unlock(&g_intf_lock);
    }

    CDBG("%s :X rc = %d",__func__,rc);
    return rc;
}

mm_camera_info_t * camera_query(uint8_t *num_cameras)
{
    int i = 0, rc = 0;
    int dev_fd = 0;
    struct media_device_info mdev_info;
    int num_media_devices = 0;
    if (!num_cameras) {
        CDBG_ERROR("%s: num_cameras is NULL\n", __func__);
        return NULL;
    }

    CDBG("%s : E",__func__);
    /* lock the mutex */
    pthread_mutex_lock(&g_intf_lock);
    *num_cameras = 0;
    while (1) {
        char dev_name[32];
        snprintf(dev_name, sizeof(dev_name), "/dev/media%d", num_media_devices);
        dev_fd = open(dev_name, O_RDWR | O_NONBLOCK);
        if (dev_fd < 0) {
            CDBG("Done discovering media devices\n");
            break;
        }
        num_media_devices++;
        rc = ioctl(dev_fd, MEDIA_IOC_DEVICE_INFO, &mdev_info);
        if (rc < 0) {
            CDBG_ERROR("Error: ioctl media_dev failed: %s\n", strerror(errno));
            close(dev_fd);
            break;
        }

        if(strncmp(mdev_info.model, QCAMERA_NAME, sizeof(mdev_info.model) != 0)) {
            close(dev_fd);
            continue;
        }

        char * mdev_cfg;
        int cam_type = 0, mount_angle = 0, info_index = 0;
        mdev_cfg = strtok(mdev_info.serial, "-");
        while(mdev_cfg != NULL) {
            if(info_index == 0) {
                if(strcmp(mdev_cfg, QCAMERA_NAME))
                  break;
            } else if(info_index == 1) {
                mount_angle = atoi(mdev_cfg);
            } else if(info_index == 2) {
                cam_type = atoi(mdev_cfg);
            }
            mdev_cfg = strtok(NULL, "-");
            info_index++;
        }

        if(info_index == 0) {
            close(dev_fd);
            continue;
        }

        int num_entities = 1;
        while (1) {
            struct media_entity_desc entity;
            memset(&entity, 0, sizeof(entity));
            entity.id = num_entities++;
            rc = ioctl(dev_fd, MEDIA_IOC_ENUM_ENTITIES, &entity);
            if (rc < 0) {
                CDBG("Done enumerating media entities\n");
                rc = 0;
                break;
            }
            if(entity.type == MEDIA_ENT_T_DEVNODE_V4L && entity.group_id == QCAMERA_VNODE_GROUP_ID) {
                strncpy(g_cam_ctrl.video_dev_name[*num_cameras],
                     entity.name, sizeof(entity.name));
                g_cam_ctrl.camera[*num_cameras].video_dev_name =
                    &g_cam_ctrl.video_dev_name[*num_cameras][0];
                break;
            }
        }

        //g_cam_ctrl.camera[*num_cameras].camera_info.camera_id = *num_cameras;
        g_cam_ctrl.camera[*num_cameras].camera_id = *num_cameras;

        g_cam_ctrl.camera[*num_cameras].
            camera_info.modes_supported = CAMERA_MODE_2D;

        if(cam_type > 1) {
            g_cam_ctrl.camera[*num_cameras].
                camera_info.modes_supported |= CAMERA_MODE_3D;
        }

        g_cam_ctrl.camera[*num_cameras].camera_info.position =
            (cam_type == 1) ? FRONT_CAMERA : BACK_CAMERA;

        g_cam_ctrl.camera[*num_cameras].camera_info.sensor_mount_angle =
            mount_angle;

        g_cam_ctrl.camera[*num_cameras].main_sensor_type = 0;

        CDBG("%s: dev_info[id=%d,name='%s',pos=%d,modes=0x%x,sensor=%d mount_angle = %d]\n",
            __func__, *num_cameras,
            g_cam_ctrl.camera[*num_cameras].video_dev_name,
            g_cam_ctrl.camera[*num_cameras].camera_info.position,
            g_cam_ctrl.camera[*num_cameras].camera_info.modes_supported,
            g_cam_ctrl.camera[*num_cameras].main_sensor_type,
            g_cam_ctrl.camera[*num_cameras].camera_info.sensor_mount_angle);

        *num_cameras += 1;
        if (dev_fd > 0) {
            close(dev_fd);
        }
    }
    *num_cameras = *num_cameras;
    g_cam_ctrl.num_cam = *num_cameras;

    /* unlock the mutex */
    pthread_mutex_unlock(&g_intf_lock);
    CDBG("%s: num_cameras=%d\n", __func__, g_cam_ctrl.num_cam);
    if(rc == 0)
        return &g_cam_ctrl.camera[0];
    else
        return NULL;
}

/* camera ops v-table */
static mm_camera_ops_t mm_camera_ops = {
    .sync = mm_camera_intf_sync,
    .is_event_supported = mm_camera_intf_is_event_supported,
    .register_event_notify = mm_camera_intf_register_event_notify,
    .qbuf = mm_camera_intf_qbuf,
    .camera_close = mm_camera_intf_close,
    .query_2nd_sensor_info = mm_camera_intf_query_2nd_sensor_info,
    .is_op_supported = mm_camera_intf_is_op_supported,
    .is_parm_supported = mm_camera_intf_is_parm_supported,
    .set_parm = mm_camera_intf_set_parm,
    .get_parm = mm_camera_intf_get_parm,
    .ch_acquire = mm_camera_intf_add_channel,
    .ch_release = mm_camera_intf_del_channel,
    .add_stream = mm_camera_intf_add_stream,
    .del_stream = mm_camera_intf_del_stream,
    .config_stream = mm_camera_intf_config_stream,
    .init_stream_bundle = mm_camera_intf_bundle_streams,
    .destroy_stream_bundle = mm_camera_intf_destroy_bundle,
    .start_streams = mm_camera_intf_start_streams,
    .stop_streams = mm_camera_intf_stop_streams,
    .async_teardown_streams = mm_camera_intf_async_teardown_streams,
    .request_super_buf = mm_camera_intf_request_super_buf,
    .cancel_super_buf_request = mm_camera_intf_cancel_super_buf_request,
    .start_focus = mm_camera_intf_start_focus,
    .abort_focus = mm_camera_intf_abort_focus,
    .prepare_snapshot = mm_camera_intf_prepare_snapshot,
    .set_stream_parm = mm_camera_intf_set_stream_parm,
    .get_stream_parm = mm_camera_intf_get_stream_parm,
    .send_command = mm_camera_intf_send_cmd,
    .open_repro_isp = mm_camera_intf_open_repro_isp,
    .config_repro_isp = mm_camera_intf_config_repro_isp,
    .attach_stream_to_repro_isp = mm_camera_intf_attach_stream_to_repro_isp,
    .start_repro_isp = mm_camera_intf_start_repro_isp,
    .reprocess =  mm_camera_intf_reprocess,
    .stop_repro_isp = mm_camera_intf_stop_repro_isp,
    .detach_stream_from_repro_isp = mm_camera_intf_detach_stream_from_repro_isp,
    .close_repro_isp = mm_camera_intf_close_repro_isp
};

/* open camera. */
mm_camera_vtbl_t * camera_open(uint8_t camera_idx,
                               mm_camear_mem_vtbl_t *mem_vtbl)
{
    int32_t rc = 0;
    mm_camera_obj_t* cam_obj = NULL;

    CDBG("%s: E camera_idx = %d\n", __func__,camera_idx);
    if (MSM_MAX_CAMERA_SENSORS <= camera_idx) {
        CDBG_ERROR("%s: Invalid camera_idx (%d)", __func__, camera_idx);
        return NULL;
    }

    pthread_mutex_lock(&g_oem_lock);
    pthread_mutex_lock(&g_intf_lock);
    /* opened already */
    if(NULL != g_cam_ctrl.cam_obj[camera_idx]) {
        /* Add reference */
        g_cam_ctrl.cam_obj[camera_idx]->ref_count++;
        pthread_mutex_unlock(&g_intf_lock);
        pthread_mutex_unlock(&g_oem_lock);
        CDBG("%s:  opened alreadyn", __func__);
        return &cam_obj->vtbl;
    }

    cam_obj = (mm_camera_obj_t *)malloc(sizeof(mm_camera_obj_t));
    if(NULL == cam_obj) {
        pthread_mutex_unlock(&g_intf_lock);
        pthread_mutex_unlock(&g_oem_lock);
        CDBG("%s:  no mem", __func__);
        return NULL;
    }

    /* initialize camera obj */
    memset(cam_obj, 0, sizeof(mm_camera_obj_t));
    cam_obj->ctrl_fd = -1;
    cam_obj->ds_fd = -1;
    cam_obj->ref_count++;
    cam_obj->my_hdl = mm_camera_util_generate_handler(camera_idx);
    cam_obj->vtbl.camera_handle = cam_obj->my_hdl; /* set handler */
    cam_obj->vtbl.camera_info = &g_cam_ctrl.camera[camera_idx];
    cam_obj->vtbl.ops = &mm_camera_ops;
    cam_obj->mem_vtbl = mem_vtbl; /* save mem_vtbl */
    pthread_mutex_init(&cam_obj->cam_lock, NULL);

    rc = mm_camera_open(cam_obj);
    if(rc != 0) {
        CDBG_ERROR("%s: mm_camera_open err = %d", __func__, rc);
        pthread_mutex_destroy(&cam_obj->cam_lock);
        g_cam_ctrl.cam_obj[camera_idx] = NULL;
        free(cam_obj);
        cam_obj = NULL;
        pthread_mutex_unlock(&g_intf_lock);
        pthread_mutex_unlock(&g_oem_lock);
        return NULL;
    }else{
        CDBG("%s: Open succeded\n", __func__);
        g_cam_ctrl.cam_obj[camera_idx] = cam_obj;
        pthread_mutex_unlock(&g_intf_lock);
        pthread_mutex_unlock(&g_oem_lock);
        return &cam_obj->vtbl;
    }
}
