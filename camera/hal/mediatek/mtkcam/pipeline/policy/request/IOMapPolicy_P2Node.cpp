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

#define LOG_TAG "mtkcam-P2NodeIOMapPolicy"

#include <memory>
#include <mtkcam/pipeline/policy/IIOMapPolicy.h>
//
#include <mtkcam/pipeline/hwnode/NodeId.h>
#include <mtkcam/pipeline/hwnode/StreamId.h>
//
#include "MyUtils.h"

/******************************************************************************
 *
 ******************************************************************************/

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {

/******************************************************************************
 *
 ******************************************************************************/
static bool getIsNeedImgo(
    StreamId_T streamid,
    NSCam::v3::pipeline::policy::iomap::RequestInputParams const& in,
    MSize rrzoSize) {
#define IS_IMGO(size, threshold) (size.w > threshold.w || size.h > threshold.h)
  for (const auto& n : in.pRequest_AppImageStreamInfo->vAppImage_Output_Proc) {
    if (streamid == n.first) {
      return IS_IMGO(n.second->getImgSize(), rrzoSize);
    }
  }
#undef IS_IMGO

  return false;
}

/******************************************************************************
 *
 ******************************************************************************/
static MERROR evaluateRequest_P2StreamNode(
    NSCam::v3::pipeline::policy::iomap::RequestOutputParams const& out,
    NSCam::v3::pipeline::policy::iomap::RequestInputParams const& in) {
  if (!in.pRequest_PipelineNodesNeed->needP2StreamNode || !in.isMainFrame) {
    MY_LOGD("No need P2StreamNode");
    return OK;
  }

  auto const& needP1Node = in.pRequest_PipelineNodesNeed->needP1Node;
  auto const& needP1Dma = *in.pRequest_NeedP1Dma;

  NSCam::v3::NSPipelineContext::IOMap imgoMap, rrzoMap, metaMap;
  std::shared_ptr<IImageStreamInfo> main1RrzoInfo;
  bool bMainHasImgo = false, bMainHasRrzo = false;
  for (size_t i = 0; i < needP1Node.size(); i++) {
    if (!needP1Node[i]) {
      continue;
    }

    auto& p1Info = (*in.pConfiguration_StreamInfo_P1)[i];
    const auto need_P1Dma = needP1Dma[i];
    const MBOOL isMain = (i == 0);

    if (isMain) {
      std::shared_ptr<IImageStreamInfo> imgoInfo =
          in.pRequest_AppImageStreamInfo->pAppImage_Output_Priv.get()
              ? in.pRequest_AppImageStreamInfo->pAppImage_Output_Priv
              : p1Info.pHalImage_P1_Imgo;

      main1RrzoInfo = p1Info.pHalImage_P1_Rrzo;

      if (need_P1Dma & P1_IMGO) {
        imgoMap.addIn(imgoInfo->getStreamId());
        bMainHasImgo = true;
      }

      if (need_P1Dma & P1_RRZO) {
        rrzoMap.addIn(main1RrzoInfo->getStreamId());
        bMainHasRrzo = true;
      }
    } else {
      std::shared_ptr<IImageStreamInfo> subRrzoInfo = p1Info.pHalImage_P1_Rrzo;
      std::shared_ptr<IImageStreamInfo> subImgoInfo = p1Info.pHalImage_P1_Imgo;
      if (subRrzoInfo != nullptr) {
        if ((need_P1Dma & P1_RRZO) && bMainHasImgo) {
          imgoMap.addIn(subRrzoInfo->getStreamId());
        }
        if ((need_P1Dma & P1_RRZO) && bMainHasRrzo) {
          rrzoMap.addIn(subRrzoInfo->getStreamId());
        }
      } else if (subImgoInfo != nullptr) {
        if ((need_P1Dma & P1_IMGO) && bMainHasImgo) {
          imgoMap.addIn(subImgoInfo->getStreamId());
        }
        if ((need_P1Dma & P1_IMGO) && bMainHasRrzo) {
          rrzoMap.addIn(subImgoInfo->getStreamId());
        }
      }
    }

    if (!in.pRequest_PipelineNodesNeed->needP2CaptureNode) {
      if (need_P1Dma & P1_LCSO) {
        imgoMap.addIn(p1Info.pHalImage_P1_Lcso->getStreamId());
        rrzoMap.addIn(p1Info.pHalImage_P1_Lcso->getStreamId());
      }
    }

    if (need_P1Dma & P1_RSSO) {
      imgoMap.addIn(p1Info.pHalImage_P1_Rsso->getStreamId());
      rrzoMap.addIn(p1Info.pHalImage_P1_Rsso->getStreamId());
    }

    metaMap.addIn(p1Info.pAppMeta_DynamicP1->getStreamId());
    metaMap.addIn(p1Info.pHalMeta_DynamicP1->getStreamId());
  }

  if (!(bMainHasImgo || bMainHasRrzo)) {
    MY_LOGE("No Imgo or Rrzo");
    return OK;
  }

  auto const vImageStreamId = *in.pvImageStreamId_from_StreamNode;
  if (!bMainHasImgo) {
    for (size_t i = 0; i < vImageStreamId.size(); i++) {
      rrzoMap.addOut(vImageStreamId[i]);
    }
  } else if (!bMainHasRrzo) {
    for (size_t i = 0; i < vImageStreamId.size(); i++) {
      imgoMap.addOut(vImageStreamId[i]);
    }
  } else {
    for (size_t i = 0; i < vImageStreamId.size(); i++) {
      if (getIsNeedImgo(vImageStreamId[i], in, main1RrzoInfo->getImgSize())) {
        imgoMap.addOut(vImageStreamId[i]);
      } else {
        rrzoMap.addOut(vImageStreamId[i]);
      }
    }
  }

  metaMap.addIn(
      in.pConfiguration_StreamInfo_NonP1->pAppMeta_Control->getStreamId());
  for (const auto n : (*in.pvMetaStreamId_from_StreamNode)) {
    metaMap.addOut(n);
  }

  NSCam::v3::NSPipelineContext::IOMapSet P2IO;
  if (imgoMap.sizeOut() > 0) {
    P2IO.add(imgoMap);
  }
  if (rrzoMap.sizeOut() > 0) {
    P2IO.add(rrzoMap);
  }

  (*out.pNodeIOMapImage)[eNODEID_P2StreamNode] = P2IO;
  (*out.pNodeIOMapMeta)[eNODEID_P2StreamNode] =
      NSCam::v3::NSPipelineContext::IOMapSet().add(metaMap);

  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
static MERROR evaluateRequest_P2CaptureNode(
    NSCam::v3::pipeline::policy::iomap::RequestOutputParams const& out,
    NSCam::v3::pipeline::policy::iomap::RequestInputParams const& in) {
  if (!in.pRequest_PipelineNodesNeed->needP2CaptureNode) {
    MY_LOGD("No need P2CaptureNode");
    return OK;
  }
  NSCam::v3::NSPipelineContext::IOMap imgoMap, metaMap;
  bool bImgo = false;
  std::shared_ptr<IImageStreamInfo> imgoInfo;
  auto const& needP1Node = in.pRequest_PipelineNodesNeed->needP1Node;
  auto const& needP1Dma = *in.pRequest_NeedP1Dma;

  for (size_t i = 0; i < needP1Node.size(); i++) {
    if (!needP1Node[i]) {
      continue;
    }

    auto& p1Info = (*in.pConfiguration_StreamInfo_P1)[i];
    if (i == 0) {
      if (in.pRequest_AppImageStreamInfo->pAppImage_Output_Priv) {
        imgoInfo = in.pRequest_AppImageStreamInfo->pAppImage_Output_Priv;
      } else if (in.pRequest_AppImageStreamInfo->pAppImage_Input_Priv) {
        imgoInfo = in.pRequest_AppImageStreamInfo->pAppImage_Input_Priv;
      } else if (in.pRequest_AppImageStreamInfo->pAppImage_Input_Yuv) {
        imgoInfo = in.pRequest_AppImageStreamInfo->pAppImage_Input_Yuv;
      } else {
        imgoInfo = p1Info.pHalImage_P1_Imgo;
      }
    } else {
      imgoInfo = p1Info.pHalImage_P1_Imgo;
    }

    auto const need_P1Dma = needP1Dma[i];

    if (need_P1Dma & P1_IMGO) {
      imgoMap.addIn(imgoInfo->getStreamId());
      bImgo = true;
    }

    if (!in.pRequest_AppImageStreamInfo->pAppImage_Input_Yuv) {
      if (need_P1Dma & P1_LCSO) {
        imgoMap.addIn(p1Info.pHalImage_P1_Lcso->getStreamId());
      }
    }

    metaMap.addIn(p1Info.pAppMeta_DynamicP1->getStreamId());
    metaMap.addIn(p1Info.pHalMeta_DynamicP1->getStreamId());
  }

  auto const vImageStreamId = *in.pvImageStreamId_from_CaptureNode;

  if (!bImgo) {
    MY_LOGE("No Imgo");
    return OK;
  }
  if (in.isMainFrame) {
    for (size_t i = 0; i < vImageStreamId.size(); i++) {
      imgoMap.addOut(vImageStreamId[i]);
    }
  }

  metaMap.addIn(
      in.pConfiguration_StreamInfo_NonP1->pAppMeta_Control->getStreamId());
  for (const auto n : (*in.pvMetaStreamId_from_CaptureNode)) {
    metaMap.addOut(n);
  }

  (*out.pNodeIOMapImage)[eNODEID_P2CaptureNode] =
      NSCam::v3::NSPipelineContext::IOMapSet().add(imgoMap);
  (*out.pNodeIOMapMeta)[eNODEID_P2CaptureNode] =
      NSCam::v3::NSPipelineContext::IOMapSet().add(metaMap);

  return OK;
}

/******************************************************************************
 * Make a function target as a policy - default version
 ******************************************************************************/
FunctionType_IOMapPolicy_P2Node makePolicy_IOMap_P2Node_Default() {
  return [](NSCam::v3::pipeline::policy::iomap::RequestOutputParams const& out,
            NSCam::v3::pipeline::policy::iomap::RequestInputParams const& in)
             -> int {
    evaluateRequest_P2StreamNode(out, in);
    evaluateRequest_P2CaptureNode(out, in);
    return OK;
  };
}

};  // namespace policy
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
