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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMTHREAD_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMTHREAD_H_

#include "MtkHeader.h"

#include <deque>
#include <memory>
#include <mutex>
#include <queue>
#include <sched.h>
#include <thread>
#include <vector>

#include "WaitQueue.h"

#define DEFAULT_CAMTHREAD_PRIORITY (ANDROID_PRIORITY_FOREGROUND)

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class VISIBILITY_PUBLIC CamThread
    : public std::enable_shared_from_this<CamThread>,
      private WaitHub {
 public:
  explicit CamThread(const char* name);
  CamThread(const char* name, MUINT32 policy, MUINT32 priority);
  virtual ~CamThread();
  virtual const char* getName() const = 0;

 public:
  MBOOL startThread();
  MBOOL stopThread();

  MVOID triggerDryRun();
  MBOOL insertCB(const std::shared_ptr<NotifyCB>& cb);
  MBOOL insertIdleCB(const std::shared_ptr<NotifyCB>& cb);
  MBOOL registerStatusCB(const std::shared_ptr<StatusCB>& cb);
  MBOOL waitIdle();

  MVOID incExtThreadDependency();
  MVOID decExtThreadDependency();
  MINT32 getExtThreadDependency();

  MVOID dumpCamThreadInfo();

 public:
  virtual MBOOL onThreadLoop() = 0;
  virtual MBOOL onThreadStart() = 0;
  virtual MBOOL onThreadStop() = 0;

 public:  // WaitHub member
  using WaitHub::addWaitQueue;
  using WaitHub::dumpWaitQueueInfo;
  using WaitHub::flushQueues;
  using WaitHub::peakAllQueue;
  using WaitHub::peakAnyQueue;
  using WaitHub::peakCondition;
  using WaitHub::registerEnqueCB;
  using WaitHub::signalEnque;
  using WaitHub::waitAllQueue;
  using WaitHub::waitAllQueueSync;
  using WaitHub::waitAnyQueue;
  using WaitHub::waitCondition;

 private:
  MBOOL tryProcessStop(MUINT32 signal);
  MBOOL tryProcessCB(MUINT32 signal);
  MBOOL tryProcessIdleCB(MUINT32 signal);
  MBOOL tryProcessStatusCB(MUINT32 signal);

 private:
  class CamThreadHandle {
   public:
    explicit CamThreadHandle(std::shared_ptr<CamThread> parent);
    virtual ~CamThreadHandle();
    MERROR run();
    MERROR join();
    bool threadLoop();
    bool _threadLoop();

   private:
    std::shared_ptr<CamThread> mParent;
    std::thread mThread;
    MBOOL mIsFirst;
  };

 private:
  std::mutex mThreadMutex;
  std::shared_ptr<CamThreadHandle> mHandle;
  std::deque<std::shared_ptr<NotifyCB> > mCB;
  std::deque<std::shared_ptr<NotifyCB> > mIdleCB;
  std::shared_ptr<StatusCB> mStatusCB;
  MINT32 mExtThreadDependency;
};  // class CamThread

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_CAMTHREAD_H_
