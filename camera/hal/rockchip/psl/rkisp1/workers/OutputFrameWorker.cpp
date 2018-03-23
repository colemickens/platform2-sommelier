/*
 * Copyright (C) 2016-2017 Intel Corporation.
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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
#include "NodeTypes.h"
#include <libyuv.h>
#include <sys/mman.h>

namespace android {
namespace camera2 {

OutputFrameWorker::OutputFrameWorker(std::shared_ptr<V4L2VideoNode> node, int cameraId,
                camera3_stream_t* stream, NodeTypes nodeName, size_t pipelineDepth) :
                FrameWorker(node, cameraId, pipelineDepth, "OutputFrameWorker"),
                mOutputBuffer(nullptr),
                mWorkingBuffer(nullptr),
                mStream(stream),
                mNeedPostProcess(false),
                mNodeName(nodeName),
                mCameraThread("OutputFrameWorker"),
                mProcessor(cameraId),
                mPostProcFramePool("PostProcFramePool")
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    if (mNode)
        LOG1("@%s, node name:%d, device name:%s", __FUNCTION__, nodeName, mNode->name());

    if (!mCameraThread.Start()) {
        LOGE("Camera thread failed to start");
    }
}

OutputFrameWorker::~OutputFrameWorker()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    if (mOutputForListener.get() && mOutputForListener->isLocked()) {
        mOutputForListener->unlock();
    }

    mCameraThread.Stop();
}

status_t
OutputFrameWorker::handleMessageProcess(MessageProcess msg)
{
    status_t status = mStreamToSWProcessMap[msg.frame->stream]->processFrame(msg.frame->processBuffer,
                                                  msg.frame->listenBuffer,
                                                  msg.frame->processingSettings,
                                                  msg.frame->request);
    if (status != OK) {
        LOGE("@%s, process for listener %p failed! [%d]!", __FUNCTION__,
             msg.frame->stream, status);
        msg.frame->request->setError();
    }

    CameraStream *stream = msg.frame->listenBuffer->getOwner();
    stream->captureDone(msg.frame->listenBuffer, msg.frame->request);
    return status;
}

void OutputFrameWorker::addListener(camera3_stream_t* stream)
{
    if (stream != nullptr) {
        LOG1("stream %p has listener %p", mStream, stream);
        mListeners.push_back(stream);
    }
}

void OutputFrameWorker::clearListeners()
{
    mListeners.clear();
    mPostProcFramePool.deInit();
}


status_t OutputFrameWorker::allocListenerProcessBuffers()
{
    mPostProcFramePool.init(mPipelineDepth);
    for (size_t i = 0; i < mPipelineDepth; i++)
    {
        std::shared_ptr<PostProcFrame> frame = nullptr;
        mPostProcFramePool.acquireItem(frame);
        if (frame.get() == nullptr) {
            LOGE("postproc task busy, no idle postproc frame!");
            return UNKNOWN_ERROR;
        }
        frame->processBuffer = MemoryUtils::allocateHeapBuffer(mFormat.width(),
                                              mFormat.height(),
                                              mFormat.bytesperline(),
                                              mFormat.pixelformat(),
                                              mCameraId,
                                              PAGE_ALIGN(mFormat.sizeimage()));
        if (frame->processBuffer.get() == nullptr)
            return NO_MEMORY;

        LOG2("%s:%d: postproc buffer allocated, address(%p)", __func__, __LINE__, frame->processBuffer.get());
    }
    return OK;
}

status_t OutputFrameWorker::configure(std::shared_ptr<GraphConfig> &/*config*/)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);

    bool listenerNeedPostProcess = false;
    status_t ret = mNode->getFormat(mFormat);
    if (ret != OK)
        return ret;

    LOG1("@%s allocate format: %s size: %d %dx%d", __func__, v4l2Fmt2Str(mFormat.pixelformat()),
            mFormat.sizeimage(),
            mFormat.width(),
            mFormat.height());

    ret = mProcessor.configure(mStream, mFormat.width(),
                               mFormat.height());
    CheckError((ret != OK), ret, "@%s mProcessor.configure failed %d",
               __FUNCTION__, ret);
    mNeedPostProcess = mProcessor.needPostProcess();

    mIndex = 0;
    mOutputBuffers.resize(mPipelineDepth);
    mWorkingBuffers.resize(mPipelineDepth);

    ret = setWorkerDeviceBuffers(
        mNeedPostProcess ? V4L2_MEMORY_MMAP : getDefaultMemoryType(mNodeName));
    CheckError((ret != OK), ret, "@%s set worker device buffers failed.",
               __FUNCTION__);

    // Allocate internal buffer.
    if (mNeedPostProcess) {
        ret = allocateWorkerBuffers();
        CheckError((ret != OK), ret, "@%s failed to allocate internal buffer.",
                   __FUNCTION__);
    }

    mStreamToSWProcessMap.clear();
    for (size_t i = 0; i < mListeners.size(); i++) {
        camera3_stream_t* listener = mListeners[i];
        std::unique_ptr<SWPostProcessor> processor(new SWPostProcessor(mCameraId));
        processor->configure(listener, mFormat.width(),
                             mFormat.height());
        mStreamToSWProcessMap[listener] = std::move(processor);
        if (mStreamToSWProcessMap[listener]->needPostProcess())
            listenerNeedPostProcess = true;
    }
    if (listenerNeedPostProcess)
        allocListenerProcessBuffers();

    return OK;
}

status_t OutputFrameWorker::prepareRun(std::shared_ptr<DeviceMessage> msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    mMsg = msg;
    status_t status = NO_ERROR;

    mOutputBuffers[mIndex] = nullptr;
    mPollMe = false;

    if (!mStream)
        return NO_ERROR;

    Camera3Request* request = mMsg->cbMetadataMsg.request;
    request->setSequenceId(-1);

    std::shared_ptr<CameraBuffer> buffer = findBuffer(request, mStream);
    if (buffer.get()) {
        // Work for mStream
        LOG2("@%s, stream:%p, mStream:%p", __FUNCTION__,
             buffer->getOwner()->getStream(), mStream);
        status = prepareBuffer(buffer);
        if (status != NO_ERROR) {
            LOGE("prepare buffer error!");
            goto exit;
        }

        // If output format is something else than
        // NV21 or Android flexible YCbCr 4:2:0, return
        if (buffer->format() != HAL_PIXEL_FORMAT_YCrCb_420_SP &&
                buffer->format() != HAL_PIXEL_FORMAT_YCbCr_420_888 &&
                buffer->format() != HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED &&
            buffer->format() != HAL_PIXEL_FORMAT_BLOB)  {
            LOGE("Bad format %d", buffer->format());
            status = BAD_TYPE;
            goto exit;
        }

        mOutputBuffers[mIndex] = buffer;
        mPollMe = true;
    } else if (checkListenerBuffer(request)) {
        // Work for listeners
        LOG2("%s: stream %p works for listener only in req %d",
             __FUNCTION__, mStream, request->getId());
        mPollMe = true;
    } else {
        LOG2("No work for this worker mStream: %p", mStream);
        mPollMe = false;
        return NO_ERROR;
    }

    /*
     * store the buffer in a map where the key is the terminal UID
     */
    if (!mNeedPostProcess) {
        // Use stream buffer for zero-copy
        unsigned long userptr;
        if (buffer.get() == nullptr) {
            buffer = getOutputBufferForListener();
            CheckError((buffer.get() == nullptr), UNKNOWN_ERROR,
                       "failed to allocate listener buffer");
        }
        switch (mNode->getMemoryType()) {
        case V4L2_MEMORY_USERPTR:
            userptr = reinterpret_cast<unsigned long>(buffer->data());
            mBuffers[mIndex].userptr(userptr);
            LOG2("%s mBuffers[%d].userptr: 0x%lx",
                __FUNCTION__, mIndex, mBuffers[mIndex].userptr());
            break;
        case V4L2_MEMORY_DMABUF:
            mBuffers[mIndex].setFd(buffer->dmaBufFd(), 0);
            LOG2("%s mBuffers[%d].fd: %d", __FUNCTION__, mIndex, mBuffers[mIndex].fd());
            break;
        case V4L2_MEMORY_MMAP:
            LOG2("%s mBuffers[%d].offset: 0x%x",
                __FUNCTION__, mIndex, mBuffers[mIndex].offset());
            break;
        default:
            LOGE("%s unsupported memory type.", __FUNCTION__);
            status = BAD_VALUE;
            goto exit;
        }
        mWorkingBuffers[mIndex] = buffer;
    } else {
        mWorkingBuffers[mIndex] = mCameraBuffers[mIndex];
    }
    status |= mNode->putFrame(mBuffers[mIndex]);
    LOG2("%s:%d:instance(%p), requestId(%d), index(%d)", __func__, __LINE__, this, request->getId(), mIndex);
    mIndex = (mIndex + 1) % mPipelineDepth;

exit:
    if (status < 0)
        returnBuffers(true);

    return status < 0 ? status : OK;
}

status_t OutputFrameWorker::run()
{
    status_t status = NO_ERROR;
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    V4L2BufferInfo outBuf;

    if (!mDevError)
        status = mNode->grabFrame(&outBuf);


    // Update request sequence if needed
    Camera3Request* request = mMsg->cbMetadataMsg.request;
    int sequence = outBuf.vbuffer.sequence();
    if (request->sequenceId() < sequence)
        request->setSequenceId(sequence);

    int index = outBuf.vbuffer.index();
    if (mDevError) {
        for (int i = 0; i < mPipelineDepth; i++)
            if (mOutputBuffers[(i + mIndex) % mPipelineDepth]) {
                index = (i + mIndex) % mPipelineDepth;
                break;
            }
    }
    mOutputBuffer = mOutputBuffers[index];
    mWorkingBuffer = mWorkingBuffers[index];
    mOutputBuffers[index] = nullptr;
    mWorkingBuffers[index] = nullptr;

    ICaptureEventListener::CaptureMessage outMsg;
    outMsg.data.event.reqId = request->getId();
    outMsg.id = ICaptureEventListener::CAPTURE_MESSAGE_ID_EVENT;
    outMsg.data.event.type = ICaptureEventListener::CAPTURE_EVENT_SHUTTER;
    outMsg.data.event.timestamp = outBuf.vbuffer.timestamp();
    outMsg.data.event.sequence = outBuf.vbuffer.sequence();
    notifyListeners(&outMsg);

    LOG2("%s:%d:instance(%p), frame_id(%d), requestId(%d), index(%d)", __func__, __LINE__, this, outBuf.vbuffer.sequence(), request->getId(), index);

    if (status < 0)
        returnBuffers(true);

    return status < 0 ? status : OK;
}

status_t OutputFrameWorker::postRun()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    status_t status = OK;
    CameraStream *stream;
    Camera3Request* request = nullptr;

    if (mMsg == nullptr) {
        LOGE("Message null - Fix the bug");
        status = UNKNOWN_ERROR;
        goto exit;
    }

    request = mMsg->cbMetadataMsg.request;
    if (request == nullptr) {
        LOGE("No request provided for captureDone");
        status = UNKNOWN_ERROR;
        goto exit;
    }

    // Handle for listeners at first
    for (size_t i = 0; i < mListeners.size(); i++) {
        camera3_stream_t* listener = mListeners[i];
        std::shared_ptr<CameraBuffer> listenerBuf = findBuffer(request, listener);
        if (listenerBuf.get() == nullptr) {
            continue;
        }

        stream = listenerBuf->getOwner();
        if (NO_ERROR != prepareBuffer(listenerBuf)) {
            LOGE("prepare listener buffer error!");
            listenerBuf->getOwner()->captureDone(listenerBuf, request);
            status = UNKNOWN_ERROR;
            continue;
        }
        if (mStreamToSWProcessMap[listener]->needPostProcess()) {
            MessageProcess msg;
            mPostProcFramePool.acquireItem(msg.frame);

            msg.frame->request = request;
            msg.frame->stream = listener;
            msg.frame->processingSettings = mMsg->pMsg.processingSettings;
            msg.frame->listenBuffer = listenerBuf;

            MEMCPY_S(msg.frame->processBuffer->data(), msg.frame->processBuffer->size(),
                     mWorkingBuffer->data(), mWorkingBuffer->size());

            base::Callback<status_t()> closure =
                base::Bind(&OutputFrameWorker::handleMessageProcess,
                        base::Unretained(this),
                        base::Passed(std::move(msg)));
            mCameraThread.PostTaskAsync(FROM_HERE, closure);
        } else {
            MEMCPY_S(listenerBuf->data(), listenerBuf->size(),
                     mWorkingBuffer->data(), mWorkingBuffer->size());
            stream->captureDone(listenerBuf, request);
        }
    }
    if (status != OK)
        goto exit;

    // All done
    if (mOutputBuffer == nullptr)
        goto exit;

    stream = mOutputBuffer->getOwner();
    if (mNeedPostProcess) {
        status = mProcessor.processFrame(mWorkingBuffer,
                                         mOutputBuffer,
                                         mMsg->pMsg.processingSettings,
                                         request);
    }
    if (status != OK) {
        LOGE("@%s, postprocess failed! [%d]!", __FUNCTION__, status);
        goto exit;
    }

    // Dump the buffers if enabled in flags
    if (mOutputBuffer->format() == HAL_PIXEL_FORMAT_BLOB) {
        mOutputBuffer->dumpImage(CAMERA_DUMP_JPEG, ".jpg");
    } else if (mOutputBuffer->format() ==
                              HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED
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

    if (status != OK)
        returnBuffers(false);

    return status;
}

bool OutputFrameWorker::isHalUsingRequestBuffer()
{
    LOG2("%s, mNeedPostProcess %d, mListeners.size() %zu",
          __FUNCTION__, mNeedPostProcess, mListeners.size());
    return (mNeedPostProcess || mListeners.size() > 0);
}

void OutputFrameWorker::returnBuffers(bool returnListenerBuffers)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    if (!mMsg || !mMsg->cbMetadataMsg.request)
        return;

    Camera3Request* request = mMsg->cbMetadataMsg.request;
    std::shared_ptr<CameraBuffer> buffer;

    buffer = findBuffer(request, mStream);
    if (buffer.get() && buffer->isRegistered())
        buffer->getOwner()->captureDone(buffer, request);

    if (!returnListenerBuffers)
        return;

    for (size_t i = 0; i < mListeners.size(); i++) {
        camera3_stream_t* listener = mListeners[i];
        buffer = findBuffer(request, listener);
        if (buffer.get() == nullptr || !buffer->isRegistered())
            continue;

        buffer->getOwner()->captureDone(buffer, request);
    }
}

status_t
OutputFrameWorker::prepareBuffer(std::shared_ptr<CameraBuffer>& buffer)
{
    CheckError((buffer.get() == nullptr), UNKNOWN_ERROR, "null buffer!");

    status_t status = NO_ERROR;
    if (!buffer->isLocked() && isHalUsingRequestBuffer()) {
        status = buffer->lock();
        if (CC_UNLIKELY(status != NO_ERROR)) {
            LOGE("Could not lock the buffer error %d", status);
            return UNKNOWN_ERROR;
        }
    }
    status = buffer->waitOnAcquireFence();
    if (CC_UNLIKELY(status != NO_ERROR)) {
        LOGW("Wait on fence for buffer %p timed out", buffer.get());
    }
    return status;
}

std::shared_ptr<CameraBuffer>
OutputFrameWorker::findBuffer(Camera3Request* request,
                              camera3_stream_t* stream)
{
    CheckError((request == nullptr || stream == nullptr), nullptr,
                "null request/stream!");

    CameraStream *s = nullptr;
    std::shared_ptr<CameraBuffer> buffer = nullptr;
    const std::vector<camera3_stream_buffer>* outBufs =
                                        request->getOutputBuffers();
    for (camera3_stream_buffer outputBuffer : *outBufs) {
        s = reinterpret_cast<CameraStream *>(outputBuffer.stream->priv);
        if (s->getStream() == stream) {
            buffer = request->findBuffer(s, false);
            if (CC_UNLIKELY(buffer == nullptr)) {
                LOGW("buffer not found for stream");
            }
            break;
        }
    }

    if (buffer.get() == nullptr) {
        LOG2("No buffer for stream %p in req %d", stream, request->getId());
    }
    return buffer;
}

bool OutputFrameWorker::checkListenerBuffer(Camera3Request* request)
{
    bool required = false;
    for (auto* s : mListeners) {
        std::shared_ptr<CameraBuffer> listenerBuf = findBuffer(request, s);
        if (listenerBuf.get()) {
            required = true;
            break;
        }
    }

    LOG2("%s, required is %s", __FUNCTION__, (required ? "true" : "false"));
    return required;
}

std::shared_ptr<CameraBuffer>
OutputFrameWorker::getOutputBufferForListener()
{
    // mOutputForListener buffer infor is same with mOutputBuffer,
    // and only allocated once
    if (mOutputForListener.get() == nullptr) {
        // Allocate buffer for listeners
        if (mNode->getMemoryType() == V4L2_MEMORY_DMABUF) {
            mOutputForListener = MemoryUtils::allocateHandleBuffer(
                    mFormat.width(),
                    mFormat.height(),
                    HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
                    GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_HW_CAMERA_WRITE,
                    mCameraId);
        } else if (mNode->getMemoryType() == V4L2_MEMORY_MMAP) {
            mOutputForListener = std::make_shared<CameraBuffer>(
                    mFormat.width(),
                    mFormat.height(),
                    mFormat.bytesperline(),
                    mNode->getFd(), -1, // dmabuf fd is not required.
                    mBuffers[0].length(),
                    mFormat.pixelformat(),
                    mBuffers[0].offset(), PROT_READ | PROT_WRITE, MAP_SHARED);
        } else if (mNode->getMemoryType() == V4L2_MEMORY_USERPTR) {
            mOutputForListener = MemoryUtils::allocateHeapBuffer(
                    mFormat.width(),
                    mFormat.height(),
                    mFormat.bytesperline(),
                    mFormat.pixelformat(),
                    mCameraId,
                    mBuffers[0].length());
        } else {
            LOGE("bad type for stream buffer %d", mNode->getMemoryType());
            return nullptr;
        }
        CheckError((mOutputForListener.get() == nullptr), nullptr,
                   "Can't allocate buffer for listeners!");
    }

    if (!mOutputForListener->isLocked()) {
        mOutputForListener->lock();
    }

    LOG2("%s, get output buffer for Listeners", __FUNCTION__);
    return mOutputForListener;
}

OutputFrameWorker::SWPostProcessor::SWPostProcessor(int cameraId) :
    mCameraId(cameraId),
    mProcessType(PROCESS_NONE),
    mStream(nullptr)
{
}

OutputFrameWorker::SWPostProcessor::~SWPostProcessor()
{
    mPostProcessBufs.clear();
}

status_t OutputFrameWorker::SWPostProcessor::configure(
                               camera3_stream_t* outStream,
                               int inputW, int inputH,
                               int inputFmt)
{
    mProcessType = PROCESS_NONE;

    if (outStream == nullptr) {
        LOG1("%s, stream is nullptr", __FUNCTION__);
        return OK;
    }
    // Support NV12 only
    CheckError((inputFmt != V4L2_PIX_FMT_NV12), BAD_VALUE,
            "Don't support format 0x%x", inputFmt);

    int type = PROCESS_NONE;
    if (getRotationDegrees(outStream) > 0) {
        type |= PROCESS_CROP_ROTATE_SCALE;
    }
    if (outStream->format == HAL_PIXEL_FORMAT_BLOB) {
        type |= PROCESS_JPEG_ENCODING;
    }
    if (inputW * inputH < outStream->width * outStream->height) {
        type |= PROCESS_SCALING;
    } else if (!(type & PROCESS_JPEG_ENCODING)
         && inputW * inputH > outStream->width * outStream->height) {
        // Don't need dowscaling for jpeg, because jpeg encoder support it
        type |= PROCESS_SCALING;
    }

    if ((type & PROCESS_JPEG_ENCODING) && !mJpegTask.get()) {
        LOG2("Create JpegEncodeTask");
        mJpegTask.reset(new JpegEncodeTask(mCameraId));

        if (mJpegTask->init() != NO_ERROR) {
            LOGE("Failed to init JpegEncodeTask Task");
            mJpegTask.reset();
            return UNKNOWN_ERROR;
        }
    }

    LOG1("%s: postprocess type 0x%x for stream %p", __FUNCTION__,
         type, outStream);
    mProcessType = type;
    mStream = outStream;
    mPostProcessBufs.clear();

    return OK;
}

status_t OutputFrameWorker::SWPostProcessor::processFrame(
                               std::shared_ptr<CameraBuffer>& input,
                               std::shared_ptr<CameraBuffer>& output,
                               std::shared_ptr<ProcUnitSettings>& settings,
                               Camera3Request* request)
{
    if (mProcessType == PROCESS_NONE) {
        return NO_ERROR;
    }

    status_t status = OK;

    // Rotate input buffer is mPostProcessBufs[0]
    // and output buffer will be mPostProcessBufs[1] or directly mOutputBuffer
    if (mProcessType & PROCESS_CROP_ROTATE_SCALE) {
        int angle = getRotationDegrees(mStream);
        // Check if any post-processing needed after rotate
        if (mProcessType & PROCESS_JPEG_ENCODING
            || mProcessType & PROCESS_SCALING) {
            if (mPostProcessBufs.empty()
                || mPostProcessBufs[0]->width() != input->width()
                || mPostProcessBufs[0]->height() != input->height()) {
                mPostProcessBufs.clear();
                // Create rotate output working buffer
                std::shared_ptr<CameraBuffer> buf;
                buf = MemoryUtils::allocateHeapBuffer(
                         input->width(),
                         input->height(),
                         input ->width(),
                         input->v4l2Fmt(),
                         mCameraId,
                         PAGE_ALIGN(input->size()));
                CheckError((buf.get() == nullptr), NO_MEMORY,
                           "@%s, No memory for rotate", __FUNCTION__);
                mPostProcessBufs.push_back(buf);
            }
            // Rotate to internal post-processing buffer
            status = cropRotateScaleFrame(input, mPostProcessBufs[0], angle);
        } else {
            // Rotate to internal post-processing buffer
            status = cropRotateScaleFrame(input, output, angle);
        }
        CheckError((status != OK), status, "@%s, Scale frame failed! [%d]!",
                   __FUNCTION__, status);
    } else {
        if (!mPostProcessBufs.empty())
            mPostProcessBufs.erase(mPostProcessBufs.begin());

        mPostProcessBufs.insert(mPostProcessBufs.begin(), input);
    }

    // Scale input buffer is mPostProcessBufs[0]
    // and output buffer will be mPostProcessBufs[1] or directly mOutputBuffer
    if (mProcessType & PROCESS_SCALING) {
        if (mProcessType & PROCESS_JPEG_ENCODING) {
            if (mPostProcessBufs.empty()
                || mPostProcessBufs.back()->width() != mStream->width
                || mPostProcessBufs.back()->height() != mStream->height) {
                // Create scale output working buffer
                std::shared_ptr<CameraBuffer> buf;
                buf = MemoryUtils::allocateHeapBuffer(
                         mStream->width,
                         mStream->height,
                         mStream->width,
                         mPostProcessBufs.back()->v4l2Fmt(),
                         mCameraId,
                         PAGE_ALIGN(mStream->width * mStream->height * 3 / 2));
                CheckError((buf.get() == nullptr), NO_MEMORY,
                           "@%s, No memory for scale", __FUNCTION__);
                mPostProcessBufs.push_back(buf);
            }
            // Scale to internal post-processing buffer
            status = scaleFrame(mPostProcessBufs[0], mPostProcessBufs[1]);
        } else {
            // Scale to output dst buffer
            status = scaleFrame(mPostProcessBufs[0], output);
        }
        CheckError((status != OK), status, "@%s, Scale frame failed! [%d]!",
                   __FUNCTION__, status);
    }

    // Jpeg input buffer is always mPostProcessBufs.back()
    if (mProcessType & PROCESS_JPEG_ENCODING) {
        mPostProcessBufs.back()->dumpImage(CAMERA_DUMP_JPEG,
                                           "before_jpeg_converion_nv12");
        // JPEG encoding
        status = mJpegTask->handleMessageSettings(*(settings.get()));
        CheckError((status != OK), status, "@%s, set settings failed! [%d]!",
                   __FUNCTION__, status);
        status = convertJpeg(mPostProcessBufs.back(), output, request);
        CheckError((status != OK), status, "@%s, JPEG conversion failed! [%d]!",
                   __FUNCTION__, status);
    }

    return status;
}

int OutputFrameWorker::SWPostProcessor::getRotationDegrees(
                                  camera3_stream_t* stream) const
{
    CheckError((stream == nullptr), 0, "%s, stream is nullptr", __FUNCTION__);

    if (stream->stream_type != CAMERA3_STREAM_OUTPUT) {
        LOG1("%s, no need rotation for stream type %d", __FUNCTION__,
             stream->stream_type);
        return 0;
    }

    if (stream->crop_rotate_scale_degrees == CAMERA3_STREAM_ROTATION_90)
        return 90;
    else if (stream->crop_rotate_scale_degrees == CAMERA3_STREAM_ROTATION_270)
        return 270;

    return 0;
}

/**
 * Do jpeg conversion
 * \param[in] input
 * \param[in] output
 * \param[in] request
 */
status_t OutputFrameWorker::SWPostProcessor::convertJpeg(
                               std::shared_ptr<CameraBuffer> input,
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

status_t OutputFrameWorker::SWPostProcessor::cropRotateScaleFrame(
                               std::shared_ptr<CameraBuffer> input,
                               std::shared_ptr<CameraBuffer> output,
                               int angle)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    // Check the output buffer resolution with device config resolution
    CheckError(
        (output->width() != input->width()
         || output->height() != input->height()),
        UNKNOWN_ERROR, "output resolution mis-match [%d x %d] -> [%d x %d]",
        input->width(), input->height(),
        output->width(), output->height());

    int width = input->width();
    int height = input->height();
    int inStride = input->stride();
    const uint8* inBuffer = (uint8*)(input->data());
    int cropped_width = height * height / width;
    if (cropped_width % 2 == 1) {
      // Make cropped_width to the closest even number.
      cropped_width++;
    }
    int cropped_height = height;
    int margin = (width - cropped_width) / 2;

    int rotated_height = cropped_width;
    int rotated_width = cropped_height;

    libyuv::RotationMode rotation_mode = libyuv::RotationMode::kRotate90;
    switch (angle) {
      case 90:
        rotation_mode = libyuv::RotationMode::kRotate90;
        break;
      case 270:
        rotation_mode = libyuv::RotationMode::kRotate270;
        break;
      default:
        LOGE("error rotation degree %d", angle);
        return -EINVAL;
    }
    // This libyuv method first crops the frame and then rotates it 90 degrees
    // clockwise or counterclockwise.
    if (mRotateBuffer.size() < input->size()) {
        mRotateBuffer.resize(input->size());
    }

    uint8* I420RotateBuffer = mRotateBuffer.data();
    int ret = libyuv::ConvertToI420(
        inBuffer, inStride,
        I420RotateBuffer, rotated_width,
        I420RotateBuffer + rotated_width * rotated_height, rotated_width / 2,
        I420RotateBuffer + rotated_width * rotated_height * 5 / 4, rotated_width / 2, margin,
        0, width, height, cropped_width, cropped_height, rotation_mode,
        libyuv::FourCC::FOURCC_I420);
    if (ret) {
      LOGE("ConvertToI420 failed: %d", ret);
      return ret;
    }

    if (mScaleBuffer.size() < input->size()) {
        mScaleBuffer.resize(input->size());
    }

    uint8* I420ScaleBuffer = mScaleBuffer.data();
    ret = libyuv::I420Scale(
        I420RotateBuffer, rotated_width,
        I420RotateBuffer + rotated_width * rotated_height, rotated_width / 2,
        I420RotateBuffer + rotated_width * rotated_height * 5 / 4, rotated_width / 2,
        rotated_width, rotated_height,
        I420ScaleBuffer, width,
        I420ScaleBuffer + width * height, width / 2,
        I420ScaleBuffer + width * height * 5 / 4, width / 2,
        width, height,
        libyuv::FilterMode::kFilterNone);
    if (ret) {
      LOGE("I420Scale failed: %d", ret);
    }
    //convert to NV12
    uint8* outBuffer = (uint8*)(output->data());
    int outStride = output->stride();
    ret = libyuv::I420ToNV12(I420ScaleBuffer, width,
                             I420ScaleBuffer + width * height, width / 2,
                             I420ScaleBuffer + width * height * 5 / 4, width / 2,
                             outBuffer, outStride,
                             outBuffer +  outStride * height, outStride,
                             width, height);
    return ret;
}

status_t OutputFrameWorker::SWPostProcessor::scaleFrame(
                               std::shared_ptr<CameraBuffer> input,
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
