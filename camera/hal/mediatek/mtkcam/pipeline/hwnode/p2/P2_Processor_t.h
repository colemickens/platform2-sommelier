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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_PROCESSOR_T_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_PROCESSOR_T_H_

#include <memory>
#include <queue>
#include <string>
#include <thread>
#include "P2_Header.h"

namespace P2 {

template <typename Init_T, typename Config_T, typename Enque_T>
class Processor : public std::enable_shared_from_this<
                      Processor<Init_T, Config_T, Enque_T>> {
 public:
  explicit Processor(const std::string& name);
  Processor(const std::string& name, MINT32 policy, MINT32 priority);
  virtual ~Processor();
  const char* getName() const;
  MBOOL setEnable(MBOOL enable);
  MBOOL isEnabled() const;
  MBOOL setNeedThread(MBOOL isThreadNeed);
  MVOID setIdleWaitMS(MUINT32 msec);
  MBOOL init(const Init_T& param);
  MVOID uninit();
  MBOOL config(const Config_T& param);
  MBOOL enque(const Enque_T& param);
  MVOID flush();
  MVOID notifyFlush();
  MVOID waitFlush();

 protected:
  virtual MBOOL onInit(const Init_T& param) = 0;
  virtual MVOID onUninit() = 0;
  virtual MVOID onThreadStart() = 0;
  virtual MVOID onThreadStop() = 0;
  virtual MBOOL onConfig(const Config_T& param) = 0;
  virtual MBOOL onEnque(const Enque_T& param) = 0;
  virtual MVOID onNotifyFlush() = 0;
  virtual MVOID onWaitFlush() = 0;
  virtual MVOID onIdle() {}  // It will NOT be called if mNeedThread is false.

 private:
  class ProcessThread {
   public:
    ProcessThread(std::shared_ptr<Processor> parent, MBOOL needThread);
    virtual ~ProcessThread();
    status_t readyToRun();
    status_t run();
    status_t join(void);
    bool threadLoop();
    bool _threadLoop();

    MVOID enque(const Enque_T& param);
    MVOID flush();
    MVOID stop();

   private:
    enum WaitResult { WAIT_OK, WAIT_ERROR, WAIT_IDLE, WAIT_EXIT };
    WaitResult waitEnqueParam(Enque_T* param);

   private:
    std::shared_ptr<Processor> mParent;
    const std::string mName;
    int64_t mIdleWaitTime;
    std::mutex mMutex;
    std::condition_variable mCondition;
    MBOOL mStop;
    MBOOL mIdle;
    MBOOL mNeedThread;
    std::thread mThread;
    std::queue<Enque_T> mQueue;
  };

 private:
  std::mutex mThreadMutex;
  std::shared_ptr<ProcessThread> mThread;
  const std::string mName;
  MBOOL mEnable;
  MUINT32 mIdleWaitMS;
  MBOOL mNeedThread;
};

}  // namespace P2

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_PROCESSOR_T_H_
