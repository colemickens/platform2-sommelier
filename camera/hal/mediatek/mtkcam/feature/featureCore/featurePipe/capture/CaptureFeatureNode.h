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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATURENODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATURENODE_H_

#include <bitset>
#include "buffer/CaptureBufferPool.h"
#include "CaptureFeatureInference.h"
#include "CaptureFeatureRequest.h"
#include "CaptureFeature_Common.h"
#include <common/include/CamThreadNode.h>
#include <common/include/SeqUtil.h>

#include <condition_variable>
#include <memory>
#include <mutex>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

class NodeSignal {
 public:
  enum Signal {
    SIGNAL_GPU_READY = 0x01 << 0,
  };

  enum Status {
    STATUS_IN_FLUSH = 0x01 << 0,
  };

  NodeSignal();
  virtual ~NodeSignal();
  MVOID setSignal(Signal signal);
  MVOID clearSignal(Signal signal);
  MBOOL getSignal(Signal signal);
  MVOID waitSignal(Signal signal);

  MVOID setStatus(Status status);
  MVOID clearStatus(Status status);
  MBOOL getStatus(Status status);

 private:
  std::mutex mMutex;
  std::condition_variable mCondition;
  MUINT32 mSignal;
  MUINT32 mStatus;
};

class CaptureFeatureDataHandler {
 public:
  typedef PathID_T DataID;

 public:
  virtual ~CaptureFeatureDataHandler();
  virtual MBOOL onData(DataID, const RequestPtr&) { return MFALSE; }

  static const char* ID2Name(DataID id);

  template <typename T>
  static unsigned getSeq(const T& data) {
    return data->getRequestNo();
  }
  static unsigned getSeq(const RequestPtr& data) {
    return data->getRequestNo();
  }

  static const bool supportSeq = true;
};

class CaptureFeatureNode : public CaptureFeatureDataHandler,
                           public CamThreadNode<CaptureFeatureDataHandler> {
 public:
  typedef CamGraph<CaptureFeatureNode> Graph_T;
  typedef CaptureFeatureDataHandler Handler_T;

 public:
  CaptureFeatureNode(NodeID_T nid, const char* name, MUINT32 uLogLevel = 0);
  virtual ~CaptureFeatureNode();
  MVOID setSensorIndex(MINT32 sensorIndex);
  MVOID setNodeSignal(const std::shared_ptr<NodeSignal>& nodeSignal);
  MVOID setCropCalculator(
      const std::shared_ptr<CropCalculator>& rCropCalculator);
  MVOID setLogLevel(MUINT32 uLogLevel);

  virtual MERROR evaluate(CaptureFeatureInferenceData* rInference) = 0;
  NodeID_T getNodeID() { return mNodeId; }

  virtual MVOID dispatch(const RequestPtr& pRequest);

 protected:
  virtual MBOOL onInit();
  virtual MBOOL onUninit() { return MTRUE; }
  virtual MBOOL onThreadStart() { return MTRUE; }
  virtual MBOOL onThreadStop() { return MTRUE; }
  virtual MBOOL onThreadLoop() = 0;

  static MBOOL dumpData(const RequestPtr& request,
                        std::shared_ptr<IImageBuffer> buffer,
                        const char* fmt,
                        ...);
  static MBOOL dumpNamedData(const RequestPtr& request,
                             std::shared_ptr<IImageBuffer> buffer,
                             const char* name);
  static MUINT32 dumpData(const char* buffer,
                          MUINT32 size,
                          const char* filename);
  static MBOOL loadData(std::shared_ptr<IImageBuffer> buffer,
                        const char* filename);
  static MUINT32 loadData(char* buffer, size_t size, const char* filename);

 protected:
  MINT32 mSensorIndex;
  NodeID_T mNodeId;
  MUINT32 mLogLevel;
  std::shared_ptr<NodeSignal> mNodeSignal;
  std::shared_ptr<CropCalculator> mpCropCalculator;
};

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATURENODE_H_
