LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS :=optional
LOCAL_SRC_FILES :=	test.cpp
LOCAL_SHARED_LIBRARIES := libutils libbinder libNvramClient
LOCAL_MODULE := NvramClientTest
include $(BUILD_EXECUTABLE)