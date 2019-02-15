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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_FEATUREPIPE_SFPIO_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_FEATUREPIPE_SFPIO_H_

#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <mtkcam/drv/iopipe/Port.h>
#include <mtkcam/drv/iopipe/PortMap.h>
#include <mtkcam/drv/def/IPostProcDef.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>

#include <mtkcam/feature/featurePipe/util/VarMap.h>
#include <mtkcam/feature/utils/p2/P2Data.h>
#include <mtkcam/feature/utils/p2/P2IO.h>
#include "base/strings/stringprintf.h"
#include <mtkcam/def/common.h>

namespace NSCam {
class IMetadata;
namespace NSCamFeature {
namespace NSFeaturePipe {

enum PathType {
  PATH_UNKNOWN = 0,
  PATH_GENERAL,
  PATH_PHYSICAL,
  PATH_LARGE,
};

class VISIBILITY_PUBLIC SFPOutput : public Feature::P2Util::P2IO {
 public:
  enum OutTargetType {
    OUT_TARGET_UNKNOWN = 0,
    OUT_TARGET_DISPLAY,
    OUT_TARGET_RECORD,
    OUT_TARGET_FD,
    OUT_TARGET_PHYSICAL,
  };

  OutTargetType mTargetType = OUT_TARGET_UNKNOWN;

  MRectF mCropRect;    // Refer to Master's input
  MSize mCropDstSize;  // Refer to Master's input
  MVOID* mpPqParam = NULL;
  MVOID* mpDpPqParam = NULL;

  MINT32 mDMAConstrainFlag;

  SFPOutput(IImageBuffer* buffer, MUINT32 transform, OutTargetType targetType);
  SFPOutput(const NSCam::NSIoPipe::Output& qOut,
            const NSCam::NSIoPipe::MCrpRsInfo& crop,
            MVOID* pqPtr,
            MVOID* dppqPtr,
            OutTargetType targetType);
  SFPOutput();

  static const char* typeToChar(const OutTargetType& type);
  MVOID appendDumpInfo(std::string* str) const;
  MBOOL isCropValid() const;
  MVOID convertTo(NSCam::NSIoPipe::Output* qOut) const;
  MVOID convertTo(NSCam::NSIoPipe::MCropRect* cropRect) const;
  MVOID convertTo(NSCam::NSIoPipe::MCrpRsInfo* cropInfo) const;
};

class SFPSensorInput {
 public:
  NSCam::IImageBuffer* mIMGO = nullptr;
  NSCam::IImageBuffer* mRRZO = nullptr;
  NSCam::IImageBuffer* mLCSO = nullptr;
  NSCam::IImageBuffer* mPrvRSSO = nullptr;
  NSCam::IImageBuffer* mCurRSSO = nullptr;

  NSCam::IMetadata* mHalIn = NULL;
  NSCam::IMetadata* mAppIn = NULL;
  NSCam::IMetadata* mAppDynamicIn = NULL;
  NSCam::IMetadata* mAppInOverride = NULL;  // for Android Physical Setting

  MVOID appendDumpInfo(std::string* str, MUINT32 sID) const;
};

class VISIBILITY_PUBLIC SFPSensorTuning {
 public:
  enum Flag {
    FLAG_NONE = 0,
    FLAG_RRZO_IN = 1 << 0,
    FLAG_IMGO_IN = 1 << 1,
    FLAG_LCSO_IN = 1 << 2,
    FLAG_FORCE_DISABLE_3DNR = 1 << 3,
  };

  MUINT32 mFlag = FLAG_NONE;

  MBOOL isRRZOin() const;
  MBOOL isIMGOin() const;
  MBOOL isLCSOin() const;
  MBOOL isDisable3DNR() const;
  MVOID addFlag(Flag flag);
  MBOOL isValid() const;
  MVOID appendDumpInfo(std::string* str) const;
};

class VISIBILITY_PUBLIC SFPIOMap {
 private:
  // Input <SensorID -> SFPSensorTuning>
  std::map<MUINT32, SFPSensorTuning> mInputMap;
  SFPSensorTuning mDummy;

 public:
  static const char* pathToChar(const PathType& type);
  // Output
  std::vector<SFPOutput> mOutList;
  NSCam::IMetadata* mHalOut = NULL;
  NSCam::IMetadata* mAppOut = NULL;

  // Description
  PathType mPathType = PATH_UNKNOWN;
  // VarMap mAddiVarMap; // Special addition parameter for this IOMap
  // description
  // Currently same sensor's parameter in different IOMap are all the same

  MVOID addInputTuning(MUINT32 sensorID, const SFPSensorTuning& input);
  MBOOL hasTuning(MUINT32 sensorID) const;
  const SFPSensorTuning& getTuning(MUINT32 sensorID) const;
  MVOID addOutput(const SFPOutput& out);
  MVOID getAllOutput(std::vector<SFPOutput>* outList) const;
  MBOOL isValid() const;
  MBOOL isGenPath() const;
  const char* pathName() const;
  MVOID appendDumpInfo(std::string* str) const;
  MUINT32 getFirstSensorID();
  MVOID getAllSensorIDs(std::vector<MUINT32>* ids) const;
  static MBOOL isSameTuning(const SFPIOMap& map1,
                            const SFPIOMap& map2,
                            MUINT32 sensorID);

 private:
  MUINT32 mFirstID = INVALID_SENSOR_ID;
};

class VISIBILITY_PUBLIC SFPIOManager {
 public:
  MBOOL addInput(MUINT32 sensorID, const SFPSensorInput& input);
  MBOOL addGeneral(const SFPIOMap& sfpio);
  MBOOL addPhysical(MUINT32 sensorID, const SFPIOMap& sfpio);
  MBOOL addLarge(MUINT32 sensorID, const SFPIOMap& sfpio);
  const SFPSensorInput& getInput(MUINT32 sensorID);
  const std::vector<SFPIOMap>& getGeneralIOs() const;
  MUINT32 countAll() const;
  MUINT32 countNonLarge() const;
  MUINT32 countLarge() const;
  MUINT32 countGeneral() const;

#define MAKE_GET_FUNC(name, list, it, itobj)              \
  const SFPIOMap& get##name##IO(MUINT32 sensorID) const { \
    for (auto& it : list) {                               \
      if (itobj.hasTuning(sensorID) > 0) {                \
        return itobj;                                     \
      }                                                   \
    }                                                     \
    return mDummy;                                        \
  }                                                       \
  MBOOL has##name##IO(MUINT32 sensorID) const {           \
    const SFPIOMap& ioMap = get##name##IO(sensorID);      \
    return ioMap.isValid();                               \
  }

  MAKE_GET_FUNC(General, mGenerals, it, it);
  MAKE_GET_FUNC(Physical, mPhysicals, it, it.second);
  MAKE_GET_FUNC(Large, mLarges, it, it.second);
#undef MAKE_GET_FUNC

  const SFPIOMap& getFirstGeneralIO();
  MVOID appendDumpInfo(std::string* str) const;

 private:
  mutable std::vector<SFPIOMap> mGenerals;
  std::unordered_map<MUINT32, SFPIOMap> mPhysicals;
  std::unordered_map<MUINT32, SFPIOMap> mLarges;
  std::unordered_map<MUINT32, SFPSensorInput> mSensorInputs;
  SFPIOMap mDummy;
  SFPSensorInput mDummyInput;
};

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam

#undef MAKE_FEATURE_MASK_FUNC

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_FEATUREPIPE_SFPIO_H_
