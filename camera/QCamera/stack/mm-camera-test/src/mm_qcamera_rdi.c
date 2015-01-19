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
#include <linux/msm_ion.h>
#include "mm_qcamera_app.h"

/*typedef enum {
  STREAM_IMAGE,
  STREAM_RAW,
  STREAM_IMAGE_AND_RAW,
  STREAM_RAW_AND_RAW,
  STREAM_MAX,
} mm_camera_channel_stream_info_t;*/

#define RDI_MASK STREAM_RAW
mm_camera_channel_stream_info_t rdi_mode;
static int rdi_op_mode;
extern int stopPreview(int cam_id);
extern void dumpFrameToFile(mm_camera_buf_def_t* newFrame, int w, int h,
                            char* name, int main_422,char *ext);
extern int mm_stream_invalid_cache(mm_camera_app_obj_t *pme,
                                   mm_camera_buf_def_t *frame);

static int rdi_counter = 0;
static void dumpRdi(mm_camera_buf_def_t* newFrame, int w, int h, char* name, int main_422)
{
    char buf[32];
    int file_fd;
    int i;
    char *ext = "yuv";
    if ( newFrame != NULL) {
        char * str;
        snprintf(buf, sizeof(buf), "/data/%s_%d.%s", name, rdi_counter, ext);
        file_fd = open(buf, O_RDWR | O_CREAT, 0777);
        if (file_fd < 0) {
            CDBG_ERROR("%s: cannot open file\n", __func__);
        } else {
            void *y_off = newFrame->buffer + newFrame->planes[0].data_offset;
            //int cbcr_off = newFrame->buffer + newFrame->planes[1].data_offset;//newFrame->buffer + newFrame->planes[0].length;
            void *cbcr_off = newFrame->buffer + newFrame->planes[0].length;
            CDBG("%s: Y_off = %p cbcr_off = %p", __func__, y_off,cbcr_off);
            CDBG("%s: Y_off length = %d cbcr_off length = %d", __func__, newFrame->planes[0].length,newFrame->planes[1].length);

            write(file_fd, (const void *)(y_off), newFrame->planes[0].length);
            write(file_fd, (const void *)(cbcr_off),
                  (newFrame->planes[1].length * newFrame->num_planes));
            close(file_fd);
            CDBG("dump %s", buf);
        }
    }
}

static int mm_app_set_opmode(int cam_id, mm_camera_op_mode_type_t op_mode)
{
    int rc = MM_CAMERA_OK;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);
    CDBG_ERROR("%s: Set rdi op mode = %d",__func__, op_mode);
    if (MM_CAMERA_OK != (rc = pme->cam->ops->set_parm(
        pme->cam->camera_handle,MM_CAMERA_PARM_OP_MODE, &op_mode))) {
        CDBG_ERROR("%s: Set rdi op mode error mode = %d",__func__, op_mode);
    }
    return rc;
}

static int mm_app_set_rdi_fmt(int cam_id, mm_camera_image_fmt_t *fmt)
{
    int rc = MM_CAMERA_OK;
    cam_ctrl_dimension_t dim;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    fmt->meta_header = MM_CAMEAR_META_DATA_TYPE_DEF;
    //pme->cam->ops->get_parm(pme->cam->camera_handle,MM_CAMERA_PARM_DIMENSION, &dim);
    fmt->fmt = pme->dim.rdi0_format;
    fmt->width = 0;
    fmt->height = 0;
    fmt->rotation = 0;

    CDBG("%s: RDI Dimensions = %d X %d", __func__, fmt->width, fmt->height);
    return rc;
}

int mm_app_open_rdi(int cam_id)
{
    int rc = MM_CAMERA_OK;
    int value = RDI_MASK;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    if (pme->cam_mode == RDI_MODE) {
        return rc;
    }

    if (MM_CAMERA_OK != (rc = stopPreview(cam_id))) {
        CDBG_ERROR("%s:Stop preview err=%d\n", __func__, rc);
        goto end;
    }
    pme->cam_mode = RDI_MODE;
    end:
    CDBG("%s: END, rc=%d\n", __func__, rc);
    return rc;
}

void rdi_cb_signal(mm_camera_app_obj_t *pme)
{
    if (pme->cam_mode == RDI_MODE) {
        mm_camera_app_done();
    }
}

static void mm_app_rdi_notify_cb(mm_camera_super_buf_t *bufs,
                                 void *user_data)
{
    int rc;
    mm_camera_buf_def_t *frame = NULL;
    mm_camera_app_obj_t *pme = NULL;
    CDBG("%s: BEGIN\n", __func__); 
    frame = bufs->bufs[0] ;
    pme = (mm_camera_app_obj_t *)user_data;

    CDBG("%s: BEGIN - length=%d, frame idx = %d\n", __func__, frame->frame_len, frame->frame_idx);

    if (rdi_op_mode == MM_CAMERA_OP_MODE_VIDEO)
      dumpFrameToFile(frame,pme->dim.rdi0_width,pme->dim.rdi0_height,"rdi_p", 1,"raw");
    else {
      rdi_counter++;
      if (rdi_counter <=5)
        dumpRdi(frame,pme->dim.rdi0_width,pme->dim.rdi0_height,"rdi_s", 1);
    }
    if (MM_CAMERA_OK != pme->cam->ops->qbuf(pme->cam->camera_handle,
                                            pme->ch_id,
                                            frame)) {
        CDBG_ERROR("%s: Failed in Preview Qbuf\n", __func__);
        return;
    }
    mm_stream_invalid_cache(pme,frame);
    if (my_cam_app.run_sanity) {
        mm_camera_app_done(pme);
    }
    CDBG("%s: END\n", __func__); 

}

int mm_app_prepare_rdi(int cam_id, uint8_t num_buf)
{
    int rc = MM_CAMERA_OK;
    int op_mode;
    int value = RDI_MASK;

    CDBG("%s: E",__func__);
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    pme->mem_cam->get_buf = mm_stream_initbuf;
    pme->mem_cam->put_buf = mm_stream_deinitbuf;
    pme->mem_cam->user_data = pme;

    //pme->cam->ops->set_parm(pme->cam->camera_handle,MM_CAMERA_PARM_CH_INTERFACE, &value);
    //pme->cam->ops->get_parm(pme->cam->camera_handle,MM_CAMERA_PARM_CH_INTERFACE, &rdi_mode);

    pme->stream[MM_CAMERA_RDI].id = pme->cam->ops->add_stream(pme->cam->camera_handle,
                                                              pme->ch_id,
                                                              mm_app_rdi_notify_cb,
                                                              pme,
                                                              MM_CAMERA_RDI,
                                                              0);

    if (!pme->stream[MM_CAMERA_RDI].id) {
        CDBG_ERROR("%s:Add RDI error =%d\n", __func__, rc);
        rc = -1;
        goto end;
    }
    CDBG("%s :Add RDI stream is successfull stream ID = %d",__func__,pme->stream[MM_CAMERA_RDI].id);

    mm_app_set_rdi_fmt(cam_id,&pme->stream[MM_CAMERA_RDI].str_config.fmt);
    pme->stream[MM_CAMERA_RDI].str_config.need_stream_on = 1;
    pme->stream[MM_CAMERA_RDI].str_config.num_of_bufs = num_buf;

    if (MM_CAMERA_OK != (rc = pme->cam->ops->config_stream(pme->cam->camera_handle,
                                                           pme->ch_id,
                                                           pme->stream[MM_CAMERA_RDI].id,
                                                           &pme->stream[MM_CAMERA_RDI].str_config))) {
        CDBG_ERROR("%s:RDI streaming err=%d\n", __func__, rc);
        goto end;
    }
    end:
    return rc;
}

int mm_app_unprepare_rdi(int cam_id)
{
    int rc = MM_CAMERA_OK;
    return rc;
}

int mm_app_streamon_rdi(int cam_id)
{
    int rc = MM_CAMERA_OK;
    uint32_t stream[2];
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);
    int num_of_streams;

    /*if(rdi_mode == STREAM_IMAGE_AND_RAW){
        num_of_streams = 2;
        stream[0] = pme->stream[MM_CAMERA_RDI].id;
        stream[1] = pme->stream[MM_CAMERA_PREVIEW].id;
    }else */{
        num_of_streams = 1;
        stream[0] = pme->stream[MM_CAMERA_RDI].id;
    }

    if (MM_CAMERA_OK != (rc = pme->cam->ops->start_streams(pme->cam->camera_handle,
                                                           pme->ch_id,
                                                           num_of_streams,
                                                           stream))) {
        CDBG_ERROR("%s : Start RDI Stream preview Error",__func__);
        goto end;
    }
    pme->cam_state = CAMERA_STATE_RDI;
    end:
    CDBG("%s: X rc = %d",__func__,rc);
    return rc;
}

int mm_app_start_rdi(int cam_id)
{
    int rc = MM_CAMERA_OK;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);
    int op_mode = 0;

    CDBG_ERROR("pme = %p, pme->cam =%p, pme->cam->camera_handle = %d",
         pme,pme->cam,pme->cam->camera_handle);

    if (pme->cam_state == CAMERA_STATE_RDI) {
        return rc;
    }

    rdi_op_mode = MM_CAMERA_OP_MODE_VIDEO;
    mm_app_set_opmode(cam_id, MM_CAMERA_OP_MODE_VIDEO);

    if (MM_CAMERA_OK != (rc = mm_app_prepare_rdi(cam_id, 7))) {
        CDBG_ERROR("%s:Prepare RDI failed rc=%d\n", __func__, rc);
        goto end;
    }

    if (MM_CAMERA_OK != (rc = mm_app_streamon_rdi(cam_id))) {
        CDBG_ERROR("%s:Stream On RDI failed rc=%d\n", __func__, rc);
        goto end;
    }

    end:
    CDBG("%s: END, rc=%d\n", __func__, rc); 
    return rc;
}

static int mm_app_streamoff_rdi(int cam_id)
{
    int rc = MM_CAMERA_OK;
    uint32_t stream[2];
    int num_of_streams;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    /*if(rdi_mode == STREAM_IMAGE_AND_RAW){
        num_of_streams = 2;
        stream[0] = pme->stream[MM_CAMERA_RDI].id;
        stream[1] = pme->stream[MM_CAMERA_PREVIEW].id;
    }else*/{
        num_of_streams = 1;
        stream[0] = pme->stream[MM_CAMERA_RDI].id;
    }

    if (MM_CAMERA_OK != (rc = pme->cam->ops->stop_streams(pme->cam->camera_handle,
                                                          pme->ch_id,
                                                          num_of_streams,
                                                          stream))) {
        CDBG_ERROR("%s : RDI Stream off Error",__func__);
        goto end;
    }

    /*if(rdi_mode == STREAM_IMAGE_AND_RAW) {
        if(MM_CAMERA_OK != (rc = pme->cam->ops->del_stream(pme->cam->camera_handle,pme->ch_id,pme->stream[MM_CAMERA_PREVIEW].id)))
        {
            CDBG_ERROR("%s : Delete Preview error",__func__);
            goto end;
        }
    }*/
    if (MM_CAMERA_OK != (rc = pme->cam->ops->del_stream(pme->cam->camera_handle,
                                                        pme->ch_id,
                                                        pme->stream[MM_CAMERA_RDI].id))) {
        CDBG_ERROR("%s : Delete Stream RDI error",__func__);
        goto end;
    }
    CDBG("del_stream successfull");
    pme->cam_state = CAMERA_STATE_OPEN;
    end:
    CDBG("%s: END, rc=%d\n", __func__, rc);

    return rc;
}

static int mm_app_streamdel_rdi(int cam_id)
{
    int rc = MM_CAMERA_OK;
    int stream[2];
    int num_of_streams;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    /*if(rdi_mode == STREAM_IMAGE_AND_RAW){
        num_of_streams = 2;
        stream[0] = pme->stream[MM_CAMERA_RDI].id;
        stream[1] = pme->stream[MM_CAMERA_PREVIEW].id;
    }else*/{
        num_of_streams = 1;
        stream[0] = pme->stream[MM_CAMERA_RDI].id;
    }

    if (MM_CAMERA_OK != (rc = pme->cam->ops->del_stream(pme->cam->camera_handle,pme->ch_id,pme->stream[MM_CAMERA_RDI].id))) {
        CDBG_ERROR("%s : Delete Stream RDI error",__func__);
        goto end;
    }
    CDBG("del_stream successfull");
    pme->cam_state = CAMERA_STATE_OPEN;
    end:
    CDBG("%s: END, rc=%d\n", __func__, rc);

    return rc;
}


int startRdi(int cam_id)
{
    int rc = MM_CAMERA_OK;

    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    CDBG("%s: Start Preview",__func__);

    if (pme->cam_mode == ZSL_MODE || pme->cam_mode == RECORDER_MODE || pme->cam_mode == CAMERA_MODE) {
        switch (pme->cam_state) {
        case CAMERA_STATE_RECORD:
            if (MM_CAMERA_OK != mm_app_stop_video(cam_id)) {
                CDBG_ERROR("%s:Cannot stop video err=%d\n", __func__, rc);
                return -1;
            }
        case CAMERA_STATE_PREVIEW:
            if (MM_CAMERA_OK != mm_app_open_rdi(cam_id)) {
                CDBG_ERROR("%s: Cannot switch to camera mode\n", __func__);
                return -1;
            }
        case CAMERA_STATE_SNAPSHOT:
        default:
            break;
        }
    }
    mm_app_start_rdi(cam_id);
    return rc;
}

int stopRdi(int cam_id)
{
    int rc = MM_CAMERA_OK;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);
    rdi_counter = 0;
    mm_app_streamoff_rdi(cam_id);

    end:
    return rc;
}

int takePicture_rdi(int cam_id)
{
    int rc = MM_CAMERA_OK;
    mm_camera_app_obj_t *pme = mm_app_get_cam_obj(cam_id);

    mm_app_streamoff_rdi(cam_id);
    rdi_op_mode = MM_CAMERA_OP_MODE_CAPTURE;
    mm_app_set_opmode(cam_id, MM_CAMERA_OP_MODE_CAPTURE);
    if (MM_CAMERA_OK != (rc = mm_app_prepare_rdi(cam_id, 1))) {
        CDBG_ERROR("%s:Prepare RDI failed rc=%d\n", __func__, rc);
        goto end;
    }
    if (MM_CAMERA_OK != (rc = mm_app_streamon_rdi(cam_id))) {
        CDBG_ERROR("%s:Stream On RDI failed rc=%d\n", __func__, rc);
        goto end;
    }
    mm_camera_app_wait(cam_id);
    usleep(50*1000);
    mm_app_streamoff_rdi(cam_id);
    mm_app_start_rdi(cam_id);
end:
    return rc;
}

