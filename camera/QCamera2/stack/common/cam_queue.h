/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include "cam_list.h"

typedef struct {
    struct cam_list list;
    void *data;
} cam_node_t;

typedef struct {
    cam_node_t head; /* dummy head */
    uint32_t size;
    pthread_mutex_t lock;
} cam_queue_t;

static inline int32_t cam_queue_init(cam_queue_t *queue)
{
    pthread_mutex_init(&queue->lock, NULL);
    cam_list_init(&queue->head.list);
    queue->size = 0;
    return 0;
}

static inline int32_t cam_queue_enq(cam_queue_t *queue, void *data)
{
    cam_node_t *node =
        (cam_node_t *)malloc(sizeof(cam_node_t));
    if (NULL == node) {
        return -1;
    }

    memset(node, 0, sizeof(cam_node_t));
    node->data = data;

    pthread_mutex_lock(&queue->lock);
    cam_list_add_tail_node(&node->list, &queue->head.list);
    queue->size++;
    pthread_mutex_unlock(&queue->lock);

    return 0;
}

static inline void *cam_queue_deq(cam_queue_t *queue)
{
    cam_node_t *node = NULL;
    void *data = NULL;
    struct cam_list *head = NULL;
    struct cam_list *pos = NULL;

    pthread_mutex_lock(&queue->lock);
    head = &queue->head.list;
    pos = head->next;
    if (pos != head) {
        node = member_of(pos, cam_node_t, list);
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

static inline int32_t cam_queue_flush(cam_queue_t *queue)
{
    cam_node_t *node = NULL;
    struct cam_list *head = NULL;
    struct cam_list *pos = NULL;

    pthread_mutex_lock(&queue->lock);
    head = &queue->head.list;
    pos = head->next;

    while(pos != head) {
        node = member_of(pos, cam_node_t, list);
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

static inline int32_t cam_queue_deinit(cam_queue_t *queue)
{
    cam_queue_flush(queue);
    pthread_mutex_destroy(&queue->lock);
    return 0;
}
