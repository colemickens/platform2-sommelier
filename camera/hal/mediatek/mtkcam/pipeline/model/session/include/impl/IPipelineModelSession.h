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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_SESSION_INCLUDE_IMPL_IPIPELINEMODELSESSION_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_SESSION_INCLUDE_IMPL_IPIPELINEMODELSESSION_H_

#include <impl/types.h>
#include <mtkcam/pipeline/model/IPipelineModel.h>
//
#include <memory>
#include <string>
#include <vector>
//

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace model {

/******************************************************************************
 *
 ******************************************************************************/
class IPipelineModelSession {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Session Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  virtual ~IPipelineModelSession() = default;

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

  /**
   * Dump debugging state.
   */
  virtual auto dumpState(const std::vector<std::string>& options) -> void = 0;
};

/******************************************************************************
 *
 ******************************************************************************/
class IPipelineModelSessionFactory {
 public:  ////
  /**
   * A structure for creation parameters.
   */
  struct CreationParams {
    std::shared_ptr<PipelineStaticInfo> pPipelineStaticInfo;
    std::shared_ptr<UserConfigurationParams> pUserConfigurationParams;

    std::shared_ptr<IPipelineModelCallback> pPipelineModelCallback;
  };

  static auto createPipelineModelSession(CreationParams const& params)
      -> std::shared_ptr<IPipelineModelSession>;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace model
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_SESSION_INCLUDE_IMPL_IPIPELINEMODELSESSION_H_
