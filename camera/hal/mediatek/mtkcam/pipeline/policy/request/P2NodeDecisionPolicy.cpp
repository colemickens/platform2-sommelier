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

#define LOG_TAG "mtkcam-P2NodeDecisionPolicy"

#include <mtkcam/pipeline/policy/IP2NodeDecisionPolicy.h>
//
#include "MyUtils.h"
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
/******************************************************************************
 *
 ******************************************************************************/
#define MAX_IMAGE_SIZE(_target, _compete) \
  do {                                    \
    if (_compete.size() > _target.size()) \
      _target = _compete;                 \
  } while (0)

/******************************************************************************
 *
 ******************************************************************************/
static auto handleFD(
    NSCam::v3::pipeline::policy::p2nodedecision::RequestOutputParams* out,
    NSCam::v3::pipeline::policy::p2nodedecision::RequestInputParams const* in)
    -> void {
  //  FD is enable only when:
  //  - A FD request is sent from users, and
  //  - P2StreamNode is enabled due to other output image streams.
  if (in->isFdEnabled) {
    out->vImageStreamId_from_StreamNode.push_back(
        in->pConfiguration_StreamInfo_NonP1->pHalImage_FD_YUV->getStreamId());
    out->needP2StreamNode = true;
  }
}

/******************************************************************************
 *
 ******************************************************************************/
static auto handleAllImageStream_ExceptFD(
    std::vector<NSCam::v3::StreamId_T>* vImageId,
    MSize* maxSize,
    NSCam::v3::pipeline::policy::p2nodedecision::RequestInputParams const* in)
    -> void {
  auto const pConfiguration_StreamInfo_NonP1 =
      in->pConfiguration_StreamInfo_NonP1;
  auto const pRequest_AppImageStreamInfo = in->pRequest_AppImageStreamInfo;

  for (auto const& it : pRequest_AppImageStreamInfo->vAppImage_Output_Proc) {
    vImageId->push_back(it.first);
    MSize srcSize = it.second->getImgSize();
    if (srcSize.size() > maxSize->size())
      maxSize = &srcSize;
  }
  if (pRequest_AppImageStreamInfo->pAppImage_Jpeg != nullptr) {
    vImageId->push_back(
        pConfiguration_StreamInfo_NonP1->pHalImage_Jpeg_YUV->getStreamId());
    MSize srcSize =
        pConfiguration_StreamInfo_NonP1->pHalImage_Jpeg_YUV->getImgSize();
    if (srcSize.size() > maxSize->size())
      maxSize = &srcSize;
    if (in->needThumbnail) {
      vImageId->push_back(pConfiguration_StreamInfo_NonP1
                              ->pHalImage_Thumbnail_YUV->getStreamId());
    }
  }
}

/******************************************************************************
 *
 ******************************************************************************/
static auto decideStreamOut(
    NSCam::v3::pipeline::policy::p2nodedecision::RequestOutputParams* out,
    NSCam::v3::pipeline::policy::p2nodedecision::RequestInputParams const* in)
    -> void {
  auto const pConfiguration_StreamInfo_NonP1 =
      in->pConfiguration_StreamInfo_NonP1;
  auto const pRequest_AppImageStreamInfo = in->pRequest_AppImageStreamInfo;

  auto& needP2CaptureNode = out->needP2CaptureNode;
  auto& needP2StreamNode = out->needP2StreamNode;

  needP2CaptureNode = false;
  needP2StreamNode = false;
  {
    auto& imageIds_Capture = out->vImageStreamId_from_CaptureNode;
    auto& imageIds_Stream = out->vImageStreamId_from_StreamNode;

    for (auto const& it : pRequest_AppImageStreamInfo->vAppImage_Output_Proc) {
      if (pRequest_AppImageStreamInfo->pAppImage_Input_Yuv ||
          pRequest_AppImageStreamInfo->pAppImage_Input_Priv) {
        needP2CaptureNode = true;
        imageIds_Capture.push_back(it.first);
        MAX_IMAGE_SIZE(out->maxP2CaptureSize, it.second->getImgSize());
      } else if (!(it.second->getUsageForConsumer() &
                   GRALLOC_USAGE_HW_TEXTURE) &&
                 !(it.second->getUsageForConsumer() &
                   GRALLOC_USAGE_HW_VIDEO_ENCODER) &&
                 !(it.second->getUsageForConsumer() &
                   GRALLOC_USAGE_HW_COMPOSER)) {
        needP2CaptureNode = true;
        imageIds_Capture.push_back(it.first);
        MAX_IMAGE_SIZE(out->maxP2CaptureSize, it.second->getImgSize());
      } else {
        needP2StreamNode = true;
        imageIds_Stream.push_back(it.first);
        MAX_IMAGE_SIZE(out->maxP2StreamSize, it.second->getImgSize());
      }
    }
    if (pRequest_AppImageStreamInfo->pAppImage_Jpeg != nullptr) {
      NSCam::v3::StreamId_T streamId_Thumbnail_YUV =
          (in->needThumbnail) ? pConfiguration_StreamInfo_NonP1
                                    ->pHalImage_Thumbnail_YUV->getStreamId()
                              : (-1);

      auto const& stream_Jpeg_YUV =
          pConfiguration_StreamInfo_NonP1->pHalImage_Jpeg_YUV;
      needP2CaptureNode = true;
      imageIds_Capture.push_back(stream_Jpeg_YUV->getStreamId());
      MAX_IMAGE_SIZE(out->maxP2CaptureSize, stream_Jpeg_YUV->getImgSize());
      if (streamId_Thumbnail_YUV >= 0) {
        imageIds_Capture.push_back(streamId_Thumbnail_YUV);
      }
    }
    if (needP2StreamNode) {
      handleFD(out, in);
    }
  }

  // metadata
  {
    auto& metaIds_Capture = out->vMetaStreamId_from_CaptureNode;
    auto& metaIds_Stream = out->vMetaStreamId_from_StreamNode;

    if (needP2CaptureNode) {
      metaIds_Capture.push_back(
          pConfiguration_StreamInfo_NonP1->pHalMeta_DynamicP2CaptureNode
              ->getStreamId());
      metaIds_Capture.push_back(
          pConfiguration_StreamInfo_NonP1->pAppMeta_DynamicP2CaptureNode
              ->getStreamId());
    }
    if (needP2StreamNode) {
      metaIds_Stream.push_back(
          pConfiguration_StreamInfo_NonP1->pHalMeta_DynamicP2StreamNode
              ->getStreamId());
      if (!needP2CaptureNode) {
        metaIds_Stream.push_back(
            pConfiguration_StreamInfo_NonP1->pAppMeta_DynamicP2StreamNode
                ->getStreamId());
      }
    }
  }
}

/******************************************************************************
 *
 ******************************************************************************/
static auto evaluateRequest(
    NSCam::v3::pipeline::policy::p2nodedecision::RequestOutputParams* out,
    NSCam::v3::pipeline::policy::p2nodedecision::RequestInputParams const& in)
    -> int {
  auto const pConfiguration_StreamInfo_NonP1 =
      in.pConfiguration_StreamInfo_NonP1;

  if (!(in.hasP2CaptureNode) && !(in.hasP2StreamNode)) {
    out->needP2CaptureNode = false;
    out->needP2StreamNode = false;
    MY_LOGD("didn't have p2 node.....");
    return NSCam::OK;
  }
  if (!(in.hasP2CaptureNode)) {
    MY_LOGD("Only use P2S node");

    handleAllImageStream_ExceptFD(&out->vImageStreamId_from_StreamNode,
                                  &out->maxP2StreamSize, &in);

    out->needP2CaptureNode = false;
    out->needP2StreamNode = !out->vImageStreamId_from_StreamNode.empty();
    handleFD(out, &in);

    if (out->needP2StreamNode) {
      out->vMetaStreamId_from_StreamNode.push_back(
          pConfiguration_StreamInfo_NonP1->pHalMeta_DynamicP2StreamNode
              ->getStreamId());
      out->vMetaStreamId_from_StreamNode.push_back(
          pConfiguration_StreamInfo_NonP1->pAppMeta_DynamicP2StreamNode
              ->getStreamId());
    }
    return NSCam::OK;
  }

  decideStreamOut(out, &in);

  MY_LOGI("requestNo:%d use P2C node(%d), P2S node(%d)", in.requestNo,
          out->needP2CaptureNode, out->needP2StreamNode);
  return NSCam::OK;
}

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
FunctionType_P2NodeDecisionPolicy makePolicy_P2NodeDecision_Default() {
  return evaluateRequest;
}

};  // namespace policy
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
