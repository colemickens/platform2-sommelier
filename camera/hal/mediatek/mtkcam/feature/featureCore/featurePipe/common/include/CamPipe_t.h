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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMPIPE_T_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMPIPE_T_H_

#include <memory>
#include <mutex>
#include "CamGraph.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

#define WATCHDOG_TIMEOUT (3000)  // ms

template <typename Node_T>
class CamPipe {
 public:
  typedef typename Node_T::DataID_T DataID_T;
  typedef typename Node_T::Handler_T Handler_T;

 public:
  explicit CamPipe(const char* name);
  virtual ~CamPipe();
  MBOOL init();
  MBOOL uninit();
  MBOOL setRootNode(std::shared_ptr<Node_T> root);
  MBOOL connectData(DataID_T id,
                    std::shared_ptr<Node_T> srcNode,
                    std::shared_ptr<Node_T> dstNode,
                    ConnectionType type = CONNECTION_DIRECT);
  MBOOL connectData(DataID_T srcID,
                    DataID_T dstID,
                    std::shared_ptr<Node_T> srcNode,
                    std::shared_ptr<Node_T> dstNode,
                    ConnectionType type = CONNECTION_DIRECT);
  MBOOL connectData(DataID_T id,
                    std::shared_ptr<Node_T>* srcNode,
                    std::shared_ptr<Handler_T> handler,
                    ConnectionType type = CONNECTION_DIRECT);
  MBOOL connectData(DataID_T srcID,
                    DataID_T dstID,
                    std::shared_ptr<Node_T>* srcNode,
                    std::shared_ptr<Handler_T> handler,
                    ConnectionType type = CONNECTION_DIRECT);
  MBOOL disconnect();
  template <typename BUFFER_T>
  MBOOL enque(DataID_T id, BUFFER_T* buffer);
  MVOID flush(unsigned watchdog_ms = WATCHDOG_TIMEOUT);

  // sync will block until all thread in graph are idle
  // use with caution !!
  MVOID sync(unsigned watchdog_ms = WATCHDOG_TIMEOUT);
  MVOID setFlushOnStop(MBOOL flushOnStop);

 protected:
  MVOID dispose();

 protected:
  virtual MBOOL onInit() = 0;
  virtual MVOID onUninit() = 0;

 private:
  enum Stage { STAGE_IDLE, STAGE_PREPARE, STAGE_READY, STAGE_DISPOSE };
  Stage mStage;
  std::mutex mStageLock;

 protected:
  // TODO(mtk): move to private when CamNode does not need
  //            direct access to CamGraph
  std::shared_ptr<CamGraph<Node_T> > mCamGraph;
};

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMPIPE_T_H_
