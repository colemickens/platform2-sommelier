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

#define LOG_TAG "mtkcam-NonP2NodeIOMapPolicy"

#include <compiler.h>
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
static MERROR evaluateRequest_Jpeg(
    NSCam::v3::pipeline::policy::iomap::RequestOutputParams const& out,
    NSCam::v3::pipeline::policy::iomap::RequestInputParams const& in) {
  if (!in.pRequest_PipelineNodesNeed->needJpegNode ||
      !in.pConfiguration_StreamInfo_NonP1->pHalImage_Jpeg_YUV.get() ||
      !in.isMainFrame) {
    MY_LOGD("No need Jpeg node");
    return OK;
  }
  //
  StreamId_T streamid =
      in.pConfiguration_StreamInfo_NonP1->pHalImage_Jpeg_YUV->getStreamId();
  bool isStreamNode = false;
  for (const auto n : (*in.pvImageStreamId_from_StreamNode)) {
    if (n == streamid) {
      isStreamNode = true;
      break;
    }
  }
  //
  NSCam::v3::NSPipelineContext::IOMap JpegMap;
  JpegMap.addIn(streamid);
  if (in.pRequest_HalImage_Thumbnail_YUV != nullptr) {
    JpegMap.addIn(in.pRequest_HalImage_Thumbnail_YUV->getStreamId());
  }
  JpegMap.addOut(in.pRequest_AppImageStreamInfo->pAppImage_Jpeg->getStreamId());
  //
  (*out.pNodeIOMapImage)[eNODEID_JpegNode] =
      NSCam::v3::NSPipelineContext::IOMapSet().add(JpegMap);
  (*out.pNodeIOMapMeta)[eNODEID_JpegNode] =
      NSCam::v3::NSPipelineContext::IOMapSet().add(
          NSCam::v3::NSPipelineContext::IOMap()
              .addIn(in.pConfiguration_StreamInfo_NonP1->pAppMeta_Control
                         ->getStreamId())
              .addIn(isStreamNode
                         ? in.pConfiguration_StreamInfo_NonP1
                               ->pHalMeta_DynamicP2StreamNode->getStreamId()
                         : in.pConfiguration_StreamInfo_NonP1
                               ->pHalMeta_DynamicP2CaptureNode->getStreamId())
              .addOut(in.pConfiguration_StreamInfo_NonP1->pAppMeta_DynamicJpeg
                          ->getStreamId()));
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
static MERROR evaluateRequest_FD(
    NSCam::v3::pipeline::policy::iomap::RequestOutputParams const& out,
    NSCam::v3::pipeline::policy::iomap::RequestInputParams const& in) {
  if (!in.pRequest_PipelineNodesNeed->needFDNode || !in.isMainFrame) {
    MY_LOGD("No need FD node");
    return OK;
  }
  //
  (*out.pNodeIOMapImage)[eNODEID_FDNode] =
      NSCam::v3::NSPipelineContext::IOMapSet().add(
          NSCam::v3::NSPipelineContext::IOMap().addIn(
              in.pConfiguration_StreamInfo_NonP1->pHalImage_FD_YUV
                  ->getStreamId()));
  (*out.pNodeIOMapMeta)[eNODEID_FDNode] =
      NSCam::v3::NSPipelineContext::IOMapSet().add(
          NSCam::v3::NSPipelineContext::IOMap()
              .addIn(in.pConfiguration_StreamInfo_NonP1->pAppMeta_Control
                         ->getStreamId())
              .addIn(in.pConfiguration_StreamInfo_NonP1
                         ->pHalMeta_DynamicP2StreamNode->getStreamId())
              .addOut(in.pConfiguration_StreamInfo_NonP1->pAppMeta_DynamicFD
                          ->getStreamId()));
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
static MERROR evaluateRequest_Pass1(
    NSCam::v3::pipeline::policy::iomap::RequestOutputParams const& out,
    NSCam::v3::pipeline::policy::iomap::RequestInputParams const& in) {
  auto const& needP1Node = in.pRequest_PipelineNodesNeed->needP1Node;
  auto const& needP1Dma = *in.pRequest_NeedP1Dma;
  for (size_t i = 0; i < needP1Node.size(); i++) {
    if (!needP1Node[i]) {
      continue;
    }
    if (CC_UNLIKELY(i >= needP1Dma.size())) {
      continue;
    }
    auto const need_P1Dma = needP1Dma[i];

    NSCam::v3::NSPipelineContext::IOMap P1Map;
    NodeId_T nodeId = (i == 1 ? eNODEID_P1Node_main2 : eNODEID_P1Node);
    switch (i) {
      case 0: {
        if (in.pRequest_AppImageStreamInfo->pAppImage_Input_Priv != nullptr &&
            in.isMainFrame) {
          P1Map.addIn(in.pRequest_AppImageStreamInfo->pAppImage_Input_Priv
                          ->getStreamId());
        } else if (in.pRequest_AppImageStreamInfo->pAppImage_Input_Yuv !=
                       nullptr &&
                   in.isMainFrame) {
          P1Map.addIn(in.pRequest_AppImageStreamInfo->pAppImage_Input_Yuv
                          ->getStreamId());
        } else {
          if (need_P1Dma & P1_IMGO) {
            if (in.pRequest_AppImageStreamInfo->pAppImage_Output_Priv.get()) {
              P1Map.addOut(in.pRequest_AppImageStreamInfo->pAppImage_Output_Priv
                               ->getStreamId());
            } else {
              P1Map.addOut((*in.pConfiguration_StreamInfo_P1)[i]
                               .pHalImage_P1_Imgo->getStreamId());
            }
          }
          if (need_P1Dma & P1_RRZO) {
            P1Map.addOut((*in.pConfiguration_StreamInfo_P1)[i]
                             .pHalImage_P1_Rrzo->getStreamId());
          }
          if (need_P1Dma & P1_LCSO) {
            P1Map.addOut((*in.pConfiguration_StreamInfo_P1)[i]
                             .pHalImage_P1_Lcso->getStreamId());
          }
          if (need_P1Dma & P1_RSSO) {
            P1Map.addOut((*in.pConfiguration_StreamInfo_P1)[i]
                             .pHalImage_P1_Rsso->getStreamId());
          }
        }
      } break;

      default:
      case 1: { /* P1 Main2 */
        if (need_P1Dma & P1_IMGO) {
          P1Map.addOut((*in.pConfiguration_StreamInfo_P1)[i]
                           .pHalImage_P1_Imgo->getStreamId());
        }
        if (need_P1Dma & P1_RRZO) {
          P1Map.addOut((*in.pConfiguration_StreamInfo_P1)[i]
                           .pHalImage_P1_Rrzo->getStreamId());
        }
        if (need_P1Dma & P1_LCSO) {
          P1Map.addOut((*in.pConfiguration_StreamInfo_P1)[i]
                           .pHalImage_P1_Lcso->getStreamId());
        }
        if (need_P1Dma & P1_RSSO) {
          P1Map.addOut((*in.pConfiguration_StreamInfo_P1)[i]
                           .pHalImage_P1_Rsso->getStreamId());
        }
      } break;
    }

    // dummy frame does not need image IOMap
    if (!in.isDummyFrame) {
      (*out.pNodeIOMapImage)[nodeId] =
          NSCam::v3::NSPipelineContext::IOMapSet().add(P1Map);
    }

    (*out.pNodeIOMapMeta)[nodeId] =
        NSCam::v3::NSPipelineContext::IOMapSet().add(
            NSCam::v3::NSPipelineContext::IOMap()
                .addIn(in.pConfiguration_StreamInfo_NonP1->pAppMeta_Control
                           ->getStreamId())
                .addIn((*in.pConfiguration_StreamInfo_P1)[i]
                           .pHalMeta_Control->getStreamId())
                .addOut((*in.pConfiguration_StreamInfo_P1)[i]
                            .pAppMeta_DynamicP1->getStreamId())
                .addOut((*in.pConfiguration_StreamInfo_P1)[i]
                            .pHalMeta_DynamicP1->getStreamId()));
  }
  return OK;
}

/******************************************************************************
 * Make a function target as a policy - default version
 ******************************************************************************/
FunctionType_IOMapPolicy_NonP2Node makePolicy_IOMap_NonP2Node_Default() {
  return [](NSCam::v3::pipeline::policy::iomap::RequestOutputParams const& out,
            NSCam::v3::pipeline::policy::iomap::RequestInputParams const& in)
             -> int {
    evaluateRequest_Jpeg(out, in);
    evaluateRequest_FD(out, in);
    evaluateRequest_Pass1(out, in);
    return OK;
  };
}

};  // namespace policy
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
