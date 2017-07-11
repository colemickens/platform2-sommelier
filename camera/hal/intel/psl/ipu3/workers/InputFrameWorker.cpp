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

#define LOG_TAG "InputFrameWorker"

#include "InputFrameWorker.h"

#include "PerformanceTraces.h"

namespace android {
namespace camera2 {

InputFrameWorker::InputFrameWorker(std::shared_ptr<V4L2VideoNode> node, int cameraId) :
        FrameWorker(node, cameraId, "InputFrameWorker")
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    mPollMe = true;
}

InputFrameWorker::~InputFrameWorker()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
}

status_t InputFrameWorker::configure(std::shared_ptr<GraphConfig> & /*config*/)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);

    status_t ret = mNode->getFormat(mFormat);
    if (ret != OK)
        return ret;

    ret = setWorkerDeviceBuffers();
    if (ret != OK)
        return ret;

    return OK;
}

status_t InputFrameWorker::prepareRun(std::shared_ptr<DeviceMessage> msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    status_t status = OK;

    mBuffers[0].m.userptr = (long unsigned int)msg->pMsg.rawNonScaledBuffer->buf->data();
    status |= mNode->putFrame(&mBuffers[0]);
    msg->pMsg.processingSettings->request->setSeqenceId(msg->pMsg.rawNonScaledBuffer->v4l2Buf.sequence);
    PERFORMANCE_HAL_ATRACE_PARAM1("seqId", msg->pMsg.rawNonScaledBuffer->v4l2Buf.sequence);

    return status;
}

status_t InputFrameWorker::run()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    return OK;
}

status_t InputFrameWorker::postRun()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    v4l2_buffer_info outBuf;
    CLEAR(outBuf);
    status_t status = OK;
    status = mNode->grabFrame(&outBuf);

    return status;
}


} /* namespace camera2 */
} /* namespace android */
