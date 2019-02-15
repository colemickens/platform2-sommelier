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
#include "CommandTable.h"
#include <list>
#include <memory>
#include <mtkcam/utils/debug/debug.h>
#include <mtkcam/utils/std/Log.h>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
namespace {
struct DebuggeeManagerImpl : public NSCam::IDebuggeeManager {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:
  struct Cookie;
  using CookieListT = std::list<std::pair<std::string, std::weak_ptr<Cookie>>>;
  struct Cookie : public NSCam::IDebuggeeCookie {
    struct timespec mTimestamp;
    int mPriority;
    CookieListT::iterator mIterator;
    std::weak_ptr<NSCam::IDebuggee> mDebuggee;

    virtual ~Cookie();
  };

 protected:
  std::mutex mMutex;
  CookieListT mCookieListH;  // 1: high
  CookieListT mCookieListM;  // 0: middle
  CookieListT mCookieListL;  // -1: low

 protected:
  auto getCookieListLocked(int priority) -> CookieListT*;
  auto detach(Cookie* c) -> void;

 public:
  ~DebuggeeManagerImpl();

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  static auto get() -> DebuggeeManagerImpl*;

  virtual auto attach(std::shared_ptr<NSCam::IDebuggee> debuggee, int priority)
      -> std::shared_ptr<NSCam::IDebuggeeCookie>;

  virtual auto detach(std::shared_ptr<NSCam::IDebuggeeCookie> cookie) -> void;

  virtual auto debug(const std::vector<std::string>& options) -> void;
};  // DebuggeeManagerImpl
};  // namespace

/******************************************************************************
 *
 ******************************************************************************/
auto initializeDefaultDebuggee(NSCam::IDebuggeeManager* pDbgMgr) -> bool;

auto DebuggeeManagerImpl::get() -> DebuggeeManagerImpl* {
  static DebuggeeManagerImpl* instance = new DebuggeeManagerImpl();
  initializeDefaultDebuggee(instance);
  return instance;
}

/******************************************************************************
 *
 ******************************************************************************/
auto NSCam::IDebuggeeManager::get() -> NSCam::IDebuggeeManager* {
  return NULL;  // Todo, will be removed
}

/******************************************************************************
 *
 ******************************************************************************/
DebuggeeManagerImpl::Cookie::~Cookie() {
  DebuggeeManagerImpl::get()->detach(this);
}

/******************************************************************************
 *
 ******************************************************************************/
DebuggeeManagerImpl::~DebuggeeManagerImpl() {
  MY_LOGD("%p", this);
}

/******************************************************************************
 *
 ******************************************************************************/
auto DebuggeeManagerImpl::getCookieListLocked(int priority) -> CookieListT* {
  switch (priority) {
    case 1:
      return &mCookieListH;
    case 0:
      return &mCookieListM;
    case -1:
      return &mCookieListL;
    default:
      break;
  }
  MY_LOGE("priority %d out of range", priority);
  return nullptr;
}

/******************************************************************************
 *
 ******************************************************************************/
auto DebuggeeManagerImpl::detach(Cookie* c) -> void {
  std::lock_guard<std::mutex> _l(mMutex);
  auto pCookieList = getCookieListLocked(c->mPriority);
  if (!pCookieList) {
    return;
  }
  auto& rCookieList = *pCookieList;
  if (rCookieList.end() != c->mIterator) {
    rCookieList.erase(c->mIterator);
    c->mIterator = rCookieList.end();
  }
}

/******************************************************************************
 *
 ******************************************************************************/
auto DebuggeeManagerImpl::attach(std::shared_ptr<NSCam::IDebuggee> debuggee,
                                 int priority)
    -> std::shared_ptr<NSCam::IDebuggeeCookie> {
  if (debuggee == nullptr) {
    MY_LOGE("bad debuggee: null");
    return nullptr;
  }

  auto const name = debuggee->debuggeeName();

  auto search = getDebuggableMap().find(name);
  if (search == getDebuggableMap().end()) {
    MY_LOGE("bad debuggee: \"%s\" not defined", name.c_str());
    return nullptr;
  }

  if (-1 > priority || 1 < priority) {
    MY_LOGE("debuggee \"%s\": priority %d out of range", name.c_str(),
            priority);
    return nullptr;
  }

  auto cookie = std::make_shared<Cookie>();
  if (cookie == nullptr) {
    MY_LOGE("fail to make a new cookie");
    return nullptr;
  }

  clock_gettime(CLOCK_REALTIME, &cookie->mTimestamp);
  cookie->mDebuggee = debuggee;
  cookie->mPriority = priority;

  std::lock_guard<std::mutex> _l(mMutex);
  auto pCookieList = getCookieListLocked(priority);
  if (!pCookieList) {
    return nullptr;
  }
  auto& rCookieList = *pCookieList;
  auto iter = rCookieList.emplace(rCookieList.end(), name, cookie);
  cookie->mIterator = iter;
  return cookie;
}

/******************************************************************************
 *
 ******************************************************************************/
auto DebuggeeManagerImpl::detach(std::shared_ptr<NSCam::IDebuggeeCookie> cookie)
    -> void {
  if (cookie == nullptr) {
    MY_LOGE("bad cookie: null");
    return;
  }

  detach(static_cast<Cookie*>(cookie.get()));
}

/******************************************************************************
 *
 ******************************************************************************/

auto DebuggeeManagerImpl::debug(const std::vector<std::string>& options)
    -> void {}
