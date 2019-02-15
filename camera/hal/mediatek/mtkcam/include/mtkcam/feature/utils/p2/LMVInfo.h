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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_LMVINFO_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_LMVINFO_H_

#include <mtkcam/utils/metadata/IMetadata.h>
#include <mtkcam/utils/std/ILogger.h>

namespace NSCam {
namespace Feature {
namespace P2Util {

class LMVInfo {
 public:
  MBOOL is_valid = MFALSE;

  MUINT32 x_int = 0;
  MUINT32 x_float = 0;
  MUINT32 y_int = 0;
  MUINT32 y_float = 0;
  MSize s;

  MUINT32 x_mv_int = 0;
  MUINT32 x_mv_float = 0;
  MUINT32 y_mv_int = 0;
  MUINT32 y_mv_float = 0;
  MUINT32 is_from_zzr = 0;

  MINT32 gmvX = 0;
  MINT32 gmvY = 0;
  MINT32 gmvMax = 0;

  MINT32 confX = 0;
  MINT32 confY = 0;
  MINT32 expTime = 0;
  MINT32 ihwTS = 0;
  MINT32 ilwTS = 0;
  MINT64 ts = 0;

  MBOOL isFrontBin = MFALSE;
};

}  // namespace P2Util
}  // namespace Feature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_LMVINFO_H_
