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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_NODES_P2ANODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_NODES_P2ANODE_H_

#include "../CaptureFeatureNode.h"

#include <util/P2Operator.h>
#include <common/include/CamThreadNode.h>

#include <mtkcam/aaa/IHal3A.h>
#include <thread>
#include <future>
#include <map>
#include <memory>
#include <vector>

using NSCam::NSIoPipe::EDIPInfoEnum;
using NSCam::NSIoPipe::ModuleInfo;
using NSCam::NSIoPipe::PQParam;

using NS3Av3::IHal3A;
using NS3Av3::MetaSet_T;
using NS3Av3::TuningParam;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

class CaptureTaskQueue;

class P2ANode : public CaptureFeatureNode {
 public:
  P2ANode(NodeID_T nid, const char* name);
  virtual ~P2ANode();

  MVOID setBufferPool(const std::shared_ptr<CaptureBufferPool>& pool);

  MBOOL configNormalStream(const StreamConfigure config);

 public:
  virtual MBOOL onData(DataID id, const RequestPtr& data);

 protected:
  virtual MBOOL onInit();
  virtual MBOOL onUninit();
  virtual MBOOL onThreadStart();
  virtual MBOOL onThreadStop();
  virtual MBOOL onThreadLoop();
  virtual MERROR evaluate(CaptureFeatureInferenceData* rInference);

 public:
  // P2 callbacks
  static MVOID onP2SuccessCallback(QParams* pParams);
  static MVOID onP2FailedCallback(QParams* pParams);

  struct RequestHolder {
    std::vector<SmartImageBuffer> mpBuffers;
    ~RequestHolder() {}
  };

  struct P2Input {
    IImageBuffer* mpBuf = nullptr;
    BufferID_T mBufId = NULL_BUFFER;
    MBOOL mPureRaw = MFALSE;
  };

  struct P2Output {
    IImageBuffer* mpBuf = nullptr;
    BufferID_T mBufId = NULL_BUFFER;
    MBOOL mHasCrop = MFALSE;
    MRect mCropRegion = MRect(0, 0);
    MBOOL mClearZoom = MFALSE;
    MUINT32 mTrans = 0;
  };

  struct MDPOutput : P2Output {
    MDPOutput() : P2Output() {}

    IImageBuffer* mpSource = nullptr;
    MRect mSourceCrop = MRect(0, 0);
    MUINT32 mSourceTrans = 0;
  };

  struct P2EnqueData {
    P2Input mIMGI = P2Input();
    P2Input mLCEI = P2Input();
    P2Output mIMG2O = P2Output();
    P2Output mWDMAO = P2Output();
    P2Output mWROTO = P2Output();
    P2Output mIMG3O = P2Output();
    // Using MDP Copy
    MDPOutput mCopy1 = MDPOutput();
    MDPOutput mCopy2 = MDPOutput();

    IMetadata* mpIMetaApp = NULL;
    IMetadata* mpIMetaHal = NULL;
    IMetadata* mpIMetaDynamic = NULL;
    IMetadata* mpOMetaHal = NULL;
    IMetadata* mpOMetaApp = NULL;

    MINT32 mSensorId = 0;
    MBOOL mResized = MFALSE;
    MBOOL mYuvRep = MFALSE;
    MBOOL mScaleUp = MFALSE;
    MSize mScaleUpSize = MSize(0, 0);
    MBOOL mEnableMFB = MFALSE;
    MBOOL mEnableDRE = MFALSE;
    MBOOL mDebugDump = MFALSE;
    MINT32 mUniqueKey = 0;
    MINT32 mRequestNo = 0;
    MINT32 mFrameNo = 0;
    MINT32 mTaskId = 0;
    std::shared_ptr<RequestHolder> mpHolder;
  };

  MBOOL enqueISP(const RequestPtr& request,
                 const std::shared_ptr<P2EnqueData>& pEnqueData);

  // image processes
  MBOOL onRequestProcess(RequestPtr&);

  // routines
  MVOID onRequestFinish(RequestPtr&);

 private:
  inline MBOOL hasSubSensor() { return MFALSE; }

  WaitQueue<RequestPtr> mRequests;

  std::shared_ptr<IHal3A> mp3AHal;
  std::shared_ptr<IHal3A> mp3AHal2;
  std::shared_ptr<P2Operator> mpP2Opt;
  std::shared_ptr<P2Operator> mpP2ReqOpt;
  std::shared_ptr<P2Operator> mpP2Opt2;

  std::shared_ptr<CaptureBufferPool> mpBufferPool;
  std::map<EDIPInfoEnum, MUINT32> mDipInfo;
#if MTK_DP_ENABLE
  MUINT32 mDipVer;
#endif
  MBOOL mISP3_0;
  MINT32 mDebugDS;
  MINT32 mDebugDSRatio;
  MBOOL mDebugDump;
  MBOOL mDebugImg3o;
  MBOOL mForceImg3o;
  MBOOL mForceImg3o422;
  std::unique_ptr<CaptureTaskQueue> mTaskQueue;

 private:
  struct EnquePackage : public Timer {
   public:
    std::shared_ptr<P2EnqueData> mpEnqueData;
    PQParam* mpPQParam;
    ModuleInfo* mpModuleInfo;
    std::shared_ptr<IImageBuffer> mTuningData;
    P2ANode* mpNode;

    EnquePackage()
        : mpPQParam(NULL),
          mpModuleInfo(NULL),
          mTuningData(nullptr),
          mpNode(NULL) {}

    ~EnquePackage();
  };

 public:
  static MBOOL copyBuffers(EnquePackage*);
  static MVOID calculateSourceCrop(MRect* rSrcCrop,
                                   std::shared_ptr<IImageBuffer> src,
                                   std::shared_ptr<IImageBuffer> dst,
                                   MINT32 dstTrans = 0);
};

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_NODES_P2ANODE_H_
