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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_SESSION_PIPELINEMODELSESSIONBASE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_SESSION_PIPELINEMODELSESSIONBASE_H_
//
#include <impl/IPipelineModelSession.h>

#include <memory>
#include <string>
#include <vector>

#include <mtkcam/pipeline/policy/IPipelineSettingPolicy.h>
#include <mtkcam/pipeline/pipeline/IPipelineBufferSetFrameControl.h>
#include <mtkcam/pipeline/pipeline/PipelineContext.h>

/*****************************************************************************
 *
 *****************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace model {

/*****************************************************************************
 *
 *****************************************************************************/
class PipelineModelSessionBase
    : public IPipelineModelSession,
      public IPipelineBufferSetFrameControl::IAppCallback {
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  using PipelineContext = NSCam::v3::NSPipelineContext::PipelineContext;
  using HalMetaStreamBuffer = NSCam::v3::Utils::HalMetaStreamBuffer;

  struct StaticInfo {
    std::shared_ptr<PipelineStaticInfo> pPipelineStaticInfo;
    std::shared_ptr<PipelineUserConfiguration> pUserConfiguration;
  };

  struct DebugInfo {};

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////    Read-only data
  std::string mSessionName;
  StaticInfo mStaticInfo;

 protected:  ////    Operatable data (allowed to use their operations)
  DebugInfo mDebugInfo;
  std::weak_ptr<IPipelineModelCallback> mPipelineModelCallback;

  using IPipelineSettingPolicy =
      NSCam::v3::pipeline::policy::pipelinesetting::IPipelineSettingPolicy;
  std::shared_ptr<IPipelineSettingPolicy> mPipelineSettingPolicy;

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Internal Operations.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////    Data member access.
  auto getSessionName() const -> std::string const& { return mSessionName; }

 protected:  ////    Template Methods.
  virtual auto getCurrentPipelineContext() const
      -> std::shared_ptr<PipelineContext> = 0;

  virtual auto submitOneRequest(
      std::shared_ptr<ParsedAppRequest> const& request) -> int = 0;

 protected:  ////    Operations.
  /**
   * Determine the timestamp of the start of frame (SOF).
   *
   * @param[in] streamId.
   *      The SOF timestamp is contained in the meta steram buffer
   *      whose stream id equal to the given one.
   *      Usually, it is the stream id of P1Node output HAL meta stream buffer.
   *
   * @param[in] vMetaStreamBuffer: a vector of meta stream buffer,
   *      one of which may have the stream id equal to the given streamId.
   *
   * @return
   *      Nonzero indicates the SOF timestamp.
   *      0 indicates that the SOF timestamp is not found.
   */

  static auto determineTimestampSOF(
      StreamId_T const streamId,
      std::vector<std::shared_ptr<IMetaStreamBuffer>> const& vMetaStreamBuffer)
      -> int64_t;
  virtual auto updateFrameTimestamp(MUINT32 const requestNo,
                                    MINTPTR const userId,
                                    Result const& result,
                                    int64_t timestampStartOfFrame) -> void;

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interfaces (called by Derived Class).
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////    Instantiation.
  struct CtorParams {
    StaticInfo staticInfo;
    DebugInfo debugInfo;
    std::weak_ptr<IPipelineModelCallback> pPipelineModelCallback;
    std::shared_ptr<IPipelineSettingPolicy> pPipelineSettingPolicy;
  };
  PipelineModelSessionBase(std::string const&& sessionName,
                           CtorParams const& rCtorParams);

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineModelSession Interfaces.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  auto submitRequest(
      std::vector<std::shared_ptr<UserRequestParams>> const& requests,
      uint32_t* numRequestProcessed) -> int override;

  auto beginFlush() -> int override;

  auto endFlush() -> void override;

  auto dumpState(const std::vector<std::string>& options) -> void override;
};

/*****************************************************************************
 *
 *****************************************************************************/
};  // namespace model
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_SESSION_PIPELINEMODELSESSIONBASE_H_
