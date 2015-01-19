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

#include <pthread.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <camera.h>
#include <semaphore.h>

#include "mm_camera_dbg.h"
#include "mm_camera_interface.h"
#include "mm_camera.h"

#if 0
#undef CDBG
#undef LOG_TAG
#define CDBG ALOGE
#define LOG_TAG "NotifyLogs"
#endif

int32_t mm_camera_queue_init(mm_camera_queue_t* queue)
{
    pthread_mutex_init(&queue->lock, NULL);
    cam_list_init(&queue->head.list);
    queue->size = 0;
    return 0;
}

int32_t mm_camera_queue_enq(mm_camera_queue_t* queue, void* data)
{
    mm_camera_q_node_t* node =
        (mm_camera_q_node_t *)malloc(sizeof(mm_camera_q_node_t));
    if (NULL == node) {
        CDBG_ERROR("%s: No memory for mm_camera_q_node_t", __func__);
        return -1;
    }

    memset(node, 0, sizeof(mm_camera_q_node_t));
    node->data = data;

    pthread_mutex_lock(&queue->lock);
    cam_list_add_tail_node(&node->list, &queue->head.list);
    queue->size++;
    pthread_mutex_unlock(&queue->lock);

    return 0;

}

void* mm_camera_queue_deq(mm_camera_queue_t* queue)
{
    mm_camera_q_node_t* node = NULL;
    void* data = NULL;
    struct cam_list *head = NULL;
    struct cam_list *pos = NULL;

    pthread_mutex_lock(&queue->lock);
    head = &queue->head.list;
    pos = head->next;
    if (pos != head) {
        node = member_of(pos, mm_camera_q_node_t, list);
        cam_list_del_node(&node->list);
        queue->size--;
    }
    pthread_mutex_unlock(&queue->lock);

    if (NULL != node) {
        data = node->data;
        free(node);
    }

    return data;
}

int32_t mm_camera_queue_deinit(mm_camera_queue_t* queue)
{
    mm_camera_queue_flush(queue);
    pthread_mutex_destroy(&queue->lock);
    return 0;
}

int32_t mm_camera_queue_flush(mm_camera_queue_t* queue)
{
    mm_camera_q_node_t* node = NULL;
    void* data = NULL;
    struct cam_list *head = NULL;
    struct cam_list *pos = NULL;

    pthread_mutex_lock(&queue->lock);
    head = &queue->head.list;
    pos = head->next;

    while(pos != head) {
        node = member_of(pos, mm_camera_q_node_t, list);
        pos = pos->next;
        cam_list_del_node(&node->list);
        queue->size--;

        /* TODO later to consider ptr inside data */
        /* for now we only assume there is no ptr inside data
         * so we free data directly */
        if (NULL != node->data) {
            free(node->data);
        }
        free(node);

    }
    queue->size = 0;
    pthread_mutex_unlock(&queue->lock);
    return 0;
}

static void *mm_camera_cmd_thread(void *data)
{
    int rc = 0;
    int running = 1;
    int ret;
    mm_camera_cmd_thread_t *cmd_thread =
                (mm_camera_cmd_thread_t *)data;
    mm_camera_cmdcb_t* node = NULL;

    do {
        do {
            ret = sem_wait(&cmd_thread->cmd_sem);
            if (ret != 0 && errno != EINVAL) {
                CDBG_ERROR("%s: sem_wait error (%s)",
                           __func__, strerror(errno));
                return NULL;
            }
        } while (ret != 0);

        /* we got notified about new cmd avail in cmd queue */
        node = (mm_camera_cmdcb_t*)mm_camera_queue_deq(&cmd_thread->cmd_queue);
        while (node != NULL) {
            switch (node->cmd_type) {
            case MM_CAMERA_CMD_TYPE_EVT_CB:
            case MM_CAMERA_CMD_TYPE_DATA_CB:
            case MM_CAMERA_CMD_TYPE_ASYNC_CB:
            case MM_CAMERA_CMD_TYPE_REQ_DATA_CB:
            case MM_CAMERA_CMD_TYPE_SUPER_BUF_DATA_CB:
                if (NULL != cmd_thread->cb) {
                    cmd_thread->cb(node, cmd_thread->user_data);
                }
                break;
            case MM_CAMERA_CMD_TYPE_EXIT:
            default:
                running = 0;
                break;
            }
            free(node);
            node = (mm_camera_cmdcb_t*)mm_camera_queue_deq(&cmd_thread->cmd_queue);
        } /* (node != NULL) */
    } while (running);
    return NULL;
}

int32_t mm_camera_cmd_thread_launch(mm_camera_cmd_thread_t * cmd_thread,
                                    mm_camera_cmd_cb_t cb,
                                    void* user_data)
{
    int32_t rc = 0;

    sem_init(&cmd_thread->cmd_sem, 0, 0);
    mm_camera_queue_init(&cmd_thread->cmd_queue);
    cmd_thread->cb = cb;
    cmd_thread->user_data = user_data;

    /* launch the thread */
    pthread_create(&cmd_thread->cmd_pid,
                   NULL,
                   mm_camera_cmd_thread,
                   (void *)cmd_thread);
    return rc;
}

int32_t mm_camera_cmd_thread_stop(mm_camera_cmd_thread_t * cmd_thread)
{
    int32_t rc = 0;
    mm_camera_buf_info_t  buf_info;
    mm_camera_cmdcb_t* node = (mm_camera_cmdcb_t *)malloc(sizeof(mm_camera_cmdcb_t));
    if (NULL == node) {
        CDBG_ERROR("%s: No memory for mm_camera_cmdcb_t", __func__);
        return -1;
    }

    memset(node, 0, sizeof(mm_camera_cmdcb_t));
    node->cmd_type = MM_CAMERA_CMD_TYPE_EXIT;

    mm_camera_queue_enq(&cmd_thread->cmd_queue, node);
    sem_post(&cmd_thread->cmd_sem);

    /* wait until cmd thread exits */
    if (pthread_join(cmd_thread->cmd_pid, NULL) != 0) {
        CDBG("%s: pthread dead already\n", __func__);
    }
    return rc;
}

int32_t mm_camera_cmd_thread_destroy(mm_camera_cmd_thread_t * cmd_thread)
{
    int32_t rc = 0;
    mm_camera_queue_deinit(&cmd_thread->cmd_queue);
    sem_destroy(&cmd_thread->cmd_sem);
    memset(cmd_thread, 0, sizeof(mm_camera_cmd_thread_t));
    return rc;
}

int32_t mm_camera_cmd_thread_release(mm_camera_cmd_thread_t * cmd_thread)
{
    int32_t rc = 0;
    rc = mm_camera_cmd_thread_stop(cmd_thread);
    if (0 == rc) {
        rc = mm_camera_cmd_thread_destroy(cmd_thread);
    }
    return rc;
}

