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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_STREAMBUFFERPOOLIMPL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_STREAMBUFFERPOOLIMPL_H_
//
#include <list>
#include <thread>
#include <mtkcam/def/common.h>
#include <mtkcam/utils/property_service/property_lib.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace Utils {

class VISIBILITY_PUBLIC StreamBufferPoolImpl {
 public:
  StreamBufferPoolImpl();
  virtual ~StreamBufferPoolImpl();

  MVOID finishImpl();

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Pool/Buffer
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                        Operations.
  MVOID dumpPoolImpl() const;
  MERROR initPoolImpl(char const* szCallerName,
                      size_t maxNumberOfBuffers,
                      size_t minNumberOfInitialCommittedBuffers);
  MVOID uninitPoolImpl(char const* szCallerName);
  MERROR commitPoolImpl(char const* szCallerName);
  MERROR acquireFromPoolImpl(char const* szCallerName,
                             MUINT32* rpBufferIndex,
                             int64_t nsTimeout);
  MERROR releaseToPoolImpl(char const* szCallerName, MUINT32 pBufferIndex);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Thread Operations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private:
  bool _threadLoop();
  bool threadLoop();

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Operations
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:
  // signal a user acquring buffer
  MVOID signalUserLocked();

 protected:
  // for alloc buffer
  virtual MERROR do_construct(MUINT32* returnIndex) = 0;
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:
  mutable std::mutex mLock;
  volatile bool mExitPending;
  volatile bool mRunning;
  std::thread mThread;
  MUINT32 muToAllocCnt;
  std::list<MUINT32> mlAvailableBuf;
  std::list<MUINT32> mlInUseBuf;
  //
  char const* mName;
  MINT32 mLogLevel;
  //
  std::list<std::condition_variable*> mWaitingList;
};  // StreamBufferPoolImpl

};      // namespace Utils
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_STREAMBUFFERPOOLIMPL_H_
