/* Copyright (c) 2012, The Linux Foundataion. All rights reserved.
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

#include "QCamera2Factory.h"

static hw_module_t camera_common = {
    tag: HARDWARE_MODULE_TAG,
    module_api_version: CAMERA_MODULE_API_VERSION_1_0,
    hal_api_version: HARDWARE_HAL_API_VERSION,
    id: CAMERA_HARDWARE_MODULE_ID,
    name: "QCamera Module",
    author: "Qualcomm Innovation Center Inc",
    methods: &qcamera::QCamera2Factory::mModuleMethods,
    dso: NULL,
    reserved:  {0},
};

camera_module_t HAL_MODULE_INFO_SYM = {
    common: camera_common,
    get_number_of_cameras: qcamera::QCamera2Factory::get_number_of_cameras,
    get_camera_info: qcamera::QCamera2Factory::get_camera_info,
#ifndef USE_JB_MR1
    set_callbacks: NULL,
#endif
#ifdef USE_VENDOR_CAMERA_EXT
    get_vendor_tag_ops: NULL,
#ifndef USE_KK_CODE
    open_legacy: NULL,
#endif
    reserved:  {0}
#endif
};
