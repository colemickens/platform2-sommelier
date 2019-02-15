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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_PIPELINE_PIPELINECONTEXTIMPL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_PIPELINE_PIPELINECONTEXTIMPL_H_
//
#include "MyUtils.h"
#include "IPipelineNodeMapControl.h"
#include "InFlightRequest.h"
#include "IPipelineFrameNumberGenerator.h"
#include <map>
#include <memory>
#include <mtkcam/pipeline/pipeline/PipelineContext.h>
#include <string>
#include <vector>
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace NSPipelineContext {
//
/******************************************************************************
 *  Definitions.
 ******************************************************************************/
typedef NSCam::v3::StreamId_T StreamId_T;
typedef NSCam::v3::Utils::HalImageStreamBuffer HalImageStreamBuffer;
typedef NSCam::v3::Utils::HalMetaStreamBuffer HalMetaStreamBuffer;
typedef NSCam::v3::Utils::IStreamInfoSetControl IStreamInfoSetControl;
typedef HalImageStreamBuffer::Allocator HalImageStreamBufferAllocatorT;
typedef HalImageStreamBufferAllocatorT::StreamBufferPoolT
    HalImageStreamBufferPoolT;
typedef NSCam::v3::Utils::IStreamBufferProvider IStreamBufferProviderT;
typedef HalMetaStreamBuffer::Allocator HalMetaStreamBufferAllocatorT;
//
typedef std::map<StreamId_T, MUINT> StreamUsageMap;
typedef std::map<NodeId_T, StreamUsageMap> NodeStreamUsageMaps;
//
typedef std::map<NodeId_T, IOMapSet> NodeIOMaps;
typedef IPipelineFrame::InfoIOMapSet InfoIOMapSet;
typedef IPipelineFrameNodeMapControl FrameNodeMapT;  // FIXME, remove this!

typedef std::map<StreamId_T, std::shared_ptr<IImageStreamBuffer> >
    ImageStreamBufferMapsT;
typedef std::map<StreamId_T, std::shared_ptr<HalImageStreamBuffer> >
    HalImageStreamBufferMapsT;
typedef std::map<StreamId_T, std::shared_ptr<IMetaStreamBuffer> >
    MetaStreamBufferMapsT;
typedef std::map<StreamId_T, std::shared_ptr<HalMetaStreamBuffer> >

    HalMetaStreamBufferMapsT;
typedef IPipelineBufferSetFrameControl::IAppCallback AppCallbackT;
typedef std::map<StreamId_T, std::shared_ptr<IImageStreamInfo> >
    ImageStreamInfoMapT;
typedef std::map<StreamId_T, std::shared_ptr<IMetaStreamInfo> >
    MetaStreamInfoMapT;
typedef std::map<StreamId_T, MUINT32> StreamTypeMapT;

/******************************************************************************
 *
 ******************************************************************************/
class ContextNode {
 public:
  ContextNode(NodeId_T const nodeId, std::shared_ptr<INodeActor> pNode);
  virtual ~ContextNode();

 public:
  NodeId_T getNodeId() const { return mNodeId; }
  std::shared_ptr<IPipelineNode> getNode() const { return mpNode->getNode(); }

 public:
  MVOID setInStreams(std::shared_ptr<IStreamInfoSetControl> pStreams) {
    mpInStreams = pStreams;
  }
  MVOID setOutStreams(std::shared_ptr<IStreamInfoSetControl> pStreams) {
    mpOutStreams = pStreams;
  }
  std::shared_ptr<INodeActor> getNodeActor() const { return mpNode; }
  std::shared_ptr<const IStreamInfoSetControl> const getInStreams() {
    return mpInStreams;
  }
  std::shared_ptr<const IStreamInfoSetControl> const getOutStreams() {
    return mpOutStreams;
  }

 protected:
  NodeId_T const mNodeId;
  std::shared_ptr<INodeActor> const mpNode;
  //
  std::shared_ptr<IStreamInfoSetControl> mpInStreams;
  std::shared_ptr<IStreamInfoSetControl> mpOutStreams;
};
typedef std::map<NodeId_T, std::shared_ptr<ContextNode> > ContextNodeMapT;

/******************************************************************************
 *
 ******************************************************************************/
class NodeBuilderImpl {
 public:
  NodeBuilderImpl(NodeId_T const nodeId, std::shared_ptr<INodeActor> pNode)
      : mpContextNode(new ContextNode(nodeId, pNode)) {}

 public:
  std::shared_ptr<ContextNode> mpContextNode;
  //
  StreamSet mInStreamSet;
  StreamSet mOutStreamSet;
  StreamUsageMap mUsageMap;
};

/******************************************************************************
 *
 ******************************************************************************/
class StreamBuilderImpl {
 public:
  eStreamType mType;
  std::shared_ptr<IImageStreamInfo> mpImageStreamInfo;
  std::shared_ptr<IMetaStreamInfo> mpMetaStreamInfo;
  std::shared_ptr<IStreamBufferProviderT> mpProvider;
};

/******************************************************************************
 *
 ******************************************************************************/
class PipelineBuilderImpl {
 public:
  NodeSet mRootNodes;
  NodeEdgeSet mNodeEdges;
};

/******************************************************************************
 *
 ******************************************************************************/
class RequestBuilderImpl {
 public:
  enum {
    FLAG_NO_CHANGE = 0x0,
    FLAG_FIRSTTIME = 0x1,
    FLAG_IOMAP_CHANGED = 0x2,
    FLAG_NODEEDGE_CHANGED = 0x4,
    FLAG_CALLBACK_CHANGED = 0x8,
    FLAG_REPLACE_STREAMINFO = 0x16,
  };

 public:
  RequestBuilderImpl() : mFlag(FLAG_FIRSTTIME), mbReprocessFrame(MFALSE) {}

 public:
  MVOID setFlag(MUINT32 flag) { mFlag |= flag; }
  MVOID clearFlag() { mFlag = 0; }
  MBOOL getFlag(MUINT32 const flag) const { return mFlag & flag; }

 public:
  MVOID onRequestConstructed();
  MVOID dump(MUINT32 const reqNo, MUINT32 const frameNo) const;

 public:  // configured data
  MUINT32 mFlag;
  //
  NodeIOMaps mImageNodeIOMaps;
  NodeIOMaps mMetaNodeIOMaps;
  //
  NodeEdgeSet mNodeEdges;
  NodeSet mRootNodes;
  MBOOL mbReprocessFrame;
  //
  std::weak_ptr<AppCallbackT> mpCallback;
  //
  ImageStreamInfoMapT mReplacingInfos;
  //
  // one-shot, should be cleared after build a request.
  ImageStreamBufferMapsT mStreamBuffers_Image;
  HalImageStreamBufferMapsT mStreamBuffers_HalImage;
  MetaStreamBufferMapsT mStreamBuffers_Meta;
  HalMetaStreamBufferMapsT mStreamBuffers_HalMeta;

 public:  // intermediates
};

/******************************************************************************
 *
 ******************************************************************************/
class StreamConfig : public IPipelineStreamBufferProvider {
 public:
  //
  struct ItemImageStream {
    std::shared_ptr<IImageStreamInfo> pInfo;
    MUINT32 type;
    std::shared_ptr<HalImageStreamBufferPoolT> pPool;
    std::shared_ptr<IStreamBufferProviderT> pProvider;
    //
    ItemImageStream(std::shared_ptr<IImageStreamInfo> rpInfo,
                    MUINT32 const rType)
        : pInfo(rpInfo), type(rType) {}
    ~ItemImageStream() {
      if (pPool != nullptr) {
        pPool->uninitPool(LOG_TAG);
      }
    }
  };

  struct ItemMetaStream {
    std::shared_ptr<IMetaStreamInfo> pInfo;
    MUINT32 type;
    //
    ItemMetaStream(std::shared_ptr<IMetaStreamInfo> rpInfo, MUINT32 const rType)
        : pInfo(rpInfo), type(rType) {}
  };
  //
 private:
  typedef std::map<StreamId_T, std::shared_ptr<ItemImageStream> > ItemMapImageT;
  typedef std::map<StreamId_T, std::shared_ptr<ItemMetaStream> > ItemMapMetaT;

 public:
  ~StreamConfig();

 public:
  MERROR add(std::shared_ptr<ItemImageStream> pItem) {
    pthread_rwlock_wrlock(&mRWLock);
    mStreamMap_Image.emplace(pItem->pInfo->getStreamId(), pItem);
    pthread_rwlock_unlock(&mRWLock);
    return OK;
  }
  MERROR add(std::shared_ptr<ItemMetaStream> pItem) {
    pthread_rwlock_wrlock(&mRWLock);
    mStreamMap_Meta.emplace(pItem->pInfo->getStreamId(), pItem);
    pthread_rwlock_unlock(&mRWLock);
    return OK;
  }

 public:
  std::shared_ptr<ItemImageStream> queryImage(StreamId_T const streamId) const {
    pthread_rwlock_rdlock(&mRWLock);
    if (mStreamMap_Image.find(streamId) != mStreamMap_Image.end()) {
      auto ret = mStreamMap_Image.at(streamId);
      pthread_rwlock_unlock(&mRWLock);
      return ret;
    }
    pthread_rwlock_unlock(&mRWLock);
    return nullptr;
  }
  std::shared_ptr<ItemMetaStream> queryMeta(StreamId_T const streamId) const {
    pthread_rwlock_rdlock(&mRWLock);
    if (mStreamMap_Meta.find(streamId) != mStreamMap_Meta.end()) {
      auto ret = mStreamMap_Meta.at(streamId);
      pthread_rwlock_unlock(&mRWLock);
      return ret;
    }
    pthread_rwlock_unlock(&mRWLock);
    return nullptr;
  }

 public:  ////    interface of IPipelineStreamBufferProvider
  virtual MERROR acquireHalStreamBuffer(
      MUINT32 const requestNo,
      std::shared_ptr<IImageStreamInfo> const pStreamInfo,
      std::shared_ptr<HalImageStreamBuffer>* rpStreamBuffer) const;

 public:
  void dumpState() const;
  MVOID dump() const;

 private:
  mutable pthread_rwlock_t mRWLock;
  ItemMapImageT mStreamMap_Image;
  ItemMapMetaT mStreamMap_Meta;
};

/******************************************************************************
 *
 ******************************************************************************/
class NodeConfig {
 public:
  NodeConfig() { pthread_rwlock_init(&mRWLock, NULL); }
  ~NodeConfig() { pthread_rwlock_destroy(&mRWLock); }

 public:
  MVOID addNode(NodeId_T const nodeId, std::shared_ptr<ContextNode> pNode);

  MVOID setImageStreamUsage(NodeId_T const nodeId,
                            StreamUsageMap const& usgMap);

  StreamUsageMap const& getImageStreamUsage(NodeId_T const nodeId) const {
    return mNodeImageStreamUsage.at(nodeId);
  }

 public:  // query
  std::shared_ptr<ContextNode> const queryNode(NodeId_T const nodeId) const;

  MUINT queryMinimalUsage(NodeId_T const nodeId,
                          StreamId_T const streamId) const;

 public:  // no lock, since caller should guarantee the calling sequence.
  ContextNodeMapT const& getContextNodeMap() const { return mConfig_NodeMap; }

 public:
  void dumpState() const;

 private:
  mutable pthread_rwlock_t mRWLock;
  ContextNodeMapT mConfig_NodeMap;
  NodeStreamUsageMaps mNodeImageStreamUsage;
};

/******************************************************************************
 *
 ******************************************************************************/
class PipelineConfig {
 public:
  MVOID setRootNode(NodeSet const& roots) { mRootNodes = roots; }
  MVOID setNodeEdges(NodeEdgeSet const& edges) { mNodeEdges = edges; }
  NodeSet const& getRootNode() const { return mRootNodes; }
  NodeEdgeSet const& getNodeEdges() const { return mNodeEdges; }
  void dumpState() const;

 private:
  NodeEdgeSet mNodeEdges;
  NodeSet mRootNodes;
};

/******************************************************************************
 *
 ******************************************************************************/
class DefaultDispatcher : public DispatcherBase {
 public:
  DefaultDispatcher() { pthread_rwlock_init(&mRWLock, NULL); }
  virtual ~DefaultDispatcher() { pthread_rwlock_destroy(&mRWLock); }

  static std::shared_ptr<DefaultDispatcher> create() {
    return std::make_shared<DefaultDispatcher>();
  }

 public:
  virtual MVOID onDispatchFrame(std::shared_ptr<IPipelineFrame> const& pFrame,
                                Pipeline_NodeId_T nodeId);

 protected:
  mutable pthread_rwlock_t mRWLock;
};

/******************************************************************************
 *
 ******************************************************************************/
class PipelineContext::PipelineContextImpl
    : public INodeCallbackToPipeline,
      public std::enable_shared_from_this<PipelineContextImpl> {
 public:
  explicit PipelineContextImpl(char const* name);
  virtual ~PipelineContextImpl();

 protected:
  virtual void onLastStrongRef(const void* id);
  //
 public:
  MERROR updateConfig(NodeBuilderImpl* pBuilder);
  MERROR updateConfig(StreamBuilderImpl* pBuilder);
  MERROR updateConfig(PipelineBuilderImpl* pBuilder);
  MERROR reuseStream(std::shared_ptr<StreamConfig::ItemImageStream> pItem);
  MERROR reuseNode(NodeId_T const nodeId,
                   std::shared_ptr<ContextNode> pNode,
                   StreamUsageMap const& usgMap);
  std::shared_ptr<IPipelineFrame> constructRequest(RequestBuilderImpl* pBuilder,
                                                   MUINT32 const requestNo);

 public:
  MERROR config(PipelineContextImpl* pOldContext, MBOOL const isAsync);
  MERROR queue(std::shared_ptr<IPipelineFrame> const& pFrame);
  MERROR kick(std::shared_ptr<IPipelineFrame> const& pFrame);
  MERROR waitUntilDrained();
  MERROR waitUntilNodeDrained(NodeId_T const nodeId);
  MERROR beginFlush();
  MERROR endFlush();
  MERROR setScenarioControl(std::shared_ptr<IScenarioControl> pControl);
  std::shared_ptr<IScenarioControl> getScenarioControl() const {
    return mpScenarioControl;
  }
  MERROR setDispatcher(std::weak_ptr<IDispatcher> pDispatcher);
  MERROR setDataCallback(std::weak_ptr<IDataCallback> pCallback);

 public:
  std::shared_ptr<HalImageStreamBufferPoolT> queryImageStreamPool(
      StreamId_T const streamId) const;

  std::shared_ptr<StreamConfig::ItemImageStream> queryImage(
      StreamId_T const streamId) const {
    return mpStreamConfig->queryImage(streamId);
  }

  std::shared_ptr<INodeActor> queryNode(NodeId_T const nodeId) const;
  uint32_t getFrameNo();

 public:
  char const* getName() const { return mName.c_str(); }
  auto dumpState(const std::vector<std::string>& options) -> void;
  std::shared_ptr<NodeConfig> getNodeConfig() const { return mpNodeConfig; }

 public:
  MVOID onCallback(CallBackParams param);

 private:
  const std::string mName;

 private:
  mutable pthread_rwlock_t mRWLock;  // FIXME, use this?

 private:
  std::shared_ptr<StreamConfig> mpStreamConfig;
  std::shared_ptr<NodeConfig> mpNodeConfig;
  std::shared_ptr<PipelineConfig> mpPipelineConfig;
  //
  std::shared_ptr<IScenarioControl> mpScenarioControl;
  std::shared_ptr<IPipelineFrameNumberGenerator> mpFrameNumberGenerator;
  std::shared_ptr<IPipelineDAG> mpPipelineDAG;
  std::shared_ptr<IPipelineNodeMapControl> mpPipelineNodeMap;
  std::weak_ptr<IDispatcher> mpDispatcher;
  std::shared_ptr<IDispatcher> mpDispatcher_Default;
  std::weak_ptr<IDataCallback> mpDataCallback;
  std::shared_ptr<InFlightRequest> mpInFlightRequest;
  //
 private:
  //
  mutable std::mutex mEnqueLock;
  std::condition_variable mCondEnque;
  //
  mutable std::mutex mKickLock;
  //
  mutable std::mutex mLastFrameLock;
  std::weak_ptr<IPipelineFrame> mpLastFrame;
  //
  mutable pthread_rwlock_t mFlushLock;
  MBOOL mInFlush;
};

/******************************************************************************
 *
 ******************************************************************************/
struct config_pipeline {
  struct Params {
    // In
    StreamConfig const* pStreamConfig;
    NodeConfig const* pNodeConfig;
    PipelineConfig const* pPipelineConfig;
    // Out
    IPipelineDAG* pDAG;
    IPipelineNodeMapControl* pNodeMap;
  };

  MERROR operator()(Params const& rParams);
};

/******************************************************************************
 *
 ******************************************************************************/
std::string* dump(IOMap const& rIomap);

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<HalImageStreamBufferPoolT> createHalStreamBufferPool(
    const char* username, std::shared_ptr<IImageStreamInfo> pStreamInfo);
/******************************************************************************
 *
 ******************************************************************************/

struct collect_from_NodeIOMaps {
  // collect information(StreamSet or NodeSet) from NodeIOMaps.
  MVOID getStreamSet(NodeIOMaps const& nodeIOMap, StreamSet* collected);
};

std::shared_ptr<IPipelineDAG> constructDAG(IPipelineDAG const* pConfigDAG,
                                           NodeSet const& rootNodes,
                                           NodeEdgeSet const& edges);

struct set_streaminfoset_from_config {
  struct Params {
    StreamSet const* pStreamSet;
    StreamConfig const* pStreamConfig;
    IStreamInfoSetControl* pSetControl;
  };
  //
  MERROR operator()(Params const& rParams);
};

//
struct collect_from_stream_config {
  struct Params {
    /********** in *********/
    StreamConfig const* pStreamConfig;
    StreamSet const* pvImageStreamSet;
    StreamSet const* pvMetaStreamSet;

    /********** out *********/
    ImageStreamInfoMapT* pvAppImageStreamInfo;
    ImageStreamInfoMapT* pvHalImageStreamInfo;
    MetaStreamInfoMapT* pvAppMetaStreamInfo;
    MetaStreamInfoMapT* pvHalMetaStreamInfo;
  };
  //
  MERROR operator()(Params const& rParams);
};

#define FRAME_STREAMINFO_DEBUG_ENABLE (0)
struct update_streaminfo_to_set {
  struct Params {
    // in
    ImageStreamInfoMapT const* pvAppImageStreamInfo;
    ImageStreamInfoMapT const* pvHalImageStreamInfo;
    MetaStreamInfoMapT const* pvAppMetaStreamInfo;
    MetaStreamInfoMapT const* pvHalMetaStreamInfo;
    // out
    IStreamInfoSetControl* pSetControl;
  };
  // update each IImageStreamInfo in InfoMap to IStreamInfoSetControl
  MERROR operator()(Params const& rParams);
};

#define FRAMENODEMAP_DEBUG_ENABLE (0)
struct construct_FrameNodeMapControl {
  struct Params {
    // in
    NodeIOMaps const* pImageNodeIOMaps;
    NodeIOMaps const* pMetaNodeIOMaps;
    IPipelineDAG const* const pReqDAG;
    IStreamInfoSet const* const pReqStreamInfoSet;
    // out
    IPipelineFrameNodeMapControl* pMapControl;
  };
  //
  MERROR operator()(Params const& rParams);
};

#define FRAMEE_STREAMBUFFER_DEBUG_ENABLE (0)
struct update_streambuffers_to_frame {
  typedef IPipelineBufferSetFrameControl PipelineFrameT;
  // Image:
  //      App: should have StreamBuffer
  //      Hal: could get StreamBuffer in later stage
  // Meta:
  //      App: control: should have StreamBuffer
  //           result: allocate here
  MERROR updateAppMetaSB(MetaStreamInfoMapT const& vStreamInfo,
                         MetaStreamBufferMapsT const& vSBuffers,
                         PipelineFrameT* pFrame) const;
  MERROR updateHalMetaSB(MetaStreamInfoMapT const& vStreamInfo,
                         HalMetaStreamBufferMapsT const& vSBuffers,
                         PipelineFrameT* pFrame) const;
  MERROR updateAppImageSB(ImageStreamInfoMapT const& vStreamInfo,
                          ImageStreamBufferMapsT const& vSBuffers,
                          PipelineFrameT* pFrame) const;
  MERROR updateHalImageSB(ImageStreamInfoMapT const& vStreamInfo,
                          HalImageStreamBufferMapsT const& vSBuffers,
                          PipelineFrameT* pFrame) const;
};

struct evaluate_buffer_users {
  struct Imp;
  // to evaluate the userGraph of each StreamBuffer
  struct Params {
    // in
    NodeConfig const* pProvider;
    IPipelineDAG const* pPipelineDAG;
    IPipelineFrameNodeMapControl const* pNodeMap;
    // out
    IPipelineBufferSetControl* pBufferSet;
  };
  MERROR operator()(Params* rParams);
};

};      // namespace NSPipelineContext
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_PIPELINE_PIPELINECONTEXTIMPL_H_
