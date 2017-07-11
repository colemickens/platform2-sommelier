/*
 * Copyright (C) 2014-2017 Intel Corporation
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

#define LOG_TAG "JpegMaker"

#include "LogHelper.h"
#include "JpegMaker.h"

NAMESPACE_DECLARATION {

JpegMaker::JpegMaker(int cameraid) :
    JpegMakerCore(cameraid)
{
    LOG1("@%s", __FUNCTION__);
}

JpegMaker::~JpegMaker()
{
    LOG1("@%s", __FUNCTION__);
}

status_t JpegMaker::setupExifWithMetaData(ImgEncoder::EncodePackage & package,
                                          ExifMetaData& metaData)
{
    LOG2("@%s", __FUNCTION__);
    ImgEncoderCore::EncodePackage corePackage;

    ImgEncoder::convertEncodePackage(package, corePackage);
    return JpegMakerCore::setupExifWithMetaData(corePackage, metaData);
}

/**
 * makeJpeg
 *
 * Create and prefix the exif header to the encoded jpeg data.
 * Skip the SOI marker got from the JPEG encoding. Append the camera3_jpeg_blob
 * at the end of the buffer.
 *
 * \param package [IN] EncodePackage from the caller with encoded main and thumb
 *  buffers , Jpeg settings, and encoded sizes
 * \param dest [IN] The final output buffer to client
 *
 */
status_t JpegMaker::makeJpeg(ImgEncoder::EncodePackage & package, std::shared_ptr<CameraBuffer> dest)
{
    LOG2("@%s", __FUNCTION__);
    UNUSED(dest);
    ImgEncoderCore::EncodePackage corePackage;

    ImgEncoder::convertEncodePackage(package, corePackage);
    return JpegMakerCore::makeJpeg(corePackage);
}

} NAMESPACE_DECLARATION_END

