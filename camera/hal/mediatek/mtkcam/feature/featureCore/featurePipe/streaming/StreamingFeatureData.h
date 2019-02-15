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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_STREAMINGFEATUREDATA_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_STREAMINGFEATUREDATA_H_

#include <unordered_map>
#include "MtkHeader.h"
#include <vector>

#include <featurePipe/common/include/WaitQueue.h>
#include <featurePipe/common/include/IIBuffer.h>
#include <featurePipe/common/include/IOUtil.h>
#include <featurePipe/common/include/TuningBufferPool.h>

#include "StreamingFeatureTimer.h"
#include "StreamingFeaturePipeUsage.h"
#include <map>
#include <memory>
#include <string>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

enum Domain { RRZO_DOMAIN, WARP_DOMAIN };

class StreamingFeatureNode;
class StreamingReqInfo;
class SrcCropInfo {  // This class will do copy from user, please keep size as
                     // small as possible
 public:
  MRect mSrcCrop = MRect(0, 0);
  MBOOL mIMGOin = MFALSE;
  MBOOL mIsSrcCrop = MFALSE;
  MSize mRRZOSize = MSize(0, 0);
  MSize mIMGOSize = MSize(0, 0);
};

class HelperRWData {
 public:
  // Should match to FeaturePipeParam MSG
  // Now only display done & frame done need wait P2AMDP done.
  enum Msg {
    Msg_PMDP_Done = 1,
    Msg_Display_Done = 1 << 1,
    Msg_Frame_Done = 1 << 2
  };

  HelperRWData() {}
  ~HelperRWData() {}
  MBOOL isMsgReceived(Msg msg) { return mMsgBits & msg; }
  MVOID markMsgReceived(Msg msg) { mMsgBits |= msg; }

 private:
  MUINT32 mMsgBits = 0;
};

class StreamingFeatureRequest {
 private:
  // must allocate extParam before everything else
  FeaturePipeParam mExtParam;
  // alias members, do not change initialize order
  NSCam::NSIoPipe::QParams& mQParams;
  std::vector<NSCam::NSIoPipe::FrameParams>& mvFrameParams;

 public:
  // alias members, do not change initialize order
  const StreamingFeaturePipeUsage& mPipeUsage;
  VarMap& mVarMap;
  const Feature::P2Util::P2Pack& mP2Pack;
  SFPIOManager& mSFPIOManager;
  MUINT32 mSlaveID = INVALID_SENSOR;
  MUINT32 mMasterID = INVALID_SENSOR;
  std::unordered_map<MUINT32, IORequest<StreamingFeatureNode, StreamingReqInfo>>
      mIORequestMap;
  IORequest<StreamingFeatureNode, StreamingReqInfo>& mMasterIOReq;

  MUINT32 mFeatureMask;
  const MUINT32 mRequestNo;
  const MUINT32 mRecordNo;
  const MUINT32 mMWFrameNo;
  IStreamingFeaturePipe::eAppMode mAppMode;
  StreamingFeatureTimer mTimer;
  MINT32 mDebugDump;
  Feature::P2Util::P2DumpType mP2DumpType = Feature::P2Util::P2_DUMP_NONE;
  HelperRWData mHelperNodeData;  // NOTE can only access by helper node thread

 private:
  std::unordered_map<MUINT32, FeaturePipeParam>& mSlaveParamMap;
  std::unordered_map<MUINT32, SrcCropInfo> mNonLargeSrcCrops;
  MSize mFullImgSize;
  MBOOL mHasGeneralOutput = MFALSE;

 public:
  StreamingFeatureRequest(const StreamingFeaturePipeUsage& pipeUsage,
                          const FeaturePipeParam& extParam,
                          MUINT32 requestNo,
                          MUINT32 recordNo);
  ~StreamingFeatureRequest();
  MVOID setDisplayFPSCounter(std::shared_ptr<FPSCounter> counter);
  MVOID setFrameFPSCounter(std::shared_ptr<FPSCounter> counter);
  MBOOL updateSFPIO();
  MVOID calSizeInfo();

  MBOOL updateResult(MBOOL result);
  MBOOL doExtCallback(FeaturePipeParam::MSG_TYPE msg);

  MSize getMasterInputSize();
  IImageBuffer* getMasterInputBuffer();
  IImageBuffer* getRecordOutputBuffer();

  MBOOL getDisplayOutput(SFPOutput* output);
  MBOOL getRecordOutput(SFPOutput* output);
  MBOOL getExtraOutput(SFPOutput* output);
  MBOOL getExtraOutputs(std::vector<SFPOutput>* outList);
  MBOOL getPhysicalOutput(SFPOutput* output, MUINT32 sensorID);
  MBOOL getLargeOutputs(std::vector<SFPOutput>* outList, MUINT32 sensorID);
  MBOOL getFDOutput(SFPOutput* output);

  std::shared_ptr<IIBuffer> requestNextFullImg(
      std::shared_ptr<StreamingFeatureNode> node, MUINT32 sensorID);

  MBOOL needDisplayOutput(std::shared_ptr<StreamingFeatureNode> node);
  MBOOL needRecordOutput(std::shared_ptr<StreamingFeatureNode> node);
  MBOOL needExtraOutput(std::shared_ptr<StreamingFeatureNode> node);
  MBOOL needFullImg(std::shared_ptr<StreamingFeatureNode> node,
                    MUINT32 sensorID);
  MBOOL needNextFullImg(std::shared_ptr<StreamingFeatureNode> node,
                        MUINT32 sensorID);
  MBOOL needPhysicalOutput(std::shared_ptr<StreamingFeatureNode> node,
                           MUINT32 sensorID);

  MBOOL hasGeneralOutput() const;
  MBOOL hasDisplayOutput() const;
  MBOOL hasRecordOutput() const;
  MBOOL hasExtraOutput() const;
  MBOOL hasPhysicalOutput(MUINT32 sensorID) const;
  MBOOL hasLargeOutput(MUINT32 sensorID) const;
  SrcCropInfo getSrcCropInfo(MUINT32 sensorID);

  DECLARE_VAR_MAP_INTERFACE(mVarMap, setVar, getVar, tryGetVar, clearVar);

  MVOID setDumpProp(MINT32 start, MINT32 count, MBOOL byRecordNo);
  MVOID setForceIMG3O(MBOOL forceIMG3O);
  MVOID setForceWarpPass(MBOOL forceWarpPass);
  MVOID setForceGpuOut(MUINT32 forceGpuOut);
  MVOID setForceGpuRGBA(MBOOL forceGpuRGBA);
  MVOID setForcePrintIO(MBOOL forcePrintIO);
  MBOOL isForceIMG3O() const;
  MBOOL hasSlave(MUINT32 sensorID) const;
  MBOOL isSlaveParamValid() const;
  FeaturePipeParam& getSlave(MUINT32 sensorID);
  const SFPSensorInput& getSensorInput(MUINT32 sensorID) const;
  VarMap& getSensorVarMap(MUINT32 sensorID);

  MBOOL getMasterFrameBasic(NSCam::NSIoPipe::FrameParams* output);

  // Legacy code for Hal1, w/o dynamic tuning & P2Pack
  MBOOL getMasterFrameTuning(NSCam::NSIoPipe::FrameParams* output);
  MBOOL getMasterFrameInput(NSCam::NSIoPipe::FrameParams* output);
  // -----

  const char* getFeatureMaskName() const;
  MBOOL need3DNR() const;
  MBOOL needFullImg() const;
  MBOOL needDump() const;
  MBOOL needNddDump() const;
  MBOOL isLastNodeP2A() const;
  MBOOL is4K2K() const;
  MBOOL isP2ACRZMode() const;
  MBOOL useWarpPassThrough() const;
  MBOOL useDirectGpuOut() const;
  MBOOL needPrintIO() const;
  MUINT32 getMasterID() const;

  MUINT32 needTPILog() const;
  MUINT32 needTPIDump() const;
  MUINT32 needTPIScan() const;
  MUINT32 needTPIBypass() const;

 private:
  MVOID createIOMapByQParams();

  MVOID checkEISQControl();

  MBOOL getCropInfo(NSCam::NSIoPipe::EPortCapbility cap,
                    MUINT32 defCropGroup,
                    NSCam::NSIoPipe::MCrpRsInfo* crop);
  MVOID calNonLargeSrcCrop(SrcCropInfo* info, MUINT32 sensorID);
  void append3DNRTag(std::string* str, MUINT32 mFeatureMask) const;
  void appendNoneTag(std::string* str, MUINT32 mFeatureMask) const;
  void appendDefaultTag(std::string* str, MUINT32 mFeatureMask) const;

 private:
  std::shared_ptr<FPSCounter> mDisplayFPSCounter;
  std::shared_ptr<FPSCounter> mFrameFPSCounter;

  static std::unordered_map<MUINT32, std::string> mFeatureMaskNameMap;

 private:
  MBOOL mResult;
  MBOOL mNeedDump;
  MBOOL mForceIMG3O;
  MBOOL mForceWarpPass;
  MUINT32 mForceGpuOut;
  MBOOL mForceGpuRGBA;
  MBOOL mForcePrintIO;
  MBOOL mIs4K2K;
  MBOOL mIsP2ACRZMode;

  MUINT32 mTPILog = 0;
  MUINT32 mTPIDump = 0;
  MUINT32 mTPIScan = 0;
  MUINT32 mTPIBypass = 0;
};
typedef std::shared_ptr<StreamingFeatureRequest> RequestPtr;
typedef std::shared_ptr<IIBuffer> ImgBuffer;
typedef std::unordered_map<MUINT32, std::shared_ptr<IBufferPool>> PoolMap;

template <typename T>
class Data {
 public:
  T mData;
  RequestPtr mRequest;

  // lower value will be process first
  MUINT32 mPriority;

  Data() : mPriority(0) {}

  virtual ~Data() {}

  Data(const T& data, const RequestPtr& request, MINT32 nice = 0)
      : mData(data), mRequest(request), mPriority(request->mRequestNo) {
    if (nice > 0) {
      // TODO(MTK): watch out for overflow
      mPriority += nice;
    }
  }

  T& operator->() { return mData; }

  const T& operator->() const { return mData; }

  class IndexConverter {
   public:
    IWaitQueue::Index operator()(const Data& data) const {
      return IWaitQueue::Index(data.mRequest->mRequestNo, data.mPriority);
    }
    static unsigned getID(const Data& data) {
      return data.mRequest->mRequestNo;
    }
    static unsigned getPriority(const Data& data) { return data.mPriority; }
  };
};

class MyFace {
 public:
  MtkCameraFaceMetadata mMeta;
  MtkCameraFace mFaceBuffer[15];
  MtkFaceInfo mPosBuffer[15];
  MyFace() {
    mMeta.faces = mFaceBuffer;
    mMeta.posInfo = mPosBuffer;
    mMeta.number_of_faces = 0;
    mMeta.ImgWidth = 0;
    mMeta.ImgHeight = 0;
  }

  MyFace(const MyFace& src) {
    mMeta = src.mMeta;
    mMeta.faces = mFaceBuffer;
    mMeta.posInfo = mPosBuffer;
    for (int i = 0; i < 15; ++i) {
      mFaceBuffer[i] = src.mFaceBuffer[i];
      mPosBuffer[i] = src.mPosBuffer[i];
    }
  }

  MyFace operator=(const MyFace& src) {
    mMeta = src.mMeta;
    mMeta.faces = mFaceBuffer;
    mMeta.posInfo = mPosBuffer;
    for (int i = 0; i < 15; ++i) {
      mFaceBuffer[i] = src.mFaceBuffer[i];
      mPosBuffer[i] = src.mPosBuffer[i];
    }
    return *this;
  }
};

class FEFMGroup {
 public:
  ImgBuffer High;
  ImgBuffer Medium;
  ImgBuffer Low;

  ImgBuffer Register_High;
  ImgBuffer Register_Medium;
  ImgBuffer Register_Low;

  MVOID clear();
  MBOOL isValid() const;
};

class FMResult {
 public:
  FEFMGroup FM_A;
  FEFMGroup FM_B;
  FEFMGroup FE;
  FEFMGroup PrevFE;
};

class RSCResult {
 public:
  union RSC_STA_0 {
    MUINT32 value;
    struct {
      MUINT16 sta_gmv_x;  // regard RSC_STA as composition of gmv_x and gmv_y
      MUINT16 sta_gmv_y;
    };
    RSC_STA_0() : value(0) {}
  };

  ImgBuffer mMV;
  ImgBuffer mBV;
  MSize mRssoSize;
  RSC_STA_0 mRscSta;  // gmv value of RSC
  MBOOL mIsValid;

  RSCResult();
  RSCResult(const ImgBuffer& mv,
            const ImgBuffer& bv,
            const MSize& rssoSize,
            const RSC_STA_0& rscSta,
            MBOOL valid);
};

class FovP2AResult {
 public:
  ImgBuffer mFEO_Master;
  ImgBuffer mFEO_Slave;
  ImgBuffer mFMO_MtoS;
  ImgBuffer mFMO_StoM;
  SmartTuningBuffer mFMTuningBuf0;
  SmartTuningBuffer mFMTuningBuf1;
  MSize mFEInSize_Master;
  MSize mFEInSize_Slave;
};

class FOVResult {
 public:
  ImgBuffer mWarpMap;
  MSize mWarpMapSize;
  MSize mWPESize;
  MRect mDisplayCrop;
  MRect mRecordCrop;
  MRect mExtraCrop;
  MSize mSensorBaseMargin;
  MSize mRRZOBaseMargin;
  MPoint mFOVShift;
  float mFOVScale;
};

class BasicImg {
 public:
  ImgBuffer mBuffer;
  MPointF mDomainOffset;
  MSizeF mDomainTransformScale;
  MBOOL mIsReady;

  BasicImg();
  explicit BasicImg(const ImgBuffer& img);
  BasicImg(const ImgBuffer& img, const MPointF& offset);
  BasicImg(const ImgBuffer& img, const MPointF& offset, const MBOOL& isReady);

  MVOID setDomainInfo(const BasicImg& img);
  MBOOL syncCache(NSCam::eCacheCtrl ctrl);
};

class N3DResult {
 public:
  ImgBuffer mFEBInputImg_Master;
  ImgBuffer mFEBInputImg_Slave;
  ImgBuffer mFECInputImg_Master;
  ImgBuffer mFECInputImg_Slave;
  ImgBuffer mFEBO_Master;
  ImgBuffer mFEBO_Slave;
  ImgBuffer mFECO_Master;
  ImgBuffer mFECO_Slave;
  ImgBuffer mFMBO_MtoS;
  ImgBuffer mFMBO_StoM;
  ImgBuffer mFMCO_MtoS;
  ImgBuffer mFMCO_StoM;
  ImgBuffer mCCin_Master;
  ImgBuffer mCCin_Slave;
  ImgBuffer mRectin_Master;
  ImgBuffer mRectin_Slave;
  SmartTuningBuffer tuningBuf1;
  SmartTuningBuffer tuningBuf2;
  SmartTuningBuffer tuningBuf3;
  SmartTuningBuffer tuningBuf4;
  SmartTuningBuffer tuningBuf5;
  SmartTuningBuffer tuningBuf6;
  SmartTuningBuffer tuningBuf7;
  SmartTuningBuffer tuningBuf8;
};

class DualBasicImg {
 public:
  BasicImg mMaster;
  BasicImg mSlave;
  DualBasicImg();
  explicit DualBasicImg(const BasicImg& master);
  DualBasicImg(const BasicImg& master, const BasicImg& slave);
};

class P2AMDPReq {
 public:
  BasicImg mMdpIn;
  std::vector<SFPOutput> mMdpOuts;
};

class HelpReq {
 public:
  enum INTERNAL_MSG_TYPE { MSG_UNKNOWN, MSG_PMDP_DONE };
  FeaturePipeParam::MSG_TYPE mCBMsg = FeaturePipeParam::MSG_INVALID;
  INTERNAL_MSG_TYPE mInternalMsg = MSG_UNKNOWN;
  HelpReq();
  explicit HelpReq(FeaturePipeParam::MSG_TYPE msg);
  HelpReq(FeaturePipeParam::MSG_TYPE msg, INTERNAL_MSG_TYPE intMsg);
};

class DepthImg {
 public:
  BasicImg mCleanYuvImg;
  ImgBuffer mDMBGImg;
  ImgBuffer mDepthMapImg;
};

class TPIRes {
 public:
  std::map<unsigned, BasicImg> mSFP;
  std::map<unsigned, BasicImg> mTP;
  std::map<unsigned, IMetadata*> mMeta;

 public:
};

typedef Data<ImgBuffer> ImgBufferData;
typedef Data<MyFace> FaceData;
typedef Data<FMResult> FMData;
typedef Data<FeaturePipeParam::MSG_TYPE> CBMsgData;
typedef Data<HelpReq> HelperData;
typedef Data<RSCResult> RSCData;
typedef Data<FovP2AResult> FOVP2AData;
typedef Data<FOVResult> FOVData;
typedef Data<BasicImg> BasicImgData;
typedef Data<N3DResult> N3DData;
typedef Data<DepthImg> DepthImgData;
typedef Data<P2AMDPReq> P2AMDPReqData;
typedef Data<TPIRes> TPIData;

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_STREAMINGFEATUREDATA_H_
