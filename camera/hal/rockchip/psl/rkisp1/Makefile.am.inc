# Copyright (C) 2017 Intel Corporation.
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

PSLSRC = \
    psl/rkisp1/Rk3aRunner.cpp \
    psl/rkisp1/GraphConfigManager.cpp \
    psl/rkisp1/PSLConfParser.cpp \
    psl/rkisp1/RKISP1CameraCapInfo.cpp \
    psl/rkisp1/GraphConfig.cpp \
    psl/rkisp1/RKISP1Common.cpp \
    psl/rkisp1/RKISP1CameraHw.cpp \
    psl/rkisp1/HwStreamBase.cpp \
    psl/rkisp1/SyncManager.cpp \
    psl/rkisp1/SensorHwOp.cpp \
    psl/rkisp1/LensHw.cpp \
    psl/rkisp1/CameraBuffer.cpp \
    psl/rkisp1/ControlUnit.cpp \
    psl/rkisp1/ImguUnit.cpp \
    psl/rkisp1/SettingsProcessor.cpp \
    psl/rkisp1/Metadata.cpp \
    psl/rkisp1/tasks/ITaskEventSource.cpp \
    psl/rkisp1/tasks/ICaptureEventSource.cpp \
    psl/rkisp1/tasks/ITaskEventListener.cpp \
    psl/rkisp1/tasks/JpegEncodeTask.cpp \
    psl/rkisp1/workers/FrameWorker.cpp \
    psl/rkisp1/workers/OutputFrameWorker.cpp \
    psl/rkisp1/workers/StatisticsWorker.cpp \
    psl/rkisp1/workers/ParameterWorker.cpp \
    psl/rkisp1/MediaCtlHelper.cpp \
    common/platformdata/gc/FormatUtils.cpp \
    psl/rkisp1/NodeTypes.cpp

PSLCPPFLAGS = \
    $(STRICTED_CPPFLAGS) \
    -I$(top_srcdir)/common/platformdata/metadataAutoGen/6.0.1 \
    -I$(top_srcdir)/psl/rkisp1 \
    $(LIBUTILS_CFLAGS) \
    -I$(top_srcdir)/include \
    -I$(top_srcdir)/include/ia_imaging \
    -I$(top_srcdir)/include/rk_imaging \
    -DCAMERA_RKISP1_SUPPORT \
    -DHAL_PIXEL_FORMAT_NV12_LINEAR_CAMERA_RK=0x10F

if REMOTE_3A_SERVER
PSLCPPFLAGS += \
    -DREMOTE_3A_SERVER \
    -I$(top_srcdir)/psl/rkisp1/ipc

PSLSRC += \
    psl/rkisp1/ipc/client/Rockchip3AClient.cpp \
    psl/rkisp1/ipc/IPCCommon.cpp \
    psl/rkisp1/ipc/IPCAiq.cpp

libcamerahal_la_LDFLAGS += -ldl
libcamerahal_la_LDFLAGS += -lrt
endif

if !REMOTE_3A_SERVER
# libmfldadvci shared libraries
libcamerahal_la_LDFLAGS += \
    -lrk_aiq
endif

if REMOTE_3A_SERVER
lib_LTLIBRARIES += libcam_algo.la
libcam_algo_la_SOURCES = \
    psl/rkisp1/ipc/server/Rockchip3AServer.cpp \
    psl/rkisp1/ipc/server/AiqLibrary.cpp \
    common/LogHelper.cpp \
    psl/rkisp1/ipc/IPCCommon.cpp \
    psl/rkisp1/ipc/IPCAiq.cpp

libcam_algo_la_CPPFLAGS = \
    $(STRICTED_CPPFLAGS) \
    -I$(top_srcdir)/common \
    -I$(top_srcdir)/psl/rkisp1 \
    -I$(top_srcdir)/psl/rkisp1/ipc \
    -I$(top_srcdir)/include \
    -I$(top_srcdir)/include/ia_imaging \
    -I$(top_srcdir)/include/rk_imaging \
    -I$(ROOT)/usr/include/android/system/core/include \
    -I$(ROOT)/usr/include/base-$(BASE_VER) \
    $(LIBUTILS_CFLAGS) \
    -D__USE_ANDROID_METADATA__ \
    -DCAMERA_HAL_DEBUG \
    -DREMOTE_3A_SERVER

libcam_algo_la_CPPFLAGS += -DNAMESPACE_DECLARATION=namespace\ android\ {\namespace\ camera2
libcam_algo_la_CPPFLAGS += -DNAMESPACE_DECLARATION_END=}
libcam_algo_la_CPPFLAGS += -DUSING_DECLARED_NAMESPACE=using\ namespace\ android::camera2

libcam_algo_la_LDFLAGS = \
    -lrk_aiq \
    $(LIBCHROME_LIBS) \
    $(LIBMOJO_LIBS) \
    $(LIBUTILS_LIBS)

endif

