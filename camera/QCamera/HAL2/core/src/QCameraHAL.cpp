/*
** Copyright (c) 2011 The Linux Foundation. All rights reserved.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/*#error uncomment this for compiler test!*/

//#define ALOG_NDEBUG 0
#define ALOG_NIDEBUG 0
#define LOG_TAG "QCameraHAL"
#include <utils/Log.h>
#include <utils/threads.h>
#include <fcntl.h>


/* include QCamera Hardware Interface Header*/
#include "QCameraHAL.h"

int HAL_numOfCameras = 0;
mm_camera_info_t * HAL_camerahandle[MSM_MAX_CAMERA_SENSORS];
camera_metadata_t *HAL_staticInfo[MSM_MAX_CAMERA_SENSORS];

namespace android {
/* HAL function implementation goes here*/

/**
 * The functions need to be provided by the camera HAL.
 *
 * If getNumberOfCameras() returns N, the valid cameraId for getCameraInfo()
 * and openCameraHardware() is 0 to N-1.
 */
extern "C" int HAL_getNumberOfCameras()
{
    /* try to query every time we get the call!*/
    uint8_t num_camera = 0;
    mm_camera_info_t * handle_base = 0;
    ALOGV("%s: E", __func__);

    handle_base= camera_query(&num_camera);

    if (!handle_base) {
        HAL_numOfCameras = 0;
    }
    else
    {
        camera_info_t* p_camera_info = NULL;
        HAL_numOfCameras=num_camera;

        ALOGI("Handle base =0x%p",handle_base);
        ALOGI("getCameraInfo: numOfCameras = %d", HAL_numOfCameras);
        for(int i = 0; i < HAL_numOfCameras; i++) {
            ALOGI("Handle [%d]=0x%p",i,handle_base+i);
            HAL_camerahandle[i]=handle_base + i;
            p_camera_info = &(HAL_camerahandle[i]->camera_info);
            if (p_camera_info) {
                ALOGI("Camera sensor %d info:", i);
                ALOGI("camera_id: %d", p_camera_info->camera_id);
                ALOGI("modes_supported: %x", p_camera_info->modes_supported);
                ALOGI("position: %d", p_camera_info->position);
                ALOGI("sensor_mount_angle: %d", p_camera_info->sensor_mount_angle);
            }
        }
    }

    ALOGV("%s: X", __func__);

    return HAL_numOfCameras;
}

extern "C" int HAL_getCameraInfo(int cameraId, struct camera_info* cameraInfo)
{
    mm_camera_info_t *mm_camer_obj = 0;
    ALOGV("%s: E", __func__);
    mm_camera_vtbl_t *p_camera_vtbl=NULL;
    status_t ret;

    if (!HAL_numOfCameras || HAL_numOfCameras < cameraId || !cameraInfo)
        return INVALID_OPERATION;
    else
        mm_camer_obj = HAL_camerahandle[cameraId];

    if (!mm_camer_obj)
        return INVALID_OPERATION;
    else {
        cameraInfo->facing =
            (FRONT_CAMERA == mm_camer_obj->camera_info.position)?
            CAMERA_FACING_FRONT : CAMERA_FACING_BACK;

        cameraInfo->orientation = mm_camer_obj->camera_info.sensor_mount_angle;
    }

    if (HAL_staticInfo[cameraId])
        goto end;

    /*
     * We need to open the camera in order to get the full list of static properties
     * required in the Google Camera API 2.0
    */
    p_camera_vtbl=camera_open(cameraId, NULL);

    if(p_camera_vtbl == NULL) {
        ALOGE("%s:camera_open failed",__func__);
        return INVALID_OPERATION;
    }
    p_camera_vtbl->ops->sync(p_camera_vtbl->camera_handle);

    ret = getStaticInfo(&HAL_staticInfo[cameraId], p_camera_vtbl, true);
    if (ret != OK) {
        ALOGE("%s: Unable to allocate static info: %s (%d)",
            __FUNCTION__, strerror(-ret), ret);
        return ret;
    }
    ret = getStaticInfo(&HAL_staticInfo[cameraId], p_camera_vtbl, false);
    if (ret != OK) {
        ALOGE("%s: Unable to fill in static info: %s (%d)",
                __FUNCTION__, strerror(-ret), ret);
        return ret;
    }
    p_camera_vtbl->ops->camera_close(p_camera_vtbl->camera_handle);

end:
    cameraInfo->device_version = HARDWARE_DEVICE_API_VERSION(2, 0);
    cameraInfo->static_camera_characteristics =
        HAL_staticInfo[cameraId];

    ALOGV("%s: X", __func__);
    return OK;
}

}; // namespace android
