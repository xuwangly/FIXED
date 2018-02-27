LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_LDLIBS    := -lm -llog
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := NvramClient.cpp
LOCAL_SHARED_LIBRARIES := libutils libbinder libnvram libfile_op liblog
LOCAL_C_INCLUDES := $(MTK_PATH_SOURCE)/external/nvram/libnvram \
					$(MTK_PATH_SOURCE)/kernel/include/ \
					$(MTK_PATH_SOURCE)/external/nvram/libfile_op \
					$(MTK_PATH_CUSTOM)/cgen/inc $(MTK_PATH_CUSTOM)/cgen/cfgfileinc \
					$(MTK_PATH_CUSTOM)/cgen/cfgdefault
LOCAL_MODULE := libNvramClient
include $(BUILD_SHARED_LIBRARY)