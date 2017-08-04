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
    psl/ipu3/tasks/ExecuteTaskBase.cpp \
    psl/ipu3/tasks/ITaskEventSource.cpp \
    psl/ipu3/tasks/ICaptureEventSource.cpp \
    psl/ipu3/tasks/TaskThreadBase.cpp \
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
    -I$(top_srcdir)/common/platformdata/metadataAutoGen/6.0.1 \
    -I$(top_srcdir)/psl/ipu3 \
    $(LIBUTILS_CFLAGS) \
    -I$(top_srcdir)/include \
    -I$(top_srcdir)/include/ia_imaging \
    -DCAMERA_IPU3_SUPPORT \
    -DHAL_PIXEL_FORMAT_NV12_LINEAR_CAMERA_INTEL=0x10F \
    -DMACRO_KBL_AIC

if !REMOTE_3A_SERVER
PSLSRC += \
    psl/ipu3/SkyCamLocalProxy.cpp
else
PSLCPPFLAGS += \
    -DREMOTE_3A_SERVER \
    -I$(top_srcdir)/psl/ipu3/ipc

PSLSRC += \
    psl/ipu3/ipc/client/ia_aiq_ipc.cpp \
    psl/ipu3/ipc/client/ia_cmc_ipc.cpp \
    psl/ipu3/ipc/client/ia_exc_ipc.cpp \
    psl/ipu3/ipc/client/ia_mkn_ipc.cpp \
    psl/ipu3/ipc/client/ia_coordinate_ipc.cpp \
    psl/ipu3/ipc/client/SkyCamSocketProxy.cpp \
    psl/ipu3/ipc/client/SocketHandler.cpp \
    psl/ipu3/ipc/client/IPCMessenger.cpp \
    psl/ipu3/ipc/client/ia_log.cpp
endif

PSLLIBS = \
    -lSkyCamAICKBL \
    -lStatsConverter

if !REMOTE_3A_SERVER
# libmfldadvci shared libraries
libcamerahal_la_LDFLAGS += \
    -lia_cmc_parser \
    -lia_aiq \
    -lia_exc \
    -lia_log \
    -lia_mkn \
    -lia_coordinate
endif

if REMOTE_3A_SERVER
bin_PROGRAMS = intel3aserver
intel3aserver_SOURCES = \
    psl/ipu3/IPU3ISPPipe.cpp \
    psl/ipu3/RuntimeParamsHelper.cpp \
    psl/ipu3/ipc/server/Intel3aServer.cpp \
    psl/ipu3/ipc/server/AicLibrary.cpp \
    psl/ipu3/ipc/server/CmcLibrary.cpp \
    psl/ipu3/ipc/server/AiqLibrary.cpp \
    psl/ipu3/ipc/server/MknLibrary.cpp \
    psl/ipu3/ipc/server/CoordinateLibrary.cpp \
    psl/ipu3/ipc/server/ExcLibrary.cpp \
    psl/ipu3/ipc/server/cca_logger_ipc.cpp \
    common/LogHelper.cpp \
    common/LogHelperChrome.cpp

intel3aserver_CPPFLAGS = -std=c++11 \
    -I$(top_srcdir)/common \
    -I$(top_srcdir)/psl/ipu3 \
    -I$(top_srcdir)/psl/ipu3/ipc \
    $(LIBUTILS_CFLAGS) \
    $(IA_IMAGING_CFLAGS) \
    -D__USE_ANDROID_METADATA__ \
    -DMACRO_KBL_AIC \
    -DCAMERA_HAL_DEBUG \
    -DREMOTE_3A_SERVER

intel3aserver_LDFLAGS = \
    -lSkyCamAICKBL \
    -lia_cmc_parser \
    -lia_aiq \
    -lia_exc \
    -lia_mkn \
    -lia_log \
    -lia_coordinate \
    $(LIBUTILS_LIBS)
endif
