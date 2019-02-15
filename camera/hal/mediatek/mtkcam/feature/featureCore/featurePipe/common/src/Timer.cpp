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

#include "../include/Timer.h"

#include "../include/DebugControl.h"
#define PIPE_TRACE TRACE_TIMER
#define PIPE_CLASS_TAG "Timer"
#include "../include/PipeLog.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

Timer::Timer(bool start) : mKeepRunning(start), mCumulative(0) {
  if (start) {
    clock_gettime(CLOCK_MONOTONIC, &mStart);
  }
}

Timer::Timer(const timespec& start)
    : mStart(start), mKeepRunning(true), mCumulative(0) {}

MVOID Timer::start() {
  TRACE_FUNC_ENTER();
  clock_gettime(CLOCK_MONOTONIC, &mStart);
  mCumulative = 0;
  mKeepRunning = true;
  TRACE_FUNC_EXIT();
}

MVOID Timer::resume() {
  TRACE_FUNC_ENTER();
  clock_gettime(CLOCK_MONOTONIC, &mStart);
  mKeepRunning = true;
  TRACE_FUNC_EXIT();
}

MVOID Timer::stop(bool keepRunning) {
  TRACE_FUNC_ENTER();
  if (mKeepRunning) {
    clock_gettime(CLOCK_MONOTONIC, &mStop);
    mCumulative += diff(mStart, mStop);
    mKeepRunning = keepRunning;
    mStart = mStop;
  }
  TRACE_FUNC_EXIT();
}

MUINT32 Timer::getElapsed() const {
  return mCumulative;
}

MUINT32 Timer::getNow() const {
  TRACE_FUNC_ENTER();
  MUINT32 ret = 0;
  timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  ret = mCumulative;
  if (mKeepRunning) {
    ret += diff(mStart, now);
  }
  TRACE_FUNC_EXIT();
  return ret;
}

timespec Timer::getTimeSpec() {
  timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return now;
}

MUINT32 Timer::diff(const timespec& from, const timespec& to) {
  TRACE_FUNC_ENTER();
  MUINT32 diff = 0;
  if (to.tv_sec || to.tv_nsec || from.tv_sec || from.tv_nsec) {
    diff = ((to.tv_sec - from.tv_sec) * 1000) +
           ((to.tv_nsec - from.tv_nsec) / 1000000);
  }
  TRACE_FUNC_EXIT();
  return diff;
}

FPSCounter::FPSCounter() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

FPSCounter::~FPSCounter() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

MVOID FPSCounter::update(const timespec& mark) {
  TRACE_FUNC_ENTER();
  mMarks.push(mark);
  if (mMarks.size() > 30) {
    mMarks.pop();
  }
  TRACE_FUNC_EXIT();
}

double FPSCounter::getFPS() const {
  TRACE_FUNC_ENTER();
  double fps = 0;
  unsigned size = mMarks.size();
  if (size > 1) {
    MUINT32 time = Timer::diff(mMarks.front(), mMarks.back());
    if (time > 0) {
      fps = 1000.0 * (size - 1) / time;
    }
  }
  TRACE_FUNC_EXIT();
  return fps;
}

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam
