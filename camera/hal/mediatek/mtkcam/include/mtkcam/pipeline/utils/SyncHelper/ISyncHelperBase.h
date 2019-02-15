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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_SYNCHELPER_ISYNCHELPERBASE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_SYNCHELPER_ISYNCHELPERBASE_H_

#include <memory>
#include <mtkcam/def/Errors.h>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace Utils {
namespace Imp {

/******************************************************************************
 *
 ******************************************************************************/
enum syncStatus {
  status_Uninit = 0,
  status_Inited,
  status_Enque,
  status_ResChk,
};

struct SyncParam {
  int mCamId;              // current camera id.
  int64_t mSyncTolerance;  // sync tolerance time.
  int mSyncFailHandle;     // sync fail handle
  int64_t mReslutTimeStamp;
  unsigned int mSyncResult;
  std::vector<int> mSyncCams;  // sync target
};

class ISyncHelperBase {
 public:
  virtual ~ISyncHelperBase() = default;

  virtual status_t start(int CamId) = 0;
  virtual status_t stop(int CamId) = 0;
  virtual status_t init(int CamId) = 0;
  virtual status_t uninit(int CamId) = 0;
  virtual status_t syncEnqHW(SyncParam const& SyncParam) = 0;
  virtual status_t syncResultCheck(SyncParam* SyncParam) = 0;

  static std::shared_ptr<ISyncHelperBase> createInstance();
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

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_SYNCHELPER_ISYNCHELPERBASE_H_
