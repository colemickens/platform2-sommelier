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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_P2IO_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_P2IO_H_

#include <mtkcam/def/common.h>
#include <mtkcam/drv/iopipe/INormalStream.h>
#include <memory>

namespace NSCam {
namespace Feature {
namespace P2Util {

class P2Flag {
 public:
  enum {
    FLAG_NONE = 0,
    FLAG_RESIZED = 1 << 1,
    FLAG_LMV = 1 << 2,
  };
};

class P2IO {
 public:
  P2IO() {}
  P2IO(IImageBuffer* buf, MINT32 trans, NSIoPipe::EPortCapbility cap)
      : mBuffer(buf), mTransform(trans), mCapability(cap) {}
  MBOOL isValid() const { return mBuffer != NULL; }
  MSize getImgSize() const {
    return mBuffer ? mBuffer->getImgSize() : MSize(0, 0);
  }
  MSize getTransformSize() const {
    MSize size(0, 0);
    if (mBuffer) {
      size = mBuffer->getImgSize();
      if (mTransform & eTransform_ROT_90) {
        size = MSize(size.h, size.w);
      }
    }
    return size;
  }

 public:
  IImageBuffer* mBuffer = nullptr;
  MINT32 mTransform = 0;
  NSIoPipe::EPortCapbility mCapability = NSIoPipe::EPortCapbility_None;
};

class P2IOPack {
 public:
  MBOOL isResized() const { return mFlag & P2Flag::FLAG_RESIZED; }
  MBOOL useLMV() const { return mFlag & P2Flag::FLAG_LMV; }

 public:
  MUINT32 mFlag = P2Flag::FLAG_NONE;

  P2IO mIMGI;

  P2IO mIMG2O;
  P2IO mWDMAO;
  P2IO mWROTO;

  P2IO mLCSO;
  P2IO mTuning;
};

}  // namespace P2Util
}  // namespace Feature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_P2IO_H_
