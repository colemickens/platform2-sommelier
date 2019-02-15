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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_SYNCHELPER_ISYNCHELPER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_SYNCHELPER_ISYNCHELPER_H_

#include "ISyncHelperBase.h"
#include <memory>
#include <mtkcam/utils/metadata/IMetadata.h>

/******************************************************************************
 *ISyncHelper is used to convert IMetadata for ISyncHelpBase
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace Utils {
namespace Imp {

/******************************************************************************
 *
 ******************************************************************************/

class ISyncHelper : public virtual ISyncHelperBase {
 public:
  virtual ~ISyncHelper() = default;

  using ISyncHelperBase::syncEnqHW;
  using ISyncHelperBase::syncResultCheck;

  virtual status_t syncEnqHW(int CamId, IMetadata* HalControl) = 0;
  /*Return the Sync Result*/
  virtual bool syncResultCheck(int CamId,
                               IMetadata* HalControl,
                               IMetadata* HalDynamic) = 0;

  static std::shared_ptr<ISyncHelper> createInstance();
};
/******************************************************************************
 *
 ******************************************************************************/
}  // namespace Imp
}  // namespace Utils
}  // namespace v3
}  // namespace NSCam
/******************************************************************************
 *
 ******************************************************************************/
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_SYNCHELPER_ISYNCHELPER_H_
