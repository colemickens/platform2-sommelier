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
#include <memory>
#include <mtkcam/pipeline/utils/streambuf/StreamBuffers.h>
#include <string>
//

/******************************************************************************
 *
 ******************************************************************************/
static std::string bufferStatusToString(const MUINT32 o) {
  std::string os;
  if (o) {
    os += "status={";
    if (o & NSCam::v3::STREAM_BUFFER_STATUS::WRITE) {
      os += " WRITE";
    }
    if (o & NSCam::v3::STREAM_BUFFER_STATUS::ERROR) {
      os += " ERROR";
    }
    os += " } ";
  }
  return os;
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::Utils::StreamBufferImp::StreamBufferImp(
    std::shared_ptr<IStreamInfo> pStreamInfo,
    MVOID* const pBuffer,
    std::shared_ptr<NSCam::v3::IUsersManager> pUsersManager)
    : mStreamInfo(pStreamInfo),
      mBufStatus(0),
      mUsersManager(
          pUsersManager != nullptr
              ? pUsersManager
              : std::make_shared<UsersManager>(pStreamInfo->getStreamId(),
                                               pStreamInfo->getStreamName()))
      //
      ,
      mpBuffer(pBuffer) {}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Utils::StreamBufferImp::setUsersManager(
    std::shared_ptr<NSCam::v3::IUsersManager> value) {
  mUsersManager = value;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Utils::StreamBufferImp::dumpLocked() const {
  printLocked();
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Utils::StreamBufferImp::printLocked() const {
  std::string os;

  os += bufferStatusToString(mBufStatus);

  for (typename RWUserListT::iterator it = mWriters.begin();
       it != mWriters.end(); it++) {
    os += base::StringPrintf("Write-locker: \"%s\" ", it->name.c_str());
  }
  //
  for (typename RWUserListT::iterator it = mReaders.begin();
       it != mReaders.end(); it++) {
    if (it == mReaders.begin()) {
      os += base::StringPrintf("Read-locker:");
    }
    os += base::StringPrintf(" \"%s\" ", it->name.c_str());
  }
  //
  mUsersManager->dumpState();
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Utils::StreamBufferImp::dumpState() const {
  if (mBufMutex.try_lock_for(std::chrono::milliseconds(100))) {
    printLocked();
    mBufMutex.unlock();
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Utils::StreamBufferImp::onUnlock(char const* szCallName,
                                            MVOID* const pBuffer) {
  std::lock_guard<std::timed_mutex> _l(mBufMutex);
  //
  if (CC_UNLIKELY(mpBuffer != pBuffer)) {
    MY_LOGE("[%s:%p] %s cannot unlock buffer:%p", getName(), mpBuffer,
            szCallName, pBuffer);
    dumpLocked();
    return;
  }
  //
  for (typename RWUserListT::iterator it = mWriters.begin();
       it != mWriters.end(); it++) {
    if (it->name == szCallName) {
      mWriters.erase(it);
      MY_LOGV("[%s:%p] Writer %s", getName(), mpBuffer, szCallName);
      return;
    }
  }
  //
  for (typename RWUserListT::iterator it = mReaders.begin();
       it != mReaders.end(); it++) {
    if (it->name == szCallName) {
      mReaders.erase(it);
      MY_LOGV("[%s:%p] Reader %s", getName(), mpBuffer, szCallName);
      return;
    }
  }
  //
  MY_LOGE("[%s:%p] %s has not locked buffer", getName(), mpBuffer, szCallName);
  dumpLocked();
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID* NSCam::v3::Utils::StreamBufferImp::onTryReadLock(
    char const* szCallName) {
  std::lock_guard<std::timed_mutex> _l(mBufMutex);
  //
  if (!mWriters.empty()) {
    MY_LOGV("[%s:%p] Writers %s:exist; %s:denied", getName(), mpBuffer,
            mWriters.begin()->name.c_str(), szCallName);
    return nullptr;
  }
  //
  MY_LOGV("[%s:%p] Reader %s", getName(), mpBuffer, szCallName);
  mReaders.push_back(RWUser(szCallName));
  return mpBuffer;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID* NSCam::v3::Utils::StreamBufferImp::onTryWriteLock(
    char const* szCallName) {
  std::lock_guard<std::timed_mutex> _l(mBufMutex);
  //
  if (!mWriters.empty()) {
    MY_LOGV("[%s:%p] Writers %s:exist; %s:denied", getName(), mpBuffer,
            mWriters.begin()->name.c_str(), szCallName);
    return nullptr;
  }
  if (!mReaders.empty()) {
    MY_LOGV("[%s:%p] Readers %s:exist; %s:denied", getName(), mpBuffer,
            mReaders.begin()->name.c_str(), szCallName);
    return nullptr;
  }
  //
  MY_LOGV("[%s:%p] Writer %s", getName(), mpBuffer, szCallName);
  mWriters.push_back(RWUser(szCallName));
  return mpBuffer;
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::IUsersManager::Subject_T
NSCam::v3::Utils::StreamBufferImp::getSubject() const {
  return mUsersManager->getSubject();
}

/******************************************************************************
 *
 ******************************************************************************/
char const* NSCam::v3::Utils::StreamBufferImp::getSubjectName() const {
  return mUsersManager->getSubjectName();
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::v3::IUsersManager::IUserGraph>
NSCam::v3::Utils::StreamBufferImp::createGraph() {
  return mUsersManager->createGraph();
}

/******************************************************************************
 *
 ******************************************************************************/
ssize_t NSCam::v3::Utils::StreamBufferImp::enqueUserGraph(
    std::shared_ptr<IUserGraph> pUserGraph) {
  return mUsersManager->enqueUserGraph(pUserGraph);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::StreamBufferImp::finishUserSetup() {
  return mUsersManager->finishUserSetup();
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Utils::StreamBufferImp::reset() {
  mUsersManager->reset();
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT32
NSCam::v3::Utils::StreamBufferImp::markUserStatus(UserId_T userId,
                                                  MUINT32 const statusMask) {
  std::lock_guard<std::timed_mutex> _l(
      mBufMutex);  // This call is to avoid race condition,
  // since buffer status and users status are
  // in the same critical section.
  //
  MUINT32 const result = mUsersManager->markUserStatus(userId, statusMask);
  //
  bool const isReleased = result & (UserStatus::RELEASE);
  bool const isUsed = result & (UserStatus::USED);
  if (!isUsed && isReleased &&
      Category::PRODUCER == mUsersManager->getUserCategory(userId)) {
    //  This producer user has released the buffer without producing its
    //  content, its content must be ERROR.  It may happen when a producer
    //  tries to flush or cancel the buffer.
    MY_LOGD("%s:%#" PRIx64 ":%p producer:%#" PRIxPTR
            " released w/o using -> ERROR buffer - user status:%#x:%#x",
            getName(), getStreamId(), mpBuffer, userId, statusMask, result);
    mBufStatus |= STREAM_BUFFER_STATUS::ERROR;
  }
  //
  return result;
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT32
NSCam::v3::Utils::StreamBufferImp::getUserStatus(UserId_T userId) const {
  return mUsersManager->getUserStatus(userId);
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT
NSCam::v3::Utils::StreamBufferImp::getUserCategory(UserId_T userId) const {
  return mUsersManager->getUserCategory(userId);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::StreamBufferImp::setUserReleaseFence(UserId_T userId,
                                                       MINT releaseFence) {
  return mUsersManager->setUserReleaseFence(userId, releaseFence);
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT64
NSCam::v3::Utils::StreamBufferImp::queryGroupUsage(UserId_T userId) const {
  return mUsersManager->queryGroupUsage(userId);
}

/******************************************************************************
 *
 ******************************************************************************/
size_t NSCam::v3::Utils::StreamBufferImp::getNumberOfProducers() const {
  return mUsersManager->getNumberOfProducers();
}

/******************************************************************************
 *
 ******************************************************************************/
size_t NSCam::v3::Utils::StreamBufferImp::getNumberOfConsumers() const {
  return mUsersManager->getNumberOfConsumers();
}

/******************************************************************************
 *
 ******************************************************************************/
MINT NSCam::v3::Utils::StreamBufferImp::createAcquireFence(
    UserId_T userId) const {
  return mUsersManager->createAcquireFence(userId);
}

/******************************************************************************
 *
 ******************************************************************************/
MINT NSCam::v3::Utils::StreamBufferImp::createReleaseFence(
    UserId_T userId) const {
  return mUsersManager->createReleaseFence(userId);
}

/******************************************************************************
 *
 ******************************************************************************/
MINT NSCam::v3::Utils::StreamBufferImp::createAcquireFence() const {
  return mUsersManager->createAcquireFence();
}

/******************************************************************************
 *
 ******************************************************************************/
MINT NSCam::v3::Utils::StreamBufferImp::createReleaseFence() const {
  return mUsersManager->createReleaseFence();
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::StreamBufferImp::haveAllUsersReleasedOrPreReleased(
    UserId_T userId) const {
  return mUsersManager->haveAllUsersReleasedOrPreReleased(userId);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::StreamBufferImp::haveAllUsersReleased() const {
  return mUsersManager->haveAllUsersReleased();
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT32
NSCam::v3::Utils::StreamBufferImp::getAllUsersStatus() const {
  return mUsersManager->getAllUsersStatus();
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::StreamBufferImp::haveAllProducerUsersReleased() const {
  return mUsersManager->haveAllProducerUsersReleased();
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::Utils::StreamBufferImp::haveAllProducerUsersUsed() const {
  return mUsersManager->haveAllProducerUsersUsed();
}

/******************************************************************************
 *
 ******************************************************************************/
template <class _StreamBufferT_, class _IStreamBufferT_>
std::shared_ptr<NSCam::v3::Utils::IStreamBufferPool<_IStreamBufferT_> >
NSCam::v3::Utils::TStreamBufferWithPool<_StreamBufferT_,
                                        _IStreamBufferT_>::tryGetBufferPool()
    const {
  std::lock_guard<std::mutex> _l(mBufPoolMutex);
  //
  auto pPool = mBufPool.lock();
  if (CC_UNLIKELY(pPool == nullptr)) {
    MY_LOGD("[%s:%p] NULL promote of buffer pool:%p", ParentT::getName(), this,
            pPool.get());
  }
  //
  return pPool;
}

/******************************************************************************
 *
 ******************************************************************************/
template <class _StreamBufferT_, class _IStreamBufferT_>
MVOID NSCam::v3::Utils::
    TStreamBufferWithPool<_StreamBufferT_, _IStreamBufferT_>::releaseBuffer() {
  auto pStreamBufferPool = tryGetBufferPool();
  if (CC_LIKELY(pStreamBufferPool != nullptr)) {
    //
    //  Reset buffer before returning to pool.
    resetBuffer();
    //
    //  Release to Pool

    MERROR err = pStreamBufferPool->releaseToPool(ParentT::getName(),
                                                  this->shared_from_this());
    MY_LOGE_IF(OK != err, "%s fail to release to pool", ParentT::getName());
  }
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::Utils::HalImageStreamBuffer::Allocator::Allocator(
    std::shared_ptr<IStreamInfoT> pStreamInfo,
    IGbmImageBufferHeap::AllocImgParam_t const& rAllocImgParam)
    : mpStreamInfo(pStreamInfo), mAllocImgParam(rAllocImgParam) {}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::v3::Utils::HalImageStreamBuffer::StreamBufferT>
NSCam::v3::Utils::HalImageStreamBuffer::Allocator::operator()(
    std::shared_ptr<IStreamBufferPoolT> pPool) {
  std::shared_ptr<IGbmImageBufferHeap> pImageBufferHeap =
      IGbmImageBufferHeap::create(mpStreamInfo->getStreamName(), mAllocImgParam,
                                  IGbmImageBufferHeap::AllocExtraParam(),
                                  MFALSE);
  if (CC_UNLIKELY(pImageBufferHeap == nullptr)) {
    MY_LOGE("IGbmImageBufferHeap::create: %s", mpStreamInfo->getStreamName());
    return nullptr;
  }
  //
  auto ptr = std::make_shared<HalImageStreamBuffer::StreamBufferT>(
      mpStreamInfo, pPool, pImageBufferHeap);

  return ptr;
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::Utils::HalImageStreamBuffer::HalImageStreamBuffer(
    std::shared_ptr<IStreamInfoT> pStreamInfo,
    std::weak_ptr<IStreamBufferPoolT> pStreamBufPool,
    std::shared_ptr<IImageBufferHeap> pImageBufferHeap)
    : TStreamBufferWithPool(
          pStreamBufPool, pStreamInfo, pImageBufferHeap.get()),
      mImageBufferHeap(pImageBufferHeap)
//
{}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Utils::HalImageStreamBuffer::resetBuffer() {
  reset();
  //
  {
    std::lock_guard<std::timed_mutex> _l(mBufMutex);
    mBufStatus = 0;
    mWriters.clear();
    mReaders.clear();
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Utils::HalImageStreamBuffer::releaseBuffer() {
  TStreamBufferWithPool<HalImageStreamBuffer,
                        IImageStreamBuffer>::releaseBuffer();
}

/******************************************************************************
 *
 ******************************************************************************/
std::string NSCam::v3::Utils::HalImageStreamBuffer::toString() const {
  std::string os;

  if (auto p = getStreamInfo()) {
    os += p->toString();
  }

  if (auto s = getStatus()) {
    os += " ";
    os += bufferStatusToString(s);
  }

  return os;
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::Utils::HalMetaStreamBuffer::Allocator::Allocator(
    std::shared_ptr<IStreamInfoT> pStreamInfo)
    : mpStreamInfo(pStreamInfo) {}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::v3::Utils::HalMetaStreamBuffer::StreamBufferT>
NSCam::v3::Utils::HalMetaStreamBuffer::Allocator::operator()(
    std::shared_ptr<IStreamBufferPoolT> pPool) {
  auto ptr = std::shared_ptr<HalMetaStreamBuffer::StreamBufferT>(
      new StreamBufferT(mpStreamInfo, pPool));
  return ptr;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::v3::Utils::HalMetaStreamBuffer::StreamBufferT>
NSCam::v3::Utils::HalMetaStreamBuffer::Allocator::operator()(
    NSCam::IMetadata const& metadata,
    std::shared_ptr<IStreamBufferPoolT> pPool) {
  auto ptr = std::shared_ptr<HalMetaStreamBuffer::StreamBufferT>(
      new StreamBufferT(metadata, mpStreamInfo, pPool));
  return ptr;
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::Utils::HalMetaStreamBuffer::HalMetaStreamBuffer(
    std::shared_ptr<IStreamInfoT> pStreamInfo,
    std::weak_ptr<IStreamBufferPoolT> pStreamBufPool)
    : TStreamBufferWithPool(pStreamBufPool, pStreamInfo, &mMetadata),
      mRepeating(MFALSE)
//
{}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::Utils::HalMetaStreamBuffer::HalMetaStreamBuffer(
    NSCam::IMetadata const& metadata,
    std::shared_ptr<IStreamInfoT> pStreamInfo,
    std::weak_ptr<IStreamBufferPoolT> pStreamBufPool)
    : TStreamBufferWithPool(pStreamBufPool, pStreamInfo, &mMetadata),
      mMetadata(metadata),
      mRepeating(MFALSE)
//
{}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Utils::HalMetaStreamBuffer::resetBuffer() {
  reset();
  //
  {
    std::lock_guard<std::timed_mutex> _l(mBufMutex);
    mBufStatus = 0;
    mWriters.clear();
    mReaders.clear();
    mMetadata.clear();
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Utils::HalMetaStreamBuffer::releaseBuffer() {
  TStreamBufferWithPool<HalMetaStreamBuffer,
                        IMetaStreamBuffer>::releaseBuffer();
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::Utils::HalMetaStreamBuffer::setRepeating(MBOOL const repeating) {
  std::lock_guard<std::timed_mutex> _l(mRepeatMutex);
  mRepeating = repeating;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
NSCam::v3::Utils::HalMetaStreamBuffer::isRepeating() const {
  std::lock_guard<std::timed_mutex> _l(mRepeatMutex);
  return mRepeating;
}

/******************************************************************************
 *
 ******************************************************************************/
std::string NSCam::v3::Utils::HalMetaStreamBuffer::toString() const {
  std::string os;

  if (auto p = getStreamInfo()) {
    os += p->toString();
  }

  if (isRepeating()) {
    os += " REPEAT";
  }

  if (auto s = getStatus()) {
    os += " ";
    os += bufferStatusToString(s);
  }

  return os;
}
