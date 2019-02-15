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

#define LOG_TAG "MtkCam/streambuf"
//
#include "MyUtils.h"
#include <list>
#include <memory>
#include "mtkcam/pipeline/utils/streambuf/StreamBufferSetControlImp.h"
//

/******************************************************************************
 *
 ******************************************************************************/

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::v3::IUsersManager>
NSCam::v3::Utils::Imp::StreamBufferSetControlImp::findSubjectUsersLocked(
    StreamId_T streamId) const {
#define _IMPLEMENT_(_map_)                      \
  {                                             \
    auto iter = _map_.find(streamId);           \
    if (iter != _map_.end()) {                  \
      return (iter->second)->getUsersManager(); \
    }                                           \
  }

  _IMPLEMENT_(mBufMap_AppImage);
  _IMPLEMENT_(mBufMap_AppMeta);
  _IMPLEMENT_(mBufMap_HalImage);
  _IMPLEMENT_(mBufMap_HalMeta);

#undef _IMPLEMENT_

  MY_LOGW("[frame:%u] cannot find streamId:%#" PRIx64, mFrameNo, streamId);
  return nullptr;
}

/******************************************************************************
 *
 ******************************************************************************/
template <class StreamBufferMapT>
std::shared_ptr<typename StreamBufferMapT::IStreamBufferT>
NSCam::v3::Utils::Imp::StreamBufferSetControlImp::getBufferLocked(
    StreamId_T streamId,
    UserId_T userId,
    StreamBufferMapT const& rBufMap) const {
  if (0 == rBufMap.mNumberOfNonNullBuffers) {
    MY_LOGD("[frameNo:%u streamId:%#" PRIx64 " userId:%#" PRIxPTR
            "] "
            "mNumberOfNonNullBuffers==0",
            getFrameNo(), streamId, userId);
    return nullptr;
  }
  //
  typename StreamBufferMapT::mapped_type pValue = rBufMap.at(streamId);
  if (pValue == nullptr) {
    MY_LOGD("[frameNo:%u streamId:%#" PRIx64 " userId:%#" PRIxPTR
            "] "
            "cannot find from map",
            getFrameNo(), streamId, userId);
    return nullptr;
  }
  //
  if (pValue->mBuffer == 0) {
    MY_LOGW("[frameNo:%u streamId:%#" PRIx64 " userId:%#" PRIxPTR
            "] "
            "mBitStatus(%#lx) pValue->mBuffer == 0",
            getFrameNo(), streamId, userId, pValue->mBitStatus.to_ulong());
    return nullptr;
  }

  /**
   * The buffer is NOT available if all users have released this buffer
   * (so as to be marked as released).
   */
  if (OK == pValue->getUsersManager()->haveAllUsersReleased()) {
    MY_LOGW("[frameNo:%u streamId:%#" PRIx64 " userId:%#" PRIxPTR
            "] "
            "all users released this buffer",
            getFrameNo(), streamId, userId);
    return nullptr;
  }

  /**
   * For a specific stream buffer (associated with a stream Id), a user (with
   * a unique user Id) could successfully acquire the buffer from this buffer
   * set only if all users ahead of this user have pre-released or released
   * the buffer.
   */
  if (OK !=
      pValue->getUsersManager()->haveAllUsersReleasedOrPreReleased(userId)) {
    MY_LOGW("[frameNo:%u streamId:%#" PRIx64 " userId:%#" PRIxPTR
            "] "
            "not all of prior users release or pre-release this buffer",
            getFrameNo(), streamId, userId);
    return nullptr;
  }
  //
  //
  return pValue->mBuffer;
}

/******************************************************************************
 *
 ******************************************************************************/
size_t
NSCam::v3::Utils::Imp::StreamBufferSetControlImp::getMetaBufferSizeLocked()
    const {
  return mBufMap_HalMeta.mNumberOfNonNullBuffers +
         mBufMap_AppMeta.mNumberOfNonNullBuffers;
}

/******************************************************************************
 *
 ******************************************************************************/
size_t
NSCam::v3::Utils::Imp::StreamBufferSetControlImp::getImageBufferSizeLocked()
    const {
  return mBufMap_HalImage.mNumberOfNonNullBuffers +
         mBufMap_AppImage.mNumberOfNonNullBuffers;
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT32
NSCam::v3::Utils::Imp::StreamBufferSetControlImp::markUserStatus(
    StreamId_T const streamId, UserId_T userId, MUINT32 eStatus) {
  std::lock_guard<std::mutex> _l(mBufMapLock);
  //
  std::shared_ptr<NSCam::v3::IUsersManager> pSubjectUsers =
      findSubjectUsersLocked(streamId);
  if (pSubjectUsers == nullptr) {
    return NAME_NOT_FOUND;
  }
  //
  return pSubjectUsers->markUserStatus(userId, eStatus);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::Imp::StreamBufferSetControlImp::setUserReleaseFence(
    StreamId_T const streamId, UserId_T userId, MINT releaseFence) {
  std::lock_guard<std::mutex> _l(mBufMapLock);
  //
  std::shared_ptr<NSCam::v3::IUsersManager> pSubjectUsers =
      findSubjectUsersLocked(streamId);
  if (pSubjectUsers == nullptr) {
    return NAME_NOT_FOUND;
  }
  //
  return pSubjectUsers->setUserReleaseFence(userId, releaseFence);
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT64
NSCam::v3::Utils::Imp::StreamBufferSetControlImp::queryGroupUsage(
    StreamId_T const streamId, UserId_T userId) const {
  std::lock_guard<std::mutex> _l(mBufMapLock);
  //
  std::shared_ptr<NSCam::v3::IUsersManager> pSubjectUsers =
      findSubjectUsersLocked(streamId);
  if (pSubjectUsers == nullptr) {
    return 0;
  }
  //
  return pSubjectUsers->queryGroupUsage(userId);
}

/******************************************************************************
 *
 ******************************************************************************/
MINT NSCam::v3::Utils::Imp::StreamBufferSetControlImp::createAcquireFence(
    StreamId_T const streamId, UserId_T userId) const {
  std::lock_guard<std::mutex> _l(mBufMapLock);
  //
  std::shared_ptr<NSCam::v3::IUsersManager> pSubjectUsers =
      findSubjectUsersLocked(streamId);
  if (pSubjectUsers == nullptr) {
    return -1;
  }
  //
  return pSubjectUsers->createAcquireFence(userId);
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::v3::Utils::IStreamBufferSetControl>
NSCam::v3::Utils::IStreamBufferSetControl::create(
    MUINT32 frameNo, std::weak_ptr<IAppCallback> const& pAppCallback) {
  auto ptr = std::make_shared<NSCam::v3::Utils::Imp::StreamBufferSetControlImp>(
      frameNo, pAppCallback);
  return ptr;
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::Utils::Imp::StreamBufferSetControlImp::StreamBufferSetControlImp(
    MUINT32 frameNo, std::weak_ptr<IAppCallback> const& pAppCallback)
    : mFrameNo(frameNo), mpAppCallback(pAppCallback) {}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::Imp::StreamBufferSetControlImp::attachListener(
    std::weak_ptr<IListener> const& pListener, MVOID* pCookie) {
  std::lock_guard<std::mutex> _lBufMapLock(mBufMapLock);
  //
  mListeners.push_back(MyListener(pListener, pCookie));
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::v3::IMetaStreamBuffer>
NSCam::v3::Utils::Imp::StreamBufferSetControlImp::getMetaBuffer(
    StreamId_T streamId, UserId_T userId) const {
  std::shared_ptr<NSCam::v3::IMetaStreamBuffer> p;
  //
  std::lock_guard<std::mutex> _lBufMapLock(mBufMapLock);
  //
  p = getBufferLocked(streamId, userId, mBufMap_HalMeta);
  if (p != nullptr) {
    return p;
  }
  //
  p = getBufferLocked(streamId, userId, mBufMap_AppMeta);
  if (p != nullptr) {
    return p;
  }
  //
  return nullptr;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::v3::IImageStreamBuffer>
NSCam::v3::Utils::Imp::StreamBufferSetControlImp::getImageBuffer(
    StreamId_T streamId, UserId_T userId) const {
  std::shared_ptr<IImageStreamBuffer> p;
  //
  std::lock_guard<std::mutex> _lBufMapLock(mBufMapLock);
  //
  p = getBufferLocked(streamId, userId, mBufMap_HalImage);
  if (p != nullptr) {
    return p;
  }
  //
  p = getBufferLocked(streamId, userId, mBufMap_AppImage);
  if (p != nullptr) {
    return p;
  }
  //
  return nullptr;
}

/******************************************************************************
 *
 ******************************************************************************/
struct NSCam::v3::Utils::Imp::StreamBufferSetControlImp::TBufMapReleaser_Hal {
 public:  ////                        Data Members.
  MUINT32 const mFrameNo;

  std::list<std::shared_ptr<BufferMap_HalImageT::StreamBufferT> >
      mListToReturn_Image;
  BufferMap_HalImageT mrBufMap_Image;

  std::list<std::shared_ptr<BufferMap_HalMetaT::StreamBufferT> >

      mListToReturn_Meta;
  BufferMap_HalMetaT mrBufMap_Meta;

 public:  ////    Operations.
  TBufMapReleaser_Hal(MUINT32 const frameNo,
                      BufferMap_HalImageT const& rBufMap_Image,
                      BufferMap_HalMetaT const& rBufMap_Meta)
      : mFrameNo(frameNo),
        mrBufMap_Image(rBufMap_Image),
        mrBufMap_Meta(rBufMap_Meta)
  //
  {}

  MVOID
  run() {
    run(mFrameNo, &mrBufMap_Image, &mListToReturn_Image);
    run(mFrameNo, &mrBufMap_Meta, &mListToReturn_Meta);
  }

  template <class StreamBufferMapT, class StreamBufferListT>
  MVOID run(MUINT32 const frameNo,
            StreamBufferMapT* rBufferMap,
            StreamBufferListT* rListToReturn) {
    typename StreamBufferMapT::iterator it = rBufferMap->begin();
    for (; it != rBufferMap->end(); it++) {
      typename StreamBufferMapT::mapped_type const& pValue = it->second;
      if (pValue == nullptr) {
        continue;
      }
      //
      // Skip if NULL buffer
      if (pValue->mBuffer == nullptr) {
        continue;
      }
      //
      //  [Hal Stream Buffers]
      //
      //  Condition:
      //      .This buffer is not returned before.
      //      .All users of this buffer have been released.
      //
      //  Execution:
      //      .Prepare a list of buffer to return without Release Fences.
      //
      std::shared_ptr<typename StreamBufferMapT::StreamBufferT>& rBuffer =
          pValue->mBuffer;
      std::bitset<32>& rBitStatus = pValue->mBitStatus;
      //
      //  action if not returned && all users released
      if ((!rBitStatus.test(eBUF_STATUS_RETURN)) &&
          (pValue->getUsersManager()->haveAllUsersReleased() == OK)) {
        rListToReturn->push_back(rBuffer);
        rBitStatus.set(eBUF_STATUS_RETURN);
        //
        rBitStatus.set(eBUF_STATUS_RELEASE);
        rBuffer = nullptr;
        rBufferMap->mNumberOfNonNullBuffers--;
      }
    }
  }

  MVOID
  handleResult() {
    returnBuffers(&mListToReturn_Image);
    returnBuffers(&mListToReturn_Meta);
  }

  template <class T>
  MVOID returnBuffers(T* listToReturn) {
    //  Return each buffer to its pool.
    typename T::iterator it = listToReturn->begin();
    for (; it != listToReturn->end(); it++) {
      if ((*it) != 0) {
        (*it)->releaseBuffer();
      }
    }

    listToReturn->clear();
  }
};

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Utils::Imp::StreamBufferSetControlImp::applyRelease(
    UserId_T userId) {
  MY_LOGD("frameNo:%u userId:%#" PRIxPTR " +", mFrameNo, userId);

  std::shared_ptr<IAppCallback> pAppCallback;
  std::list<MyListener> listeners;

  //  [Hal Image/Meta Stream Buffers]
  TBufMapReleaser_Hal releaserHal(mFrameNo, mBufMap_HalImage, mBufMap_HalMeta);
  //
  {
    std::lock_guard<std::mutex> _lBufMapLock(mBufMapLock);
    //
    releaserHal.run();
    pAppCallback = mpAppCallback.lock();
    listeners = mListeners;
  }
  //
  //  Return Stream Buffers.
  releaserHal.handleResult();
  if (pAppCallback == nullptr) {
    MY_LOGW("Caonnot promote AppCallback for frameNo:%u , userId:%#" PRIxPTR,
            mFrameNo, userId);
  } else {
    pAppCallback->updateFrame(mFrameNo, userId);
  }
  //
  //  Notify listeners.
  {
    std::list<MyListener>::iterator it = listeners.begin();
    for (; it != listeners.end(); it++) {
      std::shared_ptr<IListener> p = it->mpListener.lock();
      if (p != 0) {
        p->onStreamBufferSet_Updated(it->mpCookie);
      }
    }
  }

  MY_LOGD("frameNo:%u userId:%#" PRIxPTR " -", mFrameNo, userId);
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Utils::Imp::StreamBufferSetControlImp::applyPreRelease(
    UserId_T userId) {
  applyRelease(userId);
}
