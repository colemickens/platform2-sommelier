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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_WAITQUEUE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_WAITQUEUE_H_

#include "PipeLogHeaderBegin.h"
#include "DebugControl.h"
#define PIPE_TRACE TRACE_WAIT_HUB
#define PIPE_CLASS_TAG "WaitHub"
#include "PipeLog.h"
#include "WaitQueue_t.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

template <typename F, typename T>
MBOOL WaitHub::waitCondition(F func, T* data) {
  TRACE_N_FUNC_ENTER(mName);
  const MUINT32 PRE_BREAK =
      SIGNAL_STOP | SIGNAL_CB | SIGNAL_DRY_RUN | SIGNAL_DRY_RUN_ONCE;
  const MUINT32 POST_BREAK =
      PRE_BREAK | SIGNAL_IDLE_CB | SIGNAL_NEED_SYNC_BREAK;
  MBOOL ret = MFALSE;
  MBOOL condResult;
  std::unique_lock<std::mutex> lck(mMutex);
  while (!(mSignal & PRE_BREAK)) {
    mMutex.unlock();
    condResult = func(data);
    mMutex.lock();
    if (condResult) {
      mSignal |= SIGNAL_DATA;
      mSignal &= ~SIGNAL_IDLE;
      if (mSignal & SIGNAL_SYNC_CB) {
        mSignal |= SIGNAL_NEED_SYNC_BREAK;
      }
      ret = MTRUE;
      break;
    }
    mSignal &= ~SIGNAL_DATA;
    mSignal |= SIGNAL_IDLE;
    if (mSignal & POST_BREAK) {
      break;
    }
    mCondition.wait(lck);
  }
  TRACE_N_FUNC(mName, "signal: 0x%03x", mSignal);
  TRACE_N_FUNC_EXIT(mName);
  return ret;
}

template <typename F, typename T>
MBOOL WaitHub::peakCondition(F func, T* data) {
  TRACE_N_FUNC_ENTER(mName);
  MBOOL result = func(data);
  TRACE_N_FUNC_EXIT(mName);
  return result;
}

template <typename T>
WaitQueue<T>::WaitQueue() : mHub(NULL) {}

template <typename T>
WaitQueue<T>::~WaitQueue() {}

template <typename T>
bool WaitQueue<T>::empty() const {
  std::lock_guard<std::mutex> lock(mMutex);
  return mQueue.empty();
}

template <typename T>
void WaitQueue<T>::enque(const T& val) {
  TRACE_FUNC_ENTER();
  // Release lock before trigger signal to avoid deadlock
  mMutex.lock();
  mQueue.push(val);
  mMutex.unlock();
  if (mHub) {
    mHub->signalEnque();
  }
  TRACE_FUNC_EXIT();
}

template <typename T>
bool WaitQueue<T>::deque(T* val) {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mMutex);
  bool ret = false;
  if (!mQueue.empty()) {
    *val = mQueue.front();
    mQueue.pop();
    ret = true;
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename T>
MBOOL WaitQueue<T>::isReady() const {
  std::lock_guard<std::mutex> lock(mMutex);
  return !mQueue.empty();
}

template <typename T>
MVOID WaitQueue<T>::setWaitHub(WaitHub* hub) {
  std::lock_guard<std::mutex> lock(mMutex);
  mHub = hub;
}

template <typename T>
size_t WaitQueue<T>::size() const {
  std::lock_guard<std::mutex> lock(mMutex);
  return mQueue.size();
}

template <typename T>
MVOID WaitQueue<T>::clear() {
  std::lock_guard<std::mutex> lock(mMutex);
  while (!mQueue.empty()) {
    mQueue.pop();
  }
}

template <typename T>
IWaitQueue::IDSet WaitQueue<T>::getIDSet() const {
  std::lock_guard<std::mutex> lock(mMutex);
  IWaitQueue::IDSet idSet;
  if (!mQueue.empty()) {
    idSet.insert(0);
  }
  return idSet;
}

template <typename T>
IWaitQueue::IndexSet WaitQueue<T>::getIndexSet() const {
  std::lock_guard<std::mutex> lock(mMutex);
  IWaitQueue::IndexSet index;
  if (!mQueue.empty()) {
    index.insert(IWaitQueue::Index());
  }
  return index;
}

template <typename T, class IndexConverter>
PriorityWaitQueue<T, IndexConverter>::PriorityWaitQueue()
    : mHub(NULL), mIndexSetValid(true) {}

template <typename T, class IndexConverter>
PriorityWaitQueue<T, IndexConverter>::~PriorityWaitQueue() {}

template <typename T, class IndexConverter>
bool PriorityWaitQueue<T, IndexConverter>::empty() const {
  std::lock_guard<std::mutex> lock(mMutex);
  return mDataSet.empty();
}

template <typename T, class IndexConverter>
void PriorityWaitQueue<T, IndexConverter>::enque(const T& val) {
  TRACE_FUNC_ENTER();
  // Release lock before trigger signal to avoid deadlock
  {
    std::lock_guard<std::mutex> lock(mMutex);
    mDataSet.insert(val);
    mIDSet.insert(IndexConverter::getID(val));
    if (mIndexSetValid) {
      mIndexSet.insert(IndexConverter()(val));
    }
  }
  if (mHub) {
    mHub->triggerSignal(WaitHub::SIGNAL_DATA);
  }
  TRACE_FUNC_EXIT();
}

template <typename T, class IndexConverter>
bool PriorityWaitQueue<T, IndexConverter>::deque(T* val) {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mMutex);
  bool ret = false;
  if (!mDataSet.empty()) {
    *val = (*mDataSet.begin());
    mDataSet.erase(mDataSet.begin());
    mIDSet.erase(IndexConverter::getID(*val));
    mIndexSetValid = false;
    ret = true;
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename T, class IndexConverter>
bool PriorityWaitQueue<T, IndexConverter>::deque(unsigned id, T* val) {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mMutex);
  bool ret = false;
  typename DataSet::iterator dataIt, dataEnd;
  typename IndexSet::iterator indexIt, indexEnd;
  for (dataIt = mDataSet.begin(), dataEnd = mDataSet.end(); dataIt != dataEnd;
       ++dataIt) {
    if (id == IndexConverter::getID(*dataIt)) {
      break;
    }
  }
  if (dataIt != mDataSet.end()) {
    *val = *dataIt;
    mDataSet.erase(dataIt);
    mIDSet.erase(IndexConverter::getID(*val));
    mIndexSetValid = false;
    ret = true;
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename T, class IndexConverter>
typename PriorityWaitQueue<T, IndexConverter>::CONTAINER_TYPE
PriorityWaitQueue<T, IndexConverter>::getContents() const {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mMutex);
  CONTAINER_TYPE contents;
  typename CONTAINER_TYPE::iterator it;
  typename DataSet::iterator dataIt, dataEnd;
  contents.resize(mDataSet.size());
  it = contents.begin();
  for (dataIt = mDataSet.begin(), dataEnd = mDataSet.end(); dataIt != dataEnd;
       ++dataIt, ++it) {
    *it = *dataIt;
  }
  TRACE_FUNC_EXIT();
  return contents;
}

template <typename T, class IndexConverter>
MBOOL PriorityWaitQueue<T, IndexConverter>::isReady() const {
  std::lock_guard<std::mutex> lock(mMutex);
  return !mDataSet.empty();
}

template <typename T, class IndexConverter>
MVOID PriorityWaitQueue<T, IndexConverter>::setWaitHub(WaitHub* hub) {
  std::lock_guard<std::mutex> lock(mMutex);
  mHub = hub;
}

template <typename T, class IndexConverter>
size_t PriorityWaitQueue<T, IndexConverter>::size() const {
  std::lock_guard<std::mutex> lock(mMutex);
  return mDataSet.size();
}

template <typename T, class IndexConverter>
MVOID PriorityWaitQueue<T, IndexConverter>::clear() {
  std::lock_guard<std::mutex> lock(mMutex);
  mDataSet.clear();
  mIDSet.clear();
}

template <typename T, class IndexConverter>
IWaitQueue::IDSet PriorityWaitQueue<T, IndexConverter>::getIDSet() const {
  std::lock_guard<std::mutex> lock(mMutex);
  return mIDSet;
}

template <typename T, class IndexConverter>
IWaitQueue::IndexSet PriorityWaitQueue<T, IndexConverter>::getIndexSet() const {
  std::lock_guard<std::mutex> lock(mMutex);
  if (!mIndexSetValid) {
    mIndexSet.clear();
    for (typename DataSet::const_iterator it = mDataSet.begin(),
                                          end = mDataSet.end();
         it != end; ++it) {
      mIndexSet.insert(IndexConverter()(*it));
    }
    mIndexSetValid = true;
  }
  return mIndexSet;
}

template <typename T, class IndexConverter>
bool PriorityWaitQueue<T, IndexConverter>::DataLess::operator()(
    const T& lhs, const T& rhs) const {
  Index left = IndexConverter()(lhs);
  Index right = IndexConverter()(rhs);
  return IWaitQueue::Index::Less()(left, right);
}

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam

#include "PipeLogHeaderEnd.h"
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_WAITQUEUE_H_
