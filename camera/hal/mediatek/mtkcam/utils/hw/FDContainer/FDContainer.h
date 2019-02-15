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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_HW_FDCONTAINER_FDCONTAINER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_HW_FDCONTAINER_FDCONTAINER_H_

#include <memory>
#include <mtkcam/utils/hw/IFDContainer.h>
#include <vector>

namespace NSCam {

class FDContainerImp;

class FDContainer : public IFDContainer {
  /* implementation from IFDContainer */
 public:
  vector<FD_DATATYPE*> queryLock(void);

  vector<FD_DATATYPE*> queryLock(const int64_t& ts_start,
                                 const int64_t& ts_end);

  vector<FD_DATATYPE*> queryLock(const vector<int64_t>& vecTss);

  MBOOL queryUnlock(const vector<FD_DATATYPE*>& vecInfos);

  FD_DATATYPE* editLock(int64_t timestamp);

  MBOOL editUnlock(FD_DATATYPE* info);

  void dumpInfo(void);

  /* attributes */
 private:
  std::shared_ptr<FDContainerImp> mFleepingQueueImpl;
  char const* mUserId;
  IFDContainer::eFDContainer_Opt mOpt;

  /* constructor & destructor */
 public:
  FDContainer() = delete;
  FDContainer(char const* userId, IFDContainer::eFDContainer_Opt opt);
  virtual ~FDContainer();
};      // class FDContainer
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_HW_FDCONTAINER_FDCONTAINER_H_
