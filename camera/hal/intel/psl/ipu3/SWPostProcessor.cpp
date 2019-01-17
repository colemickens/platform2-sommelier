/*
 * Copyright (C) 2019 Intel Corporation.
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

#define LOG_TAG "SWPostProcessor"

#include "ColorConverter.h"
#include "ImageScalerCore.h"
#include "LogHelper.h"
#include "SWPostProcessor.h"

namespace cros {
namespace intel {
SWPostProcessor::SWPostProcessor(int cameraId) :
    mCameraId(cameraId),
    mProcessType(PROCESS_NONE),
    mStream(nullptr)
{
}

SWPostProcessor::~SWPostProcessor()
{
    mPostProcessBufs.clear();
}

status_t SWPostProcessor::configure(camera3_stream_t* outStream,
                                    int inputW, int inputH, int inputFmt)
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

status_t SWPostProcessor::cropFrameToSameAspectRatio(std::shared_ptr<CameraBuffer>& srcBuf,
                                                     std::shared_ptr<CameraBuffer>& dstBuf)
{
    CheckError((srcBuf == nullptr), UNKNOWN_ERROR, "@%s, srcBuf is nullptr", __FUNCTION__);
    CheckError((dstBuf == nullptr), UNKNOWN_ERROR, "@%s, dstBuf is nullptr", __FUNCTION__);
    CheckError(((srcBuf->format() != HAL_PIXEL_FORMAT_YCbCr_420_888) &&
               (srcBuf->format() != HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) &&
               (srcBuf->format() != HAL_PIXEL_FORMAT_NV12_LINEAR_CAMERA_INTEL)),
               UNKNOWN_ERROR,
               "@%s, invalid srcBuf format %x", __FUNCTION__, srcBuf->format());

    LOG2("@%s, src w:%d, h:%d; dst w:%d, h:%d", __FUNCTION__,
          srcBuf->width(), srcBuf->height(), dstBuf->width(), dstBuf->height());

    if (srcBuf->width() * dstBuf->height() == srcBuf->height() * dstBuf->width()) {
        return OK;
    }

    uint32_t w = 0;
    uint32_t h = 0;
    if (srcBuf->width() * dstBuf->height() < srcBuf->height() * dstBuf->width()) {
        w = srcBuf->width();
        h = srcBuf->width() * dstBuf->height() / dstBuf->width();
    } else {
        w = srcBuf->height() * dstBuf->width() / dstBuf->height();
        h = srcBuf->height();
    }
    LOG2("@%s, src w:%d, h:%d; dst w:%d, h:%d; crop to w:%d, h:%d", __FUNCTION__,
          srcBuf->width(), srcBuf->height(), dstBuf->width(), dstBuf->height(), w, h);

    std::shared_ptr<CameraBuffer> buf = MemoryUtils::allocateHeapBuffer(w, h, w,
                            srcBuf->v4l2Fmt(), mCameraId, PAGE_ALIGN(w * h * 3 / 2));
    CheckError((buf.get() == nullptr), NO_MEMORY, "@%s, no memory for crop", __FUNCTION__);
    status_t status = buf->lock();
    CheckError(status != NO_ERROR, UNKNOWN_ERROR, "@%s, lock fails", __FUNCTION__);

    status = ImageScalerCore::cropFrame(srcBuf, buf);
    CheckError(status != NO_ERROR, status, "@%s, cropFrame fails", __FUNCTION__);

    mPostProcessBufs.push_back(buf);

    return OK;
}

status_t SWPostProcessor::scaleFrame(std::shared_ptr<CameraBuffer>& srcBuf,
                                     std::shared_ptr<CameraBuffer>& dstBuf)
{
    CheckError((srcBuf == nullptr), UNKNOWN_ERROR, "@%s, srcBuf is nullptr", __FUNCTION__);
    CheckError((dstBuf == nullptr), UNKNOWN_ERROR, "@%s, dstBuf is nullptr", __FUNCTION__);
    CheckError(((srcBuf->format() != HAL_PIXEL_FORMAT_YCbCr_420_888) &&
               (srcBuf->format() != HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) &&
               (srcBuf->format() != HAL_PIXEL_FORMAT_NV12_LINEAR_CAMERA_INTEL)), UNKNOWN_ERROR,
               "@%s, invalid srcBuf format %x", __FUNCTION__, srcBuf->format());

    LOG2("@%s, src w:%d, h:%d; dst w:%d, h:%d", __FUNCTION__,
          srcBuf->width(), srcBuf->height(), dstBuf->width(), dstBuf->height());

    if (srcBuf->width() * dstBuf->height() != srcBuf->height() * dstBuf->width()) {
        LOGE("@%s, src w:%d, h:%d; dst w:%d, h:%d, not the same aspect ratio", __FUNCTION__,
              srcBuf->width(), srcBuf->height(), dstBuf->width(), dstBuf->height());
        return BAD_VALUE;
    }

    if (srcBuf->width() == dstBuf->width() && srcBuf->height() == dstBuf->height()) {
        return OK;
    }

    std::shared_ptr<CameraBuffer> buf = MemoryUtils::allocateHeapBuffer(
                                dstBuf->width(), dstBuf->height(),
                                dstBuf->width(),
                                srcBuf->v4l2Fmt(), mCameraId,
                                PAGE_ALIGN(dstBuf->width() * dstBuf->height() * 3 / 2));
    CheckError((buf.get() == nullptr), UNKNOWN_ERROR, "@%s, no memory for crop", __FUNCTION__);
    status_t status = buf->lock();
    CheckError(status != NO_ERROR, UNKNOWN_ERROR, "@%s, lock fails", __FUNCTION__);

    ImageScalerCore::scaleFrame(srcBuf, buf);
    mPostProcessBufs.push_back(buf);

    return OK;
}

status_t SWPostProcessor::processFrame(std::shared_ptr<CameraBuffer>& input,
                                       std::shared_ptr<CameraBuffer>& output,
                                       std::shared_ptr<ProcUnitSettings>& settings,
                                       Camera3Request* request,
                                       bool needReprocess)
{
    if (mProcessType == PROCESS_NONE && needReprocess == false) {
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

    // get input frame buffer, for yuv reprocessing
    if (needReprocess) {
        const camera3_stream_buffer* inputBuf = request->getInputBuffer();
        CheckError(!inputBuf, UNKNOWN_ERROR, "@%s, getInputBuffer fails", __FUNCTION__);

        int fmt = inputBuf->stream->format;
        CheckError((fmt != HAL_PIXEL_FORMAT_YCbCr_420_888), UNKNOWN_ERROR,
                "@%s, input stream is not YCbCr_420_888, format:%x", __FUNCTION__, fmt);

        const CameraStream* s = request->getInputStream();
        CheckError(!s, UNKNOWN_ERROR, "@%s, getInputStream fails", __FUNCTION__);

        std::shared_ptr<CameraBuffer> buf = request->findBuffer(s);
        CheckError((buf == nullptr), UNKNOWN_ERROR, "@%s, findBuffer fails", __FUNCTION__);

        if (!buf->isLocked()) {
            status_t ret = buf->lock();
            CheckError(ret != NO_ERROR, NO_MEMORY, "@%s, lock fails", __FUNCTION__);
        }

        mPostProcessBufs.push_back(buf);
    }

    int processType = PROCESS_NONE;
    if ((mProcessType & PROCESS_JPEG_ENCODING) || needReprocess) {
        // cropping
        std::shared_ptr<CameraBuffer> srcBuf = mPostProcessBufs.back();
        if (srcBuf->width() * output->height() != srcBuf->height() * output->width()) {
            processType |= PROCESS_CROP;
            status_t ret = cropFrameToSameAspectRatio(srcBuf, output);
            CheckError((ret != OK), UNKNOWN_ERROR, "@%s, cropFrame fails", __FUNCTION__);
        }

        // scaling, jpeg encoder can do scaling, so it's unnecessary to do scaling for jpeg
        if (!(mProcessType & PROCESS_JPEG_ENCODING)) {
            srcBuf = mPostProcessBufs.back();
            if (srcBuf->width() != output->width() || srcBuf->height() != output->height()) {
                processType |= PROCESS_SCALING;
                status_t ret = scaleFrame(srcBuf, output);
                CheckError((ret != OK), UNKNOWN_ERROR, "@%s, scaleFrame fails", __FUNCTION__);
            }
        }
    }

    // Jpeg input buffer is always mPostProcessBufs.back()
    if (mProcessType & PROCESS_JPEG_ENCODING) {
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
    } else if (needReprocess && output->format() == HAL_PIXEL_FORMAT_YCbCr_420_888) {
        //YUV reprocess
        ImageScalerCore::scaleFrame(mPostProcessBufs.back(), output);
    }

    int relasePostProcessBufCnt = (processType & PROCESS_SCALING ? 1 : 0) +
                                  (processType & PROCESS_CROP ? 1 : 0);
    while (relasePostProcessBufCnt--) {
        std::shared_ptr<CameraBuffer> buf = std::move(mPostProcessBufs.back());
        if (buf->isLocked()) {
            buf->unlock();
        }
        buf.reset();
        mPostProcessBufs.pop_back();
    }

    if (needReprocess) {
        std::shared_ptr<CameraBuffer> inputBuf = mPostProcessBufs.back();
        inputBuf->unlock();
        inputBuf->getOwner()->captureDone(inputBuf, request);
    }

    if (!(mProcessType & PROCESS_ROTATE)) {
        // the input is in the mPostProcessBufs[0], clear it.
        mPostProcessBufs.clear();
    }

    return status;
}

int SWPostProcessor::getRotationDegrees(camera3_stream_t* stream) const
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
status_t SWPostProcessor::convertJpeg(std::shared_ptr<CameraBuffer> input,
                                      std::shared_ptr<CameraBuffer> output,
                                      Camera3Request *request)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2, LOG_TAG);
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
} /* namespace intel */
} /* namespace cros */
