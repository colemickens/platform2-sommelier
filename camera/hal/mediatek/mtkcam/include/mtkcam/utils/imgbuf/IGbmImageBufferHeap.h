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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_IMGBUF_IGBMIMAGEBUFFERHEAP_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_IMGBUF_IGBMIMAGEBUFFERHEAP_H_
//
#include "IImageBuffer.h"
#include <mtkcam/def/common.h>

#include <memory>
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/******************************************************************************
 *  Image Buffer Heap (Gbm).
 ******************************************************************************/
class VISIBILITY_PUBLIC IGbmImageBufferHeap : public virtual IImageBufferHeap {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Params for Allocations.
  typedef IImageBufferAllocator::ImgParam AllocImgParam_t;

 public:  ////                    Creation.
  static std::shared_ptr<IGbmImageBufferHeap> create(
      char const* szCallerName,
      AllocImgParam_t const& rImgParam,
      MBOOL const enableLog = MTRUE);

 protected:  ////                    Destructor/Constructors.
  /**
   * Disallowed to directly delete a raw pointer.
   */
  virtual ~IGbmImageBufferHeap() {}

 public:  ////                    Attributes.
  static char const* magicName() { return "GbmHeap"; }

  virtual void* getHWBuffer() = 0;
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_IMGBUF_IGBMIMAGEBUFFERHEAP_H_
