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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_TIMER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_TIMER_H_

#include "MtkHeader.h"
#include <queue>
#include <time.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

#define ADD_TIMER(name)                                   \
  Timer mTimer_##name;                                    \
  MVOID start##name() { mTimer_##name.start(); }          \
  MVOID stop##name(bool keepRunning = true) {             \
    return mTimer_##name.stop(keepRunning);               \
  }                                                       \
  MVOID resume##name() { return mTimer_##name.resume(); } \
  MUINT32 getElapsed##name() const { return mTimer_##name.getElapsed(); }

#define ADD_TIMER_LIST(name, count)                                       \
  Timer mTimer_##name[count];                                             \
  MVOID start##name(int index) { mTimer_##name[index].start(); }          \
  MVOID stop##name(int index, bool keep = true) {                         \
    return mTimer_##name[index].stop(keep);                               \
  }                                                                       \
  MVOID resume##name(int index) { return mTimer_##name[index].resume(); } \
  MUINT32 getElapsed##name(int index) const {                             \
    return mTimer_##name[index].getElapsed();                             \
  }

class VISIBILITY_PUBLIC Timer {
 public:
  enum Flag { STOP_RUNNING = 0, KEEP_RUNNING = 1 };

 public:
  explicit Timer(bool start = false);
  explicit Timer(const timespec& start);
  MVOID start();
  MVOID resume();
  MVOID stop(bool keepRunning = KEEP_RUNNING);

  // millisecond
  MUINT32 getElapsed() const;
  MUINT32 getNow() const;
  static timespec getTimeSpec();
  static MUINT32 diff(const timespec& from, const timespec& to);

 protected:
  timespec mStart;
  timespec mStop;
  bool mKeepRunning;
  MUINT32 mCumulative;

 private:
  Timer& operator=(const Timer&);
};

class VISIBILITY_PUBLIC FPSCounter {
 public:
  FPSCounter();
  ~FPSCounter();
  MVOID update(const timespec& mark);
  double getFPS() const;

 private:
  std::queue<timespec> mMarks;
};

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_TIMER_H_
