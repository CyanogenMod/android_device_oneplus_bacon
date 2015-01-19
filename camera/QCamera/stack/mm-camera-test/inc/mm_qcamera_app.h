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

#ifndef __MM_QCAMERA_APP_H__
#define __MM_QCAMERA_APP_H__

#include "camera.h"
#include "mm_qcamera_main_menu.h"
#include "mm_camera_interface.h"
#include "mm_jpeg_interface.h"

#define MM_QCAMERA_APP_INTERATION 1

#define MM_APP_MAX_DUMP_FRAME_NUM 1000

#define PREVIEW_BUF_NUM 7
#define VIDEO_BUF_NUM 7
#define ISP_PIX_BUF_NUM 9

#define STATS_BUF_NUM 4

typedef enum {
    MM_CAMERA_OK,
    MM_CAMERA_E_GENERAL,
    MM_CAMERA_E_NO_MEMORY,
    MM_CAMERA_E_NOT_SUPPORTED,
    MM_CAMERA_E_INVALID_INPUT,
    MM_CAMERA_E_INVALID_OPERATION, /* 5 */
    MM_CAMERA_E_ENCODE,
    MM_CAMERA_E_BUFFER_REG,
    MM_CAMERA_E_PMEM_ALLOC,
    MM_CAMERA_E_CAPTURE_FAILED,
    MM_CAMERA_E_CAPTURE_TIMEOUT, /* 10 */
}mm_camera_status_type_t;

typedef struct {
    int num;
    mm_camera_buf_def_t bufs[MM_CAMERA_MAX_NUM_FRAMES];
    struct ion_allocation_data alloc[MM_CAMERA_MAX_NUM_FRAMES];
    struct ion_fd_data ion_info_fd[MM_CAMERA_MAX_NUM_FRAMES];
} mm_camear_app_buf_t;

typedef struct {
    uint32_t id;
    mm_camera_stream_config_t str_config;
    mm_camear_app_buf_t app_bufs;
}mm_camear_stream_t;

typedef enum{
    CAMERA_STATE_OPEN,
    CAMERA_STATE_PREVIEW,
    CAMERA_STATE_RECORD,
    CAMERA_STATE_SNAPSHOT,
    CAMERA_STATE_RDI
}camera_state;

typedef enum{
    CAMERA_MODE,
    RECORDER_MODE,
    ZSL_MODE,
    RDI_MODE
}camera_mode;

#define MM_QCAM_APP_MAX_STREAM_NUM MM_CAMERA_IMG_MODE_MAX

typedef struct {
    mm_camera_buf_def_t *frame;
    uint8_t ref_cnt;
} repro_src_buf_info;

typedef struct {
    struct cam_list list;
    void* data;
} mm_qcamera_q_node_t;

typedef struct {
    mm_qcamera_q_node_t head; /* dummy head */
    uint32_t size;
    pthread_mutex_t lock;
} mm_qcamera_queue_t;

typedef struct {
    mm_camera_vtbl_t *cam;
    mm_camear_mem_vtbl_t *mem_cam;
    int8_t my_id;
    //mm_camera_op_mode_type_t op_mode;
    uint32_t ch_id;
    cam_ctrl_dimension_t dim;
    int open_flag;
    int ionfd;
    mm_camear_stream_t stream[MM_QCAM_APP_MAX_STREAM_NUM];
    camera_mode cam_mode;
    camera_state cam_state;
    int fullSizeSnapshot;

    mm_jpeg_ops_t jpeg_ops;
    uint32_t jpeg_hdl;
    mm_camera_super_buf_t* current_job_frames;
    uint32_t current_job_id;
    mm_camear_app_buf_t jpeg_buf;

    uint32_t isp_repro_handle;
    uint8_t repro_dest_num;
    mm_qcamera_queue_t repro_q;
    repro_src_buf_info repro_buf_info[ISP_PIX_BUF_NUM];
    mm_camera_repro_config_data_t repro_config;
} mm_camera_app_obj_t;

typedef struct {
  void *ptr;
  void* ptr_jpeg;

  mm_camera_info_t *(*mm_camera_query) (uint8_t *num_cameras);
  mm_camera_vtbl_t *(*mm_camera_open) (uint8_t camera_idx,
                               mm_camear_mem_vtbl_t *mem_vtbl);
  uint32_t (*jpeg_open) (mm_jpeg_ops_t *ops);

  /*uint8_t *(*mm_camera_do_mmap)(uint32_t size, int *pmemFd);
  int (*mm_camera_do_munmap)(int pmem_fd, void *addr, size_t size);

   uint8_t *(*mm_camera_do_mmap_ion)(int ion_fd, struct ion_allocation_data *alloc,
             struct ion_fd_data *ion_info_fd, int *mapFd);
  int (*mm_camera_do_munmap_ion) (int ion_fd, struct ion_fd_data *ion_info_fd,
                   void *addr, size_t size);*/
#if 0
  uint32_t (*mm_camera_get_msm_frame_len)(cam_format_t fmt_type,
                                            camera_mode_t mode,
                                            int width,
                                            int height,
                                            int image_type,
                                            uint8_t *num_planes,
                                            uint32_t planes[]);
#ifndef DISABLE_JPEG_ENCODING
  void (*set_callbacks)(jpegfragment_callback_t fragcallback,
    jpeg_callback_t eventcallback, void* userdata, void* output_buffer,
    int * outBufferSize);
  int8_t (*omxJpegOpen)(void);
  void (*omxJpegClose)(void);
  int8_t (*omxJpegStart)(void);
  int8_t (*omxJpegEncode)(omx_jpeg_encode_params *encode_params);
  void (*omxJpegFinish)(void);
  int8_t (*mm_jpeg_encoder_setMainImageQuality)(uint32_t quality);
#endif
#endif
} hal_interface_lib_t;

typedef struct {
    //mm_camera_vtbl_t *cam;
    uint8_t num_cameras;
    mm_camera_app_obj_t *obj[2];
    mm_camera_info_t *cam_info;
    int use_overlay;
    int use_user_ptr;
    hal_interface_lib_t hal_lib;
    int cam_open;
    int run_sanity;
} mm_camera_app_t;

extern mm_camera_app_t my_cam_app;
extern USER_INPUT_DISPLAY_T input_display;
extern int mm_app_dl_render(int frame_fd, struct crop_info * cropinfo);
extern mm_camera_app_obj_t *mm_app_get_cam_obj(int8_t cam_id);
extern int mm_app_load_hal();
extern int mm_app_init();
extern void mm_app_user_ptr(int use_user_ptr);
extern int mm_app_open_ch(int cam_id);
extern void mm_app_close_ch(int cam_id, int ch_type);
extern int mm_app_set_dim(int8_t cam_id, cam_ctrl_dimension_t *dim);
extern int mm_app_run_unit_test();
extern int mm_app_unit_test_entry(mm_camera_app_t *cam_app);
extern int mm_app_unit_test();
extern void mm_app_set_dim_def(cam_ctrl_dimension_t *dim);
extern int mm_app_open(uint8_t camera_idx);
extern int mm_app_close(int8_t cam_id);
extern int startPreview(int cam_id);
extern int mm_app_stop_preview(int cam_id);
extern int mm_app_stop_video(int cam_id);
extern int startRecording(int cam_id);
extern int stopRecording(int cam_id);
extern int mm_app_take_picture(int cam_id);
extern int mm_app_take_raw_picture(int cam_id);
extern int mm_app_get_dim(int8_t cam_id, cam_ctrl_dimension_t *dim);
extern int mm_app_streamon_preview(int cam_id);
extern int mm_app_set_snapshot_fmt(int cam_id,mm_camera_image_fmt_t *fmt);

extern int mm_app_dual_test_entry(mm_camera_app_t *cam_app);
extern int mm_app_dual_test();

extern int mm_camera_app_wait();
extern void mm_camera_app_done();

extern int mm_stream_initbuf(uint32_t camera_handle,
                             uint32_t ch_id, uint32_t stream_id,
                             void *user_data,
                             mm_camera_frame_len_offset *frame_offset_info,
                             uint8_t num_bufs,
                             uint8_t *initial_reg_flag,
                             mm_camera_buf_def_t *bufs);

extern int mm_stream_deinitbuf(uint32_t camera_handle,
                             uint32_t ch_id, uint32_t stream_id,
                             void *user_data, uint8_t num_bufs,
                             mm_camera_buf_def_t *bufs);

extern void mm_app_preview_notify_cb(mm_camera_super_buf_t *bufs,
                                     void *user_data);
#endif /* __MM_QCAMERA_APP_H__ */









