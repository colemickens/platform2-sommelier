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

#define LOG_TAG "SWOutputFrameWorker"

#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "SWOutputFrameWorker.h"
#include "ColorConverter.h"
#include "CaptureBuffer.h"

namespace android {
namespace camera2 {

SWOutputFrameWorker::SWOutputFrameWorker(int cameraId, camera3_stream_t* stream) :
                IDeviceWorker(cameraId),
                mOutputBuffer(nullptr),
                mInputBuffer(nullptr),
                mStream(stream),
                mHasNewInput(false),
                mAllDone(false)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
}

SWOutputFrameWorker::~SWOutputFrameWorker()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
}

bool SWOutputFrameWorker::notifyCaptureEvent(CaptureMessage *msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    if (msg->id != CAPTURE_MESSAGE_ID_EVENT
        || msg->data.event.type != CAPTURE_EVENT_YUV)
        return true;

    std::shared_ptr<CameraBuffer> srcBuf = msg->data.event.yuvBuffer;
    if (mInputBuffer == nullptr
        || mInputBuffer->width() != srcBuf->width()
        || mInputBuffer->height() != srcBuf->height()
        || mInputBuffer->format() != srcBuf->format()) {
        // force v4l2Fmt to V4L2_PIX_FMT_NV12, as v4l2Fmt of request
        // buffer is V4L2_PIX_FMT_NV12M which is not supported by libjpeg
        int v4l2Fmt =  (mStream->format == HAL_PIXEL_FORMAT_BLOB) ? \
                        V4L2_PIX_FMT_NV12 : srcBuf->v4l2Fmt();
        mInputBuffer = nullptr;
        mInputBuffer = MemoryUtils::allocateHeapBuffer(
                                    srcBuf->width(),
                                    srcBuf->height(),
                                    srcBuf->stride(),
                                    v4l2Fmt,
                                    mCameraId,
                                    srcBuf->size());
        if (mInputBuffer.get() == nullptr){
            LOGE("failed to alloc memory for SWOutputFrameWorker, BUG");
            return false;
        }
    }

    if (mOutputBuffer != nullptr){
        MEMCPY_S(mInputBuffer->data(), mInputBuffer->size(),
             srcBuf->data(), srcBuf->size());
        mHasNewInput = true;
    }

    return true;
}

status_t SWOutputFrameWorker::configure(std::shared_ptr<GraphConfig> &/*config*/)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    return OK;
}

status_t SWOutputFrameWorker::prepareRun(std::shared_ptr<DeviceMessage> msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    mMsg = msg;
    status_t status;
    CameraStream *stream = reinterpret_cast<CameraStream *>(mStream->priv);
    Camera3Request* request = mMsg->cbMetadataMsg.request;
    std::shared_ptr<CameraBuffer> buffer = request->findBuffer(stream);
    if (buffer == nullptr) {
        LOGD("No work for this worker mStream: %p", mStream);
        mAllDone = true;
        mOutputBuffer = nullptr;
        return OK;
    }

    if (!buffer->isLocked()) {
        status = buffer->lock();
        if (CC_UNLIKELY(status != NO_ERROR)) {
            LOGE("Could not lock the buffer error %d", status);
            return status;
        }
    }

    status = buffer->waitOnAcquireFence();
    if (CC_UNLIKELY(status != NO_ERROR)) {
        LOGW("Wait on fence for buffer %p timed out", buffer.get());
    }

    if (buffer->format() == HAL_PIXEL_FORMAT_BLOB) {
        if (! mJpegTask.get()) {
            LOG2("Create JpegEncodeTask");
            mJpegTask = std::unique_ptr<JpegEncodeTask>(new JpegEncodeTask(mCameraId));

            status = mJpegTask->init();
            if (status != NO_ERROR) {
                LOGE("Failed to init JpegEncodeTask Task");
                status = NO_INIT;
                mJpegTask.reset();
                return status;
            }
        }

        status = mJpegTask->handleMessageSettings(*msg->pMsg.processingSettings);
        if (status != NO_ERROR) {
            LOGE("Failed to handleMessageSettings");
            status = NO_INIT;
            mJpegTask.reset();
            return status;
        }
    }

    /*
     * store the buffer in a map where the key is the terminal UID
     */
    mOutputBuffer = buffer;
    mHasNewInput = false;
    mAllDone = false;
    return status;
}

status_t SWOutputFrameWorker::run()
{
    return OK;
}

status_t SWOutputFrameWorker::postRun()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    if (mMsg == nullptr) {
        LOGE("Message null - Fix the bug");
        return UNKNOWN_ERROR;
    }

    if (mAllDone) {
        mAllDone = false;
        return OK;
    }

    Camera3Request* request;
    CameraStream *stream;

    request = mMsg->cbMetadataMsg.request;

    if(mOutputBuffer && mHasNewInput && request){
        stream = mOutputBuffer->getOwner();
        // Dump the buffers if enabled in flags
        if (mOutputBuffer->format() == HAL_PIXEL_FORMAT_BLOB) {
            mInputBuffer->dumpImage(CAMERA_DUMP_JPEG, "before_jpeg_converion_nv12");
            status_t status = convertJpeg(mInputBuffer, mOutputBuffer, request);
            mOutputBuffer->dumpImage(CAMERA_DUMP_JPEG, ".jpg");
            if (status != OK) {
                LOGE("JPEG conversion failed!");
                // Return buffer anyway
            }
        } else if (mOutputBuffer->format() == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED
                || mOutputBuffer->format() == HAL_PIXEL_FORMAT_YCbCr_420_888) {
            //when size of jpeg > yuv, we get the smaller yuv stream from the larger one
            // TODO: need color conversion or downscaling?
            MEMCPY_S(mOutputBuffer->data(), mOutputBuffer->size(),
                     mInputBuffer->data(), mInputBuffer->size());
        }

        // call capturedone for the stream of the buffer
        stream->captureDone(mOutputBuffer, request);

        /* Prevent from using old data */
        mMsg = nullptr;
        mOutputBuffer = nullptr;
        mHasNewInput = false;

        return OK;
    }

    return OK;
}

/**
 * Do jpeg conversion
 * \param[in] input
 * \param[in] output
 * \param[in] request
 */
status_t
SWOutputFrameWorker::convertJpeg(std::shared_ptr<CameraBuffer> input,
                               std::shared_ptr<CameraBuffer> output,
                               Camera3Request *request)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    ITaskEventListener::PUTaskEvent msg;
    msg.buffer = output;
    msg.jpegInputbuffer = input;
    msg.request = request;

    status_t status = NO_ERROR;
    if (mJpegTask.get()) {
        status = mJpegTask->handleMessageNewJpegInput(msg);
    }

    return status;
}

} /* namespace camera2 */
} /* namespace android */
