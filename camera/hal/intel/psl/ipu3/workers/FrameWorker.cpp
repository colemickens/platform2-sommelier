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
        mPipelineDepth(pipelineDepth),
        mBufferManager(cros::CameraBufferManager::GetInstance())
{
    LOG1("%s handling node %s", name.c_str(), mNode->Name().c_str());
}

FrameWorker::~FrameWorker()
{
  for (auto& it : mBufferHandles) {
    mBufferManager->Free(it);
  }
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

status_t FrameWorker::allocateWorkerBuffers(uint32_t usage, int pixelFormat)
{
    int memType = mNode->GetMemoryType();
    CheckError((memType != V4L2_MEMORY_DMABUF), BAD_VALUE,
               "@%s, Unsupported memory type %d!", __FUNCTION__, memType);
    CheckError(!mBufferManager, UNKNOWN_ERROR,
               "@%s, Failed to get buffer manager instance!", __FUNCTION__);
    std::shared_ptr<CameraBuffer> buf = nullptr;
    for (unsigned int i = 0; i < mPipelineDepth; i++) {
        LOG2("@%s allocate format: %s size: %d %dx%d bytesperline: %d", __func__, v4l2Fmt2Str(mFormat.PixelFormat()),
                mFormat.SizeImage(0),
                mFormat.Width(),
                mFormat.Height(),
                mFormat.BytesPerLine(0));
        buffer_handle_t handle;
        uint32_t stride;
        int width = (pixelFormat == HAL_PIXEL_FORMAT_BLOB) ?
            mBuffers[i].Length(0) : mFormat.Width();
        int height = (pixelFormat == HAL_PIXEL_FORMAT_BLOB) ?
            1 : mFormat.Height();
        if (mBufferManager->Allocate(
              width, height, pixelFormat, usage, cros::GRALLOC,
              &handle, &stride) != 0) {
            LOGE("Failed to allocate buffer handle!");
            for (auto& it : mBufferHandles) {
                mBufferManager->Free(it);
            }
            mBufferHandles.clear();
            return UNKNOWN_ERROR;
        }
        mBufferHandles.push_back(handle);
        mBuffers[i].SetFd(handle->data[0], 0);
        if (pixelFormat == HAL_PIXEL_FORMAT_BLOB) {
            void* addr;
            if (mBufferManager->Lock(mBufferHandles[i], 0, 0, 0, width,
                                     height, &addr) != 0) {
                LOGE("Failed to lock buffer handle!");
                for (auto& it : mBufferHandles) {
                    mBufferManager->Free(it);
                }
                mBufferHandles.clear();
                return UNKNOWN_ERROR;
            }
            mBufferAddr.push_back(addr);
        } else {
            android_ycbcr ycbcr_info;
            if (mBufferManager->LockYCbCr(mBufferHandles[i], 0, 0, 0,
                                          width, height, &ycbcr_info)
                != 0) {
              LOGE("Failed to lock buffer handle!");
              for (auto& it : mBufferHandles) {
                mBufferManager->Free(it);
              }
              mBufferHandles.clear();
              return UNKNOWN_ERROR;
            }
            // Assume planes are continous for now
            mBufferAddr.push_back(ycbcr_info.y);
        }

        mBuffers[i].SetBytesUsed(mFormat.SizeImage(0), 0);
        mCameraBuffers.push_back(buf);
    }

    return OK;
}

} /* namespace camera2 */
} /* namespace android */

