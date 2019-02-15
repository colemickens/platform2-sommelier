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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_PIPELINE_IPIPELINENODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_PIPELINE_IPIPELINENODE_H_
//
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <mtkcam/def/common.h>
#include <mtkcam/pipeline/stream/IStreamBufferSet.h>
#include <mtkcam/pipeline/stream/IStreamInfo.h>
#include "IPipelineDAG.h"

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {

/******************************************************************************
 *
 ******************************************************************************/
class IPipelineNodeMap;
class IPipelineFrame;
class IPipelineNode;
class IPipelineDAG;

/**
 * Type of Camera Pipeline Node Id.
 */
typedef MINTPTR Pipeline_NodeId_T;

/**
 * An interface of Pipeline node map (key:NodeId_T, value:NodePtr_T).
 */
class IPipelineNodeMap {
 public:  ////                    Definitions.
  typedef Pipeline_NodeId_T NodeId_T;
  typedef IPipelineNode NodeT;
  typedef std::shared_ptr<NodeT> NodePtrT;

 public:  ////                    Operations.
  virtual MBOOL isEmpty() const = 0;
  virtual size_t size() const = 0;

  virtual NodePtrT nodeFor(NodeId_T const& id) const = 0;
  virtual NodePtrT nodeAt(size_t index) const = 0;
  virtual ~IPipelineNodeMap() {}
};

/**
 * An interface of (in-flight) pipeline frame listener.
 */
class IPipelineFrameListener {
 public:  ////                    Definitions.
  typedef Pipeline_NodeId_T NodeId_T;

  enum {
    /** This frame is released */
    eMSG_FRAME_RELEASED,

    /** All output meta buffers released */
    eMSG_ALL_OUT_META_BUFFERS_RELEASED,

    /** All output image buffers released */
    eMSG_ALL_OUT_IMAGE_BUFFERS_RELEASED,
  };

  /**
   * Invoked when something happens.
   *
   * @param[in] frameNo: frame number.
   *
   * @param[in] message: a message to indicate what happen.
   *
   * @param[in] pCookie: the listener's cookie.
   */
  virtual MVOID onPipelineFrame(MUINT32 const frameNo,
                                MUINT32 const message,
                                MVOID* const pCookie) = 0;

  /**
   * Invoked when something happens.
   *
   * @param[in] frameNo: frame number.
   *
   * @param[in] nodeId: node ID.
   *
   * @param[in] message: a message to indicate what happen.
   *
   * @param[in] pCookie: the listener's cookie.
   */
  virtual MVOID onPipelineFrame(MUINT32 const frameNo,
                                NodeId_T const nodeId,
                                MUINT32 const message,
                                MVOID* const pCookie) = 0;
  virtual ~IPipelineFrameListener() {}
};

struct IPipelineNodeCallback;
/**
 * An interface of (in-flight) pipeline frame.
 */
class IPipelineFrame {
 public:  ////                    Definitions.
  typedef Pipeline_NodeId_T NodeId_T;

  struct ImageInfoIOMap {
    std::map<StreamId_T, std::shared_ptr<IImageStreamInfo> > vIn;
    std::map<StreamId_T, std::shared_ptr<IImageStreamInfo> > vOut;
  };

  struct MetaInfoIOMap {
    std::map<StreamId_T, std::shared_ptr<IMetaStreamInfo> > vIn;
    std::map<StreamId_T, std::shared_ptr<IMetaStreamInfo> > vOut;
  };

  struct ImageInfoIOMapSet : public std::vector<ImageInfoIOMap> {};

  struct MetaInfoIOMapSet : public std::vector<MetaInfoIOMap> {};

  struct InfoIOMapSet {
    typedef IPipelineFrame::ImageInfoIOMapSet ImageInfoIOMapSet;
    typedef IPipelineFrame::MetaInfoIOMapSet MetaInfoIOMapSet;
    ImageInfoIOMapSet mImageInfoIOMapSet;
    MetaInfoIOMapSet mMetaInfoIOMapSet;
  };

 public:  ////                    Operations.
  virtual MUINT32 getFrameNo() const = 0;
  virtual MUINT32 getRequestNo() const = 0;
  virtual MBOOL IsReprocessFrame() const = 0;
  virtual ~IPipelineFrame() {}
  virtual std::shared_ptr<IPipelineNodeMap const> getPipelineNodeMap()
      const = 0;
  virtual IPipelineDAG const& getPipelineDAG() const = 0;
  virtual std::shared_ptr<IPipelineDAG> getPipelineDAGSp() = 0;
  virtual IStreamBufferSet& getStreamBufferSet() const = 0;
  virtual IStreamInfoSet const& getStreamInfoSet() const = 0;
  /**
   * Note: getPipelineNodeCallback() const
   * actually, IPipelineNodeCallback is stored as wp. Calling this
   * function, this module will help to promote IPipelineNodeCallback from wp
   * to sp.
   */
  virtual std::shared_ptr<IPipelineNodeCallback> getPipelineNodeCallback()
      const = 0;

  virtual MERROR queryIOStreamInfoSet(
      NodeId_T const& nodeId,
      std::shared_ptr<IStreamInfoSet const>* rIn,
      std::shared_ptr<IStreamInfoSet const>* rOut) const = 0;

  virtual MERROR queryInfoIOMapSet(NodeId_T const& nodeId,
                                   InfoIOMapSet* rIOMapSet) const = 0;

  /**
   * Attach a pipeline frame listener.
   *
   * @param[in] pListener: the listener to attach.
   *
   * @param[in] pCookie: the listener's cookie.
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual MERROR attachListener(
      std::weak_ptr<IPipelineFrameListener> const& pListener,
      MVOID* pCookie) = 0;

  /**
   * Dump debugging state.
   */
  virtual MVOID dumpState(const std::vector<std::string>& options) const = 0;
};

/**
 * An interface of callback function from node to pipeline context
 */
class INodeCallbackToPipeline {
 public:  ////                    Definitions.
  enum eNoticeType {
    eNotice_ReadyToEnque,
  };

  struct CallBackParams {
    /**
     * A unique value for node id.
     */
    NodeId_T nodeId;

    /**
     * Last frame number of node to process
     */
    MUINT32 lastFrameNum;

    /**
     * callback notice type
     */
    eNoticeType noticeType;

    CallBackParams()
        : nodeId(0), lastFrameNum(0), noticeType(eNotice_ReadyToEnque) {}
  };

 public:  ////                    Attributes.
  virtual MVOID onCallback(CallBackParams param) = 0;
};

/**
 * An interface of pipeline node.
 */

class IPipelineNode {
 public:  ////                    Definitions.
  typedef Pipeline_NodeId_T NodeId_T;

  /**
   * Initialization Parameters.
   */
  struct InitParams {
    /**
     * An index to indicate which camera device to open.
     */
    MINT32 openId = 0;

    /**
     * A unique value for the node id.
     */
    NodeId_T nodeId = 0;

    /**
     * A pointer to a null-terminated string for the node name.
     */
    char const* nodeName = NULL;

    /**
     * index list to indicate which camera devices cooperate with this opened
     * camera device.
     */
    std::vector<MUINT32> subOpenIdList;
  };

 public:  ////                    Attributes.
  /**
   * @return
   *      An index to indicate which camera device to open.
   */
  virtual MINT32 getOpenId() const = 0;

  /**
   * @return
   *      A unique node id.
   */
  virtual NodeId_T getNodeId() const = 0;

  /**
   * @return
   *      A null-terminated string for the node name.
   */
  virtual char const* getNodeName() const = 0;

 public:  ////                    Operations.
  /**
   *
   */
  virtual MERROR init(InitParams const& rParams) = 0;

  /**
   *
   */
  virtual MERROR uninit() = 0;

  /**
   *
   */
  virtual MERROR flush() = 0;

  /**
   *
   */
  virtual MERROR flush(std::shared_ptr<IPipelineFrame> const& pFrame) = 0;

  /**
   *
   */
  virtual MERROR kick() = 0;

  /**
   *
   */
  virtual MERROR setNodeCallBack(
      std::weak_ptr<INodeCallbackToPipeline> pCallback) = 0;

  /**
   *
   */
  virtual MERROR queue(std::shared_ptr<IPipelineFrame> pFrame) = 0;
  virtual ~IPipelineNode() {}
};

/**
 * An interface of callback function from node to pipeline user
 */
struct IPipelineNodeCallback {
 public:
  enum eCtrlType {
    eCtrl_Setting,
    eCtrl_Sync,
    eCtrl_Resize,
    eCtrl_Readout,
  };

 public:
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
  virtual MVOID onCtrlSync(MUINT32 equestNo,
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

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace v3
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_PIPELINE_IPIPELINENODE_H_
