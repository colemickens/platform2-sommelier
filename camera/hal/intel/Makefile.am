# Copyright (C) 2017 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = \
    libcamerahal.la

3ASRC = common/3a/Intel3aPlus.cpp \
        common/3a/Intel3aCore.cpp \
        common/3a/Intel3aHelper.cpp \
        common/3a/IntelAEStateMachine.cpp \
        common/3a/IntelAFStateMachine.cpp \
        common/3a/IntelAWBStateMachine.cpp

AALSRC = AAL/Camera3HAL.cpp \
         AAL/Camera3HAL.h \
         AAL/Camera3Request.cpp \
         AAL/Camera3Request.h \
         AAL/CameraStream.cpp \
         AAL/CameraStream.h \
         AAL/CameraStreamNode.h \
         AAL/ICameraHw.cpp \
         AAL/ICameraHw.h \
         AAL/RequestThread.cpp \
         AAL/RequestThread.h \
         AAL/ResultProcessor.cpp \
         AAL/ResultProcessor.h

V4L2SRC = common/v4l2dev/v4l2devicebase.cpp \
          common/v4l2dev/v4l2subdevice.cpp \
          common/v4l2dev/v4l2videonode.cpp

PLATFORMDATASRC = common/platformdata/CameraConf.cpp \
                  common/platformdata/CameraMetadataHelper.cpp \
                  common/platformdata/CameraProfiles.cpp \
                  common/platformdata/ChromeCameraProfiles.cpp \
                  common/platformdata/Metadata.cpp \
                  common/platformdata/PlatformData.cpp \
                  common/platformdata/IPSLConfParser.cpp

MEDIACONTROLLERSRC = common/mediacontroller/MediaController.cpp \
                     common/mediacontroller/MediaEntity.cpp

IMAGEPROCESSSRC = common/imageProcess/ColorConverter.cpp \
                  common/imageProcess/ImageScalerCore.cpp

COMMONSRC = common/SysCall.cpp \
            common/Camera3V4l2Format.cpp \
            common/CameraWindow.cpp \
            common/DebugFrameRate.cpp \
            common/LogHelper.cpp \
            common/LogHelperChrome.cpp \
            common/MessageThread.cpp \
            common/PerformanceTraces.cpp \
            common/PollerThread.cpp \
            common/Utils.cpp \
            common/CommonBuffer.cpp \
            common/IaAtrace.cpp \
            common/GFXFormatLinuxGeneric.cpp

JPEGSRC = common/jpeg/ExifCreater.cpp \
          common/jpeg/EXIFMaker.cpp \
          common/jpeg/EXIFMetaData.cpp \
          common/jpeg/ImgEncoderCore.cpp \
          common/jpeg/ImgEncoder.cpp \
          common/jpeg/JpegMakerCore.cpp \
          common/jpeg/JpegMaker.cpp \
          common/jpeg/SWJpegEncoder.cpp

GCSSSRC = common/gcss/graph_query_manager.cpp \
          common/gcss/gcss_item.cpp \
          common/gcss/gcss_utils.cpp \
          common/gcss/GCSSParser.cpp \
          common/gcss/gcss_formats.cpp

libcamerahal_la_SOURCES = $(3ASRC) \
                          $(AALSRC) \
                          $(V4L2SRC) \
                          $(PLATFORMDATASRC) \
                          $(MEDIACONTROLLERSRC) \
                          $(IMAGEPROCESSSRC) \
                          $(COMMONSRC) \
                          $(JPEGSRC) \
                          $(PSLSRC) \
                          $(GCSSSRC) \
                          Camera3HALModule.cpp

#cpphacks
CPPHACKS = -std=c++11 \
   -D__user= \
   -DPAGESIZE=4096 \
   -DHAVE_IA_TYPES \
   -DCAMERA_HAL_DEBUG \
   -DHAVE_PTHREADS

libcamerahal_la_CPPFLAGS = $(CPPHACKS)

#Namespace Declaration
libcamerahal_la_CPPFLAGS += -DNAMESPACE_DECLARATION=namespace\ android\ {\namespace\ camera2
libcamerahal_la_CPPFLAGS += -DNAMESPACE_DECLARATION_END=}
libcamerahal_la_CPPFLAGS += -DUSING_DECLARED_NAMESPACE=using\ namespace\ android::camera2
libcamerahal_la_CPPFLAGS += $(ARC_CAMERA3_ANDROID_HEADERS_CFLAGS) \
                            $(LIBCAMERA_CLIENT_CFLAGS) \
                            $(LIBCAMERA_METADATA_CFLAGS) \
                            $(LIBCBM_CFLAGS) \
                            $(LIBCHROME_CFLAGS) \
                            $(LIBSYNC_CFLAGS)

#includes
KERNELHEADERS = -I$(top_srcdir)/LAL/ipu3/include/uapi \
                -I$(top_srcdir)/LAL/ipu3/include
ALLINCLUDES = \
    $(PSLCPPFLAGS) \
    $(KERNELHEADERS) \
    -I$(top_srcdir) \
    -I$(top_srcdir)/common \
    -I$(top_srcdir)/common/platformdata \
    -I$(top_srcdir)/common/platformdata/gc \
    -I$(top_srcdir)/common/3a \
    -I$(top_srcdir)/common/mediacontroller \
    -I$(top_srcdir)/common/v4l2dev \
    -I$(top_srcdir)/AAL \
    -I$(top_srcdir)/common/imageProcess \
    -I$(top_srcdir)/common/jpeg \
    -I$(top_srcdir)/common/gcss

libcamerahal_la_CPPFLAGS += $(ALLINCLUDES)

# libs
EXTERNAL_LIBS = -ljpeg -lexpat -lpthread $(LIBCBM_LIBS) $(LIBCHROME_LIBS)
UTIL_LIBS =  $(PSLLIBS) \
          $(IA_IMAGING_LIBS) \
          $(LIBIACSS_LIBS)
CAMERALIBS_LIBS = -lcameranvm
STATIC_LIBS = $(LIBCAMERA_CLIENT_LIBS) \
              $(LIBCAMERA_METADATA_LIBS) \
              $(LIBSYNC_LIBS)
OTHER_LIBS = -ldl
libcamerahal_la_LIBADD = $(EXTERNAL_LIBS) $(UTIL_LIBS) $(CAMERALIBS_LIBS) $(STATIC_LIBS) $(OTHER_LIBS)

libcamerahal_la_LDFLAGS = -Wl,-z,defs

include psl/ipu3/Makefile.am.inc
