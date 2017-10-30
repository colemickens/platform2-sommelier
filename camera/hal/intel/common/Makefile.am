# Copyright (C) 2017-2018 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = \
    libhalcommon.la

ALL_INCLUDES = \
    -I$(top_srcdir) \
    -I$(LIBCAMHAL_DIR)/include/ \
    -I$(top_srcdir)/3a \
    -I$(top_srcdir)/v4l2dev \
    -I$(top_srcdir)/platformdata/ \
    -I$(top_srcdir)/platformdata/gc/ \
    -I$(top_srcdir)/platformdata/metadataAutoGen/Linux/ \
    -I$(top_srcdir)/mediacontroller/ \
    -I$(top_srcdir)/copied_utils/

# source
HAL_COMMON_SRC = \
    copied_utils/utils/Thread.cpp \
    SysCall.cpp \
    v4l2dev/v4l2devicebase.cpp \
    v4l2dev/v4l2subdevice.cpp \
    v4l2dev/v4l2videonode.cpp \
    mediacontroller/MediaEntity.cpp \
    mediacontroller/MediaController.cpp \
    imageProcess/ColorConverter.cpp \
    imageProcess/ImageScalerCore.cpp \
    3a/IntelAFStateMachine.cpp \
    3a/IntelAEStateMachine.cpp \
    3a/IntelAWBStateMachine.cpp \
    3a/Intel3aPlus.cpp \
    3a/Intel3aCore.cpp \
    3a/Intel3aHelper.cpp \
    copied_utils/camera/CameraMetadata.cpp \
    copied_utils/system/camera_metadata.cpp \
    platformdata/gc/GraphConfigManager.cpp \
    platformdata/gc/GraphConfig.cpp \
    platformdata/gc/FormatUtils.cpp \
    CommonBuffer.cpp \
    CameraWindow.cpp \
    PerformanceTraces.cpp \
    LogHelper.cpp

libhalcommon_la_SOURCES = $(HAL_COMMON_SRC)

#cpphacks
CPPHACKS = -std=c++11 \
    -DPAGESIZE=4096 \
    -DCAMERA_HAL_DEBUG

libhalcommon_la_CPPFLAGS = $(CPPHACKS)

#Namespace Declaration
libhalcommon_la_CPPFLAGS += -DNAMESPACE_DECLARATION=namespace\ icamera
libhalcommon_la_CPPFLAGS += -DNAMESPACE_DECLARATION_END=//icamera
libhalcommon_la_CPPFLAGS += -DUSING_DECLARED_NAMESPACE=using\ namespace\ icamera

libhalcommon_la_CPPFLAGS += $(ALL_INCLUDES)
libhalcommon_la_CPPFLAGS += $(IA_IMAGING_CFLAGS)
libhalcommon_la_CPPFLAGS += $(LIBIACSS_CFLAGS)
libhalcommon_la_CPPFLAGS += $(LIBCAMERANVM_CFLAGS)

libhalcommon_includedir = $(includedir)
libhalcommon_utils_includedir = $(includedir)/utils
libhalcommon_cutils_includedir = $(includedir)/cutils
libhalcommon_system_includedir = $(includedir)/system
libhalcommon_camera_includedir = $(includedir)/camera
libhalcommon_utils_include_HEADERS = copied_utils/utils/*.h
libhalcommon_cutils_include_HEADERS = copied_utils/cutils/*.h
libhalcommon_system_include_HEADERS = copied_utils/system/*.h
libhalcommon_camera_include_HEADERS = copied_utils/camera/*.h
libhalcommon_include_HEADERS = \
    LogHelper.h \
    LogHelperChrome.h \
    CommonBuffer.h \
    Utils.h \
    AtomFifo.h \
    ItemPool.h \
    3ATypes.h \
    ISYSTypes.h \
    MemoryDumper.h \
    PollerThread.h \
    Camera3V4l2Format.h \
    PerformanceTraces.h \
    SharedItemPool.h \
    StreamBase.h \
    TracedSP.h \
    UtilityMacros.h \
    CameraWindow.h \
    platformdata/Metadata.h \
    platformdata/CameraConf.h \
    platformdata/CameraMetadataHelper.h \
    platformdata/CameraProfiles.h \
    platformdata/PlatformData.h \
    platformdata/IPSLConfParser.h \
    platformdata/gc/FormatUtils.h \
    platformdata/gc/GraphConfig.h \
    platformdata/gc/GraphConfigManager.h \
    platformdata/gc/platform_gcss_keys.h \
    mediacontroller/MediaCtlPipeConfig.h \
    mediacontroller/MediaEntity.h \
    mediacontroller/MediaController.h \
    imageProcess/ColorConverter.h \
    imageProcess/ImageScalerCore.h \
    3a/Intel3aControls.h \
    3a/IntelAFStateMachine.h \
    3a/IntelAEStateMachine.h \
    3a/IntelAWBStateMachine.h \
    3a/Intel3aPlus.h \
    3a/Intel3aCore.h \
    3a/Intel3aHelper.h \
    CommonUtilMacros.h

pkgconfiglibdir = $(libdir)/pkgconfig
pkgconfiglib_DATA = libhalcommon.pc

.PHONY: rpm
rpm:
	./rpm/build_rpm.sh $(DESTDIR)
