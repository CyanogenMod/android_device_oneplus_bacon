/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
 */

#ifndef ANDROID_HARDWARE_QUALCOMM_CAMERA_USBCAM_H
#define ANDROID_HARDWARE_QUALCOMM_CAMERA_USBCAM_H

extern "C" {
#include <sys/time.h>
}

#include "QCameraHWI.h"

extern "C" {

/*#include <hardware/camera.h>*/

    int usbcam_get_number_of_cameras();
    int usbcam_get_camera_info(int camera_id, struct camera_info *info);

    int usbcam_camera_device_open(const struct hw_module_t* module, const char* id,
          struct hw_device_t** device);

    hw_device_t * usbcam_open_camera_device(int cameraId);

    int usbcam_close_camera_device( hw_device_t *);

namespace android {

    /** Set the ANativeWindow to which preview frames are sent */
    int usbcam_set_preview_window(struct camera_device *,
          struct preview_stream_ops *window);

    /** Set the notification and data callbacks */
    void usbcam_set_CallBacks(struct camera_device *,
          camera_notify_callback notify_cb,
          camera_data_callback data_cb,
          camera_data_timestamp_callback data_cb_timestamp,
          camera_request_memory get_memory,
          void *user);

    /**
     * The following three functions all take a msg_type, which is a bitmask of
     * the messages defined in include/ui/Camera.h
     */

    /**
     * Enable a message, or set of messages.
     */
    void usbcam_enable_msg_type(struct camera_device *, int32_t msg_type);

    /**
     * Disable a message, or a set of messages.
     *
     * Once received a call to disableMsgType(CAMERA_MSG_VIDEO_FRAME), camera
     * HAL should not rely on its client to call releaseRecordingFrame() to
     * release video recording frames sent out by the cameral HAL before and
     * after the disableMsgType(CAMERA_MSG_VIDEO_FRAME) call. Camera HAL
     * clients must not modify/access any video recording frame after calling
     * disableMsgType(CAMERA_MSG_VIDEO_FRAME).
     */
    void usbcam_disable_msg_type(struct camera_device *, int32_t msg_type);

    /**
     * Query whether a message, or a set of messages, is enabled.  Note that
     * this is operates as an AND, if any of the messages queried are off, this
     * will return false.
     */
    int usbcam_msg_type_enabled(struct camera_device *, int32_t msg_type);

    /**
     * Start preview mode.
     */
    int usbcam_start_preview(struct camera_device *);

    /**
     * Stop a previously started preview.
     */
    void usbcam_stop_preview(struct camera_device *);

    /**
     * Returns true if preview is enabled.
     */
    int usbcam_preview_enabled(struct camera_device *);

    /**
     * Request the camera HAL to store meta data or real YUV data in the video
     * buffers sent out via CAMERA_MSG_VIDEO_FRAME for a recording session. If
     * it is not called, the default camera HAL behavior is to store real YUV
     * data in the video buffers.
     *
     * This method should be called before startRecording() in order to be
     * effective.
     *
     * If meta data is stored in the video buffers, it is up to the receiver of
     * the video buffers to interpret the contents and to find the actual frame
     * data with the help of the meta data in the buffer. How this is done is
     * outside of the scope of this method.
     *
     * Some camera HALs may not support storing meta data in the video buffers,
     * but all camera HALs should support storing real YUV data in the video
     * buffers. If the camera HAL does not support storing the meta data in the
     * video buffers when it is requested to do do, INVALID_OPERATION must be
     * returned. It is very useful for the camera HAL to pass meta data rather
     * than the actual frame data directly to the video encoder, since the
     * amount of the uncompressed frame data can be very large if video size is
     * large.
     *
     * @param enable if true to instruct the camera HAL to store
     *        meta data in the video buffers; false to instruct
     *        the camera HAL to store real YUV data in the video
     *        buffers.
     *
     * @return OK on success.
     */
    int usbcam_store_meta_data_in_buffers(struct camera_device *, int enable);

    /**
     * Start record mode. When a record image is available, a
     * CAMERA_MSG_VIDEO_FRAME message is sent with the corresponding
     * frame. Every record frame must be released by a camera HAL client via
     * releaseRecordingFrame() before the client calls
     * disableMsgType(CAMERA_MSG_VIDEO_FRAME). After the client calls
     * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
     * responsibility to manage the life-cycle of the video recording frames,
     * and the client must not modify/access any video recording frames.
     */
    int usbcam_start_recording(struct camera_device *);

    /**
     * Stop a previously started recording.
     */
    void usbcam_stop_recording(struct camera_device *);

    /**
     * Returns true if recording is enabled.
     */
    int usbcam_recording_enabled(struct camera_device *);

    /**
     * Release a record frame previously returned by CAMERA_MSG_VIDEO_FRAME.
     *
     * It is camera HAL client's responsibility to release video recording
     * frames sent out by the camera HAL before the camera HAL receives a call
     * to disableMsgType(CAMERA_MSG_VIDEO_FRAME). After it receives the call to
     * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
     * responsibility to manage the life-cycle of the video recording frames.
     */
    void usbcam_release_recording_frame(struct camera_device *,
                  const void *opaque);

    /**
     * Start auto focus, the notification callback routine is called with
     * CAMERA_MSG_FOCUS once when focusing is complete. autoFocus() will be
     * called again if another auto focus is needed.
     */
    int usbcam_auto_focus(struct camera_device *);

    /**
     * Cancels auto-focus function. If the auto-focus is still in progress,
     * this function will cancel it. Whether the auto-focus is in progress or
     * not, this function will return the focus position to the default.  If
     * the camera does not support auto-focus, this is a no-op.
     */
    int usbcam_cancel_auto_focus(struct camera_device *);

    /**
     * Take a picture.
     */
    int usbcam_take_picture(struct camera_device *);

    /**
     * Cancel a picture that was started with takePicture. Calling this method
     * when no picture is being taken is a no-op.
     */
    int usbcam_cancel_picture(struct camera_device *);

    /**
     * Set the camera parameters. This returns BAD_VALUE if any parameter is
     * invalid or not supported.
     */
    int usbcam_set_parameters(struct camera_device *, const char *parms);

    //status_t setParameters(const QCameraParameters& params);
    /** Retrieve the camera parameters.  The buffer returned by the camera HAL
        must be returned back to it with put_parameters, if put_parameters
        is not NULL.
     */
    char* usbcam_get_parameters(struct camera_device *);

    /** The camera HAL uses its own memory to pass us the parameters when we
        call get_parameters.  Use this function to return the memory back to
        the camera HAL, if put_parameters is not NULL.  If put_parameters
        is NULL, then you have to use free() to release the memory.
    */
    void usbcam_put_parameters(struct camera_device *, char *);

    /**
     * Send command to camera driver.
     */
    int usbcam_send_command(struct camera_device *,
              int32_t cmd, int32_t arg1, int32_t arg2);

    /**
     * Release the hardware resources owned by this object.  Note that this is
     * *not* done in the destructor.
     */
    void usbcam_release(struct camera_device *);

    /**
     * Dump state of the camera hardware
     */
    int usbcam_dump(struct camera_device *, int fd);

}; // namespace android

} //extern "C"

#endif
