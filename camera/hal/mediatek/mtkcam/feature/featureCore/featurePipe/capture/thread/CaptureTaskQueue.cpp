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

#include <capture/thread/CaptureTaskQueue.h>

#define PIPE_CLASS_TAG "Task"
#define PIPE_TRACE TRACE_CAPTURE_FEATURE_NODE

#include <common/include/PipeLog.h>
#include <utility>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

CaptureTaskQueue::CaptureTaskQueue(MUINT8 count)
    : mStop(MFALSE), mThreadCount(count) {}

CaptureTaskQueue::~CaptureTaskQueue() {
  mStop = MTRUE;
  for (std::thread& t : mThreads) {
    t.join();
  }
  mThreads.clear();
}

MVOID CaptureTaskQueue::addTask(const std::function<void()>& task) {
  std::unique_lock<std::mutex> lock(mTaskLock);

  if (mThreadCount > mThreads.size()) {
    addThread();
  }

  mTasks.push_back(std::move(task));

  mTaskCond.notify_one();
}

MVOID CaptureTaskQueue::addThread() {
  std::thread t([this]() {
    while (!mStop) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(mTaskLock);
        if (mTasks.empty()) {
          mTaskCond.wait_for(lock, std::chrono::duration<int, std::milli>(5));
          continue;
        }
        task = std::move(mTasks.front());
        mTasks.pop_front();
      }
      task();
    }
  });
  mThreads.push_back(std::move(t));
}

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
