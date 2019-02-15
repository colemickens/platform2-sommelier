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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_P2STREAMINGNODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_P2STREAMINGNODE_H_
//
#include <memory>
#include <vector>
//
#include <mtkcam/pipeline/pipeline/IPipelineNode.h>
#include <mtkcam/pipeline/stream/IStreamInfo.h>
#include <mtkcam/pipeline/stream/IStreamBuffer.h>
#include <mtkcam/pipeline/utils/streambuf/IStreamBufferPool.h>
//
#include <mtkcam/pipeline/hwnode/P2Common.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {

/******************************************************************************
 *
 ******************************************************************************/
class VISIBILITY_PUBLIC P2StreamingNode : virtual public IPipelineNode {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Definitions.
  typedef std::vector<std::shared_ptr<IMetaStreamInfo>> MetaStreamInfoSetT;
  typedef std::vector<std::shared_ptr<IImageStreamInfo>> ImageStreamInfoSetT;

  typedef IPipelineNode::InitParams InitParams;

  enum ePass2Type { PASS2_STREAM, PASS2_TIMESHARING, PASS2_TYPE_TOTAL };
  enum eCustomOption {
    CUSTOM_OPTION_NONE = 0,
  };

  struct ConfigParams {
    ConfigParams()
        : pInAppRetMeta(NULL),
          pOutCaptureImage(NULL),
          burstNum(0),
          uUserId(0),
          customOption(0) {}

    /**
     * A pointer to a set of input app meta stream info.
     */
    std::shared_ptr<IMetaStreamInfo> pInAppMeta;

    /**
     * A pointer to a set of input app result meta stream info.
     */
    std::shared_ptr<IMetaStreamInfo> pInAppRetMeta;

    /**
     * A pointer to a set of input hal meta stream info.
     */
    std::shared_ptr<IMetaStreamInfo> pInHalMeta;

    /**
     * A pointer to a set of input app result meta stream info.
     */
    std::shared_ptr<IMetaStreamInfo> pInAppRetMeta_Sub;

    /**
     * A pointer to a set of input hal meta stream info.
     */
    std::shared_ptr<IMetaStreamInfo> pInHalMeta_Sub;

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
    std::vector<std::shared_ptr<IImageStreamInfo>> pvInFullRaw;

    /**
     * A pointer to a full-size raw input image stream info.
     */
    std::shared_ptr<IImageStreamInfo> pInResizedRaw;

    /**
     * A pointer to a lcs raw input image stream info.
     */
    std::shared_ptr<IImageStreamInfo> pInLcsoRaw;

    /**
     * A pointer to a rss raw input image stream info.
     */
    std::shared_ptr<IImageStreamInfo> pInRssoRaw;

    /**
     * A pointer to a full-size raw input image stream info.
     */
    std::vector<std::shared_ptr<IImageStreamInfo>> pvInFullRaw_Sub;

    /**
     * A pointer to a full-size raw input image stream info.
     */
    std::shared_ptr<IImageStreamInfo> pInResizedRaw_Sub;

    /**
     * A pointer to a lcs raw input image stream info.
     */
    std::shared_ptr<IImageStreamInfo> pInLcsoRaw_Sub;

    /**
     * A pointer to a rss raw input image stream info.
     */
    std::shared_ptr<IImageStreamInfo> pInRssoRaw_Sub;

    /**
     * A pointer to a set of output image stream info.
     */
    ImageStreamInfoSetT vOutImage;

    /**
     * A pointer to FD image stream info.
     */
    std::shared_ptr<IImageStreamInfo> pOutFDImage;

    /**
     * A pointer to YUV reprocessing image stream info.
     */
    std::shared_ptr<IImageStreamInfo> pInYuvImage;

    /**
     * A pointer to Capture image stream info.
     */
    std::shared_ptr<IImageStreamInfo> pOutCaptureImage;

    /**
     * The number of burst processing: default value is 0.
     */
    MUINT8 burstNum;

    /**
     * A pointer to input image stream info. (full-zsl input port)
     */
    std::vector<std::shared_ptr<IImageStreamInfo>> pvInOpaque;

    /**
     * A pointer to input image stream info. (full-zsl input port)
     */
    std::vector<std::shared_ptr<IImageStreamInfo>> pvInOpaque_Sub;

    /**
     * user's id
     */
    MUINT64 uUserId;

    /**
     * customize option
     */
    MUINT32 customOption;

    /**
     * UsageHint for common HAL3 P2
     */
    P2Common::UsageHint mUsageHint;

    P2Common::StreamConfigure vStreamConfigure;
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Operations.
  static std::shared_ptr<P2StreamingNode> createInstance(
      ePass2Type type = PASS2_STREAM);
  static std::shared_ptr<P2StreamingNode> createInstance(
      ePass2Type type, P2Common::UsageHint usage);

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
};  // namespace v3
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_P2STREAMINGNODE_H_
