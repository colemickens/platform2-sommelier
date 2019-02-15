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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_TUNINGHELPER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_TUNINGHELPER_H_

#include <mtkcam/aaa/IHal3A.h>
#include <mtkcam/def/BuiltinTypes.h>
#include <mtkcam/drv/def/IPostProcDef.h>
#include <mtkcam/feature/3dnr/util_3dnr.h>
#include <mtkcam/feature/featurePipe/util/VarMap.h>

#include <featurePipe/common/include/TuningBufferPool.h>
#include <mtkcam/feature/utils/p2/P2IO.h>
#include <mtkcam/feature/utils/p2/P2Pack.h>
#include <mtkcam/feature/utils/p2/P2Util.h>
#include <memory>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

#define MIN_P2A_TUNING_BUF_NUM \
  (MUINT32)(4)  // 3 for P2A driver depth, add 1 buffer

class TuningHelper {
 public:
  enum Scene { Tuning_Normal, Tuning_Pure };

  class ExtraMetaParam {
   public:
    MBOOL isFDCropValid = MFALSE;
    MRect fdCrop;
  };

  class Input {
   public:
    /*Cannot be NULL*/
    const Feature::P2Util::P2Pack& mP2Pack;
    std::shared_ptr<IImageBuffer>& mTuningBuf;
    SFPSensorInput mSensorInput;
    SFPSensorTuning mTargetTuning;
    MUINT32 mSensorID = INVALID_SENSOR_ID;
    std::shared_ptr<NS3Av3::IHal3A> mp3A = nullptr;
    NSCam::v4l2::ENormalStreamTag mTag = NSCam::v4l2::ENormalStreamTag_Normal;
    MINT32 mUniqueKey = -1;
    Feature::P2Util::P2ObjPtr mP2ObjPtr;
    Scene mScene = Tuning_Normal;
    ExtraMetaParam mExtraMetaParam;
    // ---- 3DNR ----
    NR3D::NR3DTuningInfo mpNr3dTuningInfo;  // defined in 3dnr_defs.h

    Input(const Feature::P2Util::P2Pack& p2Pack,
          std::shared_ptr<IImageBuffer>* tuningBuf)
        : mP2Pack(p2Pack), mTuningBuf(*tuningBuf) {}
  };

  /* In Raw2Yuv, need update App/Hal out Meta */
  static MBOOL process3A_P2A_Raw2Yuv(const Input& in,
                                     NSIoPipe::FrameParams* frameParam,
                                     NSCam::IMetadata* halOut,
                                     NSCam::IMetadata* appOut);

 private:
  TuningHelper();
  ~TuningHelper();
};

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_TUNINGHELPER_H_
