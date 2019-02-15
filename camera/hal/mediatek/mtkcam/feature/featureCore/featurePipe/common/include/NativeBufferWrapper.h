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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_NATIVEBUFFERWRAPPER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_NATIVEBUFFERWRAPPER_H_

#define SUPPORT_AHARDWAREBUFFER 1
#include <memory>
#include <string>

#if SUPPORT_AHARDWAREBUFFER
#include <vndk/hardware_buffer.h>
#else
#include <ui/GraphicBuffer.h>
#endif

#include "MtkHeader.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

#if SUPPORT_AHARDWAREBUFFER
typedef AHardwareBuffer* NB_PTR;
typedef NB_PTR NB_SPTR;
typedef AHardwareBuffer NativeBuffer;
#else
typedef std::shared_ptr<android::GraphicBuffer> NB_PTR;
typedef NB_PTR* NB_SPTR;
typedef android::GraphicBuffer NativeBuffer;
#endif  // SUPPORT_AHARDWAREBUFFER

class NativeBufferWrapper {
 public:
  enum ColorSpace {
    NOT_SET,
    YUV_BT601_NARROW,
    YUV_BT601_FULL,
    YUV_BT709_NARROW,
    YUV_BT709_FULL,
    YUV_BT2020_NARROW,
    YUV_BT2020_FULL,
  };

  static const MUINT64 USAGE_HW_TEXTURE;
  static const MUINT64 USAGE_HW_RENDER;
  static const MUINT64 USAGE_SW;

  explicit NativeBufferWrapper(NB_PTR buffer,
                               const std::string& name = "Unknown");
  NativeBufferWrapper(MUINT32 width,
                      MUINT32 height,
                      android_pixel_format_t format,
                      MUINT64 usage,
                      const std::string& name = "Unknown");
  virtual ~NativeBufferWrapper();

 public:
  buffer_handle_t getHandle();
  NB_PTR getBuffer();
  NB_SPTR getBuffer_SPTR();
  MBOOL lock(MUINT64 usage, void** vaddr);
  MBOOL unlock();
  MBOOL setGrallocExtraParam(ColorSpace color);
#if SUPPORT_AHARDWAREBUFFER
  MUINT32 toNativeFormat(android_pixel_format_t format);
#endif
  MUINT32 toGrallocExtraColor(ColorSpace color);

 private:
  MBOOL allocate(MUINT32 width,
                 MUINT32 height,
                 android_pixel_format_t format,
                 MUINT64 usage);

 private:
  const std::string mName;
  NB_PTR mBuffer;
};

NB_PTR getNativeBuffer(NB_SPTR buffer);
NativeBuffer* getNativeBufferPtr(NB_SPTR buffer);
MBOOL lockNativeBuffer(NB_SPTR buffer, MUINT64 usage, void** vaddr);
MBOOL unlockNativeBuffer(NB_SPTR buffer);

};      // namespace NSFeaturePipe
};      // namespace NSCamFeature
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_NATIVEBUFFERWRAPPER_H_
