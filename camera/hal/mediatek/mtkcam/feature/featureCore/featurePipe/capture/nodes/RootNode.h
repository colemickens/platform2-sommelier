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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_NODES_ROOTNODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_NODES_ROOTNODE_H_

#include "../CaptureFeatureNode.h"
#include <memory>
#include <vector>
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

class RootNode : public CaptureFeatureNode {
  /*
   *  Tuning Param for BSS ALG.
   *  Should not be configure by customer
   */
  static const int MF_BSS_ON = 1;
  static const int MF_BSS_VER = 2;
  static const int MF_BSS_ROI_PERCENTAGE = 95;

  /*
   *  Tuning Param for EIS.
   *  Should not be configure by customer
   */
  static const int MFC_GMV_CONFX_TH = 25;
  static const int MFC_GMV_CONFY_TH = 25;
  static const int MAX_GMV_CNT = 12;

  struct GMV {
    MINT32 x = 0;
    MINT32 y = 0;
  };

 public:
  RootNode(NodeID_T nid, const char* name);
  virtual ~RootNode();

 public:
  virtual MBOOL onData(DataID id, const RequestPtr& data);
  virtual MERROR evaluate(CaptureFeatureInferenceData* rInference);

 protected:
  virtual MBOOL onInit();
  virtual MBOOL onThreadStart();
  virtual MBOOL onThreadLoop();
  virtual MBOOL onThreadStop();

 private:
  MVOID reorder(const std::vector<RequestPtr>& rvRequests,
                const std::vector<RequestPtr>& rvOrderedRequests);

 private:
  mutable std::mutex mLock;
  WaitQueue<RequestPtr> mRequests;
  std::vector<RequestPtr> mvPendingRequests;
};

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_NODES_ROOTNODE_H_
