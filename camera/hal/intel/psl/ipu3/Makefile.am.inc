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
    psl/ipu3/AAARunner.cpp \
    psl/ipu3/GraphConfigManager.cpp \
    psl/ipu3/PSLConfParser.cpp \
    psl/ipu3/IPU3CameraCapInfo.cpp \
    psl/ipu3/GraphConfig.cpp \
    psl/ipu3/IPU3Common.cpp \
    psl/ipu3/IPU3CameraHw.cpp \
    psl/ipu3/HwStreamBase.cpp \
    psl/ipu3/InputSystem.cpp \
    psl/ipu3/SyncManager.cpp \
    psl/ipu3/SensorHwOp.cpp \
    psl/ipu3/LensHw.cpp \
    psl/ipu3/CameraBuffer.cpp \
    psl/ipu3/CaptureUnit.cpp \
    psl/ipu3/ControlUnit.cpp \
    psl/ipu3/ImguUnit.cpp \
    psl/ipu3/SettingsProcessor.cpp \
    psl/ipu3/BufferPools.cpp \
    psl/ipu3/Metadata.cpp \
    psl/ipu3/tasks/ITaskEventSource.cpp \
    psl/ipu3/tasks/ICaptureEventSource.cpp \
    psl/ipu3/tasks/ITaskEventListener.cpp \
    psl/ipu3/tasks/JpegEncodeTask.cpp \
    psl/ipu3/workers/FrameWorker.cpp \
    psl/ipu3/workers/InputFrameWorker.cpp \
    psl/ipu3/workers/OutputFrameWorker.cpp \
    psl/ipu3/workers/StatisticsWorker.cpp \
    psl/ipu3/workers/ParameterWorker.cpp \
    psl/ipu3/workers/IPU3AicToFwEncoder.cpp \
    psl/ipu3/RuntimeParamsHelper.cpp \
    psl/ipu3/SkyCamProxy.cpp \
    psl/ipu3/IPU3ISPPipe.cpp \
    psl/ipu3/MediaCtlHelper.cpp \
    common/platformdata/gc/FormatUtils.cpp \
    psl/ipu3/statsConverter/ipu3-stats.cpp \
    psl/ipu3/NodeTypes.cpp

PSLCPPFLAGS = \
    $(STRICTED_CPPFLAGS) \
    -I$(top_srcdir)/common/platformdata/metadataAutoGen/6.0.1 \
    -I$(top_srcdir)/psl/ipu3 \
    $(LIBUTILS_CFLAGS) \
    -I$(top_srcdir)/include \
    -I$(top_srcdir)/include/ia_imaging \
    -DCAMERA_IPU3_SUPPORT \
    -DHAL_PIXEL_FORMAT_NV12_LINEAR_CAMERA_INTEL=0x10F \
    -DMACRO_KBL_AIC

PSLCPPFLAGS += \
    -I$(top_srcdir)/psl/ipu3/ipc

PSLSRC += \
    psl/ipu3/ipc/client/SkyCamMojoProxy.cpp \
    psl/ipu3/ipc/client/Intel3AClient.cpp \
    psl/ipu3/ipc/IPCCommon.cpp \
    psl/ipu3/ipc/IPCAic.cpp \
    psl/ipu3/ipc/IPCAiq.cpp \
    psl/ipu3/ipc/IPCCmc.cpp \
    psl/ipu3/ipc/IPCExc.cpp \
    psl/ipu3/ipc/IPCMkn.cpp

libcamerahal_la_LDFLAGS += -lrt
libcamerahal_la_LDFLAGS += -ldl


lib_LTLIBRARIES += libcam_algo.la
libcam_algo_la_SOURCES = \
    psl/ipu3/IPU3ISPPipe.cpp \
    psl/ipu3/RuntimeParamsHelper.cpp \
    psl/ipu3/ipc/server/Intel3AServer.cpp \
    psl/ipu3/ipc/server/AicLibrary.cpp \
    psl/ipu3/ipc/server/CmcLibrary.cpp \
    psl/ipu3/ipc/server/AiqLibrary.cpp \
    psl/ipu3/ipc/server/MknLibrary.cpp \
    psl/ipu3/ipc/server/CoordinateLibrary.cpp \
    psl/ipu3/ipc/server/ExcLibrary.cpp \
    common/LogHelper.cpp \
    psl/ipu3/ipc/IPCCommon.cpp \
    psl/ipu3/ipc/IPCAic.cpp \
    psl/ipu3/ipc/IPCAiq.cpp \
    psl/ipu3/ipc/IPCCmc.cpp \
    psl/ipu3/ipc/IPCExc.cpp \
    psl/ipu3/ipc/IPCMkn.cpp

libcam_algo_la_CPPFLAGS = \
    $(STRICTED_CPPFLAGS) \
    -I$(top_srcdir)/common \
    -I$(top_srcdir)/psl/ipu3 \
    -I$(top_srcdir)/psl/ipu3/ipc \
    -I$(top_srcdir)/include \
    -I$(top_srcdir)/include/ia_imaging \
    -I$(ROOT)/usr/include/android/system/core/include \
    -I$(ROOT)/usr/include/base-$(BASE_VER) \
    $(LIBUTILS_CFLAGS) \
    $(IA_IMAGING_CFLAGS) \
    -D__USE_ANDROID_METADATA__ \
    -DMACRO_KBL_AIC \
    -DCAMERA_HAL_DEBUG

libcam_algo_la_CPPFLAGS += -DNAMESPACE_DECLARATION=namespace\ android\ {\namespace\ camera2
libcam_algo_la_CPPFLAGS += -DNAMESPACE_DECLARATION_END=}
libcam_algo_la_CPPFLAGS += -DUSING_DECLARED_NAMESPACE=using\ namespace\ android::camera2

libcam_algo_la_LDFLAGS = \
    -lSkyCamAICKBL \
    -lStatsConverter \
    -lia_cmc_parser \
    -lia_aiq \
    -lia_exc \
    -lia_mkn \
    -lia_log \
    -lia_coordinate \
    $(LIBCHROME_LIBS) \
    $(LIBMOJO_LIBS) \
    $(LIBUTILS_LIBS)
