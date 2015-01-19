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
#include "mm_qcamera_socket.h"
#include "mm_qcamera_commands.h"
#include "mm_qcamera_dbg.h"

#define IP_ADDR                  "127.0.0.1"
#define TUNING_CHROMATIX_PORT     55555
#define TUNING_PREVIEW_PORT       55556

#define CURRENT_COMMAND_ACK_SUCCESS 1
#define CURRENT_COMMAND_ACK_FAILURE 2

pthread_t eztune_thread_id;

static ssize_t tuneserver_send_command_rsp(tuningserver_t *tsctrl,
  char *send_buf, uint32_t send_len)
{
  ssize_t rc;

  /* send ack back to client upon req */
  if (send_len <= 0) {
    ALOGE("%s:Invalid send len \n", __func__);
    return -1;
  }
  if (send_buf == NULL) {
    ALOGE("%s:Invalid send buf \n", __func__);
    return -1;
  }

  rc = send(tsctrl->clientsocket_id, send_buf, send_len, 0);
  if (rc < 0) {
    ALOGE("%s:RSP send returns error %s\n", __func__, strerror(errno));
  } else {
    rc = 0;
  }

  if (send_buf != NULL) {
    free(send_buf);
    send_buf = NULL;
  }
  return rc;
}

static void release_eztune_prevcmd_rsp(eztune_prevcmd_rsp *pHead)
{
  if (pHead != NULL ) {
    release_eztune_prevcmd_rsp((eztune_prevcmd_rsp *)pHead->next);
    free(pHead);
  }
}

static ssize_t tuneserver_ack(uint16_t a, uint32_t b, tuningserver_t *tsctrl)
{
  ssize_t rc;
  char ack_1[6];
  /*Ack the command here*/
  memcpy(ack_1, &a, 2);
  memcpy(ack_1+2, &b, 4);
  /* send echo back to client upon accept */
  rc = send(tsctrl->clientsocket_id, &ack_1, sizeof(ack_1), 0);
  if (rc < 0) {
    ALOGE("%s: eztune_server_run: send returns error %s\n", __func__,
      strerror(errno));
    return rc;
  } else if (rc < (int32_t)sizeof(ack_1)) {
    /*Shouldn't hit this for packets <1K; need to re-send if we do*/
  }
  return 0;
}

static ssize_t tuneserver_send_command_ack( uint8_t ack,
    tuningserver_t *tsctrl)
{
  ssize_t rc;
  /* send ack back to client upon req */
  rc = send(tsctrl->clientsocket_id, &ack, sizeof(ack), 0);
  if (rc < 0) {
    ALOGE("%s:ACK send returns error %s\n", __func__, strerror(errno));
    return rc;
  }
  return 0;
}

/** tuneserver_process_command
 *    @tsctrl: the server control object
 *
 *  Processes the command that the client sent
 *
 *  Return: >=0 on success, -1 on failure.
 **/
static int32_t tuneserver_process_command(tuningserver_t *tsctrl,
  char *send_buf, uint32_t send_len)
{
  tuneserver_protocol_t *p = tsctrl->proto;
  int result = 0;

  CDBG("%s: Current command is %d\n", __func__, p->current_cmd);
  switch (p->current_cmd) {
  case TUNESERVER_GET_LIST:
    if(tuneserver_send_command_ack(CURRENT_COMMAND_ACK_SUCCESS, tsctrl)) {
      ALOGE("%s: Ack Failed for cmd %d\n", __func__, p->current_cmd);
      return -1;
    }
    result = tuneserver_process_get_list_cmd(tsctrl, p->recv_buf,
      send_buf, send_len);
    if (result < 0) {
      ALOGE("%s: RSP processing Failed for cmd %d\n", __func__, p->current_cmd);
      return -1;
    }
    if(tuneserver_send_command_rsp(tsctrl, send_buf, send_len)) {
      ALOGE("%s: RSP Failed for cmd %d\n", __func__, p->current_cmd);
      return -1;
    }
    break;

  case TUNESERVER_GET_PARMS:
    if(tuneserver_send_command_ack(CURRENT_COMMAND_ACK_SUCCESS, tsctrl)) {
      ALOGE("%s: Ack Failed for cmd %d\n", __func__, p->current_cmd);
      return -1;
    }
    result = tuneserver_process_get_params_cmd(tsctrl, p->recv_buf,
      send_buf, send_len);
    if (result < 0) {
      ALOGE("%s: RSP processing Failed for cmd %d\n", __func__, p->current_cmd);
      return -1;
    }
    if(tuneserver_send_command_rsp(tsctrl, send_buf, send_len)) {
      ALOGE("%s: RSP Failed for cmd %d\n", __func__, p->current_cmd);
      return -1;
    }
    break;

  case TUNESERVER_SET_PARMS:
    if(tuneserver_send_command_ack(CURRENT_COMMAND_ACK_SUCCESS, tsctrl)) {
      ALOGE("%s: Ack Failed for cmd %d\n", __func__, p->current_cmd);
      return -1;
    }
    result = tuneserver_process_set_params_cmd(tsctrl, p->recv_buf,
      send_buf, send_len);
    if (result < 0) {
      ALOGE("%s: RSP processing Failed for cmd %d\n", __func__, p->current_cmd);
      return -1;
    }
    if(tuneserver_send_command_rsp(tsctrl, send_buf, send_len)) {
      ALOGE("%s: RSP Failed for cmd %d\n", __func__, p->current_cmd);
      return -1;
    }
    break;

  case TUNESERVER_MISC_CMDS: {
    if(tuneserver_send_command_ack(CURRENT_COMMAND_ACK_SUCCESS, tsctrl)) {
      ALOGE("%s: Ack Failed for cmd %d\n", __func__, p->current_cmd);
      return -1;
    }
    result = tuneserver_process_misc_cmd(tsctrl, p->recv_buf,
      send_buf, send_len);
    if (result < 0) {
      ALOGE("%s: RSP processing Failed for cmd %d\n", __func__, p->current_cmd);
      return -1;
    }
    if(tuneserver_send_command_rsp(tsctrl, send_buf, send_len)) {
      ALOGE("%s: RSP Failed for cmd %d\n", __func__, p->current_cmd);
      return -1;
    }
    break;
  }

  default:
    if(tuneserver_send_command_ack(CURRENT_COMMAND_ACK_SUCCESS, tsctrl)) {
      ALOGE("%s: Ack Failed for cmd %d\n", __func__, p->current_cmd);
      return -1;
    }
    ALOGE("%s: p->current_cmd: default\n", __func__);
    result = -1;
    break;
  }

  return result;
}

/** tuneserver_process_client_message
 *    @recv_buffer: received message from the client
 *    @tsctrl: the server control object
 *
 *  Processes the message from client and prepares for next
 *  message.
 *
 *  Return: >=0 on success, -1 on failure.
 **/
static int32_t tuneserver_process_client_message(void *recv_buffer,
  tuningserver_t *tsctrl)
{
  int rc = 0;
  tuneserver_protocol_t *p = tsctrl->proto;

  switch (tsctrl->proto->next_recv_code) {
  case TUNESERVER_RECV_COMMAND:
    p->current_cmd = *(uint16_t *)recv_buffer;
    p->next_recv_code = TUNESERVER_RECV_PAYLOAD_SIZE;
    p->next_recv_len = sizeof(uint32_t);
    break;

  case TUNESERVER_RECV_PAYLOAD_SIZE:
    p->next_recv_code = TUNESERVER_RECV_PAYLOAD;
    p->next_recv_len = *(uint32_t *)recv_buffer;
    p->recv_len = p->next_recv_len;
    if (p->next_recv_len > TUNESERVER_MAX_RECV)
      return -1;
    if (p->next_recv_len == 0) {
      p->next_recv_code = TUNESERVER_RECV_RESPONSE;
      p->next_recv_len = sizeof(uint32_t);
    }
    break;

  case TUNESERVER_RECV_PAYLOAD:
    p->recv_buf = malloc(p->next_recv_len);
    if (!p->recv_buf) {
      ALOGE("%s:Error allocating memory for recv_buf %s\n", __func__,
        strerror(errno));
      return -1;
    }
    memcpy(p->recv_buf, recv_buffer, p->next_recv_len);
    p->next_recv_code = TUNESERVER_RECV_RESPONSE;
    p->next_recv_len = sizeof(uint32_t);
    /*Process current command at this point*/
    break;

  case TUNESERVER_RECV_RESPONSE:
    p->next_recv_code = TUNESERVER_RECV_COMMAND;
    p->next_recv_len = 2;
    p->send_len = *(uint32_t *)recv_buffer;
    p->send_buf =  (char *)calloc(p->send_len, sizeof(char *));
    if (!p->send_buf) {
      ALOGE("%s:Error allocating memory for send_buf %s\n", __func__,
        strerror(errno));
      return -1;
    }
    rc = tuneserver_process_command(tsctrl, p->send_buf, p->send_len);
    free(p->recv_buf);
    p->recv_buf = NULL;
    p->recv_len = 0;
    break;

  default:
    ALOGE("%s: p->next_recv_code: default\n", __func__);
    rc = -1;
    break;
  }

  return rc;
}

/** tuneserver_ack_onaccept_initprotocol
 *    @tsctrl: the server control object
 *
 *  Acks a connection from the cient and sets up the
 *  protocol object to start receiving commands.
 *
 *  Return: >=0 on success, -1 on failure.
 **/
static ssize_t tuneserver_ack_onaccept_initprotocol(tuningserver_t *tsctrl)
{
  ssize_t rc = 0;
  uint32_t ack_status;

  ALOGE("%s starts\n", __func__);
/*
  if(tsctrl->camera_running) {
    ack_status = 1;
  } else {
    ack_status = 2;
  }
*/
  ack_status = 1;

  rc = tuneserver_ack(1, ack_status, tsctrl);

  tsctrl->proto = malloc(sizeof(tuneserver_protocol_t));
  if (!tsctrl->proto) {
    ALOGE("%s: malloc returns NULL with error %s\n", __func__, strerror(errno));
    return -1;
  }

  tsctrl->proto->current_cmd    = 0xFFFF;
  tsctrl->proto->next_recv_code = TUNESERVER_RECV_COMMAND;
  tsctrl->proto->next_recv_len  = 2;
  tsctrl->proto->recv_buf       = NULL;
  tsctrl->proto->send_buf       = NULL;

  CDBG("%s end\n", __func__);

  return rc;
}

/** tuneserver_check_status
 *    @tsctrl: the server control object
 *
 *  Checks if camera is running and stops it.
 *
 *  Return: >=0 on success, -1 on failure.
 **/
#if 0
static void tuneserver_check_status(tuningserver_t *tsctrl)
{
  if (tsctrl->camera_running == 1) {
    /*TODO: Stop camera here*/
    tuneserver_stop_cam(&tsctrl->lib_handle);
  }
  tsctrl->camera_running = 0;

  tuneserver_close_cam(&tsctrl->lib_handle);
}
#endif

static ssize_t prevserver_send_command_rsp(tuningserver_t *tsctrl,
  char *send_buf, uint32_t send_len)
{
  ssize_t rc;

  /* send ack back to client upon req */
  if (send_len <= 0) {
    ALOGE("%s:Invalid send len \n", __func__);
    return -1;
  }
  if (send_buf == NULL) {
    ALOGE("%s:Invalid send buf \n", __func__);
    return -1;
  }

  rc = send(tsctrl->pr_clientsocket_id, send_buf, send_len, 0);
  if (rc < 0) {
    ALOGE("%s:RSP send returns error %s\n", __func__, strerror(errno));
  } else {
    rc = 0;
  }
  if (send_buf != NULL) {
    free(send_buf);
    send_buf = NULL;
  }
  return rc;
}

static void prevserver_init_protocol(tuningserver_t *tsctrl)
{
  tsctrl->pr_proto = malloc(sizeof(prserver_protocol_t));
  if (!tsctrl->pr_proto) {
    ALOGE("%s: malloc returns NULL with error %s\n",
     __func__, strerror(errno));
    return;
  }

  tsctrl->pr_proto->current_cmd    = 0xFFFF;
  tsctrl->pr_proto->next_recv_code = TUNE_PREV_RECV_COMMAND;
  tsctrl->pr_proto->next_recv_len  = 2;
}

static int32_t prevserver_process_command(
  tuningserver_t *tsctrl, char **send_buf, uint32_t *send_len)
{
  prserver_protocol_t *p = tsctrl->pr_proto;
  int result = 0;
  eztune_prevcmd_rsp *rsp_ptr=NULL, *rspn_ptr=NULL, *head_ptr=NULL;

  CDBG("%s: Current command is %d\n", __func__, p->current_cmd);
  switch (p->current_cmd) {
  case TUNE_PREV_GET_INFO:
    result = tuneserver_preview_getinfo(tsctrl, send_buf, send_len);
    if (result < 0) {
      ALOGE("%s: RSP processing Failed for cmd %d\n", __func__,
        p->current_cmd);
      return -1;
    }
    rsp_ptr = (eztune_prevcmd_rsp *)*send_buf;
    if ((!rsp_ptr) || (!rsp_ptr->send_buf)) {
      ALOGE("%s: RSP ptr is NULL %d\n", __func__, p->current_cmd);
      return -1;
    }
    if (prevserver_send_command_rsp(tsctrl,
      rsp_ptr->send_buf, rsp_ptr->send_len)) {
      ALOGE("%s: RSP Failed for TUNE_PREV_GET_INFO ver cmd %d\n", __func__,
        p->current_cmd);
      return -1;
    }
    rspn_ptr = (eztune_prevcmd_rsp *)rsp_ptr->next;
    if ((!rspn_ptr) || (!rspn_ptr->send_buf)) {
      ALOGE("%s: RSP1 ptr is NULL %d\n", __func__, p->current_cmd);
      return -1;
    }
    if (prevserver_send_command_rsp(tsctrl,
        rspn_ptr->send_buf, rspn_ptr->send_len)) {
      ALOGE("%s: RSP Failed for TUNE_PREV_GET_INFO caps cmd %d\n", __func__,
        p->current_cmd);
      return -1;
    }
    free(rspn_ptr);
    free(rsp_ptr);
    break;

  case TUNE_PREV_CH_CNK_SIZE:
    result = tuneserver_preview_getchunksize(tsctrl, send_buf, send_len);
    if (result < 0) {
      ALOGE("%s: RSP processing Failed for cmd %d\n", __func__, p->current_cmd);
      return -1;
    }
    if (prevserver_send_command_rsp(tsctrl, *send_buf, *send_len)) {
      ALOGE("%s: RSP Failed for TUNE_PREV_CH_CNK_SIZE cmd %d\n", __func__,
        p->current_cmd);
      return -1;
    }
    break;

  case TUNE_PREV_GET_PREV_FRAME:
    result = tuneserver_preview_getframe(tsctrl, send_buf, send_len);
    if (result < 0) {
      ALOGE("%s: RSP processing Failed for cmd %d\n", __func__, p->current_cmd);
      return -1;
    }
    rsp_ptr = (eztune_prevcmd_rsp *)*send_buf;
    if ((!rsp_ptr) || (!rsp_ptr->send_buf)) {
      ALOGE("%s: RSP ptr is NULL %d\n", __func__, p->current_cmd);
      return -1;
    }
    head_ptr = rsp_ptr;

    while (rsp_ptr != NULL) {
      if ((!rsp_ptr) || (!rsp_ptr->send_buf)) {
        ALOGE("%s: RSP ptr is NULL %d\n", __func__, p->current_cmd);
        return -1;
      }
      if (prevserver_send_command_rsp(tsctrl,
        rsp_ptr->send_buf, rsp_ptr->send_len)) {
        ALOGE("%s: RSP Failed for TUNE_PREV_GET_INFO ver cmd %d\n", __func__,
          p->current_cmd);
        return -1;
      }
      rsp_ptr = (eztune_prevcmd_rsp *)rsp_ptr->next;
    }
    release_eztune_prevcmd_rsp(head_ptr);
    break;

  case TUNE_PREV_GET_JPG_SNAP:
  case TUNE_PREV_GET_RAW_SNAP:
  case TUNE_PREV_GET_RAW_PREV:
    result = tuneserver_preview_unsupported(tsctrl, send_buf, send_len);
    if (result < 0) {
       ALOGE("%s:RSP processing Failed for cmd %d\n", __func__, p->current_cmd);
      return -1;
    }
    if (prevserver_send_command_rsp(tsctrl, *send_buf, *send_len)) {
      ALOGE("%s:RSP Failed for UNSUPPORTED cmd %d\n", __func__, p->current_cmd);
      return -1;
    }
    break;

  default:
    ALOGE("%s: p->current_cmd: default\n", __func__);
    result = -1;
    break;
  }

  return result;
}

/** previewserver_process_client_message
 *    @recv_buffer: received message from the client
 *    @tsctrl: the server control object
 *
 *  Processes the message from client and prepares for next
 *  message.
 *
 *  Return: >=0 on success, -1 on failure.
 **/
static int32_t prevserver_process_client_message(void *recv_buffer,
  tuningserver_t *tsctrl)
{
  int rc = 0;
  prserver_protocol_t *p = tsctrl->pr_proto;

  switch (p->next_recv_code) {
  case TUNE_PREV_RECV_COMMAND:
    CDBG("%s  %d\n", __func__, __LINE__);
    p->current_cmd = *(uint16_t *)recv_buffer;
    if(p->current_cmd != TUNE_PREV_CH_CNK_SIZE) {
      rc = prevserver_process_command(tsctrl,
        &p->send_buf, (uint32_t *)&p->send_len);
      break;
    }
    p->next_recv_code = TUNE_PREV_RECV_NEWCNKSIZE;
    p->next_recv_len = sizeof(uint32_t);
    CDBG("%s TUNE_PREV_COMMAND X\n", __func__);
    break;
  case TUNE_PREV_RECV_NEWCNKSIZE:
    CDBG("%s  %d\n", __func__, __LINE__);
    p->new_cnk_size = *(uint32_t *)recv_buffer;
    p->next_recv_code = TUNE_PREV_RECV_COMMAND;
    p->next_recv_len  = 2;
    rc = prevserver_process_command(tsctrl,
      &p->send_buf, (uint32_t *)&p->send_len);
    break;
  default:
    ALOGE("%s prev_proc->next_recv_code: default\n", __func__);
    rc = -1;
    break;
  }

  return rc;
}

/** tunning_server_socket_listen
 *    @ip_addr: the ip addr to listen
 *    @port: the port to listen
 *
 *  Setup a listen socket for eztune.
 *
 *  Return: >0 on success, <=0 on failure.
 **/
int tunning_server_socket_listen(const char* ip_addr, uint16_t port)
{
  int sock_fd = -1;
  mm_qcamera_sock_addr_t server_addr;
  int result;
  int option;
  int socket_flag;

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.addr_in.sin_family = AF_INET;
  server_addr.addr_in.sin_port = (__be16) htons(port);
  server_addr.addr_in.sin_addr.s_addr = inet_addr(ip_addr);

  if (server_addr.addr_in.sin_addr.s_addr == INADDR_NONE) {
    ALOGE("[ERR] %s invalid address.\n", __func__);
    return -1;
  }

  /* Create an AF_INET stream socket to receive incoming connection ON */
  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd < 0) {
    ALOGE("[ERR] %s socket failed\n", __func__);
    return sock_fd;
  }

  // set listen socket to non-block, but why??
  socket_flag = fcntl(sock_fd, F_GETFL, 0);
  fcntl(sock_fd, F_SETFL, socket_flag | O_NONBLOCK);

  /* reuse in case it is in timeout */
  option = 1;
  result = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR,
    &option, sizeof(option));

  if (result < 0) {
    ALOGE("eztune setsockopt failed");
    close(sock_fd);
    sock_fd = -1;
    return sock_fd;
  }

  result = bind(sock_fd, &server_addr.addr, sizeof(server_addr.addr_in));
  if (result < 0) {
    ALOGE("eztune socket bind failed");
    close(sock_fd);
    sock_fd = -1;
    return sock_fd;
  }

  result = listen(sock_fd, 1);
  if (result < 0) {
    ALOGE("eztune socket listen failed");
    close(sock_fd);
    sock_fd = -1;
    return sock_fd;
  }

  CDBG_HIGH("%s. sock_fd: %d, listen at port: %d\n", __func__, sock_fd, port);

  return sock_fd;
}

/** main
 *
 *  Creates the server, and starts waiting for
 *  connections/messages from a prospective
 *  client
 *
 **/
void *eztune_proc(void *data)
{
  int server_socket = -1, client_socket = -1;
  int prev_server_socket = -1, prev_client_socket = -1;

  mm_qcamera_sock_addr_t addr_client_inet;
  socklen_t addr_client_len = sizeof(addr_client_inet.addr_in);
  int result;
  fd_set tsfds;
  int num_fds = 0;
  ssize_t recv_bytes;
  char buf[TUNESERVER_MAX_RECV];

  mm_camera_lib_handle *lib_handle = (mm_camera_lib_handle *)data;

  ALOGE(">>> Starting tune server <<< \n");

  // for eztune chromatix params
  server_socket = tunning_server_socket_listen(IP_ADDR, TUNING_CHROMATIX_PORT);
  if (server_socket <= 0) {
    ALOGE("[ERR] fail to setup listen socket for eztune chromatix parms...");
    return NULL;
  }
  prev_server_socket = tunning_server_socket_listen(IP_ADDR, TUNING_PREVIEW_PORT);
  if (prev_server_socket <= 0) {
    ALOGE("[ERR] fail to setup listen socket for eztune preview...\n");
    return NULL;
  }
  num_fds = TUNESERVER_MAX(server_socket, prev_server_socket);
  CDBG_HIGH("num_fds = %d\n", num_fds);

  do {
    FD_ZERO(&tsfds);
    FD_SET(server_socket, &tsfds);
    FD_SET(prev_server_socket, &tsfds);
    if (client_socket > 0) {
      FD_SET(client_socket, &tsfds);
    }
    if (prev_client_socket > 0) {
      FD_SET( prev_client_socket, &tsfds);
    }

    /* no timeout */
    result = select(num_fds + 1, &tsfds, NULL, NULL, NULL);
    if (result < 0) {
      ALOGE("[ERR] select failed: %s\n", strerror(errno));
      continue;
    }

    /*
     ** (1) CHROMATIX SERVER
     */
    if (FD_ISSET(server_socket, &tsfds)) {
      CDBG("Receiving New client connection\n");

      client_socket = accept(server_socket,
        &addr_client_inet.addr, &addr_client_len);
      if (client_socket == -1) {
        ALOGE("accept failed %s", strerror(errno));
        continue;
      }

      ALOGE("accept a new connect on 55555, sd(%d)\n", client_socket);
      num_fds = TUNESERVER_MAX(num_fds, client_socket);

      // open camera and get handle - this is needed to
      // be able to set parameters without starting
      // preview stream
      /*if (!tsctrl.camera_running) {
        result = tuneserver_open_cam(&tsctrl.lib_handle, &tsctrl);
        if(result) {
          printf("\n Camera Open Fail !!! \n");
          close(server_socket);
          return EXIT_FAILURE;
        }
      }*/
      result = tuneserver_open_cam(lib_handle);
      if(result) {
        ALOGE("\n Tuning Library open failed!!!\n");
        close(server_socket);
        return NULL;
      }
      lib_handle->tsctrl.clientsocket_id = client_socket;
      if (tuneserver_ack_onaccept_initprotocol(&lib_handle->tsctrl) < 0) {
        ALOGE("%s: Error while acking\n", __func__);
        close(client_socket);
        continue;
      }
      tuneserver_initialize_tuningp(lib_handle, client_socket,
        lib_handle->tsctrl.proto->send_buf, lib_handle->tsctrl.proto->send_len);
    }

    if (FD_ISSET(client_socket, &tsfds)) {
      if (lib_handle->tsctrl.proto == NULL) {
        ALOGE("%s: Cannot receive msg without connect\n", __func__);
        continue;
      }

      /*Receive message and process it*/
      recv_bytes = recv(client_socket, (void *)buf,
        lib_handle->tsctrl.proto->next_recv_len, 0);
      CDBG("Receive %lld bytes \n", (long long int) recv_bytes);

      if (recv_bytes == -1) {
        ALOGE("%s: Receive failed with error %s\n", __func__, strerror(errno));
        //tuneserver_check_status(&tsctrl);
        continue;
      } else if (recv_bytes == 0) {
        ALOGE("%s %d: connection has been terminated\n", __func__, __LINE__);

        tuneserver_deinitialize_tuningp(&lib_handle->tsctrl, client_socket,
          lib_handle->tsctrl.proto->send_buf,
          lib_handle->tsctrl.proto->send_len);
        free(lib_handle->tsctrl.proto);
        lib_handle->tsctrl.proto = NULL;

        close(client_socket);
        client_socket = 0;
        //tuneserver_check_status(&tsctrl);
      } else {
        CDBG("%s: Processing socket command\n", __func__);

        result = tuneserver_process_client_message(buf, &lib_handle->tsctrl);

        if (result < 0) {
          ALOGE("%s %d Protocol violated\n", __func__, __LINE__);

          free(lib_handle->tsctrl.proto);
          lib_handle->tsctrl.proto = NULL;

          close(client_socket);
          client_socket = 0;
          //tuneserver_check_status(&tsctrl);
          continue;
        }
      }
    }

    /*
     ** (2) PREVIEW SERVER
     */
    if (FD_ISSET(prev_server_socket, &tsfds)) {
      CDBG("Receiving New Preview client connection\n");

      prev_client_socket = accept(prev_server_socket,
        &addr_client_inet.addr, &addr_client_len);
      if (prev_client_socket == -1) {
        ALOGE("accept failed %s", strerror(errno));
        continue;
      }

      lib_handle->tsctrl.pr_clientsocket_id = prev_client_socket;

      CDBG("Accepted a new connection, fd(%d)\n", prev_client_socket);
      num_fds = TUNESERVER_MAX(num_fds, prev_client_socket);

      // start camera
      /*if (!tsctrl.camera_running) {
        result = 0;
        result = tuneserver_open_cam(&tsctrl.lib_handle, &tsctrl);
        if(result) {
          printf("\n Camera Open Fail !!! \n");
          return EXIT_FAILURE;
        }
      }*/
      cam_dimension_t dim;
      //dim.width = lib_handle->test_obj.buffer_width;
      //dim.height = lib_handle->test_obj.buffer_height;
      dim.width = DEFAULT_PREVIEW_WIDTH;
      dim.height = DEFAULT_PREVIEW_HEIGHT;

      CDBG("preview dimension info: w(%d), h(%d)\n", dim.width, dim.height);
      // we have to make sure that camera is running, before init connection,
      // because we need to know the frame size for allocating the memory.
      prevserver_init_protocol(&lib_handle->tsctrl);

      result = tuneserver_initialize_prevtuningp(lib_handle, prev_client_socket,
        dim, (char **)&lib_handle->tsctrl.proto->send_buf,
        &lib_handle->tsctrl.proto->send_len);
      if (result < 0) {
        ALOGE("tuneserver_initialize_prevtuningp error!");
        close(prev_client_socket);
        prev_client_socket = 0;
      }
    }

    if (FD_ISSET(prev_client_socket, &tsfds)) {
      recv_bytes = recv(prev_client_socket, (void *)buf,
        lib_handle->tsctrl.pr_proto->next_recv_len, 0);

      CDBG("%s prev_client_socket=%d\n", __func__, prev_client_socket);
      CDBG("%s next_recv_len=%d\n", __func__, buf[0]+buf[1]*256);

      if (recv_bytes <= 0) {
        if (recv_bytes == 0) {
          ALOGE("client close the connection.\n");
        } else {
          ALOGE("receive error: %s\n", strerror(errno));
        }

        //tuneserver_check_status(&tsctrl);
        // if recv error, we should close the connection, free the proto data,
        // AND wait for a new connecton..
        // close_connection();
        // stop_camera()
        // cleanup_proto_data();
        tuneserver_deinitialize_prevtuningp(&lib_handle->tsctrl,
          (char **)&lib_handle->tsctrl.proto->send_buf,
          &lib_handle->tsctrl.proto->send_len);
        close(prev_client_socket);
        prev_client_socket = 0;
      } else {
        result = prevserver_process_client_message((void *)buf,
          &lib_handle->tsctrl);
        if (result < 0) {
          ALOGE("%s %d Protocol violated\n", __func__, __LINE__);

          //free(tsctrl->preivew_proto);
          //free(tsctrl);
          //max_fd = ezt_parms_listen_sd + 1;
          tuneserver_deinitialize_prevtuningp(&lib_handle->tsctrl,
            (char **)&lib_handle->tsctrl.proto->send_buf,
            &lib_handle->tsctrl.proto->send_len);
          close(prev_client_socket);
          prev_client_socket = 0;
          //tuneserver_check_status(&tsctrl);
        }
        //sleep(1);
      }
    }
  } while (1);

  if (server_socket > 0) {
    close(server_socket);
  }
  if (client_socket > 0) {
    close(client_socket);
  }
  if (prev_server_socket > 0) {
    close(prev_server_socket);
  }
  if (prev_client_socket > 0) {
    close(prev_client_socket);
  }

  return EXIT_SUCCESS;
}

int eztune_server_start (void *lib_handle)
{
  return pthread_create(&eztune_thread_id, NULL,  eztune_proc, lib_handle);
}

