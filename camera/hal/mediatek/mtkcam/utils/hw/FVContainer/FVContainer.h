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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_HW_FVCONTAINER_FVCONTAINER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_HW_FVCONTAINER_FVCONTAINER_H_

#include <memory>
#include <mtkcam/utils/hw/IFVContainer.h>
#include <vector>

namespace NSCam {

class FVContainerImp;

class FVContainer : public IFVContainer {
  /* implementation from IFVContainer */
 public:
  vector<FV_DATATYPE> query(void);

  vector<FV_DATATYPE> query(const int32_t& mg_start, const int32_t& mg_end);

  vector<FV_DATATYPE> query(const vector<int32_t>& vecMgs);

  MBOOL push(int32_t magicNum, FV_DATATYPE fv);

  void clear(void);

  void dumpInfo(void);

  /* attributes */
 private:
  std::shared_ptr<FVContainerImp> mFleepingQueueImpl;
  char const* mUserId;
  IFVContainer::eFVContainer_Opt mOpt;

  /* constructor & destructor */
 public:
  FVContainer() = delete;
  FVContainer(char const* userId, IFVContainer::eFVContainer_Opt opt);
  virtual ~FVContainer();
};      // class FVContainer
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_HW_FVCONTAINER_FVCONTAINER_H_
