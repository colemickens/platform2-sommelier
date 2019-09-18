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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_UTIL_P2OPERATOR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_UTIL_P2OPERATOR_H_

//
/*****************************************************************************
 *
 *****************************************************************************/
#include <chrono>
#include <condition_variable>
#include <memory>
// Local header file
#include "QParamTemplate.h"
// Standard C header file
#include <string>
#include <vector>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

using std::make_shared;
using std::shared_ptr;
using std::swap;

class VISIBILITY_PUBLIC P2Operator {
 public:
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  P2Operator Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  P2Operator(const char* creatorName, MINT32 openId);

  virtual ~P2Operator();

  MBOOL configNormalStream(MINT32 tag, const StreamConfigure config);

  MERROR enque(QParams* pEnqueParam, const char* userName = "unknown");

  std::shared_ptr<IImageBuffer> getTuningBuffer();

  void putTuningBuffer(std::shared_ptr<IImageBuffer> buf);

  MERROR release();
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  RefBase Interface.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  virtual MVOID onLastStrongRef(const void* /*id*/);

 private:
  const char* mCreatorName;
  MINT32 miOpenId = -1;

  mutable std::mutex mLock;
  std::condition_variable mCond;

  std::shared_ptr<v4l2::INormalStream> mpINormalStream = nullptr;
  std::vector<std::shared_ptr<IImageBuffer> > mTuningBuffers;
};

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_UTIL_P2OPERATOR_H_
