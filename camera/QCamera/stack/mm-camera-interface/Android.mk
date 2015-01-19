#ifeq ($(call is-board-platform,msm8960),true)
OLD_LOCAL_PATH := $(LOCAL_PATH)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

MM_CAM_FILES := \
        src/mm_camera_interface.c \
        src/mm_camera_stream.c \
        src/mm_camera_channel.c \
        src/mm_camera.c \
        src/mm_camera_thread.c \
        src/mm_camera_data.c \
        src/mm_camera_sock.c \
        src/mm_camera_helper.c

ifeq ($(strip $(TARGET_USES_ION)),true)
    LOCAL_CFLAGS += -DUSE_ION
endif

LOCAL_CFLAGS += -D_ANDROID_
LOCAL_COPY_HEADERS_TO := mm-camera-interface
LOCAL_COPY_HEADERS := inc/mm_camera_interface.h
LOCAL_COPY_HEADERS += ../common/cam_list.h

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/inc \
    $(LOCAL_PATH)/../common \
    $(LOCAL_PATH)/../../../ \
    $(TARGET_OUT_HEADERS)/mm-camera \
    $(TARGET_OUT_HEADERS)/mm-camera/common

LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_C_INCLUDES += $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include/media
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

# (BEGIN) Need to remove later once dependency on jpeg removed
LOCAL_C_INCLUDES += \
    $(TARGET_OUT_HEADERS)/mm-still \
    $(TARGET_OUT_HEADERS)/mm-still/jpeg \
    $(TARGET_OUT_HEADERS)/mm-core/omxcore \
    $(TARGET_OUT_HEADERS)/mm-still/mm-omx
# (END) Need to remove later once dependency on jpeg removed

LOCAL_C_INCLUDES+= $(call project-path-for,qcom-media)/mm-core/inc
LOCAL_CFLAGS += -include bionic/libc/kernel/common/linux/socket.h
LOCAL_CFLAGS += -include bionic/libc/kernel/common/linux/un.h

LOCAL_SRC_FILES := $(MM_CAM_FILES)

LOCAL_MODULE           := libmmcamera_interface
LOCAL_PRELINK_MODULE   := false
LOCAL_SHARED_LIBRARIES := libdl libcutils liblog
LOCAL_MODULE_TAGS := optional

# (BEGIN) Need to remove later once dependency on jpeg removed
# LOCAL_SHARED_LIBRARIES += libmmjpeg_interface
# (END) Need to remove later once dependency on jpeg removed

include $(BUILD_SHARED_LIBRARY)

LOCAL_PATH := $(OLD_LOCAL_PATH)
#endif
