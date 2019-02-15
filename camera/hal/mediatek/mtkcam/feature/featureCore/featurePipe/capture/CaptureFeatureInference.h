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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATUREINFERENCE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATUREINFERENCE_H_

#include <bitset>
#include "CaptureFeatureRequest.h"
#include <map>
#include <memory>
#include <mtkcam/3rdparty/plugin/PipelinePlugin.h>
#include <vector>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

class CaptureFeatureNode;

enum eCaptureFeatureInferenceStrategy { STG_LONGEST_PATH, STG_PREDEFINED };

typedef MUINT Format_T;

struct DataItem {
  DataItem()
      : mNodeId(0),
        mTypeId(NULL_TYPE),
        mFormat(0),
        mSizeId(NULL_SIZE),
        mSize(0, 0),
        mRedirect(-1),
        mBufferId(NULL_BUFFER) {}
  inline MVOID markReference(NodeID_T nodeId) {
    mReferences.set((mReferences.size() - 1) - nodeId);
  }

  inline MVOID markFeature(FeatureID_T featureId) {
    mFeatures.set((mFeatures.size() - 1) - featureId);
  }

  NodeID_T mNodeId;  // buffer owner
  TypeID_T mTypeId;
  std::bitset<32> mReferences;  // node referred
  std::bitset<64> mFeatures;    // Feature applied
  Format_T mFormat;             // Image Format
  SizeID_T mSizeId;             // Image Size
  MSize mSize;                  // Specified Size
  MINT mRedirect;
  BufferID_T mBufferId;
};

/*
 * 1. Use source data to infer the maximum of data flow
 * 2. Map the targets into the flow
 * 3. Remove the data without any reference.
 *
 */
class CaptureFeatureInferenceData {
 public:
  struct SrcData {
    SrcData() : mTypeId(0), mFormat(0), mSizeId(0), mSize(MSize(0, 0)) {}

    TypeID_T mTypeId;
    Format_T mFormat;
    SizeID_T mSizeId;
    MSize mSize;
  };

  struct DstData {
    DstData()
        : mTypeId(0),
          mFormat(0),
          mSizeId(0),
          mSize(MSize(0, 0)),
          mInPlace(MFALSE) {}

    TypeID_T mTypeId;
    Format_T mFormat;
    SizeID_T mSizeId;
    MSize mSize;
    MBOOL mInPlace;
  };

  CaptureFeatureInferenceData();

  /*
   * Step 1:
   */
  MVOID addSource(TypeID_T tid, BufferID_T bid, Format_T fmt, MSize size);
  /*
   * Step 2:
   * a node could have multiple IO, but the IO should have no duplicated type
   *
   */
  MVOID addNodeIO(NodeID_T nid,
                  const std::vector<SrcData>& vSrcData,
                  const std::vector<DstData>& vDstData,
                  const std::vector<MetadataID_T>& vMetadata,
                  const std::vector<FeatureID_T>& vFeature,
                  MBOOL forced = MFALSE);

  /*
   * Step 3:
   *
   */
  MVOID addTarget(TypeID_T tid, BufferID_T bid);

  /*
   * Step 4:
   *
   */
  MVOID determine(std::shared_ptr<CaptureFeatureRequest> pRequest);

  MVOID dump();

  inline MBOOL hasType(TypeID_T tid) {
    return mInferredType.test((mInferredType.size() - 1) - tid);
  }

  inline MBOOL hasFeature(FeatureID_T fid) {
    return mFeatures.test((mFeatures.size() - 1) - fid);
  }

  inline MVOID markFeature(FeatureID_T fid) {
    mFeatures.set((mFeatures.size() - 1) - fid);
  }

  inline MVOID clearFeature(FeatureID_T fid) {
    mFeatures.reset((mFeatures.size() - 1) - fid);
  }

  MSize getSize(TypeID_T typeId);

  inline std::vector<SrcData>& getSharedSrcData() {
    mTempSrcData.clear();
    return mTempSrcData;
  }

  inline std::vector<DstData>& getSharedDstData() {
    mTempDstData.clear();
    return mTempDstData;
  }

  inline std::vector<FeatureID_T>& getSharedFeatures() {
    mTempFeatures.clear();
    return mTempFeatures;
  }

  inline std::vector<MetadataID_T>& getSharedMetadatas() {
    mTempMetadatas.clear();
    return mTempMetadatas;
  }
  inline MUINT8 getRequestCount() { return mRequestCount; }

  inline MUINT8 getRequestIndex() { return mRequestIndex; }

 private:
  MUINT8 addDataItem(NodeID_T nid,
                     TypeID_T tid,
                     BufferID_T bid = NULL_BUFFER,
                     std::bitset<64> features = std::bitset<64>());

 public:
  // for reuse
  std::vector<SrcData> mTempSrcData;
  std::vector<DstData> mTempDstData;
  std::vector<FeatureID_T> mTempFeatures;
  std::vector<MetadataID_T> mTempMetadatas;

  std::shared_ptr<IMetadata> mpIMetadataHal;
  std::shared_ptr<IMetadata> mpIMetadataApp;
  std::shared_ptr<IMetadata> mpIMetadataDynamic;

  DataItem mDataItems[32];
  MUINT8 mDataCount;

  MUINT8 mRequestIndex;
  MUINT8 mRequestCount;
  std::bitset<32> mInferredType;
  // map type id into data item
  MUINT8 mInferredItems[NUM_OF_TYPE];

  std::bitset<64> mFeatures;

  // the value is index of data item
  MINT8 mNodeInput[NUM_OF_NODE][NUM_OF_TYPE];
  MINT8 mNodeOutput[NUM_OF_NODE][NUM_OF_TYPE];
  std::vector<MetadataID_T> mNodeMeta[NUM_OF_NODE];

  std::bitset<32> mPathUsed;
  std::bitset<32> mNodeUsed;
  MUINT8 mPipeBufferCounter;

  // feature related
  MBOOL mRequiredFD;
  MBOOL mPerFrameFD;
};

class CaptureFeatureInference {
 public:
  CaptureFeatureInference();

  virtual MVOID addNode(NodeID_T nid,
                        std::shared_ptr<CaptureFeatureNode> pNode);

  virtual MERROR evaluate(std::shared_ptr<CaptureFeatureRequest> pRequest);

  virtual ~CaptureFeatureInference() {}

 private:
  std::map<NodeID_T, std::shared_ptr<CaptureFeatureNode> > mNodeMap;
};
}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_CAPTUREFEATUREINFERENCE_H_
