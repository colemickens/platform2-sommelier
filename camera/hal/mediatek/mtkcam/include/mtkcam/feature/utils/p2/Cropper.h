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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_CROPPER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_CROPPER_H_

#include <mtkcam/drv/def/IPostProcDef.h>

#include <mtkcam/utils/std/ILogger.h>
#include <mtkcam/utils/std/Log.h>

#include <mtkcam/feature/utils/p2/LMVInfo.h>

using NSCam::Utils::ILog;

namespace NSCam {
namespace Feature {
namespace P2Util {

class Cropper {
 public:
  enum CropMask {
    USE_RESIZED = 0x01,
    USE_EIS_12 = 0x02,
    USE_CROP_RATIO = 0x04,
  };

  Cropper() {}
  virtual ~Cropper() {}

  virtual MBOOL isValid() const = 0;
  virtual MSize getSensorSize() const = 0;
  virtual MRect getResizedCrop() const = 0;
  virtual MRect getP1Crop() const = 0;
  virtual MSize getP1OutSize() const = 0;
  virtual MRect getP1BinCrop() const = 0;
  virtual MSize getP1BinSize() const = 0;
  virtual const LMVInfo& getLMVInfo() const = 0;
  virtual NSIoPipe::MCropRect calcViewAngle(const ILog& log,
                                            const MSize& size,
                                            MUINT32 cropFlag) const = 0;
  virtual NSIoPipe::MCropRect calcViewAngle(const ILog& log,
                                            const MSize& size,
                                            MUINT32 cropFlag,
                                            const float cropRatio) const = 0;
  virtual MRectF calcViewAngleF(const ILog& log,
                                const MSize& size,
                                MUINT32 cropFlag,
                                const float cropRatio,
                                const MINT32 dmaConstrainFlag) const = 0;
  virtual MRectF applyViewRatio(const ILog& log,
                                const MRectF& src,
                                const MSize& size) const = 0;
  virtual MBOOL refineBoundary(const ILog& log,
                               const MSize& imgSize,
                               NSIoPipe::MCropRect* crop) const = 0;

  virtual MRect getCropRegion() const = 0;
  virtual MRect getActiveCrop() const = 0;
  virtual MRect toActive(const NSIoPipe::MCropRect& cropRect,
                         MBOOL resize) const = 0;
  virtual MRect toActive(const MRectF& cropF, MBOOL resize) const = 0;
  virtual MRect toActive(const MRect& crop, MBOOL resize) const = 0;
  virtual MVOID dump(const ILog& log) const = 0;
};

}  // namespace P2Util
}  // namespace Feature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_CROPPER_H_
