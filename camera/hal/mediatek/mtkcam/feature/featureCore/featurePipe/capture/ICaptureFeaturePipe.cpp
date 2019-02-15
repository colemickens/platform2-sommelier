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

#include <common/include/DebugControl.h>
#define PIPE_CLASS_TAG "Pipe"
#define PIPE_TRACE TRACE_CAPTURE_FEATURE_PIPE
#include <common/include/PipeLog.h>

#include "CaptureFeaturePipe.h"
#include <memory>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

std::shared_ptr<ICaptureFeaturePipe> ICaptureFeaturePipe::createInstance(
    MINT32 sensorIndex, const UsageHint& usageHint) {
  return std::make_shared<CaptureFeaturePipe>(sensorIndex, usageHint);
}

ICaptureFeaturePipe::UsageHint::UsageHint() : mMode(USAGE_FULL) {}

ICaptureFeaturePipe::UsageHint::UsageHint(eUsageMode mode) : mMode(mode) {}

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
