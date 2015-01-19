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

#include "mm_jpeg_dbg.h"
#include "mm_jpeg_interface.h"
#include "mm_jpeg.h"

static pthread_mutex_t g_dec_intf_lock = PTHREAD_MUTEX_INITIALIZER;

static mm_jpeg_obj* g_jpegdec_obj = NULL;

/** mm_jpeg_intf_start_job:
 *
 *  Arguments:
 *    @client_hdl: client handle
 *    @job: jpeg job object
 *    @jobId: job id
 *
 *  Return:
 *       0 success, failure otherwise
 *
 *  Description:
 *       start the jpeg job
 *
 **/
static int32_t mm_jpegdec_intf_start_job(mm_jpeg_job_t* job, uint32_t* job_id)
{
  int32_t rc = -1;

  if (NULL == job ||
    NULL == job_id) {
    CDBG_ERROR("%s:%d] invalid parameters for job or jobId", __func__, __LINE__);
    return rc;
  }

  pthread_mutex_lock(&g_dec_intf_lock);
  if (NULL == g_jpegdec_obj) {
    /* mm_jpeg obj not exists, return error */
    CDBG_ERROR("%s:%d] mm_jpeg is not opened yet", __func__, __LINE__);
    pthread_mutex_unlock(&g_dec_intf_lock);
    return rc;
  }
  rc = mm_jpegdec_start_decode_job(g_jpegdec_obj, job, job_id);
  pthread_mutex_unlock(&g_dec_intf_lock);
  return rc;
}

/** mm_jpeg_intf_create_session:
 *
 *  Arguments:
 *    @client_hdl: client handle
 *    @p_params: encode parameters
 *    @p_session_id: session id
 *
 *  Return:
 *       0 success, failure otherwise
 *
 *  Description:
 *       Create new jpeg session
 *
 **/
static int32_t mm_jpegdec_intf_create_session(uint32_t client_hdl,
    mm_jpeg_decode_params_t *p_params,
    uint32_t *p_session_id)
{
  int32_t rc = -1;

  if (0 == client_hdl || NULL == p_params || NULL == p_session_id) {
    CDBG_ERROR("%s:%d] invalid client_hdl or jobId", __func__, __LINE__);
    return rc;
  }

  pthread_mutex_lock(&g_dec_intf_lock);
  if (NULL == g_jpegdec_obj) {
    /* mm_jpeg obj not exists, return error */
    CDBG_ERROR("%s:%d] mm_jpeg is not opened yet", __func__, __LINE__);
    pthread_mutex_unlock(&g_dec_intf_lock);
    return rc;
  }

  rc = mm_jpegdec_create_session(g_jpegdec_obj, client_hdl, p_params, p_session_id);
  pthread_mutex_unlock(&g_dec_intf_lock);
  return rc;
}

/** mm_jpeg_intf_destroy_session:
 *
 *  Arguments:
 *    @session_id: session id
 *
 *  Return:
 *       0 success, failure otherwise
 *
 *  Description:
 *       Destroy jpeg session
 *
 **/
static int32_t mm_jpegdec_intf_destroy_session(uint32_t session_id)
{
  int32_t rc = -1;

  if (0 == session_id) {
    CDBG_ERROR("%s:%d] invalid client_hdl or jobId", __func__, __LINE__);
    return rc;
  }

  pthread_mutex_lock(&g_dec_intf_lock);
  if (NULL == g_jpegdec_obj) {
    /* mm_jpeg obj not exists, return error */
    CDBG_ERROR("%s:%d] mm_jpeg is not opened yet", __func__, __LINE__);
    pthread_mutex_unlock(&g_dec_intf_lock);
    return rc;
  }

  rc = mm_jpegdec_destroy_session_by_id(g_jpegdec_obj, session_id);
  pthread_mutex_unlock(&g_dec_intf_lock);
  return rc;
}

/** mm_jpegdec_intf_abort_job:
 *
 *  Arguments:
 *    @jobId: job id
 *
 *  Return:
 *       0 success, failure otherwise
 *
 *  Description:
 *       Abort the jpeg job
 *
 **/
static int32_t mm_jpegdec_intf_abort_job(uint32_t job_id)
{
  int32_t rc = -1;

  if (0 == job_id) {
    CDBG_ERROR("%s:%d] invalid jobId", __func__, __LINE__);
    return rc;
  }

  pthread_mutex_lock(&g_dec_intf_lock);
  if (NULL == g_jpegdec_obj) {
    /* mm_jpeg obj not exists, return error */
    CDBG_ERROR("%s:%d] mm_jpeg is not opened yet", __func__, __LINE__);
    pthread_mutex_unlock(&g_dec_intf_lock);
    return rc;
  }

  rc = mm_jpegdec_abort_job(g_jpegdec_obj, job_id);
  pthread_mutex_unlock(&g_dec_intf_lock);
  return rc;
}

/** mm_jpeg_intf_close:
 *
 *  Arguments:
 *    @client_hdl: client handle
 *
 *  Return:
 *       0 success, failure otherwise
 *
 *  Description:
 *       Close the jpeg job
 *
 **/
static int32_t mm_jpegdec_intf_close(uint32_t client_hdl)
{
  int32_t rc = -1;

  if (0 == client_hdl) {
    CDBG_ERROR("%s:%d] invalid client_hdl", __func__, __LINE__);
    return rc;
  }

  pthread_mutex_lock(&g_dec_intf_lock);
  if (NULL == g_jpegdec_obj) {
    /* mm_jpeg obj not exists, return error */
    CDBG_ERROR("%s:%d] mm_jpeg is not opened yet", __func__, __LINE__);
    pthread_mutex_unlock(&g_dec_intf_lock);
    return rc;
  }

  rc = mm_jpeg_close(g_jpegdec_obj, client_hdl);
  g_jpegdec_obj->num_clients--;
  if(0 == rc) {
    if (0 == g_jpegdec_obj->num_clients) {
      /* No client, close jpeg internally */
      rc = mm_jpegdec_deinit(g_jpegdec_obj);
      free(g_jpegdec_obj);
      g_jpegdec_obj = NULL;
    }
  }

  pthread_mutex_unlock(&g_dec_intf_lock);
  return rc;
}



/** jpegdec_open:
 *
 *  Arguments:
 *    @ops: ops table pointer
 *
 *  Return:
 *       0 failure, success otherwise
 *
 *  Description:
 *       Open a jpeg client
 *
 **/
uint32_t jpegdec_open(mm_jpegdec_ops_t *ops)
{
  int32_t rc = 0;
  uint32_t clnt_hdl = 0;
  mm_jpeg_obj* jpeg_obj = NULL;

  pthread_mutex_lock(&g_dec_intf_lock);
  /* first time open */
  if(NULL == g_jpegdec_obj) {
    jpeg_obj = (mm_jpeg_obj *)malloc(sizeof(mm_jpeg_obj));
    if(NULL == jpeg_obj) {
      CDBG_ERROR("%s:%d] no mem", __func__, __LINE__);
      pthread_mutex_unlock(&g_dec_intf_lock);
      return clnt_hdl;
    }

    /* initialize jpeg obj */
    memset(jpeg_obj, 0, sizeof(mm_jpeg_obj));
    rc = mm_jpegdec_init(jpeg_obj);
    if(0 != rc) {
      CDBG_ERROR("%s:%d] mm_jpeg_init err = %d", __func__, __LINE__, rc);
      free(jpeg_obj);
      pthread_mutex_unlock(&g_dec_intf_lock);
      return clnt_hdl;
    }

    /* remember in global variable */
    g_jpegdec_obj = jpeg_obj;
  }

  /* open new client */
  clnt_hdl = mm_jpeg_new_client(g_jpegdec_obj);
  if (clnt_hdl > 0) {
    /* valid client */
    if (NULL != ops) {
      /* fill in ops tbl if ptr not NULL */
      ops->start_job = mm_jpegdec_intf_start_job;
      ops->abort_job = mm_jpegdec_intf_abort_job;
      ops->create_session = mm_jpegdec_intf_create_session;
      ops->destroy_session = mm_jpegdec_intf_destroy_session;
      ops->close = mm_jpegdec_intf_close;
    }
  } else {
    /* failed new client */
    CDBG_ERROR("%s:%d] mm_jpeg_new_client failed", __func__, __LINE__);

    if (0 == g_jpegdec_obj->num_clients) {
      /* no client, close jpeg */
      mm_jpegdec_deinit(g_jpegdec_obj);
      free(g_jpegdec_obj);
      g_jpegdec_obj = NULL;
    }
  }

  pthread_mutex_unlock(&g_dec_intf_lock);
  return clnt_hdl;
}



