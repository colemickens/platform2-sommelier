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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_P2PLATINFO_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_P2PLATINFO_H_

#include <mtkcam/def/common.h>

namespace NSCam {
namespace Feature {
namespace P2Util {

class P2PlatInfo {
 public:
  enum FeatureID {
    FID_NONE,
    FID_CLEARZOOM,
    FID_DRE,
  };

 public:
  static const P2PlatInfo* getInstance(MUINT32 sensorID);

 public:
  virtual ~P2PlatInfo() {}
  virtual MBOOL isDip50() const = 0;
  virtual MRect getActiveArrayRect() const = 0;

 protected:
  P2PlatInfo() {}
};

}  // namespace P2Util
}  // namespace Feature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_P2PLATINFO_H_
