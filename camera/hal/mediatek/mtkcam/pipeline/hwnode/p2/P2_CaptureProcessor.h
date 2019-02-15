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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_CAPTUREPROCESSOR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_CAPTUREPROCESSOR_H_

#include "P2_Processor.h"
#include "P2_Util.h"

#include <mtkcam/feature/featurePipe/ICaptureFeaturePipe.h>
// feature index for pipeline plugin
#include <mtkcam/3rdparty/customer/customer_feature_type.h>
#include <mtkcam/3rdparty/mtk/mtk_feature_type.h>

#include <memory>
#include <vector>

using NSCam::NSCamFeature::NSFeaturePipe::NSCapture::ICaptureFeaturePipe;
using NSCam::NSCamFeature::NSFeaturePipe::NSCapture::ICaptureFeatureRequest;
using NSCam::NSCamFeature::NSFeaturePipe::NSCapture::RequestCallback;

namespace P2 {
class CaptureProcessor;
class CaptureRequestCallback : virtual public RequestCallback {
 public:
  explicit CaptureRequestCallback(CaptureProcessor*);

  virtual void onContinue(std::shared_ptr<ICaptureFeatureRequest> pCapRequest);
  virtual void onAborted(std::shared_ptr<ICaptureFeatureRequest> pCapRequest);
  virtual void onCompleted(std::shared_ptr<ICaptureFeatureRequest> pCapRequest,
                           NSCam::MERROR);
  virtual ~CaptureRequestCallback() {}

 private:
  CaptureProcessor* mpProcessor;
};

class CaptureProcessor
    : virtual public Processor<P2InitParam,
                               P2ConfigParam,
                               std::shared_ptr<P2FrameRequest>> {
  friend CaptureRequestCallback;

 public:
  CaptureProcessor();
  virtual ~CaptureProcessor();

 protected:
  virtual MBOOL onInit(const P2InitParam& param);
  virtual MVOID onUninit();
  virtual MVOID onThreadStart();
  virtual MVOID onThreadStop();
  virtual MBOOL onConfig(const P2ConfigParam& param);
  virtual MBOOL onEnque(const std::shared_ptr<P2FrameRequest>& request);
  virtual MVOID onNotifyFlush();
  virtual MVOID onWaitFlush();

 protected:
 private:
  struct RequestPair {
    std::shared_ptr<P2Request> mNodeRequest;
    std::shared_ptr<ICaptureFeatureRequest> mPipeRequest;
  };

  std::shared_ptr<RequestCallback> mpCallback;
  std::vector<RequestPair> mRequestPairs;

  mutable std::mutex mPairLock;

  ILog mLog;
  P2Info mP2Info;
  std::shared_ptr<ICaptureFeaturePipe> mpFeaturePipe;
};

}  // namespace P2

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_CAPTUREPROCESSOR_H_
