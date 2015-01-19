/*
Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.

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

#include "mm_qcamera_dbg.h"
#include "mm_qcamera_app.h"

static void mm_app_video_notify_cb(mm_camera_super_buf_t *bufs,
                                   void *user_data)
{
    char file_name[64];
    mm_camera_buf_def_t *frame = bufs->bufs[0];
    mm_camera_test_obj_t *pme = (mm_camera_test_obj_t *)user_data;

    CDBG("%s: BEGIN - length=%zu, frame idx = %d\n",
         __func__, frame->frame_len, frame->frame_idx);
    snprintf(file_name, sizeof(file_name), "V_C%d", pme->cam->camera_handle);
    mm_app_dump_frame(frame, file_name, "yuv", frame->frame_idx);

    if (MM_CAMERA_OK != pme->cam->ops->qbuf(bufs->camera_handle,
                                            bufs->ch_id,
                                            frame)) {
        CDBG_ERROR("%s: Failed in Preview Qbuf\n", __func__);
    }
    mm_app_cache_ops((mm_camera_app_meminfo_t *)frame->mem_info,
                     ION_IOC_INV_CACHES);

    CDBG("%s: END\n", __func__);
}

mm_camera_stream_t * mm_app_add_video_stream(mm_camera_test_obj_t *test_obj,
                                             mm_camera_channel_t *channel,
                                             mm_camera_buf_notify_t stream_cb,
                                             void *userdata,
                                             uint8_t num_bufs)
{
    int rc = MM_CAMERA_OK;
    mm_camera_stream_t *stream = NULL;
    cam_capability_t *cam_cap = (cam_capability_t *)(test_obj->cap_buf.buf.buffer);

    stream = mm_app_add_stream(test_obj, channel);
    if (NULL == stream) {
        CDBG_ERROR("%s: add stream failed\n", __func__);
        return NULL;
    }

    stream->s_config.mem_vtbl.get_bufs = mm_app_stream_initbuf;
    stream->s_config.mem_vtbl.put_bufs = mm_app_stream_deinitbuf;
    stream->s_config.mem_vtbl.clean_invalidate_buf =
      mm_app_stream_clean_invalidate_buf;
    stream->s_config.mem_vtbl.invalidate_buf = mm_app_stream_invalidate_buf;
    stream->s_config.mem_vtbl.user_data = (void *)stream;
    stream->s_config.stream_cb = stream_cb;
    stream->s_config.userdata = userdata;
    stream->num_of_bufs = num_bufs;

    stream->s_config.stream_info = (cam_stream_info_t *)stream->s_info_buf.buf.buffer;
    memset(stream->s_config.stream_info, 0, sizeof(cam_stream_info_t));
    stream->s_config.stream_info->stream_type = CAM_STREAM_TYPE_VIDEO;
    stream->s_config.stream_info->streaming_mode = CAM_STREAMING_MODE_CONTINUOUS;
    stream->s_config.stream_info->fmt = DEFAULT_VIDEO_FORMAT;
    stream->s_config.stream_info->dim.width = DEFAULT_VIDEO_WIDTH;
    stream->s_config.stream_info->dim.height = DEFAULT_VIDEO_HEIGHT;
    stream->s_config.padding_info = cam_cap->padding_info;

    rc = mm_app_config_stream(test_obj, channel, stream, &stream->s_config);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:config preview stream err=%d\n", __func__, rc);
        return NULL;
    }

    return stream;
}

mm_camera_channel_t * mm_app_add_video_channel(mm_camera_test_obj_t *test_obj)
{
    mm_camera_channel_t *channel = NULL;
    mm_camera_stream_t *stream = NULL;

    channel = mm_app_add_channel(test_obj,
                                 MM_CHANNEL_TYPE_VIDEO,
                                 NULL,
                                 NULL,
                                 NULL);
    if (NULL == channel) {
        CDBG_ERROR("%s: add channel failed", __func__);
        return NULL;
    }

    stream = mm_app_add_video_stream(test_obj,
                                     channel,
                                     mm_app_video_notify_cb,
                                     (void *)test_obj,
                                     1);
    if (NULL == stream) {
        CDBG_ERROR("%s: add video stream failed\n", __func__);
        mm_app_del_channel(test_obj, channel);
        return NULL;
    }

    return channel;
}

int mm_app_start_record_preview(mm_camera_test_obj_t *test_obj)
{
    int rc = MM_CAMERA_OK;
    mm_camera_channel_t *p_ch = NULL;
    mm_camera_channel_t *v_ch = NULL;
    mm_camera_channel_t *s_ch = NULL;

    p_ch = mm_app_add_preview_channel(test_obj);
    if (NULL == p_ch) {
        CDBG_ERROR("%s: add preview channel failed", __func__);
        return -MM_CAMERA_E_GENERAL;
    }

    v_ch = mm_app_add_video_channel(test_obj);
    if (NULL == v_ch) {
        CDBG_ERROR("%s: add video channel failed", __func__);
        mm_app_del_channel(test_obj, p_ch);
        return -MM_CAMERA_E_GENERAL;
    }

    s_ch = mm_app_add_snapshot_channel(test_obj);
    if (NULL == s_ch) {
        CDBG_ERROR("%s: add snapshot channel failed", __func__);
        mm_app_del_channel(test_obj, p_ch);
        mm_app_del_channel(test_obj, v_ch);
        return -MM_CAMERA_E_GENERAL;
    }

    rc = mm_app_start_channel(test_obj, p_ch);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:start preview failed rc=%d\n", __func__, rc);
        mm_app_del_channel(test_obj, p_ch);
        mm_app_del_channel(test_obj, v_ch);
        mm_app_del_channel(test_obj, s_ch);
        return rc;
    }

    return rc;
}

int mm_app_stop_record_preview(mm_camera_test_obj_t *test_obj)
{
    int rc = MM_CAMERA_OK;
    mm_camera_channel_t *p_ch = NULL;
    mm_camera_channel_t *v_ch = NULL;
    mm_camera_channel_t *s_ch = NULL;

    p_ch = mm_app_get_channel_by_type(test_obj, MM_CHANNEL_TYPE_PREVIEW);
    v_ch = mm_app_get_channel_by_type(test_obj, MM_CHANNEL_TYPE_VIDEO);
    s_ch = mm_app_get_channel_by_type(test_obj, MM_CHANNEL_TYPE_SNAPSHOT);

    rc = mm_app_stop_and_del_channel(test_obj, p_ch);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:Stop Preview failed rc=%d\n", __func__, rc);
    }

    rc = mm_app_stop_and_del_channel(test_obj, v_ch);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:Stop Preview failed rc=%d\n", __func__, rc);
    }

    rc = mm_app_stop_and_del_channel(test_obj, s_ch);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:Stop Preview failed rc=%d\n", __func__, rc);
    }

    return rc;
}

int mm_app_start_record(mm_camera_test_obj_t *test_obj)
{
    int rc = MM_CAMERA_OK;
    mm_camera_channel_t *v_ch = NULL;

    v_ch = mm_app_get_channel_by_type(test_obj, MM_CHANNEL_TYPE_VIDEO);

    rc = mm_app_start_channel(test_obj, v_ch);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:start recording failed rc=%d\n", __func__, rc);
    }

    return rc;
}

int mm_app_stop_record(mm_camera_test_obj_t *test_obj)
{
    int rc = MM_CAMERA_OK;
    mm_camera_channel_t *v_ch = NULL;

    v_ch = mm_app_get_channel_by_type(test_obj, MM_CHANNEL_TYPE_VIDEO);

    rc = mm_app_stop_channel(test_obj, v_ch);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:stop recording failed rc=%d\n", __func__, rc);
    }

    return rc;
}

int mm_app_start_live_snapshot(mm_camera_test_obj_t *test_obj)
{
    int rc = MM_CAMERA_OK;
    mm_camera_channel_t *s_ch = NULL;

    s_ch = mm_app_get_channel_by_type(test_obj, MM_CHANNEL_TYPE_SNAPSHOT);

    rc = mm_app_start_channel(test_obj, s_ch);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:start recording failed rc=%d\n", __func__, rc);
    }

    return rc;
}

int mm_app_stop_live_snapshot(mm_camera_test_obj_t *test_obj)
{
    int rc = MM_CAMERA_OK;
    mm_camera_channel_t *s_ch = NULL;

    s_ch = mm_app_get_channel_by_type(test_obj, MM_CHANNEL_TYPE_SNAPSHOT);

    rc = mm_app_stop_channel(test_obj, s_ch);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:stop recording failed rc=%d\n", __func__, rc);
    }

    return rc;
}
