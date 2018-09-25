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

#define LOG_TAG "OutputFrameWorker"

#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "OutputFrameWorker.h"
#include "ColorConverter.h"
#include "NodeTypes.h"
#include "Camera3GFXFormat.h"
#include "ImageScalerCore.h"
#include <sys/mman.h>

namespace android {
namespace camera2 {

OutputFrameWorker::OutputFrameWorker(std::shared_ptr<cros::V4L2VideoNode> node, int cameraId,
                camera3_stream_t* stream, IPU3NodeNames nodeName, size_t pipelineDepth) :
                FrameWorker(node, cameraId, pipelineDepth, "OutputFrameWorker"),
                mStream(stream),
                mNeedPostProcess(false),
                mNodeName(nodeName),
                mProcessor(cameraId),
                mCameraThread("OutputFrameWorker" + std::to_string(nodeName))
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    if (mNode) {
        LOG1("@%s, node name:%d, device name:%s, mStream:%p", __FUNCTION__,
        nodeName, mNode->Name().c_str(), mStream);
    }
    if (stream) {
        LOG1("@%s, node name:%d, width:%d, height:%d, format:%x, type:%d", __FUNCTION__,
        nodeName, stream->width, stream->height, stream->format, stream->stream_type);
    }

    if (!mCameraThread.Start()) {
        LOGE("Camera thread failed to start");
    }
}

OutputFrameWorker::~OutputFrameWorker()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    mCameraThread.Stop();
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
}

status_t OutputFrameWorker::configure(std::shared_ptr<GraphConfig> &/*config*/)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);

    status_t ret = mNode->GetFormat(&mFormat);
    if (ret != OK)
        return ret;

    LOG1("@%s allocate format: %s size: %d %dx%d", __func__, v4l2Fmt2Str(mFormat.PixelFormat()),
            mFormat.SizeImage(0),
            mFormat.Width(),
            mFormat.Height());

    ret = mProcessor.configure(mStream, mFormat.Width(),
                               mFormat.Height());
    CheckError((ret != OK), ret, "@%s mProcessor.configure failed %d",
               __FUNCTION__, ret);
    mNeedPostProcess = mProcessor.needPostProcess();

    mIndex = 0;
    // If using internal buffer, only one buffer is required.
    ret = setWorkerDeviceBuffers(getDefaultMemoryType(mNodeName));
    CheckError((ret != OK), ret, "@%s set worker device buffers failed.",
               __FUNCTION__);

    // Allocate internal buffer.
    if (mNeedPostProcess || mListeners.size() > 0) {
        int gfxFormat = v4L2Fmt2GFXFmt(mFormat.PixelFormat());
        if (gfxFormat == HAL_PIXEL_FORMAT_NV12_LINEAR_CAMERA_INTEL) {
          // Buffer manager does not support
          // HAL_PIXEL_FORMAT_NV12_LINEAR_CAMERA_INTEL. Use fake
          // HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED instead.
          gfxFormat = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
        }
        ret = allocateWorkerBuffers(GRALLOC_USAGE_SW_READ_OFTEN |
                                    GRALLOC_USAGE_HW_CAMERA_WRITE, gfxFormat);
        CheckError(ret != OK, ret, "@%s failed to allocate internal buffer.",
                   __FUNCTION__);
        mInternalBuffers.clear();
        std::shared_ptr<CameraBuffer> buffer = nullptr;
        for (size_t i = 0; i < mPipelineDepth; i++) {
            buffer = std::make_shared<CameraBuffer>();
            buffer->init(mFormat.Width(), mFormat.Height(), gfxFormat,
                                 mBufferHandles[i], mCameraId);
            mInternalBuffers.push_back(buffer);
        }
    }

    mListenerProcessors.clear();
    for (size_t i = 0; i < mListeners.size(); i++) {
        camera3_stream_t* listener = mListeners[i];
        std::unique_ptr<SWPostProcessor> processor(new SWPostProcessor(mCameraId));
        processor->configure(listener, mFormat.Width(),
                             mFormat.Height());
        mListenerProcessors.push_back(std::move(processor));
    }

    return OK;
}

status_t OutputFrameWorker::prepareRun(std::shared_ptr<DeviceMessage> msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    mMsg = msg;
    mPollMe = false;
    status_t status = NO_ERROR;

    if (!mStream) {
        return NO_ERROR;
    }

    Camera3Request* request = mMsg->cbMetadataMsg.request;
    std::shared_ptr<CameraBuffer> buffer = findBuffer(request, mStream);
    if (buffer.get()) {
        // Work for mStream
        LOG2("@%s, stream:%p, mStream:%p", __FUNCTION__,
             buffer->getOwner()->getStream(), mStream);
        buffer->setRequestId(request->getId());
        status = prepareBuffer(buffer);
        if (status != NO_ERROR) {
            LOGE("prepare buffer error!");
            buffer->getOwner()->captureDone(buffer, request);
            return status;
        }
    } else if (!checkListenerBuffer(request)) {
        LOG2("No work for this worker mStream: %p", mStream);
        return NO_ERROR;
    }

    /*
     * store the buffer in a map where the key is the terminal UID
     */
    if (!mNeedPostProcess) {
        // Use stream buffer for zero-copy
        std::shared_ptr<CameraBuffer> listenerBuf = buffer;
        if (listenerBuf.get() == nullptr) {
            listenerBuf = mInternalBuffers[mIndex];
            CheckError((listenerBuf.get() == nullptr), UNKNOWN_ERROR,
                       "failed to allocate listener buffer");
        }
        mBuffers[mIndex].SetFd(listenerBuf->dmaBufFd(), 0);
    }
    LOG2("%s mBuffers[%d].fd: %d, %s", __FUNCTION__, mIndex, mBuffers[mIndex].Fd(0),
          mNode->Name().c_str());
    status |= mNode->PutFrame(&mBuffers[mIndex]);
    CheckError(status < 0, status, "failed to put frame");

    {
    std::lock_guard<std::mutex> l(mProcessingDataLock);
    ProcessingData processingData;
    processingData.mOutputBuffer = buffer;
    if (mNeedPostProcess || buffer.get() == nullptr)
        processingData.mWorkingBuffer = mInternalBuffers[mIndex];
    else
        processingData.mWorkingBuffer = buffer;
    processingData.mMsg = msg;

    mProcessingDataQueue.push(std::move(processingData));
    }

    mPollMe = true;
    return OK;
}

status_t OutputFrameWorker::run()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    if (mMsg == nullptr) {
        LOGE("Message not found - Fix the bug");
        return UNKNOWN_ERROR;
    }

    if (!mPollMe) {
        LOG1("No work for this worker");
        return OK;
    }

    cros::V4L2Buffer outBuf;
    LOG2("%s mBuffers[%d].fd: %d, %s", __FUNCTION__, mIndex, mBuffers[mIndex].Fd(0),
          mNode->Name().c_str());
    status_t status = mNode->GrabFrame(&outBuf);
    return (status < 0) ? status : OK;
}

status_t OutputFrameWorker::postRun()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    mIndex = (mIndex + 1) % mPipelineDepth;

    CheckError(mMsg == nullptr, UNKNOWN_ERROR, "Message null - Fix the bug");

    Camera3Request* request = mMsg->cbMetadataMsg.request;
    if (request == nullptr) {
        LOGE("No request provided for captureDone");
        mMsg = nullptr;
        return UNKNOWN_ERROR;
    }

    {
    std::lock_guard<std::mutex> l(mProcessingDataLock);
    if (mProcessingDataQueue.empty()) {
        LOG1("No processing data available!");
        mMsg = nullptr;
        return OK;
    }
    }

    base::Callback<status_t()> closure =
            base::Bind(&OutputFrameWorker::handlePostRun, base::Unretained(this));

    mCameraThread.PostTaskAsync<status_t>(FROM_HERE, closure);

    mMsg = nullptr;
    return OK;
}

status_t OutputFrameWorker::handlePostRun()
{
    status_t status = OK;
    CameraStream *stream;
    Camera3Request* request = nullptr;

    ProcessingData processingData;
    {
    std::lock_guard<std::mutex> l(mProcessingDataLock);
    LOG2("%s, line %d, queue size %ld", __func__, __LINE__, mProcessingDataQueue.size());
    processingData = std::move(mProcessingDataQueue.front());
    mProcessingDataQueue.pop();
    }

    request = processingData.mMsg->cbMetadataMsg.request;
    // Handle for listeners at first
    for (size_t i = 0; i < mListeners.size(); i++) {
        camera3_stream_t* listener = mListeners[i];
        std::shared_ptr<CameraBuffer> listenerBuf = findBuffer(request, listener);
        if (listenerBuf.get() == nullptr) {
            continue;
        }

        listenerBuf->setRequestId(request->seqenceId());

        status = prepareBuffer(listenerBuf);
        CheckError(status != NO_ERROR, status, "prepare listener buffer error!");

        stream = listenerBuf->getOwner();
        if (mListenerProcessors[i]->needPostProcess()) {
            status = mListenerProcessors[i]->processFrame(
                                                 processingData.mWorkingBuffer,
                                                 listenerBuf,
                                                 processingData.mMsg->pMsg.processingSettings,
                                                 request);
            CheckError(status != OK, status, "@%s, process for listener %p failed! [%d]!", __FUNCTION__,
                     listener, status);
        } else {
            if (!processingData.mWorkingBuffer->isLocked()) {
                status_t ret = processingData.mWorkingBuffer->lock();
                CheckError(ret != NO_ERROR, NO_MEMORY, "@%s, lock fails", __FUNCTION__);
            }

            MEMCPY_S(listenerBuf->data(), listenerBuf->size(),
                     processingData.mWorkingBuffer->data(), processingData.mWorkingBuffer->size());
        }

        dump(listenerBuf, stream);

        stream->captureDone(listenerBuf, request);
        LOG2("%s, line %d, req id %d frameDone",  __func__, __LINE__, request->seqenceId());
    }

    if (processingData.mOutputBuffer == nullptr) {
        LOG2("No buffer provided for captureDone");
        return OK;
    }

    stream = processingData.mOutputBuffer->getOwner();
    if (mNeedPostProcess) {
        status = mProcessor.processFrame(processingData.mWorkingBuffer,
                                         processingData.mOutputBuffer,
                                         processingData.mMsg->pMsg.processingSettings,
                                         request);
        CheckError(status != OK, status, "@%s, postprocess failed! [%d]!", __FUNCTION__, status);
    } else {
        bool hasInputBuf = request->hasInputBuf();
        if (hasInputBuf) {
            const camera3_stream_buffer* inputBuf = request->getInputBuffer();
            CheckError(!inputBuf, UNKNOWN_ERROR, "@%s, getInputBuffer fails", __FUNCTION__);

            int fmt = inputBuf->stream->format;
            CheckError((fmt != HAL_PIXEL_FORMAT_YCbCr_420_888), UNKNOWN_ERROR,
                "@%s, input stream is not YCbCr_420_888, format:%x", __FUNCTION__, fmt);

            const CameraStreamNode* s = request->getInputStream();
            CheckError(!s, UNKNOWN_ERROR, "@%s, getInputStream fails", __FUNCTION__);

            std::shared_ptr<CameraBuffer> buf = request->findBuffer(s);
            CheckError((buf == nullptr), UNKNOWN_ERROR, "@%s, findBuffer fails", __FUNCTION__);

            buf->getOwner()->captureDone(buf, request);
        }
    }

    dump(processingData.mOutputBuffer, stream);

    // call capturedone for the stream of the buffer
    stream->captureDone(processingData.mOutputBuffer, request);
    LOG2("%s, line %d, req id %d frameDone",  __func__, __LINE__, request->seqenceId());

    return status;
}

bool OutputFrameWorker::isHalUsingRequestBuffer()
{
    LOG2("%s, mNeedPostProcess %d, mListeners.size() %zu",
          __FUNCTION__, mNeedPostProcess, mListeners.size());
    return (mNeedPostProcess || mListeners.size() > 0);
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
    const std::vector<camera3_stream_buffer>* outBufs = request->getOutputBuffers();
    CheckError(outBufs == nullptr, nullptr, "@%s: outBufs is nullptr", __FUNCTION__);

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

void OutputFrameWorker::dump(std::shared_ptr<CameraBuffer> buf, const CameraStream* stream)
{
    CheckError(stream == nullptr, VOID_VALUE, "@%s, stream is nullptr", __FUNCTION__);
    LOG2("@%s", __FUNCTION__);

    if (buf->format() == HAL_PIXEL_FORMAT_BLOB) {
        buf->dumpImage(CAMERA_DUMP_JPEG, ".jpg");
    } else if (buf->format() == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED ||
               buf->format() == HAL_PIXEL_FORMAT_YCbCr_420_888) {
        if (stream->usage() & GRALLOC_USAGE_HW_VIDEO_ENCODER) {
            buf->dumpImage(CAMERA_DUMP_VIDEO, "video.nv12");
        } else {
            buf->dumpImage(CAMERA_DUMP_PREVIEW, "preview.nv12");
        }
    }
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
        type |= PROCESS_ROTATE;
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
    // Rotate input buffer is always mWorkingBuffer
    // and output buffer will be mPostProcessBufs[0] or directly mOutputBuffer
    if (!input->isLocked()) {
      CheckError(input->lock() != NO_ERROR, NO_MEMORY,
                 "@%s, Failed to lock buffer", __FUNCTION__);
    }
    if (mProcessType & PROCESS_ROTATE) {
        int angle = getRotationDegrees(mStream);
        // Check if any post-processing needed after rotate
        if (mProcessType & PROCESS_JPEG_ENCODING
            || mProcessType & PROCESS_SCALING) {
            if (mPostProcessBufs.empty()
                || mPostProcessBufs[0]->width() != input->height()
                || mPostProcessBufs[0]->height() != input->width()) {
                mPostProcessBufs.clear();
                // Create rotate output working buffer
                std::shared_ptr<CameraBuffer> buf;
                buf = MemoryUtils::allocateHeapBuffer(input->height(),
                                   input->width(),
                                   input->height(),
                                   input->v4l2Fmt(),
                                   mCameraId,
                                   PAGE_ALIGN(input->size()));
                CheckError((buf.get() == nullptr), NO_MEMORY,
                           "@%s, No memory for rotate", __FUNCTION__);
                CheckError(buf->lock() != NO_ERROR, NO_MEMORY,
                           "@%s, Failed to lock buffer", __FUNCTION__);
                mPostProcessBufs.push_back(buf);
            }
            // Rotate to internal post-processing buffer
            status = ImageScalerCore::rotateFrame(input, mPostProcessBufs[0], angle, mRotateBuffer);
        } else {
            // Rotate to output dst buffer
            status = ImageScalerCore::rotateFrame(input, output, angle, mRotateBuffer);
        }
        CheckError((status != OK), status, "@%s, Rotate frame failed! [%d]!",
                   __FUNCTION__, status);
    } else {
        mPostProcessBufs.push_back(input);
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
                CheckError(buf->lock() != NO_ERROR, NO_MEMORY,
                           "@%s, Failed to lock buffer", __FUNCTION__);
                mPostProcessBufs.push_back(buf);
            }
            // Scale to internal post-processing buffer
            ImageScalerCore::scaleFrame(mPostProcessBufs[0], mPostProcessBufs[1]);
        } else {
            // Scale to output dst buffer
            ImageScalerCore::scaleFrame(mPostProcessBufs[0], output);
        }
        CheckError((status != OK), status, "@%s, Scale frame failed! [%d]!",
                   __FUNCTION__, status);
    }

    // Jpeg input buffer is always mPostProcessBufs.back()
    if (mProcessType & PROCESS_JPEG_ENCODING) {
        // get input frame buffer, for yuv reprocessing
        bool hasInputBuf = request->hasInputBuf();
        if (hasInputBuf) {
            const camera3_stream_buffer* inputBuf = request->getInputBuffer();
            CheckError(!inputBuf, UNKNOWN_ERROR, "@%s, getInputBuffer fails", __FUNCTION__);

            int fmt = inputBuf->stream->format;
            CheckError((fmt != HAL_PIXEL_FORMAT_YCbCr_420_888), UNKNOWN_ERROR,
            "@%s, input stream is not YCbCr_420_888, format:%x", __FUNCTION__, fmt);

            const CameraStreamNode* s = request->getInputStream();
            CheckError(!s, UNKNOWN_ERROR, "@%s, getInputStream fails", __FUNCTION__);

            std::shared_ptr<CameraBuffer> buf = request->findBuffer(s);
            CheckError((buf == nullptr), UNKNOWN_ERROR, "@%s, findBuffer fails", __FUNCTION__);

            if (!buf->isLocked()) {
                status_t ret = buf->lock();
                CheckError(ret != NO_ERROR, NO_MEMORY, "@%s, lock fails", __FUNCTION__);
            }

            mPostProcessBufs.push_back(buf);
        }

        // check if it needs to do crop
        bool needCrop = false;
        std::shared_ptr<CameraBuffer> srcBuf = mPostProcessBufs.back();
        if (srcBuf->width() * output->height() != srcBuf->height() * output->width()) {
            needCrop = true;
        }

        if (needCrop) {
            uint32_t w = 0;
            uint32_t h = 0;
            if (srcBuf->width() * output->height() < srcBuf->height() * output->width()) {
                w = srcBuf->width();
                h = srcBuf->width() * output->height() / output->width();
            } else {
                w = srcBuf->height() * output->width() / output->height();
                h = srcBuf->height();
            }
            LOG2("@%s, src w:%d, h:%d; out W:%d, h:%d; crop to w:%d, h:%d", __FUNCTION__,
            srcBuf->width(), srcBuf->height(), output->width(), output->height(), w, h);

            std::shared_ptr<CameraBuffer> dstBuf = MemoryUtils::allocateHeapBuffer(w, h, w,
                                    srcBuf->v4l2Fmt(), mCameraId, PAGE_ALIGN(w * h * 3 / 2));
            CheckError((dstBuf.get() == nullptr), NO_MEMORY, "@%s, no memory for crop", __FUNCTION__);
            status = dstBuf->lock();
            CheckError(status != NO_ERROR, status, "@%s, Failed to lock", __FUNCTION__);

            status = ImageScalerCore::cropFrame(srcBuf, dstBuf);
            CheckError(status != NO_ERROR, status, "@%s, cropFrame fails", __FUNCTION__);

            mPostProcessBufs.push_back(dstBuf);
        }

        mPostProcessBufs.back()->setRequestId(request->getId());

        mPostProcessBufs.back()->dumpImage(CAMERA_DUMP_JPEG, "before_nv12_to_jpeg.nv12");

        // update settings for jpeg
        status = mJpegTask->handleMessageSettings(*(settings.get()));
        CheckError((status != OK), status, "@%s, handleMessageSettings fails", __FUNCTION__);

        // encode jpeg
        status = convertJpeg(mPostProcessBufs.back(), output, request);
        if (status != OK) {
            LOGE("@%s, convertJpeg fails, status:%d", __FUNCTION__, status);
        }

        if (needCrop) {
            std::shared_ptr<CameraBuffer> dstBuf = mPostProcessBufs.back();
            dstBuf.reset();
            mPostProcessBufs.pop_back();
        }

        if (hasInputBuf) {
            std::shared_ptr<CameraBuffer> buf = mPostProcessBufs.back();
            buf->unlock();
            buf->getOwner()->captureDone(buf, request);
        }
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
} /* namespace camera2 */
} /* namespace android */
