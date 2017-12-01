/*
 * Copyright (C) 2016-2018 Intel Corporation
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

#include "utils/Errors.h"
#include "cros-camera/v4l2_device.h"
#include "ItemPool.h"
#include "FrameInfo.h"
#include "SharedItemPool.h"
#include "InputSystem.h"

#include <cros-camera/camera_buffer_manager.h>


#ifndef PSL_IPU3_BUFFERPOOLS_H_
#define PSL_IPU3_BUFFERPOOLS_H_

namespace android {
namespace camera2 {

class BufferPools
{
public:
    BufferPools();
    virtual ~BufferPools();

    /* IBufferOwner */
    void returnBuffer(cros::V4L2Buffer* buffer);

    status_t createBufferPools(int numBufs, int numSkips,
            std::shared_ptr<InputSystem> isys);

    status_t acquireItem(std::shared_ptr<cros::V4L2Buffer> &v4l2Buffer);
    status_t acquireCaptureSkipBuffer(std::shared_ptr<cros::V4L2Buffer> &v4l2Buffer); /* Skip buffers are used to track aiq settings */
    void returnCaptureSkipBuffer(std::shared_ptr<cros::V4L2Buffer> &v4l2Buffer);

    void freeBuffers();

private:
    status_t allocateCaptureBuffers(std::shared_ptr<cros::V4L2VideoNode> node,
                                    const FrameInfo &frameInfo,
                                    int numSkips,
                                    std::vector<cros::V4L2Buffer> &v4l2Buffers);

private:
    SharedItemPool<cros::V4L2Buffer>   mCaptureItemsPool;         /**< Pool of buffers for Isys capture node. */

    std::vector<std::shared_ptr<cros::V4L2Buffer>> mCaptureSkipBuffers;

    unsigned int mBufferPoolSize;

    // TODO: allocate handles in a buffer wrapper type (CameraBuffer?) and
    // remove |mBufferManager|
    cros::CameraBufferManager* mBufferManager;

    std::vector<buffer_handle_t> mBufferHandles;
};

} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_IPU3_BUFFERPOOLS_H_ */
