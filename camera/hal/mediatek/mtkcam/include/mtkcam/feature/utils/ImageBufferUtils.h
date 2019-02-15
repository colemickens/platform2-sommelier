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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_IMAGEBUFFERUTILS_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_IMAGEBUFFERUTILS_H_

#include <inttypes.h>
#include <map>
#include <memory>
#include <mtkcam/def/BuiltinTypes.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>
#include <mutex>
#include <unordered_map>

// ---------------------------------------------------------------------------

namespace NSCam {

// ---------------------------------------------------------------------------

class ImageBufferUtils {
 public:
  // allocBuffer() is the utility function that
  // 1. allocate ION buffer and map it to ImageBuffer
  // 2. locks the ImageBuffer
  //
  // NOTE: all buffers allocated from this API should be returned by
  // calling deallocBuffer()
  MBOOL allocBuffer(std::shared_ptr<IImageBuffer>* imageBuf,
                    MUINT32 w,
                    MUINT32 h,
                    MUINT32 format,
                    MBOOL isContinuous = MTRUE);

  // deallocBuffer() is the utility function that
  // 1. unlock the ImageBuffer
  void deallocBuffer(IImageBuffer* pBuf);
  void deallocBuffer(std::shared_ptr<IImageBuffer>* pBuf);

  // createBufferAlias() is the utility function that
  // 1. creates an ImageBuffer from the original ImageBuffer.
  // ('alias' means that their content are the same,
  //  excepts the alignment and the format of each ImageBuffer)
  // 2. unlocks the original ImageBuffer
  // 3. locks and returns the new ImageBuffer
  std::shared_ptr<IImageBuffer> createBufferAlias(
      std::shared_ptr<IImageBuffer> pOriginalBuf,
      MUINT32 w,
      MUINT32 h,
      EImageFormat format);

  // removeBufferAlias() is the utility function that
  // 1. unlocks and destroys the new ImageBuffer
  // 2. locks the original ImageBuffer
  MBOOL removeBufferAlias(std::shared_ptr<IImageBuffer> pOriginalBuf,
                          std::shared_ptr<IImageBuffer> pAliasBuf);

  // check later
  MBOOL createBuffer(std::shared_ptr<IImageBuffer>* output_buf,
                     std::shared_ptr<IImageBuffer>* input_buf);

 private:
  // mInternalBufferList is use for internal memory management
  mutable std::mutex mInternalBufferListLock;
  std::map<MINTPTR, std::shared_ptr<IImageBuffer> > mInternalBufferList;
};

// ---------------------------------------------------------------------------

};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_IMAGEBUFFERUTILS_H_
