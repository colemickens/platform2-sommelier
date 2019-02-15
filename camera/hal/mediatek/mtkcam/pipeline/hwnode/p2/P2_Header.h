/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_HEADER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_HEADER_H_

#include <list>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>
#include <mtkcam/def/common.h>
#include <mtkcam/aaa/IHal3A.h>
#include <mtkcam/drv/def/Dip_Notify_datatype.h>
#include <mtkcam/drv/iopipe/INormalStream.h>
#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/custom/ExifFactory.h>
#include <mtkcam/feature/lmv/lmv_ext.h>
#include <mtkcam/feature/eis/eis_type.h>
#include <mtkcam/pipeline/pipeline/IPipelineNode.h>
#include <mtkcam/utils/exif/DebugExifUtils.h>
#include <mtkcam/utils/hw/HwTransform.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <mtkcam/utils/metadata/IMetadata.h>
#include <mtkcam/utils/metastore/IMetadataProvider.h>
#include <mtkcam/utils/std/common.h>
#include <mtkcam/utils/std/DebugScanLine.h>
#include <mtkcam/feature/utils/p2/P2Trace.h>
#include <mtkcam/utils/std/DebugDrawID.h>
#include <camera_custom_eis.h>
#include <mtkcam/utils/std/ILogger.h>
#include <mtkcam/feature/utils/p2/P2Pack.h>
#include <mtkcam/feature/utils/p2/P2Util.h>

#pragma push_macro("LOG_TAG")
#undef LOG_TAG
#define LOG_TAG "MtkCam/P2"
#include "../MyUtils.h"
#pragma pop_macro("LOG_TAG")

#include <mtkcam/utils/debug/P2_DebugControl.h>

typedef NSCam::NSIoPipe::FrameParams QFRAME_T;
using NSCam::NSIoPipe::ExtraParam;
using NSCam::NSIoPipe::FrameParams;
using NSCam::NSIoPipe::Input;
using NSCam::NSIoPipe::MCropRect;
using NSCam::NSIoPipe::MCrpRsInfo;
using NSCam::NSIoPipe::Output;
using NSCam::NSIoPipe::QParams;

using NSCam::v4l2::ENormalStreamTag;
using NSCam::v4l2::ENormalStreamTag_Prv;
using NSCam::v4l2::ESDCmd_CONFIG_VENC_DIRLK;
using NSCam::v4l2::ESDCmd_RELEASE_VENC_DIRLK;
using NSCam::v4l2::INormalStream;

using NSCam::NSIoPipe::EDIPHWVersion_40;
using NSCam::NSIoPipe::EDIPHWVersion_50;
using NSCam::NSIoPipe::EDIPINFO_DIPVERSION;
using NSCam::NSIoPipe::EDIPInfoEnum;

using NSCam::NSIoPipe::EPIPE_MDP_PQPARAM_CMD;
using NSCam::NSIoPipe::ModuleInfo;
using NSCam::NSIoPipe::PQParam;

using NSCam::DebugExifUtils;
using NSCam::eTransform_ROT_90;
using NSCam::IImageBuffer;
using NSCam::IImageBufferHeap;
using NSCam::IMetadata;
using NSCam::IMetadataProvider;
using NSCam::MPoint;
using NSCam::MPointF;
using NSCam::MRect;
using NSCam::MRectF;
using NSCam::MSize;
using NSCam::MSizeF;
using NSCam::SENSOR_VHDR_MODE_NONE;
using NSCam::Type2Type;

using NSCam::Utils::Sync::IFence;

using NSCam::v3::div_round;
using NSCam::v3::IImageStreamBuffer;
using NSCam::v3::IImageStreamInfo;
using NSCam::v3::IMetaStreamBuffer;
using NSCam::v3::IMetaStreamInfo;
using NSCam::v3::IPipelineFrame;
using NSCam::v3::IPipelineNode;
using NSCam::v3::IPipelineNodeCallback;
using NSCam::v3::IStreamBuffer;
using NSCam::v3::IStreamBufferSet;
using NSCam::v3::IStreamInfo;
using NSCam::v3::IStreamInfoSet;
using NSCam::v3::IUsersManager;
using NSCam::v3::OpaqueReprocUtil;
using NSCam::v3::simpleTransform;
using NSCam::v3::STREAM_BUFFER_STATUS;
using NSCam::v3::StreamId_T;
using NSCam::v3::vector_f;

using NSCam::NSIoPipe::EPortCapbility;
using NSCam::NSIoPipe::EPortCapbility_Disp;
using NSCam::NSIoPipe::EPortCapbility_None;
using NSCam::NSIoPipe::EPortCapbility_Rcrd;
using NSCam::NSIoPipe::PORT_DEPI;
using NSCam::NSIoPipe::PORT_DMGI;
using NSCam::NSIoPipe::PORT_IMG2O;
using NSCam::NSIoPipe::PORT_IMG3O;
using NSCam::NSIoPipe::PORT_IMGBI;
using NSCam::NSIoPipe::PORT_IMGCI;
using NSCam::NSIoPipe::PORT_IMGI;
using NSCam::NSIoPipe::PORT_LCEI;
using NSCam::NSIoPipe::PORT_WDMAO;
using NSCam::NSIoPipe::PORT_WROTO;

using NSCamHW::HwMatrix;
using NSCamHW::HwTransHelper;

using NS3Av3::IHal3A;
using NS3Av3::MetaSet_T;
using NS3Av3::TuningParam;

using NSCam::Feature::P2Util::Cropper;
using NSCam::Utils::ILog;
using NSCam::Utils::spToILog;

using NSCam::Feature::P2Util::CROP_IMG2O;
using NSCam::Feature::P2Util::CROP_IMG3O;
using NSCam::Feature::P2Util::CROP_IMGO;
using NSCam::Feature::P2Util::CROP_WDMAO;
using NSCam::Feature::P2Util::CROP_WROTO;
using NSCam::Feature::P2Util::DMACONSTRAIN_2BYTEALIGN;
using NSCam::Feature::P2Util::DMACONSTRAIN_NONE;
using NSCam::Feature::P2Util::DMACONSTRAIN_NOSUBPIXEL;
using NSCam::Feature::P2Util::getCropRect;
using NSCam::Feature::P2Util::LMVInfo;
using NSCam::Feature::P2Util::P2_CAPTURE;
using NSCam::Feature::P2Util::P2_DUMMY;
using NSCam::Feature::P2Util::P2_DUMP_DEBUG;
using NSCam::Feature::P2Util::P2_DUMP_NDD;
using NSCam::Feature::P2Util::P2_DUMP_NONE;
using NSCam::Feature::P2Util::P2_HS_VIDEO;
using NSCam::Feature::P2Util::P2_PHOTO;
using NSCam::Feature::P2Util::P2_PREVIEW;
using NSCam::Feature::P2Util::P2_TIMESHARE_CAPTURE;
using NSCam::Feature::P2Util::P2_UNKNOWN;
using NSCam::Feature::P2Util::P2_VIDEO;
using NSCam::Feature::P2Util::P2ConfigInfo;
using NSCam::Feature::P2Util::P2Data;
using NSCam::Feature::P2Util::P2DataObj;
using NSCam::Feature::P2Util::P2DumpType;
using NSCam::Feature::P2Util::P2Flag;
using NSCam::Feature::P2Util::P2FrameData;
using NSCam::Feature::P2Util::P2Info;
using NSCam::Feature::P2Util::P2InfoObj;
using NSCam::Feature::P2Util::P2IO;
using NSCam::Feature::P2Util::P2IOPack;
using NSCam::Feature::P2Util::P2Obj;
using NSCam::Feature::P2Util::P2ObjPtr;
using NSCam::Feature::P2Util::P2Pack;
using NSCam::Feature::P2Util::P2PlatInfo;
using NSCam::Feature::P2Util::P2SensorData;
using NSCam::Feature::P2Util::P2SensorInfo;
using NSCam::Feature::P2Util::P2Type;
using NSCam::Feature::P2Util::P2UsageHint;
using NSCam::Feature::P2Util::toCapability;

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_HEADER_H_
