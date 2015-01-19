/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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

#define ALOG_NIDEBUG 0
#define LOG_TAG "QualcommCamera"
#include <utils/Log.h>
#include <utils/threads.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <binder/IMemory.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <utils/RefBase.h>

#include "QualcommCamera.h"
#include "QCamera2Factory.h"
#include "QCamera2HWI.h"


extern "C" {
#include <sys/time.h>
}

/* HAL function implementation goes here*/

/**
 * The functions need to be provided by the camera HAL.
 *
 * If getNumberOfCameras() returns N, the valid cameraId for getCameraInfo()
 * and openCameraHardware() is 0 to N-1.
 */


static hw_module_methods_t camera_module_methods = {
    open: camera_device_open,
};

static hw_module_t camera_common = {
    tag: HARDWARE_MODULE_TAG,
    module_api_version: CAMERA_MODULE_API_VERSION_1_0,
    hal_api_version: HARDWARE_HAL_API_VERSION,
    id: CAMERA_HARDWARE_MODULE_ID,
    name: "QCamera Module",
    author: "Quic on behalf of CAF",
    methods: &camera_module_methods,
    dso: NULL,
    reserved:  {0},
};

using namespace qcamera;
namespace android {

typedef struct {
    camera_device hw_dev;
    QCamera2HardwareInterface *hardware;
    int camera_released;
    int cameraId;
} camera_hardware_t;

typedef struct {
  camera_memory_t mem;
  int32_t msgType;
  sp<IMemory> dataPtr;
  void* user;
  unsigned int index;
} q_cam_memory_t;

QCamera2HardwareInterface *util_get_Hal_obj( struct camera_device * device)
{
    QCamera2HardwareInterface *hardware = NULL;
    if(device && device->priv){
        camera_hardware_t *camHal = (camera_hardware_t *)device->priv;
        hardware = camHal->hardware;
    }
    return hardware;
}

extern "C" int get_number_of_cameras()
{
    /* try to query every time we get the call!*/

    CDBG_HIGH("Q%s: E", __func__);
    return QCamera2Factory::get_number_of_cameras();
}

extern "C" int get_camera_info(int camera_id, struct camera_info *info)
{
    int rc = -1;
    CDBG_HIGH("Q%s: E", __func__);

    if(info) {
        QCamera2Factory::get_camera_info(camera_id, info);
    }
    CDBG("Q%s: X", __func__);
    return rc;
}


/* HAL should return NULL if it fails to open camera hardware. */
extern "C" int  camera_device_open(
  const struct hw_module_t* module, const char* id,
          struct hw_device_t** hw_device)
{
    int rc = -1;
    camera_device *device = NULL;

    if(module && id && hw_device) {
        if (!strcmp(module->name, camera_common.name)) {
            int cameraId = atoi(id);

            camera_hardware_t *camHal =
                (camera_hardware_t *) malloc(sizeof (camera_hardware_t));
            if(!camHal) {
                *hw_device = NULL;
                ALOGE("%s:  end in no mem", __func__);
                return rc;
            }
            /* we have the camera_hardware obj malloced */
            memset(camHal, 0, sizeof (camera_hardware_t));
            camHal->hardware = new QCamera2HardwareInterface((uint32_t)cameraId);
            if (camHal->hardware) {
                camHal->cameraId = cameraId;
                device = &camHal->hw_dev;
                device->common.close = close_camera_device;
                device->ops = &QCamera2HardwareInterface::mCameraOps;
                device->priv = (void *)camHal;
                rc =  0;
            } else {
                if (camHal->hardware) {
                    delete camHal->hardware;
                    camHal->hardware = NULL;
                }
                free(camHal);
                device = NULL;
                goto EXIT;
            }
        }
        /* pass actual hw_device ptr to framework. This amkes that we actally be use memberof() macro */
        *hw_device = (hw_device_t*)&device->common;
    }

EXIT:

    ALOGE("%s:  end rc %d", __func__, rc);
    return rc;
}

extern "C"  int close_camera_device( hw_device_t *hw_dev)
{
    CDBG_HIGH("Q%s: device =%p E", __func__, hw_dev);
    int rc =  -1;
    camera_device_t *device = (camera_device_t *)hw_dev;

    if(device) {
        camera_hardware_t *camHal = (camera_hardware_t *)device->priv;
        if(camHal ) {
            QCamera2HardwareInterface *hardware = util_get_Hal_obj( device);
            if(!camHal->camera_released) {
                if(hardware != NULL) {
                    hardware->release(device);
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


int set_preview_window(struct camera_device * device,
        struct preview_stream_ops *window)
{
    int rc = -1;
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);

    if(hardware != NULL) {
        rc = hardware->set_preview_window(device, window);
    }
    return rc;
}

void set_CallBacks(struct camera_device * device,
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void *user)
{
    CDBG_HIGH("Q%s: E", __func__);
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        hardware->set_CallBacks(device, notify_cb,data_cb, data_cb_timestamp, get_memory, user);
    }
}

void enable_msg_type(struct camera_device * device, int32_t msg_type)
{
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        hardware->enable_msg_type(device, msg_type);
    }
}

void disable_msg_type(struct camera_device * device, int32_t msg_type)
{
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    CDBG_HIGH("Q%s: E", __func__);
    if(hardware != NULL){
        hardware->disable_msg_type(device, msg_type);
    }
}

int msg_type_enabled(struct camera_device * device, int32_t msg_type)
{
    CDBG_HIGH("Q%s: E", __func__);
    int rc = -1;
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->msg_type_enabled(device, msg_type);
    }
    return rc;
}

int start_preview(struct camera_device * device)
{
    CDBG_HIGH("Q%s: E", __func__);
    int rc = -1;
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->start_preview(device);
    }
    CDBG_HIGH("Q%s: X", __func__);
    return rc;
}

void stop_preview(struct camera_device * device)
{
    CDBG_HIGH("Q%s: E", __func__);
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        hardware->stop_preview(device);
    }
}

int preview_enabled(struct camera_device * device)
{
    CDBG_HIGH("Q%s: E", __func__);
    int rc = -1;
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->preview_enabled(device);
    }
    return rc;
}

int store_meta_data_in_buffers(struct camera_device * device, int enable)
{
    CDBG_HIGH("Q%s: E", __func__);
    int rc = -1;
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
      rc = hardware->store_meta_data_in_buffers(device, enable);
    }
    return rc;
}

int start_recording(struct camera_device * device)
{
    CDBG_HIGH("Q%s: E", __func__);
    int rc = -1;
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->start_recording(device);
    }
    return rc;
}

void stop_recording(struct camera_device * device)
{
    CDBG_HIGH("Q%s: E", __func__);
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        hardware->stop_recording(device);
    }
}

int recording_enabled(struct camera_device * device)
{
    CDBG_HIGH("Q%s: E", __func__);
    int rc = -1;
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->recording_enabled(device);
    }
    return rc;
}

void release_recording_frame(struct camera_device * device,
                const void *opaque)
{
    CDBG("Q%s: E", __func__);
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        hardware->release_recording_frame(device, opaque);
    }
}

int auto_focus(struct camera_device * device)
{
    CDBG_HIGH("Q%s: E", __func__);
    int rc = -1;
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->auto_focus(device);
    }
    return rc;
}

int cancel_auto_focus(struct camera_device * device)
{
    CDBG_HIGH("Q%s: E", __func__);
    int rc = -1;
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->cancel_auto_focus(device);
    }
    return rc;
}

int take_picture(struct camera_device * device)
{
    CDBG_HIGH("Q%s: E", __func__);
    int rc = -1;
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->take_picture(device);
    }
    return rc;
}

int cancel_picture(struct camera_device * device)

{
    CDBG_HIGH("Q%s: E", __func__);
    int rc = -1;
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->cancel_picture(device);
    }
    return rc;
}

int set_parameters(struct camera_device * device, const char *parms)

{
    CDBG_HIGH("Q%s: E", __func__);
    int rc = -1;
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL && parms){
        rc = hardware->set_parameters(device, parms);
  }
  return rc;
}

char* get_parameters(struct camera_device * device)
{
    CDBG_HIGH("Q%s: E", __func__);
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        char *parms = NULL;
        parms = hardware->get_parameters(device);
        return parms;
    }
    return NULL;
}

void put_parameters(struct camera_device * device, char *parm)

{
    CDBG_HIGH("Q%s: E", __func__);
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
      hardware->put_parameters(device, parm);
    }
}

int send_command(struct camera_device * device,
            int32_t cmd, int32_t arg1, int32_t arg2)
{
    CDBG_HIGH("Q%s: E", __func__);
    int rc = -1;
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->send_command(device, cmd, arg1, arg2);
    }
    return rc;
}

void release(struct camera_device * device)
{
    CDBG_HIGH("Q%s: E", __func__);
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        camera_hardware_t *camHal = (camera_hardware_t *)device->priv;
        hardware->release(device);
        camHal->camera_released = true;
    }
}

int dump(struct camera_device * device, int fd)
{
    CDBG_HIGH("Q%s: E", __func__);
    int rc = -1;
    QCamera2HardwareInterface *hardware = util_get_Hal_obj(device);
    if(hardware != NULL){
        rc = hardware->dump(device, fd);
    }
    return rc;
}

}; // namespace android
