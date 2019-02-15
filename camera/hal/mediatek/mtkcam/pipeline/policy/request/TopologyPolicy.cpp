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

#define LOG_TAG "mtkcam-TopologyPolicy"

#include <compiler.h>
#include <mtkcam/pipeline/policy/ITopologyPolicy.h>

#include "MyUtils.h"

#include <mtkcam/pipeline/hwnode/NodeId.h>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {

/**
 * Make a function target as a policy - default version
 */
FunctionType_TopologyPolicy makePolicy_Topology_Default() {
  return [](topology::RequestOutputParams& out,
            topology::RequestInputParams const& in) -> int {
    auto const pCfgNodesNeed = in.pConfiguration_PipelineNodesNeed;
    auto const pCfgStream_NonP1 = in.pConfiguration_StreamInfo_NonP1;
    if (CC_UNLIKELY(!pCfgNodesNeed || !pCfgStream_NonP1)) {
      MY_LOGE("null configuration params");
      return -EINVAL;
    }
    if (in.pConfiguration_PipelineNodesNeed->needP1Node.size() > 2) {
      MY_LOGE("current flow not support more than 2 p1node.");
      return -EINVAL;
    }

    PipelineNodesNeed* pNodesNeed = out.pNodesNeed;
    std::vector<NodeId_T>* pNodeSet = out.pNodeSet;
    topology::RequestOutputParams::NodeSet* pRootNodes = out.pRootNodes;
    topology::RequestOutputParams::NodeEdgeSet* pEdges = out.pEdges;

    pRootNodes->add(eNODEID_P1Node);
    pNodesNeed->needP1Node.push_back(true);
    pNodeSet->push_back(eNODEID_P1Node);
    MBOOL needConnectMain2ToStreaming = MFALSE;
    MBOOL needConnectMain2ToCapture = MFALSE;
    if (in.pConfiguration_PipelineNodesNeed->needP1Node.size() > 1) {
      pRootNodes->add(eNODEID_P1Node_main2);
      pNodesNeed->needP1Node.push_back(true);
      pNodeSet->push_back(eNODEID_P1Node_main2);
      needConnectMain2ToStreaming = MTRUE;
      needConnectMain2ToCapture = MTRUE;
    }

    if (in.isDummyFrame) {
      return OK;
    }

    auto const pReqImageSet = in.pRequest_AppImageStreamInfo;
    // jpeg
    if (pReqImageSet != nullptr && pReqImageSet->pAppImage_Jpeg) {
      bool found = false;
      const auto& streamId =
          pCfgStream_NonP1->pHalImage_Jpeg_YUV->getStreamId();
      for (const auto& s : *(in.pvImageStreamId_from_CaptureNode)) {
        if (s == streamId) {
          pEdges->addEdge(eNODEID_P2CaptureNode, eNODEID_JpegNode);
          found = true;
          break;
        }
      }

      if (!found) {
        for (const auto& s : *(in.pvImageStreamId_from_StreamNode)) {
          if (s == streamId) {
            pEdges->addEdge(eNODEID_P2StreamNode, eNODEID_JpegNode);
            found = true;
            break;
          }
        }
      }

      if (!found) {
        MY_LOGE("no p2 streaming&capture node w/ jpeg output");
        return -EINVAL;
      }
      pNodesNeed->needJpegNode = true;
      pNodeSet->push_back(eNODEID_JpegNode);
    }

    // fd
    if (in.isFdEnabled && in.needP2StreamNode) {
      pNodesNeed->needFDNode = true;
      pNodeSet->push_back(eNODEID_FDNode);
      pEdges->addEdge(eNODEID_P2StreamNode, eNODEID_FDNode);
    }

    // p2 streaming
    if (in.needP2StreamNode) {
      pNodesNeed->needP2StreamNode = true;
      pNodeSet->push_back(eNODEID_P2StreamNode);
      pEdges->addEdge(eNODEID_P1Node, eNODEID_P2StreamNode);
      if (needConnectMain2ToStreaming) {
        pEdges->addEdge(eNODEID_P1Node_main2, eNODEID_P2StreamNode);
      }
    }

    // p2 capture
    if (in.needP2CaptureNode) {
      pNodesNeed->needP2CaptureNode = true;
      pNodeSet->push_back(eNODEID_P2CaptureNode);
      pEdges->addEdge(eNODEID_P1Node, eNODEID_P2CaptureNode);
      if (needConnectMain2ToCapture) {
        pEdges->addEdge(eNODEID_P1Node_main2, eNODEID_P2CaptureNode);
      }
    }

    return OK;
  };
}

};  // namespace policy
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
