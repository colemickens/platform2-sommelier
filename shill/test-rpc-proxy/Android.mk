LOCAL_PATH := $(call my-dir)
ifeq ($(HOST_OS), linux)

include $(CLEAR_VARS)
LOCAL_MODULE := shill-test-rpc-proxy

LOCAL_CPP_EXTENSION := .cc
LOCAL_CLANG := true
LOCAL_RTTI_FLAG := -frtti
LOCAL_CPPFLAGS := \
    -Werror \
    -Wextra\
    -Wno-unused-parameter \
    -fno-strict-aliasing \
    -Woverloaded-virtual \
    -Wno-sign-promo \
    -Wno-missing-field-initializers

LOCAL_C_INCLUDES := \
    external/cros/system_api/dbus \
    external/gtest/include \
    external/xmlrpcpp/src \
    system/connectivity \

LOCAL_SHARED_LIBRARIES := \
    libchrome \
    libchrome-dbus \
    libchromeos \
    libchromeos-dbus \
    libshill-client \
    libxmlrpc++

proxy_src_files := \
    $(wildcard $(LOCAL_PATH)/*.cc)
LOCAL_SRC_FILES := \
    $(proxy_src_files:$(LOCAL_PATH)/%=%)

include $(BUILD_EXECUTABLE)
endif # HOST_OS == linux
