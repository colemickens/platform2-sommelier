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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_P2COMMON_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_P2COMMON_H_

#include <map>
#include <memory>
#include <vector>

#include <mtkcam/pipeline/stream/IStreamInfo.h>
#include <mtkcam/utils/std/common.h>

namespace NSCam {
namespace v3 {
namespace P2Common {

enum P2NodeType {
  P2_NODE_UNKNOWN,
  P2_NODE_COMMON,
};

enum AppMode {
  APP_MODE_UNKNOWN,
  APP_MODE_PHOTO,
  APP_MODE_VIDEO,
  APP_MODE_HIGH_SPEED_VIDEO,
};

enum eCustomOption { CUSTOM_OPTION_NONE = 0 };

struct UsageHint {
 public:
  class OutConfig {
   public:
    MUINT32 mMaxOutNum =
        2;  // max out buffer num in 1 pipeline frame for 1 sensor
    MBOOL mHasPhysical = MFALSE;
    MBOOL mHasLarge = MFALSE;
  };
  P2NodeType mP2NodeType = P2_NODE_UNKNOWN;
  AppMode mAppMode = APP_MODE_UNKNOWN;
  MSize mStreamingSize;
  MUINT64 mPackedEisInfo = 0;
  MUINT32 m3DNRMode = 0;
  MBOOL mUseTSQ = MFALSE;
  OutConfig mOutCfg;
};

struct StreamConfigure {
  typedef std::vector<std::shared_ptr<IImageStreamInfo> > configure;
  configure mInStreams;
  configure mOutStreams;
};

};  // namespace P2Common
};  // namespace v3
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_P2COMMON_H_
