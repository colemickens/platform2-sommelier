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
#define LOG_TAG "IPU3Common"

#include "IPU3Common.h"
#include "Camera3GFXFormat.h"
#include "LogHelper.h"
#include "IPU3Types.h"
namespace android {
namespace camera2 {

// consts from IPU3Types.h
const int JPEG_QUALITY_DEFAULT = 95;
const int THUMBNAIL_QUALITY_DEFAULT = 90;

int isysRawBpp(int fourcc)
{
    LOG1("%s: ISYS RAW format %s", __FUNCTION__, v4l2Fmt2Str(fourcc));
    if (!isBayerFormat(fourcc)) {
        LOGE("%s: CaptureUnit RAW format %s is not bayer", __FUNCTION__, v4l2Fmt2Str(fourcc));
        return INVALID_BPP;
    }

    const CameraFormatBridge* afb = getCameraFormatBridge(fourcc);
    return afb->depth;
}

bool isWideAspectRatio(int width, int height)
{
    double value = (double)width / height;
    if (value > 1.7)
        return true; // 16:9
    else
        return false; // 4:3
}

}  // namespace camera2
}  // namespace android
