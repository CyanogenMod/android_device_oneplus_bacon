/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef MM_JPEG_INLINES_H_
#define MM_JPEG_INLINES_H_

#include "mm_jpeg.h"

/** mm_jpeg_get_session:
 *
 *  Arguments:
 *    @my_obj: jpeg object
 *    @client_idx: client index
 *
 *  Return:
 *       job index
 *
 *  Description:
 *       Get job index by client id
 *
 **/
static inline mm_jpeg_job_session_t *mm_jpeg_get_session(mm_jpeg_obj *my_obj, uint32_t job_id)
{
  mm_jpeg_job_session_t *p_session = NULL;
  int client_idx =  GET_CLIENT_IDX(job_id);
  int session_idx= GET_SESSION_IDX(job_id);

  CDBG("%s:%d] client_idx %d session_idx %d", __func__, __LINE__,
    client_idx, session_idx);
  if ((session_idx >= MM_JPEG_MAX_SESSION) ||
    (client_idx >= MAX_JPEG_CLIENT_NUM)) {
    CDBG_ERROR("%s:%d] invalid job id %x", __func__, __LINE__,
      job_id);
    return NULL;
  }
  pthread_mutex_lock(&my_obj->clnt_mgr[client_idx].lock);
  p_session = &my_obj->clnt_mgr[client_idx].session[session_idx];
  pthread_mutex_unlock(&my_obj->clnt_mgr[client_idx].lock);
  return p_session;
}

/** mm_jpeg_get_job_idx:
 *
 *  Arguments:
 *    @my_obj: jpeg object
 *    @client_idx: client index
 *
 *  Return:
 *       job index
 *
 *  Description:
 *       Get job index by client id
 *
 **/
static inline int mm_jpeg_get_new_session_idx(mm_jpeg_obj *my_obj, int client_idx,
  mm_jpeg_job_session_t **pp_session)
{
  int i = 0;
  int index = -1;
  for (i = 0; i < MM_JPEG_MAX_SESSION; i++) {
    pthread_mutex_lock(&my_obj->clnt_mgr[client_idx].lock);
    if (!my_obj->clnt_mgr[client_idx].session[i].active) {
      *pp_session = &my_obj->clnt_mgr[client_idx].session[i];
      my_obj->clnt_mgr[client_idx].session[i].active = OMX_TRUE;
      index = i;
      pthread_mutex_unlock(&my_obj->clnt_mgr[client_idx].lock);
      break;
    }
    pthread_mutex_unlock(&my_obj->clnt_mgr[client_idx].lock);
  }
  return index;
}

/** mm_jpeg_get_job_idx:
 *
 *  Arguments:
 *    @my_obj: jpeg object
 *    @client_idx: client index
 *
 *  Return:
 *       job index
 *
 *  Description:
 *       Get job index by client id
 *
 **/
static inline void mm_jpeg_remove_session_idx(mm_jpeg_obj *my_obj, uint32_t job_id)
{
  int client_idx =  GET_CLIENT_IDX(job_id);
  int session_idx= GET_SESSION_IDX(job_id);
  CDBG("%s:%d] client_idx %d session_idx %d", __func__, __LINE__,
    client_idx, session_idx);
  pthread_mutex_lock(&my_obj->clnt_mgr[client_idx].lock);
  my_obj->clnt_mgr[client_idx].session[session_idx].active = OMX_FALSE;
  pthread_mutex_unlock(&my_obj->clnt_mgr[client_idx].lock);
}



#endif /* MM_JPEG_INLINES_H_ */
