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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_MODEL_IPIPELINEMODEL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_MODEL_IPIPELINEMODEL_H_
//
#include "IPipelineModelCallback.h"
#include <mtkcam/pipeline/model/types.h>
//
#include <memory>
#include <string>
#include <vector>
//
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace model {

/******************************************************************************
 *
 ******************************************************************************/
class IPipelineModel {
 public:  ////    Interfaces.
  /**
   * Open the pipeline.
   *
   * @param[in] userName: user name.
   *  The caller must promise its value.
   *
   * @param[in] pipeline callback
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual auto open(std::string const& userName,
                    std::weak_ptr<IPipelineModelCallback> const& callback)
      -> int = 0;

  /**
   * Wait until open() is done.
   *
   * @return
   *      true indicates success; otherwise failure.
   */
  virtual auto waitUntilOpenDone() -> bool = 0;

  /**
   * Close the pipeline.
   */
  virtual auto close() -> void = 0;

  /**
   * Configure the pipeline.
   *
   * @param[in] params: the parameters to configure.
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual auto configure(std::shared_ptr<UserConfigurationParams> const& params)
      -> int = 0;

  /**
   * Submit a set of requests.
   *
   * @param[in] requests: a set of requests to submit.
   *
   * @param[out] numRequestProcessed: number of requests successfully submitted.
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual auto submitRequest(
      std::vector<std::shared_ptr<UserRequestParams>> const& requests,
      uint32_t* numRequestProcessed) -> int = 0;

  /**
   * turn on flush flag as flush begin and do flush
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual auto beginFlush() -> int = 0;

  /**
   * turn off flush flag as flush end
   *
   */
  virtual auto endFlush() -> void = 0;
  virtual ~IPipelineModel() {}
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace model
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_MODEL_IPIPELINEMODEL_H_
