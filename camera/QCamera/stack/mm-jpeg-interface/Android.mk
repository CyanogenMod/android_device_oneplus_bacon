OLD_LOCAL_PATH := $(LOCAL_PATH)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS+= -D_ANDROID_
LOCAL_COPY_HEADERS_TO := mm-camera-interface
LOCAL_COPY_HEADERS = inc/mm_jpeg_interface.h

LOCAL_C_INCLUDES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include/media
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/inc \
    $(LOCAL_PATH)/../common \
    $(LOCAL_PATH)/../../../ \
    $(TARGET_OUT_HEADERS)/mm-still \
    $(TARGET_OUT_HEADERS)/mm-still/jpeg \
    $(TARGET_OUT_HEADERS)/mm-core/omxcore \
    $(TARGET_OUT_HEADERS)/mm-still/mm-omx

ifeq ($(strip $(TARGET_USES_ION)),true)
    LOCAL_CFLAGS += -DUSE_ION
endif

LOCAL_SRC_FILES := \
    src/mm_jpeg_queue.c \
    src/mm_jpeg.c \
    src/mm_jpeg_interface.c

LOCAL_MODULE           := libmmjpeg_interface
LOCAL_PRELINK_MODULE   := false
LOCAL_SHARED_LIBRARIES := libdl libcutils liblog libmmstillomx libimage-jpeg-enc-omx-comp
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

LOCAL_PATH := $(OLD_LOCAL_PATH)
