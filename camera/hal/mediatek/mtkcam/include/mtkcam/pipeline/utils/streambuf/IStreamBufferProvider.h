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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_ISTREAMBUFFERPROVIDER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_ISTREAMBUFFERPROVIDER_H_

#include <memory>
#include <mtkcam/def/common.h>
#include <mtkcam/pipeline/stream/IStreamInfo.h>
#include "StreamBuffers.h"

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace Utils {

/**
 * @class IStreamBufferProvider
 *
 */
class IStreamBufferProvider
    : public virtual std::enable_shared_from_this<IStreamBufferProvider> {
 public:
  /**
   * deque StreamBuffer.
   *
   * @param[in] iRequestNo: the request number for using this StreamBuffer.
   *                   The callee might implement sync mechanism.
   *
   * @param[in] pStreamInfo: a non-null strong pointer for describing the
                        properties of image stream.
   *
   * @param[out] rpStreamBuffer: a reference to a newly created StreamBuffer.
   *                     The callee must promise its value by deque/allocate
   *                     image buffer heap for generating StreamBuffer.
   *
   * @return 0 indicates success; non-zero indicates an error code.
   */
  virtual MERROR dequeStreamBuffer(
      MUINT32 const iRequestNo,
      std::shared_ptr<IImageStreamInfo> const pStreamInfo,
      std::shared_ptr<HalImageStreamBuffer> const& rpStreamBuffer) = 0;

  /**
   * enque StreamBuffer.
   *
   * @param[in] pStreamInfo: a non-null strong pointer for describing the
                        properties of image stream.
   *
   * @param[in] rpStreamBuffer: a StreamBuffer to be destroyed.
   *                     The callee must enque/destroy the appended image buffer
   heap.
   *
   * @param[in] bBufStatus: the status of StreamBuffer.
   *
   * @return 0 indicates success; non-zero indicates an error code.
   */
  virtual MERROR enqueStreamBuffer(
      std::shared_ptr<IImageStreamInfo> const pStreamInfo,
      std::shared_ptr<HalImageStreamBuffer> rpStreamBuffer,
      MUINT32 bBufStatus) = 0;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace Utils
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_ISTREAMBUFFERPROVIDER_H_
