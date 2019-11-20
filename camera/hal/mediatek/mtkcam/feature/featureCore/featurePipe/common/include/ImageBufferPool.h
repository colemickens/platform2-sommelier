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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_IMAGEBUFFERPOOL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_IMAGEBUFFERPOOL_H_

#include "MtkHeader.h"

#include <memory>
#include <mutex>

#include "BufferHandle.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class ImageBufferPool;

class VISIBILITY_PUBLIC ImageBufferHandle
    : public BufferHandle<ImageBufferHandle> {
 public:
  ImageBufferHandle(
      const std::shared_ptr<BufferPool<ImageBufferHandle> >& pool);
  virtual ~ImageBufferHandle();

 public:
  std::shared_ptr<IImageBuffer> mImageBuffer;

 private:
  friend class ImageBufferPool;
  enum Type { ALLOCATE, REGISTER };
  Type mType;
  MINT mUsage;
};
typedef sb<ImageBufferHandle> SmartImageBuffer;

class VISIBILITY_PUBLIC ImageBufferPool : public BufferPool<ImageBufferHandle> {
 public:
  static const MUINT32 USAGE_HW;
  static const MUINT32 USAGE_SW;
  static const MUINT32 USAGE_HW_AND_SW;
  static const MBOOL SEPARATE_BUFFER;

 public:
  static std::shared_ptr<ImageBufferPool> create(const char* name,
                                                 MUINT32 width,
                                                 MUINT32 height,
                                                 EImageFormat format,
                                                 MUINT32 usage,
                                                 MBOOL singleBuffer = MFALSE);
  static std::shared_ptr<ImageBufferPool> create(const char* name,
                                                 const MSize& size,
                                                 EImageFormat format,
                                                 MUINT32 usage,
                                                 MBOOL singleBuffer = MFALSE);
  static MVOID destroy(std::shared_ptr<ImageBufferPool>* pool);
  virtual ~ImageBufferPool();

  virtual EImageFormat getImageFormat() const;
  virtual MSize getImageSize() const;
  virtual std::shared_ptr<IIBuffer> requestIIBuffer();
  MBOOL add(const std::shared_ptr<IImageBuffer>& image);

 public:
  explicit ImageBufferPool(const char* name);
  MBOOL init(MUINT32 width,
             MUINT32 height,
             EImageFormat format,
             MUINT32 usage,
             MBOOL singleBuffer);
  MVOID uninit();
  virtual std::shared_ptr<ImageBufferHandle> doAllocate();
  virtual MBOOL doRelease(std::shared_ptr<ImageBufferHandle> handle);

 private:
  MBOOL initConfig(MUINT32 width,
                   MUINT32 height,
                   EImageFormat format,
                   MUINT32 usage,
                   MBOOL singleBuffer);
  MBOOL initAllocatorParam();
  std::shared_ptr<IImageBuffer> createImageBuffer(
      const std::shared_ptr<IImageBufferHeap>& heap);

 private:
  std::mutex mMutex;
  MBOOL mReady;

 private:
  MUINT32 mWidth;
  MUINT32 mHeight;
  EImageFormat mFormat;
  MUINT32 mUsage;
  MUINT32 mPlane;
  size_t mStride[3];
  size_t mBoundary[3];
  MBOOL mUseSingleBuffer;
  MUINT32 mBufferSize;
  IImageBufferAllocator::ImgParam mAllocatorParam;
};

};      // namespace NSFeaturePipe
};      // namespace NSCamFeature
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_IMAGEBUFFERPOOL_H_
