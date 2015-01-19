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
#include "mm_jpeg_dbg.h"
#include "mm_jpeg.h"

int32_t mm_jpeg_queue_init(mm_jpeg_queue_t* queue)
{
    pthread_mutex_init(&queue->lock, NULL);
    cam_list_init(&queue->head.list);
    queue->size = 0;
    return 0;
}

int32_t mm_jpeg_queue_enq(mm_jpeg_queue_t* queue, void* data)
{
    mm_jpeg_q_node_t* node =
        (mm_jpeg_q_node_t *)malloc(sizeof(mm_jpeg_q_node_t));
    if (NULL == node) {
        CDBG_ERROR("%s: No memory for mm_jpeg_q_node_t", __func__);
        return -1;
    }

    memset(node, 0, sizeof(mm_jpeg_q_node_t));
    node->data = data;

    pthread_mutex_lock(&queue->lock);
    cam_list_add_tail_node(&node->list, &queue->head.list);
    queue->size++;
    pthread_mutex_unlock(&queue->lock);

    return 0;

}


void* mm_jpeg_queue_peek(mm_jpeg_queue_t* queue)
{
    mm_jpeg_q_node_t* node = NULL;
    void* data = NULL;
    struct cam_list *head = NULL;
    struct cam_list *pos = NULL;

    pthread_mutex_lock(&queue->lock);
    head = &queue->head.list;
    pos = head->next;
    if (pos != head) {
        node = member_of(pos, mm_jpeg_q_node_t, list);
    }
    pthread_mutex_unlock(&queue->lock);

    if (NULL != node) {
        data = node->data;
    }
    return data;
}

void* mm_jpeg_queue_deq(mm_jpeg_queue_t* queue)
{
    mm_jpeg_q_node_t* node = NULL;
    void* data = NULL;
    struct cam_list *head = NULL;
    struct cam_list *pos = NULL;

    pthread_mutex_lock(&queue->lock);
    head = &queue->head.list;
    pos = head->next;
    if (pos != head) {
        node = member_of(pos, mm_jpeg_q_node_t, list);
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

uint32_t mm_jpeg_queue_get_size(mm_jpeg_queue_t* queue)
{
    uint32_t size = 0;

    pthread_mutex_lock(&queue->lock);
    size = queue->size;
    pthread_mutex_unlock(&queue->lock);

    return size;

}

int32_t mm_jpeg_queue_deinit(mm_jpeg_queue_t* queue)
{
    mm_jpeg_queue_flush(queue);
    pthread_mutex_destroy(&queue->lock);
    return 0;
}

int32_t mm_jpeg_queue_flush(mm_jpeg_queue_t* queue)
{
    mm_jpeg_q_node_t* node = NULL;
    void* data = NULL;
    struct cam_list *head = NULL;
    struct cam_list *pos = NULL;

    pthread_mutex_lock(&queue->lock);
    head = &queue->head.list;
    pos = head->next;

    while(pos != head) {
        node = member_of(pos, mm_jpeg_q_node_t, list);
        cam_list_del_node(&node->list);
        queue->size--;

        /* for now we only assume there is no ptr inside data
         * so we free data directly */
        if (NULL != node->data) {
            free(node->data);
        }
        free(node);
        pos = pos->next;
    }
    queue->size = 0;
    pthread_mutex_unlock(&queue->lock);
    return 0;
}
