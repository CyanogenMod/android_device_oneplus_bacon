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

#ifndef __MM_QCAMERA_COMMANDS_H__
#define __MM_QCAMERA_COMMANDS_H__

#include "mm_qcamera_socket.h"
#include "mm_qcamera_app.h"

int tuneserver_close_cam(mm_camera_lib_handle *lib_handle);
int tuneserver_stop_cam(mm_camera_lib_handle *lib_handle);
int tuneserver_open_cam(mm_camera_lib_handle *lib_handle);

int tuneserver_initialize_tuningp(void * ctrl, int client_socket_id,
  char *send_buf, uint32_t send_len);
int tuneserver_deinitialize_tuningp(void * ctrl, int client_socket_id,
  char *send_buf, uint32_t send_len);
int tuneserver_process_get_list_cmd(void * ctrl, void *recv_cmd,
  char *send_buf, uint32_t send_len);
int tuneserver_process_misc_cmd(void * ctrl, void *recv_cmd,
  char *send_buf, uint32_t send_len);
int tuneserver_process_get_params_cmd(void * ctrl, void *recv_cmd,
  char *send_buf, uint32_t send_len);
int tuneserver_process_set_params_cmd(void * ctrl, void *recv_cmd,
  char *send_buf, uint32_t send_len);

int tuneserver_initialize_prevtuningp(void * ctrl,
  int pr_client_socket_id, cam_dimension_t dimension,
  char **send_buf, uint32_t *send_len);
int tuneserver_deinitialize_prevtuningp(void * ctrl,
  char **send_buf, uint32_t *send_len);
int tuneserver_preview_getinfo(void * ctrl,
  char **send_buf, uint32_t *send_len);
int tuneserver_preview_getchunksize(void * ctrl,
  char **send_buf, uint32_t *send_len);
int tuneserver_preview_getframe(void * ctrl,
  char **send_buf, uint32_t *send_len);
int tuneserver_preview_unsupported(void * ctrl,
  char **send_buf, uint32_t *send_len);

#endif /*__MM_QCAMERA_COMMANDS_H__*/
