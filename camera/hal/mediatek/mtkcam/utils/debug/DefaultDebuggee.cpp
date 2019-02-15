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

#define LOG_TAG "mtkcam-debug"
//
#include <memory>
#include <mtkcam/utils/debug/debug.h>
#include <mtkcam/utils/std/Log.h>

#include <property_service/property.h>

#include <string>
#include <unordered_map>
#include <vector>

using NSCam::IDebuggee;
using NSCam::IDebuggeeCookie;
using NSCam::IDebuggeeManager;

/****************************************************************************
 *
 ****************************************************************************/
namespace {
class DefaultDebuggee : public IDebuggee {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementation.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  const std::string mDebuggeeName;
  std::shared_ptr<IDebuggeeCookie> mDebuggeeCookie = nullptr;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////            IDebuggee
  virtual auto debuggeeName() const -> std::string { return mDebuggeeName; }
  virtual auto debug(const std::vector<std::string>& options) -> void;

  DefaultDebuggee();

  static auto get() -> std::shared_ptr<DefaultDebuggee>;
};
};  // namespace

/****************************************************************************
 *
 ****************************************************************************/
auto initializeDefaultDebuggee(IDebuggeeManager* pDbgMgr) -> bool {
  if (pDbgMgr) {
    auto pDebuggee = DefaultDebuggee::get();
    if (pDebuggee != nullptr) {
      pDebuggee->mDebuggeeCookie = pDbgMgr->attach(pDebuggee, -1);
      return true;
    }
  }
  return false;
}

/******************************************************************************
 *
 ******************************************************************************/
auto DefaultDebuggee::get() -> std::shared_ptr<DefaultDebuggee> {
  static auto instance = std::make_shared<DefaultDebuggee>();
  return instance;
}

/******************************************************************************
 *
 ******************************************************************************/
DefaultDebuggee::DefaultDebuggee() : IDebuggee(), mDebuggeeName("debug") {
  /**
      "--module debug [--backtrace --unreachable] \n"
      "       --backtrace: dump the currnet backtrace of this process.\n"
      "       --unreachable: dump the unreachable memory of this process.\n"
   */
}

/****************************************************************************
 *
 ****************************************************************************/
auto DefaultDebuggee::debug(const std::vector<std::string>& options) -> void {
  std::string aeeExceptionClass{""};
  for (size_t i = 0; i < options.size();) {
    if (options[i] == "--aee" && i + 1 < options.size()) {
      aeeExceptionClass = options[i + 1];
      i += 2;
      continue;
    }
    ++i;
  }
}
