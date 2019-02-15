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

#include "../include/DebugControl.h"
#include "../include/MtkHeader.h"
#define PIPE_TRACE TRACE_BUFFER_POOL
#define PIPE_CLASS_TAG "BufferPool"
#include "../include/PipeLog.h"

#include "../include/BufferPool.h"
#include <memory>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

MVOID IBufferPool::destroy(std::shared_ptr<IBufferPool>* pool) {
  TRACE_FUNC_ENTER();
  if (*pool != nullptr) {
    (*pool)->releaseAll();
    *pool = nullptr;
  }
  TRACE_FUNC_EXIT();
}

IBufferPool::IBufferPool() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

IBufferPool::~IBufferPool() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam
