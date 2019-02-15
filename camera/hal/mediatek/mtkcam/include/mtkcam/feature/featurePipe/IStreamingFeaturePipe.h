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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_FEATUREPIPE_ISTREAMINGFEATUREPIPE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_FEATUREPIPE_ISTREAMINGFEATUREPIPE_H_

#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include <mtkcam/drv/iopipe/INormalStream.h>
#include <mtkcam/feature/featurePipe/util/VarMap.h>
#include <mtkcam/feature/3dnr/3dnr_defs.h>
#include "./IStreamingFeaturePipe_var.h"
#include <mtkcam/feature/eis/EisInfo.h>
#include <mtkcam/feature/utils/p2/P2Pack.h>
#include <mtkcam/feature/featurePipe/SFPIO.h>
#include <mtkcam/pipeline/hwnode/P2Common.h>
#include <mtkcam/def/common.h>

#ifdef FEATURE_MASK
#error FEATURE_MASK macro redefine
#endif

#define FEATURE_MASK(name) (1 << OFFSET_##name)

#define MAKE_FEATURE_MASK_FUNC(name, tag)           \
  const MUINT32 MASK_##name = (1 << OFFSET_##name); \
  inline MBOOL HAS_##name(MUINT32 feature) {        \
    return (feature & FEATURE_MASK(name));          \
  }                                                 \
  inline MVOID ENABLE_##name(MUINT32& feature) {    \
    feature |= FEATURE_MASK(name);                  \
  }                                                 \
  inline MVOID DISABLE_##name(MUINT32& feature) {   \
    feature &= ~FEATURE_MASK(name);                 \
  }                                                 \
  inline const char* TAG_##name() { return tag; }

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

const MUINT32 INVALID_SENSOR = (MUINT32)(-1);

enum FEATURE_MASK_OFFSET {
  OFFSET_EIS,
  OFFSET_EIS_25,
  OFFSET_EIS_30,
  OFFSET_VHDR,
  OFFSET_3DNR,
  OFFSET_EIS_QUEUE,
  OFFSET_VENDOR_V1,
  OFFSET_VENDOR_V2,
  OFFSET_FOV,
  OFFSET_N3D,
  OFFSET_3DNR_RSC,
  OFFSET_FSC,
  OFFSET_DUMMY,
};

MAKE_FEATURE_MASK_FUNC(VHDR, "");
MAKE_FEATURE_MASK_FUNC(3DNR, "3DNR");
MAKE_FEATURE_MASK_FUNC(FOV, "FOV");
MAKE_FEATURE_MASK_FUNC(N3D, "N3D");
MAKE_FEATURE_MASK_FUNC(3DNR_RSC, "3DNR_RSC");
MAKE_FEATURE_MASK_FUNC(DUMMY, "DUMMY");

class FeaturePipeParam;

class VISIBILITY_PUBLIC IStreamingFeaturePipe {
 public:
  enum eAppMode {
    APP_PHOTO_PREVIEW = 0,
    APP_VIDEO_PREVIEW = 1,
    APP_VIDEO_RECORD = 2,
    APP_VIDEO_STOP = 3,
  };

  enum eUsageMode {
    USAGE_DEFAULT,
    USAGE_P2A_PASS_THROUGH,
    USAGE_P2A_PASS_THROUGH_TIME_SHARING,
    USAGE_P2A_FEATURE,
    USAGE_STEREO_EIS,
    USAGE_FULL,
    USAGE_DUMMY,
  };

  enum eUsageMask {
    PIPE_USAGE_EIS = 1 << 0,
    PIPE_USAGE_3DNR = 1 << 1,
    PIPE_USAGE_VENDOR = 1 << 2,
    PIPE_USAGE_EARLY_DISPLAY = 1 << 3,
  };
  class OutConfig {
   public:
    MUINT32 mMaxOutNum = 2;  // max out buffer num in 1 frame for 1 sensor
    MBOOL mHasPhysical = MFALSE;
    MBOOL mHasLarge = MFALSE;
  };

  class VISIBILITY_PUBLIC UsageHint {
   public:
    eUsageMode mMode = USAGE_FULL;
    MSize mStreamingSize;
    MSize mVendorCusSize;
    MUINT32 mVendorMode = 0;
    MUINT32 m3DNRMode = 0;
    MBOOL mUseTSQ = MFALSE;
    MBOOL mDynamicTuning = MFALSE;
    std::vector<MUINT32> mAllSensorIDs;
    OutConfig mOutCfg;

    std::map<MUINT32, MSize> mResizedRawMap;
    MUINT32 mSensorModule = 0;
    // --- Hal1 Use ---
    // true only if Hal1
    MBOOL mQParamIOValid = MFALSE;  // If True, using QParam tuning & mvOut to
                                    // generate output buffer

    UsageHint();
    UsageHint(eUsageMode mode, const MSize& streamingSize);
    MVOID enable3DNRModeMask(NSCam::NR3D::E3DNR_MODE_MASK mask) {
      m3DNRMode |= mask;
    }
  };

 public:
  virtual ~IStreamingFeaturePipe() {}

 public:
  // interface for PipelineNode
  static std::shared_ptr<IStreamingFeaturePipe> createInstance(
      MUINT32 openSensorIndex, const UsageHint& usageHint);
  virtual MBOOL init(const char* name = NULL) = 0;
  virtual MBOOL config(const StreamConfigure config) = 0;
  virtual MBOOL uninit(const char* name = NULL) = 0;
  virtual MBOOL enque(const FeaturePipeParam& param) = 0;
  virtual MBOOL flush() = 0;
  virtual MBOOL sendCommand(NSCam::v4l2::ESDCmd cmd,
                            MINTPTR arg1 = 0,
                            MINTPTR arg2 = 0,
                            MINTPTR arg3 = 0) = 0;
  virtual MBOOL addMultiSensorID(MUINT32 sensorID) = 0;

 public:
  // sync will block until all data are processed,
  // use with caution and avoid deadlock !!
  virtual MVOID sync() = 0;

  virtual IImageBuffer* requestBuffer() = 0;
  virtual MBOOL returnBuffer(IImageBuffer* buffer) = 0;

 protected:
  IStreamingFeaturePipe() {}
};

class FeaturePipeParam {
 public:
  enum MSG_TYPE {
    MSG_FRAME_DONE,
    MSG_DISPLAY_DONE,
    MSG_RSSO_DONE,
    MSG_FD_DONE,
    MSG_P2B_SET_3A,
    MSG_N3D_SET_SHOTMODE,
    MSG_INVALID
  };
  typedef MBOOL (*CALLBACK_T)(MSG_TYPE, FeaturePipeParam*);

  VarMap mVarMap;
  MUINT32 mFeatureMask;
  CALLBACK_T mCallback;
  NSCam::NSIoPipe::QParams mQParams;

  std::unordered_map<MUINT32, FeaturePipeParam>
      mSlaveParamMap;  // Only Valid in Master FeaturePipeParam

  SFPIOManager mSFPIOManager;  // Only Valid in Master FeaturePipeParam
  Feature::P2Util::P2Pack
      mP2Pack;  // All Sensor has its own pack, including all sensors' data
  Feature::P2Util::P2DumpType mDumpType = Feature::P2Util::P2_DUMP_NONE;

  FeaturePipeParam() : mFeatureMask(0), mCallback(NULL) {}

  explicit FeaturePipeParam(CALLBACK_T callback)
      : mFeatureMask(0), mCallback(callback) {}

  FeaturePipeParam(CALLBACK_T callback, const Feature::P2Util::P2Pack& p2Pack)
      : mFeatureMask(0), mCallback(callback), mP2Pack(p2Pack) {}

  MVOID setFeatureMask(MUINT32 mask, MBOOL enable) {
    if (enable) {
      mFeatureMask |= mask;
    } else {
      mFeatureMask &= (~mask);
    }
  }

  MVOID setQParams(const NSCam::NSIoPipe::QParams& qparams) {
    mQParams = qparams;
  }

  NSCam::NSIoPipe::QParams getQParams() const { return mQParams; }

  MVOID addSlaveParam(MUINT32 sensorID, const FeaturePipeParam& param) {
    mSlaveParamMap[sensorID] = param;
    if (mFirstSlaveID == INVALID_SENSOR) {
      mFirstSlaveID = sensorID;
    }
  }

  MBOOL getFirstSlaveParam(FeaturePipeParam* out) const {
    if (existSlaveParam()) {
      *out = mSlaveParamMap.at(mFirstSlaveID);
      return MTRUE;
    }
    return MFALSE;
  }

  MBOOL existSlaveParam() const { return (mFirstSlaveID != INVALID_SENSOR); }

  MBOOL addSFPIOMap(SFPIOMap* ioMap) {
    if (ioMap->mPathType == PATH_GENERAL) {
      return mSFPIOManager.addGeneral(*ioMap);
    } else if (ioMap->mPathType == PATH_PHYSICAL) {
      return mSFPIOManager.addPhysical(ioMap->getFirstSensorID(), *ioMap);
    } else if (ioMap->mPathType == PATH_LARGE) {
      return mSFPIOManager.addLarge(ioMap->getFirstSensorID(), *ioMap);
    } else {
      return MFALSE;
    }
  }

  DECLARE_VAR_MAP_INTERFACE(mVarMap, setVar, getVar, tryGetVar, clearVar);

 private:
  MUINT32 mFirstSlaveID = INVALID_SENSOR;
};

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam

#undef MAKE_FEATURE_MASK_FUNC

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_FEATUREPIPE_ISTREAMINGFEATUREPIPE_H_
