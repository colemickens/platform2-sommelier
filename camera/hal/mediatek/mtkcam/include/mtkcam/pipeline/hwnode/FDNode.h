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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_FDNODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_FDNODE_H_

#include <memory>
#include <vector>

#include <mtkcam/pipeline/pipeline/IPipelineNode.h>
#include <mtkcam/pipeline/stream/IStreamInfo.h>
#include <mtkcam/pipeline/stream/IStreamBuffer.h>
#include <mtkcam/pipeline/utils/streambuf/IStreamBufferPool.h>
//

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {

/******************************************************************************
 *
 ******************************************************************************/
class VISIBILITY_PUBLIC FdNode : virtual public IPipelineNode {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                                            Definitions.
  typedef std::vector<std::shared_ptr<IMetaStreamInfo> > MetaStreamInfoSetT;
  typedef std::vector<std::shared_ptr<IImageStreamInfo> > ImageStreamInfoSetT;
  typedef NSCam::v3::Utils::IStreamBufferPool<IImageStreamBuffer>
      IImageStreamBufferPoolT;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Definitions.
  typedef IPipelineNode::InitParams InitParams;

  /**
   * Configure Parameters.
   */
  struct ConfigParams {
    /**
     * A pointer to a set of input meta stream info.
     */
    std::shared_ptr<IMetaStreamInfo> pInAppMeta;

    /**
     * A pointer to a set of input image stream info.
     */
    std::shared_ptr<IMetaStreamInfo> pInHalMeta;

    /**
     * A pointer to a set of output meta stream info.
     */
    std::shared_ptr<IMetaStreamInfo> pOutAppMeta;

    /**
     * A pointer to a set of input image stream info.
     */
    std::shared_ptr<IImageStreamInfo> vInImage;
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Operations.
  static std::shared_ptr<FdNode> createInstance();

  virtual MERROR config(ConfigParams const& rParams) = 0;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineNode Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Operations.
  virtual MERROR init(InitParams const& rParams) = 0;

  virtual MERROR uninit() = 0;

  virtual MERROR flush() = 0;

  virtual MERROR queue(std::shared_ptr<IPipelineFrame> pFrame) = 0;

 public:  ////                    Operations.
};

};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_FDNODE_H_
