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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_P2CAPTURENODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_P2CAPTURENODE_H_
//

#include <mtkcam/pipeline/hwnode/P2Common.h>
#include <mtkcam/pipeline/pipeline/IPipelineNode.h>
#include <mtkcam/pipeline/stream/IStreamBuffer.h>
#include <mtkcam/pipeline/stream/IStreamInfo.h>
#include <mtkcam/pipeline/utils/streambuf/IStreamBufferPool.h>
//
#include <memory>
#include <vector>
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {

/******************************************************************************
 *
 ******************************************************************************/
class VISIBILITY_PUBLIC P2CaptureNode : virtual public IPipelineNode {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Definitions.
  typedef IPipelineNode::InitParams InitParams;

  enum ePass2Type { PASS2_STREAM, PASS2_TIMESHARING, PASS2_TYPE_TOTAL };

  enum eCustomOption { CUSTOM_OPTION_NONE = 0 };

  struct ConfigParams {
    ConfigParams() : uCustomOption(0) {}
    /**
     * A pointer to a set of input app meta stream info.
     */
    std::shared_ptr<IMetaStreamInfo> pInAppMeta;

    /**
     * A pointer to a set of input app result meta stream info.
     */
    std::shared_ptr<IMetaStreamInfo> pInAppRetMeta;
    std::shared_ptr<IMetaStreamInfo> pInAppRetMeta2;

    /**
     * A pointer to a set of input hal meta stream info.
     */
    std::shared_ptr<IMetaStreamInfo> pInHalMeta;
    std::shared_ptr<IMetaStreamInfo> pInHalMeta2;

    /**
     * A pointer to a set of output app meta stream info.
     */
    std::shared_ptr<IMetaStreamInfo> pOutAppMeta;

    /**
     * A pointer to a set of output hal meta stream info.
     */
    std::shared_ptr<IMetaStreamInfo> pOutHalMeta;

    /**
     * A pointer to a full-size raw input image stream info.
     */
    std::shared_ptr<IImageStreamInfo> pInFullRaw;
    std::shared_ptr<IImageStreamInfo> pInFullRaw2;

    /**
     * A pointer to a full-size raw input image stream info.
     */
    std::shared_ptr<IImageStreamInfo> pInResizedRaw;
    std::shared_ptr<IImageStreamInfo> pInResizedRaw2;

    /**
     * A pointer to input image stream info. (full-zsl input port)
     */
    std::vector<std::shared_ptr<IImageStreamInfo>> vpInOpaqueRaws;

    /**
     * A pointer to YUV reprocessing image stream info.
     */
    std::shared_ptr<IImageStreamInfo> pInFullYuv;

    /**
     * A pointer to a lcso raw input image stream info.
     */
    std::shared_ptr<IImageStreamInfo> pInLcsoRaw;
    std::shared_ptr<IImageStreamInfo> pInLcsoRaw2;

    /**
     * A set of pointer to output image stream info.
     */
    std::vector<std::shared_ptr<IImageStreamInfo>> vpOutImages;

    /**
     * A pointer to JPEG YUV image stream info.
     */
    std::shared_ptr<IImageStreamInfo> pOutJpegYuv;

    /**
     * A pointer to post view image stream info.
     */
    std::shared_ptr<IImageStreamInfo> pOutPostViewYuv;

    /**
     * A pointer to clean image stream info.
     */
    std::shared_ptr<IImageStreamInfo> pOutCleanYuv;

    /**
     * A pointer to depth stream info.
     */
    std::shared_ptr<IImageStreamInfo> pOutDepth;

    /**
     * A pointer to thumbnail image stream info.
     */
    std::shared_ptr<IImageStreamInfo> pOutThumbnailYuv;

    /**
     * customize option
     */
    MUINT32 uCustomOption;

    P2Common::StreamConfigure vStreamConfigure;
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  static std::shared_ptr<P2CaptureNode> createInstance(
      ePass2Type type = PASS2_TIMESHARING);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineNode Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  virtual MERROR init(InitParams const& rParams) = 0;

  virtual MERROR config(ConfigParams const& rParams) = 0;
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace v3
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_P2CAPTURENODE_H_
