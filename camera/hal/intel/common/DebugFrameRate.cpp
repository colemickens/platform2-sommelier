/*
 * Copyright (C) 2011-2017 The Android Open Source Project
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
#define LOG_TAG "DebugFrameRate"

#include <time.h>
#include "DebugFrameRate.h"

NAMESPACE_DECLARATION {
#ifdef LIBCAMERA_RD_FEATURES

DebugFrameRate::DebugFrameRate(const char *streamName) :
    Thread(true)
    ,mCount(0)
    ,mStartTime(0)
    ,mActive(false)
    ,mStarted(false)
    ,mStreamName(streamName)
{
    if (gPerfLevel & CAMERA_DEBUG_LOG_PERF_TRACES) {
        mActive = true;
    }
}

DebugFrameRate::~DebugFrameRate()
{
}

status_t DebugFrameRate::start()
{
    if(mActive) {
        mStarted = true;
        return run(mStreamName);
    }

   return NO_ERROR;
}

void DebugFrameRate::update()
{
    mMutex.lock();
    ++mCount;
    mMutex.unlock();
}

status_t DebugFrameRate::requestExitAndWait()
{
    mMutex.lock();
    if(!mActive) {
        mMutex.unlock();
        return NO_ERROR;
    }

    mStarted = false;
    mActive = false;
    mCondition.signal();
    mMutex.unlock();

    return Thread::requestExitAndWait();
}

bool DebugFrameRate::threadLoop()
{
    Mutex::Autolock _lock(mMutex);
    while (mActive) {
        mCount = 0;
        mStartTime = systemTime();
        mCondition.waitRelative(mMutex, WAIT_TIME_NSECS);

        if (mActive == false) {
            LOGD("Exiting...\n");
            return false;
        }

        // compute stats
        double delta = (systemTime() - mStartTime) / 1000000000.0;
        float fps;

        delta = delta < 0.0 ? -delta : delta; // make sure is positive
        fps = mCount / delta;

        LOGD("[%s]time: %f seconds, frames: %d, fps: %f\n", mStreamName, (float) delta, mCount, fps);
    }

    return false;
}

#endif
} NAMESPACE_DECLARATION_END
