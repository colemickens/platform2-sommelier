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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_POLICY_FEATURESETTINGPOLICY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_POLICY_FEATURESETTINGPOLICY_H_
//
//// MTKCAM
#include <mtkcam/def/common.h>
#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>

//// MTKCAM3
#include <mtkcam/3rdparty/common/scenario_mgr.h>
#include <mtkcam/3rdparty/plugin/PipelinePlugin.h>
#include <mtkcam/3rdparty/plugin/PipelinePluginType.h>
#include <mtkcam/feature/hdrDetection/Defs.h>
#include <mtkcam/pipeline/policy/IFeatureSettingPolicy.h>

//
#include <impl/ScenarioControl.h>
//
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {
namespace featuresetting {

struct DefaultConfigParams {
  /**************************************************************************
   * Default(first request) config parameters
   *
   * The parameters related to keep the first request params is shown as below.
   *
   **************************************************************************/

  /**************************************************************************
   * Keep the defaul config for Feature Setting Policy
   *
   * Use the default config if features don't use dedicated config.
   *
   **************************************************************************/
  bool bInit = false;
  /**
   *  The default sensor setting
   */
  std::vector<uint32_t> sensorMode;
};

struct ParsedStrategyInfo {
  // strategy info for common (per-frames info)
  uint32_t customHint = 0;

  // strategy info for capture feature
  bool isZslModeOn = false;
  bool isZslFlowOn = false;
  bool isFlashOn = false;
  bool isCShot = false;
  uint32_t exposureTime = 0;
  uint32_t realIso = 0;

  // strategy info for streaming feature (almost per-frames)
  // TODO(MTK): reserve
};

#define HW_SWITCH_VHDR_ISO_THRESHOLD 2800
#define HW_4CELL_ISO_THRESHOLD 800

enum SwitchModeStatus {
  eSwitchMode_Undefined,
  eSwitchMode_HighLightMode,   // for high light environment to use high speed
                               // sensor mode
  eSwitchMode_LowLightLvMode,  // for low light environment to use binning
                               // sensor mode
};

struct VhdrInfo {
  // vhdr debug mode
  MBOOL bVhdrDebugMode;

  // first config
  MBOOL bFirstConfig;

  // is do capture
  MBOOL bIsDoCapture;

  // dummy frame count
  MINT32 DummyCount;

  // Last frame vhdr mode
  MINT32 cfgVhdrMode;

  // config app hdrMode
  HDRMode lastAppHdrMode;

  // current app hdrMode
  HDRMode curAppHdrMode;

  // UI app hdrMode
  HDRMode UiAppHdrMode;

  // Iso switch mode
  SwitchModeStatus IsoSwitchModeStatus;

  VhdrInfo(void)
      : bVhdrDebugMode(MFALSE),
        bFirstConfig(MTRUE),
        bIsDoCapture(MFALSE),
        DummyCount(0),
        cfgVhdrMode(SENSOR_VHDR_MODE_NONE),
        lastAppHdrMode(HDRMode::OFF),
        curAppHdrMode(HDRMode::OFF),
        UiAppHdrMode(HDRMode::OFF),
        IsoSwitchModeStatus(eSwitchMode_HighLightMode) {}
};

/******************************************************************************
 *
 ******************************************************************************/
class FeatureSettingPolicy : public IFeatureSettingPolicy {
  // for Raw domain key feature (SW 4Cell, etc)
  typedef NSCam::NSPipelinePlugin::RawPlugin RawPlugin;
  typedef NSCam::NSPipelinePlugin::RawPlugin::IProvider::Ptr Raw_ProviderPtr;
  typedef NSCam::NSPipelinePlugin::RawPlugin::IInterface::Ptr Raw_InterfacePtr;
  typedef NSCam::NSPipelinePlugin::RawPlugin::Selection Raw_Selection;

  // for Yuv domain key feature
  typedef NSCam::NSPipelinePlugin::YuvPlugin YuvPlugin;
  typedef NSCam::NSPipelinePlugin::YuvPlugin::IProvider::Ptr Yuv_ProviderPtr;
  typedef NSCam::NSPipelinePlugin::YuvPlugin::IInterface::Ptr Yuv_InterfacePtr;
  typedef NSCam::NSPipelinePlugin::YuvPlugin::Selection Yuv_Selection;
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private:
  class MFPPluginWrapper;
  using MFPPluginWrapperPtr = std::shared_ptr<MFPPluginWrapper>;
  class RawPluginWrapper;
  using RawPluginWrapperPtr = std::shared_ptr<RawPluginWrapper>;
  class YuvPluginWrapper;
  using YuvPluginWrapperPtr = std::shared_ptr<YuvPluginWrapper>;

 private:
  CreationParams mPolicyParams;
  // Key feature plug-in interfaces and provider
  // MultiFrame:
  MFPPluginWrapperPtr mMFPPluginWrapperPtr;
  // Raw plugin: (for 4Cell key feature)
  RawPluginWrapperPtr mRawPluginWrapperPtr;
  // Yuv plugin: (for 4Cell key feature)
  YuvPluginWrapperPtr mYuvPluginWrapperPtr;
  // default config
  DefaultConfigParams mDefaultConfig;

  // feature configure input data
  ConfigurationInputParams mConfigInputParams;
  // feature configure output data
  ConfigurationOutputParams mConfigOutputParams;

  // property for debug
  MINT32 mbDebug;
  MINT64 mForcedKeyFeatures = -1;
  MINT64 mForcedFeatureCombination = -1;

  // for Vhdr
  VhdrInfo mVhdrInfo;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Operations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  // IFeatureSettingPolicy Interfaces.
  auto evaluateConfiguration(ConfigurationOutputParams* out,
                             ConfigurationInputParams const* in)
      -> int override;

  auto evaluateRequest(RequestOutputParams* out, RequestInputParams const* in)
      -> int override;

 private:
  // FeatureSettingPolicy Interfaces.
  // Evaluate Feature Configuration Interfaces:
  virtual auto evaluateCaptureConfiguration(ConfigurationOutputParams* out,
                                            ConfigurationInputParams const* in)
      -> bool;

  virtual auto evaluateStreamConfiguration(ConfigurationOutputParams* out,
                                           ConfigurationInputParams const* in)
      -> bool;

  // Evaluate Feature Setting Interfaces:
  virtual auto evaluateStreamSetting(RequestOutputParams* out,
                                     ParsedStrategyInfo const& parsedInfo,
                                     RequestInputParams const* in,
                                     bool enabledP2Capture) -> bool;

  virtual auto evaluateCaptureSetting(RequestOutputParams* out,
                                      ParsedStrategyInfo const& parsedInfo,
                                      RequestInputParams const* in) -> bool;

  // for capture strategy
  virtual auto querySelectionStrategyInfo(
      NSPipelinePlugin::StrategyInfo* strategyInfo,
      uint32_t sensorIndex,
      ParsedStrategyInfo const& parsedInfo,
      RequestOutputParams const* out,
      RequestInputParams const* in) -> bool;

  virtual auto updateDualCamRequestOutputParams(
      RequestOutputParams* out,
      NSPipelinePlugin::MetadataPtr pOutMetaApp_Additional,
      NSPipelinePlugin::MetadataPtr pOutMetaHal_Additional,
      uint32_t mainCamP1Dma,
      uint32_t sub1CamP1Dma,
      MINT64 featureCombination) -> bool;

  virtual auto updateVhdrDummyFrames(RequestOutputParams* out,
                                     RequestInputParams const* in) -> bool;

  virtual auto isNeedIsoReconfig(HDRMode* apphdrMode, uint32_t recodingMode)
      -> bool;

  virtual auto updateRequestInfo(
      RequestOutputParams* out,
      uint32_t sensorIndex,
      NSPipelinePlugin::RequestInfo const& requestInfo,
      RequestInputParams const* in __unused) -> bool;

  virtual auto strategySingleRawPlugin(
      MINT64 combinedKeyFeature, /*eFeatureIndexMtk and eFeatureIndexCustomer*/
      MINT64 featureCombination, /*eFeatureIndexMtk and eFeatureIndexCustomer*/
      MINT64* foundFeature,      /*eFeatureIndexMtk and eFeatureIndexCustomer*/
      RequestOutputParams* out,
      ParsedStrategyInfo const& parsedInfo,
      RequestInputParams const* in) -> bool;

  virtual auto strategySingleYuvPlugin(
      MINT64 combinedKeyFeature, /*eFeatureIndexMtk and eFeatureIndexCustomer*/
      MINT64 featureCombination, /*eFeatureIndexMtk and eFeatureIndexCustomer*/
      MINT64* foundFeature,      /*eFeatureIndexMtk and eFeatureIndexCustomer*/
      RequestOutputParams* out,
      ParsedStrategyInfo const& parsedInfo,
      RequestInputParams const* in) -> bool;
  virtual auto strategyNormalSingleCapture(
      MINT64 combinedKeyFeature, /*eFeatureIndexMtk and eFeatureIndexCustomer*/
      MINT64 featureCombination, /*eFeatureIndexMtk and eFeatureIndexCustomer*/
      RequestOutputParams* out,
      ParsedStrategyInfo const& parsedInfo,
      RequestInputParams const* in) -> bool;

  virtual auto dumpRequestOutputParams(RequestOutputParams* out,
                                       bool forcedEnable) -> bool;

  virtual auto updatePluginSelection(bool isFeatureTrigger) -> bool;

  virtual auto updateCaptureDummyFrames(
      MINT64 combinedKeyFeature, /*eFeatureIndexMtk and eFeatureIndexCustomer*/
      RequestOutputParams* out,
      const ParsedStrategyInfo& parsedInfo,
      RequestInputParams const* in) -> void;

  virtual auto strategyCaptureFeature(
      MINT64 combinedKeyFeature, /*eFeatureIndexMtk and eFeatureIndexCustomer*/
      MINT64 featureCombination, /*eFeatureIndexMtk and eFeatureIndexCustomer*/
      RequestOutputParams* out,
      ParsedStrategyInfo const& parsedInfo,
      RequestInputParams const* in) -> bool;

  //
  virtual auto evaluateReconfiguration(RequestOutputParams* out,
                                       RequestInputParams const* in) -> bool;

  virtual auto updateRequestResultParams(
      std::shared_ptr<RequestResultParams>* requestParams,
      NSPipelinePlugin::MetadataPtr pOutMetaApp_Additional,
      NSPipelinePlugin::MetadataPtr pOutMetaHal_Additional,
      uint32_t needP1Dma,
      uint32_t sensorIndex,
      MINT64 featureCombination = 0,
      MINT32 requestIndex = 0,
      MINT32 requestCount = 0) -> bool;

  virtual auto updateStreamData(RequestOutputParams* out,
                                ParsedStrategyInfo const& parsedInfo,
                                RequestInputParams const* in) -> bool;

  virtual auto collectParsedStrategyInfo(ParsedStrategyInfo* parsedInfo,
                                         RequestInputParams const* in) -> bool;

  virtual auto getCaptureP1DmaConfig(uint32_t* needP1Dma,
                                     RequestInputParams const* in,
                                     uint32_t sensorIndex) -> bool;

 public:
  // FeatureSettingPolicy Interfaces.
  explicit FeatureSettingPolicy(CreationParams const& params);
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace featuresetting
};      // namespace policy
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_POLICY_FEATURESETTINGPOLICY_H_
