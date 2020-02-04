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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_PROCESSOR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_PROCESSOR_H_

#include "P2_Processor_t.h"

#include "P2_LogHeaderBegin.h"
#include <mtkcam/utils/debug/P2_DebugControl.h>
#define P2_CLASS_TAG Processor
#define P2_TRACE TRACE_PROCESSOR
#include "P2_LogHeader.h"

#include <memory>
#include <string>

#define DEFAULT_THREAD_POLICY SCHED_OTHER
#define DEFAULT_THREAD_PRIORITY -2

namespace P2 {

template <typename Init_T, typename Config_T, typename Enque_T>
Processor<Init_T, Config_T, Enque_T>::Processor(const std::string& name)
    : mName(name), mEnable(MTRUE), mIdleWaitMS(0), mNeedThread(MTRUE) {
  TRACE_S_FUNC_ENTER(mName);
  TRACE_S_FUNC_EXIT(mName);
}

template <typename Init_T, typename Config_T, typename Enque_T>
Processor<Init_T, Config_T, Enque_T>::Processor(const std::string& name,
                                                MINT32 policy,
                                                MINT32 priority)
    : mName(name), mEnable(MTRUE), mIdleWaitMS(0), mNeedThread(MTRUE) {
  TRACE_S_FUNC_ENTER(mName);
  TRACE_S_FUNC_EXIT(mName);
}

template <typename Init_T, typename Config_T, typename Enque_T>
Processor<Init_T, Config_T, Enque_T>::~Processor() {
  TRACE_S_FUNC_ENTER(mName);
  if (mThread.get() != nullptr) {
    MY_S_LOGE(mName,
              "Processor::uninit() not called: Child class MUST ensure "
              "uninit() in own destructor");
    mThread = nullptr;
  }
  TRACE_S_FUNC_EXIT(mName);
}

template <typename Init_T, typename Config_T, typename Enque_T>
const char* Processor<Init_T, Config_T, Enque_T>::getName() const {
  return mName.c_str();
}

template <typename Init_T, typename Config_T, typename Enque_T>
MBOOL Processor<Init_T, Config_T, Enque_T>::setEnable(MBOOL enable) {
  TRACE_S_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> _lock(mThreadMutex);
  if (mThread == nullptr) {
    mEnable = enable;
  }
  TRACE_S_FUNC_EXIT(mName);
  return mEnable;
}

template <typename Init_T, typename Config_T, typename Enque_T>
MBOOL Processor<Init_T, Config_T, Enque_T>::isEnabled() {
  TRACE_S_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> _lock(mThreadMutex);
  TRACE_S_FUNC_EXIT(mName);
  return mEnable;
}

template <typename Init_T, typename Config_T, typename Enque_T>
MBOOL Processor<Init_T, Config_T, Enque_T>::setNeedThread(MBOOL isThreadNeed) {
  TRACE_S_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> _lock(mThreadMutex);
  if (mThread == nullptr) {
    mNeedThread = isThreadNeed;
  }
  TRACE_S_FUNC_EXIT(mName);
  return mNeedThread;
}

template <typename Init_T, typename Config_T, typename Enque_T>
MVOID Processor<Init_T, Config_T, Enque_T>::setIdleWaitMS(MUINT32 ms) {
  TRACE_S_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> _lock(mThreadMutex);
  if (mThread == nullptr) {
    mIdleWaitMS = ms;
  }
  TRACE_S_FUNC_EXIT(mName);
}

template <typename Init_T, typename Config_T, typename Enque_T>
MBOOL Processor<Init_T, Config_T, Enque_T>::init(const Init_T& param) {
  TRACE_S_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> _lock(mThreadMutex);
  MBOOL ret = !mEnable;
  if (mEnable && mThread == nullptr) {
    if (this->onInit(param)) {
      mThread = std::make_shared<ProcessThread>(this->shared_from_this(),
                                                mNeedThread);
      if (mThread == nullptr) {
        MY_S_LOGE(mName, "OOM: cannot create ProcessThread");
        this->onUninit();
      } else {
        if (mNeedThread) {
          mThread->run();
        } else {
          this->onThreadStart();
        }
        ret = MTRUE;
      }
    }
  }
  TRACE_S_FUNC_EXIT(mName);
  return ret;
}

template <typename Init_T, typename Config_T, typename Enque_T>
MVOID Processor<Init_T, Config_T, Enque_T>::uninit() {
  TRACE_S_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> _lock(mThreadMutex);
  if (mThread != nullptr) {
    mThread->stop();
    if (mThread->join() != OK) {
      MY_S_LOGW(mName, "ProcessThread join failed");
    }
    if (!mNeedThread) {
      this->onThreadStop();
    }
    mThread = nullptr;
    this->onUninit();
  }
  TRACE_S_FUNC_EXIT(mName);
}

template <typename Init_T, typename Config_T, typename Enque_T>
MBOOL Processor<Init_T, Config_T, Enque_T>::config(const Config_T& param) {
  TRACE_S_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> _lock(mThreadMutex);
  MBOOL ret = !mEnable;
  if (mThread != nullptr) {
    ret = this->onConfig(param);
  }
  TRACE_S_FUNC_EXIT(mName);
  return ret;
}

template <typename Init_T, typename Config_T, typename Enque_T>
MBOOL Processor<Init_T, Config_T, Enque_T>::enque(const Enque_T& param) {
  TRACE_S_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> _lock(mThreadMutex);
  MBOOL ret = MFALSE;
  if (mThread != nullptr) {
    mThread->enque(param);
    ret = MTRUE;
  }
  TRACE_S_FUNC_EXIT(mName);
  return ret;
}

template <typename Init_T, typename Config_T, typename Enque_T>
MVOID Processor<Init_T, Config_T, Enque_T>::flush() {
  TRACE_S_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> _lock(mThreadMutex);
  if (mThread != nullptr) {
    this->onNotifyFlush();
    mThread->flush();
    this->onWaitFlush();
  }
  TRACE_S_FUNC_EXIT(mName);
}

template <typename Init_T, typename Config_T, typename Enque_T>
MVOID Processor<Init_T, Config_T, Enque_T>::notifyFlush() {
  TRACE_S_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> _lock(mThreadMutex);
  if (mThread != nullptr) {
    this->onNotifyFlush();
  }
  TRACE_S_FUNC_EXIT(mName);
}

template <typename Init_T, typename Config_T, typename Enque_T>
MVOID Processor<Init_T, Config_T, Enque_T>::waitFlush() {
  TRACE_S_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> _lock(mThreadMutex);
  if (mThread != nullptr) {
    mThread->flush();
    this->onWaitFlush();
  }
  TRACE_S_FUNC_EXIT(mName);
}

template <typename Init_T, typename Config_T, typename Enque_T>
Processor<Init_T, Config_T, Enque_T>::ProcessThread::ProcessThread(
    std::shared_ptr<Processor> parent, MBOOL needThread)
    : mParent(parent),
      mName(parent->mName),
      mIdleWaitTime(NSCam::Utils::ms2ns(parent->mIdleWaitMS)),
      mStop(MFALSE),
      mIdle(MTRUE),
      mNeedThread(needThread) {
  TRACE_S_FUNC_ENTER(mName);
  TRACE_S_FUNC_EXIT(mName);
}

template <typename Init_T, typename Config_T, typename Enque_T>
Processor<Init_T, Config_T, Enque_T>::ProcessThread::~ProcessThread() {
  TRACE_S_FUNC_ENTER(mName);
  TRACE_S_FUNC_EXIT(mName);
}

template <typename Init_T, typename Config_T, typename Enque_T>
status_t Processor<Init_T, Config_T, Enque_T>::ProcessThread::readyToRun() {
  TRACE_S_FUNC_ENTER(mName);
  TRACE_S_FUNC_EXIT(mName);
  return 0;
}

template <typename Init_T, typename Config_T, typename Enque_T>
status_t Processor<Init_T, Config_T, Enque_T>::ProcessThread::run() {
  TRACE_S_FUNC_ENTER(mName);
  mThread = std::thread(std::bind(
      &Processor<Init_T, Config_T, Enque_T>::ProcessThread::threadLoop, this));
  TRACE_S_FUNC_EXIT(mName);
  return 0;
}

template <typename Init_T, typename Config_T, typename Enque_T>
status_t Processor<Init_T, Config_T, Enque_T>::ProcessThread::join() {
  TRACE_S_FUNC_ENTER(mName);
  if (mThread.joinable()) {
    mThread.join();
  }
  TRACE_S_FUNC_EXIT(mName);
  return 0;
}

template <typename Init_T, typename Config_T, typename Enque_T>
bool Processor<Init_T, Config_T, Enque_T>::ProcessThread::threadLoop() {
  while (this->_threadLoop() == true) {
  }
  return true;
}

template <typename Init_T, typename Config_T, typename Enque_T>
bool Processor<Init_T, Config_T, Enque_T>::ProcessThread::_threadLoop() {
  TRACE_S_FUNC_ENTER(mName);
  Enque_T param;
  WaitResult waitResult;
  mParent->onThreadStart();
  do {
    waitResult = waitEnqueParam(&param);
    if (waitResult == WAIT_OK) {
      mParent->onEnque(param);
    } else if (waitResult == WAIT_IDLE) {
      mParent->onIdle();
    }
    param = Enque_T();
  } while (waitResult != WAIT_EXIT);
  mParent->onThreadStop();
  TRACE_S_FUNC_EXIT(mName);
  return false;
}

template <typename Init_T, typename Config_T, typename Enque_T>
MVOID Processor<Init_T, Config_T, Enque_T>::ProcessThread::enque(
    const Enque_T& param) {
  TRACE_S_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> _lock(mMutex);
  if (mNeedThread) {
    mQueue.push(param);
    mCondition.notify_all();
  } else {
    mParent->onEnque(param);
  }
  TRACE_S_FUNC_EXIT(mName);
}

template <typename Init_T, typename Config_T, typename Enque_T>
MVOID Processor<Init_T, Config_T, Enque_T>::ProcessThread::flush() {
  TRACE_S_FUNC_ENTER(mName);
  std::unique_lock<std::mutex> _lock(mMutex);
  while (!mIdle) {
    mCondition.wait(_lock);
  }
  TRACE_S_FUNC_EXIT(mName);
}

template <typename Init_T, typename Config_T, typename Enque_T>
MVOID Processor<Init_T, Config_T, Enque_T>::ProcessThread::stop() {
  TRACE_S_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> _lock(mMutex);
  mStop = MTRUE;
  mCondition.notify_all();
  TRACE_S_FUNC_EXIT(mName);
}

template <typename Init_T, typename Config_T, typename Enque_T>
typename Processor<Init_T, Config_T, Enque_T>::ProcessThread::WaitResult
Processor<Init_T, Config_T, Enque_T>::ProcessThread::waitEnqueParam(
    Enque_T* param) {
  TRACE_S_FUNC_ENTER(mName);
  std::unique_lock<std::mutex> _lock(mMutex);
  WaitResult result = WAIT_ERROR;
  MBOOL needWaitIdle = MFALSE;
  if (mQueue.size() == 0) {
    if (!mIdle) {
      mIdle = MTRUE;
      needWaitIdle = MTRUE;
      mCondition.notify_all();
    }

    if (mStop) {
      result = WAIT_EXIT;
    } else if (needWaitIdle && mIdleWaitTime) {
      std::cv_status status =
          mCondition.wait_for(_lock, std::chrono::nanoseconds(mIdleWaitTime));
      if (status == std::cv_status::timeout) {
        result = WAIT_IDLE;
      }
    } else {
      mCondition.wait(_lock);
    }
  }
  if (mQueue.size()) {
    *param = mQueue.front();
    mQueue.pop();
    mIdle = MFALSE;
    result = WAIT_OK;
  }
  TRACE_S_FUNC_EXIT(mName);
  return result;
}

}  // namespace P2

#include "P2_LogHeaderEnd.h"
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_PROCESSOR_H_
