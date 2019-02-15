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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_STREAMINGPROCESSOR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_STREAMINGPROCESSOR_H_

#include <mtkcam/feature/3dnr/util_3dnr.h>
#include <mtkcam/feature/featurePipe/IStreamingFeaturePipe.h>
#include "P2_Processor.h"
#include "P2_Util.h"
#include <mtkcam/utils/hw/IFDContainer.h>

#include <list>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

using NSCam::IFDContainer;
using NSCam::NSCamFeature::NSFeaturePipe::FeaturePipeParam;
using NSCam::NSCamFeature::NSFeaturePipe::IStreamingFeaturePipe;

namespace P2 {

class StreamingProcessor
    : virtual public Processor<P2InitParam,
                               P2ConfigParam,
                               vector<std::shared_ptr<P2Request>>> {
  enum class ERequestPath { eGeneral, ePhysic, eLarge };

  class Payload;
  class PartialPayload;
  class P2RequestPack;

 public:
  StreamingProcessor();
  virtual ~StreamingProcessor();
  std::shared_ptr<Payload> mpayload;

 protected:
  virtual MBOOL onInit(const P2InitParam& param);
  virtual MVOID onUninit();
  virtual MVOID onThreadStart();
  virtual MVOID onThreadStop();
  virtual MBOOL onConfig(const P2ConfigParam& param);
  virtual MBOOL onEnque(const vector<std::shared_ptr<P2Request>>& requests);
  virtual MVOID onNotifyFlush();
  virtual MVOID onWaitFlush();
  virtual MVOID onIdle();

 private:
  IStreamingFeaturePipe::UsageHint getFeatureUsageHint(
      const P2ConfigInfo& config);
  MBOOL needReConfig(const P2ConfigInfo& oldConfig,
                     const P2ConfigInfo& newConfig);
  MBOOL initFeaturePipe(const P2ConfigInfo& config);
  MVOID uninitFeaturePipe();
  MBOOL init3A();
  MVOID uninit3A();
  MVOID waitFeaturePipeDone();

  MVOID incPayloadCount(const ILog& log);
  MVOID decPayloadCount(const ILog& log);

  MVOID incPayload(const std::shared_ptr<Payload>& payload);
  MBOOL decPayload(FeaturePipeParam* param,
                   const std::shared_ptr<Payload>& payload,
                   MBOOL checkOrder);

  MBOOL prepareISPTuning(P2Util::SimpleIn* input, const ILog& log) const;
  MBOOL processP2(const std::shared_ptr<Payload>& payload);
  MBOOL checkFeaturePipeParamValid(const std::shared_ptr<Payload>& payload);
  MVOID onFPipeCB(FeaturePipeParam::MSG_TYPE msg,
                  FeaturePipeParam const& param,
                  const std::shared_ptr<Payload>& payload);
  static MBOOL sFPipeCB(FeaturePipeParam::MSG_TYPE msg,
                        FeaturePipeParam* param);
  MBOOL makeRequestPacks(
      const vector<std::shared_ptr<P2Request>>& requests,
      vector<std::shared_ptr<P2RequestPack>>* rReqPacks) const;
  std::shared_ptr<Payload> makePayLoad(
      const vector<std::shared_ptr<P2Request>>& requests,
      const vector<std::shared_ptr<P2RequestPack>>& reqPacks);
  MBOOL prepareInputs(const std::shared_ptr<Payload>& payload) const;
  MBOOL prepareOutputs(const std::shared_ptr<Payload>& payload) const;
  MVOID releaseResource(const vector<std::shared_ptr<P2Request>>& requests,
                        MUINT32 res) const;
  MVOID makeSFPIOOuts(const std::shared_ptr<Payload>& payload,
                      const ERequestPath& path,
                      FeaturePipeParam* featureParam) const;
  MBOOL makeSFPIOMgr(const std::shared_ptr<Payload>& payload) const;

 private:
  // Features
  MVOID prepareFeatureParam(P2Util::SimpleIn* input, const ILog& log) const;
  MBOOL prepareCommon(P2Util::SimpleIn* input, const ILog& log) const;
  MBOOL getEISExpTime(MINT32* expTime,
                      MINT32* longExpTime,
                      const LMVInfo& lmvInfo,
                      const std::shared_ptr<P2Meta>& inHal) const;
  MVOID setLMVOParam(FeaturePipeParam* featureParam,
                     const std::shared_ptr<P2Request>& request) const;
  MVOID init3DNR();
  MVOID uninit3DNR();
  MINT32 get3DNRIsoThreshold(MUINT32 sensorID, MUINT8 ispProfile) const;
  MBOOL prepare3DNR_FeatureData(MBOOL en3DNRFlow,
                                MBOOL isEIS4K,
                                MBOOL isIMGO,
                                P2Util::SimpleIn* input,
                                P2MetaSet const& metaSet,
                                MUINT8 ispProfile,
                                const ILog& log) const;
  MBOOL getInputCrop3DNR(MBOOL* isEIS4K,
                         MBOOL* isIMGO,
                         MRect* inputCrop,
                         P2Util::SimpleIn const& input,
                         const ILog& log) const;
  MBOOL is3DNRFlowEnabled(MINT32 force3DNR,
                          IMetadata* appInMeta,
                          IMetadata* halInMeta,
                          const ILog& log) const;
  MBOOL prepare3DNR(P2Util::SimpleIn* input, const ILog& log) const;

 private:
  // P2Request that can be process together will be merged together
  class P2RequestPack {
   public:
    P2RequestPack() = delete;
    P2RequestPack(const ILog& log,
                  const std::shared_ptr<P2Request>& pReq,
                  const vector<MUINT32>& sensorIDs);
    virtual ~P2RequestPack();

    MVOID addOutput(const std::shared_ptr<P2Request>& pReq,
                    const MUINT32 outputIndex);
    MVOID updateBufferResult(MBOOL result);
    MVOID updateMetaResult(MBOOL result);
    MVOID dropRecord();
    MVOID earlyRelease(MUINT32 mask);
    MBOOL contains(const std::shared_ptr<P2Request>& pReq) const;
    P2Util::SimpleIn* getInput(MUINT32 sensorID);

   public:
    ILog mLog;
    std::shared_ptr<P2Request> mMainRequest;
    std::set<std::shared_ptr<P2Request>> mRequests;
    std::unordered_map<MUINT32, MUINT32> mSensorInputMap;
    std::vector<P2Util::SimpleIn> mInputs;
    std::unordered_map<P2Request*, vector<P2Util::SimpleOut>> mOutputs;
  };

  class PartialPayload {
   public:
    PartialPayload(const ILog& mainLog,
                   const std::shared_ptr<P2RequestPack>& pReqPack);
    virtual ~PartialPayload();
    MVOID print() const;

   public:
    std::shared_ptr<P2RequestPack> mRequestPack;
    ILog mLog;
  };

  class Payload {
   public:
    Payload(std::shared_ptr<StreamingProcessor> parent,
            const ILog& mainLog,
            MUINT32 masterSensorId);
    MVOID addRequests(const vector<std::shared_ptr<P2Request>>& requests);
    MVOID addRequestPacks(
        const vector<std::shared_ptr<P2RequestPack>>& reqPacks);
    MBOOL prepareFdData(const P2Info& p2Info, IFDContainer* pFDContainer);

    MVOID print() const;
    std::shared_ptr<P2Request> getMainRequest();
    std::shared_ptr<P2Request> getPathRequest(ERequestPath path,
                                              const MUINT32& sensorID);
    std::shared_ptr<P2RequestPack> getRequestPack(
        const std::shared_ptr<P2Request>& pReq);

    // After all the operations of streaming processor,
    // chose one main FPP and create SFPIOMgr which will be used as SFP input.
    FeaturePipeParam* getMainFeaturePipeParam();

   public:
    virtual ~Payload();

   public:
    std::shared_ptr<StreamingProcessor> mParent;
    const ILog mLog;
    const MUINT32 mMasterID = 0;

    // 1 PartialPayload for 1 P2RequestPack
    vector<std::shared_ptr<PartialPayload>> mPartialPayloads;

    // path to <sensorID, P2Request> mapping, for SFPIO
    std::unordered_map<ERequestPath,
                       std::unordered_map<MUINT32, std::shared_ptr<P2Request>>>
        mReqPaths;

    FD_DATATYPE* mpFdData = nullptr;  // for PQParam
  };

 private:
  ILog mLog;

  P2Info mP2Info;

  std::shared_ptr<IStreamingFeaturePipe> mFeaturePipe = nullptr;
  IStreamingFeaturePipe::UsageHint mPipeUsageHint;

  std::unordered_map<MUINT32, std::shared_ptr<IHal3A>>
      mHal3AMap;  // sensorID -> IHal3A
 private:
  std::atomic<MUINT32> mPayloadCount = {0};
  std::mutex mPayloadMutex;
  std::condition_variable mPayloadCondition;
  std::list<std::shared_ptr<Payload>> mPayloadList;

  MINT32 m3dnrDebugLevel = 0;
  std::unordered_map<MUINT32, std::shared_ptr<Util3dnr>>
      mUtil3dnrMap;  // sensorID -> Util3dnr

  std::mutex mRssoHolderMutex;
  std::unordered_map<MUINT32, std::shared_ptr<P2Img>>
      mRssoHolder;  // sensorID -> Rsso
  MUINT32 mDebugDrawCropMask;
};

}  // namespace P2

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_STREAMINGPROCESSOR_H_
