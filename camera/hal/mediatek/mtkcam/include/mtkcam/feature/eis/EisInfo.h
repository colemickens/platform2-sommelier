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

/**
 * @file EisInfo.h
 *
 * EisInfo Header File
 */

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_EIS_EISINFO_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_EIS_EISINFO_H_
#include <math.h>

namespace NSCam {
namespace EIS {

enum VIDEO_CFG {
  VIDEO_CFG_FHD,
  VIDEO_CFG_4K2K,
};

struct EisInfo {
  EisInfo()
      : videoConfig(VIDEO_CFG_FHD),
        queueSize(0),
        startFrame(1),
        factor(100),
        mode(0),
        lossless(0) {}

  explicit EisInfo(const MUINT64 packedInfo) {
    videoConfig = getVideoConfig(packedInfo);
    queueSize = getQueueSize(packedInfo);
    startFrame = getStartFrame(packedInfo);
    factor = getFactor(packedInfo);
    mode = getMode(packedInfo);
    lossless = isLossless(packedInfo);
  }

  static MINT32 getVideoConfig(const MUINT64 packedInfo) {
    return packedInfo % 10;
  }

  static MINT32 getQueueSize(const MUINT64 packedInfo) {
    return (packedInfo % (MUINT64)pow(10, 3)) / 10;
  }

  static MINT32 getStartFrame(const MUINT64 packedInfo) {
    return (packedInfo % (MUINT64)pow(10, 5)) / pow(10, 3);
  }

  static MINT32 getFactor(const MUINT64 packedInfo) {
    MINT32 factor = (packedInfo % (MUINT64)pow(10, 8)) / pow(10, 5);
    return (factor > 100) ? factor : 100;
  }

  static MINT32 getMode(const MUINT64 packedInfo) {
    return (packedInfo % (MUINT64)pow(10, 12)) / pow(10, 8);
  }

  static MINT32 isLossless(const MUINT64 packedInfo) {
    return (packedInfo % (MUINT64)pow(10, 13)) / pow(10, 12);
  }

  MUINT64 toPackedData() {
    MUINT64 packedInfo = 0;
    packedInfo += videoConfig;
    packedInfo += queueSize * 10;
    packedInfo += startFrame * pow(10, 3);
    packedInfo += factor * pow(10, 5);
    packedInfo += mode * pow(10, 8);
    packedInfo += lossless * pow(10, 12);
    return packedInfo;
  }

  MINT32 videoConfig;
  MINT32 queueSize;
  MINT32 startFrame;
  MINT32 factor;
  MINT32 mode;
  MINT32 lossless;
};

};      // namespace EIS
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_EIS_EISINFO_H_
