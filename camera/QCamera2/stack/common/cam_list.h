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

/* This file is a slave copy from /vendor/qcom/propreitary/mm-cammera/common,
 * Please do not modify it directly here. */

#ifndef __CAMLIST_H
#define __CAMLIST_H

#include <stddef.h>

#define member_of(ptr, type, member) ({ \
  const typeof(((type *)0)->member) *__mptr = (ptr); \
  (type *)((char *)__mptr - offsetof(type,member));})

struct cam_list {
  struct cam_list *next, *prev;
};

static inline void cam_list_init(struct cam_list *ptr)
{
  ptr->next = ptr;
  ptr->prev = ptr;
}

static inline void cam_list_add_tail_node(struct cam_list *item,
  struct cam_list *head)
{
  struct cam_list *prev = head->prev;

  head->prev = item;
  item->next = head;
  item->prev = prev;
  prev->next = item;
}

static inline void cam_list_insert_before_node(struct cam_list *item,
  struct cam_list *node)
{
  item->next = node;
  item->prev = node->prev;
  item->prev->next = item;
  node->prev = item;
}

static inline void cam_list_del_node(struct cam_list *ptr)
{
  struct cam_list *prev = ptr->prev;
  struct cam_list *next = ptr->next;

  next->prev = ptr->prev;
  prev->next = ptr->next;
  ptr->next = ptr;
  ptr->prev = ptr;
}

#endif /* __CAMLIST_H */
