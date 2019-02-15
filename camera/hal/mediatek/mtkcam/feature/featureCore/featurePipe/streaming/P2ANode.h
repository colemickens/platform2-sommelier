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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_P2ANODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_P2ANODE_H_

#include <mutex>
#include "StreamingFeatureNode.h"
#include "NormalStreamBase.h"
#include "P2CamContext.h"
#include "TuningHelper.h"

#include <featurePipe/common/include/CamThreadNode.h>
#include <memory>
#include <mtkcam/aaa/IHal3A.h>
#include <mtkcam/feature/utils/p2/P2IO.h>
#include <mtkcam/feature/utils/p2/P2Util.h>
#include <unordered_map>
#include <vector>

using NS3Av3::AE_Pline_Limitation_T;
using NS3Av3::E3ACtrl_SetAEPlineLimitation;
using NS3Av3::IHal3A;

class EisHal;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class P2ASrzRecord {
 public:
  _SRZ_SIZE_INFO_ srz4;
};

class P2AEnqueData {
 public:
  RequestPtr mRequest;
  BasicImg mFullImg;
  ImgBuffer mPrevFullImg;  // VIPI use
  BasicImg mNextFullImg;
  BasicImg mSlaveFullImg;
  BasicImg mSlaveNextFullImg;
  FMResult mFMResult;
  ImgBuffer mFE1Img;
  ImgBuffer mFE2Img;
  ImgBuffer mFE3Img;
  ImgBuffer mPureImg;
  ImgBuffer mSlavePureImg;

  FovP2AResult mFovP2AResult;
  std::vector<SFPOutput>
      mRemainingOutputs;  // Master may Need additional MDP to generate output
  /* If feature pipe run dynamic tuning, P2ANode need prepare tuning data by
   * itself */
  std::vector<std::shared_ptr<IImageBuffer>> tuningBufs;
  std::vector<std::shared_ptr<P2ASrzRecord>> tuningSrzs;
};

class P2ATuningIndex {
 public:
  MINT32 mGenMaster = -1;
  MINT32 mGenSlave = -1;
  MINT32 mPhyMaster = -1;
  MINT32 mPhySlave = -1;
  MINT32 mLargeMaster = -1;
  MINT32 mLargeSlave = -1;
  MINT32 mPureMaster = -1;
  MINT32 mPureSlave = -1;

  MBOOL isGenMasterValid() const { return mGenMaster >= 0; }
  MBOOL isGenSlaveValid() const { return mGenSlave >= 0; }
  MBOOL isPhyMasterValid() const { return mPhyMaster >= 0; }
  MBOOL isPhySlaveValid() const { return mPhySlave >= 0; }
  MBOOL isLargeMasterValid() const { return mLargeMaster >= 0; }
  MBOOL isLargeSlaveValid() const { return mLargeSlave >= 0; }
  MBOOL isPureMasterValid() const { return mPureMaster >= 0; }
  MBOOL isPureSlaveValid() const { return mPureSlave >= 0; }

  MBOOL isMasterMainValid() const {
    return isGenMasterValid() || isPureMasterValid() || isPhyMasterValid();
  }
  MUINT32 getMasterMainIndex() const {
    return isGenMasterValid() ? mGenMaster
                              : isPureMasterValid() ? mPureMaster : mPhyMaster;
  }
  MBOOL isSlaveMainValid() const {
    return isGenSlaveValid() || isPureSlaveValid() || isPhySlaveValid();
  }
  MUINT32 getSlaveMainIndex() const {
    return isGenSlaveValid() ? mGenSlave
                             : isPureSlaveValid() ? mPureSlave : mPhySlave;
  }
};

class P2ANode : public StreamingFeatureNode,
                public NormalStreamBase<P2AEnqueData> {
 public:
  explicit P2ANode(const char* name);
  virtual ~P2ANode();

  MVOID setNormalStream(std::shared_ptr<NSCam::v4l2::INormalStream> stream,
                        MUINT32 version);
  MBOOL configNormalStream(const StreamConfigure config);

  MVOID setFullImgPool(const std::shared_ptr<IBufferPool>& pool,
                       MUINT32 allocate = 0);
  MVOID setPureImgPool(const PoolMap& poolMap);

 public:
  virtual MBOOL onData(DataID id, const RequestPtr& data);
  virtual IOPolicyType getIOPolicy(StreamType stream,
                                   const StreamingReqInfo& reqInfo) const;

 protected:
  virtual MBOOL onInit();
  virtual MBOOL onUninit();
  virtual MBOOL onThreadStart();
  virtual MBOOL onThreadStop();
  virtual MBOOL onThreadLoop();

 protected:
  virtual MVOID onNormalStreamBaseCB(QParams* pParams,
                                     const P2AEnqueData& request);

 private:
  MVOID handleResultData(const RequestPtr& request, const P2AEnqueData& data);
  MBOOL initP2();
  MVOID uninitP2();
  MBOOL processP2A(const RequestPtr& request);
  MBOOL prepareQParams(QParams* params,
                       const RequestPtr& request,
                       P2ATuningIndex* tuningIndex);
  MBOOL prepareStreamTag(QParams* params, const RequestPtr& request);
  MBOOL prepareFullImgFromInput(const RequestPtr& request, P2AEnqueData* data);
  MBOOL prepareNonMdpIO(QParams* params,
                        const RequestPtr& request,
                        P2AEnqueData* data,
                        const P2ATuningIndex& tuningIndex);
  MBOOL prepareMasterMdpOuts(QParams* params,
                             const RequestPtr& request,
                             P2AEnqueData* data,
                             const P2ATuningIndex& tuningIndex);
  MBOOL prepareSlaveOuts(QParams* params,
                         const RequestPtr& request,
                         P2AEnqueData* data,
                         const P2ATuningIndex& tuningIndex);
  MBOOL prepareLargeMdpOuts(QParams* params,
                            const RequestPtr& request,
                            MINT32 frameIndex,
                            MUINT32 sensorID);
  MVOID prepareVIPI(FrameParams* frame,
                    const RequestPtr& request,
                    P2AEnqueData* data);
  MVOID prepareFDImg(FrameParams* frame,
                     const RequestPtr& request,
                     P2AEnqueData* data);
  MVOID prepareFDCrop(FrameParams* frame,
                      const RequestPtr& request,
                      P2AEnqueData* data);
  MVOID prepareFullImg(FrameParams* frame,
                       const RequestPtr& request,
                       BasicImg* outImg,
                       const FrameInInfo& inInfo,
                       MUINT32 sensorID);
  MVOID preparePureImg(FrameParams* frame,
                       const RequestPtr& request,
                       ImgBuffer* outImg,
                       MUINT32 sensorID);
  MBOOL prepareExtraMdpCrop(const BasicImg& fullImg,
                            std::vector<SFPOutput>* leftOutList);
  MVOID enqueFeatureStream(QParams* pParams, P2AEnqueData* data);
  MBOOL needFullForExtraOut(std::vector<SFPOutput>* outList);

  MBOOL needPureYuv(MUINT32 sensorID, const RequestPtr& request);
  MBOOL needNormalYuv(MUINT32 sensorID, const RequestPtr& request);

 private:
  MBOOL init3A();
  MVOID uninit3A();
  MBOOL prepare3A(QParams* params, const RequestPtr& request);
  // Tuning
  MINT32 addTuningFrameParam(
      MUINT32 sensorID,
      const SFPIOMap& ioMap,
      QParams* params,
      const RequestPtr& request,
      P2AEnqueData* data,
      TuningHelper::Scene scene = TuningHelper::Tuning_Normal);
  MBOOL prepareRawTuning(QParams* params,
                         const RequestPtr& request,
                         P2AEnqueData* data,
                         P2ATuningIndex* tuningIndex);
  MBOOL prepareOneRawTuning(NSCam::NSIoPipe::QParams* params,
                            const RequestPtr& request,
                            const SFPIOMap& ioMap,
                            std::shared_ptr<IImageBuffer>* tuningBuf,
                            MUINT32 sensorID,
                            Feature::P2Util::P2ObjPtr* p2ObjPtr,
                            MBOOL needMetaOut,
                            TuningHelper::Scene scene);

 private:
  struct eis_region {
    MUINT32 x_int;
    MUINT32 x_float;
    MUINT32 y_int;
    MUINT32 y_float;
    MSize s;
    MINT32 gmvX;
    MINT32 gmvY;
    MINT32 confX;
    MINT32 confY;
  };
  MBOOL prepare3DNR(
      QParams* params,
      const RequestPtr& request,
      /*const RSCData &rscData, */ const P2ATuningIndex& tuningIndex);
  MVOID dump_Qparam(const QParams& rParams, const char* pSep);
  MVOID dump_vOutImageBuffer(const QParams& params);
  MVOID dump_imgiImageBuffer(const QParams& params);

  MBOOL do3dnrFlow(NSCam::NSIoPipe::QParams* enqueParams,
                   const RequestPtr& request,
                   const MRect& dst_resizer_rect,
                   const MSize& resize_size,
                   const eis_region& eisInfo,
                   MINT32 iso,
                   MINT32 isoThreshold,
                   MUINT32 requestNo,
                   const P2ATuningIndex& tuningIndex);
  MINT32 m3dnrLogLevel;
  MBOOL bDump3DNR;
  std::shared_ptr<IHal3A> mp3A;

 private:
  WaitQueue<RequestPtr> mRequests;

  std::shared_ptr<NSCam::v4l2::INormalStream> mNormalStream;
  MUINT32 mDipVersion = 0;
  std::mutex mEnqueMutex;

  MUINT32 mFullImgPoolAllocateNeed;

  std::unordered_map<MUINT32, std::shared_ptr<IBufferPool>> mPureImgPoolMap;
  std::shared_ptr<IBufferPool> mFullImgPool;
  std::vector<std::shared_ptr<IImageBuffer>> mTuningBuffers;
  std::vector<std::shared_ptr<IImageBuffer>> mTuningBuffers_All;

  std::mutex mTuningLock;

  MUINT32 mCropMode;
  MBOOL mLastDualParamValid;

  typedef enum { CROP_MODE_NONE = 0, CROP_MODE_USE_CRZ } CROP_MODE_ENUM;
};

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_P2ANODE_H_
