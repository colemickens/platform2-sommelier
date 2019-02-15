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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_NODES_YUVNODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_NODES_YUVNODE_H_

#include "../CaptureFeatureNode.h"
#include <memory>
#include <mtkcam/3rdparty/plugin/PipelinePlugin.h>
#include <mtkcam/3rdparty/plugin/PipelinePluginType.h>
#include <vector>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

class YUVNode : public CaptureFeatureNode {
 public:
  YUVNode(NodeID_T nid, const char* name);
  virtual ~YUVNode();
  MVOID setBufferPool(const std::shared_ptr<CaptureBufferPool>& pool);

 public:
  typedef NSCam::NSPipelinePlugin::YuvPlugin::Request::Ptr PluginRequestPtr;

  virtual MBOOL onData(DataID id, const RequestPtr& data);
  virtual MERROR evaluate(CaptureFeatureInferenceData* rInference);
  virtual RequestPtr findRequest(PluginRequestPtr&);
  virtual MBOOL onRequestRepeat(RequestPtr&);
  virtual MBOOL onRequestProcess(RequestPtr&);
  virtual MBOOL onRequestFinish(RequestPtr&);

 protected:
  virtual MBOOL onInit();
  virtual MBOOL onUninit();
  virtual MBOOL onThreadStart();
  virtual MBOOL onThreadStop();
  virtual MBOOL onThreadLoop();

 private:
  typedef NSCam::NSPipelinePlugin::YuvPlugin YuvPlugin;
  typedef NSCam::NSPipelinePlugin::YuvPlugin::IProvider::Ptr ProviderPtr;
  typedef NSCam::NSPipelinePlugin::YuvPlugin::IInterface::Ptr InterfacePtr;
  typedef NSCam::NSPipelinePlugin::YuvPlugin::Selection Selection;
  typedef NSCam::NSPipelinePlugin::YuvPlugin::RequestCallback::Ptr
      RequestCallbackPtr;

  struct RequestPair {
    RequestPtr mPipe;
    PluginRequestPtr mPlugin;
  };

  struct ProviderPair {
    ProviderPtr mpProvider;
    FeatureID_T mFeatureId;
  };

  std::shared_ptr<CaptureBufferPool> mpBufferPool;

  YuvPlugin::Ptr mPlugin;
  InterfacePtr mpInterface;
  std::vector<ProviderPair> mProviderPairs;
  RequestCallbackPtr mpCallback;

  WaitQueue<RequestPtr> mRequests;
  std::vector<RequestPair> mRequestPairs;

  mutable std::mutex mPairLock;
};

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_NODES_YUVNODE_H_
