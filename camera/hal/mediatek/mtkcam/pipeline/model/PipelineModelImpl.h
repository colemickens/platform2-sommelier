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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_PIPELINEMODELIMPL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_PIPELINEMODELIMPL_H_
//
#include <future>
#include <impl/IHalDeviceAdapter.h>
#include <impl/IPipelineModelSession.h>
#include <impl/types.h>
#include <memory>
#include <mtkcam/pipeline/model/IPipelineModel.h>
#include <mutex>
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
class PipelineModelImpl : public IPipelineModel {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////    Instantiation data (initialized at the creation stage).
  std::shared_ptr<PipelineStaticInfo> mPipelineStaticInfo;
  //
  int32_t const mOpenId = -1;
  int32_t mLogLevel = 0;
  //
  std::shared_ptr<IHalDeviceAdapter> const mHalDeviceAdapter;

 protected:  ////    Open data (initialized at the open stage).
  std::string mUserName;
  std::weak_ptr<IPipelineModelCallback> mCallback;
  std::vector<std::future<bool>> mvOpenFutures;

 protected:  ////    Configuration data (initialized at the configuration stage)
  std::shared_ptr<IPipelineModelSession> mSession;

 protected:  ////    Used to protect Open data and Configuration data.
  std::mutex mLock;

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Operations.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////    Instantiation.
  /**
   * A structure for creation parameters.
   */
  struct CreationParams {
    /**
     * @param logical device id
     */
    int32_t openId;

    /**
     * @param printers with several levels for debugging.
     */
  };

  static auto createInstance(CreationParams const& creationParams)
      -> std::shared_ptr<PipelineModelImpl>;

  explicit PipelineModelImpl(CreationParams const& creationParams);
  virtual ~PipelineModelImpl() {}

 protected:
  virtual auto init() -> bool;
  virtual auto initPipelineStaticInfo() -> bool;

 protected:  ////    Data member access.
  auto getOpenId() const { return mOpenId; }

 protected:  ////    Operations.
  virtual auto waitUntilOpenDoneLocked() -> bool;

 public:  ////    Operations.
  /**
   * Dump debugging state.
   */
  virtual auto dumpState(const std::vector<std::string>& options) -> void;

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineModel Interfaces.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////    Interfaces.
  auto open(std::string const& userName,
            std::weak_ptr<IPipelineModelCallback> const& callback)
      -> int override;
  auto waitUntilOpenDone() -> bool override;
  auto close() -> void override;
  auto configure(std::shared_ptr<UserConfigurationParams> const& params)
      -> int override;
  auto submitRequest(
      std::vector<std::shared_ptr<UserRequestParams>> const& requests,
      uint32_t* numRequestProcessed) -> int override;
  auto beginFlush() -> int override;
  auto endFlush() -> void override;
};

/*****************************************************************************
 *
 *****************************************************************************/
};      // namespace model
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_PIPELINEMODELIMPL_H_
