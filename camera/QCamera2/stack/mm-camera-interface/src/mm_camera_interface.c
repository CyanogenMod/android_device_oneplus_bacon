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

#include <pthread.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <linux/media.h>
#include <signal.h>
#include <media/msm_cam_sensor.h>
#include <cutils/properties.h>
#include <stdlib.h>

#include "mm_camera_dbg.h"
#include "mm_camera_interface.h"
#include "mm_camera_sock.h"
#include "mm_camera.h"

static pthread_mutex_t g_intf_lock = PTHREAD_MUTEX_INITIALIZER;

static mm_camera_ctrl_t g_cam_ctrl = {0, {{0}}, {0}, {{0}}};

static pthread_mutex_t g_handler_lock = PTHREAD_MUTEX_INITIALIZER;
static uint16_t g_handler_history_count = 0; /* history count for handler */
volatile uint32_t gMmCameraIntfLogLevel = 0;

/*===========================================================================
 * FUNCTION   : mm_camera_util_generate_handler
 *
 * DESCRIPTION: utility function to generate handler for camera/channel/stream
 *
 * PARAMETERS :
 *   @index: index of the object to have handler
 *
 * RETURN     : uint32_t type of handle that uniquely identify the object
 *==========================================================================*/
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

/*===========================================================================
 * FUNCTION   : mm_camera_util_get_index_by_handler
 *
 * DESCRIPTION: utility function to get index from handle
 *
 * PARAMETERS :
 *   @handler: object handle
 *
 * RETURN     : uint8_t type of index derived from handle
 *==========================================================================*/
uint8_t mm_camera_util_get_index_by_handler(uint32_t handler)
{
    return (handler&0x000000ff);
}

/*===========================================================================
 * FUNCTION   : mm_camera_util_get_dev_name
 *
 * DESCRIPTION: utility function to get device name from camera handle
 *
 * PARAMETERS :
 *   @cam_handle: camera handle
 *
 * RETURN     : char ptr to the device name stored in global variable
 * NOTE       : caller should not free the char ptr
 *==========================================================================*/
const char *mm_camera_util_get_dev_name(uint32_t cam_handle)
{
    char *dev_name = NULL;
    uint8_t cam_idx = mm_camera_util_get_index_by_handler(cam_handle);
    if(cam_idx < MM_CAMERA_MAX_NUM_SENSORS) {
        dev_name = g_cam_ctrl.video_dev_name[cam_idx];
    }
    return dev_name;
}

/*===========================================================================
 * FUNCTION   : mm_camera_util_get_camera_by_handler
 *
 * DESCRIPTION: utility function to get camera object from camera handle
 *
 * PARAMETERS :
 *   @cam_handle: camera handle
 *
 * RETURN     : ptr to the camera object stored in global variable
 * NOTE       : caller should not free the camera object ptr
 *==========================================================================*/
mm_camera_obj_t* mm_camera_util_get_camera_by_handler(uint32_t cam_handle)
{
    mm_camera_obj_t *cam_obj = NULL;
    uint8_t cam_idx = mm_camera_util_get_index_by_handler(cam_handle);

    if (cam_idx < MM_CAMERA_MAX_NUM_SENSORS &&
        (NULL != g_cam_ctrl.cam_obj[cam_idx]) &&
        (cam_handle == g_cam_ctrl.cam_obj[cam_idx]->my_hdl)) {
        cam_obj = g_cam_ctrl.cam_obj[cam_idx];
    }
    return cam_obj;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_query_capability
 *
 * DESCRIPTION: query camera capability
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 *==========================================================================*/
static int32_t mm_camera_intf_query_capability(uint32_t camera_handle)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s E: camera_handler = %d ", __func__, camera_handle);

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_query_capability(my_obj);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d", __func__, rc);
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_set_parms
 *
 * DESCRIPTION: set parameters per camera
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @parms        : ptr to a param struct to be set to server
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 * NOTE       : Assume the parms struct buf is already mapped to server via
 *              domain socket. Corresponding fields of parameters to be set
 *              are already filled in by upper layer caller.
 *==========================================================================*/
static int32_t mm_camera_intf_set_parms(uint32_t camera_handle, void *parms)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_set_parms(my_obj, parms);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_get_parms
 *
 * DESCRIPTION: get parameters per camera
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @parms        : ptr to a param struct to be get from server
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 * NOTE       : Assume the parms struct buf is already mapped to server via
 *              domain socket. Parameters to be get from server are already
 *              filled in by upper layer caller. After this call, corresponding
 *              fields of requested parameters will be filled in by server with
 *              detailed information.
 *==========================================================================*/
static int32_t mm_camera_intf_get_parms(uint32_t camera_handle, void *parms)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_get_parms(my_obj, parms);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_do_auto_focus
 *
 * DESCRIPTION: performing auto focus
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 * NOTE       : if this call success, we will always assume there will
 *              be an auto_focus event following up.
 *==========================================================================*/
static int32_t mm_camera_intf_do_auto_focus(uint32_t camera_handle)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_do_auto_focus(my_obj);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_cancel_auto_focus
 *
 * DESCRIPTION: cancel auto focus
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 *==========================================================================*/
static int32_t mm_camera_intf_cancel_auto_focus(uint32_t camera_handle)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_cancel_auto_focus(my_obj);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_prepare_snapshot
 *
 * DESCRIPTION: prepare hardware for snapshot
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @do_af_flag   : flag indicating if AF is needed
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 *==========================================================================*/
static int32_t mm_camera_intf_prepare_snapshot(uint32_t camera_handle,
                                               int32_t do_af_flag)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_prepare_snapshot(my_obj, do_af_flag);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_close
 *
 * DESCRIPTION: close a camera by its handle
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 *==========================================================================*/
static int32_t mm_camera_intf_close(uint32_t camera_handle)
{
    int32_t rc = -1;
    uint8_t cam_idx = camera_handle & 0x00ff;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s E: camera_handler = %d ", __func__, camera_handle);

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if (my_obj){
        my_obj->ref_count--;

        if(my_obj->ref_count > 0) {
            /* still have reference to obj, return here */
            CDBG("%s: ref_count=%d\n", __func__, my_obj->ref_count);
            pthread_mutex_unlock(&g_intf_lock);
            rc = 0;
        } else {
            /* need close camera here as no other reference
             * first empty g_cam_ctrl's referent to cam_obj */
            g_cam_ctrl.cam_obj[cam_idx] = NULL;

            pthread_mutex_lock(&my_obj->cam_lock);
            pthread_mutex_unlock(&g_intf_lock);

            rc = mm_camera_close(my_obj);

            pthread_mutex_destroy(&my_obj->cam_lock);
            free(my_obj);
        }
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_add_channel
 *
 * DESCRIPTION: add a channel
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @attr         : bundle attribute of the channel if needed
 *   @channel_cb   : callback function for bundle data notify
 *   @userdata     : user data ptr
 *
 * RETURN     : uint32_t type of channel handle
 *              0  -- invalid channel handle, meaning the op failed
 *              >0 -- successfully added a channel with a valid handle
 * NOTE       : if no bundle data notify is needed, meaning each stream in the
 *              channel will have its own stream data notify callback, then
 *              attr, channel_cb, and userdata can be NULL. In this case,
 *              no matching logic will be performed in channel for the bundling.
 *==========================================================================*/
static uint32_t mm_camera_intf_add_channel(uint32_t camera_handle,
                                           mm_camera_channel_attr_t *attr,
                                           mm_camera_buf_notify_t channel_cb,
                                           void *userdata)
{
    uint32_t ch_id = 0;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E camera_handler = %d", __func__, camera_handle);
    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        ch_id = mm_camera_add_channel(my_obj, attr, channel_cb, userdata);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X ch_id = %d", __func__, ch_id);
    return ch_id;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_del_channel
 *
 * DESCRIPTION: delete a channel by its handle
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @ch_id        : channel handle
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 * NOTE       : all streams in the channel should be stopped already before
 *              this channel can be deleted.
 *==========================================================================*/
static int32_t mm_camera_intf_del_channel(uint32_t camera_handle,
                                          uint32_t ch_id)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E ch_id = %d", __func__, ch_id);
    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_del_channel(my_obj, ch_id);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X", __func__);
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_get_bundle_info
 *
 * DESCRIPTION: query bundle info of the channel
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @ch_id        : channel handle
 *   @bundle_info  : bundle info to be filled in
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 * NOTE       : all streams in the channel should be stopped already before
 *              this channel can be deleted.
 *==========================================================================*/
static int32_t mm_camera_intf_get_bundle_info(uint32_t camera_handle,
                                              uint32_t ch_id,
                                              cam_bundle_config_t *bundle_info)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E ch_id = %d", __func__, ch_id);
    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_get_bundle_info(my_obj, ch_id, bundle_info);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X", __func__);
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_register_event_notify
 *
 * DESCRIPTION: register for event notify
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @evt_cb       : callback for event notify
 *   @user_data    : user data ptr
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 *==========================================================================*/
static int32_t mm_camera_intf_register_event_notify(uint32_t camera_handle,
                                                    mm_camera_event_notify_t evt_cb,
                                                    void * user_data)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E ", __func__);
    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_register_event_notify(my_obj, evt_cb, user_data);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :E rc = %d", __func__, rc);
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_qbuf
 *
 * DESCRIPTION: enqueue buffer back to kernel
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @ch_id        : channel handle
 *   @buf          : buf ptr to be enqueued
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 *==========================================================================*/
static int32_t mm_camera_intf_qbuf(uint32_t camera_handle,
                                    uint32_t ch_id,
                                    mm_camera_buf_def_t *buf)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

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

/*===========================================================================
 * FUNCTION   : mm_camera_intf_get_queued_buf_count
 *
 * DESCRIPTION: returns the queued buffer count
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @ch_id        : channel handle
 *   @stream_id : stream id
 *
 * RETURN     : int32_t - queued buffer count
 *
 *==========================================================================*/
static int32_t mm_camera_intf_get_queued_buf_count(uint32_t camera_handle,
        uint32_t ch_id, uint32_t stream_id)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_get_queued_buf_count(my_obj, ch_id, stream_id);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X queued buffer count = %d",__func__,rc);
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_link_stream
 *
 * DESCRIPTION: link a stream into a new channel
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @ch_id        : channel handle
 *   @stream_id    : stream id
 *   @linked_ch_id : channel in which the stream will be linked
 *
 * RETURN     : uint32_t type of stream handle
 *              0  -- invalid stream handle, meaning the op failed
 *              >0 -- successfully linked a stream with a valid handle
 *==========================================================================*/
static int32_t mm_camera_intf_link_stream(uint32_t camera_handle,
        uint32_t ch_id,
        uint32_t stream_id,
        uint32_t linked_ch_id)
{
    int32_t id = 0;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s : E handle = %d ch_id = %d",
         __func__, camera_handle, ch_id);

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        id = (int32_t)mm_camera_link_stream(my_obj, ch_id, stream_id, linked_ch_id);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }

    CDBG("%s :X stream_id = %d", __func__, stream_id);
    return id;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_add_stream
 *
 * DESCRIPTION: add a stream into a channel
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @ch_id        : channel handle
 *
 * RETURN     : uint32_t type of stream handle
 *              0  -- invalid stream handle, meaning the op failed
 *              >0 -- successfully added a stream with a valid handle
 *==========================================================================*/
static uint32_t mm_camera_intf_add_stream(uint32_t camera_handle,
                                          uint32_t ch_id)
{
    uint32_t stream_id = 0;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s : E handle = %d ch_id = %d",
         __func__, camera_handle, ch_id);

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        stream_id = mm_camera_add_stream(my_obj, ch_id);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X stream_id = %d", __func__, stream_id);
    return stream_id;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_del_stream
 *
 * DESCRIPTION: delete a stream by its handle
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @ch_id        : channel handle
 *   @stream_id    : stream handle
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 * NOTE       : stream should be stopped already before it can be deleted.
 *==========================================================================*/
static int32_t mm_camera_intf_del_stream(uint32_t camera_handle,
                                         uint32_t ch_id,
                                         uint32_t stream_id)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s : E handle = %d ch_id = %d stream_id = %d",
         __func__, camera_handle, ch_id, stream_id);

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_del_stream(my_obj, ch_id, stream_id);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d", __func__, rc);
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_config_stream
 *
 * DESCRIPTION: configure a stream
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @ch_id        : channel handle
 *   @stream_id    : stream handle
 *   @config       : stream configuration
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 *==========================================================================*/
static int32_t mm_camera_intf_config_stream(uint32_t camera_handle,
                                            uint32_t ch_id,
                                            uint32_t stream_id,
                                            mm_camera_stream_config_t *config)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E handle = %d, ch_id = %d,stream_id = %d",
         __func__, camera_handle, ch_id, stream_id);

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    CDBG("%s :mm_camera_intf_config_stream stream_id = %d",__func__,stream_id);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_config_stream(my_obj, ch_id, stream_id, config);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d", __func__, rc);
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_start_channel
 *
 * DESCRIPTION: start a channel, which will start all streams in the channel
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @ch_id        : channel handle
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 *==========================================================================*/
static int32_t mm_camera_intf_start_channel(uint32_t camera_handle,
                                            uint32_t ch_id)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_start_channel(my_obj, ch_id);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d", __func__, rc);
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_stop_channel
 *
 * DESCRIPTION: stop a channel, which will stop all streams in the channel
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @ch_id        : channel handle
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 *==========================================================================*/
static int32_t mm_camera_intf_stop_channel(uint32_t camera_handle,
                                           uint32_t ch_id)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_stop_channel(my_obj, ch_id);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d", __func__, rc);
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_request_super_buf
 *
 * DESCRIPTION: for burst mode in bundle, reuqest certain amount of matched
 *              frames from superbuf queue
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @ch_id        : channel handle
 *   @num_buf_requested : number of matched frames needed
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 *==========================================================================*/
static int32_t mm_camera_intf_request_super_buf(uint32_t camera_handle,
                                                uint32_t ch_id,
                                                uint32_t num_buf_requested)
{
    int32_t rc = -1;
    CDBG("%s :E camera_handler = %d,ch_id = %d",
         __func__, camera_handle, ch_id);
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_request_super_buf(my_obj, ch_id, num_buf_requested);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d", __func__, rc);
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_cancel_super_buf_request
 *
 * DESCRIPTION: for burst mode in bundle, cancel the reuqest for certain amount
 *              of matched frames from superbuf queue
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @ch_id        : channel handle
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 *==========================================================================*/
static int32_t mm_camera_intf_cancel_super_buf_request(uint32_t camera_handle,
                                                       uint32_t ch_id)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E camera_handler = %d,ch_id = %d",
         __func__, camera_handle, ch_id);
    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_cancel_super_buf_request(my_obj, ch_id);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d", __func__, rc);
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_flush_super_buf_queue
 *
 * DESCRIPTION: flush out all frames in the superbuf queue
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @ch_id        : channel handle
 *   @frame_idx    : frame index
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 *==========================================================================*/
static int32_t mm_camera_intf_flush_super_buf_queue(uint32_t camera_handle,
                                                    uint32_t ch_id, uint32_t frame_idx)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E camera_handler = %d,ch_id = %d",
         __func__, camera_handle, ch_id);
    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_flush_super_buf_queue(my_obj, ch_id, frame_idx);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d", __func__, rc);
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_start_zsl_snapshot
 *
 * DESCRIPTION: Starts zsl snapshot
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @ch_id        : channel handle
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 *==========================================================================*/
static int32_t mm_camera_intf_start_zsl_snapshot(uint32_t camera_handle,
        uint32_t ch_id)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E camera_handler = %d,ch_id = %d",
         __func__, camera_handle, ch_id);
    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_start_zsl_snapshot_ch(my_obj, ch_id);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d", __func__, rc);
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_stop_zsl_snapshot
 *
 * DESCRIPTION: Stops zsl snapshot
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @ch_id        : channel handle
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 *==========================================================================*/
static int32_t mm_camera_intf_stop_zsl_snapshot(uint32_t camera_handle,
        uint32_t ch_id)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E camera_handler = %d,ch_id = %d",
         __func__, camera_handle, ch_id);
    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_stop_zsl_snapshot_ch(my_obj, ch_id);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d", __func__, rc);
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_configure_notify_mode
 *
 * DESCRIPTION: Configures channel notification mode
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @ch_id        : channel handle
 *   @notify_mode  : notification mode
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 *==========================================================================*/
static int32_t mm_camera_intf_configure_notify_mode(uint32_t camera_handle,
                                                    uint32_t ch_id,
                                                    mm_camera_super_buf_notify_mode_t notify_mode)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s :E camera_handler = %d,ch_id = %d",
         __func__, camera_handle, ch_id);
    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_config_channel_notify(my_obj, ch_id, notify_mode);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d", __func__, rc);
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_map_buf
 *
 * DESCRIPTION: mapping camera buffer via domain socket to server
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @buf_type     : type of buffer to be mapped. could be following values:
 *                   CAM_MAPPING_BUF_TYPE_CAPABILITY
 *                   CAM_MAPPING_BUF_TYPE_SETPARM_BUF
 *                   CAM_MAPPING_BUF_TYPE_GETPARM_BUF
 *   @fd           : file descriptor of the buffer
 *   @size         : size of the buffer
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 *==========================================================================*/
static int32_t mm_camera_intf_map_buf(uint32_t camera_handle,
                                      uint8_t buf_type,
                                      int fd,
                                      size_t size)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_map_buf(my_obj, buf_type, fd, size);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_unmap_buf
 *
 * DESCRIPTION: unmapping camera buffer via domain socket to server
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @buf_type     : type of buffer to be unmapped. could be following values:
 *                   CAM_MAPPING_BUF_TYPE_CAPABILITY
 *                   CAM_MAPPING_BUF_TYPE_SETPARM_BUF
 *                   CAM_MAPPING_BUF_TYPE_GETPARM_BUF
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 *==========================================================================*/
static int32_t mm_camera_intf_unmap_buf(uint32_t camera_handle,
                                        uint8_t buf_type)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_unmap_buf(my_obj, buf_type);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_set_stream_parms
 *
 * DESCRIPTION: set parameters per stream
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @ch_id        : channel handle
 *   @s_id         : stream handle
 *   @parms        : ptr to a param struct to be set to server
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 * NOTE       : Assume the parms struct buf is already mapped to server via
 *              domain socket. Corresponding fields of parameters to be set
 *              are already filled in by upper layer caller.
 *==========================================================================*/
static int32_t mm_camera_intf_set_stream_parms(uint32_t camera_handle,
                                               uint32_t ch_id,
                                               uint32_t s_id,
                                               cam_stream_parm_buffer_t *parms)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    CDBG("%s :E camera_handle = %d,ch_id = %d,s_id = %d",
         __func__, camera_handle, ch_id, s_id);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_set_stream_parms(my_obj, ch_id, s_id, parms);
    }else{
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s :X rc = %d", __func__, rc);
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_get_stream_parms
 *
 * DESCRIPTION: get parameters per stream
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @ch_id        : channel handle
 *   @s_id         : stream handle
 *   @parms        : ptr to a param struct to be get from server
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 * NOTE       : Assume the parms struct buf is already mapped to server via
 *              domain socket. Parameters to be get from server are already
 *              filled in by upper layer caller. After this call, corresponding
 *              fields of requested parameters will be filled in by server with
 *              detailed information.
 *==========================================================================*/
static int32_t mm_camera_intf_get_stream_parms(uint32_t camera_handle,
                                               uint32_t ch_id,
                                               uint32_t s_id,
                                               cam_stream_parm_buffer_t *parms)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    CDBG("%s :E camera_handle = %d,ch_id = %d,s_id = %d",
         __func__, camera_handle, ch_id, s_id);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_get_stream_parms(my_obj, ch_id, s_id, parms);
    }else{
        pthread_mutex_unlock(&g_intf_lock);
    }

    CDBG("%s :X rc = %d", __func__, rc);
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_map_stream_buf
 *
 * DESCRIPTION: mapping stream buffer via domain socket to server
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @ch_id        : channel handle
 *   @s_id         : stream handle
 *   @buf_type     : type of buffer to be mapped. could be following values:
 *                   CAM_MAPPING_BUF_TYPE_STREAM_BUF
 *                   CAM_MAPPING_BUF_TYPE_STREAM_INFO
 *                   CAM_MAPPING_BUF_TYPE_OFFLINE_INPUT_BUF
 *   @buf_idx      : index of buffer within the stream buffers, only valid if
 *                   buf_type is CAM_MAPPING_BUF_TYPE_STREAM_BUF or
 *                   CAM_MAPPING_BUF_TYPE_OFFLINE_INPUT_BUF
 *   @plane_idx    : plane index. If all planes share the same fd,
 *                   plane_idx = -1; otherwise, plean_idx is the
 *                   index to plane (0..num_of_planes)
 *   @fd           : file descriptor of the buffer
 *   @size         : size of the buffer
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 *==========================================================================*/
static int32_t mm_camera_intf_map_stream_buf(uint32_t camera_handle,
                                             uint32_t ch_id,
                                             uint32_t stream_id,
                                             uint8_t buf_type,
                                             uint32_t buf_idx,
                                             int32_t plane_idx,
                                             int fd,
                                             size_t size)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    CDBG("%s :E camera_handle = %d, ch_id = %d, s_id = %d, buf_idx = %d, plane_idx = %d",
         __func__, camera_handle, ch_id, stream_id, buf_idx, plane_idx);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_map_stream_buf(my_obj, ch_id, stream_id,
                                      buf_type, buf_idx, plane_idx,
                                      fd, size);
    }else{
        pthread_mutex_unlock(&g_intf_lock);
    }

    CDBG("%s :X rc = %d", __func__, rc);
    return rc;
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_unmap_stream_buf
 *
 * DESCRIPTION: unmapping stream buffer via domain socket to server
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @ch_id        : channel handle
 *   @s_id         : stream handle
 *   @buf_type     : type of buffer to be unmapped. could be following values:
 *                   CAM_MAPPING_BUF_TYPE_STREAM_BUF
 *                   CAM_MAPPING_BUF_TYPE_STREAM_INFO
 *                   CAM_MAPPING_BUF_TYPE_OFFLINE_INPUT_BUF
 *   @buf_idx      : index of buffer within the stream buffers, only valid if
 *                   buf_type is CAM_MAPPING_BUF_TYPE_STREAM_BUF or
 *                   CAM_MAPPING_BUF_TYPE_OFFLINE_INPUT_BUF
 *   @plane_idx    : plane index. If all planes share the same fd,
 *                   plane_idx = -1; otherwise, plean_idx is the
 *                   index to plane (0..num_of_planes)
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 *==========================================================================*/
static int32_t mm_camera_intf_unmap_stream_buf(uint32_t camera_handle,
                                               uint32_t ch_id,
                                               uint32_t stream_id,
                                               uint8_t buf_type,
                                               uint32_t buf_idx,
                                               int32_t plane_idx)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    CDBG("%s :E camera_handle = %d, ch_id = %d, s_id = %d, buf_idx = %d, plane_idx = %d",
         __func__, camera_handle, ch_id, stream_id, buf_idx, plane_idx);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_unmap_stream_buf(my_obj, ch_id, stream_id,
                                        buf_type, buf_idx, plane_idx);
    }else{
        pthread_mutex_unlock(&g_intf_lock);
    }

    CDBG("%s :X rc = %d", __func__, rc);
    return rc;
}

/*===========================================================================
 * FUNCTION   : get_sensor_info
 *
 * DESCRIPTION: get sensor info like facing(back/front) and mount angle
 *
 * PARAMETERS :
 *
 * RETURN     :
 *==========================================================================*/
void get_sensor_info()
{
    int rc = 0;
    int dev_fd = 0;
    struct media_device_info mdev_info;
    int num_media_devices = 0;
    size_t num_cameras = 0;

    CDBG("%s : E", __func__);
    /* lock the mutex */
    while (1) {
        char dev_name[32];
        snprintf(dev_name, sizeof(dev_name), "/dev/media%d", num_media_devices);
        dev_fd = open(dev_name, O_RDWR | O_NONBLOCK);
        if (dev_fd <= 0) {
            CDBG("Done discovering media devices\n");
            break;
        }
        num_media_devices++;
        memset(&mdev_info, 0, sizeof(mdev_info));
        rc = ioctl(dev_fd, MEDIA_IOC_DEVICE_INFO, &mdev_info);
        if (rc < 0) {
            CDBG_ERROR("Error: ioctl media_dev failed: %s\n", strerror(errno));
            close(dev_fd);
            dev_fd = 0;
            num_cameras = 0;
            break;
        }

        if(strncmp(mdev_info.model,  MSM_CONFIGURATION_NAME, sizeof(mdev_info.model)) != 0) {
            close(dev_fd);
            dev_fd = 0;
            continue;
        }

        unsigned int num_entities = 1;
        while (1) {
            struct media_entity_desc entity;
            uint32_t temp;
            uint32_t mount_angle;
            uint32_t facing;

            memset(&entity, 0, sizeof(entity));
            entity.id = num_entities++;
            rc = ioctl(dev_fd, MEDIA_IOC_ENUM_ENTITIES, &entity);
            if (rc < 0) {
                CDBG("Done enumerating media entities\n");
                rc = 0;
                break;
            }
            if(entity.type == MEDIA_ENT_T_V4L2_SUBDEV &&
                entity.group_id == MSM_CAMERA_SUBDEV_SENSOR) {
                temp = entity.flags >> 8;
                mount_angle = (temp & 0xFF) * 90;
                facing = (temp >> 8);
                ALOGD("index = %u flag = %x mount_angle = %u facing = %u\n",
                    (unsigned int)num_cameras, (unsigned int)temp,
                    (unsigned int)mount_angle, (unsigned int)facing);
                g_cam_ctrl.info[num_cameras].facing = (int)facing;
                g_cam_ctrl.info[num_cameras].orientation = (int)mount_angle;
                num_cameras++;
                continue;
            }
        }

        CDBG("%s: dev_info[id=%zu,name='%s']\n",
            __func__, num_cameras, g_cam_ctrl.video_dev_name[num_cameras]);

        close(dev_fd);
        dev_fd = 0;
    }

    /* unlock the mutex */
    CDBG("%s: num_cameras=%d\n", __func__, g_cam_ctrl.num_cam);
    return;
}

/*===========================================================================
 * FUNCTION   : sort_camera_info
 *
 * DESCRIPTION: sort camera info to keep back cameras idx is smaller than front cameras idx
 *
 * PARAMETERS : number of cameras
 *
 * RETURN     :
 *==========================================================================*/
void sort_camera_info(int num_cam)
{
    int idx = 0, i;
    struct camera_info temp_info[MM_CAMERA_MAX_NUM_SENSORS];
    memset(temp_info, 0, sizeof(temp_info));

    /* firstly save the back cameras info*/
    for (i = 0; i < num_cam; i++) {
        if (g_cam_ctrl.info[i].facing == CAMERA_FACING_BACK) {
            temp_info[idx++] = g_cam_ctrl.info[i];
        }
    }

    /* then save the front cameras info*/
    for (i = 0; i < num_cam; i++) {
        if (g_cam_ctrl.info[i].facing == CAMERA_FACING_FRONT) {
            temp_info[idx++] = g_cam_ctrl.info[i];
        }
    }

    memcpy(g_cam_ctrl.info, temp_info, sizeof(temp_info));
    return;
}

/*===========================================================================
 * FUNCTION   : get_num_of_cameras
 *
 * DESCRIPTION: get number of cameras
 *
 * PARAMETERS :
 *
 * RETURN     : number of cameras supported
 *==========================================================================*/
uint8_t get_num_of_cameras()
{
    int rc = 0;
    int dev_fd = 0;
    struct media_device_info mdev_info;
    int num_media_devices = 0;
    int8_t num_cameras = 0;
    char subdev_name[32];
    int32_t sd_fd = 0;
    struct sensor_init_cfg_data cfg;
    char prop[PROPERTY_VALUE_MAX];
    uint32_t temp;
    uint32_t log_level;
    uint32_t debug_mask;

    /*  Higher 4 bits : Value of Debug log level (Default level is 1 to print all CDBG_HIGH)
        Lower 28 bits : Control mode for sub module logging(Only 3 sub modules in HAL)
                        0x1 for HAL
                        0x10 for mm-camera-interface
                        0x100 for mm-jpeg-interface  */
    property_get("persist.camera.hal.debug.mask", prop, "268435463"); // 0x10000007=268435463
    temp = (uint32_t) atoi(prop);
    log_level = ((temp >> 28) & 0xF);
    debug_mask = (temp & HAL_DEBUG_MASK_MM_CAMERA_INTERFACE);
    if (debug_mask > 0)
        gMmCameraIntfLogLevel = log_level;
    else
        gMmCameraIntfLogLevel = 0; // Debug logs are not required if debug_mask is zero

    CDBG_HIGH("%s gMmCameraIntfLogLevel=%d",__func__, gMmCameraIntfLogLevel);

    property_get("vold.decrypt", prop, "0");
    int decrypt = atoi(prop);
    if (decrypt == 1)
     return 0;

    /* lock the mutex */
    pthread_mutex_lock(&g_intf_lock);

    while (1) {
        uint32_t num_entities = 1U;
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
            dev_fd = 0;
            break;
        }

        if (strncmp(mdev_info.model, "msm_config", sizeof(mdev_info.model) != 0)) {
            close(dev_fd);
            dev_fd = 0;
            continue;
        }

        while (1) {
            struct media_entity_desc entity;
            memset(&entity, 0, sizeof(entity));
            entity.id = num_entities++;
            CDBG_ERROR("entity id %d", entity.id);
            rc = ioctl(dev_fd, MEDIA_IOC_ENUM_ENTITIES, &entity);
            if (rc < 0) {
                CDBG_ERROR("Done enumerating media entities");
                rc = 0;
                break;
            }
            CDBG_ERROR("entity name %s type %d group id %d",
                entity.name, entity.type, entity.group_id);
            if (entity.type == MEDIA_ENT_T_V4L2_SUBDEV &&
                entity.group_id == MSM_CAMERA_SUBDEV_SENSOR_INIT) {
                snprintf(subdev_name, sizeof(dev_name), "/dev/%s", entity.name);
                break;
            }
        }
        close(dev_fd);
        dev_fd = 0;
    }

    /* Open sensor_init subdev */
    sd_fd = open(subdev_name, O_RDWR);
    if (sd_fd < 0) {
        CDBG_ERROR("Open sensor_init subdev failed");
        return FALSE;
    }

    cfg.cfgtype = CFG_SINIT_PROBE_WAIT_DONE;
    cfg.cfg.setting = NULL;
    if (ioctl(sd_fd, VIDIOC_MSM_SENSOR_INIT_CFG, &cfg) < 0) {
        CDBG_ERROR("failed");
    }
    close(sd_fd);
    dev_fd = 0;


    num_media_devices = 0;
    while (1) {
        uint32_t num_entities = 1U;
        char dev_name[32];

        snprintf(dev_name, sizeof(dev_name), "/dev/media%d", num_media_devices);
        dev_fd = open(dev_name, O_RDWR | O_NONBLOCK);
        if (dev_fd <= 0) {
            CDBG("Done discovering media devices\n");
            break;
        }
        num_media_devices++;
        memset(&mdev_info, 0, sizeof(mdev_info));
        rc = ioctl(dev_fd, MEDIA_IOC_DEVICE_INFO, &mdev_info);
        if (rc < 0) {
            CDBG_ERROR("Error: ioctl media_dev failed: %s\n", strerror(errno));
            close(dev_fd);
            dev_fd = 0;
            num_cameras = 0;
            break;
        }

        if(strncmp(mdev_info.model, MSM_CAMERA_NAME, sizeof(mdev_info.model)) != 0) {
            close(dev_fd);
            dev_fd = 0;
            continue;
        }

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
                strncpy(g_cam_ctrl.video_dev_name[num_cameras],
                     entity.name, sizeof(entity.name));
                break;
            }
        }

        CDBG("%s: dev_info[id=%d,name='%s']\n",
            __func__, (int)num_cameras, g_cam_ctrl.video_dev_name[num_cameras]);

        num_cameras++;
        close(dev_fd);
        dev_fd = 0;
    }
    g_cam_ctrl.num_cam = num_cameras;

    get_sensor_info();
    sort_camera_info(g_cam_ctrl.num_cam);
    /* unlock the mutex */
    pthread_mutex_unlock(&g_intf_lock);
    CDBG("%s: num_cameras=%d\n", __func__, (int)g_cam_ctrl.num_cam);
    return(uint8_t)g_cam_ctrl.num_cam;
}

struct camera_info *get_cam_info(uint32_t camera_id)
{
    return &g_cam_ctrl.info[camera_id];
}

/*===========================================================================
 * FUNCTION   : mm_camera_intf_process_advanced_capture
 *
 * DESCRIPTION: Configures channel advanced capture mode
 *
 * PARAMETERS :
 *   @camera_handle: camera handle
 *   @advanced_capture_type : advanced capture type
 *   @ch_id        : channel handle
 *   @notify_mode  : notification mode
 *
 * RETURN     : int32_t type of status
 *              0  -- success
 *              -1 -- failure
 *==========================================================================*/
static int32_t mm_camera_intf_process_advanced_capture(uint32_t camera_handle,
    mm_camera_advanced_capture_t advanced_capture_type,
    uint32_t ch_id,
    int8_t start_flag)
{
    int32_t rc = -1;
    mm_camera_obj_t * my_obj = NULL;

    CDBG("%s: E camera_handler = %d,ch_id = %d",
         __func__, camera_handle, ch_id);
    pthread_mutex_lock(&g_intf_lock);
    my_obj = mm_camera_util_get_camera_by_handler(camera_handle);

    if(my_obj) {
        pthread_mutex_lock(&my_obj->cam_lock);
        pthread_mutex_unlock(&g_intf_lock);
        rc = mm_camera_channel_advanced_capture(my_obj, advanced_capture_type,
            ch_id, (uint32_t)start_flag);
    } else {
        pthread_mutex_unlock(&g_intf_lock);
    }
    CDBG("%s: X ", __func__);
    return rc;
}

/* camera ops v-table */
static mm_camera_ops_t mm_camera_ops = {
    .query_capability = mm_camera_intf_query_capability,
    .register_event_notify = mm_camera_intf_register_event_notify,
    .close_camera = mm_camera_intf_close,
    .set_parms = mm_camera_intf_set_parms,
    .get_parms = mm_camera_intf_get_parms,
    .do_auto_focus = mm_camera_intf_do_auto_focus,
    .cancel_auto_focus = mm_camera_intf_cancel_auto_focus,
    .prepare_snapshot = mm_camera_intf_prepare_snapshot,
    .start_zsl_snapshot = mm_camera_intf_start_zsl_snapshot,
    .stop_zsl_snapshot = mm_camera_intf_stop_zsl_snapshot,
    .map_buf = mm_camera_intf_map_buf,
    .unmap_buf = mm_camera_intf_unmap_buf,
    .add_channel = mm_camera_intf_add_channel,
    .delete_channel = mm_camera_intf_del_channel,
    .get_bundle_info = mm_camera_intf_get_bundle_info,
    .add_stream = mm_camera_intf_add_stream,
    .link_stream = mm_camera_intf_link_stream,
    .delete_stream = mm_camera_intf_del_stream,
    .config_stream = mm_camera_intf_config_stream,
    .qbuf = mm_camera_intf_qbuf,
    .get_queued_buf_count = mm_camera_intf_get_queued_buf_count,
    .map_stream_buf = mm_camera_intf_map_stream_buf,
    .unmap_stream_buf = mm_camera_intf_unmap_stream_buf,
    .set_stream_parms = mm_camera_intf_set_stream_parms,
    .get_stream_parms = mm_camera_intf_get_stream_parms,
    .start_channel = mm_camera_intf_start_channel,
    .stop_channel = mm_camera_intf_stop_channel,
    .request_super_buf = mm_camera_intf_request_super_buf,
    .cancel_super_buf_request = mm_camera_intf_cancel_super_buf_request,
    .flush_super_buf_queue = mm_camera_intf_flush_super_buf_queue,
    .configure_notify_mode = mm_camera_intf_configure_notify_mode,
    .process_advanced_capture = mm_camera_intf_process_advanced_capture
};

/*===========================================================================
 * FUNCTION   : camera_open
 *
 * DESCRIPTION: open a camera by camera index
 *
 * PARAMETERS :
 *   @camera_idx : camera index. should within range of 0 to num_of_cameras
 *
 * RETURN     : ptr to a virtual table containing camera handle and operation table.
 *              NULL if failed.
 *==========================================================================*/
mm_camera_vtbl_t * camera_open(uint8_t camera_idx)
{
    int32_t rc = 0;
    mm_camera_obj_t* cam_obj = NULL;

    CDBG("%s: E camera_idx = %d\n", __func__, camera_idx);
    if (camera_idx >= g_cam_ctrl.num_cam) {
        CDBG_ERROR("%s: Invalid camera_idx (%d)", __func__, camera_idx);
        return NULL;
    }

    pthread_mutex_lock(&g_intf_lock);
    /* opened already */
    if(NULL != g_cam_ctrl.cam_obj[camera_idx]) {
        /* Add reference */
        g_cam_ctrl.cam_obj[camera_idx]->ref_count++;
        pthread_mutex_unlock(&g_intf_lock);
        CDBG("%s:  opened alreadyn", __func__);
        return &g_cam_ctrl.cam_obj[camera_idx]->vtbl;
    }

    cam_obj = (mm_camera_obj_t *)malloc(sizeof(mm_camera_obj_t));
    if(NULL == cam_obj) {
        pthread_mutex_unlock(&g_intf_lock);
        CDBG("%s:  no mem", __func__);
        return NULL;
    }

    /* initialize camera obj */
    memset(cam_obj, 0, sizeof(mm_camera_obj_t));
    cam_obj->ref_count++;
    cam_obj->my_hdl = mm_camera_util_generate_handler(camera_idx);
    cam_obj->vtbl.camera_handle = cam_obj->my_hdl; /* set handler */
    cam_obj->vtbl.ops = &mm_camera_ops;
    pthread_mutex_init(&cam_obj->cam_lock, NULL);
    /* unlock global interface lock, if not, in dual camera use case,
      * current open will block operation of another opened camera obj*/
    pthread_mutex_lock(&cam_obj->cam_lock);
    pthread_mutex_unlock(&g_intf_lock);

    rc = mm_camera_open(cam_obj);

    pthread_mutex_lock(&g_intf_lock);
    if(rc != 0) {
        CDBG_ERROR("%s: mm_camera_open err = %d", __func__, rc);
        pthread_mutex_destroy(&cam_obj->cam_lock);
        g_cam_ctrl.cam_obj[camera_idx] = NULL;
        free(cam_obj);
        cam_obj = NULL;
        pthread_mutex_unlock(&g_intf_lock);
        return NULL;
    }else{
        CDBG("%s: Open succeded\n", __func__);
        g_cam_ctrl.cam_obj[camera_idx] = cam_obj;
        pthread_mutex_unlock(&g_intf_lock);
        return &cam_obj->vtbl;
    }
}
