/*
Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.

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
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <pthread.h>
#include "mm_camera_dbg.h"
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include "mm_qcamera_unit_test.h"

#define MM_QCAMERA_APP_UTEST_MAX_MAIN_LOOP 4
#define MM_QCAM_APP_TEST_NUM 128

static mm_app_tc_t mm_app_tc[MM_QCAM_APP_TEST_NUM];
static int num_test_cases = 0;
static mm_jpeg_ops_t jpeg_ops;

extern int system_dimension_set(int cam_id);
extern int stopPreview(int cam_id);
extern int takePicture_yuv(int cam_id);
extern int startRdi(int cam_id);
extern int stopRdi(int cam_id);
extern int startStats(int cam_id);
extern int stopStats(int cam_id);
extern int takePicture_zsl(int cam_id);
extern int takePicture_rdi(int cam_id);
extern int mm_app_tc_reprocess_preview_only(mm_camera_app_t *cam_apps);
extern int takePicture_raw(int cam_id);

int mm_app_tc_0(mm_camera_app_t *cam_apps)
{
    int rc = MM_CAMERA_OK;
    int i,j;
    int result = 0;

    printf("\n Verifying Preview on front and back camera...\n");
    if (cam_apps->num_cameras == 0) {
        CDBG_ERROR("%s:Query Failed: Num of cameras = %d\n",__func__, cam_apps->num_cameras);
        rc = -1;
        goto end;
    }
    for (i = 0; i < cam_apps->num_cameras; i++) {
        if ( mm_app_open(i) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if (system_dimension_set(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        for (j = 0; j < MM_QCAMERA_APP_INTERATION; j++) {
            if ( MM_CAMERA_OK != (rc = startPreview(my_cam_app.cam_open))) {
                CDBG_ERROR("%s: startPreview() err=%d\n", __func__, rc);
                break;
            }
            /*if(mm_camera_app_timedwait() == ETIMEDOUT) {
                    CDBG_ERROR("%s: Preview Callback not received in time or qbuf failed\n", __func__);
                    break;
            }*/
            mm_camera_app_wait();
            if ( MM_CAMERA_OK != (rc = stopPreview(my_cam_app.cam_open))) {
                CDBG("%s: startPreview() err=%d\n", __func__, rc);
                break;
            }
            result++;
        }
        if ( mm_app_close(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }

        if (result != MM_QCAMERA_APP_INTERATION) {
            printf("%s: Preview Start/Stop Fails for Camera %d in %d iteration", __func__, i,j);
            rc = -1;
            break;
        }

        result = 0;
    }
    end:
    if (rc == 0) {
        printf("\nPassed\n");
    } else {
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_tc_1(mm_camera_app_t *cam_apps)
{
    int rc = MM_CAMERA_OK;
    int i,j;
    int result = 0;

    printf("\n Verifying Snapshot on front and back camera...\n");
    for (i = 0; i < cam_apps->num_cameras; i++) {
        if ( mm_app_open(i) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if (system_dimension_set(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }

        if ( MM_CAMERA_OK != (rc = startPreview(my_cam_app.cam_open))) {
            CDBG_ERROR("%s: startPreview() err=%d\n", __func__, rc);
            break;
        }
        for (j = 0; j < MM_QCAMERA_APP_INTERATION; j++) {
            if ( MM_CAMERA_OK != (rc = takePicture_yuv(my_cam_app.cam_open))) {
                CDBG_ERROR("%s: TakePicture() err=%d\n", __func__, rc);
                break;
            }
            /*if(mm_camera_app_timedwait() == ETIMEDOUT) {
                    CDBG_ERROR("%s: Snapshot/Preview Callback not received in time or qbuf Faile\n", __func__);
                    break;
            }*/
            mm_camera_app_wait();
            result++;
        }
        if ( MM_CAMERA_OK != (rc = stopPreview(my_cam_app.cam_open))) {
            CDBG("%s: startPreview() err=%d\n", __func__, rc);
            break;
        }
        if ( mm_app_close(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if (result != MM_QCAMERA_APP_INTERATION) {
            printf("%s: Snapshot Start/Stop Fails for Camera %d in %d iteration", __func__, i,j);
            rc = -1;
            break;
        }

        result = 0;
    }
    end:
    if (rc == 0) {
        printf("\t***Passed***\n");
    } else {
        printf("\t***Failed***\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_tc_2(mm_camera_app_t *cam_apps)
{
    int rc = MM_CAMERA_OK;
    int i,j;
    int result = 0;

    printf("\n Verifying Video on front and back camera...\n");
    for (i = 0; i < cam_apps->num_cameras; i++) {
        if ( mm_app_open(i) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if (system_dimension_set(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }

        if ( MM_CAMERA_OK != (rc = startPreview(my_cam_app.cam_open))) {
            CDBG_ERROR("%s: startPreview() err=%d\n", __func__, rc);
            break;
        }
        for (j = 0; j < MM_QCAMERA_APP_INTERATION; j++) {
            if ( MM_CAMERA_OK != (rc = startRecording(my_cam_app.cam_open))) {
                CDBG_ERROR("%s: StartVideorecording() err=%d\n", __func__, rc);
                break;
            }

            /*if(mm_camera_app_timedwait() == ETIMEDOUT) {
                    CDBG_ERROR("%s: Video Callback not received in time\n", __func__);
                    break;
            }*/
            mm_camera_app_wait();
            usleep(10*1000);
            if ( MM_CAMERA_OK != (rc = stopRecording(my_cam_app.cam_open))) {
                CDBG_ERROR("%s: Stopvideorecording() err=%d\n", __func__, rc);
                break;
            }
            result++;
        }
        if ( MM_CAMERA_OK != (rc = stopPreview(my_cam_app.cam_open))) {
            CDBG("%s: startPreview() err=%d\n", __func__, rc);
            break;
        }
        if ( mm_app_close(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if (result != MM_QCAMERA_APP_INTERATION) {
            printf("%s: Video Start/Stop Fails for Camera %d in %d iteration", __func__, i,j);
            rc = -1;
            break;
        }

        result = 0;
    }
    end:
    if (rc == 0) {
        printf("\nPassed\n");
    } else {
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_tc_3(mm_camera_app_t *cam_apps)
{
    int rc = MM_CAMERA_OK;
    int i,j;
    int result = 0;

    printf("\n Verifying RDI Stream on front and back camera...\n");
    if (cam_apps->num_cameras == 0) {
        CDBG_ERROR("%s:Query Failed: Num of cameras = %d\n",__func__, cam_apps->num_cameras);
        rc = -1;
        goto end;
    }
    CDBG_ERROR("SINGLE open RDI ");
    for (i = 1; i < cam_apps->num_cameras; i++) {
        if ( mm_app_open(i) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if (system_dimension_set(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        CDBG_ERROR("SINGLE start RDI case");
        for (j = 0; j < MM_QCAMERA_APP_INTERATION; j++) {
            if ( MM_CAMERA_OK != (rc = startRdi(my_cam_app.cam_open))) {
                CDBG_ERROR("%s: StartVideorecording() err=%d\n", __func__, rc);
                break;
            }

            /*if(mm_camera_app_timedwait() == ETIMEDOUT) {
                    CDBG_ERROR("%s: Video Callback not received in time\n", __func__);
                    break;
            }*/
            mm_camera_app_wait();
            usleep(10*1000);
            CDBG_ERROR("SINGLE stop RDI");
            if ( MM_CAMERA_OK != (rc = stopRdi(my_cam_app.cam_open))) {
                CDBG_ERROR("%s: Stopvideorecording() err=%d\n", __func__, rc);
                break;
            }
            result++;
        }
        CDBG_ERROR("SINGLE close RDI");
        if ( mm_app_close(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if (result != MM_QCAMERA_APP_INTERATION) {
            printf("%s: Video Start/Stop Fails for Camera %d in %d iteration", __func__, i,j);
            rc = -1;
            break;
        }

        result = 0;
    }
    end:
    if (rc == 0) {
        printf("\nPassed\n");
    } else {
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_tc_4(mm_camera_app_t *cam_apps)
{
    int rc = MM_CAMERA_OK;
    int i,j;
    int result = 0;

    printf("\n Verifying ZSL Snapshot on front and back camera...\n");
    for (i = 0; i < cam_apps->num_cameras; i++) {
        if ( mm_app_open(i) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if (system_dimension_set(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }

        if ( MM_CAMERA_OK != (rc = startPreview(my_cam_app.cam_open))) {
            CDBG_ERROR("%s: startPreview() err=%d\n", __func__, rc);
            break;
        }
        for (j = 0; j < MM_QCAMERA_APP_INTERATION; j++) {
            if ( MM_CAMERA_OK != (rc = takePicture_zsl(my_cam_app.cam_open))) {
                CDBG_ERROR("%s: TakePicture() ZSL err=%d\n", __func__, rc);
                break;
            }
            /*if(mm_camera_app_timedwait() == ETIMEDOUT) {
                    CDBG_ERROR("%s: Snapshot/Preview Callback not received in time or qbuf Faile\n", __func__);
                    break;
            }*/
            mm_camera_app_wait();
            result++;
        }
        if ( MM_CAMERA_OK != (rc = stopPreview(my_cam_app.cam_open))) {
            CDBG("%s: startPreview() err=%d\n", __func__, rc);
            break;
        }
        if ( mm_app_close(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if (result != MM_QCAMERA_APP_INTERATION) {
            printf("%s: Snapshot ZSL Start/Stop Fails for Camera %d in %d iteration", __func__, i,j);
            rc = -1;
            break;
        }

        result = 0;
    }
    end:
    if (rc == 0) {
        printf("\t***Passed***\n");
    } else {
        printf("\t***Failed***\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}



int mm_app_tc_5(mm_camera_app_t *cam_apps)
{
    int rc = MM_CAMERA_OK;
    int i;
    printf("Running %s - open/close ,video0, open/close snapshot channel only\n", __func__);
#if 0
    for (i = 0; i < MM_QCAMERA_APP_UTEST_MAX_MAIN_LOOP; i++) {
        if ( 0 != (rc = mm_app_open(cam_id, MM_CAMERA_OP_MODE_NOTUSED))) {
            CDBG("%s: open cam %d at opmode = %d err, loop=%d, rc=%d\n",
                 __func__, cam_id, MM_CAMERA_OP_MODE_NOTUSED, i, rc);
            goto end;
        }
        if (0 != (rc = mm_app_open_snapshot(cam_id))) {
            goto end;
        }
        if (0 != (rc = mm_app_close_snapshot(cam_id))) {
            goto end;
        }
        if ( 0 != (rc = mm_app_close(cam_id))) {
            CDBG("%s: close cam %d at opmode = %d err,loop=%d, rc=%d\n",
                 __func__, cam_id, MM_CAMERA_OP_MODE_NOTUSED, i, rc);
            goto end;
        }
    }
    end:
#endif
    CDBG("%s:END, rc=%d\n", __func__, rc);
    return rc;
}

int mm_app_tc_6(mm_camera_app_t *cam_apps)
{
    int rc = MM_CAMERA_OK;
    int i;
    printf("Running %s - simple preview \n", __func__);
#if 0
    if ( 0 != (rc = mm_app_open(cam_id, MM_CAMERA_OP_MODE_NOTUSED))) {
        CDBG("%s: open cam %d at opmode = %d err, loop=%d, rc=%d\n",
             __func__, cam_id, MM_CAMERA_OP_MODE_NOTUSED, i, rc);
        goto end;
    }

    for (i = 0; i < MM_QCAMERA_APP_UTEST_MAX_MAIN_LOOP; i++) {
        if (0 != (rc = mm_app_init_preview(cam_id))) {
            goto end;
        }
        if (0 != (rc = mm_app_start_preview(cam_id))) {
            goto end;
        }
        /* sleep 8 seconds */
        usleep(8000000);
        if (0 != (rc = mm_app_stop_preview(cam_id))) {
            goto end;
        }
        if (0 != (rc=mm_app_deinit_preview(cam_id))) {
            goto end;
        }
        if (0 != (rc = mm_app_close_preview(cam_id))) {
            goto end;
        }
    }
    if ( 0 != (rc = mm_app_close(cam_id))) {
        CDBG("%s: close cam %d at opmode = %d err,loop=%d, rc=%d\n",
             __func__, cam_id, MM_CAMERA_OP_MODE_NOTUSED, i, rc);
        goto end;
    }
end:
#endif
    CDBG("%s:END, rc=%d\n", __func__, rc);
    return rc;
}

int mm_app_tc_7(mm_camera_app_t *cam_apps)
{
    int rc = MM_CAMERA_OK;
    int i;
    printf("Running %s - simple preview and recording \n", __func__);
#if 0
    if ( 0 != (rc = mm_app_open(cam_id, MM_CAMERA_OP_MODE_NOTUSED))) {
        CDBG("%s: open cam %d at opmode = %d err, loop=%d, rc=%d\n",
             __func__, cam_id, MM_CAMERA_OP_MODE_NOTUSED, i, rc);
        goto end;
    }

    for (i = 0; i < MM_QCAMERA_APP_UTEST_MAX_MAIN_LOOP; i++) {
        if (0 != (rc = mm_app_init_preview(cam_id))) {
            goto end;
        }
        if (0 != (rc = mm_app_start_preview(cam_id))) {
            goto end;
        }
        /* sleep 8 seconds */
        usleep(8000000);
        if (0 != (rc = mm_app_start_recording(cam_id))) {
            goto end;
        }
        usleep(1000000);
        if (0 != (rc = mm_app_stop_recording(cam_id))) {
            goto end;
        }
        usleep(8000000);
        if (0 != (rc = mm_app_stop_preview(cam_id))) {
            goto end;
        }
        if (0 != (rc=mm_app_deinit_preview(cam_id))) {
            goto end;
        }
        if (0 != (rc = mm_app_close_preview(cam_id))) {
            goto end;
        }
    }
    if ( 0 != (rc = mm_app_close(cam_id))) {
        CDBG("%s: close cam %d at opmode = %d err,loop=%d, rc=%d\n",
             __func__, cam_id, MM_CAMERA_OP_MODE_NOTUSED, i, rc);
        goto end;
    }
end:
#endif
    CDBG("%s:END, rc=%d\n", __func__, rc);
    return rc;
}

int mm_app_tc_8(mm_camera_app_t *cam_apps)
{
    int rc = MM_CAMERA_OK;
    int i;
    printf("Running %s - preview, recording, and snapshot, then preview again \n", __func__);
#if 0
    if ( 0 != (rc = mm_app_open(cam_id, MM_CAMERA_OP_MODE_NOTUSED))) {
        CDBG("%s: open cam %d at opmode = %d err, loop=%d, rc=%d\n",
             __func__, cam_id, MM_CAMERA_OP_MODE_NOTUSED, i, rc);
        goto end;
    }

    for (i = 0; i < MM_QCAMERA_APP_UTEST_MAX_MAIN_LOOP; i++) {
        if (0 != (rc = mm_app_init_preview(cam_id))) {
            goto end;
        }
        if (0 != (rc = mm_app_start_preview(cam_id))) {
            goto end;
        }
        /* sleep 8 seconds */
        usleep(8000000);
        if (0 != (rc = mm_app_start_recording(cam_id))) {
            goto end;
        }
        usleep(1000000);
        if (0 != (rc = mm_app_stop_recording(cam_id))) {
            goto end;
        }
        if (0 != (rc = mm_app_stop_preview(cam_id))) {
            goto end;
        }
        if (0!=(rc=mm_app_init_snapshot(cam_id))) {
            goto end;
        }
        if (0 != (rc=mm_app_take_picture(cam_id))) {
            goto end;
        }
        if ( 0 != (rc = mm_app_deinit_snahspot(cam_id))) {
            goto end;
        }
        if (0 != (rc = mm_app_start_preview(cam_id))) {
            goto end;
        }
        usleep(8000000);
        if (0 != (rc=mm_app_deinit_preview(cam_id))) {
            goto end;
        }
        if (0 != (rc = mm_app_close_preview(cam_id))) {
            goto end;
        }
    }
    if ( 0 != (rc = mm_app_close(cam_id))) {
        CDBG("%s: close cam %d at opmode = %d err,loop=%d, rc=%d\n",
             __func__, cam_id, MM_CAMERA_OP_MODE_NOTUSED, i, rc);
        goto end;
    }
end:
#endif
    CDBG("%s:END, rc=%d\n", __func__, rc);
    return rc;
}

int mm_app_tc_9(mm_camera_app_t *cam_apps) /*RDI snapshot*/
{
    int rc = MM_CAMERA_OK;
    int i,j,k;
    int result = 0;

    printf("\n Verifying RDI Stream on front and back camera...\n");
    if (cam_apps->num_cameras == 0) {
        CDBG_ERROR("%s:Query Failed: Num of cameras = %d\n",__func__, cam_apps->num_cameras);
        rc = -1;
        goto end;
    }
    CDBG_ERROR("SINGLE open RDI snapshot test case");
    for (i = 1; i < cam_apps->num_cameras; i++) {
        if ( mm_app_open(i) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if (system_dimension_set(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        CDBG_ERROR("SINGLE start RDI case");
        for (j = 0; j < MM_QCAMERA_APP_INTERATION; j++) {
            if ( MM_CAMERA_OK != (rc = startRdi(my_cam_app.cam_open))) {
                CDBG_ERROR("%s: StartVideorecording() err=%d\n", __func__, rc);
                break;
            }
            usleep(10*1000);
            CDBG_ERROR("SINGLE start RDI snapshot");
            for (k = 0; k < MM_QCAMERA_APP_INTERATION; k++) {
              if ( MM_CAMERA_OK != (rc = takePicture_rdi(my_cam_app.cam_open))) {
                CDBG_ERROR("%s: TakePicture() err=%d\n", __func__, rc);
                break;
              }
              usleep(10*1000);
              mm_camera_app_wait();
              result++;
            }
            CDBG_ERROR("SINGLE stop RDI");
            if ( MM_CAMERA_OK != (rc = stopRdi(my_cam_app.cam_open))) {
                CDBG_ERROR("%s: Stopvideorecording() err=%d\n", __func__, rc);
                break;
            }
        }
        CDBG_ERROR("SINGLE close RDI");
        if ( mm_app_close(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if (result != (MM_QCAMERA_APP_INTERATION * MM_QCAMERA_APP_INTERATION)) {
            printf("%s: Video Start/Stop Fails for Camera %d in %d iteration", __func__, i,j);
            rc = -1;
            break;
        }

        result = 0;
    }
    end:
    if (rc == 0) {
        printf("\nPassed\n");
    } else {
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_tc_10(mm_camera_app_t *cam_apps)
{
    int rc = MM_CAMERA_OK;
    int i,j;
    int result = 0;

    printf("\n Verifying RAW Snapshot on front and back camera...\n");
    for (i = 0; i < cam_apps->num_cameras; i++) {
        if ( mm_app_open(i) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if (system_dimension_set(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }

        if ( MM_CAMERA_OK != (rc = startPreview(my_cam_app.cam_open))) {
            CDBG_ERROR("%s: startPreview() err=%d\n", __func__, rc);
            break;
        }
        for (j = 0; j < MM_QCAMERA_APP_INTERATION; j++) {
            if ( MM_CAMERA_OK != (rc = takePicture_raw(my_cam_app.cam_open))) {
                CDBG_ERROR("%s: TakePicture() err=%d\n", __func__, rc);
                break;
            }
            /*if(mm_camera_app_timedwait() == ETIMEDOUT) {
                    CDBG_ERROR("%s: Snapshot/Preview Callback not received in time or qbuf Faile\n", __func__);
                    break;
            }*/
            mm_camera_app_wait();
            result++;
        }
        if ( MM_CAMERA_OK != (rc = stopPreview(my_cam_app.cam_open))) {
            CDBG("%s: startPreview() err=%d\n", __func__, rc);
            break;
        }
        if ( mm_app_close(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if (result != MM_QCAMERA_APP_INTERATION) {
            printf("%s: Snapshot Start/Stop Fails for Camera %d in %d iteration", __func__, i,j);
            rc = -1;
            break;
        }

        result = 0;
    }
    end:
    if (rc == 0) {
        printf("\t***Passed***\n");
    } else {
        printf("\t***Failed***\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_gen_test_cases()
{
    int tc = 0;
    memset(mm_app_tc, 0, sizeof(mm_app_tc));
    if (tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_0;
    if (tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_1;
    if (tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_2;
    //if (tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_3; /*Enable only when RDI enabled for front camera*/
    //if(tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_4;
    /*if(tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_5;
      if(tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_6;
      if(tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_7;
      if(tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_8;*/
    //if(tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_9;  /*Enable only when rdi enabled for front camera*/
    if (tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_10;

    /* test case for reprocess with preview only */
    /*
    if (tc < MM_QCAM_APP_TEST_NUM) {
        mm_app_tc[tc++].f = mm_app_tc_reprocess_preview_only;
    }
    */
    /* test case for reprocess with preview and recording */
    /*
    if (tc < MM_QCAM_APP_TEST_NUM) {
        mm_app_tc[tc++].f = mm_app_tc_reprocess_preview_and_recording;
    }
    */

    return tc;
}

int mm_app_unit_test_entry(mm_camera_app_t *cam_app)
{
    int rc = MM_CAMERA_OK;
    int i, tc = 0;
    int cam_id = 0;

    tc = mm_app_gen_test_cases();
    CDBG("Running %d test cases\n",tc);
    for (i = 0; i < tc; i++) {
        mm_app_tc[i].r = mm_app_tc[i].f(cam_app);
        if (mm_app_tc[i].r != MM_CAMERA_OK) {
            printf("%s: test case %d error = %d, abort unit testing engine!!!!\n",
                   __func__, i, mm_app_tc[i].r);
            rc = mm_app_tc[i].r;
            goto end;
        }
    }
    end:
    printf("nTOTAL_TSET_CASE = %d, NUM_TEST_RAN = %d, rc=%d\n", tc, i, rc);
    return rc;
}




