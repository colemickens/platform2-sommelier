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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_CAPTURE_INCLUDE_IMPL_ICAPTUREINFLIGHTREQUEST_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_CAPTURE_INCLUDE_IMPL_ICAPTUREINFLIGHTREQUEST_H_
#include "MyUtils.h"
#include "CaptureInFlightTypes.h"

#include <memory>
#include <string>
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
class ICaptureInFlightListener {
 public:  ////    Interfaces.
  virtual auto onCaptureInFlightUpdated(CaptureInFlightUpdated const& params)
      -> void = 0;
};

/******************************************************************************
 *
 ******************************************************************************/
class ICaptureInFlightRequest {
 public:  ////
  virtual ~ICaptureInFlightRequest() {}

  static auto createInstance(int32_t openId, std::string const& name)
      -> std::shared_ptr<ICaptureInFlightRequest>;

  virtual auto registerListener(
      std::weak_ptr<ICaptureInFlightListener> const pListener) -> int = 0;
  virtual auto removeListener(
      std::weak_ptr<ICaptureInFlightListener> const pListener) -> int = 0;
  virtual auto insertRequest(uint32_t const requestNo, uint32_t const message)
      -> int = 0;

  virtual auto removeRequest(uint32_t const requestNo) -> int = 0;
};

};      // namespace model
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_CAPTURE_INCLUDE_IMPL_ICAPTUREINFLIGHTREQUEST_H_
