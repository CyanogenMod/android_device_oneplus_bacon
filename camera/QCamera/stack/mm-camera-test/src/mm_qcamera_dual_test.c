/*
Copyright (c) 2012, The Linux Foundation. All rights reserved.

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

#define MM_QCAMERA_APP_WAIT_TIME 1000000000

extern int system_dimension_set(int cam_id);
extern int stopPreview(int cam_id);
extern int takePicture_yuv(int cam_id);
extern int takePicture_rdi(int cam_id);
extern int startRdi(int cam_id);
extern int stopRdi(int cam_id);
extern int startStats(int cam_id);
extern int stopStats(int cam_id);


/*
* 1. open back
* 2. open front
* 3. start back
* 4. start front
* 5. stop back
* 6. stop front
* 7. close back
* 8. close front
* 9. take picture
* a. start recording
* b. stop recording
* c. take picture rdi
*/
static mm_app_tc_t mm_app_tc[MM_QCAM_APP_TEST_NUM];
static int num_test_cases = 0;
struct test_case_params {
  uint16_t launch;
  uint16_t preview;
  uint16_t recording;
  uint16_t snapshot;
};

/*  Test case 12436857 :*/

int mm_app_dtc_0(mm_camera_app_t *cam_apps)
{
        int rc = MM_CAMERA_OK;
        int i,j;
        int result = 0;
        int front_camera = 1;
        int back_camera = 0;

        printf("\n Verifying Preview on back Camera and RDI on Front camera 0...\n");
        CDBG_ERROR("DUAL open back camera \n");
        if(mm_app_open(back_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() back camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        if(system_dimension_set(back_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL open front camera \n");
        if(mm_app_open(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() front camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        if(system_dimension_set(front_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL start camera Rdi for front \n");
        if( MM_CAMERA_OK != (rc = startRdi(front_camera))) {
               CDBG_ERROR("%s: back camera startPreview() err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        sleep(1);

        CDBG_ERROR("DUAL start camera Preview for back \n");
        if( MM_CAMERA_OK != (rc = startPreview(back_camera))) {
                CDBG_ERROR("%s: startPreview() backcamera err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        sleep(1);

        CDBG_ERROR("DUAL stop camera Rdi for front \n");
        if( MM_CAMERA_OK != (rc = stopRdi(front_camera))) {
                CDBG_ERROR("%s: startPreview() backcamera err=%d\n", __func__, rc);
                goto end;
        }
        usleep(10*1000);

        CDBG_ERROR("DUAL close front camera\n");
        if( mm_app_close(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        sleep(1);
        CDBG_ERROR("DUAL stop camera Preview for back \n");
        if( MM_CAMERA_OK != (rc = stopPreview(back_camera))) {
                CDBG("%s: startPreview() err=%d\n", __func__, rc);
                goto end;
        }
        usleep(10*1000);
        CDBG_ERROR("DUAL close back camera \n");
        if( mm_app_close(back_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        CDBG_ERROR("DUAL end \n");

end:
        if(rc == 0) {
                printf("\nPassed\n");
        }else{
                printf("\nFailed\n");
        }
        CDBG("%s:END, rc = %d\n", __func__, rc);
        return rc;
}

/*  Test case 12436587 :*/

int mm_app_dtc_1(mm_camera_app_t *cam_apps)
{
        int rc = MM_CAMERA_OK;
        int i,j;
        int result = 0;
        int front_camera = 1;
        int back_camera = 0;

        printf("\n Verifying Preview on back Camera and RDI on Front camera 1...\n");
        CDBG_ERROR("DUAL open back camera \n");
        if(mm_app_open(back_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() back camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        if(system_dimension_set(back_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL open front camera \n");
        if(mm_app_open(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() front camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        if(system_dimension_set(front_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL start camera Rdi for front \n");
        if( MM_CAMERA_OK != (rc = startRdi(front_camera))) {
               CDBG_ERROR("%s: back camera startPreview() err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        sleep(1);

        CDBG_ERROR("DUAL start camera Preview for back \n");

        if( MM_CAMERA_OK != (rc = startPreview(back_camera))) {
                CDBG_ERROR("%s: startPreview() backcamera err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        sleep(1);
        CDBG_ERROR("DUAL end \n");

        CDBG_ERROR("DUAL stop camera Preview for front \n");
        if( MM_CAMERA_OK != (rc = stopRdi(front_camera))) {
                CDBG_ERROR("%s: startPreview() backcamera err=%d\n", __func__, rc);
                goto end;
        }
        usleep(10*1000);
        CDBG_ERROR("DUAL stop camera Preview for back \n");
        if( MM_CAMERA_OK != (rc = stopPreview(back_camera))) {
                CDBG("%s: startPreview() err=%d\n", __func__, rc);
                goto end;
        }
        usleep(10*1000);
        CDBG_ERROR("DUAL close front camera\n");
        if( mm_app_close(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        CDBG_ERROR("DUAL close back camera \n");
        if( mm_app_close(back_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

end:
        if(rc == 0) {
                printf("\nPassed\n");
        }else{
                printf("\nFailed\n");
        }
        CDBG("%s:END, rc = %d\n", __func__, rc);
        return rc;
}

/*  Test case 12436578 :*/

int mm_app_dtc_2(mm_camera_app_t *cam_apps)
{
        int rc = MM_CAMERA_OK;
        int i,j;
        int result = 0;
        int front_camera = 1;
        int back_camera = 0;

        printf("\n Verifying Preview on back Camera and RDI on Front camera 2...\n");
        CDBG_ERROR("DUAL open back camera \n");
        if(mm_app_open(back_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() back camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        if(system_dimension_set(back_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL open front camera \n");
        if(mm_app_open(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() front camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        if(system_dimension_set(front_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL start camera Rdi for front \n");
        if( MM_CAMERA_OK != (rc = startRdi(front_camera))) {
               CDBG_ERROR("%s: back camera startPreview() err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        sleep(1);

        CDBG_ERROR("DUAL start camera Preview for back \n");

        if( MM_CAMERA_OK != (rc = startPreview(back_camera))) {
                CDBG_ERROR("%s: startPreview() backcamera err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        sleep(1);
        CDBG_ERROR("DUAL end \n");

        CDBG_ERROR("DUAL stop camera Preview for front \n");
        if( MM_CAMERA_OK != (rc = stopRdi(front_camera))) {
                CDBG_ERROR("%s: startPreview() backcamera err=%d\n", __func__, rc);
                goto end;
        }
        usleep(10*1000);
        CDBG_ERROR("DUAL stop camera Preview for back \n");
        if( MM_CAMERA_OK != (rc = stopPreview(back_camera))) {
                CDBG("%s: startPreview() err=%d\n", __func__, rc);
                goto end;
        }
        usleep(10*1000);
        CDBG_ERROR("DUAL close back camera \n");
        if( mm_app_close(back_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        CDBG_ERROR("DUAL close front camera\n");
        if( mm_app_close(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

end:
        if(rc == 0) {
                printf("\nPassed\n");
        }else{
                printf("\nFailed\n");
        }
        CDBG("%s:END, rc = %d\n", __func__, rc);
        return rc;
}

/*  Test case 241395768 : 1357 * 3, This is performed three times
* And for each iteration 9 is performed thrice */

int mm_app_dtc_3(mm_camera_app_t *cam_apps)
{
        int rc = MM_CAMERA_OK;
        int i,j,k;
        int result = 0;
        int front_camera = 1;
        int back_camera = 0;

        printf("\n Verifying Preview and snapshot on back Camera and RDI on Front camera 3...\n");
        CDBG_ERROR("DUAL open front camera \n");
        if(mm_app_open(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() front camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        if(system_dimension_set(front_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL start camera Preview for front \n");

        if( MM_CAMERA_OK != (rc = startRdi(front_camera))) {
                CDBG_ERROR("%s: startPreview() frontcamera err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        usleep(10*1000);

        for (k = 0; k < MM_QCAMERA_APP_INTERATION ; k++) {
          CDBG_ERROR("DUAL open back camera %d \n",k);
          if(mm_app_open(back_camera) != MM_CAMERA_OK) {
                  CDBG_ERROR("%s:mm_app_open() back camera err=%d\n",__func__, rc);
                  rc = -1;
                  goto end;
          }

          if(system_dimension_set(back_camera) != MM_CAMERA_OK){
                  CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                  rc = -1;
                  goto end;
          }

          CDBG_ERROR("DUAL start camera Preview for back \n");
          if( MM_CAMERA_OK != (rc = startPreview(back_camera))) {
                 CDBG_ERROR("%s: back camera startPreview() err=%d\n", __func__, rc);
                  goto end;
          }

          for (j = 0; j < MM_QCAMERA_APP_INTERATION; j++) {
              CDBG_ERROR("DUAL take picture for back \n");
              if ( MM_CAMERA_OK != (rc = takePicture_yuv(back_camera))) {
                  CDBG_ERROR("%s: TakePicture() err=%d\n", __func__, rc);
                  break;
              }
              mm_camera_app_wait();

          }
          usleep(10*1000);
          CDBG_ERROR("DUAL stop camera Preview for back \n");
          if( MM_CAMERA_OK != (rc = stopPreview(back_camera))) {
                  CDBG_ERROR("%s: stopPreview() backcamera err=%d\n", __func__, rc);
                  goto end;
          }
          usleep(10*1000);

          CDBG_ERROR("DUAL close back camera\n");
          if( mm_app_close(back_camera) != MM_CAMERA_OK) {
                  CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
                  rc = -1;
                  goto end;
          }
          usleep(20*1000);
        }
        CDBG_ERROR("DUAL stop camera Preview for Rdi \n");
        if( MM_CAMERA_OK != (rc = stopRdi(front_camera))) {
                CDBG("%s: stopRdi() err=%d\n", __func__, rc);
                goto end;
        }
        usleep(10*1000);
        CDBG_ERROR("DUAL close front camera \n");
        if( mm_app_close(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        CDBG_ERROR("DUAL end \n");

end:
        if(rc == 0) {
                printf("\nPassed\n");
        }else{
                printf("\nFailed\n");
        }
        CDBG("%s:END, rc = %d\n", __func__, rc);
        return rc;
}

/*  Test case 2413ab5768 : 1357 * 3, This is performed three times
* And for each iteration ab is performed thrice */

int mm_app_dtc_4(mm_camera_app_t *cam_apps)
{
        int rc = MM_CAMERA_OK;
        int i,j,k;
        int result = 0;
        int front_camera = 1;
        int back_camera = 0;

        printf("\n Verifying Preview on back Camera and RDI on Front camera 4...\n");
        CDBG_ERROR("DUAL open front camera \n");
        if(mm_app_open(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() front camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        if(system_dimension_set(front_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL start camera Preview for front \n");

        if( MM_CAMERA_OK != (rc = startRdi(front_camera))) {
                CDBG_ERROR("%s: startPreview() frontcamera err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        usleep(20*1000);

        for (k = 0; k < MM_QCAMERA_APP_INTERATION ; k++){
          CDBG_ERROR("DUAL open back camera %d \n",k);
          if(mm_app_open(back_camera) != MM_CAMERA_OK) {
                 CDBG_ERROR("%s:mm_app_open() back camera err=%d\n",__func__, rc);
                 rc = -1;
                 goto end;
          }

          if(system_dimension_set(back_camera) != MM_CAMERA_OK){
                 CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                 rc = -1;
                 goto end;
          }

          CDBG_ERROR("DUAL start camera Preview for back \n");
          if( MM_CAMERA_OK != (rc = startPreview(back_camera))) {
                CDBG_ERROR("%s: back camera startPreview() err=%d\n", __func__, rc);
                 goto end;
          }
          usleep(30*1000);

          for (j = 0; j < MM_QCAMERA_APP_INTERATION; j++) {
             CDBG_ERROR("DUAL start camera record for back \n");
             if ( MM_CAMERA_OK != (rc = startRecording(back_camera))) {
                 CDBG_ERROR("%s: StartVideorecording() err=%d\n", __func__, rc);
                 break;
             }

             mm_camera_app_wait();
             usleep(15*1000);
             CDBG_ERROR("DUAL stop camera record for back \n");
             if ( MM_CAMERA_OK != (rc = stopRecording(back_camera))) {
                 CDBG_ERROR("%s: Stopvideorecording() err=%d\n", __func__, rc);
                 break;
             }
          }
          usleep(10*1000);

          CDBG_ERROR("DUAL stop camera Preview for back \n");
          if( MM_CAMERA_OK != (rc = stopPreview(back_camera))) {
                 CDBG_ERROR("%s: stopPreview() backcamera err=%d\n", __func__, rc);
                 goto end;
          }
          usleep(10*1000);

          CDBG_ERROR("DUAL close back camera\n");
          if( mm_app_close(back_camera) != MM_CAMERA_OK) {
                 CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
                 rc = -1;
                 goto end;
          }
          usleep(20*1000);
        }
        CDBG_ERROR("DUAL stop camera Preview for Rdi \n");
        if( MM_CAMERA_OK != (rc = stopRdi(front_camera))) {
                CDBG("%s: stopRdi() err=%d\n", __func__, rc);
                goto end;
        }
        usleep(10*1000);
        CDBG_ERROR("DUAL close front camera \n");
        if( mm_app_close(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        CDBG_ERROR("DUAL end \n");

end:
        if(rc == 0) {
                printf("\nPassed\n");
        }else{
                printf("\nFailed\n");
        }
        CDBG("%s:END, rc = %d\n", __func__, rc);
        return rc;
}

/*  Test case 24135768 : 1357 * 3, This is performed three times*/

int mm_app_dtc_5(mm_camera_app_t *cam_apps)
{
        int rc = MM_CAMERA_OK;
        int i,j,k;
        int result = 0;
        int front_camera = 1;
        int back_camera = 0;

        printf("\n Verifying Preview on back Camera and RDI on Front camera 5...\n");
        CDBG_ERROR("DUAL open front camera \n");
        if(mm_app_open(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() front camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        if(system_dimension_set(front_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL start camera Preview for front \n");

        if( MM_CAMERA_OK != (rc = startRdi(front_camera))) {
                CDBG_ERROR("%s: startPreview() frontcamera err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        sleep(1);

        for (k = 0; k < 4 ; k++) {
          CDBG_ERROR("DUAL open back camera %d \n",k);
          if(mm_app_open(back_camera) != MM_CAMERA_OK) {
                  CDBG_ERROR("%s:mm_app_open() back camera err=%d\n",__func__, rc);
                  rc = -1;
                  goto end;
          }

          if(system_dimension_set(back_camera) != MM_CAMERA_OK){
                  CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                  rc = -1;
                  goto end;
          }

          CDBG_ERROR("DUAL start camera Preview for back \n");
          if( MM_CAMERA_OK != (rc = startPreview(back_camera))) {
                 CDBG_ERROR("%s: back camera startPreview() err=%d\n", __func__, rc);
                  goto end;
          }
          mm_camera_app_wait();
          sleep(1);

          CDBG_ERROR("DUAL stop camera Preview for back \n");
          if( MM_CAMERA_OK != (rc = stopPreview(back_camera))) {
                  CDBG_ERROR("%s: stopPreview() backcamera err=%d\n", __func__, rc);
                  goto end;
          }
          usleep(10*1000);

          CDBG_ERROR("DUAL close back camera\n");
          if( mm_app_close(back_camera) != MM_CAMERA_OK) {
                  CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
                  rc = -1;
                  goto end;
          }
          sleep(1);
        }
        CDBG_ERROR("DUAL stop camera Preview for Rdi \n");
        if( MM_CAMERA_OK != (rc = stopRdi(front_camera))) {
                CDBG("%s: stopRdi() err=%d\n", __func__, rc);
                goto end;
        }
        usleep(10*1000);
        CDBG_ERROR("DUAL close front camera \n");
        if( mm_app_close(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        CDBG_ERROR("DUAL end \n");

end:
        if(rc == 0) {
                printf("\nPassed\n");
        }else{
                printf("\nFailed\n");
        }
        CDBG("%s:END, rc = %d\n", __func__, rc);
        return rc;
}

/*  Test case 13246857 : 2468 * 3, This is performed three times*/

int mm_app_dtc_6(mm_camera_app_t *cam_apps)
{
        int rc = MM_CAMERA_OK;
        int i,j,k;
        int result = 0;
        int front_camera = 1;
        int back_camera = 0;

        printf("\n Verifying Preview on back Camera and RDI on Front camera 6...\n");
        CDBG_ERROR("DUAL open back camera \n");
        if(mm_app_open(back_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() back camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        if(system_dimension_set(back_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL start camera Preview for back \n");

        if( MM_CAMERA_OK != (rc = startPreview(back_camera))) {
                CDBG_ERROR("%s: startPreview() backcamera err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        sleep(1);

        for (k = 0; k < 4 ; k++) {
        CDBG_ERROR("DUAL open front camera %d \n",k);
        if(mm_app_open(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() front camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        if(system_dimension_set(front_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL start camera Rdi for front \n");
        if( MM_CAMERA_OK != (rc = startRdi(front_camera))) {
               CDBG_ERROR("%s: back camera startPreview() err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        sleep(1);

        CDBG_ERROR("DUAL stop camera Preview for front \n");
        if( MM_CAMERA_OK != (rc = stopRdi(front_camera))) {
                CDBG_ERROR("%s: startPreview() backcamera err=%d\n", __func__, rc);
                goto end;
        }
        usleep(10*1000);

        CDBG_ERROR("DUAL close front camera\n");
        if( mm_app_close(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        sleep(1);
        }
        CDBG_ERROR("DUAL stop camera Preview for back \n");
        if( MM_CAMERA_OK != (rc = stopPreview(back_camera))) {
                CDBG("%s: startPreview() err=%d\n", __func__, rc);
                goto end;
        }
        usleep(10*1000);
        CDBG_ERROR("DUAL close back camera \n");
        if( mm_app_close(back_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        CDBG_ERROR("DUAL end \n");

end:
        if(rc == 0) {
                printf("\nPassed\n");
        }else{
                printf("\nFailed\n");
        }
        CDBG("%s:END, rc = %d\n", __func__, rc);
        return rc;
}

/*Multi Threaded Test Cases*/
static void *front_thread(void *data)
{
        int front_camera = 1;
        int rc = MM_CAMERA_OK;
        int i,j,k,m;
        struct test_case_params params
          = *((struct test_case_params *)data);
        for (i = 0; i < params.launch; i++) {
          CDBG_ERROR("DUAL open front camera %d\n",i);
          if(mm_app_open(front_camera) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() front camera err=%d\n",__func__, rc);
            rc = -1;
            goto end;
          }

          if(system_dimension_set(front_camera) != MM_CAMERA_OK){
            CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
          }

          for (j = 0; j < params.preview; j++) {
            CDBG_ERROR("DUAL start camera Rdi for front %d ,%d \n",i,j);
            if( MM_CAMERA_OK != (rc = startRdi(front_camera))) {
              CDBG_ERROR("%s: back camera startPreview() err=%d\n", __func__, rc);
              goto end;
            }
            mm_camera_app_wait();
            usleep(20*1000);
            for (k = 0; k < params.snapshot; k++) {
              CDBG_ERROR("DUAL take picture for front %d,%d,%d \n",i,j,k);
              if ( MM_CAMERA_OK != (rc = takePicture_rdi(front_camera))) {
                CDBG_ERROR("%s: TakePicture() err=%d\n", __func__, rc);
                goto end;
              }
              mm_camera_app_wait();
              usleep(30*1000);
            }
            CDBG_ERROR("DUAL stop camera Rdi for front %d,%d\n",i,j);
            if( MM_CAMERA_OK != (rc = stopRdi(front_camera))) {
              CDBG_ERROR("%s: startPreview() backcamera err=%d\n", __func__, rc);
              goto end;
            }
            usleep(10*1000);
          }

          CDBG_ERROR("DUAL close front camera %d\n",i);
          if( mm_app_close(front_camera) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
          }
        }
end:
        CDBG_ERROR("DUAL front thread close %d",rc);
        return NULL;
}

static void *back_thread(void *data)
{
        int rc = MM_CAMERA_OK;
        int back_camera = 0;
        int i,j,k,m;
        struct test_case_params params
          = *((struct test_case_params *)data);
        for (i = 0; i < params.launch; i++) {
          CDBG_ERROR("DUAL open back camera %d\n",i);
          if(mm_app_open(back_camera) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() back camera err=%d\n",__func__, rc);
            rc = -1;
            goto end;
          }
          if(system_dimension_set(back_camera) != MM_CAMERA_OK){
            CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
          }

          for (j = 0; j < params.preview; j++) {
            CDBG_ERROR("DUAL start camera Preview for back %d, %d\n",i,j);
            if( MM_CAMERA_OK != (rc = startPreview(back_camera))) {
              CDBG_ERROR("%s: startPreview() backcamera err=%d\n", __func__, rc);
              goto end;
            }
            mm_camera_app_wait();
            usleep(20*1000);
            for (k = 0; k < params.snapshot; k++) {
              CDBG_ERROR("DUAL take picture for back %d, %d, %d\n",i,j,k);
              if ( MM_CAMERA_OK != (rc = takePicture_yuv(back_camera))) {
                CDBG_ERROR("%s: TakePicture() err=%d\n", __func__, rc);
                goto end;
              }
              mm_camera_app_wait();
              usleep(30*1000);
            }

            for (m = 0; m < params.recording; m++) {
              CDBG_ERROR("DUAL start record for back %d, %d, %d\n",i,j,m);
              if ( MM_CAMERA_OK != (rc = startRecording(back_camera))) {
                CDBG_ERROR("%s: StartVideorecording() err=%d\n", __func__, rc);
                break;
              }

              mm_camera_app_wait();
              usleep(10*1000);
              CDBG_ERROR("DUAL stop camera record for back \n");
              if ( MM_CAMERA_OK != (rc = stopRecording(back_camera))) {
                CDBG_ERROR("%s: Stopvideorecording() err=%d\n", __func__, rc);
                break;
              }
              usleep(10*1000);
            }
            CDBG_ERROR("DUAL stop camera Preview for back %d, %d\n",i,j);
            if( MM_CAMERA_OK != (rc = stopPreview(back_camera))) {
              CDBG("%s: startPreview() err=%d\n", __func__, rc);
              goto end;
            }
            usleep(10*1000);
          }

          CDBG_ERROR("DUAL close back camera %d\n",i);
          if( mm_app_close(back_camera) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
          }
        }
end:
        CDBG_ERROR("DUAL back thread close %d",rc);
        return NULL;
}

/*  Test case m13572468 : Open & start  in 2 concurrent pthread*/
int mm_app_dtc_7(mm_camera_app_t *cam_apps)
{
        int rc = MM_CAMERA_OK;
        int result = 0;
        pthread_t back_thread_id, front_thread_id;
        struct test_case_params params;
        memset(&params, 0, sizeof(struct test_case_params));
        params.launch = 5;
        params.preview = 5;
        printf("\n Verifying Preview on back Camera and RDI on Front camera 7...\n");

        CDBG_ERROR("start back DUAL ");
        rc = pthread_create(&back_thread_id, NULL, back_thread, &params);
        CDBG_ERROR("start front DUAL ");
        rc = pthread_create(&front_thread_id, NULL, front_thread, &params);
        sleep(1);
        CDBG_ERROR("stop back DUAL ");
        rc = pthread_join(back_thread_id, NULL);
        CDBG_ERROR("stop front DUAL ");
        rc = pthread_join(front_thread_id, NULL);
        CDBG_ERROR("DUAL end \n");

end:
        if(rc == 0) {
          printf("\nPassed\n");
        }else{
          printf("\nFailed\n");
        }
        CDBG("%s:END, rc = %d\n", __func__, rc);
        return rc;
}

/*  Test case m139572468 : Open & start in 2 concurrent pthread*/
int mm_app_dtc_8(mm_camera_app_t *cam_apps)
{
        int rc = MM_CAMERA_OK;
        int result = 0;

        pthread_t back_thread_id, front_thread_id;
        struct test_case_params bparams, fparams;
        memset(&bparams, 0, sizeof(struct test_case_params));
        memset(&fparams, 0, sizeof(struct test_case_params));
        bparams.launch = 5;
        bparams.preview = 5;
        bparams.snapshot= 5;
        fparams.launch = 5;
        fparams.preview = 5;
        printf("\n Verifying Preview on back Camera and RDI on Front camera 8...\n");

        CDBG_ERROR("start back DUAL ");
        rc = pthread_create(&back_thread_id, NULL, back_thread, &bparams);
        CDBG_ERROR("start front DUAL ");
        rc = pthread_create(&front_thread_id, NULL, front_thread, &fparams);
        sleep(1);
        CDBG_ERROR("stop back DUAL ");
        rc = pthread_join(back_thread_id, NULL);
        CDBG_ERROR("stop front DUAL ");
        rc = pthread_join(front_thread_id, NULL);
        CDBG_ERROR("DUAL end \n");

end:
        if(rc == 0)
          printf("\nPassed\n");
        else
          printf("\nFailed\n");
        CDBG("%s:END, rc = %d\n", __func__, rc);
        return rc;
}

/*  Test case m1395724c68 : Open & start in 2 concurrent pthread*/
int mm_app_dtc_9(mm_camera_app_t *cam_apps)
{
        int rc = MM_CAMERA_OK;
        int result = 0;

        pthread_t back_thread_id, front_thread_id;
        struct test_case_params bparams, fparams;
        memset(&bparams, 0, sizeof(struct test_case_params));
        memset(&fparams, 0, sizeof(struct test_case_params));
        bparams.launch = 5;
        bparams.preview = 5;
        bparams.snapshot= 5;
        fparams.launch = 5;
        fparams.preview = 5;
        fparams.snapshot = 5;
        printf("\n Verifying Preview on back Camera and RDI on Front camera 9...\n");

        CDBG_ERROR("start back DUAL ");
        rc = pthread_create(&back_thread_id, NULL, back_thread, &bparams);
        CDBG_ERROR("start front DUAL ");
        rc = pthread_create(&front_thread_id, NULL, front_thread, &fparams);
        sleep(1);
        CDBG_ERROR("stop back DUAL ");
        rc = pthread_join(back_thread_id, NULL);
        CDBG_ERROR("stop front DUAL ");
        rc = pthread_join(front_thread_id, NULL);
        CDBG_ERROR("DUAL end \n");

end:
        if(rc == 0) {
          printf("\nPassed\n");
        }else{
          printf("\nFailed\n");
        }
        CDBG("%s:END, rc = %d\n", __func__, rc);
        return rc;
}

/*  Test case m13ab572468 : Open & start in 2 concurrent pthread*/
int mm_app_dtc_10(mm_camera_app_t *cam_apps)
{
        int rc = MM_CAMERA_OK;
        int result = 0;

        pthread_t back_thread_id, front_thread_id;
        struct test_case_params bparams, fparams;
        memset(&bparams, 0, sizeof(struct test_case_params));
        memset(&fparams, 0, sizeof(struct test_case_params));
        bparams.launch = 5;
        bparams.preview = 5;
        bparams.recording= 5;
        fparams.launch = 5;
        fparams.preview = 5;
        printf("\n Verifying Preview on back Camera and RDI on Front camera 10...\n");

        CDBG_ERROR("start back DUAL ");
        rc = pthread_create(&back_thread_id, NULL, back_thread, &bparams);
        CDBG_ERROR("start front DUAL ");
        rc = pthread_create(&front_thread_id, NULL, front_thread, &fparams);
        sleep(1);
        CDBG_ERROR("stop back DUAL ");
        rc = pthread_join(back_thread_id, NULL);
        CDBG_ERROR("stop front DUAL ");
        rc = pthread_join(front_thread_id, NULL);
        CDBG_ERROR("DUAL end \n");
end:
        if(rc == 0) {
          printf("\nPassed\n");
        }else{
          printf("\nFailed\n");
        }
        CDBG("%s:END, rc = %d\n", __func__, rc);
        return rc;
}

/*  Test case m13ab5724c68 : Open & start in 2 concurrent pthread*/
int mm_app_dtc_11(mm_camera_app_t *cam_apps)
{
        int rc = MM_CAMERA_OK;
        int result = 0;

        pthread_t back_thread_id, front_thread_id;
        struct test_case_params bparams, fparams;
        memset(&bparams, 0, sizeof(struct test_case_params));
        memset(&fparams, 0, sizeof(struct test_case_params));
        bparams.launch = 5;
        bparams.preview = 5;
        bparams.recording= 5;
        fparams.launch = 5;
        fparams.preview = 5;
        fparams.snapshot = 5;
        printf("\n Verifying Preview on back Camera and RDI on Front camera 11...\n");

        CDBG_ERROR("start back DUAL ");
        rc = pthread_create(&back_thread_id, NULL, back_thread, &bparams);
        CDBG_ERROR("start front DUAL ");
        rc = pthread_create(&front_thread_id, NULL, front_thread, &fparams);
        sleep(1);
        CDBG_ERROR("stop back DUAL ");
        rc = pthread_join(back_thread_id, NULL);
        CDBG_ERROR("stop front DUAL ");
        rc = pthread_join(front_thread_id, NULL);
        CDBG_ERROR("DUAL end \n");

end:
        if(rc == 0) {
                printf("\nPassed\n");
        }else{
                printf("\nFailed\n");
        }
        CDBG("%s:END, rc = %d\n", __func__, rc);
        return rc;
}

/*  Test case m1728 : Open & start in 2 concurrent pthread*/
int mm_app_dtc_12(mm_camera_app_t *cam_apps)
{
        int rc = MM_CAMERA_OK;
        int result = 0;

        pthread_t back_thread_id, front_thread_id;
        struct test_case_params bparams, fparams;
        memset(&bparams, 0, sizeof(struct test_case_params));
        memset(&fparams, 0, sizeof(struct test_case_params));
        bparams.launch = 15;
        fparams.launch = 15;
        printf("\n Verifying Preview on back Camera and RDI on Front camera 12...\n");

        CDBG_ERROR("start back DUAL ");
        rc = pthread_create(&back_thread_id, NULL, back_thread, &bparams);
        CDBG_ERROR("start front DUAL ");
        rc = pthread_create(&front_thread_id, NULL, front_thread, &fparams);
        sleep(1);
        CDBG_ERROR("stop back DUAL ");
        rc = pthread_join(back_thread_id, NULL);
        CDBG_ERROR("stop front DUAL ");
        rc = pthread_join(front_thread_id, NULL);
        CDBG_ERROR("DUAL end \n");

end:
        if(rc == 0) {
                printf("\nPassed\n");
        }else{
                printf("\nFailed\n");
        }
        CDBG("%s:END, rc = %d\n", __func__, rc);
        return rc;
}

/*  Test case 2413(ab)5768
 *  Test the dual camera usecase. We startPreview on front camera,
 *  but backend will allocate RDI buffers and start front camera in
 *  RDI streaming mode. It then diverts RDI frames, converts them into YUV 420
 *  through C2D and generate preview data in the buffers allocated here.
 *  Back camera will use the pixel interface as usual.
 */

int mm_app_dtc_13(mm_camera_app_t *cam_apps)
{
        int rc = MM_CAMERA_OK;
        int i,j,k;
        int result = 0;
        int front_camera = 1;
        int back_camera = 0;

        printf("\n 13. Verifying Preview + Recording on back Camera and Preview(through RDI) on Front camera\n");
        CDBG_ERROR("DUAL open front camera \n");
        if(mm_app_open(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() front camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        if(system_dimension_set(front_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL start camera Preview for front \n");
        if( MM_CAMERA_OK != (rc = startPreview(front_camera))) {
               CDBG_ERROR("%s: front camera startPreview() err=%d\n", __func__, rc);
               goto end;
        }
        mm_camera_app_wait();
        usleep(20*1000);

        for (k = 0; k < MM_QCAMERA_APP_INTERATION ; k++){
          CDBG_ERROR("DUAL open back camera %d \n",k);
          if(mm_app_open(back_camera) != MM_CAMERA_OK) {
                 CDBG_ERROR("%s:mm_app_open() back camera err=%d\n",__func__, rc);
                 rc = -1;
                 goto end;
          }

          if(system_dimension_set(back_camera) != MM_CAMERA_OK){
                 CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                 rc = -1;
                 goto end;
          }

          CDBG_ERROR("DUAL start camera Preview for back \n");
          if( MM_CAMERA_OK != (rc = startPreview(back_camera))) {
                CDBG_ERROR("%s: back camera startPreview() err=%d\n", __func__, rc);
                 goto end;
          }
          usleep(30*1000);

          for (j = 0; j < MM_QCAMERA_APP_INTERATION; j++) {
             CDBG_ERROR("DUAL start camera record for back Iteration %d \n", j);
             if ( MM_CAMERA_OK != (rc = startRecording(back_camera))) {
                 CDBG_ERROR("%s: StartVideorecording() err=%d\n", __func__, rc);
                 break;
             }

             mm_camera_app_wait();
             usleep(10*1000*1000);
             CDBG_ERROR("DUAL stop camera record for back Iteration %d\n", j);
             if ( MM_CAMERA_OK != (rc = stopRecording(back_camera))) {
                 CDBG_ERROR("%s: Stopvideorecording() err=%d\n", __func__, rc);
                 break;
             }
          }
          usleep(10*1000);

          CDBG_ERROR("DUAL stop camera Preview for back \n");
          if( MM_CAMERA_OK != (rc = stopPreview(back_camera))) {
                 CDBG_ERROR("%s: stopPreview() backcamera err=%d\n", __func__, rc);
                 goto end;
          }
          usleep(10*1000);

          CDBG_ERROR("DUAL close back camera\n");
          if( mm_app_close(back_camera) != MM_CAMERA_OK) {
                 CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
                 rc = -1;
                 goto end;
          }
          usleep(20*1000);
        }
        CDBG_ERROR("DUAL stop camera Preview for Rdi \n");
        if( MM_CAMERA_OK != (rc = stopPreview(front_camera))) {
                CDBG_ERROR("%s: stopPreview() frontcamera err=%d\n", __func__, rc);
                goto end;
        }
        usleep(10*1000);
        CDBG_ERROR("DUAL close front camera \n");
        if( mm_app_close(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        CDBG_ERROR("DUAL end \n");

end:
        if(rc == 0) {
                printf("\nPassed\n");
        }else{
                printf("\nFailed\n");
        }
        CDBG("%s:END, rc = %d\n", __func__, rc);
        return rc;
}

/*Below 6  are reference test cases just to test the open path for dual camera*/
int mm_app_dtc_1243(mm_camera_app_t *cam_apps)
{
        int rc = MM_CAMERA_OK;
        int i,j;
        int result = 0;
        int front_camera = 1;
        int back_camera = 0;

        printf("\n Verifying Preview on back Camera and RDI on Front camera...\n");
        CDBG_ERROR("DUAL open back camera \n");
        if(mm_app_open(back_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() back camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        if(system_dimension_set(back_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL open front camera \n");
        if(mm_app_open(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() front camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        if(system_dimension_set(front_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL start camera Rdi for front \n");
        if( MM_CAMERA_OK != (rc = startRdi(front_camera))) {
               CDBG_ERROR("%s: back camera startPreview() err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        sleep(1);

        CDBG_ERROR("DUAL start camera Preview for back \n");

        if( MM_CAMERA_OK != (rc = startPreview(back_camera))) {
                CDBG_ERROR("%s: startPreview() backcamera err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        sleep(1);
        CDBG_ERROR("DUAL end \n");

end:
        if(rc == 0) {
                printf("\nPassed\n");
        }else{
                printf("\nFailed\n");
        }
        CDBG("%s:END, rc = %d\n", __func__, rc);
        return rc;
}

int mm_app_dtc_2134(mm_camera_app_t *cam_apps)
{
        int rc = MM_CAMERA_OK;
        int i,j;
        int result = 0;
        int front_camera = 1;
        int back_camera = 0;

        printf("\n Verifying Preview on back Camera and RDI on Front camera...\n");
        CDBG_ERROR("DUAL open front camera \n");
        if(mm_app_open(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() back camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        if(system_dimension_set(front_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL open back camera \n");
        if(mm_app_open(back_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() front camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        if(system_dimension_set(back_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL start camera Preview for front \n");
        if( MM_CAMERA_OK != (rc = startPreview(back_camera))) {
               CDBG_ERROR("%s: back camera startPreview() err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        sleep(1);

        CDBG_ERROR("DUAL start camera Rdi for back \n");

        if( MM_CAMERA_OK != (rc = startRdi(front_camera))) {
                CDBG_ERROR("%s: startPreview() backcamera err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        sleep(1);
        CDBG_ERROR("DUAL end \n");

end:
        if(rc == 0) {
                printf("\nPassed\n");
        }else{
                printf("\nFailed\n");
        }
        CDBG("%s:END, rc = %d\n", __func__, rc);
        return rc;
}
int mm_app_dtc_2143(mm_camera_app_t *cam_apps)
{
        int rc = MM_CAMERA_OK;
        int i,j;
        int result = 0;
        int front_camera = 1;
        int back_camera = 0;

        printf("\n Verifying Preview on back Camera and RDI on Front camera...\n");
        CDBG_ERROR("DUAL open front camera \n");
        if(mm_app_open(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() back camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        if(system_dimension_set(front_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL open back camera \n");
        if(mm_app_open(back_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() front camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        if(system_dimension_set(back_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL start camera rdi for front \n");
        if( MM_CAMERA_OK != (rc = startRdi(front_camera))) {
               CDBG_ERROR("%s: back camera startPreview() err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        sleep(1);

        CDBG_ERROR("DUAL start camera preview for back \n");

        if( MM_CAMERA_OK != (rc = startPreview(back_camera))) {
                CDBG_ERROR("%s: startPreview() backcamera err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        sleep(1);
        CDBG_ERROR("DUAL end \n");

end:
        if(rc == 0) {
                printf("\nPassed\n");
        }else{
                printf("\nFailed\n");
        }
        CDBG("%s:END, rc = %d\n", __func__, rc);
        return rc;
}

int mm_app_dtc_2413(mm_camera_app_t *cam_apps)
{
        int rc = MM_CAMERA_OK;
        int i,j;
        int result = 0;
        int front_camera = 1;
        int back_camera = 0;

        printf("\n Verifying Preview on back Camera and RDI on Front camera...\n");
        CDBG_ERROR("DUAL open front camera \n");
        if(mm_app_open(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() back camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        if(system_dimension_set(front_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL start camera rdi for front \n");
        if( MM_CAMERA_OK != (rc = startRdi(front_camera))) {
               CDBG_ERROR("%s: back camera startPreview() err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        sleep(1);

        CDBG_ERROR("DUAL open back camera \n");
        if(mm_app_open(back_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() front camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        if(system_dimension_set(back_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL start camera preview for back \n");

        if( MM_CAMERA_OK != (rc = startPreview(back_camera))) {
                CDBG_ERROR("%s: startPreview() backcamera err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        sleep(1);
        CDBG_ERROR("DUAL end \n");

end:
        if(rc == 0) {
                printf("\nPassed\n");
        }else{
                printf("\nFailed\n");
        }
        CDBG("%s:END, rc = %d\n", __func__, rc);
        return rc;
}

int mm_app_dtc_1234(mm_camera_app_t *cam_apps)
{
        int rc = MM_CAMERA_OK;
        int i,j;
        int result = 0;
        int front_camera = 1;
        int back_camera = 0;

        printf("\n Verifying Preview on back Camera and RDI on Front camera...\n");
        CDBG_ERROR("DUAL open back camera \n");
        if(mm_app_open(back_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() back camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        if(system_dimension_set(back_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        CDBG_ERROR("DUAL open front camera \n");
        if(mm_app_open(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() front camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        if(system_dimension_set(front_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

        CDBG_ERROR("DUAL start camera preview for back \n");
        if( MM_CAMERA_OK != (rc = startPreview(back_camera))) {
               CDBG_ERROR("%s: back camera startPreview() err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        sleep(1);

        CDBG_ERROR("DUAL start camera rdi for front \n");

        if( MM_CAMERA_OK != (rc = startRdi(front_camera))) {
                CDBG_ERROR("%s: startPreview() backcamera err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        sleep(1);
        CDBG_ERROR("DUAL end \n");

end:
        if(rc == 0) {
                printf("\nPassed\n");
        }else{
                printf("\nFailed\n");
        }
        CDBG("%s:END, rc = %d\n", __func__, rc);
        return rc;
}

int mm_app_dtc_1324(mm_camera_app_t *cam_apps)
{
        int rc = MM_CAMERA_OK;
        int i,j;
        int result = 0;
        int front_camera = 1;
        int back_camera = 0;

        printf("\n Verifying Preview on back Camera and RDI on Front camera...\n");
        CDBG_ERROR("DUAL start back camera \n");
        if(mm_app_open(back_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() back camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        if(system_dimension_set(back_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        CDBG_ERROR("DUAL start camera preview for back \n");
        if( MM_CAMERA_OK != (rc = startPreview(back_camera))) {
                CDBG_ERROR("%s: back camera startPreview() err=%d\n", __func__, rc);
                goto end;
        }
        //mm_camera_app_wait();
        sleep(1);
        CDBG_ERROR("DUAL start front camera \n");
        if(mm_app_open(front_camera) != MM_CAMERA_OK) {
                CDBG_ERROR("%s:mm_app_open() front camera err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }

       if(system_dimension_set(front_camera) != MM_CAMERA_OK){
                CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
                rc = -1;
                goto end;
        }
        CDBG_ERROR("DUAL start rdi preview \n");

        if( MM_CAMERA_OK != (rc = startRdi(front_camera))) {
                CDBG_ERROR("%s: startPreview() backcamera err=%d\n", __func__, rc);
                goto end;
        }
        mm_camera_app_wait();
        sleep(1);
        CDBG_ERROR("DUAL end \n");

end:
        if(rc == 0) {
          printf("\nPassed\n");
        }else{
          printf("\nFailed\n");
        }
        CDBG("%s:END, rc = %d\n", __func__, rc);
        return rc;
}

/* single camera test cases*/
int mm_app_dtc_s_0(mm_camera_app_t *cam_apps)
{
    int rc = MM_CAMERA_OK;
    int i,j;
    int result = 0;
    int front_camera = 1;
    int back_camera = 0;

    printf("\n Verifying Preview on back Camera and RDI on Front camera...\n");

    if(mm_app_open(back_camera) != MM_CAMERA_OK) {
        CDBG_ERROR("%s:mm_app_open() back camera err=%d\n",__func__, rc);
        rc = -1;
        goto end;
    }
    if(system_dimension_set(back_camera) != MM_CAMERA_OK){
    CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
        rc = -1;
        goto end;
    }

    if( MM_CAMERA_OK != (rc = startPreview(back_camera))) {
        CDBG_ERROR("%s: back camera startPreview() err=%d\n", __func__, rc);
        goto end;
    }

    mm_camera_app_wait();
    if(mm_app_open(front_camera) != MM_CAMERA_OK) {
        CDBG_ERROR("%s:mm_app_open() front camera err=%d\n",__func__, rc);
        rc = -1;
        goto end;
    }
    if(system_dimension_set(front_camera) != MM_CAMERA_OK){
        CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
        rc = -1;
        goto end;
    }

    if( MM_CAMERA_OK != (rc = startRdi(front_camera))) {
        CDBG_ERROR("%s: startPreview() backcamera err=%d\n", __func__, rc);
        goto end;
    }
    mm_camera_app_wait();

    if( MM_CAMERA_OK != (rc = stopRdi(front_camera))) {
        CDBG_ERROR("%s: startPreview() backcamera err=%d\n", __func__, rc);
        goto end;
    }

    if( MM_CAMERA_OK != (rc = stopPreview(my_cam_app.cam_open))) {
        CDBG("%s: startPreview() err=%d\n", __func__, rc);
        goto end;
    }

    if( mm_app_close(my_cam_app.cam_open) != MM_CAMERA_OK) {
        CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
        rc = -1;
        goto end;
    }
end:
    if(rc == 0) {
        printf("\nPassed\n");
    }else{
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_dtc_s_1(mm_camera_app_t *cam_apps)
{
    int rc = MM_CAMERA_OK;
    int i,j;
    int result = 0;

    printf("\n Verifying Snapshot on front and back camera...\n");
    for(i = 0; i < cam_apps->num_cameras; i++) {
        if( mm_app_open(i) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if(system_dimension_set(my_cam_app.cam_open) != MM_CAMERA_OK){
            CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }

        if( MM_CAMERA_OK != (rc = startPreview(my_cam_app.cam_open))) {
                CDBG_ERROR("%s: startPreview() err=%d\n", __func__, rc);
                break;
        }
        for(j = 0; j < MM_QCAMERA_APP_INTERATION; j++) {
            if( MM_CAMERA_OK != (rc = takePicture_yuv(my_cam_app.cam_open))) {
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
        if( MM_CAMERA_OK != (rc = stopPreview(my_cam_app.cam_open))) {
            CDBG("%s: startPreview() err=%d\n", __func__, rc);
            break;
        }
        if( mm_app_close(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if(result != MM_QCAMERA_APP_INTERATION) {
            printf("%s: Snapshot Start/Stop Fails for Camera %d in %d iteration", __func__, i,j);
            rc = -1;
            break;
        }

        result = 0;
    }
end:
    if(rc == 0) {
        printf("\t***Passed***\n");
    }else{
        printf("\t***Failed***\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_dtc_s_2(mm_camera_app_t *cam_apps)
{
    int rc = MM_CAMERA_OK;
    int i,j;
    int result = 0;

    printf("\n Verifying Video on front and back camera...\n");
    for(i = 0; i < cam_apps->num_cameras; i++) {
        if( mm_app_open(i) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if(system_dimension_set(my_cam_app.cam_open) != MM_CAMERA_OK){
            CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }

        if( MM_CAMERA_OK != (rc = startPreview(my_cam_app.cam_open))) {
            CDBG_ERROR("%s: startPreview() err=%d\n", __func__, rc);
            break;
        }
        for(j = 0; j < MM_QCAMERA_APP_INTERATION; j++) {
            if( MM_CAMERA_OK != (rc = startRecording(my_cam_app.cam_open))) {
                CDBG_ERROR("%s: StartVideorecording() err=%d\n", __func__, rc);
                break;
            }

            /*if(mm_camera_app_timedwait() == ETIMEDOUT) {
            CDBG_ERROR("%s: Video Callback not received in time\n", __func__);
            break;
            }*/
            mm_camera_app_wait();
            if( MM_CAMERA_OK != (rc = stopRecording(my_cam_app.cam_open))) {
                CDBG_ERROR("%s: Stopvideorecording() err=%d\n", __func__, rc);
                break;
            }
            result++;
        }
        if( MM_CAMERA_OK != (rc = stopPreview(my_cam_app.cam_open))) {
            CDBG("%s: startPreview() err=%d\n", __func__, rc);
            break;
        }
        if( mm_app_close(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if(result != MM_QCAMERA_APP_INTERATION) {
            printf("%s: Video Start/Stop Fails for Camera %d in %d iteration", __func__, i,j);
            rc = -1;
            break;
        }

        result = 0;
    }
end:
    if(rc == 0) {
        printf("\nPassed\n");
    }else{
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_dtc_s_3(mm_camera_app_t *cam_apps)
{
    int rc = MM_CAMERA_OK;
    int i,j;
    int result = 0;

    printf("\n Verifying RDI Stream on front and back camera...\n");
    if(cam_apps->num_cameras == 0) {
        CDBG_ERROR("%s:Query Failed: Num of cameras = %d\n",__func__, cam_apps->num_cameras);
        rc = -1;
        goto end;
    }
    for(i = 0; i < cam_apps->num_cameras; i++) {
        if( mm_app_open(i) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_open() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if(system_dimension_set(my_cam_app.cam_open) != MM_CAMERA_OK){
            CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        for(j = 0; j < MM_QCAMERA_APP_INTERATION; j++) {
            if( MM_CAMERA_OK != (rc = startRdi(my_cam_app.cam_open))) {
                CDBG_ERROR("%s: StartVideorecording() err=%d\n", __func__, rc);
                break;
            }

            /*if(mm_camera_app_timedwait() == ETIMEDOUT) {
            CDBG_ERROR("%s: Video Callback not received in time\n", __func__);
            break;
            }*/
            mm_camera_app_wait();
            if( MM_CAMERA_OK != (rc = stopRdi(my_cam_app.cam_open))) {
                CDBG_ERROR("%s: Stopvideorecording() err=%d\n", __func__, rc);
                break;
            }
            result++;
        }
        if( mm_app_close(my_cam_app.cam_open) != MM_CAMERA_OK) {
            CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
            rc = -1;
            goto end;
        }
        if(result != MM_QCAMERA_APP_INTERATION) {
            printf("%s: Video Start/Stop Fails for Camera %d in %d iteration", __func__, i,j);
            rc = -1;
            break;
        }

        result = 0;
    }
end:
    if(rc == 0) {
        printf("\nPassed\n");
    }else{
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

/*Stats Test Case*/
int mm_app_dtc_s_5(mm_camera_app_t *cam_apps)
{
    int rc = MM_CAMERA_OK;
    int i,j;
    int result = 0;
    int front_camera = 1;
    int back_camera = 0;

    printf("\n Verifying Preview on back Camera and RDI on Front camera...\n");

    if(mm_app_open(back_camera) != MM_CAMERA_OK) {
        CDBG_ERROR("%s:mm_app_open() back camera err=%d\n",__func__, rc);
        rc = -1;
        goto end;
    }
    if(system_dimension_set(back_camera) != MM_CAMERA_OK){
    CDBG_ERROR("%s:system_dimension_set() err=%d\n",__func__, rc);
        rc = -1;
        goto end;
    }

    if( MM_CAMERA_OK != (rc = startStats(back_camera))) {
        CDBG_ERROR("%s: back camera startPreview() err=%d\n", __func__, rc);
        goto end;
    }

    mm_camera_app_wait();

    if( MM_CAMERA_OK != (rc = stopStats(my_cam_app.cam_open))) {
        CDBG("%s: startPreview() err=%d\n", __func__, rc);
        goto end;
    }

    if( mm_app_close(my_cam_app.cam_open) != MM_CAMERA_OK) {
        CDBG_ERROR("%s:mm_app_close() err=%d\n",__func__, rc);
        rc = -1;
        goto end;
    }
end:
    if(rc == 0) {
        printf("\nPassed\n");
    }else{
        printf("\nFailed\n");
    }
    CDBG("%s:END, rc = %d\n", __func__, rc);
    return rc;
}

int mm_app_gen_dual_test_cases()
{
    int tc = 0;
    memset(mm_app_tc, 0, sizeof(mm_app_tc));
    if(tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_dtc_0;
    if(tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_dtc_1;
    if(tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_dtc_2;
    if(tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_dtc_3;
    if(tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_dtc_4;
    if(tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_dtc_5;
    if(tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_dtc_6;
    if(tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_dtc_7;
    if(tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_dtc_8;
    if(tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_dtc_9;
    if(tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_dtc_10;
    if(tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_dtc_11;
    if(tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_dtc_12;
    if(tc < MM_QCAM_APP_TEST_NUM) mm_app_tc[tc++].f = mm_app_dtc_13;

    return tc;
}

int mm_app_dual_test_entry(mm_camera_app_t *cam_app)
{
    int rc = MM_CAMERA_OK;
    int i, tc = 0;
    int cam_id = 0;

    tc = mm_app_gen_dual_test_cases();
    CDBG("Running %d test cases\n",tc);
    for(i = 0; i < tc; i++) {
        mm_app_tc[i].r = mm_app_tc[i].f(cam_app);
        if(mm_app_tc[i].r != MM_CAMERA_OK) {
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




