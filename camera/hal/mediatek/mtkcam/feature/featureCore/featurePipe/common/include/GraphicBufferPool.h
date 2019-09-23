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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_GRAPHICBUFFERPOOL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_GRAPHICBUFFERPOOL_H_

#include "MtkHeader.h"

#include <memory>
#include <mutex>

#include "BufferPool.h"
#include "IIBuffer.h"
#include "NativeBufferWrapper.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class GraphicBufferPool;

class GraphicBufferHandle : public BufferHandle<GraphicBufferHandle> {
 public:
  GraphicBufferHandle(
      const std::shared_ptr<BufferPool<GraphicBufferHandle> >& pool);
  virtual ~GraphicBufferHandle();
  NB_SPTR getGraphicBufferAddr();

 public:
  std::shared_ptr<IImageBuffer> mImageBuffer;
  std::shared_ptr<NativeBufferWrapper> mGraphicBuffer;

 private:
  enum Type { ALLOCATE, REGISTER };
  friend class GraphicBufferPool;
  Type mType;
};
typedef sb<GraphicBufferHandle> SmartGraphicBuffer;

class GraphicBufferPool : public BufferPool<GraphicBufferHandle> {
 public:
  static const MUINT32 USAGE_HW_TEXTURE;
  static const MUINT32 USAGE_HW_RENDER;

 public:
  class BufferInfo {
   public:
    std::shared_ptr<NativeBufferWrapper> mGraphic;
    std::shared_ptr<IImageBuffer> mImage;
    MUINT32 mSize;

    BufferInfo() {}

    BufferInfo(std::shared_ptr<NativeBufferWrapper> graphic,
               std::shared_ptr<IImageBuffer> image,
               MUINT32 size)
        : mGraphic(graphic), mImage(image), mSize(size) {}
  };

 public:
  static std::shared_ptr<GraphicBufferPool> create(
      const char* name,
      MUINT32 width,
      MUINT32 height,
      android_pixel_format_t format,
      MUINT32 usage,
      NativeBufferWrapper::ColorSpace color = NativeBufferWrapper::NOT_SET);
  static MVOID destroy(std::shared_ptr<GraphicBufferPool>* pool);
  virtual ~GraphicBufferPool();

  virtual EImageFormat getImageFormat() const;
  virtual MSize getImageSize() const;

  virtual std::shared_ptr<IIBuffer> requestIIBuffer();

 protected:
  explicit GraphicBufferPool(const char* name);
  MBOOL init(MUINT32 width,
             MUINT32 height,
             android_pixel_format_t format,
             MUINT32 usage,
             NativeBufferWrapper::ColorSpace color);
  MVOID uninit();
  virtual std::shared_ptr<GraphicBufferHandle> doAllocate();
  virtual MBOOL doRelease(GraphicBufferHandle* handle);

 private:
  MBOOL initConfig(MUINT32 width,
                   MUINT32 height,
                   android_pixel_format_t format,
                   MUINT32 usage,
                   NativeBufferWrapper::ColorSpace color);
  EImageFormat toImageFormat(android_pixel_format_t graphFormat);

 private:
  std::mutex mMutex;
  MBOOL mReady;

 private:
  MUINT32 mWidth;
  MUINT32 mHeight;
  EImageFormat mImageFormat;
  android_pixel_format_t mGraphicFormat;
  MUINT32 mImageUsage;
  MUINT32 mGraphicUsage;
  MUINT32 mPlane;
  size_t mStride[3];
  size_t mBoundary[3];
  NativeBufferWrapper::ColorSpace mColorSpace;
  IImageBufferAllocator::ImgParam mAllocatorParam;
};

};      // namespace NSFeaturePipe
};      // namespace NSCamFeature
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_GRAPHICBUFFERPOOL_H_
