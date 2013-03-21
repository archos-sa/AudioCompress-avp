LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := compress.c

LOCAL_C_INCLUDES := src

ifeq ($(TARGET_ARCH_ABI),armeabi)
LOCAL_MODULE := audiocompress_no_neon
else
LOCAL_MODULE := audiocompress
endif

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
