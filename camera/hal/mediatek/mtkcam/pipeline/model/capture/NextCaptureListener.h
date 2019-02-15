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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_CAPTURE_NEXTCAPTURELISTENER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_CAPTURE_NEXTCAPTURELISTENER_H_

#include <list>
#include <memory>
#include <mutex>
#include <string>
#include "impl/INextCaptureListener.h"

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
class NextCaptureListener : public INextCaptureListener {
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////    Instantiation data (initialized at the creation stage).
  int32_t const mOpenId;
  std::string mUserName;
  int32_t mMaxJpegNum;
  int32_t mInFlightJpeg;

  mutable std::mutex mInFlightJpegRequestNoMutex;
  std::list<uint32_t> mlRequestNo;

  std::weak_ptr<IPipelineModelCallback> mpPipelineModelCallback;
  std::shared_ptr<IMetaStreamInfo> mpStreamInfo;
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Operations.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////    Instantiation.
  NextCaptureListener(int32_t openId,
                      std::string const& name,
                      INextCaptureListener::CtorParams const& rCtorParams);

 protected:
  virtual auto onNextCaptureUpdated(uint32_t requestNo) -> void;
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  ICaptureInFlightListener Interfaces.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  auto onCaptureInFlightUpdated(CaptureInFlightUpdated const& params)
      -> void override;

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  NSPipelineContext::DataCallbackBase Interfaces.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  MVOID onNextCaptureCallBack(MUINT32 requestNo) override;
};

};      // namespace model
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_CAPTURE_NEXTCAPTURELISTENER_H_
