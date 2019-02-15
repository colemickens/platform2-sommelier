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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_SESSION_PIPELINEMODELSESSIONSTREAMING_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_SESSION_PIPELINEMODELSESSIONSTREAMING_H_
//
#include "PipelineModelSessionDefault.h"
#include <memory>
#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/utils/LogicalCam/Type.h>
#include <string>
#include <unordered_map>
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
class PipelineModelSessionStreaming : public PipelineModelSessionDefault {
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interfaces (called by Session Factory).
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////    Instantiation.
  static auto makeInstance(std::string const& name,
                           CtorParams const& rCtorParams)
      -> std::shared_ptr<IPipelineModelSession>;

  PipelineModelSessionStreaming(std::string const& name,
                                CtorParams const& rCtorParams);

  virtual ~PipelineModelSessionStreaming();
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineBufferSetFrameControl::IAppCallback Interfaces.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

 protected:
  // methods
  auto processReconfiguration(
      policy::pipelinesetting::RequestOutputParams* rcfOutputParam,
      std::shared_ptr<ConfigInfo2>* pConfigInfo2 __unused,
      MUINT32 requestNo) -> int override;

  template <typename _Node_>
  auto waitUntilNodeDrainedAndFlush(
      NodeId_T const nodeId,
      std::shared_ptr<NSPipelineContext::NodeActor<_Node_>>* pNodeActor)
      -> MERROR;

  // variables
  mutable pthread_rwlock_t mRWLock_vCapConfigInfo2;
  std::unordered_map<MUINT32, std::shared_ptr<ConfigInfo2>> mvCapConfigInfo2;

 private:
  auto waitUntilP1NodeDrainedAndFlush() -> MERROR;
  auto waitUntilP2DrainedAndFlush() -> MERROR;

  auto processReconfigStream(
      policy::pipelinesetting::RequestOutputParams* rcfOutputParam,
      std::shared_ptr<ConfigInfo2>* pConfigInfo2,
      MUINT32 requestNo) -> int;
};

/*****************************************************************************
 *
 *****************************************************************************/
};      // namespace model
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_SESSION_PIPELINEMODELSESSIONSTREAMING_H_
