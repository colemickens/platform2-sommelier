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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_CAPTURE_INCLUDE_IMPL_INEXTCAPTURELISTENER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_CAPTURE_INCLUDE_IMPL_INEXTCAPTURELISTENER_H_

#include "ICaptureInFlightRequest.h"
#include <memory>
#include <mtkcam/pipeline/model/IPipelineModelCallback.h>
#include <mtkcam/pipeline/stream/IStreamInfo.h>
#include <string>

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
class INextCaptureListener : public ICaptureInFlightListener {
 public:  ////
  struct CtorParams {
    int32_t maxJpegNum;
    std::weak_ptr<IPipelineModelCallback> pCallback;
  };
  virtual ~INextCaptureListener() = default;
  static auto createInstance(int32_t openId,
                             std::string const& name,
                             CtorParams const& rCtorParams)
      -> std::shared_ptr<INextCaptureListener>;
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  ICaptureInFlightListener Interfaces.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  virtual auto onCaptureInFlightUpdated(CaptureInFlightUpdated const& params)
      -> void = 0;

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  NSPipelineContext::DataCallbackBase Interfaces.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  virtual MVOID onNextCaptureCallBack(MUINT32 requestNo) = 0;
};

};      // namespace model
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_CAPTURE_INCLUDE_IMPL_INEXTCAPTURELISTENER_H_
