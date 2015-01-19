/*
Copyright (c) 2012, The Linux Foundation. All rights reserved.

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
#include "mm_camera_dbg.h"
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <linux/ion.h>
#include "mm_qcamera_app.h"

extern int system_dimension_set(int cam_id);
extern void dumpFrameToFile(mm_camera_buf_def_t* newFrame, int w, int h, char* name, int main_422);

int32_t mm_qcamera_queue_init(mm_qcamera_queue_t* queue);
int32_t mm_qcamera_queue_enq(mm_qcamera_queue_t* queue, void* data);
void* mm_qcamera_queue_deq(mm_qcamera_queue_t* queue);
void* mm_qcamera_queue_peek(mm_qcamera_queue_t* queue);
int32_t mm_qcamera_queue_deinit(mm_qcamera_queue_t* queue);
int32_t mm_qcamera_queue_flush(mm_qcamera_queue_t* queue);

static void mm_app_preview_pp_notify_cb(mm_camera_super_buf_t *bufs,
                                        void *user_data)
{
    int rc;
    mm_camera_buf_def_t *frame = NULL;
    mm_camera_app_obj_t *pme = NULL;
    repro_src_buf_info *buf_info = NULL;

    CDBG("%s: BEGIN\n", __func__);
    frame = bufs->bufs[0];
    pme = (mm_camera_app_obj_t *)user_data;

    CDBG("%s: BEGIN - length=%d, frame idx = %d\n",
         __func__, frame->frame_len, frame->frame_idx);

    dumpFrameToFile(frame, pme->dim.display_width, pme->dim.display_height,"preview", 1);

    buf_info = (repro_src_buf_info *)mm_qcamera_queue_peek(&pme->repro_q);
    if (NULL != buf_info) {
        buf_info->ref_cnt--;
        if (0 == buf_info->ref_cnt) {
            mm_qcamera_queue_deq(&pme->repro_q);
            pme->cam->ops->qbuf(pme->cam->camera_handle, pme->ch_id, buf_info->frame);
            mm_stream_invalid_cache(pme, buf_info->frame);
            buf_info->frame = NULL;
        }
    }

    if (MM_CAMERA_OK != pme->cam->ops->qbuf(pme->cam->camera_handle, pme->ch_id, frame)) {
        CDBG_ERROR("%s: Failed in Preview Qbuf\n", __func__);
        return;
    }
    mm_stream_invalid_cache(pme,frame);
    CDBG("%s: END\n", __func__);
}

static void mm_app_video_pp_notify_cb(mm_camera_super_buf_t *bufs,
                                      void *user_data)
{
	int rc;
	mm_camera_buf_def_t *frame = NULL;
	mm_camera_app_obj_t *pme = NULL;
    repro_src_buf_info *buf_info = NULL;

	CDBG("%s: BEGIN\n", __func__);
	frame = bufs->bufs[0] ;
	pme = (mm_camera_app_obj_t *)user_data;

	CDBG("%s: BEGIN - length=%d, frame idx = %d\n",
         __func__, frame->frame_len, frame->frame_idx);

	dumpFrameToFile(frame,pme->dim.orig_video_width,pme->dim.orig_video_height,"video", 1);

    buf_info = (repro_src_buf_info *)mm_qcamera_queue_peek(&pme->repro_q);
    if (NULL != buf_info) {
        buf_info->ref_cnt--;
        if (0 == buf_info->ref_cnt) {
            mm_qcamera_queue_deq(&pme->repro_q);
            pme->cam->ops->qbuf(pme->cam->camera_handle, pme->ch_id, buf_info->frame);
            mm_stream_invalid_cache(pme, buf_info->frame);
            buf_info->frame = NULL;
        }
    }

    if(MM_CAMERA_OK != pme->cam->ops->qbuf(pme->cam->camera_handle, pme->ch_id, frame))
    {
        CDBG_ERROR("%s: Failed in Snapshot Qbuf\n", __func__);
        return;
    }
    mm_stream_invalid_cache(pme,frame);
    CDBG("%s: END\n", __func__);
}

static void mm_app_isp_pix_notify_cb(mm_camera_super_buf_t *bufs,
                                     void *user_data)
{
    int rc = MM_CAMERA_OK;
    mm_camera_buf_def_t *frame = NULL;
    mm_camera_app_obj_t *pme = NULL;
    mm_camera_repro_data_t repro_data;

    CDBG("%s: BEGIN\n", __func__);
    frame = bufs->bufs[0] ;
    pme = (mm_camera_app_obj_t *)user_data;

    CDBG("%s: BEGIN - length=%d, frame idx = %d\n",
     __func__, frame->frame_len, frame->frame_idx);

    pme->repro_buf_info[frame->buf_idx].frame = frame;
    pme->repro_buf_info[frame->buf_idx].ref_cnt = pme->repro_dest_num;
    mm_qcamera_queue_enq(&pme->repro_q,
                         (void *)&pme->repro_buf_info[frame->buf_idx]);

    memset(&repro_data, 0, sizeof(mm_camera_repro_data_t));
    repro_data.src_frame = frame;

    rc = pme->cam->ops->reprocess(
                                 pme->cam->camera_handle,
                                 pme->ch_id,
                                 pme->isp_repro_handle,
                                 &repro_data);
    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s: reprocess error = %d", __func__, rc);
    }

	CDBG("%s: END\n", __func__);
}

int prepareReprocess(int cam_id)
{
    int rc = MM_CAMERA_OK;
    int op_mode;
    mm_camera_repro_isp_config_t config;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    CDBG("%s: E",__func__);
    pme->mem_cam->get_buf = mm_stream_initbuf;
    pme->mem_cam->put_buf = mm_stream_deinitbuf;
    pme->mem_cam->user_data = pme;

    /* set mode */
    op_mode = MM_CAMERA_OP_MODE_VIDEO;
    if (MM_CAMERA_OK != (rc = pme->cam->ops->set_parm(
                                     pme->cam->camera_handle,
                                     MM_CAMERA_PARM_OP_MODE,
                                     &op_mode))) {
        CDBG_ERROR("%s: Set preview op mode error", __func__);
        goto end;
    }

    /* init reprocess queue */
    mm_qcamera_queue_init(&pme->repro_q);
    memset(pme->repro_buf_info, 0, sizeof(pme->repro_buf_info));

    /* add isp pix output1 stream */
    pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].id = pme->cam->ops->add_stream(
                                                        pme->cam->camera_handle,
                                                        pme->ch_id,
                                                        mm_app_isp_pix_notify_cb,
                                                        pme,
                                                        MM_CAMERA_ISP_PIX_OUTPUT1,
                                                        0);

    if (!pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].id) {
        CDBG_ERROR("%s:Add isp_pix_output1 error\n", __func__);
        rc = -1;
        goto end;
    }

    /* add preview stream */
    pme->stream[MM_CAMERA_PREVIEW].id = pme->cam->ops->add_stream(
                                                        pme->cam->camera_handle,
                                                        pme->ch_id,
                                                        mm_app_preview_pp_notify_cb,
                                                        pme,
                                                        MM_CAMERA_PREVIEW,
                                                        0);

    if (!pme->stream[MM_CAMERA_PREVIEW].id) {
        CDBG_ERROR("%s:Add stream preview error\n", __func__);
        rc = -1;
        pme->cam->ops->del_stream(pme->cam->camera_handle,
                                  pme->ch_id,
                                  pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].id);
        pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].id = 0;
        goto end;
    }

    /* add record stream */
    pme->stream[MM_CAMERA_VIDEO].id = pme->cam->ops->add_stream(
                                                        pme->cam->camera_handle,
                                                        pme->ch_id,
                                                        mm_app_video_pp_notify_cb,
                                                        pme,
                                                        MM_CAMERA_VIDEO,
                                                        0);

    if (!pme->stream[MM_CAMERA_VIDEO].id) {
        CDBG_ERROR("%s:Add stream video error\n", __func__);
        rc = -1;
        pme->cam->ops->del_stream(pme->cam->camera_handle,
                                  pme->ch_id,
                                  pme->stream[MM_CAMERA_PREVIEW].id);
        pme->stream[MM_CAMERA_PREVIEW].id = 0;
        pme->cam->ops->del_stream(pme->cam->camera_handle,
                                  pme->ch_id,
                                  pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].id);
        pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].id = 0;
        goto end;
    }

    /* open isp reprocess */
    pme->isp_repro_handle = pme->cam->ops->open_repro_isp(
                                                        pme->cam->camera_handle,
                                                        pme->ch_id,
                                                        MM_CAMERA_REPRO_ISP_PIX);

    if (!pme->isp_repro_handle) {
        CDBG_ERROR("%s:open isp reprocess error\n", __func__);
        rc = -1;
        pme->cam->ops->del_stream(pme->cam->camera_handle,
                                  pme->ch_id,
                                  pme->stream[MM_CAMERA_VIDEO].id);
        pme->stream[MM_CAMERA_VIDEO].id = 0;
        pme->cam->ops->del_stream(pme->cam->camera_handle,
                                  pme->ch_id,
                                  pme->stream[MM_CAMERA_PREVIEW].id);
        pme->stream[MM_CAMERA_PREVIEW].id = 0;
        pme->cam->ops->del_stream(pme->cam->camera_handle,
                                  pme->ch_id,
                                  pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].id);
        pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].id = 0;
        goto end;
    }

    /* prepare repro isp config */
    memset(&pme->repro_config, 0, sizeof(mm_camera_repro_config_data_t));
    pme->repro_config.image_mode = MM_CAMERA_ISP_PIX_OUTPUT1;
    if (pme->dim.display_width * pme->dim.display_height >
        pme->dim.video_width * pme->dim.video_height) {
        /* preview size is larger than video, use preview config as repro isp config */
        pme->repro_config.format = pme->dim.prev_format;
        pme->repro_config.width = pme->dim.display_width;
        pme->repro_config.height = pme->dim.display_height;
    } else {
        pme->repro_config.format = pme->dim.enc_format;
        pme->repro_config.width = pme->dim.video_width;
        pme->repro_config.height = pme->dim.video_height;
    }

    /* config isp reprocess */
    memset(&config, 0, sizeof(mm_camera_repro_isp_config_t));
    config.src.format = pme->repro_config.format;
    config.src.image_mode = pme->repro_config.image_mode;
    config.src.width = pme->repro_config.width;
    config.src.height = pme->repro_config.height;
    config.src.inst_handle = pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].id;

    pme->repro_dest_num = 0;
    config.num_dest = 2;
    config.dest[0].image_mode = MM_CAMERA_PREVIEW;
    config.dest[0].format = pme->dim.prev_format;
    config.dest[0].width = pme->dim.display_width;
    config.dest[0].height = pme->dim.display_height;
    config.dest[0].inst_handle = pme->stream[MM_CAMERA_PREVIEW].id;
    config.dest[1].image_mode = MM_CAMERA_VIDEO;
    config.dest[1].format = pme->dim.enc_format;
    config.dest[1].width = pme->dim.video_width;
    config.dest[1].height = pme->dim.video_height;
    config.dest[1].inst_handle = pme->stream[MM_CAMERA_VIDEO].id;

    rc = pme->cam->ops->config_repro_isp(
                                        pme->cam->camera_handle,
                                        pme->ch_id,
                                        pme->isp_repro_handle,
                                        &config);

    if (MM_CAMERA_OK != rc) {
        CDBG_ERROR("%s:config isp reprocess error = %d\n", __func__, rc);
        pme->cam->ops->close_repro_isp(pme->cam->camera_handle,
                                       pme->ch_id,
                                       pme->isp_repro_handle);
        pme->isp_repro_handle = 0;
        pme->cam->ops->del_stream(pme->cam->camera_handle,
                                  pme->ch_id,
                                  pme->stream[MM_CAMERA_VIDEO].id);
        pme->stream[MM_CAMERA_VIDEO].id = 0;
        pme->cam->ops->del_stream(pme->cam->camera_handle,
                                  pme->ch_id,
                                  pme->stream[MM_CAMERA_PREVIEW].id);
        pme->stream[MM_CAMERA_PREVIEW].id = 0;
        pme->cam->ops->del_stream(pme->cam->camera_handle,
                                  pme->ch_id,
                                  pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].id);
        pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].id = 0;
    }

end:
    CDBG("%s: X rc = %d", __func__, rc);
    return rc;
}

int unprepareReprocess(int cam_id)
{
    int rc = MM_CAMERA_OK;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    CDBG("%s: E",__func__);
    rc = pme->cam->ops->close_repro_isp(pme->cam->camera_handle,
                                        pme->ch_id,
                                        pme->isp_repro_handle);
    pme->isp_repro_handle = 0;
    mm_qcamera_queue_deinit(&pme->repro_q);

    CDBG("%s: X rc = %d", __func__, rc);
    return rc;
}

int startPreviewPP(int cam_id)
{
    int rc = MM_CAMERA_OK;
    uint32_t stream[1];
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    CDBG("%s: E",__func__);

    /* attach preview stream to isp reprocess pipe */
    if (MM_CAMERA_OK != (rc = pme->cam->ops->attach_stream_to_repro_isp(
                                                            pme->cam->camera_handle,
                                                            pme->ch_id,
                                                            pme->isp_repro_handle,
                                                            pme->stream[MM_CAMERA_PREVIEW].id))) {
        CDBG_ERROR("%s : Attach preview to reprocess error",__func__);
        goto end;
    }

    /* start isp reprocess pipe for preview */
    if (MM_CAMERA_OK != (rc = pme->cam->ops->start_repro_isp(pme->cam->camera_handle,
                                                            pme->ch_id,
                                                            pme->isp_repro_handle,
                                                            pme->stream[MM_CAMERA_PREVIEW].id))) {
        CDBG_ERROR("%s : Start reprocess for preview error",__func__);
        pme->cam->ops->detach_stream_from_repro_isp(
                                                pme->cam->camera_handle,
                                                pme->ch_id,
                                                pme->isp_repro_handle,
                                                pme->stream[MM_CAMERA_PREVIEW].id);
        goto end;
    }

    /* increase actual reprocess dest num */
    pme->repro_dest_num++;

    /* start preview stream */
    pme->stream[MM_CAMERA_PREVIEW].str_config.fmt.meta_header = MM_CAMEAR_META_DATA_TYPE_DEF;
    pme->stream[MM_CAMERA_PREVIEW].str_config.fmt.fmt = pme->dim.prev_format;
    pme->stream[MM_CAMERA_PREVIEW].str_config.fmt.width = pme->dim.display_width;
    pme->stream[MM_CAMERA_PREVIEW].str_config.fmt.height = pme->dim.display_height;
    pme->stream[MM_CAMERA_PREVIEW].str_config.need_stream_on = 1;
    pme->stream[MM_CAMERA_PREVIEW].str_config.num_of_bufs = PREVIEW_BUF_NUM;
    if (MM_CAMERA_OK != (rc = pme->cam->ops->config_stream(pme->cam->camera_handle,
                                                           pme->ch_id,
                                                           pme->stream[MM_CAMERA_PREVIEW].id,
                                                           &pme->stream[MM_CAMERA_PREVIEW].str_config))) {
        CDBG_ERROR("%s:config preview err=%d\n", __func__, rc);
        pme->cam->ops->stop_repro_isp(
                                    pme->cam->camera_handle,
                                    pme->ch_id,
                                    pme->isp_repro_handle,
                                    pme->stream[MM_CAMERA_PREVIEW].id);
        pme->cam->ops->detach_stream_from_repro_isp(
                                                pme->cam->camera_handle,
                                                pme->ch_id,
                                                pme->isp_repro_handle,
                                                pme->stream[MM_CAMERA_PREVIEW].id);
        pme->repro_dest_num--;
        goto end;
    }
    stream[0] = pme->stream[MM_CAMERA_PREVIEW].id;
    if (MM_CAMERA_OK != (rc = pme->cam->ops->start_streams(pme->cam->camera_handle,
                                                          pme->ch_id,
                                                          1,
                                                          stream))) {
        CDBG_ERROR("%s : Preview Stream on Error",__func__);
        pme->cam->ops->stop_repro_isp(
                                    pme->cam->camera_handle,
                                    pme->ch_id,
                                    pme->isp_repro_handle,
                                    pme->stream[MM_CAMERA_PREVIEW].id);
        pme->cam->ops->detach_stream_from_repro_isp(
                                                pme->cam->camera_handle,
                                                pme->ch_id,
                                                pme->isp_repro_handle,
                                                pme->stream[MM_CAMERA_PREVIEW].id);
        pme->repro_dest_num--;
        goto end;
    }

    /* start isp pix output1 stream */
    pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].str_config.fmt.meta_header = MM_CAMEAR_META_DATA_TYPE_DEF;
    pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].str_config.fmt.fmt = pme->repro_config.format;
    pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].str_config.fmt.width = pme->repro_config.width;
    pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].str_config.fmt.height = pme->repro_config.height;
    pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].str_config.need_stream_on = 1;
    pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].str_config.num_of_bufs = ISP_PIX_BUF_NUM;
    if (MM_CAMERA_OK != (rc = pme->cam->ops->config_stream(pme->cam->camera_handle,
                                                           pme->ch_id,
                                                           pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].id,
                                                           &pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].str_config))) {
        CDBG_ERROR("%s:config isp pix stream err=%d\n", __func__, rc);
        pme->cam->ops->stop_repro_isp(
                                    pme->cam->camera_handle,
                                    pme->ch_id,
                                    pme->isp_repro_handle,
                                    pme->stream[MM_CAMERA_PREVIEW].id);
        pme->cam->ops->detach_stream_from_repro_isp(
                                                pme->cam->camera_handle,
                                                pme->ch_id,
                                                pme->isp_repro_handle,
                                                pme->stream[MM_CAMERA_PREVIEW].id);
        pme->repro_dest_num--;
    }
    stream[0] = pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].id;
    if (MM_CAMERA_OK != (rc = pme->cam->ops->start_streams(pme->cam->camera_handle,
                                                          pme->ch_id,
                                                          1,
                                                          stream))) {
        CDBG_ERROR("%s : Preview Stream on Error",__func__);
        pme->cam->ops->stop_repro_isp(
                                    pme->cam->camera_handle,
                                    pme->ch_id,
                                    pme->isp_repro_handle,
                                    pme->stream[MM_CAMERA_PREVIEW].id);
        pme->cam->ops->detach_stream_from_repro_isp(
                                                pme->cam->camera_handle,
                                                pme->ch_id,
                                                pme->isp_repro_handle,
                                                pme->stream[MM_CAMERA_PREVIEW].id);
        pme->repro_dest_num--;
        goto end;
    }

end:
    CDBG("%s: X rc=%d\n", __func__, rc);
    return rc;
}

int stopPreviewPP(int cam_id)
{
    int rc = MM_CAMERA_OK;
    uint32_t stream[1];
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    CDBG("%s: E",__func__);

    /* stop preview stream */
    stream[0] = pme->stream[MM_CAMERA_PREVIEW].id;
    if (MM_CAMERA_OK != (rc = pme->cam->ops->stop_streams(pme->cam->camera_handle,
                                                          pme->ch_id,
                                                          1,
                                                          stream))) {
        CDBG_ERROR("%s : Preview Stream off Error",__func__);
    }

    if (MM_CAMERA_OK != (rc = pme->cam->ops->del_stream(pme->cam->camera_handle,
                                                        pme->ch_id,
                                                        pme->stream[MM_CAMERA_PREVIEW].id))) {
        CDBG_ERROR("%s : Delete Stream Preview error",__func__);
    }

    /* stop isp reprocess pipe for preview */
    if (MM_CAMERA_OK != (rc = pme->cam->ops->stop_repro_isp(pme->cam->camera_handle,
                                                            pme->ch_id,
                                                            pme->isp_repro_handle,
                                                            pme->stream[MM_CAMERA_PREVIEW].id))) {
        CDBG_ERROR("%s : Stop reprocess for preview error",__func__);
    }

    /* decrease actual reprocess dest num */
    pme->repro_dest_num--;

    /* detach preview stream from isp reprocess pipe */
    if (MM_CAMERA_OK != (rc = pme->cam->ops->detach_stream_from_repro_isp(
                                                            pme->cam->camera_handle,
                                                            pme->ch_id,
                                                            pme->isp_repro_handle,
                                                            pme->stream[MM_CAMERA_PREVIEW].id))) {
        CDBG_ERROR("%s : Detach preview from reprocess error",__func__);
    }

    /* stop isp pix output1 stream */
    stream[0] = pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].id;
    if (MM_CAMERA_OK != (rc = pme->cam->ops->stop_streams(pme->cam->camera_handle,
                                                          pme->ch_id,
                                                          1,
                                                          stream))) {
        CDBG_ERROR("%s : ISP pix Stream off Error",__func__);
    }

    if (MM_CAMERA_OK != (rc = pme->cam->ops->del_stream(pme->cam->camera_handle,
                                                        pme->ch_id,
                                                        pme->stream[MM_CAMERA_ISP_PIX_OUTPUT1].id))) {
        CDBG_ERROR("%s : Delete Stream isp pix error",__func__);
    }

end:
    CDBG("%s: X rc=%d\n", __func__, rc);
    return rc;
}

int startRecordPP(int cam_id)
{
    int rc = MM_CAMERA_OK;
    uint32_t stream[1];
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    CDBG("%s: E",__func__);

    /* attach video stream to isp reprocess pipe */
    if (MM_CAMERA_OK != (rc = pme->cam->ops->attach_stream_to_repro_isp(
                                                            pme->cam->camera_handle,
                                                            pme->ch_id,
                                                            pme->isp_repro_handle,
                                                            pme->stream[MM_CAMERA_VIDEO].id))) {
        CDBG_ERROR("%s : Attach video to reprocess error",__func__);
        goto end;
    }

    /* start isp reprocess pipe for video */
    if (MM_CAMERA_OK != (rc = pme->cam->ops->start_repro_isp(pme->cam->camera_handle,
                                                            pme->ch_id,
                                                            pme->isp_repro_handle,
                                                            pme->stream[MM_CAMERA_VIDEO].id))) {
        CDBG_ERROR("%s : Start reprocess for video error",__func__);
        pme->cam->ops->detach_stream_from_repro_isp(
                                                pme->cam->camera_handle,
                                                pme->ch_id,
                                                pme->isp_repro_handle,
                                                pme->stream[MM_CAMERA_VIDEO].id);
        goto end;
    }

    /* increase actual reprocess dest num */
    pme->repro_dest_num++;

    /* start preview stream */
    pme->stream[MM_CAMERA_VIDEO].str_config.fmt.meta_header = MM_CAMEAR_META_DATA_TYPE_DEF;
    pme->stream[MM_CAMERA_VIDEO].str_config.fmt.fmt = pme->dim.enc_format;
    pme->stream[MM_CAMERA_VIDEO].str_config.fmt.width = pme->dim.video_width;
    pme->stream[MM_CAMERA_VIDEO].str_config.fmt.height = pme->dim.video_height;
    pme->stream[MM_CAMERA_VIDEO].str_config.need_stream_on = 1;
    pme->stream[MM_CAMERA_VIDEO].str_config.num_of_bufs = VIDEO_BUF_NUM;
    if (MM_CAMERA_OK != (rc = pme->cam->ops->config_stream(pme->cam->camera_handle,
                                                           pme->ch_id,
                                                           pme->stream[MM_CAMERA_VIDEO].id,
                                                           &pme->stream[MM_CAMERA_VIDEO].str_config))) {
        CDBG_ERROR("%s:config video err=%d\n", __func__, rc);
        pme->cam->ops->stop_repro_isp(
                                    pme->cam->camera_handle,
                                    pme->ch_id,
                                    pme->isp_repro_handle,
                                    pme->stream[MM_CAMERA_VIDEO].id);
        pme->cam->ops->detach_stream_from_repro_isp(
                                                pme->cam->camera_handle,
                                                pme->ch_id,
                                                pme->isp_repro_handle,
                                                pme->stream[MM_CAMERA_VIDEO].id);
        pme->repro_dest_num--;
        goto end;
    }
    stream[0] = pme->stream[MM_CAMERA_VIDEO].id;
    if (MM_CAMERA_OK != (rc = pme->cam->ops->start_streams(pme->cam->camera_handle,
                                                          pme->ch_id,
                                                          1,
                                                          stream))) {
        CDBG_ERROR("%s : Preview Stream on Error",__func__);
        pme->cam->ops->stop_repro_isp(
                                    pme->cam->camera_handle,
                                    pme->ch_id,
                                    pme->isp_repro_handle,
                                    pme->stream[MM_CAMERA_VIDEO].id);
        pme->cam->ops->detach_stream_from_repro_isp(
                                                pme->cam->camera_handle,
                                                pme->ch_id,
                                                pme->isp_repro_handle,
                                                pme->stream[MM_CAMERA_VIDEO].id);
        pme->repro_dest_num--;
        goto end;
    }

end:
    CDBG("%s: X rc=%d\n", __func__, rc);
    return rc;
}

int stopRecordPP(int cam_id)
{
    int rc = MM_CAMERA_OK;
    uint32_t stream[1];
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    CDBG("%s: E",__func__);

    /* stop video stream */
    stream[0] = pme->stream[MM_CAMERA_VIDEO].id;
    if (MM_CAMERA_OK != (rc = pme->cam->ops->stop_streams(pme->cam->camera_handle,
                                                          pme->ch_id,
                                                          1,
                                                          stream))) {
        CDBG_ERROR("%s : Video Stream off Error",__func__);
    }

    if (MM_CAMERA_OK != (rc = pme->cam->ops->del_stream(pme->cam->camera_handle,
                                                        pme->ch_id,
                                                        pme->stream[MM_CAMERA_VIDEO].id))) {
        CDBG_ERROR("%s : Delete Stream Preview error",__func__);
    }

    /* stop isp reprocess pipe for video */
    if (MM_CAMERA_OK != (rc = pme->cam->ops->stop_repro_isp(pme->cam->camera_handle,
                                                            pme->ch_id,
                                                            pme->isp_repro_handle,
                                                            pme->stream[MM_CAMERA_VIDEO].id))) {
        CDBG_ERROR("%s : Stop reprocess for video error",__func__);
    }

    /* decrease actual reprocess dest num */
    pme->repro_dest_num--;

    /* detach preview stream from isp reprocess pipe */
    if (MM_CAMERA_OK != (rc = pme->cam->ops->detach_stream_from_repro_isp(
                                                            pme->cam->camera_handle,
                                                            pme->ch_id,
                                                            pme->isp_repro_handle,
                                                            pme->stream[MM_CAMERA_VIDEO].id))) {
        CDBG_ERROR("%s : Detach video from reprocess error",__func__);
    }

    CDBG("%s: X rc=%d\n", __func__, rc);
    return rc;
}

int32_t mm_qcamera_queue_init(mm_qcamera_queue_t* queue)
{
    pthread_mutex_init(&queue->lock, NULL);
    cam_list_init(&queue->head.list);
    queue->size = 0;
    return 0;
}

int32_t mm_qcamera_queue_enq(mm_qcamera_queue_t* queue, void* data)
{
    mm_qcamera_q_node_t* node =
        (mm_qcamera_q_node_t *)malloc(sizeof(mm_qcamera_q_node_t));
    if (NULL == node) {
        CDBG_ERROR("%s: No memory for mm_camera_q_node_t", __func__);
        return -1;
    }

    memset(node, 0, sizeof(mm_qcamera_q_node_t));
    node->data = data;

    pthread_mutex_lock(&queue->lock);
    cam_list_add_tail_node(&node->list, &queue->head.list);
    queue->size++;
    pthread_mutex_unlock(&queue->lock);

    return 0;

}

void* mm_qcamera_queue_deq(mm_qcamera_queue_t* queue)
{
    mm_qcamera_q_node_t* node = NULL;
    void* data = NULL;
    struct cam_list *head = NULL;
    struct cam_list *pos = NULL;

    pthread_mutex_lock(&queue->lock);
    head = &queue->head.list;
    pos = head->next;
    if (pos != head) {
        node = member_of(pos, mm_qcamera_q_node_t, list);
        cam_list_del_node(&node->list);
        queue->size--;
    }
    pthread_mutex_unlock(&queue->lock);

    if (NULL != node) {
        data = node->data;
        free(node);
    }

    return data;
}

void* mm_qcamera_queue_peek(mm_qcamera_queue_t* queue)
{
    mm_qcamera_q_node_t* node = NULL;
    void* data = NULL;
    struct cam_list *head = NULL;
    struct cam_list *pos = NULL;

    pthread_mutex_lock(&queue->lock);
    head = &queue->head.list;
    pos = head->next;
    if (pos != head) {
        node = member_of(pos, mm_qcamera_q_node_t, list);
    }
    pthread_mutex_unlock(&queue->lock);

    if (NULL != node) {
        data = node->data;
    }

    return data;
}

int32_t mm_qcamera_queue_deinit(mm_qcamera_queue_t* queue)
{
    mm_qcamera_queue_flush(queue);
    pthread_mutex_destroy(&queue->lock);
    return 0;
}

int32_t mm_qcamera_queue_flush(mm_qcamera_queue_t* queue)
{
    mm_qcamera_q_node_t* node = NULL;
    void* data = NULL;
    struct cam_list *head = NULL;
    struct cam_list *pos = NULL;

    pthread_mutex_lock(&queue->lock);
    head = &queue->head.list;
    pos = head->next;

    while(pos != head) {
        node = member_of(pos, mm_qcamera_q_node_t, list);
        pos = pos->next;
        cam_list_del_node(&node->list);
        queue->size--;

        /* TODO later to consider ptr inside data */
        /* for now we only assume there is no ptr inside data
         * so we free data directly */
        if (NULL != node->data) {
            free(node->data);
        }
        free(node);

    }
    queue->size = 0;
    pthread_mutex_unlock(&queue->lock);
    return 0;
}

/* re-processing test case for preview only */
int mm_app_tc_reprocess_preview_only(mm_camera_app_t *cam_apps)
{
    int rc = MM_CAMERA_OK;
    int i,j,k;
    int result = 0;

    printf("\n Verifying reprocess for preview on front and back camera...\n");
    if (cam_apps->num_cameras == 0) {
        CDBG_ERROR("%s:Query Failed: Num of cameras = %d\n",__func__, cam_apps->num_cameras);
        rc = -1;
        goto end;
    }
    CDBG_ERROR("Re-processing test case for preview");
    for (i = 1; i < cam_apps->num_cameras; i++) {
        CDBG_ERROR("Open camera %d", i);
        if ( mm_app_open(i) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if (system_dimension_set(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        CDBG_ERROR("Prepare re-process");
        if ( MM_CAMERA_OK != (rc = prepareReprocess(my_cam_app.cam_open))) {
            CDBG_ERROR("%s: prepareReprocess() err=%d\n", __func__, rc);
            break;
        }
        result = 0;
        for (k = 0; k < MM_QCAMERA_APP_INTERATION; k++) {
          CDBG_ERROR("Start preview with pp");
          if ( MM_CAMERA_OK != (rc = startPreviewPP(my_cam_app.cam_open))) {
            CDBG_ERROR("%s: startPreviewPP() err=%d\n", __func__, rc);
            break;
          }

          CDBG_ERROR("Let preview run for 1s");
          /* sleep for 1s */
          usleep(1000*1000);

          CDBG_ERROR("Stop preview with pp");
          if ( MM_CAMERA_OK != (rc = stopPreviewPP(my_cam_app.cam_open))) {
            CDBG_ERROR("%s: stopPreviewPP() err=%d\n", __func__, rc);
            break;
          }
          result++;
          usleep(10*1000);
        }

        CDBG_ERROR("Unprepare re-process");
        if ( MM_CAMERA_OK != (rc = unprepareReprocess(my_cam_app.cam_open))) {
            CDBG_ERROR("%s: unprepareReprocess() err=%d\n", __func__, rc);
        }

        CDBG_ERROR("Close camera");
        if ( mm_app_close(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if (result != MM_QCAMERA_APP_INTERATION) {
            printf("%s: preview re-reprocess Fails for Camera %d", __func__, i);
            rc = -1;
            break;
        }
    }
end:
    if (rc == 0) {
        printf("\nPassed\n");
    } else {
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

/* re-processing test case for preview and recording */
int mm_app_tc_reprocess_preview_and_recording(mm_camera_app_t *cam_apps)
{
    int rc = MM_CAMERA_OK;
    int i,j,k;
    int result = 0;

    printf("\n Verifying reprocess for preview/recording on front and back camera...\n");
    if (cam_apps->num_cameras == 0) {
        CDBG_ERROR("%s:Query Failed: Num of cameras = %d\n",__func__, cam_apps->num_cameras);
        rc = -1;
        goto end;
    }
    CDBG_ERROR("Re-processing test case for preview");
    for (i = 1; i < cam_apps->num_cameras; i++) {
        CDBG_ERROR("Open camera %d", i);
        if ( mm_app_open(i) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if (system_dimension_set(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        CDBG_ERROR("Prepare re-process");
        if ( MM_CAMERA_OK != (rc = prepareReprocess(my_cam_app.cam_open))) {
            CDBG_ERROR("%s: prepareReprocess() err=%d\n", __func__, rc);
            break;
        }
        result = 0;
        for (k = 0; k < MM_QCAMERA_APP_INTERATION; k++) {
          CDBG_ERROR("Start preview with pp");
          if ( MM_CAMERA_OK != (rc = startPreviewPP(my_cam_app.cam_open))) {
            CDBG_ERROR("%s: startPreviewPP() err=%d\n", __func__, rc);
            break;
          }

          CDBG_ERROR("Let preview run for 1s");
          /* sleep for 1s */
          usleep(1000*1000);

          for (j = 0; j < 2; j++) {
              CDBG_ERROR("Start recording with pp");
              if ( MM_CAMERA_OK != (rc = startRecordPP(my_cam_app.cam_open))) {
                CDBG_ERROR("%s: startRecordPP() err=%d\n", __func__, rc);
                break;
              }

              CDBG_ERROR("Let recording run for 1s");
              /* sleep for 1s */
              usleep(1000*1000);

              CDBG_ERROR("Stop recording with pp");
              if ( MM_CAMERA_OK != (rc = stopRecordPP(my_cam_app.cam_open))) {
                CDBG_ERROR("%s: stopRecordPP() err=%d\n", __func__, rc);
                break;
              }
          }

          CDBG_ERROR("Stop preview with pp");
          if ( MM_CAMERA_OK != (rc = stopPreviewPP(my_cam_app.cam_open))) {
            CDBG_ERROR("%s: stopPreviewPP() err=%d\n", __func__, rc);
            break;
          }
          result++;
          usleep(10*1000);
        }

        CDBG_ERROR("Unprepare re-process");
        if ( MM_CAMERA_OK != (rc = unprepareReprocess(my_cam_app.cam_open))) {
            CDBG_ERROR("%s: unprepareReprocess() err=%d\n", __func__, rc);
        }

        CDBG_ERROR("Close camera");
        if ( mm_app_close(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if (result != MM_QCAMERA_APP_INTERATION) {
            printf("%s: preview+recording re-reprocess Fails for Camera %d", __func__, i);
            rc = -1;
            break;
        }
    }
end:
    if (rc == 0) {
        printf("\nPassed\n");
    } else {
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

