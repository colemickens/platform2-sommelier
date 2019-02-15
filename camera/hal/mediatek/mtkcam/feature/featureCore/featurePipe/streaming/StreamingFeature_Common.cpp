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

#include "MtkHeader.h"
#include <featurePipe/common/include/DebugUtil.h>
#include "DebugControl.h"
#define PIPE_CLASS_TAG "Util"
#define PIPE_TRACE TRACE_STREAMING_FEATURE_COMMON
#include <featurePipe/common/include/PipeLog.h>
#include <featurePipe/streaming/StreamingFeature_Common.h>
#include <memory>
#include <mtkcam/feature/utils/p2/P2Util.h>
#include <utility>
#include <vector>

#if 1  // MTK_DP_ENABLE
#include "MDPWrapper.h"
#endif
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

const char* Fmt2Name(MINT fmt) {
  switch (fmt) {
    case eImgFmt_RGBA8888:
      return "rgba";
    case eImgFmt_RGB888:
      return "rgb";
    case eImgFmt_YUY2:
      return "yuy2";
    case eImgFmt_YV12:
      return "yv12";
    case eImgFmt_NV21:
      return "nv21";
    default:
      return "unknown";
  }
}

MBOOL useMDPHardware() {
  return MTRUE;
}

MINT32 getCropGroupID(const NSCam::NSIoPipe::PortID& portID) {
  return getCropGroupIDByIndex(portID.index);
}

MINT32 getCropGroupIDByIndex(MUINT32 port) {
  if (port == NSImageio::NSIspio::EPortIndex_WDMAO) {
    return WDMAO_CROP_GROUP;
  }
  if (port == NSImageio::NSIspio::EPortIndex_WROTO) {
    return WROTO_CROP_GROUP;
  }
  if (port == NSImageio::NSIspio::EPortIndex_IMG2O) {
    return IMG2O_CROP_GROUP;
  }
  MY_LOGE("[ERROR] Can not find crop group for port(%d)!", port);
  return -1;
}

MBOOL isTargetOutput(IO_TYPE target, const SFPOutput& output) {
  if (target == IO_TYPE_DISPLAY) {
    return isDisplayOutput(output);
  } else if (target == IO_TYPE_RECORD) {
    return isRecordOutput(output);
  } else if (target == IO_TYPE_EXTRA) {
    return isExtraOutput(output);
  } else if (target == IO_TYPE_FD) {
    return isFDOutput(output);
  } else if (target == IO_TYPE_PHYSICAL) {
    return isPhysicalOutput(output);
  } else {
    MY_LOGW("Invalid output target type = %d", target);
  }
  return MFALSE;
}

MBOOL isExtraOutput(const SFPOutput& output) {
  return (output.mTargetType == SFPOutput::OUT_TARGET_UNKNOWN);
}

MBOOL isDisplayOutput(const SFPOutput& output) {
  return (output.mTargetType == SFPOutput::OUT_TARGET_DISPLAY);
}

MBOOL isRecordOutput(const SFPOutput& output) {
  return (output.mTargetType == SFPOutput::OUT_TARGET_RECORD);
}

MBOOL isFDOutput(const SFPOutput& output) {
  return (output.mTargetType == SFPOutput::OUT_TARGET_FD);
}

MBOOL isPhysicalOutput(const SFPOutput& output) {
  return (output.mTargetType == SFPOutput::OUT_TARGET_PHYSICAL);
}

MBOOL isTargetOutput(IO_TYPE target, const NSCam::NSIoPipe::Output& output) {
  if (target == IO_TYPE_DISPLAY) {
    return isDisplayOutput(output);
  } else if (target == IO_TYPE_RECORD) {
    return isRecordOutput(output);
  } else if (target == IO_TYPE_EXTRA) {
    return isExtraOutput(output);
  } else if (target == IO_TYPE_FD) {
    return isFDOutput(output);
  } else {
    MY_LOGW("Invalid output target type = %d", target);
  }
  return MFALSE;
}

MBOOL isExtraOutput(const NSCam::NSIoPipe::Output& output) {
  return (output.mPortID.capbility == NSCam::NSIoPipe::EPortCapbility_None &&
          !isFDOutput(output)) ||
         (output.mPortID.capbility == NSCam::NSIoPipe::EPortCapbility_Rcrd &&
          (getGraphicBufferAddr(output.mBuffer) == NULL));
}

MBOOL isDisplayOutput(const NSCam::NSIoPipe::Output& output) {
  return output.mPortID.capbility == NSCam::NSIoPipe::EPortCapbility_Disp;
}

MBOOL isRecordOutput(const NSCam::NSIoPipe::Output& output) {
  return (output.mPortID.capbility == NSCam::NSIoPipe::EPortCapbility_Rcrd) &&
         (getGraphicBufferAddr(output.mBuffer) != nullptr);
}

MBOOL isFDOutput(const NSCam::NSIoPipe::Output& output) {
  return output.mPortID.index == NSImageio::NSIspio::EPortIndex_IMG2O;
}

MBOOL getFrameInInfo(FrameInInfo* info,
                     const NSCam::NSIoPipe::FrameParams& frame,
                     MUINT32 port) {
  IImageBuffer* buffer = findInBuffer(frame, port);
  if (buffer != nullptr) {
    info->inSize = buffer->getImgSize();
    info->timestamp = buffer->getTimestamp();
    return MTRUE;
  }
  return MFALSE;
}

IImageBuffer* findInBuffer(const NSCam::NSIoPipe::QParams& qparam,
                           MUINT32 port) {
  IImageBuffer* buffer = nullptr;
  if (qparam.mvFrameParams.size() > 0) {
    buffer = findInBuffer(qparam.mvFrameParams[0], port);
  }
  return buffer;
}

IImageBuffer* findInBuffer(const NSCam::NSIoPipe::FrameParams& param,
                           MUINT32 port) {
  IImageBuffer* buffer = nullptr;
  for (unsigned i = 0, count = param.mvIn.size(); i < count; ++i) {
    unsigned index = param.mvIn[i].mPortID.index;
    if (index == port) {
      buffer = param.mvIn[i].mBuffer;
      break;
    }
  }
  return buffer;
}

IImageBuffer* findOutBuffer(const NSCam::NSIoPipe::QParams& qparam,
                            unsigned skip) {
  IImageBuffer* buffer = nullptr;
  if (qparam.mvFrameParams.size() > 0) {
    buffer = findOutBuffer(qparam.mvFrameParams[0], skip);
  }
  return buffer;
}

IImageBuffer* findOutBuffer(const NSCam::NSIoPipe::FrameParams& param,
                            unsigned skip) {
  IImageBuffer* buffer = nullptr;
  for (unsigned i = 0, count = param.mvOut.size(); i < count; ++i) {
    unsigned index = param.mvOut[i].mPortID.index;
    if (index == NSImageio::NSIspio::EPortIndex_WROTO ||
        index == NSImageio::NSIspio::EPortIndex_WDMAO) {
      if (skip == 0) {
        buffer = param.mvOut[i].mBuffer;
        break;
      }
      --skip;
    }
  }
  return buffer;
}

MBOOL findUnusedMDPPort(const NSCam::NSIoPipe::QParams& qparam,
                        unsigned* index) {
  MBOOL ret = MFALSE;
  MBOOL wrotUsed = MFALSE;
  MBOOL wdmaUsed = MFALSE;

  if (qparam.mvFrameParams.size() > 0) {
    for (unsigned i = 0, count = qparam.mvFrameParams[0].mvOut.size();
         i < count; ++i) {
      unsigned outIndex = qparam.mvFrameParams[0].mvOut[i].mPortID.index;
      if (outIndex == NSImageio::NSIspio::EPortIndex_WROTO) {
        wrotUsed = MTRUE;
      } else if (outIndex == NSImageio::NSIspio::EPortIndex_WDMAO) {
        wdmaUsed = MTRUE;
      }
    }

    if (!wdmaUsed) {
      *index = NSImageio::NSIspio::EPortIndex_WDMAO;
      ret = MTRUE;
    } else if (!wrotUsed) {
      *index = NSImageio::NSIspio::EPortIndex_WROTO;
      ret = MTRUE;
    }
  }

  return ret;
}

MBOOL findUnusedMDPCropGroup(const NSCam::NSIoPipe::QParams& qparam,
                             unsigned* cropGroup) {
  MBOOL ret = MFALSE;
  MBOOL wrotUsed = MFALSE;
  MBOOL wdmaUsed = MFALSE;

  if (qparam.mvFrameParams.size() > 0) {
    for (unsigned i = 0, count = qparam.mvFrameParams[0].mvCropRsInfo.size();
         i < count; ++i) {
      unsigned outCropGroup = qparam.mvFrameParams[0].mvCropRsInfo[i].mGroupID;
      if (outCropGroup == WROTO_CROP_GROUP) {
        wrotUsed = MTRUE;
      } else if (outCropGroup == WDMAO_CROP_GROUP) {
        wdmaUsed = MTRUE;
      }
    }

    if (!wdmaUsed) {
      *cropGroup = WDMAO_CROP_GROUP;
      ret = MTRUE;
    } else if (!wrotUsed) {
      *cropGroup = WROTO_CROP_GROUP;
      ret = MTRUE;
    }
  }

  return ret;
}

MSize toValid(const MSize& size, const MSize& def) {
  return (size.w && size.h) ? size : def;
}

NSCam::EImageFormat toValid(const NSCam::EImageFormat fmt,
                            const NSCam::EImageFormat def) {
  return (fmt == eImgFmt_UNKNOWN) ? def : fmt;
}

const char* toName(const NSCam::EImageFormat fmt) {
  const char* name = "unknown";
  switch (fmt) {
    case eImgFmt_YUY2:
      name = "YUY2";
      break;
    case eImgFmt_UYVY:
      name = "UYVY";
      break;
    case eImgFmt_YVYU:
      name = "YVYU";
      break;
    case eImgFmt_VYUY:
      name = "VYUY";
      break;
    case eImgFmt_NV16:
      name = "NV16";
      break;
    case eImgFmt_NV61:
      name = "NV61";
      break;
    case eImgFmt_NV21:
      name = "NV21";
      break;
    case eImgFmt_NV12:
      name = "NV12";
      break;
    case eImgFmt_YV16:
      name = "YV16";
      break;
    case eImgFmt_I422:
      name = "I422";
      break;
    case eImgFmt_YV12:
      name = "YV12";
      break;
    case eImgFmt_I420:
      name = "I420";
      break;
    case eImgFmt_Y800:
      name = "Y800";
      break;
    case eImgFmt_STA_BYTE:
      name = "BYTE";
      break;
    case eImgFmt_RGB565:
      name = "RGB565";
      break;
    case eImgFmt_RGB888:
      name = "RGB888";
      break;
    case eImgFmt_ARGB888:
      name = "ARGB888";
      break;
    default:
      name = "unknown";
      break;
  }
  return name;
}
#if MTK_DP_ENABLE
MBOOL toDpColorFormat(const NSCam::EImageFormat fmt, DpColorFormat* dpFmt) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MTRUE;
  switch (fmt) {
    case eImgFmt_YUY2:
      *dpFmt = DP_COLOR_YUYV;
      break;
    case eImgFmt_UYVY:
      *dpFmt = DP_COLOR_UYVY;
      break;
    case eImgFmt_YVYU:
      *dpFmt = DP_COLOR_YVYU;
      break;
    case eImgFmt_VYUY:
      *dpFmt = DP_COLOR_VYUY;
      break;
    case eImgFmt_NV16:
      *dpFmt = DP_COLOR_NV16;
      break;
    case eImgFmt_NV61:
      *dpFmt = DP_COLOR_NV61;
      break;
    case eImgFmt_NV21:
      *dpFmt = DP_COLOR_NV21;
      break;
    case eImgFmt_NV12:
      *dpFmt = DP_COLOR_NV12;
      break;
    case eImgFmt_YV16:
      *dpFmt = DP_COLOR_YV16;
      break;
    case eImgFmt_I422:
      *dpFmt = DP_COLOR_I422;
      break;
    case eImgFmt_YV12:
      *dpFmt = DP_COLOR_YV12;
      break;
    case eImgFmt_I420:
      *dpFmt = DP_COLOR_I420;
      break;
    case eImgFmt_Y800:
      *dpFmt = DP_COLOR_GREY;
      break;
    case eImgFmt_STA_BYTE:
      *dpFmt = DP_COLOR_GREY;
      break;
    case eImgFmt_RGB565:
      *dpFmt = DP_COLOR_RGB565;
      break;
    case eImgFmt_RGB888:
      *dpFmt = DP_COLOR_RGB888;
      break;
    case eImgFmt_ARGB888:
      *dpFmt = DP_COLOR_ARGB8888;
      break;
    default:
      MY_LOGE("fmt(0x%x) not support in DP", fmt);
      ret = MFALSE;
      break;
  }
  TRACE_FUNC_EXIT();
  return ret;
}
#endif
int toPixelFormat(NSCam::EImageFormat fmt) {
  TRACE_FUNC_ENTER();
  int pixelFmt = HAL_PIXEL_FORMAT_YV12;
  switch (fmt) {
    case eImgFmt_YUY2:
      pixelFmt = HAL_PIXEL_FORMAT_YCbCr_422_I;
      break;
    case eImgFmt_NV16:
      pixelFmt = HAL_PIXEL_FORMAT_YCbCr_422_SP;
      break;
    case eImgFmt_NV21:
      pixelFmt = HAL_PIXEL_FORMAT_YCrCb_420_SP;
      break;
    case eImgFmt_YV12:
      pixelFmt = HAL_PIXEL_FORMAT_YV12;
      break;
    case eImgFmt_RGB565:
      pixelFmt = HAL_PIXEL_FORMAT_RGB_565;
      break;
    case eImgFmt_RGB888:
      pixelFmt = HAL_PIXEL_FORMAT_RGB_888;
      break;
    default:
      MY_LOGE("unknown fmt(0x%x), use eImgFmt_YV12", fmt);
      break;
  }
  TRACE_FUNC_EXIT();
  return pixelFmt;
}

MBOOL copyImageBuffer(IImageBuffer* src, IImageBuffer* dst) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MTRUE;

  if (!src || !dst) {
    MY_LOGE("Invalid buffers src=%p dst=%p", src, dst);
    ret = MFALSE;
  } else if (src->getImgSize() != dst->getImgSize()) {
    MY_LOGE("Mismatch buffer size src(%dx%d) dst(%dx%d)", src->getImgSize().w,
            src->getImgSize().h, dst->getImgSize().w, dst->getImgSize().h);
    ret = MFALSE;
  } else {
    unsigned srcPlane = src->getPlaneCount();
    unsigned dstPlane = dst->getPlaneCount();

    if (!srcPlane || !dstPlane ||
        (srcPlane != dstPlane && srcPlane != 1 && dstPlane != 1)) {
      MY_LOGE("Mismatch buffer plane src(%d) dst(%d)", srcPlane, dstPlane);
      ret = MFALSE;
    }
    for (unsigned i = 0; i < srcPlane; ++i) {
      if (!src->getBufVA(i)) {
        MY_LOGE("Invalid src plane[%d] VA", i);
        ret = MFALSE;
      }
    }
    for (unsigned i = 0; i < dstPlane; ++i) {
      if (!dst->getBufVA(i)) {
        MY_LOGE("Invalid dst plane[%d] VA", i);
        ret = MFALSE;
      }
    }

    if (srcPlane == 1) {
      MY_LOGD("src: plane=1 size=%zu stride=%zu", src->getBufSizeInBytes(0),
              src->getBufStridesInBytes(0));
      ret = MFALSE;
    }
    if (dstPlane == 1) {
      MY_LOGD("dst: plane=1 size=%zu stride=%zu", dst->getBufSizeInBytes(0),
              dst->getBufStridesInBytes(0));
      ret = MFALSE;
    }

    if (ret) {
      char *srcVA = NULL, *dstVA = NULL;
      size_t srcSize = 0;
      size_t dstSize = 0;
      size_t srcStride = 0;
      size_t dstStride = 0;

      for (unsigned i = 0; i < srcPlane && i < dstPlane; ++i) {
        if (i < srcPlane) {
          srcVA = reinterpret_cast<char*>(src->getBufVA(i));
        }
        if (i < dstPlane) {
          dstVA = reinterpret_cast<char*>(dst->getBufVA(i));
        }

        srcSize = src->getBufSizeInBytes(i);
        dstSize = dst->getBufSizeInBytes(i);
        srcStride = src->getBufStridesInBytes(i);
        dstStride = dst->getBufStridesInBytes(i);
        MY_LOGD("plane[%d] memcpy %p(%zu)=>%p(%zu)", i, srcVA, srcSize, dstVA,
                dstSize);
        if (srcStride == dstStride) {
          memcpy(reinterpret_cast<void*>(dstVA), reinterpret_cast<void*>(srcVA),
                 (srcSize <= dstSize) ? srcSize : dstSize);
        } else {
          MY_LOGD("Stride: src(%zu) dst(%zu)", srcStride, dstStride);
          size_t stride = (srcStride < dstStride) ? srcStride : dstStride;
          unsigned height = dstSize / dstStride;
          for (unsigned j = 0; j < height; ++j) {
            memcpy(reinterpret_cast<void*>(dstVA),
                   reinterpret_cast<void*>(srcVA), stride);
            srcVA += srcStride;
            dstVA += dstStride;
          }
        }
      }
    }
  }

  TRACE_FUNC_EXIT();
  return ret;
}

NB_SPTR getGraphicBufferAddr(IImageBuffer* imageBuffer) {
  TRACE_FUNC_ENTER();
  void* addr = nullptr;
  if (!imageBuffer) {
    MY_LOGE("Invalid imageBuffer");
  } else if (!imageBuffer->getImageBufferHeap()) {
    MY_LOGW("Cannot get imageBufferHeap");
  } else {
    addr = imageBuffer->getImageBufferHeap()->getHWBuffer();
  }
  TRACE_FUNC_EXIT();
  return (NB_SPTR)(addr);
}

MBOOL isTypeMatch(PathType pathT, IO_TYPE ioType) {
  if (ioType == IO_TYPE_FD) {  // FD may exist in any Path
    return MTRUE;
  }
  if (pathT == PATH_GENERAL) {
    if (ioType == IO_TYPE_DISPLAY || ioType == IO_TYPE_RECORD ||
        ioType == IO_TYPE_EXTRA) {
      return MTRUE;
    }
  }
  if (pathT == PATH_PHYSICAL) {
    if (ioType == IO_TYPE_PHYSICAL) {
      return MTRUE;
    }
  }
  return MFALSE;
}

MBOOL existOutBuffer(const SFPIOMap& sfpIO, IO_TYPE target) {
  for (auto&& out : sfpIO.mOutList) {
    if (isTargetOutput(target, out)) {
      return MTRUE;
    }
  }
  return MFALSE;
}

MBOOL existOutBuffer(const std::vector<SFPIOMap>& sfpIOList, IO_TYPE target) {
  MBOOL exist = MFALSE;
  for (auto&& sfpIO : sfpIOList) {
    if (isTypeMatch(sfpIO.mPathType, target)) {
      exist |= existOutBuffer(sfpIO, target);
    }
  }
  return exist;
}

static MBOOL findCrop(const NSCam::NSIoPipe::FrameParams& param,
                      MUINT32 port,
                      NSCam::NSIoPipe::MCrpRsInfo* out) {
  for (auto&& cropInfo : param.mvCropRsInfo) {
    if ((port == NSImageio::NSIspio::EPortIndex_WDMAO &&
         cropInfo.mGroupID == WDMAO_CROP_GROUP) ||
        (port == NSImageio::NSIspio::EPortIndex_WROTO &&
         cropInfo.mGroupID == WROTO_CROP_GROUP) |
            (port == NSImageio::NSIspio::EPortIndex_IMG2O &&
             cropInfo.mGroupID == IMG2O_CROP_GROUP)) {
      *out = cropInfo;
      return MTRUE;
    }
  }
  return MFALSE;
}

static MVOID* findPqParamPtr(const NSCam::NSIoPipe::FrameParams& param) {
  for (auto&& ext : param.mvExtraParam) {
    if (ext.CmdIdx == NSCam::NSIoPipe::EPIPE_MDP_PQPARAM_CMD) {
      return ext.moduleStruct;
    }
  }
  return NULL;
}

static MVOID* findDpPqParamPtr(const NSCam::NSIoPipe::FrameParams& param,
                               MUINT32 port) {
  if (port != NSImageio::NSIspio::EPortIndex_WDMAO &&
      port != NSImageio::NSIspio::EPortIndex_WROTO) {
    return NULL;
  }

  MVOID* modulePtr = findPqParamPtr(param);
  if (modulePtr == NULL) {
    return NULL;
  }

  NSCam::NSIoPipe::PQParam* pqParam =
      static_cast<NSCam::NSIoPipe::PQParam*>(modulePtr);
  switch (port) {
    case NSImageio::NSIspio::EPortIndex_WDMAO:
      return pqParam->WDMAPQParam;
    case NSImageio::NSIspio::EPortIndex_WROTO:
      return pqParam->WROTPQParam;
    default:
      MY_LOGE("Can not find PQParam for port(%d)", port);
      return NULL;
  }
}

MBOOL parseIO(MUINT32 sensorID,
              const NSCam::NSIoPipe::FrameParams& param,
              const VarMap& varMap,
              SFPIOMap* ioMap,
              SFPIOManager* ioMgr) {
  P2_CAM_TRACE_CALL(TRACE_ADVANCED);
  SFPSensorTuning tuning;
  SFPSensorInput sensorIn;
  MSize inputSize;
  MBOOL isIMGO = varMap.get<MBOOL>(VAR_IMGO_2IMGI_ENABLE, MFALSE);
  if (isIMGO) {
    tuning.addFlag(SFPSensorTuning::FLAG_IMGO_IN);
    sensorIn.mIMGO = findInBuffer(param, NSImageio::NSIspio::EPortIndex_IMGI);
    inputSize = sensorIn.mIMGO->getImgSize();
  } else {
    tuning.addFlag(SFPSensorTuning::FLAG_RRZO_IN);
    sensorIn.mRRZO = findInBuffer(param, NSImageio::NSIspio::EPortIndex_IMGI);
    inputSize = sensorIn.mRRZO->getImgSize();
  }
  sensorIn.mLCSO = findInBuffer(param, NSImageio::NSIspio::EPortIndex_LCEI);
  if (sensorIn.mLCSO == NULL) {
    sensorIn.mLCSO =
        varMap.get<NSCam::IImageBuffer*>(VAR_TUNING_IIMAGEBUF_LCSO, NULL);
  }

  if (sensorIn.mLCSO != NULL) {
    tuning.addFlag(SFPSensorTuning::FLAG_LCSO_IN);
  }
  sensorIn.mPrvRSSO = varMap.get<NSCam::IImageBuffer*>(VAR_PREV_RSSO, NULL);
  sensorIn.mCurRSSO = varMap.get<NSCam::IImageBuffer*>(VAR_CURR_RSSO, NULL);
  sensorIn.mHalIn =
      varMap.get<NSCam::IMetadata*>(VAR_HAL1_HAL_IN_METADATA, NULL);
  sensorIn.mAppIn =
      varMap.get<NSCam::IMetadata*>(VAR_HAL1_APP_IN_METADATA, NULL);
  ioMap->addInputTuning(sensorID, tuning);
  ioMgr->addInput(sensorID, sensorIn);

  ioMap->mOutList.reserve(param.mvOut.size());
  for (auto& output : param.mvOut) {
    NSCam::NSIoPipe::MCrpRsInfo cropInfo;
    cropInfo.mResizeDst = MSize(inputSize.w, inputSize.h);
    MRect crop(inputSize.w, inputSize.h);
    if (!findCrop(param, output.mPortID.index, &cropInfo)) {
      MY_LOGE("Can not find Crop for port(%d)", output.mPortID.index);
    }
    MVOID* pqPtr = findPqParamPtr(param);
    MVOID* dppqPtr = findDpPqParamPtr(param, output.mPortID.index);

    if (isDisplayOutput(output)) {
      ioMap->addOutput(SFPOutput(output, cropInfo, pqPtr, dppqPtr,
                                 SFPOutput::OUT_TARGET_DISPLAY));
    } else if (isRecordOutput(output)) {
      ioMap->addOutput(SFPOutput(output, cropInfo, pqPtr, dppqPtr,
                                 SFPOutput::OUT_TARGET_RECORD));
    } else if (isExtraOutput(output)) {
      ioMap->addOutput(SFPOutput(output, cropInfo, pqPtr, dppqPtr,
                                 SFPOutput::OUT_TARGET_UNKNOWN));
    } else if (isFDOutput(output)) {
      ioMap->addOutput(SFPOutput(output, cropInfo, pqPtr, dppqPtr,
                                 SFPOutput::OUT_TARGET_FD));
    } else {
      MY_LOGE("Unknown QParam output id(%d), convert to SFPOutput failed !",
              output.mPortID.index);
      return MFALSE;
    }
  }
  return MTRUE;
}

static MVOID addNewFrame(NSCam::NSIoPipe::QParams* qparam,
                         MUINT32 refFrameInd) {
  qparam->mvFrameParams.push_back(qparam->mvFrameParams.at(refFrameInd));
  NSCam::NSIoPipe::FrameParams& f =
      qparam->mvFrameParams.at(qparam->mvFrameParams.size() - 1);

  f.mvOut.clear();
  f.mvCropRsInfo.clear();
  f.mvExtraParam.clear();
}

static MVOID clearPQParam(MVOID* pqParam) {
  if (pqParam != NULL) {
    NSCam::NSIoPipe::PQParam* p =
        static_cast<NSCam::NSIoPipe::PQParam*>(pqParam);
    p->WDMAPQParam = NULL;
    p->WROTPQParam = NULL;
  }
}

static MVOID addPQParam(NSCam::NSIoPipe::FrameParams* frame, MVOID* pqParam) {
  NSCam::NSIoPipe::ExtraParam extra;
  extra.CmdIdx = NSIoPipe::EPIPE_MDP_PQPARAM_CMD;
  extra.moduleStruct = pqParam;
  frame->mvExtraParam.push_back(extra);
}

static MVOID pushPQParam(NSCam::NSIoPipe::FrameParams* frame,
                         MVOID* pqParam,
                         MVOID* dpParam,
                         MUINT32 port) {
  if (dpParam == NULL || pqParam == NULL) {
    return;
  }
  if (port != NSImageio::NSIspio::EPortIndex_WDMAO &&
      port != NSImageio::NSIspio::EPortIndex_WROTO) {
    return;
  }

  MVOID* targetParam = findPqParamPtr(*frame);

  if (targetParam == NULL) {
    addPQParam(frame, pqParam);
    clearPQParam(pqParam);
    targetParam = pqParam;
  }

  NSCam::NSIoPipe::PQParam* p =
      static_cast<NSCam::NSIoPipe::PQParam*>(targetParam);
  if (port == NSImageio::NSIspio::EPortIndex_WDMAO) {
    p->WDMAPQParam = dpParam;
  } else if (port == NSImageio::NSIspio::EPortIndex_WROTO) {
    p->WROTPQParam = dpParam;
  }
}

static MBOOL isExistPort(const NSCam::NSIoPipe::FrameParams& f,
                         const NSCam::NSIoPipe::PortID& portID) {
  for (auto&& out : f.mvOut) {
    if (out.mPortID.index == portID.index) {
      return MTRUE;
    }
  }
  return MFALSE;
}

MVOID pushSFPOutToMdp(NSCam::NSIoPipe::FrameParams* f,
                      const NSCam::NSIoPipe::PortID& portID,
                      const SFPOutput& output) {
  Feature::P2Util::push_out(f, portID, output);
  if (output.isCropValid()) {
    Feature::P2Util::push_crop(f, getCropGroupID(portID), output.mCropRect,
                               output.mCropDstSize, output.mDMAConstrainFlag);
  }
  pushPQParam(f, output.mpPqParam, output.mpDpPqParam, portID.index);
}
#if 1  // graphics
static MVOID prepareFrame(NSCam::NSIoPipe::FrameParams* frame,
                          MDPWrapper::OutCollection* collect) {
  // put in wdma port
  NSCam::NSIoPipe::PortID target = NSIoPipe::PORT_WDMAO;
  if (!isExistPort(*frame, target) && !collect->isNonRotFinish()) {
    const SFPOutput& out = collect->popFirstNonRotOut();
    pushSFPOutToMdp(frame, target, out);
  }

  // put in wrot port
  target = NSIoPipe::PORT_WROTO;
  if (!isExistPort(*frame, target)) {
    if (!collect->isRotFinish()) {
      const SFPOutput& out = collect->popFirstRotOut();
      pushSFPOutToMdp(frame, target, out);
    } else if (!collect->isNonRotFinish()) {
      const SFPOutput& out = collect->popFirstNonRotOut();
      pushSFPOutToMdp(frame, target, out);
    }
  }
}

MBOOL prepareMdpFrameParam(NSCam::NSIoPipe::QParams* qparam,
                           MUINT32 refFrameInd,
                           const std::vector<SFPOutput>& mdpOuts) {
  if (mdpOuts.size() == 0) {
    return MFALSE;
  }
  MDPWrapper::OutCollection collect(mdpOuts);
  NSCam::NSIoPipe::FrameParams& frame = qparam->mvFrameParams.at(refFrameInd);
  // prepare reference frame
  prepareFrame(&frame, &collect);
  // put remain output to new frames
  while (!collect.isFinish()) {
    addNewFrame(qparam, refFrameInd);
    NSCam::NSIoPipe::FrameParams& f =
        qparam->mvFrameParams.at(qparam->mvFrameParams.size() - 1);
    prepareFrame(&f, &collect);
  }
  return MTRUE;
}

MBOOL prepareOneMdpFrameParam(NSCam::NSIoPipe::FrameParams* frame,
                              const std::vector<SFPOutput>& mdpOuts,
                              std::vector<SFPOutput>* remainList) {
  if (mdpOuts.size() == 0) {
    return MFALSE;
  }
  MDPWrapper::OutCollection collect(mdpOuts);
  // prepare reference frame
  prepareFrame(frame, &collect);
  // put remain output to remainList
  if (!collect.isFinish()) {
    collect.storeLeftOutputs(remainList);
  }
  return MTRUE;
}
#endif

MBOOL getOutBuffer(const NSCam::NSIoPipe::QParams& qparam,
                   IO_TYPE target,
                   NSCam::NSIoPipe::Output* output) {
  TRACE_FUNC_ENTER();
  unsigned count = 0;
  if (qparam.mvFrameParams.size()) {
    for (unsigned i = 0, size = qparam.mvFrameParams[0].mvOut.size(); i < size;
         ++i) {
      if (isTargetOutput(target, qparam.mvFrameParams[0].mvOut[i])) {
        if (++count == 1) {
          *output = qparam.mvFrameParams[0].mvOut[i];
        }
      }
    }
  }
  if (count > 1) {
    TRACE_FUNC("suspicious output number = %d, type = %d", count, target);
  }
  TRACE_FUNC_EXIT();
  return count >= 1;
}

MBOOL getOutBuffer(const SFPIOMap& ioMap, IO_TYPE target, SFPOutput* output) {
  TRACE_FUNC_ENTER();
  if (!ioMap.isValid()) {
    return MFALSE;
  }
  unsigned count = 0;
  for (auto&& out : ioMap.mOutList) {
    if (isTargetOutput(target, out)) {
      if (++count == 1) {
        *output = out;
      }
    }
  }
  if (count > 1) {
    MY_LOGW("suspicious output number = %d, type = %d", count, target);
  }
  TRACE_FUNC_EXIT();
  return count >= 1;
}

MBOOL getOutBuffer(const SFPIOMap& ioMap,
                   IO_TYPE target,
                   std::vector<SFPOutput>* outList) {
  TRACE_FUNC_ENTER();
  if (!ioMap.isValid()) {
    return MFALSE;
  }
  unsigned count = 0;
  for (auto&& out : ioMap.mOutList) {
    if (isTargetOutput(target, out)) {
      outList->push_back(out);
      count++;
    }
  }
  TRACE_FUNC_EXIT();
  return count >= 1;
}

MSize calcDsImgSize(const MSize& src) {
  TRACE_FUNC_ENTER();
  MSize result;

  if (src.w * 3 == src.h * 4) {
    result = MSize(320, 24);
  } else if (src.w * 9 == src.h * 16) {
    result = MSize(320, 180);
  } else if (src.w * 3 == src.h * 5) {
    result = MSize(320, 180);
  } else {
    result = MSize(320, 320 * src.h / src.w);
  }

  TRACE_FUNC_EXIT();
  return result;
}

MBOOL dumpToFile(std::shared_ptr<IImageBuffer> buffer, const char* fmt, ...) {
  MBOOL ret = MFALSE;
  if (buffer && fmt) {
    char filename[256];
    va_list arg;
    va_start(arg, fmt);
    vsprintf(filename, fmt, arg);
    va_end(arg);
    buffer->saveToFile(filename);
    ret = MTRUE;
  }
  return ret;
}

MBOOL is4K2K(const MSize& size) {
  return (size.w >= UHD_VR_WIDTH && size.h >= UHD_VR_HEIGHT);
}

MUINT32 align(MUINT32 val, MUINT32 bits) {
  // example: align 5 bits => align 32
  MUINT32 mask = (0x01 << bits) - 1;
  return (val + mask) & (~mask);
}

MVOID moveAppend(std::vector<SFPOutput>* source, std::vector<SFPOutput>* dest) {
  if (dest->empty()) {
    *dest = std::move(*source);
  } else {
    dest->reserve(dest->size() + source->size());
    std::move(std::begin(*source), std::end(*source),
              std::back_inserter(*dest));
    source->clear();
  }
}

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
