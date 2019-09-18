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

#define LOG_TAG "mtkcam-PipelineFrameBuilder"

#include <impl/PipelineFrameBuilder.h>
#include <memory>
#include <string>
//
#include "MyUtils.h"

using NSCam::v3::NSPipelineContext::IOMapSet;
using NSCam::v3::NSPipelineContext::RequestBuilder;
/******************************************************************************
 *
 ******************************************************************************/

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace model {

/******************************************************************************
 *
 ******************************************************************************/
static auto getDebugLogLevel() -> int32_t {
  return ::property_get_int32("persist.vendor.debug.camera.log", 0);
}
static int32_t gLogLevel = getDebugLogLevel();

/******************************************************************************
 *
 ******************************************************************************/
static void print(BuildPipelineFrameInputParams const& o) {
  MY_LOGI("requestNo %d", o.requestNo);
  if (auto p = o.pAppImageStreamBuffers) {
    std::string os;
    os += "App image stream buffers=";
    os += p->toString();
    MY_LOGI("%s", os.c_str());
  }
  if (auto p = o.pAppMetaStreamBuffers) {
    std::string os;
    os += "App meta stream buffers=";
    for (auto const& v : *p) {
      os += "\n    ";
      os += v->toString();
    }
    MY_LOGI("%s", os.c_str());
  }
  if (auto p = o.pHalImageStreamBuffers) {
    std::string os;
    os += "Hal image stream buffers=";
    for (auto const& v : *p) {
      os += "\n    ";
      os += v->toString();
    }
    MY_LOGI("%s", os.c_str());
  }
  if (auto p = o.pHalMetaStreamBuffers) {
    std::string os;
    os += "Hal meta stream buffers=";
    for (auto const& v : *p) {
      os += "\n    ";
      os += v->toString();
    }
    MY_LOGI("%s", os.c_str());
  }

  if (auto p = o.pvUpdatedImageStreamInfo) {
    std::string os;
    os += "Updated image stream info=";
    for (auto const& v : *p) {
      os += "\n    ";
      os += v.second->toString();
    }
    MY_LOGI("%s", os.c_str());
  }

  if (auto p = o.pnodeSet) {
    std::string os;
    os += ".nodes={ ";
    for (auto const& v : *p) {
      os += base::StringPrintf("%#" PRIxPTR " ", v);
    }
    os += "}";
    MY_LOGI("%s", os.c_str());
  }

  if (auto p = o.pRootNodes) {
    std::string os;
    os += ".root=";
    os += NSCam::v3::NSPipelineContext::toString(*p);
    MY_LOGI("%s", os.c_str());
  }

  if (auto p = o.pEdges) {
    std::string os;
    os += ".edges=";
    os += toString(*p);
    MY_LOGI("%s", os.c_str());
  }

  if (auto p = o.pnodeIOMapImage) {
    std::string os;
    os += "IOMap(image)";
    for (auto const& v : *p) {
      os += base::StringPrintf("\n    <nodeId %#" PRIxPTR ">=", v.first);
      os += toString(v.second);
    }
    MY_LOGI("%s", os.c_str());
  }

  if (auto p = o.pnodeIOMapMeta) {
    std::string os;
    os += "IOMap(meta)";
    for (auto const& v : *p) {
      os += base::StringPrintf("\n    <nodeId %#" PRIxPTR ">=", v.first);
      os += toString(v.second);
    }
    MY_LOGI("%s", os.c_str());
  }

  if (auto p = o.pCallback.expired()) {
    std::string os;
    os += base::StringPrintf(".pCallback=%p", reinterpret_cast<void*>(p));
    MY_LOGI("%s", os.c_str());
  }

  if (auto p = o.pPipelineContext.get()) {
    p->dumpState({});
  }
}

/******************************************************************************
 *
 ******************************************************************************/
static void dumpToLog(BuildPipelineFrameInputParams const& o) {
  print(o);
}

/******************************************************************************
 *
 ******************************************************************************/
auto buildPipelineFrame(std::shared_ptr<IPipelineFrame>* out,
                        BuildPipelineFrameInputParams const& in) -> int {
  CAM_TRACE_NAME(__FUNCTION__);

  RequestBuilder builder;
  builder.setReprocessFrame(in.bReprocessFrame);
  builder.setRootNode(*in.pRootNodes);
  builder.setNodeEdges(*in.pEdges);

  if (auto&& p = in.pvUpdatedImageStreamInfo) {
    for (auto&& s : *p) {
      builder.replaceStreamInfo(s.first, s.second);
    }
  }

  // IOMap of Image/Meta
  for (auto key : *(in.pnodeSet)) {
    auto const& it_image = in.pnodeIOMapImage->find(key);
    auto const& it_meta = in.pnodeIOMapMeta->find(key);
    builder.setIOMap(
        key,
        (it_image != in.pnodeIOMapImage->end()) ? it_image->second
                                                : IOMapSet::buildEmptyIOMap(),
        (it_meta != in.pnodeIOMapMeta->end()) ? it_meta->second
                                              : IOMapSet::buildEmptyIOMap());
  }

  // set StreamBuffers of Image/Meta
  // app images
  if (auto&& pAppImageStreamBuffers = in.pAppImageStreamBuffers) {
    for (auto&& s : pAppImageStreamBuffers->vAppImage_Output_Proc) {
      if (s.second) {
        builder.setImageStreamBuffer(s.first, s.second);
      }
    }
#define SET_APP_IMAGE(_pSBuffer_, _builder_)                                \
  do {                                                                      \
    auto&& p = _pSBuffer_;                                                  \
    if (p) {                                                                \
      _builder_.setImageStreamBuffer(p->getStreamInfo()->getStreamId(), p); \
      MY_LOGD("setImageStreamBuffer for app image (%lld)",                  \
              p->getStreamInfo()->getStreamId());                           \
    }                                                                       \
  } while (0)

    SET_APP_IMAGE(pAppImageStreamBuffers->pAppImage_Input_Yuv, builder);
    SET_APP_IMAGE(pAppImageStreamBuffers->pAppImage_Output_Priv, builder);
    SET_APP_IMAGE(pAppImageStreamBuffers->pAppImage_Input_Priv, builder);
    SET_APP_IMAGE(pAppImageStreamBuffers->pAppImage_Jpeg, builder);

#undef SET_APP_IMAGE
  }

  // app meta
  if (auto&& pSBuffers = in.pAppMetaStreamBuffers) {
    for (auto&& pSBuffer : *pSBuffers) {
      builder.setMetaStreamBuffer(pSBuffer->getStreamInfo()->getStreamId(),
                                  pSBuffer);
    }
  }

  // hal image
  if (auto&& pSBuffers = in.pHalImageStreamBuffers) {
    for (auto&& pSBuffer : *pSBuffers) {
      builder.setImageStreamBuffer(pSBuffer->getStreamInfo()->getStreamId(),
                                   pSBuffer);
    }
  }

  // hal meta
  if (auto&& pSBuffers = in.pHalMetaStreamBuffers) {
    for (auto&& pSBuffer : *pSBuffers) {
      builder.setMetaStreamBuffer(pSBuffer->getStreamInfo()->getStreamId(),
                                  pSBuffer);
    }
  }

  std::shared_ptr<IPipelineFrame> pFrame =
      builder.updateFrameCallback(in.pCallback)
          .build(in.requestNo, in.pPipelineContext);
  if (CC_UNLIKELY(pFrame == nullptr)) {
    MY_LOGE("IPipelineFrame build fail(%u)", in.requestNo);
    dumpToLog(in);
    return -EINVAL;
  }

  *out = pFrame;

  if (CC_UNLIKELY(gLogLevel >= 1)) {
    dumpToLog(in);
  }

  return OK;
}
};  // namespace model
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
