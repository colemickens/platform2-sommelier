/*
 * Copyright (C) 2017-2018 Intel Corporation.
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

#ifndef PSL_IPU3_WORKERS_FRAMEWORKER_H_
#define PSL_IPU3_WORKERS_FRAMEWORKER_H_

#include "IDeviceWorker.h"
#include <cros-camera/camera_buffer_manager.h>
#include <cros-camera/v4l2_device.h>

namespace android {
namespace camera2 {

class FrameWorker : public IDeviceWorker
{
public:
    FrameWorker(std::shared_ptr<cros::V4L2VideoNode> node, int cameraId,
                size_t pipelineDepth, std::string name = "FrameWorker");
    virtual ~FrameWorker();

    virtual status_t configure(std::shared_ptr<GraphConfig> &config);
    virtual status_t startWorker();
    virtual status_t stopWorker();
    virtual status_t prepareRun(std::shared_ptr<DeviceMessage> msg) = 0;
    virtual status_t run() = 0;
    virtual status_t postRun() = 0;
    virtual bool needPolling() { return mPollMe; }

protected:
    status_t allocateWorkerBuffers(uint32_t gralloc_usage, int pixelFormat);
    status_t setWorkerDeviceFormat(FrameInfo &frame);
    status_t setWorkerDeviceBuffers(enum v4l2_memory memType);

protected:
    std::vector<cros::V4L2Buffer> mBuffers;
    unsigned int mIndex;
    std::vector<std::shared_ptr<CameraBuffer>> mCameraBuffers;

    cros::V4L2Format mFormat;
    bool mPollMe;
    size_t mPipelineDepth;

    // TODO: allocate handles in a buffer wrapper type (CameraBuffer?) and
    // remove |mBufferManager|
    cros::CameraBufferManager* mBufferManager;

    std::vector<buffer_handle_t> mBufferHandles;

    std::vector<void*> mBufferAddr;
};

} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_IPU3_WORKERS_FRAMEWORKER_H_ */
