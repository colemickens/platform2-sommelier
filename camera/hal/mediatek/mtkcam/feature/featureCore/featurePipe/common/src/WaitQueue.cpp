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

#include <algorithm>
#include <functional>

#include "../include/DebugControl.h"
#define PIPE_TRACE TRACE_WAIT_HUB
#define PIPE_CLASS_TAG "WaitHub"
#include "../include/PipeLog.h"
#include "../include/WaitQueue.h"

#include <memory>
#include <vector>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

static bool isQueueReady(IWaitQueue* queue) {
  return queue->isReady();
}

#if __cplusplus <= 199711L
template <class InputIterator, class UnaryPredicate>
bool any_of(InputIterator first, InputIterator last, UnaryPredicate pred) {
  while (first != last) {
    if (pred(*first)) {
      return true;
    }
    ++first;
  }
  return false;
}
#endif

IWaitQueue::Index::Index() : mID(0), mPriority(0) {}

IWaitQueue::Index::Index(unsigned id, unsigned priority)
    : mID(id), mPriority(priority) {}

bool IWaitQueue::Index::Less::operator()(const Index& lhs,
                                         const Index& rhs) const {
  return (lhs.mPriority < rhs.mPriority) ||
         (lhs.mPriority == rhs.mPriority && lhs.mID < rhs.mID);
}

WaitHub::WaitHub(const char* name) : mSignal(0) {
  snprintf(mName, sizeof(mName), "%s", (name != NULL) ? name : "NA");
}

WaitHub::~WaitHub() {}

MVOID WaitHub::addWaitQueue(IWaitQueue* queue) {
  TRACE_N_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> lock(mMutex);
  if (queue) {
    queue->setWaitHub(this);
    mQueues.push_back(queue);
    if (queue->isReady()) {
      mSignal |= SIGNAL_DATA;
      mCondition.notify_all();
    }
  }
  TRACE_N_FUNC_EXIT(mName);
}

MVOID WaitHub::flushQueues() {
  TRACE_N_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> lock(mMutex);
  std::vector<IWaitQueue*>::iterator it, end;
  for (it = mQueues.begin(), end = mQueues.end(); it != end; ++it) {
    (*it)->clear();
  }
  TRACE_N_FUNC_EXIT(mName);
}

MVOID WaitHub::signalEnque() {
  TRACE_N_FUNC_ENTER(mName);
  mMutex.lock();
  if (mEnqueCB != NULL) {
    std::shared_ptr<NotifyCB> cb = mEnqueCB;
    mMutex.unlock();
    cb->onNotify();
    mMutex.lock();
  }
  mSignal |= WaitHub::SIGNAL_DATA;
  mSignal &= ~(WaitHub::SIGNAL_IDLE);
  if (mSignal & WaitHub::SIGNAL_SYNC_CB) {
    mSignal |= WaitHub::SIGNAL_NEED_SYNC_BREAK;
  }
  mCondition.notify_all();
  mMutex.unlock();
  TRACE_N_FUNC_EXIT(mName);
}

MVOID WaitHub::registerEnqueCB(const std::shared_ptr<NotifyCB>& cb) {
  TRACE_N_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> lock(mMutex);
  mEnqueCB = cb;
  TRACE_N_FUNC_EXIT(mName);
}

MVOID WaitHub::triggerSignal(MUINT32 signal) {
  TRACE_N_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> lock(mMutex);
  TRACE_N_FUNC(mName, "signal: 0x%03x + 0x%03x => 0x%03x", mSignal, signal,
               mSignal | signal);
  mSignal |= signal;
  mCondition.notify_all();
  TRACE_N_FUNC_EXIT(mName);
}

MVOID WaitHub::resetSignal(MUINT32 signal) {
  TRACE_N_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> lock(mMutex);
  TRACE_N_FUNC(mName, "signal: 0x%03x - 0x%03x => 0x%03x", mSignal, signal,
               mSignal & (~signal));
  mSignal &= ~signal;
  TRACE_N_FUNC_EXIT(mName);
}

MVOID WaitHub::resetSignal() {
  TRACE_N_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> lock(mMutex);
  mSignal = 0;
  TRACE_N_FUNC_EXIT(mName);
}

MVOID WaitHub::shiftSignal(MUINT32 src, MUINT32 dst) {
  TRACE_N_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> lock(mMutex);
  if (mSignal & src) {
    mSignal &= ~src;
    mSignal |= dst;
  } else {
    mSignal &= ~dst;
  }
  TRACE_N_FUNC_EXIT(mName);
}

MBOOL WaitHub::waitAllQueue() {
  return waitCondition(std::mem_fn(&WaitHub::isAllQueueReady), this);
}

MBOOL WaitHub::waitAnyQueue() {
  return waitCondition(std::mem_fn(&WaitHub::isAnyQueueReady), this);
}

MBOOL WaitHub::waitAllQueueSync(MUINT32* id) {
  return waitCondition(
      std::bind(&WaitHub::isAllQueueReadySync, this, std::placeholders::_1),
      id);
}

MBOOL WaitHub::peakAllQueue() {
  return peakCondition(std::mem_fn(&WaitHub::isAllQueueReady), this);
}

MBOOL WaitHub::peakAnyQueue() {
  return peakCondition(std::mem_fn(&WaitHub::isAnyQueueReady), this);
}

MUINT32 WaitHub::waitSignal() {
  TRACE_N_FUNC_ENTER(mName);
  std::unique_lock<std::mutex> lck(mMutex);
  const MUINT32 TRIGGER =
      SIGNAL_STOP | SIGNAL_CB | SIGNAL_DATA | SIGNAL_IDLE_CB | SIGNAL_SYNC_CB |
      SIGNAL_NEED_SYNC_BREAK | SIGNAL_DRY_RUN | SIGNAL_DRY_RUN_ONCE;

  if (!(mSignal & WaitHub::SIGNAL_DATA) &&
      any_of(mQueues.begin(), mQueues.end(), isQueueReady)) {
    mSignal |= WaitHub::SIGNAL_DATA;
  }

  while (!(mSignal & TRIGGER)) {
    mCondition.wait(lck);
    if (any_of(mQueues.begin(), mQueues.end(), isQueueReady)) {
      mSignal |= WaitHub::SIGNAL_DATA;
    }
  }
  TRACE_N_FUNC(mName, "signal: 0x%03x", mSignal);
  TRACE_N_FUNC_EXIT(mName);
  return mSignal;
}

MBOOL WaitHub::isAllQueueEmpty() {
  TRACE_N_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> lock(mMutex);
  MBOOL ret = !any_of(mQueues.begin(), mQueues.end(), isQueueReady);
  TRACE_N_FUNC_EXIT(mName);
  return ret;
}

MVOID WaitHub::dumpWaitQueueInfo() {
  TRACE_N_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> lock(mMutex);
  unsigned n = mQueues.size();
  unsigned size = 0;
  for (unsigned i = 0; i < n; ++i) {
    size = mQueues[i]->size();
    MY_LOGW("%s queue(%u/%u) size(%u)", mName, i, n, size);
  }
  MY_LOGW("%s mSignal(0x%08X)", mName, mSignal);
  TRACE_N_FUNC_EXIT(mName);
  return;
}

MBOOL WaitHub::isAllQueueReady() {
  TRACE_N_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> lock(mMutex);
  MBOOL ret = MFALSE;
  unsigned ready;
  ready = std::count_if(mQueues.begin(), mQueues.end(), isQueueReady);
  ret = ready && (ready == mQueues.size());
  TRACE_N_FUNC_EXIT(mName);
  return ret;
}

MBOOL WaitHub::isAnyQueueReady() {
  TRACE_N_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> lock(mMutex);
  MBOOL ret = any_of(mQueues.begin(), mQueues.end(), isQueueReady);
  TRACE_N_FUNC_EXIT(mName);
  return ret;
}

MBOOL WaitHub::isAllQueueReadySync(MUINT32* id) {
  TRACE_N_FUNC_ENTER(mName);
  std::lock_guard<std::mutex> lock(mMutex);
  MBOOL found = MFALSE;
  IWaitQueue::IndexSet indexSet;
  std::vector<IWaitQueue::IDSet> idSets;
  unsigned size = mQueues.size();

  if (!id) {
    MY_LOGE("Invalid id result holder");
  } else if (size > 0) {
    idSets.resize(size);
    for (unsigned i = 0; i < size; ++i) {
      idSets[i] = mQueues[i]->getIDSet();
    }

    indexSet = mQueues[0]->getIndexSet();
    IWaitQueue::IndexSet::iterator it, end;
    for (it = indexSet.begin(), end = indexSet.end(); it != end; ++it) {
      found = MTRUE;
      for (unsigned i = 1; i < size; ++i) {
        if (idSets[i].count(it->mID) <= 0) {
          found = MFALSE;
          break;
        }
      }
      if (found) {
        *id = it->mID;
        break;
      }
    }
  }
  TRACE_N_FUNC_EXIT(mName);
  return found;
}

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam
