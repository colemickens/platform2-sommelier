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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_MISC_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_MISC_H_

// STL
#include <mutex>  // std::unique_lock
#include "../../def/common.h"
#include "../../def/UITypes.h"
//
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
struct VISIBILITY_PUBLIC refine {
  static inline void not_larger_than(MSize* size, MSize const& limit) {
    if (size->w > limit.w) {
      size->w = limit.w;
    }
    if (size->h > limit.h) {
      size->h = limit.h;
    }
  }

  static inline void not_smaller_than(MSize* size, MSize const& limit) {
    if (size->w < limit.w) {
      size->w = limit.w;
    }
    if (size->h < limit.h) {
      size->h = limit.h;
    }
  }

  static inline MSize align_2(MSize const& size) {
#define align2(x) ((x + 1) & (~1))
    return MSize(align2(size.w), align2(size.h));
#undef align2
  }

  static inline MSize scale_roundup(MSize const& size, int mul, int div) {
    return MSize((size.w * mul + div - 1) / div,
                 (size.h * mul + div - 1) / div);
  }
};

namespace Utils {

/******************************************************************************
 * @brief dump call stack
 ******************************************************************************/
void VISIBILITY_PUBLIC dumpCallStack(char const* prefix = 0);

/******************************************************************************
 * @brief make all directories in path.
 *
 * @details
 * @note
 *
 * @param[in] path: a specified path to create.
 * @param[in] mode: the argument specifies the permissions to use, like 0777
 *                 (the same to that in mkdir).
 *
 * @return
 * -    true indicates success
 * -    false indicates failure
 *****************************************************************************/
bool VISIBILITY_PUBLIC makePath(char const* const path,
                                unsigned int const mode);

/******************************************************************************
 * @brief save the buffer to the file
 *
 * @details
 *
 * @note
 *
 * @param[in] fname: The file name
 * @param[in] buf: The buffer to save
 * @param[in] size: The size in bytes to save
 *
 * @return
 * -    true indicates success
 * -    false indicates failure
 ******************************************************************************/
bool VISIBILITY_PUBLIC saveBufToFile(char const* const fname,
                                     unsigned char* const buf,
                                     unsigned int const size);

/******************************************************************************
 * @brief load the file to the buffer
 *
 * @details
 *
 * @note
 *
 * @param[in] fname: The file name
 * @param[out] buf: The output buffer
 * @param[in] capacity: The buffer capacity in bytes
 *
 * @return
 * -   The loaded size in bytes.
 ******************************************************************************/
unsigned int VISIBILITY_PUBLIC loadFileToBuf(char const* const fname,
                                             unsigned char* const buf,
                                             unsigned int const capacity);

void setLogLevelToEngLoad(bool is_camera_on_off_timing,
                          bool set_log_level_to_eng,
                          int logCount);
/******************************************************************************
 *
 *****************************************************************************/
};      // namespace Utils
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_MISC_H_
