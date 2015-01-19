/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#ifndef __MM_CAMERA_INTERFACE_H__
#define __MM_CAMERA_INTERFACE_H__
#include <linux/msm_ion.h>
#include <linux/videodev2.h>
#include <media/msmb_camera.h>
#include "cam_intf.h"
#include "cam_queue.h"

#define MM_CAMERA_MAX_NUM_SENSORS MSM_MAX_CAMERA_SENSORS
#define MM_CAMERA_MAX_NUM_FRAMES CAM_MAX_NUM_BUFS_PER_STREAM
/* num of channels allowed in a camera obj */
#define MM_CAMERA_CHANNEL_MAX 16

#define PAD_TO_SIZE(size, padding) \
        ((size + (typeof(size))(padding - 1)) & \
        (typeof(size))(~(padding - 1)))

#define CAM_FN_CNT 255
/** CAM_DUMP_TO_FILE:
 *  @filename: file name
 *  @name:filename
 *  @index: index of the file
 *  @extn: file extension
 *  @p_addr: address of the buffer
 *  @len: buffer length
 *
 *  dump the image to the file
 **/
#define CAM_DUMP_TO_FILE(path, name, index, extn, p_addr, len) ({ \
  size_t rc = 0; \
  char filename[CAM_FN_CNT]; \
  if (index >= 0) \
    snprintf(filename, CAM_FN_CNT, "%s/%s%d.%s", path, name, index, extn); \
  else \
    snprintf(filename, CAM_FN_CNT, "%s/%s.%s", path, name, extn); \
  FILE *fp = fopen(filename, "w+"); \
  if (fp) { \
    rc = fwrite(p_addr, 1, len, fp); \
    ALOGE("%s:%d] written size %d", __func__, __LINE__, len); \
    fclose(fp); \
  } else { \
    ALOGE("%s:%d] open %s failed", __func__, __LINE__, filename); \
  } \
})

/** mm_camera_buf_def_t: structure for stream frame buf
*    @stream_id : stream handler to uniquely identify a stream
*               object
*    @buf_idx : index of the buf within the stream bufs, to be
*               filled during mem allocation
*    @timespec_ts : time stamp, to be filled when DQBUF is
*                 called
*    @frame_idx : frame sequence num, to be filled when DQBUF
*    @num_planes : num of planes for the frame buffer, to be
*               filled during mem allocation
*    @planes : plane info for the frame buffer, to be filled
*               during mem allocation
*    @fd : file descriptor of the frame buffer, to be filled
*        during mem allocation
*    @buffer : pointer to the frame buffer, to be filled during
*            mem allocation
*    @frame_len : length of the whole frame, to be filled during
*               mem allocation
*    @mem_info : user specific pointer to additional mem info
**/
typedef struct {
    uint32_t stream_id;
    cam_stream_type_t stream_type;
    uint32_t buf_idx;
    uint8_t is_uv_subsampled;
    struct timespec ts;
    uint32_t frame_idx;
    int8_t num_planes;
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    int fd;
    void *buffer;
    size_t frame_len;
    void *mem_info;
} mm_camera_buf_def_t;

/** mm_camera_super_buf_t: super buf structure for bundled
*   stream frames
*    @camera_handle : camera handler to uniquely identify
*              a camera object
*    @ch_id : channel handler to uniquely ideentify a channel
*           object
*    @num_bufs : number of buffers in the super buf, should not
*              exceeds MAX_STREAM_NUM_IN_BUNDLE
*    @bufs : array of buffers in the bundle
**/
typedef struct {
    uint32_t camera_handle;
    uint32_t ch_id;
    uint32_t num_bufs;
    mm_camera_buf_def_t* bufs[MAX_STREAM_NUM_IN_BUNDLE];
} mm_camera_super_buf_t;

/** mm_camera_event_t: structure for event
*    @server_event_type : event type from serer
*    @status : status of an event, value could be
*              CAM_STATUS_SUCCESS
*              CAM_STATUS_FAILED
**/
typedef struct {
    cam_event_type_t server_event_type;
    uint32_t status;
} mm_camera_event_t;

/** mm_camera_event_notify_t: function definition for event
*   notify handling
*    @camera_handle : camera handler
*    @evt : pointer to an event struct
*    @user_data: user data pointer
**/
typedef void (*mm_camera_event_notify_t)(uint32_t camera_handle,
                                         mm_camera_event_t *evt,
                                         void *user_data);

/** mm_camera_buf_notify_t: function definition for frame notify
*   handling
*    @mm_camera_super_buf_t : received frame buffers
*    @user_data: user data pointer
**/
typedef void (*mm_camera_buf_notify_t) (mm_camera_super_buf_t *bufs,
                                        void *user_data);

/** map_stream_buf_op_t: function definition for operation of
*   mapping stream buffers via domain socket
*    @frame_idx : buffer index within stream buffers
*    @plane_idx    : plane index. If all planes share the same
*                   fd, plane_idx = -1; otherwise, plean_idx is
*                   the index to plane (0..num_of_planes)
*    @fd : file descriptor of the stream buffer
*    @size: size of the stream buffer
*    @userdata : user data pointer
**/
typedef int32_t (*map_stream_buf_op_t) (uint32_t frame_idx,
                                        int32_t plane_idx,
                                        int fd,
                                        size_t size,
                                        void *userdata);

/** unmap_stream_buf_op_t: function definition for operation of
*                          unmapping stream buffers via domain
*                          socket
*    @frame_idx : buffer index within stream buffers
*    @plane_idx : plane index. If all planes share the same
*                 fd, plane_idx = -1; otherwise, plean_idx is
*                 the index to plane (0..num_of_planes)
*    @userdata : user data pointer
**/
typedef int32_t (*unmap_stream_buf_op_t) (uint32_t frame_idx,
                                          int32_t plane_idx,
                                          void *userdata);

/** mm_camera_map_unmap_ops_tbl_t: virtual table
*                      for mapping/unmapping stream buffers via
*                      domain socket
*    @map_ops : operation for mapping
*    @unmap_ops : operation for unmapping
*    @userdata: user data pointer
**/
typedef struct {
    map_stream_buf_op_t map_ops;
    unmap_stream_buf_op_t unmap_ops;
    void *userdata;
} mm_camera_map_unmap_ops_tbl_t;

/** mm_camera_stream_mem_vtbl_t: virtual table for stream
*                      memory allocation and deallocation
*    @get_bufs : function definition for allocating
*                stream buffers
*    @put_bufs : function definition for deallocating
*                stream buffers
*    @user_data: user data pointer
**/
typedef struct {
  void *user_data;
  int32_t (*get_bufs) (cam_frame_len_offset_t *offset,
                       uint8_t *num_bufs,
                       uint8_t **initial_reg_flag,
                       mm_camera_buf_def_t **bufs,
                       mm_camera_map_unmap_ops_tbl_t *ops_tbl,
                       void *user_data);
  int32_t (*put_bufs) (mm_camera_map_unmap_ops_tbl_t *ops_tbl,
                       void *user_data);
  int32_t (*invalidate_buf)(uint32_t index, void *user_data);
  int32_t (*clean_invalidate_buf)(uint32_t index, void *user_data);
} mm_camera_stream_mem_vtbl_t;

/** mm_camera_stream_config_t: structure for stream
*                              configuration
*    @stream_info : pointer to a stream info structure
*    @padding_info: padding info obtained from querycapability
*    @mem_tbl : memory operation table for
*              allocating/deallocating stream buffers
*    @stream_cb : callback handling stream frame notify
*    @userdata : user data pointer
**/
typedef struct {
    cam_stream_info_t *stream_info;
    cam_padding_info_t padding_info;
    mm_camera_stream_mem_vtbl_t mem_vtbl;
    mm_camera_buf_notify_t stream_cb;
    void *userdata;
} mm_camera_stream_config_t;

/** mm_camera_super_buf_notify_mode_t: enum for super uffer
*                                      notification mode
*    @MM_CAMERA_SUPER_BUF_NOTIFY_BURST :
*       ZSL use case: get burst of frames
*    @MM_CAMERA_SUPER_BUF_NOTIFY_CONTINUOUS :
*       get continuous frames: when the super buf is ready
*       dispatch it to HAL
**/
typedef enum {
    MM_CAMERA_SUPER_BUF_NOTIFY_BURST = 0,
    MM_CAMERA_SUPER_BUF_NOTIFY_CONTINUOUS,
    MM_CAMERA_SUPER_BUF_NOTIFY_MAX
} mm_camera_super_buf_notify_mode_t;

/** mm_camera_super_buf_priority_t: enum for super buffer
*                                   matching priority
*    @MM_CAMERA_SUPER_BUF_PRIORITY_NORMAL :
*       Save the frame no matter focused or not. Currently only
*       this type is supported.
*    @MM_CAMERA_SUPER_BUF_PRIORITY_FOCUS :
*       only queue the frame that is focused. Will enable meta
*       data header to carry focus info
*    @MM_CAMERA_SUPER_BUF_PRIORITY_EXPOSURE_BRACKETING :
*       after shutter, only queue matched exposure index
**/
typedef enum {
    MM_CAMERA_SUPER_BUF_PRIORITY_NORMAL = 0,
    MM_CAMERA_SUPER_BUF_PRIORITY_FOCUS,
    MM_CAMERA_SUPER_BUF_PRIORITY_EXPOSURE_BRACKETING,
    MM_CAMERA_SUPER_BUF_PRIORITY_MAX
} mm_camera_super_buf_priority_t;

/** mm_camera_advanced_capture_t: enum for advanced capture type.
*    @MM_CAMERA_AF_BRACKETING :
*       to enable AF Bracketig.
*    @MM_CAMERA_AE_BRACKETING :
*       to enable AF Bracketing.
*    @MM_CAMERA_FLASH_BRACKETING :
*       to enable Flash Bracketing.
*    @MM_CAMERA_ZOOM_1X :
*       to enable zoom 1x capture request
**/
typedef enum {
   MM_CAMERA_AF_BRACKETING = 0,
   MM_CAMERA_AE_BRACKETING,
   MM_CAMERA_FLASH_BRACKETING,
   MM_CAMERA_MTF_BRACKETING,
   MM_CAMERA_ZOOM_1X,
} mm_camera_advanced_capture_t;

/** mm_camera_channel_attr_t: structure for defining channel
*                             attributes
*    @notify_mode : notify mode: burst or continuous
*    @water_mark : queue depth. Only valid for burst mode
*    @look_back : look back how many frames from last buf.
*                 Only valid for burst mode
*    @post_frame_skip : after send first frame to HAL, how many
*                     frames needing to be skipped for next
*                     delivery. Only valid for burst mode
*    @max_unmatched_frames : max number of unmatched frames in
*                     queue
*    @priority : save matched priority frames only
**/
typedef struct {
    mm_camera_super_buf_notify_mode_t notify_mode;
    uint8_t water_mark;
    uint8_t look_back;
    uint8_t post_frame_skip;
    uint8_t max_unmatched_frames;
    mm_camera_super_buf_priority_t priority;
} mm_camera_channel_attr_t;

typedef struct {
    /** query_capability: fucntion definition for querying static
     *                    camera capabilities
     *    @camera_handle : camer handler
     *  Return value: 0 -- success
     *                -1 -- failure
     *  Note: would assume cam_capability_t is already mapped
     **/
    int32_t (*query_capability) (uint32_t camera_handle);

    /** register_event_notify: fucntion definition for registering
     *                         for event notification
     *    @camera_handle : camer handler
     *    @evt_cb : callback for event notify
     *    @user_data : user data poiner
     *  Return value: 0 -- success
     *                -1 -- failure
     **/
    int32_t (*register_event_notify) (uint32_t camera_handle,
                                      mm_camera_event_notify_t evt_cb,
                                      void *user_data);

    /** close_camera: fucntion definition for closing a camera
     *    @camera_handle : camer handler
     *  Return value: 0 -- success
     *                -1 -- failure
     **/
    int32_t (*close_camera) (uint32_t camera_handle);

    /** map_buf: fucntion definition for mapping a camera buffer
     *           via domain socket
     *    @camera_handle : camer handler
     *    @buf_type : type of mapping buffers, can be value of
     *                CAM_MAPPING_BUF_TYPE_CAPABILITY
     *                CAM_MAPPING_BUF_TYPE_SETPARM_BUF
     *                CAM_MAPPING_BUF_TYPE_GETPARM_BUF
     *    @fd : file descriptor of the stream buffer
     *    @size :  size of the stream buffer
     *  Return value: 0 -- success
     *                -1 -- failure
     **/
    int32_t (*map_buf) (uint32_t camera_handle,
                        uint8_t buf_type,
                        int fd,
                        size_t size);

    /** unmap_buf: fucntion definition for unmapping a camera buffer
     *           via domain socket
     *    @camera_handle : camer handler
     *    @buf_type : type of mapping buffers, can be value of
     *                CAM_MAPPING_BUF_TYPE_CAPABILITY
     *                CAM_MAPPING_BUF_TYPE_SETPARM_BUF
     *                CAM_MAPPING_BUF_TYPE_GETPARM_BUF
     *  Return value: 0 -- success
     *                -1 -- failure
     **/
    int32_t (*unmap_buf) (uint32_t camera_handle,
                          uint8_t buf_type);

    /** set_parms: fucntion definition for setting camera
     *             based parameters to server
     *    @camera_handle : camer handler
     *    @parms : batch for parameters to be set, stored in
     *               parm_buffer_t
     *  Return value: 0 -- success
     *                -1 -- failure
     *  Note: would assume parm_buffer_t is already mapped, and
     *       according parameter entries to be set are filled in the
     *       buf before this call
     **/
    int32_t (*set_parms) (uint32_t camera_handle,
                          void *parms);

    /** get_parms: fucntion definition for querying camera
     *             based parameters from server
     *    @camera_handle : camer handler
     *    @parms : batch for parameters to be queried, stored in
     *               parm_buffer_t
     *  Return value: 0 -- success
     *                -1 -- failure
     *  Note: would assume parm_buffer_t is already mapped, and
     *       according parameter entries to be queried are filled in
     *       the buf before this call
     **/
    int32_t (*get_parms) (uint32_t camera_handle,
                          void *parms);

    /** do_auto_focus: fucntion definition for performing auto focus
     *    @camera_handle : camer handler
     *  Return value: 0 -- success
     *                -1 -- failure
     *  Note: if this call success, we will always assume there will
     *        be an auto_focus event following up.
     **/
    int32_t (*do_auto_focus) (uint32_t camera_handle);

    /** cancel_auto_focus: fucntion definition for cancelling
     *                     previous auto focus request
     *    @camera_handle : camer handler
    *  Return value: 0 -- success
    *                -1 -- failure
     **/
    int32_t (*cancel_auto_focus) (uint32_t camera_handle);

    /** prepare_snapshot: fucntion definition for preparing hardware
     *                    for snapshot.
     *    @camera_handle : camer handler
     *    @do_af_flag    : flag indicating if AF needs to be done
     *                     0 -- no AF needed
     *                     1 -- AF needed
     *  Return value: 0 -- success
     *                -1 -- failure
     **/
    int32_t (*prepare_snapshot) (uint32_t camera_handle,
                                 int32_t do_af_flag);

    /** start_zsl_snapshot: function definition for starting
     *                    zsl snapshot.
     *    @camera_handle : camer handler
     *    @ch_id         : channel id
     *  Return value: 0 -- success
     *                -1 -- failure
     **/
    int32_t (*start_zsl_snapshot) (uint32_t camera_handle, uint32_t ch_id);

    /** stop_zsl_snapshot: function definition for stopping
     *                    zsl snapshot.
     *    @camera_handle : camer handler
     *    @ch_id         : channel id
     *  Return value: 0 -- success
     *                -1 -- failure
     **/
    int32_t (*stop_zsl_snapshot) (uint32_t camera_handle, uint32_t ch_id);

    /** add_channel: fucntion definition for adding a channel
     *    @camera_handle : camer handler
     *    @ch_id : channel handler
     *    @attr : pointer to channel attribute structure
     *    @channel_cb : callbak to handle bundled super buffer
     *    @userdata : user data pointer
     *  Return value: channel id, zero is invalid ch_id
     * Note: attr, channel_cb, and userdata can be NULL if no
     *       superbufCB is needed
     **/
    uint32_t (*add_channel) (uint32_t camera_handle,
                             mm_camera_channel_attr_t *attr,
                             mm_camera_buf_notify_t channel_cb,
                             void *userdata);

    /** delete_channel: fucntion definition for deleting a channel
     *    @camera_handle : camer handler
     *    @ch_id : channel handler
     *  Return value: 0 -- success
     *                -1 -- failure
     **/
    int32_t (*delete_channel) (uint32_t camera_handle,
                               uint32_t ch_id);

    /** get_bundle_info: function definition for querying bundle
     *  info of the channel
     *    @camera_handle : camera handler
     *    @ch_id         : channel handler
     *    @bundle_info   : bundle info to be filled in
     *  Return value: 0 -- success
     *                -1 -- failure
     **/
    int32_t (*get_bundle_info) (uint32_t camera_handle,
                                uint32_t ch_id,
                                cam_bundle_config_t *bundle_info);

    /** add_stream: fucntion definition for adding a stream
     *    @camera_handle : camer handler
     *    @ch_id : channel handler
     *  Return value: stream_id. zero is invalid stream_id
     **/
    uint32_t (*add_stream) (uint32_t camera_handle,
                            uint32_t ch_id);

    /** delete_stream: fucntion definition for deleting a stream
     *    @camera_handle : camer handler
     *    @ch_id : channel handler
     *    @stream_id : stream handler
     *  Return value: 0 -- success
     *                -1 -- failure
     **/
    int32_t (*delete_stream) (uint32_t camera_handle,
                              uint32_t ch_id,
                              uint32_t stream_id);

    /** link_stream: function definition for linking a stream
     *    @camera_handle : camera handle
     *    @ch_id : channel handle from which the stream originates
     *    @stream_id : stream handle
     *    @linked_ch_id: channel handle in which the stream will be linked
     *  Return value: 0 -- success
     *                -1 -- failure
     **/
    int32_t (*link_stream) (uint32_t camera_handle,
          uint32_t ch_id,
          uint32_t stream_id,
          uint32_t linked_ch_id);

    /** config_stream: fucntion definition for configuring a stream
     *    @camera_handle : camer handler
     *    @ch_id : channel handler
     *    @stream_id : stream handler
     *    @confid : pointer to a stream configuration structure
     *  Return value: 0 -- success
     *                -1 -- failure
     **/
    int32_t (*config_stream) (uint32_t camera_handle,
                              uint32_t ch_id,
                              uint32_t stream_id,
                              mm_camera_stream_config_t *config);

    /** map_stream_buf: fucntion definition for mapping
     *                 stream buffer via domain socket
     *    @camera_handle : camer handler
     *    @ch_id : channel handler
     *    @stream_id : stream handler
     *    @buf_type : type of mapping buffers, can be value of
     *             CAM_MAPPING_BUF_TYPE_STREAM_BUF
     *             CAM_MAPPING_BUF_TYPE_STREAM_INFO
     *             CAM_MAPPING_BUF_TYPE_OFFLINE_INPUT_BUF
     *    @buf_idx : buffer index within the stream buffers
     *    @plane_idx : plane index. If all planes share the same fd,
     *               plane_idx = -1; otherwise, plean_idx is the
     *               index to plane (0..num_of_planes)
     *    @fd : file descriptor of the stream buffer
     *    @size :  size of the stream buffer
     *  Return value: 0 -- success
     *                -1 -- failure
     **/
    int32_t (*map_stream_buf) (uint32_t camera_handle,
                               uint32_t ch_id,
                               uint32_t stream_id,
                               uint8_t buf_type,
                               uint32_t buf_idx,
                               int32_t plane_idx,
                               int fd,
                               size_t size);

    /** unmap_stream_buf: fucntion definition for unmapping
     *                 stream buffer via domain socket
     *    @camera_handle : camer handler
     *    @ch_id : channel handler
     *    @stream_id : stream handler
     *    @buf_type : type of mapping buffers, can be value of
     *             CAM_MAPPING_BUF_TYPE_STREAM_BUF
     *             CAM_MAPPING_BUF_TYPE_STREAM_INFO
     *             CAM_MAPPING_BUF_TYPE_OFFLINE_INPUT_BUF
     *    @buf_idx : buffer index within the stream buffers
     *    @plane_idx : plane index. If all planes share the same fd,
     *               plane_idx = -1; otherwise, plean_idx is the
     *               index to plane (0..num_of_planes)
     *  Return value: 0 -- success
     *                -1 -- failure
     **/
    int32_t (*unmap_stream_buf) (uint32_t camera_handle,
                                 uint32_t ch_id,
                                 uint32_t stream_id,
                                 uint8_t buf_type,
                                 uint32_t buf_idx,
                                 int32_t plane_idx);

    /** set_stream_parms: fucntion definition for setting stream
     *                    specific parameters to server
     *    @camera_handle : camer handler
     *    @ch_id : channel handler
     *    @stream_id : stream handler
     *    @parms : batch for parameters to be set
     *  Return value: 0 -- success
     *                -1 -- failure
     *  Note: would assume parm buffer is already mapped, and
     *       according parameter entries to be set are filled in the
     *       buf before this call
     **/
    int32_t (*set_stream_parms) (uint32_t camera_handle,
                                 uint32_t ch_id,
                                 uint32_t s_id,
                                 cam_stream_parm_buffer_t *parms);

    /** get_stream_parms: fucntion definition for querying stream
     *                    specific parameters from server
     *    @camera_handle : camer handler
     *    @ch_id : channel handler
     *    @stream_id : stream handler
     *    @parms : batch for parameters to be queried
     *  Return value: 0 -- success
     *                -1 -- failure
     *  Note: would assume parm buffer is already mapped, and
     *       according parameter entries to be queried are filled in
     *       the buf before this call
     **/
    int32_t (*get_stream_parms) (uint32_t camera_handle,
                                 uint32_t ch_id,
                                 uint32_t s_id,
                                 cam_stream_parm_buffer_t *parms);

    /** start_channel: fucntion definition for starting a channel
     *    @camera_handle : camer handler
     *    @ch_id : channel handler
     *  Return value: 0 -- success
     *                -1 -- failure
     * This call will start all streams belongs to the channel
     **/
    int32_t (*start_channel) (uint32_t camera_handle,
                              uint32_t ch_id);

    /** stop_channel: fucntion definition for stopping a channel
     *    @camera_handle : camer handler
     *    @ch_id : channel handler
     *  Return value: 0 -- success
     *                -1 -- failure
     * This call will stop all streams belongs to the channel
     **/
    int32_t (*stop_channel) (uint32_t camera_handle,
                             uint32_t ch_id);

    /** qbuf: fucntion definition for queuing a frame buffer back to
     *        kernel for reuse
     *    @camera_handle : camer handler
     *    @ch_id : channel handler
     *    @buf : a frame buffer to be queued back to kernel
     *  Return value: 0 -- success
     *                -1 -- failure
     **/
    int32_t (*qbuf) (uint32_t camera_handle,
                     uint32_t ch_id,
                     mm_camera_buf_def_t *buf);

    /** get_queued_buf_count: fucntion definition for querying queued buf count
     *    @camera_handle : camer handler
     *    @ch_id : channel handler
     *    @stream_id : stream handler
     *  Return value: queued buf count
     **/
    int32_t (*get_queued_buf_count) (uint32_t camera_handle,
            uint32_t ch_id,
            uint32_t stream_id);

    /** request_super_buf: fucntion definition for requesting frames
     *                     from superbuf queue in burst mode
     *    @camera_handle : camer handler
     *    @ch_id : channel handler
     *    @num_buf_requested : number of super buffers requested
     *  Return value: 0 -- success
     *                -1 -- failure
     **/
    int32_t (*request_super_buf) (uint32_t camera_handle,
                                  uint32_t ch_id,
                                  uint32_t num_buf_requested);

    /** cancel_super_buf_request: fucntion definition for canceling
     *                     frames dispatched from superbuf queue in
     *                     burst mode
     *    @camera_handle : camer handler
     *    @ch_id : channel handler
     *  Return value: 0 -- success
     *                -1 -- failure
     **/
    int32_t (*cancel_super_buf_request) (uint32_t camera_handle,
                                         uint32_t ch_id);

    /** flush_super_buf_queue: function definition for flushing out
     *                     all frames in the superbuf queue up to frame_idx,
     *                     even if frames with frame_idx come in later than
     *                     this call.
     *    @camera_handle : camer handler
     *    @ch_id : channel handler
     *    @frame_idx : frame index up until which all superbufs are flushed
     *  Return value: 0 -- success
     *                -1 -- failure
     **/
    int32_t (*flush_super_buf_queue) (uint32_t camera_handle,
                                      uint32_t ch_id, uint32_t frame_idx);

    /** configure_notify_mode: function definition for configuring the
     *                         notification mode of channel
     *    @camera_handle : camera handler
     *    @ch_id : channel handler
     *    @notify_mode : notification mode
     *  Return value: 0 -- success
     *                -1 -- failure
     **/
    int32_t (*configure_notify_mode) (uint32_t camera_handle,
                                      uint32_t ch_id,
                                      mm_camera_super_buf_notify_mode_t notify_mode);

     /** process_advanced_capture: function definition for start/stop advanced capture
     *                    for snapshot.
     *    @camera_handle : camera handle
     *    @type :  advanced capture type.
     *    @ch_id : channel handler
     *    @start_flag    : flag indicating if advanced capture needs to be done
     *                     0 -- stop advanced capture
     *                     1 -- start advanced capture
     *  Return value: 0 -- success
     *                -1 -- failure
     **/
     int32_t (*process_advanced_capture) (uint32_t camera_handle,
                                          mm_camera_advanced_capture_t type,
                                          uint32_t ch_id,
                                          int8_t start_flag);
} mm_camera_ops_t;

/** mm_camera_vtbl_t: virtual table for camera operations
*    @camera_handle : camera handler which uniquely identifies a
*                   camera object
*    @ops : API call table
**/
typedef struct {
    uint32_t camera_handle;
    mm_camera_ops_t *ops;
} mm_camera_vtbl_t;

/* return number of cameras */
uint8_t get_num_of_cameras();

/* return reference pointer of camera vtbl */
mm_camera_vtbl_t * camera_open(uint8_t camera_idx);
struct camera_info *get_cam_info(uint32_t camera_id);

/* helper functions */
int32_t mm_stream_calc_offset_preview(cam_format_t fmt,
        cam_dimension_t *dim,
        cam_stream_buf_plane_info_t *buf_planes);

int32_t mm_stream_calc_offset_post_view(cam_format_t fmt,
        cam_dimension_t *dim,
        cam_stream_buf_plane_info_t *buf_planes);

int32_t mm_stream_calc_offset_snapshot(cam_stream_info_t *stream_info,
        cam_dimension_t *dim,
        cam_padding_info_t *padding,
        cam_stream_buf_plane_info_t *buf_planes);

int32_t mm_stream_calc_offset_raw(cam_format_t fmt,
        cam_dimension_t *dim,
        cam_padding_info_t *padding,
        cam_stream_buf_plane_info_t *buf_planes);

int32_t mm_stream_calc_offset_video(cam_dimension_t *dim,
        cam_stream_buf_plane_info_t *buf_planes);

int32_t mm_stream_calc_offset_metadata(cam_dimension_t *dim,
        cam_padding_info_t *padding,
        cam_stream_buf_plane_info_t *buf_planes);

int32_t mm_stream_calc_offset_postproc(cam_stream_info_t *stream_info,
        cam_padding_info_t *padding,
        cam_stream_buf_plane_info_t *buf_planes);

#endif /*__MM_CAMERA_INTERFACE_H__*/
