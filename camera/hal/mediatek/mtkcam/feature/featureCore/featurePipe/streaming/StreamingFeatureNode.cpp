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

#include <featurePipe/common/include/DebugControl.h>
#define PIPE_CLASS_TAG "Node"
#define PIPE_TRACE TRACE_STREAMING_FEATURE_NODE
#include <fcntl.h>
#include <featurePipe/common/include/PipeLog.h>
#include <featurePipe/streaming/StreamingFeatureNode.h>
#include <memory>

using NSCam::NSIoPipe::CrspInfo;
using NSCam::NSIoPipe::ExtraParam;
using NSCam::NSIoPipe::FEInfo;
using NSCam::NSIoPipe::FMInfo;
using NSCam::NSIoPipe::ModuleInfo;
using NSCam::NSIoPipe::PQParam;

#define MAKE_NAME_FROM_VA(name, len, fmt)     \
  if (name && len > 0) {                      \
    va_list ap;                               \
    va_start(ap, fmt);                        \
    if (0 >= vsnprintf(name, len, fmt, ap)) { \
      strncpy(name, "NA", len);               \
      name[len - 1] = 0;                      \
    }                                         \
    va_end(ap);                               \
  }

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
const char* StreamingFeatureDataHandler::ID2Name(DataID id) {
#define MAKE_NAME_CASE(name) \
  case name:                 \
    return #name;

  switch (id) {
    case ID_ROOT_ENQUE:
      return "root_enque";
    case ID_ROOT_TO_P2A:
      return "root_to_p2a";
    case ID_ROOT_TO_RSC:
      return "root_to_rsc";
    case ID_ROOT_TO_DEPTH:
      return "root_to_depth";
    case ID_P2A_TO_WARP_FULLIMG:
      return "p2a_to_warp";
    case ID_P2A_TO_EIS_P2DONE:
      return "p2a_to_eis_done";
    case ID_P2A_TO_EIS_FM:
      return "p2a_to_eis_fm";
    case ID_P2A_TO_PMDP:
      return "p2a_to_p2amdp";
    case ID_P2A_TO_HELPER:
      return "p2a_to_helper";
    case ID_PMDP_TO_HELPER:
      return "p2amdp_to_helper";
    case ID_BOKEH_TO_HELPER:
      return "bokeh_to_helper";
    case ID_WARP_TO_HELPER:
      return "warp_to_helper";
    case ID_EIS_TO_WARP:
      return "eis_to_warp";
    case ID_P2A_TO_VENDOR_FULLIMG:
      return "p2a_to_vendor";
    case ID_BOKEH_TO_VENDOR_FULLIMG:
      return "bokeh_to_vendor";
    case ID_VENDOR_TO_NEXT:
      return "vendor_to_next";
    case ID_VMDP_TO_NEXT_FULLIMG:
      return "vmdp_to_next";
    case ID_VMDP_TO_HELPER:
      return "vmdp_to_helper";
    case ID_RSC_TO_HELPER:
      return "rsc_to_helper";
    case ID_RSC_TO_EIS:
      return "rsc_to_eis";
    case ID_PREV_TO_DUMMY_FULLIMG:
      return "prev_to_dummy";
    case ID_DEPTH_TO_BOKEH:
      return "depth_to_bokeh";
    case ID_DEPTH_TO_VENDOR:
      return "depth_to_vendor";

    case ID_P2A_TO_FOV_FEFM:
      return "p2a_to_fov_fefm";
    case ID_P2A_TO_FOV_FULLIMG:
      return "p2a_to_fov_fullimg";
    case ID_FOV_TO_FOV_WARP:
      return "fov_to_fov_warp";
    case ID_FOV_TO_EIS_WARP:
      return "fov_to_eis_warp";
    case ID_FOV_WARP_TO_HELPER:
      return "fovwrp_to_helper";
    case ID_FOV_WARP_TO_VENDOR:
      return "fovwrp_to_vendor";
    case ID_P2A_TO_FOV_WARP:
      return "p2a_to_fov_warp";
    case ID_FOV_TO_EIS_FULLIMG:
      return "fov_to_eis_fullimg";
    case ID_P2A_TO_N3DP2:
      return "p2a_to_n3dp2";
    case ID_N3DP2_TO_N3D:
      return "n3dp2_to_n3d";
    case ID_N3D_TO_HELPER:
      return "n3d_to_helper";
    case ID_N3D_TO_VMDP:
      return "n3d_to_vmdp";
    case ID_RSC_TO_P2A:
      return "rsc_to_p2a";
    default:
      return "unknown";
  }
#undef MAKE_NAME_CASE
}

NodeSignal::NodeSignal() : mSignal(0), mStatus(0) {}

NodeSignal::~NodeSignal() {}

MVOID NodeSignal::setSignal(Signal signal) {
  std::unique_lock<std::mutex> lock(mMutex);
  mSignal |= signal;
  mCondition.notify_all();
}

MVOID NodeSignal::clearSignal(Signal signal) {
  std::lock_guard<std::mutex> lock(mMutex);
  mSignal &= ~signal;
}

MBOOL NodeSignal::getSignal(Signal signal) {
  std::lock_guard<std::mutex> lock(mMutex);
  return (mSignal & signal);
}

MVOID NodeSignal::waitSignal(Signal signal) {
  std::unique_lock<std::mutex> lock(mMutex);
  while (!(mSignal & signal)) {
    mCondition.wait(lock);
  }
}

MVOID NodeSignal::setStatus(Status status) {
  std::lock_guard<std::mutex> lock(mMutex);
  mStatus |= status;
}

MVOID NodeSignal::clearStatus(Status status) {
  std::lock_guard<std::mutex> lock(mMutex);
  mStatus &= ~status;
}

MBOOL NodeSignal::getStatus(Status status) {
  std::lock_guard<std::mutex> lock(mMutex);
  return (mStatus & status);
}

StreamingFeatureDataHandler::~StreamingFeatureDataHandler() {}

StreamingFeatureNode::StreamingFeatureNode(const char* name)
    : CamThreadNode(name),
      mSensorIndex(-1),
      mNodeDebugLV(0),
      mDebugScanLine(nullptr) {}

StreamingFeatureNode::~StreamingFeatureNode() {}

MBOOL StreamingFeatureNode::onInit() {
  mNodeDebugLV = getFormattedPropertyValue("debug.%s", this->getName());
  return MTRUE;
}

MVOID StreamingFeatureNode::setSensorIndex(MUINT32 sensorIndex) {
  mSensorIndex = sensorIndex;
}

MVOID StreamingFeatureNode::setPipeUsage(
    const StreamingFeaturePipeUsage& usage) {
  mPipeUsage = usage;
}

MVOID StreamingFeatureNode::setNodeSignal(
    const std::shared_ptr<NodeSignal>& nodeSignal) {
  mNodeSignal = nodeSignal;
}

MBOOL StreamingFeatureNode::dumpNddData(
    TuningUtils::FILE_DUMP_NAMING_HINT* hint,
    IImageBuffer* buffer,
    MUINT32 portIndex) {
  if (hint && buffer) {
    char fileName[256] = {0};
    extract(hint, buffer);

    if (portIndex == NSImageio::NSIspio::EPortIndex_IMG3O) {
      genFileName_YUV(fileName, sizeof(fileName), hint,
                      NSCam::TuningUtils::YUV_PORT_IMG3O);
    }

    MY_LOGD("dump to: %s", fileName);
    buffer->saveToFile(fileName);
  }
  return MTRUE;
}

IOPolicyType StreamingFeatureNode::getIOPolicy(
    StreamType /*stream*/, const StreamingReqInfo& /*reqInfo*/) const {
  return IOPOLICY_BYPASS;
}

MBOOL StreamingFeatureNode::getInputBufferPool(
    const StreamingReqInfo& /*reqInfo*/,
    std::shared_ptr<IBufferPool>* /*pool*/) {
  return MFALSE;
}

MVOID StreamingFeatureNode::drawScanLine(IImageBuffer* buffer) {
  if (mDebugScanLine == nullptr) {
    mDebugScanLine = std::make_unique<DebugScanLineImp>();
  }

  if (mDebugScanLine) {
    mDebugScanLine->drawScanLine(buffer->getImgSize().w, buffer->getImgSize().h,
                                 reinterpret_cast<void*>(buffer->getBufVA(0)),
                                 buffer->getBufSizeInBytes(0),
                                 buffer->getBufStridesInBytes(0));
  }
}

MVOID StreamingFeatureNode::printIO(const RequestPtr& request,
                                    const QParams& params) {
  MY_LOGD("params.mvFrameParams.siz = %d!", params.mvFrameParams.size());
  for (unsigned f = 0, fCount = params.mvFrameParams.size(); f < fCount; ++f) {
    for (unsigned i = 0, n = params.mvFrameParams[f].mvIn.size(); i < n; ++i) {
      unsigned index = params.mvFrameParams[f].mvIn[i].mPortID.index;
      MSize size = params.mvFrameParams[f].mvIn[i].mBuffer->getImgSize();
      MY_LOGD("sensor(%d) Frame %d(%d/%d) mvIn[%d] idx=%d size=(%d,%d)",
              mSensorIndex, request->mRequestNo, f, fCount, i, index, size.w,
              size.h);
    }
    for (unsigned i = 0, n = params.mvFrameParams[f].mvOut.size(); i < n; ++i) {
      unsigned index = params.mvFrameParams[f].mvOut[i].mPortID.index;
      MSize size = params.mvFrameParams[f].mvOut[i].mBuffer->getImgSize();
      MBOOL isGraphic =
          (getGraphicBufferAddr(params.mvFrameParams[f].mvOut[i].mBuffer) !=
           nullptr);
      MINT fmt = params.mvFrameParams[f].mvOut[i].mBuffer->getImgFormat();
      MUINT32 cap = params.mvFrameParams[f].mvOut[i].mPortID.capbility;
      MINT32 transform = params.mvFrameParams[f].mvOut[i].mTransform;
      MY_LOGD(
          "sensor(%d) Frame %d(%d/%d) mvOut[%d] idx=%d size=(%d,%d) fmt=%d, "
          "cap=%02x, isGraphic=%d transform=%d",
          mSensorIndex, request->mRequestNo, f, fCount, i, index, size.w,
          size.h, fmt, cap, isGraphic, transform);
    }
    for (unsigned i = 0, n = params.mvFrameParams[f].mvCropRsInfo.size(); i < n;
         ++i) {
      MCrpRsInfo crop = params.mvFrameParams[f].mvCropRsInfo[i];
      MY_LOGD("sensor(%d) Frame %d(%d/%d) crop[%d] " MCrpRsInfo_STR,
              mSensorIndex, request->mRequestNo, f, fCount, i,
              MCrpRsInfo_ARG(crop));
    }

    for (unsigned i = 0, n = params.mvFrameParams[f].mvModuleData.size(); i < n;
         ++i) {
      ModuleInfo info = params.mvFrameParams[f].mvModuleData[i];
      switch (info.moduleTag) {
        case EDipModule_SRZ1:
          MY_LOGD(
              "sensor(%d) Frame %d(%d/%d) moduleinfo[%d] SRZ1 " ModuleInfo_STR,
              mSensorIndex, request->mRequestNo, f, fCount, i,
              ModuleInfo_ARG(
                  (reinterpret_cast<_SRZ_SIZE_INFO_*>(info.moduleStruct))));
          break;
        case EDipModule_SRZ4:
          MY_LOGD(
              "sensor(%d) Frame %d(%d/%d) moduleinfo[%d] SRZ4 " ModuleInfo_STR,
              mSensorIndex, request->mRequestNo, f, fCount, i,
              ModuleInfo_ARG(
                  (reinterpret_cast<_SRZ_SIZE_INFO_*>(info.moduleStruct))));
          break;
        default:
          break;
      }
    }

    for (unsigned i = 0, n = params.mvFrameParams[f].mvExtraParam.size(); i < n;
         ++i) {
      PQParam* pqParam = NULL;
      ExtraParam ext = params.mvFrameParams[f].mvExtraParam[i];
      switch (ext.CmdIdx) {
        case NSIoPipe::EPIPE_FE_INFO_CMD:
          MY_LOGD(
              "sensor(%d) Frame %d(%d/%d) extra[%d] FE_CMD " ExtraParam_FE_STR,
              mSensorIndex, request->mRequestNo, f, fCount, i,
              ExtraParam_FE_ARG((reinterpret_cast<FEInfo*>(ext.moduleStruct))));
          break;
        case NSIoPipe::EPIPE_FM_INFO_CMD:
          MY_LOGD(
              "sensor(%d) Frame %d(%d/%d) extra[%d] FM_CMD " ExtraParam_FM_STR,
              mSensorIndex, request->mRequestNo, f, fCount, i,
              ExtraParam_FM_ARG((reinterpret_cast<FMInfo*>(ext.moduleStruct))));
          break;
        case NSIoPipe::EPIPE_MDP_PQPARAM_CMD:
          pqParam = reinterpret_cast<PQParam*>(ext.moduleStruct);
          MY_LOGD(
              "sensor(%d) Frame %d(%d/%d) extra[%d] PQ_CMD " ExtraParam_PQ_STR,
              mSensorIndex, request->mRequestNo, f, fCount, i,
              ExtraParam_PQ_ARG(pqParam));
#if MTK_DP_ENABLE
          if (pqParam->WDMAPQParam) {
            MY_LOGD(
                "sensor(%d) Frame %d(%d/%d) extra[%d] "
                "PQ_WDMA_CMD " DpPqParam_STR,
                mSensorIndex, request->mRequestNo, f, fCount, i,
                DpPqParam_ARG(
                    (reinterpret_cast<DpPqParam*>(pqParam->WDMAPQParam))));
          }
          if (pqParam->WROTPQParam) {
            MY_LOGD(
                "sensor(%d) Frame %d(%d/%d) extra[%d] "
                "PQ_WROT_CMD " DpPqParam_STR,
                mSensorIndex, request->mRequestNo, f, fCount, i,
                DpPqParam_ARG(
                    (reinterpret_cast<DpPqParam*>(pqParam->WROTPQParam))));
          }
#endif

          break;
        case NSIoPipe::EPIPE_IMG3O_CRSPINFO_CMD:
          MY_LOGD(
              "sensor(%d) Frame %d(%d/%d) extra[%d] "
              "CRSPINFO_CMD " ExtraParam_CRSPINFO_STR,
              mSensorIndex, request->mRequestNo, f, fCount, i,
              ExtraParam_CRSPINFO_ARG(
                  (reinterpret_cast<CrspInfo*>(ext.moduleStruct))));
          break;
        default:
          break;
      }
    }
  }
}

MBOOL StreamingFeatureNode::syncAndDump(const RequestPtr& request,
                                        const BasicImg& img,
                                        const char* fmt,
                                        ...) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  if (img.mBuffer != nullptr && fmt) {
    IImageBuffer* buffer = img.mBuffer->getImageBufferPtr();
    if (buffer != nullptr) {
      char name[256];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
      MAKE_NAME_FROM_VA(name, sizeof(name), fmt)
      buffer->syncCache(eCACHECTRL_INVALID);
#pragma clang diagnostic pop
      ret = dumpNamedData(request, buffer, name);
    }
  }

  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL StreamingFeatureNode::syncAndDump(const RequestPtr& request,
                                        const ImgBuffer& img,
                                        const char* fmt,
                                        ...) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  if (img != NULL && fmt) {
    IImageBuffer* buffer = img->getImageBufferPtr();
    if (buffer != nullptr) {
      char name[256];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
      MAKE_NAME_FROM_VA(name, sizeof(name), fmt)
      buffer->syncCache(eCACHECTRL_INVALID);
#pragma clang diagnostic pop
      ret = dumpNamedData(request, buffer, name);
    }
  }

  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL StreamingFeatureNode::dumpData(const RequestPtr& request,
                                     const ImgBuffer& buffer,
                                     const char* fmt,
                                     ...) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  if (buffer != nullptr && fmt) {
    char name[256];
    va_list ap;
    va_start(ap, fmt);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    if (0 >= vsnprintf(name, sizeof(name), fmt, ap)) {
      strncpy(name, "NA", sizeof(name));
      name[sizeof(name) - 1] = 0;
    }
#pragma clang diagnostic pop
    va_end(ap);
    ret = dumpNamedData(request, buffer->getImageBufferPtr(), name);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL StreamingFeatureNode::dumpData(const RequestPtr& request,
                                     const BasicImg& buffer,
                                     const char* fmt,
                                     ...) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  if (buffer.mBuffer != NULL && fmt) {
    char name[256];
    va_list ap;
    va_start(ap, fmt);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    if (0 >= vsnprintf(name, sizeof(name), fmt, ap)) {
      strncpy(name, "NA", sizeof(name));
      name[sizeof(name) - 1] = 0;
    }
#pragma clang diagnostic pop
    va_end(ap);
    ret = dumpNamedData(request, buffer.mBuffer->getImageBufferPtr(), name);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL StreamingFeatureNode::dumpData(const RequestPtr& request,
                                     IImageBuffer* buffer,
                                     const char* fmt,
                                     ...) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  if (buffer && fmt) {
    char name[256];
    va_list ap;
    va_start(ap, fmt);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    if (0 >= vsnprintf(name, sizeof(name), fmt, ap)) {
      strncpy(name, "NA", sizeof(name));
      name[sizeof(name) - 1] = 0;
    }
#pragma clang diagnostic pop
    va_end(ap);
    ret = dumpNamedData(request, buffer, name);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL StreamingFeatureNode::dumpNamedData(const RequestPtr& request,
                                          IImageBuffer* buffer,
                                          const char* name) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  if (buffer && name) {
    MUINT32 stride, pbpp, ibpp, width, height, size;
    MINT format = buffer->getImgFormat();
    stride = buffer->getBufStridesInBytes(0);
    pbpp = buffer->getPlaneBitsPerPixel(0);
    ibpp = buffer->getImgBitsPerPixel();
    size = buffer->getBufSizeInBytes(0);
    pbpp = pbpp ? pbpp : 8;
    width = stride * 8 / pbpp;
    width = width ? width : 1;
    ibpp = ibpp ? ibpp : 8;
    height = size / width;
    if (buffer->getPlaneCount() == 1) {
      height = height * 8 / ibpp;
    }

    char path[256];
    snprintf(path, sizeof(path), "/usr/local/%04d_r%04d_%s_%dx%d_%dx%d.%s.bin",
             request->mRequestNo, request->mRecordNo, name,
             buffer->getImgSize().w, buffer->getImgSize().h, width, height,
             Fmt2Name(format));

    TRACE_FUNC("dump to %s", path);
    buffer->saveToFile(path);
    ret = MTRUE;
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MUINT32 StreamingFeatureNode::dumpData(const char* buffer,
                                       MUINT32 size,
                                       const char* filename) {
  uint32_t writeCount = 0;
  int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
  if (fd < 0) {
    MY_LOGE("Cannot create file [%s]", filename);
  } else {
    for (int cnt = 0, nw = 0; writeCount < size; ++cnt) {
      nw = write(fd, buffer + writeCount, size - writeCount);
      if (nw < 0) {
        MY_LOGE("Cannot write to file [%s]", filename);
        break;
      }
      writeCount += nw;
    }
    ::close(fd);
  }
  return writeCount;
}

MBOOL StreamingFeatureNode::loadData(IImageBuffer* buffer,
                                     const char* filename) {
  MBOOL ret = MFALSE;
  if (buffer) {
    loadData(reinterpret_cast<char*>(buffer->getBufVA(0)), 0, filename);
    ret = MTRUE;
  }
  return MFALSE;
}

MUINT32 StreamingFeatureNode::loadData(char* buffer,
                                       size_t size,
                                       const char* filename) {
  uint32_t readCount = 0;
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    MY_LOGE("Cannot open file [%s]", filename);
  } else {
    if (size == 0) {
      off_t readSize = ::lseek(fd, 0, SEEK_END);
      size = (readSize < 0) ? 0 : readSize;
      ::lseek(fd, 0, SEEK_SET);
    }
    for (int cnt = 0, nr = 0; readCount < size; ++cnt) {
      nr = ::read(fd, buffer + readCount, size - readCount);
      if (nr < 0) {
        MY_LOGE("Cannot read from file [%s]", filename);
        break;
      }
      readCount += nr;
    }
    ::close(fd);
  }
  return readCount;
}

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
