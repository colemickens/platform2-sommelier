/*
 * Copyright (C) 2014-2018 Intel Corporation
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

#define LOG_TAG "ImgEncoderCore"

#include "PlatformData.h"
#include "ImgEncoderCore.h"
#include "LogHelper.h"
#include <Utils.h>
#include "Camera3V4l2Format.h"
#include "ImageScalerCore.h"
#include "Exif.h"

#include "ColorConverter.h"
#include <base/memory/ptr_util.h>
#include <base/memory/shared_memory.h>

NAMESPACE_DECLARATION {
ImgEncoderCore::ImgEncoderCore() :
    mThumbOutBuf(nullptr),
    mJpegDataBuf(nullptr),
    mMainScaled(nullptr),
    mThumbScaled(nullptr),
    mJpegSetting(nullptr),
    mInternalYU12(new YU12Buffer(RESOLUTION_14MP_WIDTH, RESOLUTION_14MP_HEIGHT)),
    mTmpBuffer(new YU12Buffer(0, 0)),
    mJpegCompressor(cros::JpegCompressor::GetInstance())
{
    LOG1("@%s", __FUNCTION__);
}

ImgEncoderCore::~ImgEncoderCore()
{
    LOG1("@%s", __FUNCTION__);
    deInit();
}

status_t ImgEncoderCore::init(void)
{
    LOG1("@%s", __FUNCTION__);

    mJpegSetting = new ExifMetaData::JpegSetting;

    return NO_ERROR;
}

void ImgEncoderCore::deInit(void)
{
    LOG2("@%s", __FUNCTION__);

    if (mJpegSetting) {
        delete mJpegSetting;
        mJpegSetting = nullptr;
    }

    mThumbOutBuf.reset();

    mJpegDataBuf.reset();
}

/**
 * thumbBufferDownScale
 * Downscaling the thumb buffer and allocate the scaled thumb input intermediate
 * buffer if scaling is needed.
 *
 * \param pkg [IN] EncodePackage from the caller
 */
void ImgEncoderCore::thumbBufferDownScale(EncodePackage & pkg)
{
    LOG2("%s", __FUNCTION__);
    int thumbwidth = mJpegSetting->thumbWidth;
    int thumbheight = mJpegSetting->thumbHeight;

    // Allocate thumbnail input buffer if downscaling is needed.
    if (thumbwidth != 0) {
        if (COMPARE_RESOLUTION(pkg.thumb, mThumbOutBuf) != 0) {
            LOG2("%s: Downscaling for thumbnail: %dx%d -> %dx%d", __FUNCTION__,
                pkg.thumb->width(), pkg.thumb->height(),
                mThumbOutBuf->width(), mThumbOutBuf->height());
            if (mThumbScaled
                && (COMPARE_RESOLUTION(mThumbScaled, mThumbOutBuf) != 0
                    || pkg.thumb->v4l2Fmt() != mThumbScaled->v4l2Fmt())) {
                mThumbScaled.reset();
            }
            if (!mThumbScaled) {
                BufferProps props;
                props.width  = thumbwidth;
                props.height = thumbheight;
                props.stride = thumbwidth;
                props.format = pkg.thumb->v4l2Fmt();
                props.type   = BMT_HEAP;
                // Using thumbwidth as stride for heap buffer
                mThumbScaled = std::make_shared<CommonBuffer>(props);

                if (mThumbScaled->allocMemory()) {
                    LOGE("Error in allocating buffer with size:%d", mThumbScaled->size());
                    return;
                }
            }
            ImageScalerCore::scaleFrame<CommonBuffer>(pkg.thumb, mThumbScaled);
            pkg.thumb = mThumbScaled;
        }
    }
}

/**
 * mainBufferDownScale
 * Downscaling the main buffer and allocate the scaled main intermediate
 * buffer if scaling is needed.
 *
 * \param pkg [IN] EncodePackage from the caller
 */
void ImgEncoderCore::mainBufferDownScale(EncodePackage & pkg)
{
    LOG2("%s", __FUNCTION__);

    // Allocate buffer for main picture downscale
    // Compare the resolution, only do downscaling.
    if (COMPARE_RESOLUTION(pkg.main, pkg.jpegOut) == 1) {
        LOG2("%s: Downscaling for main picture: %dx%d -> %dx%d", __FUNCTION__,
            pkg.main->width(), pkg.main->height(),
            pkg.jpegOut->width(), pkg.jpegOut->height());
        if (mMainScaled
            && (COMPARE_RESOLUTION(mMainScaled, pkg.jpegOut) != 0
                || pkg.main->v4l2Fmt() != mMainScaled->v4l2Fmt())) {
            mMainScaled.reset();
        }
        if (!mMainScaled) {
            BufferProps props;
            props.width  = pkg.jpegOut->width();
            props.height = pkg.jpegOut->height();
            props.stride = pkg.jpegOut->width();
            props.format = pkg.main->v4l2Fmt();
            props.type   = BMT_HEAP;
            // Use pkg.jpegOut->width() as stride for the heap buffer
            mMainScaled = std::make_shared<CommonBuffer>(props);

            if (mMainScaled->allocMemory()) {
                LOGE("Error in allocating buffer with size:%d", mMainScaled->size());
                return;
            }
        }
        ImageScalerCore::scaleFrame<CommonBuffer>(pkg.main, mMainScaled);
        pkg.main = mMainScaled;
    }
}

/**
 * allocateBufferAndDownScale
 * This method downscales the main image and thumbnail if necesary. In case it
 * needs to scale, it allocates the intermediate buffers where the scaled version
 * is stored before it is given to the encoders. jpeg.thumbnailSize (0,0) means
 * JPEG EXIF will not contain a thumbnail. We use thumbwidth to determine if
 * Thumbnail size is > 0. In case thumbsize is not 0 we create the thumb output
 * buffer with size provided in the settings. If no thumb input buffer is
 * provided with the package the main buffer is assinged to the thumb input.
 * If thumb input buffer is provided in the package then only down scaling is needed
 *
 * \param pkg [IN] EncodePackage from the caller
 */
status_t ImgEncoderCore::allocateBufferAndDownScale(EncodePackage & pkg)
{
    LOG2("%s", __FUNCTION__);

    int thumbwidth = mJpegSetting->thumbWidth;
    int thumbheight = mJpegSetting->thumbHeight;

    // Check if client provided the encoded data buffer
    if (pkg.encodedData)
        mJpegDataBuf = pkg.encodedData;

    // Allocate buffer for main image jpeg output if in first time or resolution changed
    if (pkg.encodeAll && (!mJpegDataBuf ||
                COMPARE_RESOLUTION(mJpegDataBuf, pkg.jpegOut))) {
        if (mJpegDataBuf)
            mJpegDataBuf.reset();

        if (!mJpegDataBuf) {
            LOG1("Allocating jpeg data buffer with %dx%d, stride:%d", pkg.jpegOut->width(),
                    pkg.jpegOut->height(), pkg.jpegOut->stride());

            BufferProps props;
            props.width  = pkg.jpegOut->width();
            props.height = pkg.jpegOut->height();
            props.stride = pkg.jpegOut->stride();
            props.format = pkg.jpegOut->v4l2Fmt();
            props.type   = BMT_HEAP;
            mJpegDataBuf = std::make_shared<CommonBuffer>(props);

            if (mJpegDataBuf->allocMemory()) {
                LOGE("Error in allocating buffer with size:%d", mJpegDataBuf->size());
                return NO_MEMORY;
            }
        }
    }

    // Check if client provided the thumb out data buffer
    if (pkg.thumbOut)
        mThumbOutBuf = pkg.thumbOut;

    // Allocate buffer for thumbnail output
    if (thumbwidth != 0) {
        if (!pkg.thumb)
            pkg.thumb = pkg.main;

        if (mThumbOutBuf && (mThumbOutBuf->width() != thumbwidth ||
                    mThumbOutBuf->height() != thumbheight))
            mThumbOutBuf.reset();

        if (!mThumbOutBuf) {
            LOG1("Allocating thumb data buffer with %dx%d", thumbwidth, thumbheight);

            if (!pkg.thumb) {
                LOGE("No source for thumb");
                return UNKNOWN_ERROR;
            }

            BufferProps props;
            props.width  = thumbwidth;
            props.height = thumbheight;
            props.stride = thumbwidth;
            props.format = pkg.thumb->v4l2Fmt();
            props.type   = BMT_HEAP;
            // Use thumbwidth as stride for the heap buffer
            mThumbOutBuf = std::make_shared<CommonBuffer>(props);

            if (mThumbOutBuf->allocMemory()) {
                LOGE("Error in allocating buffer with size:%d", mThumbOutBuf->size());
                return NO_MEMORY;
            }
        }
    }

    thumbBufferDownScale(pkg);
    if (pkg.encodeAll)
        mainBufferDownScale(pkg);

    return NO_ERROR;
}


/**
 * getJpegSettings
 *
 * Get the JPEG settings needed for image encoding from the Exif
 * metadata and store to internal struct
 * \param settings [IN] EncodePackage from the caller
 * \param metaData [IN] exif metadata
 */
status_t ImgEncoderCore::getJpegSettings(EncodePackage & pkg, ExifMetaData& metaData)
{
    LOG2("@%s:", __FUNCTION__);
    UNUSED(pkg);
    status_t status = NO_ERROR;

    *mJpegSetting = metaData.mJpegSetting;
    LOG1("jpegQuality=%d,thumbQuality=%d,thumbW=%d,thumbH=%d,orientation=%d",
              mJpegSetting->jpegQuality,
              mJpegSetting->jpegThumbnailQuality,
              mJpegSetting->thumbWidth,
              mJpegSetting->thumbHeight,
              mJpegSetting->orientation);

    return status;
}

bool ImgEncoderCore::convertToP411WithCorrectOrientation(
    std::shared_ptr<CommonBuffer> srcBuf)
{
    int width = srcBuf->width();
    int height = srcBuf->height();

    libyuv::RotationModeEnum rotationMode = getRotationInfo();
    if (rotationMode == libyuv::kRotate90 || rotationMode == libyuv::kRotate270) {
        mInternalYU12->reset(height, width);
    } else {
        mInternalYU12->reset(width, height);
    }
    mTmpBuffer->reset(width, height);

    bool isConverted = convertToP411UsingLibYUV(srcBuf,
        mTmpBuffer, srcBuf->v4l2Fmt());
    CheckError(isConverted == false,
        false, "@%s, Error when convert image format", __FUNCTION__);

    int status = libyuv::I420Rotate(
        mTmpBuffer->y(), mTmpBuffer->ystride(),
        mTmpBuffer->cb(), mTmpBuffer->cstride(),
        mTmpBuffer->cr(), mTmpBuffer->cstride(),
        mInternalYU12->y(), mInternalYU12->ystride(),
        mInternalYU12->cb(), mInternalYU12->cstride(),
        mInternalYU12->cr(), mInternalYU12->cstride(),
        mTmpBuffer->width(), mTmpBuffer->height(),
        rotationMode
    );
    CheckError(status != 0,
        false , "@%s, Failed to rotate I420 image", __FUNCTION__);

    LOG1("%s Successfully correct the orientation", __FUNCTION__);
    return true;
}

libyuv::RotationModeEnum ImgEncoderCore::getRotationInfo()
{
    switch (mJpegSetting->orientation) {
        case 0:
            LOG1("%s No need to correct orientation", __FUNCTION__);
            return libyuv::kRotate0;
        case 90:
            return libyuv::kRotate90;
        case 180:
            return libyuv::kRotate180;
        case 270:
            return libyuv::kRotate270;
        default:
            LOGE("%s Unsupported orientation to correct: %d", __FUNCTION__,
                mJpegSetting->orientation);
            return libyuv::kRotate0;
    }
}

bool ImgEncoderCore::convertToP411UsingLibYUV(std::shared_ptr<CommonBuffer> src,
    std::shared_ptr<YU12Buffer> dst, int format)
{
    CheckError(src->width() != dst->width() || src->height() != dst->height(),
        false, "@%s, Image size not matched: %d:%d / %lu:%lu", __FUNCTION__,
        src->width(), src->height(), dst->width(), dst->height());

    uint8_t* srcY = static_cast<uint8_t*>(src->data());
    uint8_t* srcUV = srcY + src->stride() * src->height();
    int src_stride = src->stride();
    int ystride = dst->ystride();
    int cstride = dst->cstride();

    int status;
    switch (format) {
        case V4L2_PIX_FMT_YUYV:
            status = libyuv::YUY2ToI420(
                srcY, src_stride,
                dst->y(), ystride,
                dst->cb(), cstride,
                dst->cr(), cstride,
                src->width(), src->height());
            break;
        case V4L2_PIX_FMT_NV12:
            status = libyuv::NV12ToI420(
                srcY, src_stride,
                srcUV, src_stride,
                dst->y(), ystride,
                dst->cb(), cstride,
                dst->cr(), cstride,
                src->width(), src->height());
            break;
        case V4L2_PIX_FMT_NV21:
            status = libyuv::NV21ToI420(
                srcY, src_stride,
                srcUV, src_stride,
                dst->y(), ystride,
                dst->cb(), cstride,
                dst->cr(), cstride,
                src->width(), src->height());
            break;
        default:
            LOGE("%s Unsupported format %d", __FUNCTION__, format);
            return false;
    }
    CheckError(status != 0,
        false, "@%s, Failed to convert to YUV420", __FUNCTION__);

    return true;
}

int ImgEncoderCore::doEncode(std::shared_ptr<CommonBuffer> srcBuf,
                             int quality,
                             std::shared_ptr<CommonBuffer> destBuf,
                             unsigned int destOffset)
{
    LOG2("@%s", __FUNCTION__);

    if (!convertToP411WithCorrectOrientation(srcBuf)) {
        return 0;
    }
    void* tempBuf = mInternalYU12->data();
    int width = mInternalYU12->width();
    int height = mInternalYU12->height();

    uint32_t outSize = 0;
    nsecs_t startTime = systemTime();
    void* pDst = static_cast<unsigned char*>(destBuf->data()) + destOffset;
    bool ret = mJpegCompressor->CompressImage(tempBuf,
                                              width, height, quality,
                                              nullptr, 0,
                                              destBuf->size(), pDst,
                                              &outSize);
    LOG1("%s: encoding ret:%d, %dx%d need %" PRId64 "ms, jpeg size %u, quality %d)",
         __FUNCTION__, ret, destBuf->width(), destBuf->height(),
         (systemTime() - startTime) / 1000000, outSize, quality);
    CheckError(ret == false, 0, "@%s, mJpegCompressor->CompressImage() fails",
               __FUNCTION__);

    return outSize;
}

/**
 * encodeSync
 *
 * Do HW or SW encoding of the main buffer of the package
 * Also do SW encoding of the thumb buffer
 *
 * \param srcBuf [IN] The input buffer to encode
 * \param metaData [IN] exif metadata
 *
 */
status_t ImgEncoderCore::encodeSync(EncodePackage & package, ExifMetaData& metaData)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1, LOG_TAG);
    status_t status = NO_ERROR;
    int mainSize = 0;
    uint32_t thumbSize = 0;

    std::lock_guard<std::mutex> l(mEncodeLock);

    if (package.encodeAll) {
        if (!package.main) {
            LOGE("Main buffer for JPEG encoding is nullptr");
            return UNKNOWN_ERROR;
        }

        if (!package.jpegOut) {
            LOGE("JPEG output buffer is nullptr");
            return UNKNOWN_ERROR;
        }
    }

    getJpegSettings(package, metaData);
    // Allocate buffers for thumbnail if not present and also
    // downscale the buffer for main if scaling is needed
    allocateBufferAndDownScale(package);

    if (package.thumb && mThumbOutBuf) {
        if (!convertToP411WithCorrectOrientation(package.thumb)) {
            thumbSize = 0;
        } else {
            do {
                LOG2("Encoding thumbnail with quality %d",
                    mJpegSetting->jpegThumbnailQuality);
                mJpegCompressor->GenerateThumbnail(
                    mInternalYU12->data(),
                    mInternalYU12->width(),
                    mInternalYU12->height(),
                    mInternalYU12->width(),
                    mInternalYU12->height(),
                    mJpegSetting->jpegThumbnailQuality,
                    mThumbOutBuf->size(),
                    mThumbOutBuf->data(), &thumbSize);
                mJpegSetting->jpegThumbnailQuality -= 5;
            } while (thumbSize > 0 && mJpegSetting->jpegThumbnailQuality > 0 &&
                     thumbSize > THUMBNAIL_SIZE_LIMITATION);
        }

        if (thumbSize > 0) {
            package.thumbOut = mThumbOutBuf;
            package.thumbSize = thumbSize;
        } else {
            // This is not critical, we can continue with main picture image
            LOGW("Could not encode thumbnail stream!");
        }
    } else {
        // No thumb is not critical, we can continue with main picture image
        LOG1("Exif created without thumbnail stream!");
    }

    if (package.encodeAll) {
        status = NO_ERROR;
        mainSize = doEncode(package.main, mJpegSetting->jpegQuality, mJpegDataBuf);
        if (mainSize <= 0) {
           LOGE("Error while encoding JPEG");
           status = INVALID_OPERATION;
        }

        if (mainSize > 0) {
            package.encodedData = mJpegDataBuf;
            package.encodedDataSize = mainSize;
        }
    }

    return status;
}

} NAMESPACE_DECLARATION_END
