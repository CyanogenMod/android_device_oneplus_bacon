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

static void mm_app_reprocess_notify_cb(mm_camera_super_buf_t *bufs,
                                   void *user_data)
{
    mm_camera_buf_def_t *frame = NULL;
    mm_camera_test_obj_t *pme = (mm_camera_test_obj_t *)user_data;
    mm_camera_channel_t *channel = NULL;
    mm_camera_stream_t *m_stream = NULL;
    mm_camera_buf_def_t *m_frame = NULL;
    mm_camera_super_buf_t *src_frame;
    uint32_t i = 0;
    int rc = 0;

    if (!bufs) {
        CDBG_ERROR("%s: invalid superbuf\n", __func__);
        return;
    }
    frame = bufs->bufs[0];
    CDBG_ERROR("%s: BEGIN - length=%zu, frame idx = %d\n",
         __func__, frame->frame_len, frame->frame_idx);
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

    // We have only one stream and buffer
    // in the reprocess channel.
    m_stream = &channel->streams[0];
    m_frame = bufs->bufs[0];

    /* find meta data frame from reprocess superbuff */
    mm_camera_buf_def_t *meta_frame = NULL;
    for (i = 0; bufs && (i < bufs->num_bufs); i++) {
        if (bufs->bufs[i]->stream_type == CAM_STREAM_TYPE_METADATA) {
            meta_frame = bufs->bufs[i];
            break;
        }
    }
    /* fill in meta data frame ptr */
    if (meta_frame != NULL) {
      pme->metadata = (cam_metadata_info_t *)meta_frame->buffer;
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
        CDBG_ERROR("Encoding reprocessed frame!!");
        rc = encodeData(pme, bufs, m_stream);
        pme->encodeJpeg = 0;
    } else {
        if (MM_CAMERA_OK != pme->cam->ops->qbuf(bufs->camera_handle,
                                                bufs->ch_id,
                                                frame)) {
            CDBG_ERROR("%s: Failed in Reprocess Qbuf\n", __func__);
        }
        mm_app_cache_ops((mm_camera_app_meminfo_t *)frame->mem_info,
                         ION_IOC_INV_CACHES);
    }

exit:

// Release source frame
    src_frame = ( mm_camera_super_buf_t * ) mm_qcamera_queue_dequeue(&pme->pp_frames, 1);
    if ( NULL != src_frame ) {
        mm_app_release_ppinput((void *) src_frame, (void *) pme);
    }

    CDBG_ERROR("%s: END\n", __func__);
}

mm_camera_stream_t * mm_app_add_reprocess_stream_from_source(mm_camera_test_obj_t *test_obj,
                                                             mm_camera_channel_t *channel,
                                                             mm_camera_stream_t *source,
                                                             mm_camera_buf_notify_t stream_cb,
                                                             cam_pp_feature_config_t pp_config,
                                                             void *userdata,
                                                             uint8_t num_bufs)
{
    int rc = MM_CAMERA_OK;
    mm_camera_stream_t *stream = NULL;
    cam_capability_t *cam_cap = NULL;
    cam_stream_info_t *source_stream_info;

    if ( ( NULL == test_obj ) ||
         ( NULL == channel ) ||
         ( NULL == source ) ) {
        CDBG_ERROR("%s: Invalid input\n", __func__);
        return NULL;
    }
    cam_cap = (cam_capability_t *)(test_obj->cap_buf.buf.buffer);
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
    source_stream_info = (cam_stream_info_t *) source->s_info_buf.buf.buffer;
    memset(stream->s_config.stream_info, 0, sizeof(cam_stream_info_t));
    stream->s_config.stream_info->stream_type = CAM_STREAM_TYPE_OFFLINE_PROC;
    stream->s_config.stream_info->streaming_mode = CAM_STREAMING_MODE_CONTINUOUS;
    stream->s_config.stream_info->fmt = source_stream_info->fmt;
    stream->s_config.stream_info->dim = source_stream_info->dim;
    stream->s_config.padding_info = cam_cap->padding_info;


    stream->s_config.stream_info->reprocess_config.pp_type = CAM_ONLINE_REPROCESS_TYPE;
    stream->s_config.stream_info->reprocess_config.online.input_stream_id = source->s_config.stream_info->stream_svr_id;
    stream->s_config.stream_info->reprocess_config.online.input_stream_type = source->s_config.stream_info->stream_type;
    stream->s_config.stream_info->reprocess_config.pp_feature_config = pp_config;

    rc = mm_app_config_stream(test_obj, channel, stream, &stream->s_config);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:config preview stream err=%d\n", __func__, rc);
        return NULL;
    }

    return stream;
}

mm_camera_channel_t * mm_app_add_reprocess_channel(mm_camera_test_obj_t *test_obj,
                                                   mm_camera_stream_t *source_stream)
{
    mm_camera_channel_t *channel = NULL;
    mm_camera_stream_t *stream = NULL;

    if ( NULL == source_stream ) {
        CDBG_ERROR("%s: add reprocess stream failed\n", __func__);
        return NULL;
    }

    channel = mm_app_add_channel(test_obj,
                                 MM_CHANNEL_TYPE_REPROCESS,
                                 NULL,
                                 NULL,
                                 NULL);
    if (NULL == channel) {
        CDBG_ERROR("%s: add channel failed", __func__);
        return NULL;
    }

    // pp feature config
    cam_pp_feature_config_t pp_config;
    memset(&pp_config, 0, sizeof(cam_pp_feature_config_t));

    cam_capability_t *caps = ( cam_capability_t * ) ( test_obj->cap_buf.buf.buffer );
    if (caps->min_required_pp_mask & CAM_QCOM_FEATURE_SHARPNESS) {
        pp_config.feature_mask |= CAM_QCOM_FEATURE_SHARPNESS;
        pp_config.sharpness = test_obj->reproc_sharpness;
    }

    if (test_obj->reproc_wnr.denoise_enable) {
        pp_config.feature_mask |= CAM_QCOM_FEATURE_DENOISE2D;
        pp_config.denoise2d = test_obj->reproc_wnr;
    }

    if (test_obj->enable_CAC) {
        pp_config.feature_mask |= CAM_QCOM_FEATURE_CAC;
    }

    uint8_t minStreamBufNum = source_stream->num_of_bufs;
    stream = mm_app_add_reprocess_stream_from_source(test_obj,
                                     channel,
                                     source_stream,
                                     mm_app_reprocess_notify_cb,
                                     pp_config,
                                     (void *)test_obj,
                                     minStreamBufNum);
    if (NULL == stream) {
        CDBG_ERROR("%s: add reprocess stream failed\n", __func__);
        mm_app_del_channel(test_obj, channel);
        return NULL;
    }
    test_obj->reproc_stream = stream;

    return channel;
}

int mm_app_start_reprocess(mm_camera_test_obj_t *test_obj)
{
    int rc = MM_CAMERA_OK;
    mm_camera_channel_t *r_ch = NULL;

    mm_camera_queue_init(&test_obj->pp_frames,
                         mm_app_release_ppinput,
                         ( void * ) test_obj);

    r_ch = mm_app_get_channel_by_type(test_obj, MM_CHANNEL_TYPE_REPROCESS);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s: No initialized reprocess channel d rc=%d\n",
                    __func__,
                    rc);
        return rc;
    }

    rc = mm_app_start_channel(test_obj, r_ch);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:start reprocess failed rc=%d\n", __func__, rc);
        mm_app_del_channel(test_obj, r_ch);
        return rc;
    }

    return rc;
}

int mm_app_stop_reprocess(mm_camera_test_obj_t *test_obj)
{
    int rc = MM_CAMERA_OK;
    mm_camera_channel_t *r_ch = NULL;

    r_ch = mm_app_get_channel_by_type(test_obj, MM_CHANNEL_TYPE_REPROCESS);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s: No initialized reprocess channel d rc=%d\n",
                    __func__,
                    rc);
        return rc;
    }

    rc = mm_app_stop_and_del_channel(test_obj, r_ch);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:Stop Preview failed rc=%d\n", __func__, rc);
    }

    mm_qcamera_queue_release(&test_obj->pp_frames);
    test_obj->reproc_stream = NULL;

    return rc;
}

int mm_app_do_reprocess(mm_camera_test_obj_t *test_obj,
                        mm_camera_buf_def_t *frame,
                        uint32_t meta_idx,
                        mm_camera_super_buf_t *super_buf,
                        mm_camera_stream_t *src_meta)
{
    int rc = MM_CAMERA_OK;
    mm_camera_channel_t *r_ch = NULL;
    mm_camera_super_buf_t *src_buf = NULL;

    if ( ( NULL == test_obj ) ||
         ( NULL == frame ) ||
         ( NULL == super_buf )) {
        CDBG_ERROR("%s: Invalid input rc=%d\n",
                    __func__,
                    rc);
        return rc;
    }

    if ( NULL == test_obj->reproc_stream ) {
        CDBG_ERROR("%s: No reprocess stream rc=%d\n",
                    __func__,
                    rc);
        return rc;
    }

    r_ch = mm_app_get_channel_by_type(test_obj, MM_CHANNEL_TYPE_REPROCESS);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s: No reprocess channel rc=%d\n",
                    __func__,
                    rc);
        return rc;
    }

    src_buf = ( mm_camera_super_buf_t * ) malloc(sizeof(mm_camera_super_buf_t));
    if ( NULL == src_buf ) {
        CDBG_ERROR("%s: No resources for src frame rc=%d\n",
                    __func__,
                    rc);
        return -1;
    }
    memcpy(src_buf, super_buf, sizeof(mm_camera_super_buf_t));
    mm_qcamera_queue_enqueue(&test_obj->pp_frames, src_buf);

    cam_stream_parm_buffer_t param;
    memset(&param, 0, sizeof(cam_stream_parm_buffer_t));
    param.type = CAM_STREAM_PARAM_TYPE_DO_REPROCESS;
    param.reprocess.buf_index = frame->buf_idx;
    param.reprocess.frame_idx = frame->frame_idx;
    if (src_meta != NULL) {
        param.reprocess.meta_present = 1;
        param.reprocess.meta_stream_handle = src_meta->s_config.stream_info->stream_svr_id;
        param.reprocess.meta_buf_index = meta_idx;
    } else {
        CDBG_ERROR("%s: No metadata source stream rc=%d\n",
                   __func__,
                   rc);
    }

    test_obj->reproc_stream->s_config.stream_info->parm_buf = param;
    rc = test_obj->cam->ops->set_stream_parms(test_obj->cam->camera_handle,
                                              r_ch->ch_id,
                                              test_obj->reproc_stream->s_id,
                                              &test_obj->reproc_stream->s_config.stream_info->parm_buf);

    return rc;
}

void mm_app_release_ppinput(void *data, void *user_data)
{
    uint32_t i = 0;
    mm_camera_super_buf_t *recvd_frame  = ( mm_camera_super_buf_t * ) data;
    mm_camera_test_obj_t *pme = (mm_camera_test_obj_t *)user_data;

    for ( i = 0 ; i < recvd_frame->num_bufs ; i++) {
        if (MM_CAMERA_OK != pme->cam->ops->qbuf(pme->cam->camera_handle,
                                                recvd_frame->ch_id,
                                                recvd_frame->bufs[i])) {
            CDBG_ERROR("%s: Failed in Qbuf\n", __func__);
        }
        mm_app_cache_ops((mm_camera_app_meminfo_t *) recvd_frame->bufs[i]->mem_info,
                         ION_IOC_INV_CACHES);
    }
}

