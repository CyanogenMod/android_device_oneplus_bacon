/* Copyright (c) 2012, The Linux Foundataion. All rights reserved.
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

#ifndef __QCAMERA_CMD_THREAD_H__
#define __QCAMERA_CMD_THREAD_H__

#include <pthread.h>
#include <cam_semaphore.h>

#include "cam_types.h"
#include "QCameraQueue.h"

namespace qcamera {

typedef enum
{
    CAMERA_CMD_TYPE_NONE,
    CAMERA_CMD_TYPE_START_DATA_PROC,
    CAMERA_CMD_TYPE_STOP_DATA_PROC,
    CAMERA_CMD_TYPE_DO_NEXT_JOB,
    CAMERA_CMD_TYPE_EXIT,
    CAMERA_CMD_TYPE_MAX
} camera_cmd_type_t;

typedef struct {
    camera_cmd_type_t cmd;
} camera_cmd_t;

class QCameraCmdThread {
public:
    QCameraCmdThread();
    ~QCameraCmdThread();

    int32_t launch(void *(*start_routine)(void *), void* user_data);
    int32_t setName(const char* name);
    int32_t exit();
    int32_t sendCmd(camera_cmd_type_t cmd, uint8_t sync_cmd, uint8_t priority);
    camera_cmd_type_t getCmd();

    QCameraQueue cmd_queue;      /* cmd queue */
    pthread_t cmd_pid;           /* cmd thread ID */
    cam_semaphore_t cmd_sem;               /* semaphore for cmd thread */
    cam_semaphore_t sync_sem;              /* semaphore for synchronized call signal */
};

}; // namespace qcamera

#endif /* __QCAMERA_CMD_THREAD_H__ */
