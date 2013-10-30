LOCAL_PATH:= $(call my-dir)
# HAL module implemenation, not prelinked and stored in
# hw/<COPYPIX_HARDWARE_MODULE_ID>.<ro.board.platform>.so
include $(CLEAR_VARS)

LOCAL_SRC_FILES := lights.c

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_SHARED_LIBRARIES := liblog

LOCAL_MODULE := lights.$(TARGET_DEVICE)
LOCAL_MODULE_TAGS := optional

ifneq (,$(filter userdebug eng,$(TARGET_BUILD_VARIANT)))
LOCAL_CFLAGS += -DALLOW_PERSIST_BRIGHTNESS_OVERRIDE=1
LOCAL_SHARED_LIBRARIES += libcutils
endif

include $(BUILD_SHARED_LIBRARY)
