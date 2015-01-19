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

#ifndef __MM_CAMERA_SOCKET_H__
#define __MM_CAMERA_SOCKET_H__

#include <inttypes.h>

typedef enum {
    MM_CAMERA_SOCK_TYPE_UDP,
    MM_CAMERA_SOCK_TYPE_TCP,
} mm_camera_sock_type_t;

int mm_camera_socket_create(int cam_id, mm_camera_sock_type_t sock_type);

int mm_camera_socket_sendmsg(
  int fd,
  void *msg,
  uint32_t buf_size,
  int sendfd);

int mm_camera_socket_recvmsg(
  int fd,
  void *msg,
  uint32_t buf_size,
  int *rcvdfd);

void mm_camera_socket_close(int fd);

#endif /*__MM_CAMERA_SOCKET_H__*/

