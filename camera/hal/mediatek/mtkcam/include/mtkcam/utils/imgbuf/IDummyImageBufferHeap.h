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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_IMGBUF_IDUMMYIMAGEBUFFERHEAP_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_IMGBUF_IDUMMYIMAGEBUFFERHEAP_H_
//
#include "IImageBuffer.h"

#include <memory>

namespace NSCam {

struct PortBufInfo_dummy {
  MINT32 memID;
  MUINTPTR virtAddr[3];
  MUINTPTR phyAddr[3];
  MINT32 nocache;
  MINT32 security;
  MINT32 coherence;

  PortBufInfo_dummy(MINT32 const _memID,
                    MUINTPTR const _virtAddr[],
                    MUINTPTR const _phyAddr[],
                    MUINT32 const _planeCount,
                    MINT32 _nocache = 0,
                    MINT32 _security = 0,
                    MINT32 _coherence = 0)
      : memID(_memID),
        nocache(_nocache),
        security(_security),
        coherence(_coherence) {
    for (MUINT32 i = 0; i < _planeCount; ++i) {
      virtAddr[i] = _virtAddr[i];
      phyAddr[i] = _phyAddr[i];
    }
  }
};

/******************************************************************************
 *  Image Buffer Heap (Dummy).
 ******************************************************************************/
class VISIBILITY_PUBLIC IDummyImageBufferHeap
    : public virtual IImageBufferHeap {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Params for Allocations.
  typedef IImageBufferAllocator::ImgParam ImgParam_t;

 public:  ////                    Creation.
  static std::shared_ptr<IDummyImageBufferHeap> create(
      char const* szCallerName,
      ImgParam_t const& rImgParam,
      PortBufInfo_dummy const& rPortBufInfo,
      MBOOL const enableLog = MTRUE);

  virtual ~IDummyImageBufferHeap() {}

 public:  ////                    Attributes.
  static char const* magicName() { return "DummyHeap"; }
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_IMGBUF_IDUMMYIMAGEBUFFERHEAP_H_
