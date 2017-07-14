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

#define LOG_TAG "OutputFrameWorker"

#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "OutputFrameWorker.h"
#include "ColorConverter.h"
#include "CaptureBuffer.h"

namespace android {
namespace camera2 {

OutputFrameWorker::OutputFrameWorker(std::shared_ptr<V4L2VideoNode> node, int cameraId, camera3_stream_t* stream, IPU3NodeNames name) :
                FrameWorker(node, cameraId, "OutputFrameWorker"),
                mOutputBuffer(nullptr),
                mStream(stream),
                mAllDone(false),
                mUseInternalBuffer(true)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    if (mNode)
        LOG1("@%s, node name:%d, device name:%s", __FUNCTION__, name, mNode->name());
}

OutputFrameWorker::~OutputFrameWorker()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
}

status_t OutputFrameWorker::configure(std::shared_ptr<GraphConfig> &/*config*/)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);

    status_t ret = mNode->getFormat(mFormat);
    if (ret != OK)
        return ret;

    LOGD("@%s allocate format: %s size: %d %dx%d", __func__, v4l2Fmt2Str(mFormat.fmt.pix.pixelformat),
            mFormat.fmt.pix.sizeimage,
            mFormat.fmt.pix.width,
            mFormat.fmt.pix.height);

    ret = setWorkerDeviceBuffers();
    if (ret != OK)
        return ret;

    // Use internal buffers
    mUseInternalBuffer = (mStream && mStream->format == HAL_PIXEL_FORMAT_BLOB);
    if (mUseInternalBuffer) {
        return allocateWorkerBuffers();
    }

    return OK;
}

status_t OutputFrameWorker::prepareRun(std::shared_ptr<DeviceMessage> msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    mMsg = msg;
    status_t status = NO_ERROR;
    CameraStream *stream = nullptr;
    std::shared_ptr<CameraBuffer> buffer = nullptr;

    Camera3Request* request = mMsg->cbMetadataMsg.request;
    const std::vector<camera3_stream_buffer>* outBufs = request->getOutputBuffers();
    if (CC_UNLIKELY(outBufs == nullptr)) {
        LOGE("No output buffers in request - BUG"); // should never happen
        return UNKNOWN_ERROR;
    }

    bool notFound = true;
    for (camera3_stream_buffer outputBuffer : *outBufs) {
        stream = reinterpret_cast<CameraStream *>(outputBuffer.stream->priv);

        if (!mStream) {
            LOG2("@%s, mStream is nullptr", __FUNCTION__);
            break;
        }

        LOG2("@%s, stream:%p, mStream:%p", __FUNCTION__, stream->getStream(), mStream);
        if (stream->getStream() == mStream) {
            buffer = request->findBuffer(stream);
            if (CC_UNLIKELY(buffer == nullptr)) {
                LOGE("buffer not found for stream - BUG");
                return UNKNOWN_ERROR;
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
                mJpegTask = std::unique_ptr<JpegEncodeTask>(new JpegEncodeTask(mCameraId));

                status = mJpegTask->init();
                if (status != NO_ERROR) {
                    LOGE("Failed to init JpegEncodeTask Task");
                    status = NO_INIT;
                    mJpegTask.reset();
                    return status;
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
            mAllDone = false;
            mPollMe = true;

            if (!mUseInternalBuffer) {
                // Use stream buffer for zero-copy
                mBuffers[0].bytesused = mFormat.fmt.pix.sizeimage;
                mBuffers[0].m.userptr = (unsigned long int)buffer->data();
                mCameraBuffers.push_back(buffer);
                LOG2("mBuffers[0].m.userptr: %p", mBuffers[0].m.userptr);
            }
            status |= mNode->putFrame(&mBuffers[0]);


            notFound = false;
            break;
        }
    }

    if (notFound) {
        LOGD("No work for this worker mStream: %p", mStream);
        mAllDone = true;
        mOutputBuffer = nullptr;
        mPollMe = false;
    }

    return status;
}

status_t OutputFrameWorker::run()
{
    status_t status = NO_ERROR;
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    if (mMsg == nullptr) {
        LOGE("Message not found - Fix the bug");
        return UNKNOWN_ERROR;
    }

    if (mAllDone) {
        LOGD("No work for this worker");
        return OK;
    }

    // If output format is something else than
    // NV21 or Android flexible YCbCr 4:2:0, return
    if (mOutputBuffer->format() != HAL_PIXEL_FORMAT_YCrCb_420_SP &&
            mOutputBuffer->format() != HAL_PIXEL_FORMAT_YCbCr_420_888 &&
            mOutputBuffer->format() != HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED &&
            mOutputBuffer->format() != HAL_PIXEL_FORMAT_BLOB)  {
        LOGE("Bad format %d", mOutputBuffer->format());
        return BAD_TYPE;
    }

    v4l2_buffer_info outBuf;
    CLEAR(outBuf);

    status = mNode->grabFrame(&outBuf);

    return status;
}

status_t OutputFrameWorker::postRun()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    if (mMsg == nullptr) {
        LOGE("Message null - Fix the bug");
        return UNKNOWN_ERROR;
    }

    Camera3Request* request;
    CameraStream *stream;

    request = mMsg->cbMetadataMsg.request;

    if (mAllDone) {
        mAllDone = false;
        return OK;
    }

    if (mOutputBuffer == nullptr) {
        LOGE("No buffer provided for captureDone");
        return UNKNOWN_ERROR;
    }
    if (request == nullptr) {
        LOGE("No request provided for captureDone");
        return UNKNOWN_ERROR;
    }

    stream = mOutputBuffer->getOwner();

    // Dump the buffers if enabled in flags
    if (mOutputBuffer->format() == HAL_PIXEL_FORMAT_BLOB) {
        // Use internal buffer
        mCameraBuffers[0]->dumpImage(CAMERA_DUMP_JPEG, "before_jpeg_converion_nv12");
        status_t status = convertJpeg(mCameraBuffers[0], mOutputBuffer, request);
        mOutputBuffer->dumpImage(CAMERA_DUMP_JPEG, ".jpg");
        if (status != OK) {
            LOGE("JPEG conversion failed!");
            // Return buffer anyway
        }
    } else if (mOutputBuffer->format() == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED
            || mOutputBuffer->format() == HAL_PIXEL_FORMAT_YCbCr_420_888) {
        if (stream->usage() & GRALLOC_USAGE_HW_VIDEO_ENCODER) {
            mOutputBuffer->dumpImage(CAMERA_DUMP_VIDEO, "VIDEO");
        } else {
            mOutputBuffer->dumpImage(CAMERA_DUMP_PREVIEW, "PREVIEW");
        }
    }

    // call capturedone for the stream of the buffer
    stream->captureDone(mOutputBuffer, request);

    if (!mUseInternalBuffer) {
        mCameraBuffers.erase(mCameraBuffers.begin());
    }

    /* Prevent from using old data */
    mMsg = nullptr;
    mOutputBuffer = nullptr;

    return OK;
}

/**
 * Do jpeg conversion
 * \param[in] input
 * \param[in] output
 * \param[in] request
 */
status_t
OutputFrameWorker::convertJpeg(std::shared_ptr<CameraBuffer> input,
                               std::shared_ptr<CameraBuffer> output,
                               Camera3Request *request)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    ITaskEventListener::PUTaskEvent msg;
    msg.buffer = output;
    msg.jpegInputbuffer = input;
    msg.request = request;

    status_t status = mJpegTask->handleMessageNewJpegInput(msg);
    return status;
}

} /* namespace camera2 */
} /* namespace android */
