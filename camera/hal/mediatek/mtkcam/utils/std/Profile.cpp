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
#include <inttypes.h>
#include "MyUtils.h"
#include <string>
#include <sys/time.h>

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
DurationTool::DurationTool(char const* const szSubjectName)
    : ms8SubjectName(std::string(szSubjectName)), mi4Count(0) {
  struct timeval t;
  gettimeofday(&t, NULL);
  int64_t sys_time = (t.tv_sec) * 1000000000LL + (t.tv_usec) * 1000LL;
  mnsStart = sys_time;
  mnsEnd = sys_time;
}

/******************************************************************************
 *
 ******************************************************************************/
DurationTool::DurationTool(char const* const szSubjectName,
                           int64_t nsInitTimestamp)
    : ms8SubjectName(std::string(szSubjectName)),
      mi4Count(0),
      mnsStart(nsInitTimestamp),
      mnsEnd(nsInitTimestamp) {}

/******************************************************************************
 *
 ******************************************************************************/
void DurationTool::reset() {
  struct timeval t;
  gettimeofday(&t, NULL);
  int64_t sys_time = (t.tv_sec) * 1000000000LL + (t.tv_usec) * 1000LL;
  reset(sys_time);
}

/******************************************************************************
 *
 ******************************************************************************/
void DurationTool::reset(int64_t nsInitTimestamp) {
  mi4Count = 0;
  mnsStart = nsInitTimestamp;
  mnsEnd = nsInitTimestamp;
}

/******************************************************************************
 *
 ******************************************************************************/
void DurationTool::update() {
  struct timeval t;
  gettimeofday(&t, NULL);
  int64_t sys_time = (t.tv_sec) * 1000000000LL + (t.tv_usec) * 1000LL;
  update(sys_time);
}

/******************************************************************************
 *
 ******************************************************************************/
void DurationTool::update(int64_t nsTimestamp) {
  mi4Count++;
  mnsEnd = nsTimestamp;
}

/******************************************************************************
 *
 ******************************************************************************/
void DurationTool::showFps() const {
  int64_t const nsDuration = mnsEnd - mnsStart;
  float const fFps = (static_cast<float>(mi4Count) / nsDuration) * 1000000000LL;
  CAM_LOGD_IF(1, "[%s] fps: %d / %" PRId64 " ns = %3f", ms8SubjectName.c_str(),
              mi4Count, nsDuration, fFps);
}

/******************************************************************************
 *
 ******************************************************************************/
CamProfile::CamProfile(char const* const pszFuncName,
                       char const* const pszClassName)
    : mpszClassName(pszClassName),
      mpszFuncName(pszFuncName),
      mIdx(0),
      mfgIsProfile(false) {
  struct timeval t;
  gettimeofday(&t, NULL);
  int32_t sys_time = (t.tv_sec) * 1000000000LL + (t.tv_usec) * 1000LL;
  mi4StartUs = sys_time;
  mi4LastUs = sys_time;
  mfgIsProfile = true;
}

/******************************************************************************
 *
 ******************************************************************************/
bool CamProfile::print(char const* const fmt /*= ""*/, ...) const {
  if (!mfgIsProfile) {
    return false;
  }

  struct timeval t;
  gettimeofday(&t, NULL);
  int32_t sys_time = (t.tv_sec) * 1000000000LL + (t.tv_usec) * 1000LL;
  int32_t const i4EndUs = (sys_time / 1000);
  if (0 == mIdx) {
    va_list args;
    va_start(args, fmt);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    std::string result(base::StringPrintf(fmt, args));
#pragma clang diagnostic pop

    va_end(args);
    //
    CAM_LOGD(
        "{CamProfile}[%s::%s] %s: "
        "(%d-th) ===> [start-->now: %d ms]",
        mpszClassName, mpszFuncName, result.c_str(), mIdx++,
        (i4EndUs - mi4StartUs) / 1000);
  } else {
    va_list args;
    va_start(args, fmt);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    std::string result(base::StringPrintf(fmt, args));
#pragma clang diagnostic pop
    va_end(args);
    //
    CAM_LOGD(
        "{CamProfile}[%s::%s] %s: "
        "(%d-th) ===> [start-->now: %d ms] [last-->now: %d ms]",
        mpszClassName, mpszFuncName, result.c_str(), mIdx++,
        (i4EndUs - mi4StartUs) / 1000, (i4EndUs - mi4LastUs) / 1000);
  }
  mi4LastUs = i4EndUs;
  return true;
}

/******************************************************************************
 *
 ******************************************************************************/
bool CamProfile::print_overtime(int32_t const msTimeInterval,
                                char const* const fmt /*= ""*/,
                                ...) const {
  if (!mfgIsProfile) {
    return false;
  }
  //
  bool ret = false;
  //
  struct timeval t;
  gettimeofday(&t, NULL);
  int32_t sys_time = (t.tv_sec) * 1000000000LL + (t.tv_usec) * 1000LL;
  int32_t const i4EndUs = (sys_time / 1000);
  if (0 == mIdx) {
    int32_t const msElapsed = (i4EndUs - mi4StartUs) / 1000;
    ret = (msTimeInterval < msElapsed);
    if (ret) {
      va_list args;
      va_start(args, fmt);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
      std::string result(base::StringPrintf(fmt, args));
#pragma clang diagnostic pop

      va_end(args);
      //
      CAM_LOGI(
          "{CamProfile}[%s::%s] %s: "
          "(%d-th) ===> (overtime > %d ms) [start-->now: %d ms]",
          mpszClassName, mpszFuncName, result.c_str(), mIdx, msTimeInterval,
          msElapsed);
      //
      goto lbExit;
    }
  } else {
    int32_t const msElapsed = (i4EndUs - mi4LastUs) / 1000;
    ret = (msTimeInterval < msElapsed);
    if (ret) {
      va_list args;
      va_start(args, fmt);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
      std::string result(base::StringPrintf(fmt, args));
#pragma clang diagnostic pop
      va_end(args);
      //
      CAM_LOGI(
          "{CamProfile}[%s::%s] %s: "
          "(%d-th) ===> ( overtime > %d ms) [start-->now: %d ms] [last-->now: "
          "%d ms]",
          mpszClassName, mpszFuncName, result.c_str(), mIdx, msTimeInterval,
          (i4EndUs - mi4StartUs) / 1000, msElapsed);
      //
      goto lbExit;
    }
  }
  //
lbExit:
  mIdx++;
  mi4LastUs = i4EndUs;
  return ret;
}

};  // namespace Utils
};  // namespace NSCam
