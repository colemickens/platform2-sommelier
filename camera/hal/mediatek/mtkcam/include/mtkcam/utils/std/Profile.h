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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_PROFILE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_PROFILE_H_
/******************************************************************************
 *
 ******************************************************************************/
#include <mtkcam/def/common.h>
#include <string>
#include <sys/time.h>
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace Utils {

/******************************************************************************
 *
 ******************************************************************************/

/******************************************************************************
 *  get the time in micro-seconds
 ******************************************************************************/
inline int64_t s2ns(int64_t secs) {
  return secs * 1000000000;
}

inline int64_t ns2ms(int64_t secs) {
  return secs / 1000000;
}

inline int64_t ms2ns(int64_t secs) {
  return secs * 1000000;
}

inline int64_t getTimeInUs() {
  struct timeval t;
  gettimeofday(&t, NULL);
  int64_t sys_time = (t.tv_sec) * 1000000000LL + (t.tv_usec) * 1000LL;
  return (sys_time / 1000);
}

/******************************************************************************
 *  get the time in milli-seconds
 ******************************************************************************/
inline int64_t getTimeInMs() {
  struct timeval t;
  gettimeofday(&t, NULL);
  int64_t sys_time = (t.tv_sec) * 1000000000LL + (t.tv_usec) * 1000LL;
  return (sys_time / 1000000);
}

/******************************************************************************
 *  get the time in n-seconds
 ******************************************************************************/
inline int64_t getTimeInNs() {
  struct timeval t;
  gettimeofday(&t, NULL);
  return (t.tv_sec) * 1000000000LL + (t.tv_usec) * 1000LL;
}

/******************************************************************************
 *
 ******************************************************************************/
class DurationTool {
 public:  ////        Interfaces.
  explicit DurationTool(char const* const szSubjectName);
  DurationTool(char const* const szSubjectName, int64_t nsInitTimestamp);
  //
  void reset();
  void reset(int64_t nsInitTimestamp);
  //
  void update();
  void update(int64_t nsTimestamp);
  //
  void showFps() const;
  //
  int32_t getCount() const { return mi4Count; }
  int64_t getDuration() const { return mnsEnd - mnsStart; }
  //
 protected:  ////        Data Members.
  std::string ms8SubjectName;
  //
  int32_t mi4Count;
  int64_t mnsStart;
  int64_t mnsEnd;
};

/******************************************************************************
 *
 ******************************************************************************/
class VISIBILITY_PUBLIC CamProfile {
 public:  ////        Interfaces.
  CamProfile(char const* const pszFuncName,
             char const* const pszClassName = "");
  //
  inline void enable(bool fgEnable) { mfgIsProfile = fgEnable; }
  //
  bool print(char const* const fmt, ...) const;
  //
  bool print_overtime(int32_t const msTimeInterval,
                      char const* const fmt,
                      ...) const;
  //
  int32_t getDiffTime() { return (mi4LastUs - mi4StartUs); }

 protected:  ////        Data Members.
  char const* const mpszClassName;
  char const* const mpszFuncName;
  mutable int32_t mIdx;
  mutable int32_t mi4StartUs;
  mutable int32_t mi4LastUs;
  bool mfgIsProfile;
};

/******************************************************************************
 *
 *****************************************************************************/
};      // namespace Utils
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_PROFILE_H_
