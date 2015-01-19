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
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>

#include "mm_jpeg_dbg.h"
#include "mm_jpeg_interface.h"
#include "mm_jpeg.h"

static pthread_mutex_t g_intf_lock = PTHREAD_MUTEX_INITIALIZER;
static mm_jpeg_obj* g_jpeg_obj = NULL;

static pthread_mutex_t g_handler_lock = PTHREAD_MUTEX_INITIALIZER;
static uint16_t g_handler_history_count = 0; /* history count for handler */

void static mm_jpeg_dump_job(mm_jpeg_job* job )
{
    #if 1
    int i;
    jpeg_image_buffer_config * buf_info;
    buf_info = &job->encode_job.encode_parm.buf_info;
    CDBG_ERROR("*****Dump job************");
    CDBG_ERROR("rotation =%d, exif_numEntries=%d, is_video_frame=%d",
               job->encode_job.encode_parm.rotation, job->encode_job.encode_parm.exif_numEntries,
               job->encode_job.encode_parm.buf_info.src_imgs.is_video_frame);
    CDBG_ERROR("Buff_inof: sin_img fd=%d, buf_len=%d, buf_add=%p: src img img num=%d", buf_info->sink_img.fd, buf_info->sink_img.buf_len,
               buf_info->sink_img.buf_vaddr,  buf_info->src_imgs.src_img_num);
    for (i=0; i < buf_info->src_imgs.src_img_num; i++) {
        CDBG_ERROR("src_img[%d] fmt=%d, col_fmt=%d, type=%d, bum_buf=%d, quality=%d, src: %dx%d, out:%dx%d, crop: %dx%d, x=%d, y=%d", i, buf_info->src_imgs.src_img[i].img_fmt,
                   buf_info->src_imgs.src_img[i].color_format,
                   buf_info->src_imgs.src_img[i].type,
                   buf_info->src_imgs.src_img[i].num_bufs,
                   buf_info->src_imgs.src_img[i].quality,
                   buf_info->src_imgs.src_img[i].src_dim.width, buf_info->src_imgs.src_img[i].src_dim.height,
                   buf_info->src_imgs.src_img[i].out_dim.width, buf_info->src_imgs.src_img[i].out_dim.height,
                   buf_info->src_imgs.src_img[i].crop.width, buf_info->src_imgs.src_img[i].crop.height,
                   buf_info->src_imgs.src_img[i].crop.offset_x, buf_info->src_imgs.src_img[i].crop.offset_y
                   );
    }

    #endif

}

/* utility function to generate handler */
uint32_t mm_jpeg_util_generate_handler(uint8_t index)
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

uint8_t mm_jpeg_util_get_index_by_handler(uint32_t handler)
{
    return (handler&0x000000ff);
}

static int32_t mm_jpeg_intf_start_job(uint32_t client_hdl, mm_jpeg_job* job, uint32_t* jobId)
{
    int32_t rc = -1;

    if (0 == client_hdl ||
        NULL == job ||
        NULL == jobId) {
        CDBG_ERROR("%s: invalid parameters for client_hdl, job or jobId", __func__);
        return rc;
    }

    pthread_mutex_lock(&g_intf_lock);
    if (NULL == g_jpeg_obj) {
        /* mm_jpeg obj not exists, return error */
        CDBG_ERROR("%s: mm_jpeg is not opened yet", __func__);
        pthread_mutex_unlock(&g_intf_lock);
        return rc;
    }
    mm_jpeg_dump_job(job);
    rc = mm_jpeg_start_job(g_jpeg_obj, client_hdl, job, jobId);
    pthread_mutex_unlock(&g_intf_lock);
    return rc;
}

static int32_t mm_jpeg_intf_abort_job(uint32_t client_hdl, uint32_t jobId)
{
    int32_t rc = -1;

    if (0 == client_hdl || 0 == jobId) {
        CDBG_ERROR("%s: invalid client_hdl or jobId", __func__);
        return rc;
    }

    pthread_mutex_lock(&g_intf_lock);
    if (NULL == g_jpeg_obj) {
        /* mm_jpeg obj not exists, return error */
        CDBG_ERROR("%s: mm_jpeg is not opened yet", __func__);
        pthread_mutex_unlock(&g_intf_lock);
        return rc;
    }

    rc = mm_jpeg_abort_job(g_jpeg_obj, client_hdl, jobId);
    pthread_mutex_unlock(&g_intf_lock);
    return rc;
}

static int32_t mm_jpeg_intf_close(uint32_t client_hdl)
{
    int32_t rc = -1;

    if (0 == client_hdl) {
        CDBG_ERROR("%s: invalid client_hdl", __func__);
        return rc;
    }

    pthread_mutex_lock(&g_intf_lock);
    if (NULL == g_jpeg_obj) {
        /* mm_jpeg obj not exists, return error */
        CDBG_ERROR("%s: mm_jpeg is not opened yet", __func__);
        pthread_mutex_unlock(&g_intf_lock);
        return rc;
    }

    rc = mm_jpeg_close(g_jpeg_obj, client_hdl);
    g_jpeg_obj->num_clients--;
    if(0 == rc) {
        if (0 == g_jpeg_obj->num_clients) {
            /* No client, close jpeg internally */
            rc = mm_jpeg_deinit(g_jpeg_obj);
            free(g_jpeg_obj);
            g_jpeg_obj = NULL;
        }
    }

    pthread_mutex_unlock(&g_intf_lock);
    return rc;
}

/* open jpeg client */
uint32_t jpeg_open(mm_jpeg_ops_t *ops)
{
    int32_t rc = 0;
    uint32_t clnt_hdl = 0;
    mm_jpeg_obj* jpeg_obj = NULL;

    pthread_mutex_lock(&g_intf_lock);
    /* first time open */
    if(NULL == g_jpeg_obj) {
        jpeg_obj = (mm_jpeg_obj *)malloc(sizeof(mm_jpeg_obj));
        if(NULL == jpeg_obj) {
            CDBG_ERROR("%s:  no mem", __func__);
            pthread_mutex_unlock(&g_intf_lock);
            return clnt_hdl;
        }

        /* initialize jpeg obj */
        memset(jpeg_obj, 0, sizeof(mm_jpeg_obj));
        rc = mm_jpeg_init(jpeg_obj);
        if(0 != rc) {
            CDBG_ERROR("%s: mm_jpeg_init err = %d", __func__, rc);
            free(jpeg_obj);
            pthread_mutex_unlock(&g_intf_lock);
            return clnt_hdl;
        }

        /* remember in global variable */
        g_jpeg_obj = jpeg_obj;
    }

    /* open new client */
    clnt_hdl = mm_jpeg_new_client(g_jpeg_obj);
    if (clnt_hdl > 0) {
        /* valid client */
        if (NULL != ops) {
            /* fill in ops tbl if ptr not NULL */
            ops->start_job = mm_jpeg_intf_start_job;
            ops->abort_job = mm_jpeg_intf_abort_job;
            //ops->abort_job_all = mm_jpeg_intf_close,
            ops->close = mm_jpeg_intf_close;
        }
    } else {
        /* failed new client */
        CDBG_ERROR("%s: mm_jpeg_new_client failed", __func__);

        if (0 == g_jpeg_obj->num_clients) {
            /* no client, close jpeg */
            mm_jpeg_deinit(g_jpeg_obj);
            free(g_jpeg_obj);
            g_jpeg_obj = NULL;
        }
    }

    pthread_mutex_unlock(&g_intf_lock);
    return clnt_hdl;
}
