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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_FEATUREPROFILEHELPER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_FEATUREPROFILEHELPER_H_

#include <mtkcam/def/common.h>
#include <mtkcam/utils/std/Log.h>
#include <property_lib.h>

namespace NSCam {

struct ProfileParam {
  typedef enum ProfileFlag_T {
    FLAG_NONE = 0,
    FLAG_PURE_RAW_STREAM = 1,  // use pure raw as Pass2 input
    FLAG_RECORDING = (1 << 1),
  } ProfileFlag;

  typedef enum FeatureMask_T {
    FMASK_NONE = 0,
    FMASK_AUTO_HDR_ON = 1,
    FMASK_EIS_ON = (1 << 1),
  } FeatureMask;
  /*
   * In FlowControl, streamSize is RRZO Buffer Size.
   * In Shot, streamSize is sensorSize (IMGO Buffer Size).
   */
  MSize streamSize;
  MUINT32 vhdrMode;    // PipelineSensorParam.vhdrMode, get in
                       // IParamsManager.getVHdr()
  MUINT32 sensorMode;  // PipelineSensorParam.mode
  MINT32 flag;         // mapping to_FLAG_XXX
  MINT32 featureMask;  // mapping to FMASK_XXX
  MUINT8 engProfile;   // in Eng Mode, App will set specific profile

  ProfileParam()
      : streamSize(MSize(0, 0)),
        vhdrMode(0),
        sensorMode(0),
        flag(0),
        featureMask(0),
        engProfile(0) {}

  ProfileParam(MSize _streamSize,
               MUINT32 _vhdrMode,
               MUINT32 _sensorMode,
               MINT32 _flag,
               MINT32 _featureMask)
      : streamSize(_streamSize),
        vhdrMode(_vhdrMode),
        sensorMode(_sensorMode),
        flag(_flag),
        featureMask(_featureMask),
        engProfile(0) {}

  ProfileParam(MSize _streamSize,
               MUINT32 _vhdrMode,
               MUINT32 _sensorMode,
               MINT32 _flag,
               MINT32 _featureMask,
               MUINT8 _engProfile)
      : streamSize(_streamSize),
        vhdrMode(_vhdrMode),
        sensorMode(_sensorMode),
        flag(_flag),
        featureMask(_featureMask),
        engProfile(_engProfile) {}
};

class FeatureProfileHelper {
 private:
  static MBOOL isDebugOpen() {
    static MINT32 debugDump =
        property_get_int32("vendor.debug.featureProfile.dump", 0);
    return (debugDump > 0);
  }

 public:
  /**
   *@brief FeatureProfileHelper constructor
   */
  FeatureProfileHelper() {}

  /**
   *@brief FeatureProfileHelper destructor
   */
  ~FeatureProfileHelper() {}

  /**
   *@brief In flow control (preview, record), you can use this function to get
   *streaming ISP profile
   *@param[out] outputProfile : final streaming used profile
   *@param[in]  param : input parameter.
   */
  static MBOOL getStreamingProf(const MUINT8& outputProfile,
                                const ProfileParam& param);

  /**
   *@brief In Eng flow control (preview, record), you can use this function to
   *get streaming ISP profile
   *@param[out] outputProfile : final streaming used profile
   *@param[in]  param : input parameter.
   */
  static MBOOL getEngStreamingProf(const MUINT8& outputProfile,
                                   const ProfileParam& param);

  /**
   *@brief In VSS, you can use this function to get VSS profile for feature,
   *like vHDR
   *@param[out] outputProfile : final streaming used profile
   *@param[in]  param : input parameter.
   */
  static MBOOL getVSSProf(const MUINT8& outputProfile,
                          const ProfileParam& param);

  /**
   *@brief In Shot( ex zcHDR or ZSD zcHDR), you can use this function to get ISP
   *profile
   *@param[out] outputProfile : final streaming used profile
   *@param[in]  param : input parameter.
   */
  static MBOOL getShotProf(const MUINT8& outputProfile,
                           const ProfileParam& param);

  /**
   *@brief In Eng Shot, you can use this function to get ISP profile (ex :for
   *vHDR EM Mode)
   *@param[out] outputProfile : final streaming used profile
   *@param[in]  param : input parameter.
   */
  static MBOOL getEngShotProf(const MUINT8& outputProfile,
                              const ProfileParam& param);
};

};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_UTILS_FEATUREPROFILEHELPER_H_
