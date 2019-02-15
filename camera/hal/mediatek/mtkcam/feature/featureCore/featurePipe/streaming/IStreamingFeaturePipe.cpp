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

#include <featurePipe/common/include/DebugControl.h>
#define PIPE_CLASS_TAG "Pipe"
#define PIPE_TRACE TRACE_STREAMING_FEATURE_PIPE
#include <featurePipe/common/include/PipeLog.h>
#include <memory>
#include "StreamingFeaturePipe.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

std::shared_ptr<IStreamingFeaturePipe> IStreamingFeaturePipe::createInstance(
    MUINT32 sensorIndex, const UsageHint& usageHint) {
  return std::make_shared<StreamingFeaturePipe>(sensorIndex, usageHint);
}
IStreamingFeaturePipe::UsageHint::UsageHint()
    : mMode(USAGE_FULL), mStreamingSize(0, 0), mVendorMode(0), m3DNRMode(0) {}

IStreamingFeaturePipe::UsageHint::UsageHint(eUsageMode mode,
                                            const MSize& streamingSize)
    : mMode(mode),
      mStreamingSize(streamingSize),
      mVendorMode(0),
      m3DNRMode(0) {}

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam
