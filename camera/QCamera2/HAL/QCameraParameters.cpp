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

#define LOG_TAG "QCameraParameters"

#include <cutils/properties.h>
#include <math.h>
#include <utils/Errors.h>
#include <utils/Log.h>
#include <string.h>
#include <stdlib.h>
#include <gralloc_priv.h>
#include <sys/sysinfo.h>
#include "QCamera2HWI.h"
#include "QCameraParameters.h"

#define ASPECT_TOLERANCE 0.001
#define ISP_SEC_SCALAR_MAX_LIMIT 2048*1536

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
const char QCameraParameters::KEY_QC_EXPOSURE_TIME[] = "exposure-time";
const char QCameraParameters::KEY_QC_MIN_EXPOSURE_TIME[] = "min-exposure-time";
const char QCameraParameters::KEY_QC_MAX_EXPOSURE_TIME[] = "max-exposure-time";
const char QCameraParameters::KEY_QC_LENSSHADE[] = "lensshade";
const char QCameraParameters::KEY_QC_SUPPORTED_LENSSHADE_MODES[] = "lensshade-values";
const char QCameraParameters::KEY_QC_AUTO_EXPOSURE[] = "auto-exposure";
const char QCameraParameters::KEY_QC_SUPPORTED_AUTO_EXPOSURE[] = "auto-exposure-values";
const char QCameraParameters::KEY_QC_DENOISE[] = "denoise";
const char QCameraParameters::KEY_QC_SUPPORTED_DENOISE[] = "denoise-values";
const char QCameraParameters::KEY_QC_FOCUS_ALGO[] = "selectable-zone-af";
const char QCameraParameters::KEY_QC_SUPPORTED_FOCUS_ALGOS[] = "selectable-zone-af-values";
const char QCameraParameters::KEY_QC_MANUAL_FOCUS_POSITION[] = "manual-focus-position";
const char QCameraParameters::KEY_QC_MANUAL_FOCUS_POS_TYPE[] = "manual-focus-pos-type";
const char QCameraParameters::KEY_QC_MIN_FOCUS_POS_INDEX[] = "min-focus-pos-index";
const char QCameraParameters::KEY_QC_MAX_FOCUS_POS_INDEX[] = "max-focus-pos-index";
const char QCameraParameters::KEY_QC_MIN_FOCUS_POS_DAC[] = "min-focus-pos-dac";
const char QCameraParameters::KEY_QC_MAX_FOCUS_POS_DAC[] = "max-focus-pos-dac";
const char QCameraParameters::KEY_QC_FACE_DETECTION[] = "face-detection";
const char QCameraParameters::KEY_QC_SUPPORTED_FACE_DETECTION[] = "face-detection-values";
const char QCameraParameters::KEY_QC_FACE_RECOGNITION[] = "face-recognition";
const char QCameraParameters::KEY_QC_SUPPORTED_FACE_RECOGNITION[] = "face-recognition-values";
const char QCameraParameters::KEY_QC_MEMORY_COLOR_ENHANCEMENT[] = "mce";
const char QCameraParameters::KEY_QC_SUPPORTED_MEM_COLOR_ENHANCE_MODES[] = "mce-values";
const char QCameraParameters::KEY_QC_DIS[] = "dis";
const char QCameraParameters::KEY_QC_SUPPORTED_DIS_MODES[] = "dis-values";
const char QCameraParameters::KEY_QC_VIDEO_HIGH_FRAME_RATE[] = "video-hfr";
const char QCameraParameters::KEY_QC_VIDEO_HIGH_SPEED_RECORDING[] = "video-hsr";
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
const char QCameraParameters::KEY_QC_SCALED_PICTURE_SIZES[] = "scaled-picture-sizes";
const char QCameraParameters::KEY_QC_HDR_NEED_1X[] = "hdr-need-1x";
const char QCameraParameters::KEY_QC_PREVIEW_FLIP[] = "preview-flip";
const char QCameraParameters::KEY_QC_VIDEO_FLIP[] = "video-flip";
const char QCameraParameters::KEY_QC_SNAPSHOT_PICTURE_FLIP[] = "snapshot-picture-flip";
const char QCameraParameters::KEY_QC_SUPPORTED_FLIP_MODES[] = "flip-mode-values";
const char QCameraParameters::KEY_QC_VIDEO_HDR[] = "video-hdr";
const char QCameraParameters::KEY_QC_SENSOR_HDR[] = "sensor-hdr";
const char QCameraParameters::KEY_QC_VT_ENABLE[] = "avtimer";
const char QCameraParameters::KEY_QC_SUPPORTED_VIDEO_HDR_MODES[] = "video-hdr-values";
const char QCameraParameters::KEY_QC_SUPPORTED_SENSOR_HDR_MODES[] = "sensor-hdr-values";
const char QCameraParameters::KEY_QC_AUTO_HDR_ENABLE [] = "auto-hdr-enable";
const char QCameraParameters::KEY_QC_SNAPSHOT_BURST_NUM[] = "snapshot-burst-num";
const char QCameraParameters::KEY_QC_SNAPSHOT_FD_DATA[] = "snapshot-fd-data-enable";
const char QCameraParameters::KEY_QC_TINTLESS_ENABLE[] = "tintless";
const char QCameraParameters::KEY_QC_CDS_MODE[] = "cds-mode";
const char QCameraParameters::KEY_QC_VIDEO_ROTATION[] = "video-rotation";
const char QCameraParameters::KEY_QC_AF_BRACKET[] = "af-bracket";
const char QCameraParameters::KEY_QC_SUPPORTED_AF_BRACKET_MODES[] = "af-bracket-values";
const char QCameraParameters::KEY_QC_CHROMA_FLASH[] = "chroma-flash";
const char QCameraParameters::KEY_QC_SUPPORTED_CHROMA_FLASH_MODES[] = "chroma-flash-values";
const char QCameraParameters::KEY_QC_OPTI_ZOOM[] = "opti-zoom";
const char QCameraParameters::KEY_QC_SEE_MORE[] = "see-more";
const char QCameraParameters::KEY_QC_SUPPORTED_OPTI_ZOOM_MODES[] = "opti-zoom-values";
const char QCameraParameters::KEY_QC_FSSR[] = "FSSR";
const char QCameraParameters::KEY_QC_SUPPORTED_FSSR_MODES[] = "FSSR-values";
const char QCameraParameters::KEY_QC_SUPPORTED_SEE_MORE_MODES[] = "see-more-values";
const char QCameraParameters::KEY_QC_TRUE_PORTRAIT[] = "true-portrait";
const char QCameraParameters::KEY_QC_SUPPORTED_TRUE_PORTRAIT_MODES[] = "true-portrait-values";
const char QCameraParameters::KEY_QC_MULTI_TOUCH_FOCUS[] = "multi-touch-focus";
const char QCameraParameters::KEY_QC_SUPPORTED_MULTI_TOUCH_FOCUS_MODES[] =
        "multi-touch-focus-values";
const char QCameraParameters::KEY_QC_WB_MANUAL_CCT[] = "wb-manual-cct";
const char QCameraParameters::KEY_QC_MIN_WB_CCT[] = "min-wb-cct";
const char QCameraParameters::KEY_QC_MAX_WB_CCT[] = "max-wb-cct";
const char QCameraParameters::KEY_INTERNAL_PERVIEW_RESTART[] = "internal-restart";
const char QCameraParameters::KEY_QC_LONG_SHOT[] = "long-shot";
const char QCameraParameters::KEY_QC_LONGSHOT_SUPPORTED[] = "longshot-supported";
const char QCameraParameters::KEY_QC_ZSL_HDR_SUPPORTED[] = "zsl-hdr-supported";
const char QCameraParameters::KEY_QC_AUTO_HDR_SUPPORTED[] = "auto-hdr-supported";

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
const char QCameraParameters::ISO_3200[] = "ISO3200";

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

// Values for AF Bracketing setting.
const char QCameraParameters::AF_BRACKET_OFF[] = "af-bracket-off";
const char QCameraParameters::AF_BRACKET_ON[] = "af-bracket-on";

// Values for Chroma Flash setting.
const char QCameraParameters::CHROMA_FLASH_OFF[] = "chroma-flash-off";
const char QCameraParameters::CHROMA_FLASH_ON[] = "chroma-flash-on";

// Values for Opti Zoom setting.
const char QCameraParameters::OPTI_ZOOM_OFF[] = "opti-zoom-off";
const char QCameraParameters::OPTI_ZOOM_ON[] = "opti-zoom-on";

// Values for True Portrait setting.
const char QCameraParameters::TRUE_PORTRAIT_OFF[] = "true-portrait-off";
const char QCameraParameters::TRUE_PORTRAIT_ON[] = "true-portrait-on";

// Values for FSSR setting.
const char QCameraParameters::FSSR_OFF[] = "FSSR-off";
const char QCameraParameters::FSSR_ON[] = "FSSR-on";

// Value for Multi-touch Focus setting.
const char QCameraParameters::MULTI_TOUCH_FOCUS_OFF[] = "multi-touch-focus-off";
const char QCameraParameters::MULTI_TOUCH_FOCUS_ON[] = "multi-touch-focus-on";

// Values for FLIP settings.
const char QCameraParameters::FLIP_MODE_OFF[] = "off";
const char QCameraParameters::FLIP_MODE_V[] = "flip-v";
const char QCameraParameters::FLIP_MODE_H[] = "flip-h";
const char QCameraParameters::FLIP_MODE_VH[] = "flip-vh";

const char QCameraParameters::CDS_MODE_OFF[] = "off";
const char QCameraParameters::CDS_MODE_ON[] = "on";
const char QCameraParameters::CDS_MODE_AUTO[] = "auto";

const char QCameraParameters::KEY_SELECTED_AUTO_SCENE[] = "selected-auto-scene";

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

const QCameraParameters::QCameraMap<cam_auto_exposure_mode_type>
        QCameraParameters::AUTO_EXPOSURE_MAP[] = {
    { AUTO_EXPOSURE_FRAME_AVG,           CAM_AEC_MODE_FRAME_AVERAGE },
    { AUTO_EXPOSURE_CENTER_WEIGHTED,     CAM_AEC_MODE_CENTER_WEIGHTED },
    { AUTO_EXPOSURE_SPOT_METERING,       CAM_AEC_MODE_SPOT_METERING },
    { AUTO_EXPOSURE_SMART_METERING,      CAM_AEC_MODE_SMART_METERING },
    { AUTO_EXPOSURE_USER_METERING,       CAM_AEC_MODE_USER_METERING },
    { AUTO_EXPOSURE_SPOT_METERING_ADV,   CAM_AEC_MODE_SPOT_METERING_ADV },
    { AUTO_EXPOSURE_CENTER_WEIGHTED_ADV, CAM_AEC_MODE_CENTER_WEIGHTED_ADV },
};

const QCameraParameters::QCameraMap<cam_format_t>
        QCameraParameters::PREVIEW_FORMATS_MAP[] = {
    {PIXEL_FORMAT_YUV420SP,        CAM_FORMAT_YUV_420_NV21},
    {PIXEL_FORMAT_YUV420P,         CAM_FORMAT_YUV_420_YV12},
    {PIXEL_FORMAT_YUV420SP_ADRENO, CAM_FORMAT_YUV_420_NV21_ADRENO},
    {PIXEL_FORMAT_YV12,            CAM_FORMAT_YUV_420_YV12},
    {PIXEL_FORMAT_NV12,            CAM_FORMAT_YUV_420_NV12},
    {QC_PIXEL_FORMAT_NV12_VENUS,   CAM_FORMAT_YUV_420_NV12_VENUS}
};

const QCameraParameters::QCameraMap<cam_format_t>
        QCameraParameters::PICTURE_TYPES_MAP[] = {
    {PIXEL_FORMAT_JPEG,                          CAM_FORMAT_JPEG},
    {PIXEL_FORMAT_YUV420SP,                      CAM_FORMAT_YUV_420_NV21},
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

const QCameraParameters::QCameraMap<cam_focus_mode_type>
        QCameraParameters::FOCUS_MODES_MAP[] = {
    { FOCUS_MODE_AUTO,               CAM_FOCUS_MODE_AUTO },
    { FOCUS_MODE_INFINITY,           CAM_FOCUS_MODE_INFINITY },
    { FOCUS_MODE_MACRO,              CAM_FOCUS_MODE_MACRO },
    { FOCUS_MODE_FIXED,              CAM_FOCUS_MODE_FIXED },
    { FOCUS_MODE_EDOF,               CAM_FOCUS_MODE_EDOF },
    { FOCUS_MODE_CONTINUOUS_PICTURE, CAM_FOCUS_MODE_CONTINOUS_PICTURE },
    { FOCUS_MODE_CONTINUOUS_VIDEO,   CAM_FOCUS_MODE_CONTINOUS_VIDEO },
    { FOCUS_MODE_MANUAL_POSITION,    CAM_FOCUS_MODE_MANUAL},
};

const QCameraParameters::QCameraMap<cam_effect_mode_type>
        QCameraParameters::EFFECT_MODES_MAP[] = {
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

const QCameraParameters::QCameraMap<cam_scene_mode_type>
        QCameraParameters::SCENE_MODES_MAP[] = {
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
    { SCENE_MODE_HDR,            CAM_SCENE_MODE_HDR },
};

const QCameraParameters::QCameraMap<cam_flash_mode_t>
        QCameraParameters::FLASH_MODES_MAP[] = {
    { FLASH_MODE_OFF,   CAM_FLASH_MODE_OFF },
    { FLASH_MODE_AUTO,  CAM_FLASH_MODE_AUTO },
    { FLASH_MODE_ON,    CAM_FLASH_MODE_ON },
    { FLASH_MODE_TORCH, CAM_FLASH_MODE_TORCH }
};

const QCameraParameters::QCameraMap<cam_focus_algorithm_type>
         QCameraParameters::FOCUS_ALGO_MAP[] = {
    { FOCUS_ALGO_AUTO,            CAM_FOCUS_ALGO_AUTO },
    { FOCUS_ALGO_SPOT_METERING,   CAM_FOCUS_ALGO_SPOT },
    { FOCUS_ALGO_CENTER_WEIGHTED, CAM_FOCUS_ALGO_CENTER_WEIGHTED },
    { FOCUS_ALGO_FRAME_AVERAGE,   CAM_FOCUS_ALGO_AVERAGE }
};

const QCameraParameters::QCameraMap<cam_wb_mode_type>
        QCameraParameters::WHITE_BALANCE_MODES_MAP[] = {
    { WHITE_BALANCE_AUTO,            CAM_WB_MODE_AUTO },
    { WHITE_BALANCE_INCANDESCENT,    CAM_WB_MODE_INCANDESCENT },
    { WHITE_BALANCE_FLUORESCENT,     CAM_WB_MODE_FLUORESCENT },
    { WHITE_BALANCE_WARM_FLUORESCENT,CAM_WB_MODE_WARM_FLUORESCENT},
    { WHITE_BALANCE_DAYLIGHT,        CAM_WB_MODE_DAYLIGHT },
    { WHITE_BALANCE_CLOUDY_DAYLIGHT, CAM_WB_MODE_CLOUDY_DAYLIGHT },
    { WHITE_BALANCE_TWILIGHT,        CAM_WB_MODE_TWILIGHT },
    { WHITE_BALANCE_SHADE,           CAM_WB_MODE_SHADE },
    { WHITE_BALANCE_MANUAL_CCT,      CAM_WB_MODE_CCT},
};

const QCameraParameters::QCameraMap<cam_antibanding_mode_type>
        QCameraParameters::ANTIBANDING_MODES_MAP[] = {
    { ANTIBANDING_OFF,  CAM_ANTIBANDING_MODE_OFF },
    { ANTIBANDING_50HZ, CAM_ANTIBANDING_MODE_50HZ },
    { ANTIBANDING_60HZ, CAM_ANTIBANDING_MODE_60HZ },
    { ANTIBANDING_AUTO, CAM_ANTIBANDING_MODE_AUTO }
};

const QCameraParameters::QCameraMap<cam_iso_mode_type>
        QCameraParameters::ISO_MODES_MAP[] = {
    { ISO_AUTO,  CAM_ISO_MODE_AUTO },
    { ISO_HJR,   CAM_ISO_MODE_DEBLUR },
    { ISO_100,   CAM_ISO_MODE_100 },
    { ISO_200,   CAM_ISO_MODE_200 },
    { ISO_400,   CAM_ISO_MODE_400 },
    { ISO_800,   CAM_ISO_MODE_800 },
    { ISO_1600,  CAM_ISO_MODE_1600 },
    { ISO_3200,  CAM_ISO_MODE_3200 }
};

const QCameraParameters::QCameraMap<cam_hfr_mode_t>
        QCameraParameters::HFR_MODES_MAP[] = {
    { VIDEO_HFR_OFF, CAM_HFR_MODE_OFF },
    { VIDEO_HFR_2X,  CAM_HFR_MODE_60FPS },
    { VIDEO_HFR_3X,  CAM_HFR_MODE_90FPS },
    { VIDEO_HFR_4X,  CAM_HFR_MODE_120FPS },
    { VIDEO_HFR_5X,  CAM_HFR_MODE_150FPS }
};

const QCameraParameters::QCameraMap<cam_bracket_mode>
        QCameraParameters::BRACKETING_MODES_MAP[] = {
    { AE_BRACKET_OFF, CAM_EXP_BRACKETING_OFF },
    { AE_BRACKET,         CAM_EXP_BRACKETING_ON }
};

const QCameraParameters::QCameraMap<int>
        QCameraParameters::ON_OFF_MODES_MAP[] = {
    { VALUE_OFF, 0 },
    { VALUE_ON,  1 }
};

const QCameraParameters::QCameraMap<int>
        QCameraParameters::TOUCH_AF_AEC_MODES_MAP[] = {
    { QCameraParameters::TOUCH_AF_AEC_OFF, 0 },
    { QCameraParameters::TOUCH_AF_AEC_ON, 1 }
};

const QCameraParameters::QCameraMap<int>
        QCameraParameters::ENABLE_DISABLE_MODES_MAP[] = {
    { VALUE_ENABLE,  1 },
    { VALUE_DISABLE, 0 }
};

const QCameraParameters::QCameraMap<int>
        QCameraParameters::DENOISE_ON_OFF_MODES_MAP[] = {
    { DENOISE_OFF, 0 },
    { DENOISE_ON,  1 }
};

const QCameraParameters::QCameraMap<int>
        QCameraParameters::TRUE_FALSE_MODES_MAP[] = {
    { VALUE_FALSE, 0},
    { VALUE_TRUE,  1}
};

const QCameraParameters::QCameraMap<cam_flip_t>
        QCameraParameters::FLIP_MODES_MAP[] = {
    {FLIP_MODE_OFF, FLIP_NONE},
    {FLIP_MODE_V, FLIP_V},
    {FLIP_MODE_H, FLIP_H},
    {FLIP_MODE_VH, FLIP_V_H}
};

const QCameraParameters::QCameraMap<int>
        QCameraParameters::AF_BRACKETING_MODES_MAP[] = {
    { AF_BRACKET_OFF, 0 },
    { AF_BRACKET_ON,  1 }
};

const QCameraParameters::QCameraMap<int>
        QCameraParameters::CHROMA_FLASH_MODES_MAP[] = {
    { CHROMA_FLASH_OFF, 0 },
    { CHROMA_FLASH_ON,  1 }
};

const QCameraParameters::QCameraMap<int>
        QCameraParameters::OPTI_ZOOM_MODES_MAP[] = {
    { OPTI_ZOOM_OFF, 0 },
    { OPTI_ZOOM_ON,  1 }
};

const QCameraParameters::QCameraMap<int>
        QCameraParameters::TRUE_PORTRAIT_MODES_MAP[] = {
    { TRUE_PORTRAIT_OFF, 0 },
    { TRUE_PORTRAIT_ON,  1 }
};

const QCameraParameters::QCameraMap<int>
        QCameraParameters::FSSR_MODES_MAP[] = {
    { FSSR_OFF, 0 },
    { FSSR_ON,  1 }
};

const QCameraParameters::QCameraMap<int>
        QCameraParameters::MULTI_TOUCH_FOCUS_MODES_MAP[] = {
    { MULTI_TOUCH_FOCUS_OFF, 0 },
    { MULTI_TOUCH_FOCUS_ON,  1 }
};

const QCameraParameters::QCameraMap<cam_cds_mode_type_t>
        QCameraParameters::CDS_MODES_MAP[] = {
    { CDS_MODE_OFF, CAM_CDS_MODE_OFF },
    { CDS_MODE_ON, CAM_CDS_MODE_ON },
    { CDS_MODE_AUTO, CAM_CDS_MODE_AUTO}
};

#define DEFAULT_CAMERA_AREA "(0, 0, 0, 0, 0)"
#define DATA_PTR(MEM_OBJ,INDEX) MEM_OBJ->getPtr( INDEX )
#define MIN_PP_BUF_CNT 1
#define TOTAL_RAM_SIZE_512MB 536870912
#define PARAM_MAP_SIZE(MAP) (sizeof(MAP)/sizeof(MAP[0]))


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
      m_reprocScaleParam(this),
      m_pCapability(NULL),
      m_pCamOpsTbl(NULL),
      m_pParamHeap(NULL),
      m_pParamBuf(NULL),
      m_bZslMode(false),
      m_bZslMode_new(false),
      m_bRecordingHint(false),
      m_bRecordingHint_new(false),
      m_bHistogramEnabled(false),
      m_bLongshotEnabled(false),
      m_nFaceProcMask(0),
      m_bDebugFps(false),
      mFocusMode(CAM_FOCUS_MODE_MAX),
      mPreviewFormat(CAM_FORMAT_YUV_420_NV21),
      mPictureFormat(CAM_FORMAT_JPEG),
      m_bNeedRestart(false),
      m_bNoDisplayMode(false),
      m_bWNROn(false),
      m_bInited(false),
      m_nBurstNum(1),
      m_bUpdateEffects(false),
      m_bSceneTransitionAuto(false),
      m_bPreviewFlipChanged(false),
      m_bVideoFlipChanged(false),
      m_bSnapshotFlipChanged(false),
      m_bFixedFrameRateSet(false),
      m_bHDREnabled(false),
      m_bAVTimerEnabled(false),
      m_bMobiMask(0),
      m_bDISEnabled(false),
      m_AdjustFPS(NULL),
      m_bHDR1xFrameEnabled(false),
      m_HDRSceneEnabled(false),
      m_bHDRThumbnailProcessNeeded(false),
      m_bHDR1xExtraBufferNeeded(true),
      m_bHDROutputCropEnabled(false),
      m_curCCT(-1),
      m_curFocusPos(-1),
      m_tempMap(),
      m_bAFBracketingOn(false),
      m_bMultiTouchFocusOn(false),
      m_bChromaFlashOn(false),
      m_bOptiZoomOn(false),
      m_bFssrOn(false),
      m_bSeeMoreOn(false),
      m_bHfrMode(false),
      mHfrMode(CAM_HFR_MODE_OFF),
      m_bDisplayFrame(true),
      m_bAeBracketingEnabled(false),
      mFlashValue(CAM_FLASH_MODE_OFF),
      mFlashDaemonValue(CAM_FLASH_MODE_OFF),
      m_bSensorHDREnabled(false),
      m_bTruePortraitOn(false)
{
    char value[PROPERTY_VALUE_MAX];
    // TODO: may move to parameter instead of sysprop
    property_get("persist.debug.sf.showfps", value, "0");
    m_bDebugFps = atoi(value) > 0 ? true : false;
    m_bReleaseTorchCamera = false;
    m_pTorch = NULL;

    // For thermal mode, it should be set as system property
    // because system property applies to all applications, while
    // parameters only apply to specific app.
    property_get("persist.camera.thermal.mode", value, "frameskip");
    if (!strcmp(value, "frameskip")) {
        m_ThermalMode = QCAMERA_THERMAL_ADJUST_FRAMESKIP;
    } else {
        if (strcmp(value, "fps"))
            ALOGE("%s: Invalid camera thermal mode %s", __func__, value);
        m_ThermalMode = QCAMERA_THERMAL_ADJUST_FPS;
    }

    memset(&m_LiveSnapshotSize, 0, sizeof(m_LiveSnapshotSize));
    memset(&m_default_fps_range, 0, sizeof(m_default_fps_range));
    memset(&m_MTFBracketInfo, 0, sizeof(m_MTFBracketInfo));
    memset(&m_hfrFpsRange, 0, sizeof(m_hfrFpsRange));

    // init focus steps to -1, invalid steps
    for (int i = 0; i < MAX_AF_BRACKETING_VALUES; i++) {
       m_MTFBracketInfo.focus_steps[i] = -1;
    }

    m_currNumBufMTF = 0;
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
    m_reprocScaleParam(this),
    m_pCapability(NULL),
    m_pCamOpsTbl(NULL),
    m_pParamHeap(NULL),
    m_pParamBuf(NULL),
    m_bZslMode(false),
    m_bZslMode_new(false),
    m_bRecordingHint(false),
    m_bRecordingHint_new(false),
    m_bHistogramEnabled(false),
    m_bLongshotEnabled(false),
    m_nFaceProcMask(0),
    m_bDebugFps(false),
    mFocusMode(CAM_FOCUS_MODE_MAX),
    mPreviewFormat(CAM_FORMAT_YUV_420_NV21),
    mPictureFormat(CAM_FORMAT_JPEG),
    m_bNeedRestart(false),
    m_bNoDisplayMode(false),
    m_bWNROn(false),
    m_bInited(false),
    m_nBurstNum(1),
    m_bPreviewFlipChanged(false),
    m_bVideoFlipChanged(false),
    m_bSnapshotFlipChanged(false),
    m_bFixedFrameRateSet(false),
    m_bHDREnabled(false),
    m_bAVTimerEnabled(false),
    m_AdjustFPS(NULL),
    m_bHDR1xFrameEnabled(false),
    m_HDRSceneEnabled(false),
    m_bHDRThumbnailProcessNeeded(false),
    m_bHDR1xExtraBufferNeeded(true),
    m_bHDROutputCropEnabled(false),
    m_tempMap(),
    m_bAFBracketingOn(false),
    m_bMultiTouchFocusOn(false),
    m_bChromaFlashOn(false),
    m_bOptiZoomOn(false),
    m_bFssrOn(false),
    m_bSeeMoreOn(false),
    m_bHfrMode(false),
    mHfrMode(CAM_HFR_MODE_OFF),
    m_bAeBracketingEnabled(false),
    mFlashValue(CAM_FLASH_MODE_OFF),
    mFlashDaemonValue(CAM_FLASH_MODE_OFF),
    m_bSensorHDREnabled(false),
    m_bTruePortraitOn(false)
{
    memset(&m_LiveSnapshotSize, 0, sizeof(m_LiveSnapshotSize));
    m_pTorch = NULL;
    m_bReleaseTorchCamera = false;
    m_currNumBufMTF = 0;
    memset(&m_default_fps_range, 0, sizeof(m_default_fps_range));
    memset(&m_hfrFpsRange, 0, sizeof(m_hfrFpsRange));
    memset(&m_MTFBracketInfo, 0, sizeof(m_MTFBracketInfo));

    // init focus steps to -1, invalid steps
    for (int i = 0; i < MAX_AF_BRACKETING_VALUES; i++) {
       m_MTFBracketInfo.focus_steps[i] = -1;
    }
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
String8 QCameraParameters::createSizesString(const cam_dimension_t *sizes, size_t len)
{
    String8 str;
    char buffer[32];

    if (len > 0) {
        snprintf(buffer, sizeof(buffer), "%dx%d", sizes[0].width, sizes[0].height);
        str.append(buffer);
    }
    for (size_t i = 1; i < len; i++) {
        snprintf(buffer, sizeof(buffer), ",%dx%d",
                sizes[i].width, sizes[i].height);
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
template <typename valuesType, class mapType> String8 createValuesString(
        const valuesType *values, size_t len, const mapType *map, size_t map_len)
{
    String8 str;
    int count = 0;

    for (size_t i = 0; i < len; i++ ) {
        for (size_t j = 0; j < map_len; j ++)
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
template <class mapType> String8 createValuesStringFromMap(
        const mapType *map, size_t map_len)
{
    String8 str;

    for (size_t i = 0; i < map_len; i++) {
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
String8 QCameraParameters::createZoomRatioValuesString(uint32_t *zoomRatios,
        size_t length)
{
    String8 str;
    char buffer[32] = {0};

    if(length > 0){
        snprintf(buffer, sizeof(buffer), "%d", zoomRatios[0]);
        str.append(buffer);
    }

    for (size_t i = 1; i < length; i++) {
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
String8 QCameraParameters::createHfrValuesString(const cam_hfr_info_t *values,
        size_t len, const QCameraMap<cam_hfr_mode_t> *map, size_t map_len)
{
    String8 str;
    int count = 0;

    for (size_t i = 0; i < len; i++ ) {
        for (size_t j = 0; j < map_len; j ++)
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
    if (count > 0) {
        str.append(",");
    }
    str.append(VIDEO_HFR_OFF);
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
String8 QCameraParameters::createHfrSizesString(const cam_hfr_info_t *values, size_t len)
{
    String8 str;
    char buffer[32];

    if (len > 0) {
        snprintf(buffer, sizeof(buffer), "%dx%d",
                 values[0].dim.width, values[0].dim.height);
        str.append(buffer);
    }
    for (size_t i = 1; i < len; i++) {
        snprintf(buffer, sizeof(buffer), ",%dx%d",
                 values[i].dim.width, values[i].dim.height);
        str.append(buffer);
    }
    return str;
}

/*===========================================================================
 * FUNCTION   : createFpsString
 *
 * DESCRIPTION: create string obj contains array of FPS rates
 *
 * PARAMETERS :
 *   @fps     : default fps range
 *
 * RETURN     : string obj
 *==========================================================================*/
String8 QCameraParameters::createFpsString(cam_fps_range_t &fps)
{
    char buffer[32];
    String8 fpsValues;

    int min_fps = int(fps.min_fps);
    int max_fps = int(fps.max_fps);

    if (min_fps < fps.min_fps){
        min_fps++;
    }
    if (max_fps > fps.max_fps) {
        max_fps--;
    }
    if (min_fps <= max_fps) {
        snprintf(buffer, sizeof(buffer), "%d", min_fps);
        fpsValues.append(buffer);
    }

    for (int i = min_fps+1; i <= max_fps; i++) {
        snprintf(buffer, sizeof(buffer), ",%d", i);
        fpsValues.append(buffer);
    }

    return fpsValues;
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
        size_t len, int &default_fps_index)
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
    for (size_t i = 1; i < len; i++) {
        min_fps = int(fps[i].min_fps * 1000);
        max_fps = int(fps[i].max_fps * 1000);
        if (max_range < (max_fps - min_fps)) {
            max_range = max_fps - min_fps;
            default_fps_index = (int)i;
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
template <class mapType> int lookupAttr(const mapType *arr,
        size_t len, const char *name)
{
    if (name) {
        for (size_t i = 0; i < len; i++) {
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
template <class mapType> const char *lookupNameByValue(const mapType *arr,
        size_t len, int value)
{
    for (size_t i = 0; i < len; i++) {
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
    CDBG("Requested preview size %d x %d", width, height);

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
            CDBG_HIGH("%s: Requested preview size %d x %d", __func__, width, height);
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
    CDBG("Requested picture size %d x %d", width, height);

    // Validate the picture size
    if(!m_reprocScaleParam.isScaleEnabled()){
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
                CDBG_HIGH("%s: Requested picture size %d x %d", __func__, width, height);
                CameraParameters::setPictureSize(width, height);
                return NO_ERROR;
            }
        }
    }else{
        //should use scaled picture size table to validate
        if(m_reprocScaleParam.setValidatePicSize(width, height) == NO_ERROR){
            // check if need to restart preview in case of picture size change
            int old_width, old_height;
            CameraParameters::getPictureSize(&old_width, &old_height);
            if ((m_bZslMode || m_bRecordingHint) &&
                (width != old_width || height != old_height)) {
                m_bNeedRestart = true;
            }

            // set the new value
            char val[32];
            sprintf(val, "%dx%d", width, height);
            CDBG_HIGH("%s: picture size requested %s", __func__, val);
            updateParamEntry(KEY_PICTURE_SIZE, val);
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
            CDBG_HIGH("%s: Requested video size %d x %d", __func__, width, height);
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
    char value[PROPERTY_VALUE_MAX];
    property_get("persist.camera.opt.livepic", value, "1");
    bool useOptimal = atoi(value) > 0 ? true : false;

    // use picture size from user setting
    params.getPictureSize(&m_LiveSnapshotSize.width, &m_LiveSnapshotSize.height);

    size_t livesnapshot_sizes_tbl_cnt =
            m_pCapability->livesnapshot_sizes_tbl_cnt;
    cam_dimension_t *livesnapshot_sizes_tbl =
            &m_pCapability->livesnapshot_sizes_tbl[0];

    // check if HFR is enabled
    const char *hfrStr = params.get(KEY_QC_VIDEO_HIGH_FRAME_RATE);
    cam_hfr_mode_t hfrMode = CAM_HFR_MODE_OFF;
    const char *hsrStr = params.get(KEY_QC_VIDEO_HIGH_SPEED_RECORDING);

    if ((hsrStr != NULL) && strcmp(hsrStr, "off")) {
        int32_t value = lookupAttr(HFR_MODES_MAP, PARAM_MAP_SIZE(HFR_MODES_MAP), hsrStr);
        if ((value != NAME_NOT_FOUND) && (value > CAM_HFR_MODE_OFF)) {
            // if HSR is enabled, change live snapshot size
            for (size_t i = 0; i < m_pCapability->hfr_tbl_cnt; i++) {
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
    } else if ((hfrStr != NULL) && strcmp(hfrStr, "off")) {
        int32_t value = lookupAttr(HFR_MODES_MAP, PARAM_MAP_SIZE(HFR_MODES_MAP), hfrStr);
        if ((value != NAME_NOT_FOUND) && (value > CAM_HFR_MODE_OFF)) {
            // if HFR is enabled, change live snapshot size
            for (size_t i = 0; i < m_pCapability->hfr_tbl_cnt; i++) {
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

    if (useOptimal || hfrMode != CAM_HFR_MODE_OFF) {
        bool found = false;

        // first check if picture size is within the list of supported sizes
        for (size_t i = 0; i < livesnapshot_sizes_tbl_cnt; ++i) {
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
            for (size_t i = 0; i < livesnapshot_sizes_tbl_cnt; ++i) {
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
                CDBG("%s: Cannot find matching aspect ratio, choose max of preview or video size", __func__);
                params.getVideoSize(&m_LiveSnapshotSize.width, &m_LiveSnapshotSize.height);
                if (m_LiveSnapshotSize.width < width && m_LiveSnapshotSize.height < height) {
                    m_LiveSnapshotSize.width = width;
                    m_LiveSnapshotSize.height = height;
                }
            }
        }
    }
    CDBG("%s: live snapshot size %d x %d", __func__,
          m_LiveSnapshotSize.width, m_LiveSnapshotSize.height);

    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setRawSize
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
int32_t QCameraParameters::setRawSize(cam_dimension_t &dim)
{
    m_rawSize = dim;
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
    int32_t previewFormat = lookupAttr(PREVIEW_FORMATS_MAP,
            PARAM_MAP_SIZE(PREVIEW_FORMATS_MAP), str);
    if (previewFormat != NAME_NOT_FOUND) {
        mPreviewFormat = (cam_format_t)previewFormat;

        CameraParameters::setPreviewFormat(str);
        CDBG("%s: format %d\n", __func__, mPreviewFormat);
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
    int32_t pictureFormat = lookupAttr(PICTURE_TYPES_MAP, PARAM_MAP_SIZE(PICTURE_TYPES_MAP), str);
    if (pictureFormat != NAME_NOT_FOUND) {
        mPictureFormat = pictureFormat;

        CameraParameters::setPictureFormat(str);
        CDBG("%s: format %d\n", __func__, mPictureFormat);
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

    CDBG("requested jpeg thumbnail size %d x %d", width, height);

#ifdef USE_KK_CODE
    size_t sizes_cnt = PARAM_MAP_SIZE(THUMBNAIL_SIZES_MAP);

    cam_dimension_t dim;

    // While taking livesnaphot match jpeg thumbnail size aspect
    // ratio to liveshot size. For normal snapshot match thumbnail
    // aspect ratio to picture size.
    if (m_bRecordingHint) {
        getLiveSnapshotSize(dim);
    } else {
        params.getPictureSize(&dim.width, &dim.height);
    }

    if (0 == dim.height) {
        ALOGE("%s: picture size is invalid (%d x %d)", __func__, dim.width, dim.height);
        return BAD_VALUE;
    }
    double picAspectRatio = (double)dim.width / (double)dim.height;

    int optimalWidth = 0, optimalHeight = 0;
    if (width != 0 || height != 0) {
        // If input jpeg thumnmail size is (0,0), meaning no thumbnail needed
        // hornor this setting.
        // Otherwise, search for optimal jpeg thumbnail size that has the same
        // aspect ratio as picture size.
        // If missign jpeg thumbnail size with appropriate aspect ratio,
        // just honor setting supplied by application.

        // Try to find a size matches aspect ratio and has the largest width
        for (size_t i = 0; i < sizes_cnt; i++) {
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

        if ((0 == optimalWidth) || (0 == optimalHeight)) {
            // Optimal size not found
            // Validate thumbnail size
            for (size_t i = 0; i < sizes_cnt; i++) {
                if (width == THUMBNAIL_SIZES_MAP[i].width &&
                    height == THUMBNAIL_SIZES_MAP[i].height) {
                    optimalWidth = width;
                    optimalHeight = height;
                    break;
                }
            }
        }
        if (optimalWidth == 0 && optimalHeight == 0) {
            CDBG_HIGH("%s: Could not find optimal size", __func__);
            optimalWidth = width;
            optimalHeight = height;
        }
    }
    set(KEY_JPEG_THUMBNAIL_WIDTH, optimalWidth);
    set(KEY_JPEG_THUMBNAIL_HEIGHT, optimalHeight);
#else
    set(KEY_JPEG_THUMBNAIL_WIDTH, width);
    set(KEY_JPEG_THUMBNAIL_HEIGHT, height);
#endif
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
    int prevMinFps, prevMaxFps, vidMinFps, vidMaxFps;
    int rc = NO_ERROR;
    bool found = false, updateNeeded = false;

    CameraParameters::getPreviewFpsRange(&prevMinFps, &prevMaxFps);
    params.getPreviewFpsRange(&minFps, &maxFps);

    CDBG_HIGH("%s: FpsRange Values:(%d, %d)", __func__, prevMinFps, prevMaxFps);
    CDBG_HIGH("%s: Requested FpsRange Values:(%d, %d)", __func__, minFps, maxFps);

    //first check if we need to change fps because of HFR mode change
    updateNeeded = UpdateHFRFrameRate(params);
    if(updateNeeded) {
        m_bNeedRestart = true;
        rc = setHighFrameRate(mHfrMode);
        if (rc != NO_ERROR) goto end;
    }
    CDBG("%s: UpdateHFRFrameRate %d", __func__, updateNeeded);

    vidMinFps = (int)m_hfrFpsRange.video_min_fps;
    vidMaxFps = (int)m_hfrFpsRange.video_max_fps;

    if(minFps == prevMinFps && maxFps == prevMaxFps) {
        if ( m_bFixedFrameRateSet ) {
            minFps = params.getPreviewFrameRate() * 1000;
            maxFps = params.getPreviewFrameRate() * 1000;
            m_bFixedFrameRateSet = false;
        } else if (!updateNeeded) {
            CDBG("%s: No change in FpsRange", __func__);
            rc = NO_ERROR;
            goto end;
        }
    }

    for(size_t i = 0; i < m_pCapability->fps_ranges_tbl_cnt; i++) {
        // if the value is in the supported list
        if (minFps >= m_pCapability->fps_ranges_tbl[i].min_fps * 1000 &&
                maxFps <= m_pCapability->fps_ranges_tbl[i].max_fps * 1000) {
            found = true;
            CDBG_HIGH("%s: FPS i=%d : minFps = %d, maxFps = %d"
                    " vidMinFps = %d, vidMaxFps = %d",
                    __func__, i, minFps, maxFps,
                    (int)m_hfrFpsRange.video_min_fps,
                    (int)m_hfrFpsRange.video_max_fps);
            if ((0.0f >= m_hfrFpsRange.video_min_fps) ||
                    (0.0f >= m_hfrFpsRange.video_max_fps)) {
                vidMinFps = minFps;
                vidMaxFps = maxFps;
            }
            else {
                vidMinFps = (int)m_hfrFpsRange.video_min_fps;
                vidMaxFps = (int)m_hfrFpsRange.video_max_fps;
            }

            setPreviewFpsRange(minFps, maxFps, vidMinFps, vidMaxFps);
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
 * FUNCTION   : UpdateHFRFrameRate
 *
 * DESCRIPTION: set preview FPS range based on HFR setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : bool true/false
 *                  true -if HAL needs to overwrite FPS range set by app, false otherwise.
 *==========================================================================*/

bool QCameraParameters::UpdateHFRFrameRate(const QCameraParameters& params)
{
    bool updateNeeded = false;
    int min_fps, max_fps;
    int32_t hfrMode = CAM_HFR_MODE_OFF;
    int32_t newHfrMode = CAM_HFR_MODE_OFF;

    int parm_minfps,parm_maxfps;
    int prevMinFps, prevMaxFps;
    CameraParameters::getPreviewFpsRange(&prevMinFps, &prevMaxFps);
    params.getPreviewFpsRange(&parm_minfps, &parm_maxfps);
    CDBG("%s: CameraParameters - : minFps = %d, maxFps = %d ",
                __func__, prevMinFps, prevMaxFps);
    CDBG("%s: Requested params - : minFps = %d, maxFps = %d ",
                __func__, parm_minfps, parm_maxfps);

    const char *hfrStr = params.get(KEY_QC_VIDEO_HIGH_FRAME_RATE);
    const char *hsrStr = params.get(KEY_QC_VIDEO_HIGH_SPEED_RECORDING);

    const char *prev_hfrStr = CameraParameters::get(KEY_QC_VIDEO_HIGH_FRAME_RATE);
    const char *prev_hsrStr = CameraParameters::get(KEY_QC_VIDEO_HIGH_SPEED_RECORDING);

    if ((hfrStr != NULL) && (prev_hfrStr != NULL) && strcmp(hfrStr, prev_hfrStr)) {
        updateParamEntry(KEY_QC_VIDEO_HIGH_FRAME_RATE, hfrStr);
    }

    if ((hsrStr != NULL) && (prev_hsrStr != NULL) && strcmp(hsrStr, prev_hsrStr)) {
        updateParamEntry(KEY_QC_VIDEO_HIGH_SPEED_RECORDING, hsrStr);

    }

    // check if HFR is enabled
    if ((hfrStr != NULL) && strcmp(hfrStr, "off")) {
        hfrMode = lookupAttr(HFR_MODES_MAP, PARAM_MAP_SIZE(HFR_MODES_MAP), hfrStr);
        if(NAME_NOT_FOUND != hfrMode) newHfrMode = hfrMode;
    }
    // check if HSR is enabled
    else if ((hsrStr != NULL) && strcmp(hsrStr, "off")) {
        hfrMode = lookupAttr(HFR_MODES_MAP, PARAM_MAP_SIZE(HFR_MODES_MAP), hsrStr);
        if(NAME_NOT_FOUND != hfrMode) newHfrMode = hfrMode;
    }
    CDBG("%s: prevHfrMode - %d, currentHfrMode = %d ",
                __func__, mHfrMode, newHfrMode);

    if (mHfrMode != newHfrMode) {
        updateNeeded = true;
        mHfrMode = newHfrMode;
        switch(mHfrMode){
            case CAM_HFR_MODE_60FPS:
                min_fps = 60000;
                max_fps = 60000;
                break;
            case CAM_HFR_MODE_90FPS:
                min_fps = 90000;
                max_fps = 90000;
                break;
            case CAM_HFR_MODE_120FPS:
                min_fps = 120000;
                max_fps = 120000;
                break;
            case CAM_HFR_MODE_150FPS:
                min_fps = 150000;
                max_fps = 150000;
                break;
            case CAM_HFR_MODE_OFF:
            default:
                // Set Video Fps to zero
                min_fps = 0;
                max_fps = 0;
                break;
        }
        m_hfrFpsRange.video_min_fps = (float)min_fps;
        m_hfrFpsRange.video_max_fps = (float)max_fps;

        CDBG_HIGH("%s: HFR mode (%d) Set video FPS : minFps = %d, maxFps = %d ",
                __func__, mHfrMode, min_fps, max_fps);
    }

    // Remember if HFR mode is ON
    if ((mHfrMode > CAM_HFR_MODE_OFF) && (mHfrMode < CAM_HFR_MODE_MAX)){
        CDBG_HIGH("HFR mode is ON");
        m_bHfrMode = true;
    }
    else {
        m_hfrFpsRange.video_min_fps = 0;
        m_hfrFpsRange.video_max_fps = 0;
        m_bHfrMode = false;
        CDBG_HIGH("HFR mode is OFF");
    }
    return updateNeeded;
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
    const char *str = params.get(KEY_PREVIEW_FRAME_RATE);
    const char *prev_str = get(KEY_PREVIEW_FRAME_RATE);

    if ( str ) {
        if ( prev_str &&
             strcmp(str, prev_str)) {
            CDBG("%s: Requested Fixed Frame Rate %s", __func__, str);
            updateParamEntry(KEY_PREVIEW_FRAME_RATE, str);
            m_bFixedFrameRateSet = true;
        }
    }
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
            strcmp(str, prev_str) != 0 ||
            m_bUpdateEffects == true ) {
            m_bUpdateEffects = false;
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
 * FUNCTION   : setFocusPosition
 *
 * DESCRIPTION: set focus position from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t  QCameraParameters::setFocusPosition(const QCameraParameters& params)
{
    const char *focus_str = params.get(KEY_FOCUS_MODE);
    CDBG_HIGH("%s, current focus mode: %s", __func__, focus_str);

    if (focus_str != NULL) {
        if (strcmp(focus_str, FOCUS_MODE_MANUAL_POSITION)) {
            CDBG("%s, dont set focus pos to back-end!", __func__);
            return NO_ERROR;
        }
    }
    const char *pos = params.get(KEY_QC_MANUAL_FOCUS_POSITION);
    const char *prev_pos = get(KEY_QC_MANUAL_FOCUS_POSITION);
    const char *type = params.get(KEY_QC_MANUAL_FOCUS_POS_TYPE);
    const char *prev_type = get(KEY_QC_MANUAL_FOCUS_POS_TYPE);

    if ((pos != NULL) && (type != NULL)) {
        if (prev_pos  == NULL || (strcmp(pos, prev_pos) != 0) ||
            prev_type == NULL || (strcmp(type, prev_type) != 0)) {
            return setFocusPosition(type, pos);
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

    if(params.get(KEY_QC_BRIGHTNESS) == NULL) {
       CDBG_HIGH("%s: Brigtness not set by App ",__func__);
       return NO_ERROR;
    }
    if (currentBrightness !=  brightness) {
        if (brightness >= m_pCapability->brightness_ctrl.min_value &&
            brightness <= m_pCapability->brightness_ctrl.max_value) {
            CDBG(" new brightness value : %d ", brightness);
            return setBrightness(brightness);
        } else {
            ALOGE("%s: invalid value %d out of (%d, %d)",
                  __func__, brightness,
                  m_pCapability->brightness_ctrl.min_value,
                  m_pCapability->brightness_ctrl.max_value);
            return BAD_VALUE;
        }
    } else {
        CDBG("%s: No brightness value changed.", __func__);
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

    if(params.get(KEY_QC_SHARPNESS) == NULL) {
       CDBG_HIGH("%s: Sharpness not set by App ",__func__);
       return NO_ERROR;
    }
    if (prev_sharp !=  shaprness) {
        if((shaprness >= m_pCapability->sharpness_ctrl.min_value) &&
           (shaprness <= m_pCapability->sharpness_ctrl.max_value)) {
            CDBG(" new sharpness value : %d ", shaprness);
            return setSharpness(shaprness);
        } else {
            ALOGE("%s: invalid value %d out of (%d, %d)",
                  __func__, shaprness,
                  m_pCapability->sharpness_ctrl.min_value,
                  m_pCapability->sharpness_ctrl.max_value);
            return BAD_VALUE;
        }
    } else {
        CDBG("%s: No value change in shaprness", __func__);
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

    if(params.get(KEY_QC_SCE_FACTOR) == NULL) {
       CDBG_HIGH("%s: Skintone enhancement not set by App ",__func__);
       return NO_ERROR;
    }
    if (prev_sceFactor != sceFactor) {
        if((sceFactor >= m_pCapability->sce_ctrl.min_value) &&
           (sceFactor <= m_pCapability->sce_ctrl.max_value)) {
            CDBG(" new Skintone Enhancement value : %d ", sceFactor);
            return setSkinToneEnhancement(sceFactor);
        } else {
            ALOGE("%s: invalid value %d out of (%d, %d)",
                  __func__, sceFactor,
                  m_pCapability->sce_ctrl.min_value,
                  m_pCapability->sce_ctrl.max_value);
            return BAD_VALUE;
        }
    } else {
        CDBG("%s: No value change in skintone enhancement factor", __func__);
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

    if(params.get(KEY_QC_SATURATION) == NULL) {
       CDBG_HIGH("%s: Saturation not set by App ",__func__);
       return NO_ERROR;
    }
    if (prev_sat !=  saturation) {
        if((saturation >= m_pCapability->saturation_ctrl.min_value) &&
           (saturation <= m_pCapability->saturation_ctrl.max_value)) {
            CDBG(" new saturation value : %d ", saturation);
            return setSaturation(saturation);
        } else {
            ALOGE("%s: invalid value %d out of (%d, %d)",
                  __func__, saturation,
                  m_pCapability->saturation_ctrl.min_value,
                  m_pCapability->saturation_ctrl.max_value);
            return BAD_VALUE;
        }
    } else {
        CDBG("%s: No value change in saturation factor", __func__);
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

    if(params.get(KEY_QC_CONTRAST) == NULL) {
       CDBG_HIGH("%s: Contrast not set by App ",__func__);
       return NO_ERROR;
    }
    if (prev_contrast !=  contrast) {
        if((contrast >= m_pCapability->contrast_ctrl.min_value) &&
           (contrast <= m_pCapability->contrast_ctrl.max_value)) {
            CDBG(" new contrast value : %d ", contrast);
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
        CDBG("%s: No value change in contrast", __func__);
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

    if(params.get(KEY_EXPOSURE_COMPENSATION) == NULL) {
       CDBG_HIGH("%s: Exposure compensation not set by App ",__func__);
       return NO_ERROR;
    }
    if (prev_expComp != expComp) {
        if((expComp >= m_pCapability->exposure_compensation_min) &&
           (expComp <= m_pCapability->exposure_compensation_max)) {
            CDBG(" new Exposure Compensation value : %d ", expComp);
            return setExposureCompensation(expComp);
        } else {
            ALOGE("%s: invalid value %d out of (%d, %d)",
                  __func__, expComp,
                  m_pCapability->exposure_compensation_min,
                  m_pCapability->exposure_compensation_max);
            return BAD_VALUE;
        }
    } else {
        CDBG("%s: No value change in Exposure Compensation", __func__);
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
 * FUNCTION   : setWBManualCCT
 *
 * DESCRIPTION: set wb cct value from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t  QCameraParameters::setWBManualCCT(const QCameraParameters& params)
{
    const char *wb_str = params.get(KEY_WHITE_BALANCE);
    CDBG_HIGH("%s, current wb mode: %s", __func__, wb_str);

    if (wb_str != NULL) {
        if (strcmp(wb_str, WHITE_BALANCE_MANUAL_CCT)) {
            CDBG("%s, dont set cct to back-end.", __func__);
            return NO_ERROR;
        }
    }

    const char *str = params.get(KEY_QC_WB_MANUAL_CCT);
    const char *prev_str = get(KEY_QC_WB_MANUAL_CCT);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setWBManualCCT(str);
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
 * FUNCTION   : setAlgoOptimizationsMask
 *
 * DESCRIPTION: get the value from persist file in Stats module that will
 *              control funtionality in the module
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setAlgoOptimizationsMask()
{
    uint32_t mask = 0;
    char value[PROPERTY_VALUE_MAX];

    property_get("persist.camera.stats.opt.mask", value, "0");
    mask = (uint32_t)atoi(value);

    ALOGV("%s: algo opt ctrl mask :%d", __func__, mask);

    return AddSetParmEntryToBatch(m_pParamBuf,
                                  CAM_INTF_PARM_ALGO_OPTIMIZATIONS_MASK,
                                  sizeof(mask),
                                  &mask);
}

/*===========================================================================
 * FUNCTION   : setStatsDebugMask
 *
 * DESCRIPTION: get the value from persist file in Stats module that will
 *              control funtionality in the module
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setStatsDebugMask()
{
    uint32_t mask = 0;
    char value[PROPERTY_VALUE_MAX];

    property_get("persist.camera.stats.debug.mask", value, "0");
    mask = (uint32_t)atoi(value);

    CDBG("%s: ctrl mask :%d", __func__, mask);

    return AddSetParmEntryToBatch(m_pParamBuf,
                                  CAM_INTF_PARM_STATS_DEBUG_MASK,
                                  sizeof(mask),
                                  &mask);
}

/*===========================================================================
 * FUNCTION   : setISPDebugMask
 *
 * DESCRIPTION: get the value from persist file in ISP module that will
 *              control funtionality in the module
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setISPDebugMask()
{
    uint32_t mask = 0;
    char value[PROPERTY_VALUE_MAX];

    property_get("persist.camera.ISP.debug.mask", value, "0");
    mask = (uint32_t)atoi(value);
    ALOGV("%s: ctrl mask :%d", __func__, mask);

    return AddSetParmEntryToBatch(m_pParamBuf,
                                  CAM_INTF_PARM_ISP_DEBUG_MASK,
                                  sizeof(mask),
                                  &mask);
}

/*===========================================================================
 * FUNCTION   : setSensorDebugMask
 *
 * DESCRIPTION: get the value from persist file in Sensor module that will
 *              control logging in the module
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setSensorDebugMask()
{
    uint32_t mask = 0;
    char value[PROPERTY_VALUE_MAX];

    property_get("persist.camera.sensor.debug.mask", value, "0");
    mask = (uint32_t)atoi(value);
    ALOGV("%s: ctrl mask :%d", __func__, mask);

    return AddSetParmEntryToBatch(m_pParamBuf,
                                  CAM_INTF_PARM_SENSOR_DEBUG_MASK,
                                  sizeof(mask),
                                  &mask);
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
 * FUNCTION   : setVtEnable
 *
 * DESCRIPTION: set vt Time Stamp enable from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setVtEnable(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_VT_ENABLE);
    const char *prev_str = get(KEY_QC_VT_ENABLE);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setVtEnable(str);
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
            uint32_t maxFaces = (uint32_t)params.getInt(KEY_QC_MAX_NUM_REQUESTED_FACES);
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
        CDBG_HIGH("%s: no zoom support", __func__);
        return NO_ERROR;
    }

    int zoomLevel = params.getInt(KEY_ZOOM);
    if ((zoomLevel < 0) || (zoomLevel >= (int)m_pCapability->zoom_ratio_tbl_cnt)) {
        ALOGE("%s: invalid value %d out of (%d, %d)",
              __func__, zoomLevel,
              0, m_pCapability->zoom_ratio_tbl_cnt-1);
        return BAD_VALUE;
    }

    int prevZoomLevel = getInt(KEY_ZOOM);
    if (prevZoomLevel == zoomLevel) {
        CDBG("%s: No value change in contrast", __func__);
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
 * FUNCTION   : setExposureTime
 *
 * DESCRIPTION: set exposure time from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t  QCameraParameters::setExposureTime(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_EXPOSURE_TIME);
    const char *prev_str = get(KEY_QC_EXPOSURE_TIME);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setExposureTime(str);
        }
    }

    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setVideoRotation
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
int32_t QCameraParameters::setVideoRotation(const QCameraParameters& params)
{
    int rotation = params.getInt(KEY_QC_VIDEO_ROTATION);
    if (rotation != -1) {
        if (rotation == 0 || rotation == 90 ||
            rotation == 180 || rotation == 270) {
            set(KEY_QC_VIDEO_ROTATION, rotation);
        } else {
            ALOGE("Invalid rotation value: %d", rotation);
            return BAD_VALUE;
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
 * FUNCTION   : setAutoHDR
 *
 * DESCRIPTION: Enable/disable auto HDR
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setAutoHDR(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_AUTO_HDR_ENABLE);
    const char *prev_str = get(KEY_QC_AUTO_HDR_ENABLE);
    char prop[PROPERTY_VALUE_MAX];

    memset(prop, 0, sizeof(prop));
    property_get("persist.camera.auto.hdr.enable", prop, VALUE_DISABLE);
    if (str != NULL) {
       if (prev_str == NULL ||
           strcmp(str, prev_str) != 0) {
           CDBG("%s : Auto HDR set to: %s", __func__, str);
           return updateParamEntry(KEY_QC_AUTO_HDR_ENABLE, str);
       }
    } else {
       if (prev_str == NULL ||
           strcmp(prev_str, prop) != 0 ) {
           CDBG("%s : Auto HDR set to: %s", __func__, prop);
           updateParamEntry(KEY_QC_AUTO_HDR_ENABLE, prop);
       }
    }

       return NO_ERROR;
}

/*===========================================================================
* FUNCTION   : isAutoHDREnabled
*
* DESCRIPTION: Query auto HDR status
*
* PARAMETERS : None
*
* RETURN     : bool true/false
*==========================================================================*/
bool QCameraParameters::isAutoHDREnabled()
{
    const char *str = get(KEY_QC_AUTO_HDR_ENABLE);
    if (str != NULL) {
        int32_t value = lookupAttr(ENABLE_DISABLE_MODES_MAP,
                PARAM_MAP_SIZE(ENABLE_DISABLE_MODES_MAP), str);
        if (value == NAME_NOT_FOUND) {
            ALOGE("%s: Invalid Auto HDR value %s", __func__, str);
            return false;
        }

        CDBG_HIGH("%s : Auto HDR status is: %d", __func__, value);
        return value ? true : false;
    }

    CDBG_HIGH("%s : Auto HDR status not set!", __func__);
    return false;
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
    int rc = NO_ERROR;
    const char *str = params.get(KEY_SCENE_MODE);
    const char *prev_str = get(KEY_SCENE_MODE);
    CDBG("%s: str - %s, prev_str - %s",__func__, str, prev_str);

    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {

            if(strcmp(str, SCENE_MODE_AUTO) == 0) {
                m_bSceneTransitionAuto = true;
            }
            if (strcmp(str, SCENE_MODE_HDR) == 0) {
                // If HDR is set from client and the feature is
                // not enabled in the backend, ignore it.
                if (m_pCapability->qcom_supported_feature_mask &
                     CAM_QCOM_FEATURE_SENSOR_HDR) {
                    CDBG_HIGH("%s: Sensor HDR mode Enabled",__func__);
                    m_bSensorHDREnabled = true;
                    m_bHDREnabled = false;
                } else if (m_pCapability->qcom_supported_feature_mask &
                            CAM_QCOM_FEATURE_HDR) {
                    CDBG_HIGH("%s: S/W HDR Enabled",__func__);
                    m_bSensorHDREnabled = false;
                    m_bHDREnabled = true;
                } else {
                    m_bSensorHDREnabled = false;
                    m_bHDREnabled = false;
                    return NO_ERROR;
                }
            } else {
                m_bHDREnabled = false;
                if (m_bSensorHDREnabled) {
                    m_bSensorHDREnabled = false;
                    m_bNeedRestart = true;
                    setSensorSnapshotHDR(VALUE_OFF);
                }
            }
            if (m_bSensorHDREnabled) {
                setSensorSnapshotHDR(VALUE_ON);
                m_bNeedRestart = true;
            } else if ((m_bHDREnabled) ||
                ((prev_str != NULL) && (strcmp(prev_str, SCENE_MODE_HDR) == 0))) {
                CDBG_HIGH("%s: scene mode changed between HDR and non-HDR, need restart", __func__);
                m_bNeedRestart = true;
            }

            rc = setSceneMode(str);
        }
    }
    if (m_bHDREnabled) {
        str = params.get(KEY_QC_HDR_NEED_1X);
        prev_str = get(KEY_QC_HDR_NEED_1X);
        if (str != NULL) {
            if (prev_str == NULL ||
                strcmp(str, prev_str) != 0) {
                if (strcmp(str,VALUE_ON) == 0) {
                    m_bHDR1xFrameEnabled = true;
                }
                else {
                    m_bHDR1xFrameEnabled = false;
                }
            updateParamEntry(KEY_QC_HDR_NEED_1X, str);
            AddSetParmEntryToBatch(m_pParamBuf,
                                   CAM_INTF_PARM_HDR_NEED_1X,
                                   sizeof(m_bHDR1xFrameEnabled),
                                   &m_bHDR1xFrameEnabled);
            }
        }
    }
    return rc;
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
    if (isHDREnabled()) {
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
 * FUNCTION   : setAFBracket
 *
 * DESCRIPTION: set AF bracket from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setAFBracket(const QCameraParameters& params)
{
    if ((m_pCapability->qcom_supported_feature_mask &
        CAM_QCOM_FEATURE_UBIFOCUS) == 0){
        CDBG("%s: AF Bracketing is not supported",__func__);
        return NO_ERROR;
    }
    const char *str = params.get(KEY_QC_AF_BRACKET);
    const char *prev_str = get(KEY_QC_AF_BRACKET);
    CDBG_HIGH("%s: str =%s & prev_str =%s",__func__, str, prev_str);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            m_bNeedRestart = true;
            return setAFBracket(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setMultiTouchFocus
 *
 * DESCRIPTION: set multi-touch focus value
 *
 * PARAMETERS :
 *   @multiTouchFocusStr : multi-touch focus value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setMultiTouchFocus(const char *multiTouchFocusStr)
{
    if(multiTouchFocusStr != NULL) {
        int value = lookupAttr(MULTI_TOUCH_FOCUS_MODES_MAP,
                PARAM_MAP_SIZE(MULTI_TOUCH_FOCUS_MODES_MAP), multiTouchFocusStr);
        if(value != NAME_NOT_FOUND) {
            m_bMultiTouchFocusOn = (value != 0);
            if (!m_bMultiTouchFocusOn) {
                resetMultiTouchFocusParam();
            }
            updateParamEntry(KEY_QC_MULTI_TOUCH_FOCUS, multiTouchFocusStr);
            return NO_ERROR;
        }
    }
    CDBG_HIGH("Invalid multi-touch focus value: %s",
            (multiTouchFocusStr == NULL) ? "NULL" : multiTouchFocusStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setMultiTouchFocus
 *
 * DESCRIPTION: set multi-touch focus af bracket from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setMultiTouchFocus(const QCameraParameters& params)
{
    if ((m_pCapability->qcom_supported_feature_mask &
        CAM_QCOM_FEATURE_MULTI_TOUCH_FOCUS) == 0) {
        CDBG_HIGH("%s: multi-touch focus is not supported",__func__);
        return NO_ERROR;
    }
    const char *str = params.get(KEY_QC_MULTI_TOUCH_FOCUS);
    const char *prev_str = get(KEY_QC_MULTI_TOUCH_FOCUS);
    CDBG_HIGH("%s: str =%s & prev_str =%s",__func__, str, prev_str);
    if (str != NULL) {
        if (prev_str == NULL || strcmp(str, prev_str) != 0) {
            m_bNeedRestart = true;
            return setMultiTouchFocus(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setTouchAFAEC
 *
 * DESCRIPTION: set touch af aec value
 *
 * PARAMETERS :
 *   @touchAfAecStr : touch focus value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setTouchAFAEC(const char *touchAfAecStr)
{
    if (touchAfAecStr != NULL) {
        int value = lookupAttr(TOUCH_AF_AEC_MODES_MAP,
                PARAM_MAP_SIZE(TOUCH_AF_AEC_MODES_MAP), touchAfAecStr);
        if (value != NAME_NOT_FOUND) {
            m_bTouchFocusOn = (value != 0);
            updateParamEntry(KEY_QC_TOUCH_AF_AEC, touchAfAecStr);
            return NO_ERROR;
        }
    }
    CDBG_HIGH("Invalid touch af aec value: %s",
            (touchAfAecStr == NULL) ? "NULL" : touchAfAecStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setTouchAFAEC
 *
 * DESCRIPTION: set touch AF from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setTouchAFAEC(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_TOUCH_AF_AEC);
    const char *prev_str = get(KEY_QC_TOUCH_AF_AEC);
    CDBG("%s: str =%s & prev_str =%s",__func__, str, prev_str);
    if (str != NULL) {
        if (prev_str == NULL || strcmp(str, prev_str) != 0) {
            return setTouchAFAEC(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : updateMTFInfo
 *
 * DESCRIPTION: update lens position selected by user
 *
 * PARAMETERS :
 *   @lensPos : current lens position to add into array
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/

int32_t QCameraParameters::updateMTFInfo(const int32_t lensPos)
{
   CDBG_HIGH("%s: current lens position is: %d, burst count = %d",
           __func__, lensPos, m_MTFBracketInfo.burst_count);
   if (m_MTFBracketInfo.burst_count >= MAX_AF_BRACKETING_VALUES) {
       return BAD_VALUE;
   }
   if (m_MTFBracketInfo.burst_count == 0) {
       m_MTFBracketInfo.focus_steps[0] = lensPos;
   } else {
      for (int i = 0; i < m_MTFBracketInfo.burst_count; i++) {
         if (lensPos > m_MTFBracketInfo.focus_steps[i]) {
            for (int j = m_MTFBracketInfo.burst_count; j > i; j--) {
               m_MTFBracketInfo.focus_steps[j] = m_MTFBracketInfo.focus_steps[j-1];
            }
            m_MTFBracketInfo.focus_steps[i] = lensPos;
            break;
         }
      }
   }
   for (int i = 0; i < MAX_AF_BRACKETING_VALUES; i++) {
      CDBG_HIGH("%s: current focus_step[%d] = %d", __func__, i,
          m_MTFBracketInfo.focus_steps[i]);
   }

   m_MTFBracketInfo.burst_count++;
   return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setChromaFlash
 *
 * DESCRIPTION: set chroma flash from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setChromaFlash(const QCameraParameters& params)
{
    if ((m_pCapability->qcom_supported_feature_mask &
        CAM_QCOM_FEATURE_CHROMA_FLASH) == 0) {
        CDBG("%s: Chroma Flash is not supported",__func__);
        return NO_ERROR;
    }
    const char *str = params.get(KEY_QC_CHROMA_FLASH);
    const char *prev_str = get(KEY_QC_CHROMA_FLASH);
    CDBG_HIGH("%s: str =%s & prev_str =%s",__func__, str, prev_str);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            m_bNeedRestart = true;
            return setChromaFlash(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setOptiZoom
 *
 * DESCRIPTION: set opti zoom from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setOptiZoom(const QCameraParameters& params)
{
    if ((m_pCapability->qcom_supported_feature_mask &
        CAM_QCOM_FEATURE_OPTIZOOM) == 0){
        CDBG("%s: Opti Zoom is not supported",__func__);
        return NO_ERROR;
    }
    const char *str = params.get(KEY_QC_OPTI_ZOOM);
    const char *prev_str = get(KEY_QC_OPTI_ZOOM);
    CDBG_HIGH("%s: str =%s & prev_str =%s",__func__, str, prev_str);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            m_bNeedRestart = true;
            return setOptiZoom(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setTruePortrait
 *
 * DESCRIPTION: set true portrait from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setTruePortrait(const QCameraParameters& params)
{
    if ((m_pCapability->qcom_supported_feature_mask &
        CAM_QCOM_FEATURE_TRUEPORTRAIT) == 0){
        CDBG("%s: True Portrait is not supported",__func__);
        return NO_ERROR;
    }
    const char *str = params.get(KEY_QC_TRUE_PORTRAIT);
    const char *prev_str = get(KEY_QC_TRUE_PORTRAIT);
    CDBG_HIGH("%s: str =%s & prev_str =%s",__func__, str, prev_str);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            m_bNeedRestart = true;
            return setTruePortrait(str);
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setSeeMore
 *
 * DESCRIPTION: set see more (llvd) from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setSeeMore(const QCameraParameters& params)
{
    if ((m_pCapability->qcom_supported_feature_mask &
          CAM_QCOM_FEATURE_LLVD) == 0){
      CDBG("%s: See more is not supported",__func__);
      return NO_ERROR;
    }
    const char *str = params.get(KEY_QC_SEE_MORE);
    const char *prev_str = get(KEY_QC_SEE_MORE);
    CDBG_HIGH("%s: str =%s & prev_str =%s",__func__, str, prev_str);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            m_bNeedRestart = true;
            return setSeeMore(str);
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
 * FUNCTION   : setFssr
 *
 * DESCRIPTION: set fssr from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setFssr(const QCameraParameters& params)
{
    if ((m_pCapability->qcom_supported_feature_mask &
        CAM_QCOM_FEATURE_FSSR) == 0) {
        CDBG_HIGH("%s: FSSR is not supported",__func__);
        return NO_ERROR;
    }
    const char *str = params.get(KEY_QC_FSSR);
    const char *prev_str = get(KEY_QC_FSSR);
    CDBG_HIGH("%s: str =%s & prev_str =%s",__func__, str, prev_str);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            m_bNeedRestart = true;
            return setFssr(str);
        }
    }
    return NO_ERROR;
}


/*===========================================================================
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
    int nExpnum = 0;

    const char *bracket_str = get(KEY_QC_AE_BRACKET_HDR);
    if (bracket_str != NULL && strlen(bracket_str) > 0) {
        int value = lookupAttr(BRACKETING_MODES_MAP, PARAM_MAP_SIZE(BRACKETING_MODES_MAP),
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
            nExpnum = 1 + getNumOfExtraHDROutBufsIfNeeded();
            break;
        }
    }

    CDBG_HIGH("%s: nBurstNum = %d, nExpnum = %d", __func__, nBurstNum, nExpnum);
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
            int32_t value = lookupAttr(TRUE_FALSE_MODES_MAP, PARAM_MAP_SIZE(TRUE_FALSE_MODES_MAP),
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
    CDBG_HIGH("Param m_bNoDisplayMode = %d", m_bNoDisplayMode);
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
            int32_t value = lookupAttr(ON_OFF_MODES_MAP, PARAM_MAP_SIZE(ON_OFF_MODES_MAP),
                    str_val);
            if (value != NAME_NOT_FOUND) {
                set(KEY_QC_ZSL, str_val);
                m_bZslMode_new = (value > 0)? true : false;

                // ZSL mode changed, need restart preview
                m_bNeedRestart = true;
                CDBG_HIGH("%s: ZSL Mode  -> %s", __func__, m_bZslMode_new ? "Enabled" : "Disabled");

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
    char prop[PROPERTY_VALUE_MAX];

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
        CDBG_HIGH("%s: flip is not supported.", __func__);
        return NO_ERROR;
    }

    //check preview flip setting
    const char *str = params.get(KEY_QC_PREVIEW_FLIP);
    const char *prev_val = get(KEY_QC_PREVIEW_FLIP);
    if(str != NULL){
        if (prev_val == NULL || strcmp(str, prev_val) != 0) {
            int32_t value = lookupAttr(FLIP_MODES_MAP, PARAM_MAP_SIZE(FLIP_MODES_MAP), str);
            if(value != NAME_NOT_FOUND){
                set(KEY_QC_PREVIEW_FLIP, str);
                m_bPreviewFlipChanged = true;
            }
        }
    }

    // check video filp setting
    str = params.get(KEY_QC_VIDEO_FLIP);
    prev_val = get(KEY_QC_VIDEO_FLIP);
    if(str != NULL){
        if (prev_val == NULL || strcmp(str, prev_val) != 0) {
            int32_t value = lookupAttr(FLIP_MODES_MAP, PARAM_MAP_SIZE(FLIP_MODES_MAP), str);
            if(value != NAME_NOT_FOUND){
                set(KEY_QC_VIDEO_FLIP, str);
                m_bVideoFlipChanged = true;
            }
        }
    }

    // check picture filp setting
    str = params.get(KEY_QC_SNAPSHOT_PICTURE_FLIP);
    prev_val = get(KEY_QC_SNAPSHOT_PICTURE_FLIP);
    if(str != NULL){
        if (prev_val == NULL || strcmp(str, prev_val) != 0) {
            int32_t value = lookupAttr(FLIP_MODES_MAP, PARAM_MAP_SIZE(FLIP_MODES_MAP), str);
            if(value != NAME_NOT_FOUND){
                set(KEY_QC_SNAPSHOT_PICTURE_FLIP, str);
                m_bSnapshotFlipChanged = true;
            }
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
        if (!isAdvCamFeaturesEnabled()) {
            // if burst number is not set in parameters,
            // read from sys prop
            char prop[PROPERTY_VALUE_MAX];
            memset(prop, 0, sizeof(prop));
            property_get("persist.camera.snapshot.number", prop, "0");
            nBurstNum = atoi(prop);
        } else {
            nBurstNum = 1;
        }
        if (nBurstNum <= 0) {
            nBurstNum = 1;
        }
    } else {
        set(KEY_QC_SNAPSHOT_BURST_NUM, nBurstNum);
    }
    m_nBurstNum = (uint8_t)nBurstNum;
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setSnapshotFDReq
 *
 * DESCRIPTION: set requirement of Face Detection Metadata in Snapshot mode.
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setSnapshotFDReq(const QCameraParameters& params)
{
    char prop[PROPERTY_VALUE_MAX];
    const char *str = params.get(KEY_QC_SNAPSHOT_FD_DATA);

    if(str != NULL){
        set(KEY_QC_SNAPSHOT_FD_DATA, str);
    }else{
        memset(prop, 0, sizeof(prop));
        property_get("persist.camera.snapshot.fd", prop, "0");
        set(KEY_QC_SNAPSHOT_FD_DATA, prop);
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setMobicat
 *
 * DESCRIPTION: set Mobicat on/off.
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setMobicat(const QCameraParameters& )
{
    char value [PROPERTY_VALUE_MAX];
    property_get("persist.camera.mobicat", value, "0");
    uint32_t enableMobi = (uint32_t) atoi(value);
    int32_t ret = NO_ERROR;;

    if (enableMobi) {
        tune_cmd_t tune_cmd;
        tune_cmd.type = 2;
        tune_cmd.module = 0;
        tune_cmd.value = 1;

        ret = AddSetParmEntryToBatch(m_pParamBuf,
                                CAM_INTF_PARM_SET_VFE_COMMAND,
                                sizeof(tune_cmd_t),
                                &tune_cmd);
        if (NO_ERROR != ret) {
            return ret;
        }
        tune_cmd.module = 0;

        ret = AddSetParmEntryToBatch(m_pParamBuf,
                                CAM_INTF_PARM_SET_PP_COMMAND,
                                sizeof(tune_cmd_t),
                                &tune_cmd);
    }
    if (NO_ERROR != ret) {
        return ret;
    }
    ret = AddSetParmEntryToBatch(m_pParamBuf,
                                CAM_INTF_PARM_AF_MOBICAT_CMD,
                                sizeof(enableMobi),
                                &enableMobi);
    m_bMobiMask = (uint8_t)enableMobi;
    return ret;
}

/*===========================================================================
 * FUNCTION   : setLongshotParam
 *
 * DESCRIPTION: set Longshot on/off.
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setLongshotParam(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_LONG_SHOT);
    const char *prev_str = get(KEY_QC_LONG_SHOT);

    if (str != NULL) {
        if (prev_str == NULL || strcmp(str, prev_str) != 0) {
            set(KEY_QC_LONG_SHOT, str);
            if (!strcmp(str, "off")) {
                if (m_bLongshotEnabled == true) {
                    // We restart here, to reset the FPS and no
                    // of buffers as per the requirement of single snapshot usecase.
                    m_bNeedRestart = true;
                }
                m_bLongshotEnabled = false;
            }
        }
    }

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
    if ((rc = setJpegQuality(params)))                  final_rc = rc;
    if ((rc = setOrientation(params)))                  final_rc = rc;
    if ((rc = setRotation(params)))                     final_rc = rc;
    if ((rc = setVideoRotation(params)))                final_rc = rc;
    if ((rc = setNoDisplayMode(params)))                final_rc = rc;
    if ((rc = setZslMode(params)))                      final_rc = rc;
    if ((rc = setZslAttributes(params)))                final_rc = rc;
    if ((rc = setCameraMode(params)))                   final_rc = rc;
    if ((rc = setRecordingHint(params)))                final_rc = rc;

    if ((rc = setPreviewFrameRate(params)))             final_rc = rc;
    if ((rc = setPreviewFpsRange(params)))              final_rc = rc;
    if ((rc = setAutoExposure(params)))                 final_rc = rc;
    if ((rc = setEffect(params)))                       final_rc = rc;
    if ((rc = setBrightness(params)))                   final_rc = rc;
    if ((rc = setZoom(params)))                         final_rc = rc;
    if ((rc = setSharpness(params)))                    final_rc = rc;
    if ((rc = setSaturation(params)))                   final_rc = rc;
    if ((rc = setContrast(params)))                     final_rc = rc;
    if ((rc = setFocusMode(params)))                    final_rc = rc;
    if ((rc = setISOValue(params)))                     final_rc = rc;
    if ((rc = setExposureTime(params)))                 final_rc = rc;
    if ((rc = setSkinToneEnhancement(params)))          final_rc = rc;
    if ((rc = setFlash(params)))                        final_rc = rc;
    if ((rc = setAecLock(params)))                      final_rc = rc;
    if ((rc = setAwbLock(params)))                      final_rc = rc;
    if ((rc = setLensShadeValue(params)))               final_rc = rc;
    if ((rc = setMCEValue(params)))                     final_rc = rc;
    if ((rc = setDISValue(params)))                     final_rc = rc;
    if ((rc = setAntibanding(params)))                  final_rc = rc;
    if ((rc = setExposureCompensation(params)))         final_rc = rc;
    if ((rc = setWhiteBalance(params)))                 final_rc = rc;
    if ((rc = setWBManualCCT(params)))                  final_rc = rc;
    if ((rc = setSceneMode(params)))                    final_rc = rc;
    if ((rc = setFocusAreas(params)))                   final_rc = rc;
    if ((rc = setFocusPosition(params)))                final_rc = rc;
    if ((rc = setMeteringAreas(params)))                final_rc = rc;
    if ((rc = setSelectableZoneAf(params)))             final_rc = rc;
    if ((rc = setRedeyeReduction(params)))              final_rc = rc;
    if ((rc = setAEBracket(params)))                    final_rc = rc;
    if ((rc = setAutoHDR(params)))                      final_rc = rc;
    if ((rc = setGpsLocation(params)))                  final_rc = rc;
    if ((rc = setWaveletDenoise(params)))               final_rc = rc;
    if ((rc = setFaceRecognition(params)))              final_rc = rc;
    if ((rc = setFlip(params)))                         final_rc = rc;
    if ((rc = setVideoHDR(params)))                     final_rc = rc;
    if ((rc = setVtEnable(params)))                     final_rc = rc;
    if ((rc = setBurstNum(params)))                     final_rc = rc;
    if ((rc = setSnapshotFDReq(params)))                final_rc = rc;
    if ((rc = setTintlessValue(params)))                final_rc = rc;
    if ((rc = setCDSMode(params)))                      final_rc = rc;

    // update live snapshot size after all other parameters are set
    if ((rc = setLiveSnapshotSize(params)))             final_rc = rc;
    if ((rc = setJpegThumbnailSize(params)))            final_rc = rc;
    if ((rc = setStatsDebugMask()))                     final_rc = rc;
    if ((rc = setISPDebugMask()))                       final_rc = rc;
    if ((rc = setAlgoOptimizationsMask()))              final_rc = rc;
    if ((rc = setMobicat(params)))                      final_rc = rc;
    if ((rc = setAFBracket(params)))                    final_rc = rc;
    if ((rc = setChromaFlash(params)))                  final_rc = rc;
    if ((rc = setOptiZoom(params)))                     final_rc = rc;
    if ((rc = setFssr(params)))                         final_rc = rc;
    if ((rc = setSeeMore(params)))                      final_rc = rc;
    if ((rc = setMultiTouchFocus(params)))              final_rc = rc;
    if ((rc = setTouchAFAEC(params)))                   final_rc = rc;
    if ((rc = setLongshotParam(params)))                final_rc = rc;
    if ((rc = setTruePortrait(params)))                 final_rc = rc;

    if ((rc = updateFlash(false)))                      final_rc = rc;
    if ((rc = setSensorDebugMask()))                    final_rc = rc;
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
    int32_t hal_version = CAM_HAL_V1;
    AddSetParmEntryToBatch(m_pParamBuf,
                           CAM_INTF_PARM_HAL_VERSION,
                           sizeof(hal_version),
                           &hal_version);

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
            (int)m_pCapability->qcom_supported_feature_mask);
    set(KEY_MAX_NUM_DETECTED_FACES_HW, m_pCapability->max_num_roi);
    set(KEY_MAX_NUM_DETECTED_FACES_SW, m_pCapability->max_num_roi);
    set(KEY_QC_MAX_NUM_REQUESTED_FACES, m_pCapability->max_num_roi);
    // Set focal length, horizontal view angle, and vertical view angle
    setFloat(KEY_FOCAL_LENGTH, m_pCapability->focal_length);
    setFloat(KEY_HORIZONTAL_VIEW_ANGLE, m_pCapability->hor_view_angle);
    setFloat(KEY_VERTICAL_VIEW_ANGLE, m_pCapability->ver_view_angle);
    set(QCameraParameters::KEY_FOCUS_DISTANCES, "Infinity,Infinity,Infinity");
    set(KEY_QC_AUTO_HDR_SUPPORTED,
        (m_pCapability->auto_hdr_supported)? VALUE_TRUE : VALUE_FALSE);
    // Set supported preview sizes
    if (m_pCapability->preview_sizes_tbl_cnt > 0 &&
        m_pCapability->preview_sizes_tbl_cnt <= MAX_SIZES_CNT) {
        String8 previewSizeValues = createSizesString(
                m_pCapability->preview_sizes_tbl, m_pCapability->preview_sizes_tbl_cnt);
        set(KEY_SUPPORTED_PREVIEW_SIZES, previewSizeValues.string());
        CDBG_HIGH("%s: supported preview sizes: %s", __func__, previewSizeValues.string());
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
        CDBG_HIGH("%s: supported video sizes: %s", __func__, videoSizeValues.string());
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
        CDBG_HIGH("%s: supported pic sizes: %s", __func__, pictureSizeValues.string());
        // Set default picture size to the smallest resolution
        CameraParameters::setPictureSize(
           m_pCapability->picture_sizes_tbl[m_pCapability->picture_sizes_tbl_cnt-1].width,
           m_pCapability->picture_sizes_tbl[m_pCapability->picture_sizes_tbl_cnt-1].height);
    } else {
        ALOGE("%s: supported picture sizes cnt is 0 or exceeds max!!!", __func__);
    }

    // Need check if scale should be enabled
    if (m_pCapability->scale_picture_sizes_cnt > 0 &&
        m_pCapability->scale_picture_sizes_cnt <= MAX_SCALE_SIZES_CNT){
        //get scale size, enable scaling. And re-set picture size table with scale sizes
        m_reprocScaleParam.setScaleEnable(true);
        int rc_s = m_reprocScaleParam.setScaleSizeTbl(
            m_pCapability->scale_picture_sizes_cnt, m_pCapability->scale_picture_sizes,
            m_pCapability->picture_sizes_tbl_cnt, m_pCapability->picture_sizes_tbl);
        if(rc_s == NO_ERROR){
            String8 scaledPictureSizeValues = createSizesString(
                m_pCapability->scale_picture_sizes, m_pCapability->scale_picture_sizes_cnt);
            set(KEY_QC_SCALED_PICTURE_SIZES, scaledPictureSizeValues.string());
            ALOGE("%s: scaled supported pic sizes: %s", __func__, scaledPictureSizeValues.string());
        }else{
            m_reprocScaleParam.setScaleEnable(false);
            ALOGE("%s: reset scaled picture size table failed.", __func__);
        }
    }else{
        m_reprocScaleParam.setScaleEnable(false);
    }

    // Set supported thumbnail sizes
    String8 thumbnailSizeValues = createSizesString(
            THUMBNAIL_SIZES_MAP,
            PARAM_MAP_SIZE(THUMBNAIL_SIZES_MAP));
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
        CDBG("%s: supported live snapshot sizes: %s", __func__, liveSnpashotSizeValues.string());
        m_LiveSnapshotSize =
            m_pCapability->livesnapshot_sizes_tbl[m_pCapability->livesnapshot_sizes_tbl_cnt-1];
    }

    // Set supported preview formats
    String8 previewFormatValues = createValuesString(
            m_pCapability->supported_preview_fmts,
            m_pCapability->supported_preview_fmt_cnt,
            PREVIEW_FORMATS_MAP,
            PARAM_MAP_SIZE(PREVIEW_FORMATS_MAP));
    set(KEY_SUPPORTED_PREVIEW_FORMATS, previewFormatValues.string());
    // Set default preview format
    CameraParameters::setPreviewFormat(PIXEL_FORMAT_YUV420SP);

    // Set default Video Format
    set(KEY_VIDEO_FRAME_FORMAT, PIXEL_FORMAT_YUV420SP);

    // Set supported picture formats
    String8 pictureTypeValues(PIXEL_FORMAT_JPEG);
    String8 str = createValuesString(
            m_pCapability->supported_raw_fmts,
            m_pCapability->supported_raw_fmt_cnt,
            PICTURE_TYPES_MAP,
            PARAM_MAP_SIZE(PICTURE_TYPES_MAP));
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
        CDBG_HIGH("%s: supported fps ranges: %s", __func__, fpsRangeValues.string());

        int min_fps =
            int(m_pCapability->fps_ranges_tbl[default_fps_index].min_fps * 1000);
        int max_fps =
            int(m_pCapability->fps_ranges_tbl[default_fps_index].max_fps * 1000);
        m_default_fps_range = m_pCapability->fps_ranges_tbl[default_fps_index];
        //Set video fps same as preview fps
        setPreviewFpsRange(min_fps, max_fps, min_fps, max_fps);

        // Set legacy preview fps
        String8 fpsValues = createFpsString(m_pCapability->fps_ranges_tbl[default_fps_index]);
        set(KEY_SUPPORTED_PREVIEW_FRAME_RATES, fpsValues.string());
        CDBG_HIGH("%s: supported fps rates: %s", __func__, fpsValues.string());
        CameraParameters::setPreviewFrameRate(int(m_pCapability->fps_ranges_tbl[default_fps_index].max_fps));
    } else {
        ALOGE("%s: supported fps ranges cnt is 0 or exceeds max!!!", __func__);
    }

    // Set supported focus modes
    if (m_pCapability->supported_focus_modes_cnt > 0) {
        String8 focusModeValues = createValuesString(
                m_pCapability->supported_focus_modes,
                m_pCapability->supported_focus_modes_cnt,
                FOCUS_MODES_MAP,
                PARAM_MAP_SIZE(FOCUS_MODES_MAP));
        set(KEY_SUPPORTED_FOCUS_MODES, focusModeValues);

        // Set default focus mode and update corresponding parameter buf
        const char *focusMode = lookupNameByValue(FOCUS_MODES_MAP,
                PARAM_MAP_SIZE(FOCUS_MODES_MAP),
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

    // set focus position, we should get them from m_pCapability
    m_pCapability->min_focus_pos[CAM_MANUAL_FOCUS_MODE_INDEX] = 40;
    m_pCapability->max_focus_pos[CAM_MANUAL_FOCUS_MODE_INDEX] = 60;
    set(KEY_QC_MIN_FOCUS_POS_INDEX, m_pCapability->min_focus_pos[CAM_MANUAL_FOCUS_MODE_INDEX]);
    set(KEY_QC_MAX_FOCUS_POS_INDEX, m_pCapability->max_focus_pos[CAM_MANUAL_FOCUS_MODE_INDEX]);

    m_pCapability->min_focus_pos[CAM_MANUAL_FOCUS_MODE_DAC_CODE] = 0;
    m_pCapability->max_focus_pos[CAM_MANUAL_FOCUS_MODE_DAC_CODE] = 1023;
    set(KEY_QC_MIN_FOCUS_POS_DAC, m_pCapability->min_focus_pos[CAM_MANUAL_FOCUS_MODE_DAC_CODE]);
    set(KEY_QC_MIN_FOCUS_POS_DAC, m_pCapability->max_focus_pos[CAM_MANUAL_FOCUS_MODE_DAC_CODE]);

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
            m_pCapability->supported_aec_modes,
            m_pCapability->supported_aec_modes_cnt,
            AUTO_EXPOSURE_MAP,
            PARAM_MAP_SIZE(AUTO_EXPOSURE_MAP));
    set(KEY_QC_SUPPORTED_AUTO_EXPOSURE, autoExposureValues.string());
    setAutoExposure(AUTO_EXPOSURE_FRAME_AVG);

    // Set Exposure Compensation
    set(KEY_MAX_EXPOSURE_COMPENSATION, m_pCapability->exposure_compensation_max); // 12
    set(KEY_MIN_EXPOSURE_COMPENSATION, m_pCapability->exposure_compensation_min); // -12
    setFloat(KEY_EXPOSURE_COMPENSATION_STEP, m_pCapability->exposure_compensation_step); // 1/6
    setExposureCompensation(m_pCapability->exposure_compensation_default); // 0

    // Set Antibanding
    String8 antibandingValues = createValuesString(
            m_pCapability->supported_antibandings,
            m_pCapability->supported_antibandings_cnt,
            ANTIBANDING_MODES_MAP,
            PARAM_MAP_SIZE(ANTIBANDING_MODES_MAP));
    set(KEY_SUPPORTED_ANTIBANDING, antibandingValues);
    setAntibanding(ANTIBANDING_OFF);

    // Set Effect
    String8 effectValues = createValuesString(
            m_pCapability->supported_effects,
            m_pCapability->supported_effects_cnt,
            EFFECT_MODES_MAP,
            PARAM_MAP_SIZE(EFFECT_MODES_MAP));

    if (m_pCapability->supported_effects_cnt > 0) {
        set(KEY_SUPPORTED_EFFECTS, effectValues);
    } else {
        ALOGE("Color effects are not available");
        set(KEY_SUPPORTED_EFFECTS, EFFECT_NONE);
    }
    setEffect(EFFECT_NONE);

    // Set WhiteBalance
    String8 whitebalanceValues = createValuesString(
            m_pCapability->supported_white_balances,
            m_pCapability->supported_white_balances_cnt,
            WHITE_BALANCE_MODES_MAP,
            PARAM_MAP_SIZE(WHITE_BALANCE_MODES_MAP));
    set(KEY_SUPPORTED_WHITE_BALANCE, whitebalanceValues);
    setWhiteBalance(WHITE_BALANCE_AUTO);

    // set supported wb cct, we should get them from m_pCapability
    m_pCapability->min_wb_cct = 2000;
    m_pCapability->max_wb_cct = 8000;
    set(KEY_QC_MIN_WB_CCT, m_pCapability->min_wb_cct);
    set(KEY_QC_MAX_WB_CCT, m_pCapability->max_wb_cct);

    // Set Flash mode
    if(m_pCapability->supported_flash_modes_cnt > 0) {
       String8 flashValues = createValuesString(
               m_pCapability->supported_flash_modes,
               m_pCapability->supported_flash_modes_cnt,
               FLASH_MODES_MAP,
               PARAM_MAP_SIZE(FLASH_MODES_MAP));
       set(KEY_SUPPORTED_FLASH_MODES, flashValues);
       setFlash(FLASH_MODE_OFF);
    } else {
        ALOGE("%s: supported flash modes cnt is 0!!!", __func__);
    }

    // Set Scene Mode
    String8 sceneModeValues = createValuesString(
            m_pCapability->supported_scene_modes,
            m_pCapability->supported_scene_modes_cnt,
            SCENE_MODES_MAP,
            PARAM_MAP_SIZE(SCENE_MODES_MAP));
    set(KEY_SUPPORTED_SCENE_MODES, sceneModeValues);
    setSceneMode(SCENE_MODE_AUTO);

    // Set ISO Mode
    String8 isoValues = createValuesString(
            m_pCapability->supported_iso_modes,
            m_pCapability->supported_iso_modes_cnt,
            ISO_MODES_MAP,
            PARAM_MAP_SIZE(ISO_MODES_MAP));
    set(KEY_QC_SUPPORTED_ISO_MODES, isoValues);
    setISOValue(ISO_AUTO);

    // Set exposure time, we should get them from m_pCapability
    m_pCapability->min_exposure_time = 200;
    m_pCapability->max_exposure_time = 2000000;
    set(KEY_QC_MIN_EXPOSURE_TIME, m_pCapability->min_exposure_time);
    set(KEY_QC_MAX_EXPOSURE_TIME, m_pCapability->max_exposure_time);
    //setExposureTime("0");

    // Set HFR
    String8 hfrValues = createHfrValuesString(
            m_pCapability->hfr_tbl,
            m_pCapability->hfr_tbl_cnt,
            HFR_MODES_MAP,
            PARAM_MAP_SIZE(HFR_MODES_MAP));
    set(KEY_QC_SUPPORTED_VIDEO_HIGH_FRAME_RATE_MODES, hfrValues.string());
    set(KEY_QC_VIDEO_HIGH_SPEED_RECORDING, "off");
    set(KEY_QC_VIDEO_HIGH_FRAME_RATE, "off");
    String8 hfrSizeValues = createHfrSizesString(
            m_pCapability->hfr_tbl,
            m_pCapability->hfr_tbl_cnt);
    set(KEY_QC_SUPPORTED_HFR_SIZES, hfrSizeValues.string());
    setHighFrameRate(CAM_HFR_MODE_OFF);

    // Set Focus algorithms
    String8 focusAlgoValues = createValuesString(
            m_pCapability->supported_focus_algos,
            m_pCapability->supported_focus_algos_cnt,
            FOCUS_ALGO_MAP,
            PARAM_MAP_SIZE(FOCUS_ALGO_MAP));
    set(KEY_QC_SUPPORTED_FOCUS_ALGOS, focusAlgoValues);
    setSelectableZoneAf(FOCUS_ALGO_AUTO);

    // Set Zoom Ratios
    if (m_pCapability->zoom_supported > 0) {
        String8 zoomRatioValues = createZoomRatioValuesString(
                m_pCapability->zoom_ratio_tbl,
                m_pCapability->zoom_ratio_tbl_cnt);
        set(KEY_ZOOM_RATIOS, zoomRatioValues);
        set(KEY_MAX_ZOOM, (int)(m_pCapability->zoom_ratio_tbl_cnt - 1));
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
            PARAM_MAP_SIZE(BRACKETING_MODES_MAP));
    set(KEY_QC_SUPPORTED_AE_BRACKET_MODES, bracketingValues);
    setAEBracket(AE_BRACKET_OFF);

    //Set AF Bracketing.
    for (size_t i = 0; i < m_pCapability->supported_focus_modes_cnt; i++) {
        if ((CAM_FOCUS_MODE_AUTO == m_pCapability->supported_focus_modes[i]) &&
                ((m_pCapability->qcom_supported_feature_mask &
                        CAM_QCOM_FEATURE_UBIFOCUS) > 0)) {
            String8 afBracketingValues = createValuesStringFromMap(
                    AF_BRACKETING_MODES_MAP,
                    PARAM_MAP_SIZE(AF_BRACKETING_MODES_MAP));
            set(KEY_QC_SUPPORTED_AF_BRACKET_MODES, afBracketingValues);
            setAFBracket(AF_BRACKET_OFF);
         }
    }

    //Set Chroma Flash.
    if ((m_pCapability->supported_flash_modes_cnt > 0) &&
            (m_pCapability->qcom_supported_feature_mask &
            CAM_QCOM_FEATURE_CHROMA_FLASH) > 0) {
        String8 chromaFlashValues = createValuesStringFromMap(
                CHROMA_FLASH_MODES_MAP,
                PARAM_MAP_SIZE(CHROMA_FLASH_MODES_MAP));
        set(KEY_QC_SUPPORTED_CHROMA_FLASH_MODES, chromaFlashValues);
        setChromaFlash(CHROMA_FLASH_OFF);
    }

    //Set Opti Zoom.
    if (m_pCapability->zoom_supported &&
            (m_pCapability->qcom_supported_feature_mask &
            CAM_QCOM_FEATURE_OPTIZOOM) > 0){
        String8 optiZoomValues = createValuesStringFromMap(
                OPTI_ZOOM_MODES_MAP,
                PARAM_MAP_SIZE(OPTI_ZOOM_MODES_MAP));
        set(KEY_QC_SUPPORTED_OPTI_ZOOM_MODES, optiZoomValues);
        setOptiZoom(OPTI_ZOOM_OFF);
    }

    //Set True Portrait
    if ((m_pCapability->qcom_supported_feature_mask & CAM_QCOM_FEATURE_TRUEPORTRAIT) > 0) {
        String8 truePortraitValues = createValuesStringFromMap(
                TRUE_PORTRAIT_MODES_MAP,
                PARAM_MAP_SIZE(TRUE_PORTRAIT_MODES_MAP));
        set(KEY_QC_SUPPORTED_TRUE_PORTRAIT_MODES, truePortraitValues);
    }

   //Set FSSR.
    if ((m_pCapability->qcom_supported_feature_mask &
            CAM_QCOM_FEATURE_FSSR) > 0) {
        String8 fssrValues = createValuesStringFromMap(FSSR_MODES_MAP,
                PARAM_MAP_SIZE(FSSR_MODES_MAP));
        set(KEY_QC_SUPPORTED_FSSR_MODES, fssrValues);
        setFssr(FSSR_OFF);
    }

    //Set Multi-touch Focus.
    if ((m_pCapability->qcom_supported_feature_mask &
            CAM_QCOM_FEATURE_MULTI_TOUCH_FOCUS) > 0){
        String8 multiTouchFocusValues = createValuesStringFromMap(MULTI_TOUCH_FOCUS_MODES_MAP,
                PARAM_MAP_SIZE(MULTI_TOUCH_FOCUS_MODES_MAP));
        set(KEY_QC_SUPPORTED_MULTI_TOUCH_FOCUS_MODES, multiTouchFocusValues);
        setMultiTouchFocus(MULTI_TOUCH_FOCUS_OFF);
    }

    // Set Denoise
    if ((m_pCapability->qcom_supported_feature_mask & CAM_QCOM_FEATURE_DENOISE2D) > 0){
    String8 denoiseValues = createValuesStringFromMap(
        DENOISE_ON_OFF_MODES_MAP, PARAM_MAP_SIZE(DENOISE_ON_OFF_MODES_MAP));
    set(KEY_QC_SUPPORTED_DENOISE, denoiseValues.string());
#ifdef DEFAULT_DENOISE_MODE_ON
    setWaveletDenoise(DENOISE_ON);
#else
    setWaveletDenoise(DENOISE_OFF);
#endif
    }

    // Set feature enable/disable
    String8 enableDisableValues = createValuesStringFromMap(
            ENABLE_DISABLE_MODES_MAP, PARAM_MAP_SIZE(ENABLE_DISABLE_MODES_MAP));

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
    set(KEY_QC_SUPPORTED_HISTOGRAM_MODES,
        m_pCapability->histogram_supported ? enableDisableValues : "");
    set(KEY_QC_HISTOGRAM, VALUE_DISABLE);

    //Set Red Eye Reduction
    set(KEY_QC_SUPPORTED_REDEYE_REDUCTION, enableDisableValues);
    setRedeyeReduction(VALUE_DISABLE);

    //Set SkinTone Enhancement
    set(KEY_QC_SUPPORTED_SKIN_TONE_ENHANCEMENT_MODES, enableDisableValues);

    // Set feature on/off
    String8 onOffValues = createValuesStringFromMap(
            ON_OFF_MODES_MAP, PARAM_MAP_SIZE(ON_OFF_MODES_MAP));

    //Set See more (LLVD)
    if (m_pCapability->qcom_supported_feature_mask &
            CAM_QCOM_FEATURE_LLVD) {
        set(KEY_QC_SUPPORTED_SEE_MORE_MODES, onOffValues);
        setSeeMore(VALUE_OFF);
    }

    //Set Scene Detection
    set(KEY_QC_SUPPORTED_SCENE_DETECT, onOffValues);
    setSceneDetect(VALUE_OFF);
    m_bHDREnabled = false;
    m_bHDR1xFrameEnabled = false;

    m_bHDRThumbnailProcessNeeded = false;
    m_bHDR1xExtraBufferNeeded = true;
    for (uint32_t i=0; i<m_pCapability->hdr_bracketing_setting.num_frames; i++) {
        if (0 == m_pCapability->hdr_bracketing_setting.exp_val.values[i]) {
            m_bHDR1xExtraBufferNeeded = false;
            break;
        }
    }

    // Set HDR output scaling
    char value[PROPERTY_VALUE_MAX];

    property_get("persist.camera.hdr.outcrop", value, VALUE_DISABLE);
    if (strncmp(VALUE_ENABLE, value, sizeof(VALUE_ENABLE))) {
      m_bHDROutputCropEnabled = false;
    } else {
      m_bHDROutputCropEnabled = true;
    }

    //Set Face Detection
    set(KEY_QC_SUPPORTED_FACE_DETECTION, onOffValues);
    set(KEY_QC_FACE_DETECTION, VALUE_OFF);

    //Set Face Recognition
    //set(KEY_QC_SUPPORTED_FACE_RECOGNITION, onOffValues);
    //set(KEY_QC_FACE_RECOGNITION, VALUE_OFF);

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

    //Set HW Sensor Snapshot HDR
    if ((m_pCapability->qcom_supported_feature_mask & CAM_QCOM_FEATURE_SENSOR_HDR)> 0) {
        set(KEY_QC_SUPPORTED_SENSOR_HDR_MODES, onOffValues);
        set(KEY_QC_SENSOR_HDR, VALUE_OFF);
        m_bSensorHDREnabled = false;
    }

    // Set VT TimeStamp
    set(KEY_QC_VT_ENABLE, VALUE_DISABLE);
    //Set Touch AF/AEC
    String8 touchValues = createValuesStringFromMap(
            TOUCH_AF_AEC_MODES_MAP, PARAM_MAP_SIZE(TOUCH_AF_AEC_MODES_MAP));

    set(KEY_QC_SUPPORTED_TOUCH_AF_AEC, touchValues);
    set(KEY_QC_TOUCH_AF_AEC, TOUCH_AF_AEC_OFF);

    //set flip mode
    if ((m_pCapability->qcom_supported_feature_mask & CAM_QCOM_FEATURE_FLIP) > 0) {
        String8 flipModes = createValuesStringFromMap(
                FLIP_MODES_MAP, PARAM_MAP_SIZE(FLIP_MODES_MAP));
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

    // Add support for internal preview restart
    set(KEY_INTERNAL_PERVIEW_RESTART, VALUE_TRUE);

    // Set default longshot mode
    set(KEY_QC_LONG_SHOT, "off");

    //Get RAM size and disable features which are memory rich
    struct sysinfo info;
    sysinfo(&info);

    CDBG_HIGH("%s: totalram = %ld, freeram = %ld ", __func__, info.totalram,
        info.freeram);
    if (info.totalram > TOTAL_RAM_SIZE_512MB) {
        set(KEY_QC_LONGSHOT_SUPPORTED, VALUE_TRUE);
        set(KEY_QC_ZSL_HDR_SUPPORTED, VALUE_TRUE);
    } else {
        set(KEY_QC_LONGSHOT_SUPPORTED, VALUE_FALSE);
        set(KEY_QC_ZSL_HDR_SUPPORTED, VALUE_FALSE);
    }

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
int32_t QCameraParameters::init(cam_capability_t *capabilities,
                                mm_camera_vtbl_t *mmOps,
                                QCameraAdjustFPS *adjustFPS,
                                QCameraTorchInterface *torch)
{
    int32_t rc = NO_ERROR;

    m_pCapability = capabilities;
    m_pCamOpsTbl = mmOps;
    m_AdjustFPS = adjustFPS;
    m_pTorch = torch;

    //Allocate Set Param Buffer
    m_pParamHeap = new QCameraHeapMemory(QCAMERA_ION_USE_CACHE);
    rc = m_pParamHeap->allocate(1, ONE_MB_OF_PARAMS);
    if(rc != OK) {
        rc = NO_MEMORY;
        ALOGE("Failed to allocate SETPARM Heap memory");
        goto TRANS_INIT_ERROR1;
    }

    //Map memory for parameters buffer
    rc = m_pCamOpsTbl->ops->map_buf(m_pCamOpsTbl->camera_handle,
                             CAM_MAPPING_BUF_TYPE_PARM_BUF,
                             m_pParamHeap->getFd(0),
                             ONE_MB_OF_PARAMS);
    if(rc < 0) {
        ALOGE("%s:failed to map SETPARM buffer",__func__);
        rc = FAILED_TRANSACTION;
        goto TRANS_INIT_ERROR2;
    }
    m_pParamBuf = (parm_buffer_new_t*) DATA_PTR(m_pParamHeap,0);

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

    m_AdjustFPS = NULL;

    m_tempMap.clear();

    m_bInited = false;
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
int32_t QCameraParameters::setPreviewFpsRange(int min_fps,
        int max_fps, int vid_min_fps,int vid_max_fps)
{
    char str[32];
    char value[PROPERTY_VALUE_MAX];
    int fixedFpsValue;
    /*This property get value should be the fps that user needs*/
    property_get("persist.debug.set.fixedfps", value, "0");
    fixedFpsValue = atoi(value);

    CDBG("%s: E minFps = %d, maxFps = %d , vid minFps = %d, vid maxFps = %d",
                __func__, min_fps, max_fps, vid_min_fps, vid_max_fps);

    if(fixedFpsValue != 0) {
      min_fps = (int)fixedFpsValue*1000;
      max_fps = (int)fixedFpsValue*1000;
    }
    snprintf(str, sizeof(str), "%d,%d", min_fps, max_fps);
    CDBG("%s: Setting preview fps range %s", __func__, str);
    updateParamEntry(KEY_PREVIEW_FPS_RANGE, str);
    cam_fps_range_t fps_range;
    memset(&fps_range, 0x00, sizeof(cam_fps_range_t));
    fps_range.min_fps = (float)min_fps / 1000.0f;
    fps_range.max_fps = (float)max_fps / 1000.0f;
    fps_range.video_min_fps = (float)vid_min_fps / 1000.0f;
    fps_range.video_max_fps = (float)vid_max_fps / 1000.0f;

    CDBG_HIGH("%s: Updated: minFps = %d, maxFps = %d ,"
            " vid minFps = %d, vid maxFps = %d",
            __func__, min_fps, max_fps, vid_min_fps, vid_max_fps);

    if ( NULL != m_AdjustFPS ) {
        m_AdjustFPS->recalcFPSRange(min_fps, max_fps, fps_range);
        CDBG_HIGH("%s: Thermal adjusted Preview fps range %3.2f,%3.2f, %3.2f, %3.2f",
              __func__, fps_range.min_fps, fps_range.max_fps,
              fps_range.video_min_fps, fps_range.video_max_fps);
    }

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
        int32_t value = lookupAttr(AUTO_EXPOSURE_MAP, PARAM_MAP_SIZE(AUTO_EXPOSURE_MAP), autoExp);
        if (value != NAME_NOT_FOUND) {
            CDBG("%s: Setting auto exposure %s", __func__, autoExp);
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
        int32_t value = lookupAttr(EFFECT_MODES_MAP, PARAM_MAP_SIZE(EFFECT_MODES_MAP), effect);
        if (value != NAME_NOT_FOUND) {
            CDBG("%s: Setting effect %s", __func__, effect);
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
    CDBG("%s: Setting brightness %s", __func__, val);
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
    int32_t rc;
    if (focusMode != NULL) {
        int32_t value = lookupAttr(FOCUS_MODES_MAP, PARAM_MAP_SIZE(FOCUS_MODES_MAP), focusMode);
        if (value != NAME_NOT_FOUND) {
            CDBG_HIGH("%s: Setting focus mode %s", __func__, focusMode);
            mFocusMode = (cam_focus_mode_type)value;

            updateParamEntry(KEY_FOCUS_MODE, focusMode);
            rc = AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_FOCUS_MODE,
                                          sizeof(value),
                                          &value);
            if (strcmp(focusMode,"infinity")==0){
                set(QCameraParameters::KEY_FOCUS_DISTANCES, "Infinity,Infinity,Infinity");
            }
            return rc;
        }
    }
    ALOGE("Invalid focus mode value: %s", (focusMode == NULL) ? "NULL" : focusMode);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setFocusPosition
 *
 * DESCRIPTION: set focus position
 *
 * PARAMETERS :
 *   @typeStr : focus position type, index or dac_code
 *   @posStr : focus positon.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t  QCameraParameters::setFocusPosition(const char *typeStr, const char *posStr)
{
    CDBG_HIGH("%s, type:%s, pos: %s", __func__, typeStr, posStr);
    int32_t type = atoi(typeStr);
    int32_t pos  = atoi(posStr);

    if ((type >= CAM_MANUAL_FOCUS_MODE_INDEX) &&
        (type < CAM_MANUAL_FOCUS_MODE_MAX)) {

        // get max and min focus position from m_pCapability
        int32_t minFocusPos = m_pCapability->min_focus_pos[type];
        int32_t maxFocusPos = m_pCapability->max_focus_pos[type];
        CDBG_HIGH("%s, focusPos min: %d, max: %d", __func__, minFocusPos, maxFocusPos);

        if (pos >= minFocusPos && pos <= maxFocusPos) {
            m_curFocusPos = pos;
            updateParamEntry(KEY_QC_MANUAL_FOCUS_POS_TYPE, typeStr);
            updateParamEntry(KEY_QC_MANUAL_FOCUS_POSITION, posStr);

            cam_manual_focus_parm_t manual_focus;
            manual_focus.flag = (cam_manual_focus_mode_type)type;
            manual_focus.af_manual_lens_position = pos;
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_MANUAL_FOCUS_POS,
                                          sizeof(manual_focus),
                                          &manual_focus);
        }
    }

    ALOGE("%s, invalid params, type:%d, pos: %d", __func__, type, pos);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : updateCurrentFocusPosition
 *
 * DESCRIPTION: update current focus position from metadata callback
 *
 * PARAMETERS :
 *   @pos : current focus position
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t  QCameraParameters::updateCurrentFocusPosition(int32_t pos)
{
    if (pos != m_curFocusPos) {
        ALOGE("update focus position. old:%d, now:%d", m_curFocusPos, pos);
        m_curFocusPos = pos;
        set(KEY_QC_MANUAL_FOCUS_POSITION, pos);
    }

    return NO_ERROR;
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
    CDBG("%s: Setting sharpness %s", __func__, val);

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
    CDBG("%s: Setting skintone enhancement %s", __func__, val);

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
    CDBG("%s: Setting saturation %s", __func__, val);

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
    CDBG("%s: Setting contrast %s", __func__, val);

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
        int32_t value = lookupAttr(ON_OFF_MODES_MAP, PARAM_MAP_SIZE(ON_OFF_MODES_MAP),
                sceneDetect);
        if (value != NAME_NOT_FOUND) {
            CDBG("%s: Setting Scene Detect %s", __func__, sceneDetect);
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
 * FUNCTION   : setSensorSnapshotHDR
 *
 * DESCRIPTION: set snapshot HDR value
 *
 * PARAMETERS :
 *   @snapshotHDR  : snapshot HDR value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setSensorSnapshotHDR(const char *snapshotHDR)
{
    if (snapshotHDR != NULL) {
        int32_t value = lookupAttr(ON_OFF_MODES_MAP, PARAM_MAP_SIZE(ON_OFF_MODES_MAP),
                snapshotHDR);
        if (value != NAME_NOT_FOUND) {
            CDBG("%s: Setting Sensor Snapshot HDR %s", __func__, snapshotHDR);
            updateParamEntry(KEY_QC_SENSOR_HDR, snapshotHDR);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_SENSOR_HDR,
                                          sizeof(value),
                                          &value);
        }
    }
    CDBG_HIGH("Invalid Snapshot HDR value: %s",
          (snapshotHDR == NULL) ? "NULL" : snapshotHDR);
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
        int32_t value = lookupAttr(ON_OFF_MODES_MAP, PARAM_MAP_SIZE(ON_OFF_MODES_MAP), videoHDR);
        if (value != NAME_NOT_FOUND) {
            CDBG_HIGH("%s: Setting Video HDR %s", __func__, videoHDR);
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
 * FUNCTION   : setVtEnable
 *
 * DESCRIPTION: set vt Enable value
 *
 * PARAMETERS :
 *   @videoHDR  : svtEnable value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setVtEnable(const char *vtEnable)
{
    if (vtEnable != NULL) {
        int32_t value = lookupAttr(ENABLE_DISABLE_MODES_MAP,
                PARAM_MAP_SIZE(ENABLE_DISABLE_MODES_MAP), vtEnable);
        if (value != NAME_NOT_FOUND) {
            CDBG_HIGH("%s: Setting Vt Enable %s", __func__, vtEnable);
            m_bAVTimerEnabled = true;
            updateParamEntry(KEY_QC_VT_ENABLE, vtEnable);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_VT,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("Invalid Vt Enable value: %s",
          (vtEnable == NULL) ? "NULL" : vtEnable);
    m_bAVTimerEnabled = false;
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
int32_t QCameraParameters::setFaceRecognition(const char *faceRecog,
        uint32_t maxFaces)
{
    if (faceRecog != NULL) {
        int32_t value = lookupAttr(ON_OFF_MODES_MAP, PARAM_MAP_SIZE(ON_OFF_MODES_MAP), faceRecog);
        if (value != NAME_NOT_FOUND) {
            CDBG_HIGH("%s: Setting face recognition %s", __func__, faceRecog);
            updateParamEntry(KEY_QC_FACE_RECOGNITION, faceRecog);

            uint32_t faceProcMask = m_nFaceProcMask;
            if (value > 0) {
                faceProcMask |= CAM_FACE_PROCESS_MASK_RECOGNITION;
            } else {
                faceProcMask &= (uint32_t)(~CAM_FACE_PROCESS_MASK_RECOGNITION);
            }

            if(m_nFaceProcMask == faceProcMask) {
                CDBG_HIGH("%s: face process mask not changed, no ops here", __func__);
                return NO_ERROR;
            }
            m_nFaceProcMask = faceProcMask;
            CDBG_HIGH("%s: FaceProcMask -> %d", __func__, m_nFaceProcMask);

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
    char iso[PROPERTY_VALUE_MAX];
    int32_t continous_iso = 0;
    // Check if continuous ISO is set
    property_get("persist.camera.continuous.iso", iso, "0");
    continous_iso = atoi(iso);

    if(continous_iso != 0) {
        CDBG("%s: Setting continuous ISO value %d", __func__, continous_iso);
        return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_ISO,
                                          sizeof(continous_iso),
                                          &continous_iso);
    } else if (isoValue != NULL) {
        int32_t value = lookupAttr(ISO_MODES_MAP, PARAM_MAP_SIZE(ISO_MODES_MAP), isoValue);
        if (value != NAME_NOT_FOUND) {
            CDBG("%s: Setting ISO value %s", __func__, isoValue);
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
 * FUNCTION   : setExposureTime
 *
 * DESCRIPTION: set exposure time
 *
 * PARAMETERS :
 *   @expTimeStr : string of exposure time, range (1/5, 2000) in ms
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t  QCameraParameters::setExposureTime(const char *expTimeStr)
{
    if (expTimeStr != NULL) {
        int32_t expTimeUs = atoi(expTimeStr);
        int32_t min_exp_time = m_pCapability->min_exposure_time; /* 200 */
        int32_t max_exp_time = m_pCapability->max_exposure_time; /* 2000000 */

        // expTime == 0 means not to use manual exposure time.
        if (expTimeUs == 0 ||
            (expTimeUs >= min_exp_time && expTimeUs <= max_exp_time)) {
            CDBG_HIGH("%s, exposure time: %d", __func__, expTimeUs);
            updateParamEntry(KEY_QC_EXPOSURE_TIME, expTimeStr);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_EXPOSURE_TIME,
                                          sizeof(expTimeUs),
                                          &expTimeUs);
        }
    }

    ALOGE("Invalid exposure time, value: %s",
          (expTimeStr == NULL) ? "NULL" : expTimeStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : getFlashValue
 *
 * DESCRIPTION: get fash mode
 *
 * PARAMETERS :
 *   @flashStr : none
 *
 * RETURN     : int32_t type of status
 *
 *==========================================================================*/
int32_t QCameraParameters::getFlashValue()
{
    const char *flash_str = get(QCameraParameters::KEY_FLASH_MODE);
    int32_t flash_index = lookupAttr(FLASH_MODES_MAP, PARAM_MAP_SIZE(FLASH_MODES_MAP),
            flash_str);

  return flash_index;
}


/*===========================================================================
 * FUNCTION   : getSupportedFlashModes
 *
 * DESCRIPTION: get supported fash modes
 *
 * PARAMETERS :
 *   @flash_modes : none
 *
 * RETURN     : int32_t type of status
 *
 *==========================================================================*/
int32_t QCameraParameters::getSupportedFlashModes()
{
  const char *flash_modes = get(QCameraParameters::KEY_SUPPORTED_FLASH_MODES);
  int flash_presence = -1;

  if (flash_modes != NULL)
    flash_presence = FLASH_FUNC_PRESENT;
  else
    flash_presence = NO_FLASH_FUNC;

  return flash_presence;
}

/*===========================================================================
 * FUNCTION   : getRedEyeValue
 *
 * DESCRIPTION: get redeye mode
 *
 * PARAMETERS :
 *   @redeyeStr : none
 *
 * RETURN     : int32_t type of status
 *
 *==========================================================================*/
int32_t QCameraParameters::getRedEyeValue()
{
  const char *redEye_str = get(QCameraParameters::KEY_QC_REDEYE_REDUCTION);
  int32_t redEye = lookupAttr(ENABLE_DISABLE_MODES_MAP, PARAM_MAP_SIZE(ENABLE_DISABLE_MODES_MAP),
          redEye_str);

  return redEye;
}

/*===========================================================================
 * FUNCTION   : setLongshotEnable
 *
 * DESCRIPTION: set a flag indicating longshot mode
 *
 * PARAMETERS :
 *   @enable  : true - Longshot enabled
 *              false - Longshot disabled
 *==========================================================================*/
int32_t QCameraParameters::setLongshotEnable(bool enable)
{
    int32_t rc = NO_ERROR;
    int8_t value = enable;

    if(initBatchUpdate(m_pParamBuf) < 0 ) {
        ALOGE("%s:Failed to initialize group update table", __func__);
        return BAD_TYPE;
    }

    rc = AddSetParmEntryToBatch(m_pParamBuf,
          CAM_INTF_PARM_LONGSHOT_ENABLE,
          sizeof(value),
          &value);
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to update table", __func__);
        return rc;
    }

    rc = commitSetBatch();
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to parameter changes", __func__);
        return rc;
    }

    if (enable == true) m_bLongshotEnabled = enable;

    return rc;
}

/*===========================================================================
 * FUNCTION   : setFlash
 *
 * DESCRIPTION: set flash mode
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
        int32_t value = lookupAttr(FLASH_MODES_MAP, PARAM_MAP_SIZE(FLASH_MODES_MAP), flashStr);
        if (value != NAME_NOT_FOUND) {
            CDBG_HIGH("%s: Setting Flash value %s", __func__, flashStr);

            if ( NULL != m_pTorch ) {
                if ( value == CAM_FLASH_MODE_TORCH && !m_bRecordingHint_new) {
                    m_pTorch->prepareTorchCamera();
                } else {
                    m_bReleaseTorchCamera = true;
                }
            }

            updateParamEntry(KEY_FLASH_MODE, flashStr);
            mFlashValue = value;
            return NO_ERROR;
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
        int32_t value = lookupAttr(TRUE_FALSE_MODES_MAP, PARAM_MAP_SIZE(TRUE_FALSE_MODES_MAP),
                aecLockStr);
        if (value != NAME_NOT_FOUND) {
            CDBG("%s: Setting AECLock value %s", __func__, aecLockStr);
            updateParamEntry(KEY_AUTO_EXPOSURE_LOCK, aecLockStr);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_AEC_LOCK,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("Invalid AECLock value: %s",
        (aecLockStr == NULL) ? "NULL" : aecLockStr);
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
        int32_t value = lookupAttr(TRUE_FALSE_MODES_MAP, PARAM_MAP_SIZE(TRUE_FALSE_MODES_MAP),
                awbLockStr);
        if (value != NAME_NOT_FOUND) {
            CDBG("%s: Setting AWBLock value %s", __func__, awbLockStr);
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
                PARAM_MAP_SIZE(ENABLE_DISABLE_MODES_MAP), mceStr);
        if (value != NAME_NOT_FOUND) {
            CDBG("%s: Setting AWBLock value %s", __func__, mceStr);
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
 * FUNCTION   : setTintlessValue
 *
 * DESCRIPTION: enable/disable tintless from user setting
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setTintlessValue(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_TINTLESS_ENABLE);
    const char *prev_str = get(KEY_QC_TINTLESS_ENABLE);
    char prop[PROPERTY_VALUE_MAX];

    memset(prop, 0, sizeof(prop));
    property_get("persist.camera.tintless", prop, VALUE_DISABLE);
    if (str != NULL) {
        if (prev_str == NULL ||
            strcmp(str, prev_str) != 0) {
            return setTintlessValue(str);
        }
    } else {
        if (prev_str == NULL ||
            strcmp(prev_str, prop) != 0 ) {
            setTintlessValue(prop);
        }
    }

    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setTintlessValue
 *
 * DESCRIPTION: set tintless value
 *
 * PARAMETERS :
 *   @tintStr : Tintless value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setTintlessValue(const char *tintStr)
{
    if (tintStr != NULL) {
        int32_t value = lookupAttr(ENABLE_DISABLE_MODES_MAP,
                PARAM_MAP_SIZE(ENABLE_DISABLE_MODES_MAP), tintStr);
        if (value != NAME_NOT_FOUND) {
            CDBG("%s: Setting Tintless value %s", __func__, tintStr);
            updateParamEntry(KEY_QC_TINTLESS_ENABLE, tintStr);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_TINTLESS,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("Invalid Tintless value: %s", (tintStr == NULL) ? "NULL" : tintStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setCDSMode
 *
 * DESCRIPTION: Set CDS mode
 *
 * PARAMETERS :
 *   @params  : user setting parameters
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setCDSMode(const QCameraParameters& params)
{
    const char *str = params.get(KEY_QC_CDS_MODE);
    const char *prev_str = get(KEY_QC_CDS_MODE);
    char *cds_mode_str = NULL;
    int32_t rc = NO_ERROR;
    char prop[PROPERTY_VALUE_MAX];

    if (str) {
        if (!prev_str || !strcmp(str, prev_str)) {
            cds_mode_str = (char *)str;
        }
    } else {
        memset(prop, 0, sizeof(prop));
        property_get("persist.camera.CDS", prop, CDS_MODE_OFF);
        cds_mode_str = prop;
    }

    if (cds_mode_str) {
        CDBG("%s: Set CDS mode = %s", __func__, cds_mode_str);
        int32_t cds_mode = lookupAttr(CDS_MODES_MAP, PARAM_MAP_SIZE(CDS_MODES_MAP), cds_mode_str);
        if (NAME_NOT_FOUND != cds_mode) {
            rc = AddSetParmEntryToBatch(m_pParamBuf, CAM_INTF_PARM_CDS_MODE,
                    sizeof(cds_mode), &cds_mode);
            if (rc != NO_ERROR) {
                ALOGE("%s:Failed CDS MODE to update table", __func__);
            }
        } else {
            ALOGE("%s: Invalid argument for CDS MODE %s", __func__,  cds_mode_str);
            rc = BAD_VALUE;
        }
    }
    return rc;
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
                PARAM_MAP_SIZE(ENABLE_DISABLE_MODES_MAP), disStr);
        if (value != NAME_NOT_FOUND) {
            //For some IS types (like EIS 2.0), when DIS value is changed, we need to restart
            //preview because of topology change in backend. But, for now, restart preview
            //for all IS types.
            m_bNeedRestart = true;
            CDBG_HIGH("%s: Setting DIS value %s", __func__, disStr);
            updateParamEntry(KEY_QC_DIS, disStr);
            if (!(strcmp(disStr,"enable"))) {
                m_bDISEnabled = true;
            } else {
                m_bDISEnabled = false;
            }
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_DIS_ENABLE,
                                          sizeof(value),
                                          &value);
        }
    }
    ALOGE("Invalid DIS value: %s", (disStr == NULL) ? "NULL" : disStr);
    m_bDISEnabled = false;
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setHighFrameRate
 *
 * DESCRIPTION: set high frame rate
 *
 * PARAMETERS :
 *   @hfrMode : HFR mode
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setHighFrameRate(const int32_t hfrMode)
{
    int32_t value = hfrMode;
    return AddSetParmEntryToBatch(m_pParamBuf,
                                  CAM_INTF_PARM_HFR,
                                  sizeof(value),
                                  &value);
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
                PARAM_MAP_SIZE(ENABLE_DISABLE_MODES_MAP), lensShadeStr);
        if (value != NAME_NOT_FOUND) {
            CDBG("%s: Setting LensShade value %s", __func__, lensShadeStr);
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
                PARAM_MAP_SIZE(WHITE_BALANCE_MODES_MAP), wbStr);
        if (value != NAME_NOT_FOUND) {
            CDBG("%s: Setting WhiteBalance value %s", __func__, wbStr);
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
 * FUNCTION   : setWBManualCCT
 *
 * DESCRIPTION: set setWBManualCCT time
 *
 * PARAMETERS :
 *   @cctStr : string of wb cct, range (2000, 8000) in K.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t  QCameraParameters::setWBManualCCT(const char *cctStr)
{
    if (cctStr != NULL) {
        int32_t cctVal = atoi(cctStr);
        int32_t minCct = m_pCapability->min_wb_cct; /* 2000K */
        int32_t maxCct = m_pCapability->max_wb_cct; /* 8000K */

        if (cctVal >= minCct && cctVal <= maxCct) {
            CDBG_HIGH("%s, cct value: %d", __func__, cctVal);
            m_curCCT = cctVal;
            updateParamEntry(KEY_QC_WB_MANUAL_CCT, cctStr);
            return AddSetParmEntryToBatch(m_pParamBuf,
                                          CAM_INTF_PARM_WHITE_BALANCE,
                                          sizeof(cctVal),
                                          &cctVal);
        }
    }

    ALOGE("Invalid cct, value: %s",
          (cctStr == NULL) ? "NULL" : cctStr);
    return BAD_VALUE;
}

int32_t QCameraParameters::updateCCTValue(int32_t cct)
{
    if (cct != m_curCCT) {
        CDBG_HIGH("update current cct value. old:%d, now:%d", m_curCCT, cct);
        m_curCCT = cct;
        set(KEY_QC_WB_MANUAL_CCT, cct);
    }

    return NO_ERROR;
}

int QCameraParameters::getAutoFlickerMode()
{
    /* Enable Advanced Auto Antibanding where we can set
       any of the following option
       ie. CAM_ANTIBANDING_MODE_AUTO
           CAM_ANTIBANDING_MODE_AUTO_50HZ
           CAM_ANTIBANDING_MODE_AUTO_60HZ
      Currently setting it to default    */
    char prop[PROPERTY_VALUE_MAX];
    memset(prop, 0, sizeof(prop));
    property_get("persist.camera.set.afd", prop, "3");
    return atoi(prop);
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
        int32_t value = lookupAttr(ANTIBANDING_MODES_MAP, PARAM_MAP_SIZE(ANTIBANDING_MODES_MAP),
                antiBandingStr);
        if (value != NAME_NOT_FOUND) {
            CDBG("%s: Setting AntiBanding value %s", __func__, antiBandingStr);
            updateParamEntry(KEY_ANTIBANDING, antiBandingStr);
            if(value == CAM_ANTIBANDING_MODE_AUTO) {
               value = getAutoFlickerMode();
            }
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
        CDBG("%s: Parameter string is null", __func__);
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
    af_roi_value.num_roi = (uint8_t)num_areas_found;
    for (int i = 0; i < num_areas_found; i++) {
        CDBG_HIGH("%s: FocusArea[%d] = (%d, %d, %d, %d)",
              __func__, i, (areas[i].rect.top), (areas[i].rect.left),
              (areas[i].rect.width), (areas[i].rect.height));

        // Transform the coords from (-1000, 1000)
        // to (0, previewWidth or previewHeight).
        af_roi_value.roi[i].left =
                (int32_t)(((double)areas[i].rect.left + 1000.0) *
                    ((double)previewWidth / 2000.0));
        af_roi_value.roi[i].top =
                (int32_t)(((double)areas[i].rect.top + 1000.0) *
                    ((double)previewHeight / 2000.0));
        af_roi_value.roi[i].width =
                (int32_t)((double)areas[i].rect.width *
                    (double)previewWidth / 2000.0);
        af_roi_value.roi[i].height =
                (int32_t)((double)areas[i].rect.height *
                    (double)previewHeight / 2000.0);
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
        CDBG("%s: Parameter string is null", __func__);
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
            CDBG_HIGH("%s: MeteringArea[%d] = (%d, %d, %d, %d)",
                  __func__, i, (areas[i].rect.top), (areas[i].rect.left),
                  (areas[i].rect.width), (areas[i].rect.height));

            // Transform the coords from (-1000, 1000) to
            // (0, previewWidth or previewHeight).
            aec_roi_value.cam_aec_roi_position.coordinate[i].x =
                    (uint32_t)((((double)areas[i].rect.left +
                        (double)areas[i].rect.width / 2.0) + 1000.0) *
                            (double)previewWidth / 2000.0);
            aec_roi_value.cam_aec_roi_position.coordinate[i].y =
                    (uint32_t)((((double)areas[i].rect.top +
                        (double)areas[i].rect.height / 2.0) + 1000.0) *
                            (double)previewHeight / 2000.0);
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
        int32_t value = lookupAttr(SCENE_MODES_MAP, PARAM_MAP_SIZE(SCENE_MODES_MAP), sceneModeStr);
        if (value != NAME_NOT_FOUND) {
            CDBG_HIGH("%s: Setting SceneMode %s", __func__, sceneModeStr);
            updateParamEntry(KEY_SCENE_MODE, sceneModeStr);
            if (m_bSensorHDREnabled) {
              // Incase of HW HDR mode, we do not update the same as Best shot mode.
              CDBG("%s: H/W HDR mode enabled. Do not set Best Shot Mode", __func__);
              return NO_ERROR;
            }
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
        int32_t value = lookupAttr(FOCUS_ALGO_MAP, PARAM_MAP_SIZE(FOCUS_ALGO_MAP), selZoneAFStr);
        if (value != NAME_NOT_FOUND) {
            CDBG("%s: Setting Selectable Zone AF value %s", __func__, selZoneAFStr);
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
 * FUNCTION   : isAEBracketEnabled
 *
 * DESCRIPTION: checks if AE bracketing is enabled
 *
 * PARAMETERS :
 *
 * RETURN     : TRUE/FALSE
 *==========================================================================*/
bool QCameraParameters::isAEBracketEnabled()
{
    const char *str = get(KEY_QC_AE_BRACKET_HDR);
    if (str != NULL) {
        if (strcmp(str, AE_BRACKET_OFF) != 0) {
            return true;
        }
    }
    return false;
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
        CDBG("%s: setAEBracket with NULL value", __func__);
        return NO_ERROR;
    }

    cam_exp_bracketing_t expBracket;
    memset(&expBracket, 0, sizeof(expBracket));

    int value = lookupAttr(BRACKETING_MODES_MAP, PARAM_MAP_SIZE(BRACKETING_MODES_MAP),
            aecBracketStr);
    switch (value) {
    case CAM_EXP_BRACKETING_ON:
        {
            CDBG_HIGH("%s, EXP_BRACKETING_ON", __func__);
            const char *str_val = get(KEY_QC_CAPTURE_BURST_EXPOSURE);
            if ((str_val != NULL) && (strlen(str_val)>0)) {
                expBracket.mode = CAM_EXP_BRACKETING_ON;
                m_bAeBracketingEnabled = true;
                strlcpy(expBracket.values, str_val, MAX_EXP_BRACKETING_LENGTH);
                CDBG("%s: setting Exposure Bracketing value of %s",
                      __func__, expBracket.values);
            }
            else {
                /* Apps not set capture-burst-exposures, error case fall into bracketing off mode */
                CDBG("%s: capture-burst-exposures not set, back to HDR OFF mode", __func__);
                m_bAeBracketingEnabled = false;
                expBracket.mode = CAM_EXP_BRACKETING_OFF;
            }
        }
        break;
    default:
        {
            CDBG_HIGH("%s, EXP_BRACKETING_OFF", __func__);
            m_bAeBracketingEnabled = false;
            expBracket.mode = CAM_EXP_BRACKETING_OFF;
        }
        break;
    }

    // Cache client AE bracketing configuration
    memcpy(&m_AEBracketingClient, &expBracket, sizeof(cam_exp_bracketing_t));

    /* save the value*/
    updateParamEntry(KEY_QC_AE_BRACKET_HDR, aecBracketStr);
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : set3ALock
 *
 * DESCRIPTION: enable/disable 3A lock.
 *
 * PARAMETERS :
 *   @lockStr : lock value string.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::set3ALock(const char *lockStr)
{
    int32_t rc = NO_ERROR;
    if (lockStr != NULL) {
        int value = lookupAttr(TRUE_FALSE_MODES_MAP, PARAM_MAP_SIZE(TRUE_FALSE_MODES_MAP),
                lockStr);
        if (value != NAME_NOT_FOUND) {
            CDBG_HIGH("%s: Setting Lock lockStr =%s", __func__, lockStr);
            if(initBatchUpdate(m_pParamBuf) < 0 ) {
                ALOGE("%s:Failed to initialize group update table", __func__);
                return BAD_TYPE;
            }
            int32_t focus_mode;
            if (value == 1) {
                if (isUbiFocusEnabled() || isMultiTouchFocusEnabled()) {
                    //For Ubi focus and Multi-touch Focus move focus to infinity.
                    focus_mode = CAM_FOCUS_MODE_INFINITY;
                } else if (isOptiZoomEnabled() || isfssrEnabled()){
                    //For optizoom set focus as fixed.
                    focus_mode = CAM_FOCUS_MODE_FIXED;
                }
            } else {
                // retrieve previous focus value.
                const char *focus = get(KEY_FOCUS_MODE);
                int val = lookupAttr(FOCUS_MODES_MAP, PARAM_MAP_SIZE(FOCUS_MODES_MAP), focus);
                if (val != NAME_NOT_FOUND) {
                    focus_mode = (int32_t) val;
                    CDBG("%s: focus mode %s", __func__, focus);
                }
            }
            //Lock AWB
            rc = AddSetParmEntryToBatch(m_pParamBuf,
                                    CAM_INTF_PARM_AWB_LOCK,
                                    sizeof(value),
                                    &value);
            if (NO_ERROR != rc) {
                return rc;
            }
            //Lock AEC
            rc = AddSetParmEntryToBatch(m_pParamBuf,
                                    CAM_INTF_PARM_AEC_LOCK,
                                    sizeof(value),
                                    &value);
            if (NO_ERROR != rc) {
                return rc;
            }
            rc = AddSetParmEntryToBatch(m_pParamBuf,
                                    CAM_INTF_PARM_FOCUS_MODE,
                                    sizeof(focus_mode),
                                    &focus_mode);
            if (NO_ERROR != rc) {
                return rc;
            }

            rc = commitSetBatch();
            if (rc != NO_ERROR) {
                ALOGE("%s:Failed to commit batch", __func__);
            }
        }
    }
    return rc;
}

/*===========================================================================
 * FUNCTION   : setAndCommitZoom
 *
 * DESCRIPTION: set zoom.
 *
 * PARAMETERS :
 *     @zoom_level : zoom level to set.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setAndCommitZoom(int zoom_level)
{
    CDBG_HIGH("%s: E",__func__);
    int32_t rc = NO_ERROR;
        if(initBatchUpdate(m_pParamBuf) < 0 ) {
            ALOGE("%s:Failed to initialize group update table", __func__);
            return BAD_TYPE;
        }
        rc = AddSetParmEntryToBatch(m_pParamBuf,
                              CAM_INTF_PARM_ZOOM,
                              sizeof(zoom_level),
                              &zoom_level);

        if (rc != NO_ERROR) {
             ALOGE("%s:Failed to update table", __func__);
             return rc;
        }
    rc = commitSetBatch();
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to set Flash value", __func__);
    }
    CDBG_HIGH("%s: X",__func__);
    return rc;
}

/*===========================================================================
 * FUNCTION   : isOptiZoomEnabled
 *
 * DESCRIPTION: checks whether optizoom is enabled
 *
 * PARAMETERS :
 *
 * RETURN     : true - enabled, false - disabled
 *
 *==========================================================================*/
bool QCameraParameters::isOptiZoomEnabled()
{
    if (m_bOptiZoomOn) {
        uint8_t zoom_level = (uint8_t) getInt(CameraParameters::KEY_ZOOM);
        cam_opti_zoom_t *opti_zoom_settings_need =
                &(m_pCapability->opti_zoom_settings_need);
        uint8_t zoom_threshold = opti_zoom_settings_need->zoom_threshold;
        CDBG_HIGH("%s: current zoom level =%d & zoom_threshold =%d",
                __func__, zoom_level, zoom_threshold);

        if (zoom_level >= zoom_threshold) {
            return true;
        }
    }

    return false;
}

/*===========================================================================
 * FUNCTION   : isDifferentFlipZSL
 *
 * DESCRIPTION: checks whether live preview and snapshot flip differ
 *
 * PARAMETERS :
 *
 * RETURN     : true - different, false - equal
 *
 *==========================================================================*/
bool QCameraParameters::isDifferentFlipZSL()
{
    return (getFlipMode(CAM_STREAM_TYPE_SNAPSHOT) !=
        getFlipMode(CAM_STREAM_TYPE_PREVIEW))
            && m_bZslMode;
}

/*===========================================================================
 * FUNCTION   : isMultiTouchFocusEnabled
 *
 * DESCRIPTION: checks whether Multi-touch Focus is enabled
 *
 * PARAMETERS :
 *
 * RETURN     : true - enabled, false - disabled
 *
 *==========================================================================*/
bool QCameraParameters::isMultiTouchFocusEnabled()
{
    if (m_bMultiTouchFocusOn &&
            (m_MTFBracketInfo.burst_count > 1 || m_currNumBufMTF > 1)) {
       return true;
    }
    return false;
}

/*===========================================================================
 * FUNCTION   : commitAFBracket
 *
 * DESCRIPTION: commit AF Bracket.
 *
 * PARAMETERS :
 *   @AFBracket : AF bracketing configuration
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::commitAFBracket(cam_af_bracketing_t afBracket)
{

    int32_t rc = NO_ERROR;
    if(initBatchUpdate(m_pParamBuf) < 0 ) {
        ALOGE("%s:Failed to initialize group update table", __func__);
        return BAD_TYPE;
    }

    rc = AddSetParmEntryToBatch(m_pParamBuf,
            CAM_INTF_PARM_FOCUS_BRACKETING,
            sizeof(afBracket),
            &afBracket);
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to update table", __func__);
        return rc;
    }

    rc = commitSetBatch();
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to commit batch", __func__);
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : commitMTFBracket
 *
 * DESCRIPTION: commit multi-touch focus Bracket.
 *
 * PARAMETERS :
 *   @mtfBracket : AF bracketing configuration
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::commitMTFBracket(cam_af_bracketing_t mtfBracket)
{
    int32_t rc = NO_ERROR;
    if (initBatchUpdate(m_pParamBuf) < 0 ) {
        ALOGE("%s:Failed to initialize group update table", __func__);
        return BAD_TYPE;
    }

    rc = AddSetParmEntryToBatch(m_pParamBuf,
            CAM_INTF_PARM_MULTI_TOUCH_FOCUS_BRACKETING,
            sizeof(mtfBracket),
            &mtfBracket);
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to update table", __func__);
        return rc;
    }

    rc = commitSetBatch();
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to commit batch", __func__);
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : resetMultiTouchFocusParam
 *
 * DESCRIPTION: reset MTF params to invalid state.
 *
 * PARAMETERS :
 *
 * RETURN     :
 *==========================================================================*/
void QCameraParameters::resetMultiTouchFocusParam()
{
    m_MTFBracketInfo.burst_count = 0;
    for (int i = 0; i < MAX_AF_BRACKETING_VALUES; i++) {
        m_MTFBracketInfo.focus_steps[i] = -1;
    }
}

/*===========================================================================
 * FUNCTION   : commitFlashBracket
 *
 * DESCRIPTION: commit Flash Bracket.
 *
 * PARAMETERS :
 *   @AFBracket : Flash bracketing configuration
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::commitFlashBracket(cam_flash_bracketing_t flashBracket)
{
    CDBG_HIGH("%s: E",__func__);
    int32_t rc = NO_ERROR;
    if(initBatchUpdate(m_pParamBuf) < 0 ) {
        ALOGE("%s:Failed to initialize group update table", __func__);
        return BAD_TYPE;
    }

    rc = AddSetParmEntryToBatch(m_pParamBuf,
            CAM_INTF_PARM_FLASH_BRACKETING,
            sizeof(flashBracket),
            &flashBracket);
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to update table", __func__);
        return rc;
    }

    rc = commitSetBatch();
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to commit batch", __func__);
    }

    CDBG_HIGH("%s: X",__func__);
    return rc;
}

/*===========================================================================
 * FUNCTION   : setAFBracket
 *
 * DESCRIPTION: set AF bracket value
 *
 * PARAMETERS :
 *   @afBracketStr : AF bracket value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setAFBracket(const char *afBracketStr)
{
    CDBG_HIGH("%s: afBracketStr =%s",__func__,afBracketStr);

    if(afBracketStr != NULL) {
        int value = lookupAttr(AF_BRACKETING_MODES_MAP, PARAM_MAP_SIZE(AF_BRACKETING_MODES_MAP),
                afBracketStr);
        if (value != NAME_NOT_FOUND) {
            m_bAFBracketingOn = (value != 0);
            updateParamEntry(KEY_QC_AF_BRACKET, afBracketStr);

            return NO_ERROR;
        }
    }

    ALOGE("Invalid af bracket value: %s",
        (afBracketStr == NULL) ? "NULL" : afBracketStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setChromaFlash
 *
 * DESCRIPTION: set chroma flash value
 *
 * PARAMETERS :
 *   @aecBracketStr : chroma flash value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setChromaFlash(const char *chromaFlashStr)
{
    CDBG_HIGH("%s: chromaFlashStr =%s",__func__,chromaFlashStr);
    if(chromaFlashStr != NULL) {
        int value = lookupAttr(CHROMA_FLASH_MODES_MAP, PARAM_MAP_SIZE(CHROMA_FLASH_MODES_MAP),
                chromaFlashStr);
        if(value != NAME_NOT_FOUND) {
            m_bChromaFlashOn = (value != 0);
            updateParamEntry(KEY_QC_CHROMA_FLASH, chromaFlashStr);

            return NO_ERROR;
        }
    }

    ALOGE("Invalid chroma flash value: %s",
        (chromaFlashStr == NULL) ? "NULL" : chromaFlashStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setOptiZoom
 *
 * DESCRIPTION: set opti zoom value
 *
 * PARAMETERS :
 *   @optiZoomStr : opti zoom value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setOptiZoom(const char *optiZoomStr)
{
    CDBG_HIGH("%s: optiZoomStr =%s",__func__,optiZoomStr);
    if(optiZoomStr != NULL) {
        int value = lookupAttr(OPTI_ZOOM_MODES_MAP, PARAM_MAP_SIZE(OPTI_ZOOM_MODES_MAP),
                optiZoomStr);
        if(value != NAME_NOT_FOUND) {
            m_bOptiZoomOn = (value != 0);
            updateParamEntry(KEY_QC_OPTI_ZOOM, optiZoomStr);

            return NO_ERROR;
        }
    }
    ALOGE("Invalid opti zoom value: %s",
        (optiZoomStr == NULL) ? "NULL" : optiZoomStr);
    return BAD_VALUE;
}

 /*===========================================================================
 * FUNCTION   : setFssr
 *
 * DESCRIPTION: set fssr value
 *
 * PARAMETERS :
 *   @aecBracketStr : fssr value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setFssr(const char *fssrStr)
{
    if (fssrStr != NULL) {
        CDBG_HIGH("%s: fssrStr = %s", __func__, fssrStr);
        int value = lookupAttr(FSSR_MODES_MAP, PARAM_MAP_SIZE(FSSR_MODES_MAP), fssrStr);
        if(value != NAME_NOT_FOUND) {
            m_bFssrOn = (value != 0);
            updateParamEntry(KEY_QC_FSSR, fssrStr);
            return NO_ERROR;
        }
    }

    CDBG_HIGH("Invalid fssr value: %s", (fssrStr == NULL) ? "NULL" : fssrStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setTruePortrait
 *
 * DESCRIPTION: set true portrait value
 *
 * PARAMETERS :
 *   @optiZoomStr : true portrait value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setTruePortrait(const char *truePortraitStr)
{
    CDBG_HIGH("%s: truePortraitStr =%s",__func__,truePortraitStr);
    if(truePortraitStr != NULL) {
        int value = lookupAttr(TRUE_PORTRAIT_MODES_MAP, PARAM_MAP_SIZE(TRUE_PORTRAIT_MODES_MAP),
                truePortraitStr);
        if(value != NAME_NOT_FOUND) {
            m_bTruePortraitOn = (value != 0);
            updateParamEntry(KEY_QC_TRUE_PORTRAIT, truePortraitStr);
            return NO_ERROR;
        }
    }
    ALOGE("Invalid true portrait value: %s",
        (truePortraitStr == NULL) ? "NULL" : truePortraitStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setSeeMore
 *
 * DESCRIPTION: set see more value
 *
 * PARAMETERS :
 *   @seeMoreStr : see more value string
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setSeeMore(const char *seeMoreStr)
{
    CDBG_HIGH("%s: seeMoreStr =%s",__func__,seeMoreStr);
    if(seeMoreStr != NULL) {
        int value = lookupAttr(ON_OFF_MODES_MAP, PARAM_MAP_SIZE(ON_OFF_MODES_MAP), seeMoreStr);
        if(value != NAME_NOT_FOUND) {
            m_bSeeMoreOn = (value != 0);
            updateParamEntry(KEY_QC_SEE_MORE, seeMoreStr);

            return NO_ERROR;
        }
    }
    ALOGE("Invalid see more value: %s",
        (seeMoreStr == NULL) ? "NULL" : seeMoreStr);
    return BAD_VALUE;
}

/*===========================================================================
 * FUNCTION   : setAEBracketing
 *
 * DESCRIPTION: enables AE bracketing
 *
 * PARAMETERS :
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setAEBracketing()
{
    int32_t rc = NO_ERROR;
    if(initBatchUpdate(m_pParamBuf) < 0 ) {
        ALOGE("%s:Failed to initialize group update table", __func__);
        return BAD_TYPE;
    }

    rc = AddSetParmEntryToBatch(m_pParamBuf,
            CAM_INTF_PARM_HDR,
            sizeof(m_AEBracketingClient),
            &m_AEBracketingClient);
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to update AE bracketing", __func__);
        return rc;
    }

    rc = commitSetBatch();
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to configure AE bracketing", __func__);
        return rc;
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : setHDRAEBracket
 *
 * DESCRIPTION: enables AE bracketing for HDR
 *
 * PARAMETERS :
 *   @hdrBracket : HDR bracketing configuration
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setHDRAEBracket(cam_exp_bracketing_t hdrBracket)
{
    int32_t rc = NO_ERROR;
    if(initBatchUpdate(m_pParamBuf) < 0 ) {
        ALOGE("%s:Failed to initialize group update table", __func__);
        return BAD_TYPE;
    }

    rc = AddSetParmEntryToBatch(m_pParamBuf,
            CAM_INTF_PARM_HDR,
            sizeof(hdrBracket),
            &hdrBracket);
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to update table", __func__);
        return rc;
    }

    rc = commitSetBatch();
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to configure HDR bracketing", __func__);
        return rc;
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : restoreAEBracket
 *
 * DESCRIPTION: restores client AE bracketing configuration after HDR is done
 *
 * PARAMETERS :
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::stopAEBracket()
{
  cam_exp_bracketing_t bracketing;

  bracketing.mode = CAM_EXP_BRACKETING_OFF;

  return setHDRAEBracket(bracketing);
}

/*===========================================================================
 * FUNCTION   : updateFlash
 *
 * DESCRIPTION: restores client flash configuration or disables flash
 *
 * PARAMETERS :
 *   @commitSettings : flag indicating whether settings need to be commited
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::updateFlash(bool commitSettings)
{
    int32_t rc = NO_ERROR;
    int32_t value;

    if (commitSettings) {
      if(initBatchUpdate(m_pParamBuf) < 0 ) {
          ALOGE("%s:Failed to initialize group update table", __func__);
          return BAD_TYPE;
      }
    }

    if (isHDREnabled() || m_bAeBracketingEnabled || m_bAFBracketingOn ||
          m_bOptiZoomOn || m_bFssrOn || m_bSensorHDREnabled) {
        value = CAM_FLASH_MODE_OFF;
    } else if (m_bChromaFlashOn) {
        value = CAM_FLASH_MODE_ON;
    } else {
        value = mFlashValue;
    }

    if (value != mFlashDaemonValue) {

        ALOGV("%s: Setting Flash value %d", __func__, value);
        rc = AddSetParmEntryToBatch(m_pParamBuf,
                                      CAM_INTF_PARM_LED_MODE,
                                      sizeof(value),
                                      &value);
        if (rc != NO_ERROR) {
            rc = BAD_VALUE;
            ALOGE("%s:Failed to set led mode", __func__);
            return rc;
        }

        mFlashDaemonValue = value;
    } else {
        rc = NO_ERROR;
    }

    if (commitSettings) {
      rc = commitSetBatch();
      if (rc != NO_ERROR) {
          ALOGE("%s:Failed to configure HDR bracketing", __func__);
          return rc;
      }
    }

    return rc;
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
                PARAM_MAP_SIZE(ENABLE_DISABLE_MODES_MAP), redeyeStr);
        if (value != NAME_NOT_FOUND) {
            CDBG("%s: Setting RedEye Reduce value %s", __func__, redeyeStr);
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
    cam_denoise_process_type_t processPlate = CAM_WAVELET_DENOISE_CBCR_ONLY;
    property_get("persist.denoise.process.plates", prop, "");
    if (strlen(prop) > 0) {
        switch(atoi(prop)) {
        case 0:
            processPlate = CAM_WAVELET_DENOISE_YCBCR_PLANE;
            break;
        case 1:
            processPlate = CAM_WAVELET_DENOISE_CBCR_ONLY;
            break;
        case 2:
            processPlate = CAM_WAVELET_DENOISE_STREAMLINE_YCBCR;
            break;
        case 3:
            processPlate = CAM_WAVELET_DENOISE_STREAMLINED_CBCR;
            break;
        default:
            processPlate = CAM_WAVELET_DENOISE_CBCR_ONLY;
            break;
        }
    }
    return processPlate;
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
    if ((m_pCapability->qcom_supported_feature_mask & CAM_QCOM_FEATURE_DENOISE2D) == 0){
        CDBG_HIGH("%s: WNR is not supported",__func__);
        return NO_ERROR;
    }

    if (wnrStr != NULL) {
        int value = lookupAttr(DENOISE_ON_OFF_MODES_MAP, PARAM_MAP_SIZE(DENOISE_ON_OFF_MODES_MAP),
                wnrStr);
        if (value != NAME_NOT_FOUND) {
            updateParamEntry(KEY_QC_DENOISE, wnrStr);

            cam_denoise_param_t temp;
            memset(&temp, 0, sizeof(temp));
            temp.denoise_enable = (uint8_t) value;
            m_bWNROn = (value != 0);
            if (m_bWNROn) {
                temp.process_plates = getWaveletDenoiseProcessPlate();
            }
            CDBG("%s: Denoise enable=%d, plates=%d",
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
int32_t QCameraParameters::getStreamRotation(cam_stream_type_t streamType,
                                            cam_pp_feature_config_t &featureConfig,
                                            cam_dimension_t &dim)
{
    int32_t ret = NO_ERROR;
    int rotationParam = getInt(KEY_QC_VIDEO_ROTATION);
    featureConfig.rotation = ROTATE_0;
    int swapDim = 0;
    switch (streamType) {
    case CAM_STREAM_TYPE_VIDEO:
           switch(rotationParam) {
            case 90:
               featureConfig.feature_mask |= CAM_QCOM_FEATURE_CPP;
               featureConfig.rotation = ROTATE_90;
               swapDim = 1;
               break;
            case 180:
               featureConfig.feature_mask |= CAM_QCOM_FEATURE_CPP;
               featureConfig.rotation = ROTATE_180;
               break;
            case 270:
               featureConfig.feature_mask |= CAM_QCOM_FEATURE_CPP;
               featureConfig.rotation = ROTATE_270;
               swapDim = 1;
              break;
            default:
               featureConfig.rotation = ROTATE_0;
        }
        break;
    case CAM_STREAM_TYPE_PREVIEW:
    case CAM_STREAM_TYPE_POSTVIEW:
    case CAM_STREAM_TYPE_SNAPSHOT:
    case CAM_STREAM_TYPE_RAW:
    case CAM_STREAM_TYPE_METADATA:
    case CAM_STREAM_TYPE_OFFLINE_PROC:
    case CAM_STREAM_TYPE_DEFAULT:
    default:
        break;
    }

    if (swapDim > 0) {
        int w = 0;
        w = dim.width;
        dim.width = dim.height;
        dim.height = w;
    }
    return ret;
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
            char raw_format[PROPERTY_VALUE_MAX];
            int rawFormat;
            memset(raw_format, 0, sizeof(raw_format));
            /*Default value is CAM_FORMAT_BAYER_QCOM_RAW_10BPP_GBRG*/
            property_get("persist.camera.raw.format", raw_format, "16");
            rawFormat = atoi(raw_format);
            format = (cam_format_t)rawFormat;
            CDBG_HIGH("%s: Raw stream format %d bundled with snapshot",
                   __func__, format);
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
        CDBG("%s: No flip mode for stream type %d", __func__, type);
        break;
    }

    if(str != NULL){
        //Need give corresponding filp value based on flip mode strings
        int value = lookupAttr(FLIP_MODES_MAP, PARAM_MAP_SIZE(FLIP_MODES_MAP), str);
        if(value != NAME_NOT_FOUND)
            flipMode = value;
        }

    CDBG("%s: the filp mode of stream type %d is %d .", __func__, type, flipMode);
    return flipMode;
}

/*===========================================================================
 * FUNCTION   : isSnapshotFDNeeded
 *
 * DESCRIPTION: check whether Face Detection Metadata is needed
 *
 * PARAMETERS : none
 *
 * RETURN     : bool type of status
 *              0 - need
 *              1 - not need
 *==========================================================================*/
bool QCameraParameters::isSnapshotFDNeeded()
{
    return getInt(KEY_QC_SNAPSHOT_FD_DATA);
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
        //For CTS testPreviewPictureSizesCombination
        int cur_pic_width, cur_pic_height;
        CameraParameters::getPictureSize(&cur_pic_width, &cur_pic_height);
#ifndef USE_KK_CODE
        {
            int minDimension;
            if((dim.width*dim.height) > (cur_pic_width*cur_pic_height)) {
                minDimension = (cur_pic_width * cur_pic_height);
            } else {
                minDimension = (dim.width*dim.height);
            }

            double exp_aspectRatio = (double)cur_pic_width / (double)cur_pic_height;

            if (minDimension >= ISP_SEC_SCALAR_MAX_LIMIT) {
                size_t i;
                double preview_aspectRatio = 0.0f;
                double preview_height = 0.0f;
                double preview_width = 0.0f;
                for (i = 0; i < m_pCapability->preview_sizes_tbl_cnt; ++i) {
                    if ((m_pCapability->preview_sizes_tbl[i].width *
                            m_pCapability->preview_sizes_tbl[i].height)
                            < ISP_SEC_SCALAR_MAX_LIMIT) {
                        preview_height = (double)m_pCapability->preview_sizes_tbl[i].height;
                        preview_width = (double)m_pCapability->preview_sizes_tbl[i].width;
                        preview_aspectRatio = preview_width / preview_height;
                        if (fabs(preview_aspectRatio - exp_aspectRatio) <= ASPECT_TOLERANCE) {
                            dim.width = m_pCapability->preview_sizes_tbl[i].width;
                            dim.height = m_pCapability->preview_sizes_tbl[i].height;
                            break;
                        }
                    }
                }

                if (i == m_pCapability->preview_sizes_tbl_cnt) {
                    // Fallback to next supported preview size less than scalar limitation
                    for (i = 0; i < m_pCapability->preview_sizes_tbl_cnt; ++i) {
                        if ((m_pCapability->preview_sizes_tbl[i].width *
                                m_pCapability->preview_sizes_tbl[i].height)
                                <= ISP_SEC_SCALAR_MAX_LIMIT) {
                            dim.width = m_pCapability->preview_sizes_tbl[i].width;
                            dim.height = m_pCapability->preview_sizes_tbl[i].height;
                            break;
                        }
                    }
                    break;
                }
                break;
            }
        }
#endif

        if ((dim.width > cur_pic_width && dim.height < cur_pic_height)
                || (dim.width < cur_pic_width && dim.height > cur_pic_height)) {
            size_t k;
            for (k = 0; k < m_pCapability->preview_sizes_tbl_cnt; ++k) {
                if (cur_pic_width >= m_pCapability->preview_sizes_tbl[k].width
                    && cur_pic_height >= m_pCapability->preview_sizes_tbl[k].height) {
                    dim.width = m_pCapability->preview_sizes_tbl[k].width;
                    dim.height = m_pCapability->preview_sizes_tbl[k].height;
                    CDBG("%s:re-set size, pic_width=%d, pic_height=%d, pre_width=%d,pre_height=%d",
                            __func__,cur_pic_width,cur_pic_height, dim.width, dim.height);
                    break;
                 }
            }
            if (k == m_pCapability->preview_sizes_tbl_cnt) {
                // Assign the Picture size to Preview size
                dim.width = cur_pic_width;
                dim.height = cur_pic_height;
                CDBG("%s: re-set size, pic_width=%d, pic_height=%d, pre_width=%d, pre_height=%d.",
                        __func__,cur_pic_width, cur_pic_height, dim.width, dim.height);
           }

        }
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
        //dim = m_pCapability->raw_dim;
        getRawSize(dim);
        break;
    case CAM_STREAM_TYPE_METADATA:
        dim.width = (int32_t)sizeof(cam_metadata_info_t);
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
    CDBG_HIGH("%s: format %d\n", __func__, halPixelFormat);
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
uint8_t QCameraParameters::getZSLBurstInterval()
{
    int interval = getInt(KEY_QC_ZSL_BURST_INTERVAL);
    if (interval < 0) {
        interval = 1;
    }
    return (uint8_t)interval;
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
uint8_t QCameraParameters::getZSLQueueDepth()
{
    int qdepth = getInt(KEY_QC_ZSL_QUEUE_DEPTH);
    if (qdepth < 0) {
        qdepth = 2;
    }
    return (uint8_t)qdepth;
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
uint8_t QCameraParameters::getZSLBackLookCount()
{
    int look_back = getInt(KEY_QC_ZSL_BURST_LOOKBACK);
    if (look_back < 0) {
        look_back = 2;
    }
    return (uint8_t)look_back;
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
uint8_t QCameraParameters::getMaxUnmatchedFramesInQueue()
{
    return (uint8_t)(m_pCapability->min_num_pp_bufs + (m_nBurstNum / 10));
}

/**
 * ===========================================================================
 * FUNCTION   : getMinPPBufs
 *
 * DESCRIPTION: get minimum extra buffers needed by pproc which HAL has to allocate
 *
 * PARAMETERS : none
 *
 * RETURN     : min pp buf count
 * ==========================================================================
 */
int QCameraParameters::getMinPPBufs()
{
    // Ideally we should be getting this from m_pCapability->min_num_pp_bufs. But as of now
    // this number reported by backend is wrong. It simply adds all the ppbuf requirement by
    // each module irrespective of whether its connected or not. This has to be enhanced later
    // to get the exact requirement from backend.
    return MIN_PP_BUF_CNT;
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
    CDBG("%s: VideoHint = %d", __func__, value);
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
    uint8_t numOfSnapshot = 1;
    int val = getInt(KEY_QC_NUM_SNAPSHOT_PER_SHUTTER);
    if (0 < val) {
        numOfSnapshot = (uint8_t)val;
    }

    /* update the count for refocus */
   if (isUbiRefocus()) {
       numOfSnapshot = (uint8_t) (numOfSnapshot + UfOutputCount());
   }

    return numOfSnapshot;
}

/*===========================================================================
 * FUNCTION   : MTFOutputCount
 *
 * DESCRIPTION: find # of output for MTF feature
 *
 * PARAMETERS : none
 *
 * RETURN     : # of output for MTF feature
 *==========================================================================*/
uint32_t QCameraParameters::MTFOutputCount()
{
   return (uint32_t) (m_currNumBufMTF + 1);
}

/*===========================================================================
 * FUNCTION   : getBurstCountForAdvancedCapture
 *
 * DESCRIPTION: get burst count for advanced capture.
 *
 * PARAMETERS : none
 *
 * RETURN     : number of snapshot required for advanced capture.
 *==========================================================================*/
uint8_t QCameraParameters::getBurstCountForAdvancedCapture()
{
    uint32_t burstCount = 0;
    if (isUbiFocusEnabled()) {
        //number of snapshots required for Ubi Focus.
        burstCount = m_pCapability->ubifocus_af_bracketing_need.burst_count;
    } else if (isOptiZoomEnabled()) {
        //number of snapshots required for Opti Zoom.
        burstCount = m_pCapability->opti_zoom_settings_need.burst_count;
    } else if (isfssrEnabled()) {
        //number of snapshots required for fssr.
        burstCount = m_pCapability->fssr_settings_need.burst_count;
    } else if (isChromaFlashEnabled()) {
        //number of snapshots required for Chroma Flash.
        //TODO: remove hardcoded value, add in capability.
        burstCount = 2;
    } else if (isMultiTouchFocusEnabled()) {
        //number of snapshots required by multi-touch focus.
        burstCount = m_currNumBufMTF;
    } else if (isHDREnabled()) {
        //number of snapshots required for HDR.
        burstCount = m_pCapability->hdr_bracketing_setting.num_frames;
    } else if (isAEBracketEnabled()) {
      burstCount = 0;
      const char *str_val = m_AEBracketingClient.values;
      if ((str_val != NULL) && (strlen(str_val) > 0)) {
          char prop[PROPERTY_VALUE_MAX];
          memset(prop, 0, sizeof(prop));
          strcpy(prop, str_val);
          char *saveptr = NULL;
          char *token = strtok_r(prop, ",", &saveptr);
          while (token != NULL) {
              token = strtok_r(NULL, ",", &saveptr);
              burstCount++;
          }
      }
    }

    if (burstCount <= 0) {
        burstCount = 1;
    }
    return (uint8_t)burstCount;
}

/*===========================================================================
 * FUNCTION   : getNumOfExtraHDRInBufsIfNeeded
 *
 * DESCRIPTION: get number of extra input buffers needed by HDR
 *
 * PARAMETERS : none
 *
 * RETURN     : number of extra buffers needed by HDR; 0 if not HDR enabled
 *==========================================================================*/
uint8_t QCameraParameters::getNumOfExtraHDRInBufsIfNeeded()
{
    unsigned int numOfBufs = 0;

    if (isHDREnabled()) {
        numOfBufs += m_pCapability->hdr_bracketing_setting.num_frames;
        if (isHDR1xFrameEnabled() && isHDR1xExtraBufferNeeded()) {
            numOfBufs++;
        }
        numOfBufs--; // Only additional buffers need to be returned
    }

    return (uint8_t)(numOfBufs * getBurstNum());
}

/*===========================================================================
 * FUNCTION   : getNumOfExtraHDROutBufsIfNeeded
 *
 * DESCRIPTION: get number of extra output buffers needed by HDR
 *
 * PARAMETERS : none
 *
 * RETURN     : number of extra buffers needed by HDR; 0 if not HDR enabled
 *==========================================================================*/
uint8_t QCameraParameters::getNumOfExtraHDROutBufsIfNeeded()
{
    int numOfBufs = 0;

    if (isHDREnabled() && isHDR1xFrameEnabled()) {
        numOfBufs++;
    }

    return (uint8_t)(numOfBufs * getBurstNum());
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
uint8_t QCameraParameters::getBurstNum()
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
uint32_t QCameraParameters::getJpegQuality()
{
    int quality = getInt(KEY_JPEG_QUALITY);
    if (quality < 0) {
        quality = 85; // set to default quality value
    }
    return (uint32_t)quality;
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
uint32_t QCameraParameters::getJpegRotation() {
    int rotation = getInt(KEY_ROTATION);
    if (rotation < 0) {
        rotation = 0;
    }
    return (uint32_t)rotation;
}

/*===========================================================================
 * FUNCTION   : getEffectValue
 *
 * DESCRIPTION: get effect value
 *
 * PARAMETERS : none
 *
 * RETURN     : effect value
 *==========================================================================*/
int32_t QCameraParameters::getEffectValue()
{
    uint32_t cnt = 0;
    const char *effect = get(KEY_EFFECT);
    if (effect) {
        while (NULL != EFFECT_MODES_MAP[cnt].desc) {
            if (!strcmp(EFFECT_MODES_MAP[cnt].desc, effect)) {
                return EFFECT_MODES_MAP[cnt].val;
            }
            cnt++;
        }
    } else {
        ALOGE("%s: Missing effect value", __func__);
    }
    return CAM_EFFECT_MODE_OFF;
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
    double degF = atof(coord_str);
    if (degF < 0) {
        degF = -degF;
    }
    double minF = (degF - (double)(int) degF) * 60.0;
    double secF = (minF - (double)(int) minF) * 60.0;

    getRational(&coord[0], (int)degF, 1);
    getRational(&coord[1], (int)minF, 1);
    getRational(&coord[2], (int)(secF * 10000.0), 10000);
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : getExifDateTime
 *
 * DESCRIPTION: query exif date time
 *
 * PARAMETERS :
 *   @dateTime : String to store exif date time.
 *               Should be leaved unchanged in case of error.
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::getExifDateTime(String8 &dateTime)
{
    int32_t ret = NO_ERROR;
    //get time and date from system
    time_t rawtime = time(NULL);
    if (((time_t) 0) <= rawtime) {
        struct tm *timeinfo = localtime (&rawtime);
        if (NULL != timeinfo) {
            //Write datetime according to EXIF Spec
            //"YYYY:MM:DD HH:MM:SS" (20 chars including \0)
            dateTime = String8::format("%04d:%02d:%02d %02d:%02d:%02d",
                    timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,
                    timeinfo->tm_mday, timeinfo->tm_hour,
                    timeinfo->tm_min, timeinfo->tm_sec);
        } else {
            ALOGE("%s: localtime() error: %s", __func__, strerror(errno));
            ret = UNKNOWN_ERROR;
        }
    } else {
        ALOGE("%s: rawtime() error: %s", __func__, strerror(errno));
        ret = UNKNOWN_ERROR;
    }
    return ret;
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
    if ((0 > num) || (0 > denom)) {
        ALOGE("%s: Negative values", __func__);
        return BAD_VALUE;
    }
    if (NULL == rat) {
        ALOGE("%s: NULL rat input", __func__);
        return BAD_VALUE;
    }
    rat->num = (uint32_t)num;
    rat->denom = (uint32_t)denom;
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
    int iso_index = lookupAttr(ISO_MODES_MAP, PARAM_MAP_SIZE(ISO_MODES_MAP), iso_str);
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
    case CAM_ISO_MODE_3200:
        isoSpeed = 3200;
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
        count += (uint32_t)strlen(str);
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
        return getRational(altitude, (int)(value*1000), 1000);
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
        if (UTCTimestamp != NULL) {
            strftime(gpsDateStamp, bufLen, "%Y:%m:%d", UTCTimestamp);

            getRational(&gpsTimeStamp[0], UTCTimestamp->tm_hour, 1);
            getRational(&gpsTimeStamp[1], UTCTimestamp->tm_min, 1);
            getRational(&gpsTimeStamp[2], UTCTimestamp->tm_sec, 1);
        }
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
    CDBG_HIGH("%s: setting KEY_FOCUS_DISTANCES as %s", __FUNCTION__, str.string());
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
        CDBG("%s: histogram flag not changed, no ops here", __func__);
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

    CDBG_HIGH(" Histogram -> %s", m_bHistogramEnabled ? "Enabled" : "Disabled");

    return rc;
}

/*===========================================================================
 * FUNCTION   : setIntEvent
 *
 * DESCRIPTION: set setIntEvent
 *
 * PARAMETERS :
 *   @params : image size and dimensions
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraParameters::setIntEvent(cam_int_evt_params_t params)
{
    int32_t rc = NO_ERROR;

    if ( m_pParamBuf == NULL ) {
        return NO_INIT;
    }

    if(initBatchUpdate(m_pParamBuf) < 0 ) {
        ALOGE("%s:Failed to initialize group update table", __func__);
        return BAD_TYPE;
    }

    rc = AddSetParmEntryToBatch(m_pParamBuf,
                                CAM_INTF_PARM_INT_EVT,
                                sizeof(params),
                                &params);
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
    uint32_t faceProcMask = m_nFaceProcMask;
    // set face detection mask
    if (enabled) {
        faceProcMask |= CAM_FACE_PROCESS_MASK_DETECTION;
    } else {
        faceProcMask &= ~CAM_FACE_PROCESS_MASK_DETECTION;
    }

    if(m_nFaceProcMask == faceProcMask) {
        CDBG_HIGH("%s: face process mask not changed, no ops here", __func__);
        return NO_ERROR;
    }

    m_nFaceProcMask = faceProcMask;

    // set parm for face detection
    uint32_t requested_faces = (uint32_t)getInt(KEY_QC_MAX_NUM_REQUESTED_FACES);
    cam_fd_set_parm_t fd_set_parm;
    memset(&fd_set_parm, 0, sizeof(cam_fd_set_parm_t));
    fd_set_parm.fd_mode = faceProcMask;
    fd_set_parm.num_fd = requested_faces;

    CDBG_HIGH("[KPI Perf] %s: PROFILE_FACE_DETECTION_VALUE = %d num_fd = %d",
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

    CDBG("%s: FaceProcMask -> %d", __func__, m_nFaceProcMask);

    return rc;
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

int32_t QCameraParameters::updateRAW(cam_dimension_t max_dim)
{
    int32_t rc = NO_ERROR;
    cam_dimension_t raw_dim;

    if (max_dim.width == 0 || max_dim.height == 0) {
        max_dim = m_pCapability->raw_dim;
    }

    if(initBatchUpdate(m_pParamBuf) < 0 ) {
        ALOGE("%s:Failed to initialize group update table", __func__);
        return BAD_TYPE;
    }

    rc = AddSetParmEntryToBatch(m_pParamBuf,
                                CAM_INTF_PARM_MAX_DIMENSION,
                                sizeof(cam_dimension_t),
                                &max_dim);
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to update table for CAM_INTF_PARM_MAX_DIMENSION ", __func__);
        return rc;
    }

    rc = commitSetBatch();
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to set lock CAM_INTF_PARM_MAX_DIMENSION parm", __func__);
        return rc;
    }

    if(initBatchUpdate(m_pParamBuf) < 0 ) {
        ALOGE("%s:Failed to initialize group update table", __func__);
        return BAD_TYPE;
    }

    rc = AddGetParmEntryToBatch(m_pParamBuf,
                                CAM_INTF_PARM_RAW_DIMENSION);
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to get CAM_INTF_PARM_RAW_DIMENSION", __func__);
        return rc;
    }

    rc = commitGetBatch();
    if (rc != NO_ERROR) {
        ALOGE("%s:Failed to get commit CAM_INTF_PARM_RAW_DIMENSION", __func__);
        return rc;
    }
    memcpy(&raw_dim,POINTER_OF_PARAM(CAM_INTF_PARM_RAW_DIMENSION,m_pParamBuf),sizeof(cam_dimension_t));
    CDBG_HIGH("%s : RAW Dimension = %d X %d",__func__,raw_dim.width,raw_dim.height);
    if (raw_dim.width == 0 || raw_dim.height == 0) {
        ALOGE("%s: Error getting RAW size. Setting to Capability value",__func__);
        raw_dim = m_pCapability->raw_dim;
    }
    setRawSize(raw_dim);
    return rc;
}

/*===========================================================================
 * FUNCTION   : setHDRSceneEnable
 *
 * DESCRIPTION: sets hdr scene deteced flag
 *
 * PARAMETERS :
 *   @bflag : hdr scene deteced
 *
 * RETURN     : nothing
 *==========================================================================*/
void QCameraParameters::setHDRSceneEnable(bool bflag)
{
    bool bupdate = false;
    if (m_HDRSceneEnabled != bflag) {
        bupdate = true;
    }
    m_HDRSceneEnabled = bflag;

    if (bupdate) {
        updateFlash(true);
    }
}

/*===========================================================================
 * FUNCTION   : getASDStateString
 *
 * DESCRIPTION: get ASD result in string format
 *
 * PARAMETERS :
 *   @scene : selected scene mode
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
 const char *QCameraParameters::getASDStateString(cam_auto_scene_t scene)
{
    switch (scene) {
      case S_NORMAL :
        return "Normal";
      case S_SCENERY:
        return "Scenery";
      case S_PORTRAIT:
        return "Portrait";
      case S_PORTRAIT_BACKLIGHT:
        return "Portrait-Backlight";
      case S_SCENERY_BACKLIGHT:
        return "Scenery-Backlight";
      case S_BACKLIGHT:
        return "Backlight";
      default:
        return "<Unknown!>";
      }
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
 * FUNCTION   : isYUVFrameInfoNeeded
 *
 * DESCRIPTION: In AE-Bracket mode, we need set yuv buffer information for up-layer
 *
 * PARAMETERS : none
 *
 * RETURN     : true: needed
 *              false: no need
 *==========================================================================*/
bool QCameraParameters::isYUVFrameInfoNeeded()
{
    //In AE-Bracket mode, we need set raw buffer information for up-layer
    if(!isNV21PictureFormat() && !isNV16PictureFormat()){
        return false;
    }
    const char *aecBracketStr =  get(KEY_QC_AE_BRACKET_HDR);

    int value = lookupAttr(BRACKETING_MODES_MAP, PARAM_MAP_SIZE(BRACKETING_MODES_MAP),
            aecBracketStr);
    CDBG_HIGH("%s: aecBracketStr=%s, value=%d.", __func__, aecBracketStr, value);
    return (value == CAM_EXP_BRACKETING_ON);
}

/*===========================================================================
 * FUNCTION   : getFrameFmtString
 *
 * DESCRIPTION: get string name of frame format
 *
 * PARAMETERS :
 *   @frame   : frame format
 *
 * RETURN     : string name of frame format
 *==========================================================================*/
const char *QCameraParameters::getFrameFmtString(cam_format_t fmt)
{
    return lookupNameByValue(PICTURE_TYPES_MAP, PARAM_MAP_SIZE(PICTURE_TYPES_MAP), fmt);
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
int32_t QCameraParameters::initBatchUpdate(void *p_table)
{

    m_tempMap.clear();
    CDBG("%s:Initializing batch parameter set",__func__);

    parm_buffer_new_t *param_buf = (parm_buffer_new_t *)p_table;
    memset(param_buf, 0, sizeof(ONE_MB_OF_PARAMS));
    param_buf->num_entry = 0;
    param_buf->curr_size = 0;
    param_buf->tot_rem_size = ONE_MB_OF_PARAMS - sizeof(parm_buffer_new_t);
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
int32_t QCameraParameters::AddSetParmEntryToBatch(void *p_table,
        cam_intf_parm_type_t paramType, size_t paramLength, void *paramValue)
{
    size_t j = 0;
    parm_buffer_new_t *param_buf = (parm_buffer_new_t *)p_table;
    size_t num_entry = param_buf->num_entry;
    size_t size_req = paramLength + sizeof(parm_entry_type_new_t);
    size_t aligned_size_req = (size_req + 3U) & (~3U);
    parm_entry_type_new_t *curr_param = (parm_entry_type_new_t *)
            (void *) &param_buf->entry[0];

    /* first search if the key is already present in the batch list
     * this is a search penalty but as the batch list is never more
     * than a few tens of entries at most,it should be ok.
     * if search performance becomes a bottleneck, we can
     * think of implementing a hashing mechanism.
     * but it is still better than the huge memory required for
     * direct indexing
     */
    for (j = 0; j < num_entry; j++) {
      if (paramType == curr_param->entry_type) {
        CDBG_HIGH("%s:Batch parameter overwrite for param: %d",
                                                __func__, paramType);
        break;
      }
      curr_param = GET_NEXT_PARAM(curr_param, parm_entry_type_new_t);
    }

    //new param, search not found
    if (j == num_entry) {
      if (aligned_size_req > param_buf->tot_rem_size) {
        ALOGE("%s:Batch buffer running out of size, commit and resend",__func__);
        commitSetBatch();
        initBatchUpdate(p_table);
      }

      curr_param = (parm_entry_type_new_t *)
          (void *) (&param_buf->entry[0] + param_buf->curr_size);
      param_buf->curr_size += aligned_size_req;
      param_buf->tot_rem_size -= aligned_size_req;
      param_buf->num_entry++;
    }

    curr_param->entry_type = paramType;
    curr_param->size = paramLength;
    curr_param->aligned_size = aligned_size_req;
    memcpy(&curr_param->data[0], paramValue, paramLength);
    CDBG("%s: num_entry: %d, paramType: %d, paramLength: %d, aligned_size_req: %d",
            __func__, param_buf->num_entry, paramType, paramLength, aligned_size_req);

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
int32_t QCameraParameters::AddGetParmEntryToBatch(void *p_table,
                                                  cam_intf_parm_type_t paramType)
{
    ///in get params, we have no information on the size of the param requested
    //for, hence we assume the largest size and reserve space for the same
    size_t j = 0;
    size_t paramLength = sizeof(parm_type_t);
    parm_buffer_new_t *param_buf = (parm_buffer_new_t *)p_table;
    size_t num_entry = param_buf->num_entry;
    size_t size_req = paramLength + sizeof(parm_entry_type_new_t) - sizeof(char);
    size_t aligned_size_req = (size_req + 3U) & (~3U);
    parm_entry_type_new_t *curr_param = (parm_entry_type_new_t *)
            (void *) &param_buf->entry[0];

    /* first search if the key is already present in the batch list
     * this is a search penalty but as the batch list is never more
     * than a few tens of entries at most,it should be ok.
     * if search performance becomes a bottleneck, we can
     * think of implementing a hashing mechanism.
     * but it is still better than the huge memory required for
     * direct indexing
     */
    for (j = 0; j < num_entry; j++) {
        if (paramType == curr_param->entry_type) {
            CDBG_HIGH("%s:Batch parameter overwrite for param: %d",
                    __func__, paramType);
        break;
        }
        curr_param = GET_NEXT_PARAM(curr_param, parm_entry_type_new_t);
    }

    //new param, search not found
    if (j == num_entry) {
        if (aligned_size_req > param_buf->tot_rem_size) {
            ALOGE("%s:Batch buffer running out of size, commit and resend", __func__);
            // This is an extreme corner case
            // if the size of the batch set is full, we return error
            // the caller is expected to commit the get batch, use the params
            // returned, initialize the batch again and continue.
            return NO_MEMORY;
        }

        curr_param = (parm_entry_type_new_t *) (void *)
                (&param_buf->entry[0] + param_buf->curr_size);
        param_buf->curr_size += aligned_size_req;
        param_buf->tot_rem_size -= aligned_size_req;
        param_buf->num_entry++;
    }

    curr_param->entry_type = paramType;
    curr_param->size = paramLength;
    curr_param->aligned_size = aligned_size_req;
    CDBG_HIGH("%s:num_entry: %d, paramType: %d ",__func__, param_buf->num_entry, paramType);

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
    if (m_pParamBuf->num_entry > 0) {
        rc = m_pCamOpsTbl->ops->set_parms(m_pCamOpsTbl->camera_handle,
                                                      (void *)m_pParamBuf);
        CDBG("%s: commitSetBatch done",__func__);
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
    int32_t rc = NO_ERROR;
    if (m_pParamBuf->num_entry > 0) {
        rc = m_pCamOpsTbl->ops->get_parms(m_pCamOpsTbl->camera_handle,
                                                          (void *)m_pParamBuf);
        CDBG_HIGH("%s: commitGetBatch done",__func__);
    }
    return rc;
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

    /* After applying scene mode auto,
      Camera effects need to be reapplied */
    if ( m_bSceneTransitionAuto ) {
        m_bUpdateEffects = true;
        m_bSceneTransitionAuto = false;
    }

    if ( m_bReleaseTorchCamera && ( NULL != m_pTorch)
        && !m_bRecordingHint_new ) {
        m_pTorch->releaseTorchCamera();
        m_bReleaseTorchCamera = false;
    }


    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : QCameraReprocScaleParam
 *
 * DESCRIPTION: constructor of QCameraReprocScaleParam
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
QCameraReprocScaleParam::QCameraReprocScaleParam(QCameraParameters *parent)
  : mParent(parent),
    mScaleEnabled(false),
    mIsUnderScaling(false),
    mScaleDirection(0),
    mNeedScaleCnt(0),
    mSensorSizeTblCnt(0),
    mSensorSizeTbl(NULL),
    mTotalSizeTblCnt(0)
{
    mPicSizeFromAPK.width = 0;
    mPicSizeFromAPK.height = 0;
    mPicSizeSetted.width = 0;
    mPicSizeSetted.height = 0;
    memset(mNeedScaledSizeTbl, 0, sizeof(mNeedScaledSizeTbl));
    memset(mTotalSizeTbl, 0, sizeof(mTotalSizeTbl));
}

/*===========================================================================
 * FUNCTION   : ~~QCameraReprocScaleParam
 *
 * DESCRIPTION: destructor of QCameraReprocScaleParam
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
QCameraReprocScaleParam::~QCameraReprocScaleParam()
{
    //do nothing now.
}

/*===========================================================================
 * FUNCTION   : setScaledSizeTbl
 *
 * DESCRIPTION: re-set picture size table with dimensions that need scaling if Reproc Scale is enabled
 *
 * PARAMETERS :
 *   @scale_cnt   : count of picture sizes that want scale
 *   @scale_tbl    : picture size table that want scale
 *   @org_cnt     : sensor supported picture size count
 *   @org_tbl      : sensor supported picture size table
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraReprocScaleParam::setScaleSizeTbl(size_t scale_cnt,
        cam_dimension_t *scale_tbl, size_t org_cnt, cam_dimension_t *org_tbl)
{
    int32_t rc = NO_ERROR;
    size_t i;
    mNeedScaleCnt = 0;

    if(!mScaleEnabled || scale_cnt <=0 || scale_tbl == NULL || org_cnt <=0 || org_tbl == NULL){
        return BAD_VALUE;    // Do not need scale, so also need not reset picture size table
    }

    mSensorSizeTblCnt = org_cnt;
    mSensorSizeTbl = org_tbl;
    mNeedScaleCnt = checkScaleSizeTable(scale_cnt, scale_tbl, org_cnt, org_tbl);
    if(mNeedScaleCnt <= 0){
        ALOGE("%s: do not have picture sizes need scaling.", __func__);
        return BAD_VALUE;
    }

    if(mNeedScaleCnt + org_cnt > MAX_SIZES_CNT){
        ALOGE("%s: picture size list exceed the max count.", __func__);
        return BAD_VALUE;
    }

    //get the total picture size table
    mTotalSizeTblCnt = mNeedScaleCnt + org_cnt;
    for(i = 0; i < mNeedScaleCnt; i++){
        mTotalSizeTbl[i].width = mNeedScaledSizeTbl[i].width;
        mTotalSizeTbl[i].height = mNeedScaledSizeTbl[i].height;
        CDBG_HIGH("%s: scale picture size: i =%d, width=%d, height=%d.", __func__,
            i, mTotalSizeTbl[i].width, mTotalSizeTbl[i].height);
    }
    for(; i < mTotalSizeTblCnt; i++){
        mTotalSizeTbl[i].width = org_tbl[i-mNeedScaleCnt].width;
        mTotalSizeTbl[i].height = org_tbl[i-mNeedScaleCnt].height;
        CDBG_HIGH("%s: sensor supportted picture size: i =%d, width=%d, height=%d.", __func__,
            i, mTotalSizeTbl[i].width, mTotalSizeTbl[i].height);
    }
    return rc;
}

/*===========================================================================
 * FUNCTION   : getScaledSizeTblCnt
 *
 * DESCRIPTION: get picture size cnt that need scale
 *
 * PARAMETERS : none
 *
 * RETURN     : uint8_t type of picture size count
 *==========================================================================*/
size_t QCameraReprocScaleParam::getScaleSizeTblCnt()
{
    return mNeedScaleCnt;
}

/*===========================================================================
 * FUNCTION   : getScaledSizeTbl
 *
 * DESCRIPTION: get picture size table that need scale
 *
 * PARAMETERS :  none
 *
 * RETURN     : cam_dimension_t list of picture size table
 *==========================================================================*/
cam_dimension_t *QCameraReprocScaleParam::getScaledSizeTbl()
{
    if(!mScaleEnabled)
        return NULL;

    return mNeedScaledSizeTbl;
}

/*===========================================================================
 * FUNCTION   : setScaleEnable
 *
 * DESCRIPTION: enable or disable Reproc Scale
 *
 * PARAMETERS :
 *   @enabled : enable: 1; disable 0
 *
 * RETURN     : none
 *==========================================================================*/
void QCameraReprocScaleParam::setScaleEnable(bool enabled)
{
    mScaleEnabled = enabled;
}

/*===========================================================================
 * FUNCTION   : isScaleEnabled
 *
 * DESCRIPTION: check if Reproc Scale is enabled
 *
 * PARAMETERS :  none
 *
 * RETURN     : bool type of status
 *==========================================================================*/
bool QCameraReprocScaleParam::isScaleEnabled()
{
    return mScaleEnabled;
}

/*===========================================================================
 * FUNCTION   : isScalePicSize
 *
 * DESCRIPTION: check if current picture size is from Scale Table
 *
 * PARAMETERS :
 *   @width     : current picture width
 *   @height    : current picture height
 *
 * RETURN     : bool type of status
 *==========================================================================*/
bool QCameraReprocScaleParam::isScalePicSize(int width, int height)
{
    //Check if the picture size is in scale table
    if(mNeedScaleCnt <= 0)
        return FALSE;

    for (size_t i = 0; i < mNeedScaleCnt; i++) {
        if ((mNeedScaledSizeTbl[i].width == width) && (mNeedScaledSizeTbl[i].height == height)) {
            //found match
            return TRUE;
        }
    }

    ALOGE("%s: Not in scale picture size table.", __func__);
    return FALSE;
}

/*===========================================================================
 * FUNCTION   : isValidatePicSize
 *
 * DESCRIPTION: check if current picture size is validate
 *
 * PARAMETERS :
 *   @width     : current picture width
 *   @height    : current picture height
 *
 * RETURN     : bool type of status
 *==========================================================================*/
bool QCameraReprocScaleParam::isValidatePicSize(int width, int height)
{
    size_t i = 0;

    for(i = 0; i < mSensorSizeTblCnt; i++){
        if(mSensorSizeTbl[i].width == width
            && mSensorSizeTbl[i].height== height){
            return TRUE;
        }
    }

    for(i = 0; i < mNeedScaleCnt; i++){
        if(mNeedScaledSizeTbl[i].width == width
            && mNeedScaledSizeTbl[i].height== height){
            return TRUE;
        }
    }

    ALOGE("%s: Invalidate input picture size.", __func__);
    return FALSE;
}

/*===========================================================================
 * FUNCTION   : setSensorSupportedPicSize
 *
 * DESCRIPTION: set sensor supported picture size.
 *    For Snapshot stream size configuration, we need use sensor supported size.
 *    We will use CPP to do Scaling based on output Snapshot stream.
 *
 * PARAMETERS : none
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraReprocScaleParam::setSensorSupportedPicSize()
{
    //will find a suitable picture size (here we leave a prossibility to add other scale requirement)
    //Currently we only focus on upscaling, and checkScaleSizeTable() has guaranteed the dimension ratio.

    if(!mIsUnderScaling || mSensorSizeTblCnt <= 0)
        return BAD_VALUE;

    //We just get the max sensor supported size here.
    mPicSizeSetted.width = mSensorSizeTbl[0].width;
    mPicSizeSetted.height = mSensorSizeTbl[0].height;

    return NO_ERROR;
}


/*===========================================================================
 * FUNCTION   : setValidatePicSize
 *
 * DESCRIPTION: set sensor supported size and change scale status.
 *
 * PARAMETERS :
 *   @width    : input picture width
 *   @height   : input picture height
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraReprocScaleParam::setValidatePicSize(int &width,int &height)
{
    if(!mScaleEnabled)
        return BAD_VALUE;

    mIsUnderScaling = FALSE; //default: not under scale

    if(isScalePicSize(width, height)){
        // input picture size need scaling operation. Record size from APK and setted
        mIsUnderScaling = TRUE;
        mPicSizeFromAPK.width = width;
        mPicSizeFromAPK.height = height;

        if(setSensorSupportedPicSize() != NO_ERROR)
            return BAD_VALUE;

        //re-set picture size to sensor supported size
        width = mPicSizeSetted.width;
        height = mPicSizeSetted.height;
        CDBG_HIGH("%s: mPicSizeFromAPK- with=%d, height=%d, mPicSizeSetted- with =%d, height=%d.",
            __func__, mPicSizeFromAPK.width, mPicSizeFromAPK.height, mPicSizeSetted.width, mPicSizeSetted.height);
    }else{
        mIsUnderScaling = FALSE;
        //no scale is needed for input picture size
        if(!isValidatePicSize(width, height)){
            ALOGE("%s: invalidate input picture size.", __func__);
            return BAD_VALUE;
        }
        mPicSizeSetted.width = width;
        mPicSizeSetted.height = height;
    }

    CDBG_HIGH("%s: X. mIsUnderScaling=%d, width=%d, height=%d.", __func__, mIsUnderScaling, width, height);
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : getPicSizeFromAPK
 *
 * DESCRIPTION: get picture size that get from APK
 *
 * PARAMETERS :
 *   @width     : input width
 *   @height    : input height
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraReprocScaleParam::getPicSizeFromAPK(int &width, int &height)
{
    if(!mIsUnderScaling)
        return BAD_VALUE;

    width = mPicSizeFromAPK.width;
    height = mPicSizeFromAPK.height;
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : getPicSizeSetted
 *
 * DESCRIPTION: get picture size that setted into mm-camera
 *
 * PARAMETERS :
 *   @width     : input width
 *   @height    : input height
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int32_t QCameraReprocScaleParam::getPicSizeSetted(int &width, int &height)
{
    width = mPicSizeSetted.width;
    height = mPicSizeSetted.height;
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : isUnderScaling
 *
 * DESCRIPTION: check if we are in Reproc Scaling requirment
 *
 * PARAMETERS :  none
 *
 * RETURN     : bool type of status
 *==========================================================================*/
bool QCameraReprocScaleParam::isUnderScaling()
{
    return mIsUnderScaling;
}

/*===========================================================================
 * FUNCTION   : checkScaleSizeTable
 *
 * DESCRIPTION: check PICTURE_SIZE_NEED_SCALE to choose
 *
 * PARAMETERS :
 *   @scale_cnt   : count of picture sizes that want scale
 *   @scale_tbl    : picture size table that want scale
 *   @org_cnt     : sensor supported picture size count
 *   @org_tbl      : sensor supported picture size table
 *
 * RETURN     : bool type of status
 *==========================================================================*/
size_t QCameraReprocScaleParam::checkScaleSizeTable(size_t scale_cnt,
        cam_dimension_t *scale_tbl, size_t org_cnt, cam_dimension_t *org_tbl)
{
    size_t stbl_cnt = 0;
    size_t temp_cnt = 0;
    ssize_t i = 0;
    if(scale_cnt <=0 || scale_tbl == NULL || org_tbl == NULL || org_cnt <= 0)
        return stbl_cnt;

    //get validate scale size table. Currently we only support:
    // 1. upscale. The scale size must larger than max sensor supported size
    // 2. Scale dimension ratio must be same as the max sensor supported size.
    temp_cnt = scale_cnt;
    for (i = (ssize_t)(scale_cnt - 1); i >= 0; i--) {
        if (scale_tbl[i].width > org_tbl[0].width ||
                (scale_tbl[i].width == org_tbl[0].width &&
                    scale_tbl[i].height > org_tbl[0].height)) {
            //get the smallest scale size
            break;
        }
        temp_cnt--;
    }

    //check dimension ratio
    double supported_ratio = (double)org_tbl[0].width / (double)org_tbl[0].height;
    for (i = 0; i < (ssize_t)temp_cnt; i++) {
        double cur_ratio = (double)scale_tbl[i].width / (double)scale_tbl[i].height;
        if (fabs(supported_ratio - cur_ratio) > ASPECT_TOLERANCE) {
            continue;
        }
        mNeedScaledSizeTbl[stbl_cnt].width = scale_tbl[i].width;
        mNeedScaledSizeTbl[stbl_cnt].height= scale_tbl[i].height;
        stbl_cnt++;
    }

    return stbl_cnt;
}

/*===========================================================================
 * FUNCTION   : getTotalSizeTblCnt
 *
 * DESCRIPTION: get total picture size count after adding dimensions that need scaling
 *
 * PARAMETERS : none
 *
 * RETURN     : uint8_t type of picture size count
 *==========================================================================*/
size_t QCameraReprocScaleParam::getTotalSizeTblCnt()
{
    return mTotalSizeTblCnt;
}

/*===========================================================================
 * FUNCTION   : getTotalSizeTbl
 *
 * DESCRIPTION: get picture size table after adding dimensions that need scaling
 *
 * PARAMETERS :  none
 *
 * RETURN     : cam_dimension_t list of picture size table
 *==========================================================================*/
cam_dimension_t *QCameraReprocScaleParam::getTotalSizeTbl()
{
    if(!mScaleEnabled)
        return NULL;

    return mTotalSizeTbl;
}

/*===========================================================================
 * FUNCTION   : isHDREnabled
 *
 * DESCRIPTION: if HDR is enabled
 *
 * PARAMETERS : none
 *
 * RETURN     : true: needed
 *              false: no need
 *==========================================================================*/
bool QCameraParameters::isHDREnabled()
{
    return ((m_nBurstNum == 1) && (m_bHDREnabled || m_HDRSceneEnabled));
}

/*===========================================================================
 * FUNCTION   : isAVTimerEnabled
 *
 * DESCRIPTION: if AVTimer is enabled
 *
 * PARAMETERS : none
 *
 * RETURN     : true: needed
 *              false: no need
 *==========================================================================*/
bool QCameraParameters::isAVTimerEnabled()
{
    return m_bAVTimerEnabled;
}

/*===========================================================================
* FUNCTION   : isDISEnabled
*
* DESCRIPTION: if DIS is enabled
*
* PARAMETERS : none
*
* RETURN    : true: needed
*               false: no need
*==========================================================================*/
bool QCameraParameters::isDISEnabled()
{
    return m_bDISEnabled;
}

/*===========================================================================
 * FUNCTION   : MobicatMask
 *
 * DESCRIPTION: returns mobicat mask
 *
 * PARAMETERS : none
 *
 * RETURN     : mobicat mask
 *==========================================================================*/
uint8_t QCameraParameters::getMobicatMask()
{
    return m_bMobiMask;
}

/*===========================================================================
 * FUNCTION   : needThumbnailReprocess
 *
 * DESCRIPTION: Check if thumbnail reprocessing is needed
 *
 * PARAMETERS : @pFeatureMask - feature mask
 *
 * RETURN     : true: needed
 *              false: no need
 *==========================================================================*/
bool QCameraParameters::needThumbnailReprocess(uint32_t *pFeatureMask)
{
    if (isUbiFocusEnabled() || isChromaFlashEnabled() ||
            isOptiZoomEnabled() || isfssrEnabled() ||
            isMultiTouchFocusEnabled()) {
        *pFeatureMask &= ~CAM_QCOM_FEATURE_CHROMA_FLASH;
        *pFeatureMask &= ~CAM_QCOM_FEATURE_UBIFOCUS;
        *pFeatureMask &= ~CAM_QCOM_FEATURE_OPTIZOOM;
        *pFeatureMask &= ~CAM_QCOM_FEATURE_FSSR;
        *pFeatureMask &= ~CAM_QCOM_FEATURE_MULTI_TOUCH_FOCUS;
        return false;
    } else {
        return true;
    }
}

/*===========================================================================
 * FUNCTION   : getNumOfExtraBuffersForImageProc
 *
 * DESCRIPTION: get number of extra input buffers needed by image processing
 *
 * PARAMETERS : none
 *
 * RETURN     : number of extra buffers needed by ImageProc;
 *              0 if not ImageProc enabled
 *==========================================================================*/
uint8_t QCameraParameters::getNumOfExtraBuffersForImageProc()
{
    int numOfBufs = 0;

    if (isUbiFocusEnabled()) {
        numOfBufs += m_pCapability->ubifocus_af_bracketing_need.burst_count - 1;
        if (isUbiRefocus()) {
            numOfBufs +=
                m_pCapability->ubifocus_af_bracketing_need.burst_count + 1;
        }
    } else if (isMultiTouchFocusEnabled()) {
        numOfBufs += m_currNumBufMTF - 1;
        if (isMTFRefocus()) {
            numOfBufs += m_currNumBufMTF + 1;
        }
    } else if (m_bOptiZoomOn) {
        numOfBufs += m_pCapability->opti_zoom_settings_need.burst_count - 1;
    } else if (isChromaFlashEnabled()) {
        numOfBufs += 1; /* flash and non flash */
    } else if (isfssrEnabled()) {
        numOfBufs += m_pCapability->fssr_settings_need.burst_count - 1;
        //One output buffer of 4X size excluded
    }

    return (uint8_t)(numOfBufs * getBurstNum());
}

/*===========================================================================
 * FUNCTION   : getNumOfExtraBuffersForVideo
 *
 * DESCRIPTION: get number of extra buffers needed by image processing
 *
 * PARAMETERS : none
 *
 * RETURN     : number of extra buffers needed by ImageProc;
 *              0 if not ImageProc enabled
 *==========================================================================*/
uint8_t QCameraParameters::getNumOfExtraBuffersForVideo()
{
    uint8_t numOfBufs = 0;

    if (isSeeMoreEnabled()) {
        numOfBufs = 1;
    }

    return numOfBufs;
}

/*===========================================================================
 * FUNCTION   : getNumOfExtraBuffersForPreview
 *
 * DESCRIPTION: get number of extra buffers needed by image processing
 *
 * PARAMETERS : none
 *
 * RETURN     : number of extra buffers needed by ImageProc;
 *              0 if not ImageProc enabled
 *==========================================================================*/
uint8_t QCameraParameters::getNumOfExtraBuffersForPreview()
{
    uint8_t numOfBufs = 0;

    if (isSeeMoreEnabled() && !isZSLMode() && getRecordingHintValue()) {
        numOfBufs = 1;
    }

    return numOfBufs;
}

}; // namespace qcamera
