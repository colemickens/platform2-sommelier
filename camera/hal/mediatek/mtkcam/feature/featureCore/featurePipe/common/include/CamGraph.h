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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMGRAPH_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMGRAPH_H_

#include "SyncUtil.h"

#include "PipeLogHeaderBegin.h"
#include "DebugControl.h"
#define PIPE_TRACE TRACE_CAM_GRAPH
#define PIPE_CLASS_TAG "CamGraph"
#include "PipeLog.h"
#include "CamGraph_t.h"

#include <memory>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

template <typename Node_T>
CamGraph<Node_T>::CamGraph(/*const char* name*/)
    : msName(nullptr),
      mStage(STAGE_IDLE),
      mRootNode(NULL),
      mAllowDataFlow(MTRUE),
      mFlushOnStop(MFALSE) {}

template <typename Node_T>
CamGraph<Node_T>::~CamGraph() {
  std::lock_guard<std::mutex> lock(mMutex);
  if (mStage != STAGE_IDLE || mNodes.size() != 0) {
    MY_LOGE("Error: CamGraph need to be disconnected before destroy");
  }
}

template <typename Node_T>
MBOOL CamGraph<Node_T>::setName(const char* name) {
  msName = const_cast<char*>(name);
  return MTRUE;
}

template <typename Node_T>
const char* CamGraph<Node_T>::getName() const {
  return msName;
}

template <typename Node_T>
MBOOL CamGraph<Node_T>::setRootNode(std::shared_ptr<Node_T> rootNode) {
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> lock(mMutex);
  if (mStage == STAGE_IDLE && rootNode) {
    mNodes.insert(rootNode);
    mRootNode = rootNode;
    ret = MTRUE;
  }

  return ret;
}

template <typename Node_T>
MBOOL CamGraph<Node_T>::connectData(DataID_T srcID,
                                    DataID_T dstID,
                                    std::shared_ptr<Node_T> srcNode,
                                    std::shared_ptr<Node_T> dstNode,
                                    ConnectionType type) {
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> lock(mMutex);
  if (mStage == STAGE_IDLE) {
    mNodes.insert(srcNode);
    mNodes.insert(dstNode);
    ret = srcNode->connectData(
        srcID, dstID, std::dynamic_pointer_cast<Handler_T>(dstNode), type);
    if (ret) {
      dstNode->registerInputDataID(dstID);
    }
  }
  return ret;
}

template <typename Node_T>
MBOOL CamGraph<Node_T>::connectData(DataID_T src,
                                    DataID_T dst,
                                    std::shared_ptr<Node_T> const& node,
                                    std::shared_ptr<Handler_T> handler,
                                    ConnectionType type) {
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> lock(mMutex);
  if (mStage == STAGE_IDLE) {
    mNodes.insert(node);
    ret = node->connectData(src, dst, handler, type);
  }

  return ret;
}

template <typename Node_T>
MBOOL CamGraph<Node_T>::disconnect() {
  TRACE_FUNC_ENTER();

  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> lock(mMutex);
  if (mStage == STAGE_IDLE) {
    NODE_SET_ITERATOR it, end;
    for (it = mNodes.begin(), end = mNodes.end(); it != end; ++it) {
      if (!(*it)->disconnect()) {
        MY_LOGE("disconnect failed");
      }
    }
    mRootNode = NULL;
    mNodes.clear();
    ret = MTRUE;
  }

  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Node_T>
MBOOL CamGraph<Node_T>::init() {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> lock(mMutex);
  NODE_SET_ITERATOR it, end;

  if (mStage != STAGE_IDLE) {
    MY_LOGE("invalid stage(%d)", mStage);
  } else if (!mRootNode) {
    MY_LOGE("mRootNode not set");
  }
  for (it = mNodes.begin(), end = mNodes.end(); it != end; ++it) {
    if (!(*it)->init()) {
      break;
    }
  }
  if (it != end) {
    NODE_SET_ITERATOR begin = mNodes.begin();
    while (it != begin) {
      --it;
      (*it)->uninit();
    }
  } else {
    mStage = STAGE_READY;
    ret = MTRUE;
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Node_T>
MBOOL CamGraph<Node_T>::uninit() {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> lock(mMutex);
  if (mStage == STAGE_READY) {
    typename NODE_SET::reverse_iterator it, end;
    for (it = mNodes.rbegin(), end = mNodes.rend(); it != end; ++it) {
      if (!(*it)->uninit()) {
        MY_LOGE("%s uninit failed", (*it)->getName());
      }
    }
    mStage = STAGE_IDLE;
    ret = MTRUE;
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Node_T>
MBOOL CamGraph<Node_T>::start() {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> lock(mMutex);

  if (mStage == STAGE_READY) {
    NODE_SET_ITERATOR begin, it, end;
    begin = mNodes.begin();
    end = mNodes.end();
    this->setFlow(mAllowDataFlow);
    for (it = begin; it != end; ++it) {
      if (!(*it)->start()) {
        break;
      }
    }
    if (it != end) {
      while (it != begin) {
        --it;
        (*it)->stop();
      }
    } else {
      mStage = STAGE_RUNNING;
      ret = MTRUE;
    }
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Node_T>
MBOOL CamGraph<Node_T>::stop() {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> lock(mMutex);
  if (mStage == STAGE_RUNNING) {
    NODE_SET_ITERATOR it, end;
    if (mFlushOnStop) {
      MY_LOGD("flush on stop");
      this->setFlow(MFALSE);
      this->waitFlush();
      this->waitSync();
    } else {
      MY_LOGD("sync on stop");
      this->waitSync();
      this->setFlow(MFALSE);
    }
    for (it = mNodes.begin(), end = mNodes.end(); it != end; ++it) {
      if (!(*it)->stop()) {
        MY_LOGE("%s stop failed", (*it)->getName());
      }
    }
    mStage = STAGE_READY;
    ret = MTRUE;
  }

  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Node_T>
template <typename BUFFER_T>
MBOOL CamGraph<Node_T>::enque(DataID_T id, BUFFER_T* buffer) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> lock(mMutex);
  if (mStage == STAGE_RUNNING) {
    ret = mRootNode->onData(id, *buffer);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Node_T>
template <typename MSG_T>
MBOOL CamGraph<Node_T>::broadcast(DataID_T id, MSG_T* msg) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> lock(mMutex);
  if (mStage == STAGE_RUNNING) {
    NODE_SET_ITERATOR it, end;
    for (it = mNodes.begin(), end = mNodes.end(); it != end; ++it) {
      (*it)->onData(id, *msg);
    }
    ret = MTRUE;
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Node_T>
MVOID CamGraph<Node_T>::setDataFlow(MBOOL allow) {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mMutex);
  mAllowDataFlow = allow;
  if (mStage == STAGE_RUNNING) {
    this->setFlow(mAllowDataFlow);
  }
  TRACE_FUNC_EXIT();
}

template <typename Node_T>
MVOID CamGraph<Node_T>::setFlushOnStop(MBOOL flushOnStop) {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mMutex);
  mFlushOnStop = flushOnStop;
  TRACE_FUNC_EXIT();
}

template <typename Node_T>
MVOID CamGraph<Node_T>::flush(unsigned watchdog_ms) {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mMutex);
  if (mStage == STAGE_RUNNING) {
    this->setFlow(MFALSE);
    this->waitFlush(watchdog_ms);
    this->waitSync(watchdog_ms);
    this->setFlow(mAllowDataFlow);
  }
  TRACE_FUNC_EXIT();
}

template <typename Node_T>
MVOID CamGraph<Node_T>::sync(unsigned watchdog_ms) {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mMutex);
  if (mStage == STAGE_RUNNING) {
    this->waitSync(watchdog_ms);
  }
  TRACE_FUNC_EXIT();
}

template <typename Node_T>
CamGraph<Node_T>::Watchdog::Watchdog(std::shared_ptr<CamGraph<Node_T>> parent)
    : mParent(parent) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

template <typename Node_T>
CamGraph<Node_T>::Watchdog::~Watchdog() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

template <typename Node_T>
MBOOL CamGraph<Node_T>::Watchdog::onNotify() {
  TRACE_FUNC_ENTER();
  if (mParent != NULL) {
    mParent->onDumpStatus();
  }
  TRACE_FUNC_EXIT();
  return MTRUE;
}

template <typename Node_T>
MVOID CamGraph<Node_T>::setFlow(MBOOL flow) {
  TRACE_FUNC_ENTER();
  NODE_SET_ITERATOR it, end;
  for (it = mNodes.begin(), end = mNodes.end(); it != end; ++it) {
    (*it)->setDataFlow(flow);
  }
  TRACE_FUNC_EXIT();
}

class CounterCBWrapper : public NotifyCB {
 public:
  explicit CounterCBWrapper(std::shared_ptr<CountDownLatch> counter)
      : mCounter(counter) {}

  MBOOL onNotify() {
    if (mCounter) {
      mCounter->countDown();
    }
    return MTRUE;
  }

 private:
  std::shared_ptr<CountDownLatch> mCounter;
};

template <typename Node_T>
MVOID CamGraph<Node_T>::waitFlush(unsigned watchdog_ms) {
  TRACE_FUNC_ENTER();
  NODE_SET_ITERATOR it, end;
  std::shared_ptr<CountDownLatch> counter =
      std::make_shared<CountDownLatch>(mNodes.size());
  if (watchdog_ms > 0) {
    std::shared_ptr<TimeoutCB> timeoutCB =
        std::make_shared<TimeoutCB>(watchdog_ms);
    timeoutCB->insertCB(std::make_shared<BacktraceNotifyCB>());
    timeoutCB->insertCB(std::make_shared<Watchdog>(this->shared_from_this()));
    counter->registerTimeoutCB(timeoutCB);
  }
  std::shared_ptr<NotifyCB> flushCB =
      std::make_shared<CounterCBWrapper>(counter);
  for (it = mNodes.begin(), end = mNodes.end(); it != end; ++it) {
    (*it)->flush(flushCB);
  }
  counter->wait();
  TRACE_FUNC_EXIT();
}

template <typename Node_T>
MVOID CamGraph<Node_T>::waitSync(unsigned watchdog_ms) {
  TRACE_FUNC_ENTER();
  NODE_SET_ITERATOR it, end;
  std::shared_ptr<CountDownLatch> counter =
      std::make_shared<CountDownLatch>(mNodes.size());
  if (watchdog_ms > 0) {
    std::shared_ptr<TimeoutCB> timeoutCB =
        std::make_shared<TimeoutCB>(watchdog_ms);
    timeoutCB->insertCB(std::make_shared<BacktraceNotifyCB>());
    timeoutCB->insertCB(std::make_shared<Watchdog>(this->shared_from_this()));
    counter->registerTimeoutCB(timeoutCB);
  }
  for (it = mNodes.begin(), end = mNodes.end(); it != end; ++it) {
    (*it)->registerSyncCB(counter);
  }
  counter->wait();
  for (it = mNodes.begin(), end = mNodes.end(); it != end; ++it) {
    (*it)->registerSyncCB(nullptr);
  }
  TRACE_FUNC_EXIT();
}

template <typename Node_T>
MVOID CamGraph<Node_T>::onDumpStatus() {
  TRACE_FUNC_ENTER();
  NODE_SET_ITERATOR it, end;
  for (it = mNodes.begin(), end = mNodes.end(); it != end; ++it) {
    (*it)->dumpWaitQueueInfo();
    (*it)->dumpCamThreadInfo();
  }
  TRACE_FUNC_EXIT();
}

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam

#include "PipeLogHeaderEnd.h"
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMGRAPH_H_
