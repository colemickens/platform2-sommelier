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
#include <sys/mman.h>

namespace android {
namespace camera2 {

OutputFrameWorker::OutputFrameWorker(std::shared_ptr<V4L2VideoNode> node, int cameraId,
                camera3_stream_t* stream, IPU3NodeNames nodeName, size_t pipelineDepth) :
                FrameWorker(node, cameraId, pipelineDepth, "OutputFrameWorker"),
                mOutputBuffer(nullptr),
                mWorkingBuffer(nullptr),
                mStream(stream),
                mAllDone(false),
                mPostProcessType(PROCESS_NONE),
                mNodeName(nodeName)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    if (mNode)
        LOG1("@%s, node name:%d, device name:%s", __FUNCTION__, nodeName, mNode->name());
}

OutputFrameWorker::~OutputFrameWorker()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    mPostProcessBufs.clear();
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

    mPostProcessType = PROCESS_NONE;
    if (mStream) {
        if (needRotation() > 0) {
            LOG2("need rotation");
            mPostProcessType |= PROCESS_ROTATE;
        }
        if (mStream->format == HAL_PIXEL_FORMAT_BLOB) {
            LOG2("need jpeg encoding");
            mPostProcessType |= PROCESS_JPEG_ENCODING;
        }
        if ((!(mPostProcessType & PROCESS_JPEG_ENCODING)
             && mFormat.fmt.pix.width * mFormat.fmt.pix.height
                 > mStream->width * mStream->height)
            || mFormat.fmt.pix.width * mFormat.fmt.pix.height
                < mStream->width * mStream->height) {
            // Down-scaling only for non-jpeg encoding since encoder can support this
            // Up-scaling for ISP output resolution is smaller than required in stream
            LOG2("need scaling: %dx%d -> %dx%d",
                    mFormat.fmt.pix.width, mFormat.fmt.pix.height,
                    mStream->width, mStream->height);
            mPostProcessType |= PROCESS_SCALING;
        }
    }

    LOG1("%s: process type 0x%x", __FUNCTION__, mPostProcessType);

    mIndex = 0;
    // If using internal buffer, only one buffer is required.
    if (mPostProcessType != PROCESS_NONE)
        mPipelineDepth = 1;
    ret = setWorkerDeviceBuffers(
        mPostProcessType != PROCESS_NONE ? V4L2_MEMORY_MMAP : getDefaultMemoryType(mNodeName));
    CheckError((ret != OK), ret, "@%s set worker device buffers failed.", __FUNCTION__);

    // Allocate internal buffer.
    if (mPostProcessType != PROCESS_NONE) {
        mWorkingBuffer = std::make_shared<CameraBuffer>(mFormat.fmt.pix.width,
                mFormat.fmt.pix.height,
                mFormat.fmt.pix.bytesperline,
                mNode->getFd(), -1, // dmabuf fd is not required.
                mBuffers[0].length,
                mFormat.fmt.pix.pixelformat,
                mBuffers[0].m.offset, PROT_READ | PROT_WRITE, MAP_SHARED);
        CheckError((mWorkingBuffer.get() == nullptr), NO_MEMORY,
            "@%s failed to allocate internal buffer.", __FUNCTION__);
    }

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
                if (!mJpegTask.get()) {
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
            mAllDone = false;
            mPollMe = true;

            if (mPostProcessType == PROCESS_NONE) {
                // Use stream buffer for zero-copy
                mBuffers[mIndex].bytesused = mFormat.fmt.pix.sizeimage;
                switch (mNode->getMemoryType()) {
                case V4L2_MEMORY_USERPTR:
                    mBuffers[mIndex].m.userptr = reinterpret_cast<unsigned long>(buffer->data());
                    LOG2("%s mBuffers[%d].m.userptr: %p",
                        __FUNCTION__, mIndex, mBuffers[mIndex].m.userptr);
                    break;
                case V4L2_MEMORY_DMABUF:
                    mBuffers[mIndex].m.fd = buffer->dmaBufFd();
                    LOG2("%s mBuffers[%d].m.fd: %d", __FUNCTION__, mIndex, mBuffers[mIndex].m.fd);
                    break;
                case V4L2_MEMORY_MMAP:
                    LOG2("%s mBuffers[%d].m.offset: 0x%x",
                        __FUNCTION__, mIndex, mBuffers[mIndex].m.offset);
                    break;
                default:
                    LOGE("%s unsupported memory type.", __FUNCTION__);
                    return BAD_VALUE;
                }
                mWorkingBuffer = buffer;
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

    ICaptureEventListener::CaptureMessage outMsg;
    outMsg.id = ICaptureEventListener::CAPTURE_MESSAGE_ID_EVENT;
    outMsg.data.event.type = ICaptureEventListener::CAPTURE_EVENT_YUV;
    outMsg.data.event.yuvBuffer = mWorkingBuffer;
    notifyListeners(&outMsg);
    return (status < 0) ? status : OK;
}

status_t OutputFrameWorker::postRun()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    status_t status = OK;
    int angle = 0;

    if (mMsg == nullptr) {
        LOGE("Message null - Fix the bug");
        status = UNKNOWN_ERROR;
        goto exit;
    }

    Camera3Request* request;
    CameraStream *stream;

    request = mMsg->cbMetadataMsg.request;

    if (mAllDone) {
        mAllDone = false;
        goto exit;
    }

    if (mOutputBuffer == nullptr) {
        LOGE("No buffer provided for captureDone");
        status = UNKNOWN_ERROR;
        goto exit;
    }
    if (request == nullptr) {
        LOGE("No request provided for captureDone");
        status = UNKNOWN_ERROR;
        goto exit;
    }

    stream = mOutputBuffer->getOwner();

    // Rotate input buffer is always mWorkingBuffer
    // and output buffer will be mPostProcessBufs[0] or directly mOutputBuffer
    if (mPostProcessType & PROCESS_ROTATE) {
        int angle = needRotation();
        // Check if any post-processing needed after rotate
        if (mPostProcessType & PROCESS_JPEG_ENCODING
            || mPostProcessType & PROCESS_SCALING) {
            if (mPostProcessBufs.empty()
                || mPostProcessBufs[0]->width() != mFormat.fmt.pix.height
                || mPostProcessBufs[0]->height() != mFormat.fmt.pix.width) {
                mPostProcessBufs.clear();
                // Create rotate output working buffer
                std::shared_ptr<CameraBuffer> buf;
                buf = MemoryUtils::allocateHeapBuffer(mFormat.fmt.pix.height,
                                   mFormat.fmt.pix.width,
                                   mFormat.fmt.pix.height,
                                   mFormat.fmt.pix.pixelformat,
                                   mCameraId,
                                   PAGE_ALIGN(mFormat.fmt.pix.sizeimage));
                CheckError((buf.get() == nullptr), NO_MEMORY,
                           "@%s, No memory for rotate", __FUNCTION__);
                mPostProcessBufs.push_back(buf);
            }
            // Rotate to internal post-processing buffer
            status = rotateFrame(mWorkingBuffer, mPostProcessBufs[0], angle);
        } else {
            // Rotate to output dst buffer
            status = rotateFrame(mWorkingBuffer, mOutputBuffer, angle);
        }
        if (status != OK) {
            LOGE("@%s, Rotate frame failed! [%d]!", __FUNCTION__, status);
            goto exit;
        }
    } else {
        mPostProcessBufs.push_back(mWorkingBuffer);
    }

    // Scale input buffer is mPostProcessBufs[0]
    // and output buffer will be mPostProcessBufs[1] or directly mOutputBuffer
    if (mPostProcessType & PROCESS_SCALING) {
        if (mPostProcessType & PROCESS_JPEG_ENCODING
            && (mPostProcessBufs.empty()
                || mPostProcessBufs.back()->width() != mStream->width
                || mPostProcessBufs.back()->height() != mStream->height)) {
            // Create scale output working buffer
            std::shared_ptr<CameraBuffer> buf;
            buf = MemoryUtils::allocateHeapBuffer(mStream->width,
                               mStream->height,
                               mStream->width,
                               mFormat.fmt.pix.pixelformat,
                               mCameraId,
                               PAGE_ALIGN(mStream->width * mStream->height * 3 / 2));
            CheckError((buf.get() == nullptr), NO_MEMORY,
                       "@%s, No memory for scale", __FUNCTION__);
            mPostProcessBufs.push_back(buf);
            // Scale to internal post-processing buffer
             status = scaleFrame(mPostProcessBufs[0], mPostProcessBufs[1]);
        } else {
            // Scale to output dst buffer
            status = scaleFrame(mPostProcessBufs[0], mOutputBuffer);
        }
        if (status != OK) {
            LOGE("@%s, Scale frame failed! [%d]!", __FUNCTION__, status);
            goto exit;
        }
    }

    // Jpeg input buffer is always mPostProcessBufs.back()
    if (mPostProcessType & PROCESS_JPEG_ENCODING) {
        // JPEG encoding
        status = convertJpeg(mPostProcessBufs.back(), mOutputBuffer, request);
        if (status != OK) {
            LOGE("@%s, JPEG conversion failed! [%d]!", __FUNCTION__, status);
            goto exit;
        }
        mOutputBuffer->dumpImage(CAMERA_DUMP_JPEG, ".jpg");
    }

    // Dump the buffers if enabled in flags
    if (mOutputBuffer->format() == HAL_PIXEL_FORMAT_BLOB) {
        // Use internal buffer
        mPostProcessBufs.back()->dumpImage(CAMERA_DUMP_JPEG, "before_jpeg_converion_nv12");
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

exit:
    /* Prevent from using old data */
    mMsg = nullptr;
    mOutputBuffer = nullptr;
    mIndex = (mIndex + 1) % mPipelineDepth;

    return status;
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

    status_t status = NO_ERROR;
    if (mJpegTask.get()) {
        status = mJpegTask->handleMessageNewJpegInput(msg);
    }

    return status;
}

status_t OutputFrameWorker::rotateFrame(std::shared_ptr<CameraBuffer> input,
                                        std::shared_ptr<CameraBuffer> output,
                                        int angle)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    // Check the output buffer resolution with device config resolution
    CheckError((output->width() != input->height() || output->height() != input->width()),
               UNKNOWN_ERROR, "output resolution mis-match [%d x %d] -> [%d x %d]",
               input->width(), input->height(),
               output->width(), output->height());

    const uint8* inBuffer = (uint8*)(input->data());
    uint8* outBuffer = (uint8*)(output->data());
    int outW = output->width();
    int outH = output->height();
    int outStride = output->stride();
    int inW = input->width();
    int inH = input->height();
    int inStride = input->stride();
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

status_t OutputFrameWorker::scaleFrame(std::shared_ptr<CameraBuffer> input,
                                       std::shared_ptr<CameraBuffer> output)
{
    // Y plane
    libyuv::ScalePlane((uint8*)input->data(),
                input->stride(),
                input->width(),
                input->height(),
                (uint8*)output->data(),
                output->stride(),
                output->width(),
                output->height(),
                libyuv::kFilterNone);

    // UV plane
    // TODO: should get bpl to calculate offset
    int inUVOffsetByte = input->stride() * input->height();
    int outUVOffsetByte = output->stride() * output->height();
    libyuv::ScalePlane_16((uint16*)input->data() + inUVOffsetByte / 2,
                input->stride() / 2,
                input->width() / 2,
                input->height() / 2,
                (uint16*)output->data() + outUVOffsetByte / 2,
                output->stride() / 2,
                output->width() / 2,
                output->height() / 2,
                libyuv::kFilterNone);
    return OK;
}

} /* namespace camera2 */
} /* namespace android */
