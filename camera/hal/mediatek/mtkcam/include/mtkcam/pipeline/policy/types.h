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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_TYPES_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_TYPES_H_
//
#include <mtkcam/def/common.h>  // for mtkcam3/feature/eis/EisInfo.h
#include <mtkcam/pipeline/pipeline/PipelineContext.h>
#include <mtkcam/pipeline/stream/IStreamBuffer.h>
//
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {

////////////////////////////////////////////////////////////////////////////////

/**
 *  Pipeline static information.
 *
 *  The following information is static and unchanged forever, regardless of
 *  any operation (e.g. open or configure).
 */
struct PipelineStaticInfo {
  /**
   *  Logical device open id
   */
  int32_t openId = -1;

  /**
   *  Physical sensor id (0, 1, 2)
   */
  std::vector<int32_t> sensorIds;

  /**
   *  Sensor raw type.
   *
   *  SENSOR_RAW_xxx in mtkcam/include/mtkcam/drv/IHalSensor.h
   */
  std::vector<uint32_t> sensorRawTypes;

  /**
   *  Type3 PD sensor without PD hardware (ISP3.0)
   */
  bool isType3PDSensorWithoutPDE = false;

  /**
   *  is VHDR sensor
   */
  bool isVhdrSensor = false;
};

struct ParsedAppConfiguration;
struct ParsedAppImageStreamInfo;
struct ParsedDualCamInfo;

/**
 *  Pipeline user configuration
 *
 *  The following information is given and set up at the configuration stage,
 * and is never changed AFTER the configuration stage.
 */
struct PipelineUserConfiguration {
  std::shared_ptr<ParsedAppConfiguration> pParsedAppConfiguration;

  /**
   * Parsed App image stream info set
   *
   * It results from the raw data, i.e. vImageStreams.
   */
  std::shared_ptr<ParsedAppImageStreamInfo> pParsedAppImageStreamInfo;

  /**************************************************************************
   * App image stream info set (raw data)
   **************************************************************************/

  /**
   * App image streams to configure.
   */
  std::unordered_map<StreamId_T, std::shared_ptr<IImageStreamInfo>>
      vImageStreams;

  /**
   * App meta streams to configure.
   */
  std::unordered_map<StreamId_T, std::shared_ptr<IMetaStreamInfo>> vMetaStreams;

  /**
   * App image streams min frame duration to configure.
   */
  std::unordered_map<StreamId_T, int64_t> vMinFrameDuration;

  /**
   * App image streams stall frame duration to configure.
   */
  std::unordered_map<StreamId_T, int64_t> vStallFrameDuration;
};

////////////////////////////////////////////////////////////////////////////////

/**
 * P1 DMA bitmask definitions
 *
 * Used in the following structures:
 *      IPipelineSettingPolicy.h: RequestResultParams::needP1Dma
 *      IIOMapPolicy.h: RequestInputParams::pRequest_NeedP1Dma
 *      IStreamInfoConfigurationPolicy.h:
 * FunctionType_StreamInfoConfiguration_P1
 *
 */
enum : uint32_t {
  P1_IMGO = (0x01U << 0),
  P1_RRZO = (0x01U << 1),
  P1_LCSO = (0x01U << 2),
  P1_RSSO = (0x01U << 3),
  //
  P1_MASK = 0x0F,
};

/**
 * Reconfig category enum definitions
 * For pipelineModelSession processReconfiguration use
 */
enum class ReCfgCtg : uint8_t { NO = 0, STREAMING, CAPTURE, NUM };

/**
 *  Sensor Setting
 */
struct SensorSetting {
  uint32_t sensorMode = 0;
  uint32_t sensorFps = 0;
  MSize sensorSize;
};

static inline std::string toString(const SensorSetting& o) {
  std::string os = base::StringPrintf(
      "{ .sensorMode=%d .sensorFps=%d .sensorSize=%dx%d }", o.sensorMode,
      o.sensorFps, o.sensorSize.w, o.sensorSize.h);
  return os;
}

/**
 *  Pass1-specific HW settings
 */
struct P1HwSetting {
  uint32_t pixelMode = 0;
  int32_t imgoFormat = 0;
  size_t imgoStride = 0;
  MSize imgoSize;
  int32_t rrzoFormat = 0;
  size_t rrzoStride = 0;
  MSize rrzoSize;
  MSize rssoSize;
  bool usingCamSV = false;
};

static inline std::string toString(const P1HwSetting& o) {
  std::string os;
  os += "{ ";
  os += base::StringPrintf("imgo{%dx%d stride:%zu format:%#x} ", o.imgoSize.w,
                           o.imgoSize.h, o.imgoStride, o.imgoFormat);
  os += base::StringPrintf("rrzo{%dx%d stride:%zu format:%#x} ", o.rrzoSize.w,
                           o.rrzoSize.h, o.rrzoStride, o.rrzoFormat);
  os += base::StringPrintf("pixelMode:%u usingCamSV:%d ", o.pixelMode,
                           o.usingCamSV);
  os += "}";
  return os;
}

/**
 *  Parsed metadata control request
 */
struct ParsedMetaControl {
  bool repeating = false;

  int32_t control_aeTargetFpsRange[2] = {0};  // CONTROL_AE_TARGET_FPS_RANGE
  uint8_t control_captureIntent =
      static_cast<uint8_t>(-1L);  // CONTROL_CAPTURE_INTENT
  uint8_t control_enableZsl = static_cast<uint8_t>(0);    // CONTROL_ENABLE_ZSL
  uint8_t control_mode = static_cast<uint8_t>(-1L);       // CONTROL_MODE
  uint8_t control_sceneMode = static_cast<uint8_t>(-1L);  // CONTROL_SCENE_MODE
  uint8_t control_videoStabilizationMode =
      static_cast<uint8_t>(-1L);  // CONTROL_VIDEO_STABILIZATION_MODE
};

static inline std::string toString(const ParsedMetaControl& o) {
  std::string os;
  os += "{";
  os += base::StringPrintf(" repeating:%d", o.repeating);
  os += base::StringPrintf(" control.aeTargetFpsRange:%d,%d",
                           o.control_aeTargetFpsRange[0],
                           o.control_aeTargetFpsRange[1]);
  os +=
      base::StringPrintf(" control.captureIntent:%d", o.control_captureIntent);
  os += base::StringPrintf(" control.enableZsl:%d", o.control_enableZsl);
  if (static_cast<uint8_t>(-1L) != o.control_mode) {
    os += base::StringPrintf(" control.mode:%d", o.control_mode);
  }
  if (static_cast<uint8_t>(-1L) != o.control_sceneMode) {
    os += base::StringPrintf(" control.sceneMode:%d", o.control_sceneMode);
  }
  if (static_cast<uint8_t>(-1L) != o.control_videoStabilizationMode) {
    os += base::StringPrintf(" control.videoStabilizationMode:%d",
                             o.control_videoStabilizationMode);
  }
  os += " }";
  return os;
}

/**
 *  Parsed App configuration
 */
struct ParsedAppConfiguration {
  /**
   * The operation mode of pipeline.
   * The caller must promise its value.
   */
  uint32_t operationMode = 0;

  /**
   * Session wide camera parameters.
   *
   * The session parameters contain the initial values of any request keys that
   * were made available via ANDROID_REQUEST_AVAILABLE_SESSION_KEYS. The Hal
   * implementation can advertise any settings that can potentially introduce
   * unexpected delays when their value changes during active process requests.
   * Typical examples are parameters that trigger time-consuming HW
   * re-configurations or internal camera pipeline updates. The field is
   * optional, clients can choose to ignore it and avoid including any initial
   * settings. If parameters are present, then hal must examine their values and
   * configure the internal camera pipeline accordingly.
   */
  IMetadata sessionParams;

  /**
   * operationMode = 1
   *
   * StreamConfigurationMode::CONSTRAINED_HIGH_SPEED_MODE = 1
   * Refer to
   * https://developer.android.com/reference/android/hardware/camera2/params/SessionConfiguration#SESSION_HIGH_SPEED
   */
  bool isConstrainedHighSpeedMode = false;
};

/**
 *  Parsed App image stream info
 */
struct ParsedAppImageStreamInfo {
  /**************************************************************************
   *  App image stream info set
   **************************************************************************/

  /**
   * Output streams for any processed (but not-stalling) formats
   *
   * Reference:
   * https://developer.android.com/reference/android/hardware/camera2/CameraCharacteristics.html#REQUEST_MAX_NUM_OUTPUT_PROC
   */
  std::unordered_map<StreamId_T, std::shared_ptr<IImageStreamInfo>>
      vAppImage_Output_Proc;

  /**
   * Input stream for yuv reprocessing
   */
  std::shared_ptr<IImageStreamInfo> pAppImage_Input_Yuv;

  /**
   * Output stream for private reprocessing
   */
  std::shared_ptr<IImageStreamInfo> pAppImage_Output_Priv;

  /**
   * Input stream for private reprocessing
   */
  std::shared_ptr<IImageStreamInfo> pAppImage_Input_Priv;

  /**
   * Output stream for JPEG capture.
   */
  std::shared_ptr<IImageStreamInfo> pAppImage_Jpeg;

  /**************************************************************************
   *  Parsed info
   **************************************************************************/

  /**
   * One of consumer usages of App image streams contains
   * BufferUsage::VIDEO_ENCODER.
   */
  bool hasVideoConsumer = false;

  /**
   * 4K video recording
   */
  bool hasVideo4K = false;

  /**
   * The image size of video recording, in pixels.
   */
  MSize videoImageSize;

  /**
   * The max. image size of App image streams, in pixels, regardless of stream
   * formats.
   */
  MSize maxImageSize;
};

/**
 *  (Non Pass1-specific) Parsed stream info
 */
struct ParsedStreamInfo_NonP1 {
  /******************************************
   *  app meta stream info
   ******************************************/
  std::shared_ptr<IMetaStreamInfo> pAppMeta_Control;
  std::shared_ptr<IMetaStreamInfo> pAppMeta_DynamicP2StreamNode;
  std::shared_ptr<IMetaStreamInfo> pAppMeta_DynamicP2CaptureNode;
  std::shared_ptr<IMetaStreamInfo> pAppMeta_DynamicFD;
  std::shared_ptr<IMetaStreamInfo> pAppMeta_DynamicJpeg;

  /******************************************
   *  hal meta stream info
   ******************************************/
  std::shared_ptr<IMetaStreamInfo> pHalMeta_DynamicP2StreamNode;
  std::shared_ptr<IMetaStreamInfo> pHalMeta_DynamicP2CaptureNode;
  std::shared_ptr<IMetaStreamInfo> pHalMeta_DynamicPDE;

  /******************************************
   *  hal image stream info
   ******************************************/

  /**
   *  Face detection.
   */
  std::shared_ptr<IImageStreamInfo> pHalImage_FD_YUV;

  /**
   *  The Jpeg orientation is passed to HAL at the request stage.
   *  Maybe we can create a stream set for every orientation at the
   * configuration stage, but only one within that stream set can be passed to
   * the configuration of pipeline context.
   */
  std::shared_ptr<IImageStreamInfo> pHalImage_Jpeg_YUV;

  /**
   *  The thumbnail size is passed to HAL at the request stage.
   */
  std::shared_ptr<IImageStreamInfo> pHalImage_Thumbnail_YUV;
};

/**
 *  (Pass1-specific) Parsed stream info
 */
struct ParsedStreamInfo_P1 {
  /******************************************
   *  app meta stream info
   ******************************************/
  /**
   *  Only one of P1Node can output this data.
   *  Why do we need more than one of this stream?
   */
  std::shared_ptr<IMetaStreamInfo> pAppMeta_DynamicP1;

  /******************************************
   *  hal meta stream info
   ******************************************/
  std::shared_ptr<IMetaStreamInfo> pHalMeta_Control;
  std::shared_ptr<IMetaStreamInfo> pHalMeta_DynamicP1;

  /******************************************
   *  hal image stream info
   ******************************************/
  std::shared_ptr<IImageStreamInfo> pHalImage_P1_Imgo;
  std::shared_ptr<IImageStreamInfo> pHalImage_P1_Rrzo;
  std::shared_ptr<IImageStreamInfo> pHalImage_P1_Lcso;
  std::shared_ptr<IImageStreamInfo> pHalImage_P1_Rsso;
};

/**
 *  Pipeline nodes need.
 *  true indicates its corresponding pipeline node is needed.
 */
struct PipelineNodesNeed {
  /**
   * [Note]
   * The index is shared, for example:
   *      needP1Node[index]
   *      PipelineStaticInfo::sensorId[index]
   */
  std::vector<bool> needP1Node;

  bool needP2StreamNode = false;
  bool needP2CaptureNode = false;

  bool needFDNode = false;
  bool needJpegNode = false;
};

static inline std::string toString(const PipelineNodesNeed& o) {
  std::string os;
  os += "{ ";
  for (size_t i = 0; i < o.needP1Node.size(); i++) {
    if (o.needP1Node[i]) {
      os += base::StringPrintf("P1Node[%zu] ", i);
    }
  }
  if (o.needP2StreamNode) {
    os += "P2StreamNode ";
  }
  if (o.needP2CaptureNode) {
    os += "P2CaptureNode ";
  }
  if (o.needFDNode) {
    os += "FDNode ";
  }
  if (o.needJpegNode) {
    os += "JpegNode ";
  }
  os += "}";
  return os;
}

/**
 * Streaming feature settings
 */
struct StreamingFeatureSetting {
  struct AppInfo {
    int32_t recordState = -1;
    uint32_t appMode = 0;
    uint32_t eisOn = 0;
  };

  AppInfo mLastAppInfo;
  /**
   * The vhdr mode is decided to enable or not at configuration stage.
   * SENSOR_VHDR_MODE_xxx defined in include/mtkcam/drv/IHalSensor.h
   *      SENSOR_VHDR_MODE_NONE = 0x0,
   *      SENSOR_VHDR_MODE_IVHDR = 0x1,
   *      SENSOR_VHDR_MODE_MVHDR = 0x2,
   *      SENSOR_VHDR_MODE_ZVHDR = 0x9,
   */
  uint32_t vhdrMode = 0;

  uint32_t nr3dMode = 0;
  bool bNeedLMV = false;
  bool bIsEIS = false;
  uint32_t eisExtraBufNum = 0;
  uint32_t minRrzoEisW = 0;
  //
  // hint support feature for dedicated scenario for P2 node init
  int64_t supportedScenarioFeatures =
      0; /*eFeatureIndexMtk and eFeatureIndexCustomer*/
};

/**
 * capture feature settings
 */
struct CaptureFeatureSetting {
  /**
   */
  uint32_t maxAppJpegStreamNum = 1;
  uint32_t maxZslBufferNum = 0;
  //
  // hint support feature for dedicated scenario for P2 node init
  int64_t supportedScenarioFeatures =
      0; /*eFeatureIndexMtk and eFeatureIndexCustomer*/
};

/**
 * ZSL policy
 */
enum eZslPolicy {
  // bit 0~15:    preserved for image quality. select from metadata.
  // bitwise operation. the history buffer result must fullfile all
  // requirements.
  eZslPolicy_None = 0x0,
  eZslPolicy_AfState = 0x1 << 0,
  eZslPolicy_AeState = 0x1 << 1,
  eZslPolicy_DualFrameSync = 0x1 << 2,
  eZslPolicy_PD_ProcessedRaw = 0x1 << 3,

  // bit 16~27:   preserved for zsl behavior.
  eZslPolicy_Behavior_MASK = 0x0FFF0000,
  eZslPolicy_ContinuousFrame = (0x1 << 0) << 16,
  eZslPolicy_ZeroShutterDelay = (0x1 << 1) << 16,
  //
};

#define ZslBehaviorOf(PolicyType) (PolicyType & eZslPolicy_Behavior_MASK)

struct ZslPolicyParams {
  int32_t mPolicy = eZslPolicy_None; /*eZslPolicy*/
  //
  int64_t mTimestamp = -1;
  int64_t mTimeouts = 2000;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace policy
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_TYPES_H_
