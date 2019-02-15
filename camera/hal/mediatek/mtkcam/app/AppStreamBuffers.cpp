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

#undef LOG_TAG
#define LOG_TAG "MtkCam/streambuf"
//
#include "MyUtils.h"
#include "camera/hal/mediatek/mtkcam/app/AppStreamBuffers.h"

#include <memory>
#include <string>

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::AppImageStreamBuffer::Allocator::Allocator(
    std::shared_ptr<IStreamInfoT> pStreamInfo)
    : mpStreamInfo(pStreamInfo) {}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::v3::AppImageStreamBuffer::StreamBufferT>
NSCam::v3::AppImageStreamBuffer::Allocator::operator()(
    std::shared_ptr<IGraphicImageBufferHeap> pHeap,
    std::shared_ptr<IStreamInfoT> pStreamInfo) {
  if (!pHeap) {
    MY_LOGE("NULL IGraphicImageBufferHeap: %s", mpStreamInfo->getStreamName());
    return nullptr;
  }
  //
  auto ptr = std::make_shared<StreamBufferT>(
      pStreamInfo ? pStreamInfo : mpStreamInfo, pHeap);
  return ptr;
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::AppImageStreamBuffer::AppImageStreamBuffer(
    std::shared_ptr<IStreamInfoT> pStreamInfo,
    std::shared_ptr<IGraphicImageBufferHeap> pImageBufferHeap,
    std::shared_ptr<IUsersManager> pUsersManager)
    : TStreamBufferT(pStreamInfo, pImageBufferHeap.get(), pUsersManager),
      mImageBufferHeap(pImageBufferHeap)
//
{}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::IGraphicImageBufferHeap>
NSCam::v3::AppImageStreamBuffer::getImageBufferHeap() const {
  std::lock_guard<std::timed_mutex> _l(mBufMutex);
  //
  return mImageBufferHeap;
}

/******************************************************************************
 *
 ******************************************************************************/
MINT NSCam::v3::AppImageStreamBuffer::getAcquireFence() const {
  std::lock_guard<std::timed_mutex> _l(mBufMutex);
  //
  if (mImageBufferHeap != nullptr)
    return mImageBufferHeap->getAcquireFence();
  return -1;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::AppImageStreamBuffer::setAcquireFence(MINT fence) {
  std::lock_guard<std::timed_mutex> _l(mBufMutex);
  //
  if (mImageBufferHeap != nullptr)
    mImageBufferHeap->setAcquireFence(fence);
}

/******************************************************************************
 *
 ******************************************************************************/
MINT NSCam::v3::AppImageStreamBuffer::getReleaseFence() const {
  std::lock_guard<std::timed_mutex> _l(mBufMutex);
  //
  if (mImageBufferHeap != nullptr)
    return mImageBufferHeap->getReleaseFence();
  return -1;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::AppImageStreamBuffer::setReleaseFence(MINT fence) {
  std::lock_guard<std::timed_mutex> _l(mBufMutex);
  //
  if (mImageBufferHeap != nullptr)
    mImageBufferHeap->setReleaseFence(fence);
}

/******************************************************************************
 *
 ******************************************************************************/
ssize_t NSCam::v3::AppImageStreamBuffer::enqueUserGraph(
    std::shared_ptr<IUserGraph> pUserGraph) {
  if (pUserGraph == nullptr) {
    MY_LOGE("enqueUserGraph fail");
    return -1;
  }
  ssize_t const groupIndex = mUsersManager->enqueUserGraph(pUserGraph);
  if (0 != groupIndex) {
    return groupIndex;
  }
  //
  MBOOL found = MFALSE;
  std::shared_ptr<IUserGraph> const& pMyUserGraph = pUserGraph;
  for (size_t i = 0; i < pMyUserGraph->size(); i++) {
    if (IUsersManager::Category::NONE == pMyUserGraph->getCategory(i)) {
      continue;
    }
    MY_LOGD("Node:%d is a (%d: producer or consumer)!", i,
            pMyUserGraph->getCategory(i));
    //
    MERROR err = OK;
    if (!found) {
      found = MTRUE;
      MY_LOGD("0-indegree user:%d streamid(%#" PRIx64 ") set acquire fence:%d",
              i, this->getStreamInfo()->getStreamId(), getAcquireFence());
      err = pMyUserGraph->setAcquireFence(i, getAcquireFence());
    } else {
      MINT fence = -1;
      if (getAcquireFence() > 0) {
        fence = ::dup(getAcquireFence());
        MY_LOGW("another 0-indegree user:%d; need dup acquire fence:%d->%d", i,
                getAcquireFence(), fence);
        if (fence < 0) {
          MY_LOGE("dup Fence fail");
          return -1;
        }
      }
      err = pMyUserGraph->setAcquireFence(i, fence);
    }
    //
    if (OK != err) {
      MY_LOGE("Fail to setAcquireFence(%zu, %d)", i, getAcquireFence());
    }
  }
  return groupIndex;
}

/******************************************************************************
 *
 ******************************************************************************/
std::string NSCam::v3::AppImageStreamBuffer::toString() const {
  std::string os;

  if (auto p = getStreamInfo()) {
    os += p->toString();
  }

  if (auto s = getStatus()) {
    os += base::StringPrintf(" status:%#x", s);
  }

  return os;
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::AppMetaStreamBuffer::Allocator::Allocator(
    std::shared_ptr<IStreamInfoT> pStreamInfo)
    : mpStreamInfo(pStreamInfo) {}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::v3::AppMetaStreamBuffer::StreamBufferT>
NSCam::v3::AppMetaStreamBuffer::Allocator::operator()() {
  auto ptr = std::make_shared<StreamBufferT>(mpStreamInfo);
  return ptr;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::v3::AppMetaStreamBuffer::StreamBufferT>
NSCam::v3::AppMetaStreamBuffer::Allocator::operator()(
    NSCam::IMetadata const& metadata) {
  auto ptr = std::make_shared<StreamBufferT>(mpStreamInfo, metadata);
  return ptr;
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::AppMetaStreamBuffer::AppMetaStreamBuffer(
    std::shared_ptr<IStreamInfoT> pStreamInfo,
    std::shared_ptr<IUsersManager> pUsersManager)
    : TStreamBufferT(pStreamInfo, &mMetadata, pUsersManager),
      mMetadata(),
      mRepeating(MFALSE)
//
{}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::AppMetaStreamBuffer::AppMetaStreamBuffer(
    std::shared_ptr<IStreamInfoT> pStreamInfo,
    NSCam::IMetadata const& metadata,
    std::shared_ptr<IUsersManager> pUsersManager)
    : TStreamBufferT(pStreamInfo, &mMetadata, pUsersManager),
      mMetadata(metadata),
      mRepeating(MFALSE)
//
{}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::AppMetaStreamBuffer::setRepeating(MBOOL const repeating) {
  std::lock_guard<std::timed_mutex> _l(mRepeatMutex);
  mRepeating = repeating;
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
NSCam::v3::AppMetaStreamBuffer::isRepeating() const {
  std::lock_guard<std::timed_mutex> _l(mRepeatMutex);
  return mRepeating;
}

/******************************************************************************
 *
 ******************************************************************************/
std::string NSCam::v3::AppMetaStreamBuffer::toString() const {
  std::string os;

  if (auto p = getStreamInfo()) {
    os += p->toString();
  }

  if (isRepeating()) {
    os += " REPEAT";
  }

  if (auto s = getStatus()) {
    os += base::StringPrintf(" status:%#x", s);
  }

  return os;
}
