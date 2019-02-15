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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_P2DATA_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_P2DATA_H_

#include <map>
#include <memory>
#include <mtkcam/utils/std/ILogger.h>
#include <mtkcam/feature/utils/p2/Cropper.h>
#include <mtkcam/feature/utils/p2/P2PlatInfo.h>
#include <mtkcam/pipeline/hwnode/P2Common.h>
#include <mtkcam/def/common.h>
#include <mtkcam/utils/TuningUtils/FileDumpNamingRule.h>
#include <vector>

using NSCam::v3::P2Common::StreamConfigure;

namespace NSCam {
namespace Feature {
namespace P2Util {

#define INVALID_SENSOR_ID (MUINT32)(-1)

enum P2Type {
  P2_UNKNOWN,
  P2_PREVIEW,
  P2_PHOTO,
  P2_VIDEO,
  P2_HS_VIDEO,
  P2_CAPTURE,
  P2_TIMESHARE_CAPTURE,
  P2_DUMMY
};

enum P2DumpType {
  P2_DUMP_NONE = 0,
  P2_DUMP_NDD = 1,
  P2_DUMP_DEBUG = 2,
};

class P2UsageHint {
 public:
  class OutConfig {
   public:
    MUINT32 mMaxOutNum =
        2;  // max out buffer num in 1 pipeline frame for 1 sensor
    MBOOL mHasPhysical = MFALSE;
    MBOOL mHasLarge = MFALSE;
  };
  MSize mStreamingSize;
  MUINT32 m3DNRMode = 0;
  MBOOL mUseTSQ = MFALSE;
  MBOOL mDynamicTuning = MFALSE;
  MBOOL mQParamValid = MTRUE;  // Hal1 & Develop need
  OutConfig mOutCfg;
  // for multi-cam
  MUINT32 mSensorModule = 0;
  std::map<MUINT32, MSize> mResizedRawMap;
};

class P2ConfigInfo {
 public:
  P2ConfigInfo();
  explicit P2ConfigInfo(const ILog& log);

 public:
  static const P2ConfigInfo Dummy;

 public:
  ILog mLog;
  MUINT32 mLogLevel = 0;
  P2Type mP2Type = P2_UNKNOWN;
  P2UsageHint mUsageHint;
  MUINT32 mMainSensorID = -1;
  std::vector<MUINT32> mAllSensorID;
  MUINT32 mBurstNum = 0;
  MUINT32 mCustomOption = 0;
  StreamConfigure mStreamConfigure;
};

class VISIBILITY_PUBLIC P2SensorInfo {
 public:
  P2SensorInfo();
  P2SensorInfo(const ILog& log, const MUINT32& id);

 public:
  static const P2SensorInfo Dummy;

 public:
  ILog mLog;
  MUINT32 mSensorID = INVALID_SENSOR_ID;
  const P2PlatInfo* mPlatInfo = NULL;
  MRect mActiveArray;
};

class P2FrameData {
 public:
  P2FrameData();
  explicit P2FrameData(const ILog& log);

 public:
  static const P2FrameData Dummy;

 public:
  ILog mLog;
  MUINT32 mP2FrameNo = 0;
  MINT32 mMWFrameNo = 0;
  MINT32 mMWFrameRequestNo = 0;
  MUINT32 mAppMode = 0;
  MBOOL mIsRecording = MFALSE;
  MUINT32 mMasterSensorID = INVALID_SENSOR_ID;
};

class VISIBILITY_PUBLIC P2SensorData {
 public:
  P2SensorData();
  explicit P2SensorData(const ILog& log);

 public:
  static const P2SensorData Dummy;

 public:
  ILog mLog;
  MUINT32 mSensorID = INVALID_SENSOR_ID;
  MINT32 mMWUniqueKey = 0;
  MINT32 mMagic3A = 0;
  MUINT8 mIspProfile = 0;
  MINT64 mP1TS = 0;
  MINT32 mISO = 0;

  MINT32 mSensorMode = 0;
  MSize mSensorSize;
  MRect mP1Crop;
  MRect mP1DMA;
  MSize mP1OutSize;
  MRect mP1BinCrop;
  MSize mP1BinSize;

  MBOOL mAppEISOn = MFALSE;
  MRect mAppCrop;

  std::shared_ptr<Cropper> mCropper;
  TuningUtils::FILE_DUMP_NAMING_HINT mNDDHint;
};

}  // namespace P2Util
}  // namespace Feature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_P2_P2DATA_H_
