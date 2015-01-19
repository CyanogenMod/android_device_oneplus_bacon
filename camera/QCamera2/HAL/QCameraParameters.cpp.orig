/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "QCameraParameters"

#include <cutils/properties.h>
#include <math.h>
#include <utils/Errors.h>
#include <string.h>
#include <stdlib.h>
#include <gralloc_priv.h>
#include "QCamera2HWI.h"
#include "QCameraParameters.h"

#define ASPECT_TOLERANCE 0.001
#define FLIP_V_H (FLIP_H | FLIP_V)

namespace qcamera {
// Parameter keys to communicate between camera application and driver.
const char QCameraParameters::KEY_QC_SUPPORTED_HFR_SIZES[] = "hfr-size-values";
const char QCameraParameters::KEY_QC_PREVIEW_FRAME_RATE_MODE[] = "preview-frame-rate-mode";
const char QCameraParameters::KEY_QC_SUPPORTED_PREVIEW_FRAME_RATE_MODES[] = "preview-frame-rate-modes";
const char QCameraParameters::KEY_QC_PREVIEW_FRAME_RATE_AUTO_MODE[] = "frame-rate-auto";
const char QCameraParameters::KEY_QC_PREVIEW_FRAME_RATE_FIXED_MODE[] = "frame-rate-fixed";
const char QCameraParameters::KEY_QC_TOUCH_AF_AEC[] = "touch-af-aec";
const char QCameraParameters::KEY_QC_SUPPORTED_TOUCH_AF_AEC[] = "touch-af-aec-values";
const char QCameraParameters::KEY_QC_TOUCH_INDEX_AEC[] = "touch-index-aec";
const char QCameraParameters::KEY_QC_TOUCH_INDEX_AF[] = "touch-index-af";
const char QCameraParameters::KEY_QC_SCENE_DETECT[] = "scene-detect";
const char QCameraParameters::KEY_QC_SUPPORTED_SCENE_DETECT[] = "scene-detect-values";
const char QCameraParameters::KEY_QC_ISO_MODE[] = "iso";
const char QCameraParameters::KEY_QC_SUPPORTED_ISO_MODES[] = "iso-values";
const char QCameraParameters::KEY_QC_LENSSHADE[] = "lensshade";
const char QCameraParameters::KEY_QC_SUPPORTED_LENSSHADE_MODES[] = "lensshade-values";
const char QCameraParameters::KEY_QC_AUTO_EXPOSURE[] = "auto-exposure";
const char QCameraParameters::KEY_QC_SUPPORTED_AUTO_EXPOSURE[] = "auto-exposure-values";
const char QCameraParameters::KEY_QC_DENOISE[] = "denoise";
const char QCameraParameters::KEY_QC_SUPPORTED_DENOISE[] = "denoise-values";
const char QCameraParameters::KEY_QC_FOCUS_ALGO[] = "selectable-zone-af";
const char QCameraParameters::KEY_QC_SUPPORTED_FOCUS_ALGOS[] = "selectable-zone-af-values";
const char QCameraParameters::KEY_QC_FACE_DETECTION[] = "face-detection";
const char QCameraParameters::KEY_QC_SUPPORTED_FACE_DETECTION[] = "face-detection-values";
const char QCameraParameters::KEY_QC_FACE_RECOGNITION[] = "face-recognition";
const char QCameraParameters::KEY_QC_SUPPORTED_FACE_RECOGNITION[] = "face-recognition-values";
const char QCameraParameters::KEY_QC_MEMORY_COLOR_ENHANCEMENT[] = "mce";
const char QCameraParameters::KEY_QC_SUPPORTED_MEM_COLOR_ENHANCE_MODES[] = "mce-values";
const char QCameraParameters::KEY_QC_DIS[] = "dis";
const char QCameraParameters::KEY_QC_SUPPORTED_DIS_MODES[] = "dis-values";
const char QCameraParameters::KEY_QC_VIDEO_HIGH_FRAME_RATE[] = "video-hfr";
const char QCameraParameters::KEY_QC_SUPPORTED_VIDEO_HIGH_FRAME_RATE_MODES[] = "video-hfr-values";
const char QCameraParameters::KEY_QC_REDEYE_REDUCTION[] = "redeye-reduction";
const char QCameraParameters::KEY_QC_SUPPORTED_REDEYE_REDUCTION[] = "redeye-reduction-values";
const char QCameraParameters::KEY_QC_HIGH_DYNAMIC_RANGE_IMAGING[] = "hdr";
const char QCameraParameters::KEY_QC_SUPPORTED_HDR_IMAGING_MODES[] = "hdr-values";
const char QCameraParameters::KEY_QC_ZSL[] = "zsl";
const char QCameraParameters::KEY_QC_SUPPORTED_ZSL_MODES[] = "zsl-values";
const char QCameraParameters::KEY_QC_ZSL_BURST_INTERVAL[] = "capture-burst-interval";
const char QCameraParameters::KEY_QC_ZSL_BURST_LOOKBACK[] = "capture-burst-retroactive";
const char QCameraParameters::KEY_QC_ZSL_QUEUE_DEPTH[] = "capture-burst-queue-depth";
const char QCameraParameters::KEY_QC_CAMERA_MODE[] = "camera-mode";
const char QCameraParameters::KEY_QC_AE_BRACKET_HDR[] = "ae-bracket-hdr";
const char QCameraParameters::KEY_QC_SUPPORTED_AE_BRACKET_MODES[] = "ae-bracket-hdr-values";
const char QCameraParameters::KEY_QC_SUPPORTED_RAW_FORMATS[] = "raw-format-values";
const char QCameraParameters::KEY_QC_RAW_FORMAT[] = "raw-format";
const char QCameraParameters::KEY_QC_ORIENTATION[] = "orientation";
const char QCameraParameters::KEY_QC_SELECTABLE_ZONE_AF[] = "selectable-zone-af";
const char QCameraParameters::KEY_QC_CAPTURE_BURST_EXPOSURE[] = "capture-burst-exposures";
const char QCameraParameters::KEY_QC_NUM_SNAPSHOT_PER_SHUTTER[] = "num-snaps-per-shutter";
const char QCameraParameters::KEY_QC_NO_DISPLAY_MODE[] = "no-display-mode";
const char QCameraParameters::KEY_QC_RAW_PICUTRE_SIZE[] = "raw-size";
const char QCameraParameters::KEY_QC_SUPPORTED_SKIN_TONE_ENHANCEMENT_MODES[] = "skinToneEnhancement-values";
const char QCameraParameters::KEY_QC_SUPPORTED_LIVESNAPSHOT_SIZES[] = "supported-live-snapshot-sizes";
const char QCameraParameters::KEY_QC_HDR_NEED_1X[] = "hdr-need-1x";
const char QCameraParameters::KEY_QC_PREVIEW_FLIP[] = "preview-flip";
const char QCameraParameters::KEY_QC_VIDEO_FLIP[] = "video-flip";
const char QCameraParameters::KEY_QC_SNAPSHOT_PICTURE_FLIP[] = "snapshot-picture-flip";
const char QCameraParameters::KEY_QC_SUPPORTED_FLIP_MODES[] = "flip-mode-values";
const char QCameraParameters::KEY_QC_VIDEO_HDR[] = "video-hdr";
const char QCameraParameters::KEY_QC_SUPPORTED_VIDEO_HDR_MODES[] = "video-hdr-values";
const char QCameraParameters::KEY_QC_SNAPSHOT_BURST_NUM[] = "snapshot-burst-num";

// Values for effect settings.
const char QCameraParameters::EFFECT_EMBOSS[] = "emboss";
const char QCameraParameters::EFFECT_SKETCH[] = "sketch";
const char QCameraParameters::EFFECT_NEON[] = "neon";

// Values for auto exposure settings.
const char QCameraParameters::TOUCH_AF_AEC_OFF[] = "touch-off";
const char QCameraParameters::TOUCH_AF_AEC_ON[] = "touch-on";

// Values for scene mode settings.
const char QCameraParameters::SCENE_MODE_ASD[] = "asd";   // corresponds to CAMERA_BESTSHOT_AUTO in HAL
const char QCameraParameters::SCENE_MODE_BACKLIGHT[] = "backlight";
const char QCameraParameters::SCENE_MODE_FLOWERS[] = "flowers";
const char QCameraParameters::SCENE_MODE_AR[] = "AR";
const char QCameraParameters::SCENE_MODE_HDR[] = "hdr";

// Formats for setPreviewFormat and setPictureFormat.
const char QCameraParameters::PIXEL_FORMAT_YUV420SP_ADRENO[] = "yuv420sp-adreno";
const char QCameraParameters::PIXEL_FORMAT_YV12[] = "yuv420p";
const char QCameraParameters::PIXEL_FORMAT_NV12[] = "nv12";
const char QCameraParameters::QC_PIXEL_FORMAT_NV12_VENUS[] = "nv12-venus";

// Values for raw image formats
const char QCameraParameters::QC_PIXEL_FORMAT_YUV_RAW_8BIT_YUYV[] = "yuv-raw8-yuyv";
const char QCameraParameters::QC_PIXEL_FORMAT_YUV_RAW_8BIT_YVYU[] = "yuv-raw8-yvyu";
const char QCameraParameters::QC_PIXEL_FORMAT_YUV_RAW_8BIT_UYVY[] = "yuv-raw8-uyvy";
const char QCameraParameters::QC_PIXEL_FORMAT_YUV_RAW_8BIT_VYUY[] = "yuv-raw8-vyuy";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_QCOM_RAW_8GBRG[] = "bayer-qcom-8gbrg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_QCOM_RAW_8GRBG[] = "bayer-qcom-8grbg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_QCOM_RAW_8RGGB[] = "bayer-qcom-8rggb";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_QCOM_RAW_8BGGR[] = "bayer-qcom-8bggr";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_QCOM_RAW_10GBRG[] = "bayer-qcom-10gbrg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_QCOM_RAW_10GRBG[] = "bayer-qcom-10grbg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_QCOM_RAW_10RGGB[] = "bayer-qcom-10rggb";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_QCOM_RAW_10BGGR[] = "bayer-qcom-10bggr";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_QCOM_RAW_12GBRG[] = "bayer-qcom-12gbrg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_QCOM_RAW_12GRBG[] = "bayer-qcom-12grbg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_QCOM_RAW_12RGGB[] = "bayer-qcom-12rggb";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_QCOM_RAW_12BGGR[] = "bayer-qcom-12bggr";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_MIPI_RAW_8GBRG[] = "bayer-mipi-8gbrg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_MIPI_RAW_8GRBG[] = "bayer-mipi-8grbg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_MIPI_RAW_8RGGB[] = "bayer-mipi-8rggb";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_MIPI_RAW_8BGGR[] = "bayer-mipi-8bggr";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_MIPI_RAW_10GBRG[] = "bayer-mipi-10gbrg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_MIPI_RAW_10GRBG[] = "bayer-mipi-10grbg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_MIPI_RAW_10RGGB[] = "bayer-mipi-10rggb";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_MIPI_RAW_10BGGR[] = "bayer-mipi-10bggr";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_MIPI_RAW_12GBRG[] = "bayer-mipi-12gbrg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_MIPI_RAW_12GRBG[] = "bayer-mipi-12grbg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_MIPI_RAW_12RGGB[] = "bayer-mipi-12rggb";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_MIPI_RAW_12BGGR[] = "bayer-mipi-12bggr";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_8GBRG[] = "bayer-ideal-qcom-8gbrg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_8GRBG[] = "bayer-ideal-qcom-8grbg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_8RGGB[] = "bayer-ideal-qcom-8rggb";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_8BGGR[] = "bayer-ideal-qcom-8bggr";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_10GBRG[] = "bayer-ideal-qcom-10gbrg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_10GRBG[] = "bayer-ideal-qcom-10grbg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_10RGGB[] = "bayer-ideal-qcom-10rggb";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_10BGGR[] = "bayer-ideal-qcom-10bggr";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_12GBRG[] = "bayer-ideal-qcom-12gbrg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_12GRBG[] = "bayer-ideal-qcom-12grbg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_12RGGB[] = "bayer-ideal-qcom-12rggb";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_12BGGR[] = "bayer-ideal-qcom-12bggr";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_8GBRG[] = "bayer-ideal-mipi-8gbrg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_8GRBG[] = "bayer-ideal-mipi-8grbg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_8RGGB[] = "bayer-ideal-mipi-8rggb";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_8BGGR[] = "bayer-ideal-mipi-8bggr";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_10GBRG[] = "bayer-ideal-mipi-10gbrg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_10GRBG[] = "bayer-ideal-mipi-10grbg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_10RGGB[] = "bayer-ideal-mipi-10rggb";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_10BGGR[] = "bayer-ideal-mipi-10bggr";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_12GBRG[] = "bayer-ideal-mipi-12gbrg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_12GRBG[] = "bayer-ideal-mipi-12grbg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_12RGGB[] = "bayer-ideal-mipi-12rggb";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_12BGGR[] = "bayer-ideal-mipi-12bggr";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN8_8GBRG[] = "bayer-ideal-plain8-8gbrg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN8_8GRBG[] = "bayer-ideal-plain8-8grbg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN8_8RGGB[] = "bayer-ideal-plain8-8rggb";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN8_8BGGR[] = "bayer-ideal-plain8-8bggr";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_8GBRG[] = "bayer-ideal-plain16-8gbrg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_8GRBG[] = "bayer-ideal-plain16-8grbg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_8RGGB[] = "bayer-ideal-plain16-8rggb";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_8BGGR[] = "bayer-ideal-plain16-8bggr";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_10GBRG[] = "bayer-ideal-plain16-10gbrg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_10GRBG[] = "bayer-ideal-plain16-10grbg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_10RGGB[] = "bayer-ideal-plain16-10rggb";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_10BGGR[] = "bayer-ideal-plain16-10bggr";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_12GBRG[] = "bayer-ideal-plain16-12gbrg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_12GRBG[] = "bayer-ideal-plain16-12grbg";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_12RGGB[] = "bayer-ideal-plain16-12rggb";
const char QCameraParameters::QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_12BGGR[] = "bayer-ideal-plain16-12bggr";

// Values for ISO Settings
const char QCameraParameters::ISO_AUTO[] = "auto";
const char QCameraParameters::ISO_HJR[] = "ISO_HJR";
const char QCameraParameters::ISO_100[] = "ISO100";
const char QCameraParameters::ISO_200[] = "ISO200";
const char QCameraParameters::ISO_400[] = "ISO400";
const char QCameraParameters::ISO_800[] = "ISO800";
const char QCameraParameters::ISO_1600[] = "ISO1600";

// Values for auto exposure settings.
const char QCameraParameters::AUTO_EXPOSURE_FRAME_AVG[] = "frame-average";
const char QCameraParameters::AUTO_EXPOSURE_CENTER_WEIGHTED[] = "center-weighted";
const char QCameraParameters::AUTO_EXPOSURE_SPOT_METERING[] = "spot-metering";
const char QCameraParameters::AUTO_EXPOSURE_SMART_METERING[] = "smart-metering";
const char QCameraParameters::AUTO_EXPOSURE_USER_METERING[] = "user-metering";
const char QCameraParameters::AUTO_EXPOSURE_SPOT_METERING_ADV[] = "spot-metering-adv";
const char QCameraParameters::AUTO_EXPOSURE_CENTER_WEIGHTED_ADV[] = "center-weighted-adv";

const char QCameraParameters::KEY_QC_GPS_LATITUDE_REF[] = "gps-latitude-ref";
const char QCameraParameters::KEY_QC_GPS_LONGITUDE_REF[] = "gps-longitude-ref";
const char QCameraParameters::KEY_QC_GPS_ALTITUDE_REF[] = "gps-altitude-ref";
const char QCameraParameters::KEY_QC_GPS_STATUS[] = "gps-status";

const char QCameraParameters::KEY_QC_HISTOGRAM[] = "histogram";
const char QCameraParameters::KEY_QC_SUPPORTED_HISTOGRAM_MODES[] = "histogram-values";

const char QCameraParameters::VALUE_ENABLE[] = "enable";
const char QCameraParameters::VALUE_DISABLE[] = "disable";
const char QCameraParameters::VALUE_OFF[] = "off";
const char QCameraParameters::VALUE_ON[] = "on";
const char QCameraParameters::VALUE_TRUE[] = "true";
const char QCameraParameters::VALUE_FALSE[] = "false";

const char QCameraParameters::KEY_QC_SHARPNESS[] = "sharpness";
const char QCameraParameters::KEY_QC_MIN_SHARPNESS[] = "min-sharpness";
const char QCameraParameters::KEY_QC_MAX_SHARPNESS[] = "max-sharpness";
const char QCameraParameters::KEY_QC_SHARPNESS_STEP[] = "sharpness-step";
const char QCameraParameters::KEY_QC_CONTRAST[] = "contrast";
const char QCameraParameters::KEY_QC_MIN_CONTRAST[] = "min-contrast";
const char QCameraParameters::KEY_QC_MAX_CONTRAST[] = "max-contrast";
const char QCameraParameters::KEY_QC_CONTRAST_STEP[] = "contrast-step";
const char QCameraParameters::KEY_QC_SATURATION[] = "saturation";
const char QCameraParameters::KEY_QC_MIN_SATURATION[] = "min-saturation";
const char QCameraParameters::KEY_QC_MAX_SATURATION[] = "max-saturation";
const char QCameraParameters::KEY_QC_SATURATION_STEP[] = "saturation-step";
const char QCameraParameters::KEY_QC_BRIGHTNESS[] = "luma-adaptation";
const char QCameraParameters::KEY_QC_MIN_BRIGHTNESS[] = "min-brightness";
const char QCameraParameters::KEY_QC_MAX_BRIGHTNESS[] = "max-brightness";
const char QCameraParameters::KEY_QC_BRIGHTNESS_STEP[] = "brightness-step";
const char QCameraParameters::KEY_QC_SCE_FACTOR[] = "skinToneEnhancement";
const char QCameraParameters::KEY_QC_MIN_SCE_FACTOR[] = "min-sce-factor";
const char QCameraParameters::KEY_QC_MAX_SCE_FACTOR[] = "max-sce-factor";
const char QCameraParameters::KEY_QC_SCE_FACTOR_STEP[] = "sce-factor-step";

const char QCameraParameters::KEY_QC_SUPPORTED_CAMERA_FEATURES[] = "qc-camera-features";
const char QCameraParameters::KEY_QC_MAX_NUM_REQUESTED_FACES[] = "qc-max-num-requested-faces";

//Values for DENOISE
const char QCameraParameters::DENOISE_OFF[] = "denoise-off";
const char QCameraParameters::DENOISE_ON[] = "denoise-on";

// Values for selectable zone af Settings
const char QCameraParameters::FOCUS_ALGO_AUTO[] = "auto";
const char QCameraParameters::FOCUS_ALGO_SPOT_METERING[] = "spot-metering";
const char QCameraParameters::FOCUS_ALGO_CENTER_WEIGHTED[] = "center-weighted";
const char QCameraParameters::FOCUS_ALGO_FRAME_AVERAGE[] = "frame-average";

// Values for HFR settings.
const char QCameraParameters::VIDEO_HFR_OFF[] = "off";
const char QCameraParameters::VIDEO_HFR_2X[] = "60";
const char QCameraParameters::VIDEO_HFR_3X[] = "90";
const char QCameraParameters::VIDEO_HFR_4X[] = "120";
const char QCameraParameters::VIDEO_HFR_5X[] = "150";

// Values for HDR Bracketing settings.
const char QCameraParameters::AE_BRACKET_OFF[] = "Off";
const char QCameraParameters::AE_BRACKET[] = "AE-Bracket";

// Values for FLIP settings.
const char QCameraParameters::FLIP_MODE_OFF[] = "off";
const char QCameraParameters::FLIP_MODE_V[] = "flip-v";
const char QCameraParameters::FLIP_MODE_H[] = "flip-h";
const char QCameraParameters::FLIP_MODE_VH[] = "flip-vh";

static const char* portrait = "portrait";
static const char* landscape = "landscape";

const cam_dimension_t QCameraParameters::THUMBNAIL_SIZES_MAP[] = {
    { 512, 288 }, //1.777778
    { 480, 288 }, //1.666667
    { 256, 154 }, //1.66233
    { 432, 288 }, //1.5
    { 320, 240 }, //1.33333
    { 176, 144 }, //1.222222
    { 0, 0 }      // required by Android SDK
};

const QCameraParameters::QCameraMap QCameraParameters::AUTO_EXPOSURE_MAP[] = {
    { AUTO_EXPOSURE_FRAME_AVG,           CAM_AEC_MODE_FRAME_AVERAGE },
    { AUTO_EXPOSURE_CENTER_WEIGHTED,     CAM_AEC_MODE_CENTER_WEIGHTED },
    { AUTO_EXPOSURE_SPOT_METERING,       CAM_AEC_MODE_SPOT_METERING },
    { AUTO_EXPOSURE_SMART_METERING,      CAM_AEC_MODE_SMART_METERING },
    { AUTO_EXPOSURE_USER_METERING,       CAM_AEC_MODE_USER_METERING },
    { AUTO_EXPOSURE_SPOT_METERING_ADV,   CAM_AEC_MODE_SPOT_METERING_ADV },
    { AUTO_EXPOSURE_CENTER_WEIGHTED_ADV, CAM_AEC_MODE_CENTER_WEIGHTED_ADV },
};

const QCameraParameters::QCameraMap QCameraParameters::PREVIEW_FORMATS_MAP[] = {
    {PIXEL_FORMAT_YUV420SP,        CAM_FORMAT_YUV_420_NV21},
    {PIXEL_FORMAT_YUV420P,         CAM_FORMAT_YUV_420_YV12},
    {PIXEL_FORMAT_YUV420SP_ADRENO, CAM_FORMAT_YUV_420_NV21_ADRENO},
    {PIXEL_FORMAT_YV12,            CAM_FORMAT_YUV_420_YV12},
    {PIXEL_FORMAT_NV12,            CAM_FORMAT_YUV_420_NV12},
    {QC_PIXEL_FORMAT_NV12_VENUS,   CAM_FORMAT_YUV_420_NV12_VENUS}
};

const QCameraParameters::QCameraMap QCameraParameters::PICTURE_TYPES_MAP[] = {
    {PIXEL_FORMAT_JPEG,                          CAM_FORMAT_JPEG},
    {PIXEL_FORMAT_YUV422SP,                      CAM_FORMAT_YUV_422_NV16},
    {QC_PIXEL_FORMAT_YUV_RAW_8BIT_YUYV,          CAM_FORMAT_YUV_RAW_8BIT_YUYV},
    {QC_PIXEL_FORMAT_YUV_RAW_8BIT_YVYU,          CAM_FORMAT_YUV_RAW_8BIT_YVYU},
    {QC_PIXEL_FORMAT_YUV_RAW_8BIT_UYVY,          CAM_FORMAT_YUV_RAW_8BIT_UYVY},
    {QC_PIXEL_FORMAT_YUV_RAW_8BIT_VYUY,          CAM_FORMAT_YUV_RAW_8BIT_VYUY},
    {QC_PIXEL_FORMAT_BAYER_QCOM_RAW_8GBRG,       CAM_FORMAT_BAYER_QCOM_RAW_8BPP_GBRG},
    {QC_PIXEL_FORMAT_BAYER_QCOM_RAW_8GRBG,       CAM_FORMAT_BAYER_QCOM_RAW_8BPP_GRBG},
    {QC_PIXEL_FORMAT_BAYER_QCOM_RAW_8RGGB,       CAM_FORMAT_BAYER_QCOM_RAW_8BPP_RGGB},
    {QC_PIXEL_FORMAT_BAYER_QCOM_RAW_8BGGR,       CAM_FORMAT_BAYER_QCOM_RAW_8BPP_BGGR},
    {QC_PIXEL_FORMAT_BAYER_QCOM_RAW_10GBRG,      CAM_FORMAT_BAYER_QCOM_RAW_10BPP_GBRG},
    {QC_PIXEL_FORMAT_BAYER_QCOM_RAW_10GRBG,      CAM_FORMAT_BAYER_QCOM_RAW_10BPP_GRBG},
    {QC_PIXEL_FORMAT_BAYER_QCOM_RAW_10RGGB,      CAM_FORMAT_BAYER_QCOM_RAW_10BPP_RGGB},
    {QC_PIXEL_FORMAT_BAYER_QCOM_RAW_10BGGR,      CAM_FORMAT_BAYER_QCOM_RAW_10BPP_BGGR},
    {QC_PIXEL_FORMAT_BAYER_QCOM_RAW_12GBRG,      CAM_FORMAT_BAYER_QCOM_RAW_12BPP_GBRG},
    {QC_PIXEL_FORMAT_BAYER_QCOM_RAW_12GRBG,      CAM_FORMAT_BAYER_QCOM_RAW_12BPP_GRBG},
    {QC_PIXEL_FORMAT_BAYER_QCOM_RAW_12RGGB,      CAM_FORMAT_BAYER_QCOM_RAW_12BPP_RGGB},
    {QC_PIXEL_FORMAT_BAYER_QCOM_RAW_12BGGR,      CAM_FORMAT_BAYER_QCOM_RAW_12BPP_BGGR},
    {QC_PIXEL_FORMAT_BAYER_MIPI_RAW_8GBRG,       CAM_FORMAT_BAYER_MIPI_RAW_8BPP_GBRG},
    {QC_PIXEL_FORMAT_BAYER_MIPI_RAW_8GRBG,       CAM_FORMAT_BAYER_MIPI_RAW_8BPP_GRBG},
    {QC_PIXEL_FORMAT_BAYER_MIPI_RAW_8RGGB,       CAM_FORMAT_BAYER_MIPI_RAW_8BPP_RGGB},
    {QC_PIXEL_FORMAT_BAYER_MIPI_RAW_8BGGR,       CAM_FORMAT_BAYER_MIPI_RAW_8BPP_BGGR},
    {QC_PIXEL_FORMAT_BAYER_MIPI_RAW_10GBRG,      CAM_FORMAT_BAYER_MIPI_RAW_10BPP_GBRG},
    {QC_PIXEL_FORMAT_BAYER_MIPI_RAW_10GRBG,      CAM_FORMAT_BAYER_MIPI_RAW_10BPP_GRBG},
    {QC_PIXEL_FORMAT_BAYER_MIPI_RAW_10RGGB,      CAM_FORMAT_BAYER_MIPI_RAW_10BPP_RGGB},
    {QC_PIXEL_FORMAT_BAYER_MIPI_RAW_10BGGR,      CAM_FORMAT_BAYER_MIPI_RAW_10BPP_BGGR},
    {QC_PIXEL_FORMAT_BAYER_MIPI_RAW_12GBRG,      CAM_FORMAT_BAYER_MIPI_RAW_12BPP_GBRG},
    {QC_PIXEL_FORMAT_BAYER_MIPI_RAW_12GRBG,      CAM_FORMAT_BAYER_MIPI_RAW_12BPP_GRBG},
    {QC_PIXEL_FORMAT_BAYER_MIPI_RAW_12RGGB,      CAM_FORMAT_BAYER_MIPI_RAW_12BPP_RGGB},
    {QC_PIXEL_FORMAT_BAYER_MIPI_RAW_12BGGR,      CAM_FORMAT_BAYER_MIPI_RAW_12BPP_BGGR},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_8GBRG,     CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_8BPP_GBRG},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_8GRBG,     CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_8BPP_GRBG},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_8RGGB,     CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_8BPP_RGGB},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_8BGGR,     CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_8BPP_BGGR},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_10GBRG,    CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_10BPP_GBRG},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_10GRBG,    CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_10BPP_GRBG},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_10RGGB,    CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_10BPP_RGGB},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_10BGGR,    CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_10BPP_BGGR},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_12GBRG,    CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_12BPP_GBRG},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_12GRBG,    CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_12BPP_GRBG},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_12RGGB,    CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_12BPP_RGGB},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_QCOM_12BGGR,    CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_12BPP_BGGR},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_8GBRG,     CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_8BPP_GBRG},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_8GRBG,     CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_8BPP_GRBG},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_8RGGB,     CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_8BPP_RGGB},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_8BGGR,     CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_8BPP_BGGR},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_10GBRG,    CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_10BPP_GBRG},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_10GRBG,    CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_10BPP_GRBG},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_10RGGB,    CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_10BPP_RGGB},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_10BGGR,    CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_10BPP_BGGR},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_12GBRG,    CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_12BPP_GBRG},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_12GRBG,    CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_12BPP_GRBG},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_12RGGB,    CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_12BPP_RGGB},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_MIPI_12BGGR,    CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_12BPP_BGGR},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN8_8GBRG,   CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN8_8BPP_GBRG},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN8_8GRBG,   CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN8_8BPP_GRBG},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN8_8RGGB,   CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN8_8BPP_RGGB},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN8_8BGGR,   CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN8_8BPP_BGGR},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_8GBRG,  CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_8BPP_GBRG},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_8GRBG,  CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_8BPP_GRBG},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_8RGGB,  CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_8BPP_RGGB},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_8BGGR,  CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_8BPP_BGGR},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_10GBRG, CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_10BPP_GBRG},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_10GRBG, CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_10BPP_GRBG},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_10RGGB, CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_10BPP_RGGB},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_10BGGR, CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_10BPP_BGGR},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_12GBRG, CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_12BPP_GBRG},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_12GRBG, CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_12BPP_GRBG},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_12RGGB, CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_12BPP_RGGB},
    {QC_PIXEL_FORMAT_BAYER_IDEAL_PLAIN16_12BGGR, CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_12BPP_BGGR}
};

const QCameraParameters::QCameraMap QCameraParameters::RAW_FORMATS_MAP[] = {
};

const QCameraParameters::QCameraMap QCameraParameters::FOCUS_MODES_MAP[] = {
    { FOCUS_MODE_AUTO,               CAM_FOCUS_MODE_AUTO },
    { FOCUS_MODE_INFINITY,           CAM_FOCUS_MODE_INFINITY },
    { FOCUS_MODE_MACRO,              CAM_FOCUS_MODE_MACRO },
    { FOCUS_MODE_FIXED,              CAM_FOCUS_MODE_FIXED },
    { FOCUS_MODE_EDOF,               CAM_FOCUS_MODE_EDOF },
    { FOCUS_MODE_CONTINUOUS_PICTURE, CAM_FOCUS_MODE_CONTINOUS_PICTURE },
    { FOCUS_MODE_CONTINUOUS_VIDEO,   CAM_FOCUS_MODE_CONTINOUS_VIDEO }
};

const QCameraParameters::QCameraMap QCameraParameters::EFFECT_MODES_MAP[] = {
    { EFFECT_NONE,       CAM_EFFECT_MODE_OFF },
    { EFFECT_MONO,       CAM_EFFECT_MODE_MONO },
    { EFFECT_NEGATIVE,   CAM_EFFECT_MODE_NEGATIVE },
    { EFFECT_SOLARIZE,   CAM_EFFECT_MODE_SOLARIZE },
    { EFFECT_SEPIA,      CAM_EFFECT_MODE_SEPIA },
    { EFFECT_POSTERIZE,  CAM_EFFECT_MODE_POSTERIZE },
    { EFFECT_WHITEBOARD, CAM_EFFECT_MODE_WHITEBOARD },
    { EFFECT_BLACKBOARD, CAM_EFFECT_MODE_BLACKBOARD },
    { EFFECT_AQUA,       CAM_EFFECT_MODE_AQUA },
    { EFFECT_EMBOSS,     CAM_EFFECT_MODE_EMBOSS },
    { EFFECT_SKETCH,     CAM_EFFECT_MODE_SKETCH },
    { EFFECT_NEON,       CAM_EFFECT_MODE_NEON }
};

const QCameraParameters::QCameraMap QCameraParameters::SCENE_MODES_MAP[] = {
    { SCENE_MODE_AUTO,           CAM_SCENE_MODE_OFF },
    { SCENE_MODE_ACTION,         CAM_SCENE_MODE_ACTION },
    { SCENE_MODE_PORTRAIT,       CAM_SCENE_MODE_PORTRAIT },
    { SCENE_MODE_LANDSCAPE,      CAM_SCENE_MODE_LANDSCAPE },
    { SCENE_MODE_NIGHT,          CAM_SCENE_MODE_NIGHT },
    { SCENE_MODE_NIGHT_PORTRAIT, CAM_SCENE_MODE_NIGHT_PORTRAIT },
    { SCENE_MODE_THEATRE,        CAM_SCENE_MODE_THEATRE },
    { SCENE_MODE_BEACH,          CAM_SCENE_MODE_BEACH },
    { SCENE_MODE_SNOW,           CAM_SCENE_MODE_SNOW },
    { SCENE_MODE_SUNSET,         CAM_SCENE_MODE_SUNSET },
    { SCENE_MODE_STEADYPHOTO,    CAM_SCENE_MODE_ANTISHAKE },
    { SCENE_MODE_FIREWORKS ,     CAM_SCENE_MODE_FIREWORKS },
    { SCENE_MODE_SPORTS ,        CAM_SCENE_MODE_SPORTS },
    { SCENE_MODE_PARTY,          CAM_SCENE_MODE_PARTY },
    { SCENE_MODE_CANDLELIGHT,    CAM_SCENE_MODE_CANDLELIGHT },
    { SCENE_MODE_ASD,            CAM_SCENE_MODE_AUTO },
    { SCENE_MODE_BACKLIGHT,      CAM_SCENE_MODE_BACKLIGHT },
    { SCENE_MODE_FLOWERS,        CAM_SCENE_MODE_FLOWERS },
    { SCENE_MODE_AR,             CAM_SCENE_MODE_AR },
    { SCENE_MODE_HDR,            CAM_SCENE_MODE_OFF },
};

const QCameraParameters::QCameraMap QCameraParameters::FLASH_MODES_MAP[] = {
    { FLASH_MODE_OFF,   CAM_FLASH_MODE_OFF },
    { FLASH_MODE_AUTO,  CAM_FLASH_MODE_AUTO },
    { FLASH_MODE_ON,    CAM_FLASH_MODE_ON },
    { FLASH_MODE_TORCH, CAM_FLASH_MODE_TORCH }
};

const QCameraParameters::QCameraMap QCameraParameters::FOCUS_ALGO_MAP[] = {
    { FOCUS_ALGO_AUTO,            CAM_FOCUS_ALGO_AUTO },
    { FOCUS_ALGO_SPOT_METERING,   CAM_FOCUS_ALGO_SPOT },
    { FOCUS_ALGO_CENTER_WEIGHTED, CAM_FOCUS_ALGO_CENTER_WEIGHTED },
    { FOCUS_ALGO_FRAME_AVERAGE,   CAM_FOCUS_ALGO_AVERAGE }
};

const QCameraParameters::QCameraMap QCameraParameters::WHITE_BALANCE_MODES_MAP[] = {
    { WHITE_BALANCE_AUTO,            CAM_WB_MODE_AUTO },
    { WHITE_BALANCE_INCANDESCENT,    CAM_WB_MODE_INCANDESCENT },
    { WHITE_BALANCE_FLUORESCENT,     CAM_WB_MODE_FLUORESCENT },
    { WHITE_BALANCE_WARM_FLUORESCENT,CAM_WB_MODE_WARM_FLUORESCENT},
    { WHITE_BALANCE_DAYLIGHT,        CAM_WB_MODE_DAYLIGHT },
    { WHITE_BALANCE_CLOUDY_DAYLIGHT, CAM_WB_MODE_CLOUDY_DAYLIGHT },
    { WHITE_BALANCE_TWILIGHT,        CAM_WB_MODE_TWILIGHT },
    { WHITE_BALANCE_SHADE,           CAM_WB_MODE_SHADE }
};

const QCameraParameters::QCameraMap QCameraParameters::ANTIBANDING_MODES_MAP[] = {
    { ANTIBANDING_OFF,  CAM_ANTIBANDING_MODE_OFF },
    { ANTIBANDING_50HZ, CAM_ANTIBANDING_MODE_50HZ },
    { ANTIBANDING_60HZ, CAM_ANTIBANDING_MODE_60HZ },
    { ANTIBANDING_AUTO, CAM_ANTIBANDING_MODE_AUTO }
};

const QCameraParameters::QCameraMap QCameraParameters::ISO_MODES_MAP[] = {
    { ISO_AUTO,  CAM_ISO_MODE_AUTO },
    { ISO_HJR,   CAM_ISO_MODE_DEBLUR },
    { ISO_100,   CAM_ISO_MODE_100 },
    { ISO_200,   CAM_ISO_MODE_200 },
    { ISO_400,   CAM_ISO_MODE_400 },
    { ISO_800,   CAM_ISO_MODE_800 },
    { ISO_1600,  CAM_ISO_MODE_1600 }
};

const QCameraParameters::QCameraMap QCameraParameters::HFR_MODES_MAP[] = {
    { VIDEO_HFR_OFF, CAM_HFR_MODE_OFF },
    { VIDEO_HFR_2X,  CAM_HFR_MODE_60FPS },
    { VIDEO_HFR_3X,  CAM_HFR_MODE_90FPS },
    { VIDEO_HFR_4X,  CAM_HFR_MODE_120FPS },
    { VIDEO_HFR_5X,  CAM_HFR_MODE_150FPS }
};

const QCameraParameters::QCameraMap QCameraParameters::BRACKETING_MODES_MAP[] = {
    { AE_BRACKET_OFF, CAM_EXP_BRACKETING_OFF },
    { AE_BRACKET,         CAM_EXP_BRACKETING_ON }
};

const QCameraParameters::QCameraMap QCameraParameters::ON_OFF_MODES_MAP[] = {
    { VALUE_OFF, 0 },
    { VALUE_ON,  1 }
};

const QCameraParameters::QCameraMap QCameraParameters::TOUCH_AF_AEC_MODES_MAP[] = {
    { QCameraParameters::TOUCH_AF_AEC_OFF, 0 },
    { QCameraParameters::TOUCH_AF_AEC_ON, 1 }
};

const QCameraParameters::QCameraMap QCameraParameters::ENABLE_DISABLE_MODES_MAP[] = {
    { VALUE_ENABLE,  1 },
    { VALUE_DISABLE, 0 }
};

const QCameraParameters::QCameraMap QCameraParameters::DENOISE_ON_OFF_MODES_MAP[] = {
    { DENOISE_OFF, 0 },
    { DENOISE_ON,  1 }
};

const QCameraParameters::QCameraMap QCameraParameters::TRUE_FALSE_MODES_MAP[] = {
    { VALUE_FALSE, 0},
    { VALUE_TRUE,  1}
};

const QCameraParameters::QCameraMap QCameraParameters::FLIP_MODES_MAP[] = {
    {FLIP_MODE_OFF, 0},
    {FLIP_MODE_V, FLIP_V},
    {FLIP_MODE_H, FLIP_H},
    {FLIP_MODE_VH, FLIP_V_H}
};

#define DEFAULT_CAMERA_AREA "(0, 0, 0, 0, 0)"
#define DATA_PTR(MEM_OBJ,INDEX) MEM_OBJ->getPtr( INDEX )

/*===========================================================================
 * FUNCTION   : QCameraParameters
 *
 * DESCRIPTION: default constructor of QCameraParameters
 *
 * PARAMETERS : none
 *
 * RETURN     : None
 *==========================================================================*/
QCameraParameters::QCameraParameters()
    : CameraParameters(),
      m_pCapability(NULL),
      m_pCamOpsTbl(NULL),
      m_pParamHeap(NULL),
      m_pParamBuf(NULL),
      m_bZslMode(false),
      m_bZslMode_new(false),
      m_bRecordingHint(false),
      m_bRecordingHint_new(false),
      m_bHistogramEnabled(false),
      m_nFaceProcMask(0),
      m_bDebugFps(false),
      mFocusMode(CAM_FOCUS_MODE_MAX),
      mPreviewFormat(CAM_FORMAT_YUV_420_NV21),
      mPictureFormat(CAM_FORMAT_JPEG),
      m_bNeedRestart(false),
      m_bNoDisplayMode(false),
      m_bWNROn(false),
      m_bNeedLockCAF(false),
      m_bCAFLocked(false),
      m_bAFRunning(false),
      m_bInited(false),
      m_nBurstNum(1),
      m_tempMap()
{
    char value[32];
    // TODO: may move to parameter instead of sysprop
    property_get("persist.debug.sf.showfps", value, "0");
    m_bDebugFps = atoi(value) > 0 ? true : false;

    // For thermal mode, it should be set as system property
    // because system property applies to all applications, while
    // parameters only apply to specific app.
    property_get("persist.camera.thermal.mode", value, "fps");
    if (!strcmp(value, "frameskip")) {
        m_ThermalMode = QCAMERA_THERMAL_ADJUST_FRAMESKIP;
    } else {
        if (strcmp(value, "fps"))
            ALOGE("%s: Invalid camera thermal mode %s", __func__, value);
        m_ThermalMode = QCAMERA_THERMAL_ADJUST_FPS;
    }

    memset(&m_LiveSnapshotSize, 0, sizeof(m_LiveSnapshotSize));
}

/*===========================================================================
 * FUNCTION   : QCameraParameters
 *
 * DESCRIPTION: constructor of QCameraParameters
 *
 * PARAMETERS :
 *   @params  : parameters in string
 *
 * RETURN     : None
 *==========================================================================*/
QCameraParameters::QCameraParameters(const String8 &params)
    : CameraParameters(params),
    m_pCapability(NULL),
    m_pCamOpsTbl(NULL),
    m_pParamHeap(NULL),
    m_pParamBuf(NULL),
    m_bZslMode(false),
    m_bZslMode_new(false),
    m_bRecordingHint(false),
    m_bRecordingHint_new(false),
    m_bHistogramEnabled(false),
    m_nFaceProcMask(0),
    m_bDebugFps(false),
    mFocusMode(CAM_FOCUS_MODE_MAX),
    mPreviewFormat(CAM_FORMAT_YUV_420_NV21),
    mPictureFormat(CAM_FORMAT_JPEG),
    m_bNeedRestart(false),
    m_bNoDisplayMode(false),
    m_bWNROn(false),
    m_bNeedLockCAF(false),
    m_bCAFLocked(false),
    m_bAFRunning(false),
    m_bInited(false),
    m_nBurstNum(1),
    m_tempMap()
{
    memset(&m_LiveSnapshotSize, 0, sizeof(m_LiveSnapshotSize));
}

/*===========================================================================
 * FUNCTION   : ~QCameraParameters
 *
 * DESCRIPTION: deconstructor of QCameraParameters
 *
 * PARAMETERS : none
 *
 * RETURN     : None
 *==========================================================================*/
QCameraParameters::~QCameraParameters()
{
    deinit();
}

/*===========================================================================
 * FUNCTION   : createSizesString
 *
 * DESCRIPTION: create string obj contains array of dimensions
 *
 * PARAMETERS :
 *   @sizes   : array of dimensions
 *   @len     : size of dimension array
 *
 * RETURN     : string obj
 *==========================================================================*/
String8 QCameraParameters::createSizesString(const cam_dimension_t *sizes, int len)
{
    String8 str;
    char buffer[32];

    if (len > 0) {
        snprintf(buffer, sizeof(buffer), "%dx%d", sizes[0].width, sizes[0].height);
        str.append(buffer);
    }
    for (int i = 1; i < len; i++) {
        snprintf(buffer, sizeof(buffer), ",%dx%d", sizes[i].width, sizes[i].height);
        str.append(buffer);
    }
    return str;
}

/*===========================================================================
 * FUNCTION   : createValuesString
 *
 * DESCRIPTION: create string obj contains array of values from map when matched
 *              from input values array
 *
 * PARAMETERS :
 *   @values  : array of values
 *   @len     : size of values array
 *   @map     : map contains the mapping between values and enums
 *   @map_len : size of the map
 *
 * RETURN     : string obj
 *==========================================================================*/
String8 QCameraParameters::createValuesString(const int *values,
                                              int len,
                                              const QCameraMap* map,
                                              int map_len)
{
    String8 str;
    int count = 0;

    for (int i = 0; i < len; i++ ) {
        for (int j = 0; j < map_len; j ++)
            if (map[j].val == values[i]) {
                if (NULL != map[j].desc) {
                    if (count > 0) {
                        str.append(",");
                    }
                    str.append(map[j].desc);
                    count++;
                    break; //loop j
                }
            }
    }
    return str;
}

/*===========================================================================
 * FUNCTION   : createValuesStringFromMap
 *
 * DESCRIPTION: create string obj contains array of values directly from map
 *
 * PARAMETERS :
 *   @map     : map contains the mapping between values and enums
 *   @map_len : size of the map
 *
 * RETURN     : string obj
 *==========================================================================*/
String8 QCameraParameters::createValuesStringFromMap(const QCameraMap* map,
                                                     int map_len)
{
    String8 str;

    for (int i = 0; i < map_len; i++) {
        if (NULL != map[i].desc) {
            if (i > 0) {
                str.append(",");
            }
            str.append(map[i].desc);
        }
    }
    return str;
}

/*===========================================================================
 * FUNCTION   : createZoomRatioValuesString
 *
 * DESCRIPTION: create string obj contains array of zoom ratio values
 *
 * PARAMETERS :
 *   @zoomRaios  : array of zoom ratios
 *   @length     : size of the array
 *
 * RETURN     : string obj
 *==========================================================================*/
String8 QCameraParameters::createZoomRatioValuesString(int *zoomRatios, int length)
{
    String8 str;
    char buffer[32] = {0};

    if(length > 0){
        snprintf(buffer, sizeof(buffer), "%d", zoomRatios[0]);
        str.append(buffer);
    }

    for (int i =1;i<length;i++){
        memset(buffer, 0, sizeof(buffer));
        snprintf(buffer, sizeof(buffer), ",%d", zoomRatios[i]);
        str.append(buffer);
    }
    return str;
}

/*===========================================================================
 * FUNCTION   : createHfrValuesString
 *
 * DESCRIPTION: create string obj contains array of hfr values from map when
 *              matched from input hfr values
 *
 * PARAMETERS :
 *   @values  : array of hfr info
 *   @len     : size of the array
 *   @map     : map of hfr string value and enum
 *   map_len  : size of map
 *
 * RETURN     : string obj
 *==========================================================================*/
String8 QCameraParameters::createHfrValuesString(
                                const cam_hfr_info_t *values,
                                int len,
                                const QCameraMap* map,
                                int map_len)
{
    String8 str;
    int count = 0;

    for (int i = 0; i < len; i++ ) {
        for (int j = 0; j < map_len; j ++)
            if (map[j].val == (int)values[i].mode) {
                if (NULL != map[j].desc) {
                    if (count > 0) {
                        str.append(",");
                    }
                     str.append(map[j].desc);
                     count++;
                     break; //loop j
                }
            }
    }
    return str;
}

/*===========================================================================
 * FUNCTION   : createHfrSizesString
 *
 * DESCRIPTION: create string obj contains array of hfr sizes
 *
 * PARAMETERS :
 *   @values  : array of hfr info
 *   @len     : size of the array
 *
 * RETURN     : string obj
 *==========================================================================*/
String8 QCameraParameters::createHfrSizesString(
                                const cam_hfr_info_t *values,
                                int len)
{
    String8 str;
    char buffer[32];

    if (len > 0) {
        snprintf(buffer, sizeof(buffer), "%dx%d",
                 values[0].dim.width, values[0].dim.height);
        str.append(buffer);
    }
    for (int i = 1; i < len; i++) {
        snprintf(buffer, sizeof(buffer), ",%dx%d",
                 values[i].dim.width, values[i].dim.height);
        str.append(buffer);
    }
    return str;
}

/*===========================================================================
 * FUNCTION   : compareFPSValues
 *
 * DESCRIPTION: helper function for fps sorting
 *
 * PARAMETERS :
 *   @p1     : first array element
 *   @p2     : second array element
 *
 * RETURN     : -1 - left element is greater than right
 *              0  - elements are equals
 *              1  - left element is less than right
 *==========================================================================*/
int QCameraParameters::compareFPSValues(const void *p1, const void *p2)
{
    if ( *( (int *) p1) > *( (int *) p2) ) {
        return -1;
    } else if (  *( (int *) p1) < *( (int *) p2) ) {
        return 1;
    }

    return 0;
}

/*===========================================================================
 * FUNCTION   : createFpsString
 *
 * DESCRIPTION: create string obj contains array of FPS rates
 *
 * PARAMETERS :
 *   @fps     : array of fps ranges
 *   @len     : size of the array
 *
 * RETURN     : string obj
 *==========================================================================*/
String8 QCameraParameters::createFpsString(const cam_fps_range_t *fps, int len)
{
    String8 str;
    char buffer[32];
    int duplicate = INT_MAX;

    int *fpsValues = new int[len];

    for (int i = 0; i < len; i++ ) {
        fpsValues[i] = int(fps[i].max_fps);
    }

    qsort(fpsValues, len, sizeof(int), compareFPSValues);

    for (int i = 0; i < len; i++ ) {
        if ( duplicate != fpsValues[i] ) {
            snprintf(buffer, sizeof(buffer), "%d", fpsValues[i]);
            str.append(buffer);
            if (i < len-1) {
                str.append(",");
            }
            duplicate = fpsValues[i];
        }
    }

    delete [] fpsValues;

    return str;
}

/*===========================================================================
 * FUNCTION   : createFpsRangeString
 *
 * DESCRIPTION: create string obj contains array of FPS ranges
 *
 * PARAMETERS :
 *   @fps     : array of fps ranges
 *   @len     : size of the array
 *   @default_fps_index : reference to index of default fps range
 *
 * RETURN     : string obj
 *==========================================================================*/
String8 QCameraParameters::createFpsRangeString(const cam_fps_range_t* fps,
                                                int len,
                                                int &default_fps_index)
{
    String8 str;
    char buffer[32];
    int max_range = 0;
    int min_fps, max_fps;

    if (len > 0) {
        min_fps = int(fps[0].min_fps * 1000);
        max_fps = int(fps[0].max_fps * 1000);
        max_range = max_fps - min_fps;
        default_fps_index = 0;
        snprintf(buffer, sizeof(buffer), "(%d,%d)", min_fps, max_fps);
        str.append(buffer);
    }
    for (int i = 1; i < len; i++) {
        min_fps = int(fps[i].min_fps * 1000);
        max_fps = int(fps[i].max_fps * 1000);
        if (max_range < (max_fps - min_fps)) {
            max_range = max_fps - min_fps;
            default_fps_index = i;
        }
        snprintf(buffer, sizeof(buffer), ",(%d,%d)", min_fps, max_fps);
        str.append(buffer);
    }
    return str;
}

/*===========================================================================
 * FUNCTION   : lookupAttr
 *
 * DESCRIPTION: lookup a value by its name
 *
 * PARAMETERS :
 *   @attr    : map contains <name, value>
 *   @len     : size of the map
 *   @name    : name to be looked up
 *
 * RETURN     : valid value if found
 *              NAME_NOT_FOUND if not found
 *==========================================================================*/
int QCameraParameters::lookupAttr(const QCameraMap arr[], int len, const char *name)
{
    if (name) {
        for (int i = 0; i < len; i++) {
            if (!strcmp(arr[i].desc, name))
                return arr[i].val;
        }
    }
    return NAME_NOT_FOUND;
}

/*===========================================================================
 * FUNCTION   : lookupNameByValue
 *
 * DESCRIPTION: lookup a name by its value
 *
 * PARAMETERS :
 *   @attr    : map contains <name, value>
 *   @len     : size of the map
 *   @value   : value to be looked up
 *
 * RETURN     : name str or NULL if not found
 *==========================================================================*/
const char *QCameraParameters::lookupNameByValue(const QCameraMap arr[], int len, int value)
{
    for (int i = 0; i < len; i++) {
        if (arr[i].val == value) {
            return arr[i].desc;
        }
    }
    return NULL;
}

/*===========================================================================
 * FUNCTION   : setPreviewSize
 *
 * DESCRIPTION: set preview size from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setPreviewSize(const QCameraParameters& params)
{
    int width, height;
    params.getPreviewSize(&width, &height);
    ALOGV("Requested preview size %d x %d", width, height);

    // Validate the preview size
    for (size_t i = 0; i < m_pCapability->preview_sizes_tbl_cnt; ++i) {
        if (width ==  m_pCapability->preview_sizes_tbl[i].width
           && height ==  m_pCapability->preview_sizes_tbl[i].height) {
            // check if need to restart preview in case of preview size change
            int old_width, old_height;
            CameraParameters::getPreviewSize(&old_width, &old_height);
            if (width != old_width || height != old_height) {
                m_bNeedRestart = true;
            }

            // set the new value
            CameraParameters::setPreviewSize(width, height);
            return NO_ERROR;
        }
    }
    ALOGE("Invalid preview size requested: %dx%d", width, height);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setPictureSize
 *
 * DESCRIPTION: set picture size from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setPictureSize(const QCameraParameters& params)
{
    int width, height;
    params.getPictureSize(&width, &height);
    ALOGV("Requested picture size %d x %d", width, height);

    // Validate the picture size
    for (size_t i = 0; i < m_pCapability->picture_sizes_tbl_cnt; ++i) {
        if (width ==  m_pCapability->picture_sizes_tbl[i].width
           && height ==  m_pCapability->picture_sizes_tbl[i].height) {
            // check if need to restart preview in case of picture size change
            int old_width, old_height;
            CameraParameters::getPictureSize(&old_width, &old_height);
            if ((m_bZslMode || m_bRecordingHint) &&
                (width != old_width || height != old_height)) {
                m_bNeedRestart = true;
            }

            // set the new value
            CameraParameters::setPictureSize(width, height);
            return NO_ERROR;
        }
    }
    ALOGE("Invalid picture size requested: %dx%d", width, height);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setVideoSize
 *
 * DESCRIPTION: set video size from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setVideoSize(const QCameraParameters& params)
{
    const char *str= NULL;
    int width, height;
    str = params.get(KEY_VIDEO_SIZE);
    if(!str) {
        //If application didn't set this parameter string, use the values from
        //getPreviewSize() as video dimensions.
        params.getPreviewSize(&width, &height);
        ALOGE("No Record Size requested, use the preview dimensions");
    } else {
        params.getVideoSize(&width, &height);
    }

    // Validate the video size
    for (size_t i = 0; i < m_pCapability->video_sizes_tbl_cnt; ++i) {
        if (width ==  m_pCapability->video_sizes_tbl[i].width
                && height ==  m_pCapability->video_sizes_tbl[i].height) {
            // check if need to restart preview in case of video size change
            int old_width, old_height;
            CameraParameters::getVideoSize(&old_width, &old_height);
            if (m_bRecordingHint &&
               (width != old_width || height != old_height)) {
                m_bNeedRestart = true;
            }

            // set the new value
            CameraParameters::setVideoSize(width, height);
            return NO_ERROR;
        }
    }

    ALOGE("Invalid video size requested: %dx%d", width, height);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setLiveSnapshotSize
 *
 * DESCRIPTION: set live snapshot size
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setLiveSnapshotSize(const QCameraParameters& params)
{
    char value[32];
    property_get("persist.camera.opt.livepic", value, "1");
    bool useOptimal = atoi(value) > 0 ? true : false;

    // use picture size from user setting
    params.getPictureSize(&m_LiveSnapshotSize.width, &m_LiveSnapshotSize.height);

    uint8_t livesnapshot_sizes_tbl_cnt = m_pCapability->livesnapshot_sizes_tbl_cnt;
    cam_dimension_t *livesnapshot_sizes_tbl = &m_pCapability->livesnapshot_sizes_tbl[0];

    // check if HFR is enabled
    const char *hfrStr = params.get(KEY_QC_VIDEO_HIGH_FRAME_RATE);
    cam_hfr_mode_t hfrMode = CAM_HFR_MODE_OFF;
    if (hfrStr != NULL) {
        int32_t value = lookupAttr(HFR_MODES_MAP,
                                   sizeof(HFR_MODES_MAP)/sizeof(QCameraMap),
                                   hfrStr);
        if (value != NAME_NOT_FOUND) {
            // if HFR is enabled, change live snapshot size
            if (value > CAM_HFR_MODE_OFF) {
                for (int i = 0; i < m_pCapability->hfr_tbl_cnt; i++) {
                    if (m_pCapability->hfr_tbl[i].mode == value) {
                        livesnapshot_sizes_tbl_cnt =
                            m_pCapability->hfr_tbl[i].livesnapshot_sizes_tbl_cnt;
                        livesnapshot_sizes_tbl =
                            &m_pCapability->hfr_tbl[i].livesnapshot_sizes_tbl[0];
                        hfrMode = m_pCapability->hfr_tbl[i].mode;
                        break;
                    }
                }
            }
        }
    }

    if (useOptimal || hfrMode != CAM_HFR_MODE_OFF) {
        bool found = false;

        // first check if picture size is within the list of supported sizes
        for (int i = 0; i < livesnapshot_sizes_tbl_cnt; ++i) {
            if (m_LiveSnapshotSize.width == livesnapshot_sizes_tbl[i].width &&
                m_LiveSnapshotSize.height == livesnapshot_sizes_tbl[i].height) {
                found = true;
                break;
            }
        }

        if (!found) {
            // use optimal live snapshot size from supported list,
            // that has same preview aspect ratio
            int width = 0, height = 0;
            params.getPreviewSize(&width, &height);

            double previewAspectRatio = (double)width / height;
            for (int i = 0; i < livesnapshot_sizes_tbl_cnt; ++i) {
                double ratio = (double)livesnapshot_sizes_tbl[i].width /
                                livesnapshot_sizes_tbl[i].height;
                if (fabs(previewAspectRatio - ratio) <= ASPECT_TOLERANCE) {
                    m_LiveSnapshotSize = livesnapshot_sizes_tbl[i];
                    found = true;
                    break;
                }
            }

            if (!found && hfrMode != CAM_HFR_MODE_OFF) {
                // Cannot find matching aspect ration from supported live snapshot list
                // choose the max dim from preview and video size
                ALOGI("%s: Cannot find matching aspect ratio, choose max of preview or video size", __func__);
                params.getVideoSize(&m_LiveSnapshotSize.width, &m_LiveSnapshotSize.height);
                if (m_LiveSnapshotSize.width < width && m_LiveSnapshotSize.height < height) {
                    m_LiveSnapshotSize.width = width;
                    m_LiveSnapshotSize.height = height;
                }
            }
        }
    }
    ALOGI("%s: live snapshot size %d x %d", __func__,
          m_LiveSnapshotSize.width, m_LiveSnapshotSize.height);

    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setPreviewFormat
 *
 * DESCRIPTION: set preview format from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setPreviewFormat(const QCameraParameters& params)
{
    const char *str = params.getPreviewFormat();
    int32_t previewFormat =
        lookupAttr(PREVIEW_FORMATS_MAP,
                   sizeof(PREVIEW_FORMATS_MAP) / sizeof(QCameraMap),
                   str);
    if (previewFormat != NAME_NOT_FOUND) {
        mPreviewFormat = (cam_format_t)previewFormat;

        CameraParameters::setPreviewFormat(str);
        ALOGV("%s: format %d\n", __func__, mPreviewFormat);
        return NO_ERROR;
    }
    ALOGE("Invalid preview format value: %s", (str == NULL) ? "NULL" : str);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setPictureFormat
 *
 * DESCRIPTION: set picture format from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setPictureFormat(const QCameraParameters& params)
{
    const char *str = params.getPictureFormat();
    int32_t pictureFormat =
        lookupAttr(PICTURE_TYPES_MAP,
                   sizeof(PICTURE_TYPES_MAP) / sizeof(QCameraMap),
                   str);
    if (pictureFormat != NAME_NOT_FOUND) {
        mPictureFormat = pictureFormat;

        CameraParameters::setPictureFormat(str);
        ALOGE("%s: format %d\n", __func__, mPictureFormat);
        return NO_ERROR;
    }
    ALOGE("Invalid picture format value: %s", (str == NULL) ? "NULL" : str);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setJpegThumbnailSize
 *
 * DESCRIPTION: set jpeg thumbnail size from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setJpegThumbnailSize(const QCameraParameters& params)
{
    int width = params.getInt(KEY_JPEG_THUMBNAIL_WIDTH);
    int height = params.getInt(KEY_JPEG_THUMBNAIL_HEIGHT);

    ALOGV("requested jpeg thumbnail size %d x %d", width, height);

    int sizes_cnt = sizeof(THUMBNAIL_SIZES_MAP) / sizeof(cam_dimension_t);

    int pic_width = 0, pic_height = 0;
    params.getPictureSize(&pic_width, &pic_height);
    if (pic_height == 0) {
        ALOGE("%s: picture size is invalid (%d x %d)", __func__, pic_width, pic_height);
        return BAD_VALUE;
    }
    double picAspectRatio = (double)pic_width / pic_height;

    int optimalWidth = 0, optimalHeight = 0;
    if (width != 0 || height != 0) {
        // If input jpeg thumnmail size is (0,0), meaning no thumbnail needed
        // hornor this setting.
        // Otherwise, find optimal jpeg thumbnail size that has same aspect ration
        // as picture size

        // Try to find a size matches aspect ratio and has the largest width
        for (int i = 0; i < sizes_cnt; i++) {
            if (THUMBNAIL_SIZES_MAP[i].height == 0) {
                // No thumbnail case, just skip
                continue;
            }
            double ratio =
                (double)THUMBNAIL_SIZES_MAP[i].width / THUMBNAIL_SIZES_MAP[i].height;
            if (fabs(ratio - picAspectRatio) > ASPECT_TOLERANCE)  {
                continue;
            }
            if (THUMBNAIL_SIZES_MAP[i].width > optimalWidth) {
                optimalWidth = THUMBNAIL_SIZES_MAP[i].width;
                optimalHeight = THUMBNAIL_SIZES_MAP[i].height;
            }
        }
    }

    set(KEY_JPEG_THUMBNAIL_WIDTH, optimalWidth);
    set(KEY_JPEG_THUMBNAIL_HEIGHT, optimalHeight);
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setJpegQuality
 *
 * DESCRIPTION: set jpeg encpding quality from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setJpegQuality(const QCameraParameters& params)
{
    int32_t rc = NO_ERROR;
    int quality = params.getInt(KEY_JPEG_QUALITY);
    if (quality >= 0 && quality <= 100) {
        set(KEY_JPEG_QUALITY, quality);
    } else {
        ALOGE("%s: Invalid jpeg quality=%d", __func__, quality);
        rc = BAD_VALUE;
    }

    quality = params.getInt(KEY_JPEG_THUMBNAIL_QUALITY);
    if (quality >= 0 && quality <= 100) {
        set(KEY_JPEG_THUMBNAIL_QUALITY, quality);
    } else {
        ALOGE("%s: Invalid jpeg thumbnail quality=%d", __func__, quality);
        rc = BAD_VALUE;
    }
    return rc;
}

/*===========================================================================
 * FUNCTION   : setOrientaion
 *
 * DESCRIPTION: set orientaion from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setOrientation(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_ORIENTATION);

    if (str != NULL) {
        if (strcmp(str, portrait) == 0 || strcmp(str, landscape) == 0) {
            // Camera service needs this to decide if the preview frames and raw
            // pictures should be rotated.
            set(KEY_QC_ORIENTATION, str);
        } else {
            ALOGE("%s: Invalid orientation value: %s", __func__, str);
            return BAD_VALUE;
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setAutoExposure
 *
 * DESCRIPTION: set auto exposure value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setAutoExposure(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_AUTO_EXPOSURE);
    const char *prev_str = get(KEY_QC_AUTO_EXPOSURE);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setAutoExposure(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setPreviewFpsRange
 *
 * DESCRIPTION: set preview FPS range from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setPreviewFpsRange(const QCameraParameters& params)
{
    int minFps,maxFps;
    int prevMinFps, prevMaxFps;
    int rc = NO_ERROR;
    bool found = false;

    CameraParameters::getPreviewFpsRange(&prevMinFps, &prevMaxFps);
    ALOGV("%s: Existing FpsRange Values:(%d, %d)", __func__, prevMinFps, prevMaxFps);
    params.getPreviewFpsRange(&minFps, &maxFps);
    ALOGV("%s: Requested FpsRange Values:(%d, %d)", __func__, minFps, maxFps);

    if(minFps == prevMinFps && maxFps == prevMaxFps) {
        ALOGV("%s: No change in FpsRange", __func__);
        rc = NO_ERROR;
        goto end;
    }
    for(int i = 0; i < m_pCapability->fps_ranges_tbl_cnt; i++) {
        // if the value is in the supported list
        if(minFps >= m_pCapability->fps_ranges_tbl[i].min_fps * 1000 &&
           maxFps <= m_pCapability->fps_ranges_tbl[i].max_fps * 1000) {
            found = true;
            ALOGV("%s: FPS i=%d : minFps = %d, maxFps = %d ", __func__, i, minFps, maxFps);
            setPreviewFpsRange(minFps, maxFps);
            break;
        }
    }
    if(found == false){
        ALOGE("%s: error: FPS range value not supported", __func__);
        rc = BAD_VALUE;
    }
end:
    return rc;
}

/*===========================================================================
 * FUNCTION   : setPreviewFrameRate
 *
 * DESCRIPTION: set preview frame rate from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setPreviewFrameRate(const QCameraParameters& params)
{
    uint16_t fps = (uint16_t)params.getPreviewFrameRate();
    ALOGV("%s: requested preview frame rate is %d", __func__, fps);
    CameraParameters::setPreviewFrameRate(fps);
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setEffect
 *
 * DESCRIPTION: set effect value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setEffect(const QCameraParameters& params)
{
    const char *str = params.get(KEY_EFFECT);
    const char *prev_str = get(KEY_EFFECT);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setEffect(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setFocusMode
 *
 * DESCRIPTION: set focus mode from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setFocusMode(const QCameraParameters& params)
{
    const char *str = params.get(KEY_FOCUS_MODE);
    const char *prev_str = get(KEY_FOCUS_MODE);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setFocusMode(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setBrightness
 *
 * DESCRIPTION: set brightness control value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setBrightness(const QCameraParameters& params)
{
    int currentBrightness = getInt(KEY_QC_BRIGHTNESS);
    int brightness = params.getInt(KEY_QC_BRIGHTNESS);
    if (currentBrightness !=  brightness) {
        if (brightness >= m_pCapability->brightness_ctrl.min_value &&
            brightness <= m_pCapability->brightness_ctrl.max_value) {
            ALOGV(" new brightness value : %d ", brightness);
            return setBrightness(brightness);
        } else {
            ALOGE("%s: invalid value %d out of (%d, %d)",
                  __func__, brightness,
                  m_pCapability->brightness_ctrl.min_value,
                  m_pCapability->brightness_ctrl.max_value);
            return BAD_VALUE;
        }
    } else {
        ALOGV("%s: No brightness value changed.", __func__);
        return NO_ERROR;
    }
}

/*===========================================================================
 * FUNCTION   : setSharpness
 *
 * DESCRIPTION: set sharpness control value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setSharpness(const QCameraParameters& params)
{
    int shaprness = params.getInt(KEY_QC_SHARPNESS);
    int prev_sharp = getInt(KEY_QC_SHARPNESS);
    if (prev_sharp !=  shaprness) {
        if((shaprness >= m_pCapability->sharpness_ctrl.min_value) &&
           (shaprness <= m_pCapability->sharpness_ctrl.max_value)) {
            ALOGV(" new sharpness value : %d ", shaprness);
            return setSharpness(shaprness);
        } else {
            ALOGE("%s: invalid value %d out of (%d, %d)",
                  __func__, shaprness,
                  m_pCapability->sharpness_ctrl.min_value,
                  m_pCapability->sharpness_ctrl.max_value);
            return BAD_VALUE;
        }
    } else {
        ALOGV("%s: No value change in shaprness", __func__);
        return NO_ERROR;
    }
}

/*===========================================================================
 * FUNCTION   : setSkintoneEnahancement
 *
 * DESCRIPTION: set skin tone enhancement factor from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setSkinToneEnhancement(const QCameraParameters& params)
{
    int sceFactor = params.getInt(KEY_QC_SCE_FACTOR);
    int prev_sceFactor = getInt(KEY_QC_SCE_FACTOR);
    if (prev_sceFactor !=  sceFactor) {
        if((sceFactor >= m_pCapability->sce_ctrl.min_value) &&
           (sceFactor <= m_pCapability->sce_ctrl.max_value)) {
            ALOGV(" new Skintone Enhancement value : %d ", sceFactor);
            return setSkinToneEnhancement(sceFactor);
        } else {
            ALOGE("%s: invalid value %d out of (%d, %d)",
                  __func__, sceFactor,
                  m_pCapability->sce_ctrl.min_value,
                  m_pCapability->sce_ctrl.max_value);
            return BAD_VALUE;
        }
    } else {
        ALOGV("%s: No value change in skintone enhancement factor", __func__);
        return NO_ERROR;
    }
}

/*===========================================================================
 * FUNCTION   : setSaturation
 *
 * DESCRIPTION: set saturation control value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setSaturation(const QCameraParameters& params)
{
    int saturation = params.getInt(KEY_QC_SATURATION);
    int prev_sat = getInt(KEY_QC_SATURATION);
    if (prev_sat !=  saturation) {
        if((saturation >= m_pCapability->saturation_ctrl.min_value) &&
           (saturation <= m_pCapability->saturation_ctrl.max_value)) {
            ALOGV(" new saturation value : %d ", saturation);
            return setSaturation(saturation);
        } else {
            ALOGE("%s: invalid value %d out of (%d, %d)",
                  __func__, saturation,
                  m_pCapability->saturation_ctrl.min_value,
                  m_pCapability->saturation_ctrl.max_value);
            return BAD_VALUE;
        }
    } else {
        ALOGV("%s: No value change in saturation factor", __func__);
        return NO_ERROR;
    }
}

/*===========================================================================
 * FUNCTION   : setContrast
 *
 * DESCRIPTION: set contrast control value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setContrast(const QCameraParameters& params)
{
    int contrast = params.getInt(KEY_QC_CONTRAST);
    int prev_contrast = getInt(KEY_QC_CONTRAST);
    if (prev_contrast !=  contrast) {
        if((contrast >= m_pCapability->contrast_ctrl.min_value) &&
           (contrast <= m_pCapability->contrast_ctrl.max_value)) {
            ALOGV(" new contrast value : %d ", contrast);
            int32_t rc = setContrast(contrast);
            return rc;
        } else {
            ALOGE("%s: invalid value %d out of (%d, %d)",
                  __func__, contrast,
                  m_pCapability->contrast_ctrl.min_value,
                  m_pCapability->contrast_ctrl.max_value);
            return BAD_VALUE;
        }
    } else {
        ALOGV("%s: No value change in contrast", __func__);
        return NO_ERROR;
    }
}

/*===========================================================================
 * FUNCTION   : setExposureCompensation
 *
 * DESCRIPTION: set exposure compensation value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setExposureCompensation(const QCameraParameters & params)
{
    int expComp = params.getInt(KEY_EXPOSURE_COMPENSATION);
    int prev_expComp = getInt(KEY_EXPOSURE_COMPENSATION);
    if (prev_expComp !=  expComp) {
        if((expComp >= m_pCapability->exposure_compensation_min) &&
           (expComp <= m_pCapability->exposure_compensation_max)) {
            ALOGV(" new Exposure Compensation value : %d ", expComp);
            return setExposureCompensation(expComp);
        } else {
            ALOGE("%s: invalid value %d out of (%d, %d)",
                  __func__, expComp,
                  m_pCapability->exposure_compensation_min,
                  m_pCapability->exposure_compensation_max);
            return BAD_VALUE;
        }
    } else {
        ALOGV("%s: No value change in Exposure Compensation", __func__);
        return NO_ERROR;
    }
}

/*===========================================================================
 * FUNCTION   : setWhiteBalance
 *
 * DESCRIPTION: set white balance value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setWhiteBalance(const QCameraParameters& params)
{
    const char *str = params.get(KEY_WHITE_BALANCE);
    const char *prev_str = get(KEY_WHITE_BALANCE);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setWhiteBalance(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setAntibanding
 *
 * DESCRIPTION: set antibanding value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setAntibanding(const QCameraParameters& params)
{
    const char *str = params.get(KEY_ANTIBANDING);
    const char *prev_str = get(KEY_ANTIBANDING);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setAntibanding(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setSceneDetect
 *
 * DESCRIPTION: set scenen detect value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setSceneDetect(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_SCENE_DETECT);
    const char *prev_str = get(KEY_QC_SCENE_DETECT);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setSceneDetect(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setVideoHDR
 *
 * DESCRIPTION: set video HDR value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setVideoHDR(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_VIDEO_HDR);
    const char *prev_str = get(KEY_QC_VIDEO_HDR);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setVideoHDR(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setFaceRecognition
 *
 * DESCRIPTION: set face recognition mode from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setFaceRecognition(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_FACE_RECOGNITION);
    const char *prev_str = get(KEY_QC_FACE_RECOGNITION);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            int maxFaces = params.getInt(KEY_QC_MAX_NUM_REQUESTED_FACES);
            return setFaceRecognition(str, maxFaces);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setZoom
 *
 * DESCRIPTION: set zoom value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setZoom(const QCameraParameters& params)
{
    if ((m_pCapability->zoom_supported == 0 ||
         m_pCapability->zoom_ratio_tbl_cnt == 0)) {
        ALOGD("%s: no zoom support", __func__);
        return NO_ERROR;
    }

    int zoomLevel = params.getInt(KEY_ZOOM);
    if((zoomLevel < 0) ||
       (zoomLevel >= m_pCapability->zoom_ratio_tbl_cnt)) {
        ALOGE("%s: invalid value %d out of (%d, %d)",
              __func__, zoomLevel,
              0, m_pCapability->zoom_ratio_tbl_cnt-1);
        return BAD_VALUE;
    }

    int prevZoomLevel = getInt(KEY_ZOOM);
    if (prevZoomLevel == zoomLevel) {
        ALOGV("%s: No value change in contrast", __func__);
        return NO_ERROR;
    }

    return setZoom(zoomLevel);
}

/*===========================================================================
 * FUNCTION   : setISOValue
 *
 * DESCRIPTION: set ISO value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t  QCameraParameters::setISOValue(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_ISO_MODE);
    const char *prev_str = get(KEY_QC_ISO_MODE);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setISOValue(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setRotation
 *
 * DESCRIPTION: set rotation value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setRotation(const QCameraParameters& params)
{
    int rotation = params.getInt(KEY_ROTATION);
    if (rotation != -1) {
        if (rotation == 0 || rotation == 90 ||
            rotation == 180 || rotation == 270) {
            set(KEY_ROTATION, rotation);
        } else {
            ALOGE("Invalid rotation value: %d", rotation);
            return BAD_VALUE;
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setFlash
 *
 * DESCRIPTION: set flash mode from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setFlash(const QCameraParameters& params)
{
    const char *str = params.get(KEY_FLASH_MODE);
    const char *prev_str = get(KEY_FLASH_MODE);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setFlash(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setAecLock
 *
 * DESCRIPTION: set AEC lock value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setAecLock(const QCameraParameters& params)
{
    const char *str = params.get(KEY_AUTO_EXPOSURE_LOCK);
    const char *prev_str = get(KEY_AUTO_EXPOSURE_LOCK);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setAecLock(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setAwbLock
 *
 * DESCRIPTION: set AWB lock from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setAwbLock(const QCameraParameters& params)
{
    const char *str = params.get(KEY_AUTO_WHITEBALANCE_LOCK);
    const char *prev_str = get(KEY_AUTO_WHITEBALANCE_LOCK);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setAwbLock(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setMCEValue
 *
 * DESCRIPTION: set memory color enhancement value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setMCEValue(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_MEMORY_COLOR_ENHANCEMENT);
    const char *prev_str = get(KEY_QC_MEMORY_COLOR_ENHANCEMENT);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setMCEValue(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setDISValue
 *
 * DESCRIPTION: enable/disable DIS from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setDISValue(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_DIS);
    const char *prev_str = get(KEY_QC_DIS);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setDISValue(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setHighFrameRate
 *
 * DESCRIPTION: set hight frame rate value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setHighFrameRate(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_VIDEO_HIGH_FRAME_RATE);
    const char *prev_str = get(KEY_QC_VIDEO_HIGH_FRAME_RATE);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setHighFrameRate(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setLensShadeValue
 *
 * DESCRIPTION: set lens shade value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setLensShadeValue(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_LENSSHADE);
    const char *prev_str = get(KEY_QC_LENSSHADE);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setLensShadeValue(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setFocusAreas
 *
 * DESCRIPTION: set focus areas from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setFocusAreas(const QCameraParameters& params)
{
    const char *str = params.get(KEY_FOCUS_AREAS);
    if (str != NULL) {
        int max_num_af_areas = getInt(KEY_MAX_NUM_FOCUS_AREAS);
        if(max_num_af_areas == 0) {
            ALOGE("%s: max num of AF area is 0, cannot set focus areas", __func__);
            return BAD_VALUE;
        }

        const char *prev_str = get(KEY_FOCUS_AREAS);
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setFocusAreas(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setMeteringAreas
 *
 * DESCRIPTION: set metering areas from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setMeteringAreas(const QCameraParameters& params)
{
    const char *str = params.get(KEY_METERING_AREAS);
    if (str != NULL) {
        int max_num_mtr_areas = getInt(KEY_MAX_NUM_METERING_AREAS);
        if(max_num_mtr_areas == 0) {
            ALOGE("%s: max num of metering areas is 0, cannot set focus areas", __func__);
            return BAD_VALUE;
        }

        const char *prev_str = get(KEY_METERING_AREAS);
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setMeteringAreas(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setSceneMode
 *
 * DESCRIPTION: set scenen mode from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setSceneMode(const QCameraParameters& params)
{
    const char *str = params.get(KEY_SCENE_MODE);
    const char *prev_str = get(KEY_SCENE_MODE);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            if ((strcmp(str, SCENE_MODE_HDR) == 0) ||
                ((prev_str != NULL) && (strcmp(prev_str, SCENE_MODE_HDR) == 0))) {
                ALOGD("%s: scene mode changed between HDR and non-HDR, need restart", __func__);
                m_bNeedRestart = true;

                // set if hdr 1x image is needed
                const char *need_hdr_1x = params.get(KEY_QC_HDR_NEED_1X);
                int32_t value = 0;
                if (need_hdr_1x != NULL && strcmp(need_hdr_1x, VALUE_TRUE) == 0) {
                    value = 1;
                    updateParamEntry(KEY_QC_HDR_NEED_1X, need_hdr_1x);
                    AddSetParmEntryToBatch(m_pParamBuf,
                                           CAM_INTF_PARM_HDR_NEED_1X,
                                           sizeof(value),
                                           &value);
                }
            }
            return setSceneMode(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setSelectableZoneAf
 *
 * DESCRIPTION: set selectable zone auto focus value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setSelectableZoneAf(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_SELECTABLE_ZONE_AF);
    const char *prev_str = get(KEY_QC_SELECTABLE_ZONE_AF);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setSelectableZoneAf(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setAEBracket
 *
 * DESCRIPTION: set AE bracket from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setAEBracket(const QCameraParameters& params)
{
    const char *scene_mode = params.get(KEY_SCENE_MODE);
    if (scene_mode != NULL && strcmp(scene_mode, SCENE_MODE_HDR) == 0) {
        ALOGE("%s: scene mode is HDR, overwrite AE bracket setting to off", __func__);
        return setAEBracket(AE_BRACKET_OFF);
    }

    const char *expStr = params.get(KEY_QC_CAPTURE_BURST_EXPOSURE);
    if (NULL != expStr && strlen(expStr) > 0) {
        set(KEY_QC_CAPTURE_BURST_EXPOSURE, expStr);
    } else {
        char prop[PROPERTY_VALUE_MAX];
        memset(prop, 0, sizeof(prop));
        property_get("persist.capture.burst.exposures", prop, "");
        if (strlen(prop) > 0) {
            set(KEY_QC_CAPTURE_BURST_EXPOSURE, prop);
        } else {
            remove(KEY_QC_CAPTURE_BURST_EXPOSURE);
        }
    }

    const char *str = params.get(KEY_QC_AE_BRACKET_HDR);
    const char *prev_str = get(KEY_QC_AE_BRACKET_HDR);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setAEBracket(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setRedeyeReduction
 *
 * DESCRIPTION: set red eye reduction setting from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setRedeyeReduction(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_REDEYE_REDUCTION);
    const char *prev_str = get(KEY_QC_REDEYE_REDUCTION);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setRedeyeReduction(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setGpsLocation
 *
 * DESCRIPTION: set GPS location information from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setGpsLocation(const QCameraParameters& params)
{
    const char *method = params.get(KEY_GPS_PROCESSING_METHOD);
    if (method) {
        set(KEY_GPS_PROCESSING_METHOD, method);
    }else {
        remove(KEY_GPS_PROCESSING_METHOD);
    }

    const char *latitude = params.get(KEY_GPS_LATITUDE);
    if (latitude) {
        set(KEY_GPS_LATITUDE, latitude);
    }else {
        remove(KEY_GPS_LATITUDE);
    }

    const char *latitudeRef = params.get(KEY_QC_GPS_LATITUDE_REF);
    if (latitudeRef) {
        set(KEY_QC_GPS_LATITUDE_REF, latitudeRef);
    }else {
        remove(KEY_QC_GPS_LATITUDE_REF);
    }

    const char *longitude = params.get(KEY_GPS_LONGITUDE);
    if (longitude) {
        set(KEY_GPS_LONGITUDE, longitude);
    }else {
        remove(KEY_GPS_LONGITUDE);
    }

    const char *longitudeRef = params.get(KEY_QC_GPS_LONGITUDE_REF);
    if (longitudeRef) {
        set(KEY_QC_GPS_LONGITUDE_REF, longitudeRef);
    }else {
        remove(KEY_QC_GPS_LONGITUDE_REF);
    }

    const char *altitudeRef = params.get(KEY_QC_GPS_ALTITUDE_REF);
    if (altitudeRef) {
        set(KEY_QC_GPS_ALTITUDE_REF, altitudeRef);
    }else {
        remove(KEY_QC_GPS_ALTITUDE_REF);
    }

    const char *altitude = params.get(KEY_GPS_ALTITUDE);
    if (altitude) {
        set(KEY_GPS_ALTITUDE, altitude);
    }else {
        remove(KEY_GPS_ALTITUDE);
    }

    const char *status = params.get(KEY_QC_GPS_STATUS);
    if (status) {
        set(KEY_QC_GPS_STATUS, status);
    } else {
        remove(KEY_QC_GPS_STATUS);
    }

    const char *timestamp = params.get(KEY_GPS_TIMESTAMP);
    if (timestamp) {
        set(KEY_GPS_TIMESTAMP, timestamp);
    }else {
        remove(KEY_GPS_TIMESTAMP);
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setNumOfSnapshot
 *
 * DESCRIPTION: set number of snapshot per shutter from user setting
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setNumOfSnapshot()
{
    int nBurstNum = getBurstNum();
    uint8_t nExpnum = 0;

    const char *scene_mode = get(KEY_SCENE_MODE);
    if (scene_mode != NULL && strcmp(scene_mode, SCENE_MODE_HDR) == 0) {
        /* According to Android SDK, only one snapshot,
         * but OEM might have different requirement */
        const char *need_hdr_1x = get(KEY_QC_HDR_NEED_1X);
        if (need_hdr_1x != NULL && strcmp(need_hdr_1x, VALUE_TRUE) == 0) {
            nExpnum = 2; // HDR needs both 1X and processed img
        } else {
            nExpnum = 1; // HDR only needs processed img
        }
    } else {
        const char *bracket_str = get(KEY_QC_AE_BRACKET_HDR);
        if (bracket_str != NULL && strlen(bracket_str) > 0) {
            int value = lookupAttr(BRACKETING_MODES_MAP,
                                   sizeof(BRACKETING_MODES_MAP)/sizeof(QCameraMap),
                                   bracket_str);
            switch (value) {
            case CAM_EXP_BRACKETING_ON:
                {
                    nExpnum = 0;
                    const char *str_val = get(KEY_QC_CAPTURE_BURST_EXPOSURE);
                    if ((str_val != NULL) && (strlen(str_val) > 0)) {
                        char prop[PROPERTY_VALUE_MAX];
                        memset(prop, 0, sizeof(prop));
                        strcpy(prop, str_val);
                        char *saveptr = NULL;
                        char *token = strtok_r(prop, ",", &saveptr);
                        while (token != NULL) {
                            token = strtok_r(NULL, ",", &saveptr);
                            nExpnum++;
                        }
                    }
                    if (nExpnum == 0) {
                        nExpnum = 1;
                    }
                }
                break;
            default:
                nExpnum = 1;
                break;
            }
        }
    }

    ALOGD("%s: nBurstNum = %d, nExpnum = %d", __func__, nBurstNum, nExpnum);
    set(KEY_QC_NUM_SNAPSHOT_PER_SHUTTER, nBurstNum * nExpnum);
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setRecordingHint
 *
 * DESCRIPTION: set recording hint value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setRecordingHint(const QCameraParameters& params)
{
    const char * str = params.get(KEY_RECORDING_HINT);
    const char *prev_str = get(KEY_RECORDING_HINT);
    if (str != NULL) {
        if (prev_str == NULL || strcmp(str, prev_str) != 0) {
            int32_t value = lookupAttr(TRUE_FALSE_MODES_MAP,
                                       sizeof(TRUE_FALSE_MODES_MAP)/sizeof(QCameraMap),
                                       str);
            if(value != NAME_NOT_FOUND){
                updateParamEntry(KEY_RECORDING_HINT, str);
                setRecordingHintValue(value);
                return NO_ERROR;
            } else {
                ALOGE("Invalid recording hint value: %s", str);
                return BAD_VALUE;
            }
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setNoDisplayMode
 *
 * DESCRIPTION: set no display mode from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setNoDisplayMode(const QCameraParameters& params)
{
    const char *str_val  = params.get(KEY_QC_NO_DISPLAY_MODE);
    const char *prev_str = get(KEY_QC_NO_DISPLAY_MODE);
    if(str_val && strlen(str_val) > 0) {
        if (prev_str == NULL || strcmp(str_val, prev_str) != 0) {
            m_bNoDisplayMode = atoi(str_val);
            set(KEY_QC_NO_DISPLAY_MODE, str_val);
            m_bNeedRestart = true;
        }
    } else {
        m_bNoDisplayMode = false;
    }
    ALOGD("Param m_bNoDisplayMode = %d", m_bNoDisplayMode);
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setZslMode
 *
 * DESCRIPTION: set ZSL mode from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setZslMode(const QCameraParameters& params)
{
    const char *str_val  = params.get(KEY_QC_ZSL);
    const char *prev_val  = get(KEY_QC_ZSL);

    if (str_val != NULL) {
        if (prev_val == NULL || strcmp(str_val, prev_val) != 0) {
            int32_t value = lookupAttr(ON_OFF_MODES_MAP,
                                       sizeof(ON_OFF_MODES_MAP)/sizeof(QCameraMap),
                                       str_val);
            if (value != NAME_NOT_FOUND) {
                set(KEY_QC_ZSL, str_val);
                m_bZslMode_new = (value > 0)? true : false;

                // ZSL mode changed, need restart preview
                m_bNeedRestart = true;

                return AddSetParmEntryToBatch(m_pParamBuf,
                                              CAM_INTF_PARM_ZSL_MODE,
                                              sizeof(value),
                                              &value);
            } else {
                ALOGE("Invalid ZSL mode value: %s", str_val);
                return BAD_VALUE;
            }
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setWaveletDenoise
 *
 * DESCRIPTION: set wavelet denoise value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setWaveletDenoise(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_DENOISE);
    const char *prev_str = get(KEY_QC_DENOISE);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setWaveletDenoise(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setCameraMode
 *
 * DESCRIPTION: set camera mode from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setCameraMode(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_CAMERA_MODE);
    if (str != NULL) {
        set(KEY_QC_CAMERA_MODE, str);
    } else {
        remove(KEY_QC_CAMERA_MODE);
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setZslAttributes
 *
 * DESCRIPTION: set ZSL related attributes from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setZslAttributes(const QCameraParameters& params)
{
    // TODO: may switch to pure param instead of sysprop
    char prop[32];

    const char *str = params.get(KEY_QC_ZSL_BURST_INTERVAL);
    if (str != NULL) {
        set(KEY_QC_ZSL_BURST_INTERVAL, str);
    } else {
        memset(prop, 0, sizeof(prop));
        property_get("persist.camera.zsl.interval", prop, "1");
        set(KEY_QC_ZSL_BURST_INTERVAL, prop);
    }

    str = params.get(KEY_QC_ZSL_BURST_LOOKBACK);
    if (str != NULL) {
        set(KEY_QC_ZSL_BURST_LOOKBACK, str);
    } else {
        memset(prop, 0, sizeof(prop));
        property_get("persist.camera.zsl.backlookcnt", prop, "2");
        set(KEY_QC_ZSL_BURST_LOOKBACK, prop);
    }

    str = params.get(KEY_QC_ZSL_QUEUE_DEPTH);
    if (str != NULL) {
        set(KEY_QC_ZSL_QUEUE_DEPTH, str);
    } else {
        memset(prop, 0, sizeof(prop));
        property_get("persist.camera.zsl.queuedepth", prop, "2");
        set(KEY_QC_ZSL_QUEUE_DEPTH, prop);
    }

    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setFlip
 *
 * DESCRIPTION: set preview/ video/ picture flip mode from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setFlip(const QCameraParameters& params)
{
    if ((m_pCapability->qcom_supported_feature_mask & CAM_QCOM_FEATURE_FLIP) == 0) {
        ALOGD("%s: flip is not supported.", __func__);
        return NO_ERROR;
    }

    //check preview flip setting
    const char *str = params.get(KEY_QC_PREVIEW_FLIP);
    if(str != NULL){
        int32_t value = lookupAttr(FLIP_MODES_MAP,
                                   sizeof(FLIP_MODES_MAP)/sizeof(QCameraMap),
                                   str);
        if(value != NAME_NOT_FOUND){
            set(KEY_QC_PREVIEW_FLIP, str);
        }
    }

    // check video filp setting
    str = params.get(KEY_QC_VIDEO_FLIP);
    if(str != NULL){
        int32_t value = lookupAttr(FLIP_MODES_MAP,
                                   sizeof(FLIP_MODES_MAP)/sizeof(QCameraMap),
                                   str);
        if(value != NAME_NOT_FOUND){
            set(KEY_QC_VIDEO_FLIP, str);
        }
    }

    // check picture filp setting
    str = params.get(KEY_QC_SNAPSHOT_PICTURE_FLIP);
    if(str != NULL){
        int32_t value = lookupAttr(FLIP_MODES_MAP,
                                   sizeof(FLIP_MODES_MAP)/sizeof(QCameraMap),
                                   str);
        if(value != NAME_NOT_FOUND){
            set(KEY_QC_SNAPSHOT_PICTURE_FLIP, str);
        }
    }

    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setBurstNum
 *
 * DESCRIPTION: set burst number of snapshot
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setBurstNum(const QCameraParameters& params)
{
    int nBurstNum = params.getInt(KEY_QC_SNAPSHOT_BURST_NUM);
    if (nBurstNum <= 0) {
        // if burst number is not set in parameters,
        // read from sys prop
        char prop[PROPERTY_VALUE_MAX];
        memset(prop, 0, sizeof(prop));
        property_get("persist.camera.snapshot.number", prop, "0");
        nBurstNum = atoi(prop);
        if (nBurstNum <= 0) {
            nBurstNum = 1;
        }
    } else {
        set(KEY_QC_SNAPSHOT_BURST_NUM, nBurstNum);
    }
    m_nBurstNum = nBurstNum;
    return NO_ERROR;
}


/*===========================================================================
 * FUNCTION   : updateParameters
 *
 * DESCRIPTION: update parameters from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *   @needRestart : [output] if preview need restart upon setting changes
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::updateParameters(QCameraParameters& params,
                                            bool &needRestart)
{
    int32_t final_rc = NO_ERROR;
    int32_t rc;
    m_bNeedRestart = false;

    if(initBatchUpdate(m_pParamBuf) < 0 ) {
        ALOGE("%s:Failed to initialize group update table",__func__);
        rc = BAD_TYPE;
        goto UPDATE_PARAM_DONE;
    }

    if ((rc = setPreviewSize(params)))                  final_rc = rc;
    if ((rc = setVideoSize(params)))                    final_rc = rc;
    if ((rc = setPictureSize(params)))                  final_rc = rc;
    if ((rc = setPreviewFormat(params)))                final_rc = rc;
    if ((rc = setPictureFormat(params)))                final_rc = rc;
    if ((rc = setJpegThumbnailSize(params)))            final_rc = rc;
    if ((rc = setJpegQuality(params)))                  final_rc = rc;
    if ((rc = setOrientation(params)))                  final_rc = rc;
    if ((rc = setRotation(params)))                     final_rc = rc;
    if ((rc = setNoDisplayMode(params)))                final_rc = rc;
    if ((rc = setZslMode(params)))                      final_rc = rc;
    if ((rc = setZslAttributes(params)))                final_rc = rc;
    if ((rc = setCameraMode(params)))                   final_rc = rc;
    if ((rc = setRecordingHint(params)))                final_rc = rc;

    if ((rc = setPreviewFpsRange(params)))              final_rc = rc;
    if ((rc = setPreviewFrameRate(params)))             final_rc = rc;
    if ((rc = setAutoExposure(params)))                 final_rc = rc;
    if ((rc = setEffect(params)))                       final_rc = rc;
    if ((rc = setBrightness(params)))                   final_rc = rc;
    if ((rc = setZoom(params)))                         final_rc = rc;
    if ((rc = setSharpness(params)))                    final_rc = rc;
    if ((rc = setSaturation(params)))                   final_rc = rc;
    if ((rc = setContrast(params)))                     final_rc = rc;
    if ((rc = setFocusMode(params)))                    final_rc = rc;
    if ((rc = setISOValue(params)))                     final_rc = rc;
    if ((rc = setSkinToneEnhancement(params)))          final_rc = rc;
    if ((rc = setFlash(params)))                        final_rc = rc;
    if ((rc = setAecLock(params)))                      final_rc = rc;
    if ((rc = setAwbLock(params)))                      final_rc = rc;
    if ((rc = setLensShadeValue(params)))               final_rc = rc;
    if ((rc = setMCEValue(params)))                     final_rc = rc;
    if ((rc = setDISValue(params)))                     final_rc = rc;
    if ((rc = setHighFrameRate(params)))                final_rc = rc;
    if ((rc = setAntibanding(params)))                  final_rc = rc;
    if ((rc = setExposureCompensation(params)))         final_rc = rc;
    if ((rc = setWhiteBalance(params)))                 final_rc = rc;
    if ((rc = setSceneMode(params)))                    final_rc = rc;
    if ((rc = setFocusAreas(params)))                   final_rc = rc;
    if ((rc = setMeteringAreas(params)))                final_rc = rc;
    if ((rc = setSelectableZoneAf(params)))             final_rc = rc;
    if ((rc = setRedeyeReduction(params)))              final_rc = rc;
    if ((rc = setAEBracket(params)))                    final_rc = rc;
    if ((rc = setGpsLocation(params)))                  final_rc = rc;
    if ((rc = setWaveletDenoise(params)))               final_rc = rc;
    if ((rc = setFaceRecognition(params)))              final_rc = rc;
    if ((rc = setFlip(params)))                         final_rc = rc;
    if ((rc = setVideoHDR(params)))                     final_rc = rc;
    if ((rc = setBurstNum(params)))                     final_rc = rc;

    // update live snapshot size after all other parameters are set
    if ((rc = setLiveSnapshotSize(params)))             final_rc = rc;

UPDATE_PARAM_DONE:
    needRestart = m_bNeedRestart;
    return final_rc;
}

/*===========================================================================
 * FUNCTION   : commitParameters
 *
 * DESCRIPTION: commit parameter changes to backend
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::commitParameters()
{
    return commitSetBatch();
}

/*===========================================================================
 * FUNCTION   : initDefaultParameters
 *
 * DESCRIPTION: initialize default parameters for the first time
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::initDefaultParameters()
{
    if(initBatchUpdate(m_pParamBuf) < 0 ) {
        ALOGE("%s:Failed to initialize group update table", __func__);
        return BAD_TYPE;
    }

    /*************************Initialize Values******************************/
    // Set read only parameters from camera capability
    set(KEY_SMOOTH_ZOOM_SUPPORTED,
        m_pCapability->smooth_zoom_supported? VALUE_TRUE : VALUE_FALSE);
    set(KEY_ZOOM_SUPPORTED,
        m_pCapability->zoom_supported? VALUE_TRUE : VALUE_FALSE);
    set(KEY_VIDEO_SNAPSHOT_SUPPORTED,
        m_pCapability->video_snapshot_supported? VALUE_TRUE : VALUE_FALSE);
    set(KEY_VIDEO_STABILIZATION_SUPPORTED,
        m_pCapability->video_stablization_supported? VALUE_TRUE : VALUE_FALSE);
    set(KEY_AUTO_EXPOSURE_LOCK_SUPPORTED,
        m_pCapability->auto_exposure_lock_supported? VALUE_TRUE : VALUE_FALSE);
    set(KEY_AUTO_WHITEBALANCE_LOCK_SUPPORTED,
        m_pCapability->auto_wb_lock_supported? VALUE_TRUE : VALUE_FALSE);
    set(KEY_QC_SUPPORTED_CAMERA_FEATURES,
        m_pCapability->qcom_supported_feature_mask);
    set(KEY_MAX_NUM_DETECTED_FACES_HW, m_pCapability->max_num_roi);
    set(KEY_MAX_NUM_DETECTED_FACES_SW, m_pCapability->max_num_roi);
    set(KEY_QC_MAX_NUM_REQUESTED_FACES, m_pCapability->max_num_roi);
    // Set focal length, horizontal view angle, and vertical view angle
    setFloat(KEY_FOCAL_LENGTH, m_pCapability->focal_length);
    setFloat(KEY_HORIZONTAL_VIEW_ANGLE, m_pCapability->hor_view_angle);
    setFloat(KEY_VERTICAL_VIEW_ANGLE, m_pCapability->ver_view_angle);

    // Set supported preview sizes
    if (m_pCapability->preview_sizes_tbl_cnt > 0 &&
        m_pCapability->preview_sizes_tbl_cnt <= MAX_SIZES_CNT) {
        String8 previewSizeValues = createSizesString(
                m_pCapability->preview_sizes_tbl, m_pCapability->preview_sizes_tbl_cnt);
        set(KEY_SUPPORTED_PREVIEW_SIZES, previewSizeValues.string());
        ALOGD("%s: supported preview sizes: %s", __func__, previewSizeValues.string());
        // Set default preview size
        CameraParameters::setPreviewSize(m_pCapability->preview_sizes_tbl[0].width,
                                         m_pCapability->preview_sizes_tbl[0].height);
    } else {
        ALOGE("%s: supported preview sizes cnt is 0 or exceeds max!!!", __func__);
    }

    // Set supported video sizes
    if (m_pCapability->video_sizes_tbl_cnt > 0 &&
        m_pCapability->video_sizes_tbl_cnt <= MAX_SIZES_CNT) {
        String8 videoSizeValues = createSizesString(
                m_pCapability->video_sizes_tbl, m_pCapability->video_sizes_tbl_cnt);
        set(KEY_SUPPORTED_VIDEO_SIZES, videoSizeValues.string());
        ALOGD("%s: supported video sizes: %s", __func__, videoSizeValues.string());
        // Set default video size
        CameraParameters::setVideoSize(m_pCapability->video_sizes_tbl[0].width,
                                       m_pCapability->video_sizes_tbl[0].height);

        //Set preferred Preview size for video
        String8 vSize = createSizesString(&m_pCapability->video_sizes_tbl[0], 1);
        set(KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO, vSize.string());
    } else {
        ALOGE("%s: supported video sizes cnt is 0 or exceeds max!!!", __func__);
    }

    // Set supported picture sizes
    if (m_pCapability->picture_sizes_tbl_cnt > 0 &&
        m_pCapability->picture_sizes_tbl_cnt <= MAX_SIZES_CNT) {
        String8 pictureSizeValues = createSizesString(
                m_pCapability->picture_sizes_tbl, m_pCapability->picture_sizes_tbl_cnt);
        set(KEY_SUPPORTED_PICTURE_SIZES, pictureSizeValues.string());
        ALOGD("%s: supported pic sizes: %s", __func__, pictureSizeValues.string());
        // Set default picture size to the smallest resolution
        CameraParameters::setPictureSize(
           m_pCapability->picture_sizes_tbl[m_pCapability->picture_sizes_tbl_cnt-1].width,
           m_pCapability->picture_sizes_tbl[m_pCapability->picture_sizes_tbl_cnt-1].height);
    } else {
        ALOGE("%s: supported picture sizes cnt is 0 or exceeds max!!!", __func__);
    }

    // Set supported thumbnail sizes
    String8 thumbnailSizeValues = createSizesString(
            THUMBNAIL_SIZES_MAP,
            sizeof(THUMBNAIL_SIZES_MAP)/sizeof(cam_dimension_t));
    set(KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES, thumbnailSizeValues.string());
    // Set default thumnail size
    set(KEY_JPEG_THUMBNAIL_WIDTH, THUMBNAIL_SIZES_MAP[0].width);
    set(KEY_JPEG_THUMBNAIL_HEIGHT, THUMBNAIL_SIZES_MAP[0].height);

    // Set supported livesnapshot sizes
    if (m_pCapability->livesnapshot_sizes_tbl_cnt > 0 &&
        m_pCapability->livesnapshot_sizes_tbl_cnt <= MAX_SIZES_CNT) {
        String8 liveSnpashotSizeValues = createSizesString(
                m_pCapability->livesnapshot_sizes_tbl,
                m_pCapability->livesnapshot_sizes_tbl_cnt);
        set(KEY_QC_SUPPORTED_LIVESNAPSHOT_SIZES, liveSnpashotSizeValues.string());
        ALOGI("%s: supported live snapshot sizes: %s", __func__, liveSnpashotSizeValues.string());
        m_LiveSnapshotSize =
            m_pCapability->livesnapshot_sizes_tbl[m_pCapability->livesnapshot_sizes_tbl_cnt-1];
    }

    // Set supported preview formats
    String8 previewFormatValues = createValuesString(
            (int *)m_pCapability->supported_preview_fmts,
            m_pCapability->supported_preview_fmt_cnt,
            PREVIEW_FORMATS_MAP,
            sizeof(PREVIEW_FORMATS_MAP)/sizeof(QCameraMap));
    set(KEY_SUPPORTED_PREVIEW_FORMATS, previewFormatValues.string());
    // Set default preview format
    CameraParameters::setPreviewFormat(PIXEL_FORMAT_YUV420SP);

    // Set default Video Format
    set(KEY_VIDEO_FRAME_FORMAT, PIXEL_FORMAT_YUV420SP);

    // Set supported picture formats
    String8 pictureTypeValues(PIXEL_FORMAT_JPEG);
    String8 str = createValuesString(
            (int *)m_pCapability->supported_raw_fmts,
            m_pCapability->supported_raw_fmt_cnt,
            PICTURE_TYPES_MAP,
            sizeof(PICTURE_TYPES_MAP)/sizeof(QCameraMap));
    if (str.string() != NULL) {
        pictureTypeValues.append(",");
        pictureTypeValues.append(str);
    }

    set(KEY_SUPPORTED_PICTURE_FORMATS, pictureTypeValues.string());
    // Set default picture Format
    CameraParameters::setPictureFormat(PIXEL_FORMAT_JPEG);
    // Set raw image size
    char raw_size_str[32];
    snprintf(raw_size_str, sizeof(raw_size_str), "%dx%d",
             m_pCapability->raw_dim.width, m_pCapability->raw_dim.height);
    set(KEY_QC_RAW_PICUTRE_SIZE, raw_size_str);

    //set default jpeg quality and thumbnail quality
    set(KEY_JPEG_QUALITY, 85);
    set(KEY_JPEG_THUMBNAIL_QUALITY, 85);

    // Set FPS ranges
    if (m_pCapability->fps_ranges_tbl_cnt > 0 &&
        m_pCapability->fps_ranges_tbl_cnt <= MAX_SIZES_CNT) {
        int default_fps_index = 0;
        String8 fpsRangeValues = createFpsRangeString(m_pCapability->fps_ranges_tbl,
                                                      m_pCapability->fps_ranges_tbl_cnt,
                                                      default_fps_index);
        set(KEY_SUPPORTED_PREVIEW_FPS_RANGE, fpsRangeValues.string());

        int min_fps =
            int(m_pCapability->fps_ranges_tbl[default_fps_index].min_fps * 1000);
        int max_fps =
            int(m_pCapability->fps_ranges_tbl[default_fps_index].max_fps * 1000);
        setPreviewFpsRange(min_fps, max_fps);

        // Set legacy preview fps
        String8 fpsValues = createFpsString(m_pCapability->fps_ranges_tbl,
                                            m_pCapability->fps_ranges_tbl_cnt);
        set(KEY_SUPPORTED_PREVIEW_FRAME_RATES, fpsValues.string());
        CameraParameters::setPreviewFrameRate(int(m_pCapability->fps_ranges_tbl[default_fps_index].max_fps));
    } else {
        ALOGE("%s: supported fps ranges cnt is 0 or exceeds max!!!", __func__);
    }

    // Set supported focus modes
    if (m_pCapability->supported_focus_modes_cnt > 0) {
        String8 focusModeValues = createValuesString(
                (int *)m_pCapability->supported_focus_modes,
                m_pCapability->supported_focus_modes_cnt,
                FOCUS_MODES_MAP,
                sizeof(FOCUS_MODES_MAP)/sizeof(QCameraMap));
        set(KEY_SUPPORTED_FOCUS_MODES, focusModeValues);

        // Set default focus mode and update corresponding parameter buf
        const char *focusMode = lookupNameByValue(FOCUS_MODES_MAP,
                                             sizeof(FOCUS_MODES_MAP)/sizeof(QCameraMap),
                                             m_pCapability->supported_focus_modes[0]);
        if (focusMode != NULL) {
            setFocusMode(focusMode);
        } else {
            setFocusMode(FOCUS_MODE_FIXED);
        }
    } else {
        ALOGE("%s: supported focus modes cnt is 0!!!", __func__);
    }

    // Set focus areas
    if (m_pCapability->max_num_focus_areas > MAX_ROI) {
        m_pCapability->max_num_focus_areas = MAX_ROI;
    }
    set(KEY_MAX_NUM_FOCUS_AREAS, m_pCapability->max_num_focus_areas);
    if (m_pCapability->max_num_focus_areas > 0) {
        setFocusAreas(DEFAULT_CAMERA_AREA);
    }

    // Set metering areas
    if (m_pCapability->max_num_metering_areas > MAX_ROI) {
        m_pCapability->max_num_metering_areas = MAX_ROI;
    }
    set(KEY_MAX_NUM_METERING_AREAS, m_pCapability->max_num_metering_areas);
    if (m_pCapability->max_num_metering_areas > 0) {
        setMeteringAreas(DEFAULT_CAMERA_AREA);
    }

    // Set Saturation
    set(KEY_QC_MIN_SATURATION, m_pCapability->saturation_ctrl.min_value);
    set(KEY_QC_MAX_SATURATION, m_pCapability->saturation_ctrl.max_value);
    set(KEY_QC_SATURATION_STEP, m_pCapability->saturation_ctrl.step);
    setSaturation(m_pCapability->saturation_ctrl.def_value);

    // Set Sharpness
    set(KEY_QC_MIN_SHARPNESS, m_pCapability->sharpness_ctrl.min_value);
    set(KEY_QC_MAX_SHARPNESS, m_pCapability->sharpness_ctrl.max_value);
    set(KEY_QC_SHARPNESS_STEP, m_pCapability->sharpness_ctrl.step);
    setSharpness(m_pCapability->sharpness_ctrl.def_value);

    // Set Contrast
    set(KEY_QC_MIN_CONTRAST, m_pCapability->contrast_ctrl.min_value);
    set(KEY_QC_MAX_CONTRAST, m_pCapability->contrast_ctrl.max_value);
    set(KEY_QC_CONTRAST_STEP, m_pCapability->contrast_ctrl.step);
    setContrast(m_pCapability->contrast_ctrl.def_value);

    // Set SCE factor
    set(KEY_QC_MIN_SCE_FACTOR, m_pCapability->sce_ctrl.min_value); // -100
    set(KEY_QC_MAX_SCE_FACTOR, m_pCapability->sce_ctrl.max_value); // 100
    set(KEY_QC_SCE_FACTOR_STEP, m_pCapability->sce_ctrl.step);     // 10
    setSkinToneEnhancement(m_pCapability->sce_ctrl.def_value);     // 0

    // Set Brightness
    set(KEY_QC_MIN_BRIGHTNESS, m_pCapability->brightness_ctrl.min_value); // 0
    set(KEY_QC_MAX_BRIGHTNESS, m_pCapability->brightness_ctrl.max_value); // 6
    set(KEY_QC_BRIGHTNESS_STEP, m_pCapability->brightness_ctrl.step);     // 1
    setBrightness(m_pCapability->brightness_ctrl.def_value);

    // Set Auto exposure
    String8 autoExposureValues = createValuesString(
            (int *)m_pCapability->supported_aec_modes,
            m_pCapability->supported_aec_modes_cnt,
            AUTO_EXPOSURE_MAP,
            sizeof(AUTO_EXPOSURE_MAP) / sizeof(QCameraMap));
    set(KEY_QC_SUPPORTED_AUTO_EXPOSURE, autoExposureValues.string());
    setAutoExposure(AUTO_EXPOSURE_FRAME_AVG);

    // Set Exposure Compensation
    set(KEY_MAX_EXPOSURE_COMPENSATION, m_pCapability->exposure_compensation_max); // 12
    set(KEY_MIN_EXPOSURE_COMPENSATION, m_pCapability->exposure_compensation_min); // -12
    setFloat(KEY_EXPOSURE_COMPENSATION_STEP, m_pCapability->exposure_compensation_step); // 1/6
    setExposureCompensation(m_pCapability->exposure_compensation_default); // 0

    // Set Antibanding
    String8 antibandingValues = createValuesString(
            (int *)m_pCapability->supported_antibandings,
            m_pCapability->supported_antibandings_cnt,
            ANTIBANDING_MODES_MAP,
            sizeof(ANTIBANDING_MODES_MAP) / sizeof(QCameraMap));
    set(KEY_SUPPORTED_ANTIBANDING, antibandingValues);
    setAntibanding(ANTIBANDING_OFF);

    // Set Effect
    String8 effectValues = createValuesString(
            (int *)m_pCapability->supported_effects,
            m_pCapability->supported_effects_cnt,
            EFFECT_MODES_MAP,
            sizeof(EFFECT_MODES_MAP) / sizeof(QCameraMap));
    set(KEY_SUPPORTED_EFFECTS, effectValues);
    setEffect(EFFECT_NONE);

    // Set WhiteBalance
    String8 whitebalanceValues = createValuesString(
            (int *)m_pCapability->supported_white_balances,
            m_pCapability->supported_white_balances_cnt,
            WHITE_BALANCE_MODES_MAP,
            sizeof(WHITE_BALANCE_MODES_MAP) / sizeof(QCameraMap));
    set(KEY_SUPPORTED_WHITE_BALANCE, whitebalanceValues);
    setWhiteBalance(WHITE_BALANCE_AUTO);

    // Set Flash mode
    String8 flashValues = createValuesString(
            (int *)m_pCapability->supported_flash_modes,
            m_pCapability->supported_flash_modes_cnt,
            FLASH_MODES_MAP,
            sizeof(FLASH_MODES_MAP) / sizeof(QCameraMap));
    set(KEY_SUPPORTED_FLASH_MODES, flashValues);
    setFlash(FLASH_MODE_OFF);

    // Set Scene Mode
    String8 sceneModeValues = createValuesString(
            (int *)m_pCapability->supported_scene_modes,
            m_pCapability->supported_scene_modes_cnt,
            SCENE_MODES_MAP,
            sizeof(SCENE_MODES_MAP) / sizeof(QCameraMap));
    set(KEY_SUPPORTED_SCENE_MODES, sceneModeValues);
    setSceneMode(SCENE_MODE_AUTO);

    // Set ISO Mode
    String8 isoValues = createValuesString(
            (int *)m_pCapability->supported_iso_modes,
            m_pCapability->supported_iso_modes_cnt,
            ISO_MODES_MAP,
            sizeof(ISO_MODES_MAP) / sizeof(QCameraMap));
    set(KEY_QC_SUPPORTED_ISO_MODES, isoValues);
    setISOValue(ISO_AUTO);

    // Set HFR
    String8 hfrValues = createHfrValuesString(
            m_pCapability->hfr_tbl,
            m_pCapability->hfr_tbl_cnt,
            HFR_MODES_MAP,
            sizeof(HFR_MODES_MAP) / sizeof(QCameraMap));
    set(KEY_QC_SUPPORTED_VIDEO_HIGH_FRAME_RATE_MODES, hfrValues.string());
    String8 hfrSizeValues = createHfrSizesString(
            m_pCapability->hfr_tbl,
            m_pCapability->hfr_tbl_cnt);
    set(KEY_QC_SUPPORTED_HFR_SIZES, hfrSizeValues.string());
    setHighFrameRate(VIDEO_HFR_OFF);

    // Set Focus algorithms
    String8 focusAlgoValues = createValuesString(
            (int *)m_pCapability->supported_focus_algos,
            m_pCapability->supported_focus_algos_cnt,
            FOCUS_ALGO_MAP,
            sizeof(FOCUS_ALGO_MAP) / sizeof(QCameraMap));
    set(KEY_QC_SUPPORTED_FOCUS_ALGOS, focusAlgoValues);
    setSelectableZoneAf(FOCUS_ALGO_AUTO);

    // Set Zoom Ratios
    if (m_pCapability->zoom_supported > 0) {
        String8 zoomRatioValues = createZoomRatioValuesString(
                m_pCapability->zoom_ratio_tbl,
                m_pCapability->zoom_ratio_tbl_cnt);
        set(KEY_ZOOM_RATIOS, zoomRatioValues);
        set(KEY_MAX_ZOOM, m_pCapability->zoom_ratio_tbl_cnt - 1);
        setZoom(0);
    }

    // Set Bracketing/HDR
    char prop[PROPERTY_VALUE_MAX];
    memset(prop, 0, sizeof(prop));
    property_get("persist.capture.burst.exposures", prop, "");
    if (strlen(prop) > 0) {
        set(KEY_QC_CAPTURE_BURST_EXPOSURE, prop);
    }
    String8 bracketingValues = createValuesStringFromMap(
            BRACKETING_MODES_MAP,
            sizeof(BRACKETING_MODES_MAP) / sizeof(QCameraMap));
    set(KEY_QC_SUPPORTED_AE_BRACKET_MODES, bracketingValues);
    setAEBracket(AE_BRACKET_OFF);

    // Set Denoise
    String8 denoiseValues = createValuesStringFromMap(
       DENOISE_ON_OFF_MODES_MAP, sizeof(DENOISE_ON_OFF_MODES_MAP) / sizeof(QCameraMap));
    set(KEY_QC_SUPPORTED_DENOISE, denoiseValues.string());
#ifdef DEFAULT_DENOISE_MODE_ON
    setWaveletDenoise(DENOISE_ON);
#else
    setWaveletDenoise(DENOISE_OFF);
#endif

    // Set feature enable/disable
    String8 enableDisableValues = createValuesStringFromMap(
        ENABLE_DISABLE_MODES_MAP, sizeof(ENABLE_DISABLE_MODES_MAP) / sizeof(QCameraMap));

    // Set Lens Shading
    set(KEY_QC_SUPPORTED_LENSSHADE_MODES, enableDisableValues);
    setLensShadeValue(VALUE_ENABLE);

    // Set MCE
    set(KEY_QC_SUPPORTED_MEM_COLOR_ENHANCE_MODES, enableDisableValues);
    setMCEValue(VALUE_ENABLE);

    // Set DIS
    set(KEY_QC_SUPPORTED_DIS_MODES, enableDisableValues);
    setDISValue(VALUE_DISABLE);

    // Set Histogram
    set(KEY_QC_SUPPORTED_HISTOGRAM_MODES, enableDisableValues);
    set(KEY_QC_HISTOGRAM, VALUE_DISABLE);

    //Set Red Eye Reduction
    set(KEY_QC_SUPPORTED_REDEYE_REDUCTION, enableDisableValues);
    setRedeyeReduction(VALUE_DISABLE);

    //Set SkinTone Enhancement
    set(KEY_QC_SUPPORTED_SKIN_TONE_ENHANCEMENT_MODES, enableDisableValues);

    // Set feature on/off
    String8 onOffValues = createValuesStringFromMap(
        ON_OFF_MODES_MAP, sizeof(ON_OFF_MODES_MAP) / sizeof(QCameraMap));

    //Set Scene Detection
    set(KEY_QC_SUPPORTED_SCENE_DETECT, onOffValues);
    setSceneDetect(VALUE_OFF);

    //Set Face Detection
    set(KEY_QC_SUPPORTED_FACE_DETECTION, onOffValues);
    set(KEY_QC_FACE_DETECTION, VALUE_OFF);

    //Set Face Recognition
    set(KEY_QC_SUPPORTED_FACE_RECOGNITION, onOffValues);
    set(KEY_QC_FACE_RECOGNITION, VALUE_OFF);

    //Set ZSL
    set(KEY_QC_SUPPORTED_ZSL_MODES, onOffValues);
#ifdef DEFAULT_ZSL_MODE_ON
    set(KEY_QC_ZSL, VALUE_ON);
    m_bZslMode = true;
#else
    set(KEY_QC_ZSL, VALUE_OFF);
    m_bZslMode = false;
#endif
    m_bZslMode_new = m_bZslMode;

    //Set video HDR
    if ((m_pCapability->qcom_supported_feature_mask & CAM_QCOM_FEATURE_VIDEO_HDR) > 0) {
        set(KEY_QC_SUPPORTED_VIDEO_HDR_MODES, onOffValues);
        set(KEY_QC_VIDEO_HDR, VALUE_OFF);
    }

    //Set Touch AF/AEC
    String8 touchValues = createValuesStringFromMap(
       TOUCH_AF_AEC_MODES_MAP, sizeof(TOUCH_AF_AEC_MODES_MAP) / sizeof(QCameraMap));

    set(KEY_QC_SUPPORTED_TOUCH_AF_AEC, touchValues);
    set(KEY_QC_TOUCH_AF_AEC, TOUCH_AF_AEC_OFF);

    //set flip mode
    if ((m_pCapability->qcom_supported_feature_mask & CAM_QCOM_FEATURE_FLIP) > 0) {
        String8 flipModes = createValuesStringFromMap(
           FLIP_MODES_MAP, sizeof(FLIP_MODES_MAP) / sizeof(QCameraMap));
        set(KEY_QC_SUPPORTED_FLIP_MODES, flipModes);
        set(KEY_QC_PREVIEW_FLIP, FLIP_MODE_OFF);
        set(KEY_QC_VIDEO_FLIP, FLIP_MODE_OFF);
        set(KEY_QC_SNAPSHOT_PICTURE_FLIP, FLIP_MODE_OFF);
    }

    // Set default Auto Exposure lock value
    setAecLock(VALUE_FALSE);

    // Set default AWB_LOCK lock value
    setAwbLock(VALUE_FALSE);

    // Set default Camera mode
    set(KEY_QC_CAMERA_MODE, 0);

    // TODO: hardcode for now until mctl add support for min_num_pp_bufs
    m_pCapability->min_num_pp_bufs = 3;

    int32_t rc = commitParameters();
    if (rc == NO_ERROR) {
        rc = setNumOfSnapshot();
    }
    return rc;
}

/*===========================================================================
 * FUNCTION   : init
 *
 * DESCRIPTION: initialize parameter obj
 *
 * PARAMETERS :
 *   @capabilities  : ptr to camera capabilities
 *   @mmops         : ptr to memory ops table for mapping/unmapping
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::init(cam_capability_t *capabilities, mm_camera_vtbl_t *mmOps)
{
    int32_t rc = NO_ERROR;

    m_pCapability = capabilities;
    m_pCamOpsTbl = mmOps;

    //Allocate Set Param Buffer
    m_pParamHeap = new QCameraHeapMemory(QCAMERA_ION_USE_CACHE);
    rc = m_pParamHeap->allocate(1, sizeof(parm_buffer_t));
    if(rc != OK) {
        rc = NO_MEMORY;
        ALOGE("Failed to allocate SETPARM Heap memory");
        goto TRANS_INIT_ERROR1;
    }

    //Map memory for parameters buffer
    rc = m_pCamOpsTbl->ops->map_buf(m_pCamOpsTbl->camera_handle,
                             CAM_MAPPING_BUF_TYPE_PARM_BUF,
                             m_pParamHeap->getFd(0),
                             sizeof(parm_buffer_t));
    if(rc < 0) {
        ALOGE("%s:failed to map SETPARM buffer",__func__);
        rc = FAILED_TRANSACTION;
        goto TRANS_INIT_ERROR2;
    }
    m_pParamBuf = (parm_buffer_t*) DATA_PTR(m_pParamHeap,0);

    initDefaultParameters();

    m_bInited = true;

    goto TRANS_INIT_DONE;

TRANS_INIT_ERROR2:
    m_pParamHeap->deallocate();

TRANS_INIT_ERROR1:
    delete m_pParamHeap;
    m_pParamHeap = NULL;

TRANS_INIT_DONE:
    return rc;
}

/*===========================================================================
 * FUNCTION   : deinit
 *
 * DESCRIPTION: deinitialize
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
void QCameraParameters::deinit()
{
    if (!m_bInited) {
        return;
    }

    //clear all entries in the map
    String8 emptyStr;
    QCameraParameters::unflatten(emptyStr);

    if (NULL != m_pCamOpsTbl) {
        m_pCamOpsTbl->ops->unmap_buf(
                             m_pCamOpsTbl->camera_handle,
                             CAM_MAPPING_BUF_TYPE_PARM_BUF);
        m_pCamOpsTbl = NULL;
    }
    m_pCapability = NULL;
    if (NULL != m_pParamHeap) {
        m_pParamHeap->deallocate();
        delete m_pParamHeap;
        m_pParamHeap = NULL;
        m_pParamBuf = NULL;
    }

    m_tempMap.clear();
}

/*===========================================================================
 * FUNCTION   : parse_pair
 *
 * DESCRIPTION: helper function to parse string like "640x480" or "10000,20000"
 *
 * PARAMETERS :
 *   @str     : input string to be parse
 *   @first   : [output] first value of the pair
 *   @second  : [output]  second value of the pair
 *   @delim   : [input] delimeter to seperate the pair
 *   @endptr  : [output] ptr to the end of the pair string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::parse_pair(const char *str,
                                      int *first,
                                      int *second,
                                      char delim,
                                      char **endptr = NULL)
{
    // Find the first integer.
    char *end;
    int w = (int)strtol(str, &end, 10);
    // If a delimeter does not immediately follow, give up.
    if (*end != delim) {
        ALOGE("Cannot find delimeter (%c) in str=%s", delim, str);
        return BAD_VALUE;
    }

    // Find the second integer, immediately after the delimeter.
    int h = (int)strtol(end+1, &end, 10);

    *first = w;
    *second = h;

    if (endptr) {
        *endptr = end;
    }

    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : parseSizesList
 *
 * DESCRIPTION: helper function to parse string containing sizes
 *
 * PARAMETERS :
 *   @sizesStr: [input] input string to be parse
 *   @sizes   : [output] reference to store parsed sizes
 *
 * RETURN     : none
 *==========================================================================*/
void QCameraParameters::parseSizesList(const char *sizesStr, Vector<Size> &sizes)
{
    if (sizesStr == 0) {
        return;
    }

    char *sizeStartPtr = (char *)sizesStr;

    while (true) {
        int width, height;
        int success = parse_pair(sizeStartPtr, &width, &height, 'x',
                                 &sizeStartPtr);
        if (success == -1 || (*sizeStartPtr != ',' && *sizeStartPtr != '\0')) {
            ALOGE("Picture sizes string \"%s\" contains invalid character.", sizesStr);
            return;
        }
        sizes.push(Size(width, height));

        if (*sizeStartPtr == '\0') {
            return;
        }
        sizeStartPtr++;
    }
}

/*===========================================================================
 * FUNCTION   : getSupportedHfrSizes
 *
 * DESCRIPTION: return supported HFR sizes
 *
 * PARAMETERS :
 *   @sizes  : [output] reference to a vector storing supported HFR sizes
 *
 * RETURN     : none
 *==========================================================================*/
void QCameraParameters::getSupportedHfrSizes(Vector<Size> &sizes)
{
    const char *hfrSizesStr = get(KEY_QC_SUPPORTED_HFR_SIZES);
    parseSizesList(hfrSizesStr, sizes);
}

/*===========================================================================
 * FUNCTION   : adjustPreviewFpsRanges
 *
 * DESCRIPTION: adjust preview FPS ranges
 *              according to external events
 *
 * PARAMETERS :
 *   @minFPS  : min FPS value
 *   @maxFPS  : max FPS value
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::adjustPreviewFpsRange(cam_fps_range_t *fpsRange)
{
    if ( fpsRange == NULL ) {
        return BAD_VALUE;
    }

    if ( m_pParamBuf == NULL ) {
        return NO_INIT;
    }

    int32_t rc = initBatchUpdate(m_pParamBuf);
    if ( rc != NO_ERROR ) {
        ALOGE("%s:Failed to initialize group update table", __func__);
        return rc;
    }

    rc = AddSetParmEntryToBatch(m_pParamBuf,
                                  CAM_INTF_PARM_FPS_RANGE,
                                  sizeof(cam_fps_range_t),
                                  fpsRange);
    if ( rc != NO_ERROR ) {
        ALOGE("%s: Parameters batch failed",__func__);
        return rc;
    }

    rc = commitSetBatch();
    if ( rc != NO_ERROR ) {
        ALOGE("%s:Failed to commit batch parameters", __func__);
        return rc;
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : setPreviewFpsRanges
 *
 * DESCRIPTION: set preview FPS ranges
 *
 * PARAMETERS :
 *   @minFPS  : min FPS value
 *   @maxFPS  : max FPS value
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setPreviewFpsRange(int minFPS, int maxFPS)
{
    char str[32];
    snprintf(str, sizeof(str), "%d,%d", minFPS, maxFPS);
    ALOGD("%s: Setting preview fps range %s", __func__, str);
    updateParamEntry(KEY_PREVIEW_FPS_RANGE, str);
    cam_fps_range_t fps_range = {minFPS / 1000.0, maxFPS / 1000.0};
    return AddSetParmEntryToBatch(m_pParamBuf,
                                  CAM_INTF_PARM_FPS_RANGE,
                                  sizeof(cam_fps_range_t),
                                  &fps_range);
}

/*===========================================================================
 * FUNCTION   : setAutoExposure
 *
 * DESCRIPTION: set auto exposure
 *
 * PARAMETERS :
 *   @autoExp : auto exposure value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setAutoExposure(const char *autoExp)
{
    if (autoExp != NULL) {
        int32_t value = lookupAttr(AUTO_EXPOSURE_MAP,
                                   sizeof(AUTO_EXPOSURE_MAP)/sizeof(AUTO_EXPOSURE_MAP[0]),
                                   autoExp);
        if (value != NAME_NOT_FOUND) {
            ALOGD("%s: Setting auto exposure %s", __func__, autoExp);
            updateParamEntry(KEY_QC_AUTO_EXPOSURE, autoExp);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_AEC_ALGO_TYPE,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("Invalid auto exposure value: %s", (autoExp == NULL) ? "NULL" : autoExp);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setEffect
 *
 * DESCRIPTION: set effect
 *
 * PARAMETERS :
 *   @effect  : effect value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setEffect(const char *effect)
{
    if (effect != NULL) {
        int32_t value = lookupAttr(EFFECT_MODES_MAP,
                                   sizeof(EFFECT_MODES_MAP)/sizeof(QCameraMap),
                                   effect);
        if (value != NAME_NOT_FOUND) {
            ALOGD("%s: Setting effect %s", __func__, effect);
            updateParamEntry(KEY_EFFECT, effect);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_EFFECT,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("Invalid effect value: %s", (effect == NULL) ? "NULL" : effect);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setBrightness
 *
 * DESCRIPTION: set brightness control value
 *
 * PARAMETERS :
 *   @brightness  : brightness control value
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setBrightness(int brightness)
{
    char val[16];
    sprintf(val, "%d", brightness);
    updateParamEntry(KEY_QC_BRIGHTNESS, val);

    int32_t value = brightness;
    ALOGD("%s: Setting brightness %s", __func__, val);
    return AddSetParmEntryToBatch(m_pParamBuf,
                                  CAM_INTF_PARM_BRIGHTNESS,
                                  sizeof(value),
                                  &value);
}

/*===========================================================================
 * FUNCTION   : setFocusMode
 *
 * DESCRIPTION: set focus mode
 *
 * PARAMETERS :
 *   @focusMode  : focus mode value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setFocusMode(const char *focusMode)
{
    if (focusMode != NULL) {
        int32_t value = lookupAttr(FOCUS_MODES_MAP,
                                   sizeof(FOCUS_MODES_MAP)/sizeof(QCameraMap),
                                   focusMode);
        if (value != NAME_NOT_FOUND) {
            ALOGD("%s: Setting focus mode %s", __func__, focusMode);
            mFocusMode = (cam_focus_mode_type)value;

            // reset need lock CAF flag
            m_bNeedLockCAF = false;
            m_bCAFLocked = false;
            m_bAFRunning = false;

            updateParamEntry(KEY_FOCUS_MODE, focusMode);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_FOCUS_MODE,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("Invalid focus mode value: %s", (focusMode == NULL) ? "NULL" : focusMode);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setSharpness
 *
 * DESCRIPTION: set sharpness control value
 *
 * PARAMETERS :
 *   @sharpness  : sharpness control value
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setSharpness(int sharpness)
{
    char val[16];
    sprintf(val, "%d", sharpness);
    updateParamEntry(KEY_QC_SHARPNESS, val);
    ALOGD("%s: Setting sharpness %s", __func__, val);

    int32_t value = sharpness;
    return AddSetParmEntryToBatch(m_pParamBuf,
                                  CAM_INTF_PARM_SHARPNESS,
                                  sizeof(value),
                                  &value);
}

/*===========================================================================
 * FUNCTION   : setSkinToneEnhancement
 *
 * DESCRIPTION: set skin tone enhancement value
 *
 * PARAMETERS :
 *   @sceFactore  : skin tone enhancement factor value
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setSkinToneEnhancement(int sceFactor)
{
    char val[16];
    sprintf(val, "%d", sceFactor);
    updateParamEntry(KEY_QC_SCE_FACTOR, val);
    ALOGD("%s: Setting skintone enhancement %s", __func__, val);

    int32_t value = sceFactor;
    return AddSetParmEntryToBatch(m_pParamBuf,
                                  CAM_INTF_PARM_SCE_FACTOR,
                                  sizeof(value),
                                  &value);
}

/*===========================================================================
 * FUNCTION   : setSaturation
 *
 * DESCRIPTION: set saturation control value
 *
 * PARAMETERS :
 *   @saturation : saturation control value
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setSaturation(int saturation)
{
    char val[16];
    sprintf(val, "%d", saturation);
    updateParamEntry(KEY_QC_SATURATION, val);
    ALOGD("%s: Setting saturation %s", __func__, val);

    int32_t value = saturation;
    return AddSetParmEntryToBatch(m_pParamBuf,
                                  CAM_INTF_PARM_SATURATION,
                                  sizeof(value),
                                  &value);
}

/*===========================================================================
 * FUNCTION   : setContrast
 *
 * DESCRIPTION: set contrast control value
 *
 * PARAMETERS :
 *   @contrast : contrast control value
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setContrast(int contrast)
{
    char val[16];
    sprintf(val, "%d", contrast);
    updateParamEntry(KEY_QC_CONTRAST, val);
    ALOGD("%s: Setting contrast %s", __func__, val);

    int32_t value = contrast;
    return AddSetParmEntryToBatch(m_pParamBuf,
                                  CAM_INTF_PARM_CONTRAST,
                                  sizeof(value),
                                  &value);
}

/*===========================================================================
 * FUNCTION   : setSceneDetect
 *
 * DESCRIPTION: set scenen detect value
 *
 * PARAMETERS :
 *   @sceneDetect  : scene detect value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setSceneDetect(const char *sceneDetect)
{
    if (sceneDetect != NULL) {
        int32_t value = lookupAttr(ON_OFF_MODES_MAP,
                                   sizeof(ON_OFF_MODES_MAP)/sizeof(QCameraMap),
                                   sceneDetect);
        if (value != NAME_NOT_FOUND) {
            ALOGD("%s: Setting Scene Detect %s", __func__, sceneDetect);
            updateParamEntry(KEY_QC_SCENE_DETECT, sceneDetect);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_ASD_ENABLE,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("Invalid Scene Detect value: %s",
          (sceneDetect == NULL) ? "NULL" : sceneDetect);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setVideoHDR
 *
 * DESCRIPTION: set video HDR value
 *
 * PARAMETERS :
 *   @videoHDR  : svideo HDR value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setVideoHDR(const char *videoHDR)
{
    if (videoHDR != NULL) {
        int32_t value = lookupAttr(ON_OFF_MODES_MAP,
                                   sizeof(ON_OFF_MODES_MAP)/sizeof(QCameraMap),
                                   videoHDR);
        if (value != NAME_NOT_FOUND) {
            ALOGD("%s: Setting Video HDR %s", __func__, videoHDR);
            updateParamEntry(KEY_QC_VIDEO_HDR, videoHDR);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_VIDEO_HDR,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("Invalid Video HDR value: %s",
          (videoHDR == NULL) ? "NULL" : videoHDR);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setFaceRecognition
 *
 * DESCRIPTION: set face recognition value
 *
 * PARAMETERS :
 *   @faceRecog  : face recognition value string
 *   @maxFaces   : number of max faces to be detected/recognized
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setFaceRecognition(const char *faceRecog, int maxFaces)
{
    if (faceRecog != NULL) {
        int32_t value = lookupAttr(ON_OFF_MODES_MAP,
                                   sizeof(ON_OFF_MODES_MAP)/sizeof(QCameraMap),
                                   faceRecog);
        if (value != NAME_NOT_FOUND) {
            ALOGD("%s: Setting face recognition %s", __func__, faceRecog);
            updateParamEntry(KEY_QC_FACE_RECOGNITION, faceRecog);

            int faceProcMask = m_nFaceProcMask;
            if (value > 0) {
                faceProcMask |= CAM_FACE_PROCESS_MASK_RECOGNITION;
            } else {
                faceProcMask &= ~CAM_FACE_PROCESS_MASK_RECOGNITION;
            }

            if(m_nFaceProcMask == faceProcMask) {
                ALOGD("%s: face process mask not changed, no ops here", __func__);
                return NO_ERROR;
            }
            m_nFaceProcMask = faceProcMask;
            ALOGD("%s: FaceProcMask -> %d", __func__, m_nFaceProcMask);

            // set parm for face process
            cam_fd_set_parm_t fd_set_parm;
            memset(&fd_set_parm, 0, sizeof(cam_fd_set_parm_t));
            fd_set_parm.fd_mode = m_nFaceProcMask;
            fd_set_parm.num_fd = maxFaces;

            return AddSetParmEntryToBatch(m_pParamBuf,
                                        CAM_INTF_PARM_FD,
                                        sizeof(fd_set_parm),
                                        &fd_set_parm);
        }
    }
    ALOGE("Invalid face recognition value: %s", (faceRecog == NULL) ? "NULL" : faceRecog);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setZoom
 *
 * DESCRIPTION: set zoom level
 *
 * PARAMETERS :
 *   @zoom_level : zoom level
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setZoom(int zoom_level)
{
    char val[16];
    sprintf(val, "%d", zoom_level);
    updateParamEntry(KEY_ZOOM, val);

    return AddSetParmEntryToBatch(m_pParamBuf,
                                  CAM_INTF_PARM_ZOOM,
                                  sizeof(zoom_level),
                                  &zoom_level);
}

/*===========================================================================
 * FUNCTION   : setISOValue
 *
 * DESCRIPTION: set ISO value
 *
 * PARAMETERS :
 *   @isoValue : ISO value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t  QCameraParameters::setISOValue(const char *isoValue)
{
    if (isoValue != NULL) {
        int32_t value = lookupAttr(ISO_MODES_MAP,
                                   sizeof(ISO_MODES_MAP)/sizeof(QCameraMap),
                                   isoValue);
        if (value != NAME_NOT_FOUND) {
            ALOGD("%s: Setting ISO value %s", __func__, isoValue);
            updateParamEntry(KEY_QC_ISO_MODE, isoValue);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_ISO,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("Invalid ISO value: %s",
          (isoValue == NULL) ? "NULL" : isoValue);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setFlash
 *
 * DESCRIPTION: set f;ash mode
 *
 * PARAMETERS :
 *   @flashStr : LED flash mode value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setFlash(const char *flashStr)
{
    if (flashStr != NULL) {
        int32_t value = lookupAttr(FLASH_MODES_MAP,
                                   sizeof(FLASH_MODES_MAP)/sizeof(QCameraMap),
                                   flashStr);
        if (value != NAME_NOT_FOUND) {
            ALOGD("%s: Setting Flash value %s", __func__, flashStr);
            updateParamEntry(KEY_FLASH_MODE, flashStr);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_LED_MODE,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("Invalid flash value: %s", (flashStr == NULL) ? "NULL" : flashStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setAecLock
 *
 * DESCRIPTION: set AEC lock value
 *
 * PARAMETERS :
 *   @aecLockStr : AEC lock value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setAecLock(const char *aecLockStr)
{
    if (aecLockStr != NULL) {
        int32_t value = lookupAttr(TRUE_FALSE_MODES_MAP,
                                   sizeof(TRUE_FALSE_MODES_MAP)/sizeof(QCameraMap),
                                   aecLockStr);
        if (value != NAME_NOT_FOUND) {
            ALOGD("%s: Setting AECLock value %s", __func__, aecLockStr);
            updateParamEntry(KEY_AUTO_EXPOSURE_LOCK, aecLockStr);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_AEC_LOCK,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("Invalid AECLock value: %s", (aecLockStr == NULL) ? "NULL" : aecLockStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setAwbLock
 *
 * DESCRIPTION: set AWB lock value
 *
 * PARAMETERS :
 *   @awbLockStr : AWB lock value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setAwbLock(const char *awbLockStr)
{
    if (awbLockStr != NULL) {
        int32_t value = lookupAttr(TRUE_FALSE_MODES_MAP,
                                   sizeof(TRUE_FALSE_MODES_MAP)/sizeof(QCameraMap),
                                   awbLockStr);
        if (value != NAME_NOT_FOUND) {
            ALOGD("%s: Setting AWBLock value %s", __func__, awbLockStr);
            updateParamEntry(KEY_AUTO_WHITEBALANCE_LOCK, awbLockStr);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_AWB_LOCK,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("Invalid AWBLock value: %s", (awbLockStr == NULL) ? "NULL" : awbLockStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setMCEValue
 *
 * DESCRIPTION: set memory color enhancement value
 *
 * PARAMETERS :
 *   @mceStr : MCE value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setMCEValue(const char *mceStr)
{
    if (mceStr != NULL) {
        int32_t value = lookupAttr(ENABLE_DISABLE_MODES_MAP,
                                   sizeof(ENABLE_DISABLE_MODES_MAP)/sizeof(QCameraMap),
                                   mceStr);
        if (value != NAME_NOT_FOUND) {
            ALOGD("%s: Setting AWBLock value %s", __func__, mceStr);
            updateParamEntry(KEY_QC_MEMORY_COLOR_ENHANCEMENT, mceStr);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_MCE,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("Invalid MCE value: %s", (mceStr == NULL) ? "NULL" : mceStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setDISValue
 *
 * DESCRIPTION: set DIS value
 *
 * PARAMETERS :
 *   @disStr : DIS value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setDISValue(const char *disStr)
{
    if (disStr != NULL) {
        int32_t value = lookupAttr(ENABLE_DISABLE_MODES_MAP,
                                   sizeof(ENABLE_DISABLE_MODES_MAP)/sizeof(QCameraMap),
                                   disStr);
        if (value != NAME_NOT_FOUND) {
            ALOGD("%s: Setting DIS value %s", __func__, disStr);
            updateParamEntry(KEY_QC_DIS, disStr);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_DIS_ENABLE,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("Invalid DIS value: %s", (disStr == NULL) ? "NULL" : disStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setHighFrameRate
 *
 * DESCRIPTION: set high frame rate
 *
 * PARAMETERS :
 *   @hfrStr : HFR value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setHighFrameRate(const char *hfrStr)
{
    if (hfrStr != NULL) {
        int32_t value = lookupAttr(HFR_MODES_MAP,
                                   sizeof(HFR_MODES_MAP)/sizeof(QCameraMap),
                                   hfrStr);
        if (value != NAME_NOT_FOUND) {
            // HFR value changed, need to restart preview
            m_bNeedRestart = true;
            // Set HFR value
            ALOGD("%s: Setting HFR value %s", __func__, hfrStr);
            updateParamEntry(KEY_QC_VIDEO_HIGH_FRAME_RATE, hfrStr);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_HFR,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("Invalid HFR value: %s", (hfrStr == NULL) ? "NULL" : hfrStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setLensShadeValue
 *
 * DESCRIPTION: set lens shade value
 *
 * PARAMETERS :
 *   @lensSahdeStr : lens shade value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setLensShadeValue(const char *lensShadeStr)
{
    if (lensShadeStr != NULL) {
        int32_t value = lookupAttr(ENABLE_DISABLE_MODES_MAP,
                                   sizeof(ENABLE_DISABLE_MODES_MAP)/sizeof(QCameraMap),
                                   lensShadeStr);
        if (value != NAME_NOT_FOUND) {
            ALOGD("%s: Setting LensShade value %s", __func__, lensShadeStr);
            updateParamEntry(KEY_QC_LENSSHADE, lensShadeStr);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_ROLLOFF,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("Invalid LensShade value: %s",
          (lensShadeStr == NULL) ? "NULL" : lensShadeStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setExposureCompensation
 *
 * DESCRIPTION: set exposure compensation value
 *
 * PARAMETERS :
 *   @expComp : exposure compensation value
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setExposureCompensation(int expComp)
{
    char val[16];
    sprintf(val, "%d", expComp);
    updateParamEntry(KEY_EXPOSURE_COMPENSATION, val);

    // Don't need to pass step as part of setParameter because
    // camera daemon is already aware of it.
    return AddSetParmEntryToBatch(m_pParamBuf,
                                  CAM_INTF_PARM_EXPOSURE_COMPENSATION,
                                  sizeof(expComp),
                                  &expComp);
}

/*===========================================================================
 * FUNCTION   : setWhiteBalance
 *
 * DESCRIPTION: set white balance mode
 *
 * PARAMETERS :
 *   @wbStr   : white balance mode value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setWhiteBalance(const char *wbStr)
{
    if (wbStr != NULL) {
        int32_t value = lookupAttr(WHITE_BALANCE_MODES_MAP,
                                   sizeof(WHITE_BALANCE_MODES_MAP)/sizeof(QCameraMap),
                                   wbStr);
        if (value != NAME_NOT_FOUND) {
            ALOGD("%s: Setting WhiteBalance value %s", __func__, wbStr);
            updateParamEntry(KEY_WHITE_BALANCE, wbStr);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_WHITE_BALANCE,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("Invalid WhiteBalance value: %s", (wbStr == NULL) ? "NULL" : wbStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setAntibanding
 *
 * DESCRIPTION: set antibanding value
 *
 * PARAMETERS :
 *   @antiBandingStr : antibanding value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setAntibanding(const char *antiBandingStr)
{
    if (antiBandingStr != NULL) {
        int32_t value = lookupAttr(ANTIBANDING_MODES_MAP,
                                   sizeof(ANTIBANDING_MODES_MAP)/sizeof(QCameraMap),
                                   antiBandingStr);
        if (value != NAME_NOT_FOUND) {
            ALOGD("%s: Setting AntiBanding value %s", __func__, antiBandingStr);
            updateParamEntry(KEY_ANTIBANDING, antiBandingStr);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_ANTIBANDING,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("Invalid AntiBanding value: %s",
          (antiBandingStr == NULL) ? "NULL" : antiBandingStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setFocusAreas
 *
 * DESCRIPTION: set focus areas
 *
 * PARAMETERS :
 *   @focusAreasStr : focus areas value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setFocusAreas(const char *focusAreasStr)
{
    if (m_pCapability->max_num_focus_areas == 0 ||
        focusAreasStr == NULL) {
        ALOGI("%s: Parameter string is null", __func__);
        return NO_ERROR;
    }

    cam_area_t *areas = (cam_area_t *)malloc(sizeof(cam_area_t) * m_pCapability->max_num_focus_areas);
    if (NULL == areas) {
        ALOGE("%s: No memory for areas", __func__);
        return NO_MEMORY;
    }
    memset(areas, 0, sizeof(cam_area_t) * m_pCapability->max_num_focus_areas);
    int num_areas_found = 0;
    if (parseCameraAreaString(focusAreasStr,
                              m_pCapability->max_num_focus_areas,
                              areas,
                              num_areas_found) != NO_ERROR) {
        ALOGE("%s: Failed to parse the string: %s", __func__, focusAreasStr);
        free(areas);
        return BAD_VALUE;
    }

    if (validateCameraAreas(areas, num_areas_found) == false) {
        ALOGE("%s: invalid areas specified : %s", __func__, focusAreasStr);
        free(areas);
        return BAD_VALUE;
    }

    updateParamEntry(KEY_FOCUS_AREAS, focusAreasStr);

    //for special area string (0, 0, 0, 0, 0), set the num_areas_found to 0,
    //so no action is takenby the lower layer
    if (num_areas_found == 1 &&
        areas[0].rect.left == 0 &&
        areas[0].rect.top == 0 &&
        areas[0].rect.width == 0 &&
        areas[0].rect.height == 0 &&
        areas[0].weight == 0) {
        num_areas_found = 0;
    }

    int previewWidth, previewHeight;
    getPreviewSize(&previewWidth, &previewHeight);
    cam_roi_info_t af_roi_value;
    memset(&af_roi_value, 0, sizeof(cam_roi_info_t));
    af_roi_value.num_roi = num_areas_found;
    for (int i = 0; i < num_areas_found; i++) {
        ALOGD("%s: FocusArea[%d] = (%d, %d, %d, %d)",
              __func__, i, (areas[i].rect.top), (areas[i].rect.left),
              (areas[i].rect.width), (areas[i].rect.height));

        //transform the coords from (-1000, 1000) to (0, previewWidth or previewHeight)
        af_roi_value.roi[i].left = (int32_t)((areas[i].rect.left + 1000.0f) * (previewWidth / 2000.0f));
        af_roi_value.roi[i].top = (int32_t)((areas[i].rect.top + 1000.0f) * (previewHeight / 2000.0f));
        af_roi_value.roi[i].width = (int32_t)(areas[i].rect.width * previewWidth / 2000.0f);
        af_roi_value.roi[i].height = (int32_t)(areas[i].rect.height * previewHeight / 2000.0f);
        af_roi_value.weight[i] = areas[i].weight;
    }
    free(areas);
    return AddSetParmEntryToBatch(m_pParamBuf,
                                  CAM_INTF_PARM_AF_ROI,
                                  sizeof(af_roi_value),
                                  &af_roi_value);
}

/*===========================================================================
 * FUNCTION   : setMeteringAreas
 *
 * DESCRIPTION: set metering areas value
 *
 * PARAMETERS :
 *   @meteringAreasStr : metering areas value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setMeteringAreas(const char *meteringAreasStr)
{
    if (m_pCapability->max_num_metering_areas == 0 ||
        meteringAreasStr == NULL) {
        ALOGI("%s: Parameter string is null", __func__);
        return NO_ERROR;
    }

    cam_area_t *areas = (cam_area_t *)malloc(sizeof(cam_area_t) * m_pCapability->max_num_metering_areas);
    if (NULL == areas) {
        ALOGE("%s: No memory for areas", __func__);
        return NO_MEMORY;
    }
    memset(areas, 0, sizeof(cam_area_t) * m_pCapability->max_num_metering_areas);
    int num_areas_found = 0;
    if (parseCameraAreaString(meteringAreasStr,
                              m_pCapability->max_num_metering_areas,
                              areas,
                              num_areas_found) < 0) {
        ALOGE("%s: Failed to parse the string: %s", __func__, meteringAreasStr);
        free(areas);
        return BAD_VALUE;
    }

    if (validateCameraAreas(areas, num_areas_found) == false) {
        ALOGE("%s: invalid areas specified : %s", __func__, meteringAreasStr);
        free(areas);
        return BAD_VALUE;
    }

    updateParamEntry(KEY_METERING_AREAS, meteringAreasStr);

    //for special area string (0, 0, 0, 0, 0), set the num_areas_found to 0,
    //so no action is takenby the lower layer
    if (num_areas_found == 1 &&
        areas[0].rect.left == 0 &&
        areas[0].rect.top == 0 &&
        areas[0].rect.width == 0 &&
        areas[0].rect.height == 0 &&
        areas[0].weight == 0) {
        num_areas_found = 0;
    }
    cam_set_aec_roi_t aec_roi_value;
    int previewWidth, previewHeight;
    getPreviewSize(&previewWidth, &previewHeight);

    memset(&aec_roi_value, 0, sizeof(cam_set_aec_roi_t));
    if (num_areas_found > 0) {
        aec_roi_value.aec_roi_enable = CAM_AEC_ROI_ON;
        aec_roi_value.aec_roi_type = CAM_AEC_ROI_BY_COORDINATE;

        for (int i = 0; i < num_areas_found; i++) {
            ALOGD("%s: MeteringArea[%d] = (%d, %d, %d, %d)",
                  __func__, i, (areas[i].rect.top), (areas[i].rect.left),
                  (areas[i].rect.width), (areas[i].rect.height));

            //transform the coords from (-1000, 1000) to (0, previewWidth or previewHeight)
            aec_roi_value.cam_aec_roi_position.coordinate[i].x =
                (uint32_t)(((areas[i].rect.left + areas[i].rect.width / 2) + 1000.0f) * previewWidth / 2000.0f) ;
            aec_roi_value.cam_aec_roi_position.coordinate[i].y =
                (uint32_t)(((areas[i].rect.top + areas[i].rect.height / 2) + 1000.0f) * previewHeight / 2000.0f) ;
        }
    } else {
        aec_roi_value.aec_roi_enable = CAM_AEC_ROI_OFF;
    }
    free(areas);
    return AddSetParmEntryToBatch(m_pParamBuf,
                                  CAM_INTF_PARM_AEC_ROI,
                                  sizeof(aec_roi_value),
                                  &aec_roi_value);
}

/*===========================================================================
 * FUNCTION   : setSceneMode
 *
 * DESCRIPTION: set scene mode
 *
 * PARAMETERS :
 *   @sceneModeStr : scene mode value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setSceneMode(const char *sceneModeStr)
{
    if (sceneModeStr != NULL) {
        int32_t value = lookupAttr(SCENE_MODES_MAP,
                                   sizeof(SCENE_MODES_MAP)/sizeof(QCameraMap),
                                   sceneModeStr);
        if (value != NAME_NOT_FOUND) {
            ALOGV("%s: Setting SceneMode %s", __func__, sceneModeStr);
            updateParamEntry(KEY_SCENE_MODE, sceneModeStr);
            int32_t rc = AddSetParmEntryToBatch(m_pParamBuf,
                                                CAM_INTF_PARM_BESTSHOT_MODE,
                                                sizeof(value),
                                                &value);
            return rc;
        }
    }
    ALOGE("%s: Invalid Secene Mode: %s",
          __func__, (sceneModeStr == NULL) ? "NULL" : sceneModeStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setSelectableZoneAf
 *
 * DESCRIPTION: set selectable zone AF algorithm
 *
 * PARAMETERS :
 *   @selZoneAFStr : selectable zone AF algorithm value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setSelectableZoneAf(const char *selZoneAFStr)
{
    if (selZoneAFStr != NULL) {
        int32_t value = lookupAttr(FOCUS_ALGO_MAP,
                                   sizeof(FOCUS_ALGO_MAP)/sizeof(QCameraMap),
                                   selZoneAFStr);
        if (value != NAME_NOT_FOUND) {
            ALOGV("%s: Setting Selectable Zone AF value %s", __func__, selZoneAFStr);
            updateParamEntry(KEY_QC_SELECTABLE_ZONE_AF, selZoneAFStr);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_FOCUS_ALGO_TYPE,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("%s: Invalid selectable zone af value: %s",
          __func__, (selZoneAFStr == NULL) ? "NULL" : selZoneAFStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setAEBracket
 *
 * DESCRIPTION: set AE bracket value
 *
 * PARAMETERS :
 *   @aecBracketStr : AE bracket value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setAEBracket(const char *aecBracketStr)
{
    if (aecBracketStr == NULL) {
        ALOGI("%s: setAEBracket with NULL value", __func__);
        return NO_ERROR;
    }

    cam_exp_bracketing_t expBracket;
    memset(&expBracket, 0, sizeof(expBracket));

    int value = lookupAttr(BRACKETING_MODES_MAP,
                           sizeof(BRACKETING_MODES_MAP)/sizeof(QCameraMap),
                           aecBracketStr);
    switch (value) {
    case CAM_EXP_BRACKETING_ON:
        {
            ALOGV("%s, EXP_BRACKETING_ON", __func__);
            const char *str_val = get(KEY_QC_CAPTURE_BURST_EXPOSURE);
            if ((str_val != NULL) && (strlen(str_val)>0)) {
                expBracket.mode = CAM_EXP_BRACKETING_ON;
                strlcpy(expBracket.values, str_val, MAX_EXP_BRACKETING_LENGTH);
                ALOGI("%s: setting Exposure Bracketing value of %s",
                      __func__, expBracket.values);
            }
            else {
                /* Apps not set capture-burst-exposures, error case fall into bracketing off mode */
                ALOGI("%s: capture-burst-exposures not set, back to HDR OFF mode", __func__);
                expBracket.mode = CAM_EXP_BRACKETING_OFF;
            }
        }
        break;
    default:
        {
            ALOGD("%s, EXP_BRACKETING_OFF", __func__);
            expBracket.mode = CAM_EXP_BRACKETING_OFF;
        }
        break;
    }

    /* save the value*/
    updateParamEntry(KEY_QC_AE_BRACKET_HDR, aecBracketStr);
    return AddSetParmEntryToBatch(m_pParamBuf,
                                  CAM_INTF_PARM_HDR,
                                  sizeof(expBracket),
                                  &expBracket);
}

/*===========================================================================
 * FUNCTION   : setRedeyeReduction
 *
 * DESCRIPTION: set red eye reduction value
 *
 * PARAMETERS :
 *   @redeyeStr : red eye reduction value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setRedeyeReduction(const char *redeyeStr)
{
    if (redeyeStr != NULL) {
        int32_t value = lookupAttr(ENABLE_DISABLE_MODES_MAP,
                                   sizeof(ENABLE_DISABLE_MODES_MAP)/sizeof(QCameraMap),
                                   redeyeStr);
        if (value != NAME_NOT_FOUND) {
            ALOGV("%s: Setting RedEye Reduce value %s", __func__, redeyeStr);
            updateParamEntry(KEY_QC_REDEYE_REDUCTION, redeyeStr);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_REDEYE_REDUCTION,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("%s: Invalid RedEye Reduce value: %s",
          __func__, (redeyeStr == NULL) ? "NULL" : redeyeStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : getWaveletDenoiseProcessPlate
 *
 * DESCRIPTION: query wavelet denoise process plate
 *
 * PARAMETERS : None
 *
 * RETURN     : WNR prcocess plate vlaue
 *==========================================================================*/
cam_denoise_process_type_t QCameraParameters::getWaveletDenoiseProcessPlate()
{
    char prop[PROPERTY_VALUE_MAX];
    memset(prop, 0, sizeof(prop));
    property_get("persist.denoise.process.plates", prop, "0");
    int processPlate = atoi(prop);
    switch(processPlate) {
    case 0:
        return CAM_WAVELET_DENOISE_YCBCR_PLANE;
    case 1:
        return CAM_WAVELET_DENOISE_CBCR_ONLY;
    case 2:
        return CAM_WAVELET_DENOISE_STREAMLINE_YCBCR;
    case 3:
        return CAM_WAVELET_DENOISE_STREAMLINED_CBCR;
    default:
        return CAM_WAVELET_DENOISE_STREAMLINE_YCBCR;
    }
}

/*===========================================================================
 * FUNCTION   : setWaveletDenoise
 *
 * DESCRIPTION: set wavelet denoise value
 *
 * PARAMETERS :
 *   @wnrStr : wavelet denoise value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setWaveletDenoise(const char *wnrStr)
{
    if (wnrStr != NULL) {
        int value = lookupAttr(DENOISE_ON_OFF_MODES_MAP,
                               sizeof(DENOISE_ON_OFF_MODES_MAP)/sizeof(QCameraMap),
                               wnrStr);
        if (value != NAME_NOT_FOUND) {
            updateParamEntry(KEY_QC_DENOISE, wnrStr);

            cam_denoise_param_t temp;
            memset(&temp, 0, sizeof(temp));
            temp.denoise_enable = value;
            m_bWNROn = (value != 0);
            if (m_bWNROn) {
                temp.process_plates = getWaveletDenoiseProcessPlate();
            }
            ALOGI("%s: Denoise enable=%d, plates=%d",
                  __func__, temp.denoise_enable, temp.process_plates);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_WAVELET_DENOISE,
                                          sizeof(temp),
                                          &temp);
        }
    }
    ALOGE("%s: Invalid Denoise value: %s", __func__, (wnrStr == NULL) ? "NULL" : wnrStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setPreviewFrameRateMode
 *
 * DESCRIPTION: set preview frame rate mode
 *
 * PARAMETERS :
 *   @mode    : preview frame rate mode
 *
 * RETURN     : none
 *==========================================================================*/
void QCameraParameters::setPreviewFrameRateMode(const char *mode)
{
    set(KEY_QC_PREVIEW_FRAME_RATE_MODE, mode);
}

/*===========================================================================
 * FUNCTION   : getPreviewFrameRateMode
 *
 * DESCRIPTION: get preview frame rate mode
 *
 * PARAMETERS : none
 *
 * RETURN     : preview frame rate mode string
 *==========================================================================*/
const char *QCameraParameters::getPreviewFrameRateMode() const
{
    return get(KEY_QC_PREVIEW_FRAME_RATE_MODE);
}

/*===========================================================================
 * FUNCTION   : setTouchIndexAec
 *
 * DESCRIPTION: set touch index AEC
 *
 * PARAMETERS :
 *   @x,y     :
 *
 * RETURN     : none
 *==========================================================================*/
void QCameraParameters::setTouchIndexAec(int x, int y)
{
    char str[32];
    snprintf(str, sizeof(str), "%dx%d", x, y);
    set(KEY_QC_TOUCH_INDEX_AEC, str);
}

/*===========================================================================
 * FUNCTION   : getTouchIndexAec
 *
 * DESCRIPTION: get touch index AEC
 *
 * PARAMETERS :
 *   @x,y     :
 *
 * RETURN     : none
 *==========================================================================*/
void QCameraParameters::getTouchIndexAec(int *x, int *y)
{
    *x = -1;
    *y = -1;

    // Get the current string, if it doesn't exist, leave the -1x-1
    const char *p = get(KEY_QC_TOUCH_INDEX_AEC);
    if (p == 0)
        return;

    int tempX, tempY;
    if (parse_pair(p, &tempX, &tempY, 'x') == 0) {
        *x = tempX;
        *y = tempY;
    }
}

/*===========================================================================
 * FUNCTION   : setTouchIndexAf
 *
 * DESCRIPTION: set touch index AF
 *
 * PARAMETERS :
 *   @x,y     :
 *
 * RETURN     : none
 *==========================================================================*/
void QCameraParameters::setTouchIndexAf(int x, int y)
{
    char str[32];
    snprintf(str, sizeof(str), "%dx%d", x, y);
    set(KEY_QC_TOUCH_INDEX_AF, str);
}

/*===========================================================================
 * FUNCTION   : getTouchIndexAf
 *
 * DESCRIPTION: get touch index AF
 *
 * PARAMETERS :
 *   @x,y     :
 *
 * RETURN     : none
 *==========================================================================*/
void QCameraParameters::getTouchIndexAf(int *x, int *y)
{
    *x = -1;
    *y = -1;

    // Get the current string, if it doesn't exist, leave the -1x-1
    const char *p = get(KEY_QC_TOUCH_INDEX_AF);
    if (p == 0)
        return;

    int tempX, tempY;
    if (parse_pair(p, &tempX, &tempY, 'x') == 0) {
        *x = tempX;
        *y = tempY;
	}
}

/*===========================================================================
 * FUNCTION   : getStreamFormat
 *
 * DESCRIPTION: get stream format by its type
 *
 * PARAMETERS :
 *   @streamType : [input] stream type
 *   @format     : [output] stream format
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::getStreamFormat(cam_stream_type_t streamType,
                                            cam_format_t &format)
{
    int32_t ret = NO_ERROR;

    format = CAM_FORMAT_MAX;
    switch (streamType) {
    case CAM_STREAM_TYPE_PREVIEW:
    case CAM_STREAM_TYPE_POSTVIEW:
        format = mPreviewFormat;
        break;
    case CAM_STREAM_TYPE_SNAPSHOT:
        if ( mPictureFormat == CAM_FORMAT_YUV_422_NV16 ) {
            format = CAM_FORMAT_YUV_422_NV16;
        } else {
            char prop[PROPERTY_VALUE_MAX];
            int snapshotFormat;
            memset(prop, 0, sizeof(prop));
            property_get("persist.camera.snap.format", prop, "0");
            snapshotFormat = atoi(prop);
            if(snapshotFormat == 1) {
                format = CAM_FORMAT_YUV_422_NV61;
            } else {
                format = CAM_FORMAT_YUV_420_NV21;
            }
        }
        break;
    case CAM_STREAM_TYPE_VIDEO:
        format = CAM_FORMAT_YUV_420_NV12;
        break;
    case CAM_STREAM_TYPE_RAW:
        if (mPictureFormat >= CAM_FORMAT_YUV_RAW_8BIT_YUYV) {
            format = (cam_format_t)mPictureFormat;
        } else {
            ALOGE("%s: invalid raw picture format: %d", __func__, mPictureFormat);
            ret = BAD_VALUE;
        }
        break;
    case CAM_STREAM_TYPE_METADATA:
    case CAM_STREAM_TYPE_OFFLINE_PROC:
    case CAM_STREAM_TYPE_DEFAULT:
    default:
        break;
    }

    return ret;
}

/*===========================================================================
 * FUNCTION   : getFlipMode
 *
 * DESCRIPTION: get flip mode
 *
 * PARAMETERS :
 *   @cam_intf_parm_type_t : [input] stream type
 *
 * RETURN     : int type of flip mode
 *              0 - no filp
 *              1 - FLIP_H
 *              2 - FLIP_V
 *              3 - FLIP_H | FLIP_V
 *==========================================================================*/
int QCameraParameters::getFlipMode(cam_stream_type_t type)
{
    const char *str = NULL;
    int flipMode = 0; // no flip

    switch(type){
    case CAM_STREAM_TYPE_PREVIEW:
        str = get(KEY_QC_PREVIEW_FLIP);
        break;
    case CAM_STREAM_TYPE_VIDEO:
        str = get(KEY_QC_VIDEO_FLIP);
        break;
    case CAM_STREAM_TYPE_SNAPSHOT:
    case CAM_STREAM_TYPE_POSTVIEW:
        str = get(KEY_QC_SNAPSHOT_PICTURE_FLIP);
        break;
    default:
        ALOGI("%s: No flip mode for stream type %d", __func__, type);
        break;
    }

    if(str != NULL){
        //Need give corresponding filp value based on flip mode strings
        int value = lookupAttr(FLIP_MODES_MAP,
                sizeof(FLIP_MODES_MAP)/sizeof(QCameraMap), str);
        if(value != NAME_NOT_FOUND)
            flipMode = value;
        }

    ALOGD("%s: the filp mode of stream type %d is %d .", __func__, type, flipMode);
    return flipMode;
}

/*===========================================================================
 * FUNCTION   : getStreamDimension
 *
 * DESCRIPTION: get stream dimension by its type
 *
 * PARAMETERS :
 *   @streamType : [input] stream type
 *   @dim        : [output] stream dimension
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::getStreamDimension(cam_stream_type_t streamType,
                                               cam_dimension_t &dim)
{
    int32_t ret = NO_ERROR;
    memset(&dim, 0, sizeof(cam_dimension_t));

    switch (streamType) {
    case CAM_STREAM_TYPE_PREVIEW:
        getPreviewSize(&dim.width, &dim.height);
        break;
    case CAM_STREAM_TYPE_POSTVIEW:
        getPreviewSize(&dim.width, &dim.height);
        break;
    case CAM_STREAM_TYPE_SNAPSHOT:
        if (getRecordingHintValue() == true) {
            // live snapshot
            getLiveSnapshotSize(dim);
        } else {
            getPictureSize(&dim.width, &dim.height);
        }
        break;
    case CAM_STREAM_TYPE_VIDEO:
        getVideoSize(&dim.width, &dim.height);
        break;
    case CAM_STREAM_TYPE_RAW:
        dim = m_pCapability->raw_dim;
        break;
    case CAM_STREAM_TYPE_METADATA:
        dim.width = sizeof(cam_metadata_info_t);
        dim.height = 1;
        break;
    case CAM_STREAM_TYPE_OFFLINE_PROC:
        break;
    case CAM_STREAM_TYPE_DEFAULT:
    default:
        ALOGE("%s: no dimension for unsupported stream type %d",
              __func__, streamType);
        ret = BAD_VALUE;
        break;
    }
    return ret;
}

/*===========================================================================
 * FUNCTION   : getPreviewHalPixelFormat
 *
 * DESCRIPTION: get preview HAL pixel format
 *
 * PARAMETERS : none
 *
 * RETURN     : HAL pixel format
 *==========================================================================*/
int QCameraParameters::getPreviewHalPixelFormat() const
{
    int32_t halPixelFormat;

    switch (mPreviewFormat) {
    case CAM_FORMAT_YUV_420_NV12:
        halPixelFormat = HAL_PIXEL_FORMAT_YCbCr_420_SP;
        break;
    case CAM_FORMAT_YUV_420_NV21:
        halPixelFormat = HAL_PIXEL_FORMAT_YCrCb_420_SP;
        break;
    case CAM_FORMAT_YUV_420_NV21_ADRENO:
        halPixelFormat = HAL_PIXEL_FORMAT_YCrCb_420_SP_ADRENO;
        break;
    case CAM_FORMAT_YUV_420_YV12:
        halPixelFormat = HAL_PIXEL_FORMAT_YV12;
        break;
    case CAM_FORMAT_YUV_420_NV12_VENUS:
        halPixelFormat = HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS;
        break;
    case CAM_FORMAT_YUV_422_NV16:
    case CAM_FORMAT_YUV_422_NV61:
    default:
        halPixelFormat = HAL_PIXEL_FORMAT_YCrCb_420_SP;
        break;
    }
    ALOGE("%s: format %d\n", __func__, halPixelFormat);
    return halPixelFormat;
}

/*===========================================================================
 * FUNCTION   : getthumbnailSize
 *
 * DESCRIPTION: get thumbnail size
 *
 * PARAMETERS :
 *   @width, height : [output] thumbnail width and height
 *
 * RETURN     : none
 *==========================================================================*/
void QCameraParameters::getThumbnailSize(int *width, int *height) const
{
    *width = getInt(KEY_JPEG_THUMBNAIL_WIDTH);
    *height = getInt(KEY_JPEG_THUMBNAIL_HEIGHT);
}

/*===========================================================================
 * FUNCTION   : getZSLBurstInterval
 *
 * DESCRIPTION: get ZSL burst interval setting
 *
 * PARAMETERS : none
 *
 * RETURN     : ZSL burst interval value
 *==========================================================================*/
int QCameraParameters::getZSLBurstInterval()
{
    int interval = getInt(KEY_QC_ZSL_BURST_INTERVAL);
    if (interval < 0) {
        interval = 1;
    }
    return interval;
}

/*===========================================================================
 * FUNCTION   : getZSLQueueDepth
 *
 * DESCRIPTION: get ZSL queue depth
 *
 * PARAMETERS : none
 *
 * RETURN     : ZSL queue depth value
 *==========================================================================*/
int QCameraParameters::getZSLQueueDepth()
{
    int qdepth = getInt(KEY_QC_ZSL_QUEUE_DEPTH);
    if (qdepth < 0) {
        qdepth = 2;
    }
    return qdepth;
}

/*===========================================================================
 * FUNCTION   : getZSLBackLookCount
 *
 * DESCRIPTION: get ZSL backlook count setting
 *
 * PARAMETERS : none
 *
 * RETURN     : ZSL backlook count value
 *==========================================================================*/
int QCameraParameters::getZSLBackLookCount()
{
    int look_back = getInt(KEY_QC_ZSL_BURST_LOOKBACK);
    if (look_back < 0) {
        look_back = 2;
    }
    return look_back;
}

/*===========================================================================
 * FUNCTION   : getZSLMaxUnmatchedFrames
 *
 * DESCRIPTION: get allowed ZSL max unmatched frames number
 *
 * PARAMETERS : none
 *
 * RETURN     : ZSL backlook count value
 *==========================================================================*/
int QCameraParameters::getMaxUnmatchedFramesInQueue()
{
    return m_pCapability->min_num_pp_bufs;
}

/*===========================================================================
 * FUNCTION   : setRecordingHintValue
 *
 * DESCRIPTION: set recording hint
 *
 * PARAMETERS :
 *   @value   : video hint value
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraParameters::setRecordingHintValue(int32_t value)
{
    ALOGD("%s: VideoHint = %d", __func__, value);
    bool newValue = (value > 0)? true : false;

    if ( m_bRecordingHint != newValue ) {
        m_bNeedRestart = true;
        m_bRecordingHint_new = newValue;
    } else {
        m_bRecordingHint_new = m_bRecordingHint;
    }
    return AddSetParmEntryToBatch(m_pParamBuf,
                                  CAM_INTF_PARM_RECORDING_HINT,
                                  sizeof(value),
                                  &value);
}

/*===========================================================================
 * FUNCTION   : getNumOfSnapshots
 *
 * DESCRIPTION: get number of snapshot per shutter
 *
 * PARAMETERS : none
 *
 * RETURN     : number of snapshot per shutter
 *==========================================================================*/
uint8_t QCameraParameters::getNumOfSnapshots()
{
    int numOfSnapshot = getInt(KEY_QC_NUM_SNAPSHOT_PER_SHUTTER);
    if (numOfSnapshot <= 0) {
        numOfSnapshot = 1; // set to default value
    }
    return (uint8_t)numOfSnapshot;
}

/*===========================================================================
 * FUNCTION   : getNumOfExtraHDRBufsIfNeeded
 *
 * DESCRIPTION: get number of extra buffers needed by HDR if HDR is enabled
 *
 * PARAMETERS : none
 *
 * RETURN     : number of extra buffer needed by HDR; 0 if not HDR enabled
 *==========================================================================*/
uint8_t QCameraParameters::getNumOfExtraHDRBufsIfNeeded()
{
    uint8_t numOfBufs = 0;
    const char *scene_mode = get(KEY_SCENE_MODE);
    if (scene_mode != NULL && strcmp(scene_mode, SCENE_MODE_HDR) == 0) {
        // HDR mode
        numOfBufs = getBurstNum() * m_pCapability->min_num_hdr_bufs;
    }
    return numOfBufs;
}

/*===========================================================================
 * FUNCTION   : getNumOfHDRBufsIfNeeded
 *
 * DESCRIPTION: get number of buffers needed by HDR if HDR is enabled
 *
 * PARAMETERS : none
 *
 * RETURN     : number of buffer needed by HDR; 0 if not HDR enabled
 *==========================================================================*/
uint8_t QCameraParameters::getNumOfHDRBufsIfNeeded()
{
    uint8_t numOfBufs = 0;
    const char *scene_mode = get(KEY_SCENE_MODE);
    if (scene_mode != NULL && strcmp(scene_mode, SCENE_MODE_HDR) == 0) {
        // HDR mode
        const char *need_hdr_1x = get(KEY_QC_HDR_NEED_1X);
        if (need_hdr_1x != NULL && strcmp(need_hdr_1x, VALUE_TRUE) == 0) {
            numOfBufs = 2; // HDR needs both 1X and processed img
        } else {
            numOfBufs = 1; // HDR only needs processed img
        }

        numOfBufs += m_pCapability->min_num_hdr_bufs;
    }
    return numOfBufs;
}

/*===========================================================================
 * FUNCTION   : getBurstNum
 *
 * DESCRIPTION: get burst number of snapshot
 *
 * PARAMETERS : none
 *
 * RETURN     : number of burst
 *==========================================================================*/
int QCameraParameters::getBurstNum()
{
    return m_nBurstNum;
}

/*===========================================================================
 * FUNCTION   : getJpegQuality
 *
 * DESCRIPTION: get jpeg encoding quality
 *
 * PARAMETERS : none
 *
 * RETURN     : jpeg encoding quality
 *==========================================================================*/
int QCameraParameters::getJpegQuality()
{
    int quality = getInt(KEY_JPEG_QUALITY);
    if (quality < 0) {
        quality = 85; // set to default quality value
    }
    return quality;
}

/*===========================================================================
 * FUNCTION   : getJpegRotation
 *
 * DESCRIPTION: get rotation value
 *
 * PARAMETERS : none
 *
 * RETURN     : rotation value
 *==========================================================================*/
int QCameraParameters::getJpegRotation() {
    int rotation = getInt(KEY_ROTATION);
    if (rotation < 0) {
        rotation = 0;
    }
    return rotation;
}

/*===========================================================================
 * FUNCTION   : parseGPSCoordinate
 *
 * DESCRIPTION: parse GPS coordinate string
 *
 * PARAMETERS :
 *   @coord_str : [input] coordinate string
 *   @coord     : [output]  ptr to struct to store coordinate
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraParameters::parseGPSCoordinate(const char *coord_str, rat_t* coord)
{
    if(coord == NULL) {
        ALOGE("%s: error, invalid argument coord == NULL", __func__);
        return BAD_VALUE;
    }
    float degF = atof(coord_str);
    if (degF < 0) {
        degF = -degF;
    }
    float minF = (degF - (int) degF) * 60;
    float secF = (minF - (int) minF) * 60;

    getRational(&coord[0], (int)degF, 1);
    getRational(&coord[1], (int)minF, 1);
    getRational(&coord[2], (int)(secF * 10000), 10000);
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : getExifDateTime
 *
 * DESCRIPTION: query exif date time
 *
 * PARAMETERS :
 *   @dateTime : string to store exif date time
 *   @count    : lenght of the dateTime string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::getExifDateTime(char *dateTime, uint32_t &count)
{
    //get time and date from system
    time_t rawtime;
    struct tm * timeinfo = NULL;
    memset(&rawtime, 0, sizeof(rawtime));
    time(&rawtime);
    timeinfo = localtime (&rawtime);
    if (timeinfo != NULL && count >= 20) {
        //Write datetime according to EXIF Spec
        //"YYYY:MM:DD HH:MM:SS" (20 chars including \0)
        snprintf(dateTime, 20, "%04d:%02d:%02d %02d:%02d:%02d",
                 timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,
                 timeinfo->tm_mday, timeinfo->tm_hour,
                 timeinfo->tm_min, timeinfo->tm_sec);
        count = 20;
        return NO_ERROR;
    }
    return UNKNOWN_ERROR;
}

/*===========================================================================
 * FUNCTION   : getRational
 *
 * DESCRIPTION: compose rational struct
 *
 * PARAMETERS :
 *   @rat     : ptr to struct to store rational info
 *   @num     :num of the rational
 *   @denom   : denom of the rational
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::getRational(rat_t *rat, int num, int denom)
{
    if (NULL == rat) {
        ALOGE("%s: NULL rat input", __func__);
        return BAD_VALUE;
    }
    rat->num = num;
    rat->denom = denom;
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : getExifFocalLength
 *
 * DESCRIPTION: get exif focal lenght
 *
 * PARAMETERS :
 *   @focalLength : ptr to rational strcut to store focal lenght
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::getExifFocalLength(rat_t *focalLength)
{
    int focalLengthValue =
        (int)(getFloat(QCameraParameters::KEY_FOCAL_LENGTH) * FOCAL_LENGTH_DECIMAL_PRECISION);
    return getRational(focalLength, focalLengthValue, FOCAL_LENGTH_DECIMAL_PRECISION);
}

/*===========================================================================
 * FUNCTION   : getExifIsoSpeed
 *
 * DESCRIPTION: get exif ISO speed
 *
 * PARAMETERS : none
 *
 * RETURN     : ISO speed value
 *==========================================================================*/
uint16_t QCameraParameters::getExifIsoSpeed()
{
    uint16_t isoSpeed = 0;
    const char *iso_str = get(QCameraParameters::KEY_QC_ISO_MODE);
    int iso_index = lookupAttr(ISO_MODES_MAP,
        sizeof(ISO_MODES_MAP)/sizeof(ISO_MODES_MAP[0]), iso_str);
    switch (iso_index) {
    case CAM_ISO_MODE_AUTO:
        isoSpeed = 0;
        break;
    case CAM_ISO_MODE_DEBLUR:
        isoSpeed = 1;
        break;
    case CAM_ISO_MODE_100:
        isoSpeed = 100;
        break;
    case CAM_ISO_MODE_200:
        isoSpeed = 200;
        break;
    case CAM_ISO_MODE_400:
        isoSpeed = 400;
        break;
    case CAM_ISO_MODE_800:
        isoSpeed = 800;
        break;
    case CAM_ISO_MODE_1600:
        isoSpeed = 1600;
        break;
    }
    return isoSpeed;
}

/*===========================================================================
 * FUNCTION   : getExifGpsProcessingMethod
 *
 * DESCRIPTION: get GPS processing method
 *
 * PARAMETERS :
 *   @gpsProcessingMethod : string to store GPS process method
 *   @count               : lenght of the string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::getExifGpsProcessingMethod(char *gpsProcessingMethod,
                                                      uint32_t &count)
{
    const char *str = get(KEY_GPS_PROCESSING_METHOD);
    if(str != NULL) {
        memcpy(gpsProcessingMethod, ExifAsciiPrefix, EXIF_ASCII_PREFIX_SIZE);
        count = EXIF_ASCII_PREFIX_SIZE;
        strncpy(gpsProcessingMethod + EXIF_ASCII_PREFIX_SIZE, str, strlen(str));
        count += strlen(str);
        gpsProcessingMethod[count++] = '\0'; // increase 1 for the last NULL char
        return NO_ERROR;
    } else {
        return BAD_VALUE;
    }
}

/*===========================================================================
 * FUNCTION   : getExifLatitude
 *
 * DESCRIPTION: get exif latitude
 *
 * PARAMETERS :
 *   @latitude : ptr to rational struct to store latitude info
 *   @ladRef   : charater to indicate latitude reference
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::getExifLatitude(rat_t *latitude,
                                           char *latRef)
{
    const char *str = get(KEY_GPS_LATITUDE);
    if(str != NULL) {
        parseGPSCoordinate(str, latitude);

        //set Latitude Ref
        float latitudeValue = getFloat(KEY_GPS_LATITUDE);
        if(latitudeValue < 0.0f) {
            latRef[0] = 'S';
        } else {
            latRef[0] = 'N';
        }
        latRef[1] = '\0';
        return NO_ERROR;
    }else{
        return BAD_VALUE;
    }
}

/*===========================================================================
 * FUNCTION   : getExifLongitude
 *
 * DESCRIPTION: get exif longitude
 *
 * PARAMETERS :
 *   @longitude : ptr to rational struct to store longitude info
 *   @lonRef    : charater to indicate longitude reference
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::getExifLongitude(rat_t *longitude,
                                            char *lonRef)
{
    const char *str = get(KEY_GPS_LONGITUDE);
    if(str != NULL) {
        parseGPSCoordinate(str, longitude);

        //set Longitude Ref
        float longitudeValue = getFloat(KEY_GPS_LONGITUDE);
        if(longitudeValue < 0.0f) {
            lonRef[0] = 'W';
        } else {
            lonRef[0] = 'E';
        }
        lonRef[1] = '\0';
        return NO_ERROR;
    }else{
        return BAD_VALUE;
    }
}

/*===========================================================================
 * FUNCTION   : getExifAltitude
 *
 * DESCRIPTION: get exif altitude
 *
 * PARAMETERS :
 *   @altitude : ptr to rational struct to store altitude info
 *   @altRef   : charater to indicate altitude reference
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::getExifAltitude(rat_t *altitude,
                                           char *altRef)
{
    const char *str = get(KEY_GPS_ALTITUDE);
    if(str != NULL) {
        double value = atof(str);
        *altRef = 0;
        if(value < 0){
            *altRef = 1;
            value = -value;
        }
        return getRational(altitude, value*1000, 1000);
    }else{
        return BAD_VALUE;
    }
}

/*===========================================================================
 * FUNCTION   : getExifGpsDateTimeStamp
 *
 * DESCRIPTION: get exif GPS date time stamp
 *
 * PARAMETERS :
 *   @gpsDateStamp : GPS date time stamp string
 *   @bufLen       : length of the string
 *   @gpsTimeStamp : ptr to rational struct to store time stamp info
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::getExifGpsDateTimeStamp(char *gpsDateStamp,
                                                   uint32_t bufLen,
                                                   rat_t *gpsTimeStamp)
{
    const char *str = get(KEY_GPS_TIMESTAMP);
    if(str != NULL) {
        time_t unixTime = (time_t)atol(str);
        struct tm *UTCTimestamp = gmtime(&unixTime);

        strftime(gpsDateStamp, bufLen, "%Y:%m:%d", UTCTimestamp);

        getRational(&gpsTimeStamp[0], UTCTimestamp->tm_hour, 1);
        getRational(&gpsTimeStamp[1], UTCTimestamp->tm_min, 1);
        getRational(&gpsTimeStamp[2], UTCTimestamp->tm_sec, 1);

        return NO_ERROR;
    } else {
        return BAD_VALUE;
    }
}

/*===========================================================================
 * FUNCTION   : updateFocusDistances
 *
 * DESCRIPTION: update focus distances
 *
 * PARAMETERS :
 *   @focusDistances : ptr to focus distance info
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::updateFocusDistances(cam_focus_distances_info_t *focusDistances)
{
    String8 str;
    char buffer[32] = {0};
    //set all distances to infinity if focus mode is infinity
    if(mFocusMode == CAM_FOCUS_MODE_INFINITY) {
        str.append("Infinity,Infinity,Infinity");
    } else {
        snprintf(buffer, sizeof(buffer), "%f", focusDistances->focus_distance[0]);
        str.append(buffer);
        snprintf(buffer, sizeof(buffer), ",%f", focusDistances->focus_distance[1]);
        str.append(buffer);
        snprintf(buffer, sizeof(buffer), ",%f", focusDistances->focus_distance[2]);
        str.append(buffer);
    }
    ALOGD("%s: setting KEY_FOCUS_DISTANCES as %s", __FUNCTION__, str.string());
    set(QCameraParameters::KEY_FOCUS_DISTANCES, str.string());
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : updateRecordingHintValue
 *
 * DESCRIPTION: update recording hint locally and to daemon
 *
 * PARAMETERS :
 *   @value   : video hint value
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::updateRecordingHintValue(int32_t value)
{
    int32_t rc = NO_ERROR;
    if(initBatchUpdate(m_pParamBuf) < 0 ) {
        ALOGE("%s:Failed to initialize group update table", __func__);
        return BAD_TYPE;
    }

    rc = setRecordingHintValue(value);
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to update table", __func__);
        return rc;
    }

    rc = commitSetBatch();
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to update recording hint", __func__);
        return rc;
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : setHistogram
 *
 * DESCRIPTION: set histogram
 *
 * PARAMETERS :
 *   @enabled : if histogram is enabled
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setHistogram(bool enabled)
{
    if(m_bHistogramEnabled == enabled) {
        ALOGD("%s: histogram flag not changed, no ops here", __func__);
        return NO_ERROR;
    }

    // set parm for histogram
    if(initBatchUpdate(m_pParamBuf) < 0 ) {
        ALOGE("%s:Failed to initialize group update table", __func__);
        return BAD_TYPE;
    }

    int32_t value = enabled;
    int32_t rc = NO_ERROR;
    rc = AddSetParmEntryToBatch(m_pParamBuf,
                                CAM_INTF_PARM_HISTOGRAM,
                                sizeof(value),
                                &value);
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to update table", __func__);
        return rc;
    }

    rc = commitSetBatch();
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to set histogram", __func__);
        return rc;
    }

    m_bHistogramEnabled = enabled;

    ALOGD(" Histogram -> %s", m_bHistogramEnabled ? "Enabled" : "Disabled");

    return rc;
}

/*===========================================================================
 * FUNCTION   : setFaceDetection
 *
 * DESCRIPTION: set face detection
 *
 * PARAMETERS :
 *   @enabled : if face detection is enabled
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setFaceDetection(bool enabled)
{
    int faceProcMask = m_nFaceProcMask;
    // set face detection mask
    if (enabled) {
        faceProcMask |= CAM_FACE_PROCESS_MASK_DETECTION;
    } else {
        faceProcMask &= ~CAM_FACE_PROCESS_MASK_DETECTION;
    }

    if(m_nFaceProcMask == faceProcMask) {
        ALOGD("%s: face process mask not changed, no ops here", __func__);
        return NO_ERROR;
    }

    // set parm for face detection
    int requested_faces = getInt(KEY_QC_MAX_NUM_REQUESTED_FACES);
    cam_fd_set_parm_t fd_set_parm;
    memset(&fd_set_parm, 0, sizeof(cam_fd_set_parm_t));
    fd_set_parm.fd_mode = faceProcMask;
    fd_set_parm.num_fd = requested_faces;

    ALOGD("[KPI Perf] %s: Face detection value = %d num_fd = %d",
          __func__, faceProcMask,requested_faces);
    if(initBatchUpdate(m_pParamBuf) < 0 ) {
        ALOGE("%s:Failed to initialize group update table", __func__);
        return BAD_TYPE;
    }
    int32_t rc = NO_ERROR;

    rc = AddSetParmEntryToBatch(m_pParamBuf,
                                CAM_INTF_PARM_FD,
                                sizeof(fd_set_parm),
                                &fd_set_parm);
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to update table", __func__);
        return rc;
    }

    rc = commitSetBatch();
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to set face detection parm", __func__);
        return rc;
    }

    m_nFaceProcMask = faceProcMask;
    ALOGD("%s: FaceProcMask -> %d", __func__, m_nFaceProcMask);

    return rc;
}

/*===========================================================================
 * FUNCTION   : setLockCAF
 *
 * DESCRIPTION: Lock CAF
 *
 * PARAMETERS :
 *   @bLock : if CAF needs to be locked
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setLockCAF(bool bLock)
{
    if(initBatchUpdate(m_pParamBuf) < 0 ) {
        ALOGE("%s:Failed to initialize group update table", __func__);
        return BAD_TYPE;
    }
    int32_t rc = NO_ERROR;
    int32_t value = bLock;

    rc = AddSetParmEntryToBatch(m_pParamBuf,
                                CAM_INTF_PARM_LOCK_CAF,
                                sizeof(value),
                                &value);
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to update table", __func__);
        return rc;
    }

    rc = commitSetBatch();
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to set lock CAF parm", __func__);
        return rc;
    } else {
        m_bCAFLocked = bLock;
        return NO_ERROR;
    }
}

/*===========================================================================
 * FUNCTION   : setFrameSkip
 *
 * DESCRIPTION: send ISP frame skip pattern to camera daemon
 *
 * PARAMETERS :
 *   @pattern : skip pattern for ISP
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setFrameSkip(enum msm_vfe_frame_skip_pattern pattern)
{
    int32_t rc = NO_ERROR;
    int32_t value = (int32_t)pattern;

    if ( m_pParamBuf == NULL ) {
        return NO_INIT;
    }

    if(initBatchUpdate(m_pParamBuf) < 0 ) {
        ALOGE("%s:Failed to initialize group update table", __func__);
        return BAD_TYPE;
    }

    rc = AddSetParmEntryToBatch(m_pParamBuf,
                                CAM_INTF_PARM_FRAMESKIP,
                                sizeof(value),
                                &value);
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to update table", __func__);
        return rc;
    }

    rc = commitSetBatch();
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to set frameskip info parm", __func__);
        return rc;
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : parseNDimVector
 *
 * DESCRIPTION: helper function to parse a string like "(1, 2, 3, 4, ..., N)"
 *              into N-dimension vector
 *
 * PARAMETERS :
 *   @str     : string to be parsed
 *   @num     : output array of size N to store vector element values
 *   @N       : number of dimension
 *   @delim   : delimeter to seperete string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::parseNDimVector(const char *str, int *num, int N, char delim = ',')
{
    char *start, *end;
    if (num == NULL) {
        ALOGE("%s: Invalid output array (num == NULL)", __func__);
        return BAD_VALUE;
    }

    //check if string starts and ends with parantheses
    if(str[0] != '(' || str[strlen(str)-1] != ')') {
        ALOGE("%s: Invalid format of string %s, valid format is (n1, n2, n3, n4 ...)",
              __func__, str);
        return BAD_VALUE;
    }
    start = (char*) str;
    start++;
    for(int i=0; i<N; i++) {
        *(num+i) = (int) strtol(start, &end, 10);
        if(*end != delim && i < N-1) {
            ALOGE("%s: Cannot find delimeter '%c' in string \"%s\". end = %c",
                  __func__, delim, str, *end);
            return -1;
        }
        start = end+1;
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : parseCameraAreaString
 *
 * DESCRIPTION: helper function to parse a string of camera areas like
 *              "(1, 2, 3, 4, 5),(1, 2, 3, 4, 5),..."
 *
 * PARAMETERS :
 *   @str             : string to be parsed
 *   @max_num_areas   : max number of areas
 *   @pAreas          : ptr to struct to store areas
 *   @num_areas_found : number of areas found
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::parseCameraAreaString(const char *str,
                                                 int max_num_areas,
                                                 cam_area_t *pAreas,
                                                 int& num_areas_found)
{
    char area_str[32];
    const char *start, *end, *p;
    start = str; end = NULL;
    int values[5], index=0;
    num_areas_found = 0;

    memset(values, 0, sizeof(values));
    while(start != NULL) {
       if(*start != '(') {
            ALOGE("%s: error: Ill formatted area string: %s", __func__, str);
            return BAD_VALUE;
       }
       end = strchr(start, ')');
       if(end == NULL) {
            ALOGE("%s: error: Ill formatted area string: %s", __func__, str);
            return BAD_VALUE;
       }
       int i;
       for (i=0,p=start; p<=end; p++, i++) {
           area_str[i] = *p;
       }
       area_str[i] = '\0';
       if(parseNDimVector(area_str, values, 5) < 0){
            ALOGE("%s: error: Failed to parse the area string: %s", __func__, area_str);
            return BAD_VALUE;
       }
       // no more areas than max_num_areas are accepted.
       if(index >= max_num_areas) {
            ALOGE("%s: error: too many areas specified %s", __func__, str);
            return BAD_VALUE;
       }
       pAreas[index].rect.left = values[0];
       pAreas[index].rect.top = values[1];
       pAreas[index].rect.width = values[2] - values[0];
       pAreas[index].rect.height = values[3] - values[1];
       pAreas[index].weight = values[4];

       index++;
       start = strchr(end, '('); // serach for next '('
    }
    num_areas_found = index;
    return 0;
}

/*===========================================================================
 * FUNCTION   : validateCameraAreas
 *
 * DESCRIPTION: helper function to validate camera areas within (-1000, 1000)
 *
 * PARAMETERS :
 *   @areas     : ptr to array of areas
 *   @num_areas : number of areas
 *
 * RETURN     : true --  area is in valid range
 *              false -- not valid
 *==========================================================================*/
bool QCameraParameters::validateCameraAreas(cam_area_t *areas, int num_areas)
{
    // special case: default area
    if (num_areas == 1 &&
        areas[0].rect.left == 0 &&
        areas[0].rect.top == 0 &&
        areas[0].rect.width == 0 &&
        areas[0].rect.height == 0 &&
        areas[0].weight == 0) {
        return true;
    }

    for(int i = 0; i < num_areas; i++) {
        // left should be >= -1000
        if(areas[i].rect.left < -1000) {
            return false;
        }

        // top  should be >= -1000
        if(areas[i].rect.top < -1000) {
            return false;
        }

        // width or height should be > 0
        if (areas[i].rect.width <= 0 || areas[i].rect.height <= 0) {
            return false;
        }

        // right  should be <= 1000
        if(areas[i].rect.left + areas[i].rect.width > 1000) {
            return false;
        }

        // bottom should be <= 1000
        if(areas[i].rect.top + areas[i].rect.height > 1000) {
            return false;
        }

        // weight should be within (1, 1000)
        if (areas[i].weight < 1 || areas[i].weight > 1000) {
            return false;
        }
    }
    return true;
}

/*===========================================================================
 * FUNCTION   : initBatchUpdate
 *
 * DESCRIPTION: init camera parameters buf entries
 *
 * PARAMETERS :
 *   @p_table : ptr to parameter buffer
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::initBatchUpdate(parm_buffer_t *p_table)
{
    m_tempMap.clear();

    memset(p_table, 0, sizeof(parm_buffer_t));
    p_table->first_flagged_entry = CAM_INTF_PARM_MAX;
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : AddSetParmEntryToBatch
 *
 * DESCRIPTION: add set parameter entry into batch
 *
 * PARAMETERS :
 *   @p_table     : ptr to parameter buffer
 *   @paramType   : parameter type
 *   @paramLength : length of parameter value
 *   @paramValue  : ptr to parameter value
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::AddSetParmEntryToBatch(parm_buffer_t *p_table,
                                                  cam_intf_parm_type_t paramType,
                                                  uint32_t paramLength,
                                                  void *paramValue)
{
    int position = paramType;
    int current, next;

    /*************************************************************************
    *                 Code to take care of linking next flags                *
    *************************************************************************/
    current = GET_FIRST_PARAM_ID(p_table);
    if (position == current){
        //DO NOTHING
    } else if (position < current){
        SET_NEXT_PARAM_ID(position, p_table, current);
        SET_FIRST_PARAM_ID(p_table, position);
    } else {
        /* Search for the position in the linked list where we need to slot in*/
        while (position > GET_NEXT_PARAM_ID(current, p_table))
            current = GET_NEXT_PARAM_ID(current, p_table);

        /*If node already exists no need to alter linking*/
        if (position != GET_NEXT_PARAM_ID(current, p_table)) {
            next = GET_NEXT_PARAM_ID(current, p_table);
            SET_NEXT_PARAM_ID(current, p_table, position);
            SET_NEXT_PARAM_ID(position, p_table, next);
        }
    }

    /*************************************************************************
    *                   Copy contents into entry                             *
    *************************************************************************/

    if (paramLength > sizeof(parm_type_t)) {
        ALOGE("%s:Size of input larger than max entry size",__func__);
        return BAD_VALUE;
    }
    memcpy(POINTER_OF(paramType,p_table), paramValue, paramLength);
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : AddGetParmEntryToBatch
 *
 * DESCRIPTION: add get parameter entry into batch
 *
 * PARAMETERS :
 *   @p_table     : ptr to parameter buffer
 *   @paramType   : parameter type
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::AddGetParmEntryToBatch(parm_buffer_t *p_table,
                                                  cam_intf_parm_type_t paramType)
{
    int position = paramType;
    int current, next;

    /*************************************************************************
    *                 Code to take care of linking next flags                *
    *************************************************************************/
    current = GET_FIRST_PARAM_ID(p_table);
    if (position == current){
        //DO NOTHING
    } else if (position < current){
        SET_NEXT_PARAM_ID(position, p_table, current);
        SET_FIRST_PARAM_ID(p_table, position);
    } else {
        /* Search for the position in the linked list where we need to slot in*/
        while (position > GET_NEXT_PARAM_ID(current, p_table))
            current = GET_NEXT_PARAM_ID(current, p_table);

        /*If node already exists no need to alter linking*/
        if (position != GET_NEXT_PARAM_ID(current, p_table)) {
            next=GET_NEXT_PARAM_ID(current, p_table);
            SET_NEXT_PARAM_ID(current, p_table, position);
            SET_NEXT_PARAM_ID(position, p_table, next);
        }
    }

    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : commitSetBatch
 *
 * DESCRIPTION: commit all set parameters in the batch work to backend
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::commitSetBatch()
{
    int32_t rc = NO_ERROR;
    if (m_pParamBuf->first_flagged_entry < CAM_INTF_PARM_MAX) {
        rc = m_pCamOpsTbl->ops->set_parms(m_pCamOpsTbl->camera_handle, m_pParamBuf);
    }
    if (rc == NO_ERROR) {
        // commit change from temp storage into param map
        rc = commitParamChanges();
    }
    return rc;
}

/*===========================================================================
 * FUNCTION   : commitGetBatch
 *
 * DESCRIPTION: commit all get parameters in the batch work to backend
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::commitGetBatch()
{
    if (m_pParamBuf->first_flagged_entry < CAM_INTF_PARM_MAX) {
        return m_pCamOpsTbl->ops->get_parms(m_pCamOpsTbl->camera_handle, m_pParamBuf);
    } else {
        return NO_ERROR;
    }
}

/*===========================================================================
 * FUNCTION   : updateParamEntry
 *
 * DESCRIPTION: update a parameter entry in the local temp map obj
 *
 * PARAMETERS :
 *   @key     : key of the entry
 *   @value   : value of the entry
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::updateParamEntry(const char *key, const char *value)
{
    m_tempMap.replaceValueFor(String8(key), String8(value));
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : commitParamChanges
 *
 * DESCRIPTION: commit all changes in local temp map obj into parameter obj
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::commitParamChanges()
{
    size_t size = m_tempMap.size();
    for (size_t i = 0; i < size; i++) {
        String8 k, v;
        k = m_tempMap.keyAt(i);
        v = m_tempMap.valueAt(i);
        set(k, v);
    }
    m_tempMap.clear();

    // update local changes
    m_bRecordingHint = m_bRecordingHint_new;
    m_bZslMode = m_bZslMode_new;

    return NO_ERROR;
}

}; // namespace qcamera
