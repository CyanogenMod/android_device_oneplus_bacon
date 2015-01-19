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

#ifndef __QCAMERA_INTF_H__
#define __QCAMERA_INTF_H__

#include <media/msmb_isp.h>
#include <semaphore.h>
#include "cam_types.h"

/* (1024 * 1024) */
#define ONE_MB_OF_PARAMS 1048576U

#define CAM_PRIV_IOCTL_BASE (V4L2_CID_PRIVATE_BASE + 14)
typedef enum {
    /* session based parameters */
    CAM_PRIV_PARM = CAM_PRIV_IOCTL_BASE,
    /* session based action: do auto focus.*/
    CAM_PRIV_DO_AUTO_FOCUS,
    /* session based action: cancel auto focus.*/
    CAM_PRIV_CANCEL_AUTO_FOCUS,
    /* session based action: prepare for snapshot.*/
    CAM_PRIV_PREPARE_SNAPSHOT,
    /* sync stream info.*/
    CAM_PRIV_STREAM_INFO_SYNC,
    /* stream based parameters*/
    CAM_PRIV_STREAM_PARM,
    /* start ZSL snapshot.*/
    CAM_PRIV_START_ZSL_SNAPSHOT,
    /* stop ZSL snapshot.*/
    CAM_PRIV_STOP_ZSL_SNAPSHOT,
} cam_private_ioctl_enum_t;

/* capability struct definition for HAL 1*/
typedef struct{
    cam_hal_version_t version;

    cam_position_t position;                                /* sensor position: front, back */

    uint8_t auto_hdr_supported;

    /* supported iso modes */
    size_t supported_iso_modes_cnt;
    cam_iso_mode_type supported_iso_modes[CAM_ISO_MODE_MAX];

    /* supported exposure time */
    int32_t min_exposure_time;
    int32_t max_exposure_time;

    /* supported flash modes */
    size_t supported_flash_modes_cnt;
    cam_flash_mode_t supported_flash_modes[CAM_FLASH_MODE_MAX];

    size_t zoom_ratio_tbl_cnt;                              /* table size for zoom ratios */
    uint32_t zoom_ratio_tbl[MAX_ZOOMS_CNT];                 /* zoom ratios table */

    /* supported effect modes */
    size_t supported_effects_cnt;
    cam_effect_mode_type supported_effects[CAM_EFFECT_MODE_MAX];

    /* supported scene modes */
    size_t supported_scene_modes_cnt;
    cam_scene_mode_type supported_scene_modes[CAM_SCENE_MODE_MAX];

    /* supported auto exposure modes */
    size_t supported_aec_modes_cnt;
    cam_auto_exposure_mode_type supported_aec_modes[CAM_AEC_MODE_MAX];

    size_t fps_ranges_tbl_cnt;                              /* fps ranges table size */
    cam_fps_range_t fps_ranges_tbl[MAX_SIZES_CNT];          /* fps ranges table */

    /* supported antibanding modes */
    size_t supported_antibandings_cnt;
    cam_antibanding_mode_type supported_antibandings[CAM_ANTIBANDING_MODE_MAX];

    /* supported white balance modes */
    size_t supported_white_balances_cnt;
    cam_wb_mode_type supported_white_balances[CAM_WB_MODE_MAX];

    /* supported manual wb cct */
    int32_t min_wb_cct;
    int32_t max_wb_cct;

    /* supported focus modes */
    size_t supported_focus_modes_cnt;
    cam_focus_mode_type supported_focus_modes[CAM_FOCUS_MODE_MAX];

    /* supported manual focus position */
    int32_t min_focus_pos[CAM_MANUAL_FOCUS_MODE_MAX];
    int32_t max_focus_pos[CAM_MANUAL_FOCUS_MODE_MAX];

    int32_t exposure_compensation_min;       /* min value of exposure compensation index */
    int32_t exposure_compensation_max;       /* max value of exposure compensation index */
    int32_t exposure_compensation_default;   /* default value of exposure compensation index */
    float exposure_compensation_step;
    cam_rational_type_t exp_compensation_step;    /* exposure compensation step value */

    uint8_t video_stablization_supported; /* flag id video stablization is supported */

    size_t picture_sizes_tbl_cnt;                           /* picture sizes table size */
    cam_dimension_t picture_sizes_tbl[MAX_SIZES_CNT];       /* picture sizes table */

    /* capabilities specific to HAL 1 */

    int32_t modes_supported;                                /* mask of modes supported: 2D, 3D */
    uint32_t sensor_mount_angle;                            /* sensor mount angle */

    float focal_length;                                     /* focal length */
    float hor_view_angle;                                   /* horizontal view angle */
    float ver_view_angle;                                   /* vertical view angle */

    size_t preview_sizes_tbl_cnt;                           /* preview sizes table size */
    cam_dimension_t preview_sizes_tbl[MAX_SIZES_CNT];       /* preiew sizes table */

    size_t video_sizes_tbl_cnt;                             /* video sizes table size */
    cam_dimension_t video_sizes_tbl[MAX_SIZES_CNT];         /* video sizes table */

    size_t livesnapshot_sizes_tbl_cnt;                      /* livesnapshot sizes table size */
    cam_dimension_t livesnapshot_sizes_tbl[MAX_SIZES_CNT];  /* livesnapshot sizes table */

    size_t hfr_tbl_cnt;                                     /* table size for HFR */
    cam_hfr_info_t hfr_tbl[CAM_HFR_MODE_MAX];               /* HFR table */

    /* supported preview formats */
    size_t supported_preview_fmt_cnt;
    cam_format_t supported_preview_fmts[CAM_FORMAT_MAX];

    /* supported picture formats */
    size_t supported_picture_fmt_cnt;
    cam_format_t supported_picture_fmts[CAM_FORMAT_MAX];

    /* dimension and supported output format of raw dump from camif */
    cam_dimension_t raw_dim;
    size_t supported_raw_fmt_cnt;
    cam_format_t supported_raw_fmts[CAM_FORMAT_MAX];

    /* supported focus algorithms */
    size_t supported_focus_algos_cnt;
    cam_focus_algorithm_type supported_focus_algos[CAM_FOCUS_ALGO_MAX];


    uint8_t auto_wb_lock_supported;       /* flag if auto white balance lock is supported */
    uint8_t zoom_supported;               /* flag if zoom is supported */
    uint8_t smooth_zoom_supported;        /* flag if smooth zoom is supported */
    uint8_t auto_exposure_lock_supported; /* flag if auto exposure lock is supported */
    uint8_t video_snapshot_supported;     /* flag if video snapshot is supported */

    uint8_t max_num_roi;                  /* max number of roi can be detected */
    uint8_t max_num_focus_areas;          /* max num of focus areas */
    uint8_t max_num_metering_areas;       /* max num opf metering areas */
    uint8_t max_zoom_step;                /* max zoom step value */

    /* QCOM specific control */
    cam_control_range_t brightness_ctrl;  /* brightness */
    cam_control_range_t sharpness_ctrl;   /* sharpness */
    cam_control_range_t contrast_ctrl;    /* contrast */
    cam_control_range_t saturation_ctrl;  /* saturation */
    cam_control_range_t sce_ctrl;         /* skintone enhancement factor */

    /* QCOM HDR specific control. Indicates number of frames and exposure needs for the frames */
    cam_hdr_bracketing_info_t hdr_bracketing_setting;

    uint32_t qcom_supported_feature_mask; /* mask of qcom specific features supported:
                                           * such as CAM_QCOM_FEATURE_SUPPORTED_FACE_DETECTION*/
    cam_padding_info_t padding_info;      /* padding information from PP */
    uint32_t min_num_pp_bufs;             /* minimum number of buffers needed by postproc module */
    uint32_t min_required_pp_mask;        /* min required pp feature masks for ZSL.
                                           * depends on hardware limitation, i.e. for 8974,
                                           * sharpness is required for all ZSL snapshot frames */

    /* capabilities specific to HAL 3 */

    float min_focus_distance;
    float hyper_focal_distance;

    float focal_lengths[CAM_FOCAL_LENGTHS_MAX];
    uint8_t focal_lengths_count;

    float apertures[CAM_APERTURES_MAX];
    uint8_t apertures_count;

    float filter_densities[CAM_FILTER_DENSITIES_MAX];
    uint8_t filter_densities_count;

    uint8_t optical_stab_modes[CAM_OPT_STAB_MAX];
    uint8_t optical_stab_modes_count;

    cam_dimension_t lens_shading_map_size;
    float lens_shading_map[3 * CAM_MAX_MAP_WIDTH *
              CAM_MAX_MAP_HEIGHT];

    cam_dimension_t geo_correction_map_size;
    float geo_correction_map[2 * 3 * CAM_MAX_MAP_WIDTH *
              CAM_MAX_MAP_HEIGHT];

    float lens_position[3];

    /* nano seconds */
    int64_t exposure_time_range[2];

    /* nano seconds */
    int64_t max_frame_duration;

    cam_color_filter_arrangement_t color_arrangement;

    float sensor_physical_size[2];

    /* Dimensions of full pixel array, possibly including
       black calibration pixels */
    cam_dimension_t pixel_array_size;
    /* Area of raw data which corresponds to only active
       pixels; smaller or equal to pixelArraySize. */
    cam_rect_t active_array_size;

    /* Maximum raw value output by sensor */
    int32_t white_level;

    /* A fixed black level offset for each of the Bayer
       mosaic channels */
    int32_t black_level_pattern[4];

    /* Time taken before flash can fire again in nano secs */
    int64_t flash_charge_duration;

    /* Maximum number of supported points in the tonemap
       curve */
    int32_t max_tone_map_curve_points;

    /* supported formats */
    size_t supported_scalar_format_cnt;
    cam_format_t supported_scalar_fmts[CAM_FORMAT_MAX];

    /* The minimum frame duration that is supported for above
       raw resolution */
    int64_t raw_min_duration;

    size_t supported_sizes_tbl_cnt;
    cam_dimension_t supported_sizes_tbl[MAX_SIZES_CNT];

    /* The minimum frame duration that is supported for each
     * resolution in availableProcessedSizes. Should correspond
     * to the frame duration when only that processed stream
     * is active, with all processing set to FAST */
    int64_t min_duration[MAX_SIZES_CNT];

    uint32_t max_face_detection_count;

    uint8_t histogram_supported;
    /* Number of histogram buckets supported */
    int32_t histogram_size;
    /* Maximum value possible for a histogram bucket */
    int32_t max_histogram_count;

    cam_dimension_t sharpness_map_size;

    /* Maximum value possible for a sharpness map region */
    int32_t max_sharpness_map_value;

    cam_scene_mode_overrides_t scene_mode_overrides[CAM_SCENE_MODE_MAX];

    /*Autoexposure modes for camera 3 api*/
    size_t supported_ae_modes_cnt;
    cam_ae_mode_type supported_ae_modes[CAM_AE_MODE_MAX];

    /* picture sizes need scale*/
    size_t scale_picture_sizes_cnt;
    cam_dimension_t scale_picture_sizes[MAX_SCALE_SIZES_CNT];

    uint8_t flash_available;

    cam_rational_type_t base_gain_factor;    /* sensor base gain factor */
    /* AF Bracketing info */
    cam_af_bracketing_t  ubifocus_af_bracketing_need;
    /* opti Zoom info */
    cam_opti_zoom_t      opti_zoom_settings_need;
    /* true Portrait info */
    cam_true_portrait_t  true_portrait_settings_need;
    /* FSSR info */
    cam_fssr_t      fssr_settings_need;
    /* AF bracketing info for multi-touch focus*/
    cam_af_bracketing_t  mtf_af_bracketing_parm;
    /* Sensor type information */
    cam_sensor_type_t sensor_type;
} cam_capability_t;

typedef enum {
    CAM_STREAM_CONSUMER_DISPLAY,    /* buf to be displayed */
    CAM_STREAM_CONSUMER_VIDEO_ENC,  /* buf to be encoded by video */
    CAM_STREAM_CONSUMER_JPEG_ENC,   /* ZSL YUV buf to be fed back to JPEG */
} cam_stream_consumer_t;

typedef enum {
    CAM_STREAM_PARAM_TYPE_DO_REPROCESS = CAM_INTF_PARM_DO_REPROCESS,
    CAM_STREAM_PARAM_TYPE_SET_BUNDLE_INFO = CAM_INTF_PARM_SET_BUNDLE,
    CAM_STREAM_PARAM_TYPE_SET_FLIP = CAM_INTF_PARM_STREAM_FLIP,
    CAM_STREAM_PARAM_SET_STREAM_CONSUMER,
    CAM_STREAM_PARAM_TYPE_GET_OUTPUT_CROP = CAM_INTF_PARM_GET_OUTPUT_CROP,
    CAM_STREAM_PARAM_TYPE_GET_IMG_PROP = CAM_INTF_PARM_GET_IMG_PROP,
    CAM_STREAM_PARAM_TYPE_MAX
} cam_stream_param_type_e;

typedef struct {
    uint32_t buf_index;           /* buf index to the source frame buffer that needs reprocess,
                                    (assume buffer is already mapped)*/
    uint32_t frame_idx;           /* frame id of source frame to be reprocessed */
    int32_t ret_val;              /* return value from reprocess. Could have different meanings.
                                     i.e., faceID in the case of face registration. */
    uint8_t meta_present;         /* if there is meta data associated with this reprocess frame */
    uint32_t meta_stream_handle;  /* meta data stream ID. only valid if meta_present != 0 */
    uint32_t meta_buf_index;      /* buf index to meta data buffer. only valid if meta_present != 0 */

    cam_per_frame_pp_config_t frame_pp_config; /* per frame post-proc configuration */
} cam_reprocess_param;

typedef struct {
    uint32_t flip_mask;
} cam_flip_mode_t;

#define IMG_NAME_SIZE 32
typedef struct {
    cam_rect_t crop;  /* crop info for the image */
    cam_dimension_t input; /* input dimension of the image */
    cam_dimension_t output; /* output dimension of the image */
    char name[IMG_NAME_SIZE]; /* optional name of the ext*/
    uint32_t is_raw_image; /* image is raw */
    cam_format_t format; /* image format */
    uint32_t analysis_image; /* image is used for analysis. hence skip thumbnail */
    uint32_t size; /* size of the image */
} cam_stream_img_prop_t;

typedef struct {
    cam_stream_param_type_e type;
    union {
        cam_reprocess_param reprocess;  /* do reprocess */
        cam_bundle_config_t bundleInfo; /* set bundle info*/
        cam_flip_mode_t flipInfo;       /* flip mode */
        cam_stream_consumer_t consumer; /* stream consumer */
        cam_crop_data_t outputCrop;     /* output crop for current frame */
        cam_stream_img_prop_t imgProp;  /* image properties of current frame */
    };
} cam_stream_parm_buffer_t;

/* stream info */
typedef struct {
    /* stream ID from server */
    uint32_t stream_svr_id;

    /* stream type */
    cam_stream_type_t stream_type;

    /* image format */
    cam_format_t fmt;

    /* image dimension */
    cam_dimension_t dim;

    /* buffer plane information, will be calc based on stream_type, fmt,
       dim, and padding_info(from stream config). Info including:
       offset_x, offset_y, stride, scanline, plane offset */
    cam_stream_buf_plane_info_t buf_planes;

    /* number of stream bufs will be allocated */
    uint32_t num_bufs;

    /* streaming type */
    cam_streaming_mode_t streaming_mode;
    /* num of frames needs to be generated.
     * only valid when streaming_mode = CAM_STREAMING_MODE_BURST */
    uint8_t num_of_burst;

    /* stream specific pp config */
    cam_pp_feature_config_t pp_config;

    /* this section is valid if offline reprocess type stream */
    cam_stream_reproc_config_t reprocess_config;

    cam_stream_parm_buffer_t parm_buf;    /* stream based parameters */

    uint8_t useAVTimer; /*flag to indicate use of AVTimer for TimeStamps*/

    uint8_t dis_enable;

    /* Image Stabilization type */
    cam_is_type_t is_type;

    cam_stream_secure_t is_secure;

} cam_stream_info_t;

/*****************************************************************************
 *                 Code for Domain Socket Based Parameters                   *
 ****************************************************************************/

#define POINTER_OF(PARAM_ID,TABLE_PTR)    \
        (&(TABLE_PTR->entry[PARAM_ID].data))

#define GET_FIRST_PARAM_ID(TABLE_PTR)     \
        (TABLE_PTR->first_flagged_entry)

#define SET_FIRST_PARAM_ID(TABLE_PTR,PARAM_ID)     \
        TABLE_PTR->first_flagged_entry=PARAM_ID

#define GET_NEXT_PARAM_ID(CURRENT_PARAM_ID,TABLE_PTR)    \
        (TABLE_PTR->entry[CURRENT_PARAM_ID].next_flagged_entry)

#define SET_NEXT_PARAM_ID(CURRENT_PARAM_ID,TABLE_PTR,NEXT_PARAM_ID)    \
        TABLE_PTR->entry[CURRENT_PARAM_ID].next_flagged_entry=NEXT_PARAM_ID;

#define INCLUDE(PARAM_ID,DATATYPE,COUNT)  \
        DATATYPE member_variable_##PARAM_ID[ COUNT ]

#define GET_NEXT_PARAM(TABLE_PTR, TYPE)    \
        (TYPE *)((char *)TABLE_PTR +       \
               TABLE_PTR->aligned_size)    \

typedef union {
/**************************************************************************************
 *          ID from (cam_intf_parm_type_t)          DATATYPE                     COUNT
 **************************************************************************************/
    INCLUDE(CAM_INTF_PARM_HAL_VERSION,              int32_t,                     1);
    /* Shared between HAL1 and HAL3 */
    INCLUDE(CAM_INTF_PARM_ANTIBANDING,              int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_EXPOSURE_COMPENSATION,    int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_AEC_LOCK,                 int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_AEC_ENABLE,               int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_FPS_RANGE,                cam_fps_range_t,             1);
    INCLUDE(CAM_INTF_PARM_FOCUS_MODE,               uint8_t,                     1);
    INCLUDE(CAM_INTF_PARM_MANUAL_FOCUS_POS,         cam_manual_focus_parm_t,     1);
    INCLUDE(CAM_INTF_PARM_AWB_LOCK,                 int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_AWB_ENABLE,               int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_AF_ENABLE,                int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_WHITE_BALANCE,            int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_EFFECT,                   int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_BESTSHOT_MODE,            int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_DIS_ENABLE,               int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_LED_MODE,                 int32_t,                     1);

    /* HAL1 specific */
    INCLUDE(CAM_INTF_PARM_QUERY_FLASH4SNAP,         int32_t,                     1); //read only
    INCLUDE(CAM_INTF_PARM_EXPOSURE,                 int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_SHARPNESS,                int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_CONTRAST,                 int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_SATURATION,               int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_BRIGHTNESS,               int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_ISO,                      int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_EXPOSURE_TIME,            int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_ZOOM,                     int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_ROLLOFF,                  int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_MODE,                     int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_AEC_ALGO_TYPE,            int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_FOCUS_ALGO_TYPE,          int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_AEC_ROI,                  cam_set_aec_roi_t,           1);
    INCLUDE(CAM_INTF_PARM_AF_ROI,                   cam_roi_info_t,              1);
    INCLUDE(CAM_INTF_PARM_SCE_FACTOR,               int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_FD,                       cam_fd_set_parm_t,           1);
    INCLUDE(CAM_INTF_PARM_MCE,                      int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_HFR,                      int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_REDEYE_REDUCTION,         int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_WAVELET_DENOISE,          cam_denoise_param_t,         1);
    INCLUDE(CAM_INTF_PARM_HISTOGRAM,                int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_ASD_ENABLE,               int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_RECORDING_HINT,           int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_HDR,                      cam_hdr_param_t,             1);
    INCLUDE(CAM_INTF_PARM_FRAMESKIP,                int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_ZSL_MODE,                 int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_HDR_NEED_1X,              int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_LOCK_CAF,                 int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_VIDEO_HDR,                int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_VT,                       int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_GET_CHROMATIX,            tune_chromatix_t,            1);
    INCLUDE(CAM_INTF_PARM_SET_RELOAD_CHROMATIX,     tune_chromatix_t,            1);
    INCLUDE(CAM_INTF_PARM_GET_AFTUNE,               tune_autofocus_t,            1);
    INCLUDE(CAM_INTF_PARM_SET_RELOAD_AFTUNE,        tune_autofocus_t,            1);
    INCLUDE(CAM_INTF_PARM_SET_AUTOFOCUSTUNING,      tune_actuator_t,             1);
    INCLUDE(CAM_INTF_PARM_SET_VFE_COMMAND,          tune_cmd_t,                  1);
    INCLUDE(CAM_INTF_PARM_SET_PP_COMMAND,           tune_cmd_t,                  1);
    INCLUDE(CAM_INTF_PARM_MAX_DIMENSION,            cam_dimension_t,             1);
    INCLUDE(CAM_INTF_PARM_RAW_DIMENSION,            cam_dimension_t,             1);
    INCLUDE(CAM_INTF_PARM_TINTLESS,                 int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_CDS_MODE,                 int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_EZTUNE_CMD,               cam_eztune_cmd_data_t,       1);
    INCLUDE(CAM_INTF_PARM_AF_MOBICAT_CMD,           int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_LONGSHOT_ENABLE,          int8_t,                      1);

    /* HAL3 specific */
    INCLUDE(CAM_INTF_META_FRAME_NUMBER,             uint32_t,                    1);
    INCLUDE(CAM_INTF_META_COLOR_CORRECT_MODE,       uint8_t,                     1);
    INCLUDE(CAM_INTF_META_COLOR_CORRECT_TRANSFORM,  cam_color_correct_matrix_t,  1);
    INCLUDE(CAM_INTF_META_AEC_MODE,                 uint8_t,                     1);
    INCLUDE(CAM_INTF_META_AEC_ROI,                  cam_area_t,                  5);
    INCLUDE(CAM_INTF_META_AEC_PRECAPTURE_TRIGGER,   cam_trigger_t,               1);
    INCLUDE(CAM_INTF_META_AF_ROI,                   cam_area_t,                  5);
    INCLUDE(CAM_INTF_META_AF_TRIGGER,               cam_trigger_t,               1);
    INCLUDE(CAM_INTF_META_AWB_REGIONS,              cam_area_t,                  5);
    INCLUDE(CAM_INTF_META_CAPTURE_INTENT,           uint8_t,                     1);
    INCLUDE(CAM_INTF_META_MODE,                     uint8_t,                     1);
    INCLUDE(CAM_INTF_META_DEMOSAIC,                 int32_t,                     1);
    INCLUDE(CAM_INTF_META_EDGE,                     int32_t,                     1);
    INCLUDE(CAM_INTF_META_SHARPNESS_STRENGTH,       int32_t,                     1);
    INCLUDE(CAM_INTF_META_FLASH_POWER,              uint8_t,                     1);
    INCLUDE(CAM_INTF_META_FLASH_FIRING_TIME,        int64_t,                     1);
    INCLUDE(CAM_INTF_META_GEOMETRIC_MODE,           uint8_t,                     1);
    INCLUDE(CAM_INTF_META_GEOMETRIC_STRENGTH,       uint8_t,                     1);
    INCLUDE(CAM_INTF_META_HOTPIXEL_MODE,            uint8_t,                     1);
    INCLUDE(CAM_INTF_META_LENS_APERTURE,            float,                       1);
    INCLUDE(CAM_INTF_META_LENS_FILTERDENSITY,       float,                       1);
    INCLUDE(CAM_INTF_META_LENS_FOCAL_LENGTH,        float,                       1);
    INCLUDE(CAM_INTF_META_LENS_FOCUS_DISTANCE,      float,                       1);
    INCLUDE(CAM_INTF_META_LENS_OPT_STAB_MODE,       uint8_t,                     1);
    INCLUDE(CAM_INTF_META_NOISE_REDUCTION_MODE,     uint8_t,                     1);
    INCLUDE(CAM_INTF_META_NOISE_REDUCTION_STRENGTH, int32_t,                     1);
    INCLUDE(CAM_INTF_META_SCALER_CROP_REGION,       cam_crop_region_t,           1);
    INCLUDE(CAM_INTF_META_SENSOR_EXPOSURE_TIME,     int64_t,                     1);
    INCLUDE(CAM_INTF_META_SENSOR_FRAME_DURATION,    int64_t,                     1);
    INCLUDE(CAM_INTF_META_SENSOR_SENSITIVITY,       int32_t,                     1);
    INCLUDE(CAM_INTF_META_SHADING_MODE,             uint8_t,                     1);
    INCLUDE(CAM_INTF_META_SHADING_STRENGTH,         uint8_t,                     1);
    INCLUDE(CAM_INTF_META_STATS_FACEDETECT_MODE,    uint8_t,                     1);
    INCLUDE(CAM_INTF_META_STATS_HISTOGRAM_MODE,     uint8_t,                     1);
    INCLUDE(CAM_INTF_META_STATS_SHARPNESS_MAP_MODE, uint8_t,                     1);
    INCLUDE(CAM_INTF_META_TONEMAP_CURVE_BLUE,       cam_tonemap_curve_t,         1);
    INCLUDE(CAM_INTF_META_TONEMAP_CURVE_GREEN,      cam_tonemap_curve_t,         1);
    INCLUDE(CAM_INTF_META_TONEMAP_CURVE_RED,        cam_tonemap_curve_t,         1);
    INCLUDE(CAM_INTF_META_TONEMAP_MODE,             uint8_t,                     1);
    INCLUDE(CAM_INTF_META_FLASH_MODE,               uint8_t,                     1);
    INCLUDE(CAM_INTF_PARM_STATS_DEBUG_MASK,         uint32_t,                    1);
    INCLUDE(CAM_INTF_PARM_ISP_DEBUG_MASK,           uint32_t,                    1);
    INCLUDE(CAM_INTF_PARM_ALGO_OPTIMIZATIONS_MASK,  uint32_t,                    1);
    INCLUDE(CAM_INTF_PARM_SENSOR_DEBUG_MASK,        uint32_t,                    1);
    INCLUDE(CAM_INTF_PARM_FOCUS_BRACKETING,         cam_af_bracketing_t,         1);
    INCLUDE(CAM_INTF_PARM_MULTI_TOUCH_FOCUS_BRACKETING, cam_af_bracketing_t,     1);
    INCLUDE(CAM_INTF_PARM_FLASH_BRACKETING,         cam_flash_bracketing_t,      1);
} parm_type_t;

typedef union {
/**************************************************************************************
 *  ID from (cam_intf_metadata_type_t)           DATATYPE                     COUNT
 **************************************************************************************/
    /* common between HAL1 and HAL3 */
    INCLUDE(CAM_INTF_META_HISTOGRAM,                  cam_hist_stats_t,            1);
    INCLUDE(CAM_INTF_META_FACE_DETECTION,             cam_face_detection_data_t,   1);
    INCLUDE(CAM_INTF_META_AUTOFOCUS_DATA,             cam_auto_focus_data_t,       1);

    /* Specific to HAl1 */
    INCLUDE(CAM_INTF_META_CROP_DATA,                  cam_crop_data_t,             1);
    INCLUDE(CAM_INTF_META_PREP_SNAPSHOT_DONE,         int32_t,                     1);
    INCLUDE(CAM_INTF_META_GOOD_FRAME_IDX_RANGE,       cam_frame_idx_range_t,       1);
    /* Specific to HAL3 */
    INCLUDE(CAM_INTF_META_FRAME_NUMBER_VALID,         int32_t,                     1);
    INCLUDE(CAM_INTF_META_FRAME_NUMBER,               uint32_t,                    1);
    INCLUDE(CAM_INTF_META_COLOR_CORRECT_MODE,         uint8_t,                     1);
    INCLUDE(CAM_INTF_META_AEC_PRECAPTURE_ID,          int32_t,                     1);
    INCLUDE(CAM_INTF_META_AEC_ROI,                    cam_area_t,                  5);
    INCLUDE(CAM_INTF_META_AEC_STATE,                  uint8_t,                     1);
    INCLUDE(CAM_INTF_PARM_FOCUS_MODE,                 uint8_t,                     1);
    INCLUDE(CAM_INTF_META_AF_ROI,                     cam_area_t,                  5);
    INCLUDE(CAM_INTF_META_AF_STATE,                   uint8_t,                     1);
    INCLUDE(CAM_INTF_META_AF_TRIGGER_ID,              int32_t,                     1);
    INCLUDE(CAM_INTF_PARM_WHITE_BALANCE,              int32_t,                     1);
    INCLUDE(CAM_INTF_META_AWB_REGIONS,                cam_area_t,                  5);
    INCLUDE(CAM_INTF_META_AWB_STATE,                  uint8_t,                     1);
    INCLUDE(CAM_INTF_META_MODE,                       uint8_t,                     1);
    INCLUDE(CAM_INTF_META_EDGE,                       int32_t,                     1);
    INCLUDE(CAM_INTF_META_FLASH_POWER,                uint8_t,                     1);
    INCLUDE(CAM_INTF_META_FLASH_FIRING_TIME,          int64_t,                     1);
    INCLUDE(CAM_INTF_META_FLASH_MODE,                 uint8_t,                     1);
    INCLUDE(CAM_INTF_META_FLASH_STATE,                int32_t,                     1);
    INCLUDE(CAM_INTF_META_HOTPIXEL_MODE,              uint8_t,                     1);
    INCLUDE(CAM_INTF_META_LENS_APERTURE,              float,                       1);
    INCLUDE(CAM_INTF_META_LENS_FILTERDENSITY,         float,                       1);
    INCLUDE(CAM_INTF_META_LENS_FOCAL_LENGTH,          float,                       1);
    INCLUDE(CAM_INTF_META_LENS_FOCUS_DISTANCE,        float,                       1);
    INCLUDE(CAM_INTF_META_LENS_FOCUS_RANGE,           float,                       2);
    INCLUDE(CAM_INTF_META_LENS_OPT_STAB_MODE,         uint8_t,                     1);
    INCLUDE(CAM_INTF_META_LENS_FOCUS_STATE,           uint8_t,                     1);
    INCLUDE(CAM_INTF_META_NOISE_REDUCTION_MODE,       uint8_t,                     1);
    INCLUDE(CAM_INTF_META_SCALER_CROP_REGION,         cam_crop_region_t,           1);
    INCLUDE(CAM_INTF_META_SENSOR_EXPOSURE_TIME,       int64_t,                     1);
    INCLUDE(CAM_INTF_META_SENSOR_FRAME_DURATION,      int64_t,                     1);
    INCLUDE(CAM_INTF_META_SENSOR_SENSITIVITY,         int32_t,                     1);
    INCLUDE(CAM_INTF_META_SENSOR_TIMESTAMP,           struct timeval,              1);
    INCLUDE(CAM_INTF_META_SHADING_MODE,               uint8_t,                     1);
    INCLUDE(CAM_INTF_META_STATS_FACEDETECT_MODE,      uint8_t,                     1);
    INCLUDE(CAM_INTF_META_STATS_HISTOGRAM_MODE,       uint8_t,                     1);
    INCLUDE(CAM_INTF_META_STATS_SHARPNESS_MAP_MODE,   uint8_t,                     1);
    INCLUDE(CAM_INTF_META_STATS_SHARPNESS_MAP,        cam_sharpness_map_t,         3);
    INCLUDE(CAM_INTF_META_ASD_HDR_SCENE_DATA,      cam_asd_hdr_scene_data_t,       1);
    INCLUDE(CAM_INTF_META_PRIVATE_DATA,               char,                        MAX_METADATA_PAYLOAD_SIZE);

} metadata_type_t;

/****************************DO NOT MODIFY BELOW THIS LINE!!!!*********************/

typedef struct {
    metadata_type_t data;
    uint8_t next_flagged_entry;
} metadata_entry_type_t;

typedef struct {
    uint8_t first_flagged_entry;
    metadata_entry_type_t entry[CAM_INTF_PARM_MAX];
} metadata_buffer_t;

typedef struct {
    parm_type_t data;
    uint8_t next_flagged_entry;
} parm_entry_type_t;

//we need to align these contiguous param structures in memory
typedef struct {
    cam_intf_parm_type_t entry_type;
    size_t size;
    size_t aligned_size;
    char data[1];
} parm_entry_type_new_t;

typedef struct {
    uint8_t first_flagged_entry;
    parm_entry_type_t entry[CAM_INTF_PARM_MAX];
} parm_buffer_t;

typedef struct {
    size_t num_entry;
    size_t tot_rem_size;
    size_t curr_size;
    char entry[1];
} parm_buffer_new_t;

#ifdef  __cplusplus
extern "C" {
#endif
void *POINTER_OF_PARAM(cam_intf_parm_type_t PARAM_ID,
                    void *TABLE_PTR);
#ifdef  __cplusplus
}
#endif

#endif /* __QCAMERA_INTF_H__ */
