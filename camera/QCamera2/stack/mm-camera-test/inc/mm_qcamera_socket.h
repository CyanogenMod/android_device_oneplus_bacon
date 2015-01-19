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

#ifndef __MM_QCAMERA_SOCKET_H__
#define __MM_QCAMERA_SOCKET_H__

#include <stdint.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <linux/socket.h>
#include <arpa/inet.h>
#include <utils/Log.h>

#undef __FD_SET
#define __FD_SET(fd, fdsetp) \
  (((fd_set *)(fdsetp))->fds_bits[(fd) >> 5] |= (1LU<<((fd) & 31)))

#undef __FD_CLR
#define __FD_CLR(fd, fdsetp) \
  (((fd_set *)(fdsetp))->fds_bits[(fd) >> 5] &= ~(1LU<<((fd) & 31)))

#undef  __FD_ISSET
#define __FD_ISSET(fd, fdsetp) \
  ((((fd_set *)(fdsetp))->fds_bits[(fd) >> 5] & (1LU<<((fd) & 31))) != 0)

#undef  __FD_ZERO
#define __FD_ZERO(fdsetp) \
  (memset (fdsetp, 0, sizeof (*(fd_set *)(fdsetp))))

#define TUNESERVER_MAX_RECV 2048
#define TUNESERVER_MAX(a, b)  (((a) > (b)) ? (a) : (b))

#define TUNESERVER_GET_LIST 1014
#define TUNESERVER_GET_PARMS 1015
#define TUNESERVER_SET_PARMS 1016
#define TUNESERVER_MISC_CMDS 1021

#define TUNE_PREV_GET_INFO        0x0001
#define TUNE_PREV_CH_CNK_SIZE     0x0002
#define TUNE_PREV_GET_PREV_FRAME  0x0003
#define TUNE_PREV_GET_JPG_SNAP    0x0004
#define TUNE_PREV_GET_RAW_SNAP    0x0005
#define TUNE_PREV_GET_RAW_PREV    0x0006

typedef struct {
  char data[128];
} tuneserver_misc_cmd;

typedef enum {
  TUNESERVER_RECV_COMMAND = 1,
  TUNESERVER_RECV_PAYLOAD_SIZE,
  TUNESERVER_RECV_PAYLOAD,
  TUNESERVER_RECV_RESPONSE,
  TUNESERVERER_RECV_INVALID,
} tuneserver_recv_cmd_t;

typedef struct {
  uint16_t          current_cmd;
  tuneserver_recv_cmd_t next_recv_code;
  uint32_t          next_recv_len;
  void              *recv_buf;
  uint32_t          recv_len;
  uint32_t          send_len;
  void              *send_buf;
} tuneserver_protocol_t;

typedef enum {
  TUNE_PREV_RECV_COMMAND = 1,
  TUNE_PREV_RECV_NEWCNKSIZE,
  TUNE_PREV_RECV_INVALID
} tune_prev_cmd_t;

typedef struct _eztune_preview_protocol_t {
  uint16_t         current_cmd;
  tune_prev_cmd_t  next_recv_code;
  uint32_t         next_recv_len;
  int32_t          send_len;
  char*            send_buf;
  uint32_t         send_buf_size;
  uint32_t         new_cnk_size;
  uint32_t         new_cmd_available;
} prserver_protocol_t;

typedef union {
  struct sockaddr addr;
  struct sockaddr_in addr_in;
} mm_qcamera_sock_addr_t;

int eztune_server_start(void *lib_handle);

#endif /*__MM_QCAMERA_SOCKET_H__*/
