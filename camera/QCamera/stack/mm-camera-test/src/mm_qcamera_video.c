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
#include "mm_camera_dbg.h"
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include "mm_qcamera_app.h"

#define BUFF_SIZE_128 128
static int num_run = 0;

extern int mm_app_set_live_snapshot_fmt(int cam_id,mm_camera_image_fmt_t *fmt);
extern void dumpFrameToFile(mm_camera_buf_def_t* newFrame, int w, int h,
                            char* name, int main_422,char *ext);
extern int initDisplay();
extern int mm_app_prepare_preview(int cam_id);
extern int mm_stream_invalid_cache(mm_camera_app_obj_t *pme,
                                   mm_camera_buf_def_t *frame);

static int mm_app_dump_video_frame(struct msm_frame *frame,
                                   uint32_t len)
{
    static int v_cnt = 0;
    char bufp[BUFF_SIZE_128];
    int file_fdp;
    int rc = 0;

    return rc; /* disable dump */

    v_cnt++;
    if(0 == (v_cnt % 10))
        snprintf(bufp, BUFF_SIZE_128, "/data/v_%d.yuv", v_cnt);
    else
        return 0;

    file_fdp = open(bufp, O_RDWR | O_CREAT, 0777);

    if (file_fdp < 0) {
        CDBG("cannot open file %s\n", bufp);
        rc = -1;
        goto end;
    }
    CDBG("%s:dump frame to '%s'\n", __func__, bufp);
    write(file_fdp,
        (const void *)frame->buffer, len);
    close(file_fdp);
end:
    return rc;
}

static int mm_app_set_video_fmt(int cam_id,mm_camera_image_fmt_t *fmt)
{
    int rc = MM_CAMERA_OK;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    fmt->meta_header = MM_CAMEAR_META_DATA_TYPE_DEF;
    fmt->fmt = pme->dim.enc_format;
    fmt->width = pme->dim.video_width;
    fmt->height = pme->dim.video_height;
    return rc;
}

#if 0
int mm_stream_deinit_video_buf(uint32_t camera_handle,
                      uint32_t ch_id, uint32_t stream_id,
                      void *user_data, uint8_t num_bufs,
                      mm_camera_buf_def_t *bufs)
{
    int i, rc = MM_CAMERA_OK;
    mm_camera_app_obj_t *pme = (mm_camera_app_obj_t *)user_data;

    for(i = 0; i < num_bufs; i++) {
        rc = my_cam_app.hal_lib.mm_camera_do_munmap_ion (pme->ionfd, &(pme->video_buf.frame[i].fd_data),
                   (void *)pme->video_buf.frame[i].buffer, pme->video_buf.frame_len);
        if(rc != MM_CAMERA_OK) {
          CDBG_ERROR("%s: mm_camera_do_munmap err, pmem_fd = %d, rc = %d",
               __func__, bufs[i].fd, rc);
        }
    }
    return rc;
}

int mm_stream_init_video_buf(uint32_t camera_handle,
                      uint32_t ch_id, uint32_t stream_id,
                      void *user_data,
                      mm_camera_frame_len_offset *frame_offset_info,
                      uint8_t num_bufs,
                      uint8_t *initial_reg_flag,
                      mm_camera_buf_def_t *bufs)
{
    int i,j,num_planes, frame_len, y_off, cbcr_off;
    uint32_t planes[VIDEO_MAX_PLANES];
    uint32_t pmem_addr = 0;

    mm_camera_app_obj_t *pme = (mm_camera_app_obj_t *)user_data;

    num_planes = frame_offset_info->num_planes;
    for ( i = 0; i < num_planes; i++) {
        planes[i] = frame_offset_info->mp[i].len;
    }

    frame_len = frame_offset_info->frame_len;
    y_off = frame_offset_info->mp[0].offset;
    cbcr_off = frame_offset_info->mp[1].offset;

    CDBG("Allocating video Memory for %d buffers frame_len = %d",num_bufs,frame_offset_info->frame_len);

    for (i = 0; i < num_bufs ; i++) {
        int j;
        pme->video_buf.reg[i] = 1;
        initial_reg_flag[i] = 1;

        pme->video_buf.frame_len = frame_len;
        pme->video_buf.frame[i].ion_alloc.len = pme->video_buf.frame_len;
        pme->video_buf.frame[i].ion_alloc.flags =
            (0x1 << CAMERA_ION_HEAP_ID | 0x1 << ION_IOMMU_HEAP_ID);
        pme->video_buf.frame[i].ion_alloc.align = 4096;

        pmem_addr = (unsigned long) my_cam_app.hal_lib.mm_camera_do_mmap_ion(pme->ionfd,
                       &(pme->video_buf.frame[i].ion_alloc), &(pme->video_buf.frame[i].fd_data),
                       &pme->video_buf.frame[i].fd);

        pme->video_buf.frame[i].buffer = pmem_addr;
        pme->video_buf.frame[i].path = OUTPUT_TYPE_V;
        pme->video_buf.frame[i].y_off = 0;
        pme->video_buf.frame[i].cbcr_off = planes[0];
        pme->video_buf.frame[i].phy_offset = 0;

        CDBG("Buffer allocated Successfully fd = %d",pme->video_buf.frame[i].fd);

        bufs[i].fd = pme->video_buf.frame[i].fd;
        //bufs[i].buffer = pmem_addr;
        bufs[i].frame_len = pme->video_buf.frame[i].ion_alloc.len;
        bufs[i].num_planes = num_planes;

        bufs[i].frame = &pme->video_buf.frame[i];

        /* Plane 0 needs to be set seperately. Set other planes
             * in a loop. */
        bufs[i].planes[0].length = planes[0];
        bufs[i].planes[0].m.userptr = bufs[i].fd;
        bufs[i].planes[0].data_offset = y_off;
        bufs[i].planes[0].reserved[0] = 0;
              //buf_def->buf.mp[i].frame_offset;
        for (j = 1; j < num_planes; j++) {
             bufs[i].planes[j].length = planes[j];
             bufs[i].planes[j].m.userptr = bufs[i].fd;
             bufs[i].planes[j].data_offset = cbcr_off;
             bufs[i].planes[j].reserved[0] =
                 bufs[i].planes[j-1].reserved[0] +
                 bufs[i].planes[j-1].length;
        }
    }
    return MM_CAMERA_OK;
}
#endif

void video_cb_signal(mm_camera_app_obj_t *pme)
{
    if(pme->cam_state == CAMERA_STATE_RECORD) {
        mm_camera_app_done();
    }
}

static void mm_app_video_notify_cb(mm_camera_super_buf_t *bufs,
    void *user_data)
{
    int rc;
    char buf[32];
    mm_camera_buf_def_t *frame = NULL;
    mm_camera_app_obj_t *pme = NULL;
    CDBG("%s: BEGIN\n", __func__);
    frame = bufs->bufs[MM_CAMERA_PREVIEW] ;
    pme = (mm_camera_app_obj_t *)user_data;

    CDBG("%s: BEGIN - length=%d, frame idx = %d\n", __func__, frame->frame_len, frame->frame_idx);
    //Need to code to Send to Encoder .. Simulat
    CDBG("In CB function i/p = %p o/p = %p",bufs->bufs[MM_CAMERA_PREVIEW],frame);

    snprintf(buf, sizeof(buf), "V_%dx%d_C%d", pme->dim.orig_video_width,
        pme->dim.orig_video_height, pme->cam->camera_info->camera_id);

    dumpFrameToFile(frame, pme->dim.orig_video_width,
        pme->dim.orig_video_height, buf, 1,"yuv");

    if(MM_CAMERA_OK != pme->cam->ops->qbuf(pme->cam->camera_handle,pme->ch_id,frame))
    {
        CDBG_ERROR("%s: Failed in Snapshot Qbuf\n", __func__);
        return;
    }
        mm_stream_invalid_cache(pme,frame);
    video_cb_signal(pme);
    CDBG("%s: END\n", __func__);

}

int mm_app_config_video(int cam_id)
{
    int rc = MM_CAMERA_OK;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    mm_app_set_video_fmt(cam_id,&pme->stream[MM_CAMERA_VIDEO].str_config.fmt);
    pme->stream[MM_CAMERA_VIDEO].str_config.need_stream_on = 1;
    pme->stream[MM_CAMERA_VIDEO].str_config.num_of_bufs = VIDEO_BUF_NUM;

    if(MM_CAMERA_OK != (rc = pme->cam->ops->config_stream(pme->cam->camera_handle,pme->ch_id,pme->stream[MM_CAMERA_VIDEO].id,
                                 &pme->stream[MM_CAMERA_VIDEO].str_config))) {
        CDBG_ERROR("%s:MM_CAMERA_VIDEO config streaming err=%d\n", __func__, rc);
        goto end;
    }

    CDBG("config_stream stream is successfull");

    pme->stream[MM_CAMERA_SNAPSHOT_MAIN].str_config.need_stream_on = pme->fullSizeSnapshot;
    pme->stream[MM_CAMERA_SNAPSHOT_MAIN].str_config.num_of_bufs = 1;

    mm_app_set_live_snapshot_fmt(cam_id,&pme->stream[MM_CAMERA_SNAPSHOT_MAIN].str_config.fmt);

    if(MM_CAMERA_OK != (rc = pme->cam->ops->config_stream(pme->cam->camera_handle,pme->ch_id,pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id,
                                 &pme->stream[MM_CAMERA_SNAPSHOT_MAIN].str_config))) {
        CDBG_ERROR("%s:preview streaming err=%d\n", __func__, rc);
        goto end;
    }
end:
    return rc;

}

int mm_app_prepare_video(int cam_id)
{
    int rc = MM_CAMERA_OK;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    pme->stream[MM_CAMERA_VIDEO].id = pme->cam->ops->add_stream(pme->cam->camera_handle,pme->ch_id,
                                                       mm_app_video_notify_cb,pme,
                                                       MM_CAMERA_VIDEO, 0);

    if(!pme->stream[MM_CAMERA_VIDEO].id) {
        CDBG("%s:MM_CAMERA_VIDEO streaming err = %d\n", __func__, rc);
        rc = -1;
        goto end;
    }

    CDBG("Add stream is successfull stream ID = %d",pme->stream[MM_CAMERA_PREVIEW].id);

    /* Code to add live snapshot*/
    pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id = pme->cam->ops->add_stream(pme->cam->camera_handle,pme->ch_id,
                                                   NULL,pme,
                                                   MM_CAMERA_SNAPSHOT_MAIN, 0);

    CDBG("Add Snapshot main is successfull stream ID = %d",pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id);
    if(!pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id) {
        CDBG_ERROR("%s:preview streaming err=%d\n", __func__, rc);
        rc = -1;
        goto end;
    }
    /*if(mm_app_config_video(cam_id) != MM_CAMERA_OK)
    {
        CDBG_ERROR("%s:Video config err=%d\n", __func__, rc);
    }*/
end:
    return rc;
}

int mm_app_unprepare_video(int cam_id)
{
    int rc = MM_CAMERA_OK;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    if(MM_CAMERA_OK != (rc = pme->cam->ops->del_stream(pme->cam->camera_handle,pme->ch_id,pme->stream[MM_CAMERA_VIDEO].id))){
        CDBG_ERROR("%s : Delete Stream Video error",__func__);
        goto end;
    }

    if(MM_CAMERA_OK != (rc = pme->cam->ops->del_stream(pme->cam->camera_handle,pme->ch_id,pme->stream[MM_CAMERA_SNAPSHOT_MAIN].id))){
        CDBG_ERROR("%s : Delete Stream Video error",__func__);
        goto end;
    }
end:
    CDBG("del_stream successfull");
    return rc;
}

static int mm_app_streamon_video(int cam_id)
{
    uint32_t stream;
    int rc = MM_CAMERA_OK;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    if(mm_app_config_video(cam_id) != MM_CAMERA_OK)
    {
        CDBG_ERROR("%s:Video config err=%d\n", __func__, rc);
    }

    stream = pme->stream[MM_CAMERA_VIDEO].id;
    if(MM_CAMERA_OK != (rc = pme->cam->ops->start_streams(pme->cam->camera_handle,pme->ch_id,1,&stream)))
    {
        CDBG_ERROR("%s : Start Stream video Error",__func__);
        return -1;
    }
    CDBG("Start video stream is successfull");
    pme->cam_state = CAMERA_STATE_RECORD;
    return rc;
}

static int mm_app_streamoff_video(int cam_id)
{
    uint32_t stream;
    int rc = MM_CAMERA_OK;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    stream = pme->stream[MM_CAMERA_VIDEO].id;

    if(MM_CAMERA_OK != (rc =pme->cam->ops->stop_streams(pme->cam->camera_handle,pme->ch_id,1,&stream)))
    {
        CDBG_ERROR("%s : stop Stream video Error",__func__);
        goto end;
    }
    CDBG("stop video stream is successfull");
    pme->cam_state = CAMERA_STATE_PREVIEW;
end:
    return rc;

}
int mm_app_stop_video(int cam_id)
{
    int stream[2];
    int rc = MM_CAMERA_OK;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    stream[0] = pme->stream[MM_CAMERA_VIDEO].id;

    if(MM_CAMERA_OK != (rc = mm_app_streamoff_video(cam_id))){
        CDBG_ERROR("%s : Video Stream off error",__func__);
        goto end;
    }
end:
    return rc;
}

static int mm_app_start_video(int cam_id)
{
    int rc = MM_CAMERA_OK;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    CDBG("pme = %p, pme->cam =%p, pme->ch = %d pme->cam->camera_handle = %d",
         pme,pme->cam,pme->ch_id,pme->cam->camera_handle);

    if(MM_CAMERA_OK != (rc =  mm_app_prepare_video(cam_id))){
        CDBG_ERROR("%s:MM_CAMERA_VIDEO streaming err = %d\n", __func__, rc);
        goto end;
    }
    if(MM_CAMERA_OK != (rc =  mm_app_streamon_video(cam_id))){
        CDBG_ERROR("%s:MM_CAMERA_VIDEO streaming err = %d\n", __func__, rc);
        goto end;
    }

end:
    CDBG("%s: END, rc=%d\n", __func__, rc);

    return rc;
}

int mm_app_open_recorder(int cam_id)
{
    int rc = MM_CAMERA_OK;
    int value = 1;
    int powermode = 1;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    CDBG("%s: mm_app_open_recorder",__func__);
    if(pme->cam_mode == RECORDER_MODE) {
        CDBG("%s : Already in record mode",__func__);
        return rc;
    }

    if(MM_CAMERA_OK != (rc = mm_app_stop_preview(cam_id))){
        CDBG_ERROR("%s:Stop preview err=%d\n", __func__, rc);
        goto end;
    }
    usleep(10*1000);

    if(MM_CAMERA_OK != initDisplay())
    {
        CDBG_ERROR("%s : Could not initalize display",__func__);
        goto end;
    }

    pme->cam->ops->set_parm(pme->cam->camera_handle,MM_CAMERA_PARM_RECORDING_HINT, &value);

    pme->cam->ops->set_parm(pme->cam->camera_handle,MM_CAMERA_PARM_LOW_POWER_MODE, &powermode);


    if(MM_CAMERA_OK != (rc = mm_app_prepare_preview(cam_id))){
        CDBG_ERROR("%s:stream on preview err=%d\n", __func__, rc);
        goto end;
    }

    if(MM_CAMERA_OK != (rc = mm_app_prepare_video(cam_id))){
        CDBG_ERROR("%s:stream on video err=%d\n", __func__, rc);
        goto end;
    }

    if(MM_CAMERA_OK != (rc = mm_app_streamon_preview(cam_id))){
        CDBG_ERROR("%s:start preview err=%d\n", __func__, rc);
        goto end;
    }
    pme->cam_mode = RECORDER_MODE;
end:
    CDBG("%s: END, rc=%d\n", __func__, rc);
    return rc;
}

int startRecording(int cam_id)
{
    int rc = MM_CAMERA_OK;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    CDBG("%s: Start Recording mode = %d state = %d",__func__,pme->cam_mode,pme->cam_state);

    if(pme->cam_mode == CAMERA_MODE || pme->cam_mode == ZSL_MODE) {
        switch(pme->cam_state) {
            case CAMERA_STATE_PREVIEW:
                if(MM_CAMERA_OK != mm_app_open_recorder(cam_id)){
                    CDBG_ERROR("%s: Open Record Failed \n", __func__);
                    return -1;
                }
                break;
            case CAMERA_STATE_RECORD:
            case CAMERA_STATE_SNAPSHOT:
            default:
                break;
        }
    }/*else{
        mm_app_prepare_video(cam_id);
    }*/
    CDBG("%s : startRecording : mode = %d state = %d",__func__,pme->cam_mode,pme->cam_state);
    if(pme->cam_mode == RECORDER_MODE && pme->cam_state == CAMERA_STATE_PREVIEW){
        if(MM_CAMERA_OK != mm_app_streamon_video(cam_id)){
            CDBG_ERROR("%s:start video err=%d\n", __func__, rc);
            return -1;
        }
    }
    CDBG("%s: END, rc=%d\n", __func__, rc);
    return rc;
}

int stopRecording(int cam_id)
{

    int rc = MM_CAMERA_OK;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    if(pme->cam_mode != RECORDER_MODE || pme->cam_state != CAMERA_STATE_RECORD) {
        return rc;
    }
    if(MM_CAMERA_OK != mm_app_stop_video(cam_id)){
        CDBG_ERROR("%s:stop video err=%d\n", __func__, rc);
        return -1;
    }
    return rc;
}

