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

#define LOG_TAG "MtkCam/StreamBufferPoolImpl"
//
#include "MyUtils.h"
#include <list>
#include <mtkcam/pipeline/utils/streambuf/StreamBufferPoolImpl.h>
//

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace Utils {

/******************************************************************************
 *
 ******************************************************************************/

StreamBufferPoolImpl::StreamBufferPoolImpl() {
  muToAllocCnt = 0;
  mName = "";
  mExitPending = false;
  mRunning = false;
  mLogLevel = ::property_get_int32("debug.camera.log.sbpool", 0);
}

StreamBufferPoolImpl::~StreamBufferPoolImpl() {
  MY_LOGD_IF(mLogLevel > 1, "destroy pool %s", mName);
}

MVOID
StreamBufferPoolImpl::finishImpl() {
  mlAvailableBuf.clear();
  mlInUseBuf.clear();
  mWaitingList.clear();
  muToAllocCnt = 0;
}

/******************************************************************************
 *
 ******************************************************************************/

MERROR
StreamBufferPoolImpl::initPoolImpl(char const* szCallerName,
                                   size_t maxNumberOfBuffers,
                                   size_t minNumberOfInitialCommittedBuffers) {
  MY_LOGD("initPoolImpl %s, max %zu, min %zu", szCallerName, maxNumberOfBuffers,
          minNumberOfInitialCommittedBuffers);
  mName = szCallerName;
  if (szCallerName == nullptr || maxNumberOfBuffers == 0 ||
      maxNumberOfBuffers < minNumberOfInitialCommittedBuffers) {
    MY_LOGE("wrong params: %s, %zu, %zu", szCallerName, maxNumberOfBuffers,
            minNumberOfInitialCommittedBuffers);
    return INVALID_OPERATION;
  }

  {
    std::lock_guard<std::mutex> _l(mLock);

    for (size_t i = minNumberOfInitialCommittedBuffers; i-- > 0;) {
      MUINT32 construct_result = 0;
      if (do_construct(&construct_result) == NO_MEMORY) {
        MY_LOGE("do_construct allocate buffer failed");
        return NO_MEMORY;
      }
      mlAvailableBuf.push_back(construct_result);
    }

    muToAllocCnt = maxNumberOfBuffers - minNumberOfInitialCommittedBuffers;
    ////
  }

  return OK;
}  // initPool

MVOID
StreamBufferPoolImpl::signalUserLocked() {
  if (mWaitingList.size()) {
    (*mWaitingList.begin())->notify_one();
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
StreamBufferPoolImpl::dumpPoolImpl() const {
  std::lock_guard<std::mutex> _l(mLock);
  MY_LOGD("dumpPoolImpl +");

  if (!mlAvailableBuf.empty()) {
    typename std::list<MUINT32>::const_iterator iter = mlAvailableBuf.begin();
    while (iter != mlAvailableBuf.end()) {
      MY_LOGD("available buf %d", *iter);
      iter++;
    }
  }

  if (!mlInUseBuf.empty()) {
    typename std::list<MUINT32>::const_iterator iter = mlInUseBuf.begin();
    while (iter != mlInUseBuf.end()) {
      MY_LOGD("in use buf %d", *iter);
      iter++;
    }
  }

  //
  MY_LOGD("dumpPoolImpl -");
}

MVOID
StreamBufferPoolImpl::uninitPoolImpl(char const* szCallerName) {
  MY_LOGD_IF(mLogLevel > 1, "caller(%s) uninit pool(%s)", szCallerName, mName);
  {
    std::lock_guard<std::mutex> _l(mLock);
    mExitPending = true;
  }

  mThread.join();

  {
    std::lock_guard<std::mutex> _l(mLock);
    mlAvailableBuf.clear();
    mlInUseBuf.clear();
    muToAllocCnt = 0;
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
StreamBufferPoolImpl::commitPoolImpl(char const* szCallerName) {
  MY_LOGD_IF(mLogLevel > 1, "caller(%s) commit pool(%s)", szCallerName, mName);
  if (muToAllocCnt > 0) {
    mThread = std::thread(std::bind(&StreamBufferPoolImpl::threadLoop, this));
  }
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
StreamBufferPoolImpl::acquireFromPoolImpl(char const* szCallerName,
                                          MUINT32* rpBufferIndex,
                                          int64_t nsTimeout) {
  std::unique_lock<std::mutex> _l(mLock);
  MY_LOGD_IF(mLogLevel > 2,
             "caller(%s) aquires buffer from pool(%s), available(%zu)",
             szCallerName, mName, mlAvailableBuf.size());

  if (!mlAvailableBuf.empty()) {
    typename std::list<MUINT32>::iterator iter = mlAvailableBuf.begin();
    mlInUseBuf.push_back(*iter);
    *rpBufferIndex = *iter;
    mlAvailableBuf.erase(iter);
    //
    return OK;
  } else if (!mRunning && muToAllocCnt > 0) {
    MUINT32 construct_result = 0;
    mLock.unlock();
    MERROR err = do_construct(&construct_result);
    mLock.lock();
    if (err == NO_MEMORY) {
      MY_LOGE("do_construct allocate buffer failed");
      return NO_MEMORY;
    }
    mlInUseBuf.push_back(construct_result);
    *rpBufferIndex = construct_result;

    --muToAllocCnt;

    return OK;
  }

  std::condition_variable cond;
  mWaitingList.push_back(&cond);

  // wait for buffer
  MY_LOGD("acquireFromPoolImpl waiting %" PRId64 " ns", nsTimeout);
  cond.wait_for(_l, std::chrono::nanoseconds(nsTimeout));

  std::list<std::condition_variable*>::iterator pCond = mWaitingList.begin();
  while (pCond != mWaitingList.end()) {
    if ((*pCond) == &cond) {
      mWaitingList.erase(pCond);
      break;
    }
    pCond++;
  }

  if (!mlAvailableBuf.empty()) {
    typename std::list<MUINT32>::iterator iter = mlAvailableBuf.begin();
    mlInUseBuf.push_back(*iter);
    *rpBufferIndex = *iter;
    mlAvailableBuf.erase(iter);
    //
    return OK;
  }

  MY_LOGW("mPoolName timeout: buffer available %zu, toAlloc %d",
          mlAvailableBuf.size(), muToAllocCnt);
  return TIMED_OUT;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
StreamBufferPoolImpl::releaseToPoolImpl(char const* szCallerName,
                                        MUINT32 pBufferIndex) {
  MY_LOGD_IF(mLogLevel > 2,
             "caller(%s) release buffer to pool(%s), available(%zu)",
             szCallerName, mName, mlAvailableBuf.size());
  {
    std::lock_guard<std::mutex> _l(mLock);
    typename std::list<MUINT32>::iterator iter = mlInUseBuf.begin();
    while (iter != mlInUseBuf.end()) {
      if (*iter == pBufferIndex) {
        mlAvailableBuf.push_back(*iter);
        mlInUseBuf.erase(iter);
        //
        signalUserLocked();
        return OK;
      }
      iter++;
    }
  }

  MY_LOGE("cannot find buffer index %d", pBufferIndex);
  dumpPoolImpl();

  return INVALID_OPERATION;
}

/******************************************************************************
 *
 ******************************************************************************/
bool StreamBufferPoolImpl::_threadLoop() {
  bool next = false;
  MUINT32 construct_result = 0;
  if (do_construct(&construct_result) == NO_MEMORY) {
    MY_LOGE("do_construct allocate buffer failed");
    return NO_MEMORY;
  }

  {
    std::lock_guard<std::mutex> _l(mLock);
    mlAvailableBuf.push_back(construct_result);

    next = (--muToAllocCnt) > 0;
    signalUserLocked();
  }
  return next;
}

/******************************************************************************
 *
 ******************************************************************************/
bool StreamBufferPoolImpl::threadLoop() {
  while (this->_threadLoop()) {
    std::lock_guard<std::mutex> _l(mLock);
    if (mExitPending) {
      mRunning = false;
      break;
    }
  }
  return true;
}

};  // namespace Utils
};  // namespace v3
};  // namespace NSCam
