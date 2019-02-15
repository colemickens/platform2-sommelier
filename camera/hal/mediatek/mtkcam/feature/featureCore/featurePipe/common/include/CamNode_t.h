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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMNODE_T_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMNODE_T_H_

#include "MtkHeader.h"
#include "SeqUtil.h"

#include <mutex>

#include <map>
#include <memory>
#include <set>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

enum ConnectionType { CONNECTION_DIRECT, CONNECTION_SEQUENTIAL };

template <typename Handler_T>
class CamNode {
 public:
  typedef typename Handler_T::DataID DataID_T;

 public:
  explicit CamNode(const char* name);
  virtual ~CamNode();

 public:
  virtual const char* getName() const;
  MINT32 getPropValue();
  MINT32 getPropValue(DataID_T id);

 public:
  MBOOL connectData(DataID_T src,
                    DataID_T dst,
                    std::shared_ptr<Handler_T> handler,
                    ConnectionType type);
  MBOOL registerInputDataID(DataID_T id);
  MBOOL disconnect();

 public:  // control flow related
  MVOID setDataFlow(MBOOL allow);

 protected:  // internal buffer flow related
  template <typename BUFFER_T>
  MBOOL handleData(DataID_T id, const BUFFER_T& buffer);
  template <typename BUFFER_T>
  MBOOL handleData(DataID_T id, BUFFER_T* buffer);

 public:  // child class related
  virtual MBOOL init();
  virtual MBOOL uninit();
  virtual MBOOL start();
  virtual MBOOL stop();

 public:
  virtual MBOOL onInit() = 0;
  virtual MBOOL onUninit() = 0;
  virtual MBOOL onStart() = 0;
  virtual MBOOL onStop() = 0;

 protected:
  MBOOL isRunning();

 private:
  MVOID updatePropValues();

 private:
  enum Stage { STAGE_IDLE, STAGE_READY, STAGE_RUNNING };
  mutable std::mutex mNodeLock;
  char mName[128];
  Stage mStage;
  MBOOL mAllowDataFlow;
  MINT32 mPropValue;
  std::map<DataID_T, MINT32> mDataPropValues;

  class HandlerEntry {
   public:
    DataID_T mSrcID;
    DataID_T mDstID;
    std::shared_ptr<Handler_T> mHandler;
    ConnectionType mType;
    HandlerEntry()
        : mSrcID((DataID_T)0),
          mDstID((DataID_T)0),
          mHandler(NULL),
          mType(CONNECTION_DIRECT) {}
    HandlerEntry(DataID_T src,
                 DataID_T dst,
                 std::shared_ptr<Handler_T> handler,
                 ConnectionType type)
        : mSrcID(src), mDstID(dst), mHandler(handler), mType(type) {}
  };
  typedef std::map<DataID_T, HandlerEntry> HANDLER_MAP;
  typedef typename HANDLER_MAP::iterator HANDLER_MAP_ITERATOR;
  HANDLER_MAP mHandlerMap;
  typedef std::set<DataID_T> SOURCE_SET;
  SOURCE_SET mSourceSet;
  SequentialHandler<Handler_T> mSeqHandler;
};

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMNODE_T_H_
