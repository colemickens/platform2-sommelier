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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_JPEGNODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_JPEGNODE_H_
//
#include "cros-camera/jpeg_compressor.h"
#include <mtkcam/pipeline/pipeline/IPipelineNode.h>
#include <mtkcam/pipeline/stream/IStreamBuffer.h>
#include <mtkcam/pipeline/stream/IStreamInfo.h>
#include <mtkcam/pipeline/utils/streambuf/IStreamBufferPool.h>
//
#include <memory>
#include <vector>
//

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {

/******************************************************************************
 *
 ******************************************************************************/
class VISIBILITY_PUBLIC JpegNode : virtual public IPipelineNode {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Definitions.
  typedef std::vector<std::shared_ptr<IMetaStreamInfo> > MetaStreamInfoSetT;
  typedef std::vector<std::shared_ptr<IImageStreamInfo> > ImageStreamInfoSetT;

  typedef IPipelineNode::InitParams InitParams;

  struct ConfigParams {
    /**
     * A pointer to a set of input app meta stream info.
     */
    std::shared_ptr<IMetaStreamInfo> pInAppMeta;

    /**
     * A pointer to a set of input hal meta stream info.
     */
    std::shared_ptr<IMetaStreamInfo> pInHalMeta_capture;

    std::shared_ptr<IMetaStreamInfo> pInHalMeta_streaming;

    /**
     * A pointer to a set of output app meta stream info.
     */
    std::shared_ptr<IMetaStreamInfo> pOutAppMeta;

    /**
     * A pointer to a main yuv input image stream info.
     */
    std::shared_ptr<IImageStreamInfo> pInYuv_Main;

    /**
     * A pointer to a full-size raw input image stream info.
     */
    std::shared_ptr<IImageStreamInfo> pInYuv_Thumbnail;

    /**
     * A pointer to a set of output image stream info.
     */
    std::shared_ptr<IImageStreamInfo> pOutJpeg;
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Operations.
  static std::shared_ptr<JpegNode> createInstance();
  virtual ~JpegNode() {}

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineNode Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Attributes.
  virtual MERROR init(InitParams const& rParams) = 0;

  virtual MERROR config(ConfigParams const& rParams) = 0;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_JPEGNODE_H_
