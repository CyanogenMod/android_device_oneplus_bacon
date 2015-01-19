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

#include "mm_qcamera_dbg.h"
#include "mm_qcamera_app.h"

int mm_camera_queue_init(mm_camera_queue_t *queue,
                         release_data_fn data_rel_fn,
                         void *user_data)
{
    if ( NULL == queue ) {
        return -1;
    }

    pthread_mutex_init(&queue->m_lock, NULL);
    cam_list_init(&queue->m_head.list);
    queue->m_size = 0;
    queue->m_dataFn = data_rel_fn;
    queue->m_userData = user_data;

    return MM_CAMERA_OK;
}

int mm_qcamera_queue_release(mm_camera_queue_t *queue)
{
    if ( NULL == queue ) {
        return -1;
    }

    mm_qcamera_queue_flush(queue);
    pthread_mutex_destroy(&queue->m_lock);

    return MM_CAMERA_OK;
}

int mm_qcamera_queue_isempty(mm_camera_queue_t *queue)
{
    if ( NULL == queue ) {
        return 0;
    }

    int flag = 1;
    pthread_mutex_lock(&queue->m_lock);
    if (queue->m_size > 0) {
        flag = 0;
    }
    pthread_mutex_unlock(&queue->m_lock);

    return flag;
}

int mm_qcamera_queue_enqueue(mm_camera_queue_t *queue, void *data)
{
    if ( NULL == queue ) {
        return -1;
    }

    camera_q_node *node =
        (camera_q_node *)malloc(sizeof(camera_q_node));
    if (NULL == node) {
        CDBG_ERROR("%s: No memory for camera_q_node", __func__);
        return 0;
    }

    memset(node, 0, sizeof(camera_q_node));
    node->data = data;

    pthread_mutex_lock(&queue->m_lock);
    cam_list_add_tail_node(&node->list, &queue->m_head.list);
    queue->m_size++;
    pthread_mutex_unlock(&queue->m_lock);

    return 1;
}

void* mm_qcamera_queue_dequeue(mm_camera_queue_t *queue, int bFromHead)
{
    if ( NULL == queue ) {
        return NULL;
    }

    camera_q_node* node = NULL;
    void* data = NULL;
    struct cam_list *head = NULL;
    struct cam_list *pos = NULL;

    pthread_mutex_lock(&queue->m_lock);
    head = &queue->m_head.list;
    if (bFromHead) {
        pos = head->next;
    } else {
        pos = head->prev;
    }
    if (pos != head) {
        node = member_of(pos, camera_q_node, list);
        cam_list_del_node(&node->list);
        queue->m_size--;
    }
    pthread_mutex_unlock(&queue->m_lock);

    if (NULL != node) {
        data = node->data;
        free(node);
    }

    return data;
}

void mm_qcamera_queue_flush(mm_camera_queue_t *queue)
{
    camera_q_node* node = NULL;
    struct cam_list *head = NULL;
    struct cam_list *pos = NULL;

    if ( NULL == queue ) {
        return;
    }

    pthread_mutex_lock(&queue->m_lock);
    head = &queue->m_head.list;
    pos = head->next;

    while(pos != head) {
        node = member_of(pos, camera_q_node, list);
        pos = pos->next;
        cam_list_del_node(&node->list);
        queue->m_size--;

        if (NULL != node->data) {
            if (queue->m_dataFn) {
                queue->m_dataFn(node->data, queue->m_userData);
            }
            free(node->data);
        }
        free(node);

    }
    queue->m_size = 0;
    pthread_mutex_unlock(&queue->m_lock);
}

