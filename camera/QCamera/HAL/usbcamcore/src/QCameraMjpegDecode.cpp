/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
 */

//#define ALOG_NDEBUG 0
#define ALOG_NIDEBUG 0
#define LOG_TAG "QCameraMjpegDecode"
#include <utils/Log.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

extern "C" {
#include "jpeg_buffer.h"
#include "jpeg_common.h"
#include "jpegd.h"
}

#include "QCameraMjpegDecode.h"

/* TBDJ: Can be removed */
#define MIN(a,b)  (((a) < (b)) ? (a) : (b))

// Abstract the return type of the function to be run as a thread
#define OS_THREAD_FUNC_RET_T            void *

// Abstract the argument type to the thread function
#define OS_THREAD_FUNC_ARG_T            void *

// Helpful constants for return values in the thread functions
#define OS_THREAD_FUNC_RET_SUCCEEDED    (OS_THREAD_FUNC_RET_T)0
#define OS_THREAD_FUNC_RET_FAILED       (OS_THREAD_FUNC_RET_T)1

// Abstract the function modifier placed in the beginning of the thread
// declaration (empty for Linux)
#define OS_THREAD_FUNC_MODIFIER

#define os_mutex_init(a) pthread_mutex_init(a, NULL)
#define os_cond_init(a)  pthread_cond_init(a, NULL)
#define os_mutex_lock    pthread_mutex_lock
#define os_mutex_unlock  pthread_mutex_unlock
#define os_cond_wait     pthread_cond_wait
#define os_cond_signal   pthread_cond_signal

const char event_to_string[4][14] =
{
    "EVENT_DONE",
    "EVENT_WARNING",
    "EVENT_ERROR",
};

typedef struct
{
    uint32_t   width;
    uint32_t   height;
    uint32_t   format;
    uint32_t   preference;
    uint32_t   abort_time;
    uint16_t   back_to_back_count;
    /* TBDJ: Is this required */
    int32_t    rotation;
    /* TBDJ: Is this required */
    jpeg_rectangle_t region;
    /* TBDJ: Is this required */
    jpegd_scale_type_t scale_factor;
    uint32_t   hw_rotation;

    char*       inputMjpegBuffer;
    int         inputMjpegBufferSize;
    char*       outputYptr;
    char*       outputUVptr;

} test_args_t;

typedef struct
{
    int                   tid;
    pthread_t             thread;
    jpegd_obj_t           decoder;
    uint8_t               decoding;
    uint8_t               decode_success;
    pthread_mutex_t       mutex;
    pthread_cond_t        cond;
    test_args_t           *p_args;
    jpegd_output_buf_t    *p_whole_output_buf;

} thread_ctrl_blk_t;

OS_THREAD_FUNC_RET_T OS_THREAD_FUNC_MODIFIER decoder_test(OS_THREAD_FUNC_ARG_T p_thread_args);
void decoder_event_handler(void        *p_user_data,
                           jpeg_event_t event,
                           void        *p_arg);
int decoder_output_handler(void               *p_user_data,
                           jpegd_output_buf_t *p_output_buffer,
                           uint32_t            first_row_id,
                           uint8_t             is_last_buffer);
uint32_t decoder_input_req_handler(void           *p_user_data,
                                   jpeg_buffer_t   buffer,
                                   uint32_t        start_offset,
                                   uint32_t        length);
static void* insertHuffmanTable(void *p, int size);

static int mjpegd_timer_start(timespec *p_timer);
static int mjpegd_timer_get_elapsed(timespec *p_timer, int *elapsed_in_ms, uint8_t reset_start);
static int mjpegd_cond_timedwait(pthread_cond_t *p_cond, pthread_mutex_t *p_mutex, uint32_t ms);

// Global variables
/* TBDJ: can be removed */
thread_ctrl_blk_t *thread_ctrl_blks = NULL;

/*
 * This function initializes the mjpeg decoder and returns the object
 */
MJPEGD_ERR mjpegDecoderInit(void** mjpegd_obj)
{
    test_args_t* mjpegd;

    ALOGD("%s: E", __func__);

    mjpegd = (test_args_t *)malloc(sizeof(test_args_t));
    if(!mjpegd)
        return MJPEGD_INSUFFICIENT_MEM;

    memset(mjpegd, 0, sizeof(test_args_t));

    /* Defaults */
    /* Due to current limitation, s/w decoder is selected always */
    mjpegd->preference          = JPEG_DECODER_PREF_HW_ACCELERATED_PREFERRED;
    mjpegd->back_to_back_count  = 1;
    mjpegd->rotation            = 0;
    mjpegd->hw_rotation         = 0;
    mjpegd->scale_factor        = (jpegd_scale_type_t)1;

    /* TBDJ: can be removed */
    mjpegd->width                 = 640;
    mjpegd->height                = 480;
    mjpegd->abort_time            = 0;

    *mjpegd_obj = (void *)mjpegd;

    ALOGD("%s: X", __func__);
    return  MJPEGD_NO_ERROR;
}

MJPEGD_ERR mjpegDecode(
            void*   mjpegd_obj,
            char*   inputMjpegBuffer,
            int     inputMjpegBufferSize,
            char*   outputYptr,
            char*   outputUVptr,
            int     outputFormat)
{
    int rc, c, i;
    test_args_t* mjpegd;
    test_args_t  test_args;

    ALOGD("%s: E", __func__);
    /* store input arguments in the context */
    mjpegd = (test_args_t*) mjpegd_obj;
    mjpegd->inputMjpegBuffer        = inputMjpegBuffer;
    mjpegd->inputMjpegBufferSize    = inputMjpegBufferSize;
    mjpegd->outputYptr              = outputYptr;
    mjpegd->outputUVptr             = outputUVptr;
    mjpegd->format                  = outputFormat;

    /* TBDJ: can be removed */
    memcpy(&test_args, mjpegd, sizeof(test_args_t));

    // check the formats
    if (((test_args.format == YCRCBLP_H1V2) || (test_args.format == YCBCRLP_H1V2) ||
      (test_args.format == YCRCBLP_H1V1) || (test_args.format == YCBCRLP_H1V1)) &&
      !(test_args.preference == JPEG_DECODER_PREF_HW_ACCELERATED_ONLY)) {
        ALOGE("%s:These formats are not supported by SW format %d", __func__, test_args.format);
        return 1;
    }

    // Create thread control blocks
    thread_ctrl_blks = (thread_ctrl_blk_t *)malloc( sizeof(thread_ctrl_blk_t));
    if (!thread_ctrl_blks)
    {
        ALOGE("%s: decoder_test failed: insufficient memory in creating thread control blocks", __func__);
        return 1;
    }
    memset(thread_ctrl_blks, 0, sizeof(thread_ctrl_blk_t));
    // Initialize the blocks and kick off the threads
        thread_ctrl_blks[i].tid = i;
        thread_ctrl_blks[i].p_args = &test_args;
        os_mutex_init(&thread_ctrl_blks[i].mutex);
        os_cond_init(&thread_ctrl_blks[i].cond);

    rc = (int)decoder_test(&thread_ctrl_blks[i]);

    if (!rc)
        ALOGD("%s: decoder_test finished successfully ", __func__);
    else
        ALOGE("%s: decoder_test failed",__func__);

    ALOGD("%s: X rc: %d", __func__, rc);

    return rc;
}

OS_THREAD_FUNC_RET_T OS_THREAD_FUNC_MODIFIER decoder_test(OS_THREAD_FUNC_ARG_T arg)
{
    int rc, i;
    jpegd_obj_t         decoder;
    jpegd_src_t         source;
    jpegd_dst_t         dest;
    jpegd_cfg_t         config;
    jpeg_hdr_t          header;
    jpegd_output_buf_t  p_output_buffers;
    uint32_t            output_buffers_count = 1; // currently only 1 buffer a time is supported
    uint8_t use_pmem = true;
    timespec os_timer;
    thread_ctrl_blk_t *p_thread_arg = (thread_ctrl_blk_t *)arg;
    test_args_t *p_args = p_thread_arg->p_args;
    uint32_t            output_width;
    uint32_t            output_height;
    uint32_t total_time = 0;

    ALOGD("%s: E", __func__);

    // Determine whether pmem should be used (useful for pc environment testing where
    // pmem is not available)
    if ((jpegd_preference_t)p_args->preference == JPEG_DECODER_PREF_SOFTWARE_PREFERRED ||
        (jpegd_preference_t)p_args->preference == JPEG_DECODER_PREF_SOFTWARE_ONLY) {
        use_pmem = false;
    }

    if (((jpegd_preference_t)p_args->preference !=
      JPEG_DECODER_PREF_HW_ACCELERATED_ONLY) &&
      ((jpegd_preference_t)p_args->scale_factor > 0)) {
        ALOGI("%s: Setting scale factor to 1x", __func__);
    }

    ALOGD("%s: before jpegd_init p_thread_arg: %p", __func__, p_thread_arg);

    // Initialize decoder
    rc = jpegd_init(&decoder,
                    &decoder_event_handler,
                    &decoder_output_handler,
                    p_thread_arg);

    if (JPEG_FAILED(rc)) {
        ALOGE("%s: decoder_test: jpegd_init failed", __func__);
        goto fail;
    }
    p_thread_arg->decoder = decoder;

    // Set source information
    source.p_input_req_handler = &decoder_input_req_handler;
    source.total_length        = p_args->inputMjpegBufferSize & 0xffffffff;

    rc = jpeg_buffer_init(&source.buffers[0]);
    if (JPEG_SUCCEEDED(rc)) {
        /* TBDJ: why buffer [1] */
        rc = jpeg_buffer_init(&source.buffers[1]);
    }
    if (JPEG_SUCCEEDED(rc)) {
#if 1
        rc = jpeg_buffer_allocate(source.buffers[0], 0xA000, use_pmem);
#else
        rc = jpeg_buffer_use_external_buffer(source.buffers[0],
                                             (uint8_t *)p_args->inputMjpegBuffer,
                                             p_args->inputMjpegBufferSize,
                                             0);
#endif
        ALOGD("%s: source.buffers[0]:%p compressed buffer ptr = %p", __func__,
              source.buffers[0], p_args->inputMjpegBuffer);
    }
    if (JPEG_SUCCEEDED(rc)) {
#if 1
        rc = jpeg_buffer_allocate(source.buffers[1], 0xA000, use_pmem);
#else
         rc = jpeg_buffer_use_external_buffer(source.buffers[1],
                                             (uint8_t *)p_args->inputMjpegBuffer,
                                             p_args->inputMjpegBufferSize,
                                             0);
#endif
        ALOGD("%s: source.buffers[1]:%p compressed buffer ptr  = %p", __func__,
              source.buffers[1], p_args->inputMjpegBuffer);
   }
    if (JPEG_FAILED(rc)) {
        jpeg_buffer_destroy(&source.buffers[0]);
        jpeg_buffer_destroy(&source.buffers[1]);
        goto fail;
    }

   ALOGI("%s: *** Starting back-to-back decoding of %d frame(s)***\n",
                 __func__, p_args->back_to_back_count);

	 // Loop to perform n back-to-back decoding (to the same output file)
    for(i = 0; i < p_args->back_to_back_count; i++) {
        if(mjpegd_timer_start(&os_timer) < 0) {
            ALOGE("%s: failed to get start time", __func__);
        }

        /* TBDJ: Every frame? */
        ALOGD("%s: before jpegd_set_source source.p_arg:%p", __func__, source.p_arg);
        rc = jpegd_set_source(decoder, &source);
        if (JPEG_FAILED(rc))
        {
            ALOGE("%s: jpegd_set_source failed", __func__);
            goto fail;
        }

        rc = jpegd_read_header(decoder, &header);
        if (JPEG_FAILED(rc))
        {
            ALOGE("%s: jpegd_read_header failed", __func__);
            goto fail;
        }
        p_args->width = header.main.width;
        p_args->height = header.main.height;
        ALOGD("%s: main dimension: (%dx%d) subsampling: (%d)", __func__,
                header.main.width, header.main.height, (int)header.main.subsampling);

        // main image decoding:
        // Set destination information
        dest.width = (p_args->width) ? (p_args->width) : header.main.width;
        dest.height = (p_args->height) ? (p_args->height) : header.main.height;
        dest.output_format = (jpeg_color_format_t) p_args->format;
        dest.region = p_args->region;

        // if region is defined, re-assign the output width/height
        output_width  = dest.width;
        output_height = dest.height;

        if (p_args->region.right || p_args->region.bottom)
        {
            if (0 == p_args->rotation || 180 == p_args->rotation)
            {
                output_width  = MIN((dest.width),
                        (uint32_t)(dest.region.right  - dest.region.left + 1));
                output_height = MIN((dest.height),
                        (uint32_t)(dest.region.bottom - dest.region.top  + 1));
            }
            // Swap output width/height for 90/270 rotation cases
            else if (90 == p_args->rotation || 270 == p_args->rotation)
            {
                output_height  = MIN((dest.height),
                        (uint32_t)(dest.region.right  - dest.region.left + 1));
                output_width   = MIN((dest.width),
                        (uint32_t)(dest.region.bottom - dest.region.top  + 1));
            }
            // Unsupported rotation cases
            else
            {
                goto fail;
            }
        }

        if (dest.output_format == YCRCBLP_H2V2 || dest.output_format == YCBCRLP_H2V2 ||
            dest.output_format == YCRCBLP_H2V1 || dest.output_format == YCBCRLP_H2V1 ||
            dest.output_format == YCRCBLP_H1V2 || dest.output_format == YCBCRLP_H1V2 ||
            dest.output_format == YCRCBLP_H1V1 || dest.output_format == YCBCRLP_H1V1) {
            jpeg_buffer_init(&p_output_buffers.data.yuv.luma_buf);
            jpeg_buffer_init(&p_output_buffers.data.yuv.chroma_buf);
        } else {
            jpeg_buffer_init(&p_output_buffers.data.rgb.rgb_buf);

        }

        {
            // Assign 0 to tile width and height
            // to indicate that no tiling is requested.
            p_output_buffers.tile_width  = 0;
            p_output_buffers.tile_height = 0;
        }
        p_output_buffers.is_in_q = 0;

        switch (dest.output_format)
        {
        case YCRCBLP_H2V2:
        case YCBCRLP_H2V2:
//        case YCRCBLP_H2V1:
//        case YCBCRLP_H2V1:
//        case YCRCBLP_H1V2:
//        case YCBCRLP_H1V2:
//        case YCRCBLP_H1V1:
//        case YCBCRLP_H1V1:
            jpeg_buffer_use_external_buffer(
               p_output_buffers.data.yuv.luma_buf,
               (uint8_t*)p_args->outputYptr,
               p_args->width * p_args->height * SQUARE(p_args->scale_factor),
               0);
            jpeg_buffer_use_external_buffer(
                p_output_buffers.data.yuv.chroma_buf,
                (uint8_t*)p_args->outputUVptr,
                p_args->width * p_args->height / 2 * SQUARE(p_args->scale_factor),
                0);
            break;

        default:
            ALOGE("%s: decoder_test: unsupported output format", __func__);
            goto fail;
        }

        // Set up configuration
        memset(&config, 0, sizeof(jpegd_cfg_t));
        config.preference = (jpegd_preference_t) p_args->preference;
        config.decode_from = JPEGD_DECODE_FROM_AUTO;
        config.rotation = p_args->rotation;
        config.scale_factor = p_args->scale_factor;
        config.hw_rotation = p_args->hw_rotation;
        dest.back_to_back_count = p_args->back_to_back_count;

        // Start decoding
        p_thread_arg->decoding = true;

        rc = jpegd_start(decoder, &config, &dest, &p_output_buffers, output_buffers_count);
        dest.back_to_back_count--;

        if(JPEG_FAILED(rc)) {
            ALOGE("%s: decoder_test: jpegd_start failed (rc=%d)\n",
                    __func__, rc);
            goto fail;
        }

        ALOGD("%s: decoder_test: jpegd_start succeeded", __func__);

        // Do abort
        if (p_args->abort_time) {
            os_mutex_lock(&p_thread_arg->mutex);
            while (p_thread_arg->decoding)
            {
                rc = mjpegd_cond_timedwait(&p_thread_arg->cond, &p_thread_arg->mutex, p_args->abort_time);
                if (rc == JPEGERR_ETIMEDOUT)
                {
                    // Do abort
                    os_mutex_unlock(&p_thread_arg->mutex);
                    rc = jpegd_abort(decoder);
                    if (rc)
                    {
                        ALOGE("%s: decoder_test: jpegd_abort failed: %d", __func__, rc);
                        goto fail;
                    }
                    break;
                }
            }
            if (p_thread_arg->decoding)
                os_mutex_unlock(&p_thread_arg->mutex);
        } else {
            // Wait until decoding is done or stopped due to error
            os_mutex_lock(&p_thread_arg->mutex);
            while (p_thread_arg->decoding)
            {
                os_cond_wait(&p_thread_arg->cond, &p_thread_arg->mutex);
            }
            os_mutex_unlock(&p_thread_arg->mutex);
        }

        int diff;
        // Display the time elapsed
        if (mjpegd_timer_get_elapsed(&os_timer, &diff, 0) < 0) {
            ALOGE("%s: decoder_test: failed to get elapsed time", __func__);
        } else {
            if(p_args->abort_time) {
                if(p_thread_arg->decoding) {
                    ALOGI("%s: decoder_test: decoding aborted successfully after %d ms", __func__, diff);
                    goto buffer_clean_up;
                }
                else
                {
                    ALOGI("%s: decoder_test: decoding stopped before abort is issued. "
                                    "decode time: %d ms", __func__, diff);
                }
            }
            else {
                if(p_thread_arg->decode_success) {
                    total_time += diff;
                    ALOGI("%s: decode time: %d ms (%d frame(s), total=%dms, avg=%dms/frame)",
                            __func__, diff, i+1, total_time, total_time/(i+1));
                }
                else
                {
                    fprintf(stderr, "decoder_test: decode failed\n");
                }
            }
        }
    }

    if(p_thread_arg->decode_success) {
        ALOGD("%s: Frame(s) = %d, Total Time = %dms, Avg. decode time = %dms/frame)\n",
                 __func__, p_args->back_to_back_count, total_time, total_time/p_args->back_to_back_count);
    }

buffer_clean_up:
    // Clean up decoder and allocate buffers
    jpeg_buffer_destroy(&source.buffers[0]);
    jpeg_buffer_destroy(&source.buffers[1]);
    switch (dest.output_format)
    {
    case YCRCBLP_H2V2:
    case YCBCRLP_H2V2:
    case YCRCBLP_H2V1:
    case YCBCRLP_H2V1:
    case YCRCBLP_H1V2:
    case YCBCRLP_H1V2:
    case YCRCBLP_H1V1:
    case YCBCRLP_H1V1:
        jpeg_buffer_destroy(&p_output_buffers.data.yuv.luma_buf);
        jpeg_buffer_destroy(&p_output_buffers.data.yuv.chroma_buf);
        break;
    default:
        break;
    }
    jpegd_destroy(&decoder);

    if (!p_thread_arg->decode_success)
    {
        goto fail;
    }

    ALOGD("%s: X", __func__);
    return OS_THREAD_FUNC_RET_SUCCEEDED;
fail:

    ALOGD("%s: X", __func__);
    return OS_THREAD_FUNC_RET_FAILED;
}

void decoder_event_handler(void        *p_user_data,
                           jpeg_event_t event,
                           void        *p_arg)
{
    thread_ctrl_blk_t *p_thread_arg = (thread_ctrl_blk_t *)p_user_data;

    ALOGD("%s: E", __func__);

    ALOGD("%s: Event: %s\n", __func__, event_to_string[event]);
    if (event == JPEG_EVENT_DONE)
    {
        p_thread_arg->decode_success = true;
        ALOGD("%s: decode_success: %d\n", __func__, p_thread_arg->decode_success);
    }
    // If it is not a warning event, decoder has stopped; Signal
    // main thread to clean up
    if (event != JPEG_EVENT_WARNING)
    {
        os_mutex_lock(&p_thread_arg->mutex);
        p_thread_arg->decoding = false;
        os_cond_signal(&p_thread_arg->cond);
        os_mutex_unlock(&p_thread_arg->mutex);
    }
    ALOGD("%s: X", __func__);

}

// consumes the output buffer.
/*TBDJ: Can be removed. Is this related to tiling */
int decoder_output_handler(void *p_user_data,
                           jpegd_output_buf_t *p_output_buffer,
                           uint32_t first_row_id,
                           uint8_t is_last_buffer)
{
    uint8_t* whole_output_buf_ptr, *tiling_buf_ptr;

    ALOGD("%s: E", __func__);

    thread_ctrl_blk_t *p_thread_arg = (thread_ctrl_blk_t *)p_user_data;

    jpeg_buffer_get_addr(p_thread_arg->p_whole_output_buf->data.rgb.rgb_buf, &whole_output_buf_ptr);
    jpeg_buffer_get_addr(p_output_buffer->data.rgb.rgb_buf, &tiling_buf_ptr);

    if (p_output_buffer->tile_height != 1)
        return JPEGERR_EUNSUPPORTED;

    // testing purpose only
    // This is to simulate that the user needs to bail out when error happens
    // in the middle of decoding
    //if (first_row_id == 162)
     //   return JPEGERR_EFAILED;

    // do not enqueue any buffer if it reaches the last buffer
    if (!is_last_buffer)
    {
        jpegd_enqueue_output_buf(p_thread_arg->decoder, p_output_buffer, 1);
    }
    ALOGD("%s: X", __func__);

    return JPEGERR_SUCCESS;
}

//      p_reader->p_input_req_handler(p_reader->decoder,
//                                    p_reader->p_input_buf,
//                                    p_reader->next_byte_offset,
//                                    MAX_BYTES_TO_FETCH);

uint32_t decoder_input_req_handler(void           *p_user_data,
                                   jpeg_buffer_t   buffer,
                                   uint32_t        start_offset,
                                   uint32_t        length)
{
    uint32_t buf_size;
    uint8_t *buf_ptr;
    int bytes_to_read, bytes_read, rc;
    thread_ctrl_blk_t *p_thread_arg = (thread_ctrl_blk_t *)p_user_data;
    thread_ctrl_blk_t *thread_ctrl_blk = (thread_ctrl_blk_t *)p_user_data;
    test_args_t*    mjpegd = (test_args_t*) thread_ctrl_blk->p_args;

    ALOGD("%s: E", __func__);

    jpeg_buffer_get_max_size(buffer, &buf_size);
    jpeg_buffer_get_addr(buffer, &buf_ptr);
    bytes_to_read = (length < buf_size) ? length : buf_size;
    bytes_read = 0;

    ALOGD("%s: buf_ptr = %p, start_offset = %d, length = %d buf_size = %d bytes_to_read = %d", __func__, buf_ptr, start_offset, length, buf_size, bytes_to_read);
    if (bytes_to_read)
    {
        /* TBDJ: Should avoid this Mem copy */
#if 1
        memcpy(buf_ptr, (char *)mjpegd->inputMjpegBuffer + start_offset, bytes_to_read);
#else
        if(JPEGERR_SUCCESS != jpeg_buffer_set_start_offset(buffer, start_offset))
            ALOGE("%s: jpeg_buffer_set_start_offset failed", __func__);
#endif
        bytes_read = bytes_to_read;
    }

    ALOGD("%s: X", __func__);
    return bytes_read;
}

static int mjpegd_timer_start(timespec *p_timer)
{
    if (!p_timer)
        return JPEGERR_ENULLPTR;

    if (clock_gettime(CLOCK_REALTIME, p_timer))
        return JPEGERR_EFAILED;

    return JPEGERR_SUCCESS;
}

static int mjpegd_timer_get_elapsed(timespec *p_timer, int *elapsed_in_ms, uint8_t reset_start)
{
    timespec now;
    long diff;
    int rc = mjpegd_timer_start(&now);

    if (JPEG_FAILED(rc))
        return rc;

    diff = (long)(now.tv_sec - p_timer->tv_sec) * 1000;
    diff += (long)(now.tv_nsec - p_timer->tv_nsec) / 1000000;
    *elapsed_in_ms = (int)diff;

    if (reset_start)
        *p_timer = now;

    return JPEGERR_SUCCESS;
}

int mjpegd_cond_timedwait(pthread_cond_t *p_cond, pthread_mutex_t *p_mutex, uint32_t ms)
{
    struct timespec ts;
    int rc = clock_gettime(CLOCK_REALTIME, &ts);
    if (rc < 0) return rc;

    if (ms >= 1000) {
       ts.tv_sec += (ms/1000);
       ts.tv_nsec += ((ms%1000) * 1000000);
    } else {
        ts.tv_nsec += (ms * 1000000);
    }

    rc = pthread_cond_timedwait(p_cond, p_mutex, &ts);
    if (rc == ETIMEDOUT)
    {
        rc = JPEGERR_ETIMEDOUT;
    }
    return rc;
}

