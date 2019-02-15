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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATUREPIPE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATUREPIPE_H_

#include "buffer/CaptureBufferPool.h"
#include <common/include/CamPipe.h>
#include "CaptureFeatureNode.h"
#include "CaptureFeatureInference.h"
#include <list>
#include <memory>
#include <mutex>
#include "nodes/FDNode.h"
#include "nodes/MDPNode.h"
#include "nodes/P2ANode.h"
#include "nodes/RootNode.h"
#include "nodes/YUVNode.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

class CaptureFeaturePipe
    : public virtual CamPipe<CaptureFeatureNode>,
      public virtual CaptureFeatureNode::Handler_T,
      public virtual ICaptureFeaturePipe,
      public std::enable_shared_from_this<CaptureFeaturePipe> {
 public:
  CaptureFeaturePipe(MINT32 sensorIndex, const UsageHint& usageHint);
  virtual ~CaptureFeaturePipe();

 public:
  virtual void setSensorIndex(MINT32 sensorIndex);
  virtual MVOID init();
  virtual MBOOL config(const StreamConfigure config);
  virtual MVOID uninit();
  virtual MERROR enque(std::shared_ptr<ICaptureFeatureRequest> request);
  virtual MVOID setCallback(std::shared_ptr<RequestCallback>);
  virtual MBOOL flush();

  virtual std::shared_ptr<ICaptureFeatureRequest> acquireRequest();
  virtual MVOID releaseRequest(std::shared_ptr<ICaptureFeatureRequest> request);

 protected:
  typedef CamPipe<CaptureFeatureNode> PARENT_PIPE;
  virtual MBOOL onInit();
  virtual MVOID onUninit();
  virtual MBOOL onData(DataID id, const RequestPtr& data);

 private:
  MERROR prepareNodeSetting();
  MERROR prepareNodeConnection();
  MERROR prepareBuffer();
  MVOID releaseBuffer();
  MVOID releaseNodeSetting();
  std::shared_ptr<IBufferPool> createFullImgPool(const char* name, MSize size);

  MINT32 mSensorIndex;
  MUINT32 mLogLevel;
  MBOOL mForceImg3o422;

  std::shared_ptr<RootNode> mpRootNode;
  std::shared_ptr<P2ANode> mpP2ANode;
  std::shared_ptr<FDNode> mpFDNode;
  std::shared_ptr<YUVNode> mpYUVNode;
  std::shared_ptr<MDPNode> mpMDPNode;

  std::shared_ptr<CropCalculator> mpCropCalculator;
  std::shared_ptr<CaptureBufferPool> mpBufferPool;

  typedef std::list<std::shared_ptr<CaptureFeatureNode> > NODE_LIST;
  NODE_LIST mpNodes;

  std::shared_ptr<NodeSignal> mNodeSignal;

  std::shared_ptr<RequestCallback> mpCallback;
  CaptureFeatureInference mInference;
};

class PipeBufferHandle : public BufferHandle {
 public:
  PipeBufferHandle(std::shared_ptr<CaptureBufferPool> pBufferPool,
                   Format_T format,
                   const MSize& size);
  virtual ~PipeBufferHandle() {}

  virtual MERROR acquire(MINT usage = eBUFFER_USAGE_HW_CAMERA_READWRITE |
                                      eBUFFER_USAGE_SW_READ_OFTEN);

  virtual IImageBuffer* native();
  virtual MUINT32 getTransform();
  virtual MVOID release();
  virtual MVOID dump(std::ostream& os) const;

 private:
  std::shared_ptr<CaptureBufferPool> mpBufferPool;
  SmartImageBuffer mpSmartBuffer;
  Format_T mFormat;
  MSize mSize;
};

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATUREPIPE_H_
