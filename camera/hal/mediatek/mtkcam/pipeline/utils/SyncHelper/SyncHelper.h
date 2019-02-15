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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_UTILS_SYNCHELPER_SYNCHELPER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_UTILS_SYNCHELPER_SYNCHELPER_H_

#include <mutex>
#include "mtkcam/pipeline/utils/SyncHelper/ISyncHelper.h"
#include "mtkcam/pipeline/utils/SyncHelper/SyncHelperBase.h"
#include "SyncHelperBase.h"

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace Utils {
namespace Imp {

/******************************************************************************
 *SyncHelper is used to convert IMetadata for SyncHelpBase
 ******************************************************************************/
class SyncHelper : public virtual ISyncHelper, public SyncHelperBase {
 public:
  using SyncHelperBase::init;
  using SyncHelperBase::start;
  using SyncHelperBase::stop;
  using SyncHelperBase::syncEnqHW;
  using SyncHelperBase::syncResultCheck;
  using SyncHelperBase::uninit;

  status_t syncEnqHW(int CamId, IMetadata* HalControl);
  bool syncResultCheck(int CamId, IMetadata* HalControl, IMetadata* HalDynamic);

 protected:
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

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_UTILS_SYNCHELPER_SYNCHELPER_H_
