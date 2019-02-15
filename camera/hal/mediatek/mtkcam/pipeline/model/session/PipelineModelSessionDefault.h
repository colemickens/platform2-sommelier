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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_SESSION_PIPELINEMODELSESSIONDEFAULT_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_SESSION_PIPELINEMODELSESSIONDEFAULT_H_
//
#include "PipelineModelSessionBase.h"
//
#include <impl/ICaptureInFlightRequest.h>
#include <impl/INextCaptureListener.h>
//
#include <impl/ScenarioControl.h>
#include <memory>
#include <mtkcam/pipeline/stream/IStreamInfo.h>
#include <string>
#include <vector>

using IScenarioControlV3 = NSCam::v3::pipeline::model::IScenarioControlV3;

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
class PipelineModelSessionDefault
    : public PipelineModelSessionBase,
      public NSCam::v3::NSPipelineContext::DataCallbackBase,
      public std::enable_shared_from_this<PipelineModelSessionDefault> {
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  enum FrameType_T {
    eFRAMETYPE_MAIN,
    eFRAMETYPE_SUB,
    eFRAMETYPE_PREDUMMY,
    eFRAMETYPE_POSTDUMMY,
    NUM_FRAMETYPE,
  };

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////    2nd configuration info
  struct ConfigInfo2 {
    StreamingFeatureSetting mStreamingFeatureSetting;
    CaptureFeatureSetting mCaptureFeatureSetting;
    PipelineNodesNeed mPipelineNodesNeed;
    std::vector<SensorSetting> mvSensorSetting;
    std::vector<P1HwSetting> mvP1HwSetting;
    std::vector<uint32_t> mvP1DmaNeed;
    std::vector<ParsedStreamInfo_P1> mvParsedStreamInfo_P1;
    ParsedStreamInfo_NonP1 mParsedStreamInfo_NonP1;
    bool mIsZSLMode = false;
  };
  std::shared_ptr<ConfigInfo2> mConfigInfo2;
  mutable pthread_rwlock_t mRWLock_ConfigInfo2;

 protected:  ////    private configuration info
  mutable pthread_rwlock_t mRWLock_PipelineContext;
  std::shared_ptr<PipelineContext> mCurrentPipelineContext;
  bool mbUsingMultiThreadToBuildPipelineContext = true;

 protected:  ////    private data members.
  std::shared_ptr<IScenarioControlV3> mpScenarioCtrl;
  // Capture In Flight Request
  std::shared_ptr<ICaptureInFlightRequest> mpCaptureInFlightRequest;
  std::shared_ptr<INextCaptureListener> mpNextCaptureListener;

 protected:  ////    private request info (changable)
  /**
   * The current sensor settings.
   */
  std::vector<uint32_t> mSensorMode;
  std::vector<MSize> mSensorSize;

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Internal Operations.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////    Template Methods.
  auto getCurrentPipelineContext() const
      -> std::shared_ptr<PipelineContext> override;

  auto submitOneRequest(std::shared_ptr<ParsedAppRequest> const& request)
      -> int override;

  virtual auto processReconfiguration(
      policy::pipelinesetting::RequestOutputParams* rcfOutputParam,
      std::shared_ptr<ConfigInfo2>* pConfigInfo2,
      MUINT32 requestNo) -> int;

  virtual auto processEndSubmitOneRequest(
      policy::pipelinesetting::RequestOutputParams* rcfOutputParam) -> int;

  ////    Configuration Stage.

  // create capture related instance
  virtual auto configureCaptureInFlight(const int maxJpegNum) -> int;

 protected:  ////    Operations.
  auto updateFrameTimestamp(MUINT32 const requestNo,
                            MINTPTR const userId,
                            Result const& result,
                            int64_t timestampStartOfFrame) -> void override;

 public:  ////    Utility.
  static auto print(ConfigInfo2 const& o);

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interfaces (called by Session Factory).
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////    Instantiation.
  static auto makeInstance(std::string const& name,
                           CtorParams const& rCtorParams)
      -> std::shared_ptr<IPipelineModelSession>;

  PipelineModelSessionDefault(std::string const& name,
                              CtorParams const& rCtorParams);

  virtual ~PipelineModelSessionDefault() {
    pthread_rwlock_destroy(&mRWLock_ConfigInfo2);
    pthread_rwlock_destroy(&mRWLock_PipelineContext);
  }

 public:  ////    Configuration.
  virtual auto configure() -> int;
  virtual auto updateBeforeBuildPipelineContext() -> int;

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineModelSession Interfaces.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  auto dumpState(const std::vector<std::string>& options) -> void override;

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineBufferSetFrameControl::IAppCallback Interfaces.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  MVOID updateFrame(MUINT32 const requestNo,
                    MINTPTR const userId,
                    Result const& result) override;

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineModelSession Interfaces.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  auto beginFlush() -> int override;
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  NSPipelineContext::DataCallbackBase Interfaces.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  MVOID onNextCaptureCallBack(MUINT32 requestNo, MINTPTR nodeId) override;
};

/*****************************************************************************
 *
 *****************************************************************************/
};      // namespace model
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_SESSION_PIPELINEMODELSESSIONDEFAULT_H_
