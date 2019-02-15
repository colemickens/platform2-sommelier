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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_EXIF_DEBUGEXIFUTILS_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_EXIF_DEBUGEXIFUTILS_H_

#include <mtkcam/def/BuiltinTypes.h>
#include <mtkcam/def/common.h>

#include <map>

// ---------------------------------------------------------------------------

namespace NSCam {
class IMetadata;
}

// ---------------------------------------------------------------------------

namespace NSCam {

class VISIBILITY_PUBLIC DebugExifUtils final {
 public:
  // NOTE: please expands this enumation if supoorting more debug exif
  enum class DebugExifType {
    DEBUG_EXIF_MF,
    DEBUG_EXIF_CAM,
    DEBUG_EXIF_RESERVE3,
  };

  static IMetadata* setDebugExif(
      const DebugExifType type,
      const MUINT32 tagKey,
      const MUINT32 tagData,
      const std::map<MUINT32, MUINT32>& debugInfoList,
      IMetadata* exifMetadata);

  static IMetadata* setDebugExif(const DebugExifType type,
                                 const MUINT32 tagKey,
                                 const MUINT32 tagData,
                                 const MUINT32 size,
                                 const void* debugInfoList,
                                 IMetadata* exifMetadata);
};

// ---------------------------------------------------------------------------

}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_EXIF_DEBUGEXIFUTILS_H_
