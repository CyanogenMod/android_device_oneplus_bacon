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
#include "mm_camera_dbg.h"
#include "mm_qcamera_app.h"

#define BUFF_SIZE_128 128

#ifdef TEST_ABORT_JPEG_ENCODE
static uint8_t aborted_flag = 1;
#endif

//static mm_camera_ch_data_buf_t *mCurrentFrameEncoded;
static int JpegOffset = 0;
static int raw_snapshot_cnt = 0;
static int snapshot_cnt = 0;
static pthread_mutex_t g_s_mutex;
static int g_status = 0;
static pthread_cond_t g_s_cond_v;

extern void dumpFrameToFile(mm_camera_buf_def_t* newFrame, int w, int h, char* name, int main_422,char *ext);
extern int mm_app_open_zsl(int cam_id);
extern int mm_app_open_camera(int cam_id);
extern int mm_app_start_preview(int cam_id);
extern int mm_stream_alloc_bufs(mm_camera_app_obj_t *pme,
                                mm_camear_app_buf_t* app_bufs,
                                mm_camera_frame_len_offset *frame_offset_info,
                                uint8_t num_bufs);
extern int mm_stream_release_bufs(mm_camera_app_obj_t *pme,
                                  mm_camear_app_buf_t* app_bufs);
extern int mm_stream_invalid_cache(mm_camera_app_obj_t *pme,
                                   mm_camera_buf_def_t *frame);

static void mm_app_snapshot_done()
{
    pthread_mutex_lock(&g_s_mutex);
    g_status = TRUE;
    pthread_cond_signal(&g_s_cond_v);
    pthread_mutex_unlock(&g_s_mutex);
}

static int mm_app_dump_snapshot_frame(struct msm_frame *frame,
                                      uint32_t len, int is_main, int is_raw)
{
    char bufp[BUFF_SIZE_128];
    int file_fdp;
    int rc = 0;

    if (is_raw) {
        snprintf(bufp, BUFF_SIZE_128, "/data/main_raw_%d.yuv", raw_snapshot_cnt);
    } else {
        if (is_main) {
            snprintf(bufp, BUFF_SIZE_128, "/data/main_%d.yuv", snapshot_cnt);
        } else {
            snprintf(bufp, BUFF_SIZE_128, "/data/thumb_%d.yuv", snapshot_cnt);
        }
    }

    file_fdp = open(bufp, O_RDWR | O_CREAT, 0777);

    if (file_fdp < 0) {
        CDBG("cannot open file %s\n", bufp);
        rc = -1;
        goto end;
    }
    CDBG("%s:dump snapshot frame to '%s'\n", __func__, bufp);
    write(file_fdp,
          (const void *)frame->buffer, len);
    close(file_fdp);
    end:
    return rc;
}


static void mm_app_dump_jpeg_frame(const void * data, uint32_t size, char* name, char* ext, int index)
{
    char buf[32];
    int file_fd;
    if ( data != NULL) {
        char * str;
        snprintf(buf, sizeof(buf), "/data/%s_%d.%s", name, index, ext);
        CDBG("%s: %s size =%d, jobId=%d", __func__, buf, size, index);
        file_fd = open(buf, O_RDWR | O_CREAT, 0777);
        write(file_fd, data, size);
        close(file_fd);
    }
}

static int mm_app_set_thumbnail_fmt(int cam_id, mm_camera_image_fmt_t *fmt)
{
    int rc = MM_CAMERA_OK;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    fmt->meta_header = MM_CAMEAR_META_DATA_TYPE_DEF;
    fmt->fmt = pme->dim.thumb_format;
    fmt->width = pme->dim.thumbnail_width;
    fmt->height = pme->dim.thumbnail_height;
    if (cam_id == 0) {
        /* back camera, rotate 90 */
        fmt->rotation = 90;
    }
    CDBG("Thumbnail Dimension = %dX%d",fmt->width,fmt->height);
    return rc;
}

int mm_app_set_snapshot_fmt(int cam_id, mm_camera_image_fmt_t *fmt)
{

    int rc = MM_CAMERA_OK;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);
    fmt->meta_header = MM_CAMEAR_META_DATA_TYPE_DEF;
    fmt->fmt = pme->dim.main_img_format;
    fmt->width = pme->dim.picture_width;
    fmt->height = pme->dim.picture_height;
    if (cam_id == 0) {
        /* back camera, rotate 90 */
        fmt->rotation = 90;
    }
    CDBG("Snapshot Dimension = %dX%d",fmt->width,fmt->height);
    return rc;
}

int mm_app_set_live_snapshot_fmt(int cam_id, mm_camera_image_fmt_t *fmt)
{
    int rc = MM_CAMERA_OK;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);
    fmt->meta_header = MM_CAMEAR_META_DATA_TYPE_DEF;
    fmt->fmt = pme->dim.enc_format;
    fmt->width = pme->dim.video_width;
    fmt->height = pme->dim.video_height;
    CDBG("Livesnapshot Dimension = %dX%d",fmt->width,fmt->height);
    return rc;
}

int mm_app_set_raw_snapshot_fmt(int cam_id,mm_camera_image_fmt_t *fmt)
{
    int rc = MM_CAMERA_OK;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    fmt->meta_header = MM_CAMEAR_META_DATA_TYPE_DEF;
    fmt->fmt = pme->dim.raw_img_format;
    fmt->width = pme->dim.raw_picture_width;
    fmt->height = pme->dim.raw_picture_height;
    if (cam_id == 0) {
        /* back camera, rotate 90 */
        fmt->rotation = 90;
    }
    CDBG("%s: Raw Snapshot Dimension = %d X %d",__func__,pme->dim.raw_picture_width,pme->dim.raw_picture_height);
    return rc;
}

/* This callback is received once the complete JPEG encoding is done */
static void jpeg_encode_cb(jpeg_job_status_t status,
                           uint8_t thumbnailDroppedFlag,
                           uint32_t client_hdl,
                           uint32_t jobId,
                           uint8_t* out_data,
                           uint32_t data_size,
                           void *userData)
{
    int rc;
    int i = 0;
    mm_camera_buf_def_t *main_frame = NULL;
    mm_camera_buf_def_t *thumb_frame = NULL;
    mm_camera_app_obj_t *pme = NULL;
    CDBG("%s: BEGIN\n", __func__);

    pme = (mm_camera_app_obj_t *)userData;
    if (jobId != pme->current_job_id || !pme->current_job_frames) {
        CDBG_ERROR("%s: NULL current job frames or not matching job ID (%d, %d)",
                   __func__, jobId, pme->current_job_id);
        return;
    }

    /* dump jpeg img */
    CDBG_ERROR("%s: job %d, status=%d, thumbnail_dropped=%d",
               __func__, jobId, status, thumbnailDroppedFlag);
    if (status == JPEG_JOB_STATUS_DONE) {
        mm_app_dump_jpeg_frame(out_data, data_size, "jpeg_dump", "jpg", pme->my_id);
    }

    /* buf done current encoding frames */
    pme->current_job_id = 0;
    for (i=0; i<pme->current_job_frames->num_bufs; i++) {
        if (MM_CAMERA_OK != pme->cam->ops->qbuf(pme->current_job_frames->camera_handle,
                                                pme->current_job_frames->ch_id,
                                                pme->current_job_frames->bufs[i])) {
            CDBG_ERROR("%s: Failed in Qbuf\n", __func__);
        }
    }
    free(pme->current_job_frames);
    pme->current_job_frames = NULL;

    /* signal snapshot is done */
    mm_app_snapshot_done();
}

static int encodeData(mm_camera_app_obj_t *pme,
                      mm_camera_super_buf_t* recvd_frame)
{
    int rc = -1;
    int i,index = -1;
    mm_jpeg_job job;
    mm_camera_buf_def_t *main_frame = NULL;
    mm_camera_buf_def_t *thumb_frame = NULL;
    src_image_buffer_info* main_buf_info = NULL;
    src_image_buffer_info* thumb_buf_info = NULL;
    mm_camera_frame_len_offset main_offset;
    mm_camera_frame_len_offset thumb_offset;

    /* dump raw img for debug purpose */
    CDBG("%s : total streams = %d",__func__,recvd_frame->num_bufs);
    for (i=0; i<recvd_frame->num_bufs; i++) {
        if (pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id == recvd_frame->bufs[i]->stream_id) {
            main_frame = recvd_frame->bufs[i];
            CDBG("mainframe frame_idx = %d fd = %d frame length = %d",main_frame->frame_idx,main_frame->fd,main_frame->frame_len);
            dumpFrameToFile(main_frame,pme->dim.picture_width,pme->dim.picture_height,"main", 1,"yuv");
        } else if (pme->stream[MM_CAMERA_SNAPSHOT_THUMBNAIL].id == recvd_frame->bufs[i]->stream_id){
            thumb_frame = recvd_frame->bufs[1];
            CDBG("thumnail frame_idx = %d fd = %d frame length = %d",thumb_frame->frame_idx,thumb_frame->fd,thumb_frame->frame_len);
            dumpFrameToFile(thumb_frame,pme->dim.thumbnail_width,pme->dim.thumbnail_height,"thumb", 1,"yuv");
        }
    }

    if (main_frame == NULL) {
        CDBG_ERROR("%s: Main frame is NULL", __func__);
        return rc;
    }

    /* remember current frames being encoded */
    pme->current_job_frames =
        (mm_camera_super_buf_t *)malloc(sizeof(mm_camera_super_buf_t));
    if (!pme->current_job_frames) {
        CDBG_ERROR("%s: No memory for current_job_frames", __func__);
        return rc;
    }
    memcpy(pme->current_job_frames, recvd_frame, sizeof(mm_camera_super_buf_t));

    memset(&job, 0, sizeof(job));
    job.job_type = JPEG_JOB_TYPE_ENCODE;
    job.encode_job.jpeg_cb = jpeg_encode_cb;
    job.encode_job.userdata = (void*)pme;
    job.encode_job.encode_parm.exif_data = NULL;
    job.encode_job.encode_parm.exif_numEntries = 0;
    job.encode_job.encode_parm.rotation = 0;
    job.encode_job.encode_parm.buf_info.src_imgs.src_img_num = recvd_frame->num_bufs;
    job.encode_job.encode_parm.rotation = 0;
    if (pme->my_id == 0) {
        /* back camera, rotate 90 */
        job.encode_job.encode_parm.rotation = 90;
    }

    /* fill in main src img encode param */
    main_buf_info = &job.encode_job.encode_parm.buf_info.src_imgs.src_img[JPEG_SRC_IMAGE_TYPE_MAIN];
    main_buf_info->type = JPEG_SRC_IMAGE_TYPE_MAIN;
    main_buf_info->color_format = MM_JPEG_COLOR_FORMAT_YCRCBLP_H2V2; //TODO
    main_buf_info->quality = 85;
    main_buf_info->src_dim.width = pme->dim.picture_width;
    main_buf_info->src_dim.height = pme->dim.picture_height;
    main_buf_info->out_dim.width = pme->dim.picture_width;
    main_buf_info->out_dim.height = pme->dim.picture_height;
    main_buf_info->crop.width = pme->dim.picture_width;
    main_buf_info->crop.height = pme->dim.picture_height;
    main_buf_info->crop.offset_x = 0;
    main_buf_info->crop.offset_y = 0;
    main_buf_info->img_fmt = JPEG_SRC_IMAGE_FMT_YUV;
    main_buf_info->num_bufs = 1;
    main_buf_info->src_image[0].fd = main_frame->fd;
    main_buf_info->src_image[0].buf_vaddr = main_frame->buffer;
    pme->cam->ops->get_stream_parm(pme->cam->camera_handle,
                                   pme->ch_id,
                                   pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id,
                                   MM_CAMERA_STREAM_OFFSET,
                                   &main_buf_info->src_image[0].offset);
    CDBG("%s: main offset: num_planes=%d, frame length=%d, y_offset=%d, cbcr_offset=%d",
         __func__, main_buf_info->src_image[0].offset.num_planes,
         main_buf_info->src_image[0].offset.frame_len,
         main_buf_info->src_image[0].offset.mp[0].offset,
         main_buf_info->src_image[0].offset.mp[1].offset);

    mm_stream_clear_invalid_cache(pme,main_frame);

    if (thumb_frame) {
        /* fill in thumbnail src img encode param */
        thumb_buf_info = &job.encode_job.encode_parm.buf_info.src_imgs.src_img[JPEG_SRC_IMAGE_TYPE_THUMB];
        thumb_buf_info->type = JPEG_SRC_IMAGE_TYPE_THUMB;
        thumb_buf_info->color_format = MM_JPEG_COLOR_FORMAT_YCRCBLP_H2V2; //TODO
        thumb_buf_info->quality = 85;
        thumb_buf_info->src_dim.width = pme->dim.thumbnail_width;
        thumb_buf_info->src_dim.height = pme->dim.thumbnail_height;
        thumb_buf_info->out_dim.width = pme->dim.thumbnail_width;
        thumb_buf_info->out_dim.height = pme->dim.thumbnail_height;
        thumb_buf_info->crop.width = pme->dim.thumbnail_width;
        thumb_buf_info->crop.height = pme->dim.thumbnail_height;
        thumb_buf_info->crop.offset_x = 0;
        thumb_buf_info->crop.offset_y = 0;
        thumb_buf_info->img_fmt = JPEG_SRC_IMAGE_FMT_YUV;
        thumb_buf_info->num_bufs = 1;
        thumb_buf_info->src_image[0].fd = thumb_frame->fd;
        thumb_buf_info->src_image[0].buf_vaddr = thumb_frame->buffer;
        pme->cam->ops->get_stream_parm(pme->cam->camera_handle,
                                       pme->ch_id,
                                       pme->stream[MM_CAMERA_SNAPSHOT_THUMBNAIL].id,
                                       MM_CAMERA_STREAM_OFFSET,
                                       &thumb_buf_info->src_image[0].offset);
    }

    /* fill in sink img param */
    job.encode_job.encode_parm.buf_info.sink_img.buf_len = pme->jpeg_buf.bufs[0].frame_len;
    job.encode_job.encode_parm.buf_info.sink_img.buf_vaddr = pme->jpeg_buf.bufs[0].buffer;
    job.encode_job.encode_parm.buf_info.sink_img.fd = pme->jpeg_buf.bufs[0].fd;

    mm_stream_clear_invalid_cache(pme,thumb_frame);

    rc = pme->jpeg_ops.start_job(pme->jpeg_hdl, &job, &pme->current_job_id);
    if ( 0 != rc ) {
        free(pme->current_job_frames);
        pme->current_job_frames = NULL;
    }

    return rc;
}

static void snapshot_yuv_cb(mm_camera_super_buf_t *bufs,
                            void *user_data)
{

    int rc;
    int i = 0;
    mm_camera_buf_def_t *main_frame = NULL;
    mm_camera_buf_def_t *thumb_frame = NULL;
    mm_camera_app_obj_t *pme = NULL;
    char* cmd = "This is a private cmd from test app";

    CDBG("%s: BEGIN\n", __func__);

    pme = (mm_camera_app_obj_t *)user_data;

    CDBG("%s: send private ioctl here", __func__);
    pme->cam->ops->send_command(bufs->camera_handle,
                                MM_CAMERA_CMD_TYPE_PRIVATE,
                                0,
                                strlen(cmd) + 1,
                                (void *)cmd);

    /* start jpeg encoding job */
    rc = encodeData(pme, bufs);

    /* buf done rcvd frames in error case */
    if ( 0 != rc ) {
        for (i=0; i<bufs->num_bufs; i++) {
            if (MM_CAMERA_OK != pme->cam->ops->qbuf(bufs->camera_handle,
                                                    bufs->ch_id,
                                                    bufs->bufs[i])) {
                CDBG_ERROR("%s: Failed in Qbuf\n", __func__);
            }
            mm_stream_invalid_cache(pme,bufs->bufs[i]);
        }
    }
#ifdef TEST_ABORT_JPEG_ENCODE
    else {
        if (aborted_flag) {
            aborted_flag = 0;
            /* abort the job */
            CDBG("%s: abort jpeg encode job  here", __func__);
            rc = pme->jpeg_ops.abort_job(pme->jpeg_hdl, pme->current_job_id);
            if (NULL != pme->current_job_frames) {
                free(pme->current_job_frames);
                pme->current_job_frames = NULL;
            }
            CDBG("%s: abort jpeg encode job returns %d", __func__, rc);
            for (i=0; i<bufs->num_bufs; i++) {
                if (MM_CAMERA_OK != pme->cam->ops->qbuf(bufs->camera_handle,
                                                        bufs->ch_id,
                                                        bufs->bufs[i])) {
                    CDBG_ERROR("%s: Failed in Qbuf\n", __func__);
                }
                mm_stream_invalid_cache(pme,bufs->bufs[i]);
            }

            /* signal snapshot is done */
            mm_app_snapshot_done();
        }
    }
#endif

    CDBG("%s: END\n", __func__);
}

static void snapshot_raw_cb(mm_camera_super_buf_t *recvd_frame,
                            void *user_data)
{
    int rc;
    int i = 0;
    mm_camera_buf_def_t *main_frame = NULL;
    mm_camera_app_obj_t *pme = NULL;

    CDBG("%s: BEGIN\n", __func__);

    pme = (mm_camera_app_obj_t *)user_data;

     if (pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id == recvd_frame->bufs[i]->stream_id) {
         main_frame = recvd_frame->bufs[i];
         CDBG("mainframe frame_idx = %d fd = %d frame length = %d",main_frame->frame_idx,main_frame->fd,main_frame->frame_len);
         dumpFrameToFile(main_frame,pme->dim.raw_picture_width,pme->dim.raw_picture_height,"raw_main", 1,"raw");
     }
     if (MM_CAMERA_OK != pme->cam->ops->qbuf(pme->cam->camera_handle,pme->ch_id,
                                                main_frame)) {
        CDBG_ERROR("%s: Failed in Qbuf\n", __func__);
     }

     mm_app_snapshot_done();
}

static int encodeDisplayAndSave(mm_camera_super_buf_t* recvd_frame,
                                int enqueued, mm_camera_app_obj_t *pme)
{
    int ret = -1;
#if 0


    CDBG("%s: Send frame for encoding", __func__);
    ret = encodeData(recvd_frame, pme->snapshot_buf.frame_len,
                     enqueued, pme);
    if (!ret) {
        CDBG_ERROR("%s: Failure configuring JPEG encoder", __func__);
    }

    LOGD("%s: X", __func__);
#endif
    return ret;
}

int mm_app_add_snapshot_stream(int cam_id)
{
    int rc = MM_CAMERA_OK;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id = pme->cam->ops->add_stream(pme->cam->camera_handle,pme->ch_id,
                                                                        NULL,pme,
                                                                        MM_CAMERA_SNAPSHOT_MAIN, 0);

    CDBG("Add Snapshot main is successfull stream ID = %d",pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id);
    if (!pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id) {
        CDBG_ERROR("%s:snapshot main streaming err=%d\n", __func__, rc);
        rc = -1;
        goto end;
    }

    pme->stream[MM_CAMERA_SNAPSHOT_THUMBNAIL].id = pme->cam->ops->add_stream(pme->cam->camera_handle,pme->ch_id,
                                                                             NULL,pme,
                                                                             MM_CAMERA_SNAPSHOT_THUMBNAIL, 0);
    if (!pme->stream[MM_CAMERA_SNAPSHOT_THUMBNAIL].id) {
        CDBG_ERROR("%s:snapshot thumbnail streaming err=%d\n", __func__, rc);
        rc = -1;
        goto end;
    }
    end:
    CDBG("%s: END, rc=%d\n", __func__, rc);
    return rc;
}

void mm_app_set_snapshot_mode(int cam_id,int op_mode)
{
    denoise_param_t wnr;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);
    pme->cam->ops->set_parm(pme->cam->camera_handle,MM_CAMERA_PARM_OP_MODE, &op_mode);

    /* set wnr enabled */
    memset(&wnr, 0, sizeof(denoise_param_t));
    wnr.denoise_enable = 0;
    wnr.process_plates = 0;
 //   pme->cam->ops->set_parm(pme->cam->camera_handle, MM_CAMERA_PARM_WAVELET_DENOISE, &wnr);
}

int mm_app_config_raw_format(int cam_id)
{
    int rc = MM_CAMERA_OK;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);
    mm_camera_image_fmt_t *m_fmt = NULL;
    mm_camera_image_fmt_t *t_fmt = NULL;

    mm_app_set_raw_snapshot_fmt(cam_id,&pme->stream[MM_CAMERA_SNAPSHOT_MAIN].str_config.fmt);

    pme->stream[MM_CAMERA_SNAPSHOT_MAIN].str_config.need_stream_on = 1;
    pme->stream[MM_CAMERA_SNAPSHOT_MAIN].str_config.num_of_bufs = 1;

    if (MM_CAMERA_OK != (rc = pme->cam->ops->config_stream(pme->cam->camera_handle,pme->ch_id,pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id,
                                                           &pme->stream[MM_CAMERA_SNAPSHOT_MAIN].str_config))) {
        CDBG_ERROR("%s:preview streaming err=%d\n", __func__, rc);
        goto end;
    }

end:
    CDBG("%s: END, rc=%d\n", __func__, rc);
    return rc;

}

int mm_app_config_snapshot_format(int cam_id)
{
    int rc = MM_CAMERA_OK;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    mm_app_set_snapshot_fmt(cam_id,&pme->stream[MM_CAMERA_SNAPSHOT_MAIN].str_config.fmt);

    mm_app_set_thumbnail_fmt(cam_id,&pme->stream[MM_CAMERA_SNAPSHOT_THUMBNAIL].str_config.fmt);

    pme->stream[MM_CAMERA_SNAPSHOT_MAIN].str_config.need_stream_on = 1;
    pme->stream[MM_CAMERA_SNAPSHOT_MAIN].str_config.num_of_bufs = 1;

    if (MM_CAMERA_OK != (rc = pme->cam->ops->config_stream(pme->cam->camera_handle,pme->ch_id,pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id,
                                                           &pme->stream[MM_CAMERA_SNAPSHOT_MAIN].str_config))) {
        CDBG_ERROR("%s:preview streaming err=%d\n", __func__, rc);
        goto end;
    }

    pme->stream[MM_CAMERA_SNAPSHOT_THUMBNAIL].str_config.need_stream_on = 1;
    pme->stream[MM_CAMERA_SNAPSHOT_THUMBNAIL].str_config.num_of_bufs = 1;

    if (MM_CAMERA_OK != (rc = pme->cam->ops->config_stream(pme->cam->camera_handle,pme->ch_id,pme->stream[MM_CAMERA_SNAPSHOT_THUMBNAIL].id,
                                                           &pme->stream[MM_CAMERA_SNAPSHOT_THUMBNAIL].str_config))) {
        CDBG_ERROR("%s:preview streaming err=%d\n", __func__, rc);
        goto end;
    }
    end:
    CDBG("%s: END, rc=%d\n", __func__, rc);
    return rc;

}

int mm_app_streamon_snapshot(int cam_id)
{
    int rc = MM_CAMERA_OK;
    uint32_t stream[2];
    mm_camera_bundle_attr_t attr;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    stream[0] = pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id;
    stream[1] = pme->stream[MM_CAMERA_SNAPSHOT_THUMBNAIL].id;

    attr.notify_mode = MM_CAMERA_SUPER_BUF_NOTIFY_BURST;
    attr.burst_num = 1;
    attr.look_back = 2;
    attr.post_frame_skip = 0;
    attr.water_mark = 2;

    if (MM_CAMERA_OK != (rc = pme->cam->ops->init_stream_bundle(
                                                               pme->cam->camera_handle,pme->ch_id,snapshot_yuv_cb,pme,&attr,2,stream))) {
        CDBG_ERROR("%s:init_stream_bundle err=%d\n", __func__, rc);
        goto end;
    }

    if (MM_CAMERA_OK != (rc = pme->cam->ops->start_streams(pme->cam->camera_handle,pme->ch_id,
                                                           2, stream))) {
        CDBG_ERROR("%s:start_streams err=%d\n", __func__, rc);
        goto end;
    }

    if (MM_CAMERA_OK != (rc =pme->cam->ops->request_super_buf(pme->cam->camera_handle,pme->ch_id, attr.burst_num))) {
        CDBG_ERROR("%s:request_super_buf err=%d\n", __func__, rc);
        goto end;
    }
    pme->cam_state = CAMERA_STATE_SNAPSHOT;
    end:
    CDBG("%s: END, rc=%d\n", __func__, rc);
    return rc;
}

int mm_app_streamon_raw(int cam_id)
{
    int rc = MM_CAMERA_OK;
    uint32_t stream[2];
    mm_camera_bundle_attr_t attr;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    stream[0] = pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id;

    attr.notify_mode = MM_CAMERA_SUPER_BUF_NOTIFY_BURST;
    attr.burst_num = 1;
    attr.look_back = 2;
    attr.post_frame_skip = 0;
    attr.water_mark = 2;

    if (MM_CAMERA_OK != (rc = pme->cam->ops->init_stream_bundle(
                                                               pme->cam->camera_handle,pme->ch_id,snapshot_raw_cb,pme,&attr,1,stream))) {
        CDBG_ERROR("%s:init_stream_bundle err=%d\n", __func__, rc);
        goto end;
    }

    if (MM_CAMERA_OK != (rc = pme->cam->ops->start_streams(pme->cam->camera_handle,pme->ch_id,
                                                           1, stream))) {
        CDBG_ERROR("%s:start_streams err=%d\n", __func__, rc);
        goto end;
    }

    if (MM_CAMERA_OK != (rc =pme->cam->ops->request_super_buf(pme->cam->camera_handle,pme->ch_id, attr.burst_num))) {
        CDBG_ERROR("%s:request_super_buf err=%d\n", __func__, rc);
        goto end;
    }
    pme->cam_state = CAMERA_STATE_SNAPSHOT;
end:
    CDBG("%s: END, rc=%d\n", __func__, rc);
    return rc;
}

int mm_app_streamoff_snapshot(int cam_id)
{
    int rc = MM_CAMERA_OK;
    uint32_t stream[2];

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    stream[0] = pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id;
    stream[1] = pme->stream[MM_CAMERA_SNAPSHOT_THUMBNAIL].id;

    if (MM_CAMERA_OK != (rc = pme->cam->ops->stop_streams(pme->cam->camera_handle,pme->ch_id,2,stream))) {
        CDBG_ERROR("%s : Snapshot Stream off Error",__func__);
        goto end;
    }
    CDBG("Stop snapshot main successfull");

    if (MM_CAMERA_OK != (rc = pme->cam->ops->destroy_stream_bundle(pme->cam->camera_handle,pme->ch_id))) {
        CDBG_ERROR("%s : Snapshot destroy_stream_bundle Error",__func__);
        goto end;
    }

    if (MM_CAMERA_OK != (rc = pme->cam->ops->del_stream(pme->cam->camera_handle,pme->ch_id,pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id))) {
        CDBG_ERROR("%s : Snapshot main image del_stream Error",__func__);
        goto end;
    }
    CDBG("del_stream successfull");

    if (MM_CAMERA_OK != (rc = pme->cam->ops->del_stream(pme->cam->camera_handle,pme->ch_id,pme->stream[MM_CAMERA_SNAPSHOT_THUMBNAIL].id))) {
        CDBG_ERROR("%s : Snapshot thumnail image del_stream Error",__func__);
        goto end;
    }
    CDBG("del_stream successfull");

    end:
    return rc;
}

int mm_app_start_snapshot(int cam_id)
{
    int rc = MM_CAMERA_OK;
    int stream[2];
    int op_mode = 0;
    uint8_t initial_reg_flag;
    mm_camera_frame_len_offset frame_offset_info;

    mm_camera_bundle_attr_t attr;


    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    if (MM_CAMERA_OK != mm_app_stop_preview(cam_id)) {
        CDBG_ERROR("%s: Stop preview Failed cam_id=%d\n",__func__,cam_id);
        return -1;
    }
    op_mode = MM_CAMERA_OP_MODE_CAPTURE;
    mm_app_set_snapshot_mode(cam_id,op_mode);
    usleep(20*1000);
//    pme->cam->ops->prepare_snapshot(pme->cam->camera_handle,pme->ch_id,0);

    if (MM_CAMERA_OK != (rc = mm_app_add_snapshot_stream(cam_id))) {
        CDBG_ERROR("%s : Add Snapshot stream err",__func__);
        return rc;
    }

    if (MM_CAMERA_OK != (rc = mm_app_config_snapshot_format(cam_id))) {
        CDBG_ERROR("%s : Config Snapshot stream err",__func__);
        return rc;
    }

    if (MM_CAMERA_OK != (rc = mm_app_streamon_snapshot(cam_id))) {
        CDBG_ERROR("%s : Stream on Snapshot stream err",__func__);
        return rc;
    }

    /* init jpeg buf */
    pme->cam->ops->get_stream_parm(pme->cam->camera_handle,
                                   pme->ch_id,
                                   pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id,
                                   MM_CAMERA_STREAM_OFFSET,
                                   &frame_offset_info);
    CDBG_ERROR("%s : alloc jpeg buf (len=%d)",__func__, frame_offset_info.frame_len);
    rc = mm_stream_alloc_bufs(pme,
                              &pme->jpeg_buf,
                              &frame_offset_info,
                              1);
    if (0 != rc) {
        CDBG_ERROR("%s : mm_stream_alloc_bufs err",__func__);
        return rc;
    }

    CDBG("%s: END, rc=%d\n", __func__, rc);
    return rc;
}

int mm_app_start_raw(int cam_id)
{
    int rc = MM_CAMERA_OK;
    int op_mode = 0;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    if (MM_CAMERA_OK != mm_app_stop_preview(cam_id)) {
        CDBG_ERROR("%s: Stop preview Failed cam_id=%d\n",__func__,cam_id);
        return -1;
    }
    op_mode = MM_CAMERA_OP_MODE_RAW;
    pme->cam->ops->set_parm(pme->cam->camera_handle,MM_CAMERA_PARM_OP_MODE, &op_mode);
    usleep(20*1000);

    pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id = pme->cam->ops->add_stream(pme->cam->camera_handle,pme->ch_id,
                                                                        NULL,pme,
                                                                        MM_CAMERA_SNAPSHOT_MAIN, 0);

    CDBG("Add RAW main is successfull stream ID = %d",pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id);
    if (!pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id) {
        CDBG_ERROR("%s:Raw main streaming err=%d\n", __func__, rc);
        rc = -1;
        return rc;
    }

    if (MM_CAMERA_OK != (rc = mm_app_config_raw_format(cam_id))) {
        CDBG_ERROR("%s : Config Raw Snapshot stream err",__func__);
        return rc;
    }

    if (MM_CAMERA_OK != (rc = mm_app_streamon_raw(cam_id))) {
        CDBG_ERROR("%s : Stream on Raw Snapshot stream err",__func__);
        return rc;
    }

    CDBG("%s: END, rc=%d\n", __func__, rc);
    return rc;
}

int mm_app_stop_snapshot(int cam_id)
{
    int rc = MM_CAMERA_OK;
    int i;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    if (pme->current_job_id) {
        /* have current jpeg encoding running, abort the job */
        if (pme->jpeg_ops.abort_job) {
            pme->jpeg_ops.abort_job(pme->jpeg_hdl, pme->current_job_id);
            pme->current_job_id = 0;
        }

        if (pme->current_job_frames) {
            for (i=0; i<pme->current_job_frames->num_bufs; i++) {
                if (MM_CAMERA_OK != pme->cam->ops->qbuf(pme->current_job_frames->camera_handle,
                                                        pme->current_job_frames->ch_id,
                                                        pme->current_job_frames->bufs[i])) {
                    CDBG_ERROR("%s: Failed in Qbuf\n", __func__);
                }
                mm_stream_invalid_cache(pme,pme->current_job_frames->bufs[i]);
            }
            free(pme->current_job_frames);
            pme->current_job_frames = NULL;
        }
    }

    if (MM_CAMERA_OK != (rc = mm_app_streamoff_snapshot(cam_id))) {
        CDBG_ERROR("%s : Stream off Snapshot stream err",__func__);
    }
    pme->cam_state = CAMERA_STATE_OPEN;

    /* deinit jpeg buf */
    rc = mm_stream_release_bufs(pme,
                                &pme->jpeg_buf);
end:
    CDBG("%s: END, rc=%d\n", __func__, rc);

    return rc;
}

int mm_app_stop_raw(int cam_id)
{
    int rc = MM_CAMERA_OK;
    int i;
    uint32_t stream[1];

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    stream[0] = pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id;

    if (MM_CAMERA_OK != (rc = pme->cam->ops->stop_streams(pme->cam->camera_handle,pme->ch_id,1,stream))) {
        CDBG_ERROR("%s : Snapshot Stream off Error",__func__);
        goto end;
    }
    CDBG("Stop snapshot main successfull");

    if (MM_CAMERA_OK != (rc = pme->cam->ops->destroy_stream_bundle(pme->cam->camera_handle,pme->ch_id))) {
        CDBG_ERROR("%s : Snapshot destroy_stream_bundle Error",__func__);
        goto end;
    }

    if (MM_CAMERA_OK != (rc = pme->cam->ops->del_stream(pme->cam->camera_handle,pme->ch_id,pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id))) {
        CDBG_ERROR("%s : Snapshot main image del_stream Error",__func__);
        goto end;
    }
    CDBG("del_stream successfull");

    pme->cam_state = CAMERA_STATE_OPEN;
end:
    return rc;
}

static void mm_app_snapshot_wait(int cam_id)
{
    pthread_mutex_lock(&g_s_mutex);
    if (FALSE == g_status) {
        pthread_cond_wait(&g_s_cond_v, &g_s_mutex);
        g_status = FALSE;
    }
    pthread_mutex_unlock(&g_s_mutex);
}

static void mm_app_live_notify_cb(mm_camera_super_buf_t *bufs,
                                  void *user_data)
{

    int rc;
    int i = 0;
    mm_camera_buf_def_t *main_frame = NULL;
    mm_camera_app_obj_t *pme = NULL;
    CDBG("%s: BEGIN\n", __func__);

    pme = (mm_camera_app_obj_t *)user_data;

    CDBG("%s : total streams = %d",__func__,bufs->num_bufs);
    main_frame = bufs->bufs[0];
    //thumb_frame = bufs->bufs[1];

    CDBG("mainframe frame_idx = %d fd = %d frame length = %d",main_frame->frame_idx,main_frame->fd,main_frame->frame_len);
    //CDBG("thumnail frame_idx = %d fd = %d frame length = %d",thumb_frame->frame_idx,thumb_frame->fd,thumb_frame->frame_len);

    //dumpFrameToFile(main_frame->frame,pme->dim.picture_width,pme->dim.picture_height,"main", 1);

    dumpFrameToFile(main_frame,pme->dim.picture_width,pme->dim.picture_height,"liveshot_main", 1,"yuv");

    if (MM_CAMERA_OK != pme->cam->ops->qbuf(pme->cam->camera_handle,pme->ch_id,main_frame)) {
        CDBG_ERROR("%s: Failed in thumbnail Qbuf\n", __func__);
    }
    mm_stream_invalid_cache(pme,main_frame);
    /*if(MM_CAMERA_OK != pme->cam->ops->qbuf(pme->cam->camera_handle,pme->ch_id,thumb_frame))
    {
            CDBG_ERROR("%s: Failed in thumbnail Qbuf\n", __func__);
    }*/

    mm_app_snapshot_done();
    CDBG("%s: END\n", __func__);


}

int mm_app_prepare_live_snapshot(int cam_id)
{
    int rc = 0;
    uint32_t stream[1];
    mm_camera_bundle_attr_t attr;
    int value = 0;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);
    CDBG("%s:BEGIN, cam_id=%d\n",__func__,cam_id);

    stream[0] = pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id;
    //stream[1] = pme->stream[MM_CAMERA_PREVIEW].id;  //Need to clarify

    attr.notify_mode = MM_CAMERA_SUPER_BUF_NOTIFY_BURST;
    attr.burst_num = 1;
    attr.look_back = 2;
    attr.post_frame_skip = 0;
    attr.water_mark = 2;


    if (MM_CAMERA_OK != (rc = pme->cam->ops->set_parm(
                                                     pme->cam->camera_handle,MM_CAMERA_PARM_FULL_LIVESHOT, (void *)&value))) {
        CDBG_ERROR("%s: set dimension err=%d\n", __func__, rc);
    }

    if (MM_CAMERA_OK != (rc = pme->cam->ops->init_stream_bundle(
                                                               pme->cam->camera_handle,pme->ch_id,mm_app_live_notify_cb,pme,&attr,1,stream))) {
        CDBG_ERROR("%s:init_stream_bundle err=%d\n", __func__, rc);
        rc = -1;
        goto end;
    }

    if (MM_CAMERA_OK != (rc = pme->cam->ops->start_streams(pme->cam->camera_handle,pme->ch_id,
                                                           1, stream))) {
        CDBG_ERROR("%s:start_streams err=%d\n", __func__, rc);
        rc = -1;
        goto end;
    }


    end:
    CDBG("%s:END, cam_id=%d\n",__func__,cam_id);
    return rc;
}

int mm_app_unprepare_live_snapshot(int cam_id)
{
    int rc = 0;
    uint32_t stream[2];

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);
    CDBG("%s:BEGIN, cam_id=%d\n",__func__,cam_id);

    stream[0] = pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id;
    if (MM_CAMERA_OK != (rc = pme->cam->ops->stop_streams(pme->cam->camera_handle,pme->ch_id,1,stream))) {
        CDBG_ERROR("%s : Snapshot Stream off Error",__func__);
        return -1;
    }

    if (MM_CAMERA_OK != (rc = pme->cam->ops->destroy_stream_bundle(pme->cam->camera_handle,pme->ch_id))) {
        CDBG_ERROR("%s : Snapshot destroy_stream_bundle Error",__func__);
        return -1;
    }
    CDBG("%s:END, cam_id=%d\n",__func__,cam_id);
    return rc;
}

int mm_app_take_live_snapshot(int cam_id)
{
    int rc = 0;
    int stream[3];
    mm_camera_bundle_attr_t attr;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    CDBG("%s:BEGIN, cam_id=%d\n",__func__,cam_id);

    if (pme->cam_mode == RECORDER_MODE &&
        pme->cam_state == CAMERA_STATE_RECORD) {
        //Code to get live shot
        if (mm_app_prepare_live_snapshot(cam_id) != MM_CAMERA_OK) {
            CDBG_ERROR("%s: Failed prepare liveshot",__func__);
            return -1;
        }
        if (MM_CAMERA_OK != (rc =pme->cam->ops->request_super_buf(pme->cam->camera_handle,pme->ch_id, 1))) {
            CDBG_ERROR("%s:request_super_buf err=%d\n", __func__, rc);
            return -1;
        }
        CDBG("%s:waiting images\n",__func__);
        mm_app_snapshot_wait(cam_id);

        if (MM_CAMERA_OK !=mm_app_unprepare_live_snapshot(cam_id)) {
            CDBG_ERROR("%s: Snapshot Stop error",__func__);
        }

    } else {
        CDBG_ERROR("%s: Should not come here for liveshot",__func__);
    }
    return rc;
}

int mm_app_take_picture_raw(int cam_id)
{

    int rc;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    CDBG("%s:BEGIN, cam_id=%d\n",__func__,cam_id);

    if (MM_CAMERA_OK != mm_app_start_raw(cam_id)) {
        CDBG_ERROR("%s: cam_id=%d\n",__func__,cam_id);
        rc = -1;
        goto end;
    }

    CDBG("%s:waiting images\n",__func__);
    mm_app_snapshot_wait(cam_id);

    if (MM_CAMERA_OK !=mm_app_stop_raw(cam_id)) {
        CDBG_ERROR("%s: Snapshot Stop error",__func__);
    }

preview:
    if (MM_CAMERA_OK != (rc = mm_app_start_preview(cam_id))) {
        CDBG("%s:preview start stream err=%d\n", __func__, rc);
    }
    end:
    CDBG("%s:END, cam_id=%d\n",__func__,cam_id);
    return rc;
}

int mm_app_take_picture_zsl(int cam_id)
{
    int rc = MM_CAMERA_OK;
    int value = 1;
    int op_mode;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    CDBG("%s: Take picture ZSL",__func__);

    if (MM_CAMERA_OK != (rc =pme->cam->ops->request_super_buf(pme->cam->camera_handle,pme->ch_id, 1))) {
        CDBG_ERROR("%s:request_super_buf err=%d\n", __func__, rc);
        goto end;
    }

    CDBG("%s: Start ZSL Preview",__func__);

    end:
    CDBG("%s: END, rc=%d\n", __func__, rc);
    return rc;
}

int mm_app_take_picture_yuv(int cam_id)
{
    int rc;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    CDBG("%s:BEGIN, cam_id=%d\n",__func__,cam_id);

    if (MM_CAMERA_OK != mm_app_start_snapshot(cam_id)) {
        CDBG_ERROR("%s: cam_id=%d\n",__func__,cam_id);
        rc = -1;
        goto end;
    }

    CDBG("%s:waiting images\n",__func__);
    mm_app_snapshot_wait(cam_id);

    if (MM_CAMERA_OK !=mm_app_stop_snapshot(cam_id)) {
        CDBG_ERROR("%s: Snapshot Stop error",__func__);
    }

    preview:
    if (MM_CAMERA_OK != (rc = mm_app_start_preview(cam_id))) {
        CDBG("%s:preview start stream err=%d\n", __func__, rc);
    }
    end:
    CDBG("%s:END, cam_id=%d\n",__func__,cam_id);
    return rc;
}

int mm_app_take_zsl(int cam_id)
{
    int rc = MM_CAMERA_OK;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    CDBG("%s:BEGIN, cam_id=%d\n",__func__,cam_id);

    if (pme->cam_mode == RECORDER_MODE) {
        switch (pme->cam_state) {
        case CAMERA_STATE_RECORD:
            if (MM_CAMERA_OK != mm_app_stop_video(cam_id)) {
                CDBG_ERROR("%s:Cannot stop video err=%d\n", __func__, rc);
                return -1;
            }
        case CAMERA_STATE_PREVIEW:
            if (MM_CAMERA_OK != mm_app_open_zsl(cam_id)) {
                CDBG_ERROR("%s: Cannot switch to camera mode\n", __func__);
                return -1;
            }
            break;
        case CAMERA_STATE_SNAPSHOT:
        default:
            CDBG("%s: Cannot normal pciture in record mode\n", __func__);
            break;
        }
    } else if (pme->cam_mode == CAMERA_MODE) {
        switch (pme->cam_state) {
        case CAMERA_STATE_PREVIEW:
            mm_app_open_zsl(cam_id);
            break;

        case CAMERA_STATE_SNAPSHOT:
        case CAMERA_STATE_RECORD:
        default:
            CDBG("%s: Cannot normal pciture in record mode\n", __func__);
            break;
        }
    }

    if (pme->cam_mode == ZSL_MODE && pme->cam_state == CAMERA_STATE_PREVIEW) {
        mm_app_take_picture_zsl(cam_id);
    }
    return rc;
}

int mm_app_take_picture(int cam_id)
{
    int rc = 0;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    CDBG("%s:BEGIN, cam_id=%d\n",__func__,cam_id);

    if (pme->cam_mode == RECORDER_MODE) {
        switch (pme->cam_state) {
        case CAMERA_STATE_RECORD:
            if (MM_CAMERA_OK != mm_app_stop_video(cam_id)) {
                CDBG_ERROR("%s:Cannot stop video err=%d\n", __func__, rc);
                return -1;
            }
        case CAMERA_STATE_PREVIEW:
            if (MM_CAMERA_OK != mm_app_open_camera(cam_id)) {
                CDBG_ERROR("%s: Cannot switch to camera mode=%d\n", __func__,rc);
                return -1;
            }
            break;
        case CAMERA_STATE_SNAPSHOT:
        default:
            CDBG("%s: Cannot normal pciture in record mode\n", __func__);
            break;
        }
    } else if (pme->cam_mode == ZSL_MODE) {
        switch (pme->cam_state) {
        case CAMERA_STATE_PREVIEW:
            mm_app_open_camera(cam_id);
            break;

        case CAMERA_STATE_SNAPSHOT:
        case CAMERA_STATE_RECORD:
        default:
            CDBG("%s: Cannot normal pciture in record mode\n", __func__);
            break;
        }
    }

    CDBG("%s : Takepicture : mode = %d state = %d, rc = %d",__func__,pme->cam_mode,pme->cam_state,rc);
    if (pme->cam_mode == CAMERA_MODE && pme->cam_state == CAMERA_STATE_PREVIEW) {
        mm_app_take_picture_yuv(cam_id);
    }
    return rc;
}


int mm_app_take_raw(int cam_id)
{
    int rc = 0;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    CDBG("%s:BEGIN, cam_id=%d\n",__func__,cam_id);

    if (pme->cam_mode == RECORDER_MODE) {
        switch (pme->cam_state) {
        case CAMERA_STATE_RECORD:
            if (MM_CAMERA_OK != mm_app_stop_video(cam_id)) {
                CDBG_ERROR("%s:Cannot stop video err=%d\n", __func__, rc);
                return -1;
            }
        case CAMERA_STATE_PREVIEW:
            if (MM_CAMERA_OK != mm_app_open_camera(cam_id)) {
                CDBG_ERROR("%s: Cannot switch to camera mode=%d\n", __func__,rc);
                return -1;
            }
            break;
        case CAMERA_STATE_SNAPSHOT:
        default:
            CDBG("%s: Cannot normal pciture in record mode\n", __func__);
            break;
        }
    } else if (pme->cam_mode == ZSL_MODE) {
        switch (pme->cam_state) {
        case CAMERA_STATE_PREVIEW:
            mm_app_open_camera(cam_id);
            break;

        case CAMERA_STATE_SNAPSHOT:
        case CAMERA_STATE_RECORD:
        default:
            CDBG("%s: Cannot normal pciture in record mode\n", __func__);
            break;
        }
    }

    CDBG("%s : Takepicture RAW: mode = %d state = %d, rc = %d",__func__,pme->cam_mode,pme->cam_state,rc);
    if (pme->cam_mode == CAMERA_MODE && pme->cam_state == CAMERA_STATE_PREVIEW) {
        mm_app_take_picture_raw(cam_id);
    }
    return rc;
}

