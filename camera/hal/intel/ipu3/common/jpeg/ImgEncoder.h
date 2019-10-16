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

#ifndef _CAMERA3_HAL_IMG_ENCODER_H_
#define _CAMERA3_HAL_IMG_ENCODER_H_

#include <list>

#include "CameraBuffer.h"
#include "cros-camera/jpeg_compressor.h"
#include "ImgEncoder.h"


namespace cros {
namespace intel {

/**
 * \class ImgEncoder
 * This class does JPEG encoding for input image to output image defined in the EncodePackage. This
 * class selects between hardware and software encoding and provides the output in Jpeg format in
 * the EncodePackage.
 */
class ImgEncoder {
public:  /* types */
    struct EncodePackage {
        EncodePackage():
            quality(0),
            encodedDataSize(0),
            exifData(nullptr),
            exifDataSize(0) {}

        std::shared_ptr<CameraBuffer> input;
        std::shared_ptr<CameraBuffer> output;
        int quality;
        uint32_t encodedDataSize;
        uint8_t* exifData;
        uint32_t exifDataSize;
    };

public: /* Methods */
    explicit ImgEncoder(int cameraid);
    virtual ~ImgEncoder();

    bool encodeSync(EncodePackage& package);

private:  /* Members */
    std::mutex mEncodeLock; /* protect JPEG encoding progress */
    std::unique_ptr<cros::JpegCompressor> mJpegCompressor;
};

} /* namespace intel */
} /* namespace cros */
#endif  // _CAMERA3_HAL_IMG_ENCODER_H_
