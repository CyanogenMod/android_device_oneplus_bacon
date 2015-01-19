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
#include <assert.h>
#include <sys/mman.h>
#include <semaphore.h>

static void mm_app_metadata_notify_cb(mm_camera_super_buf_t *bufs,
                                     void *user_data)
{
  uint32_t i = 0;
  mm_camera_channel_t *channel = NULL;
  mm_camera_stream_t *p_stream = NULL;
  mm_camera_test_obj_t *pme = (mm_camera_test_obj_t *)user_data;
  mm_camera_buf_def_t *frame = bufs->bufs[0];
  cam_metadata_info_t *pMetadata;
  cam_auto_focus_data_t *focus_data;

  /* find channel */
  for (i = 0; i < MM_CHANNEL_TYPE_MAX; i++) {
      if (pme->channels[i].ch_id == bufs->ch_id) {
          channel = &pme->channels[i];
          break;
      }
  }
  /* find preview stream */
  for (i = 0; i < channel->num_streams; i++) {
      if (channel->streams[i].s_config.stream_info->stream_type == CAM_STREAM_TYPE_METADATA) {
          p_stream = &channel->streams[i];
          break;
      }
  }
  if (NULL == p_stream) {
      CDBG_ERROR("%s: cannot find metadata stream", __func__);
      return;
  }

  /* find preview frame */
  for (i = 0; i < bufs->num_bufs; i++) {
      if (bufs->bufs[i]->stream_id == p_stream->s_id) {
          frame = bufs->bufs[i];
          break;
      }
  }
  if (pme->metadata == NULL) {
    /* The app will free the meta data, we don't need to bother here */
    pme->metadata = malloc(sizeof(cam_metadata_info_t));
    if (pme->metadata == NULL) {
        CDBG_ERROR("%s:error: malloc failed allocate memory",__func__);
        return;
    }
  }
  memcpy(pme->metadata, frame->buffer, sizeof(cam_metadata_info_t));

  pMetadata = (cam_metadata_info_t *)frame->buffer;

  if (pMetadata->is_focus_valid) {
    focus_data = (cam_auto_focus_data_t *)&(pMetadata->focus_data);

    if (focus_data->focus_state == CAM_AF_FOCUSED || focus_data->focus_state == CAM_AF_NOT_FOCUSED) {
      CDBG_ERROR("%s: AutoFocus Done Call Back Received\n",__func__);
      mm_camera_app_done();
    }
  }

  if (MM_CAMERA_OK != pme->cam->ops->qbuf(bufs->camera_handle,
                                          bufs->ch_id,
                                          frame)) {
      CDBG_ERROR("%s: Failed in Preview Qbuf\n", __func__);
  }
  mm_app_cache_ops((mm_camera_app_meminfo_t *)frame->mem_info,
                   ION_IOC_INV_CACHES);
}

static void mm_app_preview_notify_cb(mm_camera_super_buf_t *bufs,
                                     void *user_data)
{
    uint32_t i = 0;
    mm_camera_channel_t *channel = NULL;
    mm_camera_stream_t *p_stream = NULL;
    mm_camera_buf_def_t *frame = bufs->bufs[0];
    mm_camera_test_obj_t *pme = (mm_camera_test_obj_t *)user_data;

    CDBG_ERROR("%s: BEGIN - length=%zu, frame idx = %d\n",
            __func__, frame->frame_len, frame->frame_idx);

    /* find channel */
    for (i = 0; i < MM_CHANNEL_TYPE_MAX; i++) {
        if (pme->channels[i].ch_id == bufs->ch_id) {
            channel = &pme->channels[i];
            break;
        }
    }
    /* find preview stream */
    for (i = 0; i < channel->num_streams; i++) {
        if (channel->streams[i].s_config.stream_info->stream_type == CAM_STREAM_TYPE_PREVIEW) {
            p_stream = &channel->streams[i];
            break;
        }
    }
    /* find preview frame */
    for (i = 0; i < bufs->num_bufs; i++) {
        if (bufs->bufs[i]->stream_id == p_stream->s_id) {
            frame = bufs->bufs[i];
            break;
        }
    }

    if (NULL == p_stream) {
        CDBG_ERROR("%s: cannot find preview stream", __func__);
        return;
    }

    if ( 0 < pme->fb_fd ) {
        mm_app_overlay_display(pme, frame->fd);
    }
#ifdef DUMP_PRV_IN_FILE
    {
      char file_name[64];
      snprintf(file_name, sizeof(file_name), "P_C%d", pme->cam->camera_handle);
      mm_app_dump_frame(frame, file_name, "yuv", frame->frame_idx);
    }
#endif
    if (pme->user_preview_cb) {
        CDBG_ERROR("[DBG] %s, user defined own preview cb. calling it...", __func__);
        pme->user_preview_cb(frame);
    }
    if (MM_CAMERA_OK != pme->cam->ops->qbuf(bufs->camera_handle,
                                            bufs->ch_id,
                                            frame)) {
        CDBG_ERROR("%s: Failed in Preview Qbuf\n", __func__);
    }
    mm_app_cache_ops((mm_camera_app_meminfo_t *)frame->mem_info,
                     ION_IOC_INV_CACHES);

    CDBG("%s: END\n", __func__);
}

static void mm_app_zsl_notify_cb(mm_camera_super_buf_t *bufs,
                                 void *user_data)
{
    int rc = 0;
    uint32_t i = 0;
    mm_camera_test_obj_t *pme = (mm_camera_test_obj_t *)user_data;
    mm_camera_channel_t *channel = NULL;
    mm_camera_stream_t *p_stream = NULL;
    mm_camera_stream_t *m_stream = NULL;
    mm_camera_stream_t *md_stream = NULL;
    mm_camera_buf_def_t *p_frame = NULL;
    mm_camera_buf_def_t *m_frame = NULL;
    mm_camera_buf_def_t *md_frame = NULL;

    CDBG("%s: BEGIN\n", __func__);

    /* find channel */
    for (i = 0; i < MM_CHANNEL_TYPE_MAX; i++) {
        if (pme->channels[i].ch_id == bufs->ch_id) {
            channel = &pme->channels[i];
            break;
        }
    }
    if (NULL == channel) {
        CDBG_ERROR("%s: Wrong channel id (%d)", __func__, bufs->ch_id);
        return;
    }

    /* find preview stream */
    for (i = 0; i < channel->num_streams; i++) {
        if (channel->streams[i].s_config.stream_info->stream_type == CAM_STREAM_TYPE_PREVIEW) {
            p_stream = &channel->streams[i];
            break;
        }
    }
    if (NULL == p_stream) {
        CDBG_ERROR("%s: cannot find preview stream", __func__);
        return;
    }

    /* find snapshot stream */
    for (i = 0; i < channel->num_streams; i++) {
        if (channel->streams[i].s_config.stream_info->stream_type == CAM_STREAM_TYPE_SNAPSHOT) {
            m_stream = &channel->streams[i];
            break;
        }
    }
    if (NULL == m_stream) {
        CDBG_ERROR("%s: cannot find snapshot stream", __func__);
        return;
    }

    /* find metadata stream */
    for (i = 0; i < channel->num_streams; i++) {
        if (channel->streams[i].s_config.stream_info->stream_type == CAM_STREAM_TYPE_METADATA) {
            md_stream = &channel->streams[i];
            break;
        }
    }
    if (NULL == md_stream) {
        CDBG_ERROR("%s: cannot find metadata stream", __func__);
    }

    /* find preview frame */
    for (i = 0; i < bufs->num_bufs; i++) {
        if (bufs->bufs[i]->stream_id == p_stream->s_id) {
            p_frame = bufs->bufs[i];
            break;
        }
    }

    if(md_stream) {
      /* find metadata frame */
      for (i = 0; i < bufs->num_bufs; i++) {
          if (bufs->bufs[i]->stream_id == md_stream->s_id) {
              md_frame = bufs->bufs[i];
              break;
          }
      }
      /* fill in meta data frame ptr */
      if (md_frame != NULL) {
        pme->metadata = (cam_metadata_info_t *)md_frame->buffer;
      } else {
          CDBG_ERROR("%s: error getting metadata frame",__func__);
          goto exit;
      }
    }
    /* find snapshot frame */
    for (i = 0; i < bufs->num_bufs; i++) {
        if (bufs->bufs[i]->stream_id == m_stream->s_id) {
            m_frame = bufs->bufs[i];
            break;
        }
    }

    if (!m_frame || !p_frame) {
        CDBG_ERROR("%s: cannot find preview/snapshot frame", __func__);
        return;
    }

    CDBG("%s: ZSL CB with fb_fd = %d, m_frame = %p, p_frame = %p \n",
         __func__,
         pme->fb_fd,
         m_frame,
         p_frame);

    if ( 0 < pme->fb_fd ) {
        mm_app_overlay_display(pme, p_frame->fd);
    }/* else {
        mm_app_dump_frame(p_frame, "zsl_preview", "yuv", p_frame->frame_idx);
        mm_app_dump_frame(m_frame, "zsl_main", "yuv", m_frame->frame_idx);
    }*/

    if ( pme->enable_reproc && ( NULL != pme->reproc_stream ) ) {
        rc = mm_app_do_reprocess(pme,
                                 m_frame,
                                 md_frame->buf_idx,
                                 bufs,
                                 md_stream);
        if (MM_CAMERA_OK != rc ) {
            CDBG_ERROR("%s: reprocess failed rc = %d", __func__, rc);
        }

        return;
    }

    if ( pme->encodeJpeg ) {
        pme->jpeg_buf.buf.buffer = (uint8_t *)malloc(m_frame->frame_len);
        if ( NULL == pme->jpeg_buf.buf.buffer ) {
            CDBG_ERROR("%s: error allocating jpeg output buffer", __func__);
            goto exit;
        }

        pme->jpeg_buf.buf.frame_len = m_frame->frame_len;
        /* create a new jpeg encoding session */
        rc = createEncodingSession(pme, m_stream, m_frame);
        if (0 != rc) {
            CDBG_ERROR("%s: error creating jpeg session", __func__);
            free(pme->jpeg_buf.buf.buffer);
            goto exit;
        }

        /* start jpeg encoding job */
        rc = encodeData(pme, bufs, m_stream);
        pme->encodeJpeg = 0;
    } else {
        if (MM_CAMERA_OK != pme->cam->ops->qbuf(bufs->camera_handle,
                                                bufs->ch_id,
                                                m_frame)) {
            CDBG_ERROR("%s: Failed in main Qbuf\n", __func__);
        }
        mm_app_cache_ops((mm_camera_app_meminfo_t *)m_frame->mem_info,
                         ION_IOC_INV_CACHES);
    }

exit:

    if (MM_CAMERA_OK != pme->cam->ops->qbuf(bufs->camera_handle,
                                            bufs->ch_id,
                                            p_frame)) {
        CDBG_ERROR("%s: Failed in preview Qbuf\n", __func__);
    }
    mm_app_cache_ops((mm_camera_app_meminfo_t *)p_frame->mem_info,
                     ION_IOC_INV_CACHES);

    if(md_frame) {
      if (MM_CAMERA_OK != pme->cam->ops->qbuf(bufs->camera_handle,
                                              bufs->ch_id,
                                              md_frame)) {
          CDBG_ERROR("%s: Failed in metadata Qbuf\n", __func__);
      }
      mm_app_cache_ops((mm_camera_app_meminfo_t *)md_frame->mem_info,
                       ION_IOC_INV_CACHES);
    }

    CDBG("%s: END\n", __func__);
}

mm_camera_stream_t * mm_app_add_metadata_stream(mm_camera_test_obj_t *test_obj,
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
    stream->s_config.stream_info->stream_type = CAM_STREAM_TYPE_METADATA;
    stream->s_config.stream_info->streaming_mode = CAM_STREAMING_MODE_CONTINUOUS;
    stream->s_config.stream_info->fmt = DEFAULT_PREVIEW_FORMAT;
    stream->s_config.stream_info->dim.width = sizeof(cam_metadata_info_t);
    stream->s_config.stream_info->dim.height = 1;
    stream->s_config.padding_info = cam_cap->padding_info;

    rc = mm_app_config_stream(test_obj, channel, stream, &stream->s_config);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:config preview stream err=%d\n", __func__, rc);
        return NULL;
    }

    return stream;
}

mm_camera_stream_t * mm_app_add_preview_stream(mm_camera_test_obj_t *test_obj,
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
    stream->s_config.stream_info->stream_type = CAM_STREAM_TYPE_PREVIEW;
    stream->s_config.stream_info->streaming_mode = CAM_STREAMING_MODE_CONTINUOUS;
    stream->s_config.stream_info->fmt = DEFAULT_PREVIEW_FORMAT;

    if ((test_obj->preview_resolution.user_input_display_width == 0) ||
           ( test_obj->preview_resolution.user_input_display_height == 0)) {
        stream->s_config.stream_info->dim.width = DEFAULT_PREVIEW_WIDTH;
        stream->s_config.stream_info->dim.height = DEFAULT_PREVIEW_HEIGHT;
    } else {
        stream->s_config.stream_info->dim.width = test_obj->preview_resolution.user_input_display_width;
        stream->s_config.stream_info->dim.height = test_obj->preview_resolution.user_input_display_height;
    }

    stream->s_config.padding_info = cam_cap->padding_info;

    rc = mm_app_config_stream(test_obj, channel, stream, &stream->s_config);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:config preview stream err=%d\n", __func__, rc);
        return NULL;
    }

    return stream;
}

mm_camera_stream_t * mm_app_add_raw_stream(mm_camera_test_obj_t *test_obj,
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
    stream->s_config.stream_info->fmt = test_obj->buffer_format;
    if ( test_obj->buffer_width == 0 || test_obj->buffer_height == 0 ) {
        stream->s_config.stream_info->dim.width = DEFAULT_SNAPSHOT_WIDTH;
        stream->s_config.stream_info->dim.height = DEFAULT_SNAPSHOT_HEIGHT;
    } else {
        stream->s_config.stream_info->dim.width = (int32_t)test_obj->buffer_width;
        stream->s_config.stream_info->dim.height = (int32_t)test_obj->buffer_height;
    }
    stream->s_config.padding_info = cam_cap->padding_info;

    rc = mm_app_config_stream(test_obj, channel, stream, &stream->s_config);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:config preview stream err=%d\n", __func__, rc);
        return NULL;
    }

    return stream;
}

mm_camera_stream_t * mm_app_add_snapshot_stream(mm_camera_test_obj_t *test_obj,
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
    if ( test_obj->buffer_width == 0 || test_obj->buffer_height == 0 ) {
        stream->s_config.stream_info->dim.width = DEFAULT_SNAPSHOT_WIDTH;
        stream->s_config.stream_info->dim.height = DEFAULT_SNAPSHOT_HEIGHT;
    } else {
        stream->s_config.stream_info->dim.width = (int32_t)test_obj->buffer_width;
        stream->s_config.stream_info->dim.height = (int32_t)test_obj->buffer_height;
    }
    stream->s_config.padding_info = cam_cap->padding_info;

    rc = mm_app_config_stream(test_obj, channel, stream, &stream->s_config);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:config preview stream err=%d\n", __func__, rc);
        return NULL;
    }

    return stream;
}

mm_camera_channel_t * mm_app_add_preview_channel(mm_camera_test_obj_t *test_obj)
{
    mm_camera_channel_t *channel = NULL;
    mm_camera_stream_t *stream = NULL;

    channel = mm_app_add_channel(test_obj,
                                 MM_CHANNEL_TYPE_PREVIEW,
                                 NULL,
                                 NULL,
                                 NULL);
    if (NULL == channel) {
        CDBG_ERROR("%s: add channel failed", __func__);
        return NULL;
    }

    stream = mm_app_add_preview_stream(test_obj,
                                       channel,
                                       mm_app_preview_notify_cb,
                                       (void *)test_obj,
                                       PREVIEW_BUF_NUM);
    if (NULL == stream) {
        CDBG_ERROR("%s: add stream failed\n", __func__);
        mm_app_del_channel(test_obj, channel);
        return NULL;
    }

    return channel;
}

int mm_app_stop_and_del_channel(mm_camera_test_obj_t *test_obj,
                                mm_camera_channel_t *channel)
{
    int rc = MM_CAMERA_OK;
    mm_camera_stream_t *stream = NULL;
    uint8_t i;

    rc = mm_app_stop_channel(test_obj, channel);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:Stop Preview failed rc=%d\n", __func__, rc);
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

int mm_app_start_preview(mm_camera_test_obj_t *test_obj)
{
    int rc = MM_CAMERA_OK;
    mm_camera_channel_t *channel = NULL;
    mm_camera_stream_t *stream = NULL;
    mm_camera_stream_t *s_metadata = NULL;
    uint8_t i;

    channel =  mm_app_add_preview_channel(test_obj);
    if (NULL == channel) {
        CDBG_ERROR("%s: add channel failed", __func__);
        return -MM_CAMERA_E_GENERAL;
    }

    s_metadata = mm_app_add_metadata_stream(test_obj,
                                            channel,
                                            mm_app_metadata_notify_cb,
                                            (void *)test_obj,
                                            PREVIEW_BUF_NUM);
    if (NULL == s_metadata) {
        CDBG_ERROR("%s: add metadata stream failed\n", __func__);
        mm_app_del_channel(test_obj, channel);
        return rc;
    }

    rc = mm_app_start_channel(test_obj, channel);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:start preview failed rc=%d\n", __func__, rc);
        for (i = 0; i < channel->num_streams; i++) {
            stream = &channel->streams[i];
            mm_app_del_stream(test_obj, channel, stream);
        }
        mm_app_del_channel(test_obj, channel);
        return rc;
    }

    return rc;
}

int mm_app_stop_preview(mm_camera_test_obj_t *test_obj)
{
    int rc = MM_CAMERA_OK;

    mm_camera_channel_t *channel =
        mm_app_get_channel_by_type(test_obj, MM_CHANNEL_TYPE_PREVIEW);

    rc = mm_app_stop_and_del_channel(test_obj, channel);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:Stop Preview failed rc=%d\n", __func__, rc);
    }

    return rc;
}

int mm_app_start_preview_zsl(mm_camera_test_obj_t *test_obj)
{
    int32_t rc = MM_CAMERA_OK;
    mm_camera_channel_t *channel = NULL;
    mm_camera_stream_t *s_preview = NULL;
    mm_camera_stream_t *s_metadata = NULL;
    mm_camera_stream_t *s_main = NULL;
    mm_camera_channel_attr_t attr;

    memset(&attr, 0, sizeof(mm_camera_channel_attr_t));
    attr.notify_mode = MM_CAMERA_SUPER_BUF_NOTIFY_CONTINUOUS;
    attr.look_back = 2;
    attr.post_frame_skip = 0;
    attr.water_mark = 2;
    attr.max_unmatched_frames = 3;
    channel = mm_app_add_channel(test_obj,
                                 MM_CHANNEL_TYPE_ZSL,
                                 &attr,
                                 mm_app_zsl_notify_cb,
                                 test_obj);
    if (NULL == channel) {
        CDBG_ERROR("%s: add channel failed", __func__);
        return -MM_CAMERA_E_GENERAL;
    }

    s_preview = mm_app_add_preview_stream(test_obj,
                                          channel,
                                          mm_app_preview_notify_cb,
                                          (void *)test_obj,
                                          PREVIEW_BUF_NUM);
    if (NULL == s_preview) {
        CDBG_ERROR("%s: add preview stream failed\n", __func__);
        mm_app_del_channel(test_obj, channel);
        return rc;
    }

    s_metadata = mm_app_add_metadata_stream(test_obj,
                                            channel,
                                            mm_app_metadata_notify_cb,
                                            (void *)test_obj,
                                            PREVIEW_BUF_NUM);
    if (NULL == s_metadata) {
        CDBG_ERROR("%s: add metadata stream failed\n", __func__);
        mm_app_del_channel(test_obj, channel);
        return rc;
    }

    s_main = mm_app_add_snapshot_stream(test_obj,
                                        channel,
                                        NULL,
                                        NULL,
                                        PREVIEW_BUF_NUM,
                                        0);
    if (NULL == s_main) {
        CDBG_ERROR("%s: add main snapshot stream failed\n", __func__);
        mm_app_del_stream(test_obj, channel, s_preview);
        mm_app_del_channel(test_obj, channel);
        return rc;
    }

    rc = mm_app_start_channel(test_obj, channel);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:start zsl failed rc=%d\n", __func__, rc);
        mm_app_del_stream(test_obj, channel, s_preview);
        mm_app_del_stream(test_obj, channel, s_metadata);
        mm_app_del_stream(test_obj, channel, s_main);
        mm_app_del_channel(test_obj, channel);
        return rc;
    }

    if ( test_obj->enable_reproc ) {
        if ( NULL == mm_app_add_reprocess_channel(test_obj, s_main) ) {
            CDBG_ERROR("%s: Reprocess channel failed to initialize \n", __func__);
            mm_app_del_stream(test_obj, channel, s_preview);
#ifdef USE_METADATA_STREAM
            mm_app_del_stream(test_obj, channel, s_metadata);
#endif
            mm_app_del_stream(test_obj, channel, s_main);
            mm_app_del_channel(test_obj, channel);
            return rc;
        }
        rc = mm_app_start_reprocess(test_obj);
        if (MM_CAMERA_OK != rc) {
            CDBG_ERROR("%s: reprocess start failed rc=%d\n", __func__, rc);
            mm_app_del_stream(test_obj, channel, s_preview);
#ifdef USE_METADATA_STREAM
            mm_app_del_stream(test_obj, channel, s_metadata);
#endif
            mm_app_del_stream(test_obj, channel, s_main);
            mm_app_del_channel(test_obj, channel);
            return rc;
        }
    }

    return rc;
}

int mm_app_stop_preview_zsl(mm_camera_test_obj_t *test_obj)
{
    int rc = MM_CAMERA_OK;

    mm_camera_channel_t *channel =
        mm_app_get_channel_by_type(test_obj, MM_CHANNEL_TYPE_ZSL);

    rc = mm_app_stop_and_del_channel(test_obj, channel);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:Stop Preview failed rc=%d\n", __func__, rc);
    }

    if ( test_obj->enable_reproc ) {
        rc |= mm_app_stop_reprocess(test_obj);
    }

    return rc;
}

int mm_app_initialize_fb(mm_camera_test_obj_t *test_obj)
{
    int rc = MM_CAMERA_OK;
    int brightness_fd;
    const char brightness_level[] = BACKLIGHT_LEVEL;
    void *fb_base = NULL;

    assert( ( NULL != test_obj ) && ( 0 == test_obj->fb_fd ) );

    test_obj->fb_fd = open(FB_PATH, O_RDWR);
    if ( 0 > test_obj->fb_fd ) {
        CDBG_ERROR("%s: FB device open failed rc=%d, %s\n",
                   __func__,
                   -errno,
                   strerror(errno));
        rc = -errno;
        goto FAIL;
    }

    rc = ioctl(test_obj->fb_fd, FBIOGET_VSCREENINFO, &test_obj->vinfo);
    if ( MM_CAMERA_OK != rc ) {
        CDBG_ERROR("%s: Can not retrieve screen info rc=%d, %s\n",
                   __func__,
                   -errno,
                   strerror(errno));
        rc = -errno;
        goto FAIL;
    }

    if ( ( 0 == test_obj->vinfo.yres_virtual ) ||
         ( 0 == test_obj->vinfo.yres ) ||
         ( test_obj->vinfo.yres > test_obj->vinfo.yres_virtual ) ) {
        CDBG_ERROR("%s: Invalid FB virtual yres: %d, yres: %d\n",
                   __func__,
                   test_obj->vinfo.yres_virtual,
                   test_obj->vinfo.yres);
        rc = MM_CAMERA_E_GENERAL;
        goto FAIL;
    }

    if ( ( 0 == test_obj->vinfo.xres_virtual ) ||
         ( 0 == test_obj->vinfo.xres ) ||
         ( test_obj->vinfo.xres > test_obj->vinfo.xres_virtual ) ) {
        CDBG_ERROR("%s: Invalid FB virtual xres: %d, xres: %d\n",
                   __func__,
                   test_obj->vinfo.xres_virtual,
                   test_obj->vinfo.xres);
        rc = MM_CAMERA_E_GENERAL;
        goto FAIL;
    }

    brightness_fd = open(BACKLIGHT_CONTROL, O_RDWR);
    if ( brightness_fd >= 0 ) {
        write(brightness_fd, brightness_level, strlen(brightness_level));
        close(brightness_fd);
    }

    test_obj->slice_size = test_obj->vinfo.xres * ( test_obj->vinfo.yres - 1 ) * DEFAULT_OV_FORMAT_BPP;
    memset(&test_obj->data_overlay, 0, sizeof(struct mdp_overlay));
    test_obj->data_overlay.src.width  = test_obj->buffer_width;
    test_obj->data_overlay.src.height = test_obj->buffer_height;
    test_obj->data_overlay.src_rect.w = test_obj->buffer_width;
    test_obj->data_overlay.src_rect.h = test_obj->buffer_height;
    test_obj->data_overlay.dst_rect.w = test_obj->buffer_width;
    test_obj->data_overlay.dst_rect.h = test_obj->buffer_height;
    test_obj->data_overlay.src.format = DEFAULT_OV_FORMAT;
    test_obj->data_overlay.src_rect.x = 0;
    test_obj->data_overlay.src_rect.y = 0;
    test_obj->data_overlay.dst_rect.x = 0;
    test_obj->data_overlay.dst_rect.y = 0;
    test_obj->data_overlay.z_order = 2;
    test_obj->data_overlay.alpha = 0x80;
    test_obj->data_overlay.transp_mask = 0xffe0;
    test_obj->data_overlay.flags = MDP_FLIP_LR | MDP_FLIP_UD;

    // Map and clear FB portion
    fb_base = mmap(0,
                   test_obj->slice_size,
                   PROT_WRITE,
                   MAP_SHARED,
                   test_obj->fb_fd,
                   0);
    if ( MAP_FAILED  == fb_base ) {
            CDBG_ERROR("%s: ( Error while memory mapping frame buffer %s",
                       __func__,
                       strerror(errno));
            rc = -errno;
            goto FAIL;
    }

    memset(fb_base, 0, test_obj->slice_size);

    if (ioctl(test_obj->fb_fd, FBIOPAN_DISPLAY, &test_obj->vinfo) < 0) {
        CDBG_ERROR("%s : FBIOPAN_DISPLAY failed!", __func__);
        rc = -errno;
        goto FAIL;
    }

    munmap(fb_base, test_obj->slice_size);
    test_obj->data_overlay.id = (uint32_t)MSMFB_NEW_REQUEST;
    rc = ioctl(test_obj->fb_fd, MSMFB_OVERLAY_SET, &test_obj->data_overlay);
    if (rc < 0) {
        CDBG_ERROR("%s : MSMFB_OVERLAY_SET failed! err=%d\n",
                   __func__,
                   test_obj->data_overlay.id);
        return MM_CAMERA_E_GENERAL;
    }
    CDBG_ERROR("%s: Overlay set with overlay id: %d", __func__, test_obj->data_overlay.id);

    return rc;

FAIL:

    if ( 0 < test_obj->fb_fd ) {
        close(test_obj->fb_fd);
    }

    return rc;
}

int mm_app_close_fb(mm_camera_test_obj_t *test_obj)
{
    int rc = MM_CAMERA_OK;

    assert( ( NULL != test_obj ) && ( 0 < test_obj->fb_fd ) );

    if (ioctl(test_obj->fb_fd, MSMFB_OVERLAY_UNSET, &test_obj->data_overlay.id)) {
        CDBG_ERROR("\nERROR! MSMFB_OVERLAY_UNSET failed! (Line %d)\n", __LINE__);
    }

    if (ioctl(test_obj->fb_fd, FBIOPAN_DISPLAY, &test_obj->vinfo) < 0) {
        CDBG_ERROR("ERROR: FBIOPAN_DISPLAY failed! line=%d\n", __LINE__);
    }

    close(test_obj->fb_fd);
    test_obj->fb_fd = 0;

    return rc;
}

void memset16(void *pDst, uint16_t value, int count)
{
    uint16_t *ptr = pDst;
    while (count--)
        *ptr++ = value;
}

int mm_app_overlay_display(mm_camera_test_obj_t *test_obj, int bufferFd)
{
    int rc = MM_CAMERA_OK;
    struct msmfb_overlay_data ovdata;


    memset(&ovdata, 0, sizeof(struct msmfb_overlay_data));
    ovdata.id = test_obj->data_overlay.id;
    ovdata.data.memory_id = bufferFd;

    if (ioctl(test_obj->fb_fd, MSMFB_OVERLAY_PLAY, &ovdata)) {
        CDBG_ERROR("%s : MSMFB_OVERLAY_PLAY failed!", __func__);
        return MM_CAMERA_E_GENERAL;
    }

    if (ioctl(test_obj->fb_fd, FBIOPAN_DISPLAY, &test_obj->vinfo) < 0) {
        CDBG_ERROR("%s : FBIOPAN_DISPLAY failed!", __func__);
        return MM_CAMERA_E_GENERAL;
    }

    return rc;
}
