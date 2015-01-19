OLD_LOCAL_PATH := $(LOCAL_PATH)
LOCAL_PATH:=$(call my-dir)
include $(CLEAR_VARS)

ifeq ($(call is-board-platform,msm8960),true)
LOCAL_CFLAGS:= \
        -DAMSS_VERSION=$(AMSS_VERSION) \
        $(mmcamera_debug_defines) \
        $(mmcamera_debug_cflags) \
        $(USE_SERVER_TREE)

LOCAL_CFLAGS += -include $(TARGET_OUT_INTERMEDIATES)/include/mm-camera/camera_defs_i.h
LOCAL_CFLAGS += -include $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include/linux/msm_ion.h

ifeq ($(strip $(TARGET_USES_ION)),true)
LOCAL_CFLAGS += -DUSE_ION
endif

LOCAL_CFLAGS += -D_ANDROID_

LOCAL_SRC_FILES:= \
        src/mm_qcamera_main_menu.c \
        src/mm_qcamera_display.c \
        src/mm_qcamera_app.c \
        src/mm_qcamera_snapshot.c \
        src/mm_qcamera_video.c \
        src/mm_qcamera_preview.c \
        src/mm_qcamera_rdi.c \
        src/mm_qcamera_unit_test.c \
        src/mm_qcamera_dual_test.c \
        src/mm_qcamera_pp.c

LOCAL_C_INCLUDES:=$(LOCAL_PATH)/inc
LOCAL_C_INCLUDES+= \
        $(TARGET_OUT_INTERMEDIATES)/include/mm-still/jpeg \
        $(TARGET_OUT_INTERMEDIATES)/include/mm-camera \
        $(LOCAL_PATH)/../mm-camera-interface/inc \
        $(LOCAL_PATH)/../mm-jpeg-interface/inc \
        $(LOCAL_PATH)/../common \
        $(LOCAL_PATH)/../../../ \

LOCAL_C_INCLUDES+= $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include/media
LOCAL_C_INCLUDES+= $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

ifeq ($(call is-board-platform,msm8960),true)
LOCAL_CFLAGS += -DCAMERA_ION_HEAP_ID=ION_CP_MM_HEAP_ID
else
LOCAL_CFLAGS += -DCAMERA_ION_HEAP_ID=ION_CAMERA_HEAP_ID
endif

LOCAL_SHARED_LIBRARIES:= \
         libcutils libdl

LOCAL_MODULE:= mm-qcamera-app

LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
endif

LOCAL_PATH := $(OLD_LOCAL_PATH)
