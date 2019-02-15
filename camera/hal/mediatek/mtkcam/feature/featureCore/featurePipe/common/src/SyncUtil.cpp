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

#include "../include/SyncUtil.h"

#include "../include/DebugControl.h"
#define PIPE_TRACE TRACE_SYNC_UTIL
#define PIPE_CLASS_TAG "SyncUtil"
#include "../include/PipeLog.h"

#include <memory>

#define NS_PER_MS 1000000ULL

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

NotifyCB::NotifyCB() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

NotifyCB::~NotifyCB() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

StatusCB::StatusCB() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

StatusCB::~StatusCB() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

WaitNotifyCB::WaitNotifyCB() : mDone(MFALSE) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

WaitNotifyCB::~WaitNotifyCB() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

MBOOL WaitNotifyCB::onNotify() {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mMutex);
  mDone = MTRUE;
  mCondition.notify_all();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL WaitNotifyCB::wait() {
  TRACE_FUNC_ENTER();
  std::unique_lock<std::mutex> lck(mMutex);
  while (!mDone) {
    mCondition.wait(lck);
  }
  TRACE_FUNC_EXIT();
  return MTRUE;
}

BacktraceNotifyCB::BacktraceNotifyCB() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

BacktraceNotifyCB::~BacktraceNotifyCB() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

MBOOL BacktraceNotifyCB::onNotify() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

TimeoutCB::TimeoutCB(unsigned timeout) : mTimeout(timeout) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

TimeoutCB::~TimeoutCB() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

unsigned TimeoutCB::getTimeout() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return mTimeout;
}

uint64_t TimeoutCB::getTimeoutNs() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return mTimeout * NS_PER_MS;
}

MBOOL TimeoutCB::insertCB(const std::shared_ptr<NotifyCB>& cb) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> lock(mMutex);
  if (cb != NULL) {
    this->mCB.push_back(cb);
    ret = MTRUE;
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL TimeoutCB::onTimeout() {
  TRACE_FUNC_ENTER();
  MBOOL ret = MTRUE;
  std::lock_guard<std::mutex> lock(mMutex);
  for (unsigned i = 0, n = mCB.size(); i < n; ++i) {
    if (mCB[i] != NULL && mCB[i]->onNotify() == MFALSE) {
      ret = MFALSE;
      break;
    }
  }
  TRACE_FUNC_EXIT();
  return ret;
}

CountDownLatch::CountDownLatch(unsigned total) : mTotal(total), mDone(0) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

CountDownLatch::~CountDownLatch() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

MBOOL CountDownLatch::registerTimeoutCB(const std::shared_ptr<TimeoutCB>& cb) {
  TRACE_FUNC_ENTER();
  mMutex.lock();
  mTimeoutCB = cb;
  mMutex.unlock();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

void CountDownLatch::countDown() {
  TRACE_FUNC_ENTER();
  mMutex.lock();
  ++mDone;
  mCondition.notify_all();
  mMutex.unlock();
  TRACE_FUNC_EXIT();
}

void CountDownLatch::countBackUp() {
  TRACE_FUNC_ENTER();
  mMutex.lock();
  --mDone;
  mCondition.notify_all();
  mMutex.unlock();
  TRACE_FUNC_EXIT();
}

void CountDownLatch::wait() {
  TRACE_FUNC_ENTER();
  std::unique_lock<std::mutex> lck(mMutex);
  uint64_t timeout = (mTimeoutCB != NULL) ? mTimeoutCB->getTimeoutNs() : 0;
  while (mDone < mTotal) {
    if (timeout > 0) {
      if (mCondition.wait_for(lck, std::chrono::nanoseconds(timeout)) ==
          std::cv_status::timeout) {
        MY_LOGW("CountDownLatch timeout(%llu) done(%d) total(%d)", timeout,
                mDone, mTotal);
        if (mTimeoutCB != NULL) {
          mTimeoutCB->onTimeout();
        }
      }
    } else {
      mCondition.wait(lck);
    }
  }
  TRACE_FUNC_EXIT();
}

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam
