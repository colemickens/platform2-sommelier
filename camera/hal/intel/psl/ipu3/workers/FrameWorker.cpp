/*
 * Copyright (C) 2016-2017 Intel Corporation.
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

#define LOG_TAG "FrameWorker"

#include "FrameWorker.h"
#include "Camera3GFXFormat.h"

namespace android {
namespace camera2 {

FrameWorker::FrameWorker(std::shared_ptr<V4L2VideoNode> node, int cameraId, std::string name) :
        IDeviceWorker(cameraId),
        mNode(node),
        mPollMe(false)
{
    LOG1("%s handling node %s", name.c_str(), mNode->name());
    CLEAR(mFormat);
}

FrameWorker::~FrameWorker()
{
}

status_t FrameWorker::configure(std::shared_ptr<GraphConfig> & /*config*/)
{
    return OK;
}

status_t FrameWorker::startWorker()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t ret = mNode->start(0);
    if (ret != OK) {
        LOGE("Unable to start device: %s ret: %d", mNode->name(), ret);
    }

    return ret;
}

status_t FrameWorker::stopWorker()
{
    return OK;
}

status_t FrameWorker::setWorkerDeviceFormat(v4l2_buf_type type, FrameInfo &frame)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);

    CLEAR(mFormat);

    mFormat.type = type;
    mNode->setInputBufferType(type);

    mFormat.fmt.pix.width = frame.width;
    mFormat.fmt.pix.height = frame.height;
    if (frame.format == V4L2_PIX_FMT_YUYV) {
        mFormat.fmt.pix.bytesperline = frame.stride ? frame.stride * 2: frame.width * 2;
    } else if (frame.format == V4L2_PIX_FMT_NV12 ||
            frame.format == V4L2_PIX_FMT_NV21 ||
            frame.format == V4L2_META_FMT_IPU3_PARAMS) {
        mFormat.fmt.pix.bytesperline = frame.stride ? frame.stride: frame.width;
    } else {
        LOGE("Unsupported format: %d", frame.format);
        return UNKNOWN_ERROR;
    }
    mFormat.fmt.pix.pixelformat = frame.format;

    status_t ret = mNode->setFormat(mFormat);
    if (ret != OK) {
        LOGE("Unable to configure format: %d", ret);
        return ret;
    }
    return OK;
}

status_t FrameWorker::setWorkerDeviceBuffers(int memType)
{
    for (unsigned int i = 0; i < MAX_WORK_BUFFERS; i++) {
        v4l2_buffer buffer;
        CLEAR(buffer);
        mBuffers.push_back(buffer);
    }
    status_t ret = mNode->setBufferPool(mBuffers, true, memType);
    if (ret != OK) {
        LOGE("Unable to set buffer pool", ret);
        return ret;
    }

    return OK;
}

status_t FrameWorker::allocateWorkerBuffers()
{
    std::shared_ptr<CameraBuffer> buf = nullptr;
    for (unsigned int i = 0; i < MAX_WORK_BUFFERS; i++) {
        LOG2("@%s allocate format: %s size: %d %dx%d bytesperline: %d", __func__, v4l2Fmt2Str(mFormat.fmt.pix.pixelformat),
                mFormat.fmt.pix.sizeimage,
                mFormat.fmt.pix.width,
                mFormat.fmt.pix.height,
                mFormat.fmt.pix.bytesperline);
        buf = MemoryUtils::allocateHeapBuffer(mFormat.fmt.pix.width,
                mFormat.fmt.pix.height,
                widthToStride(mFormat.fmt.pix.pixelformat, mFormat.fmt.pix.width),
                mFormat.fmt.pix.pixelformat,
                mCameraId,
                PAGE_ALIGN(mFormat.fmt.pix.sizeimage));
        if (buf.get() == nullptr)
            return NO_MEMORY;

        mBuffers[i].bytesused = mFormat.fmt.pix.sizeimage;
        mBuffers[i].m.userptr = (unsigned long int)buf->data();
        LOG2("mBuffers[%d].m.userptr: %p", i , mBuffers[i].m.userptr);
        mCameraBuffers.push_back(buf);
        memset((void*)mBuffers[i].m.userptr, 0, buf->size());
    }

    return OK;
}

} /* namespace camera2 */
} /* namespace android */

