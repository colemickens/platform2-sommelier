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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_UTILS_SYNCHELPER_SYNCHELPERBASE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_UTILS_SYNCHELPER_SYNCHELPERBASE_H_

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include <semaphore.h>

#include "mtkcam/pipeline/utils/SyncHelper/ISyncHelperBase.h"

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
class SyncContext {
 private:
  std::vector<int> mSyncCam;

 public:
  SyncContext() {
    sem_init(&mSyncSem, 0, 0);
    sem_init(&mResultSem, 0, 0);
    mStatus = status_Inited;
    mResultTimeStamp = 0;
  }

  ~SyncContext() {
    sem_destroy(&mSyncSem);
    sem_destroy(&mResultSem);
    mStatus = status_Uninit;
  }
  sem_t mSyncSem;
  sem_t mResultSem;
  syncStatus mStatus;
  uint64_t mResultTimeStamp;
};

class SyncHelperBase : public virtual ISyncHelperBase {
 private:
  std::map<int, std::shared_ptr<SyncContext>> mContextMap;

  std::vector<int> mSyncQueue;
  std::vector<int> mResultQueue;
  mutable std::mutex mSyncQLock;
  mutable std::mutex mResultQLock;
  std::atomic<int> mUserCounter;
  // time diff for debug
  std::chrono::time_point<std::chrono::system_clock> mSyncTimeStart;
  std::chrono::time_point<std::chrono::system_clock> mResultTimeStart;

 public:
  SyncHelperBase();
  virtual ~SyncHelperBase();

  status_t start(int CamId) override;
  status_t stop(int CamId) override;
  status_t init(int CamId) override;
  status_t uninit(int CamId) override;
  status_t syncEnqHW(SyncParam const& sParam) override;
  status_t syncResultCheck(SyncParam* sParam) override;
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

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_UTILS_SYNCHELPER_SYNCHELPERBASE_H_
