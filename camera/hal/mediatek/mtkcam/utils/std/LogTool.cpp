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

#define LOG_TAG "MtkCam/Utils"
//
#include "MyUtils.h"
#include <mtkcam/utils/std/LogTool.h>
#include <string>

/******************************************************************************
 *
 *****************************************************************************/

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace Utils {

/******************************************************************************
 *
 ******************************************************************************/
LogTool* LogTool::get() {
  static LogTool* inst = new LogTool();
  return inst;
}

/******************************************************************************
 *
 ******************************************************************************/
bool LogTool::getCurrentLogTime(struct timespec* ts) {
  if (!ts) {
    MY_LOGE("timespec is nullptr");
    return false;
  }

  if (0 != clock_gettime(CLOCK_REALTIME, ts)) {
    MY_LOGE("clock_gettime: failure");
    ts->tv_sec = 0;
    ts->tv_nsec = 0;
    return false;
  }

  return true;
}

/******************************************************************************
 *
 ******************************************************************************/
std::string LogTool::convertToFormattedLogTime(const struct timespec* ts) {
  if (!ts) {
    MY_LOGE("timespec is nullptr");
    return std::string{};
  }

  struct tm tmBuf;
  localtime_r(&ts->tv_sec, &tmBuf);

  char timeBuf[32] = {"01-01 02:32:54.123000000"};
  strftime(timeBuf, sizeof(timeBuf), "%m-%d %H:%M:%S", &tmBuf);

  ::snprintf(&timeBuf[14], sizeof(timeBuf) - 14, ".%03lu",
             ts->tv_nsec / 1000000);
  return timeBuf;
}

/******************************************************************************
 *
 ******************************************************************************/
std::string LogTool::getFormattedLogTime() {
  struct timespec ts;
  if (getCurrentLogTime(&ts)) {
    return convertToFormattedLogTime(&ts);
  }
  return std::string{};
}

};  // namespace Utils
};  // namespace NSCam
