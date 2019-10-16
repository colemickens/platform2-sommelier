/*
 * Copyright (C) 2013-2017 Intel Corporation
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

#define LOG_TAG "Stream"
#include <stddef.h>

#include "LogHelper.h"
#include "CameraStream.h"
#include "PlatformData.h"
#include "PerformanceTraces.h"

namespace cros {
namespace intel {
CameraStream::CameraStream(int seqNo, camera3_stream_t * stream,
                           IRequestCallback * callback) : mActive(false),
                                                          mSeqNo(seqNo),
                                                          mCallback(callback),
                                                          mOutputBuffersInHal(0),
                                                          mStream3(stream)
{
}

CameraStream::~CameraStream()
{
    LOG2("@%s, pending request size: %zu", __FUNCTION__, mPendingRequests.size());

    std::unique_lock<std::mutex> l(mPendingLock);
    mPendingRequests.clear();
    l.unlock();

    mCamera3Buffers.clear();
}

void CameraStream::setActive(bool active)
{
    LOG1("CameraStream: %d set to: %s", mSeqNo, active ? " Active":" Inactive");
    mActive = active;
}

status_t CameraStream::capture(std::shared_ptr<CameraBuffer> aBuffer,
                               Camera3Request* request)
{
    UNUSED(aBuffer);
    UNUSED(request);
    return NO_ERROR;
}

status_t CameraStream::captureDone(std::shared_ptr<CameraBuffer> aBuffer,
                                   Camera3Request* request)
{
    LOG2("@%s", __FUNCTION__);
    std::lock_guard<std::mutex> l(mPendingLock);
    /* Usually the correct request is found at index 0 in the mPendingRequests
     * Vector, but reprocessing requests are allowed to deviate from the FIFO
     * rule. See camera3.h section "S10.3 Reprocessing pipeline characteristics"
     *
     * The PSL shall be responsible for maintaining per-stream FIFO processing
     * order among all the normal output requests and among the reprocessing
     * requests, but reprocessing requests may be completed before normal output
     * requests.
     */
    for (uint32_t i = 0; i < mPendingRequests.size(); i++) {
        Camera3Request *pendingRequest;
        pendingRequest = mPendingRequests.at(i);
        if (request == nullptr || request->getId() == pendingRequest->getId()) {
            mPendingRequests.erase(mPendingRequests.begin() + i);
            mCallback->bufferDone(pendingRequest, aBuffer);
            if (request != nullptr)
                PERFORMANCE_HAL_ATRACE_PARAM1("seqId", request->seqenceId());
            break;
        }
    }

    return NO_ERROR;
}

status_t CameraStream::processRequest(Camera3Request* request)
{
    LOG2("@%s %d", __FUNCTION__, mSeqNo);
    int status = NO_ERROR;
    std::shared_ptr<CameraBuffer> buffer;

    std::unique_lock<std::mutex> l(mPendingLock);
    mPendingRequests.push_back(request);
    l.unlock();

    buffer = request->findBuffer(this);
    if (CC_UNLIKELY(buffer == nullptr)) {
        LOGE("@%s No buffer associated with stream.", __FUNCTION__);
        return NO_MEMORY;
    }
    status = capture(buffer, request);

    return status;
}

} /* namespace intel */
} /* namespace cros */
