LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
  
LOCAL_SRC_FILES:=	nvram_server.cpp  \
					../service/NvramService.cpp
LOCAL_SHARED_LIBRARIES:=libutils libbinder libnvram libfile_op liblog libcutils
LOCAL_C_INCLUDES := $(MTK_PATH_SOURCE)/external/nvram/libnvram \
					$(MTK_PATH_SOURCE)/kernel/include/ \
					$(MTK_PATH_SOURCE)/external/nvram/libfile_op \
					$(MTK_PATH_CUSTOM)/cgen/inc $(MTK_PATH_CUSTOM)/cgen/cfgfileinc \
					$(MTK_PATH_CUSTOM)/cgen/cfgdefault
#libJACK_Service   
LOCAL_MODULE_TAGS:=optional  
LOCAL_MODULE:= nvramserver  
include $(BUILD_EXECUTABLE)