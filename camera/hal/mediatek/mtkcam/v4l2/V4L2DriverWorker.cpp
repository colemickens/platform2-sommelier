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

#include <mtkcam/v4l2/V4L2DriverWorker.h>
#include <utility>  // std::move

namespace v4l2 {

V4L2DriverWorker::V4L2DriverWorker() : m_workerThreadAlive(false) {}

V4L2DriverWorker::~V4L2DriverWorker() {
  stop();  // stop thread
}

int V4L2DriverWorker::start() {
  auto task = [this]() {
    while (m_workerThreadAlive.load(std::memory_order_relaxed)) {
      job();
    }
  };

  std::lock_guard<std::mutex> lk(m_workerMutex);
  bool bAlive = m_workerThreadAlive.load(std::memory_order_relaxed);
  if (bAlive) {
    // still alive, no need to start
    return 0;
  }

  // marked as alive, and start thread
  m_workerThreadAlive.store(true, std::memory_order_relaxed);

  std::atomic_thread_fence(std::memory_order_acquire);  // sync data

  // start a thread
  m_workerThread = std::thread(std::move(task));

  std::atomic_thread_fence(std::memory_order_release);  // sync data

  return 0;
}

int V4L2DriverWorker::stop() {
  std::lock_guard<std::mutex> lk(m_workerMutex);
  std::atomic_thread_fence(std::memory_order_acquire);

  // marked as not alive
  m_workerThreadAlive.store(false, std::memory_order_relaxed);

  // if thread has began, wait it
  if (m_workerThread.joinable())
    m_workerThread.join();

  std::atomic_thread_fence(std::memory_order_release);
  return 0;
}

int V4L2DriverWorker::requestExit() {
  std::lock_guard<std::mutex> lk(m_workerMutex);
  m_workerThreadAlive.store(false, std::memory_order_relaxed);
  return 0;
}

};  // namespace v4l2
