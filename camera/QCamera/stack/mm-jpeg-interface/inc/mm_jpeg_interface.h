/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
 */

#ifndef MM_JPEG_INTERFACE_H_
#define MM_JPEG_INTERFACE_H_
#include "QCamera_Intf.h"

typedef struct {
    int width;
    int height;
} image_resolution;

typedef enum {
    JPEG_SRC_IMAGE_FMT_YUV,
    JPEG_SRC_IMAGE_FMT_BITSTREAM
} jpeg_enc_src_img_fmt_t;

typedef enum {
    JPEG_SRC_IMAGE_TYPE_MAIN,
    JPEG_SRC_IMAGE_TYPE_THUMB,
    JPEG_SRC_IMAGE_TYPE_MAX
} jpeg_enc_src_img_type_t;

typedef struct  {
    int32_t offset_x;
    int32_t offset_y;
    int32_t width;
    int32_t height;
} image_crop_t;

typedef struct {
    uint8_t sequence;                /*for jpeg bit streams, assembling is based on sequence. sequence starts from 0*/
    uint8_t *buf_vaddr;              /*ptr to buf*/
    int fd;                          /*fd of buf*/
    uint32_t buf_size;               /* total size of buf (header + image) */
    uint32_t data_offset;            /*data offset*/
} src_bitstream_buffer_t;

typedef struct {
    uint8_t *buf_vaddr;              /*ptr to buf*/
    int fd;                          /*fd of buf*/
    cam_frame_len_offset_t offset;   /*alway use multi-planar, offset is used to skip the metadata header*/
} src_image_buffer_t;

typedef enum {
    MM_JPEG_COLOR_FORMAT_YCRCBLP_H2V2,
    MM_JPEG_COLOR_FORMAT_YCBCRLP_H2V2,
    MM_JPEG_COLOR_FORMAT_YCRCBLP_H2V1,
    MM_JPEG_COLOR_FORMAT_YCBCRLP_H2V1,
    MM_JPEG_COLOR_FORMAT_YCRCBLP_H1V2,
    MM_JPEG_COLOR_FORMAT_YCBCRLP_H1V2,
    MM_JPEG_COLOR_FORMAT_YCRCBLP_H1V1,
    MM_JPEG_COLOR_FORMAT_YCBCRLP_H1V1,
    MM_JPEG_COLOR_FORMAT_RGB565,
    MM_JPEG_COLOR_FORMAT_RGB888,
    MM_JPEG_COLOR_FORMAT_RGBa,
    MM_JPEG_COLOR_FORMAT_BITSTREAM,
    MM_JPEG_COLOR_FORMAT_MAX
} mm_jpeg_color_format;

#define MAX_SRC_BUF_NUM 2
typedef struct {
    /* src img format: YUV, Bitstream */
    jpeg_enc_src_img_fmt_t img_fmt;

	/* num of buf in src img */
    uint8_t num_bufs;

	/* src img bufs */
    union {
        src_bitstream_buffer_t bit_stream[MAX_SRC_BUF_NUM];
        src_image_buffer_t src_image[MAX_SRC_BUF_NUM];
    };

    /* src img type: main or thumbnail */
    jpeg_enc_src_img_type_t type;

    /* color format */
    mm_jpeg_color_format color_format;

    /* src img dimension */
    image_resolution src_dim;

    /* jpeg output dimension */
    image_resolution out_dim;

    /* crop information */
    image_crop_t crop;

    /* jpeg quality: range 0~100 */
    uint32_t quality;
} src_image_buffer_info;

typedef struct {
  uint8_t *buf_vaddr;              /*ptr to buf*/
  int fd;                          /*fd of buf*/
  int buf_len;
} out_image_buffer_info;

typedef struct {
    /* num of src imgs: e.g. main/thumbnail img
     * if main img only: src_img_num = 1;
     * if main+thumbnail: src_img_num = 2;
     * No support for thumbnail only case */
    uint8_t src_img_num;

    /* index 0 is always for main image
     * if thumbnail presented, it will be in index 1 */
    src_image_buffer_info src_img[JPEG_SRC_IMAGE_TYPE_MAX];

    /* flag indicating if buffer is from video stream (special case for video-sized live snapshot) */
    uint8_t is_video_frame;
} src_image_buffer_config;

typedef struct {
    src_image_buffer_config src_imgs;
    out_image_buffer_info sink_img;
} jpeg_image_buffer_config;

typedef struct {
    /* config for scr images */
    jpeg_image_buffer_config buf_info;

    /* rotation informaiton */
    int rotation;

    /* num of exif entries */
    int exif_numEntries;

    /* buf to exif entries, caller needs to
     * take care of the memory manage with insider ptr */
    exif_tags_info_t *exif_data;

    /*for mobicat support*/
    const uint8_t * mobicat_data;
    int32_t mobicat_data_length;
    int hasmobicat;
} mm_jpeg_encode_params;

typedef enum {
  JPEG_JOB_STATUS_DONE = 0,
  JPEG_JOB_STATUS_ERROR
} jpeg_job_status_t;

typedef void (*jpeg_encode_callback_t)(jpeg_job_status_t status,
                                       uint8_t thumbnailDroppedFlag,
                                       uint32_t client_hdl,
                                       uint32_t jobId,
                                       uint8_t* out_data,
                                       uint32_t data_size,
                                       void *userData);

typedef struct {
    mm_jpeg_encode_params encode_parm;
    jpeg_encode_callback_t jpeg_cb;
    void* userdata;
} mm_jpeg_encode_job;

typedef enum {
    JPEG_JOB_TYPE_ENCODE,
    //JPEG_JOB_TYPE_DECODE, /*enable decode later*/
    JPEG_JOB_TYPE_MAX
} mm_jpeg_job_type_t;

typedef struct {
    mm_jpeg_job_type_t job_type;
    union {
        mm_jpeg_encode_job encode_job;
    };
} mm_jpeg_job;

typedef struct {
    /* start a job -- async call
     * the result of job (DONE/ERROR) will rcvd through CB */
    int32_t (* start_job) (uint32_t client_hdl, mm_jpeg_job* job, uint32_t* jobId);

    /* abort a job -- sync call */
    int32_t  (* abort_job) (uint32_t client_hdl, uint32_t jobId);

    /* close a jpeg client -- sync call */
    int32_t  (* close) (uint32_t clientHdl);
} mm_jpeg_ops_t;

/* open a jpeg client -- sync call
 * returns client_handle.
 * failed if client_handle=0
 * jpeg ops tbl will be filled in if open succeeds */
uint32_t jpeg_open(mm_jpeg_ops_t *ops);

#endif /* MM_JPEG_INTERFACE_H_ */


