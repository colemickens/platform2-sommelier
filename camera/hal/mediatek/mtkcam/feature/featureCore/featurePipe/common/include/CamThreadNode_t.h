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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMTHREADNODE_T_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMTHREADNODE_T_H_

#include <memory>
#include <mutex>
#include <queue>
#include "CamNode.h"
#include "CamThread.h"
#include "SyncUtil.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

template <typename Handler_T>
class CamThreadNode : public CamNode<Handler_T>, public CamThread {
 public:
  explicit CamThreadNode(const char* name);
  CamThreadNode(const char* name, int policy, int priority);
  virtual ~CamThreadNode();

 public:  // CamNode members
  virtual const char* getName() const;
  MVOID flush(const std::shared_ptr<NotifyCB>& cb);
  MVOID registerSyncCB(const std::shared_ptr<CountDownLatch>& cb);

 public:  // CamNode members for child class
  virtual MBOOL onInit() = 0;
  virtual MBOOL onUninit() = 0;

 public:  // CamThread members for child class
  virtual MBOOL onThreadLoop() = 0;
  virtual MBOOL onThreadStart() = 0;
  virtual MBOOL onThreadStop() = 0;
  virtual MVOID onFlush();

 private:
  virtual MBOOL onStart();
  virtual MBOOL onStop();

 private:
  std::mutex mMutex;

 private:
  class FlushWrapper : virtual public NotifyCB {
   public:
    FlushWrapper(std::shared_ptr<CamThreadNode<Handler_T>> parent,
                 const std::shared_ptr<NotifyCB>& cb);
    virtual ~FlushWrapper();
    MBOOL onNotify();

   private:
    std::shared_ptr<CamThreadNode<Handler_T>> mParent;
    std::shared_ptr<NotifyCB> mCB;
  };

  class SyncCounterCB : virtual public StatusCB, virtual public NotifyCB {
   public:
    SyncCounterCB(const char* name, const std::shared_ptr<CountDownLatch>& cb);

    // from CamThread::onSyncCB
    MBOOL onUpdate(MINT32 isSync);
    MINT32 getStatus();
    // from WaitHub::onEnque
    MBOOL onNotify();

   private:
    const char* mName;
    std::shared_ptr<CountDownLatch> mCB;
    std::mutex mMutex;
    MBOOL mIsSync;
  };
};

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMTHREADNODE_T_H_
