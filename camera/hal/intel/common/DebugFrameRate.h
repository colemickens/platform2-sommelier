/*
 * Copyright (C) 2012-2017 Intel Corporation
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

#ifndef ANDROID_CAMERA_DEBUG_FRAME_RATE
#define ANDROID_CAMERA_DEBUG_FRAME_RATE

#ifdef LIBCAMERA_RD_FEATURES
#include <utils/Mutex.h>
#include <utils/Condition.h>
#endif
#include <Utils.h>
#include "LogHelper.h"

NAMESPACE_DECLARATION {
#ifdef LIBCAMERA_RD_FEATURES

class DebugFrameRate : public Thread {

public:
    explicit DebugFrameRate(const char* /*streamName*/);
    ~DebugFrameRate();

    void update();
    status_t requestExitAndWait();  // override
    status_t start();
    bool isStarted() { return mStarted; }

private:
    virtual bool threadLoop();

private:

    static const int WAIT_TIME_NSECS = 2000000000; // 2 seconds

    int mCount;
    nsecs_t mStartTime;
    Condition mCondition;
    Mutex mMutex; /* protect mCount mActive mStarted mStartTime */
    bool mActive;
    bool mStarted;
    const char *mStreamName;
};

#else

class DebugFrameRate {
public:
    DebugFrameRate(const char*) {}
    ~DebugFrameRate() {}
    void update() {}
    status_t start() { return NO_ERROR; }
    status_t requestExitAndWait() { return NO_ERROR; }
    bool isStarted() { return false; }
};

#endif
} NAMESPACE_DECLARATION_END

#endif // ANDROID_CAMERA_DEBUG_FPS
