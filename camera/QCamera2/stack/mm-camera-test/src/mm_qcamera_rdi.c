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

static uint32_t rdi_len = 0;

static void mm_app_rdi_dump_frame(mm_camera_buf_def_t *frame,
                                  char *name,
                                  char *ext,
                                  uint32_t frame_idx)
{
    char file_name[64];
    int file_fd;
    int i;
    if (frame != NULL) {
#ifdef USE_KK_CODE
        snprintf(file_name, sizeof(file_name), "/data/%s_%03u.%s", name,
                frame_idx, ext);
#else
        snprintf(file_name, sizeof(file_name), "/data/misc/camera/%s_%03u.%s", name,
                frame_idx, ext);
#endif
        file_fd = open(file_name, O_RDWR | O_CREAT, 0777);
        if (file_fd < 0) {
            CDBG_ERROR("%s: cannot open file %s \n", __func__, file_name);
        } else {
            for (i = 0; i < frame->num_planes; i++) {
                write(file_fd,
                      (uint8_t *)frame->buffer + frame->planes[i].data_offset,
                      rdi_len);
            }

            close(file_fd);
            CDBG("dump %s", file_name);
        }
    }
}

static void mm_app_rdi_notify_cb(mm_camera_super_buf_t *bufs,
                                 void *user_data)
{
    char file_name[64];
    mm_camera_buf_def_t *frame = bufs->bufs[0];
    mm_camera_test_obj_t *pme = (mm_camera_test_obj_t *)user_data;

    CDBG("%s: BEGIN - length=%zu, frame idx = %d stream_id=%d\n",
         __func__, frame->frame_len, frame->frame_idx, frame->stream_id);
    snprintf(file_name, sizeof(file_name), "RDI_dump_%d", pme->cam->camera_handle);
    mm_app_rdi_dump_frame(frame, file_name, "raw", frame->frame_idx);

    if (MM_CAMERA_OK != pme->cam->ops->qbuf(bufs->camera_handle,
                                            bufs->ch_id,
                                            frame)) {
        CDBG_ERROR("%s: Failed in RDI Qbuf\n", __func__);
    }
    mm_app_cache_ops((mm_camera_app_meminfo_t *)frame->mem_info,
                     ION_IOC_INV_CACHES);

    CDBG("%s: END\n", __func__);
}

mm_camera_stream_t * mm_app_add_rdi_stream(mm_camera_test_obj_t *test_obj,
                                               mm_camera_channel_t *channel,
                                               mm_camera_buf_notify_t stream_cb,
                                               void *userdata,
                                               uint8_t num_bufs,
                                               uint8_t num_burst)
{
    int rc = MM_CAMERA_OK;
    size_t i;
    mm_camera_stream_t *stream = NULL;
    cam_capability_t *cam_cap = (cam_capability_t *)(test_obj->cap_buf.buf.buffer);
    cam_format_t fmt = CAM_FORMAT_MAX;
    cam_stream_buf_plane_info_t *buf_planes;

    stream = mm_app_add_stream(test_obj, channel);
    if (NULL == stream) {
        CDBG_ERROR("%s: add stream failed\n", __func__);
        return NULL;
    }

    CDBG_ERROR("%s: raw_dim w:%d height:%d\n", __func__,
            cam_cap->raw_dim.width, cam_cap->raw_dim.height);
    for (i = 0;i < cam_cap->supported_raw_fmt_cnt;i++) {
        CDBG_ERROR("%s: supported_raw_fmts[%zu]=%d\n", __func__,
                i, cam_cap->supported_raw_fmts[i]);
        if (CAM_FORMAT_BAYER_MIPI_RAW_8BPP_GBRG <= cam_cap->supported_raw_fmts[i] &&
            CAM_FORMAT_BAYER_MIPI_RAW_12BPP_BGGR >= cam_cap->supported_raw_fmts[i])
        {
            fmt = cam_cap->supported_raw_fmts[i];
            CDBG_ERROR("%s: fmt=%d\n", __func__, fmt);
        }
    }

    if (CAM_FORMAT_MAX == fmt) {
        CDBG_ERROR("%s: rdi format not supported\n", __func__);
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
    stream->s_config.stream_info->stream_type = CAM_STREAM_TYPE_RAW;
    if (num_burst == 0) {
        stream->s_config.stream_info->streaming_mode = CAM_STREAMING_MODE_CONTINUOUS;
    } else {
        stream->s_config.stream_info->streaming_mode = CAM_STREAMING_MODE_BURST;
        stream->s_config.stream_info->num_of_burst = num_burst;
    }
    stream->s_config.stream_info->fmt = fmt;
    stream->s_config.stream_info->dim.width = cam_cap->raw_dim.width;
    stream->s_config.stream_info->dim.height = cam_cap->raw_dim.height;
    stream->s_config.padding_info = cam_cap->padding_info;

    rc = mm_app_config_stream(test_obj, channel, stream, &stream->s_config);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:config rdi stream err=%d\n", __func__, rc);
        return NULL;
    }

    buf_planes = &stream->s_config.stream_info->buf_planes;
    rdi_len = buf_planes->plane_info.mp[0].len;
    CDBG("%s: plane_info %dx%d len:%d frame_len:%d\n", __func__,
        buf_planes->plane_info.mp[0].stride, buf_planes->plane_info.mp[0].scanline,
        buf_planes->plane_info.mp[0].len, buf_planes->plane_info.frame_len);

    return stream;
}

mm_camera_stream_t * mm_app_add_rdi_snapshot_stream(mm_camera_test_obj_t *test_obj,
                                                mm_camera_channel_t *channel,
                                                mm_camera_buf_notify_t stream_cb,
                                                void *userdata,
                                                uint8_t num_bufs,
                                                uint8_t num_burst)
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
    stream->s_config.stream_info->stream_type = CAM_STREAM_TYPE_SNAPSHOT;
    if (num_burst == 0) {
        stream->s_config.stream_info->streaming_mode = CAM_STREAMING_MODE_CONTINUOUS;
    } else {
        stream->s_config.stream_info->streaming_mode = CAM_STREAMING_MODE_BURST;
        stream->s_config.stream_info->num_of_burst = num_burst;
    }
    stream->s_config.stream_info->fmt = DEFAULT_SNAPSHOT_FORMAT;
    stream->s_config.stream_info->dim.width = DEFAULT_SNAPSHOT_WIDTH;
    stream->s_config.stream_info->dim.height = DEFAULT_SNAPSHOT_HEIGHT;
    stream->s_config.padding_info = cam_cap->padding_info;

    rc = mm_app_config_stream(test_obj, channel, stream, &stream->s_config);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:config rdi stream err=%d\n", __func__, rc);
        return NULL;
    }

    return stream;
}

mm_camera_channel_t * mm_app_add_rdi_channel(mm_camera_test_obj_t *test_obj, uint8_t num_burst)
{
    mm_camera_channel_t *channel = NULL;
    mm_camera_stream_t *stream = NULL;

    channel = mm_app_add_channel(test_obj,
                                 MM_CHANNEL_TYPE_RDI,
                                 NULL,
                                 NULL,
                                 NULL);
    if (NULL == channel) {
        CDBG_ERROR("%s: add channel failed", __func__);
        return NULL;
    }

    stream = mm_app_add_rdi_stream(test_obj,
                                       channel,
                                       mm_app_rdi_notify_cb,
                                       (void *)test_obj,
                                       RDI_BUF_NUM,
                                       num_burst);
    if (NULL == stream) {
        CDBG_ERROR("%s: add stream failed\n", __func__);
        mm_app_del_channel(test_obj, channel);
        return NULL;
    }

    CDBG("%s: channel=%d stream=%d\n", __func__, channel->ch_id, stream->s_id);
    return channel;
}

int mm_app_stop_and_del_rdi_channel(mm_camera_test_obj_t *test_obj,
                                mm_camera_channel_t *channel)
{
    int rc = MM_CAMERA_OK;
    mm_camera_stream_t *stream = NULL;
    uint8_t i;

    rc = mm_app_stop_channel(test_obj, channel);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:Stop RDI failed rc=%d\n", __func__, rc);
    }

    for (i = 0; i < channel->num_streams; i++) {
        stream = &channel->streams[i];
        rc = mm_app_del_stream(test_obj, channel, stream);
        if (MM_CAMERA_OK != rc) {
            CDBG_ERROR("%s:del stream(%d) failed rc=%d\n", __func__, i, rc);
        }
    }

    rc = mm_app_del_channel(test_obj, channel);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:delete channel failed rc=%d\n", __func__, rc);
    }

    return rc;
}

int mm_app_start_rdi(mm_camera_test_obj_t *test_obj, uint8_t num_burst)
{
    int rc = MM_CAMERA_OK;
    mm_camera_channel_t *channel = NULL;

    channel = mm_app_add_rdi_channel(test_obj, num_burst);
    if (NULL == channel) {
        CDBG_ERROR("%s: add channel failed", __func__);
        return -MM_CAMERA_E_GENERAL;
    }

    rc = mm_app_start_channel(test_obj, channel);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:start rdi failed rc=%d\n", __func__, rc);
        mm_app_del_channel(test_obj, channel);
        return rc;
    }

    return rc;
}

int mm_app_stop_rdi(mm_camera_test_obj_t *test_obj)
{
    int rc = MM_CAMERA_OK;

    mm_camera_channel_t *channel =
        mm_app_get_channel_by_type(test_obj, MM_CHANNEL_TYPE_RDI);

    rc = mm_app_stop_and_del_rdi_channel(test_obj, channel);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:Stop RDI failed rc=%d\n", __func__, rc);
    }

    return rc;
}

