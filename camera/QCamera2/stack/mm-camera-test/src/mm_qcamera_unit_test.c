/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include "mm_qcamera_dbg.h"
#include "mm_qcamera_app.h"

#define MM_QCAMERA_APP_UTEST_MAX_MAIN_LOOP 1
#define MM_QCAMERA_APP_UTEST_OUTER_LOOP 1
#define MM_QCAMERA_APP_UTEST_INNER_LOOP 1
#define MM_QCAM_APP_TEST_NUM 128

static mm_app_tc_t mm_app_tc[MM_QCAM_APP_TEST_NUM];

int mm_app_tc_open_close(mm_camera_app_t *cam_app)
{
    int rc = MM_CAMERA_OK;
    int i;
    mm_camera_test_obj_t test_obj;

    printf("\n Verifying open/close cameras...\n");
    for (i = 0; i < cam_app->num_cameras; i++) {
        memset(&test_obj, 0, sizeof(mm_camera_test_obj_t));
        rc = mm_app_open(cam_app, i, &test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            break;
        }
        sleep(1);
        rc = mm_app_close(&test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            break;
        }
    }
    if (rc == MM_CAMERA_OK) {
        printf("\nPassed\n");
    } else {
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_tc_start_stop_preview(mm_camera_app_t *cam_app)
{
    int rc = MM_CAMERA_OK;
    int i, j;
    mm_camera_test_obj_t test_obj;

    printf("\n Verifying start/stop preview...\n");
    for (i = 0; i < cam_app->num_cameras; i++) {
        memset(&test_obj, 0, sizeof(mm_camera_test_obj_t));
        rc = mm_app_open(cam_app, i, &test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            break;
        }

        for (j = 0; j < MM_QCAMERA_APP_UTEST_INNER_LOOP; j++) {
            rc = mm_app_start_preview(&test_obj);
            if (rc != MM_CAMERA_OK) {
                CDBG_ERROR("%s: mm_app_start_preview() cam_idx=%d, err=%d\n",
                           __func__, i, rc);
                break;
            }
            sleep(1);
            rc = mm_app_stop_preview(&test_obj);
            if (rc != MM_CAMERA_OK) {
                CDBG_ERROR("%s: mm_app_stop_preview() cam_idx=%d, err=%d\n",
                           __func__, i, rc);
                break;
            }
        }

        rc |= mm_app_close(&test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            break;
        }
    }
    if (rc == MM_CAMERA_OK) {
        printf("\nPassed\n");
    } else {
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_tc_start_stop_zsl(mm_camera_app_t *cam_app)
{
    int rc = MM_CAMERA_OK;
    int i, j;
    mm_camera_test_obj_t test_obj;

    printf("\n Verifying start/stop preview...\n");
    for (i = 0; i < cam_app->num_cameras; i++) {
        memset(&test_obj, 0, sizeof(mm_camera_test_obj_t));
        rc = mm_app_open(cam_app, i, &test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            break;
        }

        for (j = 0; j < 1; j++) {
            rc = mm_app_start_preview_zsl(&test_obj);
            if (rc != MM_CAMERA_OK) {
                CDBG_ERROR("%s: mm_app_start_preview_zsl() cam_idx=%d, err=%d\n",
                           __func__, i, rc);
                break;
            }
            sleep(1);
            rc = mm_app_stop_preview_zsl(&test_obj);
            if (rc != MM_CAMERA_OK) {
                CDBG_ERROR("%s: mm_app_stop_preview_zsl() cam_idx=%d, err=%d\n",
                           __func__, i, rc);
                break;
            }
        }

        rc = mm_app_close(&test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            break;
        }
    }
    if (rc == MM_CAMERA_OK) {
        printf("\nPassed\n");
    } else {
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_tc_start_stop_video_preview(mm_camera_app_t *cam_app)
{
    int rc = MM_CAMERA_OK;
    int i, j;
    mm_camera_test_obj_t test_obj;

    printf("\n Verifying start/stop video preview...\n");
    for (i = 0; i < cam_app->num_cameras; i++) {
        memset(&test_obj, 0, sizeof(mm_camera_test_obj_t));
        rc = mm_app_open(cam_app, i, &test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            break;
        }

        for (j = 0; j < MM_QCAMERA_APP_UTEST_INNER_LOOP; j++) {
            rc = mm_app_start_record_preview(&test_obj);
            if (rc != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_start_record_preview() cam_idx=%d, err=%d\n",
                           __func__, i, rc);
                break;
            }
            sleep(1);
            rc = mm_app_stop_record_preview(&test_obj);
            if (rc != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_stop_record_preview() cam_idx=%d, err=%d\n",
                           __func__, i, rc);
                break;
            }
        }

        rc = mm_app_close(&test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            break;
        }
    }
    if (rc == MM_CAMERA_OK) {
        printf("\nPassed\n");
    } else {
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_tc_start_stop_video_record(mm_camera_app_t *cam_app)
{
    int rc = MM_CAMERA_OK;
    int i, j;
    mm_camera_test_obj_t test_obj;

    printf("\n Verifying start/stop recording...\n");
    for (i = 0; i < cam_app->num_cameras; i++) {
        memset(&test_obj, 0, sizeof(mm_camera_test_obj_t));
        rc = mm_app_open(cam_app, i, &test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            break;
        }

        rc = mm_app_start_record_preview(&test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_start_record_preview() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            mm_app_close(&test_obj);
            break;
        }

        sleep(1);

        for (j = 0; j < MM_QCAMERA_APP_UTEST_INNER_LOOP; j++) {
            rc = mm_app_start_record(&test_obj);
            if (rc != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_start_record() cam_idx=%d, err=%d\n",
                           __func__, i, rc);
                break;
            }

            sleep(1);

            rc = mm_app_stop_record(&test_obj);
            if (rc != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_stop_record() cam_idx=%d, err=%d\n",
                           __func__, i, rc);
                break;
            }
        }
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:start/stop record cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            mm_app_stop_record_preview(&test_obj);
            mm_app_close(&test_obj);
            break;
        }

        rc = mm_app_stop_record_preview(&test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_stop_record_preview() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            mm_app_close(&test_obj);
            break;
        }

        rc = mm_app_close(&test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            break;
        }
    }
    if (rc == MM_CAMERA_OK) {
        printf("\nPassed\n");
    } else {
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_tc_start_stop_live_snapshot(mm_camera_app_t *cam_app)
{
    int rc = MM_CAMERA_OK;
    int i, j;
    mm_camera_test_obj_t test_obj;

    printf("\n Verifying start/stop live snapshot...\n");
    for (i = 0; i < cam_app->num_cameras; i++) {
        memset(&test_obj, 0, sizeof(mm_camera_test_obj_t));
        rc = mm_app_open(cam_app, i, &test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            break;
        }

        rc = mm_app_start_record_preview(&test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_start_record_preview() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            mm_app_close(&test_obj);
            break;
        }

        sleep(1);

        rc = mm_app_start_record(&test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_start_record() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            mm_app_stop_record_preview(&test_obj);
            mm_app_close(&test_obj);
            break;
        }

        sleep(1);

        for (j = 0; j < MM_QCAMERA_APP_UTEST_INNER_LOOP; j++) {
            rc = mm_app_start_live_snapshot(&test_obj);
            if (rc != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_start_live_snapshot() cam_idx=%d, err=%d\n",
                           __func__, i, rc);
                break;
            }

            /* wait for jpeg is done */
            mm_camera_app_wait();

            rc = mm_app_stop_live_snapshot(&test_obj);
            if (rc != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_stop_live_snapshot() cam_idx=%d, err=%d\n",
                           __func__, i, rc);
                break;
            }
        }
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:start/stop live snapshot cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            mm_app_stop_record(&test_obj);
            mm_app_stop_record_preview(&test_obj);
            mm_app_close(&test_obj);
            break;
        }

        rc = mm_app_stop_record(&test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_stop_record() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            mm_app_stop_record_preview(&test_obj);
            mm_app_close(&test_obj);
            break;
        }

        sleep(1);

        rc = mm_app_stop_record_preview(&test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_stop_record_preview() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            mm_app_close(&test_obj);
            break;
        }

        rc = mm_app_close(&test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            break;
        }
    }
    if (rc == MM_CAMERA_OK) {
        printf("\nPassed\n");
    } else {
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_tc_capture_raw(mm_camera_app_t *cam_app)
{
    int rc = MM_CAMERA_OK;
    int i, j;
    mm_camera_test_obj_t test_obj;
    uint8_t num_snapshot = 1;
    uint8_t num_rcvd_snapshot = 0;

    printf("\n Verifying raw capture...\n");
    for (i = 0; i < cam_app->num_cameras; i++) {
        memset(&test_obj, 0, sizeof(mm_camera_test_obj_t));
        rc = mm_app_open(cam_app, i, &test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            break;
        }

        for (j = 0; j < MM_QCAMERA_APP_UTEST_INNER_LOOP; j++) {
            rc = mm_app_start_capture_raw(&test_obj, num_snapshot);
            if (rc != MM_CAMERA_OK) {
                CDBG_ERROR("%s: mm_app_start_capture() cam_idx=%d, err=%d\n",
                           __func__, i, rc);
                break;
            }
            while (num_rcvd_snapshot < num_snapshot) {
                mm_camera_app_wait();
                num_rcvd_snapshot++;
            }
            rc = mm_app_stop_capture_raw(&test_obj);
            if (rc != MM_CAMERA_OK) {
                CDBG_ERROR("%s: mm_app_stop_capture() cam_idx=%d, err=%d\n",
                           __func__, i, rc);
                break;
            }
        }

        rc |= mm_app_close(&test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            break;
        }
    }
    if (rc == MM_CAMERA_OK) {
        printf("\nPassed\n");
    } else {
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_tc_capture_regular(mm_camera_app_t *cam_app)
{
    int rc = MM_CAMERA_OK;
    int i, j;
    mm_camera_test_obj_t test_obj;
    uint8_t num_snapshot = 1;
    uint8_t num_rcvd_snapshot = 0;

    printf("\n Verifying capture...\n");
    for (i = 0; i < cam_app->num_cameras; i++) {
        memset(&test_obj, 0, sizeof(mm_camera_test_obj_t));
        rc = mm_app_open(cam_app, i, &test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            break;
        }

        for (j = 0; j < MM_QCAMERA_APP_UTEST_INNER_LOOP; j++) {
            rc = mm_app_start_capture(&test_obj, num_snapshot);
            if (rc != MM_CAMERA_OK) {
                CDBG_ERROR("%s: mm_app_start_capture() cam_idx=%d, err=%d\n",
                           __func__, i, rc);
                break;
            }
            while (num_rcvd_snapshot < num_snapshot) {
                mm_camera_app_wait();
                num_rcvd_snapshot++;
            }
            rc = mm_app_stop_capture(&test_obj);
            if (rc != MM_CAMERA_OK) {
                CDBG_ERROR("%s: mm_app_stop_capture() cam_idx=%d, err=%d\n",
                           __func__, i, rc);
                break;
            }
        }

        rc = mm_app_close(&test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            break;
        }
    }
    if (rc == MM_CAMERA_OK) {
        printf("\nPassed\n");
    } else {
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_tc_capture_burst(mm_camera_app_t *cam_app)
{
    int rc = MM_CAMERA_OK;
    int i, j;
    mm_camera_test_obj_t test_obj;
    uint8_t num_snapshot = 3;
    uint8_t num_rcvd_snapshot = 0;

    printf("\n Verifying capture...\n");
    for (i = 0; i < cam_app->num_cameras; i++) {
        memset(&test_obj, 0, sizeof(mm_camera_test_obj_t));
        rc = mm_app_open(cam_app, i, &test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            break;
        }

        for (j = 0; j < MM_QCAMERA_APP_UTEST_INNER_LOOP; j++) {
            rc = mm_app_start_capture(&test_obj, num_snapshot);
            if (rc != MM_CAMERA_OK) {
                CDBG_ERROR("%s: mm_app_start_capture() cam_idx=%d, err=%d\n",
                           __func__, i, rc);
                break;
            }
            while (num_rcvd_snapshot < num_snapshot) {
                mm_camera_app_wait();
                num_rcvd_snapshot++;
            }
            rc = mm_app_stop_capture(&test_obj);
            if (rc != MM_CAMERA_OK) {
                CDBG_ERROR("%s: mm_app_stop_capture() cam_idx=%d, err=%d\n",
                           __func__, i, rc);
                break;
            }
        }

        rc = mm_app_close(&test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            break;
        }
    }
    if (rc == MM_CAMERA_OK) {
        printf("\nPassed\n");
    } else {
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_tc_rdi_burst(mm_camera_app_t *cam_app)
{
    int rc = MM_CAMERA_OK, rc2 = MM_CAMERA_OK;
    int i, j;
    mm_camera_test_obj_t test_obj;

    printf("\n Verifying rdi burst (3) capture...\n");
    for (i = 0; i < cam_app->num_cameras; i++) {
        memset(&test_obj, 0, sizeof(mm_camera_test_obj_t));
        rc = mm_app_open(cam_app, i, &test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            break;
        }

        for (j = 0; j < MM_QCAMERA_APP_UTEST_INNER_LOOP; j++) {
            rc = mm_app_start_rdi(&test_obj, 3);
            if (rc != MM_CAMERA_OK) {
                CDBG_ERROR("%s: mm_app_start_preview() cam_idx=%d, err=%d\n",
                           __func__, i, rc);
                break;
            }
            sleep(1);
            rc = mm_app_stop_rdi(&test_obj);
            if (rc != MM_CAMERA_OK) {
                CDBG_ERROR("%s: mm_app_stop_preview() cam_idx=%d, err=%d\n",
                           __func__, i, rc);
                break;
            }
        }

        rc2 = mm_app_close(&test_obj);
        if (rc2 != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() cam_idx=%d, err=%d\n",
                       __func__, i, rc2);
            if (rc == MM_CAMERA_OK) {
                rc = rc2;
            }
            break;
        }
    }
    if (rc == MM_CAMERA_OK) {
        printf("\nPassed\n");
    } else {
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_tc_rdi_cont(mm_camera_app_t *cam_app)
{
    int rc = MM_CAMERA_OK, rc2 = MM_CAMERA_OK;
    int i, j;
    mm_camera_test_obj_t test_obj;

    printf("\n Verifying rdi continuous capture...\n");
    for (i = 0; i < cam_app->num_cameras; i++) {
        memset(&test_obj, 0, sizeof(mm_camera_test_obj_t));
        rc = mm_app_open(cam_app, i, &test_obj);
        if (rc != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() cam_idx=%d, err=%d\n",
                       __func__, i, rc);
            break;
        }

        for (j = 0; j < MM_QCAMERA_APP_UTEST_INNER_LOOP; j++) {
            rc = mm_app_start_rdi(&test_obj, 0);
            if (rc != MM_CAMERA_OK) {
                CDBG_ERROR("%s: mm_app_start_preview() cam_idx=%d, err=%d\n",
                           __func__, i, rc);
                break;
            }
            sleep(1);
            rc = mm_app_stop_rdi(&test_obj);
            if (rc != MM_CAMERA_OK) {
                CDBG_ERROR("%s: mm_app_stop_preview() cam_idx=%d, err=%d\n",
                           __func__, i, rc);
                break;
            }
        }

        rc2 = mm_app_close(&test_obj);
        if (rc2 != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() cam_idx=%d, err=%d\n",
                       __func__, i, rc2);
            if (rc == MM_CAMERA_OK) {
                rc = rc2;
            }
            break;
        }
    }
    if (rc == MM_CAMERA_OK) {
        printf("\nPassed\n");
    } else {
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_gen_test_cases()
{
    int tc = 0;
    memset(mm_app_tc, 0, sizeof(mm_app_tc));
    if (tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_open_close;
    if (tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_start_stop_preview;
    //if (tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_start_stop_zsl;
    //if (tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_start_stop_video_preview;
    //if (tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_start_stop_video_record;
    //if (tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_start_stop_live_snapshot;
    //if (tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_capture_regular;
    //if (tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_capture_burst;
    //if (tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_rdi_cont;
    //if (tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_tc_rdi_burst;

    return tc;
}

int mm_app_unit_test_entry(mm_camera_app_t *cam_app)
{
    int rc = MM_CAMERA_OK;
    int i, j, tc = 0;

    tc = mm_app_gen_test_cases();
    CDBG("Running %d test cases\n",tc);
    for (i = 0; i < tc; i++) {
        for (j = 0; j < MM_QCAMERA_APP_UTEST_OUTER_LOOP; j++) {
            mm_app_tc[i].r = mm_app_tc[i].f(cam_app);
            if (mm_app_tc[i].r != MM_CAMERA_OK) {
                printf("%s: test case %d (iteration %d) error = %d, abort unit testing engine!!!!\n",
                       __func__, i, j, mm_app_tc[i].r);
                rc = mm_app_tc[i].r;
                goto end;
            }
        }
    }
end:
    printf("nTOTAL_TSET_CASE = %d, NUM_TEST_RAN = %d, rc=%d\n", tc, i, rc);
    return rc;
}




