ifeq ($(call is-vendor-board-platform,QCOM),true)
OLD_LOCAL_PATH := $(LOCAL_PATH)
LOCAL_PATH:=$(call my-dir)

# Build command line test app: mm-qcamera-app
include $(CLEAR_VARS)

LOCAL_CFLAGS:= \
        -DAMSS_VERSION=$(AMSS_VERSION) \
        $(mmcamera_debug_defines) \
        $(mmcamera_debug_cflags) \
        $(USE_SERVER_TREE)

ifeq ($(strip $(TARGET_USES_ION)),true)
LOCAL_CFLAGS += -DUSE_ION
endif

ifneq ($(call is-platform-sdk-version-at-least,20),true)
LOCAL_CFLAGS += -DUSE_KK_CODE
endif


LOCAL_CFLAGS += -D_ANDROID_

LOCAL_SRC_FILES:= \
        src/mm_qcamera_main_menu.c \
        src/mm_qcamera_app.c \
        src/mm_qcamera_unit_test.c \
        src/mm_qcamera_video.c \
        src/mm_qcamera_preview.c \
        src/mm_qcamera_snapshot.c \
        src/mm_qcamera_rdi.c \
        src/mm_qcamera_reprocess.c\
        src/mm_qcamera_queue.c \
        src/mm_qcamera_socket.c \
        src/mm_qcamera_commands.c
#        src/mm_qcamera_dual_test.c \

LOCAL_C_INCLUDES:=$(LOCAL_PATH)/inc
LOCAL_C_INCLUDES+= \
        frameworks/native/include/media/openmax \
        $(LOCAL_PATH)/../common \
        $(LOCAL_PATH)/../../../mm-image-codec/qexif \
        $(LOCAL_PATH)/../../../mm-image-codec/qomx_core

LOCAL_C_INCLUDES+= $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include/media
LOCAL_C_INCLUDES+= $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

LOCAL_CFLAGS += -DCAMERA_ION_HEAP_ID=ION_IOMMU_HEAP_ID
ifeq ($(call is-board-platform,msm8974),true)
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_HEAP_ID=GRALLOC_USAGE_PRIVATE_IOMMU_HEAP
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_FALLBACK_HEAP_ID=GRALLOC_USAGE_PRIVATE_IOMMU_HEAP
        LOCAL_CFLAGS += -DCAMERA_ION_FALLBACK_HEAP_ID=ION_IOMMU_HEAP_ID
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_CACHING_ID=0
        LOCAL_CFLAGS += -DNUM_RECORDING_BUFFERS=9
else ifeq ($(call is-board-platform,msm8226),true)
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_HEAP_ID=GRALLOC_USAGE_PRIVATE_MM_HEAP
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_FALLBACK_HEAP_ID=GRALLOC_USAGE_PRIVATE_IOMMU_HEAP
        LOCAL_CFLAGS += -DCAMERA_ION_FALLBACK_HEAP_ID=ION_IOMMU_HEAP_ID
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_CACHING_ID=0
        LOCAL_CFLAGS += -DNUM_RECORDING_BUFFERS=9
else ifeq ($(call is-board-platform,msm8916),true)
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_HEAP_ID=GRALLOC_USAGE_PRIVATE_MM_HEAP
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_FALLBACK_HEAP_ID=GRALLOC_USAGE_PRIVATE_IOMMU_HEAP
        LOCAL_CFLAGS += -DCAMERA_ION_FALLBACK_HEAP_ID=ION_IOMMU_HEAP_ID
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_CACHING_ID=0
        LOCAL_CFLAGS += -DNUM_RECORDING_BUFFERS=9
else ifeq ($(call is-board-platform,msm8610),true)
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_HEAP_ID=GRALLOC_USAGE_PRIVATE_MM_HEAP
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_FALLBACK_HEAP_ID=GRALLOC_USAGE_PRIVATE_IOMMU_HEAP
        LOCAL_CFLAGS += -DCAMERA_ION_FALLBACK_HEAP_ID=ION_IOMMU_HEAP_ID
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_CACHING_ID=0
        LOCAL_CFLAGS += -DNUM_RECORDING_BUFFERS=9
else ifeq ($(call is-board-platform,msm8960),true)
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_HEAP_ID=GRALLOC_USAGE_PRIVATE_IOMMU_HEAP
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_FALLBACK_HEAP_ID=GRALLOC_USAGE_PRIVATE_IOMMU_HEAP
        LOCAL_CFLAGS += -DCAMERA_ION_FALLBACK_HEAP_ID=ION_IOMMU_HEAP_ID
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_CACHING_ID=0
        LOCAL_CFLAGS += -DNUM_RECORDING_BUFFERS=5
else ifeq ($(call is-chipset-prefix-in-board-platform,msm8660),true)
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_HEAP_ID=GRALLOC_USAGE_PRIVATE_CAMERA_HEAP
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_FALLBACK_HEAP_ID=GRALLOC_USAGE_PRIVATE_IOMMU_HEAP # Don't Care
        LOCAL_CFLAGS += -DCAMERA_ION_FALLBACK_HEAP_ID=ION_IOMMU_HEAP_ID # EBI
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_CACHING_ID=0
        LOCAL_CFLAGS += -DNUM_RECORDING_BUFFERS=5
else
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_HEAP_ID=GRALLOC_USAGE_PRIVATE_CAMERA_HEAP
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_FALLBACK_HEAP_ID=GRALLOC_USAGE_PRIVATE_CAMERA_HEAP # Don't Care
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_CACHING_ID=GRALLOC_USAGE_PRIVATE_UNCACHED #uncached
        LOCAL_CFLAGS += -DCAMERA_ION_FALLBACK_HEAP_ID=ION_CAMERA_HEAP_ID
        LOCAL_CFLAGS += -DNUM_RECORDING_BUFFERS=5
endif
LOCAL_CFLAGS += -Wall -Wextra -Werror

LOCAL_SHARED_LIBRARIES:= \
         libcutils libdl libmmcamera_interface

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= mm-qcamera-app
include $(BUILD_EXECUTABLE)
endif

ifeq ($(call is-vendor-board-platform,QCOM),true)
# Build tuning library
include $(CLEAR_VARS)

LOCAL_CFLAGS:= \
        -DAMSS_VERSION=$(AMSS_VERSION) \
        $(mmcamera_debug_defines) \
        $(mmcamera_debug_cflags) \
        $(USE_SERVER_TREE)

ifeq ($(strip $(TARGET_USES_ION)),true)
LOCAL_CFLAGS += -DUSE_ION
endif

LOCAL_CFLAGS += -D_ANDROID_

LOCAL_SRC_FILES:= \
        src/mm_qcamera_main_menu.c \
        src/mm_qcamera_app.c \
        src/mm_qcamera_unit_test.c \
        src/mm_qcamera_video.c \
        src/mm_qcamera_preview.c \
        src/mm_qcamera_snapshot.c \
        src/mm_qcamera_rdi.c \
        src/mm_qcamera_reprocess.c\
        src/mm_qcamera_queue.c \
        src/mm_qcamera_socket.c \
        src/mm_qcamera_commands.c
#        src/mm_qcamera_dual_test.c \

LOCAL_C_INCLUDES:=$(LOCAL_PATH)/inc
LOCAL_C_INCLUDES+= \
        frameworks/native/include/media/openmax \
        $(LOCAL_PATH)/../common \
        $(LOCAL_PATH)/../../../mm-image-codec/qexif \
        $(LOCAL_PATH)/../../../mm-image-codec/qomx_core

LOCAL_C_INCLUDES+= $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include/media
LOCAL_C_INCLUDES+= $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr/include
LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ/usr

LOCAL_CFLAGS += -DCAMERA_ION_HEAP_ID=ION_IOMMU_HEAP_ID
ifeq ($(call is-board-platform,msm8974),true)
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_HEAP_ID=GRALLOC_USAGE_PRIVATE_IOMMU_HEAP
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_FALLBACK_HEAP_ID=GRALLOC_USAGE_PRIVATE_IOMMU_HEAP
        LOCAL_CFLAGS += -DCAMERA_ION_FALLBACK_HEAP_ID=ION_IOMMU_HEAP_ID
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_CACHING_ID=0
        LOCAL_CFLAGS += -DNUM_RECORDING_BUFFERS=9
else ifeq ($(call is-board-platform,msm8226),true)
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_HEAP_ID=GRALLOC_USAGE_PRIVATE_MM_HEAP
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_FALLBACK_HEAP_ID=GRALLOC_USAGE_PRIVATE_IOMMU_HEAP
        LOCAL_CFLAGS += -DCAMERA_ION_FALLBACK_HEAP_ID=ION_IOMMU_HEAP_ID
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_CACHING_ID=0
        LOCAL_CFLAGS += -DNUM_RECORDING_BUFFERS=9
else ifeq ($(call is-board-platform,msm8916),true)
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_HEAP_ID=GRALLOC_USAGE_PRIVATE_MM_HEAP
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_FALLBACK_HEAP_ID=GRALLOC_USAGE_PRIVATE_IOMMU_HEAP
        LOCAL_CFLAGS += -DCAMERA_ION_FALLBACK_HEAP_ID=ION_IOMMU_HEAP_ID
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_CACHING_ID=0
        LOCAL_CFLAGS += -DNUM_RECORDING_BUFFERS=9
else ifeq ($(call is-board-platform,msm8610),true)
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_HEAP_ID=GRALLOC_USAGE_PRIVATE_MM_HEAP
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_FALLBACK_HEAP_ID=GRALLOC_USAGE_PRIVATE_IOMMU_HEAP
        LOCAL_CFLAGS += -DCAMERA_ION_FALLBACK_HEAP_ID=ION_IOMMU_HEAP_ID
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_CACHING_ID=0
        LOCAL_CFLAGS += -DNUM_RECORDING_BUFFERS=9
else ifeq ($(call is-board-platform,msm8960),true)
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_HEAP_ID=GRALLOC_USAGE_PRIVATE_IOMMU_HEAP
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_FALLBACK_HEAP_ID=GRALLOC_USAGE_PRIVATE_IOMMU_HEAP
        LOCAL_CFLAGS += -DCAMERA_ION_FALLBACK_HEAP_ID=ION_IOMMU_HEAP_ID
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_CACHING_ID=0
        LOCAL_CFLAGS += -DNUM_RECORDING_BUFFERS=5
else ifeq ($(call is-chipset-prefix-in-board-platform,msm8660),true)
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_HEAP_ID=GRALLOC_USAGE_PRIVATE_CAMERA_HEAP
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_FALLBACK_HEAP_ID=GRALLOC_USAGE_PRIVATE_IOMMU_HEAP # Don't Care
        LOCAL_CFLAGS += -DCAMERA_ION_FALLBACK_HEAP_ID=ION_IOMMU_HEAP_ID # EBI
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_CACHING_ID=0
        LOCAL_CFLAGS += -DNUM_RECORDING_BUFFERS=5
else
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_HEAP_ID=GRALLOC_USAGE_PRIVATE_CAMERA_HEAP
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_FALLBACK_HEAP_ID=GRALLOC_USAGE_PRIVATE_CAMERA_HEAP # Don't Care
        LOCAL_CFLAGS += -DCAMERA_GRALLOC_CACHING_ID=GRALLOC_USAGE_PRIVATE_UNCACHED #uncached
        LOCAL_CFLAGS += -DCAMERA_ION_FALLBACK_HEAP_ID=ION_CAMERA_HEAP_ID
        LOCAL_CFLAGS += -DNUM_RECORDING_BUFFERS=5
endif
LOCAL_CFLAGS += -Wall -Wextra -Werror

LOCAL_SHARED_LIBRARIES:= \
         libcutils libdl libmmcamera_interface

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= libmm-qcamera
include $(BUILD_SHARED_LIBRARY)
endif
