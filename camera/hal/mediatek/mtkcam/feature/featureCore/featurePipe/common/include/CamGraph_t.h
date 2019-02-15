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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMGRAPH_T_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMGRAPH_T_H_

#include "CamNode_t.h"
#include <memory>
#include <mutex>
#include <set>
#include <vector>
#include "SyncUtil.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

template <typename Node_T>
class CamGraph : public std::enable_shared_from_this<CamGraph<Node_T>> {
 private:
  typedef typename Node_T::Handler_T Handler_T;
  typedef typename Node_T::Handler_T::DataID DataID_T;

 public:
  CamGraph();
  ~CamGraph();

 public:  // cam related
  MBOOL setName(const char* name);
  const char* getName() const;

 public:  // node related
  MBOOL setRootNode(std::shared_ptr<Node_T> rootNode);
  MBOOL connectData(DataID_T srcID,
                    DataID_T dstID,
                    std::shared_ptr<Node_T> srcNode,
                    std::shared_ptr<Node_T> dstNode,
                    ConnectionType type);
  MBOOL connectData(DataID_T srcID,
                    DataID_T dstID,
                    std::shared_ptr<Node_T> const& srcNode,
                    std::shared_ptr<Handler_T> handler,
                    ConnectionType type);
  MBOOL disconnect();

 public:  // flow control
  MBOOL init();
  MBOOL uninit();
  MBOOL start();
  MBOOL stop();

  template <typename BUFFER_T>
  MBOOL enque(DataID_T id, BUFFER_T* buffer);
  template <typename MSG_T>
  MBOOL broadcast(DataID_T id, MSG_T* msg);

  MVOID setDataFlow(MBOOL allow);
  MVOID setFlushOnStop(MBOOL flushOnStop);
  MVOID flush(unsigned watchdog_ms);
  MVOID sync(unsigned watchdog_ms);

 private:
  class Watchdog : public NotifyCB {
   public:
    explicit Watchdog(std::shared_ptr<CamGraph<Node_T>> parent);
    virtual ~Watchdog();
    virtual MBOOL onNotify();

   private:
    std::shared_ptr<CamGraph<Node_T>> mParent;
  };
  friend class Watchdog;

  MVOID setFlow(MBOOL flow);
  MVOID waitFlush(unsigned watchdog_ms = 0);
  MVOID waitSync(unsigned watchdog_ms = 0);
  MVOID onDumpStatus();

 private:
  enum Stage { STAGE_IDLE, STAGE_READY, STAGE_RUNNING };
  typedef typename std::set<std::shared_ptr<Node_T>> NODE_SET;
  typedef
      typename std::set<std::shared_ptr<Node_T>>::iterator NODE_SET_ITERATOR;
  mutable std::mutex mMutex;
  /*const*/ char* msName;
  MUINT32 mStage;
  std::shared_ptr<Node_T> mRootNode;
  NODE_SET mNodes;
  MBOOL mAllowDataFlow;
  MBOOL mFlushOnStop;
};  // class CamGraph

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMGRAPH_T_H_
