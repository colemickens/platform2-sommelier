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

#include <mtkcam/pipeline/model/IPipelineModelManager.h>

#include <mtkcam/utils/debug/debug.h>

#include "PipelineModelImpl.h"
#include "EventLog.h"
#include "MyUtils.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>

/******************************************************************************
 *
 ******************************************************************************/
using NSCam::v3::pipeline::model::IPipelineModelManager;
/******************************************************************************
 *
 ******************************************************************************/
class PipelineModelManagerImpl
    : public std::enable_shared_from_this<PipelineModelManagerImpl>,
      public IPipelineModelManager {
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  struct PipelineModelInfo {
    std::shared_ptr<NSCam::v3::pipeline::model::PipelineModelImpl>
        mPipelineModel;
  };

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////
  mutable std::map<int32_t, std::shared_ptr<PipelineModelInfo>> mPipelineMap;
  mutable std::mutex mPipelineLock;

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Operations.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////    Instantiation.
  PipelineModelManagerImpl();
  virtual ~PipelineModelManagerImpl();

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineModelManager Interfaces.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////    Interfaces.
  auto getPipelineModel(int32_t const openId) const
      -> std::shared_ptr<NSCam::v3::pipeline::model::IPipelineModel> override;
};

/*****************************************************************************
 *
 *****************************************************************************/
auto IPipelineModelManager::get() -> IPipelineModelManager* {
  IPipelineModelManager* tmp = new PipelineModelManagerImpl();
  return tmp;
}

/*****************************************************************************
 *
 *****************************************************************************/
PipelineModelManagerImpl::PipelineModelManagerImpl() {}

/*****************************************************************************
 *
 *****************************************************************************/
PipelineModelManagerImpl::~PipelineModelManagerImpl() {}

/*****************************************************************************
 *
 *****************************************************************************/
auto PipelineModelManagerImpl::getPipelineModel(int32_t const openId) const
    -> std::shared_ptr<NSCam::v3::pipeline::model::IPipelineModel> {
  std::lock_guard<std::mutex> _l(mPipelineLock);

  auto it = mPipelineMap.find(openId);
  if (CC_LIKELY(it != mPipelineMap.end())) {
    return it->second->mPipelineModel;
  }

  // first time.
  auto pPipelineModelInfo = mPipelineMap[openId] =
      std::make_shared<PipelineModelInfo>();

  auto pPipelineModel = pPipelineModelInfo->mPipelineModel =
      NSCam::v3::pipeline::model::PipelineModelImpl::createInstance(
          NSCam::v3::pipeline::model::PipelineModelImpl::CreationParams{
              .openId = openId,
          });
  if (CC_UNLIKELY(pPipelineModel == nullptr)) {
    MY_LOGE("openId:%d: Fail on PipelineModelImpl::createInstance", openId);
    mPipelineMap.erase(openId);
    return nullptr;
  }
  return pPipelineModelInfo->mPipelineModel;
}
