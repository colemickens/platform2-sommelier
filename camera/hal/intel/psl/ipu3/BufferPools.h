/*
 * Copyright (C) 2016-2017 Intel Corporation
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
#include "CaptureBuffer.h"
#include "v4l2device.h"
#include "ItemPool.h"
#include "FrameInfo.h"
#include "SharedItemPool.h"
#include "InputSystem.h"


#ifndef PSL_IPU3_BUFFERPOOLS_H_
#define PSL_IPU3_BUFFERPOOLS_H_

namespace android {
namespace camera2 {

class BufferPools : public IBufferOwner
{
public:
    BufferPools(int cameraId);
    virtual ~BufferPools();

    /* IBufferOwner */
    void returnBuffer(CaptureBuffer* buffer);

    status_t createBufferPools(int numBufs, int numSkips,
            std::shared_ptr<InputSystem> isys);

    status_t acquireItem(std::shared_ptr<CaptureBuffer> &capBuffer);
    status_t acquireCaptureSkipBuffer(std::shared_ptr<CaptureBuffer> &capBuffer); /* Skip buffers are used to track aiq settings */
    void returnCaptureSkipBuffer(std::shared_ptr<CaptureBuffer> &capBuffer);

    void freeBuffers();

private:
    status_t createCaptureBufferPool(const FrameInfo &frameInfo, IPU3NodeNames isysNodeName,
            std::shared_ptr<InputSystem> isys, int numSkips);
    status_t allocateCaptureBuffers(const FrameInfo &frameInfo, int numSkips,
            std::vector<struct v4l2_buffer> &v4l2Buffers);
    std::shared_ptr<CameraBuffer> allocateBuffer(const FrameInfo &frameInfo,
                                    int mCameraId,
                                    size_t dataSizeOverride,
                                    bool useWidthForStride);

private:
    SharedItemPool<CaptureBuffer>   mCaptureItemsPool;         /**< Pool of buffers for Isys capture node. */

    std::vector<std::shared_ptr<CaptureBuffer>> mCaptureSkipBuffers;

    unsigned int mBufferPoolSize;
    int mCameraId;
};

} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_IPU3_BUFFERPOOLS_H_ */
