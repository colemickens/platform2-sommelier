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

#include <algorithm>
#include <mtkcam/feature/featureCore/featurePipe/streaming/StreamingFeatureTimer.h>

#include "DebugControl.h"
#define PIPE_CLASS_TAG "Timer"
#define PIPE_TRACE TRACE_STREAMING_FEATURE_TIMER
#include <featurePipe/common/include/PipeLog.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

StreamingFeatureTimer::StreamingFeatureTimer() : mDisplayReady(MFALSE) {}

MVOID StreamingFeatureTimer::markDisplayDone() {
  if (!mDisplayReady) {
    mDisplayReady = MTRUE;
    mDisplayMark = Timer::getTimeSpec();
  }
}

MVOID StreamingFeatureTimer::markFrameDone() {
  mFrameMark = Timer::getTimeSpec();
}

timespec StreamingFeatureTimer::getDisplayMark() {
  return mDisplayMark;
}

timespec StreamingFeatureTimer::getFrameMark() {
  return mFrameMark;
}

MVOID StreamingFeatureTimer::print(MUINT32 requestNo,
                                   MUINT32 recordNo,
                                   double displayFPS,
                                   double frameFPS) {
  MUINT32 total = getElapsed();
  MUINT32 depth = getElapsedDepth();
  MUINT32 depthEn = getElapsedEnqueDepth();
  MUINT32 bokeh = getElapsedBokeh();
  MUINT32 bokehEn = getElapsedEnqueBokeh();
  MUINT32 p2a = getElapsedP2A();
  MUINT32 p2aEn = getElapsedEnqueP2A();
  MUINT32 p2aTun = getElapsedP2ATuning();
  MUINT32 p2aMdp = getElapsedP2AMDP();
  MUINT32 eis = getElapsedEIS();
  MUINT32 warp = getElapsedWarp();
  MUINT32 warpEn = getElapsedEnqueWarp();
  MUINT32 warpMDP = getElapsedWarpMDP();
  MUINT32 helper = getElapsedHelper();
  MUINT32 vendor = getElapsedVendor();
  MUINT32 vendorEn = getElapsedEnqueVendor();
  MUINT32 vmdp = getElapsedVMDP();
  MUINT32 rsc = getElapsedRSC();
  MUINT32 rscEn = getElapsedEnqueRSC();
  MUINT32 fov = getElapsedFOV();
  MUINT32 fovWarp = getElapsedFOVWarp();

  const MUINT TPI_TIMER_COUNT = 3;
  MUINT32 tpi[TPI_TIMER_COUNT] = {0};
  MUINT32 tpiEn[TPI_TIMER_COUNT] = {0};
  MUINT32 tpiCount = std::min(TPI_TIMER_COUNT, MAX_TPI_COUNT);
  for (unsigned i = 0; i < tpiCount; ++i) {
    tpiEn[i] = getElapsedEnqueTPI(i);
    tpi[i] = getElapsedTPI(i);
  }

  MY_LOGD(
      "Frame timer "
      "[#%5d/%4d][t%4d][d%3d/%3d][a%3d/%3d/%3d][am%3d][b%3d/%3d][v%3d/"
      "%3d][tpi%3d/%3d/%3d/%3d/%3d/%3d][vmdp%3d][h%3d][e%3d][r%3d/%3d][f%3d/"
      "%3d][w%3d/%3d/%3d][fps%05.2f/%05.2f]",
      requestNo, recordNo, total, depth, depthEn, p2a, p2aTun, p2aEn, p2aMdp,
      bokeh, bokehEn, vendor, vendorEn, tpi[0], tpiEn[0], tpi[1], tpiEn[1],
      tpi[2], tpiEn[2], vmdp, helper, eis, rsc, rscEn, fov, fovWarp, warp,
      warpEn, warpMDP, displayFPS, frameFPS);

#if defined(DEBUG_TIMER) && (DEBUT_TIMER == 1)
  {
    MUINT32 t1 = getElapsedT1();
    MUINT32 t2 = getElapsedT2();
    MUINT32 t3 = getElapsedT3();
    MUINT32 t4 = getElapsedT4();
    MY_LOGD("Frame [t1%5d][t2%5d][t3%5d][t4%5d]", requestNo, t1, t2, t3, t4);
  }
#endif
}

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
