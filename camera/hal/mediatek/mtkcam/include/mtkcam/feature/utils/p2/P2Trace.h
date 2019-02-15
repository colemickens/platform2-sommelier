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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_P2TRACE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_P2TRACE_H_

#include <algorithm>
#include <mtkcam/utils/std/Trace.h>
#include <property_lib.h>

#define TRACE_DEFAULT 0
#define TRACE_ADVANCED 1

#define P2_NEED_TRACE NSCam::CamSysTrace::needP2Trace
#define _PASTE(x, y) x##y
#define PASTE(x, y) _PASTE(x, y)

#define P2_CAM_TRACE_NAME(lv, name) \
  NSCam::CamSysTrace PASTE(___camtracer, __LINE__)(P2_NEED_TRACE(lv), name)
#define P2_CAM_TRACE_CALL(lv) P2_CAM_TRACE_NAME(lv, __FUNCTION__)
#define P2_CAM_TRACE_BEGIN(lv, name) \
  NSCam::CamSysTrace::begin(P2_NEED_TRACE(lv), name)
#define P2_CAM_TRACE_END(lv) NSCam::CamSysTrace::end(P2_NEED_TRACE(lv))
#define P2_CAM_TRACE_ASYNC_BEGIN(lv, name, cookie) \
  NSCam::CamSysTrace::asyncBegin(P2_NEED_TRACE(lv), name, cookie)
#define P2_CAM_TRACE_ASYNC_END(lv, name, cookie) \
  NSCam::CamSysTrace::asyncEnd(P2_NEED_TRACE(lv), name, cookie)

#define P2_CAM_TRACE_FMT_BEGIN(lv, fmt, arg...) \
  do {                                          \
    if (P2_NEED_TRACE(lv)) {                    \
      CAM_TRACE_FMT_BEGIN(fmt, ##arg);          \
    }                                           \
  } while (0)

namespace NSCam {

class CamSysTrace {
 public:
  inline CamSysTrace(bool need, const char* name) : mNeed(need) {
    if (mNeed) {
      CAM_TRACE_BEGIN(name);
    }
  }
  inline ~CamSysTrace() {
    if (mNeed) {
      CAM_TRACE_END();
    }
  }

  static inline void begin(bool need, const char* name) {
    if (need) {
      CAM_TRACE_BEGIN(name);
    }
  }

  static inline void end(bool need) {
    if (need) {
      CAM_TRACE_END();
    }
  }

  static inline void asyncBegin(bool need, const char* name, int32_t cookie) {
    if (need) {
      CAM_TRACE_ASYNC_BEGIN(name, cookie);
    }
  }

  static inline void asyncEnd(bool need, const char* name, int32_t cookie) {
    if (need) {
      CAM_TRACE_ASYNC_END(name, cookie);
    }
  }

  static inline bool needP2Trace(int level) {
    static int32_t camProp =
        property_get_int32("vendor.debug.mtkcam.systrace.level",
                           0 /*MTKCAM_SYSTRACE_LEVEL_DEFAULT*/);
    static int32_t debugProp =
        property_get_int32("vendor.debug.systrace.p2", 0);
    static int32_t persistProp =
        property_get_int32("persist.vendor.systrace.p2", 0);
    static int32_t threshold =
        std::max(std::max(camProp, debugProp), persistProp);
    return threshold >= level;
  }

 private:
  bool mNeed;
};
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_P2TRACE_H_
