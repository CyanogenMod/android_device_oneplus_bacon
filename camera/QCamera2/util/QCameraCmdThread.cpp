/* Copyright (c) 2012-2013, The Linux Foundataion. All rights reserved.
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

#include <utils/Errors.h>
#include <utils/Log.h>
#include <sys/prctl.h>
#include "QCameraCmdThread.h"

using namespace android;

namespace qcamera {

/*===========================================================================
 * FUNCTION   : QCameraCmdThread
 *
 * DESCRIPTION: default constructor of QCameraCmdThread
 *
 * PARAMETERS : None
 *
 * RETURN     : None
 *==========================================================================*/
QCameraCmdThread::QCameraCmdThread() :
    cmd_queue()
{
    cmd_pid = 0;
    cam_sem_init(&sync_sem, 0);
    cam_sem_init(&cmd_sem, 0);
}

/*===========================================================================
 * FUNCTION   : ~QCameraCmdThread
 *
 * DESCRIPTION: deconstructor of QCameraCmdThread
 *
 * PARAMETERS : None
 *
 * RETURN     : None
 *==========================================================================*/
QCameraCmdThread::~QCameraCmdThread()
{
    cam_sem_destroy(&sync_sem);
    cam_sem_destroy(&cmd_sem);
}

/*===========================================================================
 * FUNCTION   : launch
 *
 * DESCRIPTION: launch Cmd Thread
 *
 * PARAMETERS :
 *   @start_routine : thread routine function ptr
 *   @user_data     : user data ptr
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraCmdThread::launch(void *(*start_routine)(void *),
                                 void* user_data)
{
    /* launch the thread */
    pthread_create(&cmd_pid,
                   NULL,
                   start_routine,
                   user_data);
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setName
 *
 * DESCRIPTION: name the cmd thread
 *
 * PARAMETERS :
 *   @name : desired name for the thread
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraCmdThread::setName(const char* name)
{
    /* name the thread */
    prctl(PR_SET_NAME, (unsigned long)name, 0, 0, 0);
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : sendCmd
 *
 * DESCRIPTION: send a command to the Cmd Thread
 *
 * PARAMETERS :
 *   @cmd     : command to be executed.
 *   @sync_cmd: flag to indicate if this is a synchorinzed cmd. If true, this call
 *              will wait until signal is set after the command is completed.
 *   @priority: flag to indicate if this is a cmd with priority. If true, the cmd
 *              will be enqueued to the head with priority.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraCmdThread::sendCmd(camera_cmd_type_t cmd, uint8_t sync_cmd, uint8_t priority)
{
    camera_cmd_t *node = (camera_cmd_t *)malloc(sizeof(camera_cmd_t));
    if (NULL == node) {
        ALOGE("%s: No memory for camera_cmd_t", __func__);
        return NO_MEMORY;
    }
    memset(node, 0, sizeof(camera_cmd_t));
    node->cmd = cmd;

    if (priority) {
        cmd_queue.enqueueWithPriority((void *)node);
    } else {
        cmd_queue.enqueue((void *)node);
    }
    cam_sem_post(&cmd_sem);

    /* if is a sync call, need to wait until it returns */
    if (sync_cmd) {
        cam_sem_wait(&sync_sem);
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : getCmd
 *
 * DESCRIPTION: dequeue a cmommand from cmd queue
 *
 * PARAMETERS : None
 *
 * RETURN     : cmd dequeued
 *==========================================================================*/
camera_cmd_type_t QCameraCmdThread::getCmd()
{
    camera_cmd_type_t cmd = CAMERA_CMD_TYPE_NONE;
    camera_cmd_t *node = (camera_cmd_t *)cmd_queue.dequeue();
    if (NULL == node) {
        ALOGD("%s: No notify avail", __func__);
        return CAMERA_CMD_TYPE_NONE;
    } else {
        cmd = node->cmd;
        free(node);
    }
    return cmd;
}

/*===========================================================================
 * FUNCTION   : exit
 *
 * DESCRIPTION: exit the CMD thread
 *
 * PARAMETERS : None
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraCmdThread::exit()
{
    int32_t rc = NO_ERROR;

    if (cmd_pid == 0) {
        return rc;
    }

    rc = sendCmd(CAMERA_CMD_TYPE_EXIT, 0, 1);
    if (NO_ERROR != rc) {
        ALOGE("%s: Error during exit, rc = %d", __func__, rc);
        return rc;
    }

    /* wait until cmd thread exits */
    if (pthread_join(cmd_pid, NULL) != 0) {
        ALOGD("%s: pthread dead already\n", __func__);
    }
    cmd_pid = 0;
    return rc;
}

}; // namespace qcamera
