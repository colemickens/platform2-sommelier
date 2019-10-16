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
#include "IPU3CameraHw.h"
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
    releaseBuffers();
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
    // TODO(wtlee): Refine the logic here since Jpeg encoder does not support downscaling currently.
    if (inputW * inputH < outStream->width * outStream->height) {
        type |= PROCESS_SCALING;
    } else if (!(type & PROCESS_JPEG_ENCODING)
         && inputW * inputH > outStream->width * outStream->height) {
        type |= PROCESS_SCALING;
    }
    if ((type & PROCESS_JPEG_ENCODING) && !mJpegTask.get()) {
        LOG2("Create JpegEncodeTask");
        mJpegTask.reset(new JpegEncodeTask(mCameraId));
    }

    LOG1("%s: postprocess type 0x%x for stream %p", __FUNCTION__,
         type, outStream);
    mProcessType = type;
    mStream = outStream;

    return OK;
}

status_t SWPostProcessor::cropFrameToSameAspectRatio(std::shared_ptr<CameraBuffer>& srcBuf,
                                                     std::shared_ptr<CameraBuffer>& refBuf,
                                                     std::shared_ptr<CameraBuffer>* dstBuf)
{
    CheckError((srcBuf == nullptr), UNKNOWN_ERROR, "@%s, srcBuf is nullptr", __FUNCTION__);
    CheckError((refBuf == nullptr), UNKNOWN_ERROR, "@%s, refBuf is nullptr", __FUNCTION__);
    CheckError(((srcBuf->format() != HAL_PIXEL_FORMAT_YCbCr_420_888) &&
               (srcBuf->format() != HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) &&
               (srcBuf->format() != HAL_PIXEL_FORMAT_NV12_LINEAR_CAMERA_INTEL)),
               UNKNOWN_ERROR,
               "@%s, invalid srcBuf format %x", __FUNCTION__, srcBuf->format());

    LOG2("@%s, src w:%d, h:%d; ref w:%d, h:%d", __FUNCTION__,
         srcBuf->width(), srcBuf->height(), refBuf->width(), refBuf->height());

    if (srcBuf->width() * refBuf->height() == srcBuf->height() * refBuf->width()) {
        return OK;
    }

    uint32_t w = 0;
    uint32_t h = 0;
    if (srcBuf->width() * refBuf->height() < srcBuf->height() * refBuf->width()) {
        w = srcBuf->width();
        h = srcBuf->width() * refBuf->height() / refBuf->width();
    } else {
        w = srcBuf->height() * refBuf->width() / refBuf->height();
        h = srcBuf->height();
    }
    LOG2("@%s, src w:%d, h:%d; dst w:%d, h:%d; crop to w:%d, h:%d", __FUNCTION__,
         srcBuf->width(), srcBuf->height(), refBuf->width(), refBuf->height(), w, h);

    std::shared_ptr<CameraBuffer> buf = requestBuffer(mCameraId, w, h);
    CheckError(buf == nullptr, UNKNOWN_ERROR, "@%s, Request buffer fails", __FUNCTION__);

    auto status = ImageScalerCore::cropFrame(srcBuf, buf);
    CheckError(status != NO_ERROR, status, "@%s, cropFrame fails", __FUNCTION__);

    *dstBuf = buf;

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

    int rotateOrientation = getRotationDegrees(mStream);
    CheckError(rotateOrientation % 90 > 0, UNKNOWN_ERROR, "@%s, unexpected rotation angle",
               __FUNCTION__);

    bool shouldRotate = rotateOrientation > 0;
    bool shouldScaleUp = mProcessType & PROCESS_SCALING;
    bool shouldEncodeToJpeg = mProcessType & PROCESS_JPEG_ENCODING;

    std::shared_ptr<CameraBuffer> srcBuf;
    if (needReprocess) {
        const camera3_stream_buffer* inputBuf = request->getInputBuffer();
        CheckError(!inputBuf, UNKNOWN_ERROR, "@%s, getInputBuffer fails", __FUNCTION__);

        int fmt = inputBuf->stream->format;
        CheckError((fmt != HAL_PIXEL_FORMAT_YCbCr_420_888), UNKNOWN_ERROR,
                   "@%s, input stream is not YCbCr_420_888, format:%x", __FUNCTION__, fmt);

        const CameraStream* s = request->getInputStream();
        CheckError(!s, UNKNOWN_ERROR, "@%s, getInputStream fails", __FUNCTION__);

        srcBuf = request->findBuffer(s);
        CheckError((srcBuf == nullptr), UNKNOWN_ERROR, "@%s, findBuffer fails", __FUNCTION__);
    } else {
        srcBuf = input;
    }

    // Steps:
    //     [Rotate -> ScaleUp] (Optional) -> Crop (Optional) -> JpegEncode /
    //     Scale
    //
    // For non-reprocess request, do rotate / scaled.

    // Rotate
    std::shared_ptr<CameraBuffer> rotatedBuf;
    if (shouldRotate && !needReprocess) {
        lockBuffer(srcBuf);
        if (!shouldEncodeToJpeg && !shouldScaleUp) {
            lockBuffer(output);
            status = ImageScalerCore::rotateFrame(srcBuf, output, rotateOrientation, mRotateBuffer);
        } else {
            int rotatedWidth = srcBuf->width();
            int rotatedHeight = srcBuf->height();
            if (rotateOrientation == 90 || rotateOrientation == 270) {
                std::swap(rotatedWidth, rotatedHeight);
            }
            rotatedBuf = requestBuffer(mCameraId, rotatedWidth, rotatedHeight);
            CheckError(rotatedBuf == nullptr, UNKNOWN_ERROR, "@%s, Request buffer fails",
                       __FUNCTION__);
            status = ImageScalerCore::rotateFrame(srcBuf, rotatedBuf, rotateOrientation,
                                                  mRotateBuffer);
        }
    } else {
        rotatedBuf = srcBuf;
    }

    // Processing: Scale Up
    std::shared_ptr<CameraBuffer> scaledBuf;
    if (shouldScaleUp && !needReprocess) {
        lockBuffer(rotatedBuf);
        if (!shouldEncodeToJpeg) {
            lockBuffer(output);
            ImageScalerCore::scaleFrame(rotatedBuf, output);
        } else {
            scaledBuf = requestBuffer(mCameraId, mStream->width, mStream->height);
            CheckError(scaledBuf == nullptr, UNKNOWN_ERROR, "@%s, Request buffer fails",
                       __FUNCTION__);
            ImageScalerCore::scaleFrame(rotatedBuf, scaledBuf);
        }
    } else {
        scaledBuf = rotatedBuf;
    }

    if (shouldEncodeToJpeg || needReprocess) {
        // Crop
        std::shared_ptr<CameraBuffer> croppedBuf;
        if (scaledBuf->width() * output->height() != scaledBuf->height() * output->width()) {
            lockBuffer(scaledBuf);
            lockBuffer(output);
            status_t ret = cropFrameToSameAspectRatio(scaledBuf, output, &croppedBuf);
            CheckError((ret != OK), UNKNOWN_ERROR, "@%s, cropFrame fails", __FUNCTION__);
        } else {
            croppedBuf = scaledBuf;
        }

        // Scale to output size.
        std::shared_ptr<CameraBuffer> finalScaledBuf;
        if (croppedBuf->width() != output->width() || croppedBuf->height() != output->height()) {
            lockBuffer(croppedBuf);
            finalScaledBuf = requestBuffer(mCameraId, output->width(), output->height());
            CheckError(finalScaledBuf == nullptr, UNKNOWN_ERROR, "@%s, Request buffer fails",
                       __FUNCTION__);
            ImageScalerCore::scaleFrame(croppedBuf, finalScaledBuf);
        } else {
            finalScaledBuf = croppedBuf;
        }

        if (shouldEncodeToJpeg) {
            // Correct orientation from metadata.
            int metadataOrientation = [&] {
                auto* camera3Request = settings.get()->request;
                auto* partRes = camera3Request->getPartialResultBuffer(CONTROL_UNIT_PARTIAL_RESULT);
                CheckError(partRes == nullptr, -1, "@%s, partial result is null", __FUNCTION__);
                auto entry = (*partRes).find(ANDROID_JPEG_ORIENTATION);
                if (entry.count == 1) {
                    return *entry.data.i32;
                }
                return 0;
            } ();

            std::shared_ptr<CameraBuffer> orientationCorrectedBuf;
            if (metadataOrientation > 0) {
                lockBuffer(finalScaledBuf);
                int rotatedWidth = finalScaledBuf->width();
                int rotatedHeight = finalScaledBuf->height();
                if (metadataOrientation == 90 || metadataOrientation == 270) {
                    std::swap(rotatedWidth, rotatedHeight);
                }
                orientationCorrectedBuf = requestBuffer(mCameraId, rotatedWidth, rotatedHeight);
                CheckError(orientationCorrectedBuf == nullptr, UNKNOWN_ERROR,
                    "@%s, Request buffer fails", __FUNCTION__);
                status = ImageScalerCore::rotateFrame(finalScaledBuf, orientationCorrectedBuf,
                                                      metadataOrientation, mRotateBuffer);
            } else {
                orientationCorrectedBuf = finalScaledBuf;
            }

            lockBuffer(orientationCorrectedBuf);
            lockBuffer(output);

            orientationCorrectedBuf->setRequestId(request->getId());
            orientationCorrectedBuf->dumpImage(CAMERA_DUMP_JPEG, "before_nv12_to_jpeg.nv12");

            // update settings for jpeg
            status = mJpegTask->handleMessageSettings(*(settings.get()));
            CheckError((status != OK), status, "@%s, handleMessageSettings fails", __FUNCTION__);

            // encode jpeg
            status = convertJpeg(orientationCorrectedBuf, output, request);
            if (status != OK) {
                LOGE("@%s, convertJpeg fails, status:%d", __FUNCTION__, status);
            }
        } else {
            // We should still put the result into |output| even if it is not encoded to Jpeg.
            lockBuffer(finalScaledBuf);
            lockBuffer(output);
            ImageScalerCore::scaleFrame(finalScaledBuf, output);
        }
    }

    if (srcBuf->isLocked()) {
        srcBuf->unlock();
    }
    if (output->isLocked()) {
        output->unlock();
    }
    releaseBuffers();
    if (needReprocess) {
        srcBuf->getOwner()->captureDone(srcBuf, request);
    }
    return status;
}

status_t SWPostProcessor::lockBuffer(const std::shared_ptr<CameraBuffer>& buffer) const
{
    if (!buffer->isLocked()) {
        CheckError(buffer->lock() != NO_ERROR, NO_MEMORY, "@%s, Failed to lock buffer",
                   __FUNCTION__);
    }
    return OK;
}

std::shared_ptr<CameraBuffer> SWPostProcessor::requestBuffer(int cameraId, int width, int height)
{
    auto buf = MemoryUtils::allocateHandleBuffer(width, height, HAL_PIXEL_FORMAT_YCbCr_420_888,
        GRALLOC_USAGE_HW_CAMERA_READ | GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN,
        cameraId);
    CheckError((buf.get() == nullptr), nullptr, "@%s, No memory for scale", __FUNCTION__);
    lockBuffer(buf);
    mPostProcessBufs.push_back(buf);

    return buf;
}

void SWPostProcessor::releaseBuffers()
{
    for (auto buf : mPostProcessBufs) {
        if (buf->isLocked()) {
            buf->unlock();
        }
        MemoryUtils::freeHandleBuffer(buf);
        buf.reset();
    }
    mPostProcessBufs.clear();
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
