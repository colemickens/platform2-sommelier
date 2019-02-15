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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMTHREADNODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMTHREADNODE_H_

#include <memory>
#include <mutex>

#include "PipeLogHeaderBegin.h"
#include "DebugControl.h"
#define PIPE_TRACE TRACE_CAM_THREAD_NODE
#define PIPE_CLASS_TAG "CamThreadNode"
#include "PipeLog.h"
#include "../include/CamThreadNode_t.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

template <typename Handler_T>
CamThreadNode<Handler_T>::CamThreadNode(const char* name)
    : CamNode<Handler_T>(name), CamThread(name) {}

template <typename Handler_T>
CamThreadNode<Handler_T>::CamThreadNode(const char* name,
                                        int policy,
                                        int priority)
    : CamNode<Handler_T>(name), CamThread(name, policy, priority) {}

template <typename Handler_T>
CamThreadNode<Handler_T>::~CamThreadNode() {}

template <typename Handler_T>
const char* CamThreadNode<Handler_T>::getName() const {
  return CamNode<Handler_T>::getName();
}

template <typename Handler_T>
CamThreadNode<Handler_T>::FlushWrapper::FlushWrapper(
    std::shared_ptr<CamThreadNode<Handler_T>> parent,
    const std::shared_ptr<NotifyCB>& cb)
    : mParent(parent), mCB(cb) {}

template <typename Handler_T>
CamThreadNode<Handler_T>::FlushWrapper::~FlushWrapper() {}

template <typename Handler_T>
MBOOL CamThreadNode<Handler_T>::FlushWrapper::onNotify() {
  if (mParent) {
    mParent->onFlush();
    if (mCB != NULL) {
      mCB->onNotify();
    }
  }
  return MTRUE;
}

template <typename Handler_T>
MVOID CamThreadNode<Handler_T>::flush(const std::shared_ptr<NotifyCB>& cb) {
  TRACE_FUNC_ENTER();
  std::shared_ptr<NotifyCB> wrapper = std::make_shared<FlushWrapper>(
      std::dynamic_pointer_cast<CamThreadNode<Handler_T>>(shared_from_this()),
      cb);
  this->insertCB(wrapper);
  TRACE_FUNC_EXIT();
}

template <typename T>
CamThreadNode<T>::SyncCounterCB::SyncCounterCB(
    const char* name, const std::shared_ptr<CountDownLatch>& cb)
    : mName(name), mCB(cb), mIsSync(0) {}

template <typename T>
MBOOL CamThreadNode<T>::SyncCounterCB::onUpdate(MINT32 isSync) {
  TRACE_N_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> lock(mMutex);
  TRACE_N_FUNC(mName, "sync %d => %d", mIsSync, isSync);
  if (isSync && !mIsSync) {
    mCB->countDown();
  } else if (!isSync && mIsSync) {
    mCB->countBackUp();
  }
  mIsSync = isSync;
  TRACE_N_FUNC_EXIT(mName);
  return MTRUE;
}

template <typename T>
MINT32 CamThreadNode<T>::SyncCounterCB::getStatus() {
  TRACE_N_FUNC_ENTER(mName);
  TRACE_N_FUNC_EXIT(mName);
  return mIsSync;
}

template <typename T>
MBOOL CamThreadNode<T>::SyncCounterCB::onNotify() {
  TRACE_N_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> lock(mMutex);
  if (mIsSync) {
    // Data enqued, bring node out of sync
    TRACE_N_FUNC(mName, "sync %d => 0", mIsSync);
    mIsSync = 0;
    mCB->countBackUp();
  }
  TRACE_N_FUNC_EXIT(mName);
  return MTRUE;
}

template <typename Handler_T>
MVOID CamThreadNode<Handler_T>::registerSyncCB(
    const std::shared_ptr<CountDownLatch>& cb) {
  TRACE_FUNC_ENTER();
  std::shared_ptr<SyncCounterCB> wrapper;
  if (cb != NULL) {
    wrapper = std::make_shared<SyncCounterCB>(getName(), cb);
  }
  CamThread::registerStatusCB(wrapper);
  CamThread::registerEnqueCB(wrapper);
  TRACE_FUNC_EXIT();
}

template <typename Handler_T>
MBOOL CamThreadNode<Handler_T>::onStart() {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> lock(mMutex);
  ret = this->startThread();
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Handler_T>
MBOOL CamThreadNode<Handler_T>::onStop() {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mMutex);
  this->stopThread();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

template <typename Handler_T>
MVOID CamThreadNode<Handler_T>::onFlush() {
  TRACE_FUNC_ENTER();
  this->flushQueues();
  TRACE_FUNC_EXIT();
}

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam

#include "PipeLogHeaderEnd.h"
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMTHREADNODE_H_
