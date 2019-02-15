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

#include "../include/IIBuffer.h"

#include "../include/DebugControl.h"
#define PIPE_TRACE TRACE_IIBUFFER
#define PIPE_CLASS_TAG "IIBuffer"
#include "../include/PipeLog.h"

#include <memory>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

IIBuffer::IIBuffer() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

IIBuffer::~IIBuffer() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

IImageBuffer* IIBuffer::getImageBufferPtr() const {
  TRACE_FUNC_ENTER();
  IImageBuffer* ptr = nullptr;
  ptr = this->getImageBuffer().get();
  TRACE_FUNC_EXIT();
  return ptr;
}

MBOOL IIBuffer::syncCache(NSCam::eCacheCtrl ctrl) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  std::shared_ptr<IImageBuffer> buffer = this->getImageBuffer();
  if (buffer != NULL) {
    ret = buffer->syncCache(ctrl);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

IIBuffer_IImageBuffer::IIBuffer_IImageBuffer(IImageBuffer* buffer) {
  mBuffer.reset(buffer);
}

IIBuffer_IImageBuffer::~IIBuffer_IImageBuffer() {}

std::shared_ptr<IImageBuffer> IIBuffer_IImageBuffer::getImageBuffer() const {
  return mBuffer;
}

std::shared_ptr<IImageBuffer> IIBuffer_IImageBuffer::operator->() const {
  return mBuffer;
}

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam
