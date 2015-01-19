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

#ifndef __QCAMERA2FACTORY_H__
#define __QCAMERA2FACTORY_H__

#include <hardware/camera.h>
#include <system/camera.h>
#include <media/msmb_camera.h>

#include "QCamera2HWI.h"

namespace qcamera {

class QCamera2Factory
{
public:
    QCamera2Factory();
    virtual ~QCamera2Factory();

    static int get_number_of_cameras();
    static int get_camera_info(int camera_id, struct camera_info *info);

private:
    int getNumberOfCameras();
    int getCameraInfo(int camera_id, struct camera_info *info);
    int cameraDeviceOpen(int camera_id, struct hw_device_t **hw_device);
    static int camera_device_open(const struct hw_module_t *module, const char *id,
                struct hw_device_t **hw_device);

public:
    static struct hw_module_methods_t mModuleMethods;

private:
    int mNumOfCameras;
};

}; /*namespace qcamera*/

extern camera_module_t HAL_MODULE_INFO_SYM;

#endif /* ANDROID_HARDWARE_QUALCOMM_CAMERA_H */
