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

#include <capture/CaptureFeatureTimer.h>

#include "DebugControl.h"
#define PIPE_CLASS_TAG "Timer"
#define PIPE_TRACE TRACE_CAPTURE_FEATURE_TIMER
#include <common/include/PipeLog.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

CaptureFeatureTimer::CaptureFeatureTimer() : Timer() {}

MVOID CaptureFeatureTimer::print(MUINT32 uRequestNo, MUINT32 uFrameNo) {
  MUINT32 total = getElapsed();
  MUINT32 raw = getElapsedRAW();
  MUINT32 p2a = getElapsedP2A();
  MUINT32 fd = getElapsedFD();
  MUINT32 mf = getElapsedMF();
  MUINT32 yuv = getElapsedYUV();
  MUINT32 mdp = getElapsedMDP();

  MY_LOGD("Frame timer [R%d/F%d][t%4d][r%4d][a%4d][fd%4d][m%4d][y%4d][m%4d]",
          uRequestNo, uFrameNo, total, raw, p2a, fd, mf, yuv, mdp);
}

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
