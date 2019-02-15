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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_FRAMECONTROLLER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_FRAMECONTROLLER_H_

#include <string>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace Utils {

/******************************************************************************
 *
 ******************************************************************************/
class FrameController {
 public:
  explicit FrameController(std::string userName);
  virtual ~FrameController();

  virtual std::string getName();
  virtual void bufferControl(int64_t frameTime);

 protected:
  std::string mName;
  int32_t miLogLevel;
  // 1: colloect first 5 frame info
  // 2: base on stage 1, find useful frame, initial display delay parameters
  // 3: start flow control
  int miDisplayControlStage;
  int64_t mAverageDisplayTime;
  int mAverageCount;
  //
  int64_t miFirstDelayTime;
  int64_t miAdjDelay;
  int64_t miP2interval;
  //
  bool mbFirstReturnFrame;
  int64_t mnLastFrameTime;
  int64_t mnCurrentFrameTime;
  int64_t mnFrameWaitTime;
  int64_t mnLastEnqueSystemTime;
  int64_t mnOldDisplayDelayTime;
  int64_t mnNewDisplayDelayTime;
  int64_t mnMinAdjDisplay;
  int64_t mnMaxAdjDisplay;
  bool mbApplyFrameControl;
  int64_t mnTimeInterval;
  int64_t mnFrameInterval;
  int64_t mnFrameMaxPlusDelay;
  int64_t mnFrameMaxSleep;  // ns
  int64_t mnFrameMinSleep;  // us
  int64_t mnAdjSleepTime;   // us
  bool mbFrameControlReset;
  bool mbFrameControlAdj;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace Utils
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_FRAMECONTROLLER_H_
