/*
** Copyright (c) 2011-2012 The Linux Foundation. All rights reserved.
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

#define ALOG_NIDEBUG 0

#define ALOG_TAG __FILE__
#include <utils/Log.h>
#include <utils/threads.h>
#include <cutils/properties.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <hardware/camera2.h>
#include <mm_camera_interface.h>


namespace android {


const uint32_t QCAvailableFormats[4] = {
        HAL_PIXEL_FORMAT_RAW_SENSOR,
        HAL_PIXEL_FORMAT_BLOB,
        HAL_PIXEL_FORMAT_YV12,
        HAL_PIXEL_FORMAT_YCrCb_420_SP
};

const uint32_t QCAvailableRawSizes[2] = {
    640, 480
};


const nsecs_t QCExposureTimeRange[2] =
    {1000L, 30000000000L} ; // 1 us - 30 sec
const nsecs_t QCFrameDurationRange[2] =
    {33331760L, 30000000000L}; // ~1/30 s - 30 sec
const nsecs_t QCMinVerticalBlank = 10000L;

const uint8_t QCColorFilterArrangement = ANDROID_SENSOR_RGGB;

// Output image data characteristics
const uint32_t QCMaxRawValue = 4000;
const uint32_t QCBlackLevel  = 1000;

const uint64_t QCAvailableRawMinDurations[1] = {
    QCFrameDurationRange[0]
};

const uint32_t QCAvailableProcessedSizes[4] = {
    640, 480, 320, 240
    //    Sensor::kResolution[0], Sensor::kResolution[1]
};

const uint64_t QCAvailableProcessedMinDurations[1] = {
    QCFrameDurationRange[0]
};


const uint32_t QCAvailableJpegSizes[2] = {
    640, 480
    //    Sensor::kResolution[0], Sensor::kResolution[1]
};

const uint64_t QCAvailableJpegMinDurations[1] = {
    QCFrameDurationRange[0]
};

static status_t addOrSize(camera_metadata_t *request,
        bool sizeRequest,
        size_t *entryCount,
        size_t *dataCount,
        uint32_t tag,
        const void *entryData,
        size_t entryDataCount) {
    status_t res;
    if (!sizeRequest) {
        return add_camera_metadata_entry(request, tag, entryData,
                entryDataCount);
    } else {
        int type = get_camera_metadata_tag_type(tag);
        if (type < 0 ) return BAD_VALUE;
        (*entryCount)++;
        (*dataCount) += calculate_camera_metadata_entry_data_size(type,
                entryDataCount);
        return OK;
    }
}

extern "C" status_t getStaticInfo(camera_metadata_t **info,
        mm_camera_vtbl_t *camera_vtbl, bool sizeRequest)
{
    size_t entryCount = 0;
    size_t dataCount = 0;
    status_t ret;

    if(!camera_vtbl) return BAD_VALUE;

    bool mFacingBack = (camera_vtbl->camera_info->camera_info.position==BACK_CAMERA)?
                       true:false;

#define ADD_OR_SIZE( tag, data, count ) \
    if ( ( ret = addOrSize(*info, sizeRequest, &entryCount, &dataCount, \
            tag, data, count) ) != OK ) return ret

    // android.lens

    static float minFocusDistance = 0;
    ADD_OR_SIZE(ANDROID_LENS_MINIMUM_FOCUS_DISTANCE,
            &minFocusDistance, 1);
    ADD_OR_SIZE(ANDROID_LENS_HYPERFOCAL_DISTANCE,
            &minFocusDistance, 1);

    static float focalLength = 0.0f;
    camera_vtbl->ops->get_parm(camera_vtbl->camera_handle,
            MM_CAMERA_PARM_FOCAL_LENGTH,
            (void *)&focalLength);
    ADD_OR_SIZE(ANDROID_LENS_AVAILABLE_FOCAL_LENGTHS,
            &focalLength, 1);

    static float aperture = 2.8f;
    camera_vtbl->ops->get_parm(camera_vtbl->camera_handle,
            MM_CAMERA_PARM_APERTURE,
            (void *)&aperture);
    ADD_OR_SIZE(ANDROID_LENS_AVAILABLE_APERTURES,
            &aperture, 1);

    static float filterDensity = 0;
    ADD_OR_SIZE(ANDROID_LENS_AVAILABLE_FILTER_DENSITY,
            &filterDensity, 1);

    static const uint8_t availableOpticalStabilization =
            ANDROID_LENS_OPTICAL_STABILIZATION_OFF;
    ADD_OR_SIZE(ANDROID_LENS_AVAILABLE_OPTICAL_STABILIZATION,
            &availableOpticalStabilization, 1);

    static const int32_t lensShadingMapSize[] = {1, 1};
    ADD_OR_SIZE(ANDROID_LENS_SHADING_MAP_SIZE, lensShadingMapSize,
            sizeof(lensShadingMapSize)/sizeof(int32_t));

    static const float lensShadingMap[3 * 1 * 1 ] =
            { 1.f, 1.f, 1.f };
    ADD_OR_SIZE(ANDROID_LENS_SHADING_MAP, lensShadingMap,
            sizeof(lensShadingMap)/sizeof(float));

    // Identity transform
    static const int32_t geometricCorrectionMapSize[] = {2, 2};
    ADD_OR_SIZE(ANDROID_LENS_GEOMETRIC_CORRECTION_MAP_SIZE,
            geometricCorrectionMapSize,
            sizeof(geometricCorrectionMapSize)/sizeof(int32_t));

    static const float geometricCorrectionMap[2 * 3 * 2 * 2] = {
            0.f, 0.f,  0.f, 0.f,  0.f, 0.f,
            1.f, 0.f,  1.f, 0.f,  1.f, 0.f,
            0.f, 1.f,  0.f, 1.f,  0.f, 1.f,
            1.f, 1.f,  1.f, 1.f,  1.f, 1.f};
    ADD_OR_SIZE(ANDROID_LENS_GEOMETRIC_CORRECTION_MAP,
            geometricCorrectionMap,
            sizeof(geometricCorrectionMap)/sizeof(float));

    int32_t lensFacing = mFacingBack ?
            ANDROID_LENS_FACING_BACK : ANDROID_LENS_FACING_FRONT;
    ADD_OR_SIZE(ANDROID_LENS_FACING, &lensFacing, 1);

    float lensPosition[3];
    if (mFacingBack) {
        // Back-facing camera is center-top on device
        lensPosition[0] = 0;
        lensPosition[1] = 20;
        lensPosition[2] = -5;
    } else {
        // Front-facing camera is center-right on device
        lensPosition[0] = 20;
        lensPosition[1] = 20;
        lensPosition[2] = 0;
    }
    ADD_OR_SIZE(ANDROID_LENS_POSITION, lensPosition, sizeof(lensPosition)/
            sizeof(float));

#if 0
    // android.sensor

    ADD_OR_SIZE(ANDROID_SENSOR_EXPOSURE_TIME_RANGE,
            QCExposureTimeRange, 2);

    ADD_OR_SIZE(ANDROID_SENSOR_MAX_FRAME_DURATION,
            &QCFrameDurationRange[1], 1);

    ADD_OR_SIZE(ANDROID_SENSOR_AVAILABLE_SENSITIVITIES,
            QCAvailableSensitivities,
            sizeof(QCAvailableSensitivities)
            /sizeof(uint32_t));

    ADD_OR_SIZE(ANDROID_SENSOR_COLOR_FILTER_ARRANGEMENT,
            &QCColorFilterArrangement, 1);
#endif
    static const float sensorPhysicalSize[2] = {3.20f, 2.40f}; // mm
    ADD_OR_SIZE(ANDROID_SENSOR_PHYSICAL_SIZE,
            sensorPhysicalSize, 2);

    const unsigned int kResolution[2]  = {640, 480};
    ADD_OR_SIZE(ANDROID_SENSOR_PIXEL_ARRAY_SIZE,
            kResolution, 2);

    ADD_OR_SIZE(ANDROID_SENSOR_ACTIVE_ARRAY_SIZE,
            kResolution, 2);

#if 0
    ADD_OR_SIZE(ANDROID_SENSOR_WHITE_LEVEL,
            &QCMaxRawValue, 1);

    static const int32_t blackLevelPattern[4] = {
            QCBlackLevel, QCBlackLevel,
            QCBlackLevel, QCBlackLevel
    };
    ADD_OR_SIZE(ANDROID_SENSOR_BLACK_LEVEL_PATTERN,
            blackLevelPattern, sizeof(blackLevelPattern)/sizeof(int32_t));

#endif
    //TODO: sensor color calibration fields

    // android.flash
    static const uint8_t flashAvailable = 0;
    ADD_OR_SIZE(ANDROID_FLASH_AVAILABLE, &flashAvailable, 1);

    static const int64_t flashChargeDuration = 0;
    ADD_OR_SIZE(ANDROID_FLASH_CHARGE_DURATION, &flashChargeDuration, 1);

    // android.tonemap

    static const int32_t tonemapCurvePoints = 128;
    ADD_OR_SIZE(ANDROID_TONEMAP_MAX_CURVE_POINTS, &tonemapCurvePoints, 1);

#if 1
    // android.scaler
    ADD_OR_SIZE(ANDROID_SCALER_AVAILABLE_FORMATS,
            QCAvailableFormats,
            sizeof(QCAvailableFormats)/sizeof(uint32_t));

    ADD_OR_SIZE(ANDROID_SCALER_AVAILABLE_RAW_SIZES,
            QCAvailableRawSizes,
            sizeof(QCAvailableRawSizes)/sizeof(uint32_t));

    ADD_OR_SIZE(ANDROID_SCALER_AVAILABLE_RAW_MIN_DURATIONS,
            QCAvailableRawMinDurations,
            sizeof(QCAvailableRawMinDurations)/sizeof(uint64_t));

    ADD_OR_SIZE(ANDROID_SCALER_AVAILABLE_PROCESSED_SIZES,
            QCAvailableProcessedSizes,
            sizeof(QCAvailableProcessedSizes)/sizeof(uint32_t));

    ADD_OR_SIZE(ANDROID_SCALER_AVAILABLE_PROCESSED_MIN_DURATIONS,
            QCAvailableProcessedMinDurations,
            sizeof(QCAvailableProcessedMinDurations)/sizeof(uint64_t));

    ADD_OR_SIZE(ANDROID_SCALER_AVAILABLE_JPEG_SIZES,
            QCAvailableJpegSizes,
            sizeof(QCAvailableJpegSizes)/sizeof(uint32_t));

    ADD_OR_SIZE(ANDROID_SCALER_AVAILABLE_JPEG_MIN_DURATIONS,
            QCAvailableJpegMinDurations,
            sizeof(QCAvailableJpegMinDurations)/sizeof(uint64_t));

    static const float maxZoom = 10;
    ADD_OR_SIZE(ANDROID_SCALER_AVAILABLE_MAX_ZOOM,
            &maxZoom, 1);

    // android.jpeg

    static const int32_t jpegThumbnailSizes[] = {
            160, 120,
            320, 240
     };
    ADD_OR_SIZE(ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES,
            jpegThumbnailSizes, sizeof(jpegThumbnailSizes)/sizeof(int32_t));

    static const int32_t jpegMaxSize = 300000;
    ADD_OR_SIZE(ANDROID_JPEG_MAX_SIZE, &jpegMaxSize, 1);

    // android.stats

    static const uint8_t availableFaceDetectModes[] = {
            ANDROID_STATS_FACE_DETECTION_OFF
    };
    ADD_OR_SIZE(ANDROID_STATS_AVAILABLE_FACE_DETECT_MODES,
            availableFaceDetectModes,
            sizeof(availableFaceDetectModes));

    static const int32_t maxFaceCount = 0;
    ADD_OR_SIZE(ANDROID_STATS_MAX_FACE_COUNT,
            &maxFaceCount, 1);

    static const int32_t histogramSize = 64;
    ADD_OR_SIZE(ANDROID_STATS_HISTOGRAM_BUCKET_COUNT,
            &histogramSize, 1);

    static const int32_t maxHistogramCount = 1000;
    ADD_OR_SIZE(ANDROID_STATS_MAX_HISTOGRAM_COUNT,
            &maxHistogramCount, 1);

    static const int32_t sharpnessMapSize[2] = {64, 64};
    ADD_OR_SIZE(ANDROID_STATS_SHARPNESS_MAP_SIZE,
            sharpnessMapSize, sizeof(sharpnessMapSize)/sizeof(int32_t));

    static const int32_t maxSharpnessMapValue = 1000;
    ADD_OR_SIZE(ANDROID_STATS_MAX_SHARPNESS_MAP_VALUE,
            &maxSharpnessMapValue, 1);

    // android.control

    static const uint8_t availableSceneModes[] = {
            ANDROID_CONTROL_SCENE_MODE_UNSUPPORTED
    };
    ADD_OR_SIZE(ANDROID_CONTROL_AVAILABLE_SCENE_MODES,
            availableSceneModes, sizeof(availableSceneModes));

    static const uint8_t availableEffects[] = {
            ANDROID_CONTROL_EFFECT_OFF
    };
    ADD_OR_SIZE(ANDROID_CONTROL_AVAILABLE_EFFECTS,
            availableEffects, sizeof(availableEffects));

    int32_t max3aRegions = 0;
    ADD_OR_SIZE(ANDROID_CONTROL_MAX_REGIONS,
            &max3aRegions, 1);

    static const uint8_t availableAeModes[] = {
            ANDROID_CONTROL_AE_OFF,
            ANDROID_CONTROL_AE_ON
    };
    ADD_OR_SIZE(ANDROID_CONTROL_AE_AVAILABLE_MODES,
            availableAeModes, sizeof(availableAeModes));

    static const camera_metadata_rational exposureCompensationStep = {
            1, 3
    };
    ADD_OR_SIZE(ANDROID_CONTROL_AE_EXP_COMPENSATION_STEP,
            &exposureCompensationStep, 1);

    int32_t exposureCompensationRange[] = {-9, 9};
    ADD_OR_SIZE(ANDROID_CONTROL_AE_EXP_COMPENSATION_RANGE,
            exposureCompensationRange,
            sizeof(exposureCompensationRange)/sizeof(int32_t));

    static const int32_t availableTargetFpsRanges[] = {
            5, 30, 15, 30
    };
    ADD_OR_SIZE(ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES,
            availableTargetFpsRanges,
            sizeof(availableTargetFpsRanges)/sizeof(int32_t));

    static const uint8_t availableAntibandingModes[] = {
            ANDROID_CONTROL_AE_ANTIBANDING_OFF,
            ANDROID_CONTROL_AE_ANTIBANDING_AUTO
    };
    ADD_OR_SIZE(ANDROID_CONTROL_AE_AVAILABLE_ANTIBANDING_MODES,
            availableAntibandingModes, sizeof(availableAntibandingModes));

    static const uint8_t availableAwbModes[] = {
            ANDROID_CONTROL_AWB_OFF,
            ANDROID_CONTROL_AWB_AUTO,
            ANDROID_CONTROL_AWB_INCANDESCENT,
            ANDROID_CONTROL_AWB_FLUORESCENT,
            ANDROID_CONTROL_AWB_DAYLIGHT,
            ANDROID_CONTROL_AWB_SHADE
    };
    ADD_OR_SIZE(ANDROID_CONTROL_AWB_AVAILABLE_MODES,
            availableAwbModes, sizeof(availableAwbModes));

    static const uint8_t availableAfModes[] = {
            ANDROID_CONTROL_AF_OFF
    };
    ADD_OR_SIZE(ANDROID_CONTROL_AF_AVAILABLE_MODES,
            availableAfModes, sizeof(availableAfModes));

    static const uint8_t availableVstabModes[] = {
            ANDROID_CONTROL_VIDEO_STABILIZATION_OFF
    };
    ADD_OR_SIZE(ANDROID_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES,
            availableVstabModes, sizeof(availableVstabModes));
#endif
#undef ADD_OR_SIZE
    /** Allocate metadata if sizing */
    if (sizeRequest) {
        ALOGV("Allocating %d entries, %d extra bytes for "
                "static camera info",
                entryCount, dataCount);
        *info = allocate_camera_metadata(entryCount, dataCount);
        if (*info == NULL) {
            ALOGE("Unable to allocate camera static info"
                    "(%d entries, %d bytes extra data)",
                    entryCount, dataCount);
            return NO_MEMORY;
        }
    }
    return OK;
}

extern "C" status_t constructDefaultRequest(int request_template,
        camera_metadata_t **request, bool sizeRequest)
{
    size_t entryCount = 0;
    size_t dataCount = 0;
    status_t ret;

#define ADD_OR_SIZE( tag, data, count ) \
    if ( ( ret = addOrSize(*request, sizeRequest, &entryCount, &dataCount, \
            tag, data, count) ) != OK ) return ret

    static const int64_t USEC = 1000LL;
    static const int64_t MSEC = USEC * 1000LL;
    static const int64_t SEC = MSEC * 1000LL;

    /** android.request */

    static const uint8_t metadataMode = ANDROID_REQUEST_METADATA_NONE;
    ADD_OR_SIZE(ANDROID_REQUEST_METADATA_MODE, &metadataMode, 1);

    static const int32_t id = 0;
    ADD_OR_SIZE(ANDROID_REQUEST_ID, &id, 1);

    static const int32_t frameCount = 0;
    ADD_OR_SIZE(ANDROID_REQUEST_FRAME_COUNT, &frameCount, 1);

    // OUTPUT_STREAMS set by user
    entryCount += 1;
    dataCount += 5; // TODO: Should be maximum stream number

    /** android.lens */

    static const float focusDistance = 0;
    ADD_OR_SIZE(ANDROID_LENS_FOCUS_DISTANCE, &focusDistance, 1);

    static const float aperture = 2.8f;
    ADD_OR_SIZE(ANDROID_LENS_APERTURE, &aperture, 1);

    static const float focalLength = 5.0f;
    ADD_OR_SIZE(ANDROID_LENS_FOCAL_LENGTH, &focalLength, 1);

    static const float filterDensity = 0;
    ADD_OR_SIZE(ANDROID_LENS_FILTER_DENSITY, &filterDensity, 1);

    static const uint8_t opticalStabilizationMode =
            ANDROID_LENS_OPTICAL_STABILIZATION_OFF;
    ADD_OR_SIZE(ANDROID_LENS_OPTICAL_STABILIZATION_MODE,
            &opticalStabilizationMode, 1);

    // FOCUS_RANGE set only in frame

    /** android.sensor */

    static const int64_t exposureTime = 10 * MSEC;
    ADD_OR_SIZE(ANDROID_SENSOR_EXPOSURE_TIME, &exposureTime, 1);

    static const int64_t frameDuration = 33333333L; // 1/30 s
    ADD_OR_SIZE(ANDROID_SENSOR_FRAME_DURATION, &frameDuration, 1);

    static const int32_t sensitivity = 100;
    ADD_OR_SIZE(ANDROID_SENSOR_SENSITIVITY, &sensitivity, 1);

    // TIMESTAMP set only in frame

    /** android.flash */

    static const uint8_t flashMode = ANDROID_FLASH_OFF;
    ADD_OR_SIZE(ANDROID_FLASH_MODE, &flashMode, 1);

    static const uint8_t flashPower = 10;
    ADD_OR_SIZE(ANDROID_FLASH_FIRING_POWER, &flashPower, 1);

    static const int64_t firingTime = 0;
    ADD_OR_SIZE(ANDROID_FLASH_FIRING_TIME, &firingTime, 1);

    /** Processing block modes */
    uint8_t hotPixelMode = 0;
    uint8_t demosaicMode = 0;
    uint8_t noiseMode = 0;
    uint8_t shadingMode = 0;
    uint8_t geometricMode = 0;
    uint8_t colorMode = 0;
    uint8_t tonemapMode = 0;
    uint8_t edgeMode = 0;
    switch (request_template) {
      case CAMERA2_TEMPLATE_PREVIEW:
        hotPixelMode = ANDROID_PROCESSING_FAST;
        demosaicMode = ANDROID_PROCESSING_FAST;
        noiseMode = ANDROID_PROCESSING_FAST;
        shadingMode = ANDROID_PROCESSING_FAST;
        geometricMode = ANDROID_PROCESSING_FAST;
        colorMode = ANDROID_PROCESSING_FAST;
        tonemapMode = ANDROID_PROCESSING_FAST;
        edgeMode = ANDROID_PROCESSING_FAST;
        break;
      case CAMERA2_TEMPLATE_STILL_CAPTURE:
        hotPixelMode = ANDROID_PROCESSING_HIGH_QUALITY;
        demosaicMode = ANDROID_PROCESSING_HIGH_QUALITY;
        noiseMode = ANDROID_PROCESSING_HIGH_QUALITY;
        shadingMode = ANDROID_PROCESSING_HIGH_QUALITY;
        geometricMode = ANDROID_PROCESSING_HIGH_QUALITY;
        colorMode = ANDROID_PROCESSING_HIGH_QUALITY;
        tonemapMode = ANDROID_PROCESSING_HIGH_QUALITY;
        edgeMode = ANDROID_PROCESSING_HIGH_QUALITY;
        break;
      case CAMERA2_TEMPLATE_VIDEO_RECORD:
        hotPixelMode = ANDROID_PROCESSING_FAST;
        demosaicMode = ANDROID_PROCESSING_FAST;
        noiseMode = ANDROID_PROCESSING_FAST;
        shadingMode = ANDROID_PROCESSING_FAST;
        geometricMode = ANDROID_PROCESSING_FAST;
        colorMode = ANDROID_PROCESSING_FAST;
        tonemapMode = ANDROID_PROCESSING_FAST;
        edgeMode = ANDROID_PROCESSING_FAST;
        break;
      case CAMERA2_TEMPLATE_VIDEO_SNAPSHOT:
        hotPixelMode = ANDROID_PROCESSING_HIGH_QUALITY;
        demosaicMode = ANDROID_PROCESSING_HIGH_QUALITY;
        noiseMode = ANDROID_PROCESSING_HIGH_QUALITY;
        shadingMode = ANDROID_PROCESSING_HIGH_QUALITY;
        geometricMode = ANDROID_PROCESSING_HIGH_QUALITY;
        colorMode = ANDROID_PROCESSING_HIGH_QUALITY;
        tonemapMode = ANDROID_PROCESSING_HIGH_QUALITY;
        edgeMode = ANDROID_PROCESSING_HIGH_QUALITY;
        break;
      case CAMERA2_TEMPLATE_ZERO_SHUTTER_LAG:
        hotPixelMode = ANDROID_PROCESSING_HIGH_QUALITY;
        demosaicMode = ANDROID_PROCESSING_HIGH_QUALITY;
        noiseMode = ANDROID_PROCESSING_HIGH_QUALITY;
        shadingMode = ANDROID_PROCESSING_HIGH_QUALITY;
        geometricMode = ANDROID_PROCESSING_HIGH_QUALITY;
        colorMode = ANDROID_PROCESSING_HIGH_QUALITY;
        tonemapMode = ANDROID_PROCESSING_HIGH_QUALITY;
        edgeMode = ANDROID_PROCESSING_HIGH_QUALITY;
        break;
      default:
        hotPixelMode = ANDROID_PROCESSING_FAST;
        demosaicMode = ANDROID_PROCESSING_FAST;
        noiseMode = ANDROID_PROCESSING_FAST;
        shadingMode = ANDROID_PROCESSING_FAST;
        geometricMode = ANDROID_PROCESSING_FAST;
        colorMode = ANDROID_PROCESSING_FAST;
        tonemapMode = ANDROID_PROCESSING_FAST;
        edgeMode = ANDROID_PROCESSING_FAST;
        break;
    }
    ADD_OR_SIZE(ANDROID_HOT_PIXEL_MODE, &hotPixelMode, 1);
    ADD_OR_SIZE(ANDROID_DEMOSAIC_MODE, &demosaicMode, 1);
    ADD_OR_SIZE(ANDROID_NOISE_MODE, &noiseMode, 1);
    ADD_OR_SIZE(ANDROID_SHADING_MODE, &shadingMode, 1);
    ADD_OR_SIZE(ANDROID_GEOMETRIC_MODE, &geometricMode, 1);
    ADD_OR_SIZE(ANDROID_COLOR_MODE, &colorMode, 1);
    ADD_OR_SIZE(ANDROID_TONEMAP_MODE, &tonemapMode, 1);
    ADD_OR_SIZE(ANDROID_EDGE_MODE, &edgeMode, 1);

    /** android.noise */
    static const uint8_t noiseStrength = 5;
    ADD_OR_SIZE(ANDROID_NOISE_STRENGTH, &noiseStrength, 1);

    /** android.color */
    static const float colorTransform[9] = {
        1.0f, 0.f, 0.f,
        0.f, 1.f, 0.f,
        0.f, 0.f, 1.f
    };
    ADD_OR_SIZE(ANDROID_COLOR_TRANSFORM, colorTransform, 9);

    /** android.tonemap */
    static const float tonemapCurve[4] = {
        0.f, 0.f,
        1.f, 1.f
    };
    ADD_OR_SIZE(ANDROID_TONEMAP_CURVE_RED, tonemapCurve, 4);
    ADD_OR_SIZE(ANDROID_TONEMAP_CURVE_GREEN, tonemapCurve, 4);
    ADD_OR_SIZE(ANDROID_TONEMAP_CURVE_BLUE, tonemapCurve, 4);

    /** android.edge */
    static const uint8_t edgeStrength = 5;
    ADD_OR_SIZE(ANDROID_EDGE_STRENGTH, &edgeStrength, 1);

    /** android.scaler */
    static const int32_t cropRegion[3] = {
        0, 0, /*kResolution[0]*/
    };
    ADD_OR_SIZE(ANDROID_SCALER_CROP_REGION, cropRegion, 3);

    /** android.jpeg */
    static const int32_t jpegQuality = 80;
    ADD_OR_SIZE(ANDROID_JPEG_QUALITY, &jpegQuality, 1);

    static const int32_t thumbnailSize[2] = {
        640, 480
    };
    ADD_OR_SIZE(ANDROID_JPEG_THUMBNAIL_SIZE, thumbnailSize, 2);

    static const int32_t thumbnailQuality = 80;
    ADD_OR_SIZE(ANDROID_JPEG_THUMBNAIL_QUALITY, &thumbnailQuality, 1);

    static const double gpsCoordinates[2] = {
        0, 0
    };
    ADD_OR_SIZE(ANDROID_JPEG_GPS_COORDINATES, gpsCoordinates, 2);

    static const uint8_t gpsProcessingMethod[32] = "None";
    ADD_OR_SIZE(ANDROID_JPEG_GPS_PROCESSING_METHOD, gpsProcessingMethod, 32);

    static const int64_t gpsTimestamp = 0;
    ADD_OR_SIZE(ANDROID_JPEG_GPS_TIMESTAMP, &gpsTimestamp, 1);

    static const int32_t jpegOrientation = 0;
    ADD_OR_SIZE(ANDROID_JPEG_ORIENTATION, &jpegOrientation, 1);

    /** android.stats */

    static const uint8_t faceDetectMode = ANDROID_STATS_FACE_DETECTION_OFF;
    ADD_OR_SIZE(ANDROID_STATS_FACE_DETECT_MODE, &faceDetectMode, 1);

    static const uint8_t histogramMode = ANDROID_STATS_OFF;
    ADD_OR_SIZE(ANDROID_STATS_HISTOGRAM_MODE, &histogramMode, 1);

    static const uint8_t sharpnessMapMode = ANDROID_STATS_OFF;
    ADD_OR_SIZE(ANDROID_STATS_SHARPNESS_MAP_MODE, &sharpnessMapMode, 1);

    // faceRectangles, faceScores, faceLandmarks, faceIds, histogram,
    // sharpnessMap only in frames

    /** android.control */

    uint8_t controlIntent = 0;
    switch (request_template) {
      case CAMERA2_TEMPLATE_PREVIEW:
        controlIntent = ANDROID_CONTROL_INTENT_PREVIEW;
        break;
      case CAMERA2_TEMPLATE_STILL_CAPTURE:
        controlIntent = ANDROID_CONTROL_INTENT_STILL_CAPTURE;
        break;
      case CAMERA2_TEMPLATE_VIDEO_RECORD:
        controlIntent = ANDROID_CONTROL_INTENT_VIDEO_RECORD;
        break;
      case CAMERA2_TEMPLATE_VIDEO_SNAPSHOT:
        controlIntent = ANDROID_CONTROL_INTENT_VIDEO_SNAPSHOT;
        break;
      case CAMERA2_TEMPLATE_ZERO_SHUTTER_LAG:
        controlIntent = ANDROID_CONTROL_INTENT_ZERO_SHUTTER_LAG;
        break;
      default:
        controlIntent = ANDROID_CONTROL_INTENT_CUSTOM;
        break;
    }
    ADD_OR_SIZE(ANDROID_CONTROL_CAPTURE_INTENT, &controlIntent, 1);

    static const uint8_t controlMode = ANDROID_CONTROL_AUTO;
    ADD_OR_SIZE(ANDROID_CONTROL_MODE, &controlMode, 1);

    static const uint8_t effectMode = ANDROID_CONTROL_EFFECT_OFF;
    ADD_OR_SIZE(ANDROID_CONTROL_EFFECT_MODE, &effectMode, 1);

    static const uint8_t sceneMode = ANDROID_CONTROL_SCENE_MODE_FACE_PRIORITY;
    ADD_OR_SIZE(ANDROID_CONTROL_SCENE_MODE, &sceneMode, 1);

    static const uint8_t aeMode = ANDROID_CONTROL_AE_ON_AUTO_FLASH;
    ADD_OR_SIZE(ANDROID_CONTROL_AE_MODE, &aeMode, 1);

    const unsigned int kResolution[2]  = {640, 480};
    static const int32_t controlRegions[5] = {
        0, 0, kResolution[0], kResolution[1], 1000
    };
    ADD_OR_SIZE(ANDROID_CONTROL_AE_REGIONS, controlRegions, 5);

    static const int32_t aeExpCompensation = 0;
    ADD_OR_SIZE(ANDROID_CONTROL_AE_EXP_COMPENSATION, &aeExpCompensation, 1);

    static const int32_t aeTargetFpsRange[2] = {
        10, 30
    };
    ADD_OR_SIZE(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, aeTargetFpsRange, 2);

    static const uint8_t aeAntibandingMode =
            ANDROID_CONTROL_AE_ANTIBANDING_AUTO;
    ADD_OR_SIZE(ANDROID_CONTROL_AE_ANTIBANDING_MODE, &aeAntibandingMode, 1);

    static const uint8_t awbMode =
            ANDROID_CONTROL_AWB_AUTO;
    ADD_OR_SIZE(ANDROID_CONTROL_AWB_MODE, &awbMode, 1);

    ADD_OR_SIZE(ANDROID_CONTROL_AWB_REGIONS, controlRegions, 5);

    uint8_t afMode = 0;
    switch (request_template) {
      case CAMERA2_TEMPLATE_PREVIEW:
        afMode = ANDROID_CONTROL_AF_AUTO;
        break;
      case CAMERA2_TEMPLATE_STILL_CAPTURE:
        afMode = ANDROID_CONTROL_AF_AUTO;
        break;
      case CAMERA2_TEMPLATE_VIDEO_RECORD:
        afMode = ANDROID_CONTROL_AF_CONTINUOUS_VIDEO;
        break;
      case CAMERA2_TEMPLATE_VIDEO_SNAPSHOT:
        afMode = ANDROID_CONTROL_AF_CONTINUOUS_VIDEO;
        break;
      case CAMERA2_TEMPLATE_ZERO_SHUTTER_LAG:
        afMode = ANDROID_CONTROL_AF_CONTINUOUS_PICTURE;
        break;
      default:
        afMode = ANDROID_CONTROL_AF_AUTO;
        break;
    }
    ADD_OR_SIZE(ANDROID_CONTROL_AF_MODE, &afMode, 1);

    ADD_OR_SIZE(ANDROID_CONTROL_AF_REGIONS, controlRegions, 5);

    static const uint8_t vstabMode = ANDROID_CONTROL_VIDEO_STABILIZATION_OFF;
    ADD_OR_SIZE(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstabMode, 1);

    // aeState, awbState, afState only in frame

    /** Allocate metadata if sizing */
    if (sizeRequest) {
        ALOGV("Allocating %d entries, %d extra bytes for "
                "request template type %d",
                entryCount, dataCount, request_template);
        *request = allocate_camera_metadata(entryCount, dataCount);
        if (*request == NULL) {
            ALOGE("Unable to allocate new request template type %d "
                    "(%d entries, %d bytes extra data)", request_template,
                    entryCount, dataCount);
            return NO_MEMORY;
        }
    }
    return OK;
#undef ADD_OR_SIZE
}

};
