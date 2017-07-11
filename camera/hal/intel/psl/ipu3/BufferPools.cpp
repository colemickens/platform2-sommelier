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
    std::shared_ptr<V4L2VideoNode> node = nullptr;
    int nodeCount = 0;
    IPU3NodeNames isysNodeName = IMGU_NODE_NULL;

    mBufferPoolSize = numBufs + numSkips;

    InputSystem::ConfiguredNodesPerName *nodes = nullptr;

    status_t status = isys->getOutputNodes(&nodes, nodeCount);
    if (status != NO_ERROR) {
        LOGE("ISYS output nodes not configured");
        return status;
    }
    FrameInfo frameInfo;

    /**
     * Init the pool of capture buffer structs. This pool contains the
     * V4L2 buffers that are registered to the V4L2 device.
     */
    mCaptureItemsPool.init(mBufferPoolSize);

    for (const auto &devNode : *nodes) {
        isysNodeName = devNode.first;
        node = devNode.second;

        if (isysNodeName == ISYS_NODE_CSI_BE_SOC) {
            LOG1("ISYS node %d", isysNodeName);
            node->getConfig(frameInfo);
            struct v4l2_format aFormat;
            CLEAR(aFormat);
            node->getFormat(aFormat);
            LOG1("@%s: creating capture buffer pool (size: %d) format: %s",
                    __FUNCTION__, mBufferPoolSize, v4l2Fmt2Str(frameInfo.format));
            status = createCaptureBufferPool(frameInfo, isysNodeName, isys, numSkips);
        } else {
            LOGE("Unsupported ISYS node %d", isysNodeName);
            status = UNKNOWN_ERROR;
        }

        if (status != NO_ERROR) {
            LOGE("Failed to create buffer pool for ISYS node %d", isysNodeName);
            return status;
        }
    }

    return status;
}

/**
 * Creates pool of CaptureBuffer's needed to be queued to Isys nodes.
 *
 * \param[in] frameInfo Width, height, format and stride information for the allocation
 * \param[in] isysNodeName Used for checkign which ISYS device node the buffers will be set to
 * \param[in,out] isys InputSystem object to which the buffer pool will be assigned to
 * \param[in] numSkips umber of skip buffers to allocate
 * \return NO_ERROR if acllocation and setting the buffer pool was successful
 */
status_t BufferPools::createCaptureBufferPool(const FrameInfo &frameInfo,
        IPU3NodeNames isysNodeName,
        std::shared_ptr<InputSystem> isys, int numSkips)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status = NO_ERROR;

    std::vector<v4l2_buffer> v4l2Buffers;
    for (unsigned int i = 0; i < mBufferPoolSize; i++) {
        std::shared_ptr<CaptureBuffer> captureBufPtr = nullptr;
        mCaptureItemsPool.acquireItem(captureBufPtr);
        if (captureBufPtr.get() == nullptr) {
            LOGE("Failed to get a capture buffer!");
            return UNKNOWN_ERROR;
        }
        CLEAR(captureBufPtr->v4l2Buf);
        captureBufPtr->owner = this;
        v4l2Buffers.push_back(captureBufPtr->v4l2Buf);
    }

    status = isys->setBufferPool(isysNodeName, v4l2Buffers, true);
    if (status != NO_ERROR) {
        LOGE("Failed to set the capture buffer pool in ISYS status: 0x%X", status);
        return status;
    }

    status = allocateCaptureBuffers(frameInfo, numSkips, v4l2Buffers);
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
 * \param[in] frameInfo Width, height, stride and format info for the buffers
 *  being allocated
 * \param[in] numSkips Number of skip buffers to allocate
 * \param[out] v4l2Buffers The allocated V4L2 buffers
 * \return status of execution
 */
status_t BufferPools::allocateCaptureBuffers(const FrameInfo &frameInfo,
        int numSkips, std::vector<struct v4l2_buffer> &v4l2Buffers)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status = NO_ERROR;
    std::shared_ptr<CameraBuffer> tmpBuf = nullptr;
    std::shared_ptr<CameraBuffer> skipBuf = nullptr;
    std::shared_ptr<CaptureBuffer> captureBuf = nullptr;

    if (v4l2Buffers.size() == 0) {
        LOGE("v4l2 buffers where not allocated");
        return NO_MEMORY;
    }

    if (v4l2Buffers.size() != mBufferPoolSize) {
        LOGE("BufferPoolSize is not matching to requested number of v4l2 buffers, \
              v4l2 Buffer size: %d, bufferpool size: %d", \
              v4l2Buffers.size(), mBufferPoolSize);
        return BAD_VALUE;
    }

    uint32_t dataSizeOverride = v4l2Buffers[0].length;

    skipBuf = allocateBuffer(frameInfo, mCameraId, dataSizeOverride, false);

    if (skipBuf == nullptr) {
        LOGE("Failed to allocate internal ISYS skip buffer");
        return NO_MEMORY;
    }

    if (skipBuf->size() != dataSizeOverride) {
        LOGE("Capture buffer is not big enough");
        return NO_MEMORY;
    }

    unsigned int numBufs = mBufferPoolSize - numSkips;
    LOG2("numBufs: %d numSkips: %d", numBufs, numSkips);
    for (unsigned int i = 0; i < mBufferPoolSize; i++) {
        if (i < numBufs) {
            tmpBuf = allocateBuffer(frameInfo, mCameraId, dataSizeOverride, false);
            if (tmpBuf == nullptr) {
                LOGE("Failed to allocate internal ISYS capture buffers");
                return NO_MEMORY;
            }
        } else {
            LOG1("capture buffer allocation, using same buffer for skip, i = %d", i);
            tmpBuf = skipBuf;
        }

        mCaptureItemsPool.acquireItem(captureBuf);
        if (captureBuf.get() == nullptr) {
            LOGE("Failed to get a capture buffer!");
            return UNKNOWN_ERROR;
        }
        captureBuf->buf = tmpBuf;
        captureBuf->v4l2Buf = v4l2Buffers[i];
        captureBuf->v4l2Buf.m.userptr = (unsigned long int)captureBuf->buf->data();

        LOG2("captureBuf->v4l2Buf.m.userptr: %p", captureBuf->v4l2Buf.m.userptr);
        LOG2("captureBuf->v4l2Buf.index: %d", captureBuf->v4l2Buf.index);

        if (i >= numBufs)
            mCaptureSkipBuffers.push_back(captureBuf);
    }

    return status;
}


std::shared_ptr<CameraBuffer> BufferPools::allocateBuffer(const FrameInfo &frameInfo,
                                             int mCameraId,
                                             size_t dataSizeOverride,
                                             bool useWidthForStride)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    std::shared_ptr<CameraBuffer> buf = nullptr;
    LOG2("@%s allocate format: %s", __func__, v4l2Fmt2Str(frameInfo.format));
    buf = MemoryUtils::allocateHeapBuffer(frameInfo.width,
                                          frameInfo.height,
                                          useWidthForStride ? frameInfo.width : frameInfo.stride,
                                          frameInfo.format,
                                          mCameraId,
                                          dataSizeOverride);
    if (buf == nullptr) {
        LOGE("Failed to allocate skip buffer");
        return nullptr;
    }
    memset(buf->data(), 0, buf->size());
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
        LOG2("@%s captureBuf->v4l2Buf.m.userptr: %p", __func__, capBuffer->v4l2Buf.m.userptr);
        LOG2("@%s captureBuf->v4l2Buf.index: %d", __func__, capBuffer->v4l2Buf.index);
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
