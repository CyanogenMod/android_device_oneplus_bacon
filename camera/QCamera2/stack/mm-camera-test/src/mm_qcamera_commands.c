/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <stdlib.h>
#include <cutils/properties.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <mm_qcamera_app.h>
#include "mm_qcamera_commands.h"
#include "mm_qcamera_dbg.h"

int tuneserver_initialize_prevtuningp(void * ctrl,
  int pr_client_socket_id, cam_dimension_t dimension,
  char **send_buf, uint32_t *send_len)
{
  int result = 0;
  mm_camera_lib_handle *lib_handle = (mm_camera_lib_handle *) ctrl;
  tuningserver_t *tctrl = &lib_handle->tsctrl;

  CDBG("%s  %d\n", __func__, __LINE__);
  if (tctrl->tuning_params.func_tbl->prevcommand_process == NULL) {
      ALOGE("%s  %d\n", __func__, __LINE__);
      return -1;
  }

  result = tctrl->tuning_params.func_tbl->prevcommand_process(
      NULL, TUNE_PREVCMD_INIT, (void *)&pr_client_socket_id,
      send_buf, send_len);
  result = tctrl->tuning_params.func_tbl->prevcommand_process(
      NULL, TUNE_PREVCMD_SETDIM, (void *)&dimension,
      send_buf, send_len);

  mm_camera_lib_set_preview_usercb(lib_handle,
      (tctrl->tuning_params.func_tbl->prevframe_callback));

  return result;
}

int tuneserver_deinitialize_prevtuningp(void * ctrl,
    char **send_buf, uint32_t *send_len)
{
  int result = 0;
  tuningserver_t *tctrl = (tuningserver_t *) ctrl;

  CDBG("%s  %d\n", __func__, __LINE__);

  result = tctrl->tuning_params.func_tbl->prevcommand_process(
    &tctrl->pr_proto, TUNE_PREVCMD_DEINIT, NULL, send_buf, send_len);

  return result;
}

int tuneserver_preview_getinfo(void * ctrl, char **send_buf, uint32_t *send_len)
{
  int result = 0;
  tuningserver_t *tctrl = (tuningserver_t *) ctrl;

  CDBG("%s  %d\n", __func__, __LINE__);
  result = tctrl->tuning_params.func_tbl->prevcommand_process(
    &tctrl->pr_proto, TUNE_PREVCMD_GETINFO, NULL, send_buf, send_len);

  return result;
}

int tuneserver_preview_getchunksize(void * ctrl,
  char **send_buf, uint32_t *send_len)
{
  int result = 0;
  tuningserver_t *tctrl = (tuningserver_t *) ctrl;

  CDBG("%s  %d\n", __func__, __LINE__);
  result = tctrl->tuning_params.func_tbl->prevcommand_process(
    &tctrl->pr_proto, TUNE_PREVCMD_GETCHUNKSIZE,
    (void *)&tctrl->pr_proto->new_cnk_size, send_buf, send_len);

  return result;
}

int tuneserver_preview_getframe(void * ctrl,
  char **send_buf, uint32_t *send_len)
{
  int result = 0;
  tuningserver_t *tctrl = (tuningserver_t *) ctrl;

  CDBG("%s  %d\n", __func__, __LINE__);
  result = tctrl->tuning_params.func_tbl->prevcommand_process(
    &tctrl->pr_proto, TUNE_PREVCMD_GETFRAME, NULL, send_buf, send_len);

  return result;
}

int tuneserver_preview_unsupported(void * ctrl,
  char **send_buf, uint32_t *send_len)
{
  int result = 0;
  tuningserver_t *tctrl = (tuningserver_t *) ctrl;

  CDBG("%s  %d\n", __func__, __LINE__);
  result = tctrl->tuning_params.func_tbl->prevcommand_process(
    &tctrl->pr_proto, TUNE_PREVCMD_UNSUPPORTED, NULL, send_buf, send_len);

  return result;
}

int tuneserver_initialize_tuningp(void * ctrl, int client_socket_id,
  char *send_buf, uint32_t send_len)
{
  int result = 0;
  mm_camera_lib_handle *lib_handle = (mm_camera_lib_handle *) ctrl;
  tuningserver_t *tctrl = &lib_handle->tsctrl;

  CDBG("%s  %d\n", __func__, __LINE__);
  result = tctrl->tuning_params.func_tbl->command_process(
    lib_handle, TUNE_CMD_INIT, &client_socket_id, send_buf, send_len);

  return result;
}

int tuneserver_deinitialize_tuningp(void * ctrl, int client_socket_id,
  char *send_buf, uint32_t send_len)
{
  int result = 0;
  tuningserver_t *tctrl = (tuningserver_t *) ctrl;

  CDBG("%s  %d\n", __func__, __LINE__);

  result = tctrl->tuning_params.func_tbl->command_process(
    NULL, TUNE_CMD_DEINIT, &client_socket_id, send_buf, send_len);

  return result;
}

int tuneserver_process_get_list_cmd(void * ctrl, void *recv_cmd,
  char *send_buf, uint32_t send_len)
{
  int result = 0;
  tuningserver_t *tctrl = (tuningserver_t *) ctrl;

  CDBG("%s  %d\n", __func__, __LINE__);
  result = tctrl->tuning_params.func_tbl->command_process(
     recv_cmd, TUNE_CMD_GET_LIST, NULL, send_buf, send_len);

  return result;
}

int tuneserver_process_get_params_cmd(void * ctrl, void *recv_cmd,
  char *send_buf, uint32_t send_len)
{
  int result = 0;
  tuningserver_t *tctrl = (tuningserver_t *) ctrl;

  CDBG("%s  %d\n", __func__, __LINE__);
  result = tctrl->tuning_params.func_tbl->command_process
    (recv_cmd, TUNE_CMD_GET_PARAMS, NULL, send_buf, send_len);

  return result;
}

int tuneserver_process_set_params_cmd(void * ctrl, void *recv_cmd,
  char *send_buf, uint32_t send_len)
{
  int result = 0;
  tuningserver_t *tctrl = (tuningserver_t *) ctrl;

  CDBG("%s  %d\n", __func__, __LINE__);
  result = tctrl->tuning_params.func_tbl->command_process(
     recv_cmd, TUNE_CMD_SET_PARAMS, NULL, send_buf, send_len);

  return result;
}

int tuneserver_process_misc_cmd(void * ctrl, void *recv_cmd,
  char *send_buf, uint32_t send_len)
{
  int result = 0;
  tuningserver_t *tctrl = (tuningserver_t *) ctrl;

  CDBG("%s  %d\n", __func__, __LINE__);
  result = tctrl->tuning_params.func_tbl->command_process(
     recv_cmd, TUNE_CMD_MISC, NULL, send_buf, send_len);

  return result;
}

/** tuneserver_close_cam
 *    @lib_handle: the camera handle object
 *
 *  closes the camera
 *
 *  Return: >=0 on success, -1 on failure.
 **/
int tuneserver_close_cam(mm_camera_lib_handle *lib_handle)
{
  int result = 0;

  result = mm_camera_lib_close(lib_handle);
  if (result < 0) {
    printf("%s: Camera close failed\n", __func__);
  } else {
    printf("Camera is closed \n");
  }
  return result;
}
#if 0
/** tuneserver_start_cam
 *    @lib_handle: the camera handle object
 *
 *  starts the camera
 *
 *  Return: >=0 on success, -1 on failure.
 **/
static int tuneserver_start_cam(mm_camera_lib_handle *lib_handle)
{
  int result = 0;

  result = mm_camera_lib_start_stream(lib_handle);
  if (result < 0) {
    printf("%s: Camera start failed\n", __func__);
    goto error1;
  }
  return result;
error1:
  mm_camera_lib_close(lib_handle);
  return result;
}
#endif

/** tuneserver_stop_cam
 *    @lib_handle: the camera handle object
 *
 *  stops the camera
 *
 *  Return: >=0 on success, -1 on failure.
 **/
int tuneserver_stop_cam(mm_camera_lib_handle *lib_handle)
{
  int result = 0;

  result = mm_camera_lib_stop_stream(lib_handle);
  if (result < 0) {
    printf("%s: Camera stop failed\n", __func__);
  }
//  result = mm_camera_lib_close(lib_handle);
  return result;
}

/** tuneserver_open_cam
 *    @lib_handle: the camera handle object
 *
 *  opens the camera
 *
 *  Return: >=0 on success, -1 on failure.
 **/
#if 1
int tuneserver_open_cam(mm_camera_lib_handle *lib_handle)
{
  int result = 0;

  CDBG("%s  %d\n", __func__, __LINE__);
  result = mm_camera_load_tuninglibrary(&lib_handle->tsctrl.tuning_params);
  if (result < 0) {
    CDBG_ERROR("%s: tuning library open failed\n", __func__);
  }
  return result;
}
#endif
