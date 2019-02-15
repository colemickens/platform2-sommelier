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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_STLUTILS_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_STLUTILS_H_

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <utility>
#include <vector>

namespace NSCam {

// ----------------------------------------------------------------------------
// SpinLock implementation
// ----------------------------------------------------------------------------
class SpinLock {
  std::atomic_flag locked = ATOMIC_FLAG_INIT;

 public:
  void lock() {
    while (locked.test_and_set(std::memory_order_acquire)) {
    }
  }

  void unlock() { locked.clear(std::memory_order_release); }
};  // Spin lock

// ----------------------------------------------------------------------------
// Scope Worker
// ----------------------------------------------------------------------------
// Give a callback function and a void* argument, Scope Worker will invoke it
// while destroying
class ScopeWorker {
 public:
  explicit ScopeWorker(std::function<void(void* arg)> future_worker,
                       void* arg = NULL) {
    mWorker = future_worker;
    mArg = arg;
  }
  ~ScopeWorker();

 private:
  void* mArg;
  std::function<void(void* arg)> mWorker;
};  // }}}

// ----------------------------------------------------------------------------
// A thread-safe state manage implementation
// ----------------------------------------------------------------------------
template <class T>
class StateManager {
 public:
  StateManager(std::function<T(void)> default_constructor = []() -> T {
    return T();
  }) {
    mState = default_constructor();
  }

  ~StateManager() {}

 private:
  std::mutex mMutex;
  T mState;

 public:
  // get the current state
  // @return  the current state that StateManager is holding
  // @note    this method is thread-safe
  // @sa      getStateNolock
  T getState() {
    std::lock_guard<std::mutex> ___l(mMutex);
    return mState;
  }

  // update the current state to the new one
  // @param s     the state to be updated
  // @note        this method is thread-safe
  // @sa          updateStateNolock
  void updateState(const T& s) {
    std::lock_guard<std::mutex> ___l(mMutex);
    mState = s;
  }

  // do a job with the same thread-safe operation
  // @tparam RTYPE    retrun type of this method
  // @param  worker   a function to work on, which will receive two arguments:
  //                    1. state, call by reference of current state (T&)
  //                       caller can update state by this value
  //                    2. customized argument (void*)
  // @param  arg      an customized argument for worker
  // @return          retruns what worker returns
  template <typename RTYPE = void>
  RTYPE doWork(std::function<RTYPE(const T& state, void* arg)> work,
               void* arg = nullptr) {
    std::lock_guard<std::mutex> ___l(mMutex);
    return work(mState, arg);
  }
};  // }}}

};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_STLUTILS_H_
