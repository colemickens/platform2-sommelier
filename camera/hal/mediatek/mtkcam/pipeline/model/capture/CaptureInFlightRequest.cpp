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

#undef LOG_TAG
#define LOG_TAG "mtkcam-CaptureInFlightRequest"
//
//
#include <mtkcam/pipeline/model/capture/CaptureInFlightRequest.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
//
//
#include "MyUtils.h"
#include <memory>
#include <string>
#include <unistd.h>
/******************************************************************************
 *
 ******************************************************************************/
using NSCam::v3::pipeline::model::CaptureInFlightRequest;
using NSCam::v3::pipeline::model::ICaptureInFlightRequest;
/******************************************************************************
 *
 ******************************************************************************/
#define CAPTUREINFLIGHTREQUEST_NAME ("Cam@CaptureInFlightRequest")
#define CAPTUREINFLIGHTREQUEST_POLICY (SCHED_OTHER)
#define CAPTUREINFLIGHTREQUEST_PRIORITY (0)

/******************************************************************************
 *
 ******************************************************************************/
auto ICaptureInFlightRequest::createInstance(int32_t openId,
                                             std::string const& name)
    -> std::shared_ptr<ICaptureInFlightRequest> {
  auto pInstance = std::make_shared<CaptureInFlightRequest>(openId, name);

  return pInstance;
}

/******************************************************************************
 *
 ******************************************************************************/
CaptureInFlightRequest::CaptureInFlightRequest(int32_t openId,
                                               std::string const& name)
    : mOpenId(openId),
      mUserName(name),
      mLogLevel(0),
      mQuitThread(false),
      mCapIFReqThread(std::bind(&CaptureInFlightRequest::threadLoop, this)) {}

CaptureInFlightRequest::~CaptureInFlightRequest() {
  MY_LOGD("deconstruction");
  onLastStrongRef(nullptr);
}

/******************************************************************************
 *
 ******************************************************************************/
void CaptureInFlightRequest::onLastStrongRef(const void* /*id*/) {
  MY_LOGD("+");
  mQuitThread = true;
  mCond.notify_one();

  MY_LOGD("thread join...");
  if (mCapIFReqThread.joinable()) {
    mCapIFReqThread.join();
  }
  MY_LOGD("-");
}

/******************************************************************************
 *
 ******************************************************************************/
auto CaptureInFlightRequest::registerListener(
    std::weak_ptr<ICaptureInFlightListener> const pListener) -> int {
  std::lock_guard<std::mutex> _l(mListenerListLock);
  mlListener.push_back(pListener);
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
auto CaptureInFlightRequest::removeListener(
    std::weak_ptr<ICaptureInFlightListener> const pListener) -> int {
  std::lock_guard<std::mutex> _l(mListenerListLock);
  auto l = pListener.lock();
  if (l == nullptr) {
    MY_LOGW("Bad listener.");
    return BAD_VALUE;
  }

  auto item = mlListener.begin();
  while (item != mlListener.end()) {
    auto lis = (*item).lock();
    if (lis == nullptr || lis == l) {
      // invalid listener or target to remove
      item = mlListener.erase(item);
    }
    item++;
  }

  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
auto CaptureInFlightRequest::insertRequest(uint32_t const requestNo,
                                           uint32_t const message __unused)
    -> int {
  std::lock_guard<std::mutex> _l(mMutex);
  auto it = std::find(mvInflightCaptureRequestNo.begin(),
                      mvInflightCaptureRequestNo.end(), requestNo);
  if (it != mvInflightCaptureRequestNo.end()) {
    MY_LOGW("requestNo(%d) already in", requestNo);
    return OK;
  }
  mvInflightCaptureRequestNo.push_back(requestNo);
  MY_LOGD("insert capture RequestNo %d, size #:%zu", requestNo,
          mvInflightCaptureRequestNo.size());
  mCond.notify_one();
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
auto CaptureInFlightRequest::removeRequest(uint32_t const requestNo) -> int {
  std::lock_guard<std::mutex> _l(mMutex);
  auto it = std::find(mvInflightCaptureRequestNo.begin(),
                      mvInflightCaptureRequestNo.end(), requestNo);
  if (it != mvInflightCaptureRequestNo.end()) {
    mvInflightCaptureRequestNo.erase(it);
    MY_LOGD("remove capture RequestNo %d, size #:%zu", requestNo,
            mvInflightCaptureRequestNo.size());
    mCond.notify_one();
  }
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
status_t CaptureInFlightRequest::readyToRun() {
  //
  return OK;
}

status_t CaptureInFlightRequest::threadLoop() {
  bool ret = true;
  while (true == ret && !mQuitThread) {
    ret = threadLoop_l();
  }
  MY_LOGD("quit threadLoop");
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
bool CaptureInFlightRequest::threadLoop_l() {
  CaptureInFlightUpdated params;
  {
    std::unique_lock<std::mutex> _l(mMutex);
    {
      mCond.wait(_l);
      params.inFlightJpegCount = mvInflightCaptureRequestNo.size();
    }
  }

  if (params.inFlightJpegCount != mParams.inFlightJpegCount) {
    std::lock_guard<std::mutex> _l(mListenerListLock);
    auto item = mlListener.begin();
    while (item != mlListener.end()) {
      auto list = (*item).lock();
      if (list == nullptr) {
        item = mlListener.erase(item);
        continue;
      }
      list->onCaptureInFlightUpdated(params);
      item++;
    }
    mParams = params;
  }

  return true;
}
