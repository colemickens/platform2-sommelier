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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_SYNCUTIL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_SYNCUTIL_H_

#include "MtkHeader.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class VISIBILITY_PUBLIC NotifyCB {
 public:
  NotifyCB();
  virtual ~NotifyCB();
  virtual MBOOL onNotify() = 0;
};

class VISIBILITY_PUBLIC StatusCB {
 public:
  StatusCB();
  virtual ~StatusCB();
  virtual MBOOL onUpdate(int status) = 0;
  virtual MINT32 getStatus() = 0;
};

class VISIBILITY_PUBLIC WaitNotifyCB : public NotifyCB {
 public:
  WaitNotifyCB();
  virtual ~WaitNotifyCB();
  virtual MBOOL onNotify();
  MBOOL wait();

 private:
  std::mutex mMutex;
  std::condition_variable mCondition;
  MBOOL mDone;
};

class VISIBILITY_PUBLIC BacktraceNotifyCB : public NotifyCB {
 public:
  BacktraceNotifyCB();
  virtual ~BacktraceNotifyCB();
  virtual MBOOL onNotify();
};

class VISIBILITY_PUBLIC TimeoutCB {
 public:
  explicit TimeoutCB(unsigned timeout = 0);
  virtual ~TimeoutCB();
  unsigned getTimeout();
  uint64_t getTimeoutNs();
  MBOOL insertCB(const std::shared_ptr<NotifyCB>& cb);
  MBOOL onTimeout();

 private:
  std::mutex mMutex;
  unsigned mTimeout;
  std::vector<std::shared_ptr<NotifyCB> > mCB;
};

class VISIBILITY_PUBLIC CountDownLatch {
 public:
  explicit CountDownLatch(unsigned total);
  virtual ~CountDownLatch();

  MBOOL registerTimeoutCB(const std::shared_ptr<TimeoutCB>& cb);
  void countDown();
  void countBackUp();
  void wait();

 private:
  std::condition_variable mCondition;
  std::mutex mMutex;
  int mTotal;
  int mDone;
  std::shared_ptr<TimeoutCB> mTimeoutCB;
};

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_SYNCUTIL_H_
