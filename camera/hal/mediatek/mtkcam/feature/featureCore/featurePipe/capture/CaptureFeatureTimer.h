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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATURETIMER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATURETIMER_H_

#include <common/include/Timer.h>
#include "DebugControl.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

class CaptureFeatureTimer : public Timer {
 public:
  ADD_TIMER(RAW);
  ADD_TIMER(P2A);
  ADD_TIMER(FD);
  ADD_TIMER(MF);
  ADD_TIMER(YUV);
  ADD_TIMER(MDP);

  CaptureFeatureTimer();

  MVOID print(MUINT32 uRequestNo, MUINT32 uFrameNo);

 private:
};

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATURETIMER_H_
