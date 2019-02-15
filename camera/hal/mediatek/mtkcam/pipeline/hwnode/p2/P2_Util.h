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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_UTIL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_UTIL_H_

#include <mtkcam/feature/featurePipe/IStreamingFeaturePipe.h>

#include "P2_Request.h"

#include <mtkcam/utils/hw/IFDContainer.h>

#include <memory>

namespace P2 {

class P2Util {
 public:
  enum FindOutMask {
    FIND_NO_ROTATE = 0x01,
    FIND_ROTATE = 0x02,
    FIND_DISP = 0x04,
    FIND_VIDEO = 0x08,
  };
  enum ReleaseMask {
    RELEASE_ALL = 0x01,
    RELEASE_DISP = 0x02,
    RELEASE_FD = 0x04,
    RELEASE_RSSO = 0x08,
  };
  enum P2PortFlag {
    USE_VENC = 0x01,
  };

  class SimpleIO {
   public:
    SimpleIO();
    MVOID setUseLMV(MBOOL useLMV);
    MBOOL hasInput() const;
    MBOOL hasOutput() const;
    MBOOL isResized() const;
    MSize getInputSize() const;
    MVOID updateResult(MBOOL result) const;
    MVOID dropRecord() const;
    MVOID earlyRelease(MUINT32 mask, MBOOL result);
    std::shared_ptr<P2Img> getMDPSrc() const;
    std::shared_ptr<P2Img> getLcso() const;
    P2IOPack toP2IOPack() const;
    MVOID printIO(const ILog& log) const;

   public:
    std::shared_ptr<IImageBuffer> mTuningBuffer = nullptr;

   private:
    MBOOL mResized;
    MBOOL mUseLMV;
    std::shared_ptr<P2Img> mIMGI;
    std::shared_ptr<P2Img> mLCEI;
    std::shared_ptr<P2Img> mIMG2O;
    std::shared_ptr<P2Img> mIMG3O;
    std::shared_ptr<P2Img> mWROTO;
    std::shared_ptr<P2Img> mWDMAO;
    friend class P2Util;
  };

  class SimpleIn {
   public:
    SimpleIn(MUINT32 sensorId, std::shared_ptr<P2Request> request);
    MUINT32 getSensorId() const;
    MVOID setUseLMV(MBOOL useLMV);
    MVOID setISResized(MBOOL isResized);
    MBOOL isResized() const;
    MBOOL useLMV() const;
    MBOOL useCropRatio() const;
    MSize getInputSize() const;
    std::shared_ptr<P2Img> getLcso() const;
    MVOID addCropRatio(const char* name, const float cropRatio);
    MBOOL hasCropRatio() const;
    float getCropRatio() const;
    MVOID releaseAllImg();

   public:
    std::shared_ptr<P2Img> mIMGI = nullptr;
    std::shared_ptr<P2Img> mLCEI = nullptr;
    std::shared_ptr<P2Img> mRSSO = nullptr;
    std::shared_ptr<P2Img> mPreRSSO = nullptr;
    std::shared_ptr<P2Request> mRequest = nullptr;  // for metadata
    TuningParam mTuning;
    std::shared_ptr<IImageBuffer> mTuningBuffer = nullptr;
    NSCam::NSCamFeature::NSFeaturePipe::FeaturePipeParam mFeatureParam;

   private:
    MUINT32 mSensorId = 0;
    MBOOL mResized = MFALSE;
    MBOOL mUseLMV = MFALSE;
    MBOOL mUseCropRatio = MFALSE;
    float mCropRatio = 1.0f;
    friend class P2Util;
  };

  class SimpleOut {
   public:
    SimpleOut(MUINT32 sensorId,
              std::shared_ptr<P2Request> request,
              std::shared_ptr<P2Img> const& pImg);
    MUINT32 getSensorId() const;
    MVOID setIsFD(MBOOL isFDBuffer);
    MBOOL isDisplay() const;
    MBOOL isRecord() const;
    MBOOL isFD() const;
    MBOOL isMDPOutput() const;

   public:
    std::shared_ptr<P2Img> mImg;
    std::shared_ptr<P2Request> mRequest;  // for metadata
    MRectF mCrop = MRectF(0, 0);
    MINT32 mDMAConstrainFlag;
    P2Obj mP2Obj;  // for isp tuning
   private:
    MUINT32 mSensorId = 0;
    MBOOL mFD = MFALSE;
    friend class P2Util;
  };

 public:
  static SimpleIO extractSimpleIO(const std::shared_ptr<P2Request>& request,
                                  MUINT32 portFlag = 0);
  static MVOID releaseTuning(TuningParam* tuning);

  static std::shared_ptr<P2Img> extractOut(
      const std::shared_ptr<P2Request>& request, MUINT32 target = 0);

 public:
  static TuningParam xmakeTuning(const P2Pack& p2Pack,
                                 const SimpleIO& io,
                                 std::shared_ptr<IHal3A> hal3A,
                                 P2MetaSet* metaSet);
  static QParams xmakeQParams(const P2Pack& p2Pack,
                              const SimpleIO& io,
                              const TuningParam& tuning,
                              const P2ObjPtr& p2ObjPtr);

  static TuningParam xmakeTuning(const P2Pack& p2Pack,
                                 const SimpleIn& in,
                                 std::shared_ptr<IHal3A> hal3A,
                                 P2MetaSet* metaSet);
  static MVOID xmakeDpPqParam(const P2Pack& p2Pack,
                              const SimpleOut& out,
                              FD_DATATYPE* const& pFdData);
};

}  // namespace P2

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_UTIL_H_
