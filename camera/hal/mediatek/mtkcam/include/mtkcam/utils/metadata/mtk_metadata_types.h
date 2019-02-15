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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_METADATA_MTK_METADATA_TYPES_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_METADATA_MTK_METADATA_TYPES_H_

#include "IMetadata.h"

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/******************************************************************************
 *  Type Info Utility
 ******************************************************************************/
enum MTK_TYPE_ENUM {
  TYPE_UNKNOWN = -1,
  TYPE_MUINT8,     //= TYPE_BYTE,  // Unsigned 8-bit integer (uint8_t)
  TYPE_MINT32,     //= TYPE_INT32,  // Signed 32-bit integer (int32_t)
  TYPE_MFLOAT,     // = TYPE_FLOAT,  // 32-bit float (float)
  TYPE_MINT64,     // = TYPE_INT64,  // Signed 64-bit integer (int64_t)
  TYPE_MDOUBLE,    // = TYPE_DOUBLE, // 64-bit float (double)
  TYPE_MRational,  // = TYPE_RATIONAL,// A 64-bit fraction
                   // (camera_metadata_rational_t)
  // -- MTK -- //
  TYPE_MPoint,
  TYPE_MSize,
  TYPE_MRect,
  TYPE_IMetadata,
  TYPE_Memory,
  NUM_MTYPES  // Number of type fields
};

/******************************************************************************
 *
 ******************************************************************************/
template <typename _T>
struct Type2TypeEnum {};
template <>
struct Type2TypeEnum<MUINT8> {
  enum { typeEnum = TYPE_MUINT8 };
};
template <>
struct Type2TypeEnum<MINT32> {
  enum { typeEnum = TYPE_MINT32 };
};
template <>
struct Type2TypeEnum<MFLOAT> {
  enum { typeEnum = TYPE_MFLOAT };
};
template <>
struct Type2TypeEnum<MINT64> {
  enum { typeEnum = TYPE_MINT64 };
};
template <>
struct Type2TypeEnum<MDOUBLE> {
  enum { typeEnum = TYPE_MDOUBLE };
};
template <>
struct Type2TypeEnum<MRational> {
  enum { typeEnum = TYPE_MRational };
};
template <>
struct Type2TypeEnum<MPoint> {
  enum { typeEnum = TYPE_MPoint };
};
template <>
struct Type2TypeEnum<MSize> {
  enum { typeEnum = TYPE_MSize };
};
template <>
struct Type2TypeEnum<MRect> {
  enum { typeEnum = TYPE_MRect };
};
template <>
struct Type2TypeEnum<IMetadata> {
  enum { typeEnum = TYPE_IMetadata };
};
template <>
struct Type2TypeEnum<IMetadata::Memory> {
  enum { typeEnum = TYPE_Memory };
};

};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_METADATA_MTK_METADATA_TYPES_H_
