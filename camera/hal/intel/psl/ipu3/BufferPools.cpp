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

#define LOG_TAG "BufferPools"

#include "BufferPools.h"
#include "LogHelper.h"
#include <linux/videodev2.h>
#include <sys/mman.h>

namespace android {
namespace camera2 {

BufferPools::BufferPools() :
        mCaptureItemsPool("CaptureItemsPool"),
        mBufferPoolSize(0),
        mBufferManager(cros::CameraBufferManager::GetInstance())
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
}

BufferPools::~BufferPools()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    freeBuffers();
}

/**
 * Creates the capture buffer pools needed by InputSystem
 *
 * \param[in] numBufs Number of capture buffers to allocate
 * \param[in] numSkips Number of skip buffers to allocate
 * \param[in,out] isys The InputSystem object to which the allocated
 *  buffer pools will be assigned to
 */
status_t BufferPools::createBufferPools(int numBufs, int numSkips,
        std::shared_ptr<InputSystem> isys)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    std::shared_ptr<cros::V4L2VideoNode> node = isys->findOutputNode(ISYS_NODE_RAW);
    status_t status = NO_ERROR;

    CheckError((node.get() == nullptr), UNKNOWN_ERROR,
        "@%s: create buffer pool failed", __FUNCTION__);
    mBufferPoolSize = numBufs + numSkips;

    /**
     * Init the pool of capture buffer structs. This pool contains the
     * V4L2 buffers that are registered to the V4L2 device.
     */
    mCaptureItemsPool.init(mBufferPoolSize);
    cros::V4L2Format aFormat;
    node->GetFormat(&aFormat);
    LOG1("@%s: creating capture buffer pool (size: %d) format: %s",
        __FUNCTION__, mBufferPoolSize, v4l2Fmt2Str(aFormat.PixelFormat()));

    std::vector<cros::V4L2Buffer> v4l2Buffers;
    for (unsigned int i = 0; i < mBufferPoolSize; i++) {
        std::shared_ptr<cros::V4L2Buffer> v4l2BufPtr = nullptr;
        mCaptureItemsPool.acquireItem(v4l2BufPtr);
        if (v4l2BufPtr.get() == nullptr) {
            LOGE("Failed to get a capture buffer!");
            return UNKNOWN_ERROR;
        }
        v4l2Buffers.push_back(*v4l2BufPtr);
    }

    status = isys->setBufferPool(ISYS_NODE_RAW, v4l2Buffers, true);
    if (status != NO_ERROR) {
        LOGE("Failed to set the capture buffer pool in ISYS status: 0x%X", status);
        return status;
    }

    FrameInfo frameInfo = {};
    frameInfo.format = aFormat.PixelFormat();
    frameInfo.width = aFormat.Width();
    frameInfo.height = aFormat.Height();
    frameInfo.stride = bytesToPixels(frameInfo.format, aFormat.BytesPerLine(0));
    frameInfo.size =
        frameSize(frameInfo.format, frameInfo.stride, frameInfo.height);
    status = allocateCaptureBuffers(node, frameInfo, numSkips, v4l2Buffers);
    if (status != NO_ERROR) {
        LOGE("Failed to allocate capture buffer in ISYS status: 0x%X", status);
        return status;
    }

    return status;
}

/**
 * Allocates CameraBuffers to each of the cros::V4L2Buffer in the
 * pool for Isys node.
 *
 * A given number of skip buffers is also allocated, but they will share
 * the same buffer memory. These buffers are taken out of the
 * mCaptureItemsPool, and inserted to a mCaptureSkipBuffers vector where they
 * are fetched from when skipping is done, and returned manually, when skips
 * come out of the driver. Effectively this will mean the buffer pool has a
 * proper v4l2 id for each buffer, including the skip buffers, and the skip
 * buffers will share memory between each other.
 *
 * \param[in] node the video node that owns the v4l2 buffers
 * \param[in] frameInfo Width, height, stride and format info for the buffers
 *  being allocated
 * \param[in] numSkips Number of skip buffers to allocate
 * \param[out] v4l2Buffers The allocated V4L2 buffers
 * \return status of execution
 */
status_t BufferPools::allocateCaptureBuffers(
        std::shared_ptr<cros::V4L2VideoNode> node,
        const FrameInfo &frameInfo,
        int numSkips, std::vector<cros::V4L2Buffer> &v4l2Buffers)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status = NO_ERROR;
    std::shared_ptr<cros::V4L2Buffer> v4l2Buf = nullptr;

    if (v4l2Buffers.size() == 0) {
        LOGE("v4l2 buffers where not allocated");
        return NO_MEMORY;
    }

    if (v4l2Buffers.size() != mBufferPoolSize) {
        LOGE("BufferPoolSize is not matching to requested number of v4l2 buffers, \
              v4l2 Buffer size: %zu, bufferpool size: %u", \
              v4l2Buffers.size(), mBufferPoolSize);
        return BAD_VALUE;
    }

    unsigned int numBufs = mBufferPoolSize - numSkips;
    LOG2("numBufs: %d numSkips: %d", numBufs, numSkips);
    for (unsigned int i = 0; i < mBufferPoolSize; i++) {
        mCaptureItemsPool.acquireItem(v4l2Buf);
        if (v4l2Buf.get() == nullptr) {
            LOGE("Failed to get a capture buffer!");
            return UNKNOWN_ERROR;
        }
        *v4l2Buf = v4l2Buffers[i];
        CheckError(v4l2Buf->Memory() != V4L2_MEMORY_DMABUF, UNKNOWN_ERROR,
                   "Unsupported memory type %d.", v4l2Buf->Memory());
        CheckError(!mBufferManager, UNKNOWN_ERROR,
                   "Failed to get buffer manager instance!");
        buffer_handle_t handle;
        uint32_t stride;
        if (mBufferManager->Allocate(
              v4l2Buf->Length(0), 1, HAL_PIXEL_FORMAT_BLOB,
              GRALLOC_USAGE_HW_CAMERA_READ |
              GRALLOC_USAGE_HW_CAMERA_WRITE, cros::GRALLOC, &handle,
              &stride) != 0) {
            LOGE("Failed to allocate buffer handle!");
            for (auto& it : mBufferHandles) {
                mBufferManager->Free(it);
            }
            mBufferHandles.clear();
            return UNKNOWN_ERROR;
        }
        mBufferHandles.push_back(handle);
        v4l2Buf->SetFd(handle->data[0], 0);
        LOG2("v4l2Buf->index: %d", v4l2Buf->Index());

        if (i >= numBufs)
            mCaptureSkipBuffers.push_back(v4l2Buf);
    }

    return status;
}

void BufferPools::freeBuffers()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);

    mCaptureSkipBuffers.clear();
    for (auto& it : mBufferHandles) {
        mBufferManager->Free(it);
    }
    mBufferHandles.clear();
}

status_t BufferPools::acquireItem(std::shared_ptr<cros::V4L2Buffer> &v4l2Buffer)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    return mCaptureItemsPool.acquireItem(v4l2Buffer);
}

void BufferPools::returnCaptureSkipBuffer(std::shared_ptr<cros::V4L2Buffer> &v4l2Buffer)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    mCaptureSkipBuffers.push_back(v4l2Buffer);
}

status_t BufferPools::acquireCaptureSkipBuffer(std::shared_ptr<cros::V4L2Buffer> &v4l2Buffer)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    if (mCaptureSkipBuffers.empty() == false) {
        v4l2Buffer = mCaptureSkipBuffers.at(0);
        mCaptureSkipBuffers.erase(mCaptureSkipBuffers.begin());
        LOG2("@%s captureBuf->v4l2Buf.m.userptr: 0x%lx", __func__, v4l2Buffer->Userptr(0));
        LOG2("@%s captureBuf->v4l2Buf.index: %d", __func__, v4l2Buffer->Index());
        return OK;
    }
    return UNKNOWN_ERROR;
}

void BufferPools::returnBuffer(cros::V4L2Buffer * /* buffer */)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    LOGW("IMPLEMENTATION MISSING");
}


} /* namespace camera2 */
} /* namespace android */
