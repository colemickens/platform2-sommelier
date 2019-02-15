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

#include <sys/resource.h>
#include "../include/DebugControl.h"
#define PIPE_TRACE TRACE_CAM_THREAD
#define PIPE_CLASS_TAG "CamThread"
#include "../include/PipeLog.h"

#include "../include/CamThread.h"
#include <functional>
#include <memory>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

CamThread::CamThread(const char* name)
    : WaitHub(name), mHandle(nullptr), mExtThreadDependency(0) {
  TRACE_N_FUNC_ENTER(mName);
  TRACE_N_FUNC_EXIT(mName);
}

CamThread::CamThread(const char* name, MUINT32 policy, MUINT32 priority)
    : WaitHub(name), mHandle(nullptr), mExtThreadDependency(0) {
  TRACE_N_FUNC_ENTER(mName);
  TRACE_N_FUNC_EXIT(mName);
}

CamThread::~CamThread() {
  TRACE_N_FUNC_ENTER(mName);
  if (mHandle != nullptr) {
    MY_LOGE("Child class MUST call stopThread() in own destrctor()");
    mHandle = nullptr;
  }
  TRACE_N_FUNC_EXIT(mName);
}

MBOOL CamThread::startThread() {
  TRACE_N_FUNC_ENTER(mName);

  std::lock_guard<std::mutex> lock(mThreadMutex);
  MBOOL ret = MFALSE;

  if (mHandle == nullptr) {
    this->resetSignal();
    mHandle = std::make_shared<CamThreadHandle>(shared_from_this());
    mHandle->run();
    ret = MTRUE;
  }

  TRACE_N_FUNC_EXIT(mName);
  return ret;
}

MBOOL CamThread::stopThread() {
  TRACE_N_FUNC_ENTER(mName);
  MBOOL ret = MFALSE;
  mThreadMutex.lock();
  if (mHandle != nullptr) {
    std::shared_ptr<CamThreadHandle> handle = mHandle;
    this->triggerSignal(WaitHub::SIGNAL_STOP);
    mThreadMutex.unlock();
    ret = (handle->join() == OK);
    mThreadMutex.lock();
    handle = nullptr;
    mHandle = nullptr;
    mCB.clear();
    mIdleCB.clear();
    mStatusCB = NULL;
  }
  TRACE_N_FUNC_EXIT(mName);
  mThreadMutex.unlock();
  return ret;
}

MVOID CamThread::triggerDryRun() {
  TRACE_N_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> lock(mThreadMutex);
  this->triggerSignal(WaitHub::SIGNAL_DRY_RUN);
  TRACE_N_FUNC_EXIT(mName);
}

MBOOL CamThread::insertCB(const std::shared_ptr<NotifyCB>& cb) {
  TRACE_N_FUNC_ENTER(mName);
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> lock(mThreadMutex);
  if (cb != nullptr) {
    this->mCB.push_back(cb);
    this->triggerSignal(WaitHub::SIGNAL_CB);
    ret = MTRUE;
  }
  TRACE_N_FUNC_EXIT(mName);
  return ret;
}

MBOOL CamThread::insertIdleCB(const std::shared_ptr<NotifyCB>& cb) {
  TRACE_N_FUNC_ENTER(mName);
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> lock(mThreadMutex);
  if (cb != nullptr) {
    this->mIdleCB.push_back(cb);
    this->triggerSignal(WaitHub::SIGNAL_IDLE_CB);
    ret = MTRUE;
  }
  TRACE_N_FUNC_EXIT(mName);
  return ret;
}

MBOOL CamThread::registerStatusCB(const std::shared_ptr<StatusCB>& cb) {
  TRACE_N_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> lock(mThreadMutex);
  this->mStatusCB = cb;
  if (this->mStatusCB != nullptr) {
    this->triggerSignal(WaitHub::SIGNAL_SYNC_CB |
                        WaitHub::SIGNAL_NEED_SYNC_BREAK);
  } else {
    this->resetSignal(WaitHub::SIGNAL_SYNC_CB |
                      WaitHub::SIGNAL_NEED_SYNC_BREAK);
  }
  TRACE_N_FUNC_EXIT(mName);
  return MTRUE;
}

MBOOL CamThread::waitIdle() {
  TRACE_N_FUNC_ENTER(mName);
  MBOOL ret = MFALSE;
  std::shared_ptr<WaitNotifyCB> waiter = std::make_shared<WaitNotifyCB>();
  if (this->insertIdleCB(waiter)) {
    ret = waiter->wait();
  }
  TRACE_N_FUNC_EXIT(mName);
  return ret;
}

MVOID CamThread::incExtThreadDependency() {
  TRACE_N_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> lock(mThreadMutex);
  if (++mExtThreadDependency == 1) {
    if (this->mStatusCB != nullptr) {
      this->triggerSignal(WaitHub::SIGNAL_NEED_SYNC_BREAK);
    }
  }
  TRACE_N_FUNC_EXIT(mName);
}

MVOID CamThread::decExtThreadDependency() {
  TRACE_N_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> lock(mThreadMutex);
  if (--mExtThreadDependency == 0) {
    if (this->mStatusCB != NULL) {
      this->triggerSignal(WaitHub::SIGNAL_NEED_SYNC_BREAK);
    }
  }
  TRACE_N_FUNC_EXIT(mName);
}

MINT32 CamThread::getExtThreadDependency() {
  std::lock_guard<std::mutex> lock(mThreadMutex);
  TRACE_N_FUNC_ENTER(mName);
  TRACE_N_FUNC_EXIT(mName);
  return mExtThreadDependency;
}

MVOID CamThread::dumpCamThreadInfo() {
  std::lock_guard<std::mutex> lock(mThreadMutex);
  TRACE_N_FUNC_ENTER(mName);
  MY_LOGW("%s extThreadDependency=%d mStatusCB=%p status=%d", mName,
          mExtThreadDependency, this->mStatusCB.get(),
          this->mStatusCB != NULL ? this->mStatusCB->getStatus() : 0);
  TRACE_N_FUNC_EXIT(mName);
}

MBOOL CamThread::tryProcessStop(MUINT32 signal) {
  TRACE_N_FUNC_ENTER(mName);
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> lock(mThreadMutex);
  ret = signal & WaitHub::SIGNAL_STOP;
  TRACE_N_FUNC_EXIT(mName);
  return ret;
}

MBOOL CamThread::tryProcessCB(MUINT32 signal) {
  TRACE_N_FUNC_ENTER(mName);
  MBOOL ret = MFALSE;
  mThreadMutex.lock();
  if (signal & WaitHub::SIGNAL_CB) {
    while (!mCB.empty()) {
      ret = MTRUE;
      std::shared_ptr<NotifyCB> cb = mCB[0];
      mCB.pop_front();
      mThreadMutex.unlock();
      cb->onNotify();
      mThreadMutex.lock();
    }
    this->resetSignal(WaitHub::SIGNAL_CB);
  }
  TRACE_N_FUNC_EXIT(mName);
  mThreadMutex.unlock();
  return ret;
}

MBOOL CamThread::tryProcessIdleCB(MUINT32 signal) {
  TRACE_N_FUNC_ENTER(mName);
  MBOOL ret = MFALSE;
  mThreadMutex.lock();
  if ((signal & WaitHub::SIGNAL_IDLE_CB) && (signal & WaitHub::SIGNAL_IDLE)) {
    ret = MTRUE;
    if (!mIdleCB.empty()) {
      std::shared_ptr<NotifyCB> cb = mIdleCB[0];
      mIdleCB.pop_front();
      mThreadMutex.unlock();
      cb->onNotify();
      mThreadMutex.lock();
    }
    if (mIdleCB.empty()) {
      this->resetSignal(WaitHub::SIGNAL_IDLE_CB);
    }
  }
  TRACE_N_FUNC_EXIT(mName);
  mThreadMutex.unlock();
  return ret;
}

MBOOL CamThread::tryProcessStatusCB(MUINT32 signal) {
  TRACE_N_FUNC_ENTER(mName);
  MBOOL ret = MFALSE;
  MBOOL inSync;
  mThreadMutex.lock();
  if ((signal & WaitHub::SIGNAL_IDLE) &&
      (signal & WaitHub::SIGNAL_NEED_SYNC_BREAK)) {
    this->resetSignal(WaitHub::SIGNAL_NEED_SYNC_BREAK);
  }
  if (signal & WaitHub::SIGNAL_SYNC_CB) {
    ret = MTRUE;
    if (mStatusCB != nullptr) {
      inSync = (signal & WaitHub::SIGNAL_IDLE) && !mExtThreadDependency &&
               this->isAllQueueEmpty();
      std::shared_ptr<StatusCB> cb = mStatusCB;
      mThreadMutex.unlock();
      cb->onUpdate(inSync);
      mThreadMutex.lock();
    }
  }
  TRACE_N_FUNC_EXIT(mName);
  mThreadMutex.unlock();
  return ret;
}

CamThread::CamThreadHandle::CamThreadHandle(std::shared_ptr<CamThread> parent)
    : mParent(parent), mIsFirst(MTRUE) {
  TRACE_N_FUNC_ENTER(mParent->mName);
  TRACE_N_FUNC_EXIT(mParent->mName);
}

CamThread::CamThreadHandle::~CamThreadHandle() {
  TRACE_N_FUNC_ENTER(mParent->mName);
  TRACE_N_FUNC_EXIT(mParent->mName);
}

MERROR CamThread::CamThreadHandle::run() {
  TRACE_N_FUNC_ENTER(mParent->mName);
  mThread =
      std::thread(std::bind(&CamThread::CamThreadHandle::threadLoop, this));
  TRACE_N_FUNC_EXIT(mParent->mName);
  return NO_ERROR;
}

MERROR CamThread::CamThreadHandle::join() {
  TRACE_N_FUNC_ENTER(mParent->mName);
  if (mThread.joinable()) {
    mThread.join();
  }
  TRACE_N_FUNC_EXIT(mParent->mName);
  return NO_ERROR;
}

bool CamThread::CamThreadHandle::threadLoop() {
  while (this->_threadLoop() == MTRUE) {
  }
  return MTRUE;
}

bool CamThread::CamThreadHandle::_threadLoop() {
  TRACE_N_FUNC_ENTER(mParent->mName);
  MBOOL ret = MTRUE;
  MUINT32 signal;

  if (mIsFirst) {
    mIsFirst = MFALSE;
    if (!mParent->onThreadStart()) {
      ret = MFALSE;
    }
  }
  if (ret) {
    signal = mParent->waitSignal();
    if ((signal & WaitHub::SIGNAL_STOP) && mParent->tryProcessStop(signal)) {
      mParent->onThreadStop();
      ret = MFALSE;
    }

    if (ret) {
      if (signal & WaitHub::SIGNAL_CB) {
        mParent->tryProcessCB(signal);
      }
      if (signal & WaitHub::SIGNAL_IDLE_CB) {
        mParent->tryProcessIdleCB(signal);
      }
      if (signal & WaitHub::SIGNAL_SYNC_CB) {
        mParent->tryProcessStatusCB(signal);
      }
      mParent->shiftSignal(WaitHub::SIGNAL_DRY_RUN,
                           WaitHub::SIGNAL_DRY_RUN_ONCE);
      mParent->onThreadLoop();
      mParent->resetSignal(WaitHub::SIGNAL_DRY_RUN_ONCE);
    }
  }

  TRACE_N_FUNC_EXIT(mParent->mName);
  return ret;
}

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam
