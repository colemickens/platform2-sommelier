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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_P1NODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_P1NODE_H_

#include <memory>
#include <vector>

#include <mtkcam/pipeline/pipeline/IPipelineNode.h>
#include <mtkcam/pipeline/stream/IStreamInfo.h>
#include <mtkcam/pipeline/stream/IStreamBuffer.h>
#include <mtkcam/pipeline/utils/streambuf/IStreamBufferPool.h>
#include <mtkcam/pipeline/utils/SyncHelper/ISyncHelper.h>
#include <mtkcam/utils/hw/IResourceConcurrency.h>
#include <mtkcam/drv/def/ICam_type.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {

/******************************************************************************
 *
 ******************************************************************************/
class VISIBILITY_PUBLIC P1Node : public virtual IPipelineNode {
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
   * The pipe mode for DRV normal pipe mode selection (EPipeSelect)
   */
  enum PIPE_MODE {
    PIPE_MODE_NORMAL = 0,
    PIPE_MODE_NORMAL_SV,
  };

  /**
   * The receive mode for p1 receive the pipeline frame
   */
  enum REV_MODE {
    REV_MODE_NORMAL = 0,    // General case
    REV_MODE_CONSERVATIVE,  // SMVR, ...
    REV_MODE_AGGRESSIVE     // Reserved for development
  };

  /**
   * The resize quality setting
   */
  enum RESIZE_QUALITY {
    RESIZE_QUALITY_UNKNOWN = 0,  // unknown/undefined
    RESIZE_QUALITY_L,            // level low
    RESIZE_QUALITY_H,            // level high
  };

  /**
   * The raw default type
   */
  enum RAW_DEF_TYPE {
    RAW_DEF_TYPE_PROCESSED_RAW = 0x0000, /*!< Processed raw */
    RAW_DEF_TYPE_PURE_RAW = 0x0001,      /*!< Pure raw */
    RAW_DEF_TYPE_AUTO = 0x000F           // if (rawProcessed ||
                                         //     post-proc-raw-unsupported)
                                         //     PROCESSED_RAW
                                         // else
                                         //     PURE_RAW
  };

  /**
   * Sensor Parameters.
   */
  struct SensorParams {
    /* Refer to SENSOR_SCENARIO_ID_XXX in IHalSensor.h */
    MUINT mode;
    MSize size;
    MUINT fps;
    MUINT32 pixelMode;
    /* Refer to SENSOR_VHDR_MODE_XXX in IHalSensor.h.
     * It is independant with SENSOR_SCENARIO_ID_XXX currently.
     * It depends on intersection of :
     * (1) app open hdr or not
     * (2) which vhdr mode sensor supported.
     */
    MUINT32 vhdrMode;

    SensorParams() : mode(0), size(), fps(0), pixelMode(0), vhdrMode(0) {}

    SensorParams(MUINT _mode, MSize _size, MUINT _fps, MUINT32 _pxlMode)
        : mode(_mode),
          size(_size),
          fps(_fps),
          pixelMode(_pxlMode),
          vhdrMode(0) {}

    SensorParams(MUINT _mode,
                 MSize _size,
                 MUINT _fps,
                 MUINT32 _pxlMode,
                 MUINT32 _vhdrMode)
        : mode(_mode),
          size(_size),
          fps(_fps),
          pixelMode(_pxlMode),
          vhdrMode(_vhdrMode) {}
  };

  /**
   * Configure Parameters.
   */
  struct ConfigParams {
    ConfigParams()
        : pInAppMeta(NULL),
          pInHalMeta(NULL),
          pInImage_opaque(NULL),
          pInImage_yuv(NULL),
          pOutAppMeta(NULL),
          pOutHalMeta(NULL),
          pOutImage_resizer(NULL),
          pOutImage_opaque(NULL),
          pStreamPool_resizer(NULL),
          pStreamPool_full(NULL),
          pResourceConcurrency(NULL),
          pSyncHelper(NULL),
          pipeMode(PIPE_MODE_NORMAL),
          pipeBit(NSCam::NSIoPipe::NSCamIOPipe::CAM_Pipeline_12BITS),
          resizeQuality(RESIZE_QUALITY_UNKNOWN),
          tgNum(0),
          burstNum(0),
          initRequest(0),
          receiveMode(REV_MODE_NORMAL),
          rawProcessed(MFALSE),
          rawDefType(RAW_DEF_TYPE_AUTO),
          disableFrontalBinning(MFALSE),
          disableDynamicTwin(MTRUE),
          disableHLR(MFALSE),
          enableUNI(MFALSE),
          enableEIS(MFALSE),
          enableLCS(MFALSE),
          enableCaptureFlow(MFALSE),
          enableFrameSync(MFALSE),
          forceSetEIS(MFALSE),
          packedEisInfo(0) {
      sensorParams.mode = 0;
      sensorParams.size = MSize(0, 0);
      sensorParams.fps = 0;
      sensorParams.pixelMode = 0;
      pvOutImage_full.clear();
    }
    /**
     * A pointer to input meta stream info.
     */
    std::shared_ptr<IMetaStreamInfo> pInAppMeta;

    /**
     * A pointer to input meta stream info.
     */
    std::shared_ptr<IMetaStreamInfo> pInHalMeta;

    /**
     * A pointer to input image stream info. (full-zsl input port)
     */
    std::shared_ptr<IImageStreamInfo> pInImage_opaque;

    /**
     * A pointer to input image stream info. (yuv input port)
     */
    std::shared_ptr<IImageStreamInfo> pInImage_yuv;

    /**
     * A pointer to output meta stream info.
     */
    std::shared_ptr<IMetaStreamInfo> pOutAppMeta;

    /**
     * A pointer to output meta stream info.
     */
    std::shared_ptr<IMetaStreamInfo> pOutHalMeta;

    /**
     * A pointer to output image stream info. (resizer output port)
     */
    std::shared_ptr<IImageStreamInfo> pOutImage_resizer;

    /**
     * A pointer to output image stream info. (lcs output port)
     */
    std::shared_ptr<IImageStreamInfo> pOutImage_lcso;

    /**
     * A pointer to output image stream info. (rss output port)
     */
    std::shared_ptr<IImageStreamInfo> pOutImage_rsso;

    /**
     * A pointer to output image stream info. (full ouput port)
     */
    std::vector<std::shared_ptr<IImageStreamInfo> > pvOutImage_full;

    /**
     * A pointer to output image stream info. (full-zsl output port)
     */
    std::shared_ptr<IImageStreamInfo> pOutImage_opaque;

    /**
     * A pointer to sensor parameter. Must Have.
     */
    SensorParams sensorParams;

    /**
     * A pointer to resizer output image stream info.
     */
    std::shared_ptr<IImageStreamBufferPoolT> pStreamPool_resizer;

    /**
     * A pointer to lcs output image stream info.
     */
    std::shared_ptr<IImageStreamBufferPoolT> pStreamPool_lcso;

    /**
     * A pointer to rss output image stream info.
     */
    std::shared_ptr<IImageStreamBufferPoolT> pStreamPool_rsso;

    /**
     * A pointer to full output image stream info.
     */
    std::shared_ptr<IImageStreamBufferPoolT> pStreamPool_full;

    /**
     * A pointer to the resource concurrency control.
     */
    std::shared_ptr<IResourceConcurrency> pResourceConcurrency;

    /**
     * A pointer to the sync helper module.
     */
    std::shared_ptr<NSCam::v3::Utils::Imp::ISyncHelper> pSyncHelper;

    /**
     * The metadata pass to 3A as NS3Av3::ConfigInfo_T::CfgAppMeta.
     */
    NSCam::IMetadata cfgAppMeta;

    /**
     * The metadata pass to 3A as NS3Av3::ConfigInfo_T::CfgHalMeta.
     */
    NSCam::IMetadata cfgHalMeta;

    /**
     * The enum for DRV normal pipe mode selection (EPipeSelect).
     */
    PIPE_MODE pipeMode;

    /**
     * The enum (E_CAM_PipelineBitDepth_SEL) for DRV pipeline raw bit depth.
     */
    MUINT32 pipeBit;

    /**
     * The resize quality level for DRV to control the frontal binning.
     * RESIZE_QUALITY_H : DRV will try to disable frontal binning
     * RESIZE_QUALITY_L : DRV will try to enable frontal binning
     * RESIZE_QUALITY_UNKNOWN, DRV will reference the disableFrontalBinning
     */
    RESIZE_QUALITY resizeQuality;

    /**
     * The number of TG as configure: default value is 0 for auto assign.
     */
    MUINT8 tgNum;

    /**
     * The number of burst processing: default value is 0.
     */
    MUINT8 burstNum;

    /**
     * The number of init request set : default value is 0.
     * (initRequest = 0) : disable the init request flow
     * (initRequest > 0) : enable the init request flow
     * and it need initRequest request-sets before DRV start
     */
    MUINT8 initRequest;

    /**
     * The receive mode while the pipeline frame arrival,
     * default value is REV_MODE_NORMAL.
     */
    REV_MODE receiveMode;

    /**
     * A boolean to enable the processed raw type of full path.
     * It will be ignored while the platform post-proc-raw-unsupported.
     */
    MBOOL rawProcessed;

    /**
     * The raw default type for a request did not set its raw type.
     * if (post-proc-raw-unsupported)
     *     it should be TYPE_PROCESSED_RAW/TYPE_AUTO
     * else if (rawProcessed)
     *     it should be TYPE_PROCESSED_RAW/TYPE_PURE_RAW/TYPE_AUTO
     * else
     *     it should be TYPE_PURE_RAW/TYPE_AUTO
     */
    RAW_DEF_TYPE rawDefType;

    /**
     * A boolean to force to disable the frontal binning.
     */
    MBOOL disableFrontalBinning;

    /**
     * A boolean to force to disable the dynamic twin mode.
     * True: forced-off while platform turn-off supported
     * False: auto, decide by the platform capability
     */
    MBOOL disableDynamicTwin;

    /**
     * A boolean to force to disable the HLR.
     * True:forced-off False:auto
     */
    MBOOL disableHLR;

    /**
     * A boolean to force to enable the UNI.
     * True:forced-on False:auto
     */
    MBOOL enableUNI;

    /**
     * A boolean to enable the EIS related function.
     */
    MBOOL enableEIS;

    /**
     * A boolean to enable the LCS related function.
     */
    MBOOL enableLCS;

    /**
     * A boolean to enable optimized capture flow.
     */
    MBOOL enableCaptureFlow;

    /**
     * A boolean indicates whether this sensor needs to be synchronized with
     * another one. Set to true to turn on hwsync module
     */
    MBOOL enableFrameSync;

    /**
     * A boolean to force set the EIS related function.
     */
    MBOOL forceSetEIS;

    /**
     An integer to store packed EIS info
     */
    MUINT64 packedEisInfo;
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Operations.
  static std::shared_ptr<P1Node> createInstance();

  virtual MERROR config(ConfigParams const& rParams) = 0;

  virtual MERROR init(InitParams const& rParams) = 0;
};

};  // namespace v3
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_P1NODE_H_
