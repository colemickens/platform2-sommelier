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

#define PIPE_CLASS_TAG "Inference"
#define PIPE_TRACE TRACE_CAPTURE_FEATURE_INFER
#include <capture/CaptureFeature_Common.h>
#include <capture/CaptureFeatureInference.h>
#include <capture/CaptureFeatureNode.h>
#include <common/include/DebugControl.h>
#include <common/include/PipeLog.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using NSCam::Utils::Format::queryImageFormatName;

// #define __DEBUG

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

CaptureFeatureInferenceData::CaptureFeatureInferenceData()
    : mDataCount(0),
      mRequestIndex(0),
      mRequestCount(1),
      mPipeBufferCounter(0),
      mRequiredFD(MFALSE),
      mPerFrameFD(MFALSE) {
  memset(&mInferredItems, NULL_TYPE, sizeof(mInferredItems));
  memset(&mNodeInput, -1, sizeof(mNodeInput));
  memset(&mNodeOutput, -1, sizeof(mNodeOutput));
}

MVOID CaptureFeatureInferenceData::addNodeIO(
    NodeID_T nodeId,
    const std::vector<SrcData>& vSrcData,
    const std::vector<DstData>& vDstData,
    const std::vector<MetadataID_T>& vMetadata,
    const std::vector<FeatureID_T>& vFeature,
    MBOOL forced) {
  MUINT8 index;
  std::bitset<64> bitFeatures;
  for (auto src : vSrcData) {
    if (mInferredItems[src.mTypeId] == NULL_TYPE) {
      continue;
    }

    index = mInferredItems[src.mTypeId];
    DataItem& item = mDataItems[index];
    // the first user decides the format & size
    if (src.mFormat != 0) {
      item.mFormat = src.mFormat;
    }
    item.mSizeId = src.mSizeId;
    if (src.mSize != MSize(0, 0)) {
      item.mSize = src.mSize;
    }
    item.markReference(nodeId);
    bitFeatures |= item.mFeatures;
  }

  for (FeatureID_T featId : vFeature) {
    bitFeatures.set((bitFeatures.size() - 1) - featId);
  }

  mNodeMeta[nodeId] = vMetadata;

  for (auto dst : vDstData) {
    MINT redirect = -1;
    if (dst.mInPlace) {
      redirect = mInferredItems[dst.mTypeId];
      // Recursive
      DataItem& item = mDataItems[redirect];
      if (item.mRedirect >= 0) {
        redirect = item.mRedirect;
      }
    }
    index = addDataItem(nodeId, dst.mTypeId, NULL_BUFFER, bitFeatures);
    DataItem& item = mDataItems[index];
    item.mFormat = dst.mFormat;
    item.mSize = dst.mSize;
    item.mSizeId = dst.mSizeId;
    if (dst.mInPlace) {
      item.mRedirect = redirect;
    }
  }

  if (forced) {
    mNodeUsed.set((mNodeUsed.size() - 1) - nodeId);
  }
}

MSize CaptureFeatureInferenceData::getSize(TypeID_T typeId) {
  MUINT8 index = mInferredItems[typeId];
  DataItem& item = mDataItems[index];

  return item.mSize;
}

MVOID CaptureFeatureInferenceData::addSource(TypeID_T typeId,
                                             BufferID_T bufId,
                                             Format_T fmt,
                                             MSize size) {
  MUINT8 index = addDataItem(NID_ROOT, typeId, bufId);
  DataItem& item = mDataItems[index];
  item.mFormat = fmt;
  item.mSize = size;
}

MVOID CaptureFeatureInferenceData::addTarget(TypeID_T typeId,
                                             BufferID_T bufId) {
  int index = -1;
  size_t featureSize = 0;
  for (int i = 0; i < mDataCount; i++) {
    DataItem& item = mDataItems[i];
    if (item.mTypeId != typeId) {
      continue;
    }

    if (item.mReferences.to_ulong() != 0) {
      continue;
    }

    if (index == -1 || item.mFeatures.count() > featureSize) {
      featureSize = item.mFeatures.count();
      index = i;
    }
  }

  if (index != -1) {
    DataItem& item = mDataItems[index];
    item.markReference(NID_ROOT);
    item.mSizeId = SID_ARBITRARY;
    item.mBufferId = bufId;
  }
}

MUINT8 CaptureFeatureInferenceData::addDataItem(NodeID_T nodeId,
                                                TypeID_T typeId,
                                                BufferID_T bufId,
                                                std::bitset<64> features) {
  DataItem& item = mDataItems[mDataCount];
  item.mNodeId = nodeId;
  item.mTypeId = typeId;
  item.mFeatures |= features;
  item.mBufferId = bufId;

  mInferredType.set((mInferredType.size() - 1) - typeId);
  // update the latest reference
  mInferredItems[typeId] = mDataCount;
  return mDataCount++;
}

MVOID CaptureFeatureInferenceData::dump() {
  std::string strFeature;
  std::string strReference;
  std::string strInput;
  std::string strOutput;
  for (int i = 0; i < mDataCount; i++) {
    DataItem& item = mDataItems[i];

#ifndef __DEBUG
    if (item.mReferences.none()) {
      continue;
    }
#endif
    for (NodeID_T nodeId = 0; nodeId < NUM_OF_NODE; nodeId++) {
      if (item.mReferences.test((item.mReferences.size() - 1) - nodeId)) {
        if (strReference.length() > 0) {
          strReference += ",";
        }
        strReference += NodeID2Name(nodeId);
      }
    }

    for (FeatureID_T featId = 0; featId < NUM_OF_FEATURE; featId++) {
      if (item.mFeatures.test((item.mFeatures.size() - 1) - featId)) {
        if (strFeature.length() > 0) {
          strFeature += ",";
        }
        strFeature += FeatID2Name(featId);
      }
    }

    MY_LOGD(
        "item[%d] node:[%s] buffer:[%d] type:[%s] feature:[%s] referred:[%s] "
        "size:[%s%s] format:[%s]%s",
        i, NodeID2Name(item.mNodeId), item.mBufferId, TypeID2Name(item.mTypeId),
        strFeature.c_str(), strReference.c_str(), SizeID2Name(item.mSizeId),
        (item.mSize != MSize(0, 0))
            ? base::StringPrintf("(%dx%d)", item.mSize.w, item.mSize.h).c_str()
            : "",
        (item.mFormat) ? queryImageFormatName(item.mFormat) : "",
        (item.mRedirect >= 0)
            ? base::StringPrintf(" redirect:[%d]", item.mRedirect).c_str()
            : "");

    strFeature.clear();
    strReference.clear();
  }
#ifdef __DEBUG
  for (PathID_T pathId = 0; pathId < NUM_OF_PATH; pathId++) {
    if (mPathUsed.test((mPathUsed.size() - 1) - pathId)) {
      MY_LOGD("path: %s", PathID2Name(pathId));
    }
  }

  for (NodeID_T nodeId = NID_ROOT + 1; nodeId < NUM_OF_NODE; nodeId++) {
    if (!mNodeUsed.test((mNodeUsed.size() - 1) - nodeId)) {
      continue;
    }

    strInput.clear();
    strOutput.clear();
    for (TypeID_T typeId = 0; typeId < NUM_OF_TYPE; typeId++) {
      if (mNodeInput[nodeId][typeId] >= 0) {
        if (strInput.length() > 0) {
          strInput += ",";
        }
        strInput += base::StringPrintf("%d", mNodeInput[nodeId][typeId]);
      }
    }

    for (TypeID_T typeId = 0; typeId < NUM_OF_TYPE; typeId++) {
      if (mNodeOutput[nodeId][typeId] >= 0) {
        if (strOutput.length() > 0) {
          strOutput += ",";
        }
        strOutput += base::StringPrintf("%d", mNodeOutput[nodeId][typeId]);
      }
    }

    MY_LOGD("node:[%s] input:[%s] output:[%s]", NodeID2Name(nodeId),
            strInput.string(), strOutput.string());
  }
#endif
}

MVOID CaptureFeatureInferenceData::determine(
    std::shared_ptr<CaptureFeatureRequest> pRequest) {
  PathID_T pathId;
  NodeID_T nodeId;
  TypeID_T typeId;
  std::bitset<32> tmp;

  CaptureFeatureRequest& req = *(pRequest.get());

  for (size_t i = 0; i < mDataCount; i++) {
    DataItem& item = mDataItems[i];
    if (item.mReferences.none()) {
      continue;
    }

    // node output
    mNodeOutput[item.mNodeId][item.mTypeId] = i;
    mNodeUsed.set((mNodeUsed.size() - 1) - item.mNodeId);
    // working buffer id
    if (item.mBufferId == NULL_BUFFER) {
      // In-place processing
      if (item.mRedirect >= 0) {
        DataItem& redirect = mDataItems[item.mRedirect];
        item.mBufferId = redirect.mBufferId;
      } else {
        item.mBufferId = PIPE_BUFFER_STARTER | (mPipeBufferCounter++);
        req.addPipeBuffer(item.mBufferId, item.mTypeId, item.mSize,
                          item.mFormat);
      }
    }

    // node input
    tmp = item.mReferences;
    do {
      uint32_t value = static_cast<uint32_t>(tmp.to_ulong());
      nodeId = __builtin_clz(value);         // tmp.firstMarkedBit();
      tmp.reset((tmp.size() - 1) - nodeId);  // tmp.clearFirstMarkedBit();

      mNodeInput[nodeId][item.mTypeId] = i;
      // there is no path for repeating request
      auto revertRepeatNode = [](NodeID_T nodeId) -> NodeID_T {
        if (nodeId == NID_YUV_R1 || nodeId == NID_YUV_R2) {
          return NID_YUV;
        }
        if (nodeId == NID_YUV2_R1 || nodeId == NID_YUV2_R2) {
          return NID_YUV2;
        }
        return nodeId;
      };
      NodeID_T itemFrom = revertRepeatNode(item.mNodeId);
      NodeID_T itemTo = revertRepeatNode(nodeId);
      pathId = FindPath(itemFrom, itemTo);
      if (pathId != NULL_PATH) {
        mPathUsed.set((mPathUsed.size() - 1) - pathId);
      }
    } while (!tmp.none());
  }

  for (pathId = 0; pathId < NUM_OF_PATH; pathId++) {
    if (mPathUsed.test((mPathUsed.size() - 1) - pathId)) {
      const NodeID_T* pNodeId = GetPath(pathId);
      NodeID_T src = pNodeId[0];
      NodeID_T dst = pNodeId[1];
      if (mNodeUsed.test((mNodeUsed.size() - 1) - src) &&
          mNodeUsed.test((mNodeUsed.size() - 1) - dst)) {
        req.addPath(pathId);
      } else {
        mPathUsed.reset((mPathUsed.size() - 1) - pathId);
      }
    }
  }

  std::vector<BufferID_T> vInBufIDs;
  std::vector<BufferID_T> vOutBufIDs;
  int index;
  for (nodeId = NID_ROOT + 1; nodeId < NUM_OF_NODE; nodeId++) {
    if (!mNodeUsed.test((mNodeUsed.size() - 1) - nodeId)) {
      continue;
    }

    vInBufIDs.clear();
    vOutBufIDs.clear();

    for (typeId = 0; typeId < NUM_OF_TYPE; typeId++) {
      index = mNodeInput[nodeId][typeId];
      if (index >= 0) {
        DataItem& item = mDataItems[index];
        vInBufIDs.push_back(item.mBufferId);
      }
    }

    for (typeId = 0; typeId < NUM_OF_TYPE; typeId++) {
      index = mNodeOutput[nodeId][typeId];
      if (index >= 0) {
        DataItem& item = mDataItems[index];
        // check the buffer which is referred by a involved node
        if (item.mReferences.to_ulong() & mNodeUsed.to_ulong()) {
          vOutBufIDs.push_back(item.mBufferId);
        }
      }
    }
    req.addNodeIO(nodeId, vInBufIDs, vOutBufIDs, mNodeMeta[nodeId]);
  }
}

CaptureFeatureInference::CaptureFeatureInference() {}

MVOID CaptureFeatureInference::addNode(
    NodeID_T nodeId, std::shared_ptr<CaptureFeatureNode> pNode) {
  mNodeMap.emplace(nodeId, pNode);
}

MERROR CaptureFeatureInference::evaluate(
    std::shared_ptr<CaptureFeatureRequest> pRequest) {
  Timer timer;
  timer.start();

  CaptureFeatureRequest& rRequest = *(pRequest.get());

  auto getMetaPtr = [&](MetadataID_T metaId) -> std::shared_ptr<IMetadata> {
    auto pHandle = rRequest.getMetadata(metaId);
    if (pHandle == NULL) {
      return NULL;
    }

    IMetadata* pMetatada = pHandle->native();
    if (pMetatada == NULL) {
      return NULL;
    }

    return std::make_shared<IMetadata>(*pMetatada);
  };

  CaptureFeatureInferenceData data;
  data.mpIMetadataHal = getMetaPtr(MID_MAIN_IN_HAL);
  data.mpIMetadataApp = getMetaPtr(MID_MAIN_IN_APP);
  data.mpIMetadataDynamic = getMetaPtr(MID_MAIN_IN_P1_DYNAMIC);

  data.mFeatures = rRequest.mFeatures;
  if (pRequest->getParameter(PID_FRAME_INDEX) >= 0) {
    data.mRequestIndex = pRequest->getParameter(PID_FRAME_INDEX);
  }
  if (pRequest->getParameter(PID_FRAME_COUNT) >= 0) {
    data.mRequestCount = pRequest->getParameter(PID_FRAME_COUNT);
  }

  auto addSource = [&](BufferID_T bufId, TypeID_T typeId) {
    if (rRequest.mBufferMap.find(bufId) != rRequest.mBufferMap.end()) {
      auto pBufHandle = rRequest.getBuffer(bufId);
      auto pImgBuf = pBufHandle->native();
      data.addSource(typeId, bufId, pImgBuf->getImgFormat(),
                     pImgBuf->getImgSize());
    }
  };

  auto addTarget = [&](BufferID_T bufId, TypeID_T typeId) {
    if (rRequest.mBufferMap.find(bufId) != rRequest.mBufferMap.end()) {
      data.addTarget(typeId, bufId);
    }
  };

  // 1. add all given input buffers
  addSource(BID_MAIN_IN_YUV, TID_MAIN_FULL_YUV);
  addSource(BID_MAIN_IN_FULL, TID_MAIN_FULL_RAW);
  addSource(BID_MAIN_IN_RSZ, TID_MAIN_RSZ_RAW);
  addSource(BID_MAIN_IN_LCS, TID_MAIN_LCS);
  addSource(BID_SUB_IN_FULL, TID_SUB_FULL_RAW);
  addSource(BID_SUB_IN_RSZ, TID_SUB_RSZ_RAW);
  addSource(BID_SUB_IN_LCS, TID_SUB_LCS);

  // 2. inference all possible outputs
  for (NodeID_T nodeId = NID_ROOT + 1; nodeId < NUM_OF_NODE; nodeId++) {
    if (mNodeMap.find(nodeId) != mNodeMap.end()) {
      mNodeMap.at(nodeId)->evaluate(&data);
    }
  }
  // 2-1. Refine the request's feature, probably be ignored by plugin
  // negotiation
  rRequest.mFeatures = data.mFeatures;

  // 3. add output buffers
  addTarget(BID_MAIN_OUT_JPEG, TID_JPEG);
  addTarget(BID_MAIN_OUT_THUMBNAIL, TID_THUMBNAIL);
  addTarget(BID_MAIN_OUT_POSTVIEW, TID_POSTVIEW);
  addTarget(BID_MAIN_OUT_YUV00, TID_MAIN_CROP1_YUV);
  addTarget(BID_MAIN_OUT_YUV01, TID_MAIN_CROP2_YUV);

  // 4. determin final pathes, which contain all node's input and output
  data.determine(pRequest);

  timer.stop();
  MY_LOGI("timeconsuming: %d ms", timer.getElapsed());

  data.dump();

  return OK;
}

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
