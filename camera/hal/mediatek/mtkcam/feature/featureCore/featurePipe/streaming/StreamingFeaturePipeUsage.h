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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_STREAMINGFEATUREPIPEUSAGE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_STREAMINGFEATUREPIPEUSAGE_H_

#include <map>
#include <mtkcam/feature/3dnr/3dnr_defs.h>
#include <mtkcam/feature/featurePipe/IStreamingFeaturePipe.h>
#include <vector>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class StreamingFeaturePipeUsage {
 public:
  StreamingFeaturePipeUsage();
  StreamingFeaturePipeUsage(IStreamingFeaturePipe::UsageHint hint,
                            MUINT32 sensorIndex);

  MBOOL supportP2AP2() const;

  MBOOL supportLargeOut() const;
  MBOOL supportPhysicalOut() const;
  MBOOL supportIMG3O() const;

  MBOOL supportP2ALarge() const;

  MBOOL support4K2K() const;

  MBOOL supportTimeSharing() const;
  MBOOL supportP2AFeature() const;
  MBOOL supportBypassP2A() const;
  MBOOL supportYUVIn() const;
  MBOOL supportPure() const;
  MBOOL support3DNR() const;
  MBOOL support3DNRRSC() const;
  MBOOL is3DNRModeMaskEnable(NR3D::E3DNR_MODE_MASK mask) const;
  MBOOL supportFull_YUY2() const;
  MBOOL supportGraphicBuffer() const;
  EImageFormat getFullImgFormat() const;

  MBOOL isDynamicTuning() const;
  MBOOL isQParamIOValid() const;

  MBOOL supportDummy() const;

  std::vector<MUINT32> getAllSensorIDs() const;
  MUINT32 getMode() const;
  MUINT32 getVendorMode() const;
  MUINT32 get3DNRMode() const;
  MSize getStreamingSize() const;
  MSize getRrzoSizeByIndex(MUINT32 index);
  MUINT32 getNumSensor() const;

  MUINT32 getNumP2ABuffer() const;
  MUINT32 getNumP2APureBuffer() const;
  MUINT32 getNumP2ATuning() const;
  MUINT32 getSensorIndex() const;
  MUINT32 getSensorModule() const;

  MUINT32 supportVendor(MUINT32 ver = 0) const;
  MBOOL supportVendorDebug() const;
  MBOOL supportVendorLog() const;
  MBOOL supportVendorInplace() const;
  MBOOL supportVendorCusSize() const;
  MBOOL supportVendorCusFormat() const;
  MBOOL supportVendorFullImg() const;
  MSize getVendorCusSize(const MSize& original) const;
  EImageFormat getVendorCusFormat(const EImageFormat& original) const;

 private:
  enum P2A_MODE_ENUM {
    P2A_MODE_NORMAL,
    P2A_MODE_TIME_SHARING,
    P2A_MODE_FEATURE,
    P2A_MODE_BYPASS
  };

  class BufferNumInfo {
   public:
    BufferNumInfo() : mBasic(0), mExtra(0) {}

    explicit BufferNumInfo(MUINT32 basic, MUINT32 extra = 0)
        : mBasic(basic), mExtra(extra) {}

   public:
    MUINT32 mBasic;
    MUINT32 mExtra;
  };

  BufferNumInfo get3DNRBufferNum() const;

  IStreamingFeaturePipe::UsageHint mUsageHint;
  MUINT32 mPipeFunc = 0;
  MUINT32 mP2AMode = 0;

  MSize mStreamingSize;
  MUINT32 mVendorMode = 0;
  MBOOL mVendorDebug = MFALSE;
  MBOOL mVendorLog = MFALSE;
  MSize mVendorCusSize;
  MBOOL mEnableVendorCusSize = MFALSE;
  MBOOL mEnableVendorCusFormat = MFALSE;
  MBOOL mEnableDummy = MFALSE;
  MUINT32 m3DNRMode = 0;
  MUINT32 mSensorIndex = INVALID_SENSOR;
  IStreamingFeaturePipe::OutConfig mOutCfg;
  MUINT32 mNumSensor = 0;
  MBOOL mSupportPure = MFALSE;
  std::map<MUINT32, MSize> mResizedRawSizeList;
};

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_STREAMINGFEATUREPIPEUSAGE_H_
