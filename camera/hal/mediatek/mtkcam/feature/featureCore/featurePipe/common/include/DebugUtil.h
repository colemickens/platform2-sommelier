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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_DEBUGUTIL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_DEBUGUTIL_H_

#include "MtkHeader.h"
// #include <mtkcam/common.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

MINT32 VISIBILITY_PUBLIC getPropertyValue(const char* key);
MINT32 VISIBILITY_PUBLIC getPropertyValue(const char* key, MINT32 defVal);
MINT32 VISIBILITY_PUBLIC getFormattedPropertyValue(const char* fmt, ...);
//
bool VISIBILITY_PUBLIC makePath(char const* const path, MUINT32 const mode);

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_DEBUGUTIL_H_
