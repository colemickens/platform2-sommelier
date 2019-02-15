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

#define LOG_TAG "MtkCam/FrameController"
//
#include <mtkcam/def/common.h>
#include <mtkcam/utils/std/FrameController.h>
#include <mtkcam/utils/std/Log.h>
#include <property_service/property.h>
#include <property_service/property_lib.h>
#include <string>
#include <sys/time.h>

using NSCam::Utils::FrameController;

/******************************************************************************
 *
 *****************************************************************************/
FrameController::FrameController(std::string userName)
    : mName(userName),
      miLogLevel(1),
      miDisplayControlStage(1),
      mAverageDisplayTime(0),
      mAverageCount(0),
      miFirstDelayTime(0),
      miAdjDelay(0),
      miP2interval(0),
      mbFirstReturnFrame(true),
      mnLastFrameTime(0),
      mnCurrentFrameTime(0),
      mnFrameWaitTime(0),
      mnLastEnqueSystemTime(0),
      mnOldDisplayDelayTime(0),
      mnNewDisplayDelayTime(0),
      mnMinAdjDisplay(0),
      mnMaxAdjDisplay(0),
      mbApplyFrameControl(false),
      mnTimeInterval(0),
      mnFrameInterval(0),
      mnFrameMaxPlusDelay(30000000)  // ns
      ,
      mnFrameMaxSleep(1000000000)  // ns
      ,
      mnFrameMinSleep(200)  // us
      ,
      mnAdjSleepTime(250000)  // ns
      ,
      mbFrameControlReset(false),
      mbFrameControlAdj(false) {
  int32_t iLogLevel = property_get_int32("debug.camera.fctrl.loglevel", -1);
  if (iLogLevel != -1) {
    miLogLevel = iLogLevel;
  }

  int64_t iFrameMaxDelay = property_get_int32("debug.camera.fctrl.fmd", -1);
  if (iFrameMaxDelay != -1) {
    mnFrameMaxPlusDelay = iFrameMaxDelay * 1000000;
  }

  int64_t iFrameMaxSleep = property_get_int32("debug.camera.fctrl.smaxms", -1);
  if (iFrameMaxSleep != -1) {
    mnFrameMaxSleep = iFrameMaxSleep * 1000000;
  }

  int64_t iFrameMinSleep = property_get_int32("debug.camera.fctrl.sminus", -1);
  if (iFrameMinSleep != -1) {
    mnFrameMinSleep = iFrameMinSleep;
  }

  int64_t iAdjSleepTime = property_get_int32("debug.camera.fctrl.adjsleep", -1);
  if (iAdjSleepTime != -1) {
    mnAdjSleepTime = iAdjSleepTime;
  }
  MY_LOGI(
      "- miLogLevel(%d), max delay(%lld), max sleep(%lld ns), min sleep(%lld "
      "us), adj sleep(%lld ns)",
      miLogLevel, mnFrameMaxPlusDelay, mnFrameMaxSleep, mnFrameMinSleep,
      mnAdjSleepTime);
}

/******************************************************************************
 *
 *****************************************************************************/
FrameController::~FrameController() {}

/******************************************************************************
 *
 *****************************************************************************/
std::string FrameController::getName() {
  return mName;
}

/******************************************************************************
 *
 *****************************************************************************/
void FrameController::bufferControl(int64_t frameTime) {
  struct timeval t;
  mnCurrentFrameTime = frameTime;
  switch (miDisplayControlStage) {
    case 1: {
      MY_LOGD("framecontrol stage(%d)", miDisplayControlStage);
      gettimeofday(&t, NULL);
      int64_t currentTime = (t.tv_sec) * 1000000000LL + (t.tv_usec) * 1000LL;
      int64_t currentFrameTime = mnCurrentFrameTime;
      int64_t currentDisplayDelay = currentTime - currentFrameTime;
      mAverageDisplayTime += currentDisplayDelay;
      mAverageCount += 1;
      if (mAverageCount == 5) {
        mAverageDisplayTime /= 5;
        miDisplayControlStage = 2;
        MY_LOGD("mAverageDisplayTime(%lld)", mAverageDisplayTime);
      }
    } break;
    case 2: {
      MY_LOGD("framecontrol stage(%d)", miDisplayControlStage);
      gettimeofday(&t, NULL);
      int64_t currentTime = (t.tv_sec) * 1000000000LL + (t.tv_usec) * 1000LL;
      int64_t currentDisplayDelay = currentTime - mnCurrentFrameTime;
      MY_LOGD("currentDisplayDelay(%lld),mAverageDisplayTime(%lld)",
              currentDisplayDelay, mAverageDisplayTime);
      if (currentDisplayDelay <= mAverageDisplayTime) {
        mnLastFrameTime = mnCurrentFrameTime;
        miFirstDelayTime = property_get_int32("debug.displaydelay.ms", 15);
        miAdjDelay = property_get_int32("debug.displaydelay.adjdelay", 5000000);
        miP2interval = property_get_int32("debug.displaydelay.p2", 30000000);
        MY_LOGI("FirstFrameDelay(%lld)ms", miFirstDelayTime);
        miFirstDelayTime = miFirstDelayTime * 1000;  // ms to us
        usleep(miFirstDelayTime);
        MY_LOGI("FirstFrameDelay(%lld)ms", miFirstDelayTime);
        miFirstDelayTime = miFirstDelayTime * 1000;
        miDisplayControlStage = 3;
      }
    } break;
    case 3: {
      gettimeofday(&t, NULL);
      int64_t currentTime = (t.tv_sec) * 1000000000LL + (t.tv_usec) * 1000LL;
      int64_t currentFrameTime = mnCurrentFrameTime;
      int64_t lastFrameTimeBK = mnLastFrameTime;
      mnFrameInterval = currentFrameTime - mnLastFrameTime;
      mnLastFrameTime = currentFrameTime;
      //
      mnOldDisplayDelayTime = mnFrameInterval + miP2interval + miFirstDelayTime;
      mnNewDisplayDelayTime = mnOldDisplayDelayTime + mnFrameMaxPlusDelay;
      mnMaxAdjDisplay = mnOldDisplayDelayTime + miAdjDelay;
      mnMinAdjDisplay = mnOldDisplayDelayTime;
      // use max delay reset method
      int64_t currentDisplayDelay = currentTime - currentFrameTime;
      int64_t currentDisplayDelayAfterSleep;
      // time delay range base on frame rate

      if (!mbFrameControlReset) {
        bool doSleep = false;
        int64_t sleepus = 0;
        gettimeofday(&t, NULL);
        int64_t currentTime = (t.tv_sec) * 1000000000LL + (t.tv_usec) * 1000LL;
        mnTimeInterval = currentTime - mnLastEnqueSystemTime;
        mnFrameWaitTime = (mnFrameInterval) - (mnTimeInterval);
        int64_t mnWaitTime = mnFrameWaitTime - mnAdjSleepTime;
        if (mnWaitTime > 0 && mnWaitTime < mnFrameMaxSleep) {
          sleepus = mnWaitTime / 1000;
          if (mbFrameControlAdj) {
            sleepus = sleepus - 1000;
          }
          // only do sleep when sleep more than 500us
          if (sleepus > mnFrameMinSleep) {
            doSleep = true;
          }
        }
        if (!doSleep) {
          sleepus = 0;
        }
        currentDisplayDelayAfterSleep = currentDisplayDelay + (sleepus * 1000);
        // if reach to max display delay, disable frame control
        if (currentDisplayDelayAfterSleep >= mnNewDisplayDelayTime) {
          mbFrameControlReset = true;
          mbFrameControlAdj = false;
          MY_LOG_ID((2 <= miLogLevel), "start to reset frame delay");
        }

        if (!mbFrameControlReset) {
          // if display delay reach to defined value, start to sleep less for
          // next frame to decrease display delay
          if (!mbFrameControlAdj) {
            if (currentDisplayDelayAfterSleep >= mnMaxAdjDisplay) {
              mbFrameControlAdj = true;
              MY_LOG_ID((2 <= miLogLevel), "start to adj frame delay");
            }
          } else {
            if (currentDisplayDelayAfterSleep <= mnMinAdjDisplay) {
              mbFrameControlAdj = false;
              MY_LOG_ID((2 <= miLogLevel), "stop adj frame delay");
            }
          }
        }
        if (doSleep) {
          MY_LOG_ID((2 <= miLogLevel), "start sleep");
          usleep(sleepus);
        }
        MY_LOG_ID((2 <= miLogLevel),
                  "off:ct(%lld),cft(%lld),cddt(%lld),oddt(%lld),nddt(%lld),max("
                  "%lld),min(%lld),fi(%lld),ti(%lld),stc(%d),st(%lld us)",
                  currentTime, currentFrameTime, currentDisplayDelayAfterSleep,
                  mnOldDisplayDelayTime, mnNewDisplayDelayTime, mnMaxAdjDisplay,
                  mnMinAdjDisplay, mnFrameInterval, mnTimeInterval,
                  mbFrameControlAdj, sleepus);
        if (mnWaitTime >= mnFrameMaxSleep) {
          MY_LOGW("Check Frame conrol parameters, something wrong");
          MY_LOGW(
              "ct(%lld),cft(%lld),lft(%lld),cddt(%lld),nddt(%lld),fi(%lld),ti(%"
              "lld)",
              currentTime, currentFrameTime, lastFrameTimeBK,
              currentDisplayDelay + (sleepus * 1000), mnNewDisplayDelayTime,
              mnFrameInterval, mnTimeInterval);
        }
      } else {
        MY_LOG_ID(
            (2 <= miLogLevel),
            " on:ct(%lld),cft(%lld),cddt(%lld),oddt(%lld),fi(%lld),ti(%lld)",
            currentTime, currentFrameTime, currentDisplayDelay,
            mnOldDisplayDelayTime, mnFrameInterval, mnTimeInterval);

        if (currentDisplayDelay <=
            ((mnOldDisplayDelayTime + mnNewDisplayDelayTime) / 2)) {
          mbFrameControlReset = false;
          mbFrameControlAdj = false;
          MY_LOG_ID((2 <= miLogLevel), "stop reset frame delay");
        }
      }
    } break;
    default:
      MY_LOGE("Display control stage error, should not happened");
      break;
  }

  gettimeofday(&t, NULL);
  mnLastEnqueSystemTime = (t.tv_sec) * 1000000000LL + (t.tv_usec) * 1000LL;
}
