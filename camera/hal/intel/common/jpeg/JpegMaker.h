/*
 * Copyright (C) 2014-2019 Intel Corporation
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

#ifndef _CAMERA3_HAL_JPEG_MAKER_H_
#define _CAMERA3_HAL_JPEG_MAKER_H_

#include "Camera3Request.h"
#include "EXIFMaker.h"
#include "EXIFMetaData.h"
#include "ImgEncoder.h"

namespace cros {
namespace intel {
/**
 * \class JpegMaker
 * Does the EXIF header creation and appending to the provided jpeg buffer
 *
 */
class JpegMaker {
public: /* Methods */
    explicit JpegMaker(int cameraid);
    virtual ~JpegMaker();
    status_t setupExifWithMetaData(int output_width, int output_height, ExifMetaData* metaData,
                                   const Camera3Request& request);
    status_t getExif(ImgEncoder::EncodePackage& thumbnailPackage, uint8_t* exifPtr,
                     uint32_t* exifSize);

private: /* Methods */
    // prevent copy constructor and assignment operator
    JpegMaker(const JpegMaker& other);
    JpegMaker& operator=(const JpegMaker& other);

    void processExifSettings(const android::CameraMetadata* settings, ExifMetaData* metaData);
    void processJpegSettings(const android::CameraMetadata* settings,
                             bool shouldSwapWidthHeight, ExifMetaData* metaData);
    void processGpsSettings(const android::CameraMetadata* settings, ExifMetaData* metaData);
    void processAwbSettings(const android::CameraMetadata* settings, ExifMetaData* metaData);
    void processScalerCropSettings(const android::CameraMetadata* settings, ExifMetaData* metaData);
    void processEvCompensationSettings(const android::CameraMetadata* settings,
                                       ExifMetaData* metaData);

private:  /* Members */
    std::unique_ptr<EXIFMaker> mExifMaker;
    int        mCameraId;
};

} /* namespace intel */
} /* namespace cros */
#endif  // _CAMERA3_HAL_JPEG_MAKER_H_
