/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
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

#ifndef ANDROID_HARDWARE_QUALCOMM_CAMERA_H
#define ANDROID_HARDWARE_QUALCOMM_CAMERA_H

#include <hardware/camera2.h>

extern "C" {

  int get_number_of_cameras();
  int get_camera_info(int camera_id, struct camera_info *info);

  int camera_device_open(const struct hw_module_t* module, const char* id,
          struct hw_device_t** device);

  hw_device_t * open_camera_device(int cameraId);

  int close_camera_device( hw_device_t *);

namespace android {
  int set_request_queue_src_ops(const struct camera2_device *,
    const camera2_request_queue_src_ops_t *request_src_ops);

  int notify_request_queue_not_empty(const struct camera2_device *);

  int set_frame_queue_dst_ops(const struct camera2_device *,
    const camera2_frame_queue_dst_ops_t *frame_dst_ops);

  int get_in_progress_count(const struct camera2_device *);

  int flush_captures_in_progress(const struct camera2_device *);

  int construct_default_request(const struct camera2_device *,
    int request_template,
    camera_metadata_t **request);

  int allocate_stream(const struct camera2_device *,
        uint32_t width,
        uint32_t height,
        int      format,
        const camera2_stream_ops_t *stream_ops,
        uint32_t *stream_id,
        uint32_t *format_actual,
        uint32_t *usage,
        uint32_t *max_buffers);

  int register_stream_buffers(
        const struct camera2_device *,
        uint32_t stream_id,
        int num_buffers,
        buffer_handle_t *buffers);

  int release_stream(
        const struct camera2_device *,
        uint32_t stream_id);

  int allocate_reprocess_stream(const struct camera2_device *,
        uint32_t width,
        uint32_t height,
        uint32_t format,
        const camera2_stream_in_ops_t *reprocess_stream_ops,
        uint32_t *stream_id,
        uint32_t *consumer_usage,
        uint32_t *max_buffers);

  int allocate_reprocess_stream_from_stream(const struct camera2_device *,
        uint32_t output_stream_id,
        const camera2_stream_in_ops_t *stream_ops,
        uint32_t *stream_id);

  int release_reprocess_stream(
        const struct camera2_device *,
        uint32_t stream_id);

  int trigger_action(const struct camera2_device *,
        uint32_t trigger_id,
        int32_t ext1,
        int32_t ext2);

  int set_notify_callback(const struct camera2_device *,
        camera2_notify_callback notify_cb,
        void *user);

  int get_metadata_vendor_tag_ops(const struct camera2_device*,
        vendor_tag_query_ops_t **ops);

  int dump(const struct camera2_device *, int fd);
}; // namespace android

} //extern "C"

#endif
