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

#define LOG_TAG "MtkCam/pipeline"
//
#include "MyUtils.h"
#include <list>
#include <memory>
#include <mtkcam/pipeline/pipeline/PipelineBufferSetFrameControlImp.h>
#include <string>
#include <vector>

//
using NSCam::v3::IImageStreamBuffer;
using NSCam::v3::IMetaStreamBuffer;
using NSCam::v3::IPipelineBufferSetFrameControl;
using NSCam::v3::IPipelineDAG;
using NSCam::v3::IPipelineNodeCallback;
using NSCam::v3::IPipelineNodeMap;
using NSCam::v3::IStreamBufferSet;
using NSCam::v3::IStreamInfoSet;
using NSCam::v3::IUsersManager;
using NSCam::v3::NSPipelineBufferSetFrameControlImp::IMyMap;
using NSCam::v3::NSPipelineBufferSetFrameControlImp::
    PipelineBufferSetFrameControlImp;

/******************************************************************************
 *
 ******************************************************************************/
static int64_t getDurationInUS(struct timespec const& t1,
                               struct timespec const& t2) {
  struct timespec diff;
  if (t2.tv_nsec - t1.tv_nsec < 0) {
    diff.tv_sec = t2.tv_sec - t1.tv_sec - 1;
    diff.tv_nsec = t2.tv_nsec - t1.tv_nsec + 1000000000;
  } else {
    diff.tv_sec = t2.tv_sec - t1.tv_sec;
    diff.tv_nsec = t2.tv_nsec - t1.tv_nsec;
  }
  return (diff.tv_sec * 1000000.0 + diff.tv_nsec / 1000.0);
}

/******************************************************************************
 *
 ******************************************************************************/
static std::string getFrameLifetimeLog(struct timespec const& start,
                                       struct timespec const& end) {
  std::string os;
  auto pLogTool = NSCam::Utils::LogTool::get();
  if (CC_LIKELY(pLogTool)) {
    os += "{";
    os += pLogTool->convertToFormattedLogTime(&start);
    if (0 != end.tv_sec || 0 != end.tv_nsec) {
      os += " -> ";
      os += pLogTool->convertToFormattedLogTime(&end);
      os += " (";
      os += std::to_string(getDurationInUS(start, end));
      os += "us)";
    }
    os += "}";
  }
  return os;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IPipelineBufferSetFrameControl>
IPipelineBufferSetFrameControl::create(
    MUINT32 requestNo,
    MUINT32 frameNo,
    MBOOL bReporcessFrame,
    std::weak_ptr<IAppCallback> const& pAppCallback,
    std::shared_ptr<IPipelineStreamBufferProvider> pBufferProvider,
    std::weak_ptr<IPipelineNodeCallback> pNodeCallback) {
  if (pNodeCallback.expired()) {
    MY_LOGE("IPipelineNodeCallback should not be NULL!");
    return nullptr;
  }
  auto ptr = std::make_shared<PipelineBufferSetFrameControlImp>(
      requestNo, frameNo, bReporcessFrame, pAppCallback, pBufferProvider,
      pNodeCallback);
  return ptr;
}

/******************************************************************************
 *
 ******************************************************************************/
PipelineBufferSetFrameControlImp::PipelineBufferSetFrameControlImp(
    MUINT32 requestNo,
    MUINT32 frameNo,
    MBOOL bReporcessFrame,
    std::weak_ptr<IAppCallback> const& pAppCallback,
    std::shared_ptr<IPipelineStreamBufferProvider> pBufferProvider,
    std::weak_ptr<IPipelineNodeCallback> pNodeCallback)
    : mFrameNo(frameNo),
      mRequestNo(requestNo),
      mbReprocessFrame(bReporcessFrame),
      mpAppCallback(pAppCallback)
      //
      ,
      mBufferProvider(pBufferProvider),
      mpPipelineCallback(pNodeCallback),
      mpStreamInfoSet(0),
      mpNodeMap(0),
      mpPipelineDAG(0)
      //
      ,
      mpReleasedCollector(std::make_shared<ReleasedCollector>()),
      mpItemMap_AppImage(
          std::make_shared<ItemMap_AppImageT>(mpReleasedCollector)),
      mpItemMap_AppMeta(
          std::make_shared<ItemMap_AppMetaT>(mpReleasedCollector)),
      mpItemMap_HalImage(
          std::make_shared<ItemMap_HalImageT>(mpReleasedCollector)),
      mpItemMap_HalMeta(
          std::make_shared<ItemMap_HalMetaT>(mpReleasedCollector)) {
  NSCam::Utils::LogTool::get()->getCurrentLogTime(&mTimestampFrameCreated);
  ::memset(&mTimestampFrameDone, 0, sizeof(mTimestampFrameDone));
  pthread_rwlock_init(&mRWLock, NULL);
}

PipelineBufferSetFrameControlImp::~PipelineBufferSetFrameControlImp() {
  MY_LOGD("deconstruction");
  onLastStrongRef(nullptr);
  pthread_rwlock_destroy(&mRWLock);
}

/******************************************************************************
 *
 ******************************************************************************/
void PipelineBufferSetFrameControlImp::onLastStrongRef(const void* /*id*/) {
  if (CC_UNLIKELY((0 != mpItemMap_AppImage->mNonReleasedNum) ||
                  (0 != mpItemMap_AppMeta->mNonReleasedNum) ||
                  (0 != mpItemMap_HalImage->mNonReleasedNum) ||
                  (0 != mpItemMap_HalMeta->mNonReleasedNum))) {
    MY_LOGW(
        "buffers are not completely released: #(AppImage, AppMeta, HalImage, "
        "HalMeta)=(%zd %zd %zd %zd)",
        mpItemMap_AppImage->mNonReleasedNum, mpItemMap_AppMeta->mNonReleasedNum,
        mpItemMap_HalImage->mNonReleasedNum,
        mpItemMap_HalMeta->mNonReleasedNum);

    auto printMap = [](auto const& map) {
      for (size_t i = 0; i < map.size(); i++) {
        auto const& pItem = map.itemAt(i);
        if (CC_LIKELY(pItem != nullptr && pItem->getUsersManager())) {
          pItem->getUsersManager()->dumpState();
        }
      }
    };

    printMap(*mpItemMap_AppMeta);
    printMap(*mpItemMap_HalMeta);
    printMap(*mpItemMap_AppImage);
    printMap(*mpItemMap_HalImage);
  }

  std::shared_ptr<IAppCallback> pAppCallback = mpAppCallback.lock();
  if (CC_UNLIKELY(pAppCallback == nullptr)) {
    MY_LOGW("Cannot promote AppCallback for requestNo:%u frameNo:%u",
            getRequestNo(), getFrameNo());
  } else {
    MY_LOGD("requestNo:%u frameNo:%u frame end", getRequestNo(), getFrameNo());
    IAppCallback::Result result = {
        .frameNo = getFrameNo(),
        .nAppOutMetaLeft = 0,
        .vAppOutMeta = std::vector<std::shared_ptr<IMetaStreamBuffer> >(),
        .nHalOutMetaLeft = 0,
        .vHalOutMeta = std::vector<std::shared_ptr<IMetaStreamBuffer> >(),
        .bFrameEnd = true};
    pAppCallback->updateFrame(getRequestNo(), 0, result);
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineBufferSetFrameControlImp::attachListener(
    std::weak_ptr<IPipelineFrameListener> const& pListener, MVOID* pCookie) {
  pthread_rwlock_wrlock(&mRWLock);
  //
  mListeners.push_back(MyListener(pListener, pCookie));
  pthread_rwlock_unlock(&mRWLock);
  //
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IPipelineNodeMap const>
PipelineBufferSetFrameControlImp::getPipelineNodeMap() const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  std::shared_ptr<IPipelineNodeMap const> p = mpPipelineNodeMap.lock();
  //
  MY_LOGE_IF(
      p == nullptr,
      "requestNo:%u frameNo:%u Bad PipelineNodeMap: wp expired %d promote:%p - "
      "%s",
      getRequestNo(), getFrameNo(), mpPipelineNodeMap.expired(), p.get(),
      getFrameLifetimeLog(mTimestampFrameCreated, mTimestampFrameDone).c_str());
  pthread_rwlock_unlock(&mRWLock);
  //
  return p;
}

/******************************************************************************
 *
 ******************************************************************************/
IPipelineDAG const& PipelineBufferSetFrameControlImp::getPipelineDAG() const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  MY_LOGE_IF(
      mpPipelineDAG == nullptr, "requestNo:%u frameNo:%u NULL PipelineDAG - %s",
      getRequestNo(), getFrameNo(),
      getFrameLifetimeLog(mTimestampFrameCreated, mTimestampFrameDone).c_str());
  pthread_rwlock_unlock(&mRWLock);
  return *mpPipelineDAG;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IPipelineDAG>
PipelineBufferSetFrameControlImp::getPipelineDAGSp() {
  pthread_rwlock_rdlock(&mRWLock);
  //
  MY_LOGE_IF(
      mpPipelineDAG == nullptr, "requestNo:%u frameNo:%u NULL PipelineDAG - %s",
      getRequestNo(), getFrameNo(),
      getFrameLifetimeLog(mTimestampFrameCreated, mTimestampFrameDone).c_str());
  pthread_rwlock_unlock(&mRWLock);
  return mpPipelineDAG;
}

/******************************************************************************
 *
 ******************************************************************************/
IStreamInfoSet const& PipelineBufferSetFrameControlImp::getStreamInfoSet()
    const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  MY_LOGE_IF(
      mpStreamInfoSet == nullptr,
      "requestNo:%u frameNo:%u NULL StreamInfoSet - %s", getRequestNo(),
      getFrameNo(),
      getFrameLifetimeLog(mTimestampFrameCreated, mTimestampFrameDone).c_str());
  pthread_rwlock_unlock(&mRWLock);
  return *mpStreamInfoSet;
}

/******************************************************************************
 *
 ******************************************************************************/
IStreamBufferSet& PipelineBufferSetFrameControlImp::getStreamBufferSet() const {
  return *const_cast<PipelineBufferSetFrameControlImp*>(this);
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IPipelineNodeCallback>
PipelineBufferSetFrameControlImp::getPipelineNodeCallback() const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  std::shared_ptr<IPipelineNodeCallback> p = mpPipelineCallback.lock();
  //
  MY_LOGE_IF(
      p == nullptr,
      "requestNo:%u frameNo:%u Bad PipelineNodeCallback: wp:%d promote:%p - %s",
      getRequestNo(), getFrameNo(), mpPipelineCallback.expired(), p.get(),
      getFrameLifetimeLog(mTimestampFrameCreated, mTimestampFrameDone).c_str());
  pthread_rwlock_unlock(&mRWLock);
  return p;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineBufferSetFrameControlImp::setNodeMap(
    std::shared_ptr<IPipelineFrameNodeMapControl> value) {
  if (CC_UNLIKELY(value == nullptr)) {
    MY_LOGE("requestNo:%u frameNo:%u - NULL value", getRequestNo(),
            getFrameNo());
    return BAD_VALUE;
  }
  //
  if (CC_UNLIKELY(value->isEmpty())) {
    MY_LOGE("requestNo:%u frameNo:%u - Empty value", getRequestNo(),
            getFrameNo());
    return BAD_VALUE;
  }
  //
  pthread_rwlock_wrlock(&mRWLock);
  mpNodeMap = value;
  pthread_rwlock_unlock(&mRWLock);
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineBufferSetFrameControlImp::setPipelineNodeMap(
    std::shared_ptr<IPipelineNodeMap const> value) {
  if (CC_UNLIKELY(value == nullptr)) {
    MY_LOGE("requestNo:%u frameNo:%u - NULL value", getRequestNo(),
            getFrameNo());
    return BAD_VALUE;
  }
  //
  if (CC_UNLIKELY(value->isEmpty())) {
    MY_LOGE("requestNo:%u frameNo:%u - Empty value", getRequestNo(),
            getFrameNo());
    return BAD_VALUE;
  }
  //
  pthread_rwlock_wrlock(&mRWLock);
  mpPipelineNodeMap = value;
  pthread_rwlock_unlock(&mRWLock);
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineBufferSetFrameControlImp::setPipelineDAG(
    std::shared_ptr<IPipelineDAG> value) {
  if (CC_UNLIKELY(value == nullptr)) {
    MY_LOGE("requestNo:%u frameNo:%u - NULL value", getRequestNo(),
            getFrameNo());
    return BAD_VALUE;
  }
  //
  pthread_rwlock_wrlock(&mRWLock);
  mpPipelineDAG = value;
  pthread_rwlock_unlock(&mRWLock);
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineBufferSetFrameControlImp::setStreamInfoSet(
    std::shared_ptr<IStreamInfoSet const> value) {
  if (CC_UNLIKELY(value == nullptr)) {
    MY_LOGE("requestNo:%u frameNo:%u - NULL value", getRequestNo(),
            getFrameNo());
    return BAD_VALUE;
  }
  //
  pthread_rwlock_wrlock(&mRWLock);
  mpStreamInfoSet = value;
  pthread_rwlock_unlock(&mRWLock);
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineBufferSetFrameControlImp::queryIOStreamInfoSet(
    NodeId_T const& nodeId,
    std::shared_ptr<IStreamInfoSet const>* rIn,
    std::shared_ptr<IStreamInfoSet const>* rOut) const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  if (CC_UNLIKELY(mpNodeMap == nullptr)) {
    MY_LOGE("requestNo:%u frameNo:%u NULL node map", getRequestNo(),
            getFrameNo());
    *rIn = 0;
    *rOut = 0;
    pthread_rwlock_unlock(&mRWLock);
    return NO_INIT;
  }
  //
  std::shared_ptr<IPipelineFrameNodeMapControl::INode> pNode =
      mpNodeMap->getNodeFor(nodeId);
  if (CC_UNLIKELY(pNode == nullptr)) {
    MY_LOGE("requestNo:%u frameNo:%u nodeId:%#" PRIxPTR " not found",
            getRequestNo(), getFrameNo(), nodeId);
    *rIn = nullptr;
    *rOut = nullptr;
    pthread_rwlock_unlock(&mRWLock);
    return NAME_NOT_FOUND;
  }
  //
  *rIn = pNode->getIStreams();
  *rOut = pNode->getOStreams();
  //
  if (CC_UNLIKELY(*rIn == nullptr || *rOut == nullptr)) {
    MY_LOGE("requestNo:%u frameNo:%u nodeId:%#" PRIxPTR
            " IStreams:%p OStreams:%p",
            getRequestNo(), getFrameNo(), nodeId, rIn->get(), rOut->get());
    pthread_rwlock_unlock(&mRWLock);
    return NO_INIT;
  }
  //
  pthread_rwlock_unlock(&mRWLock);
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineBufferSetFrameControlImp::queryInfoIOMapSet(
    NodeId_T const& nodeId, InfoIOMapSet* rIOMapSet) const {
  pthread_rwlock_rdlock(&mRWLock);
  //
  if (CC_UNLIKELY(mpNodeMap == nullptr)) {
    MY_LOGE("requestNo:%u frameNo:%u NULL node map", getRequestNo(),
            getFrameNo());
    pthread_rwlock_unlock(&mRWLock);
    return NO_INIT;
  }
  //
  std::shared_ptr<IPipelineFrameNodeMapControl::INode> pNode =
      mpNodeMap->getNodeFor(nodeId);
  if (CC_UNLIKELY(pNode == nullptr)) {
    MY_LOGE("requestNo:%u frameNo:%u nodeId:%#" PRIxPTR " not found",
            getRequestNo(), getFrameNo(), nodeId);
    pthread_rwlock_unlock(&mRWLock);
    return NAME_NOT_FOUND;
  }
  //
  *rIOMapSet = pNode->getInfoIOMapSet();
  pthread_rwlock_unlock(&mRWLock);
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineBufferSetFrameControlImp::startConfiguration() {
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineBufferSetFrameControlImp::finishConfiguration() {
  pthread_rwlock_wrlock(&mRWLock);
  std::lock_guard<std::mutex> _lItemMapLock(mItemMapLock);
  //
  if (CC_UNLIKELY(mpNodeMap == nullptr || mpNodeMap->isEmpty())) {
    MY_LOGE("Empty NodeMap: %p", mpNodeMap.get());
    pthread_rwlock_unlock(&mRWLock);
    return NO_INIT;
  }
  //
  if (CC_UNLIKELY(mpStreamInfoSet == nullptr)) {
    MY_LOGE("StreamInfoSet:%p", mpStreamInfoSet.get());
    pthread_rwlock_unlock(&mRWLock);
    return NO_INIT;
  }
  //
  if (CC_UNLIKELY(mpPipelineDAG == nullptr || mpPipelineNodeMap.expired())) {
    MY_LOGE("PipelineDAG:%p", mpPipelineDAG.get());
    pthread_rwlock_unlock(&mRWLock);
    return NO_INIT;
  }
  //
  mpReleasedCollector->finishConfiguration(
      *mpItemMap_AppImage, *mpItemMap_AppMeta, *mpItemMap_HalImage,
      *mpItemMap_HalMeta);
  //
  mNodeStatusMap.reserve(mpNodeMap->size());
  for (size_t i = 0; i < mpNodeMap->size(); i++) {
    std::shared_ptr<NodeStatus> pNodeStatus = std::make_shared<NodeStatus>();
    //
    std::shared_ptr<IPipelineFrameNodeMapControl::INode> pNode =
        mpNodeMap->getNodeAt(i);
    NodeId_T const nodeId = pNode->getNodeId();
    {
      std::shared_ptr<IStreamInfoSet const> pStreams = pNode->getIStreams();
      // I:Meta
      for (size_t j = 0; j < pStreams->getMetaInfoNum(); j++) {
        std::shared_ptr<IStreamInfo> pStreamInfo = pStreams->getMetaInfoAt(j);
        StreamId_T const streamId = pStreamInfo->getStreamId();
        //
        std::shared_ptr<NodeStatus::IO> pIO =
            std::make_shared<NodeStatus::IO>();
        pNodeStatus->mISetMeta.push_back(pIO);
        pIO->mMapItem = getMetaMapItemLocked(streamId);
        MY_LOGF_IF(pIO->mMapItem == 0, "No I meta item for streamId:%#" PRIx64,
                   streamId);
      }
      // I:Image
      for (size_t j = 0; j < pStreams->getImageInfoNum(); j++) {
        std::shared_ptr<IStreamInfo> pStreamInfo = pStreams->getImageInfoAt(j);
        StreamId_T const streamId = pStreamInfo->getStreamId();
        //
        std::shared_ptr<NodeStatus::IO> pIO =
            std::make_shared<NodeStatus::IO>();
        pNodeStatus->mISetImage.push_back(pIO);
        pIO->mMapItem = getImageMapItemLocked(streamId);
        MY_LOGF_IF(pIO->mMapItem == 0, "No I image item for streamId:%#" PRIx64,
                   streamId);
      }
    }
    {
      std::shared_ptr<IStreamInfoSet const> pStreams = pNode->getOStreams();
      // O:Meta
      for (size_t j = 0; j < pStreams->getMetaInfoNum(); j++) {
        std::shared_ptr<IStreamInfo> pStreamInfo = pStreams->getMetaInfoAt(j);
        StreamId_T const streamId = pStreamInfo->getStreamId();
        //
        std::shared_ptr<NodeStatus::IO> pIO =
            std::make_shared<NodeStatus::IO>();
        pNodeStatus->mOSetMeta.push_back(pIO);
        pIO->mMapItem = getMetaMapItemLocked(streamId);
        MY_LOGF_IF(pIO->mMapItem == 0, "No O meta item for streamId:%#" PRIx64,
                   streamId);
      }
      // O:Image
      for (size_t j = 0; j < pStreams->getImageInfoNum(); j++) {
        std::shared_ptr<IStreamInfo> pStreamInfo = pStreams->getImageInfoAt(j);
        StreamId_T const streamId = pStreamInfo->getStreamId();
        //
        std::shared_ptr<NodeStatus::IO> pIO =
            std::make_shared<NodeStatus::IO>();
        pNodeStatus->mOSetImage.push_back(pIO);
        pIO->mMapItem = getImageMapItemLocked(streamId);
        MY_LOGF_IF(pIO->mMapItem == 0, "No O image item for streamId:%#" PRIx64,
                   streamId);
      }
    }
    //
    if (CC_LIKELY(!pNodeStatus->mISetMeta.empty() ||
                  !pNodeStatus->mOSetMeta.empty() ||
                  !pNodeStatus->mISetImage.empty() ||
                  !pNodeStatus->mOSetImage.empty())) {
      mNodeStatusMap.emplace(nodeId, pNodeStatus);
      mNodeStatusMap.mInFlightNodeCount++;
      //
      MY_LOGD("nodeId:%#" PRIxPTR " Image:I/O#=%zu/%zu Meta:I/O#=%zu/%zu",
              nodeId, pNodeStatus->mISetImage.size(),
              pNodeStatus->mOSetImage.size(), pNodeStatus->mISetMeta.size(),
              pNodeStatus->mOSetMeta.size());
    }
  }
  //
  pthread_rwlock_unlock(&mRWLock);
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IUsersManager>
PipelineBufferSetFrameControlImp::findSubjectUsersLocked(
    StreamId_T streamId) const {
#define _IMPLEMENT_(_map_)                             \
  {                                                    \
    ssize_t const index = _map_->indexOfKey(streamId); \
    if (0 <= index) {                                  \
      return _map_->usersManagerAt(index);             \
    }                                                  \
  }

  _IMPLEMENT_(mpItemMap_AppImage);
  _IMPLEMENT_(mpItemMap_AppMeta);
  _IMPLEMENT_(mpItemMap_HalImage);
  _IMPLEMENT_(mpItemMap_HalMeta);

#undef _IMPLEMENT_

  MY_LOGW("[requestNo:%u frameNo:%u] streamId:%#" PRIx64 " not found",
          getRequestNo(), getFrameNo(), streamId);
  return nullptr;
}

/******************************************************************************
 *
 ******************************************************************************/
template <class ItemMapT>
std::shared_ptr<typename ItemMapT::IStreamBufferT>
PipelineBufferSetFrameControlImp::getBufferLockedImp(
    StreamId_T streamId, UserId_T userId, ItemMapT const& rMap) const {
  if (0 == rMap.mNonReleasedNum) {
    return nullptr;
  }
  //
  std::shared_ptr<typename ItemMapT::ItemT> pItem = rMap.getItemFor(streamId);
  if (pItem == nullptr) {
    return nullptr;
  }
  //
  if (!pItem->mBitStatus.test(eBUF_STATUS_ACQUIRE)) {
    if (CC_UNLIKELY(pItem->mBitStatus.test(eBUF_STATUS_ACQUIRE_FAILED))) {
      pItem->mUsersManager->markUserStatus(userId,
                                           IUsersManager::UserStatus::RELEASE);
      MY_LOGW("[requestNo:%u frameNo:%u streamId:%#" PRIx64
              "] Failure in previous acquiring buffer",
              getRequestNo(), getFrameNo(), streamId);
      return nullptr;
    }
    MY_LOGF_IF(pItem->mBuffer != 0,
               "[requestNo:%u frameNo:%u streamId:%#" PRIx64
               "] Non-null buffer but non-acquired status:%#lx",
               getRequestNo(), getFrameNo(), streamId,
               pItem->mBitStatus.to_ulong());
    std::shared_ptr<IPipelineStreamBufferProvider> pBufferProvider =
        mBufferProvider.lock();
    if (CC_UNLIKELY(pBufferProvider == nullptr)) {
      MY_LOGE("[requestNo:%u frameNo:%u streamId:%#" PRIx64
              "] Fail to promote buffer provider:%p",
              getRequestNo(), getFrameNo(), streamId, pBufferProvider.get());
      return nullptr;
    }
    //
    struct Helper {
      struct Params {
        MUINT32 const requestNo;
        UserId_T userId;
        IPipelineStreamBufferProvider* pBufferProvider;
      };
      static MERROR acquireStreamBuffer(const Params& rParams,
                                        ItemMap_HalImageT::MapValueT pItem) {
        MERROR err = rParams.pBufferProvider->acquireHalStreamBuffer(
            rParams.requestNo, pItem->mStreamInfo, &(pItem->mBuffer));
        if (OK == err && pItem->mBuffer != 0) {
          pItem->mBuffer->setUsersManager(pItem->mUsersManager);
          pItem->mBitStatus.set(eBUF_STATUS_ACQUIRE);
        } else {
          pItem->mBitStatus.set(eBUF_STATUS_ACQUIRE_FAILED);
          pItem->mUsersManager->markUserStatus(
              rParams.userId, IUsersManager::UserStatus::RELEASE);
        }
        return err;
      }

      static MERROR acquireStreamBuffer(Params&, ItemMap_HalMetaT::MapValueT) {
        return INVALID_OPERATION;
      }
      static MERROR acquireStreamBuffer(Params&, ItemMap_AppMetaT::MapValueT) {
        return INVALID_OPERATION;
      }
      static MERROR acquireStreamBuffer(Params&, ItemMap_AppImageT::MapValueT) {
        return INVALID_OPERATION;
      }
    };
    CAM_TRACE_FMT_BEGIN("acquireStreamBuffer sID%#" PRIxPTR, streamId);
    NSCam::Utils::CamProfile profile(__FUNCTION__, "acquireStreamBuffer");
    typename Helper::Params param = {getFrameNo(), userId,
                                     pBufferProvider.get()};
    MERROR err = Helper::acquireStreamBuffer(param, pItem);
    profile.print_overtime(10,
                           "[requestNo:%u frameNo:%u streamId:%#" PRIx64 "]",
                           getRequestNo(), getFrameNo(), streamId);
    CAM_TRACE_FMT_END();
    if (OK != err || pItem->mBuffer == 0) {
      pItem->mBuffer = nullptr;
      return nullptr;
    }
  }
  //
  if (CC_UNLIKELY(pItem->mBuffer == nullptr)) {
    MY_LOGW("[requestNo:%u frameNo:%u streamId:%#" PRIx64
            "] "
            "mBitStatus(%#lx) pValue->mBuffer == 0",
            getRequestNo(), getFrameNo(), streamId,
            pItem->mBitStatus.to_ulong());
    return nullptr;
  }
  //
  return pItem->mBuffer;
}

/******************************************************************************
 *
 ******************************************************************************/
template <class ItemMapT>
std::shared_ptr<typename ItemMapT::IStreamBufferT>
PipelineBufferSetFrameControlImp::getBufferLocked(StreamId_T streamId,
                                                  UserId_T userId,
                                                  ItemMapT const& rMap) const {
  std::shared_ptr<typename ItemMapT::IStreamBufferT> pBuffer =
      getBufferLockedImp(streamId, userId, rMap);
  //
  if (pBuffer == nullptr) {
    return nullptr;
  }

  /**
   * The buffer is NOT available if all users have released this buffer
   * (so as to be marked as released).
   */
  if (CC_UNLIKELY(OK == pBuffer->haveAllUsersReleased())) {
    MY_LOGW("[requestNo:%u frameNo:%u streamId:%#" PRIx64 " userId:%#" PRIxPTR
            "] "
            "all users released this buffer",
            getRequestNo(), getFrameNo(), streamId, userId);
    return nullptr;
  }

  /**
   * For a specific stream buffer (associated with a stream Id), a user (with
   * a unique user Id) could successfully acquire the buffer from this buffer
   * set only if all users ahead of this user have pre-released or released
   * the buffer.
   */
  if (CC_UNLIKELY(OK != pBuffer->haveAllUsersReleasedOrPreReleased(userId))) {
    MY_LOGW("[requestNo:%u frameNo:%u streamId:%#" PRIx64 " userId:%#" PRIxPTR
            "] "
            "not all of prior users release or pre-release this buffer",
            getRequestNo(), getFrameNo(), streamId, userId);
    return nullptr;
  }

  return pBuffer;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IMetaStreamBuffer>
PipelineBufferSetFrameControlImp::getMetaBuffer(StreamId_T streamId,
                                                UserId_T userId) const {
  std::shared_ptr<IMetaStreamBuffer> p;
  //
  std::lock_guard<std::mutex> _lItemMapLock(mItemMapLock);
  //
  p = getBufferLocked(streamId, userId, *mpItemMap_HalMeta);
  if (p != 0) {
    return p;
  }
  //
  p = getBufferLocked(streamId, userId, *mpItemMap_AppMeta);
  if (p != 0) {
    return p;
  }
  //
  return nullptr;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IImageStreamBuffer>
PipelineBufferSetFrameControlImp::getImageBuffer(StreamId_T streamId,
                                                 UserId_T userId) const {
  std::shared_ptr<IImageStreamBuffer> p;
  //
  std::lock_guard<std::mutex> _lItemMapLock(mItemMapLock);
  //
  p = getBufferLocked(streamId, userId, *mpItemMap_HalImage);
  if (p != 0) {
    return p;
  }
  //
  p = getBufferLocked(streamId, userId, *mpItemMap_AppImage);
  if (p != 0) {
    return p;
  }
  //
  return nullptr;
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT32
PipelineBufferSetFrameControlImp::markUserStatus(StreamId_T const streamId,
                                                 UserId_T userId,
                                                 MUINT32 eStatus) {
  std::lock_guard<std::mutex> _l(mItemMapLock);
  //
  std::shared_ptr<IUsersManager> pSubjectUsers =
      findSubjectUsersLocked(streamId);
  if (CC_UNLIKELY(pSubjectUsers == nullptr)) {
    return NAME_NOT_FOUND;
  }
  //
  return pSubjectUsers->markUserStatus(userId, eStatus);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineBufferSetFrameControlImp::setUserReleaseFence(StreamId_T const streamId,
                                                      UserId_T userId,
                                                      MINT releaseFence) {
  std::lock_guard<std::mutex> _l(mItemMapLock);
  //
  std::shared_ptr<IUsersManager> pSubjectUsers =
      findSubjectUsersLocked(streamId);
  if (CC_UNLIKELY(pSubjectUsers == nullptr)) {
    return NAME_NOT_FOUND;
  }
  //
  return pSubjectUsers->setUserReleaseFence(userId, releaseFence);
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT
PipelineBufferSetFrameControlImp::queryGroupUsage(StreamId_T const streamId,
                                                  UserId_T userId) const {
  std::lock_guard<std::mutex> _l(mItemMapLock);
  //
  std::shared_ptr<IUsersManager> pSubjectUsers =
      findSubjectUsersLocked(streamId);
  if (CC_UNLIKELY(pSubjectUsers == nullptr)) {
    return 0;
  }
  //
  return pSubjectUsers->queryGroupUsage(userId);
}

/******************************************************************************
 *
 ******************************************************************************/
MINT PipelineBufferSetFrameControlImp::createAcquireFence(
    StreamId_T const streamId, UserId_T userId) const {
  std::lock_guard<std::mutex> _l(mItemMapLock);
  //
  std::shared_ptr<IUsersManager> pSubjectUsers =
      findSubjectUsersLocked(streamId);
  if (CC_UNLIKELY(pSubjectUsers == nullptr)) {
    return -1;
  }
  //
  return pSubjectUsers->createAcquireFence(userId);
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IMyMap::IItem>
PipelineBufferSetFrameControlImp::getMapItemLocked(
    StreamId_T streamId, IMyMap const& rItemMap) const {
  std::shared_ptr<IMyMap::IItem> const& pItem = rItemMap.itemFor(streamId);
  if (pItem == nullptr) {
    return nullptr;
  }
  return pItem;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IMyMap::IItem>
PipelineBufferSetFrameControlImp::getMetaMapItemLocked(
    StreamId_T streamId) const {
  std::shared_ptr<IMyMap::IItem> p;
  //
  p = getMapItemLocked(streamId, *mpItemMap_HalMeta);
  if (p != 0) {
    return p;
  }
  //
  p = getMapItemLocked(streamId, *mpItemMap_AppMeta);
  if (p != 0) {
    return p;
  }
  //
  return nullptr;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IMyMap::IItem>
PipelineBufferSetFrameControlImp::getImageMapItemLocked(
    StreamId_T streamId) const {
  std::shared_ptr<IMyMap::IItem> p;
  //
  p = getMapItemLocked(streamId, *mpItemMap_HalImage);
  if (p != 0) {
    return p;
  }
  //
  p = getMapItemLocked(streamId, *mpItemMap_AppImage);
  if (p != 0) {
    return p;
  }
  //
  return nullptr;
}

/******************************************************************************
 *
 ******************************************************************************/
struct PipelineBufferSetFrameControlImp::NodeStatusUpdater {
 public:  ////                    Definitions.
  typedef NodeStatus::IOSet IOSet;

 public:  ////                    Data Members.
  MUINT32 const mFrameNo;

 public:
  explicit NodeStatusUpdater(MUINT32 frameNo) : mFrameNo(frameNo) {}

  MBOOL
  run(NodeId_T const nodeId,
      NodeStatusMap& rNodeStatusMap,
      std::bitset<32>& rNodeStatusUpdated) {
    MBOOL isAnyUpdate = MFALSE;
    //
    auto const index = rNodeStatusMap.find(nodeId);
    if (CC_UNLIKELY(index == rNodeStatusMap.end())) {
      MY_LOGE("frameNo:%u nodeId:%#" PRIxPTR " not found", mFrameNo, nodeId);
      return MFALSE;
    }
    //
    std::shared_ptr<NodeStatus> pNodeStatus = index->second;
    if (CC_UNLIKELY(pNodeStatus == nullptr)) {
      MY_LOGE("frameNo:%u nodeId:%#" PRIxPTR " nullptr buffer", mFrameNo,
              nodeId);
      return MFALSE;
    }
    //
    // O Image
    if (updateNodeStatus(nodeId, &pNodeStatus->mOSetImage)) {
      isAnyUpdate = MTRUE;
      rNodeStatusUpdated.set(
          IPipelineFrameListener::eMSG_ALL_OUT_IMAGE_BUFFERS_RELEASED);
      MY_LOGD("frameNo:%u nodeId:%#" PRIxPTR " O Image Buffers Released",
              mFrameNo, nodeId);
    }
    // I Image
    if (updateNodeStatus(nodeId, &pNodeStatus->mISetImage)) {
      isAnyUpdate = MTRUE;
      MY_LOGD("frameNo:%u nodeId:%#" PRIxPTR " I Image Buffers Released",
              mFrameNo, nodeId);
    }
    // O Meta
    if (updateNodeStatus(nodeId, &pNodeStatus->mOSetMeta)) {
      isAnyUpdate = MTRUE;
      rNodeStatusUpdated.set(
          IPipelineFrameListener::eMSG_ALL_OUT_META_BUFFERS_RELEASED);
      MY_LOGD("frameNo:%u nodeId:%#" PRIxPTR " O Meta Buffers Released",
              mFrameNo, nodeId);
    }
    // I Meta
    if (updateNodeStatus(nodeId, &pNodeStatus->mISetMeta)) {
      isAnyUpdate = MTRUE;
      MY_LOGD("frameNo:%u nodeId:%#" PRIxPTR " I Meta Buffers Released",
              mFrameNo, nodeId);
    }

    //
    // Is it a new node with all buffers released?
    if (isAnyUpdate && pNodeStatus->mOSetImage.empty() &&
        pNodeStatus->mISetImage.empty() && pNodeStatus->mOSetMeta.empty() &&
        pNodeStatus->mISetMeta.empty()) {
      rNodeStatusMap.mInFlightNodeCount--;
    }
    //
    return isAnyUpdate;
  }

 protected:
  MBOOL
  updateNodeStatus(NodeId_T const nodeId, IOSet* rIOSet) {
    if (rIOSet->mNotified) {
      return MFALSE;
    }
    //
    IOSet::iterator it = rIOSet->begin();
    for (; it != rIOSet->end();) {
      std::shared_ptr<IMyMap::IItem> pMapItem = (*it)->mMapItem;
      //
      if (OK == pMapItem->getUsersManager()->haveAllProducerUsersReleased()) {
        pMapItem->handleProducersReleased();
      }
      //
      //  Check to see if this user "nodeId" has released.
      MUINT32 const status = pMapItem->getUsersManager()->getUserStatus(nodeId);
      if (0 == (status & IUsersManager::UserStatus::RELEASE)) {
        ++it;
        continue;
      }
      //
      it = rIOSet->erase(it);  // remove if released
      //
      if (NSCam::OK == pMapItem->getUsersManager()->haveAllUsersReleased()) {
        pMapItem->handleAllUsersReleased();
      }
    }
    //
    if (rIOSet->empty()) {
      rIOSet->mNotified = MTRUE;
      return MTRUE;
    }
    //
    return MFALSE;
  }
};  // end struct PipelineBufferSetFrameControlImp::NodeStatusUpdater

/******************************************************************************
 *
 ******************************************************************************/
MVOID
PipelineBufferSetFrameControlImp::handleReleasedBuffers(
    UserId_T userId, std::weak_ptr<IAppCallback> pAppCallback) {
  typedef ReleasedCollector::HalImageSetT HalImageSetT;
  typedef ReleasedCollector::HalMetaSetT HalMetaSetT;
  typedef ReleasedCollector::AppMetaSetT AppMetaSetT;

  HalImageSetT aHalImageSet;
  HalMetaSetT aHalMetaSet;
  AppMetaSetT aAppMetaSetO;
  AppMetaSetT aHalMetaSetO;  // note: use AppMetaSetT in purpose.
  ssize_t aAppMetaNumO;
  ssize_t aHalMetaNumO;
  {
    std::lock_guard<std::mutex> _l(mpReleasedCollector->mLock);
    //
    aHalImageSet = mpReleasedCollector->mHalImageSet_AllUsersReleased;
    mpReleasedCollector->mHalImageSet_AllUsersReleased.clear();
    aHalMetaSet = mpReleasedCollector->mHalMetaSet_AllUsersReleased;
    mpReleasedCollector->mHalMetaSet_AllUsersReleased.clear();
    //
    aAppMetaSetO = mpReleasedCollector->mAppMetaSetO_ProducersReleased;
    mpReleasedCollector->mAppMetaSetO_ProducersReleased.clear();
    aAppMetaNumO = mpReleasedCollector->mAppMetaNumO_ProducersInFlight;
    //
    aHalMetaSetO = mpReleasedCollector->mHalMetaSetO_ProducersReleased;
    mpReleasedCollector->mHalMetaSetO_ProducersReleased.clear();
    aHalMetaNumO = mpReleasedCollector->mHalMetaNumO_ProducersInFlight;
  }
  //
  //  Callback to App.
  {
    if (pAppCallback.expired()) {
      MY_LOGW(
          "Cannot promote AppCallback for requestNo:%u frameNo:%u, "
          "userId:%#" PRIxPTR,
          getRequestNo(), getFrameNo(), userId);
    } else {
      MY_LOGD("requestNo:%u frameNo:%u userId:%#" PRIxPTR
              " OAppMeta#(left:%zd this:%zu)",
              getRequestNo(), getFrameNo(), userId, aAppMetaNumO,
              aAppMetaSetO.size());
      IAppCallback::Result result = {getFrameNo(), aAppMetaNumO, aAppMetaSetO,
                                     aHalMetaNumO, aHalMetaSetO, false};

      std::shared_ptr<IAppCallback> spAppCallback = pAppCallback.lock();
      spAppCallback->updateFrame(getRequestNo(), userId, result);
    }
    aAppMetaSetO.clear();
    aHalMetaSetO.clear();
  }
  //
  //  Release to Hal.
  {
    HalImageSetT::iterator it = aHalImageSet.begin();
    for (; it != aHalImageSet.end(); it++) {
      if (CC_LIKELY((*it) != 0)) {
        (*it)->releaseBuffer();
      }
    }
    aHalImageSet.clear();
  }
  {
    HalMetaSetT::iterator it = aHalMetaSet.begin();
    for (; it != aHalMetaSet.end(); it++) {
      if (CC_LIKELY((*it) != 0)) {
        (*it)->releaseBuffer();
      }
    }
    aHalMetaSet.clear();
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
PipelineBufferSetFrameControlImp::applyRelease(UserId_T userId) {
  NodeId_T const nodeId = userId;
  std::shared_ptr<IAppCallback> pAppCallback;
  std::list<MyListener> listeners;
  std::bitset<32> nodeStatusUpdated;
  NodeStatusUpdater updater(getFrameNo());
  //
  MY_LOGD("requestNo:%u frameNo:%u nodeId:%#" PRIxPTR " +", getRequestNo(),
          getFrameNo(), nodeId);
  //
  {
    pthread_rwlock_wrlock(&mRWLock);
    std::lock_guard<std::mutex> _lMapLock(mItemMapLock);
    //
    //  Update
    MBOOL isAnyUpdate = updater.run(nodeId, mNodeStatusMap, nodeStatusUpdated);
    //
    // Is the entire frame released?
    if (isAnyUpdate && 0 == mNodeStatusMap.mInFlightNodeCount) {
      nodeStatusUpdated.set(IPipelineFrameListener::eMSG_FRAME_RELEASED);
      //
      NSCam::Utils::LogTool::get()->getCurrentLogTime(&mTimestampFrameDone);
      //
      mpStreamInfoSet = 0;
      MY_LOGD("Done requestNo:%u frameNo:%u @ nodeId:%#" PRIxPTR " - %s",
              getRequestNo(), getFrameNo(), nodeId,
              getFrameLifetimeLog(mTimestampFrameCreated, mTimestampFrameDone)
                  .c_str());
    }
    //
    if (!nodeStatusUpdated.none()) {
      listeners = mListeners;
    }
    //
    pAppCallback = mpAppCallback.lock();
    pthread_rwlock_unlock(&mRWLock);
  }
  //
  //
  handleReleasedBuffers(userId, pAppCallback);
  //
  //  Callback to listeners if needed.
  if (!nodeStatusUpdated.none()) {
    NSCam::Utils::CamProfile profile(__FUNCTION__,
                                     "IPipelineBufferSetFrameControl");
    //
    std::list<MyListener>::iterator it = listeners.begin();
    for (; it != listeners.end(); ++it) {
      std::shared_ptr<MyListener::IListener> p = it->mpListener.lock();
      if (CC_UNLIKELY(p == nullptr)) {
        continue;
      }
      //
      if (nodeStatusUpdated.test(
              IPipelineFrameListener::eMSG_ALL_OUT_META_BUFFERS_RELEASED)) {
        MY_LOGD("requestNo:%u frameNo:%u nodeId:%#" PRIxPTR
                " O Meta Buffers Released",
                getRequestNo(), getFrameNo(), nodeId);
        p->onPipelineFrame(
            getFrameNo(), nodeId,
            IPipelineFrameListener::eMSG_ALL_OUT_META_BUFFERS_RELEASED,
            it->mpCookie);
      }
      //
      if (nodeStatusUpdated.test(
              IPipelineFrameListener::eMSG_ALL_OUT_IMAGE_BUFFERS_RELEASED)) {
        MY_LOGD("requestNo:%u frameNo:%u nodeId:%#" PRIxPTR
                " O Image Buffers Released",
                getRequestNo(), getFrameNo(), nodeId);
        p->onPipelineFrame(
            getFrameNo(), nodeId,
            IPipelineFrameListener::eMSG_ALL_OUT_IMAGE_BUFFERS_RELEASED,
            it->mpCookie);
      }
      //
      if (nodeStatusUpdated.test(IPipelineFrameListener::eMSG_FRAME_RELEASED)) {
        MY_LOGD("requestNo:%u frameNo:%u nodeId:%#" PRIxPTR " Frame Done",
                getRequestNo(), getFrameNo(), nodeId);
        p->onPipelineFrame(getFrameNo(),
                           IPipelineFrameListener::eMSG_FRAME_RELEASED,
                           it->mpCookie);
      }
    }
    //
    profile.print_overtime(3, "notify listeners (nodeStatusUpdated:%#x)",
                           nodeStatusUpdated);
  }
  //
  MY_LOGD("requestNo:%u frameNo:%u nodeId:%#" PRIxPTR " -", getRequestNo(),
          getFrameNo(), nodeId);
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
PipelineBufferSetFrameControlImp::applyPreRelease(UserId_T userId) {
  MY_LOGD("APPLYPRERELEASE +");
  typedef ReleasedCollector::HalImageSetT HalImageSetT;
  typedef ReleasedCollector::HalMetaSetT HalMetaSetT;
  typedef ReleasedCollector::AppMetaSetT AppMetaSetT;

  HalImageSetT aHalImageSet;
  HalMetaSetT aHalMetaSet;
  AppMetaSetT aAppMetaSetO;
  AppMetaSetT aHalMetaSetO;  // note: use AppMetaSetT in purpose.
  ssize_t aAppMetaNumO;
  ssize_t aHalMetaNumO;
  {
    std::lock_guard<std::mutex> _l(mpReleasedCollector->mLock);
    //
    aHalImageSet = mpReleasedCollector->mHalImageSet_AllUsersReleased;
    mpReleasedCollector->mHalImageSet_AllUsersReleased.clear();
    aHalMetaSet = mpReleasedCollector->mHalMetaSet_AllUsersReleased;
    mpReleasedCollector->mHalMetaSet_AllUsersReleased.clear();
    //
    aAppMetaSetO = mpReleasedCollector->mAppMetaSetO_ProducersReleased;
    mpReleasedCollector->mAppMetaSetO_ProducersReleased.clear();
    aAppMetaNumO = mpReleasedCollector->mAppMetaNumO_ProducersInFlight;
    //
    aHalMetaSetO = mpReleasedCollector->mHalMetaSetO_ProducersReleased;
    mpReleasedCollector->mHalMetaSetO_ProducersReleased.clear();
    aHalMetaNumO = mpReleasedCollector->mHalMetaNumO_ProducersInFlight;
  }

  //
  // Notify AppStreamMgr request number, AppStreamMgr would check PRE_RELEASE
  std::shared_ptr<IAppCallback> pAppCallback;
  pAppCallback = mpAppCallback.lock();
  if (CC_UNLIKELY(pAppCallback == nullptr)) {
    MY_LOGW(
        "Cannot promote AppCallback for requestNo:%u frameNo:%u, "
        "userId:%#" PRIxPTR,
        getRequestNo(), getFrameNo(), userId);
  } else {
    IAppCallback::Result result = {getFrameNo(), aAppMetaNumO, aAppMetaSetO,
                                   aHalMetaNumO, aHalMetaSetO, false};
    MY_LOGD("Prerelease: requestNo:%u frameNo:%u userId:%#" PRIxPTR
            " OAppMeta#(left:%zd)",
            getRequestNo(), getFrameNo(), userId, aAppMetaNumO);
    pAppCallback->updateFrame(getRequestNo(), userId, result);
  }
  aAppMetaSetO.clear();
  aHalMetaSetO.clear();
  //
  MY_LOGD("APPLYPRERELEASE -");
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
PipelineBufferSetFrameControlImp::dumpState(
    const std::vector<std::string>& options) const {
  {
    std::string os;

    os += base::StringPrintf("frame:%u(r%u) ", mFrameNo, mRequestNo);

    if (auto pLogTool = NSCam::Utils::LogTool::get()) {
      os +=
          pLogTool->convertToFormattedLogTime(&mTimestampFrameCreated).c_str();

      if (pthread_rwlock_tryrdlock(&mRWLock) == NSCam::OK) {
        if (0 != mTimestampFrameDone.tv_sec ||
            0 != mTimestampFrameDone.tv_nsec) {
          os += " -> ";
          os +=
              pLogTool->convertToFormattedLogTime(&mTimestampFrameDone).c_str();
          os += " (";
          os += std::to_string(getDurationInUS(mTimestampFrameCreated,
                                               mTimestampFrameDone))
                    .c_str();
          os += "ms)";
        }
        pthread_rwlock_unlock(&mRWLock);
      }
    }

    if (mbReprocessFrame) {
      os += " reprocess";
    }
  }
}
