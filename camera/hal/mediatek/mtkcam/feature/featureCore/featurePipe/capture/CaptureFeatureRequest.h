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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATUREREQUEST_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATUREREQUEST_H_

#include <bitset>
#include "CaptureFeature_Common.h"
#include <common/include/DebugControl.h>
#include <common/include/IIBuffer.h>
#include <map>
#include <memory>
#include <mtkcam/feature/featurePipe/ICaptureFeaturePipe.h>
#include <vector>

#undef PIPE_CLASS_TAG
#define PIPE_CLASS_TAG "Request"
#undef PIPE_TRACE
#define PIPE_TRACE TRACE_CAPTURE_FEATURE_DATA
#include <common/include/WaitQueue.h>
#include "CaptureFeatureTimer.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

enum CaptureFeaturePathID {
  PID_ENQUE,
  PID_ROOT_TO_RAW,
  PID_ROOT_TO_P2A,
  PID_ROOT_TO_MULTIFRAME,
  PID_RAW_TO_P2A,
  PID_P2A_TO_DEPTH,
  PID_P2A_TO_FUSION,
  PID_P2A_TO_MULTIFRAME,
  PID_P2A_TO_YUV,
  PID_P2A_TO_YUV2,
  PID_P2A_TO_MDP,
  PID_P2A_TO_FD,
  PID_FD_TO_DEPTH,
  PID_FD_TO_FUSION,
  PID_FD_TO_MULTIFRAME,
  PID_FD_TO_YUV,
  PID_FD_TO_YUV2,
  PID_MULTIFRAME_TO_YUV,
  PID_MULTIFRAME_TO_YUV2,
  PID_MULTIFRAME_TO_BOKEH,
  PID_MULTIFRAME_TO_MDP,
  PID_FUSION_TO_YUV,
  PID_FUSION_TO_MDP,
  PID_DEPTH_TO_BOKEH,
  PID_YUV_TO_BOKEH,
  PID_YUV_TO_YUV2,
  PID_YUV_TO_MDP,
  PID_BOKEH_TO_YUV2,
  PID_BOKEH_TO_MDP,
  PID_YUV2_TO_MDP,
  PID_DEQUE,
  NUM_OF_PATH,
  NULL_PATH = 0xFF,
};

enum CaptureFeatureNodeID {
  NID_ROOT,
  NID_RAW,
  NID_P2A,
  NID_FD,
  NID_MULTIFRAME,
  NID_FUSION,
  NID_DEPTH,
  NID_YUV,
  NID_YUV_R1,  // For repeating
  NID_YUV_R2,  // For repeating
  NID_BOKEH,
  NID_YUV2,
  NID_YUV2_R1,  // For repeating
  NID_YUV2_R2,  // For repeating
  NID_MDP,
  NUM_OF_NODE,
};

enum CaptureFeatureBufferTypeID {
  TID_MAN_FULL_RAW,
  TID_MAN_FULL_YUV,
  TID_MAN_RSZ_RAW,
  TID_MAN_RSZ_YUV,
  TID_MAN_CROP1_YUV,  // Only for Output
  TID_MAN_CROP2_YUV,  // Only for Output
  TID_MAN_SPEC_YUV,
  TID_MAN_DEPTH,
  TID_MAN_LCS,
  TID_MAN_FD_YUV,
  TID_MAN_FD,  // Support per-frame computinf FD
  TID_SUB_FULL_RAW,
  TID_SUB_FULL_YUV,
  TID_SUB_RSZ_RAW,
  TID_SUB_RSZ_YUV,
  TID_SUB_LCS,
  //
  TID_POSTVIEW,
  TID_JPEG,
  TID_THUMBNAIL,
  NUM_OF_TYPE,
  NULL_TYPE = 0xFF,
};

enum CaptureFeatureSizeID {
  SID_FULL,
  SID_RESIZED,
  SID_SPECIFIC,
  SID_ARBITRARY,
  SID_BINNING,
  NUM_OF_SIZE,
  NULL_SIZE = 0xFF,
};

enum Direction {
  INPUT,
  OUTPUT,
};

typedef MUINT8 NodeID_T;
typedef MUINT8 TypeID_T;
typedef MUINT8 PathID_T;
typedef MUINT8 SizeID_T;
typedef MUINT32 Format_T;

#define PIPE_BUFFER_STARTER (0x1 << 5)

class CaptureFeatureRequest;
class CaptureFeatureNodeRequest {
  friend class CaptureFeatureRequest;

 public:
  explicit CaptureFeatureNodeRequest(
      std::shared_ptr<CaptureFeatureRequest> pRequest)
      : mpRequest(pRequest) {}

  virtual BufferID_T mapBufferID(TypeID_T, Direction);

  virtual MBOOL hasMetadata(MetadataID_T);

  //
  virtual IImageBuffer* acquireBuffer(BufferID_T);
  virtual MVOID releaseBuffer(BufferID_T);
  virtual MUINT32 getImageTransform(BufferID_T) const;
  virtual IMetadata* acquireMetadata(MetadataID_T);
  virtual MVOID releaseMetadata(MetadataID_T);

  virtual ~CaptureFeatureNodeRequest() {}

 private:
  std::shared_ptr<CaptureFeatureRequest> mpRequest;
  std::map<TypeID_T, BufferID_T> mIBufferMap;
  std::map<TypeID_T, BufferID_T> mOBufferMap;
  std::bitset<32> mMetadataSet;
};

enum CaptureFeaturePrivateID {
  PID_REQUEST_REPEAT = NUM_OF_PARAMETER,
  NUM_OF_TOTAL_PARAMETER,
};

class CaptureFeatureRequest
    : public ICaptureFeatureRequest,
      public std::enable_shared_from_this<CaptureFeatureRequest> {
  friend class CaptureFeatureInference;
  friend class CaptureFeatureInferenceData;
  friend class CaptureFeaturePipe;

  // Implementation of ICaptureFeatureRequest
 public:
  CaptureFeatureRequest();

  virtual MVOID addBuffer(BufferID_T, std::shared_ptr<BufferHandle>);
  virtual MVOID addMetadata(MetadataID_T, std::shared_ptr<MetadataHandle>);

  // get the acquired buffer handle;
  virtual std::shared_ptr<BufferHandle> getBuffer(BufferID_T);
  virtual std::shared_ptr<MetadataHandle> getMetadata(MetadataID_T);

  virtual MVOID addFeature(FeatureID_T);
  virtual MBOOL hasFeature(FeatureID_T);
  virtual MVOID setFeatures(MUINT64);
  virtual MVOID addParameter(ParameterID_T, MINT32);

  virtual MINT32 getParameter(ParameterID_T);
  virtual MINT32 getRequestNo();

 public:
  virtual MINT32 getFrameNo();
  virtual MVOID setCrossRequest(
      std::shared_ptr<CaptureFeatureRequest> pRequest);

  virtual MVOID addPipeBuffer(BufferID_T, TypeID_T, MSize&, Format_T);
  virtual MVOID addPath(PathID_T);
  virtual MVOID traverse(PathID_T);
  virtual MBOOL isTraversed();
  virtual MBOOL isSatisfied(NodeID_T);
  virtual MVOID addNodeIO(NodeID_T,
                          std::vector<BufferID_T>&,
                          std::vector<BufferID_T>&,
                          std::vector<MetadataID_T>&);

  virtual MVOID decNodeReference(NodeID_T);
  virtual MVOID decBufferRef(BufferID_T);
  virtual MVOID decMetadataRef(MetadataID_T);
  virtual std::vector<NodeID_T> getPreviousNodes(NodeID_T);
  virtual std::vector<NodeID_T> getNextNodes(NodeID_T);

  virtual std::shared_ptr<CaptureFeatureNodeRequest> getNodeRequest(NodeID_T);
  virtual MVOID clear();
  virtual MVOID dump();
  virtual ~CaptureFeatureRequest();

  // Query Buffer Info
  virtual MINT getImageFormat(BufferID_T);
  virtual MSize getImageSize(BufferID_T);
  CaptureFeatureTimer mTimer;

  std::shared_ptr<RequestCallback> mpCallback;

 private:
  struct BufferItem {
    BufferItem()
        : mAcquired(MFALSE),
          mCreated(MFALSE),
          mReference(0),
          mTypeId(NULL_TYPE),
          mSize(MSize(0, 0)),
          mFormat(0) {}

    MBOOL mAcquired;
    MBOOL mCreated;
    MUINT32 mReference;
    TypeID_T mTypeId;
    MSize mSize;
    Format_T mFormat;
  };

  struct MetadataItem {
    MetadataItem() : mAcquired(MFALSE), mReference(0), mpHandle(NULL) {}

    MBOOL mAcquired;
    MUINT32 mReference;
    std::shared_ptr<MetadataHandle> mpHandle;
  };

 private:
  std::weak_ptr<CaptureFeatureRequest> mpCrossRequest;
  std::map<NodeID_T, std::shared_ptr<CaptureFeatureNodeRequest>> mNodeRequest;

  std::mutex mBufferMutex;
  std::map<BufferID_T, BufferItem> mBufferItems;
  std::map<BufferID_T, std::shared_ptr<BufferHandle>> mBufferMap;
  std::mutex mMetadataMutex;
  std::map<MetadataID_T, MetadataItem> mMetadataItems;
  std::bitset<64> mFeatures;
  MINT32 mParameter[NUM_OF_TOTAL_PARAMETER];

  // The first index is source node ID, and the second index is destination node
  // ID. The value is path ID.
  MUINT8 mNodePath[NUM_OF_NODE][NUM_OF_NODE];

  // Record all pathes to traverse
  std::bitset<64> mTraverse;
};

typedef std::shared_ptr<CaptureFeatureRequest> RequestPtr;

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATUREREQUEST_H_
