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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_IPC_QUEUE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_IPC_QUEUE_H_

// STL
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace v4l2 {

// ---------------------------------------------------------------------------
// Server Queue
// ---------------------------------------------------------------------------
template <class PARAM_T, size_t QUEUE_LIMITED_SIZE = 10>
class IPCQueueServer {
  static_assert(QUEUE_LIMITED_SIZE > 0,
                "QUEUE_LIMITED_SIZE must be greater than 0");

 public:
  int ipc_dequeue(PARAM_T* pResult, int32_t timedout_ms = 3000) {
    std::unique_lock<std::mutex> lk(m_paramMutex);
    std::atomic_thread_fence(std::memory_order_acquire);

    while (m_paramQueue.empty()) {
      if (m_state.load(std::memory_order_relaxed) == 0) {
        return -EPERM;
      }

      auto cv_s =
          m_paramCond.wait_for(lk, std::chrono::milliseconds(timedout_ms));
      if (__builtin_expect(cv_s == std::cv_status::timeout, false)) {
        return -ETIMEDOUT;
      } else {
        std::atomic_thread_fence(std::memory_order_acquire);
      }
    }

    *pResult = m_paramQueue.at(0);
    m_paramQueue.erase(m_paramQueue.begin());
    std::atomic_thread_fence(std::memory_order_release);
    return 0;
  }

  void ipc_enqueue(PARAM_T p) {
    std::lock_guard<std::mutex> lk(m_paramMutex);
    auto f = thread_fence_guard();

    if (m_paramQueue.size() >= QUEUE_LIMITED_SIZE) {
      m_paramQueue.erase(m_paramQueue.begin());  // remove the first one element
    }
    m_paramQueue.emplace_back(std::move(p));

    m_paramCond.notify_all();
  }

  void clear() {
    std::lock_guard<std::mutex> lk(m_paramMutex);
    auto f = thread_fence_guard();
    m_paramQueue.clear();
    m_paramCond.notify_all();
  }

  void invalidate() {
    std::lock_guard<std::mutex> lk(m_paramMutex);
    m_state.store(0, std::memory_order_relaxed);
    m_paramCond.notify_all();
  }

  void validate() {
    std::lock_guard<std::mutex> lk(m_paramMutex);
    m_state.store(1, std::memory_order_relaxed);
    m_paramCond.notify_all();
  }

 public:
  IPCQueueServer() : m_state(1) {
    m_paramQueue.reserve(sizeof(PARAM_T) * QUEUE_LIMITED_SIZE);
  }
  virtual ~IPCQueueServer() = default;

 protected:
  inline std::shared_ptr<void> thread_fence_guard() const {
    std::atomic_thread_fence(std::memory_order_acquire);
    return std::shared_ptr<void>(reinterpret_cast<void*>(0xBADC0DE), [](void*) {
      std::atomic_thread_fence(std::memory_order_release);
    });
  }

 protected:
  std::vector<PARAM_T> m_paramQueue;
  std::mutex m_paramMutex;
  std::condition_variable m_paramCond;
  std::atomic<int> m_state;
};

// ---------------------------------------------------------------------------
// Client Queue
// ---------------------------------------------------------------------------
template <class PARAM_T>
class IPCQueueClient {
  //
  // public APIs
  //
 public:
  virtual int ipc_dequeue(PARAM_T* pResult, uint32_t timeoutMs) {
    return _ipc_acquire_param(pResult, timeoutMs);
  }

 protected:
  // derived class has to implement this method to
  virtual int _ipc_acquire_param(PARAM_T* pResult, uint32_t timeoutMs) = 0;
  //
  // constructor / destructor
  //
 public:
  IPCQueueClient() = default;
  virtual ~IPCQueueClient() = default;
};

};      // namespace v4l2
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_IPC_QUEUE_H_
