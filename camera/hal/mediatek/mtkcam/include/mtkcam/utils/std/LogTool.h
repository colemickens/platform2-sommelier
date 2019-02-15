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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_LOGTOOL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_LOGTOOL_H_

/******************************************************************************
 *
 ******************************************************************************/
#include <string>
#include <time.h>
#include <mtkcam/def/common.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace Utils {

/******************************************************************************
 *
 ******************************************************************************/
class VISIBILITY_PUBLIC LogTool {
 public:  ////        Interfaces.
  static LogTool* get();

 public:  ////        Interfaces.
  /**
   * Get current time for logging.
   */
  bool getCurrentLogTime(struct timespec* ts);

  /**
   * Convert a given timespec to a formatted log format (e.g. "01-01
   * 01:18:29.203").
   */
  std::string convertToFormattedLogTime(const struct timespec* ts);

  /**
   * Get current time with formatted log format (e.g. "01-01 01:18:29.203").
   */
  std::string getFormattedLogTime();
};

/******************************************************************************
 *
 *****************************************************************************/
};      // namespace Utils
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_LOGTOOL_H_
