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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_PIPELINE_PIPELINECONTEXT_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_PIPELINE_PIPELINECONTEXT_H_

#include <inttypes.h>
#include <memory>
#include <string>
#include <vector>
#include "IPipelineNode.h"
#include "IPipelineBufferSetFrameControl.h"
#include <mtkcam/pipeline/utils/streambuf/StreamBuffers.h>
#include <mtkcam/pipeline/utils/streambuf/StreamBufferProvider.h>
#include <mtkcam/utils/hw/IScenarioControl.h>
#include <mtkcam/pipeline/utils/SyncHelper/ISyncHelper.h>
#include <base/strings/stringprintf.h>

/******************************************************************************
 *
 ******************************************************************************/
//
namespace NSCam {
namespace v3 {
namespace NSPipelineContext {

typedef NSCam::v3::StreamId_T StreamId_T;
typedef NSCam::v3::Set<StreamId_T> StreamSet;
typedef NSCam::v3::NodeSet NodeSet;

enum StreamCategory {
  // bit 0~3: behavior
  //    bit 3: app or hal
  eBehavior_HAL = 0x0,
  eBehavior_APP = 0x8,
  eCategory_Behavior_MASK = 0x8,
  //    bit 0~2: hal subset
  eBehavior_HAL_POOL = 0x1 | eBehavior_HAL,
  eBehavior_HAL_RUNTIME = 0x2 | eBehavior_HAL,
  eBehavior_HAL_PROVIDER = 0x3 | eBehavior_HAL,
  eCategory_Behavior_HAL_MASK = 0xF,
  //
  // bit 4: type
  eType_IMAGE = 0x0,
  eType_META = 0x10,
  eCategory_Type_MASK = 0x10,
};

#define BehaviorOf(StreamType) (StreamType & eCategory_Behavior_MASK)
#define HalBehaviorOf(StreamType) (StreamType & eCategory_Behavior_HAL_MASK)
#define TypeOf(StreamType) (StreamType & eCategory_Type_MASK)

enum eStreamType {
  /* image */
  // always have streambuffer in request stage
  eStreamType_IMG_APP = eType_IMAGE | eBehavior_APP,
  // allocate bufferpool in config stage, get streambuffer from pool
  eStreamType_IMG_HAL_POOL = eType_IMAGE | eBehavior_HAL_POOL,
  // always no streambuffer in request stage, size may changed in run-time
  eStreamType_IMG_HAL_RUNTIME = eType_IMAGE | eBehavior_HAL_RUNTIME,
  // hal stream with specified provider
  eStreamType_IMG_HAL_PROVIDER = eType_IMAGE | eBehavior_HAL_PROVIDER,

  // FIXME, workaroud here
  // Current implementation of hal metadata is going to be phased out.
  eStreamType_META_APP = eType_META | eBehavior_APP,
  eStreamType_META_HAL = eType_META | eBehavior_HAL,
};
//

/******************************************************************************
 *
 ******************************************************************************/
class INodeActor {
 public:
  enum {
    // eNodeState_None,
    eNodeState_Create,
    eNodeState_Init,
    eNodeState_Config,
  };
  //
 public:
  virtual ~INodeActor() {}
  // protected:
  explicit INodeActor(MUINT32 st) : muStatus(st) {}

 public:
  MUINT32 getStatus() const;

 public:
  MERROR init();
  MERROR config();
  MERROR uninit();

 protected:  // template methods: to be implmented by derived class
  virtual MERROR onInit() = 0;
  virtual MERROR onConfig() = 0;
  virtual MERROR onUninit() = 0;

 public:
  virtual std::shared_ptr<IPipelineNode> getNode() = 0;
  //
 private:
  mutable std::mutex mLock;
  MUINT32 muStatus;
};

template <class _Node_>
class NodeActor : public INodeActor {
 public:
  typedef _Node_ NodeT;
  typedef typename _Node_::InitParams InitParamsT;
  typedef typename _Node_::ConfigParams ConfigParamsT;

 public:
  NodeActor(std::shared_ptr<NodeT> pNode,
            MUINT32 const rNodeState = eNodeState_Create)
      : INodeActor(rNodeState), mpNode(pNode) {}

 protected:
  virtual MERROR onInit() { return mpNode->init(mInitParam); }
  virtual MERROR onConfig() { return mpNode->config(mConfigParam); }
  virtual MERROR onUninit() { return mpNode->uninit(); }

 public:
  virtual std::shared_ptr<IPipelineNode> getNode() { return mpNode; }
  virtual NodeT* getNodeImpl() { return mpNode.get(); }
  virtual ~NodeActor() { onUninit(); }

 public:
  MVOID setInitParam(InitParamsT const& rParam) { mInitParam = rParam; }
  MVOID setConfigParam(ConfigParamsT const& rParam) { mConfigParam = rParam; }
  MVOID getInitParam(InitParamsT* rParam) { *rParam = mInitParam; }
  MVOID getConfigParam(ConfigParamsT* rParam) { *rParam = mConfigParam; }

 private:
  std::shared_ptr<NodeT> mpNode;
  InitParamsT mInitParam;
  ConfigParamsT mConfigParam;
};

class IDataCallback {
 public:
  virtual MVOID onImageCallback(
      MUINT32 requestNo,
      Pipeline_NodeId_T nodeId,
      StreamId_T streamId,
      std::shared_ptr<IImageBufferHeap> const& rpImageBuffer,
      std::shared_ptr<IImageStreamInfo> const& rpStreamInfo,
      MBOOL errorResult = MFALSE) = 0;
  virtual MVOID onMetaCallback(MUINT32 requestNo,
                               Pipeline_NodeId_T nodeId,
                               StreamId_T streamId,
                               IMetadata const& rMetaData,
                               MBOOL errorResult = MFALSE) = 0;

  //
  virtual MBOOL isCtrlSetting() = 0;
  virtual MVOID onCtrlSetting(MUINT32 requestNo,
                              Pipeline_NodeId_T nodeId,
                              StreamId_T const metaAppStreamId,
                              IMetadata const& rAppMetaData,
                              StreamId_T const metaHalStreamId,
                              IMetadata const& rHalMetaData,
                              MBOOL const& rIsChanged) = 0;
  //
  virtual MBOOL isCtrlSync() = 0;
  virtual MVOID onCtrlSync(MUINT32 requestNo,
                           Pipeline_NodeId_T nodeId,
                           MUINT32 index,
                           MUINT32 type,
                           MINT64 duration) = 0;
  //
  virtual MBOOL isCtrlResize() = 0;
  virtual MVOID onCtrlResize(MUINT32 requestNo,
                             Pipeline_NodeId_T nodeId,
                             StreamId_T const metaAppStreamId,
                             IMetadata const& rAppMetaData,
                             StreamId_T const metaHalStreamId,
                             IMetadata const& rHalMetaData,
                             MBOOL const& rIsChanged) = 0;
  //
  virtual MBOOL isCtrlReadout() = 0;
  virtual MVOID onCtrlReadout(MUINT32 requestNo,
                              Pipeline_NodeId_T nodeId,
                              StreamId_T const metaAppStreamId,
                              IMetadata const& rAppMetaData,
                              StreamId_T const metaHalStreamId,
                              IMetadata const& rHalMetaData,
                              MBOOL const& rIsChanged) = 0;
  //
  virtual MVOID onNextCaptureCallBack(MUINT32 requestNo,
                                      Pipeline_NodeId_T nodeId) = 0;
};

class DataCallbackBase : public virtual IDataCallback {
 public:
  virtual MVOID onImageCallback(
      MUINT32 /*requestNo*/,
      Pipeline_NodeId_T /*nodeId*/,
      StreamId_T /*streamId*/,
      std::shared_ptr<IImageBufferHeap> const& /*rpImageBuffer*/,
      std::shared_ptr<IImageStreamInfo> const& /*rpStreamInfo*/,
      MBOOL /*errorResult = MFALSE*/
  ) {}

  virtual MVOID onMetaCallback(MUINT32 /*requestNo*/,
                               Pipeline_NodeId_T /*nodeId*/,
                               StreamId_T /*streamId*/,
                               IMetadata const& /*rMetaData*/,
                               MBOOL /*errorResult = MFALSE*/
  ) {}

  //
  virtual MBOOL isCtrlSetting() { return MFALSE; }
  virtual MVOID onCtrlSetting(MUINT32 /*requestNo*/,
                              Pipeline_NodeId_T /*nodeId*/,
                              StreamId_T const /*metaAppStreamId*/,
                              IMetadata const& /*rAppMetaData*/,
                              StreamId_T const /*metaHalStreamId*/,
                              IMetadata const& /*rHalMetaData*/,
                              MBOOL const& /*rIsChanged*/
  ) {}
  //
  virtual MBOOL isCtrlSync() { return MFALSE; }
  virtual MVOID onCtrlSync(MUINT32 /*requestNo*/,
                           Pipeline_NodeId_T /*nodeId*/,
                           MUINT32 /*index*/,
                           MUINT32 /*type*/,
                           MINT64 /*duration*/
  ) {}
  //
  virtual MBOOL isCtrlResize() { return MFALSE; }
  virtual MVOID onCtrlResize(MUINT32 /*requestNo*/,
                             Pipeline_NodeId_T /*nodeId*/,
                             StreamId_T const /*metaAppStreamId*/,
                             IMetadata const& /*rAppMetaData*/,
                             StreamId_T const /*metaHalStreamId*/,
                             IMetadata const& /*rHalMetaData*/,
                             MBOOL const& /*rIsChanged*/
  ) {}
  //
  virtual MBOOL isCtrlReadout() { return MFALSE; }
  virtual MVOID onCtrlReadout(MUINT32 /*requestNo*/,
                              Pipeline_NodeId_T /*nodeId*/,
                              StreamId_T const /*metaAppStreamId*/,
                              IMetadata const& /*rAppMetaData*/,
                              StreamId_T const /*metaHalStreamId*/,
                              IMetadata const& /*rHalMetaData*/,
                              MBOOL const& /*rIsChanged*/
  ) {}
  virtual MVOID onNextCaptureCallBack(MUINT32 /*requestNo*/,
                                      Pipeline_NodeId_T /*nodeId*/
  ) {}
};

class IDispatcher : public virtual IPipelineNodeCallback {
 public:
  virtual MVOID beginFlush() = 0;
  virtual MVOID endFlush() = 0;
  virtual MERROR setDataCallback(std::weak_ptr<IDataCallback> pCallback) = 0;

 public:  ////    interface of IPipelineNodeCallback
  virtual MVOID onDispatchFrame(std::shared_ptr<IPipelineFrame> const& pFrame,
                                Pipeline_NodeId_T nodeId) = 0;
  virtual MVOID onEarlyCallback(MUINT32 requestNo,
                                Pipeline_NodeId_T nodeId,
                                StreamId_T streamId,
                                IMetadata const& rMetaData,
                                MBOOL errorResult = MFALSE) = 0;
  // Control-Callback
  virtual MVOID onCtrlSetting(MUINT32 requestNo,
                              Pipeline_NodeId_T nodeId,
                              StreamId_T const metaAppStreamId,
                              IMetadata const& rAppMetaData,
                              StreamId_T const metaHalStreamId,
                              IMetadata const& rHalMetaData,
                              MBOOL const& rIsChanged) = 0;
  virtual MVOID onCtrlSync(MUINT32 requestNo,
                           Pipeline_NodeId_T nodeId,
                           MUINT32 index,
                           MUINT32 type,
                           MINT64 duration) = 0;
  virtual MVOID onCtrlResize(MUINT32 requestNo,
                             Pipeline_NodeId_T nodeId,
                             StreamId_T const metaAppStreamId,
                             IMetadata const& rAppMetaData,
                             StreamId_T const metaHalStreamId,
                             IMetadata const& rHalMetaData,
                             MBOOL const& rIsChanged) = 0;
  virtual MVOID onCtrlReadout(MUINT32 requestNo,
                              Pipeline_NodeId_T nodeId,
                              StreamId_T const metaAppStreamId,
                              IMetadata const& rAppMetaData,
                              StreamId_T const metaHalStreamId,
                              IMetadata const& rHalMetaData,
                              MBOOL const& rIsChanged) = 0;
  virtual MBOOL needCtrlCb(eCtrlType eType) = 0;
  // for Fast S2S callback
  virtual MVOID onNextCaptureCallBack(MUINT32 requestNo,
                                      Pipeline_NodeId_T nodeId) = 0;
};

class DispatcherBase : public virtual IDispatcher {
 public:
  DispatcherBase() {
    mInFlush = MFALSE;
    pthread_rwlock_init(&mFlushLock, NULL);
  }
  ~DispatcherBase() { pthread_rwlock_destroy(&mFlushLock); }

 public:  ////    interface of IDispatcher
  virtual MVOID beginFlush() {
    pthread_rwlock_wrlock(&mFlushLock);
    mInFlush = MTRUE;
    pthread_rwlock_unlock(&mFlushLock);
  }
  virtual MVOID endFlush() {
    pthread_rwlock_wrlock(&mFlushLock);
    mInFlush = MFALSE;
    pthread_rwlock_unlock(&mFlushLock);
  }
  MERROR setDataCallback(std::weak_ptr<IDataCallback> pCallback) {
    mpDataCallback = pCallback;
    return 0;
  }

 public:  ////    interface of IPipelineNodeCallback
  virtual MVOID onDispatchFrame(std::shared_ptr<IPipelineFrame> const& pFrame,
                                Pipeline_NodeId_T nodeId) = 0;

  MVOID onEarlyCallback(MUINT32 requestNo,
                        Pipeline_NodeId_T nodeId,
                        StreamId_T streamId,
                        IMetadata const& rMetaData,
                        MBOOL errorResult = MFALSE);

  // Control-Callback
  virtual MVOID onCtrlSetting(MUINT32 requestNo,
                              Pipeline_NodeId_T nodeId,
                              StreamId_T const metaAppStreamId,
                              IMetadata const& rAppMetaData,
                              StreamId_T const metaHalStreamId,
                              IMetadata const& rHalMetaData,
                              MBOOL const& rIsChanged);
  virtual MVOID onCtrlSync(MUINT32 requestNo,
                           Pipeline_NodeId_T nodeId,
                           MUINT32 index,
                           MUINT32 type,
                           MINT64 duration);
  virtual MVOID onCtrlResize(MUINT32 requestNo,
                             Pipeline_NodeId_T nodeId,
                             StreamId_T const metaAppStreamId,
                             IMetadata const& rAppMetaData,
                             StreamId_T const metaHalStreamId,
                             IMetadata const& rHalMetaData,
                             MBOOL const& rIsChanged);
  virtual MVOID onCtrlReadout(MUINT32 requestNo,
                              Pipeline_NodeId_T nodeId,
                              StreamId_T const metaAppStreamId,
                              IMetadata const& rAppMetaData,
                              StreamId_T const metaHalStreamId,
                              IMetadata const& rHalMetaData,
                              MBOOL const& rIsChanged);
  MBOOL needCtrlCb(eCtrlType eType);
  // for Fast S2S callback
  virtual MVOID onNextCaptureCallBack(MUINT32 requestNo,
                                      Pipeline_NodeId_T nodeId);

 protected:
  mutable pthread_rwlock_t mFlushLock;
  MBOOL mInFlush;

 private:
  std::weak_ptr<IDataCallback> mpDataCallback;
};

class VISIBILITY_PUBLIC PipelineContext {
  friend class StreamBuilder;
  friend class NodeBuilder;
  friend class PipelineBuilder;
  friend class RequestBuilder;

 public:
  typedef NSCam::v3::Utils::HalImageStreamBuffer HalImageStreamBuffer;
  typedef HalImageStreamBuffer::Allocator HalImageStreamBufferAllocatorT;
  typedef HalImageStreamBufferAllocatorT::StreamBufferPoolT
      HalImageStreamBufferPoolT;
  //
 public:
  static std::shared_ptr<PipelineContext> create(char const* name);
  //
 public:
  explicit PipelineContext(char const* name);
  virtual ~PipelineContext();
  virtual void onLastStrongRef(const void* id);

 public:
  char const* getName() const;

 public:
  MERROR beginConfigure(std::shared_ptr<PipelineContext> oldContext = NULL);
  MERROR endConfigure(MBOOL asyncConfig = MFALSE);

 public:
  MERROR queue(std::shared_ptr<IPipelineFrame> const& pFrame);
  MERROR kick(std::shared_ptr<IPipelineFrame> const& pFrame);
  MERROR flush();
  MERROR waitUntilDrained();
  MERROR waitUntilNodeDrained(NodeId_T const nodeId);
  //
 public:
  MERROR setScenarioControl(std::shared_ptr<IScenarioControl> pControl);
  std::shared_ptr<IScenarioControl> getScenarioControl() const;
  //
  MERROR setDispatcher(std::weak_ptr<IDispatcher> pDispatcher);
  MERROR setDataCallback(std::weak_ptr<IDataCallback> pCallback);
  //
  template <typename _Node_>
  MERROR queryNodeActor(NodeId_T const nodeId,
                        std::shared_ptr<NodeActor<_Node_> >* pNodeActor) {
    typedef NodeActor<_Node_> NodeActorT;
    typedef std::shared_ptr<INodeActor> SP_INodeActorT;
    //
    SP_INodeActorT pINodeActor = queryINodeActor(nodeId);
    if (!pINodeActor.get()) {
      return -ENOENT;
    }
    *pNodeActor = std::dynamic_pointer_cast<NodeActorT>(pINodeActor);

    return 0;
  }
  //
 public:  // FIXME: workaround to get pool
  std::shared_ptr<HalImageStreamBufferPoolT> queryImageStreamPool(
      StreamId_T const streamId) const;

  std::shared_ptr<PipelineContext> queryOldContext() const {
    return mpOldContext;
  }

  /**
   * Query a image stream info.
   *
   * @param[in] streamId: id of stream for quried.
   *
   * @param[in] pStreamInfo:
   *            caller set NULL;.callee must promise its value
   *
   * @return
   *      0 indicates success, no matter it exists or not;
   *      otherwise failure.
   */
  MERROR queryStream(StreamId_T const streamId,
                     std::shared_ptr<IImageStreamInfo>* pStreamInfo);

  /**
   * Reuse a image stream from old context StreamConfig.
   *
   * @param[in] pStreamInfo:
   *            caller must promise its value
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  MERROR reuseStream(std::shared_ptr<IImageStreamInfo>* pStreamInfo);

  MERROR reuseNode(NodeId_T const nodeId);

 public:
  std::shared_ptr<INodeActor> queryINodeActor(NodeId_T const nodeId) const;

 public:
  /**
   * Dump debugging state.
   */
  auto dumpState(const std::vector<std::string>& options) -> void;
  MVOID dump();
  uint32_t getFrameNo();

 public:  // for multi-cam sync used
  typedef NSCam::v3::Utils::Imp::ISyncHelper MultiCamSyncHelper;
  MVOID setMultiCamSyncHelper(
      std::shared_ptr<MultiCamSyncHelper> const& helper);
  std::shared_ptr<MultiCamSyncHelper> getMultiCamSyncHelper();

 protected:  //      Pipeline's status
  enum ContextState {
    eContextState_empty,
    eContextState_configuring,
    eContextState_configured,
  };
  MUINT32 getState() const { return 0; }

 private:
  mutable std::mutex mLock;
  std::shared_ptr<PipelineContext> mpOldContext;

  class PipelineContextImpl;
  std::shared_ptr<PipelineContextImpl> mpImpl;

 private:
  PipelineContextImpl* getImpl() const;

 private:  // for multi-cam sync used
  std::shared_ptr<MultiCamSyncHelper> mpSyncHelper = nullptr;
};

class StreamBuilderImpl;
class VISIBILITY_PUBLIC StreamBuilder {
  typedef NSCam::v3::Utils::IStreamBufferProvider IStreamBufferProviderT;

 public:
  StreamBuilder(eStreamType const type,
                std::shared_ptr<IImageStreamInfo> pStreamInfo);

  StreamBuilder(eStreamType const type,
                std::shared_ptr<IMetaStreamInfo> pStreamInfo);

  StreamBuilder(const StreamBuilder& builder);

  ~StreamBuilder();

 public:
  StreamBuilder& setProvider(std::shared_ptr<IStreamBufferProviderT> pProvider);

  MERROR build(std::shared_ptr<PipelineContext> pContext) const;

 private:
  std::shared_ptr<StreamBuilderImpl> mpImpl;
};

/******************************************************************************
 *
 ******************************************************************************/
class NodeBuilderImpl;
class VISIBILITY_PUBLIC NodeBuilder {
 public:
  typedef enum Direction {
    // direction
    eDirection_IN = 0x0,
    eDirection_OUT = 0x1,
  } eDirection;

 public:
  NodeBuilder(NodeId_T const nodeId, std::shared_ptr<INodeActor> pNode);
  NodeBuilder(const NodeBuilder& builder);
  ~NodeBuilder();

 public:
  NodeBuilder& addStream(eDirection const direction, StreamSet const& streams);
  NodeBuilder& setImageStreamUsage(StreamId_T const streamId,
                                   MUINT const bufUsage);
  MERROR build(std::shared_ptr<PipelineContext> pContext) const;

 private:
  std::shared_ptr<NodeBuilderImpl> mpImpl;
};

/******************************************************************************
 *
 ******************************************************************************/
struct NodeEdge {
  NodeId_T src;
  NodeId_T dst;
};

inline bool operator==(NodeEdge const& lhs, NodeEdge const& rhs) {
  return lhs.src == rhs.src && lhs.dst == rhs.dst;
}

class NodeEdgeSet : public Set<NodeEdge> {
 public:
  typedef std::vector<NodeEdge>::iterator iterator;
  typedef std::vector<NodeEdge>::const_iterator const_iterator;

 public:
  NodeEdgeSet& addEdge(NodeId_T const src, NodeId_T const dst) {
    struct NodeEdge e = {src, dst};
    Set<NodeEdge>::add(e);
    return *this;
  }
};

/******************************************************************************
 *
 ******************************************************************************/
class PipelineBuilderImpl;
class VISIBILITY_PUBLIC PipelineBuilder {
 public:
  PipelineBuilder();
  PipelineBuilder(const PipelineBuilder& builder);
  ~PipelineBuilder();

 public:
  MERROR build(std::shared_ptr<PipelineContext> pContext) const;

 public:
  PipelineBuilder& setRootNode(NodeSet const& roots);

  PipelineBuilder& setNodeEdges(NodeEdgeSet const& edges);

 private:
  std::shared_ptr<PipelineBuilderImpl> mpImpl;
};

/******************************************************************************
 *
 ******************************************************************************/
struct IOMap {
  StreamSet vIn;
  StreamSet vOut;
  //
  MBOOL isEmpty() const { return vIn.size() == 0 && vOut.size() == 0; }
  //
  IOMap& addIn(StreamId_T const stream) {
    vIn.add(stream);
    return *this;
  }
  IOMap& addOut(StreamId_T const stream) {
    vOut.add(stream);
    return *this;
  }
  IOMap& addIn(StreamSet const& stream) {
    vIn.add(stream);
    return *this;
  }
  IOMap& addOut(StreamSet const& stream) {
    vOut.add(stream);
    return *this;
  }
  size_t sizeIn() { return vIn.size(); }
  size_t sizeOut() { return vOut.size(); }
};

struct VISIBILITY_PUBLIC IOMapSet : public std::vector<IOMap> {
  typedef typename std::vector<IOMap>::iterator iterator;
  typedef typename std::vector<IOMap>::const_iterator const_iterator;
  //
  IOMapSet& add(IOMap const& map) {
    this->push_back(map);
    return *this;
  }

  static const IOMapSet& buildEmptyIOMap();
};

/******************************************************************************
 *
 ******************************************************************************/
class RequestBuilderImpl;
class VISIBILITY_PUBLIC RequestBuilder {
 public:
  typedef IPipelineBufferSetFrameControl::IAppCallback AppCallbackT;

  typedef NSCam::v3::Utils::HalImageStreamBuffer HalImageStreamBuffer;
  typedef NSCam::v3::Utils::HalMetaStreamBuffer HalMetaStreamBuffer;

 public:
  RequestBuilder();
  ~RequestBuilder();

 public:
  std::shared_ptr<IPipelineFrame> build(
      MUINT32 const requestNo, std::shared_ptr<PipelineContext> pContext);

 public:
  RequestBuilder& setReprocessFrame(MBOOL const bReprocessFrame);

  RequestBuilder& setIOMap(NodeId_T const nodeId,
                           IOMapSet const& imageIOMap,
                           IOMapSet const& metaIOMap);

  RequestBuilder& setRootNode(NodeSet const& roots);

  RequestBuilder& setNodeEdges(NodeEdgeSet const& edges);

  /* provide new IImageStreamInfo to overwrite the previously configured one. */
  RequestBuilder& replaceStreamInfo(
      StreamId_T const streamId, std::shared_ptr<IImageStreamInfo> pStreamInfo);

  /* provide stream buffer if existed */
  RequestBuilder& setImageStreamBuffer(
      StreamId_T const streamId, std::shared_ptr<IImageStreamBuffer> buffer);
  // FIXME: workaround. Should not use Hal...
  RequestBuilder& setImageStreamBuffer(
      StreamId_T const streamId, std::shared_ptr<HalImageStreamBuffer> buffer);
  RequestBuilder& setMetaStreamBuffer(
      StreamId_T const streamId, std::shared_ptr<IMetaStreamBuffer> buffer);
  // FIXME: workaround. Should not use Hal...
  RequestBuilder& setMetaStreamBuffer(
      StreamId_T const streamId, std::shared_ptr<HalMetaStreamBuffer> buffer);
  RequestBuilder& updateFrameCallback(std::weak_ptr<AppCallbackT> pCallback);

 private:
  std::shared_ptr<RequestBuilderImpl> mpImpl;
};

/******************************************************************************
 *
 ******************************************************************************/
static inline std::string toString(
    const NSCam::v3::NSPipelineContext::IOMapSet& o) {
  std::string os;
  os += "{ ";
  for (size_t i = 0; i < o.size(); i++) {
    auto const& iomap = o[i];
    os += "( ";
    for (size_t j = 0; j < iomap.vIn.size(); j++) {
      auto const& streamId = iomap.vIn[j];
      os += base::StringPrintf("%#" PRIx64 " ", streamId);
    }
    os += "-> ";
    for (size_t j = 0; j < iomap.vOut.size(); j++) {
      auto const& streamId = iomap.vOut[j];
      os += base::StringPrintf("%#" PRIx64 " ", streamId);
    }
    os += ")";
  }
  os += " }";
  return os;
}

/******************************************************************************
 *
 ******************************************************************************/
static inline std::string toString(
    const NSCam::v3::NSPipelineContext::NodeSet& o) {
  std::string os;
  os += "{ ";
  for (size_t i = 0; i < o.size(); i++) {
    auto const& v = o[i];
    os += base::StringPrintf("%#" PRIxPTR " ", v);
  }
  os += "}";
  return os;
}

/******************************************************************************
 *
 ******************************************************************************/
static inline std::string toString(
    const NSCam::v3::NSPipelineContext::NodeEdgeSet& o) {
  std::string os;
  os += "{ ";
  for (size_t i = 0; i < o.size(); i++) {
    auto const& v = o[i];
    os += base::StringPrintf("(%#" PRIxPTR " -> %#" PRIxPTR ") ", v.src, v.dst);
  }
  os += "}";
  return os;
}

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSPipelineContext
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_PIPELINE_PIPELINECONTEXT_H_
