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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_SYNC_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_SYNC_H_

#include <memory>
#include <mtkcam/def/common.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace Utils {
namespace Sync {

/******************************************************************************
 *  Fence Interface
 ******************************************************************************/
class VISIBILITY_PUBLIC IFence {
 public:  ////                        Definitions.
  static std::shared_ptr<IFence> const NO_FENCE;

  enum { TIMEOUT_NEVER = -1 };

 public:  ////                        Operations.
  /**
   * Create a new fence object to manage a given fence file descriptor.
   * If a valid fence file descriptor is given, it will be closed when the
   * newly createdly object is destructed.
   */
  static std::shared_ptr<IFence> create(MINT fenceFd);

  /**
   * Merge two fence objects, creating a new fence object that becomes
   * signaled when both f1 and f2 are signaled (even if f1 or f2 is destroyed
   * before it becomes signaled)
   *
   * @param[in] szFenceName: a name to associated with the new fence.
   *
   * @param[in] f1,f2: fence objects to merge.
   *
   * @return a newly createdly fence.
   */
  static std::shared_ptr<IFence> merge(char const* szFenceName,
                                       std::shared_ptr<IFence> const& f1,
                                       std::shared_ptr<IFence> const& f2);

  /**
   * Return a duplicate of the fence file descriptor.
   * The caller is responsible for closing the returned file descriptor.
   * On error, -1 will be returned and errno will indicate the problem.
   */
  virtual MINT dup() const = 0;

  /**
   * Wait with a given timeout for a fence to signal.
   *
   * @param[in] timeoutMs: a timeout in milliseconds.
   *      A timeout of TIMEOUT_NEVER may be used to indicate that the call
   *      should wait indefinitely for the fence to signal.
   *
   * @return
   *      OK if the fence is signaled.
   *      TIMED_OUT if the timeout expires before the fence signals.
   */
  virtual MERROR wait(MINT timeoutMs) = 0;

  /**
   * Wait forever for a fence to signal.
   * Just like wait(TIMEOUT_NEVER), this is a convenience function for waiting
   * forever but issuing an error to the system log and fence state to the
   * kernel log if the wait lasts longer than a warning timeout.
   *
   * @param[in] logname: a timeout in milliseconds.
   *      The logname argument should be a string identifying the caller and
   *      will be included in the log message.
   *
   * @return
   *      OK if the fence is signaled.
   *      TIMED_OUT if the timeout expires before the fence signals.
   */
  virtual MERROR waitForever(char const* logname) = 0;

 public:  ////                        Attributes.
  /**
   * Fence name.
   */
  virtual char const* name() const = 0;

  /**
   * Check to see whether this fence is valid or not.
   */
  virtual MBOOL isValid() const = 0;

  /**
   * Get fence fd.
   */
  virtual MINT getFd() const = 0;

  /**
   * Return the system monotonic clock time at which the fence transitioned to
   * the signaled state.
   *
   * @return
   *      -1 if the fence is invalid or if an error occurs.
   *      INT64_MAX if the fence is not signaled.
   *      Otherwise, a timestamp in ns, at which the fence is signaled.
   */
  virtual MINT64 getSignalTime() const = 0;
};

/******************************************************************************
 *  Timeline Interface
 ******************************************************************************/
class ITimeline {
 public:  ////                        Operations.
  /**
   * Create a new sync timeline object.
   *
   * @param[in] name: a name to associated with the timeline.
   */
  static std::shared_ptr<ITimeline> create(char const* szTimelineName);

  /**
   * Increase timeline
   *
   * @param[in] count: timline increase count.
   */
  virtual MERROR inc(size_t count) = 0;

  /**
   * Create a new Fence
   *
   * @param[in] szFenceName: The name of Fence
   * @param[in] value: Timeline FD
   */
  virtual MINT createFence(char const* szFenceName, size_t value) = 0;

 public:  ////                        Attributes.
  /**
   * Timeline name.
   */
  virtual char const* name() const = 0;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace Sync
};      // namespace Utils
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_SYNC_H_
