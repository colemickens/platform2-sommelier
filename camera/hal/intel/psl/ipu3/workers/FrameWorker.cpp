/*
 * Copyright (C) 2016-2018 Intel Corporation.
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
#include <sys/mman.h>

namespace android {
namespace camera2 {

FrameWorker::FrameWorker(std::shared_ptr<cros::V4L2VideoNode> node,
                         int cameraId, size_t pipelineDepth, std::string name) :
        IDeviceWorker(node, cameraId),
        mIndex(0),
        mPollMe(false),
        mPipelineDepth(pipelineDepth)
{
    LOG1("%s handling node %s", name.c_str(), mNode->Name().c_str());
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
    status_t ret = mNode->Start();
    if (ret != OK) {
        LOGE("Unable to start device: %s ret: %d", mNode->Name().c_str(), ret);
    }

    return ret;
}

status_t FrameWorker::stopWorker()
{
    return mNode->Stop();
}

status_t FrameWorker::setWorkerDeviceFormat(FrameInfo &frame)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);

    cros::V4L2Format v4l2_fmt;
    v4l2_fmt.SetWidth(frame.width);
    v4l2_fmt.SetHeight(frame.height);
    v4l2_fmt.SetPixelFormat(frame.format);
    v4l2_fmt.SetBytesPerLine(
        static_cast<unsigned int>(pixelsToBytes(frame.format, frame.stride)), 0);
    v4l2_fmt.SetSizeImage(0, 0);
    v4l2_fmt.SetField(frame.field);
    status_t ret = mNode->SetFormat(v4l2_fmt);
    CheckError(ret != NO_ERROR, ret, "@%s set worker format failed", __FUNCTION__);
    ret = mNode->GetFormat(&mFormat);
    CheckError(ret != NO_ERROR, ret, "@%s get worker format failed", __FUNCTION__);

    return OK;
}

status_t FrameWorker::setWorkerDeviceBuffers(enum v4l2_memory memType)
{
    status_t ret = mNode->SetupBuffers(mPipelineDepth, true, memType,
                                       &mBuffers);
    if (ret != OK) {
        LOGE("Unable to set buffer pool, ret = %d", ret);
        return ret;
    }

    return OK;
}

status_t FrameWorker::allocateWorkerBuffers()
{
    int memType = mNode->GetMemoryType();
    std::vector<int> dmaBufFds;
    unsigned long userptr;
    std::shared_ptr<CameraBuffer> buf = nullptr;
    for (unsigned int i = 0; i < mPipelineDepth; i++) {
        LOG2("@%s allocate format: %s size: %d %dx%d bytesperline: %d", __func__, v4l2Fmt2Str(mFormat.PixelFormat()),
                mFormat.SizeImage(0),
                mFormat.Width(),
                mFormat.Height(),
                mFormat.BytesPerLine(0));
        switch (memType) {
        case V4L2_MEMORY_USERPTR:
            buf = MemoryUtils::allocateHeapBuffer(mFormat.Width(),
                mFormat.Height(),
                mFormat.BytesPerLine(0),
                mFormat.PixelFormat(),
                mCameraId,
                PAGE_ALIGN(mFormat.SizeImage(0)));
            if (buf.get() == nullptr)
                return NO_MEMORY;
            userptr = reinterpret_cast<unsigned long>(buf->data());
            mBuffers[i].SetUserptr(userptr, 0);
            memset(buf->data(), 0, buf->size());
            LOG2("mBuffers[%d].userptr: 0x%lx", i , mBuffers[i].Userptr(0));
            break;
        case V4L2_MEMORY_MMAP:
            if (mNode->ExportFrame(i, &dmaBufFds) != 0 || dmaBufFds.empty()) {
                LOGE("Failed to export buffer");
                return UNKNOWN_ERROR;
            }
            buf = std::make_shared<CameraBuffer>(mFormat.Width(),
                mFormat.Height(),
                mFormat.BytesPerLine(0),
                *mNode,
                i,
                dmaBufFds[0],
                mFormat.PixelFormat(),
                mBuffers[i].Length(0),
                PROT_READ | PROT_WRITE, MAP_SHARED);
            break;
        default:
            LOGE("@%s Unsupported memory type %d", __func__, memType);
            return BAD_VALUE;
        }

        mBuffers[i].SetBytesUsed(mFormat.SizeImage(0), 0);
        mCameraBuffers.push_back(buf);
    }

    return OK;
}

} /* namespace camera2 */
} /* namespace android */

