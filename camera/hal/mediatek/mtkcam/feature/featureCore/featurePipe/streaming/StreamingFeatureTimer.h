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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_STREAMINGFEATURETIMER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_STREAMINGFEATURETIMER_H_

#include "DebugControl.h"
#include <featurePipe/common/include/Timer.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class StreamingFeatureTimer : public Timer {
 public:
  ADD_TIMER(Depth);
  ADD_TIMER(EnqueDepth);
  ADD_TIMER(P2A);
  ADD_TIMER(EnqueP2A);
  ADD_TIMER(P2ATuning);
  ADD_TIMER(P2AMDP);
  ADD_TIMER(Bokeh);
  ADD_TIMER(EnqueBokeh);
  ADD_TIMER(EIS);
  ADD_TIMER(Helper);
  ADD_TIMER(Vendor);
  ADD_TIMER(EnqueVendor);
  ADD_TIMER_LIST(TPI, MAX_TPI_COUNT);
  ADD_TIMER_LIST(EnqueTPI, MAX_TPI_COUNT);
  ADD_TIMER(VMDP);
  ADD_TIMER(RSC);
  ADD_TIMER(EnqueRSC);
  ADD_TIMER(Warp);
  ADD_TIMER(EnqueWarp);
  ADD_TIMER(WarpMDP);
  ADD_TIMER(FOV);
  ADD_TIMER(FOVWarp);
  ADD_TIMER(N3DP2);
  ADD_TIMER(N3D);

#if defined(DEBUG_TIMER) && (DEBUG_TIMER == 1)
  ADD_TIMER(T1);
  ADD_TIMER(T2);
  ADD_TIMER(T3);
  ADD_TIMER(T4);
#endif

  StreamingFeatureTimer();
  MVOID markDisplayDone();
  MVOID markFrameDone();
  timespec getDisplayMark();
  timespec getFrameMark();
  MVOID print(MUINT32 requestNo,
              MUINT32 recordNo,
              double displayFPS = 0,
              double frameFPS = 0);

 private:
  MBOOL mDisplayReady;
  timespec mDisplayMark;
  timespec mFrameMark;
};

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_STREAMINGFEATURETIMER_H_
