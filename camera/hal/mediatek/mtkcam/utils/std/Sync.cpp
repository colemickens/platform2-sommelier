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

#define LOG_TAG "MtkCam/Utils"

//
#define __STDC_LIMIT_MACROS
#include "MyUtils.h"
#include <memory>
#include <mtkcam/utils/std/Sync.h>
#include <stdint.h>
#include <string>
#include <sync/sync.h>

using NSCam::MPoint;
using NSCam::MPointF;
using NSCam::MRational;
using NSCam::MRect;
using NSCam::MRectF;
using NSCam::MSize;
using NSCam::MSizeF;
using NSCam::Type2Type;
using NSCam::Utils::Sync::IFence;

/******************************************************************************
 *
 ******************************************************************************/
namespace {
class FenceImp : public IFence {
  friend class IFence;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                        Data Members.
  MINT mFenceFd;
  std::string mFenceName;

 public:  ////
  virtual ~FenceImp();
  explicit FenceImp(MINT fenceFd = -1);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IFence Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                        Operations.
  virtual MINT dup() const;

  virtual MERROR wait(MINT timeoutMs);

  virtual MERROR waitForever(char const* logname);

 public:  ////                        Attributes.
  virtual char const* name() const { return mFenceName.c_str(); }

  virtual MBOOL isValid() const { return (mFenceFd != -1); }

  virtual MINT getFd() const { return mFenceFd; }

  virtual MINT64 getSignalTime() const;
};
};  // namespace

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IFence> const IFence::NO_FENCE(new FenceImp);

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IFence> IFence::create(MINT fenceFd) {
  return std::make_shared<FenceImp>(fenceFd);
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IFence> IFence::merge(char const* szFenceName,
                                      std::shared_ptr<IFence> const& f1,
                                      std::shared_ptr<IFence> const& f2) {
  CAM_TRACE_CALL();
  //
  int result;
  if (f1->isValid() && f2->isValid()) {
    result = ::sync_merge(szFenceName, f1->getFd(), f2->getFd());
  } else if (f1->isValid()) {
    result = ::sync_merge(szFenceName, f1->getFd(), f1->getFd());
  } else if (f2->isValid()) {
    result = ::sync_merge(szFenceName, f2->getFd(), f2->getFd());
  } else {
    return NO_FENCE;
  }

  if (result == -1) {
    MY_LOGE("Error merge: sync_merge(\"%s\", %d, %d)", szFenceName, f1->getFd(),
            f2->getFd());
    return NO_FENCE;
  }

  return std::make_shared<FenceImp>(result);
}

/******************************************************************************
 *
 ******************************************************************************/
FenceImp::~FenceImp() {
  if (mFenceFd != -1) {
    ::close(mFenceFd);
    mFenceFd = -1;
  }
}

/******************************************************************************
 *
 ******************************************************************************/
FenceImp::FenceImp(MINT fenceFd) : mFenceFd(fenceFd) {
  if (0 <= fenceFd) {
    struct sync_fence_info_data* info = ::sync_fence_info(mFenceFd);
    if (info) {
      mFenceName = info->name;
    }
    ::sync_fence_info_free(info);
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MINT FenceImp::dup() const {
  return ::dup(mFenceFd);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
FenceImp::wait(MINT timeoutMs) {
  CAM_TRACE_CALL();
  //
  if (mFenceFd == -1) {
    return OK;
  }
  int err = ::sync_wait(mFenceFd, timeoutMs);
  return err < 0 ? -errno : MERROR(OK);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
FenceImp::waitForever(char const* logname) {
  CAM_TRACE_CALL();

  if (mFenceFd == -1) {
    return OK;
  }
  unsigned int warningTimeout = 3000;
  int err = ::sync_wait(mFenceFd, warningTimeout);
  if (err < 0 && errno == ETIME) {
    MY_LOGW("%s: fence %d didn't signal in %u ms", logname, mFenceFd,
            warningTimeout);

    err = ::sync_wait(mFenceFd, NSCam::Utils::Sync::IFence::TIMEOUT_NEVER);
  }
  return err < 0 ? -errno : MERROR(OK);
}

/******************************************************************************
 *
 ******************************************************************************/
MINT64
FenceImp::getSignalTime() const {
  if (mFenceFd == -1) {
    return -1;
  }

  struct sync_fence_info_data* finfo = ::sync_fence_info(mFenceFd);
  if (finfo == NULL) {
    MY_LOGE("sync_fence_info returned NULL for fd %d", mFenceFd);
    return -1;
  }
  if (finfo->status != 1) {
    ::sync_fence_info_free(finfo);
    return INT64_MAX;
    // return -1;
  }

  struct sync_pt_info* pinfo = NULL;
  uint64_t timestamp = 0;
  while ((pinfo = ::sync_pt_info(finfo, pinfo)) != NULL) {
    if (pinfo->timestamp_ns > timestamp) {
      timestamp = pinfo->timestamp_ns;
    }
  }
  ::sync_fence_info_free(finfo);

  return timestamp;
}
