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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_IMGBUF_IGRAPHICIMAGEBUFFERHEAP_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_IMGBUF_IGRAPHICIMAGEBUFFERHEAP_H_

#include <memory>
//
#include <hardware/camera3.h>
#include "IImageBuffer.h"
#include <mtkcam/def/common.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/******************************************************************************
 *  Image Buffer Heap.
 ******************************************************************************/
class VISIBILITY_PUBLIC IGraphicImageBufferHeap
    : public virtual IImageBufferHeap {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Creation.
  static std::shared_ptr<IGraphicImageBufferHeap> create(
      char const* szCallerName, camera3_stream_buffer const* stream_buffer);

 protected:  ////                    Destructor/Constructors.
  /**
   * Disallowed to directly delete a raw pointer.
   */
  virtual ~IGraphicImageBufferHeap() {}

 public:  ////                    Attributes.
  static char const* magicName() { return "IGraphicImageBufferHeap"; }

 public:  ////                    Accessors.
  virtual buffer_handle_t* getBufferHandlePtr() const = 0;
  virtual MINT getAcquireFence() const = 0;
  virtual MVOID setAcquireFence(MINT fence) = 0;
  virtual MINT getReleaseFence() const = 0;
  virtual MVOID setReleaseFence(MINT fence) = 0;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_IMGBUF_IGRAPHICIMAGEBUFFERHEAP_H_
