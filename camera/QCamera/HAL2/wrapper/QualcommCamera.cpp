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
 *
 */
/*#error uncomment this for compiler test!*/

//#define ALOG_NDEBUG 0
#define ALOG_NIDEBUG 0
#define LOG_TAG "QualcommCamera"
#include <utils/Log.h>
#include <utils/threads.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "QCameraHAL.h"
/* include QCamera Hardware Interface Header*/
#include "QualcommCamera.h"

extern "C" {
#include <sys/time.h>
}

/**
 * The functions need to be provided by the camera HAL.
 *
 * If getNumberOfCameras() returns N, the valid cameraId for getCameraInfo()
 * and openCameraHardware() is 0 to N-1.
 */

static hw_module_methods_t camera_module_methods = {
    open: camera_device_open,
};

static hw_module_t camera_common  = {
    tag: HARDWARE_MODULE_TAG,
    module_api_version: CAMERA_MODULE_API_VERSION_2_0,
    hal_api_version: HARDWARE_HAL_API_VERSION,
    id: CAMERA_HARDWARE_MODULE_ID,
    name: "Qcamera",
    author:"Qcom",
    methods: &camera_module_methods,
    dso: NULL,
    reserved:  {0},
};

camera_module_t HAL_MODULE_INFO_SYM = {
    common: camera_common,
    get_number_of_cameras: get_number_of_cameras,
    get_camera_info: get_camera_info,
};

camera2_device_ops_t camera_ops = {
    set_request_queue_src_ops:           android::set_request_queue_src_ops,
    notify_request_queue_not_empty:      android::notify_request_queue_not_empty,
    set_frame_queue_dst_ops:             android::set_frame_queue_dst_ops,
    get_in_progress_count:               android::get_in_progress_count,
    flush_captures_in_progress:          android::flush_captures_in_progress,
    construct_default_request:           android::construct_default_request,

    allocate_stream:                     android::allocate_stream,
    register_stream_buffers:             android::register_stream_buffers,
    release_stream:                      android::release_stream,

    allocate_reprocess_stream:           android::allocate_reprocess_stream,
    allocate_reprocess_stream_from_stream: android::allocate_reprocess_stream_from_stream,
    release_reprocess_stream:            android::release_reprocess_stream,

    trigger_action:                      android::trigger_action,
    set_notify_callback:                 android::set_notify_callback,
    get_metadata_vendor_tag_ops:         android::get_metadata_vendor_tag_ops,
    dump:                                android::dump,
};

namespace android {

typedef struct {
  camera2_device_t hw_dev;
  QCameraHardwareInterface *hardware;
  int camera_released;
  int cameraId;
} camera_hardware_t;

QCameraHardwareInterface *util_get_Hal_obj(const camera2_device_t * device)
{
    QCameraHardwareInterface *hardware = NULL;
    if(device && device->priv){
        camera_hardware_t *camHal = (camera_hardware_t *)device->priv;
        hardware = camHal->hardware;
    }
    return hardware;
}

extern "C" int get_number_of_cameras()
{
    /* try to query every time we get the call!*/
    ALOGE("Q%s: E", __func__);
    return android::HAL_getNumberOfCameras();
}

extern "C" int get_camera_info(int camera_id, struct camera_info *info)
{
    int rc = -1;
    ALOGE("Q%s: E, id = %d", __func__, camera_id);
    if(info) {
        rc = android::HAL_getCameraInfo(camera_id, info);
    }
    ALOGE("Q%s: X, id = %d", __func__, camera_id);
    return rc;
}


/* HAL should return NULL if it fails to open camera hardware. */
extern "C" int  camera_device_open(
  const struct hw_module_t* module, const char* id,
          struct hw_device_t** hw_device)
{
    int rc = -1;
    int mode = 0;
    camera2_device_t *device = NULL;
    ALOGE("Q%s: E, id = %s", __func__, id);
    if(module && id && hw_device) {
        int cameraId = atoi(id);

        if (!strcmp(module->name, camera_common.name)) {
            camera_hardware_t *camHal =
                (camera_hardware_t *) malloc(sizeof (camera_hardware_t));
            if(!camHal) {
                *hw_device = NULL;
	        ALOGE("%s:  end in no mem", __func__);
		return rc;
	    }
            /* we have the camera_hardware obj malloced */
            memset(camHal, 0, sizeof (camera_hardware_t));
            camHal->hardware = new QCameraHardwareInterface(cameraId, mode);
            if (camHal->hardware && camHal->hardware->isCameraReady()) {
		camHal->cameraId = cameraId;
	        device = &camHal->hw_dev;
                device->common.close = close_camera_device;
                device->common.version = CAMERA_DEVICE_API_VERSION_2_0;
                device->ops = &camera_ops;
                device->priv = (void *)camHal;
                rc =  0;
            } else {
                if (camHal->hardware) {
                    delete camHal->hardware;
                    camHal->hardware = NULL;
                }
                free(camHal);
                device = NULL;
            }
        }
    }
    /* pass actual hw_device ptr to framework. This amkes that we actally be use memberof() macro */
    *hw_device = (hw_device_t*)&device->common;
    ALOGE("%s:  end rc %d", __func__, rc);
    return rc;
}

extern "C" int close_camera_device(hw_device_t *hw_dev)
{
    ALOGE("Q%s: device =%p E", __func__, hw_dev);
    int rc =  -1;
    camera2_device_t *device = (camera2_device_t *)hw_dev;

    if(device) {
        camera_hardware_t *camHal = (camera_hardware_t *)device->priv;
        if(camHal ) {
            QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
            if(!camHal->camera_released) {
                if(hardware != NULL) {
                    hardware->release( );
                }
            }
            if(hardware != NULL)
                delete hardware;
            free(camHal);
        }
        rc = 0;
    }
    return rc;
}

int set_request_queue_src_ops(const struct camera2_device *device,
    const camera2_request_queue_src_ops_t *request_src_ops)
{
    int rc = INVALID_OPERATION;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);

    if(hardware != NULL) {
        rc = hardware->set_request_queue_src_ops(request_src_ops);
    }
    return rc;
}

int notify_request_queue_not_empty(const struct camera2_device *device)
{
    int rc = INVALID_OPERATION;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    
    if(hardware != NULL) {
        rc = hardware->notify_request_queue_not_empty();
    }
    return rc;
}

int set_frame_queue_dst_ops(const struct camera2_device *device,
    const camera2_frame_queue_dst_ops_t *frame_dst_ops)
{
    int rc = INVALID_OPERATION;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    
    if(hardware != NULL) {
        rc = hardware->set_frame_queue_dst_ops(frame_dst_ops);
    }
    return rc;
}

int get_in_progress_count(const struct camera2_device *device)
{
    int rc = INVALID_OPERATION;
    ALOGE("%s:E",__func__);
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    
    if(hardware != NULL) {
        rc = hardware->get_in_progress_count();
    }
    ALOGE("%s:X",__func__);
    return rc;
}

int flush_captures_in_progress(const struct camera2_device *)
{
    ALOGE("%s:E",__func__);
    ALOGE("%s:X",__func__);
    return INVALID_OPERATION;
}

int construct_default_request(const struct camera2_device *device,
    int request_template,
    camera_metadata_t **request)
{
    int rc = INVALID_OPERATION;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    
    if(hardware != NULL) {
        rc = hardware->construct_default_request(request_template, request);
    }
    return rc;
}

int allocate_stream(const struct camera2_device *device,
        uint32_t width,
        uint32_t height,
        int      format,
        const camera2_stream_ops_t *stream_ops,
        uint32_t *stream_id,
        uint32_t *format_actual,
        uint32_t *usage,
        uint32_t *max_buffers)
{
    int rc = INVALID_OPERATION;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    
    if(hardware != NULL) {
        rc = hardware->allocate_stream(width, height, format, stream_ops,
            stream_id, format_actual, usage, max_buffers);
    }
    return rc;
}

int register_stream_buffers(
        const struct camera2_device *device,
        uint32_t stream_id,
        int num_buffers,
        buffer_handle_t *buffers)
{
    int rc = INVALID_OPERATION;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    
    if(hardware != NULL) {
        rc = hardware->register_stream_buffers(stream_id, num_buffers, buffers);
    }
    return rc;
}

int release_stream(
        const struct camera2_device *device,
        uint32_t stream_id)
{
    int rc = INVALID_OPERATION;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    
    if(hardware != NULL) {
        rc = hardware->release_stream(stream_id);
    }
    return rc;
}

int allocate_reprocess_stream(const struct camera2_device *,
        uint32_t width,
        uint32_t height,
        uint32_t format,
        const camera2_stream_in_ops_t *reprocess_stream_ops,
        uint32_t *stream_id,
        uint32_t *consumer_usage,
        uint32_t *max_buffers)
{
    return INVALID_OPERATION;
}

int allocate_reprocess_stream_from_stream(const struct camera2_device *,
        uint32_t output_stream_id,
        const camera2_stream_in_ops_t *reprocess_stream_ops,
        uint32_t *stream_id)
{
    return INVALID_OPERATION;
}

int release_reprocess_stream(
        const struct camera2_device *,
        uint32_t stream_id)
{
    return INVALID_OPERATION;
}

int trigger_action(const struct camera2_device *,
        uint32_t trigger_id,
        int32_t ext1,
        int32_t ext2)
{
    return INVALID_OPERATION;
}

int set_notify_callback(const struct camera2_device *device,
        camera2_notify_callback notify_cb,
        void *user)
{
    int rc = INVALID_OPERATION;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);

    if(hardware != NULL) {
        rc = hardware->set_notify_callback(notify_cb, user);
    }
    return rc;
}

int get_metadata_vendor_tag_ops(const struct camera2_device *device,
        vendor_tag_query_ops_t **ops)
{
    int rc = INVALID_OPERATION;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);

    if(hardware != NULL) {
        rc = hardware->get_metadata_vendor_tag_ops(ops);
    }
    return rc;
}

int dump(const struct camera2_device *, int fd)
{
    return INVALID_OPERATION;
}

#if 0
int set_preview_window(camera2_device_t * device,
        struct preview_stream_ops *window)
{
    int rc = -1;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);

    if(hardware != NULL) {
        rc = hardware->setPreviewWindow(window);
    }
    return rc;
}

void set_CallBacks(camera2_device_t * device,
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void *user)
{
    ALOGE("Q%s: E", __func__);
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        hardware->setCallbacks(notify_cb,data_cb, data_cb_timestamp, get_memory, user);
    }
}

void enable_msg_type(camera2_device_t * device, int32_t msg_type)
{
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        hardware->enableMsgType(msg_type);
    }
}

void disable_msg_type(camera2_device_t * device, int32_t msg_type)
{
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    ALOGE("Q%s: E", __func__);
    if(hardware != NULL){
        hardware->disableMsgType(msg_type);
    }
}

int msg_type_enabled(camera2_device_t * device, int32_t msg_type)
{
    ALOGE("Q%s: E", __func__);
    int rc = -1;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->msgTypeEnabled(msg_type);
    }
    return rc;
}

int start_preview(camera2_device_t * device)
{
    ALOGE("Q%s: E", __func__);
    int rc = -1;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->startPreview( );
    }
    ALOGE("Q%s: X", __func__);
    return rc;
}

void stop_preview(camera2_device_t * device)
{
    ALOGE("Q%s: E", __func__);
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        hardware->stopPreview( );
    }
}

int preview_enabled(camera2_device_t * device)
{
    ALOGE("Q%s: E", __func__);
    int rc = -1;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->previewEnabled( );
    }
    return rc;
}

int store_meta_data_in_buffers(camera2_device_t *device, int enable)
{
    ALOGE("Q%s: E", __func__);
    int rc = -1;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
      rc = hardware->storeMetaDataInBuffers(enable);
    }
    return rc;
}

int start_recording(camera2_device_t *device)
{
    ALOGE("Q%s: E", __func__);
    int rc = -1;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->startRecording( );
    }
    return rc;
}

void stop_recording(camera2_device_t *device)
{
    ALOGE("Q%s: E", __func__);
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        hardware->stopRecording( );
    }
}

int recording_enabled(camera2_device_t *device)
{
    ALOGE("Q%s: E", __func__);
    int rc = -1;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->recordingEnabled( );
    }
    return rc;
}

void release_recording_frame(camera2_device_t *device,
                const void *opaque)
{
    ALOGV("Q%s: E", __func__);
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        hardware->releaseRecordingFrame(opaque);
    }
}

int auto_focus(camera2_device_t *device)
{
    ALOGE("Q%s: E", __func__);
    int rc = -1;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->autoFocus( );
    }
    return rc;
}

int cancel_auto_focus(camera2_device_t *device)
{
    ALOGE("Q%s: E", __func__);
    int rc = -1;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->cancelAutoFocus( );
    }
    return rc;
}

int take_picture(camera2_device_t *device)
{
    ALOGE("Q%s: E", __func__);
    int rc = -1;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->takePicture( );
    }
    return rc;
}

int cancel_picture(camera2_device_t *device)

{
    ALOGE("Q%s: E", __func__);
    int rc = -1;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->cancelPicture( );
    }
    return rc;
}

int set_parameters(camera2_device_t *device, const char *parms)

{
    ALOGE("Q%s: E", __func__);
    int rc = -1;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL && parms){
        //QCameraParameters param;// = util_get_HAL_parameter(device);
        //String8 str = String8(parms);

        //param.unflatten(str);
        rc = hardware->setParameters(parms);
        //rc = 0;
  }
  return rc;
}

char* get_parameters(camera2_device_t *device)
{
    ALOGE("Q%s: E", __func__);
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
		char *parms = NULL;
        hardware->getParameters(&parms);
		return parms;
    }
    return NULL;
}

void put_parameters(camera2_device_t *device, char *parm)

{
    ALOGE("Q%s: E", __func__);
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
      hardware->putParameters(parm);
    }
}

int send_command(camera2_device_t *device,
            int32_t cmd, int32_t arg1, int32_t arg2)
{
    ALOGE("Q%s: E", __func__);
    int rc = -1;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->sendCommand( cmd, arg1, arg2);
    }
    return rc;
}

void release(camera2_device_t *device)
{
    ALOGE("Q%s: E", __func__);
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        camera_hardware_t *camHal = (camera_hardware_t *)device->priv;
        hardware->release( );
        camHal->camera_released = true;
    }
}

int dump(camera2_device_t *device, int fd)
{
    ALOGE("Q%s: E", __func__);
    int rc = -1;
    QCameraHardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->dump( fd );
    }
    return rc;
}
#endif

}; // namespace android
