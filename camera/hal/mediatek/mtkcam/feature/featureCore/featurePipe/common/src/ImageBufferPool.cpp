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

#include "../include/DebugControl.h"
#include "../include/MtkHeader.h"
#define PIPE_TRACE TRACE_IMAGE_BUFFER_POOL
#define PIPE_CLASS_TAG "ImageBufferPool"
#include "../include/PipeLog.h"

#include "../include/BufferPool.h"
#include "../include/ImageBufferPool.h"
#include <memory>

// use one single contiguous memory for all buffer planes
#define USE_SINGLE_BUFFER MTRUE

// using namespace NSCam::Utils::Format;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

const MUINT32 ImageBufferPool::USAGE_HW = eBUFFER_USAGE_HW_CAMERA_READWRITE |
                                          eBUFFER_USAGE_SW_READ_OFTEN |
                                          eBUFFER_USAGE_SW_WRITE_RARELY;
const MUINT32 ImageBufferPool::USAGE_SW =
    eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_OFTEN;
const MUINT32 ImageBufferPool::USAGE_HW_AND_SW =
    eBUFFER_USAGE_SW_READ_OFTEN | eBUFFER_USAGE_SW_WRITE_OFTEN |
    eBUFFER_USAGE_HW_CAMERA_READWRITE;
const MBOOL ImageBufferPool::SEPARATE_BUFFER = MFALSE;

static MUINT32 queryPlanePixel(MUINT32 fmt,
                               MUINT32 i,
                               MUINT32 width,
                               MUINT32 height) {
  TRACE_FUNC_ENTER();
  MUINT32 pixel;
  pixel = (NSCam::Utils::Format::queryPlaneWidthInPixels(fmt, i, width) *
           NSCam::Utils::Format::queryPlaneBitsPerPixel(fmt, i) / 8) *
          NSCam::Utils::Format::queryPlaneHeightInPixels(fmt, i, height);
  TRACE_FUNC_EXIT();
  return pixel;
}

static MUINT32 queryStrideInPixels(MUINT32 fmt, MUINT32 i, MUINT32 width) {
  TRACE_FUNC_ENTER();
  MUINT32 pixel;
  pixel = NSCam::Utils::Format::queryPlaneWidthInPixels(fmt, i, width) *
          NSCam::Utils::Format::queryPlaneBitsPerPixel(fmt, i) / 8;
  TRACE_FUNC_EXIT();
  return pixel;
}

ImageBufferHandle::ImageBufferHandle(
    const std::shared_ptr<BufferPool<ImageBufferHandle> >& pool)
    : BufferHandle(pool), mType(ImageBufferHandle::ALLOCATE), mUsage(0) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

ImageBufferHandle::~ImageBufferHandle() {}

std::shared_ptr<ImageBufferPool> ImageBufferPool::create(const char* name,
                                                         MUINT32 width,
                                                         MUINT32 height,
                                                         EImageFormat format,
                                                         MUINT32 usage,
                                                         MBOOL singleBuffer) {
  TRACE_FUNC_ENTER();
  auto pool = std::make_shared<ImageBufferPool>(name);
  if (pool == nullptr) {
    MY_LOGE("OOM: %s: Cannot create ImageBufferPool", name);
  } else if (!pool->init(width, height, format, usage, singleBuffer)) {
    MY_LOGE("%s: ImageBufferPool init failed", name);
    pool = nullptr;
  }
  TRACE_FUNC_EXIT();
  return pool;
}

std::shared_ptr<ImageBufferPool> ImageBufferPool::create(const char* name,
                                                         const MSize& size,
                                                         EImageFormat format,
                                                         MUINT32 usage,
                                                         MBOOL singleBuffer) {
  TRACE_FUNC_ENTER();
  auto pool = ImageBufferPool::create(name, size.w, size.h, format, usage,
                                      singleBuffer);
  TRACE_FUNC_EXIT();
  return pool;
}

MVOID ImageBufferPool::destroy(std::shared_ptr<ImageBufferPool>* pool) {
  TRACE_FUNC_ENTER();
  if (*pool != NULL) {
    (*pool)->releaseAll();
  }
  TRACE_FUNC_EXIT();
}

ImageBufferPool::ImageBufferPool(const char* name)
    : BufferPool<ImageBufferHandle>(name),
      mReady(MFALSE),
      mWidth(0),
      mHeight(0),
      mFormat(eImgFmt_YV12),
      mUsage(0),
      mPlane(0),
      mUseSingleBuffer(MTRUE),
      mBufferSize(0),
      mAllocatorParam(0, 0) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

ImageBufferPool::~ImageBufferPool() {
  TRACE_FUNC_ENTER();
  uninit();
  TRACE_FUNC_EXIT();
}

EImageFormat ImageBufferPool::getImageFormat() const {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return mFormat;
}

MSize ImageBufferPool::getImageSize() const {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return MSize(mWidth, mHeight);
}

MBOOL ImageBufferPool::init(MUINT32 width,
                            MUINT32 height,
                            EImageFormat format,
                            MUINT32 usage,
                            MBOOL singleBuffer) {
  TRACE_FUNC_ENTER();

  std::lock_guard<std::mutex> lock(mMutex);
  MBOOL ret = MFALSE;

  if (mReady) {
    MY_LOGE("%s: Already init", mName);
  } else if (initConfig(width, height, format, usage, singleBuffer)) {
    mReady = MTRUE;
    ret = MTRUE;
  }

  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL ImageBufferPool::initConfig(MUINT32 width,
                                  MUINT32 height,
                                  EImageFormat format,
                                  MUINT32 usage,
                                  MBOOL singleBuffer) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  if (!usage) {
    MY_LOGE("%s: Should specify image buffer usage", mName);
  } else if (!width || !height) {
    MY_LOGE("%s: Erronuous dimension (%dx%d)", mName, width, height);
  } else {
    MY_LOGD("%s: %dx%d, fmt(0x%x)", mName, width, height, format);
    mWidth = width;
    mHeight = height;
    mFormat = format;
    mUsage = usage;
    mPlane = NSCam::Utils::Format::queryPlaneCount(format);

    if (mPlane > 3) {
      MY_LOGE("%s: plane counter larger than 3, not supported", mName);
    } else {
      memset(mStride, 0, sizeof(mStride));
      memset(mBoundary, 0, sizeof(mBoundary));
      mBufferSize = 0;
      for (unsigned i = 0; i < mPlane; ++i) {
        mStride[i] = queryStrideInPixels(mFormat, i, mWidth);
        mBufferSize += queryPlanePixel(mFormat, i, mWidth, mHeight);
      }
      mUseSingleBuffer = singleBuffer;
      ret = initAllocatorParam();
    }
  }

  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL ImageBufferPool::initAllocatorParam() {
  TRACE_FUNC_ENTER();
  if (mUseSingleBuffer) {
    mAllocatorParam = IImageBufferAllocator::ImgParam(mBufferSize, 0);
  } else {
    mAllocatorParam = IImageBufferAllocator::ImgParam(
        mFormat, MSize(mWidth, mHeight), mStride, mBoundary, mPlane);
  }
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MVOID ImageBufferPool::uninit() {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mMutex);
  if (mReady) {
    this->releaseAll();
    mReady = MFALSE;
  }
  TRACE_FUNC_EXIT();
}

MBOOL ImageBufferPool::add(const std::shared_ptr<IImageBuffer>& image) {
  TRACE_FUNC_ENTER();

  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> lock(mMutex);
  std::shared_ptr<ImageBufferHandle> handle =
      std::make_shared<ImageBufferHandle>(this->shared_from_this());

  if (!mReady) {
    MY_LOGE("%s: pool need init first", mName);
  } else if (image == nullptr) {
    MY_LOGE("%s: invalid buffer handle", mName);
  } else if (handle == nullptr) {
    MY_LOGE("OOM: %s: create bufferHandle failed", mName);
  } else {
    handle->mImageBuffer = image;
    handle->mType = ImageBufferHandle::REGISTER;
    addToPool(handle);
    ret = MTRUE;
  }

  TRACE_FUNC_EXIT();
  return ret;
}

std::shared_ptr<ImageBufferHandle> ImageBufferPool::doAllocate() {
  TRACE_FUNC_ENTER();

  std::lock_guard<std::mutex> lock(mMutex);

  std::shared_ptr<ImageBufferHandle> bufferHandle =
      std::make_shared<ImageBufferHandle>(this->shared_from_this());
  std::shared_ptr<IImageBufferHeap> heap;

  if (!mReady) {
    MY_LOGE("%s: pool need init first", mName);
    return nullptr;
  }

  if (bufferHandle == nullptr) {
    MY_LOGE("OOM: %s: create bufferHandle failed", mName);
    return nullptr;
  }

  heap = IGbmImageBufferHeap::create(mName, mAllocatorParam, NULL);
  if (heap == nullptr) {
    MY_LOGE("%s: IIonImageBufferHeap create failed", mName);
    return MFALSE;
  }

  bufferHandle->mImageBuffer = createImageBuffer(heap);
  if (bufferHandle->mImageBuffer == nullptr) {
    MY_LOGE("%s: heap->createImageBuffer failed", mName);
    return nullptr;
  }

  if (!bufferHandle->mImageBuffer->lockBuf(mName, mUsage)) {
    MY_LOGE("%s: mImageBuffer->lockBuf failed", mName);
    return nullptr;
  }

  bufferHandle->mUsage = mUsage;
  bufferHandle->mType = ImageBufferHandle::ALLOCATE;

  TRACE_FUNC_EXIT();
  return bufferHandle;
}

std::shared_ptr<IImageBuffer> ImageBufferPool::createImageBuffer(
    const std::shared_ptr<IImageBufferHeap>& heap) {
  TRACE_FUNC_ENTER();
  std::shared_ptr<IImageBuffer> imageBuffer = nullptr;
  if (heap != nullptr) {
    if (mUseSingleBuffer) {
      imageBuffer = heap->createImageBuffer_FromBlobHeap(
          0, mFormat, MSize(mWidth, mHeight), mStride);
    } else {
      imageBuffer = heap->createImageBuffer();
    }
  }
  TRACE_FUNC_EXIT();
  return imageBuffer;
}

MBOOL ImageBufferPool::doRelease(std::shared_ptr<ImageBufferHandle> handle) {
  TRACE_FUNC_ENTER();

  // release should not need lock(mMutex)
  // becuare only BufferPool::releaseAll and
  // BufferPool::recycle calls release for handles for the pool,
  // and no handle can exist when IMemDrv is invalid

  MBOOL ret = MTRUE;

  if (!handle) {
    MY_LOGE("%s: ImageBufferHandle missing", mName);
    ret = MFALSE;
  } else {
    if (handle->mImageBuffer == nullptr) {
      MY_LOGE("%s: ImageBufferHandle::mImageBuffer missing", mName);
      ret = MFALSE;
    } else if (handle->mType == ImageBufferHandle::ALLOCATE) {
      if (!handle->mImageBuffer->unlockBuf(mName)) {
        MY_LOGE("%s: ImageBufferHandle unlockBuf failed", mName);
        ret = MFALSE;
      }
    }
  }

  TRACE_FUNC_EXIT();
  return ret;
}

class IIBuffer_ImageBufferHandle : public IIBuffer {
 public:
  explicit IIBuffer_ImageBufferHandle(sb<ImageBufferHandle> handle)
      : mHandle(handle) {}

  virtual ~IIBuffer_ImageBufferHandle() {}

  virtual std::shared_ptr<IImageBuffer> getImageBuffer() const {
    std::shared_ptr<IImageBuffer> buffer;
    if (mHandle.get() != nullptr) {
      buffer = mHandle->mImageBuffer;
    }
    return buffer;
  }

  virtual std::shared_ptr<IImageBuffer> operator->() const {
    std::shared_ptr<IImageBuffer> buffer;
    if (mHandle.get() != nullptr) {
      buffer = mHandle->mImageBuffer;
    }
    return buffer;
  }

 private:
  sb<ImageBufferHandle> mHandle;
};

std::shared_ptr<IIBuffer> ImageBufferPool::requestIIBuffer() {
  TRACE_FUNC_ENTER();
  sb<ImageBufferHandle> handle;
  std::shared_ptr<IIBuffer> buffer;
  handle = this->request();
  buffer = std::make_shared<IIBuffer_ImageBufferHandle>(handle);
  TRACE_FUNC_EXIT();
  return buffer;
}

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam
