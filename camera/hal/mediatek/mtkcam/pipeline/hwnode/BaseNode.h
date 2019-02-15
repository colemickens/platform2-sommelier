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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_BASENODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_BASENODE_H_

#include <memory>
#include <mtkcam/pipeline/pipeline/IPipelineNode.h>
#include <string>
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {

/******************************************************************************
 *
 ******************************************************************************/
class BaseNode : public virtual IPipelineNode {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                 Data Members.
  MINT32 mOpenId;
  IPipelineNode::NodeId_T mNodeId;
  std::string mNodeName;

 private:
  MINT32 mLogLevel;

 protected:  ////                 Operations.
  virtual MERROR ensureMetaBufferAvailable(
      MUINT32 const frameNo,
      StreamId_T const streamId,
      IStreamBufferSet* rStreamBufferSet,
      std::shared_ptr<IMetaStreamBuffer>* rpStreamBuffer,
      MBOOL acquire = MTRUE);

  virtual MERROR ensureImageBufferAvailable(
      MUINT32 const frameNo,
      StreamId_T const streamId,
      IStreamBufferSet* rStreamBufferSet,
      std::shared_ptr<IImageStreamBuffer>* rpStreamBuffer,
      MBOOL acquire = MTRUE);

  virtual MVOID onDispatchFrame(std::shared_ptr<IPipelineFrame> const& pFrame);

  virtual MVOID onEarlyCallback(std::shared_ptr<IPipelineFrame> const& pFrame,
                                StreamId_T const streamId,
                                IMetadata const& rMetaData,
                                MBOOL error = MFALSE);

  virtual MVOID onCtrlSetting(std::shared_ptr<IPipelineFrame> const& pFrame,
                              StreamId_T const metaAppStreamId,
                              IMetadata* rAppMetaData,
                              StreamId_T const metaHalStreamId,
                              IMetadata* rHalMetaData,
                              MBOOL* rIsChanged);

  virtual MVOID onCtrlSync(std::shared_ptr<IPipelineFrame> const& pFrame,
                           MUINT32 index,
                           MUINT32 type,
                           MINT64 duration);

  virtual MVOID onCtrlResize(std::shared_ptr<IPipelineFrame> const& pFrame,
                             StreamId_T const metaAppStreamId,
                             IMetadata* rAppMetaData,
                             StreamId_T const metaHalStreamId,
                             IMetadata* rHalMetaData,
                             MBOOL* rIsChanged);

  virtual MVOID onCtrlReadout(std::shared_ptr<IPipelineFrame> const& pFrame,
                              StreamId_T const metaAppStreamId,
                              IMetadata* rAppMetaData,
                              StreamId_T const metaHalStreamId,
                              IMetadata* rHalMetaData,
                              MBOOL* rIsChanged);

  virtual MBOOL needCtrlCb(std::shared_ptr<IPipelineFrame> const& pFrame,
                           IPipelineNodeCallback::eCtrlType eType);

  virtual MVOID onNextCaptureCallBack(
      std::shared_ptr<IPipelineFrame> const& pFrame);

  virtual MERROR setNodeCallBack(
      std::weak_ptr<INodeCallbackToPipeline> pCallback);

  virtual MERROR kick();

  virtual MERROR flush(std::shared_ptr<IPipelineFrame> const& pFrame);

 public:  ////                    Operations.
  BaseNode();
  virtual ~BaseNode() {}

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineNode Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Attributes.
  virtual MINT32 getOpenId() const;

  virtual IPipelineNode::NodeId_T getNodeId() const;

  virtual char const* getNodeName() const;
};

};  // namespace v3
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_BASENODE_H_
