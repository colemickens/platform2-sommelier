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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_DEBUG_DEBUG_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_DEBUG_DEBUG_H_
//
/******************************************************************************
 *
 ******************************************************************************/
#include <memory>
#include <string>
#include <vector>
#include <mtkcam/def/common.h>
//
#include <cutils/compiler.h>

/******************************************************************************
 *
 ******************************************************************************/
#ifndef __unused
#define __unused __attribute__((__unused__))
#endif

namespace NSCam {

/**
 *
 */
struct IDebuggee {
  virtual ~IDebuggee() {}

  /**
   * Get the debuggee name.
   * This name must match to one of the names defined in CommandTable.h
   */
  virtual auto debuggeeName() const -> std::string = 0;

  /**
   * Dump debugging state.
   */
  virtual auto debug(const std::vector<std::string>& options) -> void = 0;
};

/**
 *
 */
struct IDebuggeeCookie {
  virtual ~IDebuggeeCookie() {}
};

/**
 *
 */
struct VISIBILITY_PUBLIC IDebuggeeManager {
  static auto get() -> IDebuggeeManager*;

  virtual ~IDebuggeeManager() {}

  /**
   * Attach/detach a debuggee for debugging.
   *
   * @param[in] priority: 0=middle, 1=high, -1=low
   */
  virtual auto attach(std::shared_ptr<IDebuggee> debuggee, int priority = 0)
      -> std::shared_ptr<IDebuggeeCookie> = 0;

  virtual auto detach(std::shared_ptr<IDebuggeeCookie> cookie) -> void = 0;

  /**
   * Dump debugging state.
   */
  virtual auto debug(const std::vector<std::string>& options) -> void = 0;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_DEBUG_DEBUG_H_
