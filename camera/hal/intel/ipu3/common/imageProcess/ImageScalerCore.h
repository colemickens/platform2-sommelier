/*
 * Copyright (C) 2012-2019 Intel Corporation
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
#include "CommonUtilMacros.h"
#include <libyuv.h>
#include <memory>
#include <unordered_map>
#include <vector>

namespace cros {
namespace intel {
class ImageScalerCore {
public:
    template<typename T>
    static status_t cropFrame(std::shared_ptr<T> srcBuf, std::shared_ptr<T> dstBuf) {
        int srcW = srcBuf->stride();
        int srcH = srcBuf->height();
        int dstW = dstBuf->stride();
        int dstH = dstBuf->height();

        std::unique_ptr<uint8_t[]> srcI420Buf;
        unsigned int srcI420BufSize = srcW * srcH * 3 / 2;
        srcI420Buf.reset(new uint8_t[srcI420BufSize]);

        const uint8_t* srcBufY = static_cast<uint8_t*>(srcBuf->data());
        const uint8_t* srcBufUV = srcBufY + srcW * srcH;
        uint8_t* srcI420BufY = static_cast<uint8_t*>(srcI420Buf.get());
        uint8_t* srcI420BufU = srcI420BufY + srcW * srcH;
        uint8_t* srcI420BufV = srcI420BufU + srcW * srcH / 4;
        int ret = libyuv::NV12ToI420(srcBufY, srcW,
                                     srcBufUV, srcW,
                                     srcI420BufY, srcW,
                                     srcI420BufU, srcW / 2,
                                     srcI420BufV, srcW / 2,
                                     srcW, srcH);
        CheckError((ret != 0), UNKNOWN_ERROR, "@%s, NV12ToI420 fails", __FUNCTION__);

        std::unique_ptr<uint8_t[]> dstI420BufUV;
        unsigned int dstI420BufUVSize = dstW * dstH / 2;
        dstI420BufUV.reset(new uint8_t[dstI420BufUVSize]);

        uint8_t* dstI420BufU = static_cast<uint8_t*>(dstI420BufUV.get());
        uint8_t* dstI420BufV = dstI420BufU + dstW * dstH / 4;
        ret = libyuv::ConvertToI420(static_cast<uint8_t*>(srcI420Buf.get()), srcI420BufSize,
                                    static_cast<uint8_t*>(dstBuf->data()), dstW,
                                    dstI420BufU, (dstW + 1) / 2,
                                    dstI420BufV, (dstW + 1) / 2,
                                    (srcW - dstW) / 2, (srcH - dstH) / 2,
                                    srcW, srcH, dstW, dstH,
                                    libyuv::RotationMode::kRotate0, libyuv::FourCC::FOURCC_I420);
        CheckError(ret != 0, UNKNOWN_ERROR, "@%s, ConvertToI420 fails", __FUNCTION__);

        uint8_t* dstBufUV = static_cast<uint8_t*>(dstBuf->data()) + dstW * dstH;
        libyuv::MergeUVPlane(dstI420BufU, (dstW + 1) / 2,
                             dstI420BufV, (dstW + 1) / 2,
                             dstBufUV, dstW,
                             (dstW + 1) / 2, (dstH + 1) / 2);

        return NO_ERROR;
    }

    template<typename T>
    static void scaleFrame(std::shared_ptr<T> input, std::shared_ptr<T> output) {
        // Y plane
        libyuv::ScalePlane(static_cast<uint8_t*>(input->data()),
                            input->stride(),
                            input->width(),
                            input->height(),
                            static_cast<uint8_t*>(output->data()),
                            output->stride(),
                            output->width(),
                            output->height(),
                            libyuv::kFilterNone);

        // UV plane
        int inUVOffsetByte = input->stride() * input->height();
        int outUVOffsetByte = output->stride() * output->height();
        libyuv::ScalePlane_16(static_cast<uint16_t*>(input->data()) + inUVOffsetByte / sizeof(uint16_t),
                                input->stride() / 2,
                                input->width() / 2,
                                input->height() / 2,
                                static_cast<uint16_t*>(output->data()) + outUVOffsetByte / sizeof(uint16_t),
                                output->stride() / 2,
                                output->width() / 2,
                                output->height() / 2,
                                libyuv::kFilterNone);
    }

    template<typename T>
    static status_t rotateFrame(std::shared_ptr<T> input, std::shared_ptr<T> output,
                                int angle, std::vector<uint8_t>& rotateBuf) {
        CheckError((output->width() != input->width() || output->height() != input->height()) &&
                   (output->width() != input->height() || output->height() != input->width()),
                   BAD_VALUE, "output resolution mis-match [%d x %d] -> [%d x %d]",
                   input->width(), input->height(), output->width(), output->height());
        CheckError((angle < 0 || angle >= 360 || angle % 90 != 0), BAD_VALUE,
                   "angle value:%d is wrong", angle);

        const uint8_t* inBuffer = static_cast<uint8_t*>(input->data());
        uint8_t* outBuffer = static_cast<uint8_t*>(output->data());
        int outW = output->width();
        int outH = output->height();
        int outStride = output->stride();
        int inW = input->width();
        int inH = input->height();
        int inStride = input->stride();
        int tmpBufferSize = std::max(inStride * inH, outStride * outH) * 3 / 2;
        if (rotateBuf.size() < tmpBufferSize) {
            rotateBuf.resize(tmpBufferSize);
        }
        // TODO: find a way to rotate NV12 directly.
        uint8_t* I420Buffer = rotateBuf.data();
        std::unordered_map<int, libyuv::RotationMode> rotationModeMap = {
            {0, libyuv::RotationMode::kRotate0},
            {90, libyuv::RotationMode::kRotate90},
            {180, libyuv::RotationMode::kRotate180},
            {270, libyuv::RotationMode::kRotate270}
        };

        if (rotationModeMap[angle] == libyuv::RotationMode::kRotate0) {
            libyuv::CopyPlane(inBuffer, inStride, outBuffer, outStride, inW, inH);
            libyuv::CopyPlane(inBuffer + inH * inStride, inStride,
                              outBuffer + outH * outStride, outStride,
                              inW, inH / 2);
        } else {
            int ret = libyuv::NV12ToI420Rotate(
                inBuffer, inStride,
                inBuffer + inH * inStride, inStride,
                I420Buffer, outStride,
                I420Buffer + outStride * outH, outStride / 2,
                I420Buffer + outStride * outH * 5 / 4, outStride / 2,
                inW, inH, rotationModeMap[angle]);
            CheckError((ret < 0), UNKNOWN_ERROR, "@%s, rotate fail [%d]!", __FUNCTION__, ret);

            ret = libyuv::I420ToNV12(I420Buffer, outStride,
                                    I420Buffer + outStride * outH, outStride / 2,
                                    I420Buffer + outStride * outH * 5 / 4, outStride / 2,
                                    outBuffer, outStride,
                                    outBuffer +  outStride * outH, outStride,
                                    outW, outH);
            CheckError((ret < 0), UNKNOWN_ERROR, "@%s, convert fail [%d]!", __FUNCTION__, ret);
        }
        return OK;
    }
};

} /* namespace intel */
} /* namespace cros */
#endif // _IMAGESCALER_CORE_H_
