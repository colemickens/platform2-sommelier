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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_PIPELINE_IPIPELINENODEMAPCONTROL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_PIPELINE_IPIPELINENODEMAPCONTROL_H_
//
#include <memory>
#include <mtkcam/pipeline/pipeline/IPipelineNode.h>
#include <mtkcam/pipeline/utils/streaminfo/IStreamInfoSetControl.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {

/**
 * An interface of Pipeline node map (key:NodeId_T, value:NodePtrT).
 */
class IPipelineNodeMapControl : public virtual IPipelineNodeMap {
 public:  ////                            Definitions.
  typedef IPipelineNodeMapControl ThisT;
  typedef Utils::IStreamInfoSetControl IStreamSetT;
  typedef std::shared_ptr<IStreamSetT> IStreamSetPtr;
  typedef std::shared_ptr<IStreamSetT const> IStreamSetPtr_CONST;

  struct INode {
   public:  ////                Operations.
    virtual NodePtrT const& getNode() const = 0;

    virtual IStreamSetPtr_CONST getInStreams() const = 0;
    virtual IStreamSetPtr_CONST getOutStreams() const = 0;
    virtual IStreamSetPtr const& editInStreams() const = 0;
    virtual IStreamSetPtr const& editOutStreams() const = 0;
    virtual ~INode() {}
  };

 public:  ////                    Operations.
  static ThisT* create();

  virtual MVOID clear() = 0;

  virtual ssize_t add(NodeId_T const& id, NodePtrT const& node) = 0;

  virtual std::shared_ptr<INode> getNodeFor(NodeId_T const& id) const = 0;
  virtual std::shared_ptr<INode> getNodeAt(size_t index) const = 0;
  virtual ~IPipelineNodeMapControl() {}
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_PIPELINE_IPIPELINENODEMAPCONTROL_H_
