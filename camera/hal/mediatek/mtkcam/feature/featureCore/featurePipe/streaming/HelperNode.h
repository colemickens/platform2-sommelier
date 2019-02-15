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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_HELPERNODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_HELPERNODE_H_

#include <deque>
#include "StreamingFeatureNode.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class HelperNode : public StreamingFeatureNode {
 public:
  explicit HelperNode(const char* name);
  virtual ~HelperNode();

 public:
  virtual MBOOL onData(DataID id, const HelperData& request);

 protected:
  virtual MBOOL onInit();
  virtual MBOOL onUninit();
  virtual MBOOL onThreadStart();
  virtual MBOOL onThreadStop();
  virtual MBOOL onThreadLoop();

 private:
  MBOOL processHelper(const RequestPtr& request, const HelpReq& helpReq);
  MBOOL processCB(const RequestPtr& request, FeaturePipeParam::MSG_TYPE msg);
  MVOID clearTSQ();

  MVOID prepareCB(const RequestPtr& request, FeaturePipeParam::MSG_TYPE msg);
  MVOID storeMessage(const RequestPtr& request, FeaturePipeParam::MSG_TYPE msg);
  MVOID handleStoredMessage(const RequestPtr& request);

 private:
  WaitQueue<HelperData> mCBRequests;
  std::deque<MINT64> mTSQueue;
};

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_HELPERNODE_H_
