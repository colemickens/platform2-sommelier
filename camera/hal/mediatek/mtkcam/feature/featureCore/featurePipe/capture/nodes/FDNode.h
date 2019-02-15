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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_NODES_FDNODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_NODES_FDNODE_H_

#include "../CaptureFeatureNode.h"
#include <memory>
#include <mtkcam/feature/FaceDetection/fd_hal_base.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

class FDNode : public CaptureFeatureNode {
 public:
  FDNode(NodeID_T nid, const char* name);
  virtual ~FDNode();

 public:
  virtual MBOOL onData(DataID id, const RequestPtr& data);
  virtual MERROR evaluate(CaptureFeatureInferenceData* rInference);

 protected:
  virtual MBOOL onInit();
  virtual MBOOL onUninit();
  virtual MBOOL onThreadStart();
  virtual MBOOL onThreadStop();
  virtual MBOOL onThreadLoop();

  virtual MBOOL onRequestProcess(RequestPtr&);

 private:
  WaitQueue<RequestPtr> mRequests;

  // face detection
  std::shared_ptr<halFDBase> mpFDHalObj;
  MUINT8* mpFDWorkingBuffer;
  MUINT8* mpPureYBuffer;
  MtkCameraFaceMetadata* mpDetectedFaces;
};

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_NODES_FDNODE_H_
