/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
 *
 */

#include <pthread.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <poll.h>

#include "mm_jpeg_dbg.h"
#include "mm_jpeg_interface.h"
#include "mm_jpeg.h"
#include "mm_jpeg_inlines.h"

#ifdef LOAD_ADSP_RPC_LIB
#include <dlfcn.h>
#include <stdlib.h>
#endif

#define ENCODING_MODE_PARALLEL 1

#define META_KEYFILE "/data/metadata.key"

#define MM_JPG_USE_TURBO_CLOCK (0)

OMX_ERRORTYPE mm_jpeg_ebd(OMX_HANDLETYPE hComponent,
    OMX_PTR pAppData,
    OMX_BUFFERHEADERTYPE* pBuffer);
OMX_ERRORTYPE mm_jpeg_fbd(OMX_HANDLETYPE hComponent,
    OMX_PTR pAppData,
    OMX_BUFFERHEADERTYPE* pBuffer);
OMX_ERRORTYPE mm_jpeg_event_handler(OMX_HANDLETYPE hComponent,
    OMX_PTR pAppData,
    OMX_EVENTTYPE eEvent,
    OMX_U32 nData1,
    OMX_U32 nData2,
    OMX_PTR pEventData);

static int32_t mm_jpegenc_destroy_job(mm_jpeg_job_session_t *p_session);
static void mm_jpegenc_job_done(mm_jpeg_job_session_t *p_session);
mm_jpeg_job_q_node_t* mm_jpeg_queue_remove_job_by_dst_ptr(
  mm_jpeg_queue_t* queue, void * dst_ptr);
static OMX_ERRORTYPE mm_jpeg_session_configure(mm_jpeg_job_session_t *p_session);

/** mm_jpeg_session_send_buffers:
 *
 *  Arguments:
 *    @data: job session
 *
 *  Return:
 *       OMX error values
 *
 *  Description:
 *       Send the buffers to OMX layer
 *
 **/
OMX_ERRORTYPE mm_jpeg_session_send_buffers(void *data)
{
  uint32_t i = 0;
  mm_jpeg_job_session_t* p_session = (mm_jpeg_job_session_t *)data;
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  QOMX_BUFFER_INFO lbuffer_info;
  mm_jpeg_encode_params_t *p_params = &p_session->params;

  memset(&lbuffer_info, 0x0, sizeof(QOMX_BUFFER_INFO));
  for (i = 0; i < p_params->num_src_bufs; i++) {
    CDBG("%s:%d] Source buffer %d", __func__, __LINE__, i);
    lbuffer_info.fd = (OMX_U32)p_params->src_main_buf[i].fd;
    ret = OMX_UseBuffer(p_session->omx_handle, &(p_session->p_in_omx_buf[i]), 0,
      &lbuffer_info, p_params->src_main_buf[i].buf_size,
      p_params->src_main_buf[i].buf_vaddr);
    if (ret) {
      CDBG_ERROR("%s:%d] Error %d", __func__, __LINE__, ret);
      return ret;
    }
  }

  for (i = 0; i < p_params->num_tmb_bufs; i++) {
    CDBG("%s:%d] Source buffer %d", __func__, __LINE__, i);
    lbuffer_info.fd = (OMX_U32)p_params->src_thumb_buf[i].fd;
    ret = OMX_UseBuffer(p_session->omx_handle,
        &(p_session->p_in_omx_thumb_buf[i]), 2,
        &lbuffer_info, p_params->src_thumb_buf[i].buf_size,
        p_params->src_thumb_buf[i].buf_vaddr);
    if (ret) {
      CDBG_ERROR("%s:%d] Error %d", __func__, __LINE__, ret);
      return ret;
    }
  }

  for (i = 0; i < p_params->num_dst_bufs; i++) {
    CDBG("%s:%d] Dest buffer %d", __func__, __LINE__, i);
    ret = OMX_UseBuffer(p_session->omx_handle, &(p_session->p_out_omx_buf[i]),
      1, NULL, p_params->dest_buf[i].buf_size,
      p_params->dest_buf[i].buf_vaddr);
    if (ret) {
      CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
      return ret;
    }
  }
  CDBG("%s:%d]", __func__, __LINE__);
  return ret;
}


/** mm_jpeg_session_free_buffers:
 *
 *  Arguments:
 *    @data: job session
 *
 *  Return:
 *       OMX error values
 *
 *  Description:
 *       Free the buffers from OMX layer
 *
 **/
OMX_ERRORTYPE mm_jpeg_session_free_buffers(void *data)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  uint32_t i = 0;
  mm_jpeg_job_session_t* p_session = (mm_jpeg_job_session_t *)data;
  mm_jpeg_encode_params_t *p_params = &p_session->params;

  for (i = 0; i < p_params->num_src_bufs; i++) {
    CDBG("%s:%d] Source buffer %d", __func__, __LINE__, i);
    ret = OMX_FreeBuffer(p_session->omx_handle, 0, p_session->p_in_omx_buf[i]);
    if (ret) {
      CDBG_ERROR("%s:%d] Error %d", __func__, __LINE__, ret);
      return ret;
    }
  }

  for (i = 0; i < p_params->num_tmb_bufs; i++) {
    CDBG("%s:%d] Source buffer %d", __func__, __LINE__, i);
    ret = OMX_FreeBuffer(p_session->omx_handle, 2, p_session->p_in_omx_thumb_buf[i]);
    if (ret) {
      CDBG_ERROR("%s:%d] Error %d", __func__, __LINE__, ret);
      return ret;
    }
  }

  for (i = 0; i < p_params->num_dst_bufs; i++) {
    CDBG("%s:%d] Dest buffer %d", __func__, __LINE__, i);
    ret = OMX_FreeBuffer(p_session->omx_handle, 1, p_session->p_out_omx_buf[i]);
    if (ret) {
      CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
      return ret;
    }
  }
  CDBG("%s:%d]", __func__, __LINE__);
  return ret;
}




/** mm_jpeg_session_change_state:
 *
 *  Arguments:
 *    @p_session: job session
 *    @new_state: new state to be transitioned to
 *    @p_exec: transition function
 *
 *  Return:
 *       OMX error values
 *
 *  Description:
 *       This method is used for state transition
 *
 **/
OMX_ERRORTYPE mm_jpeg_session_change_state(mm_jpeg_job_session_t* p_session,
  OMX_STATETYPE new_state,
  mm_jpeg_transition_func_t p_exec)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  OMX_STATETYPE current_state;
  CDBG("%s:%d] new_state %d p_exec %p", __func__, __LINE__,
    new_state, p_exec);


  pthread_mutex_lock(&p_session->lock);

  ret = OMX_GetState(p_session->omx_handle, &current_state);

  if (ret) {
    pthread_mutex_unlock(&p_session->lock);
    return ret;
  }

  if (current_state == new_state) {
    pthread_mutex_unlock(&p_session->lock);
    return OMX_ErrorNone;
  }

  p_session->state_change_pending = OMX_TRUE;
  pthread_mutex_unlock(&p_session->lock);
  ret = OMX_SendCommand(p_session->omx_handle, OMX_CommandStateSet,
    new_state, NULL);
  pthread_mutex_lock(&p_session->lock);
  if (ret) {
    CDBG_ERROR("%s:%d] Error %d", __func__, __LINE__, ret);
    pthread_mutex_unlock(&p_session->lock);
    return OMX_ErrorIncorrectStateTransition;
  }
  CDBG("%s:%d] ", __func__, __LINE__);
  if ((OMX_ErrorNone != p_session->error_flag) &&
      (OMX_ErrorOverflow != p_session->error_flag)) {
    CDBG_ERROR("%s:%d] Error %d", __func__, __LINE__, p_session->error_flag);
    pthread_mutex_unlock(&p_session->lock);
    return p_session->error_flag;
  }
  if (p_exec) {
    ret = p_exec(p_session);
    if (ret) {
      CDBG_ERROR("%s:%d] Error %d", __func__, __LINE__, ret);
      pthread_mutex_unlock(&p_session->lock);
      return ret;
    }
  }
  CDBG("%s:%d] ", __func__, __LINE__);
  if (p_session->state_change_pending) {
    CDBG("%s:%d] before wait", __func__, __LINE__);
    pthread_cond_wait(&p_session->cond, &p_session->lock);
    CDBG("%s:%d] after wait", __func__, __LINE__);
  }
  pthread_mutex_unlock(&p_session->lock);
  CDBG("%s:%d] ", __func__, __LINE__);
  return ret;
}

/** mm_jpeg_session_create:
 *
 *  Arguments:
 *    @p_session: job session
 *
 *  Return:
 *       OMX error types
 *
 *  Description:
 *       Create a jpeg encode session
 *
 **/
OMX_ERRORTYPE mm_jpeg_session_create(mm_jpeg_job_session_t* p_session)
{
  OMX_ERRORTYPE rc = OMX_ErrorNone;
  mm_jpeg_obj *my_obj = (mm_jpeg_obj *) p_session->jpeg_obj;

  pthread_mutex_init(&p_session->lock, NULL);
  pthread_cond_init(&p_session->cond, NULL);
  cirq_reset(&p_session->cb_q);
  p_session->state_change_pending = OMX_FALSE;
  p_session->abort_state = MM_JPEG_ABORT_NONE;
  p_session->error_flag = OMX_ErrorNone;
  p_session->ebd_count = 0;
  p_session->fbd_count = 0;
  p_session->encode_pid = -1;
  p_session->config = OMX_FALSE;
  p_session->exif_count_local = 0;
  p_session->auto_out_buf = OMX_FALSE;

  p_session->omx_callbacks.EmptyBufferDone = mm_jpeg_ebd;
  p_session->omx_callbacks.FillBufferDone = mm_jpeg_fbd;
  p_session->omx_callbacks.EventHandler = mm_jpeg_event_handler;


  rc = OMX_GetHandle(&p_session->omx_handle,
      "OMX.qcom.image.jpeg.encoder",
      (void *)p_session,
      &p_session->omx_callbacks);
  if (OMX_ErrorNone != rc) {
    CDBG_ERROR("%s:%d] OMX_GetHandle failed (%d)", __func__, __LINE__, rc);
    return rc;
  }

  my_obj->num_sessions++;

  return rc;
}



/** mm_jpeg_session_destroy:
 *
 *  Arguments:
 *    @p_session: job session
 *
 *  Return:
 *       none
 *
 *  Description:
 *       Destroy a jpeg encode session
 *
 **/
void mm_jpeg_session_destroy(mm_jpeg_job_session_t* p_session)
{
  OMX_ERRORTYPE rc = OMX_ErrorNone;
  OMX_STATETYPE state;
  mm_jpeg_obj *my_obj = (mm_jpeg_obj *) p_session->jpeg_obj;

  CDBG("%s:%d] E", __func__, __LINE__);
  if (NULL == p_session->omx_handle) {
    CDBG_ERROR("%s:%d] invalid handle", __func__, __LINE__);
    return;
  }

  rc = OMX_GetState(p_session->omx_handle, &state);

  //Check state before state transition
  if ((state == OMX_StateExecuting) || (state == OMX_StatePause)) {
    rc = mm_jpeg_session_change_state(p_session, OMX_StateIdle, NULL);
    if (rc) {
      CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
    }
  }

  rc = OMX_GetState(p_session->omx_handle, &state);

  if (state == OMX_StateIdle) {
    rc = mm_jpeg_session_change_state(p_session, OMX_StateLoaded,
      mm_jpeg_session_free_buffers);
    if (rc) {
      CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
    }
  }

  rc = OMX_FreeHandle(p_session->omx_handle);
  if (0 != rc) {
    CDBG_ERROR("%s:%d] OMX_FreeHandle failed (%d)", __func__, __LINE__, rc);
  }
  p_session->omx_handle = NULL;

  pthread_mutex_destroy(&p_session->lock);
  pthread_cond_destroy(&p_session->cond);

  if (NULL != p_session->meta_enc_key) {
    free(p_session->meta_enc_key);
    p_session->meta_enc_key = NULL;
  }

  my_obj->num_sessions--;

  // Destroy next session
  if (p_session->next_session) {
    mm_jpeg_session_destroy(p_session->next_session);
  }

  CDBG("%s:%d] X", __func__, __LINE__);
}



/** mm_jpeg_session_config_main_buffer_offset:
 *
 *  Arguments:
 *    @p_session: job session
 *
 *  Return:
 *       OMX error values
 *
 *  Description:
 *       Configure the buffer offsets
 *
 **/
OMX_ERRORTYPE mm_jpeg_session_config_main_buffer_offset(
  mm_jpeg_job_session_t* p_session)
{
  OMX_ERRORTYPE rc = 0;
  OMX_INDEXTYPE buffer_index;
  QOMX_YUV_FRAME_INFO frame_info;
  size_t totalSize = 0;
  mm_jpeg_encode_params_t *p_params = &p_session->params;

  mm_jpeg_buf_t *p_src_buf =
    &p_params->src_main_buf[0];

  memset(&frame_info, 0x0, sizeof(QOMX_YUV_FRAME_INFO));

  frame_info.cbcrStartOffset[0] = p_src_buf->offset.mp[0].len;
  frame_info.cbcrStartOffset[1] = p_src_buf->offset.mp[1].len;
  frame_info.yOffset = p_src_buf->offset.mp[0].offset;
  frame_info.cbcrOffset[0] = p_src_buf->offset.mp[1].offset;
  frame_info.cbcrOffset[1] = p_src_buf->offset.mp[2].offset;
  totalSize = p_src_buf->buf_size;

  rc = OMX_GetExtensionIndex(p_session->omx_handle,
    QOMX_IMAGE_EXT_BUFFER_OFFSET_NAME, &buffer_index);
  if (rc != OMX_ErrorNone) {
    CDBG_ERROR("%s:%d] Failed", __func__, __LINE__);
    return rc;
  }

  CDBG_HIGH("%s:%d] yOffset = %d, cbcrOffset = (%d %d), totalSize = %zu,"
    "cbcrStartOffset = (%d %d)", __func__, __LINE__,
    (int)frame_info.yOffset,
    (int)frame_info.cbcrOffset[0],
    (int)frame_info.cbcrOffset[1],
    totalSize,
    (int)frame_info.cbcrStartOffset[0],
    (int)frame_info.cbcrStartOffset[1]);

  rc = OMX_SetParameter(p_session->omx_handle, buffer_index, &frame_info);
  if (rc != OMX_ErrorNone) {
    CDBG_ERROR("%s:%d] Failed", __func__, __LINE__);
    return rc;
  }
  return rc;
}

/** mm_jpeg_encoding_mode:
 *
 *  Arguments:
 *    @p_session: job session
 *
 *  Return:
 *       OMX error values
 *
 *  Description:
 *       Configure the serial or parallel encoding
 *       mode
 *
 **/
OMX_ERRORTYPE mm_jpeg_encoding_mode(
  mm_jpeg_job_session_t* p_session)
{
  OMX_ERRORTYPE rc = 0;
  OMX_INDEXTYPE indextype;
  QOMX_ENCODING_MODE encoding_mode;

  rc = OMX_GetExtensionIndex(p_session->omx_handle,
    QOMX_IMAGE_EXT_ENCODING_MODE_NAME, &indextype);
  if (rc != OMX_ErrorNone) {
    CDBG_ERROR("%s:%d] Failed", __func__, __LINE__);
    return rc;
  }

  if (ENCODING_MODE_PARALLEL) {
    encoding_mode = OMX_Parallel_Encoding;
  } else {
    encoding_mode = OMX_Serial_Encoding;
  }
  CDBG_HIGH("%s:%d] encoding mode = %d ", __func__, __LINE__,
    (int)encoding_mode);
  rc = OMX_SetParameter(p_session->omx_handle, indextype, &encoding_mode);
  if (rc != OMX_ErrorNone) {
    CDBG_ERROR("%s:%d] Failed", __func__, __LINE__);
    return rc;
  }
  return rc;
}

/** mm_jpeg_speed_mode:
 *
 *  Arguments:
 *    @p_session: job session
 *
 *  Return:
 *       OMX error values
 *
 *  Description:
 *      Configure normal or high speed jpeg
 *
 **/
OMX_ERRORTYPE mm_jpeg_speed_mode(
  mm_jpeg_job_session_t* p_session)
{
  OMX_ERRORTYPE rc = 0;
  OMX_INDEXTYPE indextype;
  QOMX_JPEG_SPEED jpeg_speed;

  rc = OMX_GetExtensionIndex(p_session->omx_handle,
    QOMX_IMAGE_EXT_JPEG_SPEED_NAME, &indextype);
  if (rc != OMX_ErrorNone) {
    CDBG_ERROR("%s:%d] Failed", __func__, __LINE__);
    return rc;
  }

  if (MM_JPG_USE_TURBO_CLOCK) {
    jpeg_speed.speedMode = QOMX_JPEG_SPEED_MODE_HIGH;
  } else {
    jpeg_speed.speedMode = QOMX_JPEG_SPEED_MODE_NORMAL;
  }

  rc = OMX_SetParameter(p_session->omx_handle, indextype, &jpeg_speed);
  if (rc != OMX_ErrorNone) {
    CDBG_ERROR("%s:%d] Failed", __func__, __LINE__);
    return rc;
  }
  return rc;
}


/** mm_jpeg_mem_ops:
 *
 *  Arguments:
 *    @p_session: job session
 *
 *  Return:
 *       OMX error values
 *
 *  Description:
 *       Configure the serial or parallel encoding
 *       mode
 *
 **/
OMX_ERRORTYPE mm_jpeg_mem_ops(
  mm_jpeg_job_session_t* p_session)
{
  OMX_ERRORTYPE rc = 0;
  OMX_INDEXTYPE indextype;
  QOMX_MEM_OPS mem_ops;
  mm_jpeg_encode_params_t *p_params = &p_session->params;

  mem_ops.get_memory = p_params->get_memory;

  rc = OMX_GetExtensionIndex(p_session->omx_handle,
    QOMX_IMAGE_EXT_MEM_OPS_NAME, &indextype);
  if (rc != OMX_ErrorNone) {
    CDBG_ERROR("%s:%d] Failed", __func__, __LINE__);
    return rc;
  }

  rc = OMX_SetParameter(p_session->omx_handle, indextype, &mem_ops);
  if (rc != OMX_ErrorNone) {
    CDBG_ERROR("%s:%d] Failed", __func__, __LINE__);
    return rc;
  }
  return rc;
}

/** mm_jpeg_metadata:
 *
 *  Arguments:
 *    @p_session: job session
 *
 *  Return:
 *       OMX error values
 *
 *  Description:
 *       Pass meta data
 *
 **/
OMX_ERRORTYPE mm_jpeg_metadata(
  mm_jpeg_job_session_t* p_session)
{
  OMX_ERRORTYPE rc = OMX_ErrorNone;
  OMX_INDEXTYPE indexType;
  mm_jpeg_encode_job_t *p_jobparams = &p_session->encode_job;
  QOMX_METADATA lMeta;

  rc = OMX_GetExtensionIndex(p_session->omx_handle,
      QOMX_IMAGE_EXT_METADATA_NAME, &indexType);

  if (rc != OMX_ErrorNone) {
    CDBG_ERROR("%s:%d] Failed", __func__, __LINE__);
    return rc;
  }

  lMeta.metadata = (OMX_U8 *)p_jobparams->p_metadata;
  lMeta.metaPayloadSize = sizeof(*p_jobparams->p_metadata);
  lMeta.mobicat_mask = p_jobparams->mobicat_mask;

  rc = OMX_SetConfig(p_session->omx_handle, indexType, &lMeta);
  if (rc != OMX_ErrorNone) {
    CDBG_ERROR("%s:%d] Failed", __func__, __LINE__);
    return rc;
  }
  return OMX_ErrorNone;
}

/** mm_jpeg_meta_enc_key:
 *
 *  Arguments:
 *    @p_session: job session
 *
 *  Return:
 *       OMX error values
 *
 *  Description:
 *       Pass metadata encrypt key
 *
 **/
OMX_ERRORTYPE mm_jpeg_meta_enc_key(
  mm_jpeg_job_session_t* p_session)
{
  OMX_ERRORTYPE rc = OMX_ErrorNone;
  OMX_INDEXTYPE indexType;
  QOMX_META_ENC_KEY lKey;

  lKey.metaKey = p_session->meta_enc_key;
  lKey.keyLen = p_session->meta_enc_keylen;

  if ((!lKey.metaKey) || (!lKey.keyLen)){
    CDBG_ERROR("%s:%d] Key is invalid", __func__, __LINE__);
    return OMX_ErrorNone;
  }

  rc = OMX_GetExtensionIndex(p_session->omx_handle,
      QOMX_IMAGE_EXT_META_ENC_KEY_NAME, &indexType);

  if (rc != OMX_ErrorNone) {
    CDBG_ERROR("%s:%d] Failed", __func__, __LINE__);
    return rc;
  }

  rc = OMX_SetConfig(p_session->omx_handle, indexType, &lKey);
  if (rc != OMX_ErrorNone) {
    CDBG_ERROR("%s:%d] Failed", __func__, __LINE__);
    return rc;
  }
  return OMX_ErrorNone;
}

/** map_jpeg_format:
 *
 *  Arguments:
 *    @color_fmt: color format
 *
 *  Return:
 *       OMX color format
 *
 *  Description:
 *       Map mmjpeg color format to OMX color format
 *
 **/
int map_jpeg_format(mm_jpeg_color_format color_fmt)
{
  switch (color_fmt) {
  case MM_JPEG_COLOR_FORMAT_YCRCBLP_H2V2:
    return (int)OMX_QCOM_IMG_COLOR_FormatYVU420SemiPlanar;
  case MM_JPEG_COLOR_FORMAT_YCBCRLP_H2V2:
    return (int)OMX_COLOR_FormatYUV420SemiPlanar;
  case MM_JPEG_COLOR_FORMAT_YCRCBLP_H2V1:
    return (int)OMX_QCOM_IMG_COLOR_FormatYVU422SemiPlanar;
  case MM_JPEG_COLOR_FORMAT_YCBCRLP_H2V1:
    return (int)OMX_COLOR_FormatYUV422SemiPlanar;
  case MM_JPEG_COLOR_FORMAT_YCRCBLP_H1V2:
    return (int)OMX_QCOM_IMG_COLOR_FormatYVU422SemiPlanar_h1v2;
  case MM_JPEG_COLOR_FORMAT_YCBCRLP_H1V2:
    return (int)OMX_QCOM_IMG_COLOR_FormatYUV422SemiPlanar_h1v2;
  case MM_JPEG_COLOR_FORMAT_YCRCBLP_H1V1:
    return (int)OMX_QCOM_IMG_COLOR_FormatYVU444SemiPlanar;
  case MM_JPEG_COLOR_FORMAT_YCBCRLP_H1V1:
    return (int)OMX_QCOM_IMG_COLOR_FormatYUV444SemiPlanar;
  case MM_JPEG_COLOR_FORMAT_MONOCHROME:
     return (int)OMX_COLOR_FormatMonochrome;
  default:
    CDBG_ERROR("%s:%d] invalid format %d", __func__, __LINE__, color_fmt);
    return (int)OMX_QCOM_IMG_COLOR_FormatYVU420SemiPlanar;
  }
}

/** mm_jpeg_session_config_port:
 *
 *  Arguments:
 *    @p_session: job session
 *
 *  Return:
 *       OMX error values
 *
 *  Description:
 *       Configure OMX ports
 *
 **/
OMX_ERRORTYPE mm_jpeg_session_config_ports(mm_jpeg_job_session_t* p_session)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  mm_jpeg_encode_params_t *p_params = &p_session->params;
  OMX_CONFIG_ROTATIONTYPE rotate;

  mm_jpeg_buf_t *p_src_buf =
    &p_params->src_main_buf[0];

  p_session->inputPort.nPortIndex = 0;
  p_session->outputPort.nPortIndex = 1;
  p_session->inputTmbPort.nPortIndex = 2;

  ret = OMX_GetParameter(p_session->omx_handle, OMX_IndexParamPortDefinition,
    &p_session->inputPort);
  if (ret) {
    CDBG_ERROR("%s:%d] failed", __func__, __LINE__);
    return ret;
  }

  ret = OMX_GetParameter(p_session->omx_handle, OMX_IndexParamPortDefinition,
    &p_session->inputTmbPort);
  if (ret) {
    CDBG_ERROR("%s:%d] failed", __func__, __LINE__);
    return ret;
  }

  ret = OMX_GetParameter(p_session->omx_handle, OMX_IndexParamPortDefinition,
    &p_session->outputPort);
  if (ret) {
    CDBG_ERROR("%s:%d] failed", __func__, __LINE__);
    return ret;
  }

  p_session->inputPort.format.image.nFrameWidth =
    (OMX_U32)p_params->main_dim.src_dim.width;
  p_session->inputPort.format.image.nFrameHeight =
    (OMX_U32)p_params->main_dim.src_dim.height;
  p_session->inputPort.format.image.nStride =
    p_src_buf->offset.mp[0].stride;
  p_session->inputPort.format.image.nSliceHeight =
    (OMX_U32)p_src_buf->offset.mp[0].scanline;
  p_session->inputPort.format.image.eColorFormat =
    map_jpeg_format(p_params->color_format);
  p_session->inputPort.nBufferSize =
    p_params->src_main_buf[0/*p_jobparams->src_index*/].buf_size;
  p_session->inputPort.nBufferCountActual = (OMX_U32)p_params->num_src_bufs;
  ret = OMX_SetParameter(p_session->omx_handle, OMX_IndexParamPortDefinition,
    &p_session->inputPort);
  if (ret) {
    CDBG_ERROR("%s:%d] failed", __func__, __LINE__);
    return ret;
  }

  if (p_session->params.encode_thumbnail) {
    mm_jpeg_buf_t *p_tmb_buf =
      &p_params->src_thumb_buf[0];
    p_session->inputTmbPort.format.image.nFrameWidth =
      (OMX_U32)p_params->thumb_dim.src_dim.width;
    p_session->inputTmbPort.format.image.nFrameHeight =
      (OMX_U32)p_params->thumb_dim.src_dim.height;
    p_session->inputTmbPort.format.image.nStride =
      p_tmb_buf->offset.mp[0].stride;
    p_session->inputTmbPort.format.image.nSliceHeight =
      (OMX_U32)p_tmb_buf->offset.mp[0].scanline;
    p_session->inputTmbPort.format.image.eColorFormat =
      map_jpeg_format(p_params->thumb_color_format);
    p_session->inputTmbPort.nBufferSize =
      p_params->src_thumb_buf[0].buf_size;
    p_session->inputTmbPort.nBufferCountActual = (OMX_U32)p_params->num_tmb_bufs;
    ret = OMX_SetParameter(p_session->omx_handle, OMX_IndexParamPortDefinition,
      &p_session->inputTmbPort);

    if (ret) {
      CDBG_ERROR("%s:%d] failed", __func__, __LINE__);
      return ret;
    }

    // Enable thumbnail port
    ret = OMX_SendCommand(p_session->omx_handle, OMX_CommandPortEnable,
        p_session->inputTmbPort.nPortIndex, NULL);

    if (ret) {
      CDBG_ERROR("%s:%d] failed", __func__, __LINE__);
      return ret;
    }
  } else {
    // Disable thumbnail port
    ret = OMX_SendCommand(p_session->omx_handle, OMX_CommandPortDisable,
        p_session->inputTmbPort.nPortIndex, NULL);

    if (ret) {
      CDBG_ERROR("%s:%d] failed", __func__, __LINE__);
      return ret;
    }
  }

  p_session->outputPort.nBufferSize =
    p_params->dest_buf[0].buf_size;
  p_session->outputPort.nBufferCountActual = (OMX_U32)p_params->num_dst_bufs;
  ret = OMX_SetParameter(p_session->omx_handle, OMX_IndexParamPortDefinition,
    &p_session->outputPort);
  if (ret) {
    CDBG_ERROR("%s:%d] failed", __func__, __LINE__);
    return ret;
  }

  /* set rotation */
  memset(&rotate, 0, sizeof(rotate));
  rotate.nPortIndex = 1;
  rotate.nRotation = (OMX_S32)p_params->rotation;
  ret = OMX_SetConfig(p_session->omx_handle, OMX_IndexConfigCommonRotate,
      &rotate);
  if (OMX_ErrorNone != ret) {
    CDBG_ERROR("%s:%d] Error %d", __func__, __LINE__, ret);
    return ret;
  }
  CDBG("%s:%d] Set rotation to %d at port_idx = %d", __func__, __LINE__,
      (int)p_params->rotation, (int)rotate.nPortIndex);

  return ret;
}

/** mm_jpeg_omx_config_thumbnail:
 *
 *  Arguments:
 *    @p_session: job session
 *
 *  Return:
 *       OMX error values
 *
 *  Description:
 *       Configure OMX ports
 *
 **/
OMX_ERRORTYPE mm_jpeg_session_config_thumbnail(mm_jpeg_job_session_t* p_session)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  QOMX_THUMBNAIL_INFO thumbnail_info;
  OMX_INDEXTYPE thumb_indextype;
  mm_jpeg_encode_params_t *p_params = &p_session->params;
  mm_jpeg_encode_job_t *p_jobparams = &p_session->encode_job;
  mm_jpeg_dim_t *p_thumb_dim = &p_jobparams->thumb_dim;
  QOMX_YUV_FRAME_INFO *p_frame_info = &thumbnail_info.tmbOffset;
  mm_jpeg_buf_t *p_tmb_buf = &p_params->src_thumb_buf[p_jobparams->thumb_index];

  CDBG_HIGH("%s:%d] encode_thumbnail %u", __func__, __LINE__,
    p_params->encode_thumbnail);
  if (OMX_FALSE == p_params->encode_thumbnail) {
    return ret;
  }

  if ((p_thumb_dim->dst_dim.width == 0) || (p_thumb_dim->dst_dim.height == 0)) {
    CDBG_ERROR("%s:%d] Error invalid output dim for thumbnail",
      __func__, __LINE__);
    return OMX_ErrorBadParameter;
  }

  if ((p_thumb_dim->src_dim.width == 0) || (p_thumb_dim->src_dim.height == 0)) {
    CDBG_ERROR("%s:%d] Error invalid input dim for thumbnail",
      __func__, __LINE__);
    return OMX_ErrorBadParameter;
  }

  if ((p_thumb_dim->crop.width == 0) || (p_thumb_dim->crop.height == 0)) {
    p_thumb_dim->crop.width = p_thumb_dim->src_dim.width;
    p_thumb_dim->crop.height = p_thumb_dim->src_dim.height;
  }

  /* check crop boundary */
  if ((p_thumb_dim->crop.width + p_thumb_dim->crop.left > p_thumb_dim->src_dim.width) ||
    (p_thumb_dim->crop.height + p_thumb_dim->crop.top > p_thumb_dim->src_dim.height)) {
    CDBG_ERROR("%s:%d] invalid crop boundary (%d, %d) offset (%d, %d) out of (%d, %d)",
      __func__, __LINE__,
      p_thumb_dim->crop.width,
      p_thumb_dim->crop.height,
      p_thumb_dim->crop.left,
      p_thumb_dim->crop.top,
      p_thumb_dim->src_dim.width,
      p_thumb_dim->src_dim.height);
    return OMX_ErrorBadParameter;
  }

  memset(&thumbnail_info, 0x0, sizeof(QOMX_THUMBNAIL_INFO));
  ret = OMX_GetExtensionIndex(p_session->omx_handle,
    QOMX_IMAGE_EXT_THUMBNAIL_NAME,
    &thumb_indextype);
  if (ret) {
    CDBG_ERROR("%s:%d] Error %d", __func__, __LINE__, ret);
    return ret;
  }

  /* fill thumbnail info */
  thumbnail_info.scaling_enabled = 1;
  thumbnail_info.input_width = (OMX_U32)p_thumb_dim->src_dim.width;
  thumbnail_info.input_height = (OMX_U32)p_thumb_dim->src_dim.height;
  thumbnail_info.crop_info.nWidth = (OMX_U32)p_thumb_dim->crop.width;
  thumbnail_info.crop_info.nHeight = (OMX_U32)p_thumb_dim->crop.height;
  thumbnail_info.crop_info.nLeft = p_thumb_dim->crop.left;
  thumbnail_info.crop_info.nTop = p_thumb_dim->crop.top;
  thumbnail_info.rotation = (OMX_U32)p_params->thumb_rotation;

  if ((p_thumb_dim->dst_dim.width > p_thumb_dim->src_dim.width)
    || (p_thumb_dim->dst_dim.height > p_thumb_dim->src_dim.height)) {
    CDBG_ERROR("%s:%d] Incorrect thumbnail dim %dx%d resetting to %dx%d",
      __func__, __LINE__,
      p_thumb_dim->dst_dim.width,
      p_thumb_dim->dst_dim.height,
      p_thumb_dim->src_dim.width,
      p_thumb_dim->src_dim.height);
    thumbnail_info.output_width = (OMX_U32)p_thumb_dim->src_dim.width;
    thumbnail_info.output_height = (OMX_U32)p_thumb_dim->src_dim.height;
  } else {
    thumbnail_info.output_width = (OMX_U32)p_thumb_dim->dst_dim.width;
    thumbnail_info.output_height = (OMX_U32)p_thumb_dim->dst_dim.height;
  }

  memset(p_frame_info, 0x0, sizeof(*p_frame_info));

  p_frame_info->cbcrStartOffset[0] = p_tmb_buf->offset.mp[0].len;
  p_frame_info->cbcrStartOffset[1] = p_tmb_buf->offset.mp[1].len;
  p_frame_info->yOffset = p_tmb_buf->offset.mp[0].offset;
  p_frame_info->cbcrOffset[0] = p_tmb_buf->offset.mp[1].offset;
  p_frame_info->cbcrOffset[1] = p_tmb_buf->offset.mp[2].offset;

  ret = OMX_SetConfig(p_session->omx_handle, thumb_indextype,
    &thumbnail_info);
  if (ret) {
    CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
    return ret;
  }

  return ret;
}

/** mm_jpeg_session_config_main_crop:
 *
 *  Arguments:
 *    @p_session: job session
 *
 *  Return:
 *       OMX error values
 *
 *  Description:
 *       Configure main image crop
 *
 **/
OMX_ERRORTYPE mm_jpeg_session_config_main_crop(mm_jpeg_job_session_t *p_session)
{
  OMX_CONFIG_RECTTYPE rect_type_in, rect_type_out;
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  mm_jpeg_encode_job_t *p_jobparams = &p_session->encode_job;
  mm_jpeg_dim_t *dim = &p_jobparams->main_dim;

  if ((dim->crop.width == 0) || (dim->crop.height == 0)) {
    dim->crop.width = dim->src_dim.width;
    dim->crop.height = dim->src_dim.height;
  }
  /* error check first */
  if ((dim->crop.width + dim->crop.left > dim->src_dim.width) ||
    (dim->crop.height + dim->crop.top > dim->src_dim.height)) {
    CDBG_ERROR("%s:%d] invalid crop boundary (%d, %d) out of (%d, %d)",
      __func__, __LINE__,
      dim->crop.width + dim->crop.left,
      dim->crop.height + dim->crop.top,
      dim->src_dim.width,
      dim->src_dim.height);
    return OMX_ErrorBadParameter;
  }

  memset(&rect_type_in, 0, sizeof(rect_type_in));
  memset(&rect_type_out, 0, sizeof(rect_type_out));
  rect_type_in.nPortIndex = 0;
  rect_type_out.nPortIndex = 0;

  if ((dim->src_dim.width != dim->crop.width) ||
    (dim->src_dim.height != dim->crop.height) ||
    (dim->src_dim.width != dim->dst_dim.width) ||
    (dim->src_dim.height != dim->dst_dim.height)) {
    /* Scaler information */
    rect_type_in.nWidth = CEILING2(dim->crop.width);
    rect_type_in.nHeight = CEILING2(dim->crop.height);
    rect_type_in.nLeft = dim->crop.left;
    rect_type_in.nTop = dim->crop.top;

    if (dim->dst_dim.width && dim->dst_dim.height) {
      rect_type_out.nWidth = (OMX_U32)dim->dst_dim.width;
      rect_type_out.nHeight = (OMX_U32)dim->dst_dim.height;
    }
  }

  ret = OMX_SetConfig(p_session->omx_handle, OMX_IndexConfigCommonInputCrop,
    &rect_type_in);
  if (OMX_ErrorNone != ret) {
    CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
    return ret;
  }

  CDBG("%s:%d] OMX_IndexConfigCommonInputCrop w = %d, h = %d, l = %d, t = %d,"
    " port_idx = %d", __func__, __LINE__,
    (int)rect_type_in.nWidth, (int)rect_type_in.nHeight,
    (int)rect_type_in.nLeft, (int)rect_type_in.nTop,
    (int)rect_type_in.nPortIndex);

  ret = OMX_SetConfig(p_session->omx_handle, OMX_IndexConfigCommonOutputCrop,
    &rect_type_out);
  if (OMX_ErrorNone != ret) {
    CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
    return ret;
  }
  CDBG("%s:%d] OMX_IndexConfigCommonOutputCrop w = %d, h = %d,"
    " port_idx = %d", __func__, __LINE__,
    (int)rect_type_out.nWidth, (int)rect_type_out.nHeight,
    (int)rect_type_out.nPortIndex);

  return ret;
}

/** mm_jpeg_session_config_main:
 *
 *  Arguments:
 *    @p_session: job session
 *
 *  Return:
 *       OMX error values
 *
 *  Description:
 *       Configure main image
 *
 **/
OMX_ERRORTYPE mm_jpeg_session_config_main(mm_jpeg_job_session_t *p_session)
{
  OMX_ERRORTYPE rc = OMX_ErrorNone;

  /* config port */
  CDBG("%s:%d] config port", __func__, __LINE__);
  rc = mm_jpeg_session_config_ports(p_session);
  if (OMX_ErrorNone != rc) {
    CDBG_ERROR("%s: config port failed", __func__);
    return rc;
  }

  /* config buffer offset */
  CDBG("%s:%d] config main buf offset", __func__, __LINE__);
  rc = mm_jpeg_session_config_main_buffer_offset(p_session);
  if (OMX_ErrorNone != rc) {
    CDBG_ERROR("%s: config buffer offset failed", __func__);
    return rc;
  }

  /* set the encoding mode */
  rc = mm_jpeg_encoding_mode(p_session);
  if (OMX_ErrorNone != rc) {
    CDBG_ERROR("%s: config encoding mode failed", __func__);
    return rc;
  }

  /* set the metadata encrypt key */
  rc = mm_jpeg_meta_enc_key(p_session);
  if (OMX_ErrorNone != rc) {
    CDBG_ERROR("%s: config session failed", __func__);
    return rc;
  }

  /* set the mem ops */
  rc = mm_jpeg_mem_ops(p_session);
  if (OMX_ErrorNone != rc) {
    CDBG_ERROR("%s: config mem ops failed", __func__);
    return rc;
  }
  /* set the jpeg speed mode */
  rc = mm_jpeg_speed_mode(p_session);
  if (OMX_ErrorNone != rc) {
    CDBG_ERROR("%s: config speed mode failed", __func__);
    return rc;
  }

  return rc;
}

/** mm_jpeg_session_config_common:
 *
 *  Arguments:
 *    @p_session: job session
 *
 *  Return:
 *       OMX error values
 *
 *  Description:
 *       Configure common parameters
 *
 **/
OMX_ERRORTYPE mm_jpeg_session_config_common(mm_jpeg_job_session_t *p_session)
{
  OMX_ERRORTYPE rc = OMX_ErrorNone;
  OMX_INDEXTYPE exif_idx;
  OMX_CONFIG_ROTATIONTYPE rotate;
  mm_jpeg_encode_job_t *p_jobparams = &p_session->encode_job;
  QOMX_EXIF_INFO exif_info;

  /* set rotation */
  memset(&rotate, 0, sizeof(rotate));
  rotate.nPortIndex = 1;
  rotate.nRotation = (OMX_S32)p_jobparams->rotation;
  rc = OMX_SetConfig(p_session->omx_handle, OMX_IndexConfigCommonRotate,
    &rotate);
  if (OMX_ErrorNone != rc) {
      CDBG_ERROR("%s:%d] Error %d", __func__, __LINE__, rc);
      return rc;
  }
  CDBG("%s:%d] Set rotation to %d at port_idx = %d", __func__, __LINE__,
    (int)p_jobparams->rotation, (int)rotate.nPortIndex);

  /* Set Exif data*/
  memset(&p_session->exif_info_local[0], 0, sizeof(p_session->exif_info_local));
  rc = OMX_GetExtensionIndex(p_session->omx_handle, QOMX_IMAGE_EXT_EXIF_NAME,
    &exif_idx);
  if (OMX_ErrorNone != rc) {
    CDBG_ERROR("%s:%d] Error %d", __func__, __LINE__, rc);
    return rc;
  }

  CDBG("%s:%d] Num of exif entries passed from HAL: %d", __func__, __LINE__,
      (int)p_jobparams->exif_info.numOfEntries);
  if (p_jobparams->exif_info.numOfEntries > 0) {
    rc = OMX_SetConfig(p_session->omx_handle, exif_idx,
        &p_jobparams->exif_info);
    if (OMX_ErrorNone != rc) {
      CDBG_ERROR("%s:%d] Error %d", __func__, __LINE__, rc);
      return rc;
    }
  }
  /*parse aditional exif data from the metadata*/
  if (NULL != p_jobparams->p_metadata) {
    exif_info.numOfEntries = 0;
    exif_info.exif_data = &p_session->exif_info_local[0];
    process_meta_data(p_jobparams->p_metadata, &exif_info, &p_jobparams->cam_exif_params);
    /* After Parse metadata */
    p_session->exif_count_local = (int)exif_info.numOfEntries;

    if (exif_info.numOfEntries > 0) {
      /* set exif tags */
      CDBG("%s:%d] exif tags from metadata count %d", __func__, __LINE__,
        (int)exif_info.numOfEntries);

      rc = OMX_SetConfig(p_session->omx_handle, exif_idx,
        &exif_info);
      if (OMX_ErrorNone != rc) {
        CDBG_ERROR("%s:%d] Error %d", __func__, __LINE__, rc);
        return rc;
      }
    }
  }

  return rc;
}




/** mm_jpeg_session_abort:
 *
 *  Arguments:
 *    @p_session: jpeg session
 *
 *  Return:
 *       OMX_BOOL
 *
 *  Description:
 *       Abort ongoing job
 *
 **/
OMX_BOOL mm_jpeg_session_abort(mm_jpeg_job_session_t *p_session)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  int rc = 0;

  CDBG("%s:%d] E", __func__, __LINE__);
  pthread_mutex_lock(&p_session->lock);
  if (MM_JPEG_ABORT_NONE != p_session->abort_state) {
    pthread_mutex_unlock(&p_session->lock);
    CDBG("%s:%d] **** ALREADY ABORTED", __func__, __LINE__);
    return 0;
  }
  p_session->abort_state = MM_JPEG_ABORT_INIT;
  if (OMX_TRUE == p_session->encoding) {
    p_session->state_change_pending = OMX_TRUE;

    CDBG("%s:%d] **** ABORTING", __func__, __LINE__);
    pthread_mutex_unlock(&p_session->lock);

    ret = OMX_SendCommand(p_session->omx_handle, OMX_CommandStateSet,
    OMX_StateIdle, NULL);

    if (ret != OMX_ErrorNone) {
      CDBG("%s:%d] OMX_SendCommand returned error %d", __func__, __LINE__, ret);
      return 1;
    }
    rc = mm_jpegenc_destroy_job(p_session);
    if (rc != 0) {
      CDBG("%s:%d] Destroy job returned error %d", __func__, __LINE__, rc);
    }

    pthread_mutex_lock(&p_session->lock);
    if (MM_JPEG_ABORT_INIT == p_session->abort_state) {
      CDBG("%s:%d] before wait", __func__, __LINE__);
      pthread_cond_wait(&p_session->cond, &p_session->lock);
    }
    CDBG("%s:%d] after wait", __func__, __LINE__);
  }
  p_session->abort_state = MM_JPEG_ABORT_DONE;
  pthread_mutex_unlock(&p_session->lock);


  // Abort next session
  if (p_session->next_session) {
    mm_jpeg_session_abort(p_session->next_session);
  }

  CDBG("%s:%d] X", __func__, __LINE__);
  return 0;
}


/** mm_jpeg_configure_params
 *
 *  Arguments:
 *    @p_session: encode session
 *
 *  Return:
 *       none
 *
 *  Description:
 *       Configure the job specific params
 *
 **/
static OMX_ERRORTYPE mm_jpeg_configure_job_params(
  mm_jpeg_job_session_t *p_session)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  OMX_IMAGE_PARAM_QFACTORTYPE q_factor;
  QOMX_WORK_BUFFER work_buffer;
  OMX_INDEXTYPE work_buffer_index;
  mm_jpeg_encode_params_t *p_params = &p_session->params;

  /* common config */
  ret = mm_jpeg_session_config_common(p_session);
  if (OMX_ErrorNone != ret) {
    CDBG_ERROR("%s:%d] config common failed", __func__, __LINE__);

  }

  /* config Main Image crop */
  CDBG("%s:%d] config main crop", __func__, __LINE__);
  ret = mm_jpeg_session_config_main_crop(p_session);
  if (OMX_ErrorNone != ret) {
    CDBG_ERROR("%s: config crop failed", __func__);
    return ret;
  }

  /* set quality */
  memset(&q_factor, 0, sizeof(q_factor));
  q_factor.nPortIndex = 0;
  q_factor.nQFactor = p_params->quality;
  ret = OMX_SetConfig(p_session->omx_handle, OMX_IndexParamQFactor, &q_factor);
  CDBG("%s:%d] config QFactor: %d", __func__, __LINE__, (int)q_factor.nQFactor);
  if (OMX_ErrorNone != ret) {
    CDBG_ERROR("%s:%d] Error setting Q factor %d", __func__, __LINE__, ret);
    return ret;
  }

  /* config thumbnail */
  ret = mm_jpeg_session_config_thumbnail(p_session);
  if (OMX_ErrorNone != ret) {
    CDBG_ERROR("%s:%d] config thumbnail img failed", __func__, __LINE__);
    return ret;
  }

  //Pass the ION buffer to be used as o/p for HW
  memset(&work_buffer, 0x0, sizeof(QOMX_WORK_BUFFER));
  ret = OMX_GetExtensionIndex(p_session->omx_handle,
    QOMX_IMAGE_EXT_WORK_BUFFER_NAME,
    &work_buffer_index);
  if (ret) {
    CDBG_ERROR("%s:%d] Error getting work buffer index %d",
      __func__, __LINE__, ret);
    return ret;
  }
  work_buffer.fd = p_session->work_buffer.p_pmem_fd;
  work_buffer.vaddr = p_session->work_buffer.addr;
  work_buffer.length = (uint32_t)p_session->work_buffer.size;
  CDBG_ERROR("%s:%d] Work buffer %d %p WorkBufSize: %d", __func__, __LINE__,
    work_buffer.fd, work_buffer.vaddr, work_buffer.length);

  buffer_invalidate(&p_session->work_buffer);

  ret = OMX_SetConfig(p_session->omx_handle, work_buffer_index,
    &work_buffer);
  if (ret) {
    CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
    return ret;
  }

  /* set metadata */
  ret = mm_jpeg_metadata(p_session);
  CDBG_ERROR("%s: config makernote data failed", __func__);
  if (OMX_ErrorNone != ret) {
    return ret;
  }

  return ret;
}

/** mm_jpeg_session_configure:
 *
 *  Arguments:
 *    @data: encode session
 *
 *  Return:
 *       none
 *
 *  Description:
 *       Configure the session
 *
 **/
static OMX_ERRORTYPE mm_jpeg_session_configure(mm_jpeg_job_session_t *p_session)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;

  CDBG("%s:%d] E ", __func__, __LINE__);

  MM_JPEG_CHK_ABORT(p_session, ret, error);

  /* config main img */
  ret = mm_jpeg_session_config_main(p_session);
  if (OMX_ErrorNone != ret) {
    CDBG_ERROR("%s:%d] config main img failed", __func__, __LINE__);
    goto error;
  }
  ret = mm_jpeg_session_change_state(p_session, OMX_StateIdle,
    mm_jpeg_session_send_buffers);
  if (ret) {
    CDBG_ERROR("%s:%d] change state to idle failed %d",
      __func__, __LINE__, ret);
    goto error;
  }

  ret = mm_jpeg_session_change_state(p_session, OMX_StateExecuting,
    NULL);
  if (ret) {
    CDBG_ERROR("%s:%d] change state to executing failed %d",
      __func__, __LINE__, ret);
    goto error;
  }

error:
  CDBG("%s:%d] X ret %d", __func__, __LINE__, ret);
  return ret;
}






/** mm_jpeg_session_encode:
 *
 *  Arguments:
 *    @p_session: encode session
 *
 *  Return:
 *       OMX_ERRORTYPE
 *
 *  Description:
 *       Start the encoding
 *
 **/
static OMX_ERRORTYPE mm_jpeg_session_encode(mm_jpeg_job_session_t *p_session)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  mm_jpeg_encode_job_t *p_jobparams = &p_session->encode_job;

  pthread_mutex_lock(&p_session->lock);
  p_session->abort_state = MM_JPEG_ABORT_NONE;
  p_session->encoding = OMX_FALSE;
  pthread_mutex_unlock(&p_session->lock);

  if (OMX_FALSE == p_session->config) {
    ret = mm_jpeg_session_configure(p_session);
    if (ret) {
      CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
      goto error;
    }
    p_session->config = OMX_TRUE;
  }

  ret = mm_jpeg_configure_job_params(p_session);
  if (ret) {
      CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
      goto error;
  }
  pthread_mutex_lock(&p_session->lock);
  p_session->encoding = OMX_TRUE;
  pthread_mutex_unlock(&p_session->lock);

  MM_JPEG_CHK_ABORT(p_session, ret, error);

#ifdef MM_JPEG_DUMP_INPUT
  char filename[256];
#ifdef USE_KK_CODE
  snprintf(filename, 255, "/data/jpeg/mm_jpeg_int%d.yuv", p_session->ebd_count);
#else
  snprintf(filename, 255, "/data/misc/camera/jpeg/mm_jpeg_int%d.yuv", p_session->ebd_count);
#endif
  DUMP_TO_FILE(filename,
    p_session->p_in_omx_buf[p_jobparams->src_index]->pBuffer,
    (size_t)p_session->p_in_omx_buf[p_jobparams->src_index]->nAllocLen);
#endif

  ret = OMX_EmptyThisBuffer(p_session->omx_handle,
    p_session->p_in_omx_buf[p_jobparams->src_index]);
  if (ret) {
    CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
    goto error;
  }

  if (p_session->params.encode_thumbnail) {
#ifdef MM_JPEG_DUMP_INPUT
  char thumb_filename[256];
#ifdef USE_KK_CODE
  snprintf(thumb_filename, 255, "/data/jpeg/mm_jpeg_int_t%d.yuv",
    p_session->ebd_count);
#else
  snprintf(thumb_filename, 255, "/data/misc/camera/jpeg/mm_jpeg_int_t%d.yuv",
    p_session->ebd_count);
#endif
  DUMP_TO_FILE(filename,
    p_session->p_in_omx_thumb_buf[p_jobparams->thumb_index]->pBuffer,
    (size_t)p_session->p_in_omx_thumb_buf[p_jobparams->thumb_index]->nAllocLen);
#endif
    ret = OMX_EmptyThisBuffer(p_session->omx_handle,
        p_session->p_in_omx_thumb_buf[p_jobparams->thumb_index]);
    if (ret) {
      CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
      goto error;
    }
  }

  ret = OMX_FillThisBuffer(p_session->omx_handle,
    p_session->p_out_omx_buf[p_jobparams->dst_index]);
  if (ret) {
    CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
    goto error;
  }

  MM_JPEG_CHK_ABORT(p_session, ret, error);

error:

  CDBG("%s:%d] X ", __func__, __LINE__);
  return ret;
}

/** mm_jpeg_process_encoding_job:
 *
 *  Arguments:
 *    @my_obj: jpeg client
 *    @job_node: job node
 *
 *  Return:
 *       0 for success -1 otherwise
 *
 *  Description:
 *       Start the encoding job
 *
 **/
int32_t mm_jpeg_process_encoding_job(mm_jpeg_obj *my_obj, mm_jpeg_job_q_node_t* job_node)
{
  mm_jpeg_q_data_t qdata;
  int32_t rc = 0;
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  mm_jpeg_job_session_t *p_session = NULL;
  uint32_t buf_idx;

  /* check if valid session */
  p_session = mm_jpeg_get_session(my_obj, job_node->enc_info.job_id);
  if (NULL == p_session) {
    CDBG_ERROR("%s:%d] invalid job id %x", __func__, __LINE__,
        job_node->enc_info.job_id);
    return -1;
  }

  CDBG_HIGH("%s:%d] before dequeue session %d",
                __func__, __LINE__, ret);

  /* dequeue available omx handle */
  qdata = mm_jpeg_queue_deq(p_session->session_handle_q);
  p_session = qdata.p;

  if (NULL == p_session) {
    CDBG_HIGH("%s:%d] No available sessions %d",
          __func__, __LINE__, ret);
    /* No available handles */
    qdata.p = job_node;
    mm_jpeg_queue_enq_head(&my_obj->job_mgr.job_queue, qdata);

    CDBG_HIGH("%s:%d]end enqueue %d",
              __func__, __LINE__, ret);
    return rc;

  }

  p_session->auto_out_buf = OMX_FALSE;
  if (job_node->enc_info.encode_job.dst_index < 0) {
    /* dequeue available output buffer idx */
    qdata = mm_jpeg_queue_deq(p_session->out_buf_q);
    buf_idx = qdata.u32;

    if (0U == buf_idx) {
      CDBG_ERROR("%s:%d] No available output buffers %d",
          __func__, __LINE__, ret);
      return OMX_ErrorUndefined;
    }

    buf_idx--;

    job_node->enc_info.encode_job.dst_index = (int32_t)buf_idx;
    p_session->auto_out_buf = OMX_TRUE;
  }

  /* sent encode cmd to OMX, queue job into ongoing queue */
  qdata.p = job_node;
  rc = mm_jpeg_queue_enq(&my_obj->ongoing_job_q, qdata);
  if (rc) {
    CDBG_ERROR("%s:%d] jpeg enqueue failed %d",
      __func__, __LINE__, ret);
    goto error;
  }

  p_session->encode_job = job_node->enc_info.encode_job;
  p_session->jobId = job_node->enc_info.job_id;
  ret = mm_jpeg_session_encode(p_session);
  if (ret) {
    CDBG_ERROR("%s:%d] encode session failed", __func__, __LINE__);
    goto error;
  }

  CDBG("%s:%d] Success X ", __func__, __LINE__);
  return rc;

error:

  if ((OMX_ErrorNone != ret) &&
    (NULL != p_session->params.jpeg_cb)) {
    p_session->job_status = JPEG_JOB_STATUS_ERROR;
    CDBG("%s:%d] send jpeg error callback %d", __func__, __LINE__,
      p_session->job_status);
    p_session->params.jpeg_cb(p_session->job_status,
      p_session->client_hdl,
      p_session->jobId,
      NULL,
      p_session->params.userdata);
  }

  /*remove the job*/
  mm_jpegenc_job_done(p_session);
  CDBG("%s:%d] Error X ", __func__, __LINE__);

  return rc;
}



/** mm_jpeg_jobmgr_thread:
 *
 *  Arguments:
 *    @my_obj: jpeg object
 *
 *  Return:
 *       0 for success else failure
 *
 *  Description:
 *       job manager thread main function
 *
 **/
static void *mm_jpeg_jobmgr_thread(void *data)
{
  mm_jpeg_q_data_t qdata;
  int rc = 0;
  int running = 1;
  uint32_t num_ongoing_jobs = 0;
  mm_jpeg_obj *my_obj = (mm_jpeg_obj*)data;
  mm_jpeg_job_cmd_thread_t *cmd_thread = &my_obj->job_mgr;
  mm_jpeg_job_q_node_t* node = NULL;
  prctl(PR_SET_NAME, (unsigned long)"mm_jpeg_thread", 0, 0, 0);

  do {
    do {
      rc = cam_sem_wait(&cmd_thread->job_sem);
      if (rc != 0 && errno != EINVAL) {
        CDBG_ERROR("%s: cam_sem_wait error (%s)",
          __func__, strerror(errno));
        return NULL;
      }
    } while (rc != 0);

    /* check ongoing q size */
    num_ongoing_jobs = mm_jpeg_queue_get_size(&my_obj->ongoing_job_q);
    if (num_ongoing_jobs >= NUM_MAX_JPEG_CNCURRENT_JOBS) {
      CDBG("%s:%d] ongoing job already reach max %d", __func__,
        __LINE__, num_ongoing_jobs);
      continue;
    }

    pthread_mutex_lock(&my_obj->job_lock);
    /* can go ahead with new work */
    qdata = mm_jpeg_queue_deq(&cmd_thread->job_queue);
    node = (mm_jpeg_job_q_node_t*)qdata.p;
    if (node != NULL) {
      switch (node->type) {
      case MM_JPEG_CMD_TYPE_JOB:
        rc = mm_jpeg_process_encoding_job(my_obj, node);
        break;
      case MM_JPEG_CMD_TYPE_DECODE_JOB:
        rc = mm_jpegdec_process_decoding_job(my_obj, node);
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

/** mm_jpeg_jobmgr_thread_launch:
 *
 *  Arguments:
 *    @my_obj: jpeg object
 *
 *  Return:
 *       0 for success else failure
 *
 *  Description:
 *       launches the job manager thread
 *
 **/
int32_t mm_jpeg_jobmgr_thread_launch(mm_jpeg_obj *my_obj)
{
  int32_t rc = 0;
  mm_jpeg_job_cmd_thread_t *job_mgr = &my_obj->job_mgr;

  cam_sem_init(&job_mgr->job_sem, 0);
  mm_jpeg_queue_init(&job_mgr->job_queue);

  /* launch the thread */
  pthread_create(&job_mgr->pid,
    NULL,
    mm_jpeg_jobmgr_thread,
    (void *)my_obj);
  return rc;
}

/** mm_jpeg_jobmgr_thread_release:
 *
 *  Arguments:
 *    @my_obj: jpeg object
 *
 *  Return:
 *       0 for success else failure
 *
 *  Description:
 *       Releases the job manager thread
 *
 **/
int32_t mm_jpeg_jobmgr_thread_release(mm_jpeg_obj * my_obj)
{
  mm_jpeg_q_data_t qdata;
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

  qdata.p = node;
  mm_jpeg_queue_enq(&cmd_thread->job_queue, qdata);
  cam_sem_post(&cmd_thread->job_sem);

  /* wait until cmd thread exits */
  if (pthread_join(cmd_thread->pid, NULL) != 0) {
    CDBG("%s: pthread dead already", __func__);
  }
  mm_jpeg_queue_deinit(&cmd_thread->job_queue);

  cam_sem_destroy(&cmd_thread->job_sem);
  memset(cmd_thread, 0, sizeof(mm_jpeg_job_cmd_thread_t));
  return rc;
}

/** mm_jpeg_init:
 *
 *  Arguments:
 *    @my_obj: jpeg object
 *
 *  Return:
 *       0 for success else failure
 *
 *  Description:
 *       Initializes the jpeg client
 *
 **/
int32_t mm_jpeg_init(mm_jpeg_obj *my_obj)
{
  int32_t rc = 0;
  uint32_t work_buf_size;
  unsigned int i = 0;
  unsigned int initial_workbufs_cnt = 1;

  /* init locks */
  pthread_mutex_init(&my_obj->job_lock, NULL);

  /* init ongoing job queue */
  rc = mm_jpeg_queue_init(&my_obj->ongoing_job_q);
  if (0 != rc) {
    CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
    pthread_mutex_destroy(&my_obj->job_lock);
    return -1;
  }


  /* init job semaphore and launch jobmgr thread */
  CDBG("%s:%d] Launch jobmgr thread rc %d", __func__, __LINE__, rc);
  rc = mm_jpeg_jobmgr_thread_launch(my_obj);
  if (0 != rc) {
    CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
    mm_jpeg_queue_deinit(&my_obj->ongoing_job_q);
    pthread_mutex_destroy(&my_obj->job_lock);
    return -1;
  }

  /* set work buf size from max picture size */
  if (my_obj->max_pic_w <= 0 || my_obj->max_pic_h <= 0) {
    CDBG_ERROR("%s:%d] Width and height are not valid "
      "dimensions, cannot calc work buf size",__func__, __LINE__);
    mm_jpeg_jobmgr_thread_release(my_obj);
    mm_jpeg_queue_deinit(&my_obj->ongoing_job_q);
    pthread_mutex_destroy(&my_obj->job_lock);
    return -1;
  }
  work_buf_size = CEILING64((uint32_t)my_obj->max_pic_w) *
    CEILING64((uint32_t)my_obj->max_pic_h) * 3U / 2U;
  for (i = 0; i < initial_workbufs_cnt; i++) {
    my_obj->ionBuffer[i].size = CEILING32(work_buf_size);
    CDBG_HIGH("Max picture size %d x %d, WorkBufSize = %zu",
        my_obj->max_pic_w, my_obj->max_pic_h, my_obj->ionBuffer[i].size);

    my_obj->ionBuffer[i].addr = (uint8_t *)buffer_allocate(&my_obj->ionBuffer[i], 1);
    if (NULL == my_obj->ionBuffer[i].addr) {
      while (i--) {
        buffer_deallocate(&my_obj->ionBuffer[i]);
      }
      mm_jpeg_jobmgr_thread_release(my_obj);
      mm_jpeg_queue_deinit(&my_obj->ongoing_job_q);
      pthread_mutex_destroy(&my_obj->job_lock);
      CDBG_ERROR("%s:%d] Ion allocation failed",__func__, __LINE__);
      return -1;
    }
  }

  my_obj->work_buf_cnt = i;

  /* load OMX */
  if (OMX_ErrorNone != OMX_Init()) {
    /* roll back in error case */
    CDBG_ERROR("%s:%d] OMX_Init failed (%d)", __func__, __LINE__, rc);
    for (i = 0; i < initial_workbufs_cnt; i++) {
      buffer_deallocate(&my_obj->ionBuffer[i]);
    }
    mm_jpeg_jobmgr_thread_release(my_obj);
    mm_jpeg_queue_deinit(&my_obj->ongoing_job_q);
    pthread_mutex_destroy(&my_obj->job_lock);
  }

#ifdef LOAD_ADSP_RPC_LIB
  my_obj->adsprpc_lib_handle = dlopen("libadsprpc.so", RTLD_NOW);
  if (NULL == my_obj->adsprpc_lib_handle) {
    CDBG_ERROR("%s:%d] Cannot load the library", __func__, __LINE__);
    /* not returning error here bcoz even if this loading fails
        we can go ahead with SW JPEG enc */
  }
#endif

  return rc;
}

/** mm_jpeg_deinit:
 *
 *  Arguments:
 *    @my_obj: jpeg object
 *
 *  Return:
 *       0 for success else failure
 *
 *  Description:
 *       Deinits the jpeg client
 *
 **/
int32_t mm_jpeg_deinit(mm_jpeg_obj *my_obj)
{
  int32_t rc = 0;
  uint32_t i = 0;

  /* release jobmgr thread */
  rc = mm_jpeg_jobmgr_thread_release(my_obj);
  if (0 != rc) {
    CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
  }

  /* unload OMX engine */
  OMX_Deinit();

  /* deinit ongoing job and cb queue */
  rc = mm_jpeg_queue_deinit(&my_obj->ongoing_job_q);
  if (0 != rc) {
    CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
  }

  for (i = 0; i < my_obj->work_buf_cnt; i++) {
    /*Release the ION buffer*/
    rc = buffer_deallocate(&my_obj->ionBuffer[i]);
    if (0 != rc) {
      CDBG_ERROR("%s:%d] Error releasing ION buffer", __func__, __LINE__);
    }
  }

  /* destroy locks */
  pthread_mutex_destroy(&my_obj->job_lock);

  return rc;
}

/** mm_jpeg_new_client:
 *
 *  Arguments:
 *    @my_obj: jpeg object
 *
 *  Return:
 *       0 for success else failure
 *
 *  Description:
 *       Create new jpeg client
 *
 **/
uint32_t mm_jpeg_new_client(mm_jpeg_obj *my_obj)
{
  uint32_t client_hdl = 0;
  uint8_t idx;
  int i = 0;

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
    /* client session avail */
    /* generate client handler by index */
    client_hdl = mm_jpeg_util_generate_handler(idx);

    /* update client session */
    my_obj->clnt_mgr[idx].is_used = 1;
    my_obj->clnt_mgr[idx].client_handle = client_hdl;

    pthread_mutex_init(&my_obj->clnt_mgr[idx].lock, NULL);
    for (i = 0; i < MM_JPEG_MAX_SESSION; i++) {
      memset(&my_obj->clnt_mgr[idx].session[i], 0x0, sizeof(mm_jpeg_job_session_t));
    }

    /* increse client count */
    my_obj->num_clients++;
  }

  return client_hdl;
}

/** mm_jpeg_start_job:
 *
 *  Arguments:
 *    @my_obj: jpeg object
 *    @client_hdl: client handle
 *    @job: pointer to encode job
 *    @jobId: job id
 *
 *  Return:
 *       0 for success else failure
 *
 *  Description:
 *       Start the encoding job
 *
 **/
int32_t mm_jpeg_start_job(mm_jpeg_obj *my_obj,
  mm_jpeg_job_t *job,
  uint32_t *job_id)
{
  mm_jpeg_q_data_t qdata;
  int32_t rc = -1;
  uint8_t session_idx = 0;
  uint8_t client_idx = 0;
  mm_jpeg_job_q_node_t* node = NULL;
  mm_jpeg_job_session_t *p_session = NULL;
  mm_jpeg_encode_job_t *p_jobparams  = &job->encode_job;

  *job_id = 0;

  /* check if valid session */
  session_idx = GET_SESSION_IDX(p_jobparams->session_id);
  client_idx = GET_CLIENT_IDX(p_jobparams->session_id);
  CDBG("%s:%d] session_idx %d client idx %d", __func__, __LINE__,
    session_idx, client_idx);

  if ((session_idx >= MM_JPEG_MAX_SESSION) ||
    (client_idx >= MAX_JPEG_CLIENT_NUM)) {
    CDBG_ERROR("%s:%d] invalid session id %x", __func__, __LINE__,
      job->encode_job.session_id);
    return rc;
  }

  p_session = &my_obj->clnt_mgr[client_idx].session[session_idx];
  if (OMX_FALSE == p_session->active) {
    CDBG_ERROR("%s:%d] session not active %x", __func__, __LINE__,
      job->encode_job.session_id);
    return rc;
  }

  if ((p_jobparams->src_index >= (int32_t)p_session->params.num_src_bufs) ||
    (p_jobparams->dst_index >= (int32_t)p_session->params.num_dst_bufs)) {
    CDBG_ERROR("%s:%d] invalid buffer indices", __func__, __LINE__);
    return rc;
  }

  /* enqueue new job into todo job queue */
  node = (mm_jpeg_job_q_node_t *)malloc(sizeof(mm_jpeg_job_q_node_t));
  if (NULL == node) {
    CDBG_ERROR("%s: No memory for mm_jpeg_job_q_node_t", __func__);
    return -1;
  }

  *job_id = job->encode_job.session_id |
    (((uint32_t)p_session->job_hist++ % JOB_HIST_MAX) << 16);

  memset(node, 0, sizeof(mm_jpeg_job_q_node_t));
  node->enc_info.encode_job = job->encode_job;
  node->enc_info.job_id = *job_id;
  node->enc_info.client_handle = p_session->client_hdl;
  node->type = MM_JPEG_CMD_TYPE_JOB;



  qdata.p = node;
  rc = mm_jpeg_queue_enq(&my_obj->job_mgr.job_queue, qdata);
  if (0 == rc) {
      cam_sem_post(&my_obj->job_mgr.job_sem);
  }

  CDBG_ERROR("%s:%d] X", __func__, __LINE__);

  return rc;
}



/** mm_jpeg_abort_job:
 *
 *  Arguments:
 *    @my_obj: jpeg object
 *    @client_hdl: client handle
 *    @jobId: job id
 *
 *  Return:
 *       0 for success else failure
 *
 *  Description:
 *       Abort the encoding session
 *
 **/
int32_t mm_jpeg_abort_job(mm_jpeg_obj *my_obj,
  uint32_t jobId)
{
  int32_t rc = -1;
  mm_jpeg_job_q_node_t *node = NULL;
  mm_jpeg_job_session_t *p_session = NULL;

  CDBG("%s:%d] ", __func__, __LINE__);
  pthread_mutex_lock(&my_obj->job_lock);

  /* abort job if in todo queue */
  node = mm_jpeg_queue_remove_job_by_job_id(&my_obj->job_mgr.job_queue, jobId);
  if (NULL != node) {
    free(node);
    goto abort_done;
  }

  /* abort job if in ongoing queue */
  node = mm_jpeg_queue_remove_job_by_job_id(&my_obj->ongoing_job_q, jobId);
  if (NULL != node) {
    /* find job that is OMX ongoing, ask OMX to abort the job */
    p_session = mm_jpeg_get_session(my_obj, node->enc_info.job_id);
    if (p_session) {
      mm_jpeg_session_abort(p_session);
    } else {
      CDBG_ERROR("%s:%d] Invalid job id 0x%x", __func__, __LINE__,
        node->enc_info.job_id);
    }
    free(node);
    goto abort_done;
  }

abort_done:
  pthread_mutex_unlock(&my_obj->job_lock);

  return rc;
}


#ifdef MM_JPEG_READ_META_KEYFILE
static int32_t mm_jpeg_read_meta_keyfile(mm_jpeg_job_session_t *p_session,
    const char *filename)
{
  int rc = 0;
  FILE *fp = NULL;
  size_t file_size = 0;
  fp = fopen(filename, "r");
  if (!fp) {
    CDBG_ERROR("%s:%d] Key not present", __func__, __LINE__);
    return -1;
  }
  fseek(fp, 0, SEEK_END);
  file_size = (size_t)ftell(fp);
  fseek(fp, 0, SEEK_SET);

  p_session->meta_enc_key = (uint8_t *) malloc((file_size + 1) * sizeof(uint8_t));

  if (!p_session->meta_enc_key) {
    CDBG_ERROR("%s:%d] error", __func__, __LINE__);
    return -1;
  }

  fread(p_session->meta_enc_key, 1, file_size, fp);
  fclose(fp);

  p_session->meta_enc_keylen = file_size;

  return rc;
}
#endif // MM_JPEG_READ_META_KEYFILE

/** mm_jpeg_create_session:
 *
 *  Arguments:
 *    @my_obj: jpeg object
 *    @client_hdl: client handle
 *    @p_params: pointer to encode params
 *    @p_session_id: session id
 *
 *  Return:
 *       0 for success else failure
 *
 *  Description:
 *       Start the encoding session
 *
 **/
int32_t mm_jpeg_create_session(mm_jpeg_obj *my_obj,
  uint32_t client_hdl,
  mm_jpeg_encode_params_t *p_params,
  uint32_t* p_session_id)
{
  mm_jpeg_q_data_t qdata;
  int32_t rc = 0;
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  uint8_t clnt_idx = 0;
  int session_idx = -1;
  mm_jpeg_job_session_t *p_session = NULL;
  mm_jpeg_job_session_t * p_prev_session = NULL;
  *p_session_id = 0;
  uint32_t i = 0;
  uint32_t num_omx_sessions;
  uint32_t work_buf_size;
  mm_jpeg_queue_t *p_session_handle_q, *p_out_buf_q;
  uint32_t work_bufs_need;

  /* validate the parameters */
  if ((p_params->num_src_bufs > MM_JPEG_MAX_BUF)
    || (p_params->num_dst_bufs > MM_JPEG_MAX_BUF)) {
    CDBG_ERROR("%s:%d] invalid num buffers", __func__, __LINE__);
    return rc;
  }

  /* check if valid client */
  clnt_idx = mm_jpeg_util_get_index_by_handler(client_hdl);
  if (clnt_idx >= MAX_JPEG_CLIENT_NUM) {
    CDBG_ERROR("%s: invalid client with handler (%d)", __func__, client_hdl);
    return rc;
  }

  num_omx_sessions = 1;
  if (p_params->burst_mode) {
    num_omx_sessions = MM_JPEG_CONCURRENT_SESSIONS_COUNT;
  }
  work_bufs_need = my_obj->num_sessions + num_omx_sessions;
  if (work_bufs_need > MM_JPEG_CONCURRENT_SESSIONS_COUNT) {
    work_bufs_need = MM_JPEG_CONCURRENT_SESSIONS_COUNT;
  }
  CDBG_ERROR("%s:%d] >>>> Work bufs need %d", __func__, __LINE__, work_bufs_need);
  work_buf_size = CEILING64(my_obj->max_pic_w) *
      CEILING64(my_obj->max_pic_h) * 1.5;
  for (i = my_obj->work_buf_cnt; i < work_bufs_need; i++) {
     my_obj->ionBuffer[i].size = CEILING32(work_buf_size);
     CDBG_HIGH("Max picture size %d x %d, WorkBufSize = %zu",
         my_obj->max_pic_w, my_obj->max_pic_h, my_obj->ionBuffer[i].size);

     my_obj->ionBuffer[i].addr = (uint8_t *)buffer_allocate(&my_obj->ionBuffer[i], 1);
     if (NULL == my_obj->ionBuffer[i].addr) {
       CDBG_ERROR("%s:%d] Ion allocation failed",__func__, __LINE__);
       return -1;
     }
     my_obj->work_buf_cnt++;
  }

  /* init omx handle queue */
  p_session_handle_q = (mm_jpeg_queue_t *) malloc(sizeof(*p_session_handle_q));
  if (NULL == p_session_handle_q) {
    CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
    return -1;
  }
  rc = mm_jpeg_queue_init(p_session_handle_q);
  if (0 != rc) {
    CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
    return -1;
  }

  /* init output buf queue */
  p_out_buf_q = (mm_jpeg_queue_t *) malloc(sizeof(*p_out_buf_q));
  if (NULL == p_out_buf_q) {
    CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
    return -1;
  }
  /* init omx handle queue */
  rc = mm_jpeg_queue_init(p_out_buf_q);
  if (0 != rc) {
    CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
    return -1;
  }

  for (i = 0; i < num_omx_sessions; i++) {
    uint32_t buf_idx = 0U;
    session_idx = mm_jpeg_get_new_session_idx(my_obj, clnt_idx, &p_session);
    if (session_idx < 0) {
      CDBG_ERROR("%s:%d] invalid session id (%d)", __func__, __LINE__, session_idx);
      return rc;
    }

    p_session->next_session = NULL;

    if (p_prev_session) {
      p_prev_session->next_session = p_session;
    }
    p_prev_session = p_session;

    buf_idx = my_obj->num_sessions + i;
    if (buf_idx < MM_JPEG_CONCURRENT_SESSIONS_COUNT) {
      p_session->work_buffer = my_obj->ionBuffer[buf_idx];
    } else {
      p_session->work_buffer.addr = NULL;
      p_session->work_buffer.ion_fd = -1;
      p_session->work_buffer.p_pmem_fd = -1;
    }

    p_session->jpeg_obj = (void*)my_obj; /* save a ptr to jpeg_obj */

    ret = mm_jpeg_session_create(p_session);
    if (OMX_ErrorNone != ret) {
      p_session->active = OMX_FALSE;
      CDBG_ERROR("%s:%d] jpeg session create failed", __func__, __LINE__);
      return rc;
    }

    uint32_t session_id = (JOB_ID_MAGICVAL << 24) |
        ((uint32_t)session_idx << 8) | clnt_idx;

    if (!*p_session_id) {
      *p_session_id = session_id;
    }

    /*copy the params*/
    p_session->params = *p_params;
    p_session->client_hdl = client_hdl;
    p_session->sessionId = session_id;
    p_session->session_handle_q = p_session_handle_q;
    p_session->out_buf_q = p_out_buf_q;

    qdata.p = p_session;
    mm_jpeg_queue_enq(p_session_handle_q, qdata);

    if (OMX_FALSE == p_session->config) {
      rc = mm_jpeg_session_configure(p_session);
      if (rc) {
        CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
        return rc;
      }
      p_session->config = OMX_TRUE;
    }
    p_session->num_omx_sessions = num_omx_sessions;

    CDBG("%s:%d] session id %x", __func__, __LINE__, session_id);
  }

  // Queue the output buf indexes
  for (i = 0; i < p_params->num_dst_bufs; i++) {
    qdata.u32 = i + 1;
    mm_jpeg_queue_enq(p_out_buf_q, qdata);
  }

  p_session->meta_enc_key = NULL;
  p_session->meta_enc_keylen = 0;

#ifdef MM_JPEG_READ_META_KEYFILE
  mm_jpeg_read_meta_keyfile(p_session, META_KEYFILE);
#endif

  return rc;
}

/** mm_jpegenc_destroy_job
 *
 *  Arguments:
 *    @p_session: Session obj
 *
 *  Return:
 *       0 for success else failure
 *
 *  Description:
 *       Destroy the job based paramenters
 *
 **/
static int32_t mm_jpegenc_destroy_job(mm_jpeg_job_session_t *p_session)
{
  mm_jpeg_encode_job_t *p_jobparams = &p_session->encode_job;
  int i = 0, rc = 0;

  CDBG_ERROR("%s:%d] Exif entry count %d %d", __func__, __LINE__,
    (int)p_jobparams->exif_info.numOfEntries,
    (int)p_session->exif_count_local);
  for (i = 0; i < p_session->exif_count_local; i++) {
    rc = releaseExifEntry(&p_session->exif_info_local[i]);
    if (rc) {
      CDBG_ERROR("%s:%d] Exif release failed (%d)", __func__, __LINE__, rc);
    }
  }
  p_session->exif_count_local = 0;

  return rc;
}

/** mm_jpeg_session_encode:
 *
 *  Arguments:
 *    @p_session: encode session
 *
 *  Return:
 *       OMX_ERRORTYPE
 *
 *  Description:
 *       Start the encoding
 *
 **/
static void mm_jpegenc_job_done(mm_jpeg_job_session_t *p_session)
{
  mm_jpeg_q_data_t qdata;
  mm_jpeg_obj *my_obj = (mm_jpeg_obj *)p_session->jpeg_obj;
  mm_jpeg_job_q_node_t *node = NULL;

  /*Destroy job related params*/
  mm_jpegenc_destroy_job(p_session);

  /*remove the job*/
  node = mm_jpeg_queue_remove_job_by_job_id(&my_obj->ongoing_job_q,
    p_session->jobId);
  if (node) {
    free(node);
  }
  p_session->encoding = OMX_FALSE;

  // Queue to available sessions
  qdata.p = p_session;
  mm_jpeg_queue_enq(p_session->session_handle_q, qdata);

  if (p_session->auto_out_buf) {
    //Queue out buf index
    qdata.u32 = (uint32_t)(p_session->encode_job.dst_index + 1);
    mm_jpeg_queue_enq(p_session->out_buf_q, qdata);
  }

  /* wake up jobMgr thread to work on new job if there is any */
  cam_sem_post(&my_obj->job_mgr.job_sem);
}

/** mm_jpeg_destroy_session:
 *
 *  Arguments:
 *    @my_obj: jpeg object
 *    @session_id: session index
 *
 *  Return:
 *       0 for success else failure
 *
 *  Description:
 *       Destroy the encoding session
 *
 **/
int32_t mm_jpeg_destroy_session(mm_jpeg_obj *my_obj,
  mm_jpeg_job_session_t *p_session)
{
  mm_jpeg_q_data_t qdata;
  int32_t rc = 0;
  mm_jpeg_job_q_node_t *node = NULL;
  uint32_t session_id = 0;
  mm_jpeg_job_session_t *p_cur_sess;

  if (NULL == p_session) {
    CDBG_ERROR("%s:%d] invalid session", __func__, __LINE__);
    return rc;
  }

  session_id = p_session->sessionId;

  pthread_mutex_lock(&my_obj->job_lock);

  /* abort job if in todo queue */
  CDBG("%s:%d] abort todo jobs", __func__, __LINE__);
  node = mm_jpeg_queue_remove_job_by_session_id(&my_obj->job_mgr.job_queue, session_id);
  while (NULL != node) {
    free(node);
    node = mm_jpeg_queue_remove_job_by_session_id(&my_obj->job_mgr.job_queue, session_id);
  }

  /* abort job if in ongoing queue */
  CDBG("%s:%d] abort ongoing jobs", __func__, __LINE__);
  node = mm_jpeg_queue_remove_job_by_session_id(&my_obj->ongoing_job_q, session_id);
  while (NULL != node) {
    free(node);
    node = mm_jpeg_queue_remove_job_by_session_id(&my_obj->ongoing_job_q, session_id);
  }

  /* abort the current session */
  mm_jpeg_session_abort(p_session);
  mm_jpeg_session_destroy(p_session);

  p_cur_sess = p_session;

  do {
    mm_jpeg_remove_session_idx(my_obj, p_cur_sess->sessionId);
  } while (NULL != (p_cur_sess = p_cur_sess->next_session));


  pthread_mutex_unlock(&my_obj->job_lock);

  while (1) {
    qdata = mm_jpeg_queue_deq(p_session->session_handle_q);
    if (NULL == qdata.p)
      break;
  }
  mm_jpeg_queue_deinit(p_session->session_handle_q);
  free(p_session->session_handle_q);
  p_session->session_handle_q = NULL;

  while (1) {
    qdata = mm_jpeg_queue_deq(p_session->out_buf_q);
    if (0U == qdata.u32)
      break;
  }
  mm_jpeg_queue_deinit(p_session->out_buf_q);
  free(p_session->out_buf_q);
  p_session->out_buf_q = NULL;


  /* wake up jobMgr thread to work on new job if there is any */
  cam_sem_post(&my_obj->job_mgr.job_sem);

  CDBG("%s:%d] X", __func__, __LINE__);


  return rc;
}




/** mm_jpeg_destroy_session:
 *
 *  Arguments:
 *    @my_obj: jpeg object
 *    @session_id: session index
 *
 *  Return:
 *       0 for success else failure
 *
 *  Description:
 *       Destroy the encoding session
 *
 **/
int32_t mm_jpeg_destroy_session_unlocked(mm_jpeg_obj *my_obj,
  mm_jpeg_job_session_t *p_session)
{
  int32_t rc = -1;
  mm_jpeg_job_q_node_t *node = NULL;
  uint32_t session_id = 0;
  if (NULL == p_session) {
    CDBG_ERROR("%s:%d] invalid session", __func__, __LINE__);
    return rc;
  }

  session_id = p_session->sessionId;

  /* abort job if in todo queue */
  CDBG("%s:%d] abort todo jobs", __func__, __LINE__);
  node = mm_jpeg_queue_remove_job_by_session_id(&my_obj->job_mgr.job_queue, session_id);
  while (NULL != node) {
    free(node);
    node = mm_jpeg_queue_remove_job_by_session_id(&my_obj->job_mgr.job_queue, session_id);
  }

  /* abort job if in ongoing queue */
  CDBG("%s:%d] abort ongoing jobs", __func__, __LINE__);
  node = mm_jpeg_queue_remove_job_by_session_id(&my_obj->ongoing_job_q, session_id);
  while (NULL != node) {
    free(node);
    node = mm_jpeg_queue_remove_job_by_session_id(&my_obj->ongoing_job_q, session_id);
  }

  /* abort the current session */
  mm_jpeg_session_abort(p_session);
  mm_jpeg_remove_session_idx(my_obj, session_id);

  return rc;
}

/** mm_jpeg_destroy_session:
 *
 *  Arguments:
 *    @my_obj: jpeg object
 *    @session_id: session index
 *
 *  Return:
 *       0 for success else failure
 *
 *  Description:
 *       Destroy the encoding session
 *
 **/
int32_t mm_jpeg_destroy_session_by_id(mm_jpeg_obj *my_obj, uint32_t session_id)
{
  mm_jpeg_job_session_t *p_session = mm_jpeg_get_session(my_obj, session_id);

  return mm_jpeg_destroy_session(my_obj, p_session);
}



/** mm_jpeg_close:
 *
 *  Arguments:
 *    @my_obj: jpeg object
 *    @client_hdl: client handle
 *
 *  Return:
 *       0 for success else failure
 *
 *  Description:
 *       Close the jpeg client
 *
 **/
int32_t mm_jpeg_close(mm_jpeg_obj *my_obj, uint32_t client_hdl)
{
  int32_t rc = -1;
  uint8_t clnt_idx = 0;
  int i = 0;

  /* check if valid client */
  clnt_idx = mm_jpeg_util_get_index_by_handler(client_hdl);
  if (clnt_idx >= MAX_JPEG_CLIENT_NUM) {
    CDBG_ERROR("%s: invalid client with handler (%d)", __func__, client_hdl);
    return rc;
  }

  CDBG("%s:%d] E", __func__, __LINE__);

  /* abort all jobs from the client */
  pthread_mutex_lock(&my_obj->job_lock);

  CDBG("%s:%d] ", __func__, __LINE__);

  for (i = 0; i < MM_JPEG_MAX_SESSION; i++) {
    if (OMX_TRUE == my_obj->clnt_mgr[clnt_idx].session[i].active)
      mm_jpeg_destroy_session_unlocked(my_obj,
        &my_obj->clnt_mgr[clnt_idx].session[i]);
  }

  CDBG("%s:%d] ", __func__, __LINE__);

#ifdef LOAD_ADSP_RPC_LIB
  if (NULL != my_obj->adsprpc_lib_handle) {
    dlclose(my_obj->adsprpc_lib_handle);
    my_obj->adsprpc_lib_handle = NULL;
  }
#endif

  pthread_mutex_unlock(&my_obj->job_lock);
  CDBG("%s:%d] ", __func__, __LINE__);

  /* invalidate client session */
  pthread_mutex_destroy(&my_obj->clnt_mgr[clnt_idx].lock);
  memset(&my_obj->clnt_mgr[clnt_idx], 0, sizeof(mm_jpeg_client_t));

  rc = 0;
  CDBG("%s:%d] X", __func__, __LINE__);
  return rc;
}

OMX_ERRORTYPE mm_jpeg_ebd(OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE *pBuffer)
{
  mm_jpeg_job_session_t *p_session = (mm_jpeg_job_session_t *) pAppData;

  CDBG("%s:%d] count %d ", __func__, __LINE__, p_session->ebd_count);
  pthread_mutex_lock(&p_session->lock);
  p_session->ebd_count++;
  pthread_mutex_unlock(&p_session->lock);
  return 0;
}

OMX_ERRORTYPE mm_jpeg_fbd(OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE *pBuffer)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  mm_jpeg_job_session_t *p_session = (mm_jpeg_job_session_t *) pAppData;
  mm_jpeg_output_t output_buf;
  CDBG("%s:%d] count %d ", __func__, __LINE__, p_session->fbd_count);
  CDBG_HIGH("[KPI Perf] : PROFILE_JPEG_FBD");

  pthread_mutex_lock(&p_session->lock);

  if (MM_JPEG_ABORT_NONE != p_session->abort_state) {
    pthread_mutex_unlock(&p_session->lock);
    return ret;
  }

  p_session->fbd_count++;
  if (NULL != p_session->params.jpeg_cb) {

    p_session->job_status = JPEG_JOB_STATUS_DONE;
    output_buf.buf_filled_len = (uint32_t)pBuffer->nFilledLen;
    output_buf.buf_vaddr = pBuffer->pBuffer;
    output_buf.fd = 0;
    CDBG("%s:%d] send jpeg callback %d buf 0x%p len %u JobID %u",
      __func__, __LINE__,
      p_session->job_status, pBuffer->pBuffer,
      (unsigned int)pBuffer->nFilledLen, p_session->jobId);
    p_session->params.jpeg_cb(p_session->job_status,
      p_session->client_hdl,
      p_session->jobId,
      &output_buf,
      p_session->params.userdata);

    mm_jpegenc_job_done(p_session);

  }
  pthread_mutex_unlock(&p_session->lock);
  CDBG("%s:%d] ", __func__, __LINE__);

  return ret;
}



OMX_ERRORTYPE mm_jpeg_event_handler(OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_EVENTTYPE eEvent,
  OMX_U32 nData1,
  OMX_U32 nData2,
  OMX_PTR pEventData)
{
  mm_jpeg_job_session_t *p_session = (mm_jpeg_job_session_t *) pAppData;

  CDBG("%s:%d] %d %d %d state %d", __func__, __LINE__, eEvent, (int)nData1,
    (int)nData2, p_session->abort_state);

  pthread_mutex_lock(&p_session->lock);

  if (MM_JPEG_ABORT_INIT == p_session->abort_state) {
    p_session->abort_state = MM_JPEG_ABORT_DONE;
    pthread_cond_signal(&p_session->cond);
    pthread_mutex_unlock(&p_session->lock);
    return OMX_ErrorNone;
  }

  if (eEvent == OMX_EventError) {
    p_session->error_flag = nData2;
    if (p_session->encoding == OMX_TRUE) {
      CDBG("%s:%d] Error during encoding", __func__, __LINE__);

      /* send jpeg callback */
      if (NULL != p_session->params.jpeg_cb) {
        p_session->job_status = JPEG_JOB_STATUS_ERROR;
        CDBG("%s:%d] send jpeg error callback %d", __func__, __LINE__,
          p_session->job_status);
        p_session->params.jpeg_cb(p_session->job_status,
          p_session->client_hdl,
          p_session->jobId,
          NULL,
          p_session->params.userdata);
      }

      /* remove from ready queue */
      mm_jpegenc_job_done(p_session);
    }
    pthread_cond_signal(&p_session->cond);
  } else if (eEvent == OMX_EventCmdComplete) {
    if (p_session->state_change_pending == OMX_TRUE) {
      p_session->state_change_pending = OMX_FALSE;
      pthread_cond_signal(&p_session->cond);
    }
  }

  pthread_mutex_unlock(&p_session->lock);
  CDBG("%s:%d]", __func__, __LINE__);
  return OMX_ErrorNone;
}



/* remove the first job from the queue with matching client handle */
mm_jpeg_job_q_node_t* mm_jpeg_queue_remove_job_by_client_id(
  mm_jpeg_queue_t* queue, uint32_t client_hdl)
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
    data = (mm_jpeg_job_q_node_t *)node->data.p;

    if (data && (data->enc_info.client_handle == client_hdl)) {
      CDBG_ERROR("%s:%d] found matching client handle", __func__, __LINE__);
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

/* remove the first job from the queue with matching session id */
mm_jpeg_job_q_node_t* mm_jpeg_queue_remove_job_by_session_id(
  mm_jpeg_queue_t* queue, uint32_t session_id)
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
    data = (mm_jpeg_job_q_node_t *)node->data.p;

    if (data && (data->enc_info.encode_job.session_id == session_id)) {
      CDBG_ERROR("%s:%d] found matching session id", __func__, __LINE__);
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
mm_jpeg_job_q_node_t* mm_jpeg_queue_remove_job_by_job_id(
  mm_jpeg_queue_t* queue, uint32_t job_id)
{
  mm_jpeg_q_node_t* node = NULL;
  mm_jpeg_job_q_node_t* data = NULL;
  mm_jpeg_job_q_node_t* job_node = NULL;
  struct cam_list *head = NULL;
  struct cam_list *pos = NULL;
  uint32_t lq_job_id;

  pthread_mutex_lock(&queue->lock);
  head = &queue->head.list;
  pos = head->next;
  while(pos != head) {
    node = member_of(pos, mm_jpeg_q_node_t, list);
    data = (mm_jpeg_job_q_node_t *)node->data.p;

    if (data != NULL) {
        if (data->type == MM_JPEG_CMD_TYPE_DECODE_JOB) {
            lq_job_id = data->dec_info.job_id;
        } else {
            lq_job_id = data->enc_info.job_id;
        }

      if (lq_job_id == job_id) {
        CDBG_ERROR("%s:%d] found matching job id", __func__, __LINE__);
        job_node = data;
        cam_list_del_node(&node->list);
        queue->size--;
        free(node);
        break;
      }
    }
    pos = pos->next;
  }

  pthread_mutex_unlock(&queue->lock);

  return job_node;
}

/* remove job from the queue with matching job id */
mm_jpeg_job_q_node_t* mm_jpeg_queue_remove_job_unlk(
  mm_jpeg_queue_t* queue, uint32_t job_id)
{
  mm_jpeg_q_node_t* node = NULL;
  mm_jpeg_job_q_node_t* data = NULL;
  mm_jpeg_job_q_node_t* job_node = NULL;
  struct cam_list *head = NULL;
  struct cam_list *pos = NULL;

  head = &queue->head.list;
  pos = head->next;
  while(pos != head) {
    node = member_of(pos, mm_jpeg_q_node_t, list);
    data = (mm_jpeg_job_q_node_t *)node->data.p;

    if (data && (data->enc_info.job_id == job_id)) {
      job_node = data;
      cam_list_del_node(&node->list);
      queue->size--;
      free(node);
      break;
    }
    pos = pos->next;
  }

  return job_node;
}
