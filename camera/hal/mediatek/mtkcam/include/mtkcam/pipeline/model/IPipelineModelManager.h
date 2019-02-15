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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_MODEL_IPIPELINEMODELMANAGER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_MODEL_IPIPELINEMODELMANAGER_H_

#include "IPipelineModel.h"
#include "./types.h"
#include <mtkcam/def/common.h>
//
#include <memory>

namespace NSCam {
namespace v3 {
namespace pipeline {
namespace model {

/******************************************************************************
 *
 ******************************************************************************/
class VISIBILITY_PUBLIC IPipelineModelManager {
 public:  ////    Interfaces.
  /**
   * Get the singleton instance of IPipelineModelManager.
   */
  static auto get() -> IPipelineModelManager*;

  /**
   * Get the singleton instance of IPipelineModel for a specific open id.
   *
   * @param[in] open Id.
   */
  virtual auto getPipelineModel(int32_t const openId) const
      -> std::shared_ptr<IPipelineModel> = 0;
  virtual ~IPipelineModelManager() {}
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace model
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_MODEL_IPIPELINEMODELMANAGER_H_
