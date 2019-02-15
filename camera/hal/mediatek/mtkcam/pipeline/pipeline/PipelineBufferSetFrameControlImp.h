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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_PIPELINE_PIPELINEBUFFERSETFRAMECONTROLIMP_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_PIPELINE_PIPELINEBUFFERSETFRAMECONTROLIMP_H_
//
#include <iostream>
#include <list>
#include <memory>
#include <mtkcam/pipeline/pipeline/IPipelineBufferSetFrameControl.h>
#include <string>
#include <unordered_map>
#include <vector>
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace NSPipelineBufferSetFrameControlImp {

/******************************************************************************
 *  Forward Declaration
 ******************************************************************************/
class ReleasedCollector;

/******************************************************************************
 *  Buffer Status
 ******************************************************************************/
enum {
  eBUF_STATUS_ACQUIRE = 0,        // buffer has been acquired.
  eBUF_STATUS_RELEASE,            // all producers/consumers users release
  eBUF_STATUS_PRODUCERS_RELEASE,  // all producers release
  eBUF_STATUS_ACQUIRE_FAILED      // Has try to acquire buffer but failed
};

/******************************************************************************
 *  Buffer Map
 ******************************************************************************/
struct IMyMap {
  struct IItem {
    virtual std::shared_ptr<IStreamInfo> getStreamInfo() const = 0;
    virtual std::shared_ptr<IUsersManager> getUsersManager() const = 0;
    virtual std::shared_ptr<IItem> handleAllUsersReleased() = 0;
    virtual MVOID handleProducersReleased() = 0;
    virtual ~IItem() {}
  };

  virtual std::shared_ptr<IItem> itemFor(StreamId_T streamId) const = 0;
  virtual std::shared_ptr<IItem> itemAt(size_t index) const = 0;
  virtual size_t size() const = 0;
  virtual ssize_t indexOfKey(StreamId_T const key) const = 0;
  virtual StreamId_T keyAt(size_t index) const = 0;
  virtual ~IMyMap() {}
};

template <class _StreamBufferT_,
          class _IStreamBufferT_ = typename _StreamBufferT_::IStreamBufferT>
class TItemMap : public IMyMap,
                 public IPipelineBufferSetFrameControl::IMap<_StreamBufferT_>,
                 public virtual std::enable_shared_from_this<
                     TItemMap<_StreamBufferT_, _IStreamBufferT_>> {
 public:  ////                Definitions.
  using StreamBufferT = _StreamBufferT_;
  using IStreamBufferT = _IStreamBufferT_;
  using IStreamInfoT = typename StreamBufferT::IStreamInfoT;

  struct Item;
  friend struct Item;
  using ItemT = Item;
  using MapValueT = std::shared_ptr<ItemT>;
  using MapT = std::unordered_map<StreamId_T, MapValueT>;

 public:  ////                Data Members.
  MapT mMap;
  ssize_t mNonReleasedNum;  // In + Inout
  std::shared_ptr<ReleasedCollector> mReleasedCollector;

 public:  ////                Operations.
  explicit TItemMap(std::shared_ptr<ReleasedCollector> pReleasedCollector);
  virtual ~TItemMap() {}
  MVOID onProducersReleased(ItemT* item);
  MVOID onAllUsersReleased(ItemT* item);

  virtual std::shared_ptr<Item> getItemFor(StreamId_T streamId) const {
    if (mMap.find(streamId) != mMap.end()) {
      return mMap.at(streamId);
    }
    return 0;
  }

 public:  ////                Operations: IMyMap
  virtual std::shared_ptr<IItem> itemFor(StreamId_T streamId) const {
    if (mMap.find(streamId) != mMap.end()) {
      return mMap.at(streamId);
    }
    return 0;
  }
  virtual std::shared_ptr<IItem> itemAt(size_t index) const {
    if (index >= mMap.size()) {
      return 0;
    }
    auto iter = mMap.begin();
    std::advance(iter, index);
    return iter->second;
  }

 public:  ////                Operations: IMap
  virtual ssize_t setCapacity(size_t size) {
    mMap.reserve(size);
    return mMap.size();
  }
  virtual bool isEmpty() const { return mMap.empty(); }
  virtual size_t size() const { return mMap.size(); }
  virtual ssize_t indexOfKey(StreamId_T const key) const {
    auto iter = mMap.find(key);
    if (iter == mMap.end()) {
      return -1;
    }
    return std::distance(mMap.begin(), iter);
  }
  virtual StreamId_T keyAt(size_t index) const {
    if (index >= mMap.size()) {
      return 0;
    }
    auto iter = mMap.begin();
    std::advance(iter, index);
    return iter->first;
  }
  virtual std::shared_ptr<IUsersManager> usersManagerAt(size_t index) const {
    if (index >= mMap.size()) {
      return nullptr;
    }
    auto iter = mMap.begin();
    std::advance(iter, index);
    return (iter->second)->mUsersManager;
  }
  virtual std::shared_ptr<IStreamInfoT> streamInfoAt(size_t index) const {
    if (index >= mMap.size()) {
      return nullptr;
    }
    auto iter = mMap.begin();
    std::advance(iter, index);
    return (iter->second)->mStreamInfo;
  }

  virtual ssize_t add(std::shared_ptr<IStreamInfoT>,
                      std::shared_ptr<IUsersManager>);
  virtual ssize_t add(std::shared_ptr<StreamBufferT>);
};

/******************************************************************************
 *
 ******************************************************************************/
template <class _StreamBufferT_, class _IStreamBufferT_>
struct TItemMap<_StreamBufferT_, _IStreamBufferT_>::Item
    : public IMyMap::IItem,
      public virtual std::enable_shared_from_this<Item> {
 public:  ////    Data Members.
  std::weak_ptr<TItemMap> mItselfMap;
  std::shared_ptr<StreamBufferT> mBuffer;
  std::shared_ptr<IStreamInfoT> mStreamInfo;
  std::shared_ptr<IUsersManager> mUsersManager;
  std::bitset<32> mBitStatus;

 public:  ////    Operations.
  Item(std::shared_ptr<TItemMap> pItselfMap,
       std::shared_ptr<StreamBufferT> pStreamBuffer,
       std::shared_ptr<IStreamInfoT> pStreamInfo,
       std::shared_ptr<IUsersManager> pUsersManager)
      : mItselfMap(pItselfMap),
        mBuffer(pStreamBuffer),
        mStreamInfo(pStreamInfo),
        mUsersManager(pUsersManager),
        mBitStatus(0) {
    if (pStreamBuffer) {
      mBitStatus.set(eBUF_STATUS_ACQUIRE);
    }
  }
  virtual ~Item() {}
  virtual std::shared_ptr<IStreamInfo> getStreamInfo() const {
    return mStreamInfo;
  }
  virtual std::shared_ptr<IUsersManager> getUsersManager() const {
    return mUsersManager;
  }

  virtual std::shared_ptr<IItem> handleAllUsersReleased() {
    std::shared_ptr<Item> pItem = this->shared_from_this();
    if (!mBitStatus.test(eBUF_STATUS_RELEASE)) {
      mBitStatus.set(eBUF_STATUS_RELEASE);
      std::shared_ptr<TItemMap> pMap = mItselfMap.lock();
      if (pMap != nullptr) {
        pMap->onAllUsersReleased(this);
      }
    }
    return pItem;
  }

  virtual MVOID handleProducersReleased() {
    if (!mBitStatus.test(eBUF_STATUS_PRODUCERS_RELEASE)) {
      mBitStatus.set(eBUF_STATUS_PRODUCERS_RELEASE);
      std::shared_ptr<TItemMap> pMap = mItselfMap.lock();
      if (pMap != nullptr) {
        pMap->onProducersReleased(this);
      }
    }
  }
};

typedef TItemMap<IImageStreamBuffer, IImageStreamBuffer> ItemMap_AppImageT;
typedef TItemMap<IMetaStreamBuffer, IMetaStreamBuffer> ItemMap_AppMetaT;
typedef TItemMap<IPipelineBufferSetFrameControl::HalImageStreamBuffer>
    ItemMap_HalImageT;
typedef TItemMap<IPipelineBufferSetFrameControl::HalMetaStreamBuffer>
    ItemMap_HalMetaT;

/******************************************************************************
 *  Frame Releaser
 ******************************************************************************/
class ReleasedCollector {
 public:  ////    Definitions.
  using HalMetaStreamBufferT = ItemMap_HalMetaT ::StreamBufferT;
  using AppMetaStreamBufferT = ItemMap_AppMetaT ::StreamBufferT;
  using HalImageStreamBufferT = ItemMap_HalImageT::StreamBufferT;
  using AppImageStreamBufferT = ItemMap_AppImageT::StreamBufferT;

  using HalMetaSetT = std::vector<std::shared_ptr<HalMetaStreamBufferT>>;
  using AppMetaSetT = std::vector<std::shared_ptr<AppMetaStreamBufferT>>;
  using HalImageSetT = std::vector<std::shared_ptr<HalImageStreamBufferT>>;

 public:  ////    Data Members.
  mutable std::mutex mLock;
  HalImageSetT mHalImageSet_AllUsersReleased;
  HalMetaSetT mHalMetaSet_AllUsersReleased;

  AppMetaSetT mAppMetaSetO_ProducersReleased;  // out
  ssize_t mAppMetaNumO_ProducersInFlight;      // out
  // note: use AppMetaSetT, since IMetaStreamBuffer is used in callback
  AppMetaSetT mHalMetaSetO_ProducersReleased;  // out
  ssize_t mHalMetaNumO_ProducersInFlight;      // out

 public:  ////    Operations.
  MVOID finishConfiguration(ItemMap_AppImageT const&,
                            ItemMap_AppMetaT const& rItemMap_AppMeta,
                            ItemMap_HalImageT const&,
                            ItemMap_HalMetaT const& rItemMap_HalMeta) {
    {
      mAppMetaNumO_ProducersInFlight = 0;
      ItemMap_AppMetaT const& rItemMap = rItemMap_AppMeta;
      for (size_t i = 0; i < rItemMap.size(); i++) {
        if (0 < rItemMap.usersManagerAt(i)->getNumberOfProducers()) {
          mAppMetaNumO_ProducersInFlight++;
        }
      }
    }
    {
      mHalMetaNumO_ProducersInFlight = 0;
      ItemMap_HalMetaT const& rItemMap = rItemMap_HalMeta;
      for (size_t i = 0; i < rItemMap.size(); i++) {
        if (0 < rItemMap.usersManagerAt(i)->getNumberOfProducers()) {
          mHalMetaNumO_ProducersInFlight++;
        }
      }
    }
  }

 public:  ////    Operations.
  MVOID onAllUsersReleased(ItemMap_AppImageT::Item*) {}
  MVOID onAllUsersReleased(ItemMap_AppMetaT::Item*) {}
  MVOID onAllUsersReleased(ItemMap_HalImageT::Item* rItem) {
    std::lock_guard<std::mutex> _l(mLock);
    if (rItem->mBuffer != nullptr) {
      mHalImageSet_AllUsersReleased.push_back(rItem->mBuffer);
    }
  }
  MVOID onAllUsersReleased(ItemMap_HalMetaT::Item* rItem) {
    std::lock_guard<std::mutex> _l(mLock);
    if (rItem->mBuffer != nullptr) {
      mHalMetaSet_AllUsersReleased.push_back(rItem->mBuffer);
    }
  }

 public:  ////    Operations.
  MVOID onProducersReleased(ItemMap_HalImageT::Item*) {}
  MVOID onProducersReleased(ItemMap_HalMetaT::Item* rItem) {
    std::lock_guard<std::mutex> _l(mLock);
    if (0 < rItem->getUsersManager()->getNumberOfProducers()) {
      mHalMetaNumO_ProducersInFlight--;
      if (rItem->mBuffer != nullptr) {
        mHalMetaSetO_ProducersReleased.push_back(rItem->mBuffer);
      }
    }
  }
  MVOID onProducersReleased(ItemMap_AppImageT::Item*) {}
  MVOID onProducersReleased(ItemMap_AppMetaT::Item* rItem) {
    std::lock_guard<std::mutex> _l(mLock);
    if (0 < rItem->getUsersManager()->getNumberOfProducers()) {
      mAppMetaNumO_ProducersInFlight--;
      if (rItem->mBuffer != nullptr) {
        mAppMetaSetO_ProducersReleased.push_back(rItem->mBuffer);
      }
    }
  }
};

/******************************************************************************
 *
 ******************************************************************************/
template <class _StreamBufferT_, class _IStreamBufferT_>
TItemMap<_StreamBufferT_, _IStreamBufferT_>::TItemMap(
    std::shared_ptr<ReleasedCollector> pReleasedCollector)
    : mNonReleasedNum(0), mReleasedCollector(pReleasedCollector) {}

/******************************************************************************
 *
 ******************************************************************************/
template <class _StreamBufferT_, class _IStreamBufferT_>
MVOID TItemMap<_StreamBufferT_, _IStreamBufferT_>::onProducersReleased(
    Item* item) {
  if (CC_LIKELY(mReleasedCollector != nullptr)) {
    mReleasedCollector->onProducersReleased(item);
  }
}

/******************************************************************************
 *
 ******************************************************************************/
template <class _StreamBufferT_, class _IStreamBufferT_>
MVOID TItemMap<_StreamBufferT_, _IStreamBufferT_>::onAllUsersReleased(
    ItemT* item) {
  if (mMap.find(item->mStreamInfo->getStreamId()) == mMap.end())
    return;
  std::shared_ptr<ItemT>& rpItem = mMap.at(item->mStreamInfo->getStreamId());
  if (rpItem != nullptr) {
    if (CC_LIKELY(mReleasedCollector != nullptr)) {
      mReleasedCollector->onAllUsersReleased(item);
    }
    mNonReleasedNum--;
    rpItem = nullptr;
  }
}

/******************************************************************************
 *
 ******************************************************************************/
template <class _StreamBufferT_, class _IStreamBufferT_>
ssize_t TItemMap<_StreamBufferT_, _IStreamBufferT_>::add(
    std::shared_ptr<IStreamInfoT> pStreamInfo,
    std::shared_ptr<IUsersManager> pUsersManager) {
  StreamId_T const streamId = pStreamInfo->getStreamId();
  //
  if (pUsersManager == nullptr) {
    pUsersManager = std::make_shared<NSCam::v3::Utils::UsersManager>(
        streamId, pStreamInfo->getStreamName());
  }
  //
  mNonReleasedNum++;
  auto iter = mMap.emplace(
      streamId, std::make_shared<ItemT>(this->shared_from_this(), nullptr,
                                        pStreamInfo, pUsersManager));
  return std::distance(mMap.begin(), iter.first);
}

/******************************************************************************
 *
 ******************************************************************************/
template <class _StreamBufferT_, class _IStreamBufferT_>
ssize_t TItemMap<_StreamBufferT_, _IStreamBufferT_>::add(
    std::shared_ptr<StreamBufferT> value) {
  if (CC_UNLIKELY(value == 0)) {
    return -EINVAL;
  }
  //
  StreamId_T const streamId = value->getStreamInfo()->getStreamId();
  //
  mNonReleasedNum++;
  auto iter = mMap.emplace(
      streamId, std::make_shared<ItemT>(this->shared_from_this(), value,
                                        value->getStreamInfo(), value));
  return std::distance(mMap.begin(), iter.first);
}

/**
 * An Implementation of Pipeline Buffer Set Frame Control.
 */
class PipelineBufferSetFrameControlImp : public IPipelineBufferSetFrameControl {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions: Frame Listener
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  struct MyListener {
    using IListener = IPipelineFrameListener;
    std::weak_ptr<IListener> mpListener;
    MVOID* mpCookie;
    //
    explicit MyListener(std::weak_ptr<IListener> listener,
                        MVOID* const cookie = nullptr)
        : mpListener(listener), mpCookie(cookie) {}
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions: Node Status
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  struct NodeStatus {
    struct IO {
      std::shared_ptr<IMyMap::IItem> mMapItem;
    };

    struct IOSet : public std::list<std::shared_ptr<IO>> {
      MBOOL mNotified;
      //
      IOSet() : mNotified(MFALSE) {}
    };

    IOSet mISetImage;
    IOSet mOSetImage;
    IOSet mISetMeta;
    IOSet mOSetMeta;
  };

  struct NodeStatusMap
      : public std::unordered_map<NodeId_T, std::shared_ptr<NodeStatus>> {
    size_t mInFlightNodeCount = 0;
  };

  struct NodeStatusUpdater;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                    Common.
  MUINT32 const mFrameNo;
  MUINT32 const mRequestNo;
  MBOOL const mbReprocessFrame;
  mutable pthread_rwlock_t mRWLock;
  std::weak_ptr<IAppCallback> const mpAppCallback;
  std::list<MyListener> mListeners;

  struct timespec mTimestampFrameCreated;
  struct timespec mTimestampFrameDone;

 protected:  ////                    Configuration.
  std::weak_ptr<IPipelineStreamBufferProvider> mBufferProvider;

  std::weak_ptr<IPipelineNodeCallback> mpPipelineCallback;

  std::shared_ptr<IStreamInfoSet const> mpStreamInfoSet;

  std::shared_ptr<IPipelineFrameNodeMapControl> mpNodeMap;

  std::weak_ptr<IPipelineNodeMap const> mpPipelineNodeMap;

  std::shared_ptr<IPipelineDAG> mpPipelineDAG;

 protected:  ////
  mutable std::mutex mItemMapLock;
  NodeStatusMap mNodeStatusMap;
  std::shared_ptr<ReleasedCollector> mpReleasedCollector;
  std::shared_ptr<ItemMap_AppImageT> mpItemMap_AppImage;
  std::shared_ptr<ItemMap_AppMetaT> mpItemMap_AppMeta;
  std::shared_ptr<ItemMap_HalImageT> mpItemMap_HalImage;
  std::shared_ptr<ItemMap_HalMetaT> mpItemMap_HalMeta;
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Operations.
  PipelineBufferSetFrameControlImp(
      MUINT32 requestNo,
      MUINT32 frameNo,
      MBOOL bReporcessFrame,
      std::weak_ptr<IAppCallback> const& pAppCallback,
      std::shared_ptr<IPipelineStreamBufferProvider> pBufferProvider,
      std::weak_ptr<IPipelineNodeCallback> pNodeCallback);
  virtual ~PipelineBufferSetFrameControlImp();

 protected:  ////                    Operations.
  MVOID handleReleasedBuffers(UserId_T userId,
                              std::weak_ptr<IAppCallback> pAppCallback);

 protected:  ////                    Operations.
  std::shared_ptr<IUsersManager> findSubjectUsersLocked(
      StreamId_T streamId) const;

  std::shared_ptr<IMyMap::IItem> getMapItemLocked(StreamId_T streamId,
                                                  IMyMap const& rItemMap) const;

  std::shared_ptr<IMyMap::IItem> getMetaMapItemLocked(
      StreamId_T streamId) const;
  std::shared_ptr<IMyMap::IItem> getImageMapItemLocked(
      StreamId_T streamId) const;

  template <class ItemMapT>
  std::shared_ptr<typename ItemMapT::IStreamBufferT> getBufferLockedImp(
      StreamId_T streamId, UserId_T userId, ItemMapT const& rMap) const;

  template <class ItemMapT>
  std::shared_ptr<typename ItemMapT::IStreamBufferT> getBufferLocked(
      StreamId_T streamId, UserId_T userId, ItemMapT const& rMap) const;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineBufferSetFrameControl Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Configuration.
  virtual MERROR startConfiguration();
  virtual MERROR finishConfiguration();

  virtual MERROR setNodeMap(
      std::shared_ptr<IPipelineFrameNodeMapControl> value);
  virtual MERROR setPipelineNodeMap(
      std::shared_ptr<IPipelineNodeMap const> value);
  virtual MERROR setPipelineDAG(std::shared_ptr<IPipelineDAG> value);
  virtual MERROR setStreamInfoSet(std::shared_ptr<IStreamInfoSet const> value);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineBufferSetControl Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Operations
#define _EDITMAP_(_NAME_, _TYPE_)                                           \
  virtual std::shared_ptr<IMap<ItemMap_##_NAME_##_TYPE_##T::StreamBufferT>> \
      editMap_##_NAME_##_TYPE_() {                                          \
    return mpItemMap_##_NAME_##_TYPE_;                                      \
  }

  _EDITMAP_(Hal, Image)  // editMap_HalImage
  _EDITMAP_(App, Image)  // editMap_AppImage
  _EDITMAP_(Hal, Meta)   // editMap_HalMeta
  _EDITMAP_(App, Meta)   // editMap_AppMeta

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IStreamBufferSet Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Operations.
  virtual MVOID applyPreRelease(UserId_T userId);
  virtual MVOID applyRelease(UserId_T userId);

  virtual MUINT32 markUserStatus(StreamId_T const streamId,
                                 UserId_T userId,
                                 MUINT32 eStatus);

  virtual MERROR setUserReleaseFence(StreamId_T const streamId,
                                     UserId_T userId,
                                     MINT releaseFence);

  virtual MUINT queryGroupUsage(StreamId_T const streamId,
                                UserId_T userId) const;

  virtual MINT createAcquireFence(StreamId_T const streamId,
                                  UserId_T userId) const;

 public:  ////                    Operations.
  virtual std::shared_ptr<IMetaStreamBuffer> getMetaBuffer(
      StreamId_T streamId, UserId_T userId) const;

  virtual std::shared_ptr<IImageStreamBuffer> getImageBuffer(
      StreamId_T streamId, UserId_T userId) const;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineFrame Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Attributes.
  virtual MUINT32 getFrameNo() const { return mFrameNo; }
  virtual MUINT32 getRequestNo() const { return mRequestNo; }
  virtual MBOOL IsReprocessFrame() const { return mbReprocessFrame; }

 public:  ////                    Operations.
  virtual MERROR attachListener(std::weak_ptr<IPipelineFrameListener> const&,
                                MVOID* pCookie);

  virtual MVOID dumpState(
      const std::vector<std::string>& options __unused) const;

  virtual std::shared_ptr<IPipelineNodeMap const> getPipelineNodeMap() const;
  virtual IPipelineDAG const& getPipelineDAG() const;
  virtual std::shared_ptr<IPipelineDAG> getPipelineDAGSp();
  virtual IStreamInfoSet const& getStreamInfoSet() const;
  virtual IStreamBufferSet& getStreamBufferSet() const;
  virtual std::shared_ptr<IPipelineNodeCallback> getPipelineNodeCallback()
      const;

  virtual MERROR queryIOStreamInfoSet(
      NodeId_T const& nodeId,
      std::shared_ptr<IStreamInfoSet const>* rIn,
      std::shared_ptr<IStreamInfoSet const>* rOut) const;

  virtual MERROR queryInfoIOMapSet(NodeId_T const& nodeId,
                                   InfoIOMapSet* rIOMapSet) const;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  RefBase Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////
  void onLastStrongRef(const void* id);
};

/****************************************************************************
 *
 ****************************************************************************/
};      // namespace NSPipelineBufferSetFrameControlImp
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_PIPELINE_PIPELINEBUFFERSETFRAMECONTROLIMP_H_
