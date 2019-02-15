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

#include <mtkcam/custom/ExifFactory.h>
#include <mtkcam/def/common.h>
#include <mtkcam/drv/def/Dip_Notify_datatype.h>
#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/utils/exif/DebugExifUtils.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#if MTK_DP_ENABLE  // check later
#include <DpDataType.h>
#endif
#include <mtkcam/feature/utils/p2/P2Trace.h>
#include <mtkcam/feature/utils/p2/P2Util.h>
#define ILOG_MODULE_TAG P2Util
#include <mtkcam/utils/std/ILogHeader.h>

#include <map>
#include <memory>

using NS3Av3::IHal3A;
using NS3Av3::MetaSet_T;
using NS3Av3::TuningParam;
using NSCam::NSIoPipe::EPortCapbility;
using NSCam::NSIoPipe::EPortCapbility_Disp;
using NSCam::NSIoPipe::EPortCapbility_None;
using NSCam::NSIoPipe::EPortCapbility_Rcrd;
using NSCam::NSIoPipe::ModuleInfo;
using NSCam::v4l2::INormalStream;

using NSImageio::NSIspio::EPortIndex;
using NSImageio::NSIspio::EPortIndex_DEPI;
using NSImageio::NSIspio::EPortIndex_DMGI;
using NSImageio::NSIspio::EPortIndex_IMG2O;
using NSImageio::NSIspio::EPortIndex_IMG3O;
using NSImageio::NSIspio::EPortIndex_IMGBI;
using NSImageio::NSIspio::EPortIndex_IMGCI;
using NSImageio::NSIspio::EPortIndex_IMGI;
using NSImageio::NSIspio::EPortIndex_LCEI;
using NSImageio::NSIspio::EPortIndex_VIPI;
using NSImageio::NSIspio::EPortIndex_WDMAO;
using NSImageio::NSIspio::EPortIndex_WROTO;

using NSCam::NSIoPipe::PORT_DEPI;
using NSCam::NSIoPipe::PORT_DMGI;
using NSCam::NSIoPipe::PORT_IMG2O;
using NSCam::NSIoPipe::PORT_IMG3O;
using NSCam::NSIoPipe::PORT_IMGBI;
using NSCam::NSIoPipe::PORT_IMGCI;
using NSCam::NSIoPipe::PORT_IMGI;
using NSCam::NSIoPipe::PORT_LCEI;
using NSCam::NSIoPipe::PORT_TUNING;
using NSCam::NSIoPipe::PORT_VIPI;
using NSCam::NSIoPipe::PORT_WDMAO;
using NSCam::NSIoPipe::PORT_WROTO;

namespace NSCam {
namespace Feature {
namespace P2Util {

/*******************************************
Common function
*******************************************/

MBOOL is4K2K(const MSize& size) {
  const MINT32 UHD_VR_WIDTH = 3840;
  const MINT32 UHD_VR_HEIGHT = 2160;
  return (size.w >= UHD_VR_WIDTH && size.h >= UHD_VR_HEIGHT);
}

MCropRect getCropRect(const MRectF& rectF) {
#define MAX_MDP_FRACTION_BIT (20)  // MDP use 20bits
  MCropRect cropRect(rectF.p.toMPoint(), rectF.s.toMSize());
  cropRect.p_fractional.x =
      (rectF.p.x - cropRect.p_integral.x) * (1 << MAX_MDP_FRACTION_BIT);
  cropRect.p_fractional.y =
      (rectF.p.y - cropRect.p_integral.y) * (1 << MAX_MDP_FRACTION_BIT);
  cropRect.w_fractional =
      (rectF.s.w - cropRect.s.w) * (1 << MAX_MDP_FRACTION_BIT);
  cropRect.h_fractional =
      (rectF.s.h - cropRect.s.h) * (1 << MAX_MDP_FRACTION_BIT);
  return cropRect;
}

auto getDebugExif() {
  static auto const sInst = MAKE_DebugExif();
  return sInst;
}

/*******************************************
Tuning function
*******************************************/
TuningParam makeTuningParam(const ILog& log,
                            const P2Pack& p2Pack,
                            std::shared_ptr<NS3Av3::IHal3A> hal3A,
                            MetaSet_T* inMetaSet,
                            MetaSet_T* pOutMetaSet,
                            MBOOL resized,
                            std::shared_ptr<IImageBuffer> tuningBuffer,
                            IImageBuffer* lcso) {
  (void)log;
  TRACE_S_FUNC_ENTER(log);
  TuningParam tuning;
  tuning.pRegBuf = reinterpret_cast<void*>(tuningBuffer->getBufVA(0));
  tuning.RegBuf_fd = tuningBuffer->getFD();
  tuning.pLcsBuf = lcso;

  trySet<MUINT8>(&((*inMetaSet).halMeta), MTK_3A_PGN_ENABLE, resized ? 0 : 1);
  P2_CAM_TRACE_BEGIN(TRACE_DEFAULT, "P2Util:Tuning");

  if (hal3A && tuningBuffer->getBufVA(0)) {
    MINT32 ret3A = hal3A->setIsp(0, *inMetaSet, &tuning, pOutMetaSet);
    if (ret3A < 0) {
      MY_S_LOGW(log, "hal3A->setIsp failed, memset regBuffer to 0");
      if (tuning.pRegBuf) {
        memset(tuning.pRegBuf, 0, tuningBuffer->getBitstreamSize());
      }
    }
    if (pOutMetaSet) {
      updateExtraMeta(p2Pack, &(pOutMetaSet->halMeta));
      updateDebugExif(p2Pack, (*inMetaSet).halMeta, &(pOutMetaSet->halMeta));
    }
  } else {
    MY_S_LOGE(log, "cannot run setIsp: hal3A=%p reg=%#" PRIxPTR "", hal3A.get(),
              tuningBuffer->getBufVA(0));
  }

  P2_CAM_TRACE_END(TRACE_DEFAULT);

  TRACE_S_FUNC_EXIT(log);
  return tuning;
}

/*******************************************
Metadata function
*******************************************/

MVOID updateExtraMeta(const P2Pack& p2Pack, IMetadata* outHal) {
  P2_CAM_TRACE_CALL(TRACE_ADVANCED);
  TRACE_S_FUNC_ENTER(p2Pack.mLog);
  trySet<MINT32>(outHal, MTK_PIPELINE_FRAME_NUMBER,
                 p2Pack.getFrameData().mMWFrameNo);
  trySet<MINT32>(outHal, MTK_PIPELINE_REQUEST_NUMBER,
                 p2Pack.getFrameData().mMWFrameRequestNo);
  TRACE_S_FUNC_EXIT(p2Pack.mLog);
}

MVOID updateDebugExif(const P2Pack& p2Pack,
                      const IMetadata& inHal,
                      IMetadata* outHal) {
  (void)p2Pack;
  P2_CAM_TRACE_CALL(TRACE_ADVANCED);
  TRACE_S_FUNC_ENTER(p2Pack.mLog);
  MUINT8 needExif = 0;
  if (tryGet<MUINT8>(inHal, MTK_HAL_REQUEST_REQUIRE_EXIF, &needExif) &&
      needExif) {
    MINT32 vhdrMode = SENSOR_VHDR_MODE_NONE;
    if (tryGet<MINT32>(inHal, MTK_P1NODE_SENSOR_VHDR_MODE, &vhdrMode) &&
        vhdrMode != SENSOR_VHDR_MODE_NONE) {
      std::map<MUINT32, MUINT32> debugInfoList;
      debugInfoList[getDebugExif()->getTagId_MF_TAG_IMAGE_HDR()] = 1;

      IMetadata exifMeta;
      tryGet<IMetadata>(*outHal, MTK_3A_EXIF_METADATA, &exifMeta);
      if (DebugExifUtils::setDebugExif(
              DebugExifUtils::DebugExifType::DEBUG_EXIF_MF,
              static_cast<MUINT32>(MTK_MF_EXIF_DBGINFO_MF_KEY),
              static_cast<MUINT32>(MTK_MF_EXIF_DBGINFO_MF_DATA), debugInfoList,
              &exifMeta) != NULL) {
        trySet<IMetadata>(outHal, MTK_3A_EXIF_METADATA, exifMeta);
      }
    }
  }
  TRACE_S_FUNC_EXIT(p2Pack.mLog);
}

MBOOL updateCropRegion(IMetadata* outHal, const MRect& rect) {
  // TODO(mtk): set output crop w/ margin setting
  MBOOL ret = MFALSE;
  ret = trySet<MRect>(outHal, MTK_SCALER_CROP_REGION, rect);
  return ret;
}

/*******************************************
QParams util function
*******************************************/

EPortCapbility toCapability(MUINT32 usage) {
  EPortCapbility cap = EPortCapbility_None;
  if (usage & (GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_HW_TEXTURE)) {
    cap = EPortCapbility_Disp;
  } else if (usage & GRALLOC_USAGE_HW_VIDEO_ENCODER) {
    cap = EPortCapbility_Rcrd;
  }
  return cap;
}

const char* toName(EPortIndex index) {
  switch (index) {
    case EPortIndex_IMGI:
      return "imgi";
    case EPortIndex_IMGBI:
      return "imgbi";
    case EPortIndex_IMGCI:
      return "imgci";
    case EPortIndex_VIPI:
      return "vipi";
    case EPortIndex_DEPI:
      return "depi";
    case EPortIndex_LCEI:
      return "lcei";
    case EPortIndex_DMGI:
      return "dmgi";
    case EPortIndex_IMG2O:
      return "img2o";
    case EPortIndex_IMG3O:
      return "img3o";
    case EPortIndex_WDMAO:
      return "wdmao";
    case EPortIndex_WROTO:
      return "wroto";
    default:
      return "unknown";
  }
  return NULL;
}

const char* toName(MUINT32 index) {
  return toName((EPortIndex)index);
}

const char* toName(const NSCam::NSIoPipe::PortID& port) {
  return toName((EPortIndex)port.index);
}

const char* toName(const Input& input) {
  return toName((EPortIndex)input.mPortID.index);
}

const char* toName(const Output& output) {
  return toName((EPortIndex)output.mPortID.index);
}

MBOOL is(const PortID& port, EPortIndex index) {
  return port.index == index;
}

MBOOL is(const Input& input, EPortIndex index) {
  return input.mPortID.index == index;
}

MBOOL is(const Output& output, EPortIndex index) {
  return output.mPortID.index == index;
}

MBOOL is(const PortID& port, const PortID& rhs) {
  return port.index == rhs.index;
}

MBOOL is(const Input& input, const PortID& rhs) {
  return input.mPortID.index == rhs.index;
}

MBOOL is(const Output& output, const PortID& rhs) {
  return output.mPortID.index == rhs.index;
}

MVOID printQParams(const ILog& log, unsigned i, const Input& input) {
  (void)log;
  (void)i;
  const unsigned index = input.mPortID.index;
  MSize size;
  MINT fmt = 0;
  if (input.mBuffer) {
    size = input.mBuffer->getImgSize();
    fmt = input.mBuffer->getImgFormat();
  }
  MY_S_LOGD(log, "mvIn[%d] idx=%d size=(%d,%d) fmt=0x%08x", i, index, size.w,
            size.h, fmt);
}

MVOID printQParams(const ILog& log, unsigned i, const Output& output) {
  (void)log;
  (void)i;
  const unsigned index = output.mPortID.index;
  const MUINT32 cap = output.mPortID.capbility;
  const MINT32 transform = output.mTransform;
  MSize size;
  MINT fmt = 0;
  if (output.mBuffer) {
    size = output.mBuffer->getImgSize();
    fmt = output.mBuffer->getImgFormat();
  }
  MY_S_LOGD(
      log, "mvOut[%d] idx=%s size=(%d,%d) fmt=0x%08x, cap=0x%02x, transform=%d",
      i, toName(index), size.w, size.h, fmt, cap, transform);
}

MVOID printQParams(const ILog& log, unsigned i, const MCrpRsInfo& crop) {
  (void)log;
  (void)i;
  (void)crop;
  MY_S_LOGD(log,
            "mvCropRsInfo[%d] groupID=%d frameGroup=%d i(%d,%d) f(%d,%d) "
            "s(%dx%d) r(%dx%d)",
            i, crop.mGroupID, crop.mFrameGroup, crop.mCropRect.p_integral.x,
            crop.mCropRect.p_integral.y, crop.mCropRect.p_fractional.x,
            crop.mCropRect.p_fractional.y, crop.mCropRect.s.w,
            crop.mCropRect.s.h, crop.mResizeDst.w, crop.mResizeDst.h);
}

MVOID printQParams(const ILog& log, unsigned i, const ModuleInfo& info) {
  (void)log;
  (void)i;
  switch (info.moduleTag) {
    case EDipModule_SRZ1:
      MY_S_LOGD(log, "mvModuleData[%d] tag=SRZ1(%d)", i, info.moduleTag);
      break;
    case EDipModule_SRZ4:
      MY_S_LOGD(log, "mvModuleData[%d] tag=SRZ4(%d) ", i, info.moduleTag);
      break;
    default:
      MY_S_LOGD(log, "mvModuleData[%d] tag=UNKNWON(%d)", i, info.moduleTag);
      break;
  }
}

MVOID printQParams(const ILog& log, unsigned i, const ExtraParam& extra) {
  (void)log;
  (void)i;
  switch (extra.CmdIdx) {
    case NSIoPipe::EPIPE_FE_INFO_CMD:
      MY_S_LOGD(log, "mvExtraParam[%d] cmd=FE(%d)", i, extra.CmdIdx);
      break;
    case NSIoPipe::EPIPE_FM_INFO_CMD:
      MY_S_LOGD(log, "mvExtraParam[%d] cmd=FM(%d)", i, extra.CmdIdx);
      break;
    case NSIoPipe::EPIPE_MDP_PQPARAM_CMD:
      MY_S_LOGD(log, "mvExtraParam[%d] cmd=PQParam(%d)", i, extra.CmdIdx);
      break;
    default:
      MY_S_LOGD(log, "mvExtraParam[%d] cmd=UNKOWN(%d)", i, extra.CmdIdx);
      break;
  }
}

MVOID printQParams(const ILog& log, const QParams& params) {
  for (unsigned f = 0, fCount = params.mvFrameParams.size(); f < fCount; ++f) {
    const FrameParams& frame = params.mvFrameParams[f];
    MY_S_LOGD(log, "QParams %d/%d", f, fCount);

    for (unsigned i = 0, n = frame.mvIn.size(); i < n; ++i) {
      printQParams(log, i, frame.mvIn[i]);
    }
    for (unsigned i = 0, n = frame.mvOut.size(); i < n; ++i) {
      printQParams(log, i, frame.mvOut[i]);
    }
    for (unsigned i = 0, n = frame.mvCropRsInfo.size(); i < n; ++i) {
      printQParams(log, i, frame.mvCropRsInfo[i]);
    }
    for (unsigned i = 0, n = frame.mvModuleData.size(); i < n; ++i) {
      printQParams(log, i, frame.mvModuleData[i]);
    }
    for (unsigned i = 0, n = frame.mvExtraParam.size(); i < n; ++i) {
      printQParams(log, i, frame.mvExtraParam[i]);
    }
  }
}

MVOID printTuningParam(const ILog& log, const TuningParam& tuning) {
  MY_S_LOGD(log, "reg=%p lcs=%p", tuning.pRegBuf, tuning.pLcsBuf);
}

MVOID push_in(FrameParams* frame, const PortID& portID, IImageBuffer* buffer) {
  Input input;
  input.mPortID = portID, input.mPortID.group = 0;
  input.mBuffer = buffer;
  frame->mvIn.push_back(input);
}

MVOID push_in(FrameParams* frame, const PortID& portID, const P2IO& in) {
  push_in(frame, portID, in.mBuffer);
}

MVOID push_out(FrameParams* frame, const PortID& portID, IImageBuffer* buffer) {
  push_out(frame, portID, buffer, EPortCapbility_None, 0);
}

MVOID push_out(FrameParams* frame,
               const PortID& portID,
               IImageBuffer* buffer,
               EPortCapbility cap,
               MINT32 transform) {
  Output output;
  output.mPortID = portID;
  output.mPortID.group = 0;
  output.mPortID.capbility = cap;
  output.mTransform = transform;
  output.mBuffer = buffer;
  frame->mvOut.push_back(output);
}

MVOID push_out(FrameParams* frame, const PortID& portID, const P2IO& out) {
  push_out(frame, portID, out.mBuffer, out.mCapability, out.mTransform);
}

MVOID push_crop(FrameParams* frame,
                MUINT32 cropID,
                const MCropRect& crop,
                const MSize& size) {
  MCrpRsInfo cropInfo;
  cropInfo.mGroupID = cropID;
  cropInfo.mCropRect = crop;
  cropInfo.mResizeDst = size;
  frame->mvCropRsInfo.push_back(cropInfo);
}

MVOID push_crop(FrameParams* frame,
                MUINT32 cropID,
                const MRectF& crop,
                const MSize& size,
                const MINT32 dmaConstrainFlag) {
  MCrpRsInfo cropInfo;
  cropInfo.mGroupID = cropID;
  cropInfo.mCropRect = getCropRect(crop);

  if ((dmaConstrainFlag & DMACONSTRAIN_NOSUBPIXEL) ||
      (dmaConstrainFlag & DMACONSTRAIN_2BYTEALIGN)) {
    cropInfo.mCropRect.p_fractional.x = 0;
    cropInfo.mCropRect.p_fractional.y = 0;
    cropInfo.mCropRect.w_fractional = 0;
    cropInfo.mCropRect.h_fractional = 0;
    if (dmaConstrainFlag & DMACONSTRAIN_2BYTEALIGN) {
      cropInfo.mCropRect.p_integral.x &= (~1);
      cropInfo.mCropRect.p_integral.y &= (~1);
    }
  }
  cropInfo.mResizeDst = size;
  frame->mvCropRsInfo.push_back(cropInfo);
}
/*******************************************
QParams function
*******************************************/
#if MTK_DP_ENABLE
DpPqParam* makeDpPqParam(DpPqParam* param,
                         const P2Pack& p2Pack,
                         const Output& out) {
  if (!param) {
    MY_S_LOGE(p2Pack.mLog, "Invalid DpPqParam buffer = nullptr, port:%s(%d)",
              toName(out.mPortID), out.mPortID.index);
    return nullptr;
  }

  return makeDpPqParam(param, p2Pack, out.mPortID.capbility);
}

DpPqParam* makeDpPqParam(DpPqParam* param,
                         const P2Pack& p2Pack,
                         const MUINT32 portCapabitity) {
  if (!param) {
    MY_S_LOGE(p2Pack.mLog,
              "Invalid DpPqParam buffer = nullptr, portCapabitity:(%d)",
              portCapabitity);
    return nullptr;
  }

  DpIspParam& ispParam = param->u.isp;

  param->scenario = MEDIA_ISP_PREVIEW;
  param->enable = false;

  ispParam.iso = p2Pack.getSensorData().mISO;
  ispParam.timestamp = p2Pack.getSensorData().mMWUniqueKey;
  ispParam.frameNo = p2Pack.getFrameData().mMWFrameNo;
  ispParam.requestNo = p2Pack.getFrameData().mMWFrameRequestNo;
  ispParam.lensId = p2Pack.getSensorData().mSensorID;
  ispParam.userString[0] = '\0';

  const P2PlatInfo* plat = p2Pack.getPlatInfo();

  return param;
}
#endif
MVOID push_PQParam(FrameParams* frame,
                   const P2Pack& p2Pack,
                   const P2ObjPtr& obj) {
  if (!obj.pqParam) {
    MY_S_LOGE(p2Pack.mLog, "Invalid pqParam buffer = NULL");
    return;
  }

  obj.pqParam->WDMAPQParam = NULL;
  obj.pqParam->WROTPQParam = NULL;
#if MTK_DP_ENABLE
  for (const Output& out : frame->mvOut) {
    if (out.mPortID.index == EPortIndex_WDMAO &&
        obj.pqParam->WDMAPQParam == NULL) {
      obj.pqParam->WDMAPQParam = makeDpPqParam(obj.pqWDMA, p2Pack, out);
    } else if (out.mPortID.index == EPortIndex_WROTO &&
               obj.pqParam->WROTPQParam == NULL) {
      obj.pqParam->WROTPQParam = makeDpPqParam(obj.pqWROT, p2Pack, out);
    }
  }
#endif
  ExtraParam extra;
  extra.CmdIdx = NSIoPipe::EPIPE_MDP_PQPARAM_CMD;
  extra.moduleStruct = reinterpret_cast<void*>(obj.pqParam);
  frame->mvExtraParam.push_back(extra);
}

MVOID updateQParams(QParams* qparams,
                    const P2Pack& p2Pack,
                    const P2IOPack& io,
                    const P2ObjPtr& obj,
                    const TuningParam& tuning) {
  updateFrameParams(&(qparams->mvFrameParams[0]), p2Pack, io, obj, tuning);
}

QParams makeQParams(const P2Pack& p2Pack,
                    ENormalStreamTag tag,
                    const P2IOPack& io,
                    const P2ObjPtr& obj,
                    const TuningParam& tuning) {
  QParams qparams;
  qparams.mvFrameParams.push_back(
      makeFrameParams(p2Pack, tag, io, obj, tuning));
  return qparams;
}

QParams makeQParams(const P2Pack& p2Pack,
                    ENormalStreamTag tag,
                    const P2IOPack& io,
                    const P2ObjPtr& obj) {
  QParams qparams;
  qparams.mvFrameParams.push_back(makeFrameParams(p2Pack, tag, io, obj));
  return qparams;
}

MVOID updateFrameParams(FrameParams* frame,
                        const P2Pack& p2Pack,
                        const P2IOPack& io,
                        const P2ObjPtr& obj,
                        const TuningParam& tuning) {
  TRACE_S_FUNC_ENTER(p2Pack.mLog);

  MBOOL dip50 = MTRUE;

  MSize inSize = io.mIMGI.getImgSize();
  for (auto& in : frame->mvIn) {
    if (is(in, PORT_IMGI) && in.mBuffer) {
      inSize = in.mBuffer->getImgSize();
      break;
    }
  }

  if (tuning.pRegBuf) {
    frame->mTuningData = tuning.pRegBuf;
    frame->mTuningData_fd = tuning.RegBuf_fd;
  }
  if (tuning.pLsc2Buf) {
    push_in(frame, dip50 ? PORT_IMGCI : PORT_DEPI,
            reinterpret_cast<IImageBuffer*>(tuning.pLsc2Buf));
  }
  if (tuning.pBpc2Buf) {
    push_in(frame, dip50 ? PORT_IMGBI : PORT_DMGI,
            reinterpret_cast<IImageBuffer*>(tuning.pBpc2Buf));
  }

  if (tuning.pLcsBuf) {
    IImageBuffer* lcso = reinterpret_cast<IImageBuffer*>(tuning.pLcsBuf);
    push_in(frame, PORT_LCEI, lcso);
  }

  TRACE_S_FUNC_EXIT(p2Pack.mLog);
}

FrameParams makeFrameParams(const P2Pack& p2Pack,
                            ENormalStreamTag tag,
                            const P2IOPack& io,
                            const P2ObjPtr& obj,
                            const TuningParam& tuning) {
  TRACE_S_FUNC_ENTER(p2Pack.mLog);
  FrameParams fparam = makeFrameParams(p2Pack, tag, io, obj);
  updateFrameParams(&fparam, p2Pack, io, obj, tuning);
  TRACE_S_FUNC_EXIT(p2Pack.mLog);
  return fparam;
}

FrameParams makeFrameParams(const P2Pack& p2Pack,
                            ENormalStreamTag tag,
                            const P2IOPack& io,
                            const P2ObjPtr& obj) {
  const ILog& log = p2Pack.mLog;
  TRACE_S_FUNC_ENTER(log);

  const std::shared_ptr<Cropper> cropper = p2Pack.getSensorData().mCropper;
  MUINT32 cropFlag = 0;
  cropFlag |= io.isResized() ? Cropper::USE_RESIZED : 0;
  cropFlag |= io.useLMV() ? Cropper::USE_EIS_12 : 0;

  FrameParams frame;
  frame.mStreamTag = tag;

  if (io.mIMGI.isValid()) {
    push_in(&frame, PORT_IMGI, io.mIMGI);
  }

  if (io.mIMG2O.isValid()) {
    MCropRect crop =
        cropper->calcViewAngle(log, io.mIMG2O.getTransformSize(), cropFlag);
    push_out(&frame, PORT_IMG2O, io.mIMG2O);
    push_crop(&frame, CROP_IMG2O, crop, io.mIMG2O.getImgSize());
  }
  if (io.mWDMAO.isValid()) {
    MCropRect crop =
        cropper->calcViewAngle(log, io.mWDMAO.getTransformSize(), cropFlag);
    push_out(&frame, PORT_WDMAO, io.mWDMAO);
    push_crop(&frame, CROP_WDMAO, crop, io.mWDMAO.getImgSize());
  }
  if (io.mWROTO.isValid()) {
    MCropRect crop =
        cropper->calcViewAngle(log, io.mWROTO.getTransformSize(), cropFlag);
    push_out(&frame, PORT_WROTO, io.mWROTO);
    push_crop(&frame, CROP_WROTO, crop, io.mWROTO.getImgSize());
  }

  if (obj.hasPQ) {
    push_PQParam(&frame, p2Pack, obj);
  }

  if (io.mTuning.isValid()) {
    push_in(&frame, PORT_TUNING, io.mTuning);
  }

  TRACE_S_FUNC_EXIT(log);
  return frame;
}

}  // namespace P2Util
}  // namespace Feature
}  // namespace NSCam
