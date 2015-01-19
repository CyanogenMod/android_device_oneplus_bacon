/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <fcntl.h>
#include <poll.h>

#include "mm_jpeg_dbg.h"
#include "mm_jpeg_interface.h"
#include "mm_jpeg.h"
#include "mm_jpeg_inlines.h"

OMX_ERRORTYPE mm_jpegdec_ebd(OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE *pBuffer);
OMX_ERRORTYPE mm_jpegdec_fbd(OMX_HANDLETYPE hComponent,
    OMX_PTR pAppData,
    OMX_BUFFERHEADERTYPE* pBuffer);
OMX_ERRORTYPE mm_jpegdec_event_handler(OMX_HANDLETYPE hComponent,
    OMX_PTR pAppData,
    OMX_EVENTTYPE eEvent,
    OMX_U32 nData1,
    OMX_U32 nData2,
    OMX_PTR pEventData);


/** mm_jpegdec_destroy_job
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
static int32_t mm_jpegdec_destroy_job(mm_jpeg_job_session_t *p_session)
{
  int32_t rc = 0;

  return rc;
}

/** mm_jpeg_job_done:
 *
 *  Arguments:
 *    @p_session: decode session
 *
 *  Return:
 *       OMX_ERRORTYPE
 *
 *  Description:
 *       Finalize the job
 *
 **/
static void mm_jpegdec_job_done(mm_jpeg_job_session_t *p_session)
{
  mm_jpeg_obj *my_obj = (mm_jpeg_obj *)p_session->jpeg_obj;
  mm_jpeg_job_q_node_t *node = NULL;

  /*Destroy job related params*/
  mm_jpegdec_destroy_job(p_session);

  /*remove the job*/
  node = mm_jpeg_queue_remove_job_by_job_id(&my_obj->ongoing_job_q,
    p_session->jobId);
  if (node) {
    free(node);
  }
  p_session->encoding = OMX_FALSE;

  /* wake up jobMgr thread to work on new job if there is any */
  cam_sem_post(&my_obj->job_mgr.job_sem);
}


/** mm_jpegdec_session_send_buffers:
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
OMX_ERRORTYPE mm_jpegdec_session_send_buffers(void *data)
{
  uint32_t i = 0;
  mm_jpeg_job_session_t* p_session = (mm_jpeg_job_session_t *)data;
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  QOMX_BUFFER_INFO lbuffer_info;
  mm_jpeg_decode_params_t *p_params = &p_session->dec_params;

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
OMX_ERRORTYPE mm_jpegdec_session_free_buffers(void *data)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  uint32_t i = 0;
  mm_jpeg_job_session_t* p_session = (mm_jpeg_job_session_t *)data;
  mm_jpeg_decode_params_t *p_params = &p_session->dec_params;

  for (i = 0; i < p_params->num_src_bufs; i++) {
    CDBG("%s:%d] Source buffer %d", __func__, __LINE__, i);
    ret = OMX_FreeBuffer(p_session->omx_handle, 0, p_session->p_in_omx_buf[i]);
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

/** mm_jpegdec_session_create:
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
OMX_ERRORTYPE mm_jpegdec_session_create(mm_jpeg_job_session_t* p_session)
{
  OMX_ERRORTYPE rc = OMX_ErrorNone;

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

  p_session->omx_callbacks.EmptyBufferDone = mm_jpegdec_ebd;
  p_session->omx_callbacks.FillBufferDone = mm_jpegdec_fbd;
  p_session->omx_callbacks.EventHandler = mm_jpegdec_event_handler;
  p_session->exif_count_local = 0;

  rc = OMX_GetHandle(&p_session->omx_handle,
    "OMX.qcom.image.jpeg.decoder",
    (void *)p_session,
    &p_session->omx_callbacks);

  if (OMX_ErrorNone != rc) {
    CDBG_ERROR("%s:%d] OMX_GetHandle failed (%d)", __func__, __LINE__, rc);
    return rc;
  }
  return rc;
}

/** mm_jpegdec_session_destroy:
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
void mm_jpegdec_session_destroy(mm_jpeg_job_session_t* p_session)
{
  OMX_ERRORTYPE rc = OMX_ErrorNone;

  CDBG("%s:%d] E", __func__, __LINE__);
  if (NULL == p_session->omx_handle) {
    CDBG_ERROR("%s:%d] invalid handle", __func__, __LINE__);
    return;
  }

  rc = mm_jpeg_session_change_state(p_session, OMX_StateIdle, NULL);
  if (rc) {
    CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
  }

  rc = mm_jpeg_session_change_state(p_session, OMX_StateLoaded,
    mm_jpegdec_session_free_buffers);
  if (rc) {
    CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
  }

  rc = OMX_FreeHandle(p_session->omx_handle);
  if (0 != rc) {
    CDBG_ERROR("%s:%d] OMX_FreeHandle failed (%d)", __func__, __LINE__, rc);
  }
  p_session->omx_handle = NULL;


  pthread_mutex_destroy(&p_session->lock);
  pthread_cond_destroy(&p_session->cond);
  CDBG("%s:%d] X", __func__, __LINE__);
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
OMX_ERRORTYPE mm_jpegdec_session_config_ports(mm_jpeg_job_session_t* p_session)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  mm_jpeg_decode_params_t *p_params = &p_session->dec_params;
  mm_jpeg_decode_job_t *p_jobparams = &p_session->decode_job;

  mm_jpeg_buf_t *p_src_buf =
    &p_params->src_main_buf[p_jobparams->src_index];

  p_session->inputPort.nPortIndex = 0;
  p_session->outputPort.nPortIndex = 1;


  ret = OMX_GetParameter(p_session->omx_handle, OMX_IndexParamPortDefinition,
    &p_session->inputPort);
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
    (OMX_U32)p_jobparams->main_dim.src_dim.width;
  p_session->inputPort.format.image.nFrameHeight =
    (OMX_U32)p_jobparams->main_dim.src_dim.height;
  p_session->inputPort.format.image.nStride =
    p_src_buf->offset.mp[0].stride;
  p_session->inputPort.format.image.nSliceHeight =
    (OMX_U32)p_src_buf->offset.mp[0].scanline;
  p_session->inputPort.format.image.eColorFormat =
    map_jpeg_format(p_params->color_format);
  p_session->inputPort.nBufferSize =
    p_params->src_main_buf[p_jobparams->src_index].buf_size;
  p_session->inputPort.nBufferCountActual = (OMX_U32)p_params->num_src_bufs;
  ret = OMX_SetParameter(p_session->omx_handle, OMX_IndexParamPortDefinition,
    &p_session->inputPort);
  if (ret) {
    CDBG_ERROR("%s:%d] failed", __func__, __LINE__);
    return ret;
  }

  return ret;
}


/** mm_jpegdec_session_config_main:
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
OMX_ERRORTYPE mm_jpegdec_session_config_main(mm_jpeg_job_session_t *p_session)
{
  OMX_ERRORTYPE rc = OMX_ErrorNone;

  /* config port */
  CDBG("%s:%d] config port", __func__, __LINE__);
  rc = mm_jpegdec_session_config_ports(p_session);
  if (OMX_ErrorNone != rc) {
    CDBG_ERROR("%s: config port failed", __func__);
    return rc;
  }


  /* TODO: config crop */

  return rc;
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
static OMX_ERRORTYPE mm_jpegdec_session_configure(mm_jpeg_job_session_t *p_session)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;

  CDBG("%s:%d] E ", __func__, __LINE__);

  MM_JPEG_CHK_ABORT(p_session, ret, error);

  /* config main img */
  ret = mm_jpegdec_session_config_main(p_session);
  if (OMX_ErrorNone != ret) {
    CDBG_ERROR("%s:%d] config main img failed", __func__, __LINE__);
    goto error;
  }

  /* TODO: common config (if needed) */

  ret = mm_jpeg_session_change_state(p_session, OMX_StateIdle,
    mm_jpegdec_session_send_buffers);
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

static OMX_ERRORTYPE mm_jpeg_session_port_enable(
    mm_jpeg_job_session_t *p_session,
    OMX_U32 nPortIndex,
    OMX_BOOL wait)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  OMX_EVENTTYPE lEvent;

  pthread_mutex_lock(&p_session->lock);
  p_session->event_pending = OMX_TRUE;
  pthread_mutex_unlock(&p_session->lock);

  ret = OMX_SendCommand(p_session->omx_handle, OMX_CommandPortEnable,
      nPortIndex, NULL);

  if (ret) {
    CDBG_ERROR("%s:%d] failed", __func__, __LINE__);
    return ret;
  }

  if (wait == OMX_TRUE) {
    // Wait for cmd complete
    pthread_mutex_lock(&p_session->lock);
    if (p_session->event_pending == OMX_TRUE) {
      CDBG("%s:%d] before wait", __func__, __LINE__);
      pthread_cond_wait(&p_session->cond, &p_session->lock);
      lEvent = p_session->omxEvent;
      CDBG("%s:%d] after wait", __func__, __LINE__);
    }
    lEvent = p_session->omxEvent;
    pthread_mutex_unlock(&p_session->lock);

    if (lEvent != OMX_EventCmdComplete) {
      CDBG("%s:%d] Unexpected event %d", __func__, __LINE__,lEvent);
      return OMX_ErrorUndefined;
    }
  }
  return OMX_ErrorNone;
}

static OMX_ERRORTYPE mm_jpeg_session_port_disable(
    mm_jpeg_job_session_t *p_session,
    OMX_U32 nPortIndex,
    OMX_BOOL wait)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  OMX_EVENTTYPE lEvent;

  pthread_mutex_lock(&p_session->lock);
  p_session->event_pending = OMX_TRUE;
  pthread_mutex_unlock(&p_session->lock);

  ret = OMX_SendCommand(p_session->omx_handle, OMX_CommandPortDisable,
      nPortIndex, NULL);

  if (ret) {
    CDBG_ERROR("%s:%d] failed", __func__, __LINE__);
    return ret;
  }
  if (wait == OMX_TRUE) {
    // Wait for cmd complete
    pthread_mutex_lock(&p_session->lock);
    if (p_session->event_pending == OMX_TRUE) {
      CDBG("%s:%d] before wait", __func__, __LINE__);
      pthread_cond_wait(&p_session->cond, &p_session->lock);

      CDBG("%s:%d] after wait", __func__, __LINE__);
    }
    lEvent = p_session->omxEvent;
    pthread_mutex_unlock(&p_session->lock);

    if (lEvent != OMX_EventCmdComplete) {
      CDBG("%s:%d] Unexpected event %d", __func__, __LINE__,lEvent);
      return OMX_ErrorUndefined;
    }
  }
  return OMX_ErrorNone;
}


/** mm_jpegdec_session_decode:
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
static OMX_ERRORTYPE mm_jpegdec_session_decode(mm_jpeg_job_session_t *p_session)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  mm_jpeg_decode_params_t *p_params = &p_session->dec_params;
  mm_jpeg_decode_job_t *p_jobparams = &p_session->decode_job;
  OMX_EVENTTYPE lEvent;
  uint32_t i;
  QOMX_BUFFER_INFO lbuffer_info;

  pthread_mutex_lock(&p_session->lock);
  p_session->abort_state = MM_JPEG_ABORT_NONE;
  p_session->encoding = OMX_FALSE;
  pthread_mutex_unlock(&p_session->lock);

  if (OMX_FALSE == p_session->config) {
    ret = mm_jpegdec_session_configure(p_session);
    if (ret) {
      CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
      goto error;
    }
    p_session->config = OMX_TRUE;
  }

  pthread_mutex_lock(&p_session->lock);
  p_session->encoding = OMX_TRUE;
  pthread_mutex_unlock(&p_session->lock);

  MM_JPEG_CHK_ABORT(p_session, ret, error);

  p_session->event_pending = OMX_TRUE;

  ret = OMX_EmptyThisBuffer(p_session->omx_handle,
    p_session->p_in_omx_buf[p_jobparams->src_index]);
  if (ret) {
    CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
    goto error;
  }

  // Wait for port settings changed
  pthread_mutex_lock(&p_session->lock);
  if (p_session->event_pending == OMX_TRUE) {
    CDBG("%s:%d] before wait", __func__, __LINE__);
    pthread_cond_wait(&p_session->cond, &p_session->lock);
  }
  lEvent = p_session->omxEvent;
  CDBG("%s:%d] after wait", __func__, __LINE__);
  pthread_mutex_unlock(&p_session->lock);

  if (lEvent != OMX_EventPortSettingsChanged) {
    CDBG("%s:%d] Unexpected event %d", __func__, __LINE__,lEvent);
    goto error;
  }

  // Disable output port (wait)
  mm_jpeg_session_port_disable(p_session,
      p_session->outputPort.nPortIndex,
      OMX_TRUE);

  // Get port definition
  ret = OMX_GetParameter(p_session->omx_handle, OMX_IndexParamPortDefinition,
      &p_session->outputPort);
  if (ret) {
    CDBG_ERROR("%s:%d] failed", __func__, __LINE__);
    return ret;
  }

  // Set port definition
  p_session->outputPort.format.image.nFrameWidth =
    (OMX_U32)p_jobparams->main_dim.dst_dim.width;
  p_session->outputPort.format.image.nFrameHeight =
    (OMX_U32)p_jobparams->main_dim.dst_dim.height;
  p_session->outputPort.format.image.eColorFormat =
    map_jpeg_format(p_params->color_format);

  p_session->outputPort.nBufferSize =
     p_params->dest_buf[p_jobparams->dst_index].buf_size;
   p_session->outputPort.nBufferCountActual = (OMX_U32)p_params->num_dst_bufs;

   p_session->outputPort.format.image.nSliceHeight =
       (OMX_U32)
       p_params->dest_buf[p_jobparams->dst_index].offset.mp[0].scanline;
   p_session->outputPort.format.image.nStride =
       p_params->dest_buf[p_jobparams->dst_index].offset.mp[0].stride;

   ret = OMX_SetParameter(p_session->omx_handle, OMX_IndexParamPortDefinition,
     &p_session->outputPort);
   if (ret) {
     CDBG_ERROR("%s:%d] failed", __func__, __LINE__);
     return ret;
   }

  // Enable port (no wait)
  mm_jpeg_session_port_enable(p_session,
      p_session->outputPort.nPortIndex,
      OMX_FALSE);

  memset(&lbuffer_info, 0x0, sizeof(QOMX_BUFFER_INFO));
  // Use buffers
  for (i = 0; i < p_params->num_dst_bufs; i++) {
    lbuffer_info.fd = (OMX_U32)p_params->dest_buf[i].fd;
    CDBG("%s:%d] Dest buffer %d", __func__, __LINE__, (unsigned int)i);
    ret = OMX_UseBuffer(p_session->omx_handle, &(p_session->p_out_omx_buf[i]),
        1, &lbuffer_info, p_params->dest_buf[i].buf_size,
        p_params->dest_buf[i].buf_vaddr);
    if (ret) {
      CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
      return ret;
    }
  }

  // Wait for port enable completion
  pthread_mutex_lock(&p_session->lock);
  if (p_session->event_pending == OMX_TRUE) {
    CDBG("%s:%d] before wait", __func__, __LINE__);
    pthread_cond_wait(&p_session->cond, &p_session->lock);
    lEvent = p_session->omxEvent;
    CDBG("%s:%d] after wait", __func__, __LINE__);
  }
  lEvent = p_session->omxEvent;
  pthread_mutex_unlock(&p_session->lock);

  if (lEvent != OMX_EventCmdComplete) {
    CDBG("%s:%d] Unexpected event %d", __func__, __LINE__,lEvent);
    goto error;
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

/** mm_jpegdec_process_decoding_job:
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
int32_t mm_jpegdec_process_decoding_job(mm_jpeg_obj *my_obj, mm_jpeg_job_q_node_t* job_node)
{
  mm_jpeg_q_data_t qdata;
  int32_t rc = 0;
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  mm_jpeg_job_session_t *p_session = NULL;

  /* check if valid session */
  p_session = mm_jpeg_get_session(my_obj, job_node->dec_info.job_id);
  if (NULL == p_session) {
    CDBG_ERROR("%s:%d] invalid job id %x", __func__, __LINE__,
      job_node->dec_info.job_id);
    return -1;
  }

  /* sent encode cmd to OMX, queue job into ongoing queue */
  qdata.p = job_node;
  rc = mm_jpeg_queue_enq(&my_obj->ongoing_job_q, qdata);
  if (rc) {
    CDBG_ERROR("%s:%d] jpeg enqueue failed %d",
      __func__, __LINE__, ret);
    goto error;
  }

  p_session->decode_job = job_node->dec_info.decode_job;
  p_session->jobId = job_node->dec_info.job_id;
  ret = mm_jpegdec_session_decode(p_session);
  if (ret) {
    CDBG_ERROR("%s:%d] encode session failed", __func__, __LINE__);
    goto error;
  }

  CDBG("%s:%d] Success X ", __func__, __LINE__);
  return rc;

error:

  if ((OMX_ErrorNone != ret) &&
    (NULL != p_session->dec_params.jpeg_cb)) {
    p_session->job_status = JPEG_JOB_STATUS_ERROR;
    CDBG("%s:%d] send jpeg error callback %d", __func__, __LINE__,
      p_session->job_status);
    p_session->dec_params.jpeg_cb(p_session->job_status,
      p_session->client_hdl,
      p_session->jobId,
      NULL,
      p_session->dec_params.userdata);
  }

  /*remove the job*/
  mm_jpegdec_job_done(p_session);
  CDBG("%s:%d] Error X ", __func__, __LINE__);

  return rc;
}

/** mm_jpeg_start_decode_job:
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
int32_t mm_jpegdec_start_decode_job(mm_jpeg_obj *my_obj,
  mm_jpeg_job_t *job,
  uint32_t *job_id)
{
  mm_jpeg_q_data_t qdata;
  int32_t rc = -1;
  uint8_t session_idx = 0;
  uint8_t client_idx = 0;
  mm_jpeg_job_q_node_t* node = NULL;
  mm_jpeg_job_session_t *p_session = NULL;
  mm_jpeg_decode_job_t *p_jobparams  = &job->decode_job;

  *job_id = 0;

  /* check if valid session */
  session_idx = GET_SESSION_IDX(p_jobparams->session_id);
  client_idx = GET_CLIENT_IDX(p_jobparams->session_id);
  CDBG("%s:%d] session_idx %d client idx %d", __func__, __LINE__,
    session_idx, client_idx);

  if ((session_idx >= MM_JPEG_MAX_SESSION) ||
    (client_idx >= MAX_JPEG_CLIENT_NUM)) {
    CDBG_ERROR("%s:%d] invalid session id %x", __func__, __LINE__,
      job->decode_job.session_id);
    return rc;
  }

  p_session = &my_obj->clnt_mgr[client_idx].session[session_idx];
  if (OMX_FALSE == p_session->active) {
    CDBG_ERROR("%s:%d] session not active %x", __func__, __LINE__,
      job->decode_job.session_id);
    return rc;
  }

  if ((p_jobparams->src_index >= (int32_t)p_session->dec_params.num_src_bufs) ||
    (p_jobparams->dst_index >= (int32_t)p_session->dec_params.num_dst_bufs)) {
    CDBG_ERROR("%s:%d] invalid buffer indices", __func__, __LINE__);
    return rc;
  }

  /* enqueue new job into todo job queue */
  node = (mm_jpeg_job_q_node_t *)malloc(sizeof(mm_jpeg_job_q_node_t));
  if (NULL == node) {
    CDBG_ERROR("%s: No memory for mm_jpeg_job_q_node_t", __func__);
    return -1;
  }

  *job_id = job->decode_job.session_id |
    ((p_session->job_hist++ % JOB_HIST_MAX) << 16);

  memset(node, 0, sizeof(mm_jpeg_job_q_node_t));
  node->dec_info.decode_job = job->decode_job;
  node->dec_info.job_id = *job_id;
  node->dec_info.client_handle = p_session->client_hdl;
  node->type = MM_JPEG_CMD_TYPE_DECODE_JOB;

  qdata.p = node;
  rc = mm_jpeg_queue_enq(&my_obj->job_mgr.job_queue, qdata);
  if (0 == rc) {
    cam_sem_post(&my_obj->job_mgr.job_sem);
  }

  return rc;
}

/** mm_jpegdec_create_session:
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
int32_t mm_jpegdec_create_session(mm_jpeg_obj *my_obj,
  uint32_t client_hdl,
  mm_jpeg_decode_params_t *p_params,
  uint32_t* p_session_id)
{
  int32_t rc = 0;
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  uint8_t clnt_idx = 0;
  int session_idx = -1;
  mm_jpeg_job_session_t *p_session = NULL;
  *p_session_id = 0;

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

  session_idx = mm_jpeg_get_new_session_idx(my_obj, clnt_idx, &p_session);
  if (session_idx < 0) {
    CDBG_ERROR("%s:%d] invalid session id (%d)", __func__, __LINE__, session_idx);
    return rc;
  }

  ret = mm_jpegdec_session_create(p_session);
  if (OMX_ErrorNone != ret) {
    p_session->active = OMX_FALSE;
    CDBG_ERROR("%s:%d] jpeg session create failed", __func__, __LINE__);
    return rc;
  }

  *p_session_id = (JOB_ID_MAGICVAL << 24) |
    ((unsigned)session_idx << 8) | clnt_idx;

  /*copy the params*/
  p_session->dec_params = *p_params;
  p_session->client_hdl = client_hdl;
  p_session->sessionId = *p_session_id;
  p_session->jpeg_obj = (void*)my_obj; /* save a ptr to jpeg_obj */
  CDBG("%s:%d] session id %x", __func__, __LINE__, *p_session_id);

  return rc;
}

/** mm_jpegdec_destroy_session:
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
int32_t mm_jpegdec_destroy_session(mm_jpeg_obj *my_obj,
  mm_jpeg_job_session_t *p_session)
{
  int32_t rc = 0;
  mm_jpeg_job_q_node_t *node = NULL;
  uint32_t session_id = p_session->sessionId;

  if (NULL == p_session) {
    CDBG_ERROR("%s:%d] invalid session", __func__, __LINE__);
    return rc;
  }

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
  mm_jpegdec_session_destroy(p_session);
  mm_jpeg_remove_session_idx(my_obj, session_id);
  pthread_mutex_unlock(&my_obj->job_lock);

  /* wake up jobMgr thread to work on new job if there is any */
  cam_sem_post(&my_obj->job_mgr.job_sem);
  CDBG("%s:%d] X", __func__, __LINE__);

  return rc;
}

/** mm_jpegdec_destroy_session_by_id:
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
int32_t mm_jpegdec_destroy_session_by_id(mm_jpeg_obj *my_obj, uint32_t session_id)
{
  mm_jpeg_job_session_t *p_session = mm_jpeg_get_session(my_obj, session_id);
  if (p_session == NULL) {
      CDBG_ERROR("%s: error: mm_jpeg_get_session returned NULL",__func__);
      return -1;
  }
  return mm_jpegdec_destroy_session(my_obj, p_session);
}



OMX_ERRORTYPE mm_jpegdec_ebd(OMX_HANDLETYPE hComponent,
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

OMX_ERRORTYPE mm_jpegdec_fbd(OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_BUFFERHEADERTYPE *pBuffer)
{
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  mm_jpeg_job_session_t *p_session = (mm_jpeg_job_session_t *) pAppData;
  mm_jpeg_output_t output_buf;

  CDBG("%s:%d] count %d ", __func__, __LINE__, p_session->fbd_count);

  pthread_mutex_lock(&p_session->lock);

  if (MM_JPEG_ABORT_NONE != p_session->abort_state) {
    pthread_mutex_unlock(&p_session->lock);
    return ret;
  }

  p_session->fbd_count++;
  if (NULL != p_session->dec_params.jpeg_cb) {
    p_session->job_status = JPEG_JOB_STATUS_DONE;
    output_buf.buf_filled_len = (uint32_t)pBuffer->nFilledLen;
    output_buf.buf_vaddr = pBuffer->pBuffer;
    output_buf.fd = 0;
    CDBG("%s:%d] send jpeg callback %d", __func__, __LINE__,
      p_session->job_status);
    p_session->dec_params.jpeg_cb(p_session->job_status,
      p_session->client_hdl,
      p_session->jobId,
      &output_buf,
      p_session->dec_params.userdata);

    /* remove from ready queue */
    mm_jpegdec_job_done(p_session);
  }
  pthread_mutex_unlock(&p_session->lock);
  CDBG("%s:%d] ", __func__, __LINE__);

  return ret;
}

OMX_ERRORTYPE mm_jpegdec_event_handler(OMX_HANDLETYPE hComponent,
  OMX_PTR pAppData,
  OMX_EVENTTYPE eEvent,
  OMX_U32 nData1,
  OMX_U32 nData2,
  OMX_PTR pEventData)
{
  mm_jpeg_job_session_t *p_session = (mm_jpeg_job_session_t *) pAppData;

  CDBG("%s:%d] %d %d %d state %d", __func__, __LINE__, eEvent, (int)nData1,
    (int)nData2, p_session->abort_state);

  CDBG("%s:%d] AppData=%p ", __func__, __LINE__, pAppData);

  pthread_mutex_lock(&p_session->lock);
  p_session->omxEvent = eEvent;
  if (MM_JPEG_ABORT_INIT == p_session->abort_state) {
    p_session->abort_state = MM_JPEG_ABORT_DONE;
    pthread_cond_signal(&p_session->cond);
    pthread_mutex_unlock(&p_session->lock);
    return OMX_ErrorNone;
  }

  if (eEvent == OMX_EventError) {
    if (p_session->encoding == OMX_TRUE) {
      CDBG("%s:%d] Error during encoding", __func__, __LINE__);

      /* send jpeg callback */
      if (NULL != p_session->dec_params.jpeg_cb) {
        p_session->job_status = JPEG_JOB_STATUS_ERROR;
        CDBG("%s:%d] send jpeg error callback %d", __func__, __LINE__,
          p_session->job_status);
        p_session->dec_params.jpeg_cb(p_session->job_status,
          p_session->client_hdl,
          p_session->jobId,
          NULL,
          p_session->dec_params.userdata);
      }

      /* remove from ready queue */
      mm_jpegdec_job_done(p_session);
    }
    pthread_cond_signal(&p_session->cond);
  } else if (eEvent == OMX_EventCmdComplete) {
    p_session->state_change_pending = OMX_FALSE;
    p_session->event_pending = OMX_FALSE;
    pthread_cond_signal(&p_session->cond);
  }  else if (eEvent == OMX_EventPortSettingsChanged) {
    p_session->event_pending = OMX_FALSE;
    pthread_cond_signal(&p_session->cond);
  }

  pthread_mutex_unlock(&p_session->lock);
  CDBG("%s:%d]", __func__, __LINE__);
  return OMX_ErrorNone;
}

/** mm_jpegdec_abort_job:
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
int32_t mm_jpegdec_abort_job(mm_jpeg_obj *my_obj,
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
    p_session = mm_jpeg_get_session(my_obj, node->dec_info.job_id);
    if (p_session) {
      mm_jpeg_session_abort(p_session);
    } else {
      CDBG_ERROR("%s:%d] Invalid job id 0x%x", __func__, __LINE__,
        node->dec_info.job_id);
    }
    free(node);
    goto abort_done;
  }

abort_done:
  pthread_mutex_unlock(&my_obj->job_lock);

  return rc;
}
/** mm_jpegdec_init:
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
int32_t mm_jpegdec_init(mm_jpeg_obj *my_obj)
{
  int32_t rc = 0;

  /* init locks */
  pthread_mutex_init(&my_obj->job_lock, NULL);

  /* init ongoing job queue */
  rc = mm_jpeg_queue_init(&my_obj->ongoing_job_q);
  if (0 != rc) {
    CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
    return -1;
  }

  /* init job semaphore and launch jobmgr thread */
  CDBG("%s:%d] Launch jobmgr thread rc %d", __func__, __LINE__, rc);
  rc = mm_jpeg_jobmgr_thread_launch(my_obj);
  if (0 != rc) {
    CDBG_ERROR("%s:%d] Error", __func__, __LINE__);
    return -1;
  }

  /* load OMX */
  if (OMX_ErrorNone != OMX_Init()) {
    /* roll back in error case */
    CDBG_ERROR("%s:%d] OMX_Init failed (%d)", __func__, __LINE__, rc);
    mm_jpeg_jobmgr_thread_release(my_obj);
    mm_jpeg_queue_deinit(&my_obj->ongoing_job_q);
    pthread_mutex_destroy(&my_obj->job_lock);
  }

  return rc;
}

/** mm_jpegdec_deinit:
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
int32_t mm_jpegdec_deinit(mm_jpeg_obj *my_obj)
{
  int32_t rc = 0;

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

  /* destroy locks */
  pthread_mutex_destroy(&my_obj->job_lock);

  return rc;
}
