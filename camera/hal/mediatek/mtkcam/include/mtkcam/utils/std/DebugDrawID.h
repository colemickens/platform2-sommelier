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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_DEBUGDRAWID_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_DEBUGDRAWID_H_
#include <mtkcam/def/common.h>

class VISIBILITY_PUBLIC DebugDrawID {
 public:
  DebugDrawID(unsigned digit = 5,
              unsigned x = 200,
              unsigned y = 200,
              unsigned linePixel = 10,
              char bg = 250,
              char fg = 20);
  bool needDraw() const;
  void draw(unsigned num,
            char* buffer,
            unsigned width,
            unsigned height,
            unsigned stride,
            unsigned bufSize) const;
  void draw(unsigned num,
            unsigned digit,
            unsigned offsetX,
            unsigned offsetY,
            char* buffer,
            unsigned width,
            unsigned height,
            unsigned stride,
            unsigned bufSize) const;
  static void draw(unsigned num,
                   unsigned digit,
                   unsigned offsetX,
                   unsigned offsetY,
                   char* buffer,
                   unsigned width,
                   unsigned height,
                   unsigned stride,
                   unsigned bufSize,
                   unsigned linePixel,
                   char bg,
                   char fg);

 private:
  bool mNeedDraw;
  unsigned mDigit;
  unsigned mOffsetX;
  unsigned mOffsetY;
  unsigned mLinePixel;
  unsigned mBG;
  unsigned mFG;
};
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_DEBUGDRAWID_H_
