/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_HEADER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_HEADER_H_

#include <mtkcam/def/BasicTypes.h>
#include <mtkcam/def/BuiltinTypes.h>
#include <mtkcam/def/Errors.h>
#include <mtkcam/def/ImageFormat.h>
#include <mtkcam/def/TypeManip.h>
#include <mtkcam/def/UITypes.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>

using NSCam::MPoint;
using NSCam::MRational;
using NSCam::MRect;
using NSCam::MSize;
using NSCam::Type2Type;

using NSCam::ALREADY_EXISTS;
using NSCam::BAD_VALUE;
using NSCam::DEAD_OBJECT;
using NSCam::FAILED_TRANSACTION;
using NSCam::INVALID_OPERATION;
using NSCam::MERROR;
using NSCam::NAME_NOT_FOUND;
using NSCam::NO_ERROR;
using NSCam::NO_INIT;
using NSCam::NO_MEMORY;
using NSCam::OK;
using NSCam::PERMISSION_DENIED;
using NSCam::status_t;
using NSCam::UNKNOWN_ERROR;

using NSCam::eBUFFER_USAGE_HW_CAMERA_READ;
using NSCam::eBUFFER_USAGE_HW_CAMERA_READWRITE;
using NSCam::eBUFFER_USAGE_HW_CAMERA_WRITE;
using NSCam::eBUFFER_USAGE_HW_MASK;
using NSCam::eBUFFER_USAGE_HW_RENDER;
using NSCam::eBUFFER_USAGE_HW_TEXTURE;
using NSCam::eBUFFER_USAGE_SW_MASK;
using NSCam::eBUFFER_USAGE_SW_READ_MASK;
using NSCam::eBUFFER_USAGE_SW_READ_OFTEN;
using NSCam::eBUFFER_USAGE_SW_READ_RARELY;
using NSCam::eBUFFER_USAGE_SW_WRITE_MASK;
using NSCam::eBUFFER_USAGE_SW_WRITE_OFTEN;
using NSCam::eBUFFER_USAGE_SW_WRITE_RARELY;

using NSCam::eCOLORPROFILE_BT2020_FULL;
using NSCam::eCOLORPROFILE_BT2020_LIMITED;
using NSCam::eCOLORPROFILE_BT601_FULL;
using NSCam::eCOLORPROFILE_BT601_LIMITED;
using NSCam::eCOLORPROFILE_BT709_FULL;
using NSCam::eCOLORPROFILE_BT709_LIMITED;
using NSCam::eCOLORPROFILE_JPEG;
using NSCam::eCOLORPROFILE_UNKNOWN;

using NSCam::eCACHECTRL_FLUSH;
using NSCam::eCACHECTRL_INVALID;

using NSCam::IImageBuffer;
using NSCam::IImageBufferHeap;

using NSCam::eTransform_FLIP_H;
using NSCam::eTransform_FLIP_V;
using NSCam::eTransform_None;
using NSCam::eTransform_ROT_180;
using NSCam::eTransform_ROT_270;
using NSCam::eTransform_ROT_90;

using NSCam::eImgFmt_ARGB888;
using NSCam::eImgFmt_ARGB8888;
using NSCam::eImgFmt_BAYER10;
using NSCam::eImgFmt_BAYER10_MIPI;
using NSCam::eImgFmt_BAYER10_UNPAK;
using NSCam::eImgFmt_BAYER12;
using NSCam::eImgFmt_BAYER12_UNPAK;
using NSCam::eImgFmt_BAYER14;
using NSCam::eImgFmt_BAYER14_UNPAK;
using NSCam::eImgFmt_BAYER8;
using NSCam::eImgFmt_BAYER8_UNPAK;
using NSCam::eImgFmt_BGRA8888;
using NSCam::eImgFmt_BLOB;
using NSCam::eImgFmt_BLOB_START;
using NSCam::eImgFmt_CAMERA_OPAQUE;
using NSCam::eImgFmt_FG_BAYER10;
using NSCam::eImgFmt_FG_BAYER12;
using NSCam::eImgFmt_FG_BAYER14;
using NSCam::eImgFmt_FG_BAYER8;
using NSCam::eImgFmt_I420;
using NSCam::eImgFmt_I422;
using NSCam::eImgFmt_IMPLEMENTATION_DEFINED;
using NSCam::eImgFmt_JPEG;
using NSCam::eImgFmt_JPG_I420;
using NSCam::eImgFmt_JPG_I422;
using NSCam::eImgFmt_NV12;
using NSCam::eImgFmt_NV12_BLK;
using NSCam::eImgFmt_NV16;
using NSCam::eImgFmt_NV21;
using NSCam::eImgFmt_NV21_BLK;
using NSCam::eImgFmt_NV61;
using NSCam::eImgFmt_RAW16;
using NSCam::eImgFmt_RAW_OPAQUE;
using NSCam::eImgFmt_RAW_START;
using NSCam::eImgFmt_RGB48;
using NSCam::eImgFmt_RGB565;
using NSCam::eImgFmt_RGB888;
using NSCam::eImgFmt_RGB_START;
using NSCam::eImgFmt_RGBA8888;
using NSCam::eImgFmt_RGBX8888;
using NSCam::eImgFmt_STA_2BYTE;
using NSCam::eImgFmt_STA_BYTE;
using NSCam::eImgFmt_STA_START;
using NSCam::eImgFmt_UFEO_BAYER10;
using NSCam::eImgFmt_UFEO_BAYER12;
using NSCam::eImgFmt_UFEO_BAYER14;
using NSCam::eImgFmt_UFEO_BAYER8;
using NSCam::eImgFmt_UFO_BAYER10;
using NSCam::eImgFmt_UFO_BAYER12;
using NSCam::eImgFmt_UFO_BAYER14;
using NSCam::eImgFmt_UFO_BAYER8;
using NSCam::eImgFmt_UFO_FG;
using NSCam::eImgFmt_UFO_FG_BAYER10;
using NSCam::eImgFmt_UFO_FG_BAYER12;
using NSCam::eImgFmt_UFO_FG_BAYER14;
using NSCam::eImgFmt_UFO_FG_BAYER8;
using NSCam::eImgFmt_UNKNOWN;
using NSCam::eImgFmt_UYVY;
using NSCam::eImgFmt_VENDOR_DEFINED_START;
using NSCam::eImgFmt_VYUY;
using NSCam::eImgFmt_WARP_2PLANE;
using NSCam::eImgFmt_WARP_3PLANE;
using NSCam::eImgFmt_Y16;
using NSCam::eImgFmt_Y8;
using NSCam::eImgFmt_Y800;
using NSCam::eImgFmt_YUV_START;
using NSCam::eImgFmt_YUY2;
using NSCam::eImgFmt_YV12;
using NSCam::eImgFmt_YV16;
using NSCam::eImgFmt_YVYU;

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_HEADER_H_
