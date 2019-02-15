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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_THREAD_CAPTURETASKQUEUE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_THREAD_CAPTURETASKQUEUE_H_

#include "../CaptureFeature_Common.h"

#include <future>
#include <thread>
#include <deque>
#include <vector>
#include <utility>
#include <chrono>
#include <functional>
#include <type_traits>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

class CaptureTaskQueue {
 public:
  explicit CaptureTaskQueue(MUINT8 count = 1);

  virtual ~CaptureTaskQueue();

  virtual MVOID addTask(const std::function<void()>& task);

 private:
  virtual MVOID addThread();

  MBOOL mStop;
  MUINT8 mThreadCount;
  std::deque<std::function<void()>> mTasks;
  std::mutex mTaskLock;
  std::condition_variable mTaskCond;
  std::vector<std::thread> mThreads;
};

/*******************************************************************************
 * Namespace end.
 ****************************************************************************/
};      // namespace NSCapture
};      // namespace NSFeaturePipe
};      // namespace NSCamFeature
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_THREAD_CAPTURETASKQUEUE_H_
