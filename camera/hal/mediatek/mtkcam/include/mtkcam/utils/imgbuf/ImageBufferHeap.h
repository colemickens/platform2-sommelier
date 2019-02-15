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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_IMGBUF_IMAGEBUFFERHEAP_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_IMGBUF_IMAGEBUFFERHEAP_H_
//
#include "IImageBuffer.h"
#include <memory>
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

struct PortBufInfo_v1 {
  MINT32 memID[3];
  MUINTPTR virtAddr[3];
  MINT32 nocache;
  MINT32 security;
  MINT32 coherence;
  MBOOL continuos;
  // for continuous memory
  PortBufInfo_v1(MINT32 const _memID,
                 MUINTPTR const _virtAddr,
                 MINT32 _nocache = 0,
                 MINT32 _security = 0,
                 MINT32 _coherence = 0)
      : nocache(_nocache),
        security(_security),
        coherence(_coherence),
        continuos(MTRUE) {
    memID[0] = _memID;
    virtAddr[0] = _virtAddr;
  }
  // for non-continuous memory
  PortBufInfo_v1(MINT32 const _memID[],
                 MUINTPTR const _virtAddr[],
                 MUINT32 const _planeCount,
                 MINT32 _nocache = 0,
                 MINT32 _security = 0,
                 MINT32 _coherence = 0)
      : nocache(_nocache),
        security(_security),
        coherence(_coherence),
        continuos(MFALSE) {
    for (MUINT32 i = 0; i < _planeCount; ++i) {
      memID[i] = _memID[i];
      virtAddr[i] = _virtAddr[i];
    }
  }
};

/******************************************************************************
 *  Image Buffer Heap (Camera1).
 ******************************************************************************/
class VISIBILITY_PUBLIC ImageBufferHeap : public virtual IImageBufferHeap {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Params for v1 Buffer
  typedef IImageBufferAllocator::ImgParam ImgParam_t;

 public:  ////                    Creation.
  static std::shared_ptr<ImageBufferHeap> create(
      char const* szCallerName,
      ImgParam_t const& rImgParam,
      PortBufInfo_v1 const& rPortBufInfo,
      MBOOL const enableLog = MTRUE);

 public:  ////                    Attributes.
  static char const* magicName() { return "Cam1Heap"; }
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_IMGBUF_IMAGEBUFFERHEAP_H_
