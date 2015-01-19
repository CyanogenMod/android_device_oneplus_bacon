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
#include <poll.h>
#include <semaphore.h>

#include "mm_jpeg_dbg.h"
#include "mm_jpeg_interface.h"
#include "mm_jpeg.h"

/* define max num of supported concurrent jpeg jobs by OMX engine.
 * Current, only one per time */
#define NUM_MAX_JPEG_CNCURRENT_JOBS 1

#define INPUT_PORT_MAIN         0
#define INPUT_PORT_THUMBNAIL    2
#define OUTPUT_PORT             1

void mm_jpeg_job_wait_for_event(mm_jpeg_obj *my_obj, uint32_t evt_mask);
void mm_jpeg_job_wait_for_cmd_complete(mm_jpeg_obj *my_obj,
                                       int cmd,
                                       int status);
OMX_ERRORTYPE mm_jpeg_etbdone(OMX_HANDLETYPE hComponent,
                              OMX_PTR pAppData,
                              OMX_BUFFERHEADERTYPE* pBuffer);
OMX_ERRORTYPE mm_jpeg_ftbdone(OMX_HANDLETYPE hComponent,
                              OMX_PTR pAppData,
                              OMX_BUFFERHEADERTYPE* pBuffer);
OMX_ERRORTYPE mm_jpeg_handle_omx_event(OMX_HANDLETYPE hComponent,
                                       OMX_PTR pAppData,
                                       OMX_EVENTTYPE eEvent,
                                       OMX_U32 nData1,
                                       OMX_U32 nData2,
                                       OMX_PTR pEventData);
/* special queue functions for job queue */
int32_t mm_jpeg_queue_update_flag(mm_jpeg_queue_t* queue, uint32_t job_id, uint8_t flag);
mm_jpeg_job_q_node_t* mm_jpeg_queue_remove_job_by_client_id(mm_jpeg_queue_t* queue, uint32_t client_hdl);
mm_jpeg_job_q_node_t* mm_jpeg_queue_remove_job_by_job_id(mm_jpeg_queue_t* queue, uint32_t job_id);

static OMX_INDEXTYPE mobicat_data;
static omx_jpeg_mobicat mobicat_d;

int32_t mm_jpeg_omx_load(mm_jpeg_obj* my_obj)
{
    int32_t rc = 0;

    my_obj->omx_callbacks.EmptyBufferDone = mm_jpeg_etbdone;
    my_obj->omx_callbacks.FillBufferDone = mm_jpeg_ftbdone;
    my_obj->omx_callbacks.EventHandler = mm_jpeg_handle_omx_event;
    rc = OMX_GetHandle(&my_obj->omx_handle,
                       "OMX.qcom.image.jpeg.encoder",
                       (void*)my_obj,
                       &my_obj->omx_callbacks);

    if (0 != rc) {
        CDBG_ERROR("%s : OMX_GetHandle failed (%d)",__func__, rc);
        return rc;
    }

    rc = OMX_Init();
    if (0 != rc) {
        CDBG_ERROR("%s : OMX_Init failed (%d)",__func__, rc);
    }

    return rc;
}

int32_t mm_jpeg_omx_unload(mm_jpeg_obj *my_obj)
{
    int32_t rc = 0;
    rc = OMX_Deinit();
    return rc;
}

int32_t mm_jpeg_clean_omx_job(mm_jpeg_obj *my_obj, mm_jpeg_job_entry* job_entry)
{
    int32_t rc = 0;
    uint8_t i, j;

    OMX_SendCommand(my_obj->omx_handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    OMX_SendCommand(my_obj->omx_handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);

    /* free buffers used in OMX processing */
    for (i = 0; i < JPEG_SRC_IMAGE_TYPE_MAX; i++) {
        for (j = 0; j < job_entry->src_bufs[i].num_bufs; j++) {
            if (NULL != job_entry->src_bufs[i].bufs[j].buf_header) {
                OMX_FreeBuffer(my_obj->omx_handle,
                               job_entry->src_bufs[i].bufs[j].portIdx,
                               job_entry->src_bufs[i].bufs[j].buf_header);
            }
        }
    }
    OMX_FreeBuffer(my_obj->omx_handle,
                   job_entry->sink_buf.portIdx,
                   job_entry->sink_buf.buf_header);

    return rc;
}

int32_t mm_jpeg_omx_abort_job(mm_jpeg_obj *my_obj, mm_jpeg_job_entry* job_entry)
{
    int32_t rc = 0;

    OMX_SendCommand(my_obj->omx_handle, OMX_CommandFlush, 0, NULL);
    mm_jpeg_job_wait_for_event(my_obj,
        MM_JPEG_EVENT_MASK_JPEG_DONE|MM_JPEG_EVENT_MASK_JPEG_ABORT|MM_JPEG_EVENT_MASK_JPEG_ERROR);
    CDBG("%s:waitForEvent: OMX_CommandFlush: DONE", __func__);

    /* clean omx job */
    mm_jpeg_clean_omx_job(my_obj, job_entry);

    return rc;
}

/* TODO: needs revisit after omx lib supports multi src buffers */
int32_t mm_jpeg_omx_config_main_buffer_offset(mm_jpeg_obj* my_obj,
                                              src_image_buffer_info *src_buf,
                                              uint8_t is_video_frame)
{
    int32_t rc = 0;
    uint8_t i;
    OMX_INDEXTYPE buf_offset_idx;
    omx_jpeg_buffer_offset buffer_offset;

    for (i = 0; i < src_buf->num_bufs; i++) {
        OMX_GetExtensionIndex(my_obj->omx_handle,
                              "omx.qcom.jpeg.exttype.buffer_offset",
                              &buf_offset_idx);
        memset(&buffer_offset, 0, sizeof(buffer_offset));

        switch (src_buf->img_fmt) {
        case JPEG_SRC_IMAGE_FMT_YUV:
            if (1 == src_buf->src_image[i].offset.num_planes) {
                buffer_offset.yOffset =
                    src_buf->src_image[i].offset.sp.y_offset;
                buffer_offset.cbcrOffset =
                    src_buf->src_image[i].offset.sp.cbcr_offset;
                buffer_offset.totalSize =
                    src_buf->src_image[i].offset.sp.len;
            } else {
                buffer_offset.yOffset =
                    src_buf->src_image[i].offset.mp[0].offset;
                buffer_offset.cbcrOffset =
                    src_buf->src_image[i].offset.mp[1].offset;
                buffer_offset.totalSize =
                    src_buf->src_image[i].offset.frame_len;
            }
            CDBG("%s: idx=%d, yOffset =%d, cbcrOffset =%d, totalSize = %d\n",
                 __func__, i, buffer_offset.yOffset, buffer_offset.cbcrOffset, buffer_offset.totalSize);
            OMX_SetParameter(my_obj->omx_handle, buf_offset_idx, &buffer_offset);

            /* set acbcr (special case for video-sized live snapshot)*/
            if (is_video_frame) {
                CDBG("Using acbcroffset\n");
                memset(&buffer_offset, 0, sizeof(buffer_offset));
                buffer_offset.cbcrOffset = src_buf->src_image[i].offset.mp[0].offset +
                                          src_buf->src_image[i].offset.mp[0].len +
                                          src_buf->src_image[i].offset.mp[1].offset;;
                OMX_GetExtensionIndex(my_obj->omx_handle,"omx.qcom.jpeg.exttype.acbcr_offset",&buf_offset_idx);
                OMX_SetParameter(my_obj->omx_handle, buf_offset_idx, &buffer_offset);
            }
            break;
        case JPEG_SRC_IMAGE_FMT_BITSTREAM:
            /* TODO: need visit here when bit stream is supported */
            buffer_offset.yOffset =
                src_buf->bit_stream[i].data_offset;
            buffer_offset.totalSize =
                src_buf->bit_stream[i].buf_size;
            CDBG("%s: idx=%d, yOffset =%d, cbcrOffset =%d, totalSize = %d\n",
                 __func__, i, buffer_offset.yOffset, buffer_offset.cbcrOffset, buffer_offset.totalSize);
            OMX_SetParameter(my_obj->omx_handle, buf_offset_idx, &buffer_offset);
            break;
        default:
            break;
        }
    }

    return rc;
}

/* TODO: needs revisit after omx lib supports multi src buffers */
int32_t mm_jpeg_omx_config_port(mm_jpeg_obj* my_obj, src_image_buffer_info *src_buf, int port_idx)
{
    int32_t rc = 0;
    uint8_t i;
    OMX_PARAM_PORTDEFINITIONTYPE input_port;

    for (i = 0; i < src_buf->num_bufs; i++) {
        memset(&input_port, 0, sizeof(input_port));
        input_port.nPortIndex = port_idx;
        OMX_GetParameter(my_obj->omx_handle, OMX_IndexParamPortDefinition, &input_port);
        input_port.format.image.nFrameWidth = src_buf->src_dim.width;
        input_port.format.image.nFrameHeight =src_buf->src_dim.height;
        input_port.format.image.nStride = src_buf->src_dim.width;
        input_port.format.image.nSliceHeight = src_buf->src_dim.height;
        switch (src_buf->img_fmt) {
        case JPEG_SRC_IMAGE_FMT_YUV:
            input_port.nBufferSize = src_buf->src_image[i].offset.frame_len;
            break;
        case JPEG_SRC_IMAGE_FMT_BITSTREAM:
            input_port.nBufferSize = src_buf->bit_stream[i].buf_size;
            break;
        }

        CDBG("%s: port_idx=%d, width =%d, height =%d, stride = %d, slice = %d, bufsize = %d\n",
             __func__, port_idx,
             input_port.format.image.nFrameWidth, input_port.format.image.nFrameHeight,
             input_port.format.image.nStride, input_port.format.image.nSliceHeight,
             input_port.nBufferSize);
        OMX_SetParameter(my_obj->omx_handle, OMX_IndexParamPortDefinition, &input_port);
    }

    return rc;
}

omx_jpeg_color_format map_jpeg_format(mm_jpeg_color_format color_fmt)
{
    switch (color_fmt) {
    case MM_JPEG_COLOR_FORMAT_YCRCBLP_H2V2:
        return OMX_YCRCBLP_H2V2;
    case MM_JPEG_COLOR_FORMAT_YCBCRLP_H2V2:
        return OMX_YCBCRLP_H2V2;
    case MM_JPEG_COLOR_FORMAT_YCRCBLP_H2V1:
        return OMX_YCRCBLP_H2V1;
    case MM_JPEG_COLOR_FORMAT_YCBCRLP_H2V1:
        return OMX_YCBCRLP_H2V1;
    case MM_JPEG_COLOR_FORMAT_YCRCBLP_H1V2:
        return OMX_YCRCBLP_H1V2;
    case MM_JPEG_COLOR_FORMAT_YCBCRLP_H1V2:
        return OMX_YCBCRLP_H1V2;
    case MM_JPEG_COLOR_FORMAT_YCRCBLP_H1V1:
        return OMX_YCRCBLP_H1V1;
    case MM_JPEG_COLOR_FORMAT_YCBCRLP_H1V1:
        return OMX_YCBCRLP_H1V1;
    case MM_JPEG_COLOR_FORMAT_RGB565:
        return OMX_RGB565;
    case MM_JPEG_COLOR_FORMAT_RGB888:
        return OMX_RGB888;
    case MM_JPEG_COLOR_FORMAT_RGBa:
        return OMX_RGBa;
    case MM_JPEG_COLOR_FORMAT_BITSTREAM:
        /* TODO: need to change to new bitstream value once omx interface changes */
        return OMX_JPEG_BITSTREAM_H2V2;
    default:
        return OMX_JPEG_COLOR_FORMAT_MAX;
    }
}

int32_t mm_jpeg_omx_config_user_preference(mm_jpeg_obj* my_obj, mm_jpeg_encode_job* job)
{
    int32_t rc = 0;
    OMX_INDEXTYPE user_pref_idx;
    omx_jpeg_user_preferences user_preferences;

    memset(&user_preferences, 0, sizeof(user_preferences));
    user_preferences.color_format =
        map_jpeg_format(job->encode_parm.buf_info.src_imgs.src_img[JPEG_SRC_IMAGE_TYPE_MAIN].color_format);
    if (job->encode_parm.buf_info.src_imgs.src_img_num > 1) {
        user_preferences.thumbnail_color_format =
            map_jpeg_format(job->encode_parm.buf_info.src_imgs.src_img[JPEG_SRC_IMAGE_TYPE_THUMB].color_format);
    }
    OMX_GetExtensionIndex(my_obj->omx_handle,
                          "omx.qcom.jpeg.exttype.user_preferences",
                          &user_pref_idx);
    CDBG("%s:User Preferences: color_format %d, thumbnail_color_format = %d",
         __func__, user_preferences.color_format, user_preferences.thumbnail_color_format);

    if (job->encode_parm.buf_info.src_imgs.is_video_frame != 0) {
        user_preferences.preference = OMX_JPEG_PREF_SOFTWARE_ONLY;
    } else {
        user_preferences.preference = OMX_JPEG_PREF_HW_ACCELERATED_PREFERRED;
    }

    OMX_SetParameter(my_obj->omx_handle, user_pref_idx, &user_preferences);
    return rc;
}

int32_t mm_jpeg_omx_config_thumbnail(mm_jpeg_obj* my_obj, mm_jpeg_encode_job* job)
{
    int32_t rc = -1;
    OMX_INDEXTYPE thumb_idx, q_idx;
    omx_jpeg_thumbnail thumbnail;
    omx_jpeg_thumbnail_quality quality;

    if (job->encode_parm.buf_info.src_imgs.src_img_num < 2) {
        /*No thumbnail is required*/
        return 0;
    }
    src_image_buffer_info *src_buf =
        &(job->encode_parm.buf_info.src_imgs.src_img[JPEG_SRC_IMAGE_TYPE_THUMB]);

    /* config port */
    CDBG("%s: config port", __func__);
    rc = mm_jpeg_omx_config_port(my_obj, src_buf, INPUT_PORT_THUMBNAIL);
    if (0 != rc) {
        CDBG_ERROR("%s: config port failed", __func__);
        return rc;
    }

    if ((src_buf->crop.width == 0) || (src_buf->crop.height == 0)) {
        src_buf->crop.width = src_buf->src_dim.width;
        src_buf->crop.height = src_buf->src_dim.height;
    }

    /* check crop boundary */
    if ((src_buf->crop.width + src_buf->crop.offset_x > src_buf->src_dim.width) ||
        (src_buf->crop.height + src_buf->crop.offset_y > src_buf->src_dim.height)) {
        CDBG_ERROR("%s: invalid crop boundary (%d, %d) offset (%d, %d) out of (%d, %d)",
                   __func__,
                   src_buf->crop.width,
                   src_buf->crop.height,
                   src_buf->crop.offset_x,
                   src_buf->crop.offset_y,
                   src_buf->src_dim.width,
                   src_buf->src_dim.height);
        return -1;
    }

    memset(&thumbnail, 0, sizeof(thumbnail));

    /* thumbnail crop info */
    thumbnail.cropWidth = CEILING2(src_buf->crop.width);
    thumbnail.cropHeight = CEILING2(src_buf->crop.height);
    thumbnail.left = src_buf->crop.offset_x;
    thumbnail.top = src_buf->crop.offset_y;

    /* thumbnail output dimention */
    thumbnail.width = src_buf->out_dim.width;
    thumbnail.height = src_buf->out_dim.height;

    /* set scaling flag */
    if (thumbnail.left > 0 || thumbnail.top > 0 ||
        src_buf->crop.width != src_buf->src_dim.width ||
        src_buf->crop.height != src_buf->src_dim.height ||
        src_buf->src_dim.width != src_buf->out_dim.width ||
        src_buf->src_dim.height != src_buf->out_dim.height) {
        thumbnail.scaling = 1;
    }

    /* set omx thumbnail info */
    OMX_GetExtensionIndex(my_obj->omx_handle, "omx.qcom.jpeg.exttype.thumbnail", &thumb_idx);
    OMX_SetParameter(my_obj->omx_handle, thumb_idx, &thumbnail);
    CDBG("%s: set thumbnail info: crop_w=%d, crop_h=%d, l=%d, t=%d, w=%d, h=%d, scaling=%d",
          __func__, thumbnail.cropWidth, thumbnail.cropHeight,
          thumbnail.left, thumbnail.top, thumbnail.width, thumbnail.height,
          thumbnail.scaling);

    OMX_GetExtensionIndex(my_obj->omx_handle, "omx.qcom.jpeg.exttype.thumbnail_quality", &q_idx);
    OMX_GetParameter(my_obj->omx_handle, q_idx, &quality);
    quality.nQFactor = src_buf->quality;
    OMX_SetParameter(my_obj->omx_handle, q_idx, &quality);
    CDBG("%s: thumbnail_quality=%d", __func__, quality.nQFactor);

    rc = 0;
    return rc;
}

int32_t mm_jpeg_omx_config_main_crop(mm_jpeg_obj* my_obj, src_image_buffer_info *src_buf)
{
    int32_t rc = 0;
    OMX_CONFIG_RECTTYPE rect_type_in, rect_type_out;

    if ((src_buf->crop.width == 0) || (src_buf->crop.height == 0)) {
        src_buf->crop.width = src_buf->src_dim.width;
        src_buf->crop.height = src_buf->src_dim.height;
    }
    /* error check first */
    if ((src_buf->crop.width + src_buf->crop.offset_x > src_buf->src_dim.width) ||
        (src_buf->crop.height + src_buf->crop.offset_y > src_buf->src_dim.height)) {
        CDBG_ERROR("%s: invalid crop boundary (%d, %d) out of (%d, %d)", __func__,
                   src_buf->crop.width + src_buf->crop.offset_x,
                   src_buf->crop.height + src_buf->crop.offset_y,
                   src_buf->src_dim.width,
                   src_buf->src_dim.height);
        return -1;
    }

    memset(&rect_type_in, 0, sizeof(rect_type_in));
    memset(&rect_type_out, 0, sizeof(rect_type_out));
    rect_type_in.nPortIndex = OUTPUT_PORT;
    rect_type_out.nPortIndex = OUTPUT_PORT;

    if ((src_buf->src_dim.width != src_buf->crop.width) ||
        (src_buf->src_dim.height != src_buf->crop.height) ||
        (src_buf->src_dim.width != src_buf->out_dim.width) ||
        (src_buf->src_dim.height != src_buf->out_dim.height)) {
        /* Scaler information */
        rect_type_in.nWidth = CEILING2(src_buf->crop.width);
        rect_type_in.nHeight = CEILING2(src_buf->crop.height);
        rect_type_in.nLeft = src_buf->crop.offset_x;
        rect_type_in.nTop = src_buf->crop.offset_y;

        if (src_buf->out_dim.width && src_buf->out_dim.height) {
            rect_type_out.nWidth = src_buf->out_dim.width;
            rect_type_out.nHeight = src_buf->out_dim.height;
        }
    }

    OMX_SetConfig(my_obj->omx_handle, OMX_IndexConfigCommonInputCrop, &rect_type_in);
    CDBG("%s: OMX_IndexConfigCommonInputCrop w=%d, h=%d, l=%d, t=%d, port_idx=%d", __func__,
         rect_type_in.nWidth, rect_type_in.nHeight,
         rect_type_in.nLeft, rect_type_in.nTop,
         rect_type_in.nPortIndex);

    OMX_SetConfig(my_obj->omx_handle, OMX_IndexConfigCommonOutputCrop, &rect_type_out);
    CDBG("%s: OMX_IndexConfigCommonOutputCrop w=%d, h=%d, port_idx=%d", __func__,
         rect_type_out.nWidth, rect_type_out.nHeight,
         rect_type_out.nPortIndex);

    return rc;
}

int32_t mm_jpeg_omx_config_main(mm_jpeg_obj* my_obj, mm_jpeg_encode_job* job)
{
    int32_t rc = 0;
    src_image_buffer_info *src_buf =
        &(job->encode_parm.buf_info.src_imgs.src_img[JPEG_SRC_IMAGE_TYPE_MAIN]);
    OMX_IMAGE_PARAM_QFACTORTYPE q_factor;

    /* config port */
    CDBG("%s: config port", __func__);
    rc = mm_jpeg_omx_config_port(my_obj, src_buf, INPUT_PORT_MAIN);
    if (0 != rc) {
        CDBG_ERROR("%s: config port failed", __func__);
        return rc;
    }

    /* config buffer offset */
    CDBG("%s: config main buf offset", __func__);
    rc = mm_jpeg_omx_config_main_buffer_offset(my_obj, src_buf, job->encode_parm.buf_info.src_imgs.is_video_frame);
    if (0 != rc) {
        CDBG_ERROR("%s: config buffer offset failed", __func__);
        return rc;
    }

    /* config crop */
    CDBG("%s: config main crop", __func__);
    rc = mm_jpeg_omx_config_main_crop(my_obj, src_buf);
    if (0 != rc) {
        CDBG_ERROR("%s: config crop failed", __func__);
        return rc;
    }

    /* set quality */
    memset(&q_factor, 0, sizeof(q_factor));
    q_factor.nPortIndex = INPUT_PORT_MAIN;
    OMX_GetParameter(my_obj->omx_handle, OMX_IndexParamQFactor, &q_factor);
    q_factor.nQFactor = src_buf->quality;
    OMX_SetParameter(my_obj->omx_handle, OMX_IndexParamQFactor, &q_factor);
    CDBG("%s: config QFactor: %d", __func__, q_factor.nQFactor);

    return rc;
}

int32_t mm_jpeg_omx_config_common(mm_jpeg_obj* my_obj, mm_jpeg_encode_job* job)
{
    int32_t rc;
    int i;
    OMX_INDEXTYPE exif_idx;
    omx_jpeg_exif_info_tag tag;
    OMX_CONFIG_ROTATIONTYPE rotate;

    /* config user prefernces */
    CDBG("%s: config user preferences", __func__);
    rc = mm_jpeg_omx_config_user_preference(my_obj, job);
    if (0 != rc) {
        CDBG_ERROR("%s: config user preferences failed", __func__);
        return rc;
    }

    /* set rotation */
    memset(&rotate, 0, sizeof(rotate));
    rotate.nPortIndex = OUTPUT_PORT;
    rotate.nRotation = job->encode_parm.rotation;
    OMX_SetConfig(my_obj->omx_handle, OMX_IndexConfigCommonRotate, &rotate);
    CDBG("%s: Set rotation to %d at port_idx=%d", __func__,
         job->encode_parm.rotation, rotate.nPortIndex);

    /* set exif tags */
    CDBG("%s: Set rexif tags", __func__);
    OMX_GetExtensionIndex(my_obj->omx_handle, "omx.qcom.jpeg.exttype.exif", &exif_idx);
    for(i = 0; i < job->encode_parm.exif_numEntries; i++) {
        memcpy(&tag, job->encode_parm.exif_data + i, sizeof(omx_jpeg_exif_info_tag));
        OMX_SetParameter(my_obj->omx_handle, exif_idx, &tag);
    }

    /* set mobicat info */
    CDBG("%s: set Mobicat info", __func__);
    if(job->encode_parm.hasmobicat) {
        mobicat_d.mobicatData = job->encode_parm.mobicat_data;
        mobicat_d.mobicatDataLength =  job->encode_parm.mobicat_data_length;
        OMX_GetExtensionIndex(my_obj->omx_handle, "omx.qcom.jpeg.exttype.mobicat", &mobicat_data);
        OMX_SetParameter(my_obj->omx_handle, mobicat_data, &mobicat_d);
    }

    return rc;
}

int32_t mm_jpeg_omx_use_buf(mm_jpeg_obj* my_obj,
                            src_image_buffer_info *src_buf,
                            mm_jpeg_omx_src_buf* omx_src_buf,
                            int port_idx)
{
    int32_t rc = 0;
    uint8_t i;
    omx_jpeg_pmem_info pmem_info;

    omx_src_buf->num_bufs = src_buf->num_bufs;
    for (i = 0; i < src_buf->num_bufs; i++) {
        memset(&pmem_info, 0, sizeof(pmem_info));
        switch (src_buf->img_fmt) {
        case JPEG_SRC_IMAGE_FMT_YUV:
            pmem_info.fd = src_buf->src_image[i].fd;
            pmem_info.offset = 0;
            omx_src_buf->bufs[i].portIdx = port_idx;
            OMX_UseBuffer(my_obj->omx_handle,
                          &omx_src_buf->bufs[i].buf_header,
                          port_idx,
                          &pmem_info,
                          src_buf->src_image[i].offset.frame_len,
                          (void *)src_buf->src_image[i].buf_vaddr);
            CDBG("%s: port_idx=%d, fd=%d, offset=%d, len=%d, ptr=%p",
                 __func__, port_idx, pmem_info.fd, pmem_info.offset,
                 src_buf->src_image[i].offset.frame_len,
                 src_buf->src_image[i].buf_vaddr);
            break;
        case JPEG_SRC_IMAGE_FMT_BITSTREAM:
            pmem_info.fd = src_buf->bit_stream[i].fd;
            pmem_info.offset = 0;
            omx_src_buf->bufs[i].portIdx = port_idx;
            OMX_UseBuffer(my_obj->omx_handle,
                          &omx_src_buf->bufs[i].buf_header,
                          port_idx,
                          &pmem_info,
                          src_buf->bit_stream[i].buf_size,
                          (void *)src_buf->bit_stream[i].buf_vaddr);
            break;
        default:
            rc = -1;
            break;
        }
    }

    return rc;
}

int32_t mm_jpeg_omx_encode(mm_jpeg_obj* my_obj, mm_jpeg_job_entry* job_entry)
{
    int32_t rc = 0;
    uint8_t i;
    mm_jpeg_encode_job* job = &job_entry->job.encode_job;
    uint8_t has_thumbnail = job->encode_parm.buf_info.src_imgs.src_img_num > 1? 1 : 0;
    src_image_buffer_info *src_buf[JPEG_SRC_IMAGE_TYPE_MAX];
    mm_jpeg_omx_src_buf *omx_src_buf[JPEG_SRC_IMAGE_TYPE_MAX];

    /* config main img */
    rc = mm_jpeg_omx_config_main(my_obj, job);
    if (0 != rc) {
        CDBG_ERROR("%s: config main img failed", __func__);
        return rc;
    }

    /* config thumbnail */
    if (has_thumbnail) {
        rc = mm_jpeg_omx_config_thumbnail(my_obj, job);
        if (0 != rc) {
            CDBG_ERROR("%s: config thumbnail img failed", __func__);
            return rc;
        }
    }

    /* common config */
    rc = mm_jpeg_omx_config_common(my_obj, job);
    if (0 != rc) {
        CDBG_ERROR("%s: config common failed", __func__);
        return rc;
    }

    /* config input omx buffer for main input */
    src_buf[JPEG_SRC_IMAGE_TYPE_MAIN] = &(job->encode_parm.buf_info.src_imgs.src_img[JPEG_SRC_IMAGE_TYPE_MAIN]);
    omx_src_buf[JPEG_SRC_IMAGE_TYPE_MAIN] = &job_entry->src_bufs[JPEG_SRC_IMAGE_TYPE_MAIN];
    rc = mm_jpeg_omx_use_buf(my_obj,
                             src_buf[JPEG_SRC_IMAGE_TYPE_MAIN],
                             omx_src_buf[JPEG_SRC_IMAGE_TYPE_MAIN],
                             INPUT_PORT_MAIN);
    if (0 != rc) {
        CDBG_ERROR("%s: config main input omx buffer failed", __func__);
        return rc;
    }

    /* config input omx buffer for thumbnail input if there is thumbnail */
    if (has_thumbnail) {
        src_buf[JPEG_SRC_IMAGE_TYPE_THUMB] = &(job->encode_parm.buf_info.src_imgs.src_img[JPEG_SRC_IMAGE_TYPE_THUMB]);
        omx_src_buf[JPEG_SRC_IMAGE_TYPE_THUMB] = &job_entry->src_bufs[JPEG_SRC_IMAGE_TYPE_THUMB];
        rc = mm_jpeg_omx_use_buf(my_obj,
                                 src_buf[JPEG_SRC_IMAGE_TYPE_THUMB],
                                 omx_src_buf[JPEG_SRC_IMAGE_TYPE_THUMB],
                                 INPUT_PORT_THUMBNAIL);
        if (0 != rc) {
            CDBG_ERROR("%s: config thumbnail input omx buffer failed", __func__);
            return rc;
        }
    }

    /* config output omx buffer */
    job_entry->sink_buf.portIdx = OUTPUT_PORT;
    OMX_UseBuffer(my_obj->omx_handle,
                  &job_entry->sink_buf.buf_header,
                  job_entry->sink_buf.portIdx,
                  NULL,
                  job->encode_parm.buf_info.sink_img.buf_len,
                  (void *)job->encode_parm.buf_info.sink_img.buf_vaddr);
    CDBG("%s: Use output buf: port_idx=%d, len=%d, ptr=%p",
         __func__, job_entry->sink_buf.portIdx,
         job->encode_parm.buf_info.sink_img.buf_len,
         job->encode_parm.buf_info.sink_img.buf_vaddr);

    /* wait for OMX state ready */
    CDBG("%s: Wait for state change to idle \n", __func__);
    mm_jpeg_job_wait_for_cmd_complete(my_obj, OMX_CommandStateSet, OMX_StateIdle);
    CDBG("%s: State changed to OMX_StateIdle\n", __func__);

    /* start OMX encoding by sending executing cmd */
    CDBG("%s: Send command to executing\n", __func__);
    OMX_SendCommand(my_obj->omx_handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    mm_jpeg_job_wait_for_cmd_complete(my_obj, OMX_CommandStateSet, OMX_StateExecuting);

    /* start input feeding and output writing */
    CDBG("%s: start main input feeding\n", __func__);
    for (i = 0; i < job_entry->src_bufs[JPEG_SRC_IMAGE_TYPE_MAIN].num_bufs; i++) {
        OMX_EmptyThisBuffer(my_obj->omx_handle,
                            job_entry->src_bufs[JPEG_SRC_IMAGE_TYPE_MAIN].bufs[i].buf_header);
    }
    if (has_thumbnail) {
        CDBG("%s: start thumbnail input feeding\n", __func__);
        for (i = 0; i < job_entry->src_bufs[JPEG_SRC_IMAGE_TYPE_THUMB].num_bufs; i++) {
            OMX_EmptyThisBuffer(my_obj->omx_handle,
                                job_entry->src_bufs[JPEG_SRC_IMAGE_TYPE_THUMB].bufs[i].buf_header);
        }
    }
    CDBG("%s: start output writing\n", __func__);
    OMX_FillThisBuffer(my_obj->omx_handle, job_entry->sink_buf.buf_header);
    return rc;
}

static void *mm_jpeg_notify_thread(void *data)
{
    mm_jpeg_job_q_node_t* job_node = (mm_jpeg_job_q_node_t *)data;
    mm_jpeg_job_entry * job_entry = &job_node->entry;
    mm_jpeg_obj* my_obj = (mm_jpeg_obj *)job_entry->jpeg_obj;
    void* node = NULL;
    int32_t rc = 0;
    uint32_t jobId = job_entry->jobId;

    if (NULL == my_obj) {
        CDBG_ERROR("%s: jpeg obj is NULL", __func__);
        return NULL;
    }

    /* call cb */
    if (NULL != job_entry->job.encode_job.jpeg_cb) {
        /* Add to cb queue */
        rc = mm_jpeg_queue_enq(&my_obj->cb_q, data);
        if (0 != rc) {
            CDBG_ERROR("%s: enqueue into cb_q failed", __func__);
            free(job_node);
            return NULL;
        }

        CDBG("%s: send jpeg callback", __func__);
        /* has callback, send CB */
        job_entry->job.encode_job.jpeg_cb(job_entry->job_status,
                                          job_entry->thumbnail_dropped,
                                          job_entry->client_hdl,
                                          job_entry->jobId,
                                          job_entry->job.encode_job.encode_parm.buf_info.sink_img.buf_vaddr,
                                          job_entry->jpeg_size,
                                          job_entry->job.encode_job.userdata);

        /* Remove from cb queue */
        CDBG_ERROR("%s: remove job %d from cb queue", __func__, jobId);
        node = mm_jpeg_queue_remove_job_by_job_id(&my_obj->cb_q, jobId);
        if (NULL != node) {
            free(node);
        }
    } else {
        CDBG_ERROR("%s: no cb provided, no action", __func__);
        /* note here we are not freeing any internal memory */
        free(data);
    }

    return NULL;
}

/* process encoding job */
int32_t mm_jpeg_process_encoding_job(mm_jpeg_obj *my_obj, mm_jpeg_job_q_node_t* job_node)
{
    int32_t rc = 0;
    mm_jpeg_job_entry* job_entry = &job_node->entry;

    /* call OMX_Encode */
    rc = mm_jpeg_omx_encode(my_obj, job_entry);
    if (0 == rc) {
        /* sent encode cmd to OMX, queue job into ongoing queue */
        rc = mm_jpeg_queue_enq(&my_obj->ongoing_job_q, job_node);
    } else {
        /* OMX encode failed, notify error through callback */
        job_entry->job_status = JPEG_JOB_STATUS_ERROR;
        if (NULL != job_entry->job.encode_job.jpeg_cb) {
            /* has callback, create a thread to send CB */
            pthread_create(&job_entry->cb_pid,
                           NULL,
                           mm_jpeg_notify_thread,
                           (void *)job_node);

        } else {
            CDBG_ERROR("%s: no cb provided, return here", __func__);
            free(job_node);
        }
    }

    return rc;
}
/* process job (encoding/decoding) */
int32_t mm_jpeg_process_job(mm_jpeg_obj *my_obj, mm_jpeg_job_q_node_t* job_node)
{
    int32_t rc = 0;

    switch (job_node->entry.job.job_type) {
    case JPEG_JOB_TYPE_ENCODE:
        rc = mm_jpeg_process_encoding_job(my_obj, job_node);
        break;
    default:
        CDBG_ERROR("%s: job type not supported (%d)",
                   __func__, job_node->entry.job.job_type);
        free(job_node);
        rc = -1;
        break;
    }

    return rc;
}

static void *mm_jpeg_jobmgr_thread(void *data)
{
    int rc = 0;
    int running = 1;
    uint32_t num_ongoing_jobs = 0;
    mm_jpeg_obj *my_obj = (mm_jpeg_obj*)data;
    mm_jpeg_job_cmd_thread_t *cmd_thread = &my_obj->job_mgr;
    mm_jpeg_job_q_node_t* node = NULL;

    do {
        do {
            rc = sem_wait(&cmd_thread->job_sem);
            if (rc != 0 && errno != EINVAL) {
                CDBG_ERROR("%s: sem_wait error (%s)",
                           __func__, strerror(errno));
                return NULL;
            }
        } while (rc != 0);

        /* check ongoing q size */
        num_ongoing_jobs = mm_jpeg_queue_get_size(&my_obj->ongoing_job_q);
        if (num_ongoing_jobs >= NUM_MAX_JPEG_CNCURRENT_JOBS) {
            CDBG("%s: ongoing job already reach max %d", __func__, num_ongoing_jobs);
            continue;
        }

        pthread_mutex_lock(&my_obj->job_lock);
        /* can go ahead with new work */
        node = (mm_jpeg_job_q_node_t*)mm_jpeg_queue_deq(&cmd_thread->job_queue);
        if (node != NULL) {
            switch (node->type) {
            case MM_JPEG_CMD_TYPE_JOB:
                mm_jpeg_process_job(my_obj, node);
                break;
            case MM_JPEG_CMD_TYPE_EXIT:
            default:
                /* free node */
                free(node);
                /* set running flag to false */
                running = 0;
                break;
            }
        }
        pthread_mutex_unlock(&my_obj->job_lock);

    } while (running);
    return NULL;
}

int32_t mm_jpeg_jobmgr_thread_launch(mm_jpeg_obj * my_obj)
{
    int32_t rc = 0;
    mm_jpeg_job_cmd_thread_t * job_mgr = &my_obj->job_mgr;

    sem_init(&job_mgr->job_sem, 0, 0);
    mm_jpeg_queue_init(&job_mgr->job_queue);

    /* launch the thread */
    pthread_create(&job_mgr->pid,
                   NULL,
                   mm_jpeg_jobmgr_thread,
                   (void *)my_obj);
    return rc;
}

int32_t mm_jpeg_jobmgr_thread_release(mm_jpeg_obj * my_obj)
{
    int32_t rc = 0;
    mm_jpeg_job_cmd_thread_t * cmd_thread = &my_obj->job_mgr;
    mm_jpeg_job_q_node_t* node =
        (mm_jpeg_job_q_node_t *)malloc(sizeof(mm_jpeg_job_q_node_t));
    if (NULL == node) {
        CDBG_ERROR("%s: No memory for mm_jpeg_job_q_node_t", __func__);
        return -1;
    }

    memset(node, 0, sizeof(mm_jpeg_job_q_node_t));
    node->type = MM_JPEG_CMD_TYPE_EXIT;

    mm_jpeg_queue_enq(&cmd_thread->job_queue, node);
    sem_post(&cmd_thread->job_sem);

    /* wait until cmd thread exits */
    if (pthread_join(cmd_thread->pid, NULL) != 0) {
        CDBG("%s: pthread dead already\n", __func__);
    }
    mm_jpeg_queue_deinit(&cmd_thread->job_queue);

    sem_destroy(&cmd_thread->job_sem);
    memset(cmd_thread, 0, sizeof(mm_jpeg_job_cmd_thread_t));
    return rc;
}

int32_t mm_jpeg_init(mm_jpeg_obj *my_obj)
{
    int32_t rc = 0;

    /* init locks */
    pthread_mutex_init(&my_obj->job_lock, NULL);
    pthread_mutex_init(&my_obj->omx_evt_lock, NULL);
    pthread_cond_init(&my_obj->omx_evt_cond, NULL);

    /* init ongoing job queue */
    rc = mm_jpeg_queue_init(&my_obj->ongoing_job_q);
    rc = mm_jpeg_queue_init(&my_obj->cb_q);

    /* init job semaphore and launch jobmgr thread */
    CDBG("%s : Launch jobmgr thread",__func__);
    rc = mm_jpeg_jobmgr_thread_launch(my_obj);

    /* load OMX */
    rc = mm_jpeg_omx_load(my_obj);
    if (0 != rc) {
        /* roll back in error case */
        mm_jpeg_jobmgr_thread_release(my_obj);
        mm_jpeg_queue_deinit(&my_obj->ongoing_job_q);
        pthread_mutex_destroy(&my_obj->job_lock);
        pthread_mutex_destroy(&my_obj->omx_evt_lock);
        pthread_cond_destroy(&my_obj->omx_evt_cond);
    }

    return rc;
}

int32_t mm_jpeg_deinit(mm_jpeg_obj *my_obj)
{
    int32_t rc = 0;

    /* unload OMX engine */
    rc = mm_jpeg_omx_unload(my_obj);

    /* release jobmgr thread */
    rc = mm_jpeg_jobmgr_thread_release(my_obj);

    /* deinit ongoing job and cb queue */
    rc = mm_jpeg_queue_deinit(&my_obj->ongoing_job_q);
    rc = mm_jpeg_queue_deinit(&my_obj->cb_q);

    /* destroy locks */
    pthread_mutex_destroy(&my_obj->job_lock);
    pthread_mutex_destroy(&my_obj->omx_evt_lock);
    pthread_cond_destroy(&my_obj->omx_evt_cond);

    return rc;
}

uint32_t mm_jpeg_new_client(mm_jpeg_obj *my_obj)
{
    uint32_t client_hdl = 0;
    uint8_t idx;

    if (my_obj->num_clients >= MAX_JPEG_CLIENT_NUM) {
        CDBG_ERROR("%s: num of clients reached limit", __func__);
        return client_hdl;
    }

    for (idx = 0; idx < MAX_JPEG_CLIENT_NUM; idx++) {
        if (0 == my_obj->clnt_mgr[idx].is_used) {
            break;
        }
    }

    if (idx < MAX_JPEG_CLIENT_NUM) {
        /* client entry avail */
        /* generate client handler by index */
        client_hdl = mm_jpeg_util_generate_handler(idx);

        /* update client entry */
        my_obj->clnt_mgr[idx].is_used = 1;
        my_obj->clnt_mgr[idx].client_handle = client_hdl;

        /* increse client count */
        my_obj->num_clients++;
    }

    return client_hdl;
}

int32_t mm_jpeg_start_job(mm_jpeg_obj *my_obj,
                          uint32_t client_hdl,
                          mm_jpeg_job* job,
                          uint32_t* jobId)
{
    int32_t rc = -1;
    uint8_t clnt_idx = 0;
    uint32_t job_id = 0;
    mm_jpeg_job_q_node_t* node = NULL;

    *jobId = 0;

    /* check if valid client */
    clnt_idx = mm_jpeg_util_get_index_by_handler(client_hdl);
    if (clnt_idx >= MAX_JPEG_CLIENT_NUM ) {
        CDBG_ERROR("%s: invalid client with handler (%d)", __func__, client_hdl);
        return rc;
    }

    /* generate client handler by index */
    job_id = mm_jpeg_util_generate_handler(clnt_idx);

    /* enqueue new job into todo job queue */
    node = (mm_jpeg_job_q_node_t *)malloc(sizeof(mm_jpeg_job_q_node_t));
    if (NULL == node) {
        CDBG_ERROR("%s: No memory for mm_jpeg_job_q_node_t", __func__);
        return -rc;
    }

    memset(node, 0, sizeof(mm_jpeg_job_q_node_t));
    node->type = MM_JPEG_CMD_TYPE_JOB;
    node->entry.client_hdl = client_hdl;
    node->entry.jobId = job_id;
    memcpy(&node->entry.job, job, sizeof(mm_jpeg_job));
    node->entry.jpeg_obj = (void*)my_obj; /* save a ptr to jpeg_obj */

    rc = mm_jpeg_queue_enq(&my_obj->job_mgr.job_queue, node);
    if (0 == rc) {
        sem_post(&my_obj->job_mgr.job_sem);
        *jobId = job_id;
    }

    return rc;
}

int32_t mm_jpeg_abort_job(mm_jpeg_obj *my_obj,
                          uint32_t client_hdl,
                          uint32_t jobId)
{
    int32_t rc = -1;
    uint8_t clnt_idx = 0;
    void * node = NULL;
    mm_jpeg_job_entry* job_entry = NULL;

    /* check if valid client */
    clnt_idx = mm_jpeg_util_get_index_by_handler(client_hdl);
    if (clnt_idx >= MAX_JPEG_CLIENT_NUM ) {
        CDBG_ERROR("%s: invalid client with handler (%d)", __func__, client_hdl);
        return rc;
    }

    pthread_mutex_lock(&my_obj->job_lock);

    /* abort job if in ongoing queue */
    node = mm_jpeg_queue_remove_job_by_job_id(&my_obj->ongoing_job_q, jobId);
    if (NULL != node) {
        /* find job that is OMX ongoing, ask OMX to abort the job */
        job_entry = &(((mm_jpeg_job_q_node_t *)node)->entry);
        rc = mm_jpeg_omx_abort_job(my_obj, job_entry);
        free(node);
        goto abort_done;
    }

    /* abort job if in todo queue */
    node = mm_jpeg_queue_remove_job_by_job_id(&my_obj->job_mgr.job_queue, jobId);
    if (NULL != node) {
        /* simply delete it */
        free(node);
        goto abort_done;
    }
    /* abort job if in cb queue */
    node = mm_jpeg_queue_remove_job_by_job_id(&my_obj->cb_q, jobId);
    if (NULL != node) {
        /* join cb thread */
        job_entry = &(((mm_jpeg_job_q_node_t *)node)->entry);
        if (pthread_join(job_entry->cb_pid, NULL) != 0) {
            CDBG("%s: pthread dead already\n", __func__);
        }
        free(node);
    }
abort_done:
    pthread_mutex_unlock(&my_obj->job_lock);

    /* wake up jobMgr thread to work on new job if there is any */
    sem_post(&my_obj->job_mgr.job_sem);

    return rc;
}

int32_t mm_jpeg_close(mm_jpeg_obj *my_obj, uint32_t client_hdl)
{
    int32_t rc = -1;
    uint8_t clnt_idx = 0;
    void* node = NULL;
    mm_jpeg_job_entry* job_entry = NULL;

    /* check if valid client */
    clnt_idx = mm_jpeg_util_get_index_by_handler(client_hdl);
    if (clnt_idx >= MAX_JPEG_CLIENT_NUM ) {
        CDBG_ERROR("%s: invalid client with handler (%d)", __func__, client_hdl);
        return rc;
    }

    CDBG("%s: E", __func__);

    /* abort all jobs from the client */
    pthread_mutex_lock(&my_obj->job_lock);

    /* abort job if in ongoing queue */
    CDBG("%s: abort ongoing jobs", __func__);
    node = mm_jpeg_queue_remove_job_by_client_id(&my_obj->ongoing_job_q, client_hdl);
    while (NULL != node) {
        /* find job that is OMX ongoing, ask OMX to abort the job */
        job_entry = &(((mm_jpeg_job_q_node_t *)node)->entry);
        rc = mm_jpeg_omx_abort_job(my_obj, job_entry);
        free(node);

        /* find next job from ongoing queue that belongs to this client */
        node = mm_jpeg_queue_remove_job_by_client_id(&my_obj->ongoing_job_q, client_hdl);
    }

    /* abort job if in todo queue */
    CDBG("%s: abort todo jobs", __func__);
    node = mm_jpeg_queue_remove_job_by_client_id(&my_obj->job_mgr.job_queue, client_hdl);
    while (NULL != node) {
        /* simply delete the job if in todo queue */
        free(node);

        /* find next job from todo queue that belongs to this client */
        node = mm_jpeg_queue_remove_job_by_client_id(&my_obj->job_mgr.job_queue, client_hdl);
    }

    /* abort job if in cb queue */
    CDBG("%s: abort done jobs in cb threads", __func__);
    node = mm_jpeg_queue_remove_job_by_client_id(&my_obj->cb_q, client_hdl);
    while (NULL != node) {
        /* join cb thread */
        job_entry = &(((mm_jpeg_job_q_node_t *)node)->entry);
        if (pthread_join(job_entry->cb_pid, NULL) != 0) {
            CDBG("%s: pthread dead already\n", __func__);
        }
        free(node);

        /* find next job from cb queue that belongs to this client */
        node = mm_jpeg_queue_remove_job_by_client_id(&my_obj->cb_q, client_hdl);
    }

    pthread_mutex_unlock(&my_obj->job_lock);

    /* invalidate client entry */
    memset(&my_obj->clnt_mgr[clnt_idx], 0, sizeof(mm_jpeg_client_t));

    rc = 0;
    CDBG("%s: X", __func__);
    return rc;
}

void mm_jpeg_job_wait_for_event(mm_jpeg_obj *my_obj, uint32_t evt_mask)
{
    pthread_mutex_lock(&my_obj->omx_evt_lock);
    while (!(my_obj->omx_evt_rcvd.evt & evt_mask)) {
        pthread_cond_wait(&my_obj->omx_evt_cond, &my_obj->omx_evt_lock);
    }
    CDBG("%s:done", __func__);
    pthread_mutex_unlock(&my_obj->omx_evt_lock);
}

void mm_jpeg_job_wait_for_cmd_complete(mm_jpeg_obj *my_obj,
                                       int cmd,
                                       int status)
{
    pthread_mutex_lock(&my_obj->omx_evt_lock);
    while (!((my_obj->omx_evt_rcvd.evt & MM_JPEG_EVENT_MASK_CMD_COMPLETE) &&
             (my_obj->omx_evt_rcvd.omx_value1 == cmd) &&
             (my_obj->omx_evt_rcvd.omx_value2 == status))) {
        pthread_cond_wait(&my_obj->omx_evt_cond, &my_obj->omx_evt_lock);
    }
    CDBG("%s:done", __func__);
    pthread_mutex_unlock(&my_obj->omx_evt_lock);
}

OMX_ERRORTYPE mm_jpeg_etbdone(OMX_HANDLETYPE hComponent,
                              OMX_PTR pAppData,
                              OMX_BUFFERHEADERTYPE* pBuffer)
{
    /* no process needed for etbdone, return here */
    return 0;
}

OMX_ERRORTYPE mm_jpeg_ftbdone(OMX_HANDLETYPE hComponent,
                              OMX_PTR pAppData,
                              OMX_BUFFERHEADERTYPE* pBuffer)
{
    int rc = 0;
    void* node = NULL;
    mm_jpeg_job_entry* job_entry = NULL;
    mm_jpeg_obj * my_obj = (mm_jpeg_obj*)pAppData;

    if (NULL == my_obj) {
        CDBG_ERROR("%s: pAppData is NULL, return here", __func__);
        return rc;
    }

    CDBG("%s: jpeg done", __func__);

    /* signal JPEG_DONE event */
    pthread_mutex_lock(&my_obj->omx_evt_lock);
    my_obj->omx_evt_rcvd.evt = MM_JPEG_EVENT_MASK_JPEG_DONE;
    pthread_cond_signal(&my_obj->omx_evt_cond);
    pthread_mutex_unlock(&my_obj->omx_evt_lock);

    /* find job that is OMX ongoing */
    /* If OMX can support multi encoding, it should provide a way to pass jobID.
     * then we can find job by jobID from ongoing queue.
     * For now, since OMX only support one job per time, we simply dequeue it. */
    pthread_mutex_lock(&my_obj->job_lock);
    node = mm_jpeg_queue_deq(&my_obj->ongoing_job_q);
    if (NULL != node) {
        job_entry = &(((mm_jpeg_job_q_node_t *)node)->entry);

        /* update status in job entry */
        job_entry->jpeg_size = pBuffer->nFilledLen;
        job_entry->job_status = JPEG_JOB_STATUS_DONE;
        CDBG("%s:filled len = %u, status = %d",
             __func__, job_entry->jpeg_size, job_entry->job_status);

        /* clean omx job */
        mm_jpeg_clean_omx_job(my_obj, job_entry);

        if (NULL != job_entry->job.encode_job.jpeg_cb) {
            /* has callback, create a thread to send CB */
            pthread_create(&job_entry->cb_pid,
                           NULL,
                           mm_jpeg_notify_thread,
                           node);

        } else {
            CDBG_ERROR("%s: no cb provided, return here", __func__);
            free(node);
        }
    }
    pthread_mutex_unlock(&my_obj->job_lock);

    /* Wake up jobMgr thread to work on next job if there is any */
    sem_post(&my_obj->job_mgr.job_sem);

    return rc;
}

OMX_ERRORTYPE mm_jpeg_handle_omx_event(OMX_HANDLETYPE hComponent,
                                       OMX_PTR pAppData,
                                       OMX_EVENTTYPE eEvent,
                                       OMX_U32 nData1,
                                       OMX_U32 nData2,
                                       OMX_PTR pEventData)
{
    int rc = 0;
    void* node = NULL;
    mm_jpeg_job_entry* job_entry = NULL;
    mm_jpeg_obj * my_obj = (mm_jpeg_obj*)pAppData;
    uint32_t jobId = 0;

    if (NULL == my_obj) {
        CDBG_ERROR("%s: pAppData is NULL, return here", __func__);
        return rc;
    }

    /* signal event */
    switch (eEvent) {
    case  OMX_EVENT_JPEG_ABORT:
        {
            CDBG("%s: eEvent=OMX_EVENT_JPEG_ABORT", __func__);
            /* signal error evt */
            pthread_mutex_lock(&my_obj->omx_evt_lock);
            my_obj->omx_evt_rcvd.evt = MM_JPEG_EVENT_MASK_JPEG_ABORT;
            pthread_cond_signal(&my_obj->omx_evt_cond);
            pthread_mutex_unlock(&my_obj->omx_evt_lock);
        }
        break;
    case OMX_EventError:
        {
            CDBG("%s: eEvent=OMX_EventError", __func__);
            switch (nData1) {
            case OMX_EVENT_THUMBNAIL_DROPPED:
                {
                    uint8_t thumbnail_dropped_flag = 1;
                    mm_jpeg_job_q_node_t* data = (mm_jpeg_job_q_node_t*)mm_jpeg_queue_peek(&my_obj->ongoing_job_q);
                    jobId = data->entry.jobId;
                    mm_jpeg_queue_update_flag(&my_obj->ongoing_job_q,
                                              jobId,
                                              thumbnail_dropped_flag);
                }
                break;
            case OMX_EVENT_JPEG_ERROR:
                {
                    /* signal error evt */
                    pthread_mutex_lock(&my_obj->omx_evt_lock);
                    my_obj->omx_evt_rcvd.evt = MM_JPEG_EVENT_MASK_JPEG_ERROR;
                    pthread_cond_signal(&my_obj->omx_evt_cond);
                    pthread_mutex_unlock(&my_obj->omx_evt_lock);

                    /* send CB for error case */
                    /* If OMX can support multi encoding, it should provide a way to pass jobID.
                     * then we can find job by jobID from ongoing queue.
                     * For now, since OMX only support one job per time, we simply dequeue it. */
                    pthread_mutex_lock(&my_obj->job_lock);
                    node = mm_jpeg_queue_deq(&my_obj->ongoing_job_q);
                    if (NULL != node) {
                        job_entry = &(((mm_jpeg_job_q_node_t *)node)->entry);;

                        /* clean omx job */
                        mm_jpeg_clean_omx_job(my_obj, job_entry);

                        /* find job that is OMX ongoing */
                        job_entry->job_status = JPEG_JOB_STATUS_ERROR;
                        if (NULL != job_entry->job.encode_job.jpeg_cb) {
                            /* has callback, create a thread to send CB */
                            pthread_create(&job_entry->cb_pid,
                                           NULL,
                                           mm_jpeg_notify_thread,
                                           node);

                        } else {
                            CDBG_ERROR("%s: no cb provided, return here", __func__);
                            free(node);
                        }
                    }
                    pthread_mutex_unlock(&my_obj->job_lock);

                    /* Wake up jobMgr thread to work on next job if there is any */
                    sem_post(&my_obj->job_mgr.job_sem);
                }
                break;
            default:
                break;
            }
        }
        break;
    case OMX_EventCmdComplete:
        {
            CDBG("%s: eEvent=OMX_EventCmdComplete, value1=%d, value2=%d",
                 __func__, nData1, nData2);
            /* signal cmd complete evt */
            pthread_mutex_lock(&my_obj->omx_evt_lock);
            my_obj->omx_evt_rcvd.evt = MM_JPEG_EVENT_MASK_CMD_COMPLETE;
            my_obj->omx_evt_rcvd.omx_value1 = nData1;
            my_obj->omx_evt_rcvd.omx_value2 = nData2;
            pthread_cond_signal(&my_obj->omx_evt_cond);
            pthread_mutex_unlock(&my_obj->omx_evt_lock);
        }
        break;
    default:
        break;
    }
    return rc;
}

/* remove the first job from the queue with matching client handle */
mm_jpeg_job_q_node_t* mm_jpeg_queue_remove_job_by_client_id(mm_jpeg_queue_t* queue, uint32_t client_hdl)
{
    mm_jpeg_q_node_t* node = NULL;
    mm_jpeg_job_q_node_t* data = NULL;
    mm_jpeg_job_q_node_t* job_node = NULL;
    struct cam_list *head = NULL;
    struct cam_list *pos = NULL;

    pthread_mutex_lock(&queue->lock);
    head = &queue->head.list;
    pos = head->next;
    while(pos != head) {
        node = member_of(pos, mm_jpeg_q_node_t, list);
        data = (mm_jpeg_job_q_node_t *)node->data;

        if (data && (data->entry.client_hdl == client_hdl)) {
            CDBG_ERROR("%s: found matching client node", __func__);
            job_node = data;
            cam_list_del_node(&node->list);
            queue->size--;
            free(node);
            CDBG_ERROR("%s: queue size = %d", __func__, queue->size);
            break;
        }
        pos = pos->next;
    }

    pthread_mutex_unlock(&queue->lock);

    return job_node;
}

/* remove job from the queue with matching job id */
mm_jpeg_job_q_node_t* mm_jpeg_queue_remove_job_by_job_id(mm_jpeg_queue_t* queue, uint32_t job_id)
{
    mm_jpeg_q_node_t* node = NULL;
    mm_jpeg_job_q_node_t* data = NULL;
    mm_jpeg_job_q_node_t* job_node = NULL;
    struct cam_list *head = NULL;
    struct cam_list *pos = NULL;

    pthread_mutex_lock(&queue->lock);
    head = &queue->head.list;
    pos = head->next;
    while(pos != head) {
        node = member_of(pos, mm_jpeg_q_node_t, list);
        data = (mm_jpeg_job_q_node_t *)node->data;

        if (data && (data->entry.jobId == job_id)) {
            job_node = data;
            cam_list_del_node(&node->list);
            queue->size--;
            free(node);
            break;
        }
        pos = pos->next;
    }

    pthread_mutex_unlock(&queue->lock);

    return job_node;
}

/* update thumbnail dropped flag in job queue */
int32_t mm_jpeg_queue_update_flag(mm_jpeg_queue_t* queue, uint32_t job_id, uint8_t flag)
{
    int32_t rc = 0;
    mm_jpeg_q_node_t* node = NULL;
    mm_jpeg_job_q_node_t* data = NULL;
    mm_jpeg_job_q_node_t* job_node = NULL;
    struct cam_list *head = NULL;
    struct cam_list *pos = NULL;

    pthread_mutex_lock(&queue->lock);
    head = &queue->head.list;
    pos = head->next;
    while(pos != head) {
        node = member_of(pos, mm_jpeg_q_node_t, list);
        data = (mm_jpeg_job_q_node_t *)node->data;
        if (data && (data->entry.jobId == job_id)) {
            job_node = data;
            break;
        }
        pos = pos->next;
    }

    if (job_node) {
        /* find matching job for its job id */
        job_node->entry.thumbnail_dropped = flag;
    } else {
        CDBG_ERROR("%s: No entry for jobId = %d", __func__, job_id);
        rc = -1;
    }
    pthread_mutex_unlock(&queue->lock);
    return rc;
}
