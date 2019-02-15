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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_CAPTURE_CAPTUREINFLIGHTREQUEST_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_CAPTURE_CAPTUREINFLIGHTREQUEST_H_

#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include "impl/ICaptureInFlightRequest.h"
#include <mtkcam/def/Errors.h>

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
class CaptureInFlightRequest : public ICaptureInFlightRequest {
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////    Instantiation data (initialized at the creation stage).
  int32_t const mOpenId;
  std::string mUserName;
  int32_t mLogLevel;

  std::condition_variable mCond;
  mutable std::mutex mMutex;
  bool mQuitThread;
  mutable std::mutex mListenerListLock;
  std::list<std::weak_ptr<ICaptureInFlightListener>> mlListener;

  std::list<uint32_t> mvInflightCaptureRequestNo;
  CaptureInFlightUpdated mParams;
  std::thread mCapIFReqThread;
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Operations.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////    Instantiation.
  CaptureInFlightRequest(int32_t openId, std::string const& name);
  virtual ~CaptureInFlightRequest();
  void onLastStrongRef(const void* /*id*/);
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  ICaptureInFlightRequest Interfaces.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  auto registerListener(std::weak_ptr<ICaptureInFlightListener> const pListener)
      -> int override;
  auto removeListener(std::weak_ptr<ICaptureInFlightListener> const pListener)
      -> int override;
  auto insertRequest(uint32_t const requestNo, uint32_t const message)
      -> int override;
  auto removeRequest(uint32_t const requestNo) -> int override;

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Thread Interface.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  // Good place to do one-time initializations
  virtual status_t readyToRun();

 private:
  virtual status_t threadLoop();

  virtual bool threadLoop_l();
};

};  // namespace model
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_CAPTURE_CAPTUREINFLIGHTREQUEST_H_
