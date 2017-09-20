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

#define LOG_TAG "BufferPools"

#include "BufferPools.h"
#include "LogHelper.h"
#include <linux/videodev2.h>
#include <sys/mman.h>

namespace android {
namespace camera2 {

BufferPools::BufferPools(int cameraId) :
        mCaptureItemsPool("CaptureItemsPool"),
        mBufferPoolSize(0),
        mCameraId(cameraId)
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
    std::shared_ptr<V4L2VideoNode> node = isys->findOutputNode(ISYS_NODE_RAW);
    FrameInfo frameInfo;
    status_t status = NO_ERROR;

    CheckError((node.get() == nullptr), UNKNOWN_ERROR,
        "@%s: create buffer pool failed", __FUNCTION__);
    mBufferPoolSize = numBufs + numSkips;

    /**
     * Init the pool of capture buffer structs. This pool contains the
     * V4L2 buffers that are registered to the V4L2 device.
     */
    mCaptureItemsPool.init(mBufferPoolSize);
    node->getConfig(frameInfo);
    V4L2Format aFormat;
    node->getFormat(aFormat);
    LOG1("@%s: creating capture buffer pool (size: %d) format: %s",
        __FUNCTION__, mBufferPoolSize, v4l2Fmt2Str(frameInfo.format));

    std::vector<V4L2Buffer> v4l2Buffers;
    for (unsigned int i = 0; i < mBufferPoolSize; i++) {
        std::shared_ptr<CaptureBuffer> captureBufPtr = nullptr;
        mCaptureItemsPool.acquireItem(captureBufPtr);
        if (captureBufPtr.get() == nullptr) {
            LOGE("Failed to get a capture buffer!");
            return UNKNOWN_ERROR;
        }
        captureBufPtr->owner = this;
        v4l2Buffers.push_back(captureBufPtr->v4l2Buf);
    }

    status = isys->setBufferPool(ISYS_NODE_RAW, v4l2Buffers, true);
    if (status != NO_ERROR) {
        LOGE("Failed to set the capture buffer pool in ISYS status: 0x%X", status);
        return status;
    }

    status = allocateCaptureBuffers(node, frameInfo, numSkips, v4l2Buffers);
    if (status != NO_ERROR) {
        LOGE("Failed to allocate capture buffer in ISYS status: 0x%X", status);
        return status;
    }

    return status;
}

/**
 * Allocates CameraBuffers to each of the CaptureBuffer in the
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
        std::shared_ptr<V4L2VideoNode> node,
        const FrameInfo &frameInfo,
        int numSkips, std::vector<V4L2Buffer> &v4l2Buffers)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status = NO_ERROR;
    std::shared_ptr<CameraBuffer> tmpBuf = nullptr;
    std::shared_ptr<CaptureBuffer> captureBuf = nullptr;
    InputSystem::ConfiguredNodesPerName *nodes = nullptr;

    if (v4l2Buffers.size() == 0) {
        LOGE("v4l2 buffers where not allocated");
        return NO_MEMORY;
    }

    if (v4l2Buffers.size() != mBufferPoolSize) {
        LOGE("BufferPoolSize is not matching to requested number of v4l2 buffers, \
              v4l2 Buffer size: %lu, bufferpool size: %u", \
              v4l2Buffers.size(), mBufferPoolSize);
        return BAD_VALUE;
    }

    uint32_t dataSizeOverride = v4l2Buffers[0].length();
    unsigned int numBufs = mBufferPoolSize - numSkips;
    LOG2("numBufs: %d numSkips: %d", numBufs, numSkips);
    for (unsigned int i = 0; i < mBufferPoolSize; i++) {
        tmpBuf = allocateBuffer(node, frameInfo, mCameraId, v4l2Buffers[i],
                                dataSizeOverride);
        CheckError((tmpBuf == nullptr), NO_MEMORY,
            "Failed to allocate internal ISYS %s buffer", i < numBufs ? "capture" : "skip");
        CheckError((tmpBuf->size() != dataSizeOverride), NO_MEMORY,
            "Capture buffer is not big enough");

        mCaptureItemsPool.acquireItem(captureBuf);
        if (captureBuf.get() == nullptr) {
            LOGE("Failed to get a capture buffer!");
            return UNKNOWN_ERROR;
        }
        captureBuf->buf = tmpBuf;
        captureBuf->v4l2Buf = v4l2Buffers[i];
        if (v4l2Buffers[i].memory() == V4L2_MEMORY_USERPTR) {
            unsigned long userptr = reinterpret_cast<unsigned long>(captureBuf->buf->data());
            captureBuf->v4l2Buf.setUserptr(userptr);
            LOG2("captureBuf->v4l2Buf.m.userptr: %p", (void*)captureBuf->v4l2Buf.userptr());
        }
        LOG2("captureBuf->v4l2Buf.index: %d", captureBuf->v4l2Buf.index());

        if (i >= numBufs)
            mCaptureSkipBuffers.push_back(captureBuf);
    }

    return status;
}


std::shared_ptr<CameraBuffer> BufferPools::allocateBuffer(std::shared_ptr<V4L2VideoNode> node,
                                             const FrameInfo &frameInfo,
                                             int mCameraId,
                                             V4L2Buffer &v4l2Buf,
                                             size_t dataSizeOverride)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    std::shared_ptr<CameraBuffer> buf = nullptr;
    int dmaBufFd;
    LOG2("@%s allocate format: %s", __func__, v4l2Fmt2Str(frameInfo.format));
    switch (v4l2Buf.memory()) {
    case V4L2_MEMORY_USERPTR:
        buf = MemoryUtils::allocateHeapBuffer(frameInfo.width,
                                              frameInfo.height,
                                              frameInfo.stride,
                                              frameInfo.format,
                                              mCameraId,
                                              dataSizeOverride);
        memset(buf->data(), 0, buf->size());
        break;
    case V4L2_MEMORY_MMAP:
        dmaBufFd = node->exportFrame(v4l2Buf.index());
        buf = std::make_shared<CameraBuffer>(frameInfo.width,
                                             frameInfo.height,
                                             frameInfo.stride,
                                             node->getFd(),
                                             dmaBufFd,
                                             dataSizeOverride,
                                             frameInfo.format,
                                             v4l2Buf.offset(), PROT_READ | PROT_WRITE, MAP_SHARED);
        break;
    default:
        LOGE("Unsupported memory type.");
        return nullptr;
    }
    if (buf == nullptr) {
        LOGE("Failed to allocate skip buffer");
        return nullptr;
    }
    return buf;
}


void BufferPools::freeBuffers()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);

    mCaptureSkipBuffers.clear();
}

status_t BufferPools::acquireItem(std::shared_ptr<CaptureBuffer> &capBuffer)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    return mCaptureItemsPool.acquireItem(capBuffer);
}

void BufferPools::returnCaptureSkipBuffer(std::shared_ptr<CaptureBuffer> &capBuffer)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    mCaptureSkipBuffers.push_back(capBuffer);
}

status_t BufferPools::acquireCaptureSkipBuffer(std::shared_ptr<CaptureBuffer> &capBuffer)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    if (mCaptureSkipBuffers.empty() == false) {
        capBuffer = mCaptureSkipBuffers.at(0);
        mCaptureSkipBuffers.erase(mCaptureSkipBuffers.begin());
        LOG2("@%s captureBuf->v4l2Buf.m.userptr: 0x%lx", __func__, capBuffer->v4l2Buf.userptr());
        LOG2("@%s captureBuf->v4l2Buf.index: %d", __func__, capBuffer->v4l2Buf.index());
        return OK;
    }
    return UNKNOWN_ERROR;
}

void BufferPools::returnBuffer(CaptureBuffer * /* buffer */)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    LOGW("IMPLEMENTATION MISSING");
}


} /* namespace camera2 */
} /* namespace android */
