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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_PIPELINE_IPIPELINEBUFFERSETFRAMECONTROL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_PIPELINE_IPIPELINEBUFFERSETFRAMECONTROL_H_

#include <mtkcam/pipeline/stream/IStreamBufferSet.h>
#include <mtkcam/pipeline/utils/streambuf/StreamBuffers.h>
#include "IPipelineNode.h"
//
#include <memory>
#include <vector>
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {

/**
 * An interface of pipeline stream buffer provider.
 */
struct IPipelineStreamBufferProvider {
 public:  ////                Definitions.
  typedef NSCam::v3::Utils::HalImageStreamBuffer HalImageStreamBuffer;

 public:  ////                Operations.
  virtual MERROR acquireHalStreamBuffer(
      MUINT32 const requestNo,
      std::shared_ptr<IImageStreamInfo> const pStreamInfo,
      std::shared_ptr<HalImageStreamBuffer>* rpStreamBuffer) const = 0;
  virtual ~IPipelineStreamBufferProvider() {}
};

/**
 * An interface of pipeline frame node map.
 */
class IPipelineFrameNodeMapControl {
 public:  ////
  typedef IPipelineFrameNodeMapControl ThisT;
  typedef IPipelineFrame::NodeId_T NodeId_T;
  typedef IPipelineFrame::InfoIOMapSet InfoIOMapSet;
  typedef IStreamInfoSet IStreamInfoSetT;
  typedef std::shared_ptr<IStreamInfoSetT> IStreamInfoSetPtr;
  typedef std::shared_ptr<IStreamInfoSetT const> IStreamInfoSetPtr_CONST;

 public:  ////                Definitions.
  /**
   *
   */
  struct INode {
    virtual NodeId_T getNodeId() const = 0;

    virtual IStreamInfoSetPtr_CONST getIStreams() const = 0;
    virtual MVOID setIStreams(IStreamInfoSetPtr p) = 0;

    virtual IStreamInfoSetPtr_CONST getOStreams() const = 0;
    virtual MVOID setOStreams(IStreamInfoSetPtr p) = 0;

    virtual InfoIOMapSet const& getInfoIOMapSet() const = 0;
    virtual InfoIOMapSet& editInfoIOMapSet() = 0;
    virtual ~INode() {}
  };

 public:  ////                Operations.
  static std::shared_ptr<ThisT> create();
  virtual MVOID clear() = 0;
  virtual ssize_t addNode(NodeId_T const nodeId) = 0;

 public:  ////                Operations.
  virtual MBOOL isEmpty() const = 0;
  virtual size_t size() const = 0;

  virtual std::shared_ptr<INode> getNodeFor(NodeId_T const nodeId) const = 0;
  virtual std::shared_ptr<INode> getNodeAt(size_t index) const = 0;
  virtual ~IPipelineFrameNodeMapControl() {}
};

/**
 * An interface of pipeline buffer set control.
 */
class IPipelineBufferSetControl : public IStreamBufferSet {
 public:  ////                Definitions.
  typedef NSCam::v3::Utils::HalImageStreamBuffer HalImageStreamBuffer;
  typedef NSCam::v3::Utils::HalMetaStreamBuffer HalMetaStreamBuffer;

 public:  ////                Definitions.
  /**
   * Stream Buffer Map Interface.
   */
  template <class _StreamBuffer_>
  class IMap {
   public:  ////            Definitions.
    typedef _StreamBuffer_ StreamBufferT;
    typedef typename StreamBufferT::IStreamInfoT IStreamInfoT;

   public:  ////            Operations.
    virtual ssize_t add(std::shared_ptr<IStreamInfoT> pStreamInfo,
                        std::shared_ptr<IUsersManager> pUsersManager) = 0;

    virtual ssize_t add(std::shared_ptr<StreamBufferT> value) = 0;

    virtual ssize_t setCapacity(size_t size) = 0;

    virtual bool isEmpty() const = 0;

    virtual size_t size() const = 0;

    virtual ssize_t indexOfKey(StreamId_T const key) const = 0;

    virtual StreamId_T keyAt(size_t index) const = 0;

    virtual std::shared_ptr<IUsersManager> usersManagerAt(
        size_t index) const = 0;

    virtual std::shared_ptr<IStreamInfoT> streamInfoAt(size_t index) const = 0;

    virtual ~IMap() {}
  };

 public:  ////                Operations.
  virtual std::shared_ptr<IMap<IImageStreamBuffer> > editMap_AppImage() = 0;

  virtual std::shared_ptr<IMap<IMetaStreamBuffer> > editMap_AppMeta() = 0;

  virtual std::shared_ptr<IMap<HalImageStreamBuffer> > editMap_HalImage() = 0;

  virtual std::shared_ptr<IMap<HalMetaStreamBuffer> > editMap_HalMeta() = 0;

 public:  ////                Operations.
  virtual MUINT32 getFrameNo() const = 0;
  virtual ~IPipelineBufferSetControl() {}
};

/**
 * An interface of (in-flight) pipeline frame control.
 */
class IPipelineBufferSetFrameControl : public IPipelineFrame,
                                       public IPipelineBufferSetControl {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  typedef IPipelineBufferSetFrameControl ThisT;

 public:  ////
  typedef NSCam::v3::Utils::HalImageStreamBuffer HalImageStreamBuffer;
  typedef NSCam::v3::Utils::HalMetaStreamBuffer HalMetaStreamBuffer;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                Callback.
  /**
   *
   */
  virtual ~IPipelineBufferSetFrameControl() {}
  class IAppCallback {
   public:
    /*
     * Result structure used in updateFrame(...)
     */
    struct Result {
      uint32_t frameNo;
      ssize_t nAppOutMetaLeft;
      std::vector<std::shared_ptr<IMetaStreamBuffer> > vAppOutMeta;
      ssize_t nHalOutMetaLeft;
      std::vector<std::shared_ptr<IMetaStreamBuffer> > vHalOutMeta;
      bool bFrameEnd;
    };

   public:  ////            Operations.
    /*
     * Invoked when some node update the results.
     *
     * @param[in] requestNo: the request number.
     *
     * @param[in] userId: for debug only, SHOULD NOT be used.
     * This userId cannot be mapped to certain expected result metadata
     * streams. This is left to debug if each user(a.k.a, node) has done
     * callback.
     *
     * @param[in] result: the result metadata, including app/hal meta.
     */
    virtual MVOID updateFrame(MUINT32 const requestNo,
                              MINTPTR const userId,
                              Result const& result) = 0;
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Operations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                Creation.
  static std::shared_ptr<ThisT> create(
      MUINT32 requestNo,
      MUINT32 frameNo,
      MBOOL bReporcessFrame,
      std::weak_ptr<IAppCallback> const& pAppCallback,
      std::shared_ptr<IPipelineStreamBufferProvider> pBufferProvider,
      std::weak_ptr<IPipelineNodeCallback> pNodeCallback);

 public:  ////                Configuration.
  virtual MERROR startConfiguration() = 0;
  virtual MERROR finishConfiguration() = 0;

  virtual MERROR setNodeMap(
      std::shared_ptr<IPipelineFrameNodeMapControl> value) = 0;

  virtual MERROR setPipelineNodeMap(
      std::shared_ptr<IPipelineNodeMap const> value) = 0;

  virtual MERROR setPipelineDAG(std::shared_ptr<IPipelineDAG> value) = 0;

  virtual MERROR setStreamInfoSet(
      std::shared_ptr<IStreamInfoSet const> value) = 0;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_PIPELINE_IPIPELINEBUFFERSETFRAMECONTROL_H_
