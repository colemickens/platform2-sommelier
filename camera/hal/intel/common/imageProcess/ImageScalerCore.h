/*
 * Copyright (C) 2012-2018 Intel Corporation
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

#ifndef _IMAGESCALER_CORE_H_
#define _IMAGESCALER_CORE_H_
#include <memory>
#include <libyuv.h>

NAMESPACE_DECLARATION {
class ImageScalerCore {
public:
    template<typename T>
    static void scaleFrame(std::shared_ptr<T> input, std::shared_ptr<T> output) {
        // Y plane
        libyuv::ScalePlane(static_cast<uint8*>(input->data()),
                            input->stride(),
                            input->width(),
                            input->height(),
                            static_cast<uint8*>(output->data()),
                            output->stride(),
                            output->width(),
                            output->height(),
                            libyuv::kFilterNone);

        // UV plane
        int inUVOffsetByte = input->stride() * input->height();
        int outUVOffsetByte = output->stride() * output->height();
        libyuv::ScalePlane_16(static_cast<uint16*>(input->data()) + inUVOffsetByte / sizeof(uint16),
                                input->stride() / 2,
                                input->width() / 2,
                                input->height() / 2,
                                static_cast<uint16*>(output->data()) + outUVOffsetByte / sizeof(uint16),
                                output->stride() / 2,
                                output->width() / 2,
                                output->height() / 2,
                                libyuv::kFilterNone);
    }

    template<typename T>
    static status_t rotateFrame(std::shared_ptr<T> input, std::shared_ptr<T> output,
                                int angle, std::vector<uint8_t>& rotateBuf) {
        CheckError((output->width() != input->height() || output->height() != input->width()),
                    BAD_VALUE, "output resolution mis-match [%d x %d] -> [%d x %d]",
                    input->width(), input->height(), output->width(), output->height());
        CheckError((angle != 90 && angle != 270), BAD_VALUE, "angle value:%d is wrong", angle);

        const uint8* inBuffer = static_cast<uint8*>(input->data());
        uint8* outBuffer = static_cast<uint8*>(output->data());
        int outW = output->width();
        int outH = output->height();
        int outStride = output->stride();
        int inW = input->width();
        int inH = input->height();
        int inStride = input->stride();
        if (rotateBuf.size() < input->size()) {
            rotateBuf.resize(input->size());
        }

        // TODO: find a way to rotate NV12 directly.
        uint8* I420Buffer = rotateBuf.data();
        int ret = libyuv::NV12ToI420Rotate(
            inBuffer, inStride, inBuffer + inH * inStride, inStride,
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
};

} NAMESPACE_DECLARATION_END
#endif // _IMAGESCALER_CORE_H_
