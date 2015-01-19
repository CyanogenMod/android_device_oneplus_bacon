/*
Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.

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

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/ioctl.h>
struct file;
struct inode;
#include <linux/android_pmem.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/time.h>
#include <string.h>

#include <inttypes.h>
#include <linux/msm_mdp.h>
#include <linux/fb.h>
#include "camera.h"
#include "mm_camera_dbg.h"

#ifdef DRAW_RECTANGLES
extern roi_info_t camframe_roi;

#undef CAM_FRM_DRAW_RECT
#define CAM_FRM_DRAW_RECT
#endif

#ifdef CAM_FRM_DRAW_FD_RECT
#undef CAM_FRM_DRAW_RECT
#define CAM_FRM_DRAW_RECT
#endif

struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
int fb_fd = 0;
union {
  char dummy[sizeof(struct mdp_blit_req_list) +
    sizeof(struct mdp_blit_req) * 1];
  struct mdp_blit_req_list list;
} yuv;

static pthread_t cam_frame_fb_thread_id;
static int camframe_fb_exit;

static int is_camframe_fb_thread_ready;
USER_INPUT_DISPLAY_T input_display;

unsigned use_overlay = 0;
struct msmfb_overlay_data ov_front, ov_back, *ovp_front, *ovp_back;
struct mdp_overlay overlay, *overlayp;
int vid_buf_front_id, vid_buf_back_id;
static unsigned char please_initialize = 1;
int num_of_ready_frames = 0;

static pthread_cond_t  sub_thread_ready_cond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t sub_thread_ready_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  camframe_fb_cond  = PTHREAD_COND_INITIALIZER;
pthread_mutex_t camframe_fb_mutex = PTHREAD_MUTEX_INITIALIZER;
static void notify_camframe_fb_thread();

void use_overlay_fb_display_driver(void)
{
  use_overlay = 1;
}

void overlay_set_params(struct mdp_blit_req *e)
{
  int result;

  if (please_initialize) {
    overlayp = &overlay;
    ovp_front = &ov_front;
    ovp_back = &ov_back;

    overlayp->id = MSMFB_NEW_REQUEST;
  }

  overlayp->src.width  = e->src.width;
  overlayp->src.height = e->src.height;
  overlayp->src.format = e->src.format;

  overlayp->src_rect.x = e->src_rect.x;
  overlayp->src_rect.y = e->src_rect.y;
  overlayp->src_rect.w = e->src_rect.w;
  overlayp->src_rect.h = e->src_rect.h;

  overlayp->dst_rect.x = e->dst_rect.x;
  overlayp->dst_rect.y = e->dst_rect.y;
  /* ROTATOR is enabled in overlay library, swap dimensions
     here to take care of that */
  overlayp->dst_rect.w = e->dst_rect.h;
  overlayp->dst_rect.h = e->dst_rect.w;

  if (overlayp->dst_rect.w > 480)
    overlayp->dst_rect.w = 480;
  if (overlayp->dst_rect.h > 800)
    overlayp->dst_rect.h = 800;

  overlayp->z_order = 0; // FB_OVERLAY_VID_0;
  overlayp->alpha = e->alpha;
  overlayp->transp_mask = 0; /* 0xF81F */
  overlayp->flags = e->flags;
  overlayp->is_fg = 1;

  if (please_initialize) {
    CDBG("src.width %d height %d; src_rect.x %d y %d w %d h %d; dst_rect.x %d y %d w %d h %d\n",
      overlayp->src.width, overlayp->src.height,
      overlayp->src_rect.x, overlayp->src_rect.y, overlayp->src_rect.w, overlayp->src_rect.h,
      overlayp->dst_rect.x, overlayp->dst_rect.y, overlayp->dst_rect.w, overlayp->dst_rect.h
      );

    result = ioctl(fb_fd, MSMFB_OVERLAY_SET, overlayp);
    if (result < 0) {
      CDBG("ERROR: MSMFB_OVERLAY_SET failed!, result =%d\n", result);
    }
  }

  if (please_initialize) {
    vid_buf_front_id = overlayp->id; /* keep return id */

    ov_front.id = overlayp->id;
    ov_back.id = overlayp->id;
    please_initialize = 0;
  }

  return;
}

void overlay_set_frame(struct msm_frame *frame)
{
  ov_front.data.offset = 0;
  ov_front.data.memory_id = frame->fd;
  return;
}

/*===========================================================================
 * FUNCTION     test_app_camframe_callback
 * DESCRIPTION  display frame
 *==========================================================================*/
void test_app_camframe_callback(struct msm_frame *frame)
{
  int result = 0;
  struct mdp_blit_req *e;
  struct timeval td1, td2;
  struct timezone tz;

  common_crop_t *crop = (common_crop_t *) (frame->cropinfo);

  /* Initialize yuv structure */
  yuv.list.count = 1;

  e = &yuv.list.req[0];

  e->src.width = input_display.user_input_display_width;
  e->src.height = input_display.user_input_display_height;
  e->src.format = MDP_Y_CRCB_H2V2;
  e->src.offset = 0;
  e->src.memory_id = frame->fd;

  e->dst.width = vinfo.xres;
  e->dst.height = vinfo.yres;
  e->dst.format = MDP_RGB_565;
  e->dst.offset = 0;
  e->dst.memory_id = fb_fd;

  e->transp_mask = 0xffffffff;
  e->flags = 0;
  e->alpha = 0xff;

  /* Starting doing MDP Cropping */
  if (frame->path == OUTPUT_TYPE_P) {

    if (crop->in2_w != 0 || crop->in2_h != 0) {

      e->src_rect.x = (crop->out2_w - crop->in2_w + 1) / 2 - 1;

      e->src_rect.y = (crop->out2_h - crop->in2_h + 1) / 2 - 1;

      e->src_rect.w = crop->in2_w;
      e->src_rect.h = crop->in2_h;

      CDBG("e->src_rect.x = %d\n", e->src_rect.x);
      CDBG("e->src_rect.y = %d\n", e->src_rect.y);
      CDBG("e->src_rect.w = %d\n", e->src_rect.w);
      CDBG("e->src_rect.h = %d\n", e->src_rect.h);

      e->dst_rect.x = 0;
      e->dst_rect.y = 0;
      e->dst_rect.w = input_display.user_input_display_width;
      e->dst_rect.h = input_display.user_input_display_height;
    } else {
      e->src_rect.x = 0;
      e->src_rect.y = 0;
      e->src_rect.w = input_display.user_input_display_width;
      e->src_rect.h = input_display.user_input_display_height;

      e->dst_rect.x = 0;
      e->dst_rect.y = 0;
      e->dst_rect.w = input_display.user_input_display_width;
      e->dst_rect.h = input_display.user_input_display_height;
    }
    if (use_overlay) overlay_set_params(e);
  } else {

  }

  gettimeofday(&td1, &tz);

  if (use_overlay) overlay_set_frame(frame);
  else {
    result = ioctl(fb_fd, MSMFB_BLIT, &yuv.list);
    if (result < 0) {
      CDBG("MSM_FBIOBLT failed! line=%d\n", __LINE__);
    }
  }

  gettimeofday(&td2, &tz);
  CDBG("Profiling: MSMFB_BLIT takes %ld microseconds\n",
    ((td2.tv_sec - td1.tv_sec) * 1000000 + (td2.tv_usec - td1.tv_usec)));

  td1 = td2;
  notify_camframe_fb_thread();
  /* add frame back to the free queue*/
  //camframe_add_frame(CAM_PREVIEW_FRAME, frame);
}

void notify_camframe_fb_thread()
{
  pthread_mutex_lock(&camframe_fb_mutex);

  num_of_ready_frames ++;
  pthread_cond_signal(&camframe_fb_cond);

  pthread_mutex_unlock(&camframe_fb_mutex);
}

void camframe_fb_thread_ready_signal(void);

void *camframe_fb_thread(void *data)
{
  int result = 0;
  static struct timeval td1, td2;
  struct timezone tz;

#ifdef _ANDROID_
  fb_fd = open(ANDROID_FB0, O_RDWR);
  CDBG("%s:android dl '%s', fd=%d\n", __func__, ANDROID_FB0, fb_fd);
#else
  fb_fd = open(LE_FB0, O_RDWR);
  CDBG("%s:LE_FB0 dl, '%s', fd=%d\n", __func__, LE_FB0, fb_fd);
#endif
  if (fb_fd < 0) {
    CDBG_ERROR("cannot open framebuffer %s or %s file node\n",
      ANDROID_FB0, LE_FB0);
    goto fail1;
  }

  if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
    CDBG_ERROR("cannot retrieve vscreenInfo!\n");
    goto fail;
  }

  if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
    CDBG_ERROR("can't retrieve fscreenInfo!\n");
    goto fail;
  }

  vinfo.activate = FB_ACTIVATE_VBL;

  camframe_fb_thread_ready_signal();

  pthread_mutex_lock(&camframe_fb_mutex);
  while (!camframe_fb_exit) {
    CDBG("cam_frame_fb_thread: num_of_ready_frames: %d\n", num_of_ready_frames);
    if (num_of_ready_frames <= 0) {
      pthread_cond_wait(&camframe_fb_cond, &camframe_fb_mutex);
    }
    if (num_of_ready_frames > 0) {
      num_of_ready_frames --;

      gettimeofday(&td1, &tz);
      if (use_overlay) {
        result = ioctl(fb_fd, MSMFB_OVERLAY_PLAY, ovp_front);
      } else {
        result = ioctl(fb_fd, FBIOPAN_DISPLAY, &vinfo);
      }

      gettimeofday(&td2, &tz);

      CDBG("Profiling: frame timestamp after FBIO display = %ld ms\n",
        (td2.tv_sec*1000) + (td2.tv_usec/1000));

      CDBG("cam_frame_fb_thread: elapse time for FBIOPAN_DISPLAY = %ld, return = %d\n",
        (td2.tv_sec - td1.tv_sec) * 1000000 + td2.tv_usec - td1.tv_usec, result);

      if (result < 0) {
        CDBG("DISPLAY: Failed\n");
      }
    }
  }

  pthread_mutex_unlock(&camframe_fb_mutex);

  if (use_overlay) {
    if (ioctl(fb_fd, MSMFB_OVERLAY_UNSET, &vid_buf_front_id)) {
      CDBG("\nERROR! MSMFB_OVERLAY_UNSET failed! (Line %d)\n", __LINE__);
      goto fail;
    }
  }

  return NULL;

  fail:
  close(fb_fd);
  fail1:
  camframe_fb_exit = -1;
  camframe_fb_thread_ready_signal();
  return NULL;
}

int launch_camframe_fb_thread(void)
{

  camframe_fb_exit = 0;
  is_camframe_fb_thread_ready = 0;
  pthread_create(&cam_frame_fb_thread_id, NULL, camframe_fb_thread, NULL);

  /* Waiting for launching sub thread ready signal. */
  CDBG("launch_camframe_fb_thread(), call pthread_cond_wait\n");

  pthread_mutex_lock(&sub_thread_ready_mutex);
  if (!is_camframe_fb_thread_ready) {
    pthread_cond_wait(&sub_thread_ready_cond, &sub_thread_ready_mutex);
  }
  pthread_mutex_unlock(&sub_thread_ready_mutex);

  CDBG("launch_camframe_fb_thread(), call pthread_cond_wait done\n");
  CDBG("%s:fb rc=%d\n", __func__, camframe_fb_exit);
  return camframe_fb_exit;
}

void release_camframe_fb_thread(void)
{
  camframe_fb_exit = 1;
  please_initialize = 1;

  /* Notify the camframe fb thread to wake up */
  if (cam_frame_fb_thread_id != 0) {
     pthread_mutex_lock(&camframe_fb_mutex);
     pthread_cond_signal(&camframe_fb_cond);
     pthread_mutex_unlock(&camframe_fb_mutex);
     if (pthread_join(cam_frame_fb_thread_id, NULL) != 0) {
       CDBG("cam_frame_fb_thread exit failure!\n");
     }
     close(fb_fd);
  }
}

void camframe_fb_thread_ready_signal(void)
{
  /* Send the signal to control thread to indicate that the cam frame fb
   * ready.
   */
  CDBG("cam_frame_fb_thread() is ready, call pthread_cond_signal\n");

  pthread_mutex_lock(&sub_thread_ready_mutex);
  is_camframe_fb_thread_ready = 1;
  pthread_cond_signal(&sub_thread_ready_cond);
  pthread_mutex_unlock(&sub_thread_ready_mutex);

  CDBG("cam_frame_fb_thread() is ready, call pthread_cond_signal done\n");
}

#ifdef CAM_FRM_DRAW_RECT
void draw_rect(char *buf, int buf_w,
  int x, int y, int dx, int dy)
{
  int i;
  int left   = x;
  int right  = x+dx;
  int top    = y;
  int bottom = y+dy;

  for (i = left; i < right; i++) {
    buf[top*buf_w+i] = 0xff;
    buf[bottom*buf_w+i] = 0xff;
  }
  for (i = top; i < bottom; i++) {
    buf[i*buf_w+left] = 0xff;
    buf[i*buf_w+right] = 0xff;
  }
}
#endif

void draw_rectangles(struct msm_frame* newFrame)
{
  struct fd_roi_t *p_fd_roi;
#ifdef DRAW_RECTANGLES
  uint8_t i;
  for (i = 0; i < camframe_roi.num_roi; i++) {
    CDBG("%s: camframe_roi: i=%d, x=%d, y=%d, dx=%d, dy=%d\n", __func__,
      i, camframe_roi.roi[i].x, camframe_roi.roi[i].y,
      camframe_roi.roi[i].dx, camframe_roi.roi[i].dy);
    draw_rect((char*)newFrame->buffer, 640,
      camframe_roi.roi[i].x, camframe_roi.roi[i].y,
      camframe_roi.roi[i].dx, camframe_roi.roi[i].dy);
  }
#endif

#ifdef CAM_FRM_DRAW_FD_RECT
  p_fd_roi = (struct fd_roi_t *)newFrame->roi_info.info;
  if(p_fd_roi && p_fd_roi->rect_num > 0){
    int i;
    for(i =0; i < p_fd_roi->rect_num; i++)
    {
      draw_rect((char*)newFrame->buffer, 800,
        p_fd_roi->faces[i].x, p_fd_roi->faces[i].y,
        p_fd_roi->faces[i].dx, p_fd_roi->faces[i].dy);
    }
  }
#endif
}

/*===========================================================================
 * FUNCTION    - v4l2_render -
 *
 * DESCRIPTION:
 *==========================================================================*/
int v4l2_render(int frame_fd, struct v4l2_buffer *vb, struct v4l2_crop *crop)
{
  struct mdp_blit_req *e;
  /* Initialize yuv structure */
  yuv.list.count = 1;
  e = &yuv.list.req[0];

  e->src.width = input_display.user_input_display_width;
  e->src.height = input_display.user_input_display_height;
  e->src.format = MDP_Y_CBCR_H2V2;
  e->src.offset = 0;
  e->src.memory_id = frame_fd;

  e->dst.width = vinfo.xres;
  e->dst.height = vinfo.yres;
  e->dst.format = MDP_RGB_565;
  e->dst.offset = 0;
  e->dst.memory_id = fb_fd;

  e->transp_mask = 0xffffffff;
  e->flags = 0;
  e->alpha = 0xff;

 if (crop != NULL && (crop->c.width != 0 || crop->c.height != 0)) {
    e->src_rect.x = crop->c.left;
    e->src_rect.y = crop->c.top;
    e->src_rect.w = crop->c.width;
    e->src_rect.h = crop->c.height;

    e->dst_rect.x = 0;
    e->dst_rect.y = 0;
    e->dst_rect.w = input_display.user_input_display_width;
    e->dst_rect.h = input_display.user_input_display_height;
  } else {
    e->dst_rect.x = 0;
    e->dst_rect.y = 0;
    e->dst_rect.w  = input_display.user_input_display_width;
    e->dst_rect.h  = input_display.user_input_display_height;

    e->src_rect.x = 0;
    e->src_rect.y = 0;
    e->src_rect.w  = input_display.user_input_display_width;
    e->src_rect.h  = input_display.user_input_display_height;
  }
  overlay_set_params(e);
  ov_front.data.offset = 0;
  ov_front.data.memory_id = frame_fd;
  notify_camframe_fb_thread();

  return TRUE;
}

int mm_app_dl_render(int frame_fd, struct crop_info * cropinfo)
{
  struct mdp_blit_req *e;
  int croplen = 0;
  //struct crop_info *cropinfo;
  common_crop_t *crop;

  //cropinfo = (struct crop_info *)vb->input;
  if(cropinfo != NULL) {
    crop = (common_crop_t *)cropinfo->info;
  }
  /* Initialize yuv structure */
  yuv.list.count = 1;
  e = &yuv.list.req[0];

  e->src.width = input_display.user_input_display_width;
  e->src.height = input_display.user_input_display_height;
  e->src.format = MDP_Y_CRCB_H2V2;
  e->src.offset = 0;
  e->src.memory_id = frame_fd;

  e->dst.width = vinfo.xres;
  e->dst.height = vinfo.yres;
  e->dst.format = MDP_RGB_565;
  e->dst.offset = 0;
  e->dst.memory_id = fb_fd;

  e->transp_mask = 0xffffffff;
  e->flags = 0;
  e->alpha = 0xff;

  if (cropinfo != NULL && (crop->in2_w != 0 || crop->in2_h != 0)) {
    e->src_rect.x = (crop->out2_w - crop->in2_w + 1) / 2 - 1;
    e->src_rect.y = (crop->out2_h - crop->in2_h + 1) / 2 - 1;
    e->src_rect.w = crop->in2_w;
    e->src_rect.h = crop->in2_h;

    e->dst_rect.x = 0;
    e->dst_rect.y = 0;
    e->dst_rect.w = input_display.user_input_display_width;
    e->dst_rect.h = input_display.user_input_display_height;
  } else {
    e->dst_rect.x = 0;
    e->dst_rect.y = 0;
    e->dst_rect.w  = input_display.user_input_display_width;
    e->dst_rect.h  = input_display.user_input_display_height;

    e->src_rect.x = 0;
    e->src_rect.y = 0;
    e->src_rect.w  = input_display.user_input_display_width;
    e->src_rect.h  = input_display.user_input_display_height;
  }

  overlay_set_params(e);

  ov_front.data.offset = 0;
  ov_front.data.memory_id = frame_fd;
  notify_camframe_fb_thread();

  return TRUE;
}


