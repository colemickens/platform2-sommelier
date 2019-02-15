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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_3DNR_UTIL_3DNR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_3DNR_UTIL_3DNR_H_

#include <mtkcam/def/common.h>
#include <mtkcam/feature/3dnr/3dnr_defs.h>
#include <mtkcam/utils/sys/SensorProvider.h>

using NSCam::MPoint;
using NSCam::MRect;
using NSCam::MSize;
using NSCam::NR3D::NR3DMVInfo;  // for backward-compatible

namespace NSCam {

class IImageBuffer;
class IMetadata;

namespace NSCamFeature {
namespace NSFeaturePipe {

class FeaturePipeParam;

};
};  // namespace NSCamFeature
};  // namespace NSCam

class VISIBILITY_PUBLIC Util3dnr {
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Operations.
  explicit Util3dnr(MINT32 openId);
  virtual ~Util3dnr();

  void init(MINT32 force3dnr = 0);
  MBOOL canEnable3dnr(MBOOL isUIEnable, MINT32 iso, MINT32 isoThreshold);
  void modifyMVInfo(MBOOL canEnable3dnr,
                    MBOOL isIMGO,
                    const MRect& cropP1Sensor,
                    const MSize& dstSizeResizer,
                    NR3DMVInfo* mvInfo);
  void prepareFeatureData(
      MBOOL canEnable3dnr,
      const NR3DMVInfo& mvInfo,
      MINT32 iso,
      MINT32 isoThreshold,
      MBOOL isCRZMode,
      NSCam::NSCamFeature::NSFeaturePipe::FeaturePipeParam* featureEnqueParams);
  void prepareISPData(MBOOL canEnable3dnr,
                      const NR3DMVInfo& mvInfo,
                      const MSize& inputSize,
                      const MRect& inputCrop,
                      MINT32 iso,
                      MINT32 isoThreshold,
                      MBOOL isSl2eEnable,
                      NSCam::IMetadata* pMeta_InHal);
  MBOOL prepareGyro(
      NSCam::NR3D::GyroData* outGyroData,
      NSCam::NSCamFeature::NSFeaturePipe::FeaturePipeParam* featureEnqueParams);
  void resetFrame() { mforceFrameReset = MTRUE; }
  MBOOL is3dnrDebugMode(void);

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Variables.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

 private:
  const MINT32 mOpenId;
  MINT32 mLogLevel;
  MINT32 mDebugLevel;
  MINT32 mforce3dnr;
  MBOOL mforceFrameReset;
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_3DNR_UTIL_3DNR_H_
