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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMNODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMNODE_H_

#include "PipeLogHeaderBegin.h"
#include "DebugControl.h"
#define PIPE_TRACE TRACE_CAM_NODE
#define PIPE_CLASS_TAG "CamNode"
#include "PipeLog.h"
#include "CamNode_t.h"
#include "CamGraph_t.h"
#include "DebugUtil.h"

#include <memory>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

#define CAM_NODE_START_SEQ (1)
#define CAM_NODE_PROP_PREFIX "vendor.debug"

/***************************************************************************
 *
 ****************************************************************************/
template <typename Handler_T>
CamNode<Handler_T>::CamNode(const char* name)
    : mStage(STAGE_IDLE),
      mAllowDataFlow(MTRUE),
      mPropValue(0),
      mSeqHandler(CAM_NODE_START_SEQ) {
  snprintf(mName, sizeof(mName), "%s", (name != NULL) ? name : "NA");
}

template <typename Handler_T>
CamNode<Handler_T>::~CamNode() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC("%s", getName());
  this->stop();
  this->uninit();
  this->disconnect();
  TRACE_FUNC_EXIT();
}

template <typename Handler_T>
const char* CamNode<Handler_T>::getName() const {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return mName;
}

template <typename Handler_T>
MINT32 CamNode<Handler_T>::getPropValue() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return mPropValue;
}

template <typename Handler_T>
MINT32 CamNode<Handler_T>::getPropValue(DataID_T id) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return mDataPropValues[id];
}

template <typename Handler_T>
MBOOL CamNode<Handler_T>::connectData(DataID_T src,
                                      DataID_T dst,
                                      std::shared_ptr<Handler_T> handler,
                                      ConnectionType type) {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> _l(mNodeLock);
  MBOOL ret = MFALSE;
  if (mStage == STAGE_IDLE) {
    mHandlerMap[src] = HandlerEntry(src, dst, handler, type);
    ret = MTRUE;
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Handler_T>
MBOOL CamNode<Handler_T>::registerInputDataID(DataID_T id) {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> _l(mNodeLock);
  MBOOL ret = MFALSE;
  if (mStage == STAGE_IDLE) {
    mSourceSet.insert(id);
    ret = MTRUE;
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Handler_T>
MBOOL CamNode<Handler_T>::disconnect() {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> _l(mNodeLock);
  if (mStage == STAGE_IDLE) {
    mHandlerMap.clear();
    mSourceSet.clear();
    mSeqHandler.clear();
    ret = MTRUE;
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Handler_T>
MVOID CamNode<Handler_T>::setDataFlow(MBOOL allow) {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> _l(mNodeLock);
  mAllowDataFlow = allow;
  TRACE_FUNC_EXIT();
}

template <typename Handler_T>
MBOOL CamNode<Handler_T>::init() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC("%s", getName());
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> _l(mNodeLock);
  if (mStage == STAGE_IDLE) {
    this->updatePropValues();
    if (!this->onInit()) {
      MY_LOGE("%s onInit() failed", this->getName());
    } else {
      mStage = STAGE_READY;
      ret = MTRUE;
    }
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Handler_T>
MBOOL CamNode<Handler_T>::uninit() {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> _l(mNodeLock);
  if (mStage == STAGE_READY) {
    this->onUninit();
    mStage = STAGE_IDLE;
    ret = MTRUE;
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Handler_T>
MBOOL CamNode<Handler_T>::start() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC("%s", getName());
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> _l(mNodeLock);
  if (mStage == STAGE_READY) {
    if (!this->onStart()) {
      MY_LOGE("%s onStart() failed", this->getName());
    } else {
      mStage = STAGE_RUNNING;
      ret = MTRUE;
    }
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Handler_T>
MBOOL CamNode<Handler_T>::stop() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC("%s", getName());
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> _l(mNodeLock);
  if (mStage == STAGE_RUNNING) {
    if (!this->onStop()) {
      MY_LOGE("%s onStop() failed", this->getName());
    }
    mStage = STAGE_READY;
    ret = MTRUE;
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Handler_T>
MBOOL CamNode<Handler_T>::isRunning() {
  TRACE_FUNC_ENTER();
  MBOOL ret;
  ret = (mStage == STAGE_RUNNING);
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Handler_T>
MVOID CamNode<Handler_T>::updatePropValues() {
  TRACE_FUNC_ENTER();
  const char* name;
  name = this->getName();
  mPropValue = getFormattedPropertyValue("%s.%s", CAM_NODE_PROP_PREFIX, name);
  mDataPropValues.clear();
  {
    HANDLER_MAP_ITERATOR it, end;
    for (it = mHandlerMap.begin(), end = mHandlerMap.end(); it != end; ++it) {
      DataID_T id = it->first;
      MINT32 prop = getFormattedPropertyValue("%s.%s.%s", CAM_NODE_PROP_PREFIX,
                                              name, Handler_T::ID2Name(id));
      mDataPropValues[id] = prop;
    }
  }
  {
    typename SOURCE_SET::iterator it, end;
    for (it = mSourceSet.begin(), end = mSourceSet.end(); it != end; ++it) {
      DataID_T id = *it;
      MINT32 prop = getFormattedPropertyValue("%s.%s.%s", CAM_NODE_PROP_PREFIX,
                                              name, Handler_T::ID2Name(id));
      mDataPropValues[id] = prop;
    }
  }

  TRACE_FUNC_EXIT();
}

template <typename Handler_T>
template <typename BUFFER_T>
MBOOL CamNode<Handler_T>::handleData(DataID_T id, const BUFFER_T& buffer) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC("%s", getName());
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> _l(mNodeLock);
  if (mStage == STAGE_RUNNING && mAllowDataFlow) {
    HANDLER_MAP_ITERATOR it = mHandlerMap.find(id);
    if (it != mHandlerMap.end()) {
      Handler_T* handler = it->second.mHandler.get();
      DataID_T dstID = it->second.mDstID;
      ConnectionType type = it->second.mType;
      if (handler) {
        if (type == CONNECTION_DIRECT) {
          ret = handler->onData(dstID, buffer);
        } else if (type == CONNECTION_SEQUENTIAL) {
          ret = mSeqHandler.onData(dstID, buffer, handler);
        } else {
          MY_LOGE("%s: (%d:%s) use unsupported connection type(%d)", getName(),
                  id, Handler_T::ID2Name(id), type);
        }
      }
    }

    if (!ret) {
      MY_LOGE("%s: handleData(%d:%s) failed", getName(), id,
              Handler_T::ID2Name(id));
    }
  }
  TRACE_FUNC_EXIT();
  return ret;
}

template <typename Handler_T>
template <typename BUFFER_T>
MBOOL CamNode<Handler_T>::handleData(DataID_T id, BUFFER_T* buffer) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC("%s", getName());
  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> _l(mNodeLock);
  if (mStage == STAGE_RUNNING && mAllowDataFlow) {
    HANDLER_MAP_ITERATOR it = mHandlerMap.find(id);
    if (it != mHandlerMap.end()) {
      Handler_T* handler = it->second.mHandler.get();
      DataID_T dstID = it->second.mDstID;
      ConnectionType type = it->second.mType;
      if (handler) {
        if (type == CONNECTION_DIRECT) {
          ret = handler->onData(dstID, *buffer);
        } else if (type == CONNECTION_SEQUENTIAL) {
          ret = mSeqHandler.onData(dstID, *buffer, handler);
        } else {
          MY_LOGE("%s: (%d:%s) use unsupported connection type(%d)", getName(),
                  id, Handler_T::ID2Name(id), type);
        }
      }
    }

    if (!ret) {
      MY_LOGE("%s: handleData(%d:%s) failed", getName(), id,
              Handler_T::ID2Name(id));
    }
  }
  TRACE_FUNC_EXIT();
  return ret;
}

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam

#include "PipeLogHeaderEnd.h"
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMNODE_H_
