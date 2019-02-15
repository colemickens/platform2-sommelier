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

#define LOG_TAG "mtkcam-ConfigAppImageStreamInfoMaxBufNumPolicy"

#include <memory>
#include <mtkcam/pipeline/policy/IConfigAppImageStreamInfoMaxBufNumPolicy.h>
//
#include <mtkcam/pipeline/stream/IStreamInfo.h>
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

/**
 * Make a function target as a policy - default version
 */
FunctionType_Configuration_AppImageStreamInfoMaxBufNumPolicy
makePolicy_Configuration_AppImageStreamInfoMaxBufNum_Default() {
  return
      [](ParsedAppImageStreamInfo* pInOut,
         StreamingFeatureSetting const* pStreamingFeatureSetting,
         CaptureFeatureSetting const* pCaptureFeatureSetting,
         PipelineStaticInfo const* pPipelineStaticInfo __unused,
         PipelineUserConfiguration const* pPipelineUserConfiguration) -> int {
        uint32_t const maxJpegNum =
            (pCaptureFeatureSetting)
                ? pCaptureFeatureSetting->maxAppJpegStreamNum
                : 1;

        if (pInOut->pAppImage_Jpeg) {
          pInOut->pAppImage_Jpeg->setMaxBufNum(maxJpegNum);
        }
        if (pInOut->pAppImage_Input_Yuv) {
          pInOut->pAppImage_Input_Yuv->setMaxBufNum(2);
        }
        if (pInOut->pAppImage_Input_Priv) {
          pInOut->pAppImage_Input_Priv->setMaxBufNum(2);
        }
        if (pInOut->pAppImage_Output_Priv) {
          pInOut->pAppImage_Output_Priv->setMaxBufNum(6);
        }

        std::shared_ptr<IImageStreamInfo> pStreamInfo;
        for (const auto& n : pInOut->vAppImage_Output_Proc) {
          if ((pStreamInfo = n.second) != nullptr) {
            pStreamInfo->setMaxBufNum(8);
          }
        }

        return OK;
      };
}

/**
 * Make a function target as a policy - SMVR version
 */
FunctionType_Configuration_AppImageStreamInfoMaxBufNumPolicy
makePolicy_Configuration_AppImageStreamInfoMaxBufNum_SMVR() {
  return
      [](ParsedAppImageStreamInfo* pInOut,
         StreamingFeatureSetting const* pStreamingFeatureSetting __unused,
         CaptureFeatureSetting const* pCaptureFeatureSetting __unused,
         PipelineStaticInfo const* pPipelineStaticInfo __unused,
         PipelineUserConfiguration const* pPipelineUserConfiguration) -> int {
        if (pInOut->pAppImage_Jpeg) {
          pInOut->pAppImage_Jpeg->setMaxBufNum(1);
        }
        if (pInOut->pAppImage_Input_Yuv) {
          pInOut->pAppImage_Input_Yuv->setMaxBufNum(2);
        }
        if (pInOut->pAppImage_Input_Priv) {
          pInOut->pAppImage_Input_Priv->setMaxBufNum(2);
        }
        if (pInOut->pAppImage_Output_Priv) {
          pInOut->pAppImage_Output_Priv->setMaxBufNum(6);
        }

        std::shared_ptr<IImageStreamInfo> pStreamInfo;
        for (const auto& n : pInOut->vAppImage_Output_Proc) {
          if ((pStreamInfo = n.second) != nullptr) {
            if (pStreamInfo->getUsageForConsumer() &
                GRALLOC_USAGE_HW_VIDEO_ENCODER) {
              pStreamInfo->setMaxBufNum(52);
            } else {
              pStreamInfo->setMaxBufNum(12);
            }
          }
        }

        return OK;
      };
}

};  // namespace policy
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
