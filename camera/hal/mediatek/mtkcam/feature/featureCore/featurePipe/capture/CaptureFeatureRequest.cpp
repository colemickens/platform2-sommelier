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

#include <algorithm>
#include <utility>
#include <common/include/DebugControl.h>
#define PIPE_CLASS_TAG "Request"
#define PIPE_TRACE TRACE_CAPTURE_FEATURE_DATA
#include <capture/CaptureFeatureRequest.h>
#include <capture/CaptureFeature_Common.h>
#include <common/include/PipeLog.h>
#include <memory>
#include <vector>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

CaptureFeatureRequest::CaptureFeatureRequest() {
  clear();
}

CaptureFeatureRequest::~CaptureFeatureRequest() {}

MVOID CaptureFeatureRequest::addBuffer(
    BufferID_T bufId, std::shared_ptr<BufferHandle> pBufHandle) {
  mBufferMap.emplace(bufId, pBufHandle);
  TypeID_T typeId = NULL_TYPE;
  switch (bufId) {
    case BID_MAN_IN_FULL:
      typeId = TID_MAN_FULL_RAW;
      break;
    case BID_MAN_IN_YUV:
      typeId = TID_MAN_FULL_YUV;
      break;
    case BID_MAN_IN_LCS:
      typeId = TID_MAN_LCS;
      break;
    case BID_MAN_IN_RSZ:
      typeId = TID_MAN_RSZ_RAW;
      break;
    case BID_MAN_OUT_YUV00:
      typeId = TID_MAN_CROP1_YUV;
      break;
    case BID_MAN_OUT_YUV01:
      typeId = TID_MAN_CROP2_YUV;
      break;
    case BID_MAN_OUT_JPEG:
      typeId = TID_JPEG;
      break;
    case BID_MAN_OUT_THUMBNAIL:
      typeId = TID_THUMBNAIL;
      break;
    case BID_MAN_OUT_POSTVIEW:
      typeId = TID_POSTVIEW;
      break;
    case BID_MAN_OUT_DEPTH:
      typeId = TID_MAN_DEPTH;
      break;
    case BID_MAN_OUT_CLEAN:
      // TODO(MTK)
      break;
    case BID_SUB_IN_FULL:
      typeId = TID_SUB_FULL_RAW;
      break;
    case BID_SUB_IN_LCS:
      typeId = TID_SUB_LCS;
      break;
    case BID_SUB_IN_RSZ:
      typeId = TID_SUB_RSZ_RAW;
      break;
    case BID_SUB_OUT_YUV00:
      // TODO(MTK)
      break;
    case BID_SUB_OUT_YUV01:
      // TODO(MTK)
      break;
    default:
      MY_LOGE("unknown buffer id: %d", bufId);
  }

  BufferItem item;
  item.mTypeId = typeId;
  item.mCreated = MTRUE;
  mBufferItems.emplace(bufId, item);
}

std::shared_ptr<BufferHandle> CaptureFeatureRequest::getBuffer(
    BufferID_T bufId) {
  std::lock_guard<std::mutex> lock(mBufferMutex);
  std::shared_ptr<BufferHandle> pBuffer;

  // Should not call the cross request's getBuffer()
  auto spCrossRequest = mpCrossRequest.lock();
  if (spCrossRequest != nullptr) {
    if (spCrossRequest->mBufferMap.find(bufId) !=
        spCrossRequest->mBufferMap.end()) {
      pBuffer = spCrossRequest->mBufferMap.at(bufId);
      if (!spCrossRequest->mBufferItems.at(bufId).mAcquired) {
        pBuffer->acquire();
        spCrossRequest->mBufferItems.at(bufId).mAcquired = MTRUE;
      }
      return pBuffer;
    }
  }

  if (mBufferMap.find(bufId) == mBufferMap.end()) {
    return nullptr;
  }

  pBuffer = mBufferMap.at(bufId);

  if (!mBufferItems.at(bufId).mAcquired) {
    pBuffer->acquire();
    mBufferItems.at(bufId).mAcquired = MTRUE;
  }

  return pBuffer;
}

MVOID CaptureFeatureRequest::setCrossRequest(
    std::shared_ptr<CaptureFeatureRequest> pRequest) {
  mpCrossRequest = pRequest;
}

MVOID CaptureFeatureRequest::addPipeBuffer(BufferID_T bufId,
                                           TypeID_T typeId,
                                           MSize& size,
                                           Format_T fmt) {
  BufferItem item;
  item.mTypeId = typeId;
  item.mCreated = MFALSE;
  item.mSize = size;
  item.mFormat = fmt;
  mBufferItems.emplace(bufId, item);
}

MVOID CaptureFeatureRequest::addMetadata(
    MetadataID_T metaId, std::shared_ptr<MetadataHandle> pMetaHandle) {
  MetadataItem item;
  item.mpHandle = pMetaHandle;
  mMetadataItems.emplace(metaId, item);
}

std::shared_ptr<MetadataHandle> CaptureFeatureRequest::getMetadata(
    MetadataID_T metaId) {
  std::shared_ptr<MetadataHandle> pMetadata = nullptr;
  auto spCrossRequest = mpCrossRequest.lock();
  if (spCrossRequest != nullptr) {
    if (spCrossRequest->mMetadataItems.find(metaId) !=
        spCrossRequest->mMetadataItems.end()) {
      pMetadata = spCrossRequest->mMetadataItems.at(metaId).mpHandle;

      if (!spCrossRequest->mMetadataItems.at(metaId).mAcquired) {
        pMetadata->acquire();
        spCrossRequest->mMetadataItems.at(metaId).mAcquired = MTRUE;
      }
      return pMetadata;
    }
  }

  if (mMetadataItems.find(metaId) == mMetadataItems.end()) {
    return nullptr;
  }

  pMetadata = mMetadataItems.at(metaId).mpHandle;

  if (!mMetadataItems.at(metaId).mAcquired) {
    pMetadata->acquire();
    mMetadataItems.at(metaId).mAcquired = MTRUE;
  }
  return pMetadata;
}

MVOID CaptureFeatureRequest::addParameter(ParameterID_T paramId, MINT32 value) {
  mParameter[paramId] = value;
}

MINT32 CaptureFeatureRequest::getParameter(ParameterID_T paramId) {
  return mParameter[paramId];
}

MINT32 CaptureFeatureRequest::getRequestNo() {
  return mParameter[PID_REQUEST_NUM];
}

MINT32 CaptureFeatureRequest::getFrameNo() {
  return mParameter[PID_FRAME_NUM];
}

MVOID CaptureFeatureRequest::addPath(PathID_T pathId) {
  const NodeID_T* nodeId = GetPath(pathId);
  NodeID_T src = nodeId[0];
  NodeID_T dst = nodeId[1];

  mNodePath[src][dst] = pathId;

  mTraverse.set((mTraverse.size() - 1) - pathId);
}

MVOID CaptureFeatureRequest::traverse(PathID_T pathId) {
  mTraverse.reset((mTraverse.size() - 1) - pathId);
}

MBOOL CaptureFeatureRequest::isTraversed() {
  return mTraverse.none();
}

MBOOL CaptureFeatureRequest::isSatisfied(NodeID_T nodeId) {
  for (NodeID_T i = 0; i < NUM_OF_NODE; i++) {
    if (mNodePath[i][nodeId] == NULL_PATH) {
      continue;
    }

    PathID_T pathId = FindPath(i, nodeId);
    if (pathId == NULL_PATH) {
      continue;
    }

    if (mTraverse.test((mTraverse.size() - 1) - pathId)) {
      return MFALSE;
    }
  }

  return MTRUE;
}

std::vector<NodeID_T> CaptureFeatureRequest::getPreviousNodes(NodeID_T nodeId) {
  std::vector<NodeID_T> vec;
  for (int i = 0; i < NUM_OF_NODE; i++) {
    if (mNodePath[i][nodeId] != NULL_PATH) {
      vec.push_back(i);
    }
  }
  return vec;
}

std::vector<NodeID_T> CaptureFeatureRequest::getNextNodes(NodeID_T nodeId) {
  std::vector<NodeID_T> vec;
  for (int i = 0; i < NUM_OF_NODE; i++) {
    if (mNodePath[nodeId][i] != NULL_PATH) {
      vec.push_back(i);
    }
  }
  return vec;
}

MVOID CaptureFeatureRequest::addFeature(FeatureID_T fid) {
  mFeatures.set((mFeatures.size() - 1) - fid);
}

MVOID CaptureFeatureRequest::setFeatures(MUINT64 features) {
  mFeatures = features;
}

MBOOL CaptureFeatureRequest::hasFeature(FeatureID_T fid) {
  return mFeatures.test((mFeatures.size() - 1) - fid);
}

MVOID CaptureFeatureRequest::clear() {
  memset(mNodePath, NULL_PATH, sizeof(mNodePath));
  std::fill_n(mParameter, NUM_OF_TOTAL_PARAMETER, -1);
}

MVOID CaptureFeatureRequest::addNodeIO(NodeID_T nodeId,
                                       std::vector<BufferID_T>& vInBufId,
                                       std::vector<BufferID_T>& vOutBufId,
                                       std::vector<MetadataID_T>& vMetaId) {
  auto pNodeRequest =
      std::make_shared<CaptureFeatureNodeRequest>(shared_from_this());

  for (BufferID_T bufId : vInBufId) {
    if (mBufferItems.find(bufId) == mBufferItems.end()) {
      MY_LOGE("can not find buffer, id:%d", bufId);
      continue;
    }

    TypeID_T typeId = mBufferItems.at(bufId).mTypeId;

    pNodeRequest->mIBufferMap.emplace(typeId, bufId);
    mBufferItems.at(bufId).mReference++;
  }

  for (BufferID_T bufId : vOutBufId) {
    if (mBufferItems.find(bufId) == mBufferItems.end()) {
      MY_LOGE("can not find buffer, id:%d", bufId);
      continue;
    }

    TypeID_T typeId = mBufferItems.at(bufId).mTypeId;

    pNodeRequest->mOBufferMap.emplace(typeId, bufId);
    mBufferItems.at(bufId).mReference++;
  }

  for (MetadataID_T metaId : vMetaId) {
    if (mMetadataItems.find(metaId) == mMetadataItems.end()) {
      continue;
    }

    pNodeRequest->mMetadataSet.set((pNodeRequest->mMetadataSet.size() - 1) -
                                   metaId);
    mMetadataItems.at(metaId).mReference++;
  }

  mNodeRequest.emplace(nodeId, pNodeRequest);
}

MVOID CaptureFeatureRequest::decBufferRef(BufferID_T bufId) {
  std::lock_guard<std::mutex> lock(mBufferMutex);
  if (mBufferItems.find(bufId) != mBufferItems.end()) {
    MUINT32& ref = mBufferItems.at(bufId).mReference;

    if (--ref <= 0) {
      std::shared_ptr<BufferHandle> pBuffer = mBufferMap.at(bufId);
      pBuffer->release();
      mBufferMap.erase(bufId);
    }
  }
}

MVOID CaptureFeatureRequest::decMetadataRef(MetadataID_T metaId) {
  std::lock_guard<std::mutex> lock(mMetadataMutex);
  if (mMetadataItems.find(metaId) != mMetadataItems.end()) {
    MUINT32& ref = mMetadataItems.at(metaId).mReference;

    if (--ref <= 0) {
      auto& item = mMetadataItems.at(metaId);
      std::shared_ptr<MetadataHandle> pHandle = item.mpHandle;
      pHandle->release();
      item.mpHandle = nullptr;
    }
  }
}

std::shared_ptr<CaptureFeatureNodeRequest>
CaptureFeatureRequest::getNodeRequest(NodeID_T nodeId) {
  if (mNodeRequest.find(nodeId) != mNodeRequest.end()) {
    return mNodeRequest.at(nodeId);
  }

  return nullptr;
}

MVOID CaptureFeatureRequest::decNodeReference(NodeID_T nodeId) {
  if (mNodeRequest.find(nodeId) == mNodeRequest.end()) {
    return;
  }
  std::shared_ptr<CaptureFeatureNodeRequest> pNodeReq = mNodeRequest.at(nodeId);
  auto& rIBufferMap = pNodeReq->mIBufferMap;
  auto& rOBufferMap = pNodeReq->mOBufferMap;

  for (auto& it : rIBufferMap) {
    BufferID_T bufId = it.second;
    decBufferRef(bufId);
  }

  for (auto& it : rOBufferMap) {
    BufferID_T bufId = it.second;
    decBufferRef(bufId);
  }

  for (MetadataID_T i = 0; i < NUM_OF_METADATA; i++) {
    if (pNodeReq->hasMetadata(i))
      decMetadataRef(i);
  }
  return;
}

MVOID CaptureFeatureRequest::dump() {
  for (NodeID_T n1 = 0; n1 < NUM_OF_NODE; n1++)
    for (NodeID_T n2 = 0; n2 < NUM_OF_NODE; n2++)
      if (mNodePath[n1][n2] != NULL_PATH) {
        MY_LOGD("path: %s", PathID2Name(mNodePath[n1][n2]));
      }

  for (auto& it : mNodeRequest) {
    NodeID_T nodeId = it.first;
    const std::shared_ptr<CaptureFeatureNodeRequest> pNodeReq = it.second;

    MY_LOGD("node:[%s]", NodeID2Name(nodeId));
    auto& rIBufferMap = pNodeReq->mIBufferMap;
    auto& rOBufferMap = pNodeReq->mOBufferMap;
    for (auto& it : rIBufferMap) {
      TypeID_T typeId = it.first;
      BufferID_T bufId = it.second;
      const std::shared_ptr<BufferHandle> pBuffer = this->getBuffer(bufId);

      MY_LOGD("  type[%s] handle[%p]", TypeID2Name(typeId), pBuffer.get());
    }

    for (auto& it : rOBufferMap) {
      TypeID_T typeId = it.first;
      BufferID_T bufId = it.second;
      const std::shared_ptr<BufferHandle> pBuffer = this->getBuffer(bufId);

      MY_LOGD("   type[%s] handle[%p]", TypeID2Name(typeId), pBuffer.get());
    }
  }
}

BufferID_T CaptureFeatureNodeRequest::mapBufferID(TypeID_T typeId,
                                                  Direction dir) {
  BufferID_T bufId = NULL_BUFFER;
  if (dir == INPUT) {
    if (mIBufferMap.find(typeId) != mIBufferMap.end()) {
      bufId = mIBufferMap.at(typeId);
    }
  } else {
    if (mOBufferMap.find(typeId) != mOBufferMap.end()) {
      bufId = mOBufferMap.at(typeId);
    }
  }

  return bufId;
}

MBOOL CaptureFeatureNodeRequest::hasMetadata(MetadataID_T metaId) {
  return mMetadataSet.test((mMetadataSet.size() - 1) - metaId);
}

IImageBuffer* CaptureFeatureNodeRequest::acquireBuffer(BufferID_T bufId) {
  if (bufId == NULL_BUFFER) {
    return NULL;
  }

  auto pBufferHandle = mpRequest->getBuffer(bufId);
  if (pBufferHandle == nullptr) {
    return NULL;
  }

  return pBufferHandle->native();
}

MVOID CaptureFeatureNodeRequest::releaseBuffer(BufferID_T bufId) {
  mpRequest->decBufferRef(bufId);
}

IMetadata* CaptureFeatureNodeRequest::acquireMetadata(MetadataID_T metaId) {
  if (metaId == NULL_METADATA) {
    return NULL;
  }

  if (!hasMetadata(metaId)) {
    return NULL;
  }

  std::shared_ptr<MetadataHandle> pMetaHandle = mpRequest->getMetadata(metaId);
  if (pMetaHandle == nullptr) {
    return NULL;
  }

  return pMetaHandle->native();
}

MVOID CaptureFeatureNodeRequest::releaseMetadata(MetadataID_T metaId) {
  if (!hasMetadata(metaId)) {
    return;
  }

  mpRequest->decMetadataRef(metaId);
}

MUINT32 CaptureFeatureNodeRequest::getImageTransform(BufferID_T bufId) const {
  auto pBuffer = mpRequest->getBuffer(bufId);
  if (pBuffer == nullptr) {
    return 0;
  }

  return pBuffer->getTransform();
}

MINT CaptureFeatureRequest::getImageFormat(BufferID_T bufId) {
  if (mBufferItems.find(bufId) == mBufferItems.end()) {
    MY_LOGE("can not find buffer ID:%d", bufId);
    return 0;
  }

  if (bufId & PIPE_BUFFER_STARTER) {
    return mBufferItems.at(bufId).mFormat;
  } else {
    auto pBufferHandle = this->getBuffer(bufId);
    if (pBufferHandle == nullptr) {
      return 0;
    }
    return pBufferHandle->native()->getImgFormat();
  }
}

MSize CaptureFeatureRequest::getImageSize(BufferID_T bufId) {
  if (mBufferItems.find(bufId) == mBufferItems.end()) {
    MY_LOGE("can not find buffer ID:%d", bufId);
    return MSize(0, 0);
  }

  if (bufId & PIPE_BUFFER_STARTER) {
    return mBufferItems.at(bufId).mSize;
  } else {
    auto pBufferHandle = this->getBuffer(bufId);
    if (pBufferHandle == nullptr) {
      return MSize(0, 0);
    }
    return pBufferHandle->native()->getImgSize();
  }
}

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
