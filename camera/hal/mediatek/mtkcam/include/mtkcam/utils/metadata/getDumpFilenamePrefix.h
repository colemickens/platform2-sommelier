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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_METADATA_GETDUMPFILENAMEPREFIX_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_METADATA_GETDUMPFILENAMEPREFIX_H_

#include <mtkcam/utils/metadata/IMetadata.h>

namespace NSCam {

//////////////////////////////////////////////////////////////////////////
// Get dump filename prefix from metadata
//
// Prefix:
//   (out) pointer to char buffer, in which will be stored dump path and
//   filename prefix ex: (2017.6.14)
//       /camera_dump/uuuuuuuuu-ffff-rrrr
//   where:
//       uuuuuuuuu : 9 digits, pipeline model uniquekey
//       ffff      : 4 digits (maximun 8 digits), frame number
//       rrrr      : 4 digits, request number
//
// nPrefix:
//   (in)  sizeof Prefix (in byte), suggest 64 bytes.
// appMeta:
//   (in)  pointer to APP metadata (reserved, not used now)
// halMeta:
//   (in)  pointer to HAL metadata
//
// return value
//   if success, return a pointer to Prefix buffer
//   if fail, return a pointer to "" (empty string buffer)
const char* getDumpFilenamePrefix(char* pPrefix,
                                  int nPrefix,
                                  const IMetadata* appMeta,
                                  const IMetadata* halMeta);

};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_METADATA_GETDUMPFILENAMEPREFIX_H_
