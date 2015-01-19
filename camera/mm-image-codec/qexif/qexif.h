/*Copyright (c) 2012, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.*/


#ifndef __QEXIF_H__
#define __QEXIF_H__

#include <stdio.h>

/* Exif Info (opaque definition) */
struct exif_info_t;
typedef struct exif_info_t * exif_info_obj_t;

/* Exif Tag ID */
typedef uint32_t exif_tag_id_t;


/* Exif Rational Data Type */
typedef struct
{
    uint32_t  num;    // Numerator
    uint32_t  denom;  // Denominator

} rat_t;

/* Exif Signed Rational Data Type */
typedef struct
{
    int32_t  num;    // Numerator
    int32_t  denom;  // Denominator

} srat_t;

/* Exif Tag Data Type */
typedef enum
{
    EXIF_BYTE      = 1,
    EXIF_ASCII     = 2,
    EXIF_SHORT     = 3,
    EXIF_LONG      = 4,
    EXIF_RATIONAL  = 5,
    EXIF_UNDEFINED = 7,
    EXIF_SLONG     = 9,
    EXIF_SRATIONAL = 10
} exif_tag_type_t;

/* Exif Tag Entry
 * Used in exif_set_tag as an input argument and
 * in exif_get_tag as an output argument. */
typedef struct
{
    /* The Data Type of the Tag *
     * Rational, etc */
    exif_tag_type_t type;

    /* Copy
     * This field is used when a user pass this structure to
     * be stored in an exif_info_t via the exif_set_tag method.
     * The routine would look like this field and decide whether
     * it is necessary to make a copy of the data pointed by this
     * structure (all string and array types).
     * If this field is set to false, only a pointer to the actual
     * data is retained and it is the caller's responsibility to
     * ensure the validity of the data before the exif_info_t object
     * is destroyed.
     */
    uint8_t copy;

    /* Data count
     * This indicates the number of elements of the data. For example, if
     * the type is EXIF_BYTE and the count is 1, that means the actual data
     * is one byte and is accessible by data._byte. If the type is EXIF_BYTE
     * and the count is more than one, the actual data is contained in an
     * array and is accessible by data._bytes. In case of EXIF_ASCII, it
     * indicates the string length and in case of EXIF_UNDEFINED, it indicates
     * the length of the array.
     */
    uint32_t count;

    /* Data
     * A union which covers all possible data types. The user should pick
     * the right field to use depending on the data type and the count.
     * See in-line comment below.
     */
    union
    {
        char      *_ascii;      // EXIF_ASCII (count indicates string length)
        uint8_t   *_bytes;      // EXIF_BYTE  (count > 1)
        uint8_t    _byte;       // EXIF_BYTE  (count = 1)
        uint16_t  *_shorts;     // EXIF_SHORT (count > 1)
        uint16_t   _short;      // EXIF_SHORT (count = 1)
        uint32_t  *_longs;      // EXIF_LONG  (count > 1)
        uint32_t   _long;       // EXIF_LONG  (count = 1)
        rat_t     *_rats;       // EXIF_RATIONAL  (count > 1)
        rat_t      _rat;        // EXIF_RATIONAL  (count = 1)
        uint8_t   *_undefined;  // EXIF_UNDEFINED (count indicates length)
        int32_t   *_slongs;     // EXIF_SLONG (count > 1)
        int32_t    _slong;      // EXIF_SLONG (count = 1)
        srat_t    *_srats;      // EXIF_SRATIONAL (count > 1)
        srat_t     _srat;       // EXIF_SRATIONAL (count = 1)

    } data;

} exif_tag_entry_t;

/* =======================================================================
**                          Macro Definitions
** ======================================================================= */
/* Enum defined to let compiler generate unique offset numbers for different
 * tags - ordering matters! NOT INTENDED to be used by any application. */
typedef enum
{
    // GPS IFD
    GPS_VERSION_ID = 0,
    GPS_LATITUDE_REF,
    GPS_LATITUDE,
    GPS_LONGITUDE_REF,
    GPS_LONGITUDE,
    GPS_ALTITUDE_REF,
    GPS_ALTITUDE,
    GPS_TIMESTAMP,
    GPS_SATELLITES,
    GPS_STATUS,
    GPS_MEASUREMODE,
    GPS_DOP,
    GPS_SPEED_REF,
    GPS_SPEED,
    GPS_TRACK_REF,
    GPS_TRACK,
    GPS_IMGDIRECTION_REF,
    GPS_IMGDIRECTION,
    GPS_MAPDATUM,
    GPS_DESTLATITUDE_REF,
    GPS_DESTLATITUDE,
    GPS_DESTLONGITUDE_REF,
    GPS_DESTLONGITUDE,
    GPS_DESTBEARING_REF,
    GPS_DESTBEARING,
    GPS_DESTDISTANCE_REF,
    GPS_DESTDISTANCE,
    GPS_PROCESSINGMETHOD,
    GPS_AREAINFORMATION,
    GPS_DATESTAMP,
    GPS_DIFFERENTIAL,

    // TIFF IFD
    NEW_SUBFILE_TYPE,
    SUBFILE_TYPE,
    IMAGE_WIDTH,
    IMAGE_LENGTH,
    BITS_PER_SAMPLE,
    COMPRESSION,
    PHOTOMETRIC_INTERPRETATION,
    THRESH_HOLDING,
    CELL_WIDTH,
    CELL_HEIGHT,
    FILL_ORDER,
    DOCUMENT_NAME,
    IMAGE_DESCRIPTION,
    MAKE,
    MODEL,
    STRIP_OFFSETS,
    ORIENTATION,
    SAMPLES_PER_PIXEL,
    ROWS_PER_STRIP,
    STRIP_BYTE_COUNTS,
    MIN_SAMPLE_VALUE,
    MAX_SAMPLE_VALUE,
    X_RESOLUTION,
    Y_RESOLUTION,
    PLANAR_CONFIGURATION,
    PAGE_NAME,
    X_POSITION,
    Y_POSITION,
    FREE_OFFSET,
    FREE_BYTE_COUNTS,
    GRAY_RESPONSE_UNIT,
    GRAY_RESPONSE_CURVE,
    T4_OPTION,
    T6_OPTION,
    RESOLUTION_UNIT,
    PAGE_NUMBER,
    TRANSFER_FUNCTION,
    SOFTWARE,
    DATE_TIME,
    ARTIST,
    HOST_COMPUTER,
    PREDICTOR,
    WHITE_POINT,
    PRIMARY_CHROMATICITIES,
    COLOR_MAP,
    HALFTONE_HINTS,
    TILE_WIDTH,
    TILE_LENGTH,
    TILE_OFFSET,
    TILE_BYTE_COUNTS,
    INK_SET,
    INK_NAMES,
    NUMBER_OF_INKS,
    DOT_RANGE,
    TARGET_PRINTER,
    EXTRA_SAMPLES,
    SAMPLE_FORMAT,
    TRANSFER_RANGE,
    JPEG_PROC,
    JPEG_INTERCHANGE_FORMAT,
    JPEG_INTERCHANGE_FORMAT_LENGTH,
    JPEG_RESTART_INTERVAL,
    JPEG_LOSSLESS_PREDICTORS,
    JPEG_POINT_TRANSFORMS,
    JPEG_Q_TABLES,
    JPEG_DC_TABLES,
    JPEG_AC_TABLES,
    YCBCR_COEFFICIENTS,
    YCBCR_SUB_SAMPLING,
    YCBCR_POSITIONING,
    REFERENCE_BLACK_WHITE,
    GAMMA,
    ICC_PROFILE_DESCRIPTOR,
    SRGB_RENDERING_INTENT,
    IMAGE_TITLE,
    COPYRIGHT,
    EXIF_IFD,
    ICC_PROFILE,
    GPS_IFD,


    // TIFF IFD (Thumbnail)
    TN_IMAGE_WIDTH,
    TN_IMAGE_LENGTH,
    TN_BITS_PER_SAMPLE,
    TN_COMPRESSION,
    TN_PHOTOMETRIC_INTERPRETATION,
    TN_IMAGE_DESCRIPTION,
    TN_MAKE,
    TN_MODEL,
    TN_STRIP_OFFSETS,
    TN_ORIENTATION,
    TN_SAMPLES_PER_PIXEL,
    TN_ROWS_PER_STRIP,
    TN_STRIP_BYTE_COUNTS,
    TN_X_RESOLUTION,
    TN_Y_RESOLUTION,
    TN_PLANAR_CONFIGURATION,
    TN_RESOLUTION_UNIT,
    TN_TRANSFER_FUNCTION,
    TN_SOFTWARE,
    TN_DATE_TIME,
    TN_ARTIST,
    TN_WHITE_POINT,
    TN_PRIMARY_CHROMATICITIES,
    TN_JPEGINTERCHANGE_FORMAT,
    TN_JPEGINTERCHANGE_FORMAT_L,
    TN_YCBCR_COEFFICIENTS,
    TN_YCBCR_SUB_SAMPLING,
    TN_YCBCR_POSITIONING,
    TN_REFERENCE_BLACK_WHITE,
    TN_COPYRIGHT,

    // EXIF IFD
    EXPOSURE_TIME,
    F_NUMBER,
    EXPOSURE_PROGRAM,
    SPECTRAL_SENSITIVITY,
    ISO_SPEED_RATING,
    OECF,
    EXIF_VERSION,
    EXIF_DATE_TIME_ORIGINAL,
    EXIF_DATE_TIME_DIGITIZED,
    EXIF_COMPONENTS_CONFIG,
    EXIF_COMPRESSED_BITS_PER_PIXEL,
    SHUTTER_SPEED,
    APERTURE,
    BRIGHTNESS,
    EXPOSURE_BIAS_VALUE,
    MAX_APERTURE,
    SUBJECT_DISTANCE,
    METERING_MODE,
    LIGHT_SOURCE,
    FLASH,
    FOCAL_LENGTH,
    SUBJECT_AREA,
    EXIF_MAKER_NOTE,
    EXIF_USER_COMMENT,
    SUBSEC_TIME,
    SUBSEC_TIME_ORIGINAL,
    SUBSEC_TIME_DIGITIZED,
    EXIF_FLASHPIX_VERSION,
    EXIF_COLOR_SPACE,
    EXIF_PIXEL_X_DIMENSION,
    EXIF_PIXEL_Y_DIMENSION,
    RELATED_SOUND_FILE,
    INTEROP,
    FLASH_ENERGY,
    SPATIAL_FREQ_RESPONSE,
    FOCAL_PLANE_X_RESOLUTION,
    FOCAL_PLANE_Y_RESOLUTION,
    FOCAL_PLANE_RESOLUTION_UNIT,
    SUBJECT_LOCATION,
    EXPOSURE_INDEX,
    SENSING_METHOD,
    FILE_SOURCE,
    SCENE_TYPE,
    CFA_PATTERN,
    CUSTOM_RENDERED,
    EXPOSURE_MODE,
    WHITE_BALANCE,
    DIGITAL_ZOOM_RATIO,
    FOCAL_LENGTH_35MM,
    SCENE_CAPTURE_TYPE,
    GAIN_CONTROL,
    CONTRAST,
    SATURATION,
    SHARPNESS,
    DEVICE_SETTINGS_DESCRIPTION,
    SUBJECT_DISTANCE_RANGE,
    IMAGE_UID,
    PIM,

    EXIF_TAG_MAX_OFFSET

} exif_tag_offset_t;

/* Below are the supported Tags (ID and structure for their data) */
#define CONSTRUCT_TAGID(offset,ID) (offset << 16 | ID)

// GPS tag version
// Use EXIFTAGTYPE_GPS_VERSION_ID as the exif_tag_type (EXIF_BYTE)
// Count should be 4
#define _ID_GPS_VERSION_ID 0x0000
#define EXIFTAGID_GPS_VERSION_ID \
  CONSTRUCT_TAGID(GPS_VERSION_ID, _ID_GPS_VERSION_ID)
#define EXIFTAGTYPE_GPS_VERSION_ID EXIF_BYTE
// North or South Latitude
// Use EXIFTAGTYPE_GPS_LATITUDE_REF as the exif_tag_type (EXIF_ASCII)
// It should be 2 characters long including the null-terminating character.
#define _ID_GPS_LATITUDE_REF 0x0001
#define EXIFTAGID_GPS_LATITUDE_REF \
  CONSTRUCT_TAGID(GPS_LATITUDE_REF, _ID_GPS_LATITUDE_REF)
#define EXIFTAGTYPE_GPS_LATITUDE_REF EXIF_ASCII
// Latitude
// Use EXIFTAGTYPE_GPS_LATITUDE as the exif_tag_type (EXIF_RATIONAL)
// Count should be 3
#define _ID_GPS_LATITUDE 0x0002
#define EXIFTAGID_GPS_LATITUDE CONSTRUCT_TAGID(GPS_LATITUDE, _ID_GPS_LATITUDE)
#define EXIFTAGTYPE_GPS_LATITUDE EXIF_RATIONAL
// East or West Longitude
// Use EXIFTAGTYPE_GPS_LONGITUDE_REF as the exif_tag_type (EXIF_ASCII)
// It should be 2 characters long including the null-terminating character.
#define _ID_GPS_LONGITUDE_REF 0x0003
#define EXIFTAGID_GPS_LONGITUDE_REF \
  CONSTRUCT_TAGID(GPS_LONGITUDE_REF, _ID_GPS_LONGITUDE_REF)
#define EXIFTAGTYPE_GPS_LONGITUDE_REF EXIF_ASCII
// Longitude
// Use EXIFTAGTYPE_GPS_LONGITUDE as the exif_tag_type (EXIF_RATIONAL)
// Count should be 3
#define _ID_GPS_LONGITUDE 0x0004
#define EXIFTAGID_GPS_LONGITUDE \
  CONSTRUCT_TAGID(GPS_LONGITUDE, _ID_GPS_LONGITUDE)
#define EXIFTAGTYPE_GPS_LONGITUDE EXIF_RATIONAL
// Altitude reference
// Use EXIFTAGTYPE_GPS_ALTITUDE_REF as the exif_tag_type (EXIF_BYTE)
#define _ID_GPS_ALTITUDE_REF 0x0005
#define EXIFTAGID_GPS_ALTITUDE_REF \
  CONSTRUCT_TAGID(GPS_ALTITUDE_REF, _ID_GPS_ALTITUDE_REF)
#define EXIFTAGTYPE_GPS_ALTITUDE_REF EXIF_BYTE
// Altitude
// Use EXIFTAGTYPE_GPS_ALTITUDE as the exif_tag_type (EXIF_RATIONAL)
#define _ID_GPS_ALTITUDE 0x0006
#define EXIFTAGID_GPS_ALTITUDE CONSTRUCT_TAGID(GPS_ALTITUDE, _ID_GPS_ALTITUDE)
#define EXIFTAGTYPE_GPS_ALTITUE EXIF_RATIONAL
// GPS time (atomic clock)
// Use EXIFTAGTYPE_GPS_TIMESTAMP as the exif_tag_type (EXIF_RATIONAL)
// Count should be 3
#define _ID_GPS_TIMESTAMP 0x0007
#define EXIFTAGID_GPS_TIMESTAMP \
  CONSTRUCT_TAGID(GPS_TIMESTAMP, _ID_GPS_TIMESTAMP)
#define EXIFTAGTYPE_GPS_TIMESTAMP EXIF_RATIONAL
// GPS Satellites
// Use EXIFTAGTYPE_GPS_SATELLITES as the exif_tag_type (EXIF_ASCII)
// Count can be anything.
#define _ID_GPS_SATELLITES 0x0008
#define EXIFTAGID_GPS_SATELLITES \
 CONSTRUCT_TAGID(GPS_SATELLITES, _ID_GPS_SATELLITES)
#define EXIFTAGTYPE_GPS_SATELLITES EXIF_ASCII
// GPS Status
// Use EXIFTAGTYPE_GPS_STATUS as the exif_tag_type (EXIF_ASCII)
// It should be 2 characters long including the null-terminating character.
// "A" - Measurement in progress
// "V" - Measurement Interoperability
// Other - Reserved
#define _ID_GPS_STATUS 0x0009
#define EXIFTAGID_GPS_STATUS CONSTRUCT_TAGID(GPS_STATUS, _ID_GPS_STATUS)
#define EXIFTATTYPE_GPS_STATUS EXIF_ASCII
// GPS Measure Mode
// Use EXIFTAGTYPE_GPS_MEASUREMODE as the exif_tag_type (EXIF_ASCII)
// It should be 2 characters long including the null-terminating character.
// "2" - 2-dimensional measurement
// "3" - 3-dimensional measurement
// Other - Reserved
#define _ID_GPS_MEASUREMODE 0x000a
#define EXIFTAGID_GPS_MEASUREMODE \
  CONSTRUCT_TAGID(GPS_MEASUREMODE, _ID_GPS_MEASUREMODE)
#define EXIFTAGTYPE_GPS_MEASUREMODE EXIF_ASCII
// GPS Measurement precision (DOP)
// Use EXIFTAGTYPE_GPS_DOP as the exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_GPS_DOP 0x000b
#define EXIFTAGID_GPS_DOP CONSTRUCT_TAGID(GPS_DOP, _ID_GPS_DOP)
#define EXIFTAGTYPE_GPS_DOP EXIF_RATIONAL
// Speed Unit
// Use EXIFTAGTYPE_GPS_SPEED_REF as the exif_tag_type (EXIF_ASCII)
// It should be 2 characters long including the null-terminating character.
// "K" - Kilometers per hour
// "M" - Miles per hour
// "N" - Knots
// Other - Reserved
#define _ID_GPS_SPEED_REF 0x000c
#define EXIFTAGID_GPS_SPEED_REF \
  CONSTRUCT_TAGID(GPS_SPEED_REF, _ID_GPS_SPEED_REF)
#define EXIFTAGTYPE_GPS_SPEED_REF EXIF_ASCII
// Speed of GPS receiver
// Use EXIFTAGTYPE_GPS_SPEED as the exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_GPS_SPEED 0x000d
#define EXIFTAGID_GPS_SPEED CONSTRUCT_TAGID(GPS_SPEED, _ID_GPS_SPEED)
#define EXIFTAGTYPE_GPS_SPEED EXIF_RATIONAL
// Reference of direction of movement
// Use EXIFTAGTYPE_GPS_TRACK_REF as the exif_tag_type (EXIF_ASCII)
// It should be 2 characters long including the null-terminating character.
// "T" - True direction
// "M" - Magnetic direction
// Other - Reserved
#define _ID_GPS_TRACK_REF 0x000e
#define EXIFTAGID_GPS_TRACK_REF \
  CONSTRUCT_TAGID(GPS_TRACK_REF, _ID_GPS_TRACK_REF)
#define EXIFTAGTYPE_GPS_TRACK_REF EXIF_ASCII
// Direction of movement
// Use EXIFTAGTYPE_GPS_TRACK as the exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_GPS_TRACK 0x000f
#define EXIFTAGID_GPS_TRACK CONSTRUCT_TAGID(GPS_TRACK, _ID_GPS_TRACK)
#define EXIFTAGTYPE_GPS_TRACK EXIF_RATIONAL
// Reference of direction of image
// Use EXIFTAGTYPE_GPS_IMGDIRECTION_REF as the exif_tag_type (EXIF_ASCII)
// It should be 2 characters long including the null-terminating character.
// "T" - True direction
// "M" - Magnetic direction
// Other - Reserved
#define _ID_GPS_IMGDIRECTION_REF 0x0010
#define EXIFTAGID_GPS_IMGDIRECTION_REF \
  CONSTRUCT_TAGID(GPS_IMGDIRECTION_REF, _ID_GPS_IMGDIRECTION_REF)
#define EXIFTAGTYPE_GPS_IMGDIRECTION_REF EXIF_ASCII
// Direction of image
// Use EXIFTAGTYPE_GPS_IMGDIRECTION as the exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_GPS_IMGDIRECTION 0x0011
#define EXIFTAGID_GPS_IMGDIRECTION \
  CONSTRUCT_TAGID(GPS_IMGDIRECTION, _ID_GPS_IMGDIRECTION)
#define EXIFTAGTYPE_GPS_IMGDIRECTION EXIF_RATIONAL
// Geodetic survey data used
// Use EXIFTAGTYPE_GPS_MAPDATUM as the exif_tag_type (EXIF_ASCII)
// Count can be any
#define _ID_GPS_MAPDATUM 0x0012
#define EXIFTAGID_GPS_MAPDATUM CONSTRUCT_TAGID(GPS_MAPDATUM, _ID_GPS_MAPDATUM)
#define EXIFTAGTYPE_GPS_MAPDATUM EXIF_ASCII
// Reference for latitude of destination
// Use EXIFTAGTYPE_GPS_DESTLATITUDE_REF as the exif_tag_type (EXIF_ASCII)
// It should be 2 characters long including the null-terminating character.
// "N" - North latitude
// "S" - South latitude
// Other - Reserved
#define _ID_GPS_DESTLATITUDE_REF 0x0013
#define EXIFTAGID_GPS_DESTLATITUDE_REF \
  CONSTRUCT_TAGID(GPS_DESTLATITUDE_REF, _ID_GPS_DESTLATITUDE_REF)
#define EXIFTAGTYPE_GPS_DESTLATITUDE_REF EXIF_ASCII
// Latitude of destination
// Use EXIFTAGTYPE_GPS_DESTLATITUDE as the exif_tag_type (EXIF_RATIONAL)
// Count should be 3
#define _ID_GPS_DESTLATITUDE 0x0014
#define EXIFTAGID_GPS_DESTLATITUDE \
  CONSTRUCT_TAGID(GPS_DESTLATITUDE, _ID_GPS_DESTLATITUDE)
#define EXIFTAGTYPE_GPS_DESTLATITUDE EXIF_RATIONAL
// Reference for longitude of destination
// Use EXIFTAGTYPE_GPS_DESTLONGITUDE_REF as the exif_tag_type (EXIF_ASCII)
// It should be 2 characters long including the null-terminating character.
// "E" - East longitude
// "W" - West longitude
// Other - Reserved
#define _ID_GPS_DESTLONGITUDE_REF 0x0015
#define EXIFTAGID_GPS_DESTLONGITUDE_REF \
  CONSTRUCT_TAGID(GPS_DESTLONGITUDE_REF, _ID_GPS_DESTLONGITUDE_REF)
#define EXIFTAGTYPE_GPS_DESTLONGITUDE_REF EXIF_ASCII
// Longitude of destination
// Use EXIFTAGTYPE_GPS_DESTLONGITUDE as the exif_tag_type (EXIF_RATIONAL)
// Count should be 3
#define _ID_GPS_DESTLONGITUDE 0x0016
#define EXIFTAGID_GPS_DESTLONGITUDE CONSTRUCT_TAGID(GPS_DESTLONGITUDE, _ID_GPS_DESTLONGITUDE)
#define EXIFTAGTYPE_GPS_DESTLONGITUDE EXIF_RATIONAL
// Reference for bearing of destination
// Use EXIFTAGTYPE_GPS_DESTBEARING_REF as the exif_tag_type (EXIF_ASCII)
// It should be 2 characters long including the null-terminating character.
// "T" - True direction
// "M" - Magnetic direction
// Other - Reserved
#define _ID_GPS_DESTBEARING_REF 0x0017
#define EXIFTAGID_GPS_DESTBEARING_REF \
  CONSTRUCT_TAGID(GPS_DESTBEARING_REF, _ID_GPS_DESTBEARING_REF)
#define EXIFTAGTYPE_GPS_DESTBEARING_REF EXIF_ASCII
// Bearing of destination
// Use EXIFTAGTYPE_GPS_DESTBEARING as the exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_GPS_DESTBEARING 0x0018
#define EXIFTAGID_GPS_DESTBEARING \
  CONSTRUCT_TAGID(GPS_DESTBEARING, _ID_GPS_DESTBEARING)
#define EXIFTAGTYPE_GPS_DESTBEARING EXIF_RATIONAL
// Reference for distance to destination
// Use EXIFTAGTYPE_GPS_DESTDISTANCE_REF as the exif_tag_type (EXIF_ASCII)
// It should be 2 characters long including the null-terminating character.
// "K" - Kilometers per hour
// "M" - Miles per hour
// "N" - Knots
// Other - Reserved
#define _ID_GPS_DESTDISTANCE_REF 0x0019
#define EXIFTAGID_GPS_DESTDISTANCE_REF \
  CONSTRUCT_TAGID(GPS_DESTDISTANCE_REF, _ID_GPS_DESTDISTANCE_REF)
#define EXIFTAGTYPE_GPS_DESTDISTANCE_REF EXIF_ASCII
// Distance to destination
// Use EXIFTAGTYPE_GPS_DESTDISTANCE as the exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_GPS_DESTDISTANCE 0x001a
#define EXIFTAGID_GPS_DESTDISTANCE \
  CONSTRUCT_TAGID(GPS_DESTDISTANCE, _ID_GPS_DESTDISTANCE)
#define EXIFTAGTYPE_GPS_DESTDISTANCE EXIF_RATIONAL
// Name of GPS processing method
// Use EXIFTAGTYPE_GPS_PROCESSINGMETHOD as the exif_tag_type (EXIF_UNDEFINED)
// Count can be any
#define _ID_GPS_PROCESSINGMETHOD 0x001b
#define EXIFTAGID_GPS_PROCESSINGMETHOD \
  CONSTRUCT_TAGID(GPS_PROCESSINGMETHOD, _ID_GPS_PROCESSINGMETHOD)
#define EXIFTAGTYPE_GPS_PROCESSINGMETHOD EXIF_UNDEFINED
// Name of GPS area
// Use EXIFTAGTYPE_GPS_AREAINFORMATION as the exif_tag_type (EXIF_UNDEFINED)
// Count can be any
#define _ID_GPS_AREAINFORMATION 0x001c
#define EXIFTAGID_GPS_AREAINFORMATION \
  CONSTRUCT_TAGID(GPS_AREAINFORMATION, _ID_GPS_AREAINFORMATION)
#define EXIFTAGTYPE_GPS_AREAINFORMATION EXIF_UNDEFINED
// GPS date
// Use EXIFTAGTYPE_GPS_DATESTAMP as the exif_tag_type (EXIF_ASCII)
// It should be 11 characters long including the null-terminating character.
#define _ID_GPS_DATESTAMP 0x001d
#define EXIFTAGID_GPS_DATESTAMP \
  CONSTRUCT_TAGID(GPS_DATESTAMP, _ID_GPS_DATESTAMP)
#define EXIFTAGTYPE_GPS_DATESTAMP EXIF_ASCII
// GPS differential correction
// Use EXIFTAGTYPE_GPS_DIFFERENTIAL as the exif_tag_type (EXIF_SHORT)
// Count should be 1
// 0 - Measurement without differential correction
// 1 - Differential correction applied
// Other - Reserved
#define _ID_GPS_DIFFERENTIAL 0x001e
#define EXIFTAGID_GPS_DIFFERENTIAL \
  CONSTRUCT_TAGID(GPS_DIFFERENTIAL, _ID_GPS_DIFFERENTIAL)
#define EXIFTAGTYPE_GPS_DIFFERENTIAL EXIF_SHORT
// Image width
// Use EXIFTAGTYPE_IMAGE_WIDTH as the exif_tag_type (EXIF_LONG)
// Count should be 1
#define _ID_IMAGE_WIDTH 0x0100
#define EXIFTAGID_IMAGE_WIDTH CONSTRUCT_TAGID(IMAGE_WIDTH, _ID_IMAGE_WIDTH)
#define EXIFTAGTYPE_IMAGE_WIDTH EXIF_LONG
// Image height
// Use EXIFTAGTYPE_IMAGE_LENGTH as the exif_tag_type (EXIF_SHORT_OR_LONG)
// Count should be 1
#define _ID_IMAGE_LENGTH 0x0101
#define EXIFTAGID_IMAGE_LENGTH CONSTRUCT_TAGID(IMAGE_LENGTH, _ID_IMAGE_LENGTH)
#define EXIFTAGTYPE_IMAGE_LENGTH EXIF_LONG
// Number of bits per component
// Use EXIFTAGTYPE_BITS_PER_SAMPLE as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_BITS_PER_SAMPLE 0x0102
#define EXIFTAGID_BITS_PER_SAMPLE \
  CONSTRUCT_TAGID(BITS_PER_SAMPLE, _ID_BITS_PER_SAMPLE)
#define EXIFTAGTYPE_BITS_PER_SAMPLE EXIF_SHORT
// Compression scheme
// Use EXIFTAGTYPE_COMPRESSION as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_COMPRESSION 0x0103
#define EXIFTAGID_COMPRESSION CONSTRUCT_TAGID(COMPRESSION, _ID_COMPRESSION)
#define EXIFTAGTYPE_COMPRESSION EXIF_SHORT
// Pixel composition
// Use EXIFTAGTYPE_PHOTOMETRIC_INTERPRETATION as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_PHOTOMETRIC_INTERPRETATION 0x0106
#define EXIFTAGID_PHOTOMETRIC_INTERPRETATION \
  CONSTRUCT_TAGID(PHOTOMETRIC_INTERPRETATION, _ID_PHOTOMETRIC_INTERPRETATION)
#define EXIFTAGTYPE_PHOTOMETRIC_INTERPRETATION EXIF_SHORT

// Thresholding
// Use EXIFTAGTYPE_THRESH_HOLDING as the exif_tag_type (EXIF_SHORT)
//
//1 = No dithering or halftoning
//2 = Ordered dither or halftone
//3 = Randomized dither
#define _ID_THRESH_HOLDING 0x0107
#define EXIFTAGID_THRESH_HOLDING \
  CONSTRUCT_TAGID(THRESH_HOLDING, _ID_THRESH_HOLDING)
#define EXIFTAGTYPE_THRESH_HOLDING EXIF_SHORT

// Cell Width
// Use EXIFTAGTYPE_CELL_WIDTH as the exif_tag_type (EXIF_SHORT)
//
#define _ID_CELL_WIDTH 0x0108
#define EXIFTAGID_CELL_WIDTH CONSTRUCT_TAGID(CELL_WIDTH, _ID_CELL_WIDTH)
#define EXIFTAGTYPE_CELL_WIDTH EXIF_SHORT
// Cell Height
// Use EXIFTAGTYPE_CELL_HEIGHT as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_CELL_HEIGHT 0x0109
#define EXIFTAGID_CELL_HEIGHT CONSTRUCT_TAGID(CELL_HEIGHT, _ID_CELL_HEIGHT)
#define EXIFTAGTYPE_CELL_HEIGHT EXIF_SHORT
// Fill Order
// Use EXIFTAGTYPE_FILL_ORDER as the exif_tag_type (EXIF_SHORT)
// 	1 = Normal
//  2 = Reversed
#define _ID_FILL_ORDER 0x010A
#define EXIFTAGID_FILL_ORDER CONSTRUCT_TAGID(FILL_ORDER, _ID_FILL_ORDER)
#define EXIFTAGTYPE_FILL_ORDER EXIF_SHORT

// DOCUMENT NAME
// Use EXIFTAGTYPE_DOCUMENT_NAME as the exif_tag_type (EXIF_ASCII)
//
#define _ID_DOCUMENT_NAME 0x010D
#define EXIFTAGID_DOCUMENT_NAME CONSTRUCT_TAGID(DOCUMENT_NAME, _ID_DOCUMENT_NAME)
#define EXIFTAGTYPE_DOCUMENT_NAME EXIF_ASCII

// Image title
// Use EXIFTAGTYPE_IMAGE_DESCRIPTION as the exif_tag_type (EXIF_ASCII)
// Count can be any
#define _ID_IMAGE_DESCRIPTION 0x010e
#define EXIFTAGID_IMAGE_DESCRIPTION \
  CONSTRUCT_TAGID(IMAGE_DESCRIPTION, _ID_IMAGE_DESCRIPTION)
#define EXIFTAGTYPE_IMAGE_DESCRIPTION EXIF_ASCII
// Image input equipment manufacturer
// Use EXIFTAGTYPE_MAKE as the exif_tag_type (EXIF_ASCII)
// Count can be any
#define _ID_MAKE 0x010f
#define EXIFTAGID_MAKE CONSTRUCT_TAGID(MAKE, _ID_MAKE)
#define EXIFTAGTYPE_MAKE EXIF_ASCII
// Image input equipment model
// Use EXIFTAGTYPE_MODEL as the exif_tag_type (EXIF_ASCII)
// Count can be any
#define _ID_MODEL 0x0110
#define EXIFTAGID_MODEL CONSTRUCT_TAGID(MODEL, _ID_MODEL)
#define EXIFTAGTYPE_MODEL EXIF_ASCII
// Image data location
// Use EXIFTAGTYPE_STRIP_OFFSETS as the exif_tag_type (EXIF_LONG)
// Count = StripsPerImage                    when PlanarConfiguration = 1
//       = SamplesPerPixel * StripsPerImage  when PlanarConfiguration = 2
#define _ID_STRIP_OFFSETS 0x0111
#define EXIFTAGID_STRIP_OFFSETS \
  CONSTRUCT_TAGID(STRIP_OFFSETS, _ID_STRIP_OFFSETS)
#define EXIFTAGTYPE_STRIP_OFFSETS EXIF_LONG
// Orientation of image
// Use EXIFTAGTYPE_ORIENTATION as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_ORIENTATION 0x0112
#define EXIFTAGID_ORIENTATION CONSTRUCT_TAGID(ORIENTATION, _ID_ORIENTATION)
#define EXIFTAGTYPE_ORIENTATION EXIF_SHORT
// Number of components
// Use EXIFTAGTYPE_SAMPLES_PER_PIXEL as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_SAMPLES_PER_PIXEL 0x0115
#define EXIFTAGID_SAMPLES_PER_PIXEL \
  CONSTRUCT_TAGID(SAMPLES_PER_PIXEL, _ID_SAMPLES_PER_PIXEL)
#define EXIFTAGTYPE_SAMPLES_PER_PIXEL EXIF_SHORT
// Number of rows per strip
// Use EXIFTAGTYPE_ROWS_PER_STRIP as the exif_tag_type (EXIF_LONG)
// Count should be 1
#define _ID_ROWS_PER_STRIP 0x0116
#define EXIFTAGID_ROWS_PER_STRIP \
  CONSTRUCT_TAGID(ROWS_PER_STRIP, _ID_ROWS_PER_STRIP)
#define EXIFTAGTYPE_ROWS_PER_STRIP EXIF_LONG
// Bytes per compressed strip
// Use EXIFTAGTYPE_STRIP_BYTE_COUNTS as the exif_tag_type (EXIF_LONG)
// Count = StripsPerImage                    when PlanarConfiguration = 1
//       = SamplesPerPixel * StripsPerImage  when PlanarConfiguration = 2
#define _ID_STRIP_BYTE_COUNTS 0x0117
#define EXIFTAGID_STRIP_BYTE_COUNTS \
  CONSTRUCT_TAGID(STRIP_BYTE_COUNTS, _ID_STRIP_BYTE_COUNTS)
#define EXIFTAGTYPE_STRIP_BYTE_COUNTS EXIF_LONG
// MinSampleValue
// Use EXIFTAGTYPE_MIN_SAMPLE_VALUE as the exif_tag_type (EXIF_SHORT)
#define _ID_MIN_SAMPLE_VALUE 0x0118
#define EXIFTAGID_MIN_SAMPLE_VALUE  \
  CONSTRUCT_TAGID(MIN_SAMPLE_VALUE, _ID_MIN_SAMPLE_VALUE)
#define EXIFTAGTYPE_MIN_SAMPLE_VALUE EXIF_SHORT
// MaxSampleValue
// Use EXIFTAGTYPE_MAX_SAMPLE_VALUE as the exif_tag_type (EXIF_SHORT)
#define _ID_MAX_SAMPLE_VALUE 0x0119
#define EXIFTAGID_MAX_SAMPLE_VALUE CONSTRUCT_TAGID(MAX_SAMPLE_VALUE, _ID_MAX_SAMPLE_VALUE)
#define EXIFTAGTYPE_MAX_SAMPLE_VALUE EXIF_SHORT

// Image resolution in width direction
// Use EXIFTAGTYPE_X_RESOLUTION as the exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_X_RESOLUTION 0x011a
#define EXIFTAGID_X_RESOLUTION \
  CONSTRUCT_TAGID(X_RESOLUTION, _ID_X_RESOLUTION)
#define EXIFTAGTYPE_X_RESOLUTION EXIF_RATIONAL
// Image resolution in height direction
// Use EXIFTAGTYPE_Y_RESOLUTION as the exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_Y_RESOLUTION 0x011b
#define EXIFTAGID_Y_RESOLUTION \
  CONSTRUCT_TAGID(Y_RESOLUTION, _ID_Y_RESOLUTION)
#define EXIFTAGTYPE_Y_RESOLUTION EXIF_RATIONAL
// Image data arrangement
// Use EXIFTAGTYPE_PLANAR_CONFIGURATION as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_PLANAR_CONFIGURATION 0x011c
#define EXIFTAGID_PLANAR_CONFIGURATION \
  CONSTRUCT_TAGID(PLANAR_CONFIGURATION, _ID_PLANAR_CONFIGURATION)
#define EXIFTAGTYPE_PLANAR_CONFIGURATION EXIF_SHORT
// PageName
// Use EXIFTAGTYPE_PAGE_NAME as the exif_tag_type (EXIF_ASCII)
// Count should be 1
#define _ID_PAGE_NAME 0x011d
#define EXIFTAGID_PAGE_NAME CONSTRUCT_TAGID(PAGE_NAME, _ID_PAGE_NAME)
#define EXIFTAGTYPE_PAGE_NAME EXIF_ASCII
// XPosition
// Use EXIFTAGTYPE_X_POSITION as the exif_tag_type (EXIF_RATIONAL)
//
#define _ID_X_POSITION 0x011e
#define EXIFTAGID_X_POSITION CONSTRUCT_TAGID(X_POSITION, _ID_X_POSITION)
#define EXIFTAGTYPE_X_POSITION EXIF_RATIONAL
// YPosition
// Use EXIFTAGTYPE_Y_POSITION as the exif_tag_type (EXIF_RATIONAL)
//
#define _ID_Y_POSITION 0x011f
#define EXIFTAGID_Y_POSITION CONSTRUCT_TAGID(Y_POSITION, _ID_Y_POSITION)
#define EXIFTAGTYPE_Y_POSITION EXIF_RATIONAL

// FREE_OFFSET
// Use EXIFTAGTYPE_FREE_OFFSET as the exif_tag_type (EXIF_LONG)
//
#define _ID_FREE_OFFSET 0x0120
#define EXIFTAGID_FREE_OFFSET CONSTRUCT_TAGID(FREE_OFFSET, _ID_FREE_OFFSET)
#define EXIFTAGTYPE_FREE_OFFSET EXIF_LONG
// FREE_BYTE_COUNTS
// Use EXIFTAGTYPE_FREE_BYTE_COUNTS as the exif_tag_type (EXIF_LONG)
//
#define _ID_FREE_BYTE_COUNTS 0x0121
#define EXIFTAGID_FREE_BYTE_COUNTS \
  CONSTRUCT_TAGID(FREE_BYTE_COUNTS, _ID_FREE_BYTE_COUNTS)
#define EXIFTAGTYPE_FREE_BYTE_COUNTS EXIF_LONG

// GrayResponseUnit
// Use EXIFTAGTYPE_GRAY_RESPONSE_UNIT as the exif_tag_type (EXIF_SHORT)
//
#define _ID_GRAY_RESPONSE_UNIT 0x0122
#define EXIFTAGID_GRAY_RESPONSE_UNIT \
  CONSTRUCT_TAGID(GRAY_RESPONSE_UNIT, _ID_GRAY_RESPONSE_UNIT)
#define EXIFTAGTYPE_GRAY_RESPONSE_UNIT EXIF_SHORT
// GrayResponseCurve
// Use EXIFTAGTYPE_GRAY_RESPONSE_CURVE  as the exif_tag_type (EXIF_SHORT)
//
#define _ID_GRAY_RESPONSE_CURVE 0x0123
#define EXIFTAGID_GRAY_RESPONSE_CURVE \
  CONSTRUCT_TAGID(GRAY_RESPONSE_CURVE , _ID_GRAY_RESPONSE_CURVE )
#define EXIFTAGTYPE_GRAY_RESPONSE_CURVE EXIF_SHORT

// T4_OPTION
// Use EXIFTAGTYPE_T4_OPTION as the exif_tag_type (EXIF_LONG)
//
#define _ID_T4_OPTION  0x0124
#define EXIFTAGID_T4_OPTION CONSTRUCT_TAGID(T4_OPTION, _ID_T4_OPTION)
#define EXIFTAGTYPE_T4_OPTION EXIF_LONG
// T6_OPTION
// Use EXIFTAGTYPE_T6_OPTION as the exif_tag_type (EXIF_LONG)
//
#define _ID_T6_OPTION 0x0125
#define EXIFTAGID_T6_OPTION CONSTRUCT_TAGID(T6_OPTION, _ID_T6_OPTION)
#define EXIFTAGTYPE_T6_OPTION EXIF_LONG

// Unit of X and Y resolution
// Use EXIFTAGTYPE_RESOLUTION_UNIT as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_RESOLUTION_UNIT 0x0128
#define EXIFTAGID_RESOLUTION_UNIT \
  CONSTRUCT_TAGID(RESOLUTION_UNIT, _ID_RESOLUTION_UNIT)
#define EXIFTAGTYPE_RESOLUTION_UNIT EXIF_SHORT

// Page Number
// Use EXIFTAGTYPE_PAGE_NUMBER  as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_PAGE_NUMBER 0x0129
#define EXIFTAGID_PAGE_NUMBER CONSTRUCT_TAGID(PAGE_NUMBER, _ID_PAGE_NUMBER)
#define EXIFTAGTYPE_PAGE_NUMBER EXIF_SHORT
// Transfer function
// Use EXIFTAGTYPE_TRANSFER_FUNCTION as the exif_tag_type (EXIF_SHORT)
// Count should be 3*256
#define _ID_TRANSFER_FUNCTION 0x012d
#define EXIFTAGID_TRANSFER_FUNCTION \
  CONSTRUCT_TAGID(TRANSFER_FUNCTION, _ID_TRANSFER_FUNCTION)
#define EXIFTAGTYPE_TRANSFER_FUNCTION EXIF_SHORT
// Software used
// Use EXIFTAGTYPE_SOFTWARE as the exif_tag_type (EXIF_ASCII)
// Count can be any
#define _ID_SOFTWARE 0x0131
#define EXIFTAGID_SOFTWARE CONSTRUCT_TAGID(SOFTWARE, _ID_SOFTWARE)
#define EXIFTAGTYPE_SOFTWARE EXIF_ASCII
// File change date and time
// Use EXIFTAGTYPE_DATE_TIME as the exif_tag_type (EXIF_ASCII)
// Count should be 20
#define _ID_DATE_TIME 0x0132
#define EXIFTAGID_DATE_TIME CONSTRUCT_TAGID(DATE_TIME, _ID_DATE_TIME)
#define EXIFTAGTYPE_DATE_TIME EXIF_ASCII
// ARTIST, person who created this image
// Use EXIFTAGTYPE_ARTIST as the exif_tag_type (EXIF_ASCII)
// Count can be any
#define _ID_ARTIST 0x013b
#define EXIFTAGID_ARTIST CONSTRUCT_TAGID(ARTIST, _ID_ARTIST)
#define EXIFTAGTYPE_ARTIST EXIF_ASCII
// Host Computer Name
// Use EXIFTAGTYPE_HOST_COMPUTER as the exif_tag_type (EXIF_ASCII)
// Count can be any
#define _ID_HOST_COMPUTER 0x013c
#define EXIFTAGID_HOST_COMPUTER \
  CONSTRUCT_TAGID(HOST_COMPUTER , _ID_HOST_COMPUTER )
#define EXIFTAGTYPE_HOST_COMPUTER EXIF_ASCII
// Predictor
// Use EXIFTAGTYPE_PREDICTOR as the exif_tag_type (EXIF_SHORT)
// Count can be any
#define _ID_PREDICTOR 0x013d
#define EXIFTAGID_PREDICTOR CONSTRUCT_TAGID(PREDICTOR , _ID_PREDICTOR )
#define EXIFTAGTYPE_PREDICTOR EXIF_SHORT
// White point chromaticity
// Use EXIFTAGTYPE_WHITE_POINT as the exif_tag_type (EXIF_RATIONAL)
// Count should be 2
#define _ID_WHITE_POINT 0x013e
#define EXIFTAGID_WHITE_POINT CONSTRUCT_TAGID(WHITE_POINT, _ID_WHITE_POINT)
#define EXIFTAGTYPE_WHITE_POINT EXIF_RATIONAL
// Chromaticities of primaries
// Use EXIFTAGTYPE_PRIMARY_CHROMATICITIES as the exif_tag_type (EXIF_RATIONAL)
// Count should be 6
#define _ID_PRIMARY_CHROMATICITIES 0x013f
#define EXIFTAGID_PRIMARY_CHROMATICITIES \
  CONSTRUCT_TAGID(PRIMARY_CHROMATICITIES, _ID_PRIMARY_CHROMATICITIES)
#define EXIFTAGTYPE_PRIMARY_CHROMATICITIES EXIF_RATIONAL

// COLOR_MAP
// Use EXIFTAGTYPE_COLOR_MAP as the exif_tag_type (EXIF_SHORT)
// Count should be 6
#define _ID_COLOR_MAP 0x0140
#define EXIFTAGID_COLOR_MAP CONSTRUCT_TAGID(COLOR_MAP, _ID_COLOR_MAP)
#define EXIFTAGTYPE_COLOR_MAP EXIF_SHORT
// HALFTONE_HINTS
// Use EXIFTAGTYPE_HALFTONE_HINTS as the exif_tag_type (EXIF_SHORT)
// Count should be 6
#define _ID_HALFTONE_HINTS 0x0141
#define EXIFTAGID_HALFTONE_HINTS \
  CONSTRUCT_TAGID(HALFTONE_HINTS, _ID_HALFTONE_HINTS)
#define EXIFTAGTYPE_HALFTONE_HINTS EXIF_SHORT

// TILE_WIDTH
// Use EXIFTAGTYPE_TILE_WIDTH as the exif_tag_type (EXIF_LONG)
// Count should be 6
#define _ID_TILE_WIDTH 0x0142
#define EXIFTAGID_TILE_WIDTH CONSTRUCT_TAGID(TILE_WIDTH, _ID_TILE_WIDTH)
#define EXIFTAGTYPE_TILE_WIDTH EXIF_LONG
// TILE_LENGTH
// Use EXIFTAGTYPE_TILE_LENGTH  as the exif_tag_type (EXIF_LONG)
// Count should be 6
#define _ID_TILE_LENGTH 0x0143
#define EXIFTAGID_TILE_LENGTH CONSTRUCT_TAGID(TILE_LENGTH , _ID_TILE_LENGTH )
#define EXIFTAGTYPE_TILE_LENGTH EXIF_LONG
// TILE_OFFSET
// Use EXIFTAGTYPE_TILE_OFFSET as the exif_tag_type (EXIF_LONG)
//
#define _ID_TILE_OFFSET 0x0144
#define EXIFTAGID_TILE_OFFSET CONSTRUCT_TAGID(TILE_OFFSET , _ID_TILE_OFFSET )
#define EXIFTAGTYPE_TILE_OFFSET EXIF_LONG
// tile Byte Counts
// Use EXIFTAGTYPE_TILE_OFFSET as the exif_tag_type (EXIF_LONG)
//
#define _ID_TILE_BYTE_COUNTS 0x0145
#define EXIFTAGID_TILE_BYTE_COUNTS  \
  CONSTRUCT_TAGID(TILE_BYTE_COUNTS  , _ID_TILE_BYTE_COUNTS  )
#define EXIFTAGTYPE_TILE_BYTE_COUNTS EXIF_LONG

// INK_SET
// Use EXIFTAGTYPE_TILE_LENGTH  as the exif_tag_type (EXIF_SHORT)
// Count should be 6
#define _ID_INK_SET 0x014c
#define EXIFTAGID_INK_SET CONSTRUCT_TAGID(INK_SET , _ID_INK_SET )
#define EXIFTAGTYPE_INK_SET EXIF_SHORT
// INK_NAMES
// Use EXIFTAGTYPE_INK_NAMES  as the exif_tag_type (EXIF_ASCII)
// Count should be 6
#define _ID_INK_NAMES 0x014D
#define EXIFTAGID_INK_NAMES CONSTRUCT_TAGID(INK_NAMES , _ID_INK_NAMES)
#define EXIFTAGTYPE_INK_NAMES EXIF_ASCII
// NUMBER_OF_INKS
// Use EXIFTAGTYPE_NUMBER_OF_INKS  as the exif_tag_type (EXIF_SHORT)
// Count should be 6
#define _ID_NUMBER_OF_INKS 0x014e
#define EXIFTAGID_NUMBER_OF_INKS \
  CONSTRUCT_TAGID(NUMBER_OF_INKS , _ID_NUMBER_OF_INKS )
#define EXIFTAGTYPE_NUMBER_OF_INKS EXIF_SHORT

// DOT_RANGE
// Use EXIFTAGTYPE_DOT_RANGE  as the exif_tag_type (EXIF_ASCII)
// Count should be 6
#define _ID_DOT_RANGE 0x0150
#define EXIFTAGID_DOT_RANGE CONSTRUCT_TAGID(DOT_RANGE , _ID_DOT_RANGE )
#define EXIFTAGTYPE_DOT_RANGE EXIF_ASCII

// TARGET_PRINTER
// Use EXIFTAGTYPE_TARGET_PRINTER  as the exif_tag_type (EXIF_ASCII)
// Count should be 6
#define _ID_TARGET_PRINTER 0x0151
#define EXIFTAGID_TARGET_PRINTER \
  CONSTRUCT_TAGID(TARGET_PRINTER , _ID_TARGET_PRINTER)
#define EXIFTAGTYPE_TARGET_PRINTER EXIF_ASCII
// EXTRA_SAMPLES
// Use EXIFTAGTYPE_EXTRA_SAMPLES as the exif_tag_type (EXIF_SHORT)
// Count should be 6
#define _ID_EXTRA_SAMPLES 0x0152
#define EXIFTAGID_EXTRA_SAMPLES \
  CONSTRUCT_TAGID(EXTRA_SAMPLES , _ID_EXTRA_SAMPLES )
#define EXIFTAGTYPE_EXTRA_SAMPLES EXIF_SHORT

// SAMPLE_FORMAT
// Use EXIFTAGTYPE_SAMPLE_FORMAT  as the exif_tag_type (EXIF_SHORT)
// Count should be 6
#define _ID_SAMPLE_FORMAT 0x0153
#define EXIFTAGID_SAMPLE_FORMAT \
  CONSTRUCT_TAGID(SAMPLE_FORMAT , _ID_SAMPLE_FORMAT )
#define EXIFTAGTYPE_SAMPLE_FORMAT EXIF_SHORT

// Table of values that extends the range of the transfer function.
// Use EXIFTAGTYPE_TRANSFER_RANGE as the exif_tag_type (EXIF_SHORT)
// Count should be 6
#define _ID_TRANSFER_RANGE 0x0156
#define EXIFTAGID_TRANSFER_RANGE \
  CONSTRUCT_TAGID(TRANSFER_RANGE , _ID_TRANSFER_RANGE )
#define EXIFTAGTYPE_TRANSFER_RANGE EXIF_SHORT

// JPEG compression process.
// Use EXIFTAGTYPE_JPEG_PROC as the exif_tag_type (EXIF_SHORT)
//
#define _ID_JPEG_PROC 0x0200
#define EXIFTAGID_JPEG_PROC CONSTRUCT_TAGID(JPEG_PROC , _ID_JPEG_PROC )
#define EXIFTAGTYPE_JPEG_PROC EXIF_SHORT


// Offset to JPEG SOI
// Use EXIFTAGTYPE_JPEG_INTERCHANGE_FORMAT as the exif_tag_type (EXIF_LONG)
// Count is undefined
#define _ID_JPEG_INTERCHANGE_FORMAT 0x0201
#define EXIFTAGID_JPEG_INTERCHANGE_FORMAT \
  CONSTRUCT_TAGID(JPEG_INTERCHANGE_FORMAT, _ID_JPEG_INTERCHANGE_FORMAT)
#define EXIFTAGTYPE_JPEG_INTERCHANGE_FORMAT EXIF_LONG
// Bytes of JPEG data
// Use EXIFTAGTYPE_JPEG_INTERCHANGE_FORMAT_LENGTH as the exif_tag_type (EXIF_LONG)
// Count is undefined
#define _ID_JPEG_INTERCHANGE_FORMAT_LENGTH 0x0202
#define EXIFTAGID_JPEG_INTERCHANGE_FORMAT_LENGTH \
  CONSTRUCT_TAGID(JPEG_INTERCHANGE_FORMAT_LENGTH, \
  _ID_JPEG_INTERCHANGE_FORMAT_LENGTH)
#define EXIFTAGTYPE_JPEG_INTERCHANGE_FORMAT_LENGTH EXIF_LONG

// Length of the restart interval.
// Use EXIFTAGTYPE_JPEG_RESTART_INTERVAL as the exif_tag_type (EXIF_SHORT)
// Count is undefined
#define _ID_JPEG_RESTART_INTERVAL 0x0203
#define EXIFTAGID_JPEG_RESTART_INTERVAL \
  CONSTRUCT_TAGID(JPEG_RESTART_INTERVAL, _ID_JPEG_RESTART_INTERVAL)
#define EXIFTAGTYPE_JPEG_RESTART_INTERVAL EXIF_SHORT

// JPEGLosslessPredictors
// Use EXIFTAGTYPE_JPEG_LOSSLESS_PREDICTORS as the exif_tag_type (EXIF_SHORT)
// Count is undefined
#define _ID_JPEG_LOSSLESS_PREDICTORS 0x0205
#define EXIFTAGID_JPEG_LOSSLESS_PREDICTORS  \
  CONSTRUCT_TAGID(JPEG_LOSSLESS_PREDICTORS, _ID_JPEG_LOSSLESS_PREDICTORS)
#define EXIFTAGTYPE_JPEG_LOSSLESS_PREDICTORS EXIF_SHORT

// JPEGPointTransforms
// Use EXIFTAGTYPE_JPEG_POINT_TRANSFORMS as the exif_tag_type (EXIF_SHORT)
// Count is undefined
#define _ID_JPEG_POINT_TRANSFORMS 0x0206
#define EXIFTAGID_JPEG_POINT_TRANSFORMS  \
  CONSTRUCT_TAGID(JPEG_POINT_TRANSFORMS, _ID_JPEG_POINT_TRANSFORMS)
#define EXIFTAGTYPE_JPEG_POINT_TRANSFORMS EXIF_SHORT

// JPEG_Q_TABLES
// Use EXIFTAGTYPE_JPEG_Q_TABLES as the exif_tag_type (EXIF_LONG)
// Count is undefined
#define _ID_JPEG_Q_TABLES 0x0207
#define EXIFTAGID_JPEG_Q_TABLES \
  CONSTRUCT_TAGID(JPEG_Q_TABLES, _ID_JPEG_Q_TABLES)
#define EXIFTAGTYPE_JPEG_Q_TABLES EXIF_LONG
// JPEG_DC_TABLES
// Use EXIFTAGTYPE_JPEG_DC_TABLES as the exif_tag_type (EXIF_LONG)
// Count is undefined
#define _ID_JPEG_DC_TABLES 0x0208
#define EXIFTAGID_JPEG_DC_TABLES \
  CONSTRUCT_TAGID(JPEG_DC_TABLES, _ID_JPEG_DC_TABLES)
#define EXIFTAGTYPE_JPEG_DC_TABLES EXIF_LONG
// JPEG_AC_TABLES
// Use EXIFTAGTYPE_JPEG_AC_TABLES as the exif_tag_type (EXIF_LONG)
// Count is undefined
#define _ID_JPEG_AC_TABLES 0x0209
#define EXIFTAGID_JPEG_AC_TABLES \
  CONSTRUCT_TAGID(JPEG_AC_TABLES, _ID_JPEG_AC_TABLES)
#define EXIFTAGTYPE_JPEG_AC_TABLES EXIF_LONG

// Color space transformation matrix coefficients
// Use EXIFTAGTYPE_YCBCR_COEFFICIENTS as the exif_tag_type (EXIF_RATIONAL)
// Count should be 3
#define _ID_YCBCR_COEFFICIENTS 0x0211
#define EXIFTAGID_YCBCR_COEFFICIENTS \
  CONSTRUCT_TAGID(YCBCR_COEFFICIENTS, _ID_YCBCR_COEFFICIENTS)
#define EXIFTAGTYPE_YCBCR_COEFFICIENTS EXIF_RATIONAL
// Subsampling ratio of Y to C
// Use EXIFTAGTYPE_YCBCR_SUB_SAMPLING as the exif_tag_type (EXIF_SHORT)
// Count should be 2
#define _ID_YCBCR_SUB_SAMPLING 0x0212
#define EXIFTAGID_YCBCR_SUB_SAMPLING  \
  CONSTRUCT_TAGID(YCBCR_SUB_SAMPLING, _ID_YCBCR_SUB_SAMPLING)
#define EXIFTAGTYPE_YCBCR_SUB_SAMPLING EXIF_SHORT
// Y and C positioning
// Use EXIFTAGTYPE_YCBCR_POSITIONING as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_YCBCR_POSITIONING 0x0213
#define EXIFTAGID_YCBCR_POSITIONING  \
  CONSTRUCT_TAGID(YCBCR_POSITIONING, _ID_YCBCR_POSITIONING)
#define EXIFTAGTYPE_YCBCR_POSITIONING EXIF_SHORT
// Pair of black and white reference values
// Use EXIFTAGTYPE_REFERENCE_BLACK_WHITE as the exif_tag_type (EXIF_RATIONAL)
// Count should be 6
#define _ID_REFERENCE_BLACK_WHITE 0x0214
#define EXIFTAGID_REFERENCE_BLACK_WHITE \
  CONSTRUCT_TAGID(REFERENCE_BLACK_WHITE, _ID_REFERENCE_BLACK_WHITE)
#define EXIFTAGTYPE_REFERENCE_BLACK_WHITE EXIF_RATIONAL
// GAMMA
// Use EXIFTAGTYPE_GAMMA as the exif_tag_type (EXIF_RATIONAL)
// Count should be 6
#define _ID_GAMMA 0x0301
#define EXIFTAGID_GAMMA CONSTRUCT_TAGID(GAMMA, _ID_GAMMA)
#define EXIFTAGTYPE_GAMMA EXIF_RATIONAL
// Null-terminated character string that identifies an ICC profile.
// Use EXIFTAGTYPE_ICC_PROFILE_DESCRIPTOR as the exif_tag_type (EXIF_ASCII)
// Count should be 6
#define _ID_ICC_PROFILE_DESCRIPTOR 0x0302
#define EXIFTAGID_ICC_PROFILE_DESCRIPTOR \
  CONSTRUCT_TAGID(ICC_PROFILE_DESCRIPTOR, _ID_ICC_PROFILE_DESCRIPTOR)
#define EXIFTAGTYPE_ICC_PROFILE_DESCRIPTOR EXIF_ASCII
// SRGB_RENDERING_INTENT
// Use EXIFTAGTYPE_SRGB_RENDERING_INTENT as the exif_tag_type (EXIF_BYTE)
// Count should be 6
#define _ID_SRGB_RENDERING_INTENT 0x0303
#define EXIFTAGID_SRGB_RENDERING_INTENT \
  CONSTRUCT_TAGID(SRGB_RENDERING_INTENT, _ID_SRGB_RENDERING_INTENT)
#define EXIFTAGTYPE_SRGB_RENDERING_INTENT EXIF_BYTE

// Null-terminated character string that specifies the title of the image.
// Use EXIFTAGTYPE_IMAGE_TITLE as the exif_tag_type (EXIF_ASCII		)
//
#define _ID_IMAGE_TITLE 0x0320
#define EXIFTAGID_IMAGE_TITLE CONSTRUCT_TAGID(IMAGE_TITLE, _ID_IMAGE_TITLE)
#define EXIFTAGTYPE_IMAGE_TITLE EXIF_ASCII

// Copyright holder
// Use EXIFTAGTYPE_COPYRIGHT as the exif_tag_type (EXIF_ASCII)
// Count can be any
#define _ID_COPYRIGHT 0x8298
#define EXIFTAGID_COPYRIGHT CONSTRUCT_TAGID(COPYRIGHT, _ID_COPYRIGHT)
#define EXIFTAGTYPE_COPYRIGHT EXIF_ASCII
// Old Subfile Type
// Use EXIFTAGTYPE_NEW_SUBFILE_TYPE as the exif_tag_type (EXIF_SHORT)
// Count can be any
#define _ID_NEW_SUBFILE_TYPE 0x00fe
#define EXIFTAGID_NEW_SUBFILE_TYPE \
  CONSTRUCT_TAGID(NEW_SUBFILE_TYPE, _ID_NEW_SUBFILE_TYPE)
#define EXIFTAGTYPE_NEW_SUBFILE_TYPE EXIF_SHORT

// New Subfile Type
// Use EXIFTAGTYPE_NEW_SUBFILE_TYPE as the exif_tag_type (EXIF_LONG)
// Count can be any
#define _ID_SUBFILE_TYPE 0x00ff
#define EXIFTAGID_SUBFILE_TYPE CONSTRUCT_TAGID(SUBFILE_TYPE, _ID_SUBFILE_TYPE)
#define EXIFTAGTYPE_SUBFILE_TYPE EXIF_LONG

// Image width (of thumbnail)
// Use EXIFTAGTYPE_TN_IMAGE_WIDTH as the exif_tag_type (EXIF_LONG)
// Count should be 1
#define _ID_TN_IMAGE_WIDTH 0x0100
#define EXIFTAGID_TN_IMAGE_WIDTH \
  CONSTRUCT_TAGID(TN_IMAGE_WIDTH, _ID_TN_IMAGE_WIDTH)
#define EXIFTAGTYPE_TN_IMAGE_WIDTH EXIF_LONG
// Image height (of thumbnail)
// Use EXIFTAGTYPE_TN_IMAGE_LENGTH as the exif_tag_type (EXIF_SHORT_OR_LONG)
// Count should be 1
#define _ID_TN_IMAGE_LENGTH 0x0101
#define EXIFTAGID_TN_IMAGE_LENGTH \
  CONSTRUCT_TAGID(TN_IMAGE_LENGTH, _ID_TN_IMAGE_LENGTH)
#define EXIFTAGTYPE_TN_IMAGE_LENGTH EXIF_LONG
// Number of bits per component (of thumbnail)
// Use EXIFTAGTYPE_TN_BITS_PER_SAMPLE as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_TN_BITS_PER_SAMPLE 0x0102
#define EXIFTAGID_TN_BITS_PER_SAMPLE \
  CONSTRUCT_TAGID(TN_BITS_PER_SAMPLE, _ID_TN_BITS_PER_SAMPLE)
#define EXIFTAGTYPE_TN_BITS_PER_SAMPLE EXIF_SHORT
// Compression scheme (of thumbnail)
// Use EXIFTAGTYPE_TN_COMPRESSION as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_TN_COMPRESSION 0x0103
#define EXIFTAGID_TN_COMPRESSION \
  CONSTRUCT_TAGID(TN_COMPRESSION, _ID_TN_COMPRESSION)
#define EXIFTAGTYPE_TN_COMPRESSION EXIF_SHORT
// Pixel composition (of thumbnail)
// Use EXIFTAGTYPE_TN_PHOTOMETRIC_INTERPRETATION as the
// exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_TN_PHOTOMETRIC_INTERPRETATION 0x0106
#define EXIFTAGID_TN_PHOTOMETRIC_INTERPRETATION \
  CONSTRUCT_TAGID(TN_PHOTOMETRIC_INTERPRETATION, \
  _ID_TN_PHOTOMETRIC_INTERPRETATION)
#define EXIFTAGTYPE_TN_PHOTOMETRIC_INTERPRETATION EXIF_SHORT
// Image title (of thumbnail)
// Use EXIFTAGTYPE_TN_IMAGE_DESCRIPTION as the exif_tag_type (EXIF_ASCII)
// Count can be any
#define _ID_TN_IMAGE_DESCRIPTION 0x010e
#define EXIFTAGID_TN_IMAGE_DESCRIPTION \
  CONSTRUCT_TAGID(TN_IMAGE_DESCRIPTION, _ID_TN_IMAGE_DESCRIPTION)
#define EXIFTAGTYPE_TN_IMAGE_DESCRIPTION EXIF_ASCII
// Image input equipment manufacturer (of thumbnail)
// Use EXIFTAGTYPE_TN_MAKE as the exif_tag_type (EXIF_ASCII)
// Count can be any
#define _ID_TN_MAKE 0x010f
#define EXIFTAGID_TN_MAKE CONSTRUCT_TAGID(TN_MAKE, _ID_TN_MAKE)
#define EXIFTAGTYPE_TN_MAKE EXIF_ASCII
// Image input equipment model (of thumbnail)
// Use EXIFTAGTYPE_TN_MODEL as the exif_tag_type (EXIF_ASCII)
// Count can be any
#define _ID_TN_MODEL 0x0110
#define EXIFTAGID_TN_MODEL CONSTRUCT_TAGID(TN_MODEL, _ID_TN_MODEL)
#define EXIFTAGTYPE_TN_MODEL EXIF_ASCII
// Image data location (of thumbnail)
// Use EXIFTAGTYPE_TN_STRIP_OFFSETS as the exif_tag_type (EXIF_LONG)
// Count = StripsPerImage                    when PlanarConfiguration = 1
//       = SamplesPerPixel * StripsPerImage  when PlanarConfiguration = 2
#define _ID_TN_STRIP_OFFSETS 0x0111
#define EXIFTAGID_TN_STRIP_OFFSETS \
  CONSTRUCT_TAGID(STRIP_TN_OFFSETS, _ID_TN_STRIP_OFFSETS)
#define EXIFTAGTYPE_TN_STRIP_OFFSETS EXIF_LONG
// Orientation of image (of thumbnail)
// Use EXIFTAGTYPE_TN_ORIENTATION as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_TN_ORIENTATION 0x0112
#define EXIFTAGID_TN_ORIENTATION \
  CONSTRUCT_TAGID(TN_ORIENTATION, _ID_TN_ORIENTATION)
#define EXIFTAGTYPE_TN_ORIENTATION EXIF_SHORT
// Number of components (of thumbnail)
// Use EXIFTAGTYPE_TN_SAMPLES_PER_PIXEL as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_TN_SAMPLES_PER_PIXEL 0x0115
#define EXIFTAGID_TN_SAMPLES_PER_PIXEL \
  CONSTRUCT_TAGID(TN_SAMPLES_PER_PIXEL, _ID_TN_SAMPLES_PER_PIXEL)
#define EXIFTAGTYPE_TN_SAMPLES_PER_PIXEL EXIF_SHORT
// Number of rows per strip (of thumbnail)
// Use EXIFTAGTYPE_TN_ROWS_PER_STRIP as the exif_tag_type (EXIF_LONG)
// Count should be 1
#define _ID_TN_ROWS_PER_STRIP 0x0116
#define EXIFTAGID_TN_ROWS_PER_STRIP \
  CONSTRUCT_TAGID(TN_ROWS_PER_STRIP, _ID_TN_ROWS_PER_STRIP)
#define EXIFTAGTYPE_TN_ROWS_PER_STRIP EXIF_LONG
// Bytes per compressed strip (of thumbnail)
// Use EXIFTAGTYPE_TN_STRIP_BYTE_COUNTS as the exif_tag_type (EXIF_LONG)
// Count = StripsPerImage                    when PlanarConfiguration = 1
//       = SamplesPerPixel * StripsPerImage  when PlanarConfiguration = 2
#define _ID_TN_STRIP_BYTE_COUNTS 0x0117
#define EXIFTAGID_TN_STRIP_BYTE_COUNTS \
  CONSTRUCT_TAGID(TN_STRIP_BYTE_COUNTS, _ID_TN_STRIP_BYTE_COUNTS)
#define EXIFTAGTYPE_TN_STRIP_BYTE_COUNTS EXIF_LONG
// Image resolution in width direction (of thumbnail)
// Use EXIFTAGTYPE_TN_X_RESOLUTION as the exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_TN_X_RESOLUTION 0x011a
#define EXIFTAGID_TN_X_RESOLUTION \
  CONSTRUCT_TAGID(TN_X_RESOLUTION, _ID_TN_X_RESOLUTION)
#define EXIFTAGTYPE_TN_X_RESOLUTION EXIF_RATIONAL
// Image resolution in height direction  (of thumbnail)
// Use EXIFTAGTYPE_TN_Y_RESOLUTION as the exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_TN_Y_RESOLUTION 0x011b
#define EXIFTAGID_TN_Y_RESOLUTION \
  CONSTRUCT_TAGID(TN_Y_RESOLUTION, _ID_TN_Y_RESOLUTION)
#define EXIFTAGTYPE_TN_Y_RESOLUTION EXIF_RATIONAL
// Image data arrangement (of thumbnail)
// Use EXIFTAGTYPE_TN_PLANAR_CONFIGURATION as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_TN_PLANAR_CONFIGURATION 0x011c
#define EXIFTAGID_TN_PLANAR_CONFIGURATION \
  CONSTRUCT_TAGID(TN_PLANAR_CONFIGURATION, _ID_TN_PLANAR_CONFIGURATION)
#define EXIFTAGTYPE_TN_PLANAR_CONFIGURATION EXIF_SHORT
// Unit of X and Y resolution (of thumbnail)
// Use EXIFTAGTYPE_TN_RESOLUTION_UNIT as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_TN_RESOLUTION_UNIT 0x128
#define EXIFTAGID_TN_RESOLUTION_UNIT \
  CONSTRUCT_TAGID(TN_RESOLUTION_UNIT, _ID_TN_RESOLUTION_UNIT)
#define EXIFTAGTYPE_TN_RESOLUTION_UNIT EXIF_SHORT
// Transfer function (of thumbnail)
// Use EXIFTAGTYPE_TN_TRANSFER_FUNCTION as the exif_tag_type (EXIF_SHORT)
// Count should be 3*256
#define _ID_TN_TRANSFER_FUNCTION 0x012d
#define EXIFTAGID_TN_TRANSFER_FUNCTION \
  CONSTRUCT_TAGID(TN_TRANSFER_FUNCTION, _ID_TN_TRANSFER_FUNCTION)
#define EXIFTAGTYPE_TN_TRANSFER_FUNCTION EXIF_SHORT
// Software used (of thumbnail)
// Use EXIFTAGTYPE_TN_SOFTWARE as the exif_tag_type (EXIF_ASCII)
// Count can be any
#define _ID_TN_SOFTWARE 0x0131
#define EXIFTAGID_TN_SOFTWARE CONSTRUCT_TAGID(TN_SOFTWARE, _ID_TN_SOFTWARE)
#define EXIFTAGTYPE_TN_SOFTWARE EXIF_ASCII
// File change date and time (of thumbnail)
// Use EXIFTAGTYPE_TN_DATE_TIME as the exif_tag_type (EXIF_ASCII)
// Count should be 20
#define _ID_TN_DATE_TIME 0x0132
#define EXIFTAGID_TN_DATE_TIME CONSTRUCT_TAGID(TN_DATE_TIME, _ID_TN_DATE_TIME)
#define EXIFTAGTYPE_TN_DATE_TIME EXIF_ASCII
// ARTIST, person who created this image (of thumbnail)
// Use EXIFTAGTYPE_TN_ARTIST as the exif_tag_type (EXIF_ASCII)
// Count can be any
#define _ID_TN_ARTIST 0x013b
#define EXIFTAGID_TN_ARTIST CONSTRUCT_TAGID(TN_ARTIST, _ID_TN_ARTIST)
#define EXIFTAGTYPE_TN_ARTIST EXIF_ASCII
// White point chromaticity (of thumbnail)
// Use EXIFTAGTYPE_TN_WHITE_POINT as the exif_tag_type (EXIF_RATIONAL)
// Count should be 2
#define _ID_TN_WHITE_POINT 0x013e
#define EXIFTAGID_TN_WHITE_POINT \
  CONSTRUCT_TAGID(TN_WHITE_POINT, _ID_TN_WHITE_POINT)
#define EXIFTAGTYPE_TN_WHITE_POINT EXIF_RATIONAL
// Chromaticities of primaries (of thumbnail)
// Use EXIFTAGTYPE_TN_PRIMARY_CHROMATICITIES as the exif_tag_type (EXIF_RATIONAL)
// Count should be 6
#define _ID_TN_PRIMARY_CHROMATICITIES 0x013f
#define EXIFTAGID_TN_PRIMARY_CHROMATICITIES \
  CONSTRUCT_TAGID(TN_PRIMARY_CHROMATICITIES, _ID_TN_PRIMARY_CHROMATICITIES)
#define EXIFTAGTYPE_TN_PRIMARY_CHROMATICITIES EXIF_RATIONAL
// Offset to JPEG SOI (of thumbnail)
// Use EXIFTAGTYPE_TN_JPEGINTERCHANGE_FORMAT as the exif_tag_type (EXIF_LONG)
// Count is undefined
#define _ID_TN_JPEGINTERCHANGE_FORMAT 0x0201
#define EXIFTAGID_TN_JPEGINTERCHANGE_FORMAT \
  CONSTRUCT_TAGID(TN_JPEGINTERCHANGE_FORMAT, _ID_TN_JPEGINTERCHANGE_FORMAT)
#define EXIFTAGTYPE_TN_JPEGINTERCHANGE_FORMAT EXIF_LONG
// Bytes of JPEG data (of thumbnail)
// Use EXIFTAGTYPE_TN_JPEGINTERCHANGE_FORMAT_L as the exif_tag_type (EXIF_LONG)
// Count is undefined
#define _ID_TN_JPEGINTERCHANGE_FORMAT_L 0x0202
#define EXIFTAGID_TN_JPEGINTERCHANGE_FORMAT_L \
  CONSTRUCT_TAGID(TN_JPEGINTERCHANGE_FORMAT_L, _ID_TN_JPEGINTERCHANGE_FORMAT_L)
#define EXIFTAGTYPE_TN_JPEGINTERCHANGE_FORMAT_L EXIF_LONG
// Color space transformation matrix coefficients (of thumbnail)
// Use EXIFTAGTYPE_TN_YCBCR_COEFFICIENTS as the exif_tag_type (EXIF_RATIONAL)
// Count should be 3
#define _ID_TN_YCBCR_COEFFICIENTS 0x0211
#define EXIFTAGID_TN_YCBCR_COEFFICIENTS \
  CONSTRUCT_TAGID(TN_YCBCR_COEFFICIENTS, _ID_TN_YCBCR_COEFFICIENTS)
#define EXIFTAGTYPE_TN_YCBCR_COEFFICIENTS EXIF_RATIONAL
// Subsampling ratio of Y to C (of thumbnail)
// Use EXIFTAGTYPE_TN_YCBCR_SUB_SAMPLING as the exif_tag_type (EXIF_SHORT)
// Count should be 2
#define _ID_TN_YCBCR_SUB_SAMPLING 0x0212
#define EXIFTAGID_TN_YCBCR_SUB_SAMPLING \
  CONSTRUCT_TAGID(TN_YCBCR_SUB_SAMPLING, _ID_TN_YCBCR_SUB_SAMPLING)
#define EXIFTAGTYPE_TN_YCBCR_SUB_SAMPLING EXIF_SHORT
// Y and C positioning (of thumbnail)
// Use EXIFTAGTYPE_TN_YCBCR_POSITIONING as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_TN_YCBCR_POSITIONING 0x0213
#define EXIFTAGID_TN_YCBCR_POSITIONING \
  CONSTRUCT_TAGID(TN_YCBCR_POSITIONING, _ID_TN_YCBCR_POSITIONING)
#define EXIFTAGTYPE_TN_YCBCR_POSITIONING    EXIF_SHORT
// Pair of black and white reference values (of thumbnail)
// Use EXIFTAGTYPE_TN_REFERENCE_BLACK_WHITE as the exif_tag_type (EXIF_RATIONAL)
// Count should be 6
#define _ID_TN_REFERENCE_BLACK_WHITE 0x0214
#define EXIFTAGID_TN_REFERENCE_BLACK_WHITE \
  CONSTRUCT_TAGID(TN_REFERENCE_BLACK_WHITE, _ID_TN_REFERENCE_BLACK_WHITE)
#define EXIFTAGTYPE_TN_REFERENCE_BLACK_WHITE EXIF_RATIONAL
// Copyright holder (of thumbnail)
// Use EXIFTAGTYPE_TN_COPYRIGHT as the exif_tag_type (EXIF_ASCII)
// Count can be any
#define _ID_TN_COPYRIGHT 0x8298
#define EXIFTAGID_TN_COPYRIGHT CONSTRUCT_TAGID(TN_COPYRIGHT, _ID_TN_COPYRIGHT)
#define EXIFTAGTYPE_TN_COPYRIGHT EXIF_ASCII
// Exposure time
// Use EXIFTAGTYPE_EXPOSURE_TIME as the exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_EXPOSURE_TIME 0x829a
#define EXIFTAGID_EXPOSURE_TIME \
  CONSTRUCT_TAGID(EXPOSURE_TIME, _ID_EXPOSURE_TIME)
#define EXIFTAGTYPE_EXPOSURE_TIME EXIF_RATIONAL
// F number
// Use EXIFTAGTYPE_F_NUMBER as the exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_F_NUMBER 0x829d
#define EXIFTAGID_F_NUMBER \
  CONSTRUCT_TAGID(F_NUMBER, _ID_F_NUMBER)
#define EXIFTAGTYPE_F_NUMBER EXIF_RATIONAL
// Exif IFD pointer (NOT INTENDED to be accessible to user)
#define _ID_EXIF_IFD_PTR 0x8769
#define EXIFTAGID_EXIF_IFD_PTR \
  CONSTRUCT_TAGID(EXIF_IFD, _ID_EXIF_IFD_PTR)
#define EXIFTAGTYPE_EXIF_IFD_PTR EXIF_LONG

// ICC_PROFILE (NOT INTENDED to be accessible to user)
#define _ID_ICC_PROFILE 0x8773
#define EXIFTAGID_ICC_PROFILE CONSTRUCT_TAGID(ICC_PROFILE, _ID_ICC_PROFILE)
#define EXIFTAGTYPE_ICC_PROFILE EXIF_LONG
// Exposure program
// Use EXIFTAGTYPE_EXPOSURE_PROGRAM as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_EXPOSURE_PROGRAM 0x8822
#define EXIFTAGID_EXPOSURE_PROGRAM \
  CONSTRUCT_TAGID(EXPOSURE_PROGRAM, _ID_EXPOSURE_PROGRAM)
#define EXIFTAGTYPE_EXPOSURE_PROGRAM EXIF_SHORT
// Spectral sensitivity
// Use EXIFTAGTYPE_SPECTRAL_SENSITIVITY as the exif_tag_type (EXIF_ASCII)
// Count can be any
#define _ID_SPECTRAL_SENSITIVITY 0x8824
#define EXIFTAGID_SPECTRAL_SENSITIVITY \
  CONSTRUCT_TAGID(SPECTRAL_SENSITIVITY, _ID_SPECTRAL_SENSITIVITY)
#define EXIFTAGTYPE_SPECTRAL_SENSITIVITY EXIF_ASCII
// GPS IFD pointer (NOT INTENDED to be accessible to user)
#define _ID_GPS_IFD_PTR 0x8825
#define EXIFTAGID_GPS_IFD_PTR \
  CONSTRUCT_TAGID(GPS_IFD, _ID_GPS_IFD_PTR)
#define EXIFTAGTYPE_GPS_IFD_PTR EXIF_LONG
// ISO Speed Rating
// Use EXIFTAGTYPE_ISO_SPEED_RATING as the exif_tag_type (EXIF_SHORT)
// Count can be any
#define _ID_ISO_SPEED_RATING 0x8827
#define EXIFTAGID_ISO_SPEED_RATING \
  CONSTRUCT_TAGID(ISO_SPEED_RATING, _ID_ISO_SPEED_RATING)
#define EXIFTAGTYPE_ISO_SPEED_RATING EXIF_SHORT
// Optoelectric conversion factor
// Use EXIFTAGTYPE_OECF as the exif_tag_type (EXIF_UNDEFINED)
// Count can be any
#define _ID_OECF 0x8828
#define EXIFTAGID_OECF CONSTRUCT_TAGID(OECF, _ID_OECF)
#define EXIFTAGTYPE_OECF EXIF_UNDEFINED
// Exif version
// Use EXIFTAGTYPE_EXIF_VERSION as the exif_tag_type (EXIF_UNDEFINED)
// Count should be 4
#define _ID_EXIF_VERSION 0x9000
#define EXIFTAGID_EXIF_VERSION \
  CONSTRUCT_TAGID(EXIF_VERSION, _ID_EXIF_VERSION)
#define EXIFTAGTYPE_EXIF_VERSION EXIF_UNDEFINED
// Date and time of original data gerneration
// Use EXIFTAGTYPE_EXIF_DATE_TIME_ORIGINAL as the exif_tag_type (EXIF_ASCII)
// It should be 20 characters long including the null-terminating character.
#define _ID_EXIF_DATE_TIME_ORIGINAL 0x9003
#define EXIFTAGID_EXIF_DATE_TIME_ORIGINAL \
  CONSTRUCT_TAGID(EXIF_DATE_TIME_ORIGINAL, _ID_EXIF_DATE_TIME_ORIGINAL)
#define EXIFTAGTYPE_EXIF_DATE_TIME_ORIGINAL EXIF_ASCII
// Date and time of digital data generation
// Use EXIFTAGTYPE_EXIF_DATE_TIME_DIGITIZED as the exif_tag_type (EXIF_ASCII)
// It should be 20 characters long including the null-terminating character.
#define _ID_EXIF_DATE_TIME_DIGITIZED 0x9004
#define EXIFTAGID_EXIF_DATE_TIME_DIGITIZED \
  CONSTRUCT_TAGID(EXIF_DATE_TIME_DIGITIZED, _ID_EXIF_DATE_TIME_DIGITIZED)
#define EXIFTAGTYPE_EXIF_DATE_TIME_DIGITIZED EXIF_ASCII
// Meaning of each component
// Use EXIFTAGTYPE_EXIF_COMPONENTS_CONFIG as the exif_tag_type (EXIF_UNDEFINED)
// Count should be 4
#define _ID_EXIF_COMPONENTS_CONFIG 0x9101
#define EXIFTAGID_EXIF_COMPONENTS_CONFIG \
  CONSTRUCT_TAGID(EXIF_COMPONENTS_CONFIG, _ID_EXIF_COMPONENTS_CONFIG)
#define EXIFTAGTYPE_EXIF_COMPONENTS_CONFIG EXIF_UNDEFINED
// Meaning of Image compression mode
// Use EXIFTAGTYPE_EXIF_COMPRESSED_BITS_PER_PIXEL as the exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_EXIF_COMPRESSED_BITS_PER_PIXEL 0x9102
#define EXIFTAGID_EXIF_COMPRESSED_BITS_PER_PIXEL \
  CONSTRUCT_TAGID(EXIF_COMPRESSED_BITS_PER_PIXEL, _ID_EXIF_COMPRESSED_BITS_PER_PIXEL)
#define EXIFTAGTYPE_EXIF_COMPRESSED_BITS_PER_PIXEL EXIF_RATIONAL
// Shutter speed
// Use EXIFTAGTYPE_SHUTTER_SPEED as the exif_tag_type (EXIF_SRATIONAL)
// Count should be 1
#define _ID_SHUTTER_SPEED 0x9201
#define EXIFTAGID_SHUTTER_SPEED \
  CONSTRUCT_TAGID(SHUTTER_SPEED, _ID_SHUTTER_SPEED)
#define EXIFTAGTYPE_SHUTTER_SPEED EXIF_SRATIONAL
// Aperture
// Use EXIFTAGTYPE_APERTURE as the exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_APERTURE 0x9202
#define EXIFTAGID_APERTURE CONSTRUCT_TAGID(APERTURE, _ID_APERTURE)
#define EXIFTAGTYPE_APERTURE EXIF_RATIONAL
// Brigthness
// Use EXIFTAGTYPE_BRIGHTNESS as the exif_tag_type (EXIF_SRATIONAL)
// Count should be 1
#define _ID_BRIGHTNESS 0x9203
#define EXIFTAGID_BRIGHTNESS CONSTRUCT_TAGID(BRIGHTNESS, _ID_BRIGHTNESS)
#define EXIFTAGTYPE_BRIGHTNESS EXIF_SRATIONAL
// Exposure bias
// Use EXIFTAGTYPE_EXPOSURE_BIAS_VALUE as the exif_tag_type (EXIF_SRATIONAL)
// Count should be 1
#define _ID_EXPOSURE_BIAS_VALUE 0x9204
#define EXIFTAGID_EXPOSURE_BIAS_VALUE \
  CONSTRUCT_TAGID(EXPOSURE_BIAS_VALUE, _ID_EXPOSURE_BIAS_VALUE)
#define EXIFTAGTYPE_EXPOSURE_BIAS_VALUE EXIF_SRATIONAL
// Maximum lens aperture
// Use EXIFTAGTYPE_MAX_APERTURE as the exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_MAX_APERTURE 0x9205
#define EXIFTAGID_MAX_APERTURE CONSTRUCT_TAGID(MAX_APERTURE, _ID_MAX_APERTURE)
#define EXIFTAGTYPE_MAX_APERTURE EXIF_RATIONAL
// Subject distance
// Use EXIFTAGTYPE_SUBJECT_DISTANCE as the exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_SUBJECT_DISTANCE 0x9206
#define EXIFTAGID_SUBJECT_DISTANCE \
  CONSTRUCT_TAGID(SUBJECT_DISTANCE, _ID_SUBJECT_DISTANCE)
#define EXIFTAGTYPE_SUBJECT_DISTANCE EXIF_RATIONAL
// Metering mode
// Use EXIFTAGTYPE_METERING_MODE as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_METERING_MODE 0x9207
#define EXIFTAGID_METERING_MODE \
  CONSTRUCT_TAGID(METERING_MODE, _ID_METERING_MODE)
#define EXIFTAGTYPE_METERING_MODE EXIF_SHORT
// Light source
// Use EXIFTAGTYPE_LIGHT_SOURCE as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_LIGHT_SOURCE 0x9208
#define EXIFTAGID_LIGHT_SOURCE CONSTRUCT_TAGID(LIGHT_SOURCE, _ID_LIGHT_SOURCE)
#define EXIFTAGTYPE_LIGHT_SOURCE EXIF_SHORT
// Flash
// Use EXIFTAGTYPE_FLASH as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_FLASH 0x9209
#define EXIFTAGID_FLASH CONSTRUCT_TAGID(FLASH, _ID_FLASH)
#define EXIFTAGTYPE_FLASH EXIF_SHORT
// Lens focal length
// Use EXIFTAGTYPE_FOCAL_LENGTH as the exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_FOCAL_LENGTH 0x920a
#define EXIFTAGID_FOCAL_LENGTH CONSTRUCT_TAGID(FOCAL_LENGTH, _ID_FOCAL_LENGTH)
#define EXIFTAGTYPE_FOCAL_LENGTH EXIF_RATIONAL
// Subject area
// Use EXIFTAGTYPE_SUBJECT_AREA as exif_tag_type (EXIF_SHORT)
// Count should be 2 or 3 or 4
#define _ID_SUBJECT_AREA 0x9214
#define EXIFTAGID_SUBJECT_AREA CONSTRUCT_TAGID(SUBJECT_AREA, _ID_SUBJECT_AREA)
#define EXIFTAGTYPE_SUBJECT_AREA EXIF_SHORT
// Maker note
// Use EXIFTAGTYPE_EXIF_MAKER_NOTE as the exif_tag_type (EXIF_UNDEFINED)
// Count can be any
#define _ID_EXIF_MAKER_NOTE 0x927c
#define EXIFTAGID_EXIF_MAKER_NOTE \
  CONSTRUCT_TAGID(EXIF_MAKER_NOTE, _ID_EXIF_MAKER_NOTE)
#define EXIFTAGTYPE_EXIF_MAKER_NOTE EXIF_UNDEFINED
// User comments
// Use EXIFTAGTYPE_EXIF_USER_COMMENT as the exif_tag_type (EXIF_UNDEFINED)
// Count can be any
#define _ID_EXIF_USER_COMMENT 0x9286
#define EXIFTAGID_EXIF_USER_COMMENT \
  CONSTRUCT_TAGID(EXIF_USER_COMMENT, _ID_EXIF_USER_COMMENT)
#define EXIFTAGTYPE_EXIF_USER_COMMENT EXIF_UNDEFINED
// Date time sub-seconds
// Use EXIFTAGTYPE_SUBSEC_TIME as the exif_tag_type (EXIF_ASCII)
// Count could be any
#define _ID_SUBSEC_TIME 0x9290
#define EXIFTAGID_SUBSEC_TIME CONSTRUCT_TAGID(SUBSEC_TIME, _ID_SUBSEC_TIME)
#define EXIFTAGTYPE_SEBSEC_TIME EXIF_ASCII
// Date time original sub-seconds
// use EXIFTAGTYPE_SUBSEC_TIME_ORIGINAL as the exif_tag_type (EXIF_ASCII)
// Count could be any
#define _ID_SUBSEC_TIME_ORIGINAL 0x9291
#define EXIFTAGID_SUBSEC_TIME_ORIGINAL \
  CONSTRUCT_TAGID(SUBSEC_TIME_ORIGINAL, _ID_SUBSEC_TIME_ORIGINAL)
#define EXIFTAGTYPE_SUBSEC_TIME_ORIGINAL EXIF_ASCII
// Date time digitized sub-seconds
// use EXIFTAGTYPE_SUBSEC_TIME_DIGITIZED as the exif_tag_type (EXIF_ASCII)
// Count could be any
#define _ID_SUBSEC_TIME_DIGITIZED 0x9292
#define EXIFTAGID_SUBSEC_TIME_DIGITIZED \
  CONSTRUCT_TAGID(SUBSEC_TIME_DIGITIZED, _ID_SUBSEC_TIME_DIGITIZED)
#define EXIFTAGTYPE_SUBSEC_TIME_DIGITIZED EXIF_ASCII
// Supported Flashpix version
// Use EXIFTAGTYPE_EXIF_FLASHPIX_VERSION as the exif_tag_type (EXIF_UNDEFINED)
// Count should be 4
#define _ID_EXIF_FLASHPIX_VERSION 0xa000
#define EXIFTAGID_EXIF_FLASHPIX_VERSION \
  CONSTRUCT_TAGID(EXIF_FLASHPIX_VERSION, _ID_EXIF_FLASHPIX_VERSION)
#define EXIFTAGTYPE_EXIF_FLASHPIX_VERSION EXIF_UNDEFINED
//  Color space information
// Use EXIFTAGTYPE_EXIF_COLOR_SPACE as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_EXIF_COLOR_SPACE 0xa001
#define EXIFTAGID_EXIF_COLOR_SPACE \
  CONSTRUCT_TAGID(EXIF_COLOR_SPACE, _ID_EXIF_COLOR_SPACE)
#define EXIFTAGTYPE_EXIF_COLOR_SPACE EXIF_SHORT
//  Valid image width
// Use EXIFTAGTYPE_EXIF_PIXEL_X_DIMENSION as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_EXIF_PIXEL_X_DIMENSION 0xa002
#define EXIFTAGID_EXIF_PIXEL_X_DIMENSION \
  CONSTRUCT_TAGID(EXIF_PIXEL_X_DIMENSION, _ID_EXIF_PIXEL_X_DIMENSION)
#define EXIFTAGTYPE_EXIF_PIXEL_X_DIMENSION EXIF_SHORT
// Valid image height
// Use EXIFTAGTYPE_EXIF_PIXEL_Y_DIMENSION as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_EXIF_PIXEL_Y_DIMENSION 0xa003
#define EXIFTAGID_EXIF_PIXEL_Y_DIMENSION \
  CONSTRUCT_TAGID(EXIF_PIXEL_Y_DIMENSION, _ID_EXIF_PIXEL_Y_DIMENSION)
#define EXIFTAGTYPE_EXIF_PIXEL_Y_DIMENSION  EXIF_SHORT
// Related audio file
// Use EXIFTAGTYPE_EXIF_RELATED_SOUND_FILE as the exif_tag_type (EXIF_ASCII)
// Count should be 13
#define _ID_RELATED_SOUND_FILE 0xa004
#define EXIFTAGID_RELATED_SOUND_FILE \
  CONSTRUCT_TAGID(RELATED_SOUND_FILE, _ID_RELATED_SOUND_FILE)
#define EXIFTAGTYPE_RELATED_SOUND_FILE EXIF_ASCII
// Interop IFD pointer (NOT INTENDED to be accessible to user)
#define _ID_INTEROP_IFD_PTR 0xa005
#define EXIFTAGID_INTEROP_IFD_PTR CONSTRUCT_TAGID(INTEROP, _ID_INTEROP_IFD_PTR)
#define EXIFTAGTYPE_INTEROP_IFD_PTR EXIF_LONG
// Flash energy
// Use EXIFTAGTYPE_EXIF_FLASH_ENERGY as the exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_FLASH_ENERGY 0xa20b
#define EXIFTAGID_FLASH_ENERGY CONSTRUCT_TAGID(FLASH_ENERGY, _ID_FLASH_ENERGY)
#define EXIFTAGTYPE_FLASH_ENERGY EXIF_RATIONAL
// Spatial frequency response
// Use EXIFTAGTYPE_SPATIAL_FREQ_RESPONSE as exif_tag_type (EXIF_UNDEFINED)
// Count would be any
#define _ID_SPATIAL_FREQ_RESPONSE 0xa20c
#define EXIFTAGID_SPATIAL_FREQ_RESPONSE \
  CONSTRUCT_TAGID(SPATIAL_FREQ_RESPONSE, _ID_SPATIAL_FREQ_RESPONSE)
#define EXIFTAGTYPE_SPATIAL_FREQ_RESPONSE EXIF_UNDEFINED
// Focal plane x resolution
// Use EXIFTAGTYPE_FOCAL_PLANE_X_RESOLUTION as exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_FOCAL_PLANE_X_RESOLUTION 0xa20e
#define EXIFTAGID_FOCAL_PLANE_X_RESOLUTION \
  CONSTRUCT_TAGID(FOCAL_PLANE_X_RESOLUTION, _ID_FOCAL_PLANE_X_RESOLUTION)
#define EXIFTAGTYPE_FOCAL_PLANE_X_RESOLUTION EXIF_RATIONAL
// Focal plane y resolution
// Use EXIFTAGTYPE_FOCAL_PLANE_Y_RESOLUTION as exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_FOCAL_PLANE_Y_RESOLUTION 0xa20f
#define EXIFTAGID_FOCAL_PLANE_Y_RESOLUTION \
  CONSTRUCT_TAGID(FOCAL_PLANE_Y_RESOLUTION, _ID_FOCAL_PLANE_Y_RESOLUTION)
#define EXIFTAGTYPE_FOCAL_PLANE_Y_RESOLUTION EXIF_RATIONAL
// Focal plane  resolution unit
// Use EXIFTAGTYPE_FOCAL_PLANE_RESOLUTION_UNIT as exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_FOCAL_PLANE_RESOLUTION_UNIT 0xa210
#define EXIFTAGID_FOCAL_PLANE_RESOLUTION_UNIT \
  CONSTRUCT_TAGID(FOCAL_PLANE_RESOLUTION_UNIT, _ID_FOCAL_PLANE_RESOLUTION_UNIT)
#define EXIFTAGTYPE_FOCAL_PLANE_RESOLUTION_UNIT EXIF_SHORT
// Subject location
// Use EXIFTAGTYPE_SUBJECT_LOCATION as the exif_tag_type (EXIF_SHORT)
// Count should be 2
#define _ID_SUBJECT_LOCATION 0xa214
#define EXIFTAGID_SUBJECT_LOCATION \
  CONSTRUCT_TAGID(SUBJECT_LOCATION, _ID_SUBJECT_LOCATION)
#define EXIFTAGTYPE_SUBJECT_LOCATION EXIF_SHORT
// Exposure index
// Use EXIFTAGTYPE_EXPOSURE_INDEX as the exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_EXPOSURE_INDEX 0xa215
#define EXIFTAGID_EXPOSURE_INDEX \
  CONSTRUCT_TAGID(EXPOSURE_INDEX, _ID_EXPOSURE_INDEX)
#define EXIFTAGTYPE_EXPOSURE_INDEX EXIF_RATIONAL
// Sensing method
// Use EXIFTAGTYPE_SENSING_METHOD as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_SENSING_METHOD 0xa217
#define EXIFTAGID_SENSING_METHOD \
  CONSTRUCT_TAGID(SENSING_METHOD, _ID_SENSING_METHOD)
#define EXIFTAGTYPE_SENSING_METHOD EXIF_SHORT
// File source
// Use EXIFTAGTYPE_FILE_SOURCE as the exif_tag_type (EXIF_UNDEFINED)
// Count should be 1
#define _ID_FILE_SOURCE 0xa300
#define EXIFTAGID_FILE_SOURCE CONSTRUCT_TAGID(FILE_SOURCE, _ID_FILE_SOURCE)
#define EXIFTAGTYPE_FILE_SOURCE EXIF_UNDEFINED
// Scene type
// Use EXIFTAGTYPE_SCENE_TYPE as the exif_tag_type (EXIF_UNDEFINED)
// Count should be 1
#define _ID_SCENE_TYPE 0xa301
#define EXIFTAGID_SCENE_TYPE CONSTRUCT_TAGID(SCENE_TYPE, _ID_SCENE_TYPE)
#define EXIFTAGTYPE_SCENE_TYPE EXIF_UNDEFINED
// CFA pattern
// Use EXIFTAGTYPE_CFA_PATTERN as the exif_tag_type (EXIF_UNDEFINED)
// Count can be any
#define _ID_CFA_PATTERN 0xa302
#define EXIFTAGID_CFA_PATTERN CONSTRUCT_TAGID(CFA_PATTERN, _ID_CFA_PATTERN)
#define EXIFTAGTYPE_CFA_PATTERN EXIF_UNDEFINED
// Custom image processing
// Use EXIFTAGTYPE_CUSTOM_RENDERED as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_CUSTOM_RENDERED 0xa401
#define EXIFTAGID_CUSTOM_RENDERED \
  CONSTRUCT_TAGID(CUSTOM_RENDERED, _ID_CUSTOM_RENDERED)
#define EXIFTAGTYPE_CUSTOM_RENDERED EXIF_SHORT
// Exposure mode
// Use EXIFTAGTYPE_EXPOSURE_MODE as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_EXPOSURE_MODE 0xa402
#define EXIFTAGID_EXPOSURE_MODE \
  CONSTRUCT_TAGID(EXPOSURE_MODE, _ID_EXPOSURE_MODE)
#define EXIFTAGTYPE_EXPOSURE_MODE EXIF_SHORT
// White balance
// Use EXIFTAGTYPE_WHITE_BALANCE as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_WHITE_BALANCE 0xa403
#define EXIFTAGID_WHITE_BALANCE \
  CONSTRUCT_TAGID(WHITE_BALANCE, _ID_WHITE_BALANCE)
#define EXIFTAGTYPE_WHITE_BALANCE EXIF_SHORT
// Digital zoom ratio
// Use EXIFTAGTYPE_DIGITAL_ZOOM_RATIO as the exif_tag_type (EXIF_RATIONAL)
// Count should be 1
#define _ID_DIGITAL_ZOOM_RATIO 0xa404
#define EXIFTAGID_DIGITAL_ZOOM_RATIO \
  CONSTRUCT_TAGID(DIGITAL_ZOOM_RATIO, _ID_DIGITAL_ZOOM_RATIO)
#define EXIFTAGTYPE_DIGITAL_ZOOM_RATIO EXIF_RATIONAL
// Focal length in 35mm film
// Use EXIFTAGTYPE_FOCAL_LENGTH_35MM as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_FOCAL_LENGTH_35MM 0xa405
#define EXIFTAGID_FOCAL_LENGTH_35MM CONSTRUCT_TAGID(FOCAL_LENGTH_35MM, _ID_FOCAL_LENGTH_35MM)
#define EXIFTAGTYPE_FOCAL_LENGTH_35MM EXIF_SHORT
// Scene capture type
// Use EXIFTAGTYPE_SCENE_CAPTURE_TYPE as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_SCENE_CAPTURE_TYPE 0xa406
#define EXIFTAGID_SCENE_CAPTURE_TYPE \
  CONSTRUCT_TAGID(SCENE_CAPTURE_TYPE, _ID_SCENE_CAPTURE_TYPE)
#define EXIFTAGTYPE_SCENE_CAPTURE_TYPE EXIF_SHORT
// Gain control
// Use EXIFTAGTYPE_GAIN_CONTROL as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_GAIN_CONTROL 0xa407
#define EXIFTAGID_GAIN_CONTROL CONSTRUCT_TAGID(GAIN_CONTROL, _ID_GAIN_CONTROL)
#define EXIFTAGTYPE_GAIN_CONTROL EXIF_SHORT
// Contrast
// Use EXIFTAGTYPE_CONTRAST as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_CONTRAST 0xa408
#define EXIFTAGID_CONTRAST CONSTRUCT_TAGID(CONTRAST, _ID_CONTRAST)
#define EXIFTAGTYPE_CONTRAST EXIF_SHORT
// Saturation
// Use EXIFTAGTYPE_SATURATION as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_SATURATION  0xa409
#define EXIFTAGID_SATURATION CONSTRUCT_TAGID(SATURATION, _ID_SATURATION)
#define EXIFTAGTYPE_SATURATION EXIF_SHORT
// Sharpness
// Use EXIFTAGTYPE_SHARPNESS as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_SHARPNESS 0xa40a
#define EXIFTAGID_SHARPNESS CONSTRUCT_TAGID(SHARPNESS, _ID_SHARPNESS)
#define EXIFTAGTYPE_SHARPNESS EXIF_SHORT
// Device settings description
// Use EXIFTAGID_DEVICE_SETTINGS_DESCRIPTION as exif_tag_type (EXIF_UNDEFINED)
// Count could be any
#define _ID_DEVICE_SETTINGS_DESCRIPTION 0xa40b
#define EXIFTAGID_DEVICE_SETTINGS_DESCRIPTION \
  CONSTRUCT_TAGID(DEVICE_SETTINGS_DESCRIPTION, _ID_DEVICE_SETTINGS_DESCRIPTION)
#define EXIFTAGTYPE_DEVIC_SETTIGNS_DESCRIPTION EXIF_UNDEFINED
// Subject distance range
// Use EXIFTAGTYPE_SUBJECT_DISTANCE_RANGE as the exif_tag_type (EXIF_SHORT)
// Count should be 1
#define _ID_SUBJECT_DISTANCE_RANGE 0xa40c
#define EXIFTAGID_SUBJECT_DISTANCE_RANGE \
  CONSTRUCT_TAGID(SUBJECT_DISTANCE_RANGE, _ID_SUBJECT_DISTANCE_RANGE)
#define EXIFTAGTYPE_SUBJECT_DISTANCE_RANGE EXIF_SHORT
// Unique image id
// Use EXIFTAG_TYPE_IMAGE_UIDas the exif_tag_type (EXIF_ASCII)
// Count should be 33
#define _ID_IMAGE_UID 0xa420
#define EXIFTAGID_IMAGE_UID CONSTRUCT_TAGID(IMAGE_UID, _ID_IMAGE_UID)
#define EXIFTAGTYPE_IMAGE_UID EXIF_ASCII
// PIM tag
// Use EXIFTAGTYPE_PIM_TAG as the exif_tag_type (EXIF_UNDEFINED)
// Count can be any
#define _ID_PIM 0xc4a5
#define EXIFTAGID_PIM_TAG CONSTRUCT_TAGID(PIM, _ID_PIM)
#define EXIFTAGTYPE_PIM_TAG EXIF_UNDEFINED
#endif // __QEXIF_H__

