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

#include "mm_jpeg_dbg.h"
#include "mm_jpeg.h"
#include <errno.h>
#include <math.h>


#define LOWER(a)               ((a) & 0xFFFF)
#define UPPER(a)               (((a)>>16) & 0xFFFF)
#define CHANGE_ENDIAN_16(a) \
        ((uint16_t)((0x00FF & ((a)>>8)) | (0xFF00 & ((a)<<8))))
#define ROUND(a) \
        ((a >= 0) ? (uint32_t)(a + 0.5) : (uint32_t)(a - 0.5))

#define AAA_EXIF_BUF_SIZE   10
#define AE_EXIF_SIZE        2
#define AWB_EXIF_SIZE       4
#define AF_EXIF_SIZE        2

/** addExifEntry:
 *
 *  Arguments:
 *   @exif_info : Exif info struct
 *   @p_session: job session
 *   @tagid   : exif tag ID
 *   @type    : data type
 *   @count   : number of data in uint of its type
 *   @data    : input data ptr
 *
 *  Retrun     : int32_t type of status
 *               0  -- success
 *              none-zero failure code
 *
 *  Description:
 *       Function to add an entry to exif data
 *
 **/
int32_t addExifEntry(QOMX_EXIF_INFO *p_exif_info, exif_tag_id_t tagid,
  exif_tag_type_t type, uint32_t count, void *data)
{
    int32_t rc = 0;
    uint32_t numOfEntries = (uint32_t)p_exif_info->numOfEntries;
    QEXIF_INFO_DATA *p_info_data = p_exif_info->exif_data;
    if(numOfEntries >= MAX_EXIF_TABLE_ENTRIES) {
        ALOGE("%s: Number of entries exceeded limit", __func__);
        return -1;
    }

    p_info_data[numOfEntries].tag_id = tagid;
    p_info_data[numOfEntries].tag_entry.type = type;
    p_info_data[numOfEntries].tag_entry.count = count;
    p_info_data[numOfEntries].tag_entry.copy = 1;
    switch (type) {
    case EXIF_BYTE: {
      if (count > 1) {
        uint8_t *values = (uint8_t *)malloc(count);
        if (values == NULL) {
          ALOGE("%s: No memory for byte array", __func__);
          rc = -1;
        } else {
          memcpy(values, data, count);
          p_info_data[numOfEntries].tag_entry.data._bytes = values;
        }
      } else {
        p_info_data[numOfEntries].tag_entry.data._byte = *(uint8_t *)data;
      }
    }
    break;
    case EXIF_ASCII: {
      char *str = NULL;
      str = (char *)malloc(count + 1);
      if (str == NULL) {
        ALOGE("%s: No memory for ascii string", __func__);
        rc = -1;
      } else {
        memset(str, 0, count + 1);
        memcpy(str, data, count);
        p_info_data[numOfEntries].tag_entry.data._ascii = str;
      }
    }
    break;
    case EXIF_SHORT: {
      if (count > 1) {
        uint16_t *values = (uint16_t *)malloc(count * sizeof(uint16_t));
        if (values == NULL) {
          ALOGE("%s: No memory for short array", __func__);
          rc = -1;
        } else {
          memcpy(values, data, count * sizeof(uint16_t));
          p_info_data[numOfEntries].tag_entry.data._shorts = values;
        }
      } else {
        p_info_data[numOfEntries].tag_entry.data._short = *(uint16_t *)data;
      }
    }
    break;
    case EXIF_LONG: {
      if (count > 1) {
        uint32_t *values = (uint32_t *)malloc(count * sizeof(uint32_t));
        if (values == NULL) {
          ALOGE("%s: No memory for long array", __func__);
          rc = -1;
        } else {
          memcpy(values, data, count * sizeof(uint32_t));
          p_info_data[numOfEntries].tag_entry.data._longs = values;
        }
      } else {
        p_info_data[numOfEntries].tag_entry.data._long = *(uint32_t *)data;
      }
    }
    break;
    case EXIF_RATIONAL: {
      if (count > 1) {
        rat_t *values = (rat_t *)malloc(count * sizeof(rat_t));
        if (values == NULL) {
          ALOGE("%s: No memory for rational array", __func__);
          rc = -1;
        } else {
          memcpy(values, data, count * sizeof(rat_t));
          p_info_data[numOfEntries].tag_entry.data._rats = values;
        }
      } else {
        p_info_data[numOfEntries].tag_entry.data._rat = *(rat_t *)data;
      }
    }
    break;
    case EXIF_UNDEFINED: {
      uint8_t *values = (uint8_t *)malloc(count);
      if (values == NULL) {
        ALOGE("%s: No memory for undefined array", __func__);
        rc = -1;
      } else {
        memcpy(values, data, count);
        p_info_data[numOfEntries].tag_entry.data._undefined = values;
      }
    }
    break;
    case EXIF_SLONG: {
      if (count > 1) {
        int32_t *values = (int32_t *)malloc(count * sizeof(int32_t));
        if (values == NULL) {
          ALOGE("%s: No memory for signed long array", __func__);
          rc = -1;
        } else {
          memcpy(values, data, count * sizeof(int32_t));
          p_info_data[numOfEntries].tag_entry.data._slongs = values;
        }
      } else {
        p_info_data[numOfEntries].tag_entry.data._slong = *(int32_t *)data;
      }
    }
    break;
    case EXIF_SRATIONAL: {
      if (count > 1) {
        srat_t *values = (srat_t *)malloc(count * sizeof(srat_t));
        if (values == NULL) {
          ALOGE("%s: No memory for signed rational array", __func__);
          rc = -1;
        } else {
          memcpy(values, data, count * sizeof(srat_t));
          p_info_data[numOfEntries].tag_entry.data._srats = values;
        }
      } else {
        p_info_data[numOfEntries].tag_entry.data._srat = *(srat_t *)data;
      }
    }
    break;
    }

    // Increase number of entries
    p_exif_info->numOfEntries++;
    return rc;
}

/** releaseExifEntry
 *
 *  Arguments:
 *   @p_exif_data : Exif info struct
 *
 *  Retrun     : int32_t type of status
 *               0  -- success
 *              none-zero failure code
 *
 *  Description:
 *       Function to release an entry from exif data
 *
 **/
int32_t releaseExifEntry(QEXIF_INFO_DATA *p_exif_data)
{
 switch (p_exif_data->tag_entry.type) {
  case EXIF_BYTE: {
    if (p_exif_data->tag_entry.count > 1 &&
      p_exif_data->tag_entry.data._bytes != NULL) {
      free(p_exif_data->tag_entry.data._bytes);
      p_exif_data->tag_entry.data._bytes = NULL;
    }
  }
  break;
  case EXIF_ASCII: {
    if (p_exif_data->tag_entry.data._ascii != NULL) {
      free(p_exif_data->tag_entry.data._ascii);
      p_exif_data->tag_entry.data._ascii = NULL;
    }
  }
  break;
  case EXIF_SHORT: {
    if (p_exif_data->tag_entry.count > 1 &&
      p_exif_data->tag_entry.data._shorts != NULL) {
      free(p_exif_data->tag_entry.data._shorts);
      p_exif_data->tag_entry.data._shorts = NULL;
    }
  }
  break;
  case EXIF_LONG: {
    if (p_exif_data->tag_entry.count > 1 &&
      p_exif_data->tag_entry.data._longs != NULL) {
      free(p_exif_data->tag_entry.data._longs);
      p_exif_data->tag_entry.data._longs = NULL;
    }
  }
  break;
  case EXIF_RATIONAL: {
    if (p_exif_data->tag_entry.count > 1 &&
      p_exif_data->tag_entry.data._rats != NULL) {
      free(p_exif_data->tag_entry.data._rats);
      p_exif_data->tag_entry.data._rats = NULL;
    }
  }
  break;
  case EXIF_UNDEFINED: {
    if (p_exif_data->tag_entry.data._undefined != NULL) {
      free(p_exif_data->tag_entry.data._undefined);
      p_exif_data->tag_entry.data._undefined = NULL;
    }
  }
  break;
  case EXIF_SLONG: {
    if (p_exif_data->tag_entry.count > 1 &&
      p_exif_data->tag_entry.data._slongs != NULL) {
      free(p_exif_data->tag_entry.data._slongs);
      p_exif_data->tag_entry.data._slongs = NULL;
    }
  }
  break;
  case EXIF_SRATIONAL: {
    if (p_exif_data->tag_entry.count > 1 &&
      p_exif_data->tag_entry.data._srats != NULL) {
      free(p_exif_data->tag_entry.data._srats);
      p_exif_data->tag_entry.data._srats = NULL;
    }
  }
  break;
  } /*end of switch*/

  return 0;
}

/** process_sensor_data:
 *
 *  Arguments:
 *   @p_sensor_params : ptr to sensor data
 *
 *  Return     : int32_t type of status
 *               NO_ERROR  -- success
 *              none-zero failure code
 *
 *  Description:
 *       process sensor data
 *
 *  Notes: this needs to be filled for the metadata
 **/
int process_sensor_data(cam_sensor_params_t *p_sensor_params,
  QOMX_EXIF_INFO *exif_info, mm_jpeg_exif_params_t *p_cam_exif_params)
{
  int rc = 0;
  rat_t val_rat;

  if (NULL == p_sensor_params) {
    ALOGE("%s %d: Sensor params are null", __func__, __LINE__);
    return 0;
  }

  CDBG_HIGH("%s:%d] From metadata aperture = %f ", __func__, __LINE__,
    p_sensor_params->aperture_value );

  val_rat.num = (uint32_t)(p_sensor_params->aperture_value * 100);
  val_rat.denom = 100;
  rc = addExifEntry(exif_info, EXIFTAGID_APERTURE, EXIF_RATIONAL, 1, &val_rat);
  if (rc) {
    ALOGE("%s:%d]: Error adding Exif Entry", __func__, __LINE__);
  }

  short flash_tag = -1;
  uint8_t flash_fired = 0;
  uint8_t strobe_state = 0;
  uint8_t flash_mode = 0;
  uint8_t flash_presence = 0;
  uint8_t red_eye_mode = 0;

  if (!p_cam_exif_params->flash_presence) {
    if (p_cam_exif_params->ui_flash_mode == CAM_FLASH_MODE_AUTO) {
      CDBG_HIGH("%s %d: flashmode auto, take from sensor: %d", __func__, __LINE__,
        p_sensor_params->flash_mode);
      if(p_sensor_params->flash_mode == CAM_FLASH_MODE_ON)
        flash_fired = FLASH_FIRED;
      else if(p_sensor_params->flash_mode == CAM_FLASH_MODE_OFF)
        flash_fired = FLASH_NOT_FIRED;

      flash_mode = CAMERA_FLASH_AUTO;
    } else {
      CDBG_HIGH("%s %d: flashmode from ui: %d", __func__, __LINE__,
                 p_cam_exif_params->ui_flash_mode);
      if (p_cam_exif_params->ui_flash_mode == CAM_FLASH_MODE_ON) {
        flash_mode = CAMERA_FLASH_COMPULSORY;
        flash_fired = FLASH_FIRED;
      } else if(p_cam_exif_params->ui_flash_mode == CAM_FLASH_MODE_OFF) {
        flash_mode = CAMERA_FLASH_SUPRESSION;
        flash_fired = FLASH_NOT_FIRED;
      }
   }

   if((p_cam_exif_params->red_eye) && (flash_fired == FLASH_FIRED))
     red_eye_mode = REDEYE_MODE;

  } else {
    flash_presence = NO_FLASH_FUNC;
    red_eye_mode = NO_REDEYE_MODE;
  }

  /* No strobe flash support */
  strobe_state = NO_STROBE_RETURN_DETECT;

  /* Generating the flash tag */
  flash_tag = 0x00 | flash_fired |
    strobe_state | flash_mode |
    flash_presence | red_eye_mode;

  CDBG_HIGH("%s %d: flash_tag: 0x%x", __func__, __LINE__, flash_tag);


  /*FLASH*/
  rc = addExifEntry(exif_info, EXIFTAGID_FLASH, EXIF_SHORT,
    sizeof(flash_tag)/2, &flash_tag);
  if (rc) {
    ALOGE("%s:%d]: Error adding flash Exif Entry", __func__, __LINE__);
  }

  return rc;
}


/** process_3a_data:
 *
 *  Arguments:
 *   @p_ae_params : ptr to aec data
 *
 *  Return     : int32_t type of status
 *               NO_ERROR  -- success
 *              none-zero failure code
 *
 *  Description:
 *       process 3a data
 *
 *  Notes: this needs to be filled for the metadata
 **/
int process_3a_data(cam_ae_params_t *p_ae_params, cam_awb_params_t *p_awb_params,
        cam_auto_focus_data_t *p_focus_data, QOMX_EXIF_INFO *exif_info)
{
  int rc = 0;
  srat_t val_srat;
  rat_t val_rat;
  double shutter_speed_value;
  uint16_t aaa_exif_buff[AAA_EXIF_BUF_SIZE];
  uint32_t exif_byte_cnt = 0;

  memset(aaa_exif_buff, 0x0, sizeof(aaa_exif_buff));

  if (NULL == p_ae_params) {
    ALOGE("%s %d: AE params are null", __func__, __LINE__);
    /* increment exif_byte_cnt, so that this info will be filled with 0s */
    exif_byte_cnt += AE_EXIF_SIZE;
  } else {
    ALOGE("%s:%d] exp_time %f, iso_value %d exp idx: %d, lc: %d, gain: %f", __func__, __LINE__,
      p_ae_params->exp_time, p_ae_params->iso_value, p_ae_params->exp_index,
      p_ae_params->line_count, p_ae_params->real_gain);

    /* Exposure time */
    if (0.0f >= p_ae_params->exp_time) {
      val_rat.num = 0;
      val_rat.denom = 0;
    } else {
      val_rat.num = 1;
      val_rat.denom = ROUND(1.0/p_ae_params->exp_time);
    }
    CDBG_HIGH("%s: numer %d denom %d", __func__, val_rat.num, val_rat.denom );

    rc = addExifEntry(exif_info, EXIFTAGID_EXPOSURE_TIME, EXIF_RATIONAL,
      (sizeof(val_rat)/(8)), &val_rat);
    if (rc) {
      ALOGE("%s:%d]: Error adding Exif Entry Exposure time",
        __func__, __LINE__);
    }

    /* Shutter Speed*/
    if (p_ae_params->exp_time > 0) {
      shutter_speed_value = log10(1/p_ae_params->exp_time)/log10(2);
      val_srat.num = (int32_t)(shutter_speed_value * 1000.0f);
      val_srat.denom = 1000;
    } else {
      val_srat.num = 0;
      val_srat.denom = 0;
    }
    rc = addExifEntry(exif_info, EXIFTAGID_SHUTTER_SPEED, EXIF_SRATIONAL,
      (sizeof(val_srat)/(8)), &val_srat);
    if (rc) {
      ALOGE("%s:%d]: Error adding Exif Entry", __func__, __LINE__);
    }

    /* ISO */
    short val_short;
    val_short = (short) p_ae_params->iso_value;
    rc = addExifEntry(exif_info, EXIFTAGID_ISO_SPEED_RATING, EXIF_SHORT,
      sizeof(val_short)/2, &val_short);
    if (rc) {
      ALOGE("%s:%d]: Error adding Exif Entry ISO", __func__, __LINE__);
    }

    /* Gain */
    val_short = (short) p_ae_params->real_gain;
    rc = addExifEntry(exif_info, EXIFTAGID_GAIN_CONTROL, EXIF_SHORT,
      sizeof(val_short)/2, &val_short);
    if (rc) {
      ALOGE("%s:%d]: Error adding Exif Entry Gain", __func__, __LINE__);
    }

    /* Exposure Index */
    val_rat.num = p_ae_params->exp_index;
    val_rat.denom = 1;

    CDBG_HIGH("%s: numer %d denom %d", __func__, val_rat.num, val_rat.denom );

    rc = addExifEntry(exif_info, EXIFTAGID_EXPOSURE_INDEX, EXIF_RATIONAL,
      (sizeof(val_rat)/(8)), &val_rat);
    if (rc) {
      ALOGE("%s:%d]: Error adding Exif Entry Exposure Index",
        __func__, __LINE__);
    }

    /* AE line count */
    aaa_exif_buff[exif_byte_cnt++] = CHANGE_ENDIAN_16(LOWER(p_ae_params->line_count));
    aaa_exif_buff[exif_byte_cnt++] = CHANGE_ENDIAN_16(UPPER(p_ae_params->line_count));
  }

  if (NULL == p_awb_params) {
    ALOGE("%s %d: AWB params are null", __func__, __LINE__);
    /* increment exif_byte_cnt, so that this info will be filled with 0s */
    exif_byte_cnt += AWB_EXIF_SIZE;
  } else {
    aaa_exif_buff[exif_byte_cnt++] = CHANGE_ENDIAN_16(LOWER(p_awb_params->cct_value));
    aaa_exif_buff[exif_byte_cnt++] = CHANGE_ENDIAN_16(UPPER(p_awb_params->cct_value));
    aaa_exif_buff[exif_byte_cnt++] = CHANGE_ENDIAN_16(LOWER(p_awb_params->decision));
    aaa_exif_buff[exif_byte_cnt++] = CHANGE_ENDIAN_16(UPPER(p_awb_params->decision));
  }

  if (NULL == p_focus_data) {
    ALOGE("%s %d: AF params are null", __func__, __LINE__);
    /* increment exif_byte_cnt, so that this info will be filled with 0s */
    exif_byte_cnt += AF_EXIF_SIZE;
  } else {
    aaa_exif_buff[exif_byte_cnt++] = CHANGE_ENDIAN_16(LOWER(p_focus_data->focus_pos));
    aaa_exif_buff[exif_byte_cnt]   = CHANGE_ENDIAN_16(UPPER(p_focus_data->focus_pos));
  }

  /* Add to exif data */
  rc = addExifEntry(exif_info, EXIFTAGID_EXIF_MAKER_NOTE, EXIF_UNDEFINED,
    (exif_byte_cnt * 2), aaa_exif_buff);
  if (rc) {
    ALOGE("%s:%d]: Error adding Exif Entry Maker note", __func__, __LINE__);
  }

 return rc;

}

/** processMetaData:
 *
 *  Arguments:
 *   @p_meta : ptr to metadata
 *   @exif_info: Exif info struct
 *
 *  Return     : int32_t type of status
 *               NO_ERROR  -- success
 *              none-zero failure code
 *
 *  Description:
 *       process awb debug info
 *
 *  Notes: this needs to be filled for the metadata
 **/
int process_meta_data(cam_metadata_info_t *p_meta, QOMX_EXIF_INFO *exif_info,
  mm_jpeg_exif_params_t *p_cam_exif_params)
{
  int rc = 0;

  if (!p_meta) {
    ALOGE("%s %d:Meta data is NULL", __func__, __LINE__);
    return 0;
  }
  cam_ae_params_t *p_ae_params = p_meta->is_ae_params_valid ?
    &p_meta->ae_params : &p_cam_exif_params->ae_params;

  cam_awb_params_t *p_awb_params = p_meta->is_awb_params_valid ?
    &p_meta->awb_params : &p_cam_exif_params->awb_params;

  cam_auto_focus_data_t *p_focus_data = p_meta->is_focus_valid ?
    &p_meta->focus_data : &p_cam_exif_params->af_params;

  if(p_cam_exif_params->sensor_params.sens_type != CAM_SENSOR_YUV) {
      rc = process_3a_data(p_ae_params, p_awb_params, p_focus_data, exif_info);
      if (rc) {
        ALOGE("%s %d: Failed to extract 3a params", __func__, __LINE__);
      }
  }

  cam_sensor_params_t *p_sensor_params = p_meta->is_sensor_params_valid ?
    &p_meta->sensor_params : &p_cam_exif_params->sensor_params;

  if (NULL != p_sensor_params) {
    rc = process_sensor_data(p_sensor_params, exif_info, p_cam_exif_params);
    if (rc) {
      ALOGE("%s %d: Failed to extract sensor params", __func__, __LINE__);
    }
  }
  return rc;
}
