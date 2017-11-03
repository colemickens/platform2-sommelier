/*
 * Copyright (C) 2014-2017 Intel Corporation.
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

#ifndef CAMERA3_HAL_EXECUTETASKBASE_H_
#define CAMERA3_HAL_EXECUTETASKBASE_H_

#include <memory>
#include <string>
#include <vector>
#include "CaptureBuffer.h"
#include "Camera3Request.h"
#include "CameraStream.h"
#include "CameraWindow.h"
#include "IExecuteTask.h"
#include "ITaskEventListener.h"
#include "Intel3aControls.h"
#include "GraphConfig.h"
#include "ProcUnitSettings.h"

namespace android {
namespace camera2 {

struct CaptureUnitSettings;
struct ProcUnitSettings;

struct StreamConfig {
    std::vector<camera3_stream_t *> yuvStreams;
    std::vector<camera3_stream_t *> rawStreams;
    std::vector<camera3_stream_t *> blobStreams;
    camera3_stream_t *inputStream;
};

/**
 * \struct ProcTaskMsg
 * Structure to pass data to ExecuteTaskBase-based task objects
 */
struct ProcTaskMsg {
    bool immediate;
    unsigned int reqId;
    std::shared_ptr<CaptureBuffer> rawNonScaledBuffer;
    CaptureBuffer *statsCapture;
    std::shared_ptr<ProcUnitSettings> processingSettings;

    // Default constructor:
    ProcTaskMsg() : immediate(false), reqId(0), statsCapture(nullptr) {}
};

}  // namespace camera2
}  // namespace android

#endif  // CAMERA3_HAL_EXECUTETASKBASE_H_
