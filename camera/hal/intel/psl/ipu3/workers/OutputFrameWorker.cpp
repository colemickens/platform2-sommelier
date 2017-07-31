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
#include "NodeTypes.h"
#include <libyuv.h>

namespace android {
namespace camera2 {

const unsigned int OUTPUTFRAME_WORK_BUFFERS = 3;

OutputFrameWorker::OutputFrameWorker(std::shared_ptr<V4L2VideoNode> node, int cameraId, camera3_stream_t* stream, IPU3NodeNames nodeName) :
                FrameWorker(node, cameraId, "OutputFrameWorker"),
                mOutputBuffer(nullptr),
                mStream(stream),
                mAllDone(false),
                mUseInternalBuffer(true),
                mNodeName(nodeName)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    if (mNode)
        LOG1("@%s, node name:%d, device name:%s", __FUNCTION__, nodeName, mNode->name());
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

    LOG1("@%s allocate format: %s size: %d %dx%d", __func__, v4l2Fmt2Str(mFormat.fmt.pix.pixelformat),
            mFormat.fmt.pix.sizeimage,
            mFormat.fmt.pix.width,
            mFormat.fmt.pix.height);

    ret = setWorkerDeviceBuffers(getDefaultMemoryType(mNodeName), OUTPUTFRAME_WORK_BUFFERS);
    if (ret != OK)
        return ret;

    // Use internal buffers
    mUseInternalBuffer =
        mStream && (mStream->format == HAL_PIXEL_FORMAT_BLOB || needRotation() > 0);
    if (mUseInternalBuffer) {
        return allocateWorkerBuffers(OUTPUTFRAME_WORK_BUFFERS);
    }

    mIndex = 0;

    return OK;
}

int OutputFrameWorker::needRotation()
{
    CheckError((mStream == nullptr), 0, "%s, mStream is nullptr", __FUNCTION__);

    if (mStream->stream_type != CAMERA3_STREAM_OUTPUT) {
        LOG1("%s, no need rotation for stream type %d", __FUNCTION__, mStream->stream_type);
        return 0;
    }

    if (mStream->crop_rotate_scale_degrees == CAMERA3_STREAM_ROTATION_90)
        return 90;
    else if (mStream->crop_rotate_scale_degrees == CAMERA3_STREAM_ROTATION_270)
        return 270;

    return 0;
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
                mBuffers[mIndex].bytesused = mFormat.fmt.pix.sizeimage;
                mBuffers[mIndex].m.userptr = (unsigned long int)buffer->data();
                mCameraBuffers.push_back(buffer);
                LOG2("mBuffers[mIndex].m.userptr: %p", mBuffers[mIndex].m.userptr);
            }
            status |= mNode->putFrame(&mBuffers[mIndex]);


            notFound = false;
            break;
        }
    }

    if (notFound) {
        LOG1("No work for this worker mStream: %p", mStream);
        mAllDone = true;
        mOutputBuffer = nullptr;
        mPollMe = false;
    }

    return (status < 0) ? status : OK;
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
        LOG1("No work for this worker");
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

    int angle = 0;
    if (mUseInternalBuffer && (angle = needRotation()) > 0) {
       rotateFrame(mOutputBuffer->format(), angle);
    }

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

status_t OutputFrameWorker::rotateFrame(int outFormat, int angle)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    // Check the output buffer resolution with device config resolution
    if ((mOutputBuffer->width() != mFormat.fmt.pix.height)
         || (mOutputBuffer->height() != mFormat.fmt.pix.width)) {
        LOGE("output resolution not match [%d x %d] vs [%d x %d]",
              mOutputBuffer->width(), mOutputBuffer->height(),
              mFormat.fmt.pix.width, mFormat.fmt.pix.height);
        return UNKNOWN_ERROR;
    }

    const uint8* inBuffer = (uint8*)mBuffers[0].m.userptr;
    uint8* outBuffer = (outFormat == HAL_PIXEL_FORMAT_BLOB)
                           ? (uint8*)(mCameraBuffers[0]->data())
                           : (uint8*)(mOutputBuffer->data());
    int outW = mOutputBuffer->width();
    int outH = mOutputBuffer->height();
    int outStride = (outFormat == HAL_PIXEL_FORMAT_BLOB)
                        ? mCameraBuffers[0]->stride()
                        : mOutputBuffer->stride();
    int inW = mFormat.fmt.pix.width;
    int inH = mFormat.fmt.pix.height;
    int inStride = mCameraBuffers[0]->stride();
    if (mRotateBuffer.size() < mFormat.fmt.pix.sizeimage) {
        mRotateBuffer.resize(mFormat.fmt.pix.sizeimage);
    }

    uint8* I420Buffer = mRotateBuffer.data();
    int ret = libyuv::NV12ToI420Rotate(inBuffer, inStride, inBuffer + inH * inStride, inStride,
                                       I420Buffer, outW,
                                       I420Buffer + outW * outH, outW / 2,
                                       I420Buffer + outW * outH * 5 / 4, outW / 2,
                                       inW, inH,
                                       (angle == 90) ? libyuv::RotationMode::kRotate90
                                                     : libyuv::RotationMode::kRotate270);
    CheckError((ret < 0), UNKNOWN_ERROR, "@%s, rotate fail [%d]!", __FUNCTION__, ret);

    ret = libyuv::I420ToNV12(I420Buffer, outW,
                             I420Buffer + outW * outH, outW / 2,
                             I420Buffer + outW * outH * 5 / 4, outW / 2,
                             outBuffer, outStride,
                             outBuffer +  outStride * outH, outStride,
                             outW, outH);
    CheckError((ret < 0), UNKNOWN_ERROR, "@%s, convert fail [%d]!", __FUNCTION__, ret);

    return OK;
}
} /* namespace camera2 */
} /* namespace android */
