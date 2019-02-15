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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_STREAMINGFEATUREPIPE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_STREAMINGFEATUREPIPE_H_

#include <featurePipe/common/include/CamPipe.h>
#include <featurePipe/common/include/IOUtil.h>
#include "ImgBufferStore.h"
#include <list>
#include <memory>
#include <mutex>
#include "P2ANode.h"
#include "P2CamContext.h"
#include "RootNode.h"
#include <set>
#include "StreamingFeatureNode.h"
#include "StreamingFeaturePipeUsage.h"
#include <vector>

#if MTK_DP_ENABLE
#include "P2AMDPNode.h"
#endif
#include "HelperNode.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class StreamingFeaturePipe : public CamPipe<StreamingFeatureNode>,
                             public StreamingFeatureNode::Handler_T,
                             public IStreamingFeaturePipe {
 public:
  StreamingFeaturePipe(MUINT32 sensorIndex, const UsageHint& usageHint);
  virtual ~StreamingFeaturePipe();

 public:
  // IStreamingFeaturePipe Members
  virtual void setSensorIndex(MUINT32 sensorIndex);
  virtual MBOOL init(const char* name = NULL);
  virtual MBOOL config(const StreamConfigure config);
  virtual MBOOL uninit(const char* name = NULL);
  virtual MBOOL enque(const FeaturePipeParam& param);
  virtual MBOOL flush();
  virtual MBOOL sendCommand(NSCam::v4l2::ESDCmd cmd,
                            MINTPTR arg1 = 0,
                            MINTPTR arg2 = 0,
                            MINTPTR arg3 = 0);
  virtual MBOOL addMultiSensorID(MUINT32 sensorID);

 public:
  virtual MVOID sync();
  virtual IImageBuffer* requestBuffer();
  virtual MBOOL returnBuffer(IImageBuffer* buffer);

 protected:
  typedef CamPipe<StreamingFeatureNode> PARENT_PIPE;
  virtual MBOOL onInit();
  virtual MVOID onUninit();

 protected:
  virtual MBOOL onData(DataID id, const RequestPtr& data);

 private:
  MBOOL earlyInit();
  MVOID lateUninit();
  MBOOL initNodes();
  MBOOL uninitNodes();

  MBOOL prepareDebugSetting();
  MBOOL prepareGeneralPipe();
  MBOOL prepareNodeSetting();
  MBOOL prepareNodeConnection();
  MBOOL prepareIOControl();
  MBOOL prepareBuffer();
  MBOOL prepareCamContext();
  MVOID prepareFeatureRequest(const FeaturePipeParam& param);
  MVOID prepareEISQControl(const FeaturePipeParam& param);
  MVOID prepareIORequest(const RequestPtr& request);
  MVOID prepareIORequest(const RequestPtr& request,
                         const std::set<StreamType>& generalStreams,
                         MUINT32 sensorID);

  std::shared_ptr<IBufferPool> createFullImgPool(const char* name, MSize size);
  std::shared_ptr<IBufferPool> createImgPool(const char* name,
                                             MSize size,
                                             EImageFormat fmt);
  MVOID createPureImgPools(const char* name, MSize size);

  MVOID releaseGeneralPipe();
  MVOID releaseNodeSetting();
  MVOID releaseBuffer();
  MVOID releaseCamContext();

  MVOID applyMaskOverride(const RequestPtr& request);
  MVOID applyVarMapOverride(const RequestPtr& request);

 private:
  MUINT32 mForceOnMask;
  MUINT32 mForceOffMask;
  MUINT32 mSensorIndex;
  StreamingFeaturePipeUsage mPipeUsage;
  MUINT32 mCounter = 0;
  MUINT32 mRecordCounter;
  std::shared_ptr<FPSCounter> mDisplayFPSCounter;
  std::shared_ptr<FPSCounter> mFrameFPSCounter;

  MINT32 mDebugDump;
  MINT32 mDebugDumpCount;
  MBOOL mDebugDumpByRecordNo;
  MBOOL mForceIMG3O;
  MBOOL mForceWarpPass;
  MUINT32 mForceGpuOut;
  MBOOL mForceGpuRGBA;
  MBOOL mUsePerFrameSetting;
  MBOOL mForcePrintIO;
  MBOOL mEarlyInited;

  std::shared_ptr<RootNode> mRootNode;
  std::shared_ptr<P2ANode> mP2A;
#if MTK_DP_ENABLE
  std::shared_ptr<P2AMDPNode> mP2AMDP;
#endif
  std::shared_ptr<HelperNode> mHelper;

  PoolMap mPureImgPoolMap;
  std::shared_ptr<IBufferPool> mFullImgPool;
  std::shared_ptr<IBufferPool> mDepthYuvOutPool;
  std::shared_ptr<IBufferPool> mBokehOutPool;
  std::shared_ptr<IBufferPool> mDummyImgPool;
  std::shared_ptr<IBufferPool> mVendorInPool;
  std::shared_ptr<IBufferPool> mVendorOutPool;
  std::shared_ptr<IBufferPool> mTPIInPool;
  std::shared_ptr<IBufferPool> mTPIOutPool;
  std::shared_ptr<IBufferPool> mEisFullImgPool;
  std::shared_ptr<IBufferPool> mWarpOutputPool;
  std::shared_ptr<IBufferPool> mFOVWarpOutputPool;

  std::shared_ptr<NSCam::v4l2::INormalStream> mNormalStream = nullptr;
  MUINT32 mDipVersion = 0;

  typedef std::list<std::shared_ptr<StreamingFeatureNode> > NODE_LIST;
  NODE_LIST mNodes;

  NODE_LIST mDisplayPath;
  NODE_LIST mRecordPath;
  NODE_LIST mPhysicalPath;

  std::shared_ptr<NodeSignal> mNodeSignal;

  ImgBufferStore mInputBufferStore;

  std::mutex mContextMutex;
  MBOOL mContextCreated[P2CamContext::SENSOR_INDEX_MAX];

  IOControl<StreamingFeatureNode, StreamingReqInfo> mIOControl;
  std::vector<MUINT32> mAllSensorIDs;
};

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_STREAMINGFEATUREPIPE_H_
