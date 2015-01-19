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

#ifndef __QCAMERA_SEMAPHORE_H__
#define __QCAMERA_SEMAPHORE_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Implement semaphore with mutex and conditional variable.
 * Reason being, POSIX semaphore on Android are not used or
 * well tested.
 */

typedef struct {
    int val;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} cam_semaphore_t;

static inline void cam_sem_init(cam_semaphore_t *s, int n)
{
    pthread_mutex_init(&(s->mutex), NULL);
    pthread_cond_init(&(s->cond), NULL);
    s->val = n;
}

static inline void cam_sem_post(cam_semaphore_t *s)
{
    pthread_mutex_lock(&(s->mutex));
    s->val++;
    pthread_cond_signal(&(s->cond));
    pthread_mutex_unlock(&(s->mutex));
}

static inline int cam_sem_wait(cam_semaphore_t *s)
{
    int rc = 0;
    pthread_mutex_lock(&(s->mutex));
    while (s->val == 0)
        rc = pthread_cond_wait(&(s->cond), &(s->mutex));
    s->val--;
    pthread_mutex_unlock(&(s->mutex));
    return rc;
}

static inline void cam_sem_destroy(cam_semaphore_t *s)
{
    pthread_mutex_destroy(&(s->mutex));
    pthread_cond_destroy(&(s->cond));
    s->val = 0;
}

#ifdef __cplusplus
}
#endif

#endif /* __QCAMERA_SEMAPHORE_H__ */
