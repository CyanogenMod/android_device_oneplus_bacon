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

#ifndef __QCAMERA_TYPES_H__
#define __QCAMERA_TYPES_H__

#include <stdint.h>
#include <pthread.h>
#include <inttypes.h>
#include <media/msmb_camera.h>

#define CAM_MAX_NUM_BUFS_PER_STREAM 24
#define MAX_METADATA_PAYLOAD_SIZE 1024

#define CEILING64(X) (((X) + 0x0003F) & 0xFFFFFFC0)
#define CEILING32(X) (((X) + 0x0001F) & 0xFFFFFFE0)
#define CEILING16(X) (((X) + 0x000F) & 0xFFF0)
#define CEILING4(X)  (((X) + 0x0003) & 0xFFFC)
#define CEILING2(X)  (((X) + 0x0001) & 0xFFFE)

#define MAX_ZOOMS_CNT 79
#define MAX_SIZES_CNT 24
#define MAX_EXP_BRACKETING_LENGTH 32
#define MAX_ROI 5
#define MAX_STREAM_NUM_IN_BUNDLE 4
#define MAX_NUM_STREAMS          8
#define CHROMATIX_SIZE 21292
#define COMMONCHROMATIX_SIZE 42044
#define AFTUNE_SIZE 2000
#define MAX_SCALE_SIZES_CNT 8
#define MAX_SAMP_DECISION_CNT     64

#define MAX_ISP_DATA_SIZE 9000
#define MAX_PP_DATA_SIZE 2000
#define MAX_AE_STATS_DATA_SIZE  1000
#define MAX_AWB_STATS_DATA_SIZE 1000
#define MAX_AF_STATS_DATA_SIZE  1000



#define TUNING_DATA_VERSION        1
#define TUNING_SENSOR_DATA_MAX     0x10000 /*(need value from sensor team)*/
#define TUNING_VFE_DATA_MAX        0x10000 /*(need value from vfe team)*/
#define TUNING_CPP_DATA_MAX        0x10000 /*(need value from pproc team)*/
#define TUNING_CAC_DATA_MAX        0x10000 /*(need value from imglib team)*/
#define TUNING_DATA_MAX            (TUNING_SENSOR_DATA_MAX + \
                                   TUNING_VFE_DATA_MAX + TUNING_CPP_DATA_MAX + \
                                   TUNING_CAC_DATA_MAX)

#define TUNING_SENSOR_DATA_OFFSET  0
#define TUNING_VFE_DATA_OFFSET     TUNING_SENSOR_DATA_MAX
#define TUNING_CPP_DATA_OFFSET     (TUNING_SENSOR_DATA_MAX + TUNING_VFE_DATA_MAX)
#define TUNING_CAC_DATA_OFFSET     (TUNING_SENSOR_DATA_MAX + \
                                   TUNING_VFE_DATA_MAX + TUNING_CPP_DATA_MAX)
#define MAX_ISP_DATA_SIZE 9000
#define MAX_PP_DATA_SIZE 2000
#define MAX_STATS_DATA_SIZE 4000



typedef enum {
    CAM_HAL_V1 = 1,
    CAM_HAL_V3 = 3
} cam_hal_version_t;

typedef enum {
    CAM_STATUS_SUCCESS,       /* Operation Succeded */
    CAM_STATUS_FAILED,        /* Failure in doing operation */
    CAM_STATUS_INVALID_PARM,  /* Inavlid parameter provided */
    CAM_STATUS_NOT_SUPPORTED, /* Parameter/operation not supported */
    CAM_STATUS_ACCEPTED,      /* Parameter accepted */
    CAM_STATUS_MAX,
} cam_status_t;

typedef enum {
    CAM_POSITION_BACK,
    CAM_POSITION_FRONT
} cam_position_t;

typedef enum {
    CAM_FORMAT_JPEG = 0,
    CAM_FORMAT_YUV_420_NV12 = 1,
    CAM_FORMAT_YUV_420_NV21,
    CAM_FORMAT_YUV_420_NV21_ADRENO,
    CAM_FORMAT_YUV_420_YV12,
    CAM_FORMAT_YUV_422_NV16,
    CAM_FORMAT_YUV_422_NV61,
    CAM_FORMAT_YUV_420_NV12_VENUS,

    /* Please note below are the defintions for raw image.
     * Any format other than raw image format should be declared
     * before this line!!!!!!!!!!!!! */

    /* Note: For all raw formats, each scanline needs to be 16 bytes aligned */

    /* Packed YUV/YVU raw format, 16 bpp: 8 bits Y and 8 bits UV.
     * U and V are interleaved with Y: YUYV or YVYV */
    CAM_FORMAT_YUV_RAW_8BIT_YUYV,
    CAM_FORMAT_YUV_RAW_8BIT_YVYU,
    CAM_FORMAT_YUV_RAW_8BIT_UYVY,
    CAM_FORMAT_YUV_RAW_8BIT_VYUY,

    /* QCOM RAW formats where data is packed into 64bit word.
     * 8BPP: 1 64-bit word contains 8 pixels p0 - p7, where p0 is
     *       stored at LSB.
     * 10BPP: 1 64-bit word contains 6 pixels p0 - p5, where most
     *       significant 4 bits are set to 0. P0 is stored at LSB.
     * 12BPP: 1 64-bit word contains 5 pixels p0 - p4, where most
     *       significant 4 bits are set to 0. P0 is stored at LSB. */
    CAM_FORMAT_BAYER_QCOM_RAW_8BPP_GBRG,
    CAM_FORMAT_BAYER_QCOM_RAW_8BPP_GRBG,
    CAM_FORMAT_BAYER_QCOM_RAW_8BPP_RGGB,
    CAM_FORMAT_BAYER_QCOM_RAW_8BPP_BGGR,
    CAM_FORMAT_BAYER_QCOM_RAW_10BPP_GBRG,
    CAM_FORMAT_BAYER_QCOM_RAW_10BPP_GRBG,
    CAM_FORMAT_BAYER_QCOM_RAW_10BPP_RGGB,
    CAM_FORMAT_BAYER_QCOM_RAW_10BPP_BGGR,
    CAM_FORMAT_BAYER_QCOM_RAW_12BPP_GBRG,
    CAM_FORMAT_BAYER_QCOM_RAW_12BPP_GRBG,
    CAM_FORMAT_BAYER_QCOM_RAW_12BPP_RGGB,
    CAM_FORMAT_BAYER_QCOM_RAW_12BPP_BGGR,
    /* MIPI RAW formats based on MIPI CSI-2 specifiction.
     * 8BPP: Each pixel occupies one bytes, starting at LSB.
     *       Output with of image has no restrictons.
     * 10BPP: Four pixels are held in every 5 bytes. The output
     *       with of image must be a multiple of 4 pixels.
     * 12BPP: Two pixels are held in every 3 bytes. The output
     *       width of image must be a multiple of 2 pixels. */
    CAM_FORMAT_BAYER_MIPI_RAW_8BPP_GBRG,
    CAM_FORMAT_BAYER_MIPI_RAW_8BPP_GRBG,
    CAM_FORMAT_BAYER_MIPI_RAW_8BPP_RGGB,
    CAM_FORMAT_BAYER_MIPI_RAW_8BPP_BGGR,
    CAM_FORMAT_BAYER_MIPI_RAW_10BPP_GBRG,
    CAM_FORMAT_BAYER_MIPI_RAW_10BPP_GRBG,
    CAM_FORMAT_BAYER_MIPI_RAW_10BPP_RGGB,
    CAM_FORMAT_BAYER_MIPI_RAW_10BPP_BGGR,
    CAM_FORMAT_BAYER_MIPI_RAW_12BPP_GBRG,
    CAM_FORMAT_BAYER_MIPI_RAW_12BPP_GRBG,
    CAM_FORMAT_BAYER_MIPI_RAW_12BPP_RGGB,
    CAM_FORMAT_BAYER_MIPI_RAW_12BPP_BGGR,
    /* Ideal raw formats where image data has gone through black
     * correction, lens rolloff, demux/channel gain, bad pixel
     * correction, and ABF.
     * Ideal raw formats could output any of QCOM_RAW and MIPI_RAW
     * formats, plus plain8 8bbp, plain16 800, plain16 10bpp, and
     * plain 16 12bpp */
    CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_8BPP_GBRG,
    CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_8BPP_GRBG,
    CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_8BPP_RGGB,
    CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_8BPP_BGGR,
    CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_10BPP_GBRG,
    CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_10BPP_GRBG,
    CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_10BPP_RGGB,
    CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_10BPP_BGGR,
    CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_12BPP_GBRG,
    CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_12BPP_GRBG,
    CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_12BPP_RGGB,
    CAM_FORMAT_BAYER_IDEAL_RAW_QCOM_12BPP_BGGR,
    CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_8BPP_GBRG,
    CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_8BPP_GRBG,
    CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_8BPP_RGGB,
    CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_8BPP_BGGR,
    CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_10BPP_GBRG,
    CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_10BPP_GRBG,
    CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_10BPP_RGGB,
    CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_10BPP_BGGR,
    CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_12BPP_GBRG,
    CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_12BPP_GRBG,
    CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_12BPP_RGGB,
    CAM_FORMAT_BAYER_IDEAL_RAW_MIPI_12BPP_BGGR,
    CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN8_8BPP_GBRG,
    CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN8_8BPP_GRBG,
    CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN8_8BPP_RGGB,
    CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN8_8BPP_BGGR,
    CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_8BPP_GBRG,
    CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_8BPP_GRBG,
    CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_8BPP_RGGB,
    CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_8BPP_BGGR,
    CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_10BPP_GBRG,
    CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_10BPP_GRBG,
    CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_10BPP_RGGB,
    CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_10BPP_BGGR,
    CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_12BPP_GBRG,
    CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_12BPP_GRBG,
    CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_12BPP_RGGB,
    CAM_FORMAT_BAYER_IDEAL_RAW_PLAIN16_12BPP_BGGR,

    /* generic 8-bit raw */
    CAM_FORMAT_JPEG_RAW_8BIT,
    CAM_FORMAT_META_RAW_8BIT,

    CAM_FORMAT_MAX
} cam_format_t;

typedef enum {
    /* applies to HAL 1 */
    CAM_STREAM_TYPE_DEFAULT,       /* default stream type */
    CAM_STREAM_TYPE_PREVIEW,       /* preview */
    CAM_STREAM_TYPE_POSTVIEW,      /* postview */
    CAM_STREAM_TYPE_SNAPSHOT,      /* snapshot */
    CAM_STREAM_TYPE_VIDEO,         /* video */

    /* applies to HAL 3 */
    CAM_STREAM_TYPE_IMPL_DEFINED, /* opaque format: could be display, video enc, ZSL YUV */
    CAM_STREAM_TYPE_YUV,          /* app requested callback stream type */

    /* applies to both HAL 1 and HAL 3 */
    CAM_STREAM_TYPE_METADATA,      /* meta data */
    CAM_STREAM_TYPE_RAW,           /* raw dump from camif */
    CAM_STREAM_TYPE_OFFLINE_PROC,  /* offline process */
    CAM_STREAM_TYPE_MAX,
} cam_stream_type_t;

typedef enum {
    CAM_PAD_NONE = 1,
    CAM_PAD_TO_2 = 2,
    CAM_PAD_TO_4 = 4,
    CAM_PAD_TO_WORD = CAM_PAD_TO_4,
    CAM_PAD_TO_8 = 8,
    CAM_PAD_TO_16 = 16,
    CAM_PAD_TO_32 = 32,
    CAM_PAD_TO_64 = 64,
    CAM_PAD_TO_1K = 1024,
    CAM_PAD_TO_2K = 2048,
    CAM_PAD_TO_4K = 4096,
    CAM_PAD_TO_8K = 8192
} cam_pad_format_t;

typedef enum {
    /* followings are per camera */
    CAM_MAPPING_BUF_TYPE_CAPABILITY,  /* mapping camera capability buffer */
    CAM_MAPPING_BUF_TYPE_PARM_BUF,    /* mapping parameters buffer */

    /* followings are per stream */
    CAM_MAPPING_BUF_TYPE_STREAM_BUF,        /* mapping stream buffers */
    CAM_MAPPING_BUF_TYPE_STREAM_INFO,       /* mapping stream information buffer */
    CAM_MAPPING_BUF_TYPE_OFFLINE_INPUT_BUF, /* mapping offline process input buffer */
    CAM_MAPPING_BUF_TYPE_MAX
} cam_mapping_buf_type;

typedef struct {
    cam_mapping_buf_type type;
    uint32_t stream_id;   /* stream id: valid if STREAM_BUF */
    uint32_t frame_idx;   /* frame index: valid if type is STREAM_BUF */
    int32_t plane_idx;    /* planner index. valid if type is STREAM_BUF.
                           * -1 means all planners shanre the same fd;
                           * otherwise, each planner has its own fd */
    unsigned long cookie; /* could be job_id(uint32_t) to identify mapping job */
    int fd;               /* origin fd */
    uint32_t size;        /* size of the buffer */
} cam_buf_map_type;

typedef struct {
    cam_mapping_buf_type type;
    uint32_t stream_id;   /* stream id: valid if STREAM_BUF */
    uint32_t frame_idx;   /* frame index: valid if STREAM_BUF or HIST_BUF */
    int32_t plane_idx;    /* planner index. valid if type is STREAM_BUF.
                           * -1 means all planners shanre the same fd;
                           * otherwise, each planner has its own fd */
    unsigned long cookie; /* could be job_id(uint32_t) to identify unmapping job */
} cam_buf_unmap_type;

typedef enum {
    CAM_MAPPING_TYPE_FD_MAPPING,
    CAM_MAPPING_TYPE_FD_UNMAPPING,
    CAM_MAPPING_TYPE_MAX
} cam_mapping_type;

typedef struct {
    cam_mapping_type msg_type;
    union {
        cam_buf_map_type buf_map;
        cam_buf_unmap_type buf_unmap;
    } payload;
} cam_sock_packet_t;

typedef enum {
    CAM_MODE_2D = (1<<0),
    CAM_MODE_3D = (1<<1)
} cam_mode_t;

typedef struct {
    uint32_t len;
    uint32_t y_offset;
    uint32_t cbcr_offset;
} cam_sp_len_offset_t;

typedef struct{
    uint32_t len;
    uint32_t offset;
    int32_t offset_x;
    int32_t offset_y;
    int32_t stride;
    int32_t scanline;
    int32_t width;    /* width without padding */
    int32_t height;   /* height without padding */
} cam_mp_len_offset_t;

typedef struct {
    uint32_t width_padding;
    uint32_t height_padding;
    uint32_t plane_padding;
} cam_padding_info_t;

typedef struct {
    int num_planes;
    union {
        cam_sp_len_offset_t sp;
        cam_mp_len_offset_t mp[VIDEO_MAX_PLANES];
    };
    uint32_t frame_len;
} cam_frame_len_offset_t;

typedef struct {
    int32_t width;
    int32_t height;
} cam_dimension_t;

typedef struct {
    cam_frame_len_offset_t plane_info;
} cam_stream_buf_plane_info_t;

typedef struct {
    float min_fps;
    float max_fps;
} cam_fps_range_t;

typedef enum {
    CAM_HFR_MODE_OFF,
    CAM_HFR_MODE_60FPS,
    CAM_HFR_MODE_90FPS,
    CAM_HFR_MODE_120FPS,
    CAM_HFR_MODE_150FPS,
    CAM_HFR_MODE_MAX
} cam_hfr_mode_t;

typedef struct {
    cam_hfr_mode_t mode;
    cam_dimension_t dim;
    uint8_t frame_skip;
    uint8_t livesnapshot_sizes_tbl_cnt;                     /* livesnapshot sizes table size */
    cam_dimension_t livesnapshot_sizes_tbl[MAX_SIZES_CNT];  /* livesnapshot sizes table */
} cam_hfr_info_t;

typedef enum {
    CAM_WB_MODE_AUTO,
    CAM_WB_MODE_CUSTOM,
    CAM_WB_MODE_INCANDESCENT,
    CAM_WB_MODE_FLUORESCENT,
    CAM_WB_MODE_WARM_FLUORESCENT,
    CAM_WB_MODE_DAYLIGHT,
    CAM_WB_MODE_CLOUDY_DAYLIGHT,
    CAM_WB_MODE_TWILIGHT,
    CAM_WB_MODE_SHADE,
    CAM_WB_MODE_OFF,
    CAM_WB_MODE_MAX
} cam_wb_mode_type;

typedef enum {
    CAM_ANTIBANDING_MODE_OFF,
    CAM_ANTIBANDING_MODE_60HZ,
    CAM_ANTIBANDING_MODE_50HZ,
    CAM_ANTIBANDING_MODE_AUTO,
    CAM_ANTIBANDING_MODE_AUTO_50HZ,
    CAM_ANTIBANDING_MODE_AUTO_60HZ,
    CAM_ANTIBANDING_MODE_MAX,
} cam_antibanding_mode_type;

/* Enum Type for different ISO Mode supported */
typedef enum {
    CAM_ISO_MODE_AUTO,
    CAM_ISO_MODE_DEBLUR,
    CAM_ISO_MODE_100,
    CAM_ISO_MODE_200,
    CAM_ISO_MODE_400,
    CAM_ISO_MODE_800,
    CAM_ISO_MODE_1600,
    CAM_ISO_MODE_MAX
} cam_iso_mode_type;

typedef enum {
    CAM_AEC_MODE_FRAME_AVERAGE,
    CAM_AEC_MODE_CENTER_WEIGHTED,
    CAM_AEC_MODE_SPOT_METERING,
    CAM_AEC_MODE_SMART_METERING,
    CAM_AEC_MODE_USER_METERING,
    CAM_AEC_MODE_SPOT_METERING_ADV,
    CAM_AEC_MODE_CENTER_WEIGHTED_ADV,
    CAM_AEC_MODE_MAX
} cam_auto_exposure_mode_type;

typedef enum {
    CAM_AE_MODE_OFF,
    CAM_AE_MODE_ON,
    CAM_AE_MODE_MAX
} cam_ae_mode_type;

typedef enum {
    CAM_FOCUS_ALGO_AUTO,
    CAM_FOCUS_ALGO_SPOT,
    CAM_FOCUS_ALGO_CENTER_WEIGHTED,
    CAM_FOCUS_ALGO_AVERAGE,
    CAM_FOCUS_ALGO_MAX
} cam_focus_algorithm_type;

/* Auto focus mode */
typedef enum {
    CAM_FOCUS_MODE_AUTO,
    CAM_FOCUS_MODE_INFINITY,
    CAM_FOCUS_MODE_MACRO,
    CAM_FOCUS_MODE_FIXED,
    CAM_FOCUS_MODE_EDOF,
    CAM_FOCUS_MODE_CONTINOUS_VIDEO,
    CAM_FOCUS_MODE_CONTINOUS_PICTURE,
    CAM_FOCUS_MODE_MAX
} cam_focus_mode_type;

typedef enum {
    CAM_SCENE_MODE_OFF,
    CAM_SCENE_MODE_AUTO,
    CAM_SCENE_MODE_LANDSCAPE,
    CAM_SCENE_MODE_SNOW,
    CAM_SCENE_MODE_BEACH,
    CAM_SCENE_MODE_SUNSET,
    CAM_SCENE_MODE_NIGHT,
    CAM_SCENE_MODE_PORTRAIT,
    CAM_SCENE_MODE_BACKLIGHT,
    CAM_SCENE_MODE_SPORTS,
    CAM_SCENE_MODE_ANTISHAKE,
    CAM_SCENE_MODE_FLOWERS,
    CAM_SCENE_MODE_CANDLELIGHT,
    CAM_SCENE_MODE_FIREWORKS,
    CAM_SCENE_MODE_PARTY,
    CAM_SCENE_MODE_NIGHT_PORTRAIT,
    CAM_SCENE_MODE_THEATRE,
    CAM_SCENE_MODE_ACTION,
    CAM_SCENE_MODE_AR,
    CAM_SCENE_MODE_FACE_PRIORITY,
    CAM_SCENE_MODE_BARCODE,
    CAM_SCENE_MODE_HDR,
    CAM_SCENE_MODE_MAX
} cam_scene_mode_type;

typedef enum {
    CAM_EFFECT_MODE_OFF,
    CAM_EFFECT_MODE_MONO,
    CAM_EFFECT_MODE_NEGATIVE,
    CAM_EFFECT_MODE_SOLARIZE,
    CAM_EFFECT_MODE_SEPIA,
    CAM_EFFECT_MODE_POSTERIZE,
    CAM_EFFECT_MODE_WHITEBOARD,
    CAM_EFFECT_MODE_BLACKBOARD,
    CAM_EFFECT_MODE_AQUA,
    CAM_EFFECT_MODE_EMBOSS,
    CAM_EFFECT_MODE_SKETCH,
    CAM_EFFECT_MODE_NEON,
    CAM_EFFECT_MODE_MAX
} cam_effect_mode_type;

typedef enum {
    CAM_FLASH_MODE_OFF,
    CAM_FLASH_MODE_AUTO,
    CAM_FLASH_MODE_ON,
    CAM_FLASH_MODE_TORCH,
    CAM_FLASH_MODE_MAX
} cam_flash_mode_t;

typedef enum {
    CAM_AEC_TRIGGER_IDLE,
    CAM_AEC_TRIGGER_START
} cam_aec_trigger_type_t;

typedef enum {
    CAM_AF_TRIGGER_IDLE,
    CAM_AF_TRIGGER_START,
    CAM_AF_TRIGGER_CANCEL
} cam_af_trigger_type_t;

typedef enum {
    CAM_AE_STATE_INACTIVE,
    CAM_AE_STATE_SEARCHING,
    CAM_AE_STATE_CONVERGED,
    CAM_AE_STATE_LOCKED,
    CAM_AE_STATE_FLASH_REQUIRED,
    CAM_AE_STATE_PRECAPTURE
} cam_ae_state_t;

typedef struct  {
    int32_t left;
    int32_t top;
    int32_t width;
    int32_t height;
} cam_rect_t;

typedef struct  {
    cam_rect_t rect;
    int32_t weight; /* weight of the area, valid for focusing/metering areas */
} cam_area_t;

typedef enum {
    CAM_STREAMING_MODE_CONTINUOUS, /* continous streaming */
    CAM_STREAMING_MODE_BURST,      /* burst streaming */
    CAM_STREAMING_MODE_MAX
} cam_streaming_mode_t;

typedef enum {
    IS_TYPE_DIS,
    IS_TYPE_GA_DIS,
    IS_TYPE_EIS_1_0,
    IS_TYPE_EIS_2_0
} cam_is_type_t;

#define CAM_REPROCESS_MASK_TYPE_WNR (1<<0)

/* event from server */
typedef enum {
    CAM_EVENT_TYPE_MAP_UNMAP_DONE  = (1<<0),
    CAM_EVENT_TYPE_AUTO_FOCUS_DONE = (1<<1),
    CAM_EVENT_TYPE_ZOOM_DONE       = (1<<2),
    CAM_EVENT_TYPE_DAEMON_DIED     = (1<<3),
    CAM_EVENT_TYPE_MAX
} cam_event_type_t;

typedef enum {
    CAM_EXP_BRACKETING_OFF,
    CAM_EXP_BRACKETING_ON
} cam_bracket_mode;

typedef struct {
    cam_bracket_mode mode;
    char values[MAX_EXP_BRACKETING_LENGTH];  /* user defined values */
} cam_exp_bracketing_t;

typedef struct {
  unsigned int num_frames;
  cam_exp_bracketing_t exp_val;
} cam_hdr_bracketing_info_t;

typedef struct {
    uint8_t chromatixData[CHROMATIX_SIZE];
    uint8_t snapchromatixData[CHROMATIX_SIZE];
    uint8_t common_chromatixData[COMMONCHROMATIX_SIZE];
} tune_chromatix_t;

typedef struct {
    uint8_t af_tuneData[AFTUNE_SIZE];
} tune_autofocus_t;

typedef struct {
    uint8_t stepsize;
    uint8_t direction;
    uint8_t num_steps;
    uint8_t ttype;
} tune_actuator_t;

typedef struct {
    uint8_t module;
    uint8_t type;
    int32_t value;
} tune_cmd_t;

typedef enum {
    CAM_AEC_ROI_OFF,
    CAM_AEC_ROI_ON
} cam_aec_roi_ctrl_t;

typedef enum {
    CAM_AEC_ROI_BY_INDEX,
    CAM_AEC_ROI_BY_COORDINATE,
} cam_aec_roi_type_t;

typedef struct {
    uint32_t x;
    uint32_t y;
} cam_coordinate_type_t;

typedef struct {
    int32_t numerator;
    int32_t denominator;
} cam_rational_type_t;

typedef struct {
    cam_aec_roi_ctrl_t aec_roi_enable;
    cam_aec_roi_type_t aec_roi_type;
    union {
        cam_coordinate_type_t coordinate[MAX_ROI];
        uint32_t aec_roi_idx[MAX_ROI];
    } cam_aec_roi_position;
} cam_set_aec_roi_t;

typedef struct {
    uint32_t frm_id;
    uint8_t num_roi;
    cam_rect_t roi[MAX_ROI];
    int32_t weight[MAX_ROI];
    uint8_t is_multiwindow;
} cam_roi_info_t;

typedef enum {
    CAM_WAVELET_DENOISE_YCBCR_PLANE,
    CAM_WAVELET_DENOISE_CBCR_ONLY,
    CAM_WAVELET_DENOISE_STREAMLINE_YCBCR,
    CAM_WAVELET_DENOISE_STREAMLINED_CBCR
} cam_denoise_process_type_t;

typedef struct {
    int denoise_enable;
    cam_denoise_process_type_t process_plates;
} cam_denoise_param_t;

#define CAM_FACE_PROCESS_MASK_DETECTION    (1<<0)
#define CAM_FACE_PROCESS_MASK_RECOGNITION  (1<<1)
typedef struct {
    int fd_mode;               /* mask of face process */
    int num_fd;
} cam_fd_set_parm_t;

typedef enum {
    QCAMERA_FD_PREVIEW,
    QCAMERA_FD_SNAPSHOT
}qcamera_face_detect_type_t;

typedef struct {
    int8_t face_id;            /* unique id for face tracking within view unless view changes */
    int8_t score;              /* score of confidence (0, -100) */
    cam_rect_t face_boundary;  /* boundary of face detected */
    cam_coordinate_type_t left_eye_center;  /* coordinate of center of left eye */
    cam_coordinate_type_t right_eye_center; /* coordinate of center of right eye */
    cam_coordinate_type_t mouth_center;     /* coordinate of center of mouth */
    uint8_t smile_degree;      /* smile degree (0, -100) */
    uint8_t smile_confidence;  /* smile confidence (0, 100) */
    uint8_t face_recognised;   /* if face is recognised */
    int8_t gaze_angle;         /* -90 -45 0 45 90 for head left to rigth tilt */
    int8_t updown_dir;         /* up down direction (-90, 90) */
    int8_t leftright_dir;      /* left right direction (-90, 90) */
    int8_t roll_dir;           /* roll direction (-90, 90) */
    int8_t left_right_gaze;    /* left right gaze degree (-50, 50) */
    int8_t top_bottom_gaze;    /* up down gaze degree (-50, 50) */
    uint8_t blink_detected;    /* if blink is detected */
    uint8_t left_blink;        /* left eye blink degeree (0, -100) */
    uint8_t right_blink;       /* right eye blink degree (0, - 100) */
} cam_face_detection_info_t;

typedef struct {
    uint32_t frame_id;                         /* frame index of which faces are detected */
    uint8_t num_faces_detected;                /* number of faces detected */
    cam_face_detection_info_t faces[MAX_ROI];  /* detailed information of faces detected */
    qcamera_face_detect_type_t fd_type;        /* face detect for preview or snapshot frame*/
} cam_face_detection_data_t;

#define CAM_HISTOGRAM_STATS_SIZE 256
typedef struct {
    uint32_t max_hist_value;
    uint32_t hist_buf[CAM_HISTOGRAM_STATS_SIZE]; /* buf holding histogram stats data */
} cam_histogram_data_t;

typedef struct {
    cam_histogram_data_t r_stats;
    cam_histogram_data_t b_stats;
    cam_histogram_data_t gr_stats;
    cam_histogram_data_t gb_stats;
} cam_bayer_hist_stats_t;

typedef enum {
    CAM_HISTOGRAM_TYPE_BAYER,
    CAM_HISTOGRAM_TYPE_YUV
} cam_histogram_type_t;

typedef struct {
    cam_histogram_type_t type;
    union {
        cam_bayer_hist_stats_t bayer_stats;
        cam_histogram_data_t yuv_stats;
    };
} cam_hist_stats_t;

enum cam_focus_distance_index{
  CAM_FOCUS_DISTANCE_NEAR_INDEX,  /* 0 */
  CAM_FOCUS_DISTANCE_OPTIMAL_INDEX,
  CAM_FOCUS_DISTANCE_FAR_INDEX,
  CAM_FOCUS_DISTANCE_MAX_INDEX
};

typedef struct {
  float focus_distance[CAM_FOCUS_DISTANCE_MAX_INDEX];
} cam_focus_distances_info_t;

/* Different autofocus cycle when calling do_autoFocus
 * CAM_AF_COMPLETE_EXISTING_SWEEP: Complete existing sweep
 * if one is ongoing, and lock.
 * CAM_AF_DO_ONE_FULL_SWEEP: Do one full sweep, regardless
 * of the current state, and lock.
 * CAM_AF_START_CONTINUOUS_SWEEP: Start continous sweep.
 * After do_autoFocus, HAL receives an event: CAM_AF_FOCUSED,
 * or CAM_AF_NOT_FOCUSED.
 * cancel_autoFocus stops any lens movement.
 * Each do_autoFocus call only produces 1 FOCUSED/NOT_FOCUSED
 * event, not both.
 */
typedef enum {
    CAM_AF_COMPLETE_EXISTING_SWEEP,
    CAM_AF_DO_ONE_FULL_SWEEP,
    CAM_AF_START_CONTINUOUS_SWEEP
} cam_autofocus_cycle_t;

typedef enum {
    CAM_AF_SCANNING,
    CAM_AF_FOCUSED,
    CAM_AF_NOT_FOCUSED
} cam_autofocus_state_t;

typedef struct {
    cam_autofocus_state_t focus_state;           /* state of focus */
    cam_focus_distances_info_t focus_dist;       /* focus distance */
} cam_auto_focus_data_t;

typedef struct {
  uint32_t is_hdr_scene;
  float    hdr_confidence;
} cam_asd_hdr_scene_data_t;

typedef struct {
    uint32_t stream_id;
    cam_rect_t crop;
} cam_stream_crop_info_t;

typedef struct {
    uint8_t num_of_streams;
    cam_stream_crop_info_t crop_info[MAX_NUM_STREAMS];
} cam_crop_data_t;

typedef enum {
    DO_NOT_NEED_FUTURE_FRAME,
    NEED_FUTURE_FRAME,
} cam_prep_snapshot_state_t;

typedef struct {
    uint32_t min_frame_idx;
    uint32_t max_frame_idx;
} cam_frame_idx_range_t;

typedef enum {
  S_NORMAL = 0,
  S_SCENERY,
  S_PORTRAIT,
  S_PORTRAIT_BACKLIGHT,
  S_SCENERY_BACKLIGHT,
  S_BACKLIGHT,
  S_MAX,
} cam_auto_scene_t;

typedef struct {
   uint32_t meta_frame_id;
} cam_meta_valid_t;

typedef struct {
    cam_flash_mode_t flash_mode;
    float aperture_value;
} cam_sensor_params_t;

typedef struct {
    float exp_time;
    int iso_value;
    uint32_t flash_needed;
} cam_ae_params_t;



typedef struct {
    uint32_t tuning_data_version;
    uint32_t tuning_sensor_data_size;
    uint32_t tuning_vfe_data_size;
    uint32_t tuning_cpp_data_size;
    uint32_t tuning_cac_data_size;
    uint8_t  data[TUNING_DATA_MAX];
}tuning_params_t;

typedef struct {
  uint8_t private_isp_data[MAX_ISP_DATA_SIZE];
} cam_chromatix_lite_isp_t;

typedef struct {
  uint8_t private_pp_data[MAX_PP_DATA_SIZE];
} cam_chromatix_lite_pp_t;

typedef struct {
  uint8_t private_stats_data[MAX_AE_STATS_DATA_SIZE];
} cam_chromatix_lite_ae_stats_t;

typedef struct {
  uint8_t private_stats_data[MAX_AWB_STATS_DATA_SIZE];
} cam_chromatix_lite_awb_stats_t;

typedef struct {
  uint8_t private_stats_data[MAX_AF_STATS_DATA_SIZE];
} cam_chromatix_lite_af_stats_t;

typedef  struct {
    uint8_t is_stats_valid;               /* if histgram data is valid */
    cam_hist_stats_t stats_data;          /* histogram data */

    uint8_t is_faces_valid;               /* if face detection data is valid */
    cam_face_detection_data_t faces_data; /* face detection result */

    uint8_t is_focus_valid;               /* if focus data is valid */
    cam_auto_focus_data_t focus_data;     /* focus data */

    uint8_t is_crop_valid;                /* if crop data is valid */
    cam_crop_data_t crop_data;            /* crop data */

    uint8_t is_prep_snapshot_done_valid;  /* if prep snapshot done is valid */
    cam_prep_snapshot_state_t prep_snapshot_done_state;  /* prepare snapshot done state */

    /* if good frame idx range is valid */
    uint8_t is_good_frame_idx_range_valid;
    /* good frame idx range, make sure:
     * 1. good_frame_idx_range.min_frame_idx > current_frame_idx
     * 2. good_frame_idx_range.min_frame_idx - current_frame_idx < 100 */
    cam_frame_idx_range_t good_frame_idx_range;

    uint32_t is_hdr_scene_data_valid;
    cam_asd_hdr_scene_data_t hdr_scene_data;
    uint8_t is_asd_decision_valid;
    cam_auto_scene_t scene; //scene type as decided by ASD

    char private_metadata[MAX_METADATA_PAYLOAD_SIZE];

    /* AE parameters */
    uint8_t is_ae_params_valid;
    cam_ae_params_t ae_params;

    /* sensor parameters */
    uint8_t is_sensor_params_valid;
    cam_sensor_params_t sensor_params;

    /* Meta valid params */
    uint8_t is_meta_valid;
    cam_meta_valid_t meta_valid_params;

    /*Tuning Data*/
    uint8_t is_tuning_params_valid;
    tuning_params_t tuning_params;

    uint8_t is_chromatix_lite_isp_valid;
    cam_chromatix_lite_isp_t chromatix_lite_isp_data;

    uint8_t is_chromatix_lite_pp_valid;
    cam_chromatix_lite_pp_t chromatix_lite_pp_data;

    uint8_t is_chromatix_lite_ae_stats_valid;
    cam_chromatix_lite_ae_stats_t chromatix_lite_ae_stats_data;

    uint8_t is_chromatix_lite_awb_stats_valid;
    cam_chromatix_lite_awb_stats_t chromatix_lite_awb_stats_data;

    uint8_t is_chromatix_lite_af_stats_valid;
    cam_chromatix_lite_af_stats_t chromatix_lite_af_stats_data;
} cam_metadata_info_t;

typedef enum {
    CAM_INTF_PARM_HAL_VERSION,
    /* common between HAL1 and HAL3 */
    CAM_INTF_PARM_ANTIBANDING,
    CAM_INTF_PARM_EXPOSURE_COMPENSATION,
    CAM_INTF_PARM_AEC_LOCK,
    CAM_INTF_PARM_FPS_RANGE,
    CAM_INTF_PARM_AWB_LOCK,
    CAM_INTF_PARM_WHITE_BALANCE,
    CAM_INTF_PARM_EFFECT,
    CAM_INTF_PARM_BESTSHOT_MODE,
    CAM_INTF_PARM_DIS_ENABLE,
    CAM_INTF_PARM_LED_MODE,
    CAM_INTF_META_HISTOGRAM, /* 10 */
    CAM_INTF_META_FACE_DETECTION,
    CAM_INTF_META_AUTOFOCUS_DATA,

    /* specific to HAl1 */
    CAM_INTF_PARM_QUERY_FLASH4SNAP,
    CAM_INTF_PARM_EXPOSURE,
    CAM_INTF_PARM_SHARPNESS,
    CAM_INTF_PARM_CONTRAST,
    CAM_INTF_PARM_SATURATION,
    CAM_INTF_PARM_BRIGHTNESS,
    CAM_INTF_PARM_ISO,
    CAM_INTF_PARM_ZOOM, /* 20 */
    CAM_INTF_PARM_ROLLOFF,
    CAM_INTF_PARM_MODE,             /* camera mode */
    CAM_INTF_PARM_AEC_ALGO_TYPE,    /* auto exposure algorithm */
    CAM_INTF_PARM_FOCUS_ALGO_TYPE,  /* focus algorithm */
    CAM_INTF_PARM_AEC_ROI,
    CAM_INTF_PARM_AF_ROI,
    CAM_INTF_PARM_FOCUS_MODE,
    CAM_INTF_PARM_SCE_FACTOR,
    CAM_INTF_PARM_FD,
    CAM_INTF_PARM_MCE, /* 30 */
    CAM_INTF_PARM_HFR,
    CAM_INTF_PARM_REDEYE_REDUCTION,
    CAM_INTF_PARM_WAVELET_DENOISE,
    CAM_INTF_PARM_HISTOGRAM,
    CAM_INTF_PARM_ASD_ENABLE,
    CAM_INTF_PARM_RECORDING_HINT,
    CAM_INTF_PARM_HDR,
    CAM_INTF_PARM_MAX_DIMENSION,
    CAM_INTF_PARM_RAW_DIMENSION,
    CAM_INTF_PARM_FRAMESKIP,
    CAM_INTF_PARM_ZSL_MODE,  /* indicating if it's running in ZSL mode */
    CAM_INTF_PARM_HDR_NEED_1X, /* if HDR needs 1x output */ /* 40 */
    CAM_INTF_PARM_LOCK_CAF,
    CAM_INTF_PARM_VIDEO_HDR,
    CAM_INTF_PARM_ROTATION,
    CAM_INTF_PARM_SCALE,
    CAM_INTF_PARM_VT, /* indicating if it's a Video Call Apllication */
    CAM_INTF_META_CROP_DATA,
    CAM_INTF_META_PREP_SNAPSHOT_DONE,
    CAM_INTF_META_GOOD_FRAME_IDX_RANGE,
    CAM_INTF_PARM_GET_CHROMATIX,
    CAM_INTF_PARM_SET_RELOAD_CHROMATIX,
    CAM_INTF_PARM_SET_AUTOFOCUSTUNING,
    CAM_INTF_PARM_GET_AFTUNE,
    CAM_INTF_PARM_SET_RELOAD_AFTUNE,
    CAM_INTF_PARM_SET_VFE_COMMAND,
    CAM_INTF_PARM_SET_PP_COMMAND,
    CAM_INTF_PARM_TINTLESS,

    /* stream based parameters */
    CAM_INTF_PARM_DO_REPROCESS,
    CAM_INTF_PARM_SET_BUNDLE,
    CAM_INTF_PARM_STREAM_FLIP,
    CAM_INTF_PARM_GET_OUTPUT_CROP,

    CAM_INTF_PARM_EZTUNE_CMD,

    /* specific to HAL3 */
    /* Whether the metadata maps to a valid frame number */
    CAM_INTF_META_FRAME_NUMBER_VALID,
    /* COLOR CORRECTION.*/
    CAM_INTF_META_COLOR_CORRECT_MODE,
    /* A transform matrix to chromatically adapt pixels in the CIE XYZ (1931)
     * color space from the scene illuminant to the sRGB-standard D65-illuminant. */
    CAM_INTF_META_COLOR_CORRECT_TRANSFORM, /* 50 */
    /* CONTROL */
//    CAM_INTF_META_REQUEST_ID,
    /* A frame counter set by the framework. Must be maintained unchanged in
     * output frame. */
    CAM_INTF_META_FRAME_NUMBER,
    /* Whether AE is currently updating the sensor exposure and sensitivity
     * fields */
    CAM_INTF_META_AEC_MODE,
    /* List of areas to use for metering */
    CAM_INTF_META_AEC_ROI,
    /* Whether the HAL must trigger precapture metering.*/
    CAM_INTF_META_AEC_PRECAPTURE_TRIGGER,
    /* The ID sent with the latest CAMERA2_TRIGGER_PRECAPTURE_METERING call */
    CAM_INTF_META_AEC_PRECAPTURE_ID,
    /* Current state of AE algorithm */
    CAM_INTF_META_AEC_STATE,
    /* List of areas to use for focus estimation */
    CAM_INTF_META_AF_ROI,
    /* Whether the HAL must trigger autofocus. */
    CAM_INTF_META_AF_TRIGGER,
    /* Current state of AF algorithm */
    CAM_INTF_META_AF_STATE,
    /* The ID sent with the latest CAMERA2_TRIGGER_AUTOFOCUS call */
    CAM_INTF_META_AF_TRIGGER_ID,
    /* List of areas to use for illuminant estimation */
    CAM_INTF_META_AWB_REGIONS,
    /* Current state of AWB algorithm */
    CAM_INTF_META_AWB_STATE,
    /* Information to 3A routines about the purpose of this capture, to help
     * decide optimal 3A strategy */
    CAM_INTF_META_CAPTURE_INTENT,
    /* Overall mode of 3A control routines. We need to have this parameter
     * because not all android.control.* have an OFF option, for example,
     * AE_FPS_Range, aePrecaptureTrigger */
    CAM_INTF_META_MODE,
    /* DEMOSAIC */
    /* Controls the quality of the demosaicing processing */
    CAM_INTF_META_DEMOSAIC,
    /* EDGE */
    /* Operation mode for edge enhancement */
    CAM_INTF_META_EDGE,
    /* Control the amount of edge enhancement applied to the images.*/
    /* 1-10; 10 is maximum sharpening */
    CAM_INTF_META_SHARPNESS_STRENGTH,
    /* FLASH */
    /* Power for flash firing/torch, 10 is max power; 0 is no flash. Linear */
    CAM_INTF_META_FLASH_POWER,
    /* Firing time of flash relative to start of exposure, in nanoseconds*/
    CAM_INTF_META_FLASH_FIRING_TIME,
    /* Current state of the flash unit */
    CAM_INTF_META_FLASH_STATE,
    /* GEOMETRIC */
    /* Operating mode of geometric correction */
    CAM_INTF_META_GEOMETRIC_MODE,
    /* Control the amount of shading correction applied to the images */
    CAM_INTF_META_GEOMETRIC_STRENGTH,
    /* HOT PIXEL */
    /* Set operational mode for hot pixel correction */
    CAM_INTF_META_HOTPIXEL_MODE,
    /* LENS */
    /* Size of the lens aperture */
    CAM_INTF_META_LENS_APERTURE,
    /* State of lens neutral density filter(s) */
    CAM_INTF_META_LENS_FILTERDENSITY,
    /* Lens optical zoom setting */
    CAM_INTF_META_LENS_FOCAL_LENGTH,
    /* Distance to plane of sharpest focus, measured from frontmost surface
     * of the lens */
    CAM_INTF_META_LENS_FOCUS_DISTANCE,
    /* The range of scene distances that are in sharp focus (depth of field) */
    CAM_INTF_META_LENS_FOCUS_RANGE,
    /* Whether optical image stabilization is enabled. */
    CAM_INTF_META_LENS_OPT_STAB_MODE,
    /* Current lens status */
    CAM_INTF_META_LENS_STATE,
    /* NOISE REDUCTION */
    /* Mode of operation for the noise reduction algorithm */
    CAM_INTF_META_NOISE_REDUCTION_MODE,
   /* Control the amount of noise reduction applied to the images.
    * 1-10; 10 is max noise reduction */
    CAM_INTF_META_NOISE_REDUCTION_STRENGTH,
    /* SCALER */
    /* Top-left corner and width of the output region to select from the active
     * pixel array */
    CAM_INTF_META_SCALER_CROP_REGION,
    /* SENSOR */
    /* Duration each pixel is exposed to light, in nanoseconds */
    CAM_INTF_META_SENSOR_EXPOSURE_TIME,
    /* Duration from start of frame exposure to start of next frame exposure,
     * in nanoseconds */
    CAM_INTF_META_SENSOR_FRAME_DURATION,
    /* Gain applied to image data. Must be implemented through analog gain only
     * if set to values below 'maximum analog sensitivity'. */
    CAM_INTF_META_SENSOR_SENSITIVITY,
    /* Time at start of exposure of first row */
    CAM_INTF_META_SENSOR_TIMESTAMP,
    /* SHADING */
    /* Quality of lens shading correction applied to the image data */
    CAM_INTF_META_SHADING_MODE,
    /* Control the amount of shading correction applied to the images.
     * unitless: 1-10; 10 is full shading compensation */
    CAM_INTF_META_SHADING_STRENGTH,
    /* STATISTICS */
    /* State of the face detector unit */
    CAM_INTF_META_STATS_FACEDETECT_MODE,
    /* Operating mode for histogram generation */
    CAM_INTF_META_STATS_HISTOGRAM_MODE,
    /* Operating mode for sharpness map generation */
    CAM_INTF_META_STATS_SHARPNESS_MAP_MODE,
    /* A 3-channel sharpness map, based on the raw sensor data,
     * If only a monochrome sharpness map is supported, all channels
     * should have the same data
     */
    CAM_INTF_META_STATS_SHARPNESS_MAP,

    /* TONEMAP */
    /* Table mapping blue input values to output values */
    CAM_INTF_META_TONEMAP_CURVE_BLUE,
    /* Table mapping green input values to output values */
    CAM_INTF_META_TONEMAP_CURVE_GREEN,
    /* Table mapping red input values to output values */
    CAM_INTF_META_TONEMAP_CURVE_RED,
    /* Tone map mode */
    CAM_INTF_META_TONEMAP_MODE,
    CAM_INTF_META_FLASH_MODE,
    CAM_INTF_META_ASD_HDR_SCENE_DATA,
    CAM_INTF_META_PRIVATE_DATA,
    CAM_INTF_PARM_STATS_DEBUG_MASK,

    CAM_INTF_PARM_MAX
} cam_intf_parm_type_t;

typedef struct {
    int   forced;
    union {
      uint32_t force_linecount_value;
      float    force_gain_value;
      float    force_snap_exp_value;
      float    force_exp_value;
      uint32_t force_snap_linecount_value;
      float    force_snap_gain_value;
    } u;
} cam_ez_force_params_t;

typedef enum {
    CAM_EZTUNE_CMD_STATUS,
    CAM_EZTUNE_CMD_AEC_ENABLE,
    CAM_EZTUNE_CMD_AWB_ENABLE,
    CAM_EZTUNE_CMD_AF_ENABLE,
    CAM_EZTUNE_CMD_AEC_FORCE_LINECOUNT,
    CAM_EZTUNE_CMD_AEC_FORCE_GAIN,
    CAM_EZTUNE_CMD_AEC_FORCE_EXP,
    CAM_EZTUNE_CMD_AEC_FORCE_SNAP_LC,
    CAM_EZTUNE_CMD_AEC_FORCE_SNAP_GAIN,
    CAM_EZTUNE_CMD_AEC_FORCE_SNAP_EXP,
} cam_eztune_cmd_type_t;

typedef struct {
  cam_eztune_cmd_type_t   cmd;
  union {
    int                   running;
    int                   aec_enable;
    int                   awb_enable;
    int                   af_enable;
    cam_ez_force_params_t ez_force_param;
  } u;
} cam_eztune_cmd_data_t;


/*****************************************************************************
 *                 Code for HAL3 data types                                  *
 ****************************************************************************/
typedef enum {
    CAM_INTF_METADATA_MAX
} cam_intf_metadata_type_t;

typedef enum {
    CAM_INTENT_CUSTOM,
    CAM_INTENT_PREVIEW,
    CAM_INTENT_STILL_CAPTURE,
    CAM_INTENT_VIDEO_RECORD,
    CAM_INTENT_VIDEO_SNAPSHOT,
    CAM_INTENT_ZERO_SHUTTER_LAG,
    CAM_INTENT_MAX,
} cam_intent_t;

typedef enum {
    /* Full application control of pipeline. All 3A routines are disabled,
     * no other settings in android.control.* have any effect */
    CAM_CONTROL_OFF,
    /* Use settings for each individual 3A routine. Manual control of capture
     * parameters is disabled. All controls in android.control.* besides sceneMode
     * take effect */
    CAM_CONTROL_AUTO,
    /* Use specific scene mode. Enabling this disables control.aeMode,
     * control.awbMode and control.afMode controls; the HAL must ignore those
     * settings while USE_SCENE_MODE is active (except for FACE_PRIORITY scene mode).
     * Other control entries are still active. This setting can only be used if
     * availableSceneModes != UNSUPPORTED. TODO: Should we remove this and handle this
     * in HAL ?*/
    CAM_CONTROL_USE_SCENE_MODE,
    CAM_CONTROL_MAX
} cam_control_mode_t;

typedef enum {
    /* Use the android.colorCorrection.transform matrix to do color conversion */
    CAM_COLOR_CORRECTION_TRANSFORM_MATRIX,
    /* Must not slow down frame rate relative to raw bayer output */
    CAM_COLOR_CORRECTION_FAST,
    /* Frame rate may be reduced by high quality */
    CAM_COLOR_CORRECTION_HIGH_QUALITY,
} cam_color_correct_mode_t;

typedef struct {
    /* 3x3 float matrix in row-major order. each element is in range of (0, 1) */
    float transform[3][3];
} cam_color_correct_matrix_t;

#define CAM_FOCAL_LENGTHS_MAX     1
#define CAM_APERTURES_MAX         1
#define CAM_FILTER_DENSITIES_MAX  1
#define CAM_MAX_MAP_HEIGHT        6
#define CAM_MAX_MAP_WIDTH         6

#define CAM_MAX_TONEMAP_CURVE_SIZE    128

typedef struct {
    int tonemap_points_cnt;

    /* A 1D array of pairs of floats.
     * Mapping a 0-1 input range to a 0-1 output range.
     * The input range must be monotonically increasing with N,
     * and values between entries should be linearly interpolated.
     * For example, if the array is: [0.0, 0.0, 0.3, 0.5, 1.0, 1.0],
     * then the input->output mapping for a few sample points would be:
     * 0 -> 0, 0.15 -> 0.25, 0.3 -> 0.5, 0.5 -> 0.64 */
    float tonemap_points[CAM_MAX_TONEMAP_CURVE_SIZE][2];
} cam_tonemap_curve_t;

typedef enum {
    OFF,
    FAST,
    QUALITY,
} cam_quality_preference_t;

typedef enum {
    CAM_FLASH_CTRL_OFF,
    CAM_FLASH_CTRL_SINGLE,
    CAM_FLASH_CTRL_TORCH
} cam_flash_ctrl_t;

typedef struct {
    uint8_t ae_mode;
    uint8_t awb_mode;
    uint8_t af_mode;
} cam_scene_mode_overrides_t;

typedef struct {
    int32_t left;
    int32_t top;
    int32_t width;
} cam_crop_region_t;

typedef struct {
    /* Estimated sharpness for each region of the input image.
     * Normalized to be between 0 and maxSharpnessMapValue.
     * Higher values mean sharper (better focused) */
    int32_t sharpness[CAM_MAX_MAP_WIDTH][CAM_MAX_MAP_HEIGHT];
} cam_sharpness_map_t;

typedef struct {
    int32_t min_value;
    int32_t max_value;
    int32_t def_value;
    int32_t step;
} cam_control_range_t;

#define CAM_QCOM_FEATURE_FACE_DETECTION (1<<0)
#define CAM_QCOM_FEATURE_DENOISE2D      (1<<1)
#define CAM_QCOM_FEATURE_CROP           (1<<2)
#define CAM_QCOM_FEATURE_ROTATION       (1<<3)
#define CAM_QCOM_FEATURE_FLIP           (1<<4)
#define CAM_QCOM_FEATURE_HDR            (1<<5)
#define CAM_QCOM_FEATURE_REGISTER_FACE  (1<<6)
#define CAM_QCOM_FEATURE_SHARPNESS      (1<<7)
#define CAM_QCOM_FEATURE_VIDEO_HDR      (1<<8)
#define CAM_QCOM_FEATURE_CAC            (1<<9)
#define CAM_QCOM_FEATURE_SCALE          (1<<10)
#define CAM_QCOM_FEATURE_EFFECT         (1<<11)

// Counter clock wise
typedef enum {
    ROTATE_0 = 1<<0,
    ROTATE_90 = 1<<1,
    ROTATE_180 = 1<<2,
    ROTATE_270 = 1<<3,
} cam_rotation_t;

typedef enum {
    FLIP_H = 1<<0,
    FLIP_V = 1<<1,
} cam_flip_t;

typedef struct {
    uint32_t bundle_id;                            /* bundle id */
    uint8_t num_of_streams;                        /* number of streams in the bundle */
    uint32_t stream_ids[MAX_STREAM_NUM_IN_BUNDLE]; /* array of stream ids to be bundled */
} cam_bundle_config_t;

typedef enum {
    CAM_ONLINE_REPROCESS_TYPE,    /* online reprocess, frames from running streams */
    CAM_OFFLINE_REPROCESS_TYPE,   /* offline reprocess, frames from external source */
} cam_reprocess_type_enum_t;

typedef enum {
    CAM_HDR_MODE_SINGLEFRAME,    /* Single frame HDR mode which does only tone mapping */
    CAM_HDR_MODE_MULTIFRAME,     /* Multi frame HDR mode which needs two frames with 0.5x and 2x exposure respectively */
} cam_hdr_mode_enum_t;

typedef struct {
    uint32_t hdr_enable;
    uint32_t hdr_need_1x; /* when CAM_QCOM_FEATURE_HDR enabled, indicate if 1x is needed for output */
    cam_hdr_mode_enum_t hdr_mode;
} cam_hdr_param_t;

typedef struct {
    int32_t output_width;
    int32_t output_height;
} cam_scale_param_t;

typedef struct {
    /* reprocess feature mask */
    uint32_t feature_mask;

    /* individual setting for features to be reprocessed */
    cam_denoise_param_t denoise2d;
    cam_rect_t input_crop;
    cam_rotation_t rotation;
    uint32_t flip;
    int32_t sharpness;
    int32_t effect;
    cam_hdr_param_t hdr_param;
    cam_scale_param_t scale_param;
} cam_pp_feature_config_t;

typedef struct {
    uint32_t input_stream_id;
    /* input source stream type */
    cam_stream_type_t input_stream_type;
} cam_pp_online_src_config_t;

typedef struct {
    /* image format */
    cam_format_t input_fmt;

    /* image dimension */
    cam_dimension_t input_dim;

    /* buffer plane information, will be calc based on stream_type, fmt,
       dim, and padding_info(from stream config). Info including:
       offset_x, offset_y, stride, scanline, plane offset */
    cam_stream_buf_plane_info_t input_buf_planes;

    /* number of input reprocess buffers */
    uint8_t num_of_bufs;
} cam_pp_offline_src_config_t;

/* reprocess stream input configuration */
typedef struct {
    /* input source config */
    cam_reprocess_type_enum_t pp_type;
    union {
        cam_pp_online_src_config_t online;
        cam_pp_offline_src_config_t offline;
    };

    /* pp feature config */
    cam_pp_feature_config_t pp_feature_config;
} cam_stream_reproc_config_t;

typedef struct {
    uint8_t crop_enabled;
    cam_rect_t input_crop;
} cam_crop_param_t;

typedef struct {
    uint8_t trigger;
    int32_t trigger_id;
} cam_trigger_t;

typedef struct {
    cam_denoise_param_t denoise;
    cam_crop_param_t crop;
    uint32_t flip;     /* 0 means no flip */
    uint32_t uv_upsample; /* 0 means no chroma upsampling */
    int32_t sharpness; /* 0 means no sharpness */
} cam_per_frame_pp_config_t;

typedef enum {
    CAM_OPT_STAB_OFF,
    CAM_OPT_STAB_ON,
    CAM_OPT_STAB_MAX
} cam_optical_stab_modes_t;

typedef enum {
    CAM_FILTER_ARRANGEMENT_RGGB,
    CAM_FILTER_ARRANGEMENT_GRBG,
    CAM_FILTER_ARRANGEMENT_GBRG,
    CAM_FILTER_ARRANGEMENT_BGGR,

    /* Sensor is not Bayer; output has 3 16-bit values for each pixel,
     * instead of just 1 16-bit value per pixel.*/
    CAM_FILTER_ARRANGEMENT_RGB
} cam_color_filter_arrangement_t;

typedef enum {
    CAM_AF_STATE_INACTIVE,
    CAM_AF_STATE_PASSIVE_SCAN,
    CAM_AF_STATE_PASSIVE_FOCUSED,
    CAM_AF_STATE_ACTIVE_SCAN,
    CAM_AF_STATE_FOCUSED_LOCKED,
    CAM_AF_STATE_NOT_FOCUSED_LOCKED
} cam_af_state_t;

typedef enum {
    CAM_AWB_STATE_INACTIVE,
    CAM_AWB_STATE_SEARCHING,
    CAM_AWB_STATE_CONVERGED,
    CAM_AWB_STATE_LOCKED
} cam_awb_state_t;

#endif /* __QCAMERA_TYPES_H__ */
