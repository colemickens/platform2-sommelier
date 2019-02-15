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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMPIPE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMPIPE_H_

#include "PipeLogHeaderBegin.h"
#include "DebugControl.h"
#define PIPE_TRACE TRACE_CAM_PIPE
#define PIPE_CLASS_TAG "CamPipe"
#include "PipeLog.h"
#include "CamPipe_t.h"
#include <memory>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

template <typename Node_T>
CamPipe<Node_T>::CamPipe(const char* name) : mStage(STAGE_IDLE) {
  TRACE_FUNC_ENTER();
  mCamGraph = std::make_shared<CamGraph<Node_T> >();
  mCamGraph->setName(name);
  TRACE_FUNC_EXIT();
}

template <typename Node_T>
CamPipe<Node_T>::~CamPipe() {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mStageLock);
  if (mStage != STAGE_DISPOSE) {
    MY_LOGE("Error: CamPipe::dispose() not called before destroy");
  }
  TRACE_FUNC_EXIT();
}

template <typename Node_T>
MBOOL CamPipe<Node_T>::init() {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  MBOOL pipeInit, graphInit, graphStart;
  mStageLock.lock();

  pipeInit = graphInit = graphStart = MTRUE;
  if (mStage == STAGE_IDLE) {
    mStage = STAGE_PREPARE;
    mStageLock.unlock();
    pipeInit = this->onInit();
    mStageLock.lock();

    if (pipeInit) {
      if ((graphInit = mCamGraph->init())) {
        graphStart = mCamGraph->start();
      }
    }

    if (pipeInit && graphInit && graphStart) {
      mStage = STAGE_READY;
      ret = MTRUE;
    } else {
      if (graphInit) {
        mCamGraph->uninit();
      }
      mStage = STAGE_IDLE;
      if (pipeInit) {
        mStageLock.unlock();
        this->onUninit();
        mStageLock.lock();
      }
    }
  }

  TRACE_FUNC_EXIT();
  mStageLock.unlock();
  return ret;
}

template <typename Node_T>
MBOOL CamPipe<Node_T>::uninit() {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  mStageLock.lock();
  if (mStage == STAGE_READY) {
    mCamGraph->stop();
    mCamGraph->uninit();
    mStage = STAGE_IDLE;
    mStageLock.unlock();
    this->onUninit();
    mStageLock.lock();
    ret = MTRUE;
  }
  TRACE_FUNC_EXIT();
  mStageLock.unlock();
  return ret;
}

template <typename Node_T>
MBOOL CamPipe<Node_T>::setRootNode(std::shared_ptr<Node_T> root) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> lock(mStageLock);
  if (mStage == STAGE_IDLE || mStage == STAGE_PREPARE) {
    ret = mCamGraph->setRootNode(root);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Node_T>
MBOOL CamPipe<Node_T>::connectData(DataID_T id,
                                   std::shared_ptr<Node_T> srcNode,
                                   std::shared_ptr<Node_T> dstNode,
                                   ConnectionType type) {
  TRACE_FUNC_ENTER();
  MBOOL ret;
  ret = this->connectData(id, id, srcNode, dstNode, type);
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Node_T>
MBOOL CamPipe<Node_T>::connectData(DataID_T srcID,
                                   DataID_T dstID,
                                   std::shared_ptr<Node_T> srcNode,
                                   std::shared_ptr<Node_T> dstNode,
                                   ConnectionType type) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> lock(mStageLock);
  if (mStage == STAGE_IDLE || mStage == STAGE_PREPARE) {
    ret = mCamGraph->connectData(srcID, dstID, srcNode, dstNode, type);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Node_T>
MBOOL CamPipe<Node_T>::connectData(DataID_T id,
                                   std::shared_ptr<Node_T>* srcNode,
                                   std::shared_ptr<Handler_T> handler,
                                   ConnectionType type) {
  TRACE_FUNC_ENTER();
  MBOOL ret;
  ret = this->connectData(id, id, *srcNode, handler, type);
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Node_T>
MBOOL CamPipe<Node_T>::connectData(DataID_T srcID,
                                   DataID_T dstID,
                                   std::shared_ptr<Node_T>* srcNode,
                                   std::shared_ptr<Handler_T> handler,
                                   ConnectionType type) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> lock(mStageLock);
  if (mStage == STAGE_IDLE || mStage == STAGE_PREPARE) {
    ret = mCamGraph->connectData(srcID, dstID, *srcNode, handler, type);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Node_T>
MBOOL CamPipe<Node_T>::disconnect() {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> lock(mStageLock);
  if (mStage == STAGE_IDLE || mStage == STAGE_PREPARE) {
    ret = mCamGraph->disconnect();
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Node_T>
template <typename BUFFER_T>
MBOOL CamPipe<Node_T>::enque(DataID_T id, BUFFER_T* buffer) {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mStageLock);
  MBOOL ret = MFALSE;
  if (mStage == STAGE_READY) {
    ret = mCamGraph->enque(id, buffer);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Node_T>
MVOID CamPipe<Node_T>::flush(unsigned watchdog_ms) {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mStageLock);
  if (mStage == STAGE_READY) {
    mCamGraph->setDataFlow(MFALSE);
    mCamGraph->flush(watchdog_ms);
    mCamGraph->setDataFlow(MTRUE);
  }
  TRACE_FUNC_EXIT();
}

template <typename Node_T>
MVOID CamPipe<Node_T>::sync(unsigned watchdog_ms) {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mStageLock);
  if (mStage == STAGE_READY) {
    mCamGraph->sync(watchdog_ms);
  }
  TRACE_FUNC_EXIT();
}

template <typename Node_T>
MVOID CamPipe<Node_T>::setFlushOnStop(MBOOL flushOnStop) {
  TRACE_FUNC_ENTER();
  mCamGraph->setFlushOnStop(flushOnStop);
  TRACE_FUNC_EXIT();
}

template <typename Node_T>
MVOID CamPipe<Node_T>::dispose() {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mStageLock);
  if (mStage == STAGE_READY) {
    mCamGraph->stop();
    mCamGraph->uninit();
    this->onUninit();
  }
  mCamGraph->disconnect();
  mStage = STAGE_DISPOSE;
  TRACE_FUNC_EXIT();
}

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam

#include "PipeLogHeaderEnd.h"
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMPIPE_H_
